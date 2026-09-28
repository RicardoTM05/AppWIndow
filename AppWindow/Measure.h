#pragma once

#include <Windows.h>
#include <shobjidl.h>
#include "dwmapi.h"
#include <propkey.h>
#include <propvarutil.h>
#include <vector>
#include <string>
#include <memory>
#include <type_traits>
#include <wincodec.h>
#include "../API/RainmeterAPI.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

// Pointer helpers
struct IconDeleter {
	void operator()(HICON icon) const {
		if (icon) DestroyIcon(icon);
	}
};
using UniqueIcon = std::unique_ptr<std::remove_pointer_t<HICON>, IconDeleter>;

template <typename T>
struct ComDeleter {
	void operator()(T* p) const { if (p) p->Release(); }
};
template <typename T>
using ComPtr = std::unique_ptr<T, ComDeleter<T>>;

// Taskbar configuration structs
struct TaskbarIdentity {
	std::wstring appId, relaunchCommand, displayName, iconResource;
	bool preventPinning = true;

	bool operator==(const TaskbarIdentity& other) const {
		return appId == other.appId && relaunchCommand == other.relaunchCommand && displayName == other.displayName
			&& iconResource == other.iconResource && preventPinning == other.preventPinning;
	}
};

struct TaskbarTask {
	std::wstring title, command, icon;

	bool operator==(const TaskbarTask& other) const {
		return title == other.title && command == other.command && icon == other.icon;
	}
};

struct TaskbarButton {
	UINT id = 0, image = 0;
	std::wstring tooltip, command;
	THUMBBUTTONFLAGS flags = THBF_ENABLED;

	bool operator==(const TaskbarButton& other) const {
		return id == other.id && image == other.image && tooltip == other.tooltip && command == other.command && flags == other.flags;
	}
};

enum class WindowState {
	Normal, Minimized, Hidden, Unknown
};

// Measure
struct Measure {
	void* rm = nullptr;
	void* skin = nullptr;
	HWND skinWindow = nullptr;
	std::wstring skinName, exePath, originalTitle;
	HICON originalIconBig = nullptr, originalIconSmall = nullptr;
	UniqueIcon currentIconBig, currentIconSmall;
	LONG_PTR originalStyle = 0, originalExStyle = 0, currentStyle = 0, currentExStyle = 0;
	RECT lastRect{};
	TaskbarIdentity originalTaskbarProps, taskbarProps;
	std::vector<TaskbarTask> taskbarTasks;
	std::vector<TaskbarButton> thumbnailButtons;
	HIMAGELIST thumbnailImageListHandle = nullptr;
	bool isForced = false, active = false, isSubclassed = false, isUnloading = false, autoStartPending = false;
	bool haveLastRect = false, bypassNC = false, hadOriginalTaskbarProps = false, hadOriginalPreventPinning = false;

	bool autoStart = false, debug = false;
	std::wstring thumbnailImageList, windowTitle, windowIcon, taskbarOverlayIcon, taskbarOverlayDescription;
	std::wstring onStartAction, onStopAction, onMinimizeAction, onRestoreAction, onFocusAction, onBlurAction, onHideAction, onShowAction, onStateChangeAction;

	WindowState state = WindowState::Unknown;
};

// Logging (defined in Utilities.cpp)
void Log(Measure* m, int type, const wchar_t* format, ...);

// String / path / icon utilities (defined in Utilities.cpp)
std::wstring BuildAppId(const std::wstring& appId);
std::wstring MakeFileNameSafe(std::wstring name);
std::wstring GetExecutablePathW();
std::wstring NumberSuffix(int i);
bool ParseIconResource(const std::wstring& resource, std::wstring& path, int& index);

std::wstring GetEffectiveTaskbarIcon(Measure* m);
std::wstring GetEffectiveTaskIcon(Measure* m, const TaskbarTask& task);

bool SetWindowTitle(Measure* m, const std::wstring& title);
bool RestoreWindowTitle(Measure* m);

bool SetRainmeterIcon(Measure* m);
bool SetWindowIcon(Measure* m, const std::wstring& path);
bool RestoreWindowIcon(Measure* m);

// Taskbar / Shell integration (defined in Taskbar.cpp)
ITaskbarList3* GetTaskbarList3(Measure* m);
void ReleaseTaskbarLists();

bool SetWindowTaskbarProperties(Measure* m, const TaskbarIdentity& id);
bool SaveWindowTaskbarProperties(Measure* m);

bool UpdateTaskbarTasks(Measure* m);

bool ParseTaskbarButtonFlags(const std::wstring& value, THUMBBUTTONFLAGS& flags);
bool SetTaskbarImageList(Measure* m);
bool AddTaskbarButtons(Measure* m);
bool UpdateTaskbarButtons(Measure* m);

bool SetTaskbarOverlayIcon(Measure* m);

bool AddToTaskbar(Measure* m);
bool RemoveFromTaskbar(Measure* m);