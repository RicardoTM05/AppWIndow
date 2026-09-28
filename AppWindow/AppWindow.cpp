#include "Measure.h"
#include <unordered_map>
#include <mutex>

// GLOBALS & MODULE LIFECYCLE
static constexpr UINT_PTR SUBCLASS_ID = 1;
static std::unordered_map<void*, Measure*> g_measuresBySkin;
static std::mutex g_measuresMutex;

static LRESULT CALLBACK SkinSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID lpReserved) {
	if (reason == DLL_PROCESS_DETACH) {
		if (lpReserved == nullptr) ReleaseTaskbarLists();
	}
	return TRUE;
}

// MEASURE REGISTRY
static void AddMeasure(Measure* m) {
	if (!m) return;

	std::lock_guard<std::mutex> lock(g_measuresMutex);
	g_measuresBySkin[m->skin] = m;
}

static void RemoveMeasure(Measure* m) {
	if (!m) return;

	std::lock_guard<std::mutex> lock(g_measuresMutex);
	g_measuresBySkin.erase(m->skin);
}

static Measure* FindMeasureBySkin(void* skin) {
	if (!skin) return nullptr;

	std::lock_guard<std::mutex> lock(g_measuresMutex);
	auto it = g_measuresBySkin.find(skin);

	return it != g_measuresBySkin.end() ? it->second : nullptr;
}

// COMMAND PARSING HELPER
static bool ParseCommand(LPCWSTR args, LPCWSTR command, LPCWSTR& outRest) {
	size_t len = wcslen(command);
	if (_wcsnicmp(args, command, len) != 0) return false;

	wchar_t next = args[len];
	if (next != L'\0' && next != L' ') return false;

	outRest = args + len;
	while (*outRest == L' ') ++outRest;
	return true;
}

// WINDOW CONTENT VISIBILITY
static void HideWindowContents(Measure* m) {
	if (!m || m->isUnloading || !IsWindow(m->skinWindow)) return;

	BOOL cloak = TRUE;
	HRESULT hr = DwmSetWindowAttribute(m->skinWindow, DWMWA_CLOAK, &cloak, sizeof(cloak));

	if (FAILED(hr))	Log(m, LOG_ERROR, L"DwmSetWindowAttribute(DWMWA_CLOAK) failed " L"(HWND: %p, cloak: %d, HRESULT: 0x%08X)", m->skinWindow, cloak, hr);
}

static void ShowWindowContents(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return;

	BOOL cloak = FALSE;
	HRESULT hr = DwmSetWindowAttribute(m->skinWindow, DWMWA_CLOAK, &cloak, sizeof(cloak));

	if (FAILED(hr))	Log(m, LOG_ERROR, L"DwmSetWindowAttribute(DWMWA_CLOAK) failed " L"(HWND: %p, cloak: %d, HRESULT: 0x%08X)", m->skinWindow, cloak, hr);
}

// WINDOW SUBCLASSING
static bool SubclassSkin(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;
	if (m->isSubclassed) return true;

	m->isSubclassed = SetWindowSubclass(m->skinWindow, SkinSubclassProc, SUBCLASS_ID, (DWORD_PTR)m);
	if (!m->isSubclassed) Log(m, LOG_ERROR, L"Failed to subclass skin window (HWND: %p, error: %lu)", m->skinWindow, GetLastError());

	return m->isSubclassed;
}

static bool RemoveSkinSubclass(Measure* m) {
	if (!m || !m->isSubclassed) return true;

	if (!IsWindow(m->skinWindow)) {
		m->isSubclassed = false;
		return true;
	}
	m->isSubclassed = false;
	if (!RemoveWindowSubclass(m->skinWindow, SkinSubclassProc, SUBCLASS_ID)) {
		m->isSubclassed = true;
		Log(m, LOG_ERROR, L"Failed to remove subclass from skin window (HWND: %p, error: %lu)", m->skinWindow, GetLastError());
		return false;
	}
	return true;
}

// PROCESS IDENTITY
static void setProcessID() {
	PWSTR appID = nullptr;

	if (SUCCEEDED(GetCurrentProcessExplicitAppUserModelID(&appID))) {
		if (wcscmp(appID, L"Rainmeter.Rainmeter") != 0) SetCurrentProcessExplicitAppUserModelID(L"Rainmeter.Rainmeter");
		CoTaskMemFree(appID);
	}
	else SetCurrentProcessExplicitAppUserModelID(L"Rainmeter.Rainmeter");
}

