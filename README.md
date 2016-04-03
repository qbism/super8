qbism Super8

The goal of Super8 is to expand the capabilities of authentic 8-bit rendering to play contemporary Quake maps: epic environments, hundreds of monsters, crunchy pixelated graphics. The source code is forked from Makaqu with enhancements from other open source Quake engine projects, plus a few original ideas.

Super8 is developed in Windows 7 and is occasionally tested in Linux under Wine.  Windows 10, 8, XP, and Vista are reported to run it by users.

Thanks to everyone who gave feedback on the test builds from the color-overhaul experiments leading up to now!


New in build 278
——————————————————

More accurate color rendition.

r_saturation - cvar for map colored lighting intensity.

pixel doubling - for extra chunkiness (and render speed) automatically generate pixel-doubled option for compatible video modes.  These resolutions are listed with an 'S' in the video mode menu.

Resolution support up to 4K, but not fully tested.

cl_beams_quakepositionhack - cvar to turn off automatic centering of lighting bolts.  For mods that want an off-center bolt effect.

Improved overlapping transparencies -  It will usually be possible to see translucent water through a translucent window, for example.

automatic unvised map detection (slightly glitchy) - if water was created opaque (like classic Quake maps) transparency is turned off.  BUT to 'fake it', toggle r_novis and sv_novis cvars to 1.

r_novis - make water and similar textures transparent on old 'unvised' maps. 

sv_novis - don't cull any items serverside.  Normally the server culls most items that are hidden behind walls.

Fixed dedicated server (I hope).

Add tab-completion to loadpalette and r_palette.

Cheats always allowed in single-player mode.


Other features
————————————————————————————————

Gigantic bsp2 map support.

Alpha and fence textures.

cl_bobmodel cvar for side-to-side view model motion. Try values from 1 to 7 for variations, 0 for off. Simplified code from engoo, attributed to Sajt .

vid_nativeaspect cvar: By default, super8 will guess the native aspect from the maximum detected resolution. But if it looks wrong, set it manually with vid_nativeaspect.

Plug a few save vulnerabilities. A game can’t be saved as ‘pak0.pak’ for example.

Added help text to cvars. Type “chase_active” in the console for an example.

‘status bar scale’ on video setup menu.

Statusbar, menu, and centerprint text scale based on screen resolution and sbar_scale cvar. sbar_scale is a factor of screen size, 0.0 to 1.0

sv_cullentities cvar – Reduces network traffic and cheat potential by culling entities that player can’t see. On by default.

Play mp3 music with “music” console command. Also music_stop, music_pause, music_loop. It should also play ogg but that’s untested so far. Control music volume from audio menu or bgmvolume cvar. Naming examples: ‘track01.mp3′, ‘track10.ogg’. Put tracks in /id/music/ directory, or a /music folder in a mod directory.

Freeze physics except players with cvar sv_freezephysics = 1. Requires sv_nocheats = 1. Stop action in the middle of a battle and look around.

List of models not to lerp, torches and the like: r_nolerp_list

Start and stop demo record any time during gameplay.

Fastforward and rewind demo during playback.

Video can be captured any time during gameplay or demo playback.  Record demos directly to xvid, mp4, divx, or other avi-compatible formats.
Example binds:
bind F7 capture_start
bind F8 capture_stop

Real 8-bit software graphics… right up to the point where it must begrudgingly convert it’s output to the actual color depth of your modern display card.

Plays most modern epic Quake maps that require enhanced engine limits and extensions.

Physics and movement fixes for a smoother experience.

Effects including scorch marks (stainmaps), fog, transparent water and particles, and skymaps.

Skyboxes: Can load skyboxes in pcx format, or tga with automatic conversion to 8-bit textures.

Fog: Reads fog info from maps with fog, or set with “fog” console command.

Unique colored lighting support that doesn’t slow down the engine.

Load custom palettes. Generates colormap, alphamap and additivemap tables on-the-fly. Set a default custom palette with r_palette.

Modernized for high-resolutions and to play nice with weird laptop screen resolutions.

MOVETYPE_FOLLOW and MOVETYPE_BOUNCEMISSILE implemented, gives modders more features.

Several DP extensions supported including: drawonlytoclient, nodrawtoclient, exteriormodeltoclient, sv_modelflags_as_effects.

Handy console helpers like autocomplete with the TAB key, including command line completion and map name completion.
Enter ‘map *’ and TAB to list all maps.

Enhanced console scrollback.

Interpolated (smooth) model animation and movement.

Fixed SV_TouchLinks and various other devious old bugs.

Fisheye mode.

Smooth fisheye warping: r_fishaccel cvar. It is the change to zoom velocity per frame. For the Tormentarium video on youtube, the following binds zoom during demo playback:

alias +fishin “r_fishaccel -1.0?
alias -fishin “r_fishaccel 0.0?
alias +fishout “r_fishaccel 1.0?
alias -fishout “r_fishaccel 0.0?
bind HOME +fishin
bind END +fishout

Search code for //qb: for changes relative to Makaqu 1.3.

Code credits:
————
Forked from Makaqu programmed by Manoel Kasimier
ToChriS Quake by Victor Luchitz
engoo by Leilei
enhanced WinQuake by Bengt Jardrup
FitzQuake Mark V by Baker
joequake engine by Jozsef Szalontai
qrack engine coded by R00k
fteqw engine by Spike and FTE Team
FitzQuake coded by John Fitz
fisheye code from Aardappel
Fixes and cleanup by Levent
DarkPlaces engine by Lord Havoc
GoldQuake engine by Sajt
Tutorials by MH, Baker, Kryten (inside3d.com) and Fett, JTR (quakesrc.org)
Quake Standards Base reference
