#include "Measure.h"

// MODULE-LOCAL COM TASKBAR LIST CACHE
static ComPtr<ITaskbarList> g_taskbarList;
static ComPtr<ITaskbarList3> g_taskbarList3;

void ReleaseTaskbarLists() {
	g_taskbarList3.reset();
	g_taskbarList.reset();
}

static ITaskbarList* GetTaskbarList(Measure* m) {
	if (!m) return nullptr;

	if (!g_taskbarList) {
		ITaskbarList* rawList = nullptr;
		if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&rawList)))) {
			Log(m, LOG_ERROR, L"Failed to create ITaskbarList instance");
			return nullptr;
		}
		g_taskbarList.reset(rawList);

		if (FAILED(g_taskbarList->HrInit())) {
			g_taskbarList.reset();
			Log(m, LOG_ERROR, L"Failed to initialize ITaskbarList instance");
			return nullptr;
		}
	}
	return g_taskbarList.get();
}

ITaskbarList3* GetTaskbarList3(Measure* m) {
	if (!m) return nullptr;

	if (!g_taskbarList3) {
		ITaskbarList* baseList = GetTaskbarList(m);
		if (!baseList) return nullptr;

		ITaskbarList3* raw = nullptr;
		HRESULT hr = baseList->QueryInterface(IID_PPV_ARGS(&raw));
		if (FAILED(hr)) {
			Log(m, LOG_ERROR, L"Failed to query ITaskbarList3 (HRESULT: 0x%08X)", static_cast<unsigned>(hr));
			return nullptr;
		}
		g_taskbarList3.reset(raw);
	}
	return g_taskbarList3.get();
}

// PROPVARIANT / PROPERTY STORE UTILITIES
static HRESULT SetPropVariantString(IPropertyStore* pStore, const PROPERTYKEY& key, const wchar_t* value) {
	if (!value || !*value) return S_OK;

	PROPVARIANT pv;
	HRESULT hr = InitPropVariantFromString(value, &pv);
	if (SUCCEEDED(hr)) {
		hr = pStore->SetValue(key, pv);
		PropVariantClear(&pv);
	}
	return hr;
}

static HRESULT SetPropVariantBool(IPropertyStore* pStore, const PROPERTYKEY& key, bool value) {
	PROPVARIANT pv;
	HRESULT hr = InitPropVariantFromBoolean(value, &pv);

	if (SUCCEEDED(hr)) {
		hr = pStore->SetValue(key, pv);
		PropVariantClear(&pv);
	}

	return hr;
}

static std::wstring GetPropVariantString(IPropertyStore* pStore, const PROPERTYKEY& key) {
	PROPVARIANT pv;
	PropVariantInit(&pv);

	std::wstring result;
	if (SUCCEEDED(pStore->GetValue(key, &pv))) {
		if (pv.vt == VT_LPWSTR && pv.pwszVal)
			result = pv.pwszVal;
		PropVariantClear(&pv);
	}
	return result;
}

static bool GetPropVariantBool(IPropertyStore* pStore, const PROPERTYKEY& key, bool& value) {
	PROPVARIANT pv;
	PropVariantInit(&pv);

	HRESULT hr = pStore->GetValue(key, &pv);

	if (SUCCEEDED(hr) && pv.vt == VT_BOOL) {
		value = (pv.boolVal == VARIANT_TRUE);
		PropVariantClear(&pv);
		return true;
	}

	PropVariantClear(&pv);
	return false;
}

static HRESULT ClearProperty(IPropertyStore* pStore, const PROPERTYKEY& key) {
	PROPVARIANT empty;
	PropVariantInit(&empty);
	return pStore->SetValue(key, empty);
}

