Name: ToChriS Quake v 1.67
Author: Vic
E-mail: vic@quakesrc.org
Homepage: http://hkitchen.quakesrc.org/
	  http://www.qexpo.com/
Release Date: Around 15th of December, 2001
		     25th of January,  2002
		     9th of July, QExpo 2003
		     17th of August, 2003
Build Time: I wish I knew
Description: A NetQuake engine that adds some neat features to old boring WinQuake. I hope it will become a source
	of valuable information for all Quake1 engine coders. And... software quake has never been so fun before! Enjoy!

/*================================================*/
Installation
/*================================================*/

Extract tochris.exe and gltochris.exe to your Quake folder (the one that contains quake.exe).

/*================================================*/
What's new in 1.67
/*================================================*/
- new visibility check algorithm for static entities, they cause much less overdraw than in 1.66
- corrected frames timings in framegroups (software)
- removed code for triangles subdivision - improved quality of rendering alias models (software)
- fixed possible crashes in intermission screen

/*================================================*/
What's new in 1.66
/*================================================*/
- GL version of ToChriS works again
- caps lock is now bindable
- 'crosshair 2' and 'crosshair 3' are gone
- fixed stereo sound support
- fixed .pcx loading in software so skyboxes are not borked in corners
- fixed a nasty interpolation crash (software)
- fixed dynamic lights on rotating\moving bmodels and .bsp models (ammo boxes, etc.) (both GL and software)
- rewrote some parts of frame interpolation of alias models in asm for extra speed (software)
- rendering module (d_ and r_ files in software and gl_ in gl) is now completely independent from client and server modules
- proper client-side collision detection that works in multiplayer (you can use it for particles. Other engines that can do proper client-side collision detection are DarkPlaces and probably Tenebrae 2, but it doesn' support q1 maps).
- new interesting effects like rotating player model in Multiplayer->Setup menu (ala Quake2)
- improvements in software renderer to make it look a bit nicer
- major code cleanup

/*================================================*/
What's new in 1.11
/*================================================*/
- .lmp and gfx.wad pictures support alpha pixels now. Tixels that have their color set to 255 will be automatically detected by TOChriS Quake and skipped at drawing stage.
- added '()' to say_team, so team messages are QW-style now.
- skyboxes support. To load a skybox you can use type 'loadsky skyboxname' or 'r_skyname skyboxname' at the console. Mapper can assign skybox to his map by setting "sky" field of worldspawn entity to the name of the wanted skybox. 6 .pcx files, that should be placed in gfx/env/ directory, are required.
- stereo sound support ('loadas8bit' cvar is gone, added 'snd_swapstereo' cvar for people with backwards sound wiring).
- added '+use' button support.
- win32 Copy & Paste to console.
- 'crosshair 2' (cross), 'crosshair 3' (square) and 'crosshair 4' (dot), 'crosshaircolor' cvars.
- 'cl_freelook' cvar (actually it was implemented in 1.10 but I forgot to document it).
- 'viewalias' command (usage: 'viewalias aliasname'). Displays body of the alias.
- 'writeconfig' command (usage: 'writeconfig filename'). Also was implemented in 1.10 but left undocumented until 1.11.
- fixed the nasty bug, that could lead to invalid rendering of surfaces.

/*================================================*/
What's new in 1.10
/*================================================*/
- Frame ('r_im_animation' cvar) and movement ('r_im_transform' cvar) interpolation (!).
- Ability to load .lit files (set 'r_loadlits' to 1 once in the game and restart the map). You won't notice any difference between original mode (no coloured lights), but I kept in the case you want it...
- Fixed chase cam. Use 'chase_active' cvar to turn it on/off.
- Bobbing items ('r_bobbing' cvar).
- Advanced console.
- Increased number of maximum vertexes per model to 2024 (so FBI MOD works).
- Fixed bounding boxes of models (affects static entities).
- Support for rotating bmodels.
- 'show_fps' cvar.
- Fast sky. Use 'r_fastsky' and 'r_skycolor' cvars to change options of the sky.
- 'Graphics options' menu to play with.
- Tons of tweaks, speedups and bugfixes that may be not that visible but make game a lot more playable.

/*================================================*/
Thanks
/*================================================*/

- LordHavoc: bounding boxes fix, .lit files, stereo sound support (just great programmer!).
- Pheonix: original interpolation code, which mine is based on.
- FrikaC: improved\fixed chasecam and support for more than 3 years (or maybe even 4?).
- EvilTypeGuy: advanced console. Be as evil as possible!
- Tonik (aka Bemep): fast sky code, advice, help and examples of good coding!
- All Twilight (http://twilight.sf.net/) guys. Hey, I'm one of them!
