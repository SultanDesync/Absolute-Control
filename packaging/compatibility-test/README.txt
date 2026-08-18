Starfield Local Options Panel (SLOP)
Pause Menu Compatibility Test 0.1.0

PURPOSE
This pre-release build tests SLOP as a native Starfield configuration menu and
adds a STARFIELD LOCAL OPTIONS PANEL row to the vanilla pause menu at runtime.
It does not replace pausemenu.swf and does not require an ESM.

REQUIREMENTS
- Starfield runtime 1.16.244
- SFSE for that runtime
- Address Library for SFSE Plugins for that runtime

INSTALL WITH MOD ORGANIZER 2
1. Install this ZIP as a mod.
2. Enable the mod after SFSE and Address Library.
3. Launch the game through sfse_loader.

QUICK TEST
1. Load a save and press Escape.
2. Select STARFIELD LOCAL OPTIONS PANEL at the bottom of the pause menu.
3. Confirm SLOP opens and its controls can be changed.
4. Close SLOP, reopen the pause menu, and confirm both the vanilla menu and the
   SLOP row are still present.
5. Confirm no standalone panel hotkey is registered by default. For a focused
   recovery-path test only, set OpenHotkey=0x71 and test F2.

COMPATIBILITY REPORT
Please record whether another mod changes the pause menu, whether the SLOP row
appears, whether every vanilla row still works, and whether the default
configuration leaves F2 available to other mods.

MANUAL FAILURE CAPTURE
For the most useful cold-start trace, load a save and open the pause menu as soon
as gameplay becomes responsive. If the SLOP row is missing, leave the pause menu
open for three seconds, then close and reopen it. Repeat three cycles. Enable an
explicit recovery hotkey only when that separate path is under test.
Under Mod Organizer 2, the focused diagnostic trace is written to:
  <MO2 instance>\overwrite\SFSE\Plugins\
  AbsoluteControlPanelResearch.evidence.jsonl
Outside MO2, it uses Data\SFSE\Plugins with the normal SFSE log directory as a
fallback.
It records pause-menu readiness changes and injection decisions without recording
continuous mouse/controller activity. Logs rotate at 8 MiB.

FAIL-SAFE / ROLLBACK
To keep SLOP but disable pause-menu integration, edit:
  SFSE\Plugins\AbsoluteControlPanelResearch.ini
and set:
  EnablePauseMenuEntry=false

To remove the test completely, disable or uninstall this MO2 mod. No save-game
data is created by the pause-menu integration.

NOTES
- AbsoluteControlPanelResearch is the temporary internal filename of this
  pre-release SLOP host.
- No controller automation, virtual-controller output, or title-screen input
  automation is enabled in this package.
