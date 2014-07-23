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

//
// these are the key numbers that should be passed to Key_Event
//
#define	K_TAB			9
#define	K_ENTER			13
#define	K_ESCAPE		27
#define	K_SPACE			32

// normal keys should be passed as lowercased ascii

#define	K_BACKSPACE		127
#define	K_UPARROW		128
#define	K_DOWNARROW		129
#define	K_LEFTARROW		130
#define	K_RIGHTARROW	131

#define	K_ALT			132
#define	K_CTRL			133
#define	K_SHIFT			134
#define	K_F1			135
#define	K_F2			136
#define	K_F3			137
#define	K_F4			138
#define	K_F5			139
#define	K_F6			140
#define	K_F7			141
#define	K_F8			142
#define	K_F9			143
#define	K_F10			144
#define	K_F11			145
#define	K_F12			146
#define	K_INS			147
#define	K_DEL			148
#define	K_PGDN			149
#define	K_PGUP			150
#define	K_HOME			151
#define	K_END			152

#define K_PAUSE			255

//
// mouse buttons generate virtual keys
//
#define	K_MOUSE1		200
#define	K_MOUSE2		201
#define	K_MOUSE3		202

//
// joystick buttons
//
#define	K_JOY1			203
#define	K_JOY2			204
#define	K_JOY3			205
#define	K_JOY4			206

//
// aux keys are for multi-buttoned joysticks to generate so they can use
// the normal binding process
//
#define	K_AUX1			207
#define	K_AUX2			208
#define	K_AUX3			209
#define	K_AUX4			210
#define	K_AUX5			211
#define	K_AUX6			212
#define	K_AUX7			213
#define	K_AUX8			214
#define	K_AUX9			215
#define	K_AUX10			216
#define	K_AUX11			217
#define	K_AUX12			218
#define	K_AUX13			219
#define	K_AUX14			220
#define	K_AUX15			221
#define	K_AUX16			222
#define	K_AUX17			223
#define	K_AUX18			224
#define	K_AUX19			225
#define	K_AUX20			226
#define	K_AUX21			227
#define	K_AUX22			228
#define	K_AUX23			229
#define	K_AUX24			230
#define	K_AUX25			231
#define	K_AUX26			232
#define	K_AUX27			233
#define	K_AUX28			234
#define	K_AUX29			235
#define	K_AUX30			236
#define	K_AUX31			237
#define	K_AUX32			238

// JACK: Intellimouse(c) Mouse Wheel Support

#define K_MWHEELUP		239
#define K_MWHEELDOWN	240

// MH - POV hat support - keep it separate from the joy buttons
#define K_POV1		241
#define K_POV2		242
#define K_POV3		243
#define K_POV4		244

/* Extra mouse buttons */
#define K_MOUSE4		245
#define K_MOUSE5		246
#define K_MOUSE6		247
#define K_MOUSE7		248
#define K_MOUSE8		249

// Manoel Kasimier - for testing purposes - begin
#if 1
#define	K_DC_A			K_JOY1
#define	K_DC_B			K_JOY2
#define K_DC_C			K_JOY3
#define	K_DC_X			K_JOY4
#define	K_DC_Y			K_AUX5
#define K_DC_Z			K_AUX6
#define	K_DC_LTRIG		K_AUX7
#define	K_DC_RTRIG		K_AUX8
#define	K_DC_START		K_AUX10
#define	K_DPAD1_LEFT	K_AUX13
#define	K_DPAD1_DOWN	K_AUX14
#define	K_DPAD1_RIGHT	K_AUX15
#define	K_DPAD1_UP		K_AUX16
#endif
// Manoel Kasimier - for testing purposes - end

//qb: qrack command line begin
#define	MAXCMDLINE		256

//qb: qrack command line end

typedef enum {key_game, key_console, key_message, key_menu} keydest_t;

extern qboolean shift_function; // Manoel Kasimier - function shift
extern keydest_t	key_dest;
extern char *keybindings[256];
extern char *shiftbindings[256]; // Manoel Kasimier - function shift
extern	int		key_repeats[256];
extern	int		key_count;			// incremented every key event
extern	int		key_lastpress;

void Key_Event (int key, qboolean down);
void Key_Init (void);
void Key_WriteBindings (FILE *f);
void Key_SetBinding (int keynum, char *binding);
void Key_SetShiftBinding (int keynum, char *binding); // Manoel Kasimier - function shift
void Key_ClearStates (void);
int Key_StringToKeynum (char *str); // Manoel Kasimier

// Manoel Kasimier - on-screen keyboard - begin
int M_OnScreenKeyboard_Key (int key);
void M_OnScreenKeyboard_Reset (void);
// Manoel Kasimier - on-screen keyboard - end
