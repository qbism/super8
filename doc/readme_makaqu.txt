==============
    Makaqu
==============

Title    : Makaqu
Filename : Makaqu1.3.zip
Version  : 1.3
Date     : March/23/2008
Author   : Manoel Kasimier
Email    : manoelkasimier@yahoo.com.br
Website  : http://quakedev.dcemulation.org/fragger/



====================
    Introduction
====================

This is the Makaqu engine, a modified Quake engine for PC and Sega Dreamcast. The DC version is called nxMakaqu.

First of all, I'd like to warn there are some things missing in this readme, because I don't have enough time to do a decent job of updating it. Sorry for the inconvenience. The list of changes covers most relevant changes though.

The Dreamcast version is called nxMakaqu because it's the sucessor of nxQuake (a port of the Quake engine to the Sega Dreamcast). Aside from the name and the Dreamcast-specific features, like VMU support, it is identical to the PC version of Makaqu. The source code is the same for both the PC and Dreamcast versions.

This is a software-renderer-only engine. There are several great hardware-accelerated engines out there, and I wanted to try making a good software-based one.

There's a good number of features that were implemented thanks to the tutorials at quakesrc.org, and some that were ported from other engines. Also, when adding code from other sources I studied them carefully, fixing bugs, looking for better ways to implement them, and sometimes improving them.

The following tutorials from the Quake Standards Group were implemented:
Rotating brushes (MrG)
Fixing the incorrect bounding boxes on rotating bmodel objects (LordHavoc)
enabling new EF_ effects (Kryten)
Frames Per Second Display (muff) - modified a bit
max_fps cvar (MrG)
QC Alpha Scale Glow (Tomaz) - modified a lot. Glow accepts negative values, and .alpha & .scale works in static entities.

I've implemented the skybox support and alias model interpolation from the version 1.11a of the ToChriS engine, and made heavy modifications to them.

Most of the code is marked with comments, so it shouldn't be hard to port most of the changes to any other engine.

=========================
    Credits 'n stuff:
=========================

Programming:
- Manoel Kasimier ( manoelkasimier@yahoo.com.br )

Port to the Dreamcast, and translucent & additive colormaps:
- BlackAura ( badcdev@gmail.com )

Beta testing:
- Tyne/RenegadeC ( jsnipez@hotmail.com )
- DCmad/MasterMan ( masterman194@hotmail.com )
- Christuserloeser ( Christuserloeser@dcevolution.net )

Engines I've got code from:
- Darkplaces
- QIP_Quake
- Quake II
- ToChriS

Thanks to LordHavoc (lordhavoc@ghdigital.com) for tips on how to change the angles to 16 bit.

Important note:
- I'd like to mention here some special people. Their positive, respectful and grateful attitude is an honor for the DC scene.
- Here they are: Wraggster, souLLy, ace, Ender, Butters and Christuserloeser.
- Also Tyne, DCmad, TheDumbAss, Moi, Metafox, |darc|, and the rest of the people at DCEmulation.
- And the staff at DCEvolution for their motivation and hard work.

Finally, a very special "Thank You" for Christuserloeser, LyingWake and BlackAura. Thanks to them I didn't quit from the DC scene.



=====================
    Misc changes:
=====================

- Reduced the size of the config file (makaqu.cfg).
- Independent config file (makaqu.cfg). When loading the file config.cfg, if the file makaqu.cfg exists the engine will load it instead. And now the engine uses the file makaqu.cfg to save the settings.
- The message "execing file.ext" is a developer message now.
- Removed the message "VERSION 1.00 SERVER (%i CRC)" upon map loading.
- Various crashes changed to host errors.
- Added a FPS display and a cl_showfps cvar to toggle it.
- Added auto-repeat for the keys while in the console.
- Added the commands cmdlist, cvarlist, cvarlist_a and cvarlist_s.
- cvarlist_a lists the cvars which are saved in the config file, and cvarlist_s list the cvars used by the server.
- cvarlist, cvarlist_a and cvarlist_s shows the attributes of the variables, as well as their current value.
- cmdlist, cvarlist, cvarlist_a and cvarlist_s outputs the lists in alphabetical order, and accepts the beginning of the names of the commands/cvars as a parameter.
- Added cvars to control the size and position of the screen, and an "Adjust screen" menu to set them.