// TASKBAR IDENTITY PROPERTIES
bool SetWindowTaskbarProperties(Measure* m, const TaskbarIdentity& id) {
	if (!m) return false;

	IPropertyStore* rawStore = nullptr;
	HRESULT hr = SHGetPropertyStoreForWindow(m->skinWindow, IID_PPV_ARGS(&rawStore));
	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"SHGetPropertyStoreForWindow failed HRESULT: 0x%08X hwnd=%p",
			static_cast<unsigned>(hr), m->skinWindow);
		return false;
	}
	ComPtr<IPropertyStore> pStore(rawStore);

	const wchar_t* failedStep = nullptr;

	auto trySet = [&](const wchar_t* step, const PROPERTYKEY& key, const std::wstring& value) {
		if (FAILED(hr) || value.empty()) return;
		hr = SetPropVariantString(pStore.get(), key, value.c_str());
		if (FAILED(hr)) failedStep = step;
		};

	std::wstring appId = BuildAppId(id.appId);
	std::wstring relaunchCommand;

	if (id.relaunchCommand.empty()) {
		std::wstring rootConfig = RmReplaceVariables(m->rm, L"[#ROOTCONFIG]");
		relaunchCommand = L"\"" + m->exePath + L"\" [!ActivateConfig \"" + rootConfig + L"\"]";
	}
	else relaunchCommand = L"\"" + m->exePath + L"\" " + id.relaunchCommand;

	std::wstring iconResource = GetEffectiveTaskbarIcon(m);
	std::wstring displayName = MakeFileNameSafe(id.displayName);

	if (SUCCEEDED(hr)) {
		hr = SetPropVariantBool(pStore.get(), PKEY_AppUserModel_PreventPinning, id.preventPinning);
		if (FAILED(hr)) failedStep = L"AppUserModel_PreventPinning";
	}
	trySet(L"AppUserModel_ID", PKEY_AppUserModel_ID, appId);
	if (id.preventPinning) {
		if (SUCCEEDED(hr)) hr = ClearProperty(pStore.get(), PKEY_AppUserModel_RelaunchCommand);
		if (SUCCEEDED(hr)) hr = ClearProperty(pStore.get(), PKEY_AppUserModel_RelaunchDisplayNameResource);
		if (SUCCEEDED(hr)) hr = ClearProperty(pStore.get(), PKEY_AppUserModel_RelaunchIconResource);
	}
	else {
		trySet(L"AppUserModel_RelaunchCommand", PKEY_AppUserModel_RelaunchCommand, relaunchCommand);
		trySet(L"AppUserModel_RelaunchDisplayNameResource", PKEY_AppUserModel_RelaunchDisplayNameResource, displayName);
		trySet(L"AppUserModel_RelaunchIconResource", PKEY_AppUserModel_RelaunchIconResource, iconResource);
	}

	if (SUCCEEDED(hr)) {
		hr = pStore->Commit();
		if (FAILED(hr)) failedStep = L"Commit";
	}

	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"%s failed HRESULT:0x%08X", failedStep, static_cast<unsigned>(hr));
	}

	return SUCCEEDED(hr);
}

static bool ClearWindowTaskbarProperties(Measure* m) {
	if (!m) return false;

	IPropertyStore* rawStore = nullptr;
	HRESULT hr = SHGetPropertyStoreForWindow(m->skinWindow, IID_PPV_ARGS(&rawStore));
	if (FAILED(hr)) return false;
	ComPtr<IPropertyStore> pStore(rawStore);

	PROPVARIANT pvEmpty;
	PropVariantInit(&pvEmpty);

	const PROPERTYKEY* keys[] = {
		&PKEY_AppUserModel_ID,
		&PKEY_AppUserModel_RelaunchCommand,
		&PKEY_AppUserModel_RelaunchDisplayNameResource,
		&PKEY_AppUserModel_RelaunchIconResource,
		&PKEY_AppUserModel_PreventPinning
	};

	for (const auto* key : keys) {
		hr = pStore->SetValue(*key, pvEmpty);
		if (FAILED(hr)) break;
	}
	if (SUCCEEDED(hr)) hr = pStore->Commit();

	return SUCCEEDED(hr);
}

