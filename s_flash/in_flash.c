/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// in_null.c -- for systems without a mouse

#include "../quakedef.h"
extern cvar_t m_look; // Manoel Kasimier - m_look

byte _asToQKey[256];

float	mouse_x, mouse_y;
float	old_mouse_x, old_mouse_y;

cvar_t	m_filter = {"m_filter","0"};


void IN_Init (void)
{
	Cvar_RegisterVariable (&m_filter);

	//Use mouse look by default
	m_look.value = 1;

	//Set the Actionscript to Quake keyboard mappings

	//Derived from http://flash-creations.com/notes/asclass_key.php
//Ascii()  	String  	Flash Code			Quake Code
/*27 	(esc) 	*/		_asToQKey[27]		= K_ESCAPE;
/*8 	(backspace) */	_asToQKey[8]		= K_BACKSPACE;
/*0 	(capslock)		_asToQKey[20] = */
/*0 	(shift) 	*/ _asToQKey[16] = K_SHIFT;
/*	(alt) 	*/ _asToQKey[18] = K_ALT;
/*0 	(ctrl) 	*/ _asToQKey[17] = K_CTRL;
/*13 	(enter) 	*/ _asToQKey[13] = K_ENTER;
/*32 	(space) 	*/ _asToQKey[32] = K_SPACE;
/* 		(tab)	*/		_asToQKey[9]		= K_TAB;
/*function keys */
/*0 	(F1) 	*/ _asToQKey[112] = K_F1;
/*0 	(F2) 	*/ _asToQKey[113] = K_F2;
/*0 	(F3) 	*/ _asToQKey[114] = K_F3;
/*0 	(F4) 	*/ _asToQKey[115] = K_F4;
/*0 	(F5) 	*/ _asToQKey[116] = K_F5;
/*0 	(F6) 	*/ _asToQKey[117] = K_F6;
/*0 	(F7) 	*/ _asToQKey[118] = K_F7;
/*0 	(F8) 	*/ _asToQKey[119] = K_F8;
/*0 	(F9) 	*/ _asToQKey[120] = K_F9;
/*0 	(F10) 	*/ _asToQKey[121] = K_F10;
/*0 	(F11) 	*/ _asToQKey[122] = K_F11;
/*0 	(F12) 	*/ _asToQKey[123] = K_F12;
/*keypad keys */
/*0 	(Enter) */		_asToQKey[12]		= '5';	// w/numlock off
/*13 	(Enter) 	*/ _asToQKey[13] = K_ENTER;
/*middlekeys	*/
/*0 	(ScrollLock)	_asToQKey[145] = */
/*0 	(Pause) 	*/ _asToQKey[19] = K_PAUSE;
/*0 	(Ins) 	*/ _asToQKey[45] = K_INS;
/*0 	(Home) 	*/ _asToQKey[36] = K_HOME;
/*0 	(PageUp) 	*/ _asToQKey[33] = K_PGUP;
/*127 	(Delete)*/		_asToQKey[46]		= K_DEL;
/*0 	(End) 	*/ _asToQKey[35] = K_END;
/*0 	(PageDown) 	*/ _asToQKey[34] = K_PGDN;
/*arrowkeys	*/
/*0 	(left) 	*/ _asToQKey[37] = K_LEFTARROW;
/*0 	(up) 	*/ _asToQKey[38] = K_UPARROW;
/*0 	(down) 	*/ _asToQKey[40] = K_DOWNARROW;
/*0 	(right) */ _asToQKey[39]  = K_RIGHTARROW;
}

// Manoel Kasimier - begin
void Vibration_Update (void)
{
}
void Vibration_Stop (int player) // 0=player1, 1=player2
{
}
// Manoel Kasimier - end

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

void IN_MouseMove (usercmd_t *cmd)
{
	float mx = mouse_x;
	float my = mouse_y;

	if (m_filter.value)
	{
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}
	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe.value && m_look.value )) // Manoel Kasimier - m_look - edited
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (m_look.value) // Manoel Kasimier - m_look
		V_StopPitchDrift ();

	if ( m_look.value && !(in_strafe.state & 1)) // Manoel Kasimier - m_look - edited
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}

	mouse_x = 0.0f;
	mouse_y = 0.0f;
}


void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove(cmd);
}