=========================
    Gameplay changes:
=========================

- Added horizontal autoaim, and a sv_aim_h cvar. Set the "Aim Assistance" option to "on" to enable it, or to "vertical" to use vertical autoaim only. The vertical autoaim is less effective when the aim assistance is set to "on".
- Added support to up to 16 clients by default.
- created "map transition lists" for normal game and deathmatch. It works like a maplist, by redirecting the changelevel command. The list for normal game is "maps\spmaps.txt" and the deathmatch list is "maps\dmmaps.txt". The contents of these lists should be as follows:

Each line contains the name of three maps. When you're playing in the first map and the game issues a changelevel to go to the second map, the changelevel will be redirected to the third map. Example:

start e1m1 e1m7 // When you're in the map "start" and tries to go to the map "e1m1", the game will load the map "e1m7"

Notes:
- Each line must contain three names of maps, without extension.
- Lines started with a double slash ("//") will be ignored.

Wildcard notes:
- You can replace the second name with an asterisk. It's useful for things like this:

start e1m1 e1m7 // if you try to go from the map "start" to the map "e1m1", you will go to the map "e1m7"
start * e1m8    // if you try to go from the map "start" to any other map, you will go to the map "e1m8"



==============================
    Command-input changes:
==============================

- Replaced the +mlook command with a m_look cvar.
- Mouse sensitivity is scaled according to the FOV.
- Up to 256 keys/buttons can be bound for each command in the key config menu.
- Now you can unbind one key/button at each time in the key config menu if the command has multiple keys/buttons bound to it.
- Added the commands "menu_keys_clear" and "menu_keys_addcmd" so the key config menu can be fully customized.
- Added a "bindable" command to define which keys/buttons shouldn't have their bindings changed.

- Created a "function shift" system, similar to the L trigger in Soldier of Fortune for Dreamcast. Added commands: +shift, bindshift, unbindshift, unbindallshifts.
Example: if you enter "bind DC_TRIGL +shift;bind DC_TRIGR +attack;bindshift DC_TRIGR +jump" at the console, the R trigger will make the player shot, but if you hold the L trigger and press the R trigger, the player will jump.
- Added support for shifted keys to the key config menu.



============================
    Save system changes:
============================

- Fixed the initial value of the serverflags being not included in the saves. Due to this, saves created by Makaqu aren't compatible with saves created by other engines.
- Optimized the save system to reduce the filesize of the saves. The optimized saves can sometimes reach up to half of their normal size.
- The save menu supports up to 100 saves now.
- Now the save menu waits for confirmation from the user before overwriting savegames.
- Added an info panel to the save/load menus. It displays the name of the map (ex: E1M1), the skill, the elapsed time, the number of enemies (killed/total) and the amount of secrets (found/total).
- Added a "savename" cvar to set the filename of the saves. Its default value is "QUAKE___". The extension of the filename is defined by the save menu, and has the format ".G##" for small saves (savegames) and ".S##" for normal saves (savestates).
- Created a small save system, and added the console commands "savesmall" and "loadsmall".
- Renamed the "Save Game" and "Load Game" menus to "Save State" and "Load State", and added menus "Save Game" and "Load Game" for the small saves.



======================
    Sound changes:
======================

- Sounds with no attenuation will play full volume on both channels.



============================
    2D renderer changes:
============================

- Added support for hi-res console backgrounds
- The console messages won't go off the bottom of the screen when the console is in full screen anymore. Now they will follow the height of the bottom of the screen.
- Removed the version number from the console background, because it's not needed anymore since the credits page was added to the help menu.
- Made the character drawing functions to not give an error if a character is partially or entirely out of the screen
- Centered the intermission screen.