// WINDOW STYLE FORCING
static bool SetWindowStyles(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;

	HWND hwnd = m->skinWindow;
	m->isForced = true;

	m->currentStyle = (m->originalStyle & ~WS_POPUP) | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX;

	m->currentExStyle = (m->originalExStyle & ~WS_EX_TOOLWINDOW) | WS_EX_APPWINDOW;

	SetWindowLongPtr(hwnd, GWL_STYLE, m->currentStyle);
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, m->currentExStyle);

	// Sending a show message cancels the default fade-in animation that occurs when setting window styles.
	ShowWindow(hwnd, SW_SHOW);
	AddToTaskbar(m);

	return true;
}

static bool RestoreWindowStyles(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;
	if (!m->isForced) return true;

	HWND hwnd = m->skinWindow;
	SetWindowLongPtr(hwnd, GWL_STYLE, m->originalStyle);
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, m->originalExStyle);
	m->currentStyle = m->originalStyle;
	m->currentExStyle = m->originalExStyle;
	ShowWindow(hwnd, SW_SHOW);

	RemoveFromTaskbar(m);
	return true;
}

// WINDOW STATE TRANSITIONS
static void SetState(Measure* m, WindowState state) {
	if (!m || m->state == state) return;

	m->state = state;
	if (!m->onStateChangeAction.empty() && !m->isUnloading) RmExecute(m->skin, m->onStateChangeAction.c_str());
}

static void PrepareForMinimize(Measure* m) {
	if (!m || !IsWindow(m->skinWindow) || m->state == WindowState::Minimized) return;

	m->bypassNC = true;

	HWND hwnd = m->skinWindow;
	m->haveLastRect = GetWindowRect(hwnd, &m->lastRect) != FALSE;

	LONG_PTR newStyle = m->currentStyle | WS_CAPTION;
	SetWindowLongPtr(hwnd, GWL_STYLE, newStyle);

	ShowWindow(hwnd, SW_SHOW);
}

static void Hide(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return;
	HideWindowContents(m);
	if (m->state == WindowState::Normal) {
		SetState(m, WindowState::Hidden);
		if (!m->onHideAction.empty() && !m->isUnloading) RmExecute(m->skin, m->onHideAction.c_str());
	}
}

static void Show(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return;
	ShowWindowContents(m);
	if (m->state == WindowState::Hidden) {
		SetState(m, WindowState::Normal);
		if (!m->onShowAction.empty() && !m->isUnloading) RmExecute(m->skin, m->onShowAction.c_str());
	}
}

static void PrepareForRestore(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return;
	if (m->state == WindowState::Minimized) Show(m);
}

static void Minimize(Measure* m) {
	PrepareForMinimize(m);
	ShowWindow(m->skinWindow, SW_MINIMIZE);
}

static void Restore(Measure* m) {
	PrepareForRestore(m);
	ShowWindow(m->skinWindow, SW_RESTORE);
}

static void FinishRestore(HWND hwnd) {
	if (!IsWindow(hwnd)) return;
	Measure* m = (Measure*)GetPropW(hwnd, L"Measure");
	RemovePropW(hwnd, L"Measure");
	if (!m) return;

	LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
	style &= ~WS_CAPTION;
	SetWindowLongPtr(hwnd, GWL_STYLE, style);

	if (m && m->haveLastRect) {
		RECT current{};
		GetWindowRect(hwnd, &current);
		if (current.left != m->lastRect.left || current.top != m->lastRect.top || (current.right - current.left) != (m->lastRect.right - m->lastRect.left)
			|| (current.bottom - current.top) != (m->lastRect.bottom - m->lastRect.top)) {
			// Fixes the window drifting position after restoring.
			SetWindowPos(hwnd, nullptr, m->lastRect.left, m->lastRect.top, m->lastRect.right - m->lastRect.left, m->lastRect.bottom - m->lastRect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
		ShowWindow(hwnd, SW_SHOW);
		m->haveLastRect = false;
	}
}

static void Close(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return;
	SendMessage(m->skinWindow, WM_CLOSE, 0, 0);
}

static void Focus(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return;
	HWND hwnd = m->skinWindow;
	SetForegroundWindow(hwnd);
	SetActiveWindow(hwnd);
	SetFocus(hwnd);
}

static void Blur(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return;
	HWND desktop = GetDesktopWindow();
	SetForegroundWindow(desktop);
	SetFocus(desktop);
}

// SKIN WINDOW PROCEDURE
static LRESULT CALLBACK SkinSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
	Measure* m = (Measure*)ref;
	if (!m || m->isUnloading) return DefSubclassProc(hwnd, msg, wp, lp);

	if (m && m->isForced) {
		switch (msg) {
		case WM_SYSCOMMAND: {
			switch (wp & 0xFFF0) {
			case SC_MAXIMIZE:
				if (m->state == WindowState::Minimized) Restore(m);
				return 0;
			case SC_MINIMIZE:
				if (m->state != WindowState::Minimized) Minimize(m);
				return 0;
			case SC_RESTORE:
				if (m->state == WindowState::Minimized) Restore(m);
				return 0;
			}
			return DefSubclassProc(hwnd, msg, wp, lp);
		}
		case WM_NCCALCSIZE: {
			if (wp && m->bypassNC) {
				// Bypass the default non-client area calculation to prevent NC area from being resized.
				auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
				params->rgrc[0] = params->rgrc[1];
				return 0;
			}
			return DefSubclassProc(hwnd, msg, wp, lp);
		}
		case WM_DPICHANGED: {
			UINT newDpi = HIWORD(wp);
			RECT* suggested = reinterpret_cast<RECT*>(lp);
			SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
			return 0;
		}
		case WM_ACTIVATE: {
			if (LOWORD(wp) == WA_ACTIVE || LOWORD(wp) == WA_CLICKACTIVE) {
				if (!m->onFocusAction.empty()) RmExecute(m->skin, m->onFocusAction.c_str());
				return 0;
			}
			else if (LOWORD(wp) == WA_INACTIVE) {
				if (!m->onBlurAction.empty()) RmExecute(m->skin, m->onBlurAction.c_str());
				return 0;
			}
			return DefSubclassProc(hwnd, msg, wp, lp);
		}
		case WM_SIZE: {
			if (m->state != WindowState::Minimized && wp == SIZE_MINIMIZED) {
				SetState(m, WindowState::Minimized);
				Hide(m);
				m->bypassNC = false;
				if (!m->onMinimizeAction.empty() && !m->isUnloading) RmExecute(m->skin, m->onMinimizeAction.c_str());
				return 0;
			}
			else if (m->state == WindowState::Minimized && wp == SIZE_RESTORED) {
				SetState(m, WindowState::Normal);
				if (!m->onRestoreAction.empty()) RmExecute(m->skin, m->onRestoreAction.c_str());
				SetPropW(hwnd, L"Measure", (HANDLE)m);
				SetTimer(m->skinWindow, 1, 100, [](HWND hwnd, UINT, UINT_PTR id, DWORD) {
					KillTimer(hwnd, id);
					FinishRestore(hwnd);
					});
				return 0;
			}
			break;
		}
		case WM_CLOSE: {
			RmExecute(m->skin, L"[!DeactivateConfig]");
			return 0;
		}
		case WM_COMMAND: {
			if (HIWORD(wp) == THBN_CLICKED) {
				UINT id = LOWORD(wp);
				for (const auto& button : m->thumbnailButtons) {
					if (button.id == id) {
						if (!button.command.empty() && !m->isUnloading)
							RmExecute(m->skin, button.command.c_str());
						return 0;
					}
				}
			}
			break;
		}
		}
	}
	return DefSubclassProc(hwnd, msg, wp, lp);
}

// PLUGIN START/STOP
bool StartPlugin(Measure* m) {
	if (!m || m->isUnloading || !IsWindow(m->skinWindow)) return false;
	if (m->isForced) return true;
	if (!SubclassSkin(m)) return false;
	if (!SetWindowStyles(m)) return false;
	SetState(m, WindowState::Normal);
	if (!m->windowTitle.empty()) SetWindowTitle(m, m->windowTitle);
	if (!m->windowIcon.empty()) SetWindowIcon(m, m->windowIcon);
	else SetRainmeterIcon(m);

	if (!m->taskbarTasks.empty()) UpdateTaskbarTasks(m);
	if (!m->thumbnailButtons.empty()) AddTaskbarButtons(m);
	if (!m->taskbarOverlayIcon.empty()) SetTaskbarOverlayIcon(m);

	if (!m->onStartAction.empty() && !m->isUnloading) RmExecute(m->skin, m->onStartAction.c_str());

	return true;
}

bool StopPlugin(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;
	if (!m->isForced) return true;

	if (m->state == WindowState::Minimized) Restore(m);
	RestoreWindowStyles(m);
	RemoveSkinSubclass(m);
	m->isForced = false;
	SetState(m, WindowState::Unknown);
	RestoreWindowTitle(m);
	RestoreWindowIcon(m);
	if (!m->onStopAction.empty() && !m->isUnloading) RmExecute(m->skin, m->onStopAction.c_str());

	return true;
}

// CONFIGURATION LOADING
void LoadConfig(Measure* m, void* rm) {
	if (!m || !rm) return;
	m->debug = RmReadInt(rm, L"Debug", 0) >= 1;

	m->autoStart = RmReadInt(rm, L"AutoStart", 1) >= 1;
	m->windowTitle = RmReadString(rm, L"WindowTitle", m->originalTitle.c_str());
	m->windowIcon = RmReadPath(rm, L"WindowIcon", L"");
	m->thumbnailImageList = RmReadPath(rm, L"ThumbnailImageList", L"");
	m->taskbarOverlayIcon = RmReadPath(rm, L"TaskbarOverlayIcon", L"");
	m->taskbarOverlayDescription = RmReadString(rm, L"TaskbarOverlayDescription", L"");

	m->taskbarProps.appId = RmReadString(rm, L"AppId", L"");
	m->taskbarProps.relaunchCommand = RmReadString(rm, L"TaskbarCommand", L"");
	m->taskbarProps.displayName = RmReadString(rm, L"TaskbarDisplayName", L"");
	m->taskbarProps.iconResource = RmReadPath(rm, L"TaskbarIcon", L"");
	m->taskbarProps.preventPinning = RmReadInt(rm, L"TaskbarPreventPinning", 1) >= 1;

	m->taskbarTasks.clear();

	for (int i = 1;; ++i) {
		std::wstring suffix = NumberSuffix(i);

		std::wstring title = RmReadString(rm, (L"TaskbarTaskTitle" + suffix).c_str(), L"");
		std::wstring command = RmReadString(rm, (L"TaskbarTaskCommand" + suffix).c_str(), L"");

		if (title.empty() || command.empty()) break;

		std::wstring icon = RmReadPath(rm, (L"TaskbarTaskIcon" + suffix).c_str(), L"");

		m->taskbarTasks.push_back({ title, command, icon });
	}

	m->thumbnailButtons.clear();

	for (int i = 1; i <= 7; ++i) {
		std::wstring suffix = NumberSuffix(i);

		std::wstring imageString = RmReadString(rm, (L"ThumbnailButtonIndex" + suffix).c_str(), L"0");
		std::wstring tooltip = RmReadString(rm, (L"ThumbnailButtonToolTip" + suffix).c_str(), L"");
		std::wstring command = RmReadString(rm, (L"ThumbnailButtonAction" + suffix).c_str(), L"", false);
		std::wstring flagsString = RmReadString(rm, (L"ThumbnailButtonFlag" + suffix).c_str(), L"ENABLED");

		if (tooltip.empty()) break;

		UINT id = i;
		UINT image = _wtoi(imageString.c_str());

		if (id == 0) {
			Log(m, LOG_ERROR, L"ThumbnailButton%s has invalid ID", suffix.c_str());
			continue;
		}

		THUMBBUTTONFLAGS flags;
		if (!ParseTaskbarButtonFlags(flagsString, flags)) {
			Log(m, LOG_ERROR, L"ThumbnailButton%s has invalid flags \"%s\"", suffix.c_str(), flagsString.c_str());
			continue;
		}

		TaskbarButton button;
		button.id = id;
		button.image = image;
		button.tooltip = tooltip;
		button.command = command;
		button.flags = flags;

		m->thumbnailButtons.push_back(std::move(button));
	}

	m->onStartAction = RmReadString(rm, L"OnStartAction", L"", false);
	m->onStopAction = RmReadString(rm, L"OnStopAction", L"", false);
	m->onMinimizeAction = RmReadString(rm, L"OnMinimizeAction", L"", false);
	m->onRestoreAction = RmReadString(rm, L"OnRestoreAction", L"", false);
	m->onFocusAction = RmReadString(rm, L"OnFocusAction", L"", false);
	m->onBlurAction = RmReadString(rm, L"OnBlurAction", L"", false);
	m->onStateChangeAction = RmReadString(rm, L"OnStateChangeAction", L"", false);
	m->onHideAction = RmReadString(rm, L"OnHideAction", L"", false);
	m->onShowAction = RmReadString(rm, L"OnShowAction", L"", false);
}

// PLUGIN EXPORTS
PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	*data = nullptr;
	if (!rm) return;

	void* skin = RmGetSkin(rm);
	if (!skin || FindMeasureBySkin(skin)) return;

	auto* m = new Measure;
	m->rm = rm;
	m->skin = skin;
	m->skinWindow = RmGetSkinWindow(rm);

	if (!IsWindow(m->skinWindow)) {
		delete m;
		return;
	}

	AddMeasure(m);
	*data = m;
	m->active = true;
	m->skinName = RmGetSkinName(rm);
	m->exePath = GetExecutablePathW();
	SaveWindowTaskbarProperties(m);

	m->originalStyle = GetWindowLongPtr(m->skinWindow, GWL_STYLE);
	m->currentStyle = m->originalStyle;
	m->originalExStyle = GetWindowLongPtr(m->skinWindow, GWL_EXSTYLE);
	m->currentExStyle = m->originalExStyle;

	int len = GetWindowTextLengthW(m->skinWindow);
	if (len > 0) {
		m->originalTitle.resize(len + 1);
		int actualLen = GetWindowTextW(m->skinWindow, &m->originalTitle[0], static_cast<int>(m->originalTitle.size()));
		m->originalTitle.resize(actualLen);
	}

	m->originalIconBig = (HICON)SendMessageW(m->skinWindow, WM_GETICON, ICON_BIG, 0);
	m->originalIconSmall = (HICON)SendMessageW(m->skinWindow, WM_GETICON, ICON_SMALL, 0);
	setProcessID();
	LoadConfig(m, rm);
	m->autoStartPending = m->autoStart;
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double*) {
	Measure* m = (Measure*)data;
	if (!m || !m->active) return;

	bool wasForced = m->isForced;
	std::wstring oldTitle = m->windowTitle;
	std::wstring oldIcon = m->windowIcon;
	TaskbarIdentity oldTaskbarProps = m->taskbarProps;
	std::vector<TaskbarTask> oldTaskbarTasks = m->taskbarTasks;
	std::wstring oldThumbnailImageList = m->thumbnailImageList;
	std::vector<TaskbarButton> oldThumbnailButtons = m->thumbnailButtons;
	std::wstring oldOverlayIcon = m->taskbarOverlayIcon;
	std::wstring oldOverlayDescription = m->taskbarOverlayDescription;
	ITaskbarList3* taskbarList3 = GetTaskbarList3(m);

	LoadConfig(m, rm);

	if (!wasForced) return;

	bool titleChanged = oldTitle != m->windowTitle;
	bool iconChanged = oldIcon != m->windowIcon;
	bool taskbarPropsChanged = !(oldTaskbarProps == m->taskbarProps);
	bool taskbarTasksChanged = oldTaskbarTasks != m->taskbarTasks;
	bool thumbnailImageListChanged = oldThumbnailImageList != m->thumbnailImageList;
	bool thumbnailButtonsChanged = oldThumbnailButtons != m->thumbnailButtons;
	bool taskbarOverlayChanged = oldOverlayIcon != m->taskbarOverlayIcon;
	bool taskbarOverlayDescChanged = oldOverlayDescription != m->taskbarOverlayDescription;

	if (taskbarPropsChanged) SetWindowTaskbarProperties(m, m->taskbarProps);
	if (taskbarTasksChanged) UpdateTaskbarTasks(m);
	if (thumbnailImageListChanged) SetTaskbarImageList(m);
	if (thumbnailImageListChanged || thumbnailButtonsChanged) UpdateTaskbarButtons(m);
	if (taskbarOverlayChanged || taskbarOverlayDescChanged) SetTaskbarOverlayIcon(m);
	if (titleChanged) SetWindowTitle(m, m->windowTitle);
	if (iconChanged) {
		if (!m->windowIcon.empty()) SetWindowIcon(m, m->windowIcon);
		else SetRainmeterIcon(m);
	}
}

PLUGIN_EXPORT double Update(void* data) {
	Measure* m = (Measure*)data;
	if (!m || !m->active) return -1;

	if (m->autoStartPending) {
		m->autoStartPending = false;
		if (!m->isUnloading) StartPlugin(m);
	}
	if (!m->isForced) return -1;

	switch (m->state) {
	case WindowState::Unknown:   return -1;
	case WindowState::Minimized: return 2;
	case WindowState::Hidden:    return 3;
	default:                     return 1;
	}
}

PLUGIN_EXPORT LPCWSTR GetString(void* data) {
	Measure* m = (Measure*)data;

	if (!m || !m->active || !m->isForced) return L"";
	return m->windowTitle.c_str();
}

PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args) {
	Measure* m = (Measure*)data;
	if (!m || !m->active || !args) return;
	LPCWSTR rest = nullptr;

	// Plugin Commands
	if (_wcsicmp(args, L"Start") == 0) StartPlugin(m);
	else if (_wcsicmp(args, L"Stop") == 0) StopPlugin(m);
	// Window Commands
	else if (!m->isForced) return;
	else if (_wcsicmp(args, L"Minimize") == 0) Minimize(m);
	else if (_wcsicmp(args, L"Restore") == 0) Restore(m);
	else if (_wcsicmp(args, L"ToggleRestore") == 0) m->state == WindowState::Minimized ? Restore(m) : Minimize(m);
	else if (_wcsicmp(args, L"Close") == 0) Close(m);
	else if (_wcsicmp(args, L"Focus") == 0) Focus(m);
	else if (_wcsicmp(args, L"Blur") == 0) Blur(m);
	else if (_wcsicmp(args, L"Hide") == 0 && m->state == WindowState::Normal) Hide(m);
	else if (_wcsicmp(args, L"Show") == 0) m->state == WindowState::Minimized ? Restore(m) : Show(m);
	else if (_wcsicmp(args, L"ToggleShow") == 0) {
		if (m->state == WindowState::Minimized) Restore(m);
		else if (m->state == WindowState::Hidden) Show(m);
		else Hide(m);
	}
	//Taskbar Commands
	else if (ParseCommand(args, L"SetProgressState", rest)) {
		ITaskbarList3* taskbarList3 = GetTaskbarList3(m);
		if (!taskbarList3) return;

		TBPFLAG state = TBPF_NOPROGRESS;
		if (*rest) {
			if (_wcsicmp(rest, L"NoProgress") == 0)          state = TBPF_NOPROGRESS;
			else if (_wcsicmp(rest, L"Indeterminate") == 0)  state = TBPF_INDETERMINATE;
			else if (_wcsicmp(rest, L"Normal") == 0)         state = TBPF_NORMAL;
			else if (_wcsicmp(rest, L"Error") == 0)          state = TBPF_ERROR;
			else if (_wcsicmp(rest, L"Paused") == 0)         state = TBPF_PAUSED;
			else {
				switch (_wtoi(rest)) {
				case 0: state = TBPF_NOPROGRESS;    break;
				case 1: state = TBPF_INDETERMINATE; break;
				case 2: state = TBPF_NORMAL;        break;
				case 3: state = TBPF_ERROR;         break;
				case 4: state = TBPF_PAUSED;        break;
				default:
					Log(m, LOG_ERROR, L"SetProgressState: unknown state \"%s\"", rest);
					return;
				}
			}
		}
		HRESULT hr = taskbarList3->SetProgressState(m->skinWindow, state);
		if (FAILED(hr)) Log(m, LOG_ERROR, L"SetProgressState failed (HRESULT: 0x%08X)", static_cast<unsigned>(hr));
	}
	else if (ParseCommand(args, L"SetProgressValue", rest)) {
		ITaskbarList3* taskbarList3 = GetTaskbarList3(m);
		if (!taskbarList3) return;

		wchar_t buffer[256];
		wcsncpy_s(buffer, rest, _TRUNCATE);

		wchar_t* context = nullptr;
		wchar_t* tokCompleted = wcstok_s(buffer, L" ", &context);
		wchar_t* tokTotal = tokCompleted ? wcstok_s(nullptr, L" ", &context) : nullptr;

		ULONGLONG completed = tokCompleted ? _wcstoui64(tokCompleted, nullptr, 10) : 0;
		ULONGLONG total = tokTotal ? _wcstoui64(tokTotal, nullptr, 10) : 100;
		if (total == 0) total = 1;

		HRESULT hr = taskbarList3->SetProgressValue(m->skinWindow, completed, total);
		if (FAILED(hr)) Log(m, LOG_ERROR, L"SetProgressValue failed (HRESULT: 0x%08X)", static_cast<unsigned>(hr));
	}
}

PLUGIN_EXPORT void Finalize(void* data) {
	Measure* m = (Measure*)data;
	if (!m) return;

	m->isUnloading = true;
	StopPlugin(m);
	m->active = false;
	RemoveMeasure(m);

	delete m;
}