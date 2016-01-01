The goal of Super8 is to expand the capabilities of authentic 8-bit rendering to play contemporary Quake maps: epic environments, hundreds of monsters, crunchy pixelated graphics. The source code is forked from Makaqu with enhancements from other open source Quake engine projects, plus a few original ideas.

Super8 runs in Windows, and in Linux under Wine. Memory and processor requirements are light. It remains snappy on those little 1Ghz Atom netbooks for example.

Recent new stuff
——————-
Gigantic bsp2 map support.

Alpha and fence textures.

Added cl_bobmodel cvar for side-to-side view model motion. Try values from 1 to 7 for variations, 0 for off. Simplified code from engoo, attributed to Sajt .

vid_nativeaspect cvar: By default, super8 will guess the native aspect from the maximum detected resolution. But if it looks wrong, set it manually with vid_nativeaspect.

Plug a few save vulnerabilities. A game can’t be saved as ‘pak0.pak’ for example.

Added help text to cvars. Type “chase_active” in the console for an example.

Added ‘status bar scale’ to video setup menu.

Added help text to cvars. Only a few are done so far. Type “chase_active” in the console for an example.

Moved super8 files to /super8 mod directory to reduce clutter. The engine will sandwich super8 between id1 and any other mod in precidence. If you have a previous version installed, pak88.pak and command_history.txt can be deleted from id1 dir. The readme and gnu3.txt can be removed from the main dir.

Added colored lighting for original ID levels to pak88 for convenience, “THE MH UNOFFICIAL 2009 ID1 LIT FILE PACK”. Thanks to mh for the definitive lits!

Statusbar, menu, and centerprint text scale based on screen resolution and sbar_scale cvar. sbar_scale is a factor of screen size, 0.0 to 1.0

sv_cullentities cvar – Reduces network traffic and cheat potential by culling entities that player can’t see. On by default.

Play mp3 music with “music” console command. Also music_stop, music_pause, music_loop. It should also play ogg but that’s untested so far. Control music volume from audio menu or bgmvolume cvar. Naming examples: ‘track01.mp3′, ‘track10.ogg’. Put tracks in /id/music/ directory, or a /music folder in a mod directory.

Freeze physics except players with cvar sv_freezephysics = 1. Requires sv_nocheats = 1. Stop action in the middle of a battle and look around.

The pak88.pak adds 21 bent palettes from Amon26. Thanks! Set includes amon26_pal01.lmp through amon26_pal21.lmp. Set “r_palette = amon26_pal21.lmp” prior to map load, or also hit “restart” if it’s after map load.

List of models not to lerp, torches and the like: r_nolerp_list

Start and stop demo record any time during gameplay.
Pgup and Pgdn keys fastforward and rewind demo during playback.
Video can be captured any time during gameplay or demo playback, including fastforward and rewind. Added these binds to make it handy:
bind F7 capture_start
bind F8 capture_stop

Smooth fisheye warping: r_fishaccel cvar. It is the change to zoom velocity per frame. For the Tormentarium video on youtube, the following binds zoom during demo playback:

alias +fishin “r_fishaccel -1.0?
alias -fishin “r_fishaccel 0.0?
alias +fishout “r_fishaccel 1.0?
alias -fishout “r_fishaccel 0.0?
bind HOME +fishin
bind END +fishout

Features
————
Real 8-bit software graphics… right up to the point where it must begrudgingly convert it’s output to the actual color depth of your modern display card.
Plays most modern epic Quake maps that require enhanced engine limits and extensions.
“Big map” support: increased map coordinates from short to long, higher map limits, modified protocol, etc.
Physics and movement fixes for a smoother experience.
Colored static and dynamic lights. Reads standard .lit lighting files.
Effects like scorch marks (stainmaps), fog, transparent water and particles, and skymaps.
Skyboxes: Can load skyboxes in pcx format, or tga with automatic conversion to 8-bit textures.
Fog: Reads fog info from maps with fog, or set with “fog” console command.
Unique colored lighting support that doesn’t slow down the engine. Adust color strength with with r_clintensity.
Load custom palettes. Generates colormap, alphamap and additivemap tables on-the-fly. Set a default custom palette with r_palette.
High-resolution, automatically detects available modes.
Modernized to play nice with weird laptop screen resolutions and Windows XP/ Vista/ 7.
Record demos directly to xvid, mp4, divx, or other avi-compatible formats. Produce HD video recording suitable for Youtube.
Demo fastforward and rewind. “pageup” and “pagedown” by default.
Start and stop demo recording any time during a game.
Start and stop video recording any time during a game or demo playback.
Handy console helpers like autocomplete with the TAB key, including command line completion and map name completion.
Enter ‘map *’ and TAB to list all maps.
Enhanced console scrollback.
Fisheye mode! [Optimize me!]
Interpolated (smooth) model animation and movement.
MOVETYPE_FOLLOW and MOVETYPE_BOUNCEMISSILE implemented, gives modders more features.
Many DP extensions supported including: drawonlytoclient, nodrawtoclient, exteriormodeltoclient, sv_modelflags_as_effects.
Fixed SV_TouchLinks and various other devious old bugs.
Search code for //qbism and see qbS8src/docs for more.

Default keyboard binds and aliases
——————————————
The traditional forward-backward-left-right key formation, typically W-S-A-D, is changed to R-F-D-G by default. This was so ‘S’ could be the fire key for the (discontinued) Flash version and ‘A’ could be an alt-fire or use-item if implemented in future. The arrangement seems to be an improvement on small keyboards. Flash uses left mouse click to change window focus and right click is not implemented, so mouse buttons can’t be used for fire.

When using a mod with Frikbot: In multiplayer mode ‘bot’ adds a Frikbot computer opponent, ‘removebot’ takes one away. Other binds and aliases below:

//qbism- Frikbots
alias bot “impulse 100?
alias addbot “impulse 100?
alias bot_team2 “impulse 101?
alias addbot_team2 “impulse 101?
alias bot_remove “impulse 102?
alias removebot “impulse 102?
bind END “impulse 102?
bind INS “impulse 100?

//qbism R-F-D-G instead of W-S-A-D to get fingers closer to higher number keys.
bind r +forward
bind R +forward //qbism Flash is case-sensitive
bind f +back
bind F +back
bind d +moveleft
bind D +moveleft
bind g +moveright
bind G +moveright
bind e +moveup
bind E +moveup
bind c +movedown
bind C +movedown
bind s +attack
bind S +attack
bind t impulse 10 //next weapon
bind T impulse 10
bind b impulse 12 //previous weapon
bind B impulse 12

//this is video capture start/stop, works any time during gameplay or demo playback!
bind F7 capture_start
bind F8 capture_stop

Code credits
————
Forked from Makaqu programmed by Manoel Kasimier
ToChriS Quake by Victor Luchitz
engoo by Leilei
enhanced WinQuake by Bengt Jardrup
FitzQuake Mark V port by Baker
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
