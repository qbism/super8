c:/CodeBlocks/Bin/svnversion > version.txt
set /p version= < version.txt
echo #define BUILDVERSION "%version%" > version.h

del version.txt