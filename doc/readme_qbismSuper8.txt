Last updated 09/06/12

qbism Super8 is an open source (GPL) 3D game engine with advanced yet intentionally retro 8-bit graphics for a crunchy pixelated look.  The source code is forked from Makaqu Quake with enhancements from other Quake engine projects, plus a few original ideas.  The goal is to expand the capabilities of authentic software-rendering without sacrificing performance.

Super8 runs in Windows, and in Linux under Wine last time I checked. Previous builds also ran in Flash and DOS.  (Flash support is in limbo while Adobe works out new tools, and my last DOS box has died.)  Memory and processor requirements are light.  It remains snappy on those little 1Ghz Atom netbooks for example.

Recent new stuff
-------------------
Start and stop demo record any time during gameplay.
Pgup and Pgdn keys fastforward and rewind demo during playback.
Video can be captured any time during gameplay or demo playback, including fastforward and rewind.  Added these binds to make it handy:
bind F7 capture_start
bind F8 capture_stop


Features
------------
    Real 8-bit software graphics... right up to the point where it must begrudgingly convert it's output to the actual color depth of the display (typically 32-bit).  
    Plays most modern epic Quake maps that require enhanced engine limits and extensions.
    "Big map" support:  increased map coordinates from short to long, higher map limits, modified protocol, etc.
    Physics and movement fixes for a smoother experience.
	Colored static and dynamic lights.  Reads standard .lit lighting files.
    Effects like scorch marks (stainmaps), fog, transparent water and particles, and skymaps.
    Skyboxes:  Can load skyboxes in pcx format, or tga with automatic conversion to 8-bit textures.
	Fog:  Reads fog info from maps with fog, or set with "fog" console command.
	Unique colored lighting support that doesn't slow down the engine. Adust color strength with with r_clintensity.
	Load custom palettes.  Generates colormap, alphamap and additivemap tables on-the-fly.  Set a default custom palette with r_palette.
    High-resolution, automatically detects available modes.
    Modernized to play nice with weird laptop screen resolutions and Windows XP/ Vista/ 7.
    Record demos directly to xvid, mp4, divx, or other avi-compatible formats.  Produce HD video recording suitable for Youtube.
	Demo fastforward and rewind.  "pageup" and "pagedown" by default.
	Start and stop demo recording any time during a game.
	Start and stop video recording any time during a game or demo playback.
    Handy console helpers like autocomplete with the TAB key, including command line completion and map name completion.
    Enter 'map *' and TAB to list all maps.
    Enhanced console scrollback.
    Fisheye mode!  [Optimize me!]
    Interpolated (smooth) model animation and movement.
    MOVETYPE_FOLLOW and MOVETYPE_BOUNCEMISSILE implemented, gives modders more features.
    Many DP extensions supported including: drawonlytoclient, nodrawtoclient, exteriormodeltoclient, sv_modelflags_as_effects.
    Fixed SV_TouchLinks and various other devious old bugs.
    Search code for //qbism and see qbS8src/docs for more.
	
Default keyboard binds and aliases
------------------------------------------
The traditional forward-backward-left-right key formation, typically W-S-A-D, is changed to R-F-D-G by default.  This is so 'S' can be the fire key for the Flash version and 'A' could be an alt-fire or use-item if implemented in future.  The arrangement seems to be an improvement on small keyboards.  Flash uses left mouse click to change window focus and right click is not implemented, so mouse buttons can't be used for fire.

When using a mod with Frikbot:  In multiplayer mode 'bot' adds a Frikbot computer opponent, 'removebot' takes one away.  Other binds and aliases below:

//qbism- Frikbots
alias bot "impulse 100"
alias addbot "impulse 100"
alias bot_team2 "impulse 101"
alias addbot_team2 "impulse 101"
alias bot_remove "impulse 102"
alias removebot "impulse 102"
bind END "impulse 102"
bind INS "impulse 100"

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
bind t impulse 10  //next weapon
bind T impulse 10
bind b impulse 12  //previous weapon
bind B impulse 12

//this is video capture start/stop, works any time during gameplay or demo playback!
bind F7 capture_start
bind F8 capture_stop

Code credits
------------
originally based on Makaqu 1.3 programmed by Manoel Kasimier
ToChriS Quake by Victor Luchitz
FlashQuake port by Michael Rennie
FlashProQuake port by Baker
joequake engine by Jozsef Szalontai
qrack engine coded by R00k
fteqw engine by Spike and FTE Team
FitzQuake coded by John Fitz
DarkPlaces engine by Lord Havoc
GoldQuake engine by Sajt
Tutorials by MH, Baker, Kryten (inside3d.com) and Fett, JTR (quakesrc.org)
Quake Standards Base reference
http://quakery.quakedev.com/qwiki/index.php/QSB
and related
http://quakery.quakedev.com/qwiki/index.php/Engine_Limits