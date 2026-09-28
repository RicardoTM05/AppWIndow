# AppWindow Plugin
Make your skins behave as normal windows that are availabe on the taskbar.

<img width="600" height="400" alt="AppWindow" src="https://github.com/user-attachments/assets/8725bbf5-596f-4a4b-8326-1af347c07626" />


Download on the Rainmeter forums:
[Download](https://forum.rainmeter.net/viewtopic.php?t=46032)

## Features

* Set custom window title and icon
* Show window in taskbar
* Working minimize/restore animations
* Closing the window unloads the skin
* Working Alt+Tab, Win+Tab and Win+Up/Down hotkeys
* Commands to Start and Stop the plugin
* Commands to Minimize and Restore the window
* Commands to Show and Hide the window
* Commands to Focus and Blur the window
* Actions to react to window state changes
* Window grouping
* Support for Taskbar Tasks
* Support for Taskbar Overlay Icons (Badges)
* Support for Taskbar Thumbnail Buttons
* Support for Taskbar Progress bars

## Usage

Add the plugin as a measure:

```ini
; Drop this measure into any skin. Modify options as needed.
; The measure will return -1 when stopped, 1 when the window is visible, 
; 2 when minimized and 3 when hidden.
; Returns the window title as string while active.

[AppWindow]
Measure=Plugin
Plugin=AppWindow

; Plugin Options
AutoStart=1

; Window Options
WindowTitle=Your window title
WindowIcon=#@#YourWindowIcon.ico

; Use dynamic variables if you plan on changing options dynamically.
;DynamicVariables=1

; Taskbar Options
AppId=#CURRENTCONFIG#
TaskbarPreventPinning=-1
;-- These options are only availabe when TaskbarPreventPinning=-1
TaskbarDisplayName=#CURRENTCONFIG#
TaskbarCommand=[!ActivateConfig #CURRENTCONFIG#]
TaskbarIcon=#@#YourTaskbarIcon.ico
;--

;Taskbar Tasks Options (Up to 13 tasks)
TaskbarTaskTitle=Edit skin
TaskbarTaskCommand=[!EditSkin #CURRENTCONFIG#]
TaskbarTaskIcon=#@#CustomTaskIcon.ico
TaskbarTaskTitle2=Refresh skin
TaskbarTaskCommand2=[!Refresh #CURRENTCONFIG#]
TaskbarTaskIcon2=#@#CustomTaskIcon2.ico

;Taskbar Overlay Icon Options (a 16x16 icon)
TaskbarOverlayIcon=#@#YourOverlayIcon.ico
TaskbarOverlayDescription=Status

;Taskbar Thumbnail Button Options (Up to 7 buttons)
ThumbnailImageList=#@#ThumbnailButtons.bmp
ThumbnailButtonIndex=0
ThumbnailButtonToolTip=Play
ThumbnailButtonFlag=ENABLED
ThumbnailButtonAction=[Play #@#Sound.wav]
ThumbnailButtonIndex2=1
ThumbnailButtonToolTip2=Stop
ThumbnailButtonFlag2=ENABLED
ThumbnailButtonAction2=[PlayStop]

; Actions
OnStateChangeAction=[!UpdateMeasure #CURRENTSECTION#]
OnStartAction=[]
OnStopAction=[]
OnFocusAction=[]
OnBlurAction=[]
OnHideAction=[]
OnShowAction=[]
OnMinimizeAction=[]
OnRestoreAction=[]

;Plugin Commands
; [!CommandMeasure AppWindow Start]
; [!CommandMeasure AppWindow Stop]
;Window Commands
; [!CommandMeasure AppWindow Minimize]
; [!CommandMeasure AppWindow Restore]
; [!CommandMeasure AppWindow ToggleRestore]
; [!CommandMeasure AppWindow Hide]
; [!CommandMeasure AppWindow Show]
; [!CommandMeasure AppWindow ToggleShow]
; [!CommandMeasure AppWindow Close]
; [!CommandMeasure AppWindow Focus]
; [!CommandMeasure AppWindow Blur]
;Taskbar Commands
; [!CommandMeasure AppWindow "SetProgressState 0"] - 0|1|2|3|4 or NoProgress|Indeterminate|Normal|Error|Paused
; [!CommandMeasure AppWindow "SetProgressValue 50"] - range from 0-100.
; [!CommandMeasure AppWindow "SetProgressValue 50 250"] - range from 0-250.
```

### Measure Values

The measure will return the current window state as its number value.
| Value    | Meaning   | Description                     |
| -------  | --------: | ------------------------------- |
| `-1`     | Stopped   | The plugin is stopped.			 |
| `1`      | Normal    | Window is visible.              |
| `2`      | Minimized | Window is minimized.	         |
| `3`      | Hidden    | Window is hidden.	             |

 The measure will return the window title as string value while active                            

### Options

| Option                                                  	                           |  Default  | Description                                                                                                      |
| ----------------------------------------------------------------------------------- | :-------: | ---------------------------------------------------------------------------------------------------------------- |
| `AutoStart`                                             	                           |    `1`    | Automatically start the plugin.                                                                                  |
| `WindowTitle`                                           	                           |    `""`   | Custom window title. Defaults to the original window title.                                                      |
| `WindowIcon`                                            	                           |    `""`   | Custom `.ico` file path. Defaults to the Rainmeter icon.                                                         |
| `AppId`                                                 	                           |    `""`   | AppUserModelID used to identify the window in the taskbar.                                                       |
| `TaskbarPreventPinning`                                 	                           |    `1`    | Controls taskbar pinning behavior.																				                                                           |
| `TaskbarDisplayName`                                    	                           |    `""`   | Display name used for the taskbar item. Only used when `TaskbarPreventPinning` is disabled.                      |
| `TaskbarCommand`                                        	                           |    `""`   | Comm0and used to relaunch the taskbar item. Only used when `TaskbarPreventPinning` is disabled.                  |
| `TaskbarIcon`                                           	                           |    `""`   | Custom `.ico` file path for the taskbar item. Only used when `TaskbarPreventPinning` is disabled.                |
| `TaskbarTaskTitle` ... `TaskbarTaskTitle2` ... `TaskbarTaskTitle13`                 |    `""`   | Title of the corresponding taskbar task. Up to 13 tasks are supported.                                           |
| `TaskbarTaskCommand` ... `TaskbarTaskCommand2` ... `TaskbarTaskCommand13`           |    `""`   | Command executed when the corresponding taskbar task is clicked.                                                 |
| `TaskbarTaskIcon` ... `TaskbarTaskIcon2` ... `TaskbarTaskIcon13`                    |    `""`   | Custom `.ico` file path for the corresponding taskbar task.                                                      |
| `TaskbarOverlayIcon`                                                                |    `""`   | Custom `.ico` file path for the taskbar overlay icon (badge).                                                    |
| `TaskbarOverlayDescription`                                                         |    `""`   | Accessibility description for the taskbar overlay icon.                                                          |
| `ThumbnailImageList`                                                                |    `""`   | Path to 32-bit `.bmp` image containing the images used by taskbar thumbnail buttons.                             |
| `ThumbnailButtonIndex` ... `ThumbnailButtonIndex2` ... `ThumbnailButtonIndex7`      |    `0`    | Zero-based image index used by the corresponding thumbnail button.                                               |
| `ThumbnailButtonToolTip` ... `ThumbnailButtonToolTip2` ... `ThumbnailButtonToolTip7`|    `""`   | Tooltip displayed for the corresponding thumbnail button.                                                        |
| `ThumbnailButtonFlag` ... `ThumbnailButtonAction2` ... `ThumbnailButtonAction7`     | `ENABLED` | Flags controlling the state of the corresponding thumbnail button.                                               |
| `ThumbnailButtonAction` ... `ThumbnailButtonAction2` ... `ThumbnailButtonAction7`   |    `""`   | Rainmeter action executed when the corresponding thumbnail button is clicked.                                    |


>Important:
>>`Command` options are command line arguuments and thus all bangs require the config parameter.
>>`Action` options do not require the config paramenter.

### Thumbnail Button Flags

`ThumbnailButtonFlag` and its numbered variants accept the following values:

| Flag             | Description                                       |
| ---------------- | ------------------------------------------------- |
| `ENABLED`        | Enables the button.                               |
| `DISABLED`       | Disables the button.                              |
| `DISMISSONCLICK` | Closes the preview when clicked.                  |
| `NOBACKGROUND`   | Hides the button background.                      |
| `HIDDEN`         | Hides the button.                                 |
| `NONINTERACTIVE` | Displays the button without allowing interaction. |

Multiple flags can be combined using `|`.
Example: `ThumbnailButtonFlag=DISABLED | NOBACKGROUND | DISMISSONCLICK`

### Actions

| Action               | Description                                        |
| ------------------   | -------------------------------------------------- |
| `OnStateChangeAction`| Action executed whenever the window state changes. |
| `OnStartAction`      | Action executed when started.                      |
| `OnStopAction`       | Action executed when stopped.                      |
| `OnFocusAction`      | Action executed when the window gains focus.       |
| `OnBlurAction`       | Action executed when the window loses focus.       |
| `OnMinimizeAction`   | Action executed when minimized.                    |
| `OnRestoreAction`    | Action executed when restored from minimized.      |
| `OnHideAction`       | Action executed when hidden.                       |
| `OnShowAction`       | Action executed when shown.                        |

### Commands

```ini
;Plugin Commands
[!CommandMeasure AppWindow Start]
[!CommandMeasure AppWindow Stop]
;Window Commands
[!CommandMeasure AppWindow Minimize]
[!CommandMeasure AppWindow Restore]
[!CommandMeasure AppWindow ToggleRestore]
[!CommandMeasure AppWindow Hide]
[!CommandMeasure AppWindow Show]
[!CommandMeasure AppWindow ToggleShow]
[!CommandMeasure AppWindow Close]
[!CommandMeasure AppWindow Focus]
[!CommandMeasure AppWindow Blur]
;Taskbar Commands
[!CommandMeasure AppWindow "SetProgressState 0"] - 0|1|2|3|4 or NoProgress|Indeterminate|Normal|Error|Paused
[!CommandMeasure AppWindow "SetProgressValue 50"] - range from 0-100.
[!CommandMeasure AppWindow "SetProgressValue 50 250"] - range from 0-250.
```
