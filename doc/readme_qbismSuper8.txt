qbismSuper8 readme
09-19-2010

qbismSuper8 is an open source 3D game engine with enhanced yet intentionally retro 8-bit graphics.  It's originally based on the heavily modified and updated Makaqu 1.3 Quake engine, with additional changes.

qbismSuper8 runs in Windows and Flash.  Memory and processor requirements are light... less so for the Flash port.  As a software engine, it's not dependent on 3D graphics card features, and so may ease porting to other platforms (Silverlight?) The beauty of Makaqu is that it's both feature-rich and easily portable.  It was straighforward to port this to Flash using FlashQuake and FlashProQuake as guides.  nxMakaqu (Dreamcast port) is not supported.

Note, this is not an official continuation or branch of Makaqu, ToChriS, or other engines.  It was just a great code base to start an 8-bit port to Flash.  In fact, official Makaqu development is continuing.

See qbismS8_flash_compile.txt for more detail on the Flash port.


Features
--------
8-bit software graphics.  For that sweet crunchy 3D-pixelated look.

Supports transparent textures, sprites, and particles.

Interpolated model animation and movement.

MOVETYPE_FOLLOW and MOVETYPE_BOUNCEMISSILE implemented (DarkPlaces).

Many DP extensions supported including drawonlytoclient nodrawtoclient exteriormodeltoclient sv_modelflags_as_effects

Stainmaps.  (Modified from fteqw)

Increased engine limits including edicts, particles, sounds, and map edges/surfaces/spans.  Network protocol changes as required. (joequake)

Autocomplete with the TAB key, including command line completion and map name completion.  Enter 'map *' and TAB to list all maps. (qrack/joequake)

Record movies directly to XVID and other AVI formats. (Baker tute/joequake)

Wrapped memory and string operations including Q_Free and Q_malloc (joequake and others)

Fixed SV_TouchLinks (ported from mh GLquake tute)

Increased map coordinates from short to long.  Untested. (originally JTR)

All Con_Printf calls are safeprints.

Add FOV slider.

Enhanced console scrollback. (Fett)

Always attempt to disable mouse acceleration. (mh tute)

Very cool improved model shading effect, seems more consistent with the contrast of map shading.

Skyboxes

Search code for //qbism and see qbS8src/docs for more.


Default keyboard binds and aliases
----------------------------------
The traditional forward-backward-left-right key formation, typically W-S-A-D, is changed to R-F-D-G by default.  This is so 'S' can be the fire key and 'A' could be an alt-fire if implemented in future.  Flash uses left mouse click to change window focus and right click is not implemented, so mouse buttons can't be used for fire.

In multiplayer mode 'bot' adds a Frikbot computer opponent, 'removebot' takes one away.  Other binds and aliases below:

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