===========================
    Status bar changes:
===========================

- Made a new status bar system. You can choose between 4 status bar styles (classic, classic + inventory, classic + inventory & level status, and a new one). New status bar styles can be easily added in the Sbar_Draw function in the source code.
- You can select which status bar items will be displayed in the new status bar.
- Now the +showscores command (TAB key) shows the full status bar.
- Added a sbar_bg cvar to toggle the background of the classic status bar.
- Added status bar options to the Video menu:
|- Status bar
|- Status bar width
|- Status bar height
|- Background
|- Level status
|- Weapon list
|- Ammo list
|- Keys
|- Runes
|- Powerups
|- Armor
|- Health
|- Ammo



==========================
    Crosshair changes:
==========================

- Replaced the old crosshair with a new one featuring 5 styles and several colors.
- The crosshair isn't shown during intermission anymore.
- Added a "crosshair_color" cvar to set the color of the crosshair. It supports integer values from 0 to 17.
- Added a "Crosshair" and a "Color" options with a box showing a crosshair to the Player Setup menu.



============================
    3D renderer changes:
============================

- Added a letterbox effect
- Rotating brushes.
- The sizeup and sizedown commands doesn't change the status bar anymore.
- The console variable "viewsize" isn't tied to the status bar anymore, and now its range is 50-100.
- Fixed some classic bugs in the chase camera (chase_active).



=============================
    Sky renderer changes:
=============================

- Added transparent sky. The bottom layer (the one who moves faster) of the sky will be transparent if the cvar r_skyalpha is set to any value between zero and 1. The intensity of the transparency depends on the value of the cvar r_skyalpha. The bottom layer won't be rendered if r_skyalpha is set to zero.

- Added support for skyboxes. 6 .pcx files, that should be placed into the gfx/env/ directory, are required. The names of the .pcx files should start with the name of the skybox, followed by "rt.pcx", "bk.pcx", "lf.pcx", "ft.pcx", "up.pcx" and "dn.pcx", without spaces.
The .pcx files must have the dimensions 256x256, 512x256, 256x512, or 512x512.
When a map is started, first the engine looks for the name of the skybox into the "sky" field of the world. If it's not set then it looks into the "r_skyname" cvar. You can also type "loadsky <name>" at the console.
It has a bug: Parts of the map which should be hidden by the skybox are shown. Start the map e4m7 and look up to see.



=======================================
    Alias models' renderer changes:
=======================================

- Transparencies, used in the weapon model when the player is invisible, and in any entity with the self.alpha value between zero and 1. Also supported in 2D sprites.
- Interpolation of position, rotation, frames and lighting of alias models. It also interpolates transitions from single frames to framegroups, from framegroups to single frames, and from a framegroup to another.
- Timed framegroups are interpolated according to the time interval defined in their frames.
- Changed some things in the lighting of alias models. They look darker now.
- There are some new cvars to adjust the lighting: r_light_vec_x, r_light_vec_y, r_light_vec_z, and r_light_style.
- Directional shading on r_light_style 1



==================================
    Particle renderer changes:
==================================

- Round particles.
- Fixed the distance between particles generated by trails.
- Now the particles are scaled on zoom.



=====================
    Cvar changes:
=====================

- Replaced +mlook with a m_look cvar
- Changed the default value of _cl_color to 22
- Changed the default value of v_centerspeed to 200
- Changed the default value of scr_printspeed to 16
- Added a "cl_nobob" cvar
- Added a "cl_vibration" cvar



=========================
    Protocol changes:
=========================

- Changed the angles to 16-bit
- Support for 16-bit ammo values in the statusbar (limited to 999)
- QC support for .glow (dynamic lights, including negative values)
- QC support for .alpha (to make sprites and alias models trasparent)
- QC support for .scale (to shrink/enlarge alias models and sprites)
- QC support for .scalev (similar to .scale, scales the x/y/z axis of alias models and the x/y axis of sprites)
- .scalev works cumulatively with self.scale
- .scale and .alpha also works in static entities



