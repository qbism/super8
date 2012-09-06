/*  Copyright (C) 1996-1997 Id Software, Inc.
    Copyright (C) 1999-2012 other authors as noted in code comments

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.   */

#include "../quakedef.h"
#include "errno.h"

qboolean			isDedicated;	//Always false for Null Driver
//qb: pointless in Flash     cvar_t		_windowed_mouse = {"_windowed_mouse","0", true};

/*
===============================================================================

FILE IO

===============================================================================
*/

#define MAX_HANDLES             64 //qbism was 10
FILE    *sys_handles[MAX_HANDLES];

int             findhandle (void)
{
	int             i;

	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
filelength
================
*/
int filelength (FILE *f)
{
	int             pos;
	int             end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenAppend (char *path) //qbism - mh FRIK_FILE tute
{
   FILE   *f;
   int      i;

   i = findhandle ();
   f = fopen (path, "ab");

   // change this Sys_Error to something that works in your code
   if (!f)
      Sys_Error ("Error opening %s: %s", path, strerror (errno));

   sys_handles[i] = f;
   return i;
}


int Sys_FileOpenRead (char *path, int *hndl)
{
	FILE    *f;
	int             i;

	i = findhandle ();

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;

	return filelength(f);
}

int Sys_FileOpenWrite (char *path)
{
	FILE    *f;
	int             i;

	i = findhandle ();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));
	sys_handles[i] = f;

	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int     Sys_FileTime (char *path)
{
	FILE    *f;

	f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}

	return -1;
}

void Sys_mkdir (char *path)
{
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}


void Sys_Error (char *error, ...)
{
	va_list         argptr;

	printf ("Sys_Error: ");
	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");

	exit (1);
}

void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;

	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
}

void Sys_Quit (void)
{
}


double _as3Time;

double Sys_DoubleTime (void) //qbism was Sys_FloatTime
{
	return _as3Time;
}

char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_Sleep (void)
{
}

void Sys_SendKeyEvents (void)
{
}


#if defined(NULL_DRIVER)

void	VID_LockBuffer (void) {}
void	VID_UnlockBuffer (void) {}

#endif

//=============================================================================


AS3_Val _swfMain;
double _oldtime;

void trace(char *fmt, ...)
{
	va_list		argptr;
	char		msg[10000] = "TRACE: ";
	AS3_Val as3Str;

	va_start (argptr,fmt);
	vsprintf (&msg[7],fmt,argptr);
	va_end (argptr);

	as3Str = AS3_String(msg);
	AS3_Trace(as3Str);
	AS3_Release(as3Str);
}

/* Does a FILE * read against a ByteArray */
static int readByteArray(void *cookie, char *dst, int size)
{
	return AS3_ByteArray_readBytes(dst, (AS3_Val)cookie, size);
}

/* Does a FILE * write against a ByteArray */
static int writeByteArray(void *cookie, const char *src, int size)
{
	return AS3_ByteArray_writeBytes((AS3_Val)cookie, (char *)src, size);
}

/* Does a FILE * lseek against a ByteArray */
static fpos_t seekByteArray(void *cookie, fpos_t offs, int whence)
{
	return AS3_ByteArray_seek((AS3_Val)cookie, offs, whence);
}

/* Does a FILE * close against a ByteArray */
static int closeByteArray(void * cookie)
{
	AS3_Val zero = AS3_Int(0);

	/* just reset the position */
	AS3_SetS((AS3_Val)cookie, "position", zero);
	AS3_Release(zero);
	return 0;
}

FILE* as3OpenWriteFile(const char* filename)
{
	FILE* ret;
	AS3_Val byteArray;

	AS3_Val params = AS3_Array(
		"AS3ValType",
		AS3_String(filename));

	byteArray = AS3_CallS("fileWriteSharedObject", _swfMain, params);
	AS3_Release(params);

	//This opens a file for writing on a ByteArray, as explained in http://blog.debit.nl/?p=79
	//It does NOT reset its length to 0, so this must already have been done.
	ret = funopen((void *)byteArray, readByteArray, writeByteArray, seekByteArray, closeByteArray);

	return ret;
}

void as3UpdateFileSharedObject(const char* filename)
{
	AS3_Val params = AS3_Array("AS3ValType", AS3_String(filename));

	AS3_CallS("fileUpdateSharedObject", _swfMain, params);

	AS3_Release(params);
}

