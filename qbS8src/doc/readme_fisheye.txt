Fisheye Quake!

By Aardappel (aardappel@planetquake.com) & Brick (brick@gamesinferno.com)

http://www.planetquake.com/aardappel
http://www.ecs.soton.ac.uk/~wvo96r/gfxengine/fisheyequake/
http://www.gamesinferno.com/aardappel

http://www.gamesinferno.com/brick


Running it
==========
Put fisheyequake.exe in your quake directory and run it! couldn't be easier!

WARNING: Fisheye Quake is by nature very computationally intensive.
Run in 320x200, and unless you have a p500 or better do not expect to get
playable fps.

Two new cvars are available:
- "ffov" (default = 180). Change this to ANY value for the corresponding fov.
  Most practical values lie between 120 and 250, depending on you taste.
- "fviews" (default = 6). Fisheye Quake renders by first having quake render
  6 fov 90 views, and then sampling the final view from that. If your ffov
  is around 200 or lower, you will never see the back view so you can set
  fviews to 5 for a tiny speedup. If you use a really low ffov like around
  120, you can set it to 3 for quite a significant speedup (panorama mode :).

A couple of old cvars have had their name changed because they should never
be changed from their default values, because the fisheye renderer expects
them to have certain values. They are: "fov" (duh!) and "viewsize" (can't
have a statusbar, sorry!).


Problems
========
Fisheye Quake is a drop-in replacement for WinQuake, so if you never had that
installed you may miss some DLLs. Get it here:
http://www.cdrom.com/pub/idgames2/planetquake/academy/essentials/wq100.zip
Also, if your machine has problems running winquake, it will have the same
problems running fisheyequake. You may need to experiment with commandline
flags (note that things like -dibonly can slow it down greatly).

On some machines, it used to crash randonly after a few minutes. This is likely
fixed now though.


What! Why? How?
===============
If you are left full of questions about fisheye, check out the homepage, esp.
the page on there that compares fisheye with normal fov (url above).


Source Code
===========
In the source directory you will find the modified files (screen.c, r_main.c
and view.c). Almost all fisheye code sits at the end of view.c.
To compile for yourself, get the original id sourcecode release and simply
replace these 3 files. If you want to add fisheye to an already modified
port, you'll have to make a diff, and merge the code in yourself.

The code is covered by the same license as the original id code, the GNU public
license. Do not ask me to help in using the source, if you can't get it to work
you probably shouldn't be doing it anyway.


Cheating
========
There will be some people who will see no other purpose to Fisheye Quake than
cheating. Yes it would be quite an effective cheat (given you have an Athlon
800 or better), but that's not the reason I made it. Fisheye is simply a
superior way of displaying a 3d world, deal with it. I dig it for the way it
looks (more curves than q3a!) and the way it feels when you move, if your
only interest is cheating you are a sad bastard, I can't fix that.
Similarly if you feel like accusing me of helping people cheat, grow up.


Timedemos
=========
If you have a brutally fast machine and wanna impress me with your framerate,
do a "timedemo demo1" on (note well!) 320x200 full screen mode. Send me your
fps along with details of cpu/videocard/os (last two can influence performance
a lot, as Fisheye Quake does a lot of copying from and to videomemory).
For comparison, my machine (ppro200/voodoo3/w95) gets 6.5 fps, a mates
p3-600/geforce/w98 machine gets about 24 fps, and a celeron oc to 450/tnt/
w2k only got about 9. 

History
=======
I just love fisheye, and when I stumbled across a leaked copy of the quake
source code (for linux, version 1.01) I just couldn't resist. To avoid being
sued by id I couldn't release it though, so best I could do was screenshots
and AVIs on my homepage... highly irritating as some people even thought they
were merely fakes, warped in photoshop. Then in december'99 came the official
source code release so I could finally release the code.


Future
======
I intend to do this all in OpenGL, and a bit faster :)