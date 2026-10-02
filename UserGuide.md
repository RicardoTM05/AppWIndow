# USER GUIDE

Content
* [Basic Usage](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#basic-usage)
  * [Minimizing a skin](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#minimizing-a-skin)
  * [Hiding a skin](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#hiding-a-skin)
  * [Plugin measure values](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#plugin-measure-values)
  * [Actions](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#actions)
* [Setting a window title and icon](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#setting-a-window-title-and-icon)
* [Converting a skin into an "app"](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#converting-a-skin-into-an-app)
  * [Introducing AppId](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#introducing-appid)
  * [Grouping multiple skins](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#grouping-multiple-skins)
* [Customizing the taskbar icon](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#customizing-the-taskbar-icon)
  * [Allow pinning](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#allow-pinning)
  * [Customizing the .lnk file](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#customizing-the-lnk-file)
* [Taskbar features](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#taskbar-features)
  * [Tasks](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#tasks)
  * [Overlay Icons](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#overlay-icons)
  * [Progress Bars](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#progress-bars)
* [Thumbnail features](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#thumbnail-features)
  * [Thumbnail Buttons](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#thumbnail-buttons)
    * [Setting up an image list](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#setting-up-an-image-list)
    * [Displaying Thumbnail Buttons](https://github.com/RicardoTM05/AppWIndow/blob/main/UserGuide.md#displaying-thumbnail-buttons)

## Basic Usage
The most basic use of this plugin is this:

```ini
[AppWindow]
Measure=Plugin
Plugin=AppWindow
```

This will simply make your skin behave as a normal window, it can now be minimized, restored and will show on the taskbar, it will show on Rainmeter's own taskbar group (will show along the Debug and Manage windows) and will use Rainmeter's icon for both the taskbar and the window.

The skin will also be selectable using `ALT+TAB` or `WIN+TAB`

### Minimizing a skin
You can minimize the skin as any other window, either click on its icon on the taskbar, or press `WIN+DOWN` and `WIN+UP` to restore it.
You can also use commands:

```ini
[!CommandMeasure AppWindow Minimize]
[!CommandMeasure AppWindow Restore]
[!CommandMeasure AppWindow ToggleRestore]
```

### Hiding a skin
If you attempt to hide the skin using the `[!Hide]` bang, you'll notice it will dissapear from the taskbar.

To avoid that from happening, use these commands instead:

```ini
[!CommandMeasure AppWindow Hide]
[!CommandMeasure AppWindow Show]
[!CommandMeasure AppWindow ToggleShow]
```

### Plugin measure values
The measure returns a number value and a string. The number represents the window state while the string the current window title.

States are:
| Value    | Meaning   | Description                     |
| -------  | --------: | ------------------------------- |
| `-1`     | Stopped   | The plugin is stopped.			 |
| `1`      | Normal    | Window is visible.              |
| `2`      | Minimized | Window is minimized.	         |
| `3`      | Hidden    | Window is hidden.	             |

In some situations you may want to react to a change of state, that's where actions can be useful.

### Actions
These are all the actions available:

| Action               | Description                                  |
| ------------------   | -------------------------------------------- |
| `OnStateChangeAction`| The plugin state changed.                    |
| `OnStartAction`      | The plugin started.                          |
| `OnStopAction`       | The plugin stopped.                          |
| `OnFocusAction`      | The window gained focus.                     |
| `OnBlurAction`       | The window lost focus.                       |
| `OnMinimizeAction`   | The window was minimized.                    |
| `OnRestoreAction`    | The window was restored from minimized.      |
| `OnHideAction`       | The window was hidden.                       |
| `OnShowAction`       | The window was shown.                        |

`OnStateChangeAction` can be used to self-update the measure and use `ifConditions` to react to its own value. Especially useful when using `Update=-1`.
For example:

`OnStateChangeAction=[!UpdateMeasure #CURRENTSECTION#]`

Additionally you can react to specific events such as focusing or blurring the window.


## Setting a window title and icon
You may want your window to look like a real window, to get there you will need to set a custom title, and maybe a custom icon as well.

```ini
[AppWindow]
Measure=Plugin
Plugin=AppWindow
WindowTitle=My window title
WindowIcon=#@#MyWindowIcon.ico
```
>Rules
> * The icon must be `.ico`. Make sure it is big enough for it to look nice on higher dpi's. 128x128 may be enough for a window icon.
> * Both `WindowTitle` and `WindowIcon` can be changed dynamically (Requires `DynamicVariables=1`).

> Be aware!
>* The window icon may show as the taskbar icon, this is normal Windows behavior.

If you read the caution above, you may be wonderig.. "What if I want a custom taskbar icon?".
Well, keep reading.

## Converting a skin into an "app"
Yeah, you may want a custom taskbar icon, but that's not really what we'll do.

We will basically give our skin its own space on the taskbar. So now the skin won't group with Rainmeter windows, it will now have its own space instead.
To do that, we need to fool windows into thinking our skin window belongs to a different app, that's where `AppUserModelID` (or `AppId` for short) comes into play.

### Introducing AppId
Windows groups windows into taskbar "icons" or "buttons". To do so, it relies on multiple heuristics to infer where in the taskbar a window belongs. To save resources Windows introduced `AppUserModelID`, which is what it now uses to identify a certain app, its `.lnk` shortcuts and windows.
For short, we call it just `AppId` on the plugin.

An `AppId` is basically a string, and it can only be `128` characters long.

It's format, according to Microsoft, is this:

`CompanyName.ProductName.SubProduct.VersionInformation`

being the `CompanyName.ProductName` part "obligatory".

Since Rainmeter's process lacks an AppUserModelID, the plugin will automatically set Rainmeter's process level ID to `Rainmeter.Rainmeter`. This is done so Rainmeter's Debug and Manage windows always group under their correct taskbar icon.
To always comply with Microsoft's requiremets, the plugin's `AppId` option always takes Rainmeter's `AppId` as the `CompanyName.ProductName` part.

So, for example:

`AppId=AppWindow` is in reality `Rainmeter.Rainmeter.AppWindow`

Since the app id can only be `128` characters long, and the first part is already taking `20` characters, `AppId` can only be `108` characters long.

Again, this is important:

>Attention:
>* `AppId` can only be `108` characters long.

### Grouping multiple skins
As already mentioned, `AppId` makes Windows think your skin window belongs to an "app". This means that you can set the same `AppId` on multiple skins and they will all group together on the taskbar.

To make it easier, you may want to create the `[AppWindow]` measure on a shared `.inc` file, for example:

`AppWindow.inc`
```ini
[AppWindow]
Measure=Plugin
Plugin=AppWindow
WindowTitle=#WindowTitle#
WindowIcon=#WindowIcon#
AppId=#ROOTCONFIG#
```
then include it on all skins and set the Variables:

```ini
@include=#@#AppWindow.inc

[Variables]
WindowTitle=My window title
WindowIcon=#@#MyWindowIcon.ico
```

If `#ROOTCONFIG#` is `AppWindow`, then all skins IDs will be `Rainmeter.Rainmeter.AppWindow` and they will all share the same taskbar button.

>Note:
>* When using `#CURRENTCONFIG#` on `AppId`, `\` will be automatically replaced with a `.`, so `AppWindow\ProgressBar` will become `AppWindow.ProgressBar` automatically. 

## Customizing the taskbar icon
It's important to understand that the "taskbar icon" doesn't belong to a given skin window, but to the whole "fake app".
By default, Windows will use whatever icon it finds, in this case it will be the Window's icon. For real apps, it is normally based on the `.exe` file.
Since our fake app doesn't have any `.exe`, Windows will automatically create a `.lnk` file which will use for taskbar functions. This `.lnk` file will be automatically created once the user right clicks the taskbar icon.
It can then be found inside a folder at the following location:

`%appdata%\Microsoft\Internet Explorer\Quick Launch\User Pinned\ImplicitAppShortcuts`

However, this will only happen if it's allowed to be pinned to the taskbar, because that file is what will be pinned.

So before customizing our taskbar icon, first we need to allow it to be pinned.

### Allow pinning
To allow pinning is as simple as setting `TaskbarPreventPinning=-1`, but wait, please don't do it just yet.

As already mentioned, an actual `.lnk` file will be created, so we want to set its properties first. Otherwise you will need to delete the `.lnk` file and restart `explorer.exe` to make changes.

### Customizing the .lnk file
The `.lnk` file will require a title, an icon, and a command (what happens when you "launch" it).

>Note:
> * The `.lnk` shortcut file is created by Windows, the plugins has anything to do with when or where that file is created.

So, we'll set the following options:

```ini
TaskbarPreventPinning=-1
TaskbarDisplayName=MyAppName
TaskbarIcon=#@#MyTaskbarIcon.ico
TaskbarCommand=[!ActivateConfig "MyConfig"]
```
Now you can refresh the skin, you will immediately notice it now uses `MyTaskbarIcon.ico` and, if you right click it, you'll see the option to pin to the taskbar. You will also see another entry that says `MyAppName`, if you click it will then activate `MyConfig`.
Maybe you noticed that Windows "stole" your first right click, that's because it created the `.lnk` file.
As previously mentioned, this file was created at the `ImplicitAppShortcuts` location.
The file will be called `MyAppName.lnk` and will use the icon we set.
If you hold `Alt` and click it to open the properties panel, you'll see as target:

`"C:\Program Files\Rainmeter\Rainmeter.exe" [!ActivateConfig "MyConfig"]`

So when you launch it, Rainmeter will execute the bang.

As you may probably noticed already, the `TaskbarCommand` option is in fact a [command line argument](https://docs.rainmeter.net/manual/bangs/#CommandLineArguments), this means it is executed from outside Rainmeter, which means that it needs to follow Rainmeter's own recommendations:
>Many bangs have a `Config` parameter. Unless otherwise specified, valid values are the [config name](https://docs.rainmeter.net/manual/skins/#Config) of a currently loaded skin to be acted upon or `*` (asterisk) to act on all currently loaded skins. When optional and not supplied, the parameter defaults to the current config. If executing a bang with a "config" parameter from the Windows command line, the parameter is always required.
>>* This applies to all `Command` options.

Remember that for suites, these options are for the whole "app". So if multiple skins are using the same `AppId`, then all skins should share these options to the same value.

>Notes
>* If no valid icon is given, the `.lnk` file and the taskbar will use Rainmeter's icon by default.
>* To make changes to these options, it is necessary to **manually** delete the `.lnk` file and restart `explorer.exe`. It may need to be restarted a couple times before changes are visible. For this reason, `TaskbarDisplayName`, `TaskbarIcon` and `TaskbarCommand` are not meant to be changed dynamically.
>* `TaskbarDisplayName` will be used for the `.lnk` name, for this reason, it has to be a file name safe string. It will automatically replace any invalid characters with `_`. Thus, `AppWindow\ProgressBar` will be automatically converted to `AppWindow_ProgressBar`.

So far our measure looks like this:

`AppWindow.inc`
```ini
[AppWindow]
Measure=Plugin
Plugin=AppWindow

WindowTitle=#WindowTitle#
WindowIcon=#WindowIcon#

AppId=#ROOTCONFIG#

TaskbarPreventPinning=-1
TaskbarDisplayName=#ROOTCONFIG#
TaskbarIcon=#@#MyTaskbarIcon.ico
TaskbarCommand=[!ActivateConfig "#ROOTCONFIG#"]
```

## Taskbar features
Additionally, there are a few other nice features for the taskbar:

* Tasks
* Overlay Icons
* Progress Bars

### Tasks
When you right click a taskbar button, a *JumpList* will open. `Tasks` are entries on this list that allow you to set additional commands that are executed when the user clicks on them. Think of it like a Taskbar Custom Context Menu.

The implementation is pretty much the same as setting a Rainmeter Custom Context Menu, with 2 key differences: You can set a different icon for each entry, and they're also command line arguments, this means they're also executed from outside Rainmeter.

> Rules
> * There can only be a maximum of  `13` entries for Windows 11, and `11` for Windows 10. Additional entries will not be displayed.
> * If no valid icon is set, they will use either the taskbar icon, or the window icon, or else, the Rainmeter icon.
> * Commands are CLI arguments.
> * Just as all the other taskbar options, these entries are shared by all skins using the same `AppId`, although unlike the others, these can be set dynamically without issues.
> * If more than one skin that uses the same `AppId` set Tasks, only the last will be displayed. For this reason, it is recommended to also set these options on an included file.

With that being said, the options are:

```ini
TaskbarTaskTitle=Your Task Title
TaskbarTaskIcon=#@#TaskIcon.ico
TaskbarTaskCommand=[!SomeBang "Some\Config"]
TaskbarTaskTitle2=Your Task2 Title
TaskbarTaskIcon2=#@#Task2Icon.ico
TaskbarTaskCommand2=[!SomeOtherBang "Some\Other\Config"]
...
...
TaskbarTaskTitle13=Your Task13 Title
TaskbarTaskIcon13=#@#Task13Icon.ico
TaskbarTaskCommand13=[!SomeOtherBang "Some\Other\Config"]
```

### Overlay Icons
These are small `16x16px` icons also popularly called "badges" which are displayed on top of the taskbar icon.
They're particularly useful to indicate states or notifications.

> Rules
> * The icon must be 16x16 pixels.
> * The icon can be set dynamically.
> * Setting `TaskbarOverlayIcon=""` removes the icon.
> * Overlays are also shared by all skins using the same `AppId`.
   
Setting one is pretty easy:

```ini
TaskbarOverlayIcon=Overlay_Running.ico
TaskbarOverlayDescription=Running
```

Setting a description is recommended by Microsoft for accessibility reasons, it won't display anywhere though.

### Progress Bars
You may have noticed a little grey bar right under the taskbar icon. Well, that little grey bar can work as a progress bar, which turns yellow when "paused" and red when "error". It also features an "Indeterminate" animation.

To control the progress bar you simply set a value and a state through commands. 

> Rules
> * When a value is set, the bar will automatically go from the `NoProgress` state to the `Normal` state.
> * To reset the bar, manually set `NoProgress` state.
> * When on `Paused` state, the bar will turn yellow, and red when on `Error` state. These colors are set by Windows and can't be changed.
> * The `Indeterminate` state will play an "idle" animation which will stop as soon as a value is set.

States:
 * `NoProgress`
 * `Indeterminate`
 * `Normal`
 * `Error`
 * `Paused`

Commands:
* `SetProgressState state`
* `SetProgressValue value range`

For example, to set a value, you use the `SetProgressValue value range`

This will set the bar to 50% (50/100)
`[!CommandMeasure AppWindow "SetProgressValue 50"]`

This will set the bar to 30% (50/150)
`[!CommandMeasure AppWindow "SetProgressValue 50 150"]`

To change the bar to a "paused" state, you send the `SetProgressState state` command:

`[!CommandMeasure AppWindow "SetProgressState Paused"]`

## Thumbnail features
The thumbnail is the little window preview that is displayed when hovering over a taskbar button. The plugin offers a nice feature for it:

### Thumbnail Buttons
This is a pretty nice feature for media players. It allows you to display up to `7` functional buttons on the window's thumbnail preview.

> Rules
> * Up to `7` buttons.
> * A .bmp image list must be provided.
> * Actions are NOT command line arguments.
> * The buttons can be set, changed and removed dynamically.
> * Buttons are window independent, so different buttons can be set to different skins even if they share the same AppId.

#### Setting up an image list
An image list is an indexed bitmap that contains multiple icons arranged horizontally.

To set the image list, simply add this option to the measure:

`ThumbnailImageList=#@#MyThumbnailButtons.bmp`

Since the image list is an indexed set of icons, you can now choose which icons to display using that index.

#### Displaying Thumbnail Buttons
Display a button is easy, just set the right index for the button, add a tooltip, a flag and an action. Indices are 0 based.

Button Flags:
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

Simply set the following options:

```ini
ThumbnailImageList=#@#MyThumbnailButtons.bmp

ThumbnailButtonIndex=0
ThumbnailButtonToolTip=Play
ThumbnailButtonFlag=ENABLED
ThumbnailButtonAction=[Play #@#Sound.wav]

ThumbnailButtonIndex2=1
ThumbnailButtonToolTip2=Stop
ThumbnailButtonFlag2=ENABLED
ThumbnailButtonAction2=[PlayStop]
...
...
ThumbnailButtonIndex7=
ThumbnailButtonToolTip7=
ThumbnailButtonFlag7=ENABLED
ThumbnailButtonAction7=[]
```

>Notes
> * `ThumbnailButtonFlag` defaults to ENABLED, so it is not necessary to be set all the time.
> * You can have multiple buttons stored on the same bmp and only display those that are needed at the time.