=====================
    Menu changes:
=====================

- Many optimizations to the menu code.
- Several cosmetic changes in the menus.
- Enabled transparent pixels in all menu's plaques.
- Now the images and sounds of the menus are precached on initialization. This eliminates the loading times in the menus.

- Added a page for Makaqu's credits and another for the Quake engine's credits to the help menu.
- Added a console variable "help_pages" to define the amount of help pages shown in the help menu.
- If the "help_pages" variable is set to zero then only the credit pages of Makaqu and Quake will be shown in the help menu.
- The credit screens are displayed during initialization.

- Added mouse input to the menu. Use the mouse wheel to change the position of the cursor, and the left button to select the options. The mouse buttons 1 and 2 also scrolls through the previous and next values in options with multiple values.

- Now the settings of the multiplayer menu only takes effect when a game is started.
- Added deathmatch 2 and 3 to the multiplayer menu.
- Added a "Same Level" option to the multiplayer menu.
- Added the following commands to customize the maplist in the multiplayer menu:
|- menu_addepisode
|- menu_addmap
|- menu_clearmaps

- Created some submenus in the Options menu:
|- Controller options (Dreamcast only)
|- Mouse options
|- Gameplay options
|- Audio options
|- Video options

- Added the options "Save setup" (saves makaqu.cfg) and "Load setup" (loads config.cfg/makaqu.cfg) to the Options menu.
- Moved all options, except "Go to console" and "Reset to defaults", from the main options menu to the submenus.
- Moved the "Video modes" menu into the Video options menu.

- The "Player setup" menu is acessible from the Options menu now.
- Added the options "Crosshair" and "Color" (for the crosshair) to the Player Setup menu.
- Removed the "Accept Changes" option from the Player Setup menu.
- Now the settings in the Player Setup menu takes effect when you exit the menu.

- Added the "Walk Speed" and "Strafe Speed" options to the mouse menu.

- Added the following options to the "Gameplay" menu:
|- Aim Assistance
|- Weapon Position
|- View Player Weapon
|- Bobbing
|- Slope Look
|- Renamed the Lookspring option to "Auto Center View"
|- Body View (chase camera)
|- Body View Distance
|- Body View Height

