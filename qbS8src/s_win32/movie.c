/*
Copyright (C) 2002 Quake done Quick

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// movie.c -- video capturing

#include "../quakedef.h"
#include "movie.h"
#include "movie_avi.h"

extern	float	scr_con_current;
extern qboolean	scr_drawloading;
extern	short	*snd_out;
extern	int	snd_linear_count, soundtime;

// Variables for buffering audio
short	capture_audio_samples[44100];	// big enough buffer for 1fps at 44100Hz
int	captured_audio_samples;

static	int	out_size, ssize, outbuf_size;
static	byte	*outbuf, *picture_buf;
static	FILE	*moviefile;

static	float	hack_ctr;

static qboolean OnChange_capture_dir (cvar_t *var, char *string);

cvar_t   capture_codec   = {"capture_codec", "XVID", true}; //qbism jqavi, change to XVID default

cvar_t	capture_fps	= {"capture_fps", "30.0", true};
cvar_t	capture_console	= {"capture_console", "1", true};
cvar_t	capture_hack	= {"capture_hack", "0", true};
cvar_t	capture_mp3	= {"capture_mp3", "0", true};
cvar_t	capture_mp3_kbps = {"capture_mp3_kbps", "128", true};

static qboolean movie_is_capturing = false;
qboolean	avi_loaded, acm_loaded;

qboolean Movie_IsActive (void)
{
	// don't output whilst console is down or 'loading' is displayed
	if ((!capture_console.value && scr_con_current > 0) || scr_drawloading)
		return false;

	// otherwise output if a file is open to write to
	return movie_is_capturing;
}

void Movie_Start_f (void)
{
	char	name[MAX_OSPATH], path[256]; //qbism jqavi was MAX_FILELENGTH

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("capture_start <filename> : Start capturing to named file\n");
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	COM_ForceExtension (name, ".avi");

	//hack_ctr = capture_hack.value;
	//Q_snprintfz (path, sizeof(path), "%s/%s", capture_dir.string, name);
	//if (!(moviefile = fopen(path, "wb")))

	hack_ctr = capture_hack.value;
	Q_snprintfz (path, sizeof(path), "%s/%s", com_gamedir, name);
	if (!(moviefile = fopen(path, "wb")))
	{
		COM_CreatePath (path);
		if (!(moviefile = fopen(path, "wb")))
		{
			Con_Printf ("ERROR: Couldn't open %s\n", name);
			return;
		}
	}

	movie_is_capturing = Capture_Open (path);
}

void Movie_Stop (void)
{
	movie_is_capturing = false;
	Capture_Close ();
	fclose (moviefile);
}

void Movie_Stop_f (void)
{
	if (!Movie_IsActive())
	{
		Con_Printf ("Not capturing\n");
		return;
	}

	if (cls.capturedemo)
		cls.capturedemo = false;

	Movie_Stop ();

	Con_Printf ("Stopped capturing\n");
}

void Movie_CaptureDemo_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: capturedemo <demoname>\n");
		return;
	}

	Con_Printf ("Capturing %s.dem\n", Cmd_Argv(1));

	CL_PlayDemo_f ();
	if (!cls.demoplayback)
		return;

	Movie_Start_f ();
	cls.capturedemo = true;

	if (!movie_is_capturing)
		Movie_StopPlayback ();
}

void Movie_Init (void)
{
	AVI_LoadLibrary ();
	if (!avi_loaded)
		return;

	captured_audio_samples = 0;

	Cmd_AddCommand ("capture_start", Movie_Start_f);
	Cmd_AddCommand ("capture_stop", Movie_Stop_f);
	Cmd_AddCommand ("capturedemo", Movie_CaptureDemo_f);

	//qbism jqavi per Baker tute
	Cvar_RegisterVariable (&capture_codec);
   Cvar_RegisterVariable (&capture_fps);
   Cvar_RegisterVariable (&capture_console);
   Cvar_RegisterVariable (&capture_hack);

   ACM_LoadLibrary ();
   if (!acm_loaded)
      return;

   Cvar_RegisterVariable (&capture_mp3);
   Cvar_RegisterVariable (&capture_mp3_kbps);
}

void Movie_StopPlayback (void)
{
	if (!cls.capturedemo)
		return;

	cls.capturedemo = false;
	Movie_Stop ();
}

double Movie_FrameTime (void)
{
	double	time;

	if (capture_fps.value > 0)
		time = !capture_hack.value ? 1.0 / capture_fps.value : 1.0 / (capture_fps.value * (capture_hack.value + 1.0));
	else
		time = 1.0 / 30.0;
	return bound(1.0 / 1000, time, 1.0);
}

void Movie_UpdateScreen (void)
{
	int	i, j, rowp;
	byte	*buffer, *p;

	if (!Movie_IsActive())
		return;

	if (capture_hack.value)
	{
		if (hack_ctr != capture_hack.value)
		{
			if (!hack_ctr)
				hack_ctr = capture_hack.value;
			else
				hack_ctr--;
			return;
		}
		hack_ctr--;
	}

	buffer = Q_malloc (vid.width * vid.height * 3);

	p = buffer;
	for (i = vid.height - 1 ; i >= 0 ; i--)
	{
		rowp = i * vid.rowbytes;
		for (j = 0 ; j < vid.width ; j++)
		{
			*p++ = host_basepal[vid.buffer[rowp]*3+2];  //qbism Baker tute
			*p++ = host_basepal[vid.buffer[rowp]*3+1];
			*p++ = host_basepal[vid.buffer[rowp]*3+0];
			rowp++;
		}
	}

	Capture_WriteVideo (buffer);

	free (buffer);
}

void Movie_TransferStereo16 (void)
{
	if (!Movie_IsActive())
		return;

	// Copy last audio chunk written into our temporary buffer
	memcpy (capture_audio_samples + (captured_audio_samples << 1), snd_out, snd_linear_count * shm->channels);
	captured_audio_samples += (snd_linear_count >> 1);

	if (captured_audio_samples >= Q_rint (host_frametime * shm->speed))
	{
		// We have enough audio samples to match one frame of video
		Capture_WriteAudio (captured_audio_samples, (byte *)capture_audio_samples);
		captured_audio_samples = 0;
	}
}

qboolean Movie_GetSoundtime (void)
{
	if (!Movie_IsActive())
		return false;

	soundtime += Q_rint (host_frametime * shm->speed * (Movie_FrameTime() / host_frametime));

	return true;
}

static qboolean OnChange_capture_dir (cvar_t *var, char *string)
{
	if (Movie_IsActive())
	{
		Con_Printf ("Cannot change capture_dir whilst capturing. Use `capture_stop` to cease capturing first.\n");
		return true;
	}

	return false;
}