void as3ReadFileSharedObject(const char* filename)
{
	AS3_Val params = AS3_Array("AS3ValType", AS3_String(filename));

	AS3_CallS("fileReadSharedObject", _swfMain, params);

	AS3_Release(params);
}

int swcQuakeInit (int argc, char **argv);

AS3_Val swcInit(void *data, AS3_Val args)
{
	int argc;
	char *argv;

	//Save the byte array, which will be read in COM_InitFilesystem
	AS3_ArrayValue(args, "AS3ValType", &_swfMain);

	//Launch the quake init routines all the way until before the main loop
	argc = 1;
	argv = "";
	swcQuakeInit(argc, &argv);

	//Return the ByteArray object representing all of the C++ ram - used to render the bitmap
	return AS3_Ram();
}

AS3_Val swcFrame(void *data, AS3_Val args)
{
	AS3_ArrayValue(args, "DoubleType", &_as3Time);	//This will allow Sys_DoubleTime() to work

	if(!_oldtime)
		_oldtime = Sys_DoubleTime ();

	{//Copied from the loop from the linux main function

		double		time, newtime;

// find time spent rendering last frame
        newtime = Sys_DoubleTime ();
        time = newtime - _oldtime;

        if (time > sys_ticrate.value*2)
            _oldtime = newtime;
        else
            _oldtime += time;

        Host_Frame (time);
    }

	//Return the position of the screen buffer in the Alchemy ByteArray of memory
	extern unsigned _vidBuffer4b[];
	return AS3_Ptr(_vidBuffer4b);
}

extern AS3_Val _flashSampleData;
extern int soundtime;
void S_Update_();

AS3_Val swcWriteSoundData(void *data, AS3_Val args)
{
	int soundDeltaT;

	AS3_ArrayValue(args, "AS3ValType,IntType", &_flashSampleData, &soundDeltaT);
	soundtime += soundDeltaT;
	S_Update_();

	return NULL;
}

AS3_Val swcKey(void *data, AS3_Val args)
{
#ifndef FLASHDEMO //qbism disable input for flash demo-mode-only
	int key, charCode, state;

	AS3_ArrayValue(args, "IntType,IntType,IntType", &key, &charCode, &state);

	extern byte _asToQKey[256];
	if(_asToQKey[key])
	{
		//Modifier, Fkey, or special keys for example, so we need to look up its Quake code
		Key_Event(_asToQKey[key], state);
	}
	else
	{
		//For most keys, the Quake code is the same as the char code
		Key_Event(charCode, state);
	}
#endif //FLASHDEMO
	return NULL;
}

extern float mouse_x, mouse_y;
AS3_Val swcDeltaMouse(void *data, AS3_Val args)
{
	#ifndef FLASHDEMO //qbism disable input for flash demo-mode-only
	int deltaX, deltaY;
	AS3_ArrayValue(args, "IntType,IntType", &deltaX, &deltaY);

	mouse_x = deltaX;
	mouse_y = deltaY;
	#endif //FLASHDEMO

	return NULL;
}

int main (int c, char **v)
{
	int i;

	AS3_Val swcEntries[] =
	{
		AS3_Function(NULL, swcInit),
		AS3_Function(NULL, swcFrame),
		AS3_Function(NULL, swcKey),
		AS3_Function(NULL, swcDeltaMouse),
		AS3_Function(NULL, swcWriteSoundData)
	};

	// construct an object that holds refereces to the functions
	AS3_Val result = AS3_Object(
		"swcInit:AS3ValType,swcFrame:AS3ValType,swcKey:AS3ValType,swcDeltaMouse:AS3ValType,swcWriteSoundData:AS3ValType",
		swcEntries[0],
		swcEntries[1],
		swcEntries[2],
		swcEntries[3],
		swcEntries[4]);

	for(i = 0; i < sizeof(swcEntries)/sizeof(swcEntries[0]); i++)
		AS3_Release(swcEntries[i]);

	// notify that we initialized -- THIS DOES NOT RETURN!
	AS3_LibInit(result);
}

int swcQuakeInit (int argc, char **argv)
{
	static quakeparms_t    parms;

	parms.memsize = 8*1024*1024*4;
	parms.membase = Q_malloc (parms.memsize);
	parms.basedir = ".";

	COM_InitArgv (argc, argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	printf ("Host_Init\n");
	Host_Init (&parms);

	return 0;
}