- Added CD player controls to the audio menu:
|- CD music (on/off)
|- Play track (number/total) (left/right changes, enter key/A button plays)
|- Loop (on/off)
|- Pause (on/off)
|- Stop
|- Open drive (doesn't exist in the DC version)
|- Close drive (doesn't exist in the DC version)
|- Reset CD audio (doesn't exist in the DC version)

- Added a Developer menu. Use the menu_developer command or press the X button at the "Go to console" option to go to it.

- Added the following options to the Developer menu:
|- cl_showfps
|- developer
|- registered
|- r_drawentities
|- r_fullbright
|- r_waterwarp
|- gl_polyblend
|- loadas8bit
|- god
|- fly
|- noclip
|- notarget
|- impulse 9
|- timedemo



==================================
    Dreamcast-related changes:
==================================

- Added an on-screen keyboard that can be used to edit console commands, chat messages and the player's name.

- In the on-screen keyboard, you can use:
|- The L trigger to input a backspace
|- The R trigger to input a space
|- The X button to use the autocomplete feature in the console
|- The Y button to input the Enter key
|- The A button to input the key under the cursor
|- The B button to close the on-screen keyboard

- Added controller functionality to the console:
|- The up and down buttons scrolls the console
|- the L and R triggers scrolls the console to the bottom and to the top
|- the left and right buttons cycles through the previous entered commands
|- the Y button enters the command
|- the B button closes the console
|- the A button opens the on-screen keyboard

- Added controller functionality to the menu:
|- The start buttons turns the menu off
|- The A button acts as enter, or activates the on-screen keyboard
|- The B button returns to the previous menu
|- The X button acts as backspace
|- The Y button is used to confirm in the pop-up dialog boxes

- Added the following options to the "Controller options" menu:
|- Axis X Deadzone
|- Axis Y Deadzone
|- Trigger L Deadzone
|- Trigger R Deadzone
|- Axis X Sensitivity
|- Axis Y Sensitivity
|- L Sensitivity
|- R Sensitivity



============================
    1.3 version changes:
============================

- Changed the "r_fullbright" cvar to render BSP models with neutral lighting instead of maximum lighting.
- Rewrote most of the PC controller code and added support for a second controller.
- Enabled the second controller on the Dreamcast version.
- Implemented savegame deletion by pressing backspace, delete, button 4 on the PC controller, or X on the DC controller.
- Done some changes to EF_SHADOW.
- Implemented EF_CELSHADING.
- Changed some other stuff.



============================
    1.2 version changes:
============================

Dreamcast-related bugfixes:
- Updated the default deadzone values for the controller.
- Fixed the cd_enabled cvar being not registered if there is no audio tracks on the disc.

Common changes:
- Updated BlackAura's e-mail address in the credits screen.



============================
    1.1 version changes:
============================

Dreamcast-related bugfixes:
- Corrected the name of mouse button 1.
- Fixed the "Pause" option in the Audio Options menu.
- nxMakaqu would crash when trying to save FrikFiles, because it tried to save them in the CD. Now the FrikFiles are saved in the RAM. There's no way to same them on the VMU yet though.
- The "screenshot" command is ignored now. This was done to prevent crashes, because it tried to save them in the CD.

Windows-related bugfixes:
- Fixed the crosshair being not displayed correctly in some video cards.
- Fixed the network menus (they were disabled by mistake).

Common bugfixes:
- Fixed a small problem with the interpolation (sorry, can't remember what it was).
- Now the translucent viewmodels are drawn over the translucent water instead of under it.
- Fixed the network protocol (demos recorded with some mods were not working).
- Fixed the "cmdline" cvar.
- Fixed some menu options that were not saved in the config file.

Dreamcast-related changes:
- Added an IP.BIN file.
- Modlist support for the /ID1 directory.
- Added support for the file /quake/gamedir/nxmakaqu.ini. This file can be used to set the title and description of the game in the modlist, as well as the sound samplerate, the video resolution, the size of the heap memory, and extra command line parameters.
- Added 320x480 and 640x240 resolutions (640x240 is faster).
- Resolutions up to 768x576 are selectable in the modlist.
- Changed the default audio samplerate to 11025 Hz, thus fixing some games that needs more RAM.
- The default setting for the stereo sound is set accordingly to the DC BIOS setting.
- Added support for volume control of the CD audio.
- The CD audio can be changed between stereo/mono, and the stereo can be reversed.
- The engine can detect the amount of CD tracks and display it on the Audio Options menu.
- Added support for mouse button 3 and mouse wheel. The mouse is fully supported now.
- Significant changes to the analog input support. Now the analog stick's axes can be bound to any command, and the "Axis/Trigger Function" options are gone.
- Added auto-repeat for the keys and face buttons while in the console or menu.
- Added vibration (Puru Puru/Jump Pack) support.
- Added a SVC_VIBRATE (61) message to the network protocol. Usage: SVC_VIBRATE, + 5 writebytes (special, effect1, effect2, duration, controller).
- VMU saves are compressed now, and saves from version 1.0 can still be loaded.
- Replaced the VMU file icon. The new one features colored borders for each game.
- Added a simple VMU LCD icon.
- FrikFiles are opened from the RAM if found in there, otherwise they are opened from the CD. Saving is always done in the RAM.

Windows-related changes:
- Removed the CD audio volume control from the audio menu.
- Now the engine knows the difference between CDs with no audio tracks and empty drives.
- The underwater screen buffer is always used, thus making the transparencies run faster on some computers.

Common changes:
- Improved the models of the super shotgun, nailgun, super nailgun, rocket launcher and thunderbolt. These new versions works better with interpolation and transparencies.
- Added a new progs.dat file with many bugfixes and some new features.
- The new progs.dat features a workaround for the solid skies in the original Quake levels. It can be disabled by typing "savedgamecfg 1" in the console.
- The status bar isn't displayed when changing levels anymore.
- The underwater warping effect runs at the same resolution of the video now.
- The camera tilting during weapon recoil is smoother. Now it uses 16-bit angles instead of 8.
- The Thunderbolt's lightning follows the player's rotation and movement smoothly now.
- Created QC support for self.frame_interval, to customize the time of the interpolation for each entity. Setting it to -1 disables interpolation, and setting it to zero uses the default value (0.1 seconds).
- Non-audio tracks are skipped when selecting an audio track in the Audio Options menu.
- Added a 3D stereo mode, using the r_stereo_separation cvar. It has a few bugs.
- Added options for fog and 3D stereo separation in the Developer menu. To disable the fog you must turn the developer mode off.
- Added the Timescale extension.
- Added a sv_compatibility cvar, to allow both network gaming and demo recording compatible with normal Quake.
- Added a sv_makaqu cvar to enable Makaqu-exclusive effects (EF_SHADOW, etc.).



============================
    1.0 version changes:
============================

Bugfixes:
- Fixed compatibility with the ASM code
- Fixed the "cd eject" command not working if the drive is empty
- Fixed camera in the wrong position when cl_nobob is 1
- Fixed the rotating brushes, to prevent the world model from rotating. Example: map E3M3 (The Tomb of Terror)
- Fixed all known problems in the interpolation
- Fixed the particles going partially out of screen if the FOV is smaller than 90
- Fixed incorrect screen size under the water when using the classic status bar with the background enabled

Dreamcast-related changes:
- Merged Makaqu and nxQuake sources
- Added SDL sound
- Added support for CD audio
- Support for OGG music (OGG music and sound effects are mutually exclusive) (not compiled in the released binary) (BlackAura)
- OGG files are loaded from /quake/gamedir/music/track##.ogg, and must not be inside of PAK files (BlackAura)
- Proper support for the official mission packs
- Improved nxQuake modlist
- Fixed modlist scrolling
- Holding the X button in the controller forces the modlist to appear if there's only one game on the disc
- Disc swapping support in the modlist. Just open the drive and replace the disc, it will be recognized automatically
- Multiple resolutions, selectable in the modlist
- VMU support for savegames, savestates and config files
- Independent VMU files for each game
- Support for multiple VMUs
- Automatic detection for which VMU is the config file stored in

3D renderer changes:
- The sky moves smoothly now, and it uses less RAM
- Translucent water. Lava is always opaque
- True transparencies, using BlackAura's alpha map
- Additive rendering, using BlackAura's additive map
- Added self.effects EF_NODRAW (16), EF_ADDITIVE (32), EF_FULLBRIGHT (512), and EF_SHADOW (8192)
- EF_ADDITIVE has priority over self.alpha
- self.alpha controls the intensity of EF_SHADOW
- EF_SHADOW is only drawn when close to solid surfaces, to prevent shadows in mid-air
- QC support for .scale on sprites, and for .scalev (similar to .scale, scales the x/y/z axis of alias models and the x/y axis of sprites)
- Improved the new lighting style
- Added the old lighting system back and made the new one optional
- r_sprite_addblend sets additive blending by default for all sprites (useful for normal Quake and closed-source mods)

2D renderer changes:
- (re)optimized the use of Draw_Fill. The engine was losing about 3 fps because of the way I was using Draw_Fill before.
- Rewrote the crosshair. Now it scales acording to the video resolution, and has "PNG-like" transparency. Also added more colors and a new style.
- Replaced r_letterbox with a SVC message (SVC_LETTERBOX = 60)
- SVC_LETTERBOX usage: writebyte(SVC_LETTERBOX) + writebyte(0 to 100)
- The centerprints are displayed in the black bar at the bottom of the screen when the letterbox is active
- SVC_LETTERBOX and SVC_INTERMISSION are mutually exclusive. This also means SVC_LETTERBOX can be used to turn SVC_INTERMISSION off
- Text shadows (done & disabled in the source code)

New cvars:
- r_wateralpha
- sw_stipplealpha
- r_sprite_addblend

New menu options:
- Allow "use" button
- Stereo Sound
- Swap L/R Channels
- Classic lighting
- Stipple alpha
- Water opacity
- Translucent particles
- Additive sprite blend

Miscellaneous:
- Support for more PAK files
- All PAK#.PAK files are loaded, even if there's one or more PAK files missing in the sequence (ex: PAK0.PAK, PAK2.PAK)
- Added a developer message "Loading filename.ext" when loading a map
- cmdlist accepts * at the beginning of the commands (example: cmdlist *list)
- cvarlist displays the "uses callback" attribute
- Host_Startdemos_f: clear demo list before adding demos
- Host_Startdemos_f: don't add invalid demos to the list
- The registered version isn't needed to run modified games anymore
- "-game hipnotic" and "-game rogue" works exactly like the "-hipnotic" and "-rogue" parameters
- Improved zoom scaling (for mouse/joystick/keyboard axis turning, particle scaling, sky scaling, etc.)
- Included a PAK10.PAK file containing the alpha map, the additive map, .vis and .ent patches, improved and bugfixed models and HUD & menu images, and a new default configuration

Tutorials implemented from www.quakesrc.org:
- MrG - incorrect BSP version is no longer fatal
- Enhanced BuiltIn Function System (EBFS) by Maddes
- Extension System by LordHavoc
- New BuiltIn Functions: etof() and ftoe() by Maddes
- Heffo - Cvar Callback Function (modified)
- FrikaC - EndFrame function
- FrikaC - qcexec function
- QuakeC file access by FrikaC/Maddes, QuakeC string manipulation by FrikaC/Maddes, and Enhanced temp string handling by Maddes (it's a 3-in-1 tutorial)

Changes from the QIP_Quake (Quake Info Pool) engine:
- .ENT support (modified)
- .VIS support - loading external vis+leafs data from VISPatch files (modified)
- .VIS packs (like ID1.VIS) are loaded from quake/gamedir/gamedir.vis instead of from quake/gamedir/maps/gamedir.vis, to prevent mistakes in case there's a map named quake/gamedir/maps/gamedir.bsp (Manoel Kasimier)
	Note: it is not possible to load .vis packs from inside of PAK files, but individual .vis patches works fine from inside of PAK files (Manoel Kasimier)
- Use full filename for hunk allocation
- DarkPlaces general builtin functions by LordHavoc
- Q_strcasecmp fix
- Do not stop server on objerror - quake/pr_cmds.c
- Cachelist command
- Hunklist command
- SV_Trace_Toss endless loop fix by LordHavoc
- PF_changepitch entity check by LordHavoc
- Kill command does not work for zombie players fix
- Tell command accepts player number
- Names with carriage return fix by Charles "Psykotik" Pence
- Piped output of a dedicated server not written immediately fix by Hendrik Lipka (sys_win.c only)
- Maximum precision for FTOS
- +USE unbound fix (modified to use sv_enable_use_button)
- ATOF problems with leading spaces fix
- added explicit brackets
- Coop not cleared when maxplayers changed/Coop and deathmatch flag fix by Frog/Maddes
- MOVETYPE_PUSH fix
- SV_MAXVELOCITY fix
- Returning information about loaded file
- Returning from which searchpath a file was loaded
- Finding the last searchpath of a directory



============================
    0.2 version changes:
============================

- Optimized and improved the sky transparency, now it supports variable intensity (it was always at 50% before)
- Fixed a bug with the skybox renderer not updating the Z buffer (my bad, I had disabled it before because I wasn't sure if it was needed)
- Frame interpolation for alias models is more stable now.