bool SaveWindowTaskbarProperties(Measure* m) {
	IPropertyStore* rawStore = nullptr;
	HRESULT hr = SHGetPropertyStoreForWindow(m->skinWindow, IID_PPV_ARGS(&rawStore));
	if (FAILED(hr)) return false;
	ComPtr<IPropertyStore> pStore(rawStore);

	m->originalTaskbarProps.appId = GetPropVariantString(pStore.get(), PKEY_AppUserModel_ID);
	m->originalTaskbarProps.relaunchCommand = GetPropVariantString(pStore.get(), PKEY_AppUserModel_RelaunchCommand);
	m->originalTaskbarProps.displayName = GetPropVariantString(pStore.get(), PKEY_AppUserModel_RelaunchDisplayNameResource);
	m->originalTaskbarProps.iconResource = GetPropVariantString(pStore.get(), PKEY_AppUserModel_RelaunchIconResource);
	m->hadOriginalPreventPinning = GetPropVariantBool(pStore.get(), PKEY_AppUserModel_PreventPinning, m->originalTaskbarProps.preventPinning);
	m->hadOriginalTaskbarProps = true;

	return true;
}

static bool RestoreWindowTaskbarProperties(Measure* m) {
	if (!m->hadOriginalTaskbarProps) return ClearWindowTaskbarProperties(m);

	bool result = SetWindowTaskbarProperties(m, m->originalTaskbarProps);

	if (result && !m->hadOriginalPreventPinning) {
		IPropertyStore* rawStore = nullptr;
		HRESULT hr = SHGetPropertyStoreForWindow(m->skinWindow, IID_PPV_ARGS(&rawStore));
		if (SUCCEEDED(hr)) {
			ComPtr<IPropertyStore> pStore(rawStore);
			PROPVARIANT empty;
			PropVariantInit(&empty);

			hr = pStore->SetValue(PKEY_AppUserModel_PreventPinning, empty);
			if (SUCCEEDED(hr)) hr = pStore->Commit();

			result = SUCCEEDED(hr);
		}
	}
	return result;
}

