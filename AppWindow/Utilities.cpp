#include "Measure.h"

// LOGGING
void Log(Measure* m, int type, const wchar_t* format, ...) {
	if (type == LOG_DEBUG && m && !m->debug) return;

	va_list args;
	va_start(args, format);
	wchar_t buffer[1024];
	_vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
	va_end(args);

	RmLogF(m ? m->rm : nullptr, type, L"AppWindow: %s", buffer);
}

// STRING / PATH UTILITIES
std::wstring BuildAppId(const std::wstring& appId) {
	if (appId.empty()) return L"Rainmeter.Rainmeter";

	std::wstring sanitized = appId;
	for (auto& ch : sanitized) {
		if (ch == L'\\') ch = L'.';
	}

	return L"Rainmeter.Rainmeter." + sanitized;
}

std::wstring MakeFileNameSafe(std::wstring name) {
	if (name.empty()) return L"";

	for (auto& ch : name) {
		switch (ch) {
		case L'\\': case L'/': case L':': case L'*':
		case L'?':  case L'"': case L'<': case L'>': case L'|':
			ch = L'_';
			break;
		default:
			if (ch < 0x20) ch = L'_';
			break;
		}
	}

	size_t end = name.find_last_not_of(L". ");
	if (end == std::wstring::npos) name.clear();
	else  name.erase(end + 1);

	static const wchar_t* reserved[] = {
		L"CON", L"PRN", L"AUX", L"NUL",
		L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
		L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"
	};
	std::wstring base = name.substr(0, name.find(L'.'));
	for (auto& r : base) r = towupper(r);
	for (auto* r : reserved) {
		if (base == r) {
			name = L"_" + name;
			break;
		}
	}
	if (name.empty()) name = L"_";

	return name;
}

std::wstring GetExecutablePathW() {
	std::vector<wchar_t> buffer(MAX_PATH);
	DWORD length = GetModuleFileNameW(NULL, buffer.data(), (DWORD)buffer.size());

	while (length == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
		buffer.resize(buffer.size() * 2);
		length = GetModuleFileNameW(NULL, buffer.data(), (DWORD)buffer.size());
	}

	return std::wstring(buffer.data(), length);
}

std::wstring NumberSuffix(int i) {
	if (i == 1) return L"";

	wchar_t buffer[16];
	swprintf_s(buffer, L"%d", i);
	return buffer;
}

bool ParseIconResource(const std::wstring& resource, std::wstring& path, int& index) {
	path = resource;
	index = 0;

	size_t comma = resource.rfind(L',');
	if (comma == std::wstring::npos) return true;

	const wchar_t* indexString = resource.c_str() + comma + 1;
	wchar_t* end = nullptr;
	long value = wcstol(indexString, &end, 10);

	if (*indexString == L'\0' || *end != L'\0') return true;

	path = resource.substr(0, comma);
	index = static_cast<int>(value);
	return true;
}

// WINDOW TITLE HELPERS
bool SetWindowTitle(Measure* m, const std::wstring& title) {
	return m && IsWindow(m->skinWindow) && SetWindowTextW(m->skinWindow, title.c_str());
}

bool RestoreWindowTitle(Measure* m) {
	return m && IsWindow(m->skinWindow) && SetWindowTextW(m->skinWindow, m->originalTitle.c_str());
}

// ICON HELPERS
static std::wstring GetProcessIconResource(Measure* m) {
	return m ? m->exePath + L",0" : L"";
}

static std::wstring GetEffectiveWindowIcon(Measure* m) {
	if (!m) return L"";
	if (!m->windowIcon.empty()) return m->windowIcon;

	return GetProcessIconResource(m);
}

std::wstring GetEffectiveTaskbarIcon(Measure* m) {
	if (!m) return L"";
	if (!m->taskbarProps.iconResource.empty()) return m->taskbarProps.iconResource;

	return GetEffectiveWindowIcon(m);
}

std::wstring GetEffectiveTaskIcon(Measure* m, const TaskbarTask& task) {
	if (!m) return L"";
	if (!task.icon.empty()) return task.icon;

	return GetEffectiveTaskbarIcon(m);
}

static void ApplyIcon(HWND hwnd, WPARAM iconType, UniqueIcon& storedIcon, HICON newIcon) {
	if (!newIcon) return;

	SendMessageW(hwnd, WM_SETICON, iconType, (LPARAM)newIcon);
	storedIcon.reset(newIcon);
}

bool SetRainmeterIcon(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;

	WCHAR path[MAX_PATH]{};
	if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) {
		Log(m, LOG_ERROR, L"GetModuleFileNameW failed (error: %lu)", GetLastError());
		return false;
	}

	HICON iconBig = nullptr, iconSmall = nullptr;
	UINT result = ExtractIconExW(path, 0, &iconBig, &iconSmall, 1);

	if (result == 0 || result == UINT_MAX) {
		Log(m, LOG_ERROR, L"ExtractIconExW failed (path: %s, error: %lu)", path, GetLastError());
		return false;
	}

	ApplyIcon(m->skinWindow, ICON_BIG, m->currentIconBig, iconBig);
	ApplyIcon(m->skinWindow, ICON_SMALL, m->currentIconSmall, iconSmall);

	return iconBig || iconSmall;
}

bool SetWindowIcon(Measure* m, const std::wstring& path) {
	if (!m || !IsWindow(m->skinWindow) || path.empty()) return false;

	HICON iconBig = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
	HICON iconSmall = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);

	if (!iconBig && !iconSmall) {
		Log(m, LOG_ERROR, L"Failed to load icon from path: %s", path.c_str());
		SetRainmeterIcon(m);
		return false;
	}

	ApplyIcon(m->skinWindow, ICON_BIG, m->currentIconBig, iconBig);
	ApplyIcon(m->skinWindow, ICON_SMALL, m->currentIconSmall, iconSmall);

	return true;
}

bool RestoreWindowIcon(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;

	SendMessageW(m->skinWindow, WM_SETICON, ICON_BIG, (LPARAM)m->originalIconBig);
	SendMessageW(m->skinWindow, WM_SETICON, ICON_SMALL, (LPARAM)m->originalIconSmall);

	m->currentIconBig.reset();
	m->currentIconSmall.reset();

	return true;
}