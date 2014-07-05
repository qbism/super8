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

int CDAudio_Init(void);
int CDAudio_Play(byte track, qboolean looping);
void CDAudio_Stop(void);
void CDAudio_Pause(void);
void CDAudio_Resume(void);
void CDAudio_Shutdown(void);
void CDAudio_Update(void);
// Manoel Kasimier - CD player in menu - begin
extern qboolean cdValid;
extern qboolean	playing;
extern qboolean	wasPlaying;
extern qboolean	initialized;
extern qboolean	enabled;
extern qboolean playLooping;
//extern float	cdvolume;
//extern byte 	remap[100];
extern byte		cdrom;
extern byte		playTrack;
extern byte		maxTrack;
extern byte audioTrack[1+99+1]; // first and last values are dummy, and should always be zero
extern	cvar_t cd_enabled;
// Manoel Kasimier - CD player in menu - end