// JUMP LIST TASKS
bool UpdateTaskbarTasks(Measure* m) {
	if (!m || m->taskbarProps.appId.empty()) return false;

	std::wstring appId = BuildAppId(m->taskbarProps.appId);

	ICustomDestinationList* rawList = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawList));
	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"Failed to create ICustomDestinationList (HRESULT: 0x%08X)", static_cast<unsigned>(hr));
		return false;
	}
	ComPtr<ICustomDestinationList> destinationList(rawList);

	bool listBegun = false;
	auto Fail = [&](const wchar_t* fmt) {
		Log(m, LOG_ERROR, fmt, static_cast<unsigned>(hr));
		if (listBegun) destinationList->AbortList();
		return false;
		};

	hr = destinationList->SetAppID(appId.c_str());
	if (FAILED(hr)) return Fail(L"ICustomDestinationList::SetAppID failed (HRESULT: 0x%08X)");

	UINT maxSlots = 0;
	IObjectArray* rawRemoved = nullptr;
	hr = destinationList->BeginList(&maxSlots, IID_PPV_ARGS(&rawRemoved));
	if (FAILED(hr)) return Fail(L"ICustomDestinationList::BeginList failed (HRESULT: 0x%08X)");
	ComPtr<IObjectArray> removed(rawRemoved);
	listBegun = true;

	if (m->taskbarTasks.empty()) {
		hr = destinationList->CommitList();
		if (FAILED(hr)) return Fail(L"Failed to clear Jump List tasks (HRESULT: 0x%08X)");
		return true;
	}

	IObjectCollection* rawTasks = nullptr;
	hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawTasks));
	if (FAILED(hr)) return Fail(L"Failed to create IObjectCollection (HRESULT: 0x%08X)");
	ComPtr<IObjectCollection> tasks(rawTasks);

	for (const auto& task : m->taskbarTasks) {
		IShellLinkW* rawLink = nullptr;
		hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawLink));
		if (FAILED(hr)) return Fail(L"Failed to create IShellLink (HRESULT: 0x%08X)");
		ComPtr<IShellLinkW> link(rawLink);

		hr = link->SetPath(m->exePath.c_str());
		if (SUCCEEDED(hr)) hr = link->SetArguments(task.command.c_str());


		if (SUCCEEDED(hr)) {
			std::wstring icon = GetEffectiveTaskIcon(m, task);
			std::wstring iconPath;
			int iconIndex = 0;
			ParseIconResource(icon, iconPath, iconIndex);
			hr = link->SetIconLocation(iconPath.c_str(), iconIndex);
		}

		if (SUCCEEDED(hr)) {
			IPropertyStore* rawStore = nullptr;
			hr = link->QueryInterface(IID_PPV_ARGS(&rawStore));
			if (SUCCEEDED(hr)) {
				ComPtr<IPropertyStore> propertyStore(rawStore);
				hr = SetPropVariantString(propertyStore.get(), PKEY_Title, task.title.c_str());
			}
		}

		if (SUCCEEDED(hr)) hr = tasks->AddObject(link.get());

		if (FAILED(hr)) {
			Log(m, LOG_ERROR, L"Failed to create Jump List task \"%s\" (HRESULT: 0x%08X)",
				task.title.c_str(), static_cast<unsigned>(hr));
			destinationList->AbortList();
			return false;
		}
	}

	IObjectArray* rawTaskArray = nullptr;
	hr = tasks->QueryInterface(IID_PPV_ARGS(&rawTaskArray));
	if (SUCCEEDED(hr)) {
		ComPtr<IObjectArray> taskArray(rawTaskArray);
		hr = destinationList->AddUserTasks(taskArray.get());
	}
	if (FAILED(hr)) return Fail(L"Failed to add Jump List tasks (HRESULT: 0x%08X)");

	hr = destinationList->CommitList();
	if (FAILED(hr)) Log(m, LOG_ERROR, L"ICustomDestinationList::CommitList failed (HRESULT: 0x%08X)", static_cast<unsigned>(hr));

	return SUCCEEDED(hr);
}

// THUMBNAIL BUTTONS & IMAGE LIST
static bool LoadThumbnailImageList(Measure* m) {
	if (!m || m->thumbnailImageList.empty()) return false;

	if (m->thumbnailImageListHandle) {
		ImageList_Destroy(m->thumbnailImageListHandle);
		m->thumbnailImageListHandle = nullptr;
	}
	HBITMAP hBitmap = (HBITMAP)LoadImageW(nullptr, m->thumbnailImageList.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

	if (!hBitmap) {
		Log(m, LOG_ERROR, L"Failed to load taskbar image \"%s\" (error: %lu)", m->thumbnailImageList.c_str(), GetLastError());
		return false;
	}

	BITMAP bm{};
	GetObject(hBitmap, sizeof(bm), &bm);
	DeleteObject(hBitmap);

	if (bm.bmHeight <= 0 || bm.bmWidth <= 0 || (bm.bmWidth % bm.bmHeight) != 0) {
		Log(m, LOG_ERROR, L"Taskbar image \"%s\" is not a valid strip of square icons (%dx%d)", m->thumbnailImageList.c_str(), bm.bmWidth, bm.bmHeight);
		return false;
	}

	const int iconSize = bm.bmHeight;

	HIMAGELIST imageList = ImageList_LoadImageW(nullptr, m->thumbnailImageList.c_str(), iconSize, 0, CLR_NONE, IMAGE_BITMAP, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

	if (!imageList) {
		Log(m, LOG_ERROR, L"Failed to load taskbar image list \"%s\" (error: %lu)", m->thumbnailImageList.c_str(), GetLastError());
		return false;
	}

	m->thumbnailImageListHandle = imageList;
	return true;
}

bool SetTaskbarImageList(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;

	ITaskbarList3* taskbarList3 = GetTaskbarList3(m);
	if (!taskbarList3) return false;

	if (!LoadThumbnailImageList(m)) return false;

	HRESULT hr = taskbarList3->ThumbBarSetImageList(m->skinWindow, m->thumbnailImageListHandle);
	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"ThumbBarSetImageList failed (HRESULT: 0x%08X)", static_cast<unsigned>(hr));
		return false;
	}
	return true;
}

static bool ParseSingleTaskbarButtonFlag(const std::wstring& token, unsigned long& outValue) {
	if (_wcsicmp(token.c_str(), L"ENABLED") == 0) {
		outValue = THBF_ENABLED;
		return true;
	}
	if (_wcsicmp(token.c_str(), L"DISABLED") == 0) {
		outValue = THBF_DISABLED;
		return true;
	}
	if (_wcsicmp(token.c_str(), L"DISMISSONCLICK") == 0) {
		outValue = THBF_DISMISSONCLICK;
		return true;
	}
	if (_wcsicmp(token.c_str(), L"NOBACKGROUND") == 0) {
		outValue = THBF_NOBACKGROUND;
		return true;
	}
	if (_wcsicmp(token.c_str(), L"HIDDEN") == 0) {
		outValue = THBF_HIDDEN;
		return true;
	}
	if (_wcsicmp(token.c_str(), L"NONINTERACTIVE") == 0) {
		outValue = THBF_NONINTERACTIVE;
		return true;
	}

	wchar_t* end = nullptr;
	unsigned long numeric = wcstoul(token.c_str(), &end, 0);

	if (end != token.c_str() && *end == L'\0') {
		outValue = numeric;
		return true;
	}

	return false;
}

bool ParseTaskbarButtonFlags(const std::wstring& value, THUMBBUTTONFLAGS& flags) {
	if (value.empty()) {
		flags = THBF_ENABLED;
		return true;
	}

	constexpr unsigned long validFlags = THBF_ENABLED | THBF_DISABLED | THBF_DISMISSONCLICK | THBF_NOBACKGROUND | THBF_HIDDEN | THBF_NONINTERACTIVE;
	unsigned long combined = 0;
	size_t start = 0;

	while (start <= value.size()) {
		size_t sep = value.find(L'|', start);
		size_t end = (sep == std::wstring::npos) ? value.size() : sep;

		size_t tokenStart = start;
		size_t tokenEnd = end;
		while (tokenStart < tokenEnd && iswspace(value[tokenStart])) ++tokenStart;
		while (tokenEnd > tokenStart && iswspace(value[tokenEnd - 1])) --tokenEnd;

		if (tokenStart == tokenEnd)  return false;

		std::wstring token = value.substr(tokenStart, tokenEnd - tokenStart);

		unsigned long tokenValue = 0;
		if (!ParseSingleTaskbarButtonFlag(token, tokenValue)) return false;

		combined |= tokenValue;

		if (sep == std::wstring::npos) break;
		start = sep + 1;
	}

	if ((combined & ~validFlags) != 0) return false;

	flags = static_cast<THUMBBUTTONFLAGS>(combined);
	return true;
}

static void DestroyTaskbarImageList(Measure* m) {
	if (!m || !m->thumbnailImageListHandle) return;

	ImageList_Destroy(m->thumbnailImageListHandle);
	m->thumbnailImageListHandle = nullptr;
}

bool AddTaskbarButtons(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;
	if (!SetTaskbarImageList(m)) return false;

	ITaskbarList3* taskbarList3 = GetTaskbarList3(m);
	if (!taskbarList3) return false;

	std::vector<THUMBBUTTON> buttons;
	buttons.reserve(m->thumbnailButtons.size());

	for (const auto& config : m->thumbnailButtons) {
		THUMBBUTTON button{};
		button.dwMask = THB_FLAGS | THB_TOOLTIP | THB_BITMAP;
		button.iId = config.id;
		button.iBitmap = config.image;
		button.dwFlags = config.flags;
		wcsncpy_s(button.szTip, config.tooltip.c_str(), _TRUNCATE);
		buttons.push_back(button);
	}

	HRESULT hr = taskbarList3->ThumbBarAddButtons(m->skinWindow, static_cast<UINT>(buttons.size()), buttons.data());
	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"ThumbBarAddButtons failed (HRESULT: 0x%08X)", static_cast<unsigned>(hr));
		return false;
	}

	return true;
}

bool UpdateTaskbarButtons(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;

	ITaskbarList3* taskbarList3 = GetTaskbarList3(m);
	if (!taskbarList3) return false;

	std::vector<THUMBBUTTON> buttons;
	buttons.reserve(m->thumbnailButtons.size());

	for (const auto& config : m->thumbnailButtons) {
		THUMBBUTTON button{};
		button.dwMask = THB_FLAGS | THB_TOOLTIP | THB_BITMAP;
		button.iId = config.id;
		button.iBitmap = config.image;
		button.dwFlags = config.flags;
		wcsncpy_s(button.szTip, config.tooltip.c_str(), _TRUNCATE);
		buttons.push_back(button);
	}

	HRESULT hr = taskbarList3->ThumbBarUpdateButtons(m->skinWindow, static_cast<UINT>(buttons.size()), buttons.data());

	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"ThumbBarUpdateButtons failed (HRESULT: 0x%08X)", static_cast<unsigned>(hr));
		return false;
	}

	return true;
}

// TASKBAR OVERLAY ICON
bool SetTaskbarOverlayIcon(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;

	ITaskbarList3* taskbarList3 = GetTaskbarList3(m);
	if (!taskbarList3) return false;

	HICON hIcon = nullptr;

	if (!m->taskbarOverlayIcon.empty()) {
		hIcon = static_cast<HICON>(LoadImageW(nullptr, m->taskbarOverlayIcon.c_str(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE));

		if (!hIcon) {
			Log(m, LOG_ERROR, L"Failed to load taskbar overlay icon \"%s\" (error: %lu)", m->taskbarOverlayIcon.c_str(), GetLastError());
			return false;
		}
	}

	HRESULT hr = taskbarList3->SetOverlayIcon(m->skinWindow, hIcon, m->taskbarOverlayDescription.empty() ? nullptr : m->taskbarOverlayDescription.c_str());

	if (hIcon) DestroyIcon(hIcon);

	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"SetOverlayIcon failed (HRESULT: 0x%08X)", static_cast<unsigned>(hr));
		return false;
	}

	return true;
}

// TASKBAR TAB REGISTRATION
bool AddToTaskbar(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;

	if (!m->taskbarProps.appId.empty()) SetWindowTaskbarProperties(m, m->taskbarProps);

	ITaskbarList* taskbarList = GetTaskbarList(m);
	if (!taskbarList) return false;

	HRESULT hr = taskbarList->AddTab(m->skinWindow);

	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"AddTab failed (HWND: %p, HRESULT: 0x%08X)", m->skinWindow, hr);
		return false;
	}

	return true;
}

bool RemoveFromTaskbar(Measure* m) {
	if (!m || !IsWindow(m->skinWindow)) return false;

	ITaskbarList* taskbarList = GetTaskbarList(m);
	if (!taskbarList) return false;

	HRESULT hr = taskbarList->DeleteTab(m->skinWindow);

	if (FAILED(hr)) {
		Log(m, LOG_ERROR, L"DeleteTab failed (HWND: %p, HRESULT: 0x%08X)", m->skinWindow, hr);
		return false;
	}

	DestroyTaskbarImageList(m);

	if (!m->taskbarProps.appId.empty()) RestoreWindowTaskbarProperties(m);

	return true;
}