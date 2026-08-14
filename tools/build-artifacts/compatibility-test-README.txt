Absolute Control Panel
Pause Menu Compatibility Test

PURPOSE
This pre-release package tests Absolute Control Panel as a native Starfield
configuration menu. It adds an ABSOLUTE CONTROL PANEL row to the vanilla pause
menu at runtime without replacing pausemenu.swf or requiring an ESM.

REQUIREMENTS
- A Starfield runtime listed as verified by the project release
- The matching SFSE release
- Address Library for SFSE Plugins for that runtime

INSTALL WITH MOD ORGANIZER 2
1. Install this ZIP as a mod.
2. Enable it after SFSE and Address Library.
3. Launch the game through sfse_loader.

QUICK TEST
1. Load a save and press Escape.
2. Select ABSOLUTE CONTROL PANEL at the bottom of the pause menu.
3. Confirm the panel opens and its controls can be changed.
4. Close it, reopen the pause menu, and confirm the vanilla menu and panel row
   are still present.
5. F2 is the fallback panel hotkey.

COMPATIBILITY REPORT
Record whether another mod changes the pause menu, whether the panel row appears,
whether every vanilla row still works, and whether F2 still opens the panel.

FAIL-SAFE / ROLLBACK
To disable pause-menu integration while retaining F2 access, set
EnablePauseMenuEntry=false in SFSE\Plugins\AbsoluteControlPanel.ini.
To remove the test, disable or uninstall this MO2 mod. The integration creates
no save-game data.

No controller output, virtual-controller automation, or title-screen automation
is enabled by this package.
