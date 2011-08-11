Last updated 8/10/11

qbismSuper8 is an open source 3D game engine with enhanced yet intentionally retro 8-bit graphics.  It's originally based on the heavily modified and updated Makaqu 1.3 Quake engine, with additional changes.

qbismSuper8 runs in Windows and Flash.  Memory and processor requirements are light... less so for the Flash port.  As a software engine, it's not dependent on 3D graphics card features, and so may ease porting to other platforms (Silverlight?) The beauty of Makaqu is that it's both feature-rich and easily portable.  It was straighforward to port this to Flash using FlashQuake and FlashProQuake as guides.  nxMakaqu (Dreamcast port) is not supported.

Note, this is not an official continuation or branch of Makaqu, ToChriS, or other engines.  It was just a great code base to start an 8-bit port to Flash.

See Flash compiling notes for more detail on the Flash port.

Features
------------

    8-bit software graphics.  For that sweet crunchy 3D-pixelated look.
    Interpolated model animation and movement.
    Plays most modern epic Quake maps that require enhanced engine limits and extensions.
    "Big map" support:  increased map coordinates from short to long, higher map limits, modified protocol, etc.
    Physics and movement fixes for a smoother experience.
    Effects like scorch marks (stainmaps), transparent water and particles, and skymaps.
    Skyboxes:  can load skyboxes in pcx format,  or tga with automatic conversion to 8-bit textures.
    High-resolution, automatically detects available modes.
    Modernized to play nice with weird laptop screen resolutions and Windows XP/ Vista/ 7.
    Record demos directly to xvid, mp4, divx, or other avi-compatible formats.  Produce HD video recording suitable for Youtube.
    Handy console helpers like autocomplete with the TAB key, including command line completion and map name completion.
     Enter 'map *' and TAB to list all maps.
    Enhanced console scrollback.
    Fisheye mode!
    Load custom palettes.  Generates colormap, alphamap and additivemap tables on-the-fly.
    MOVETYPE_FOLLOW and MOVETYPE_BOUNCEMISSILE implemented, gives modders more features.
    Many DP extensions supported including drawonlytoclient nodrawtoclient exteriormodeltoclient sv_modelflags_as_effects.
    Fixed SV_TouchLinks and various other devious old bugs.
    Search code for //qbism and see qbS8src/docs for more.

 

Default keyboard binds and aliases
------------------------------------------
The traditional forward-backward-left-right key formation, typically W-S-A-D, is changed to R-F-D-G by default.  This is so 'S' can be the fire key and 'A' could be an alt-fire if implemented in future.  Flash uses left mouse click to change window focus and right click is not implemented, so mouse buttons can't be used for fire.

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