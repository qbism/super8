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
// console.c

#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <fcntl.h>
#include "quakedef.h"

unsigned 		con_linewidth;

float		con_cursorspeed = 4;

#define		CON_TEXTSIZE	1024*64  //was 16384  qb: 262144 per qsb

qboolean 	con_forcedup;		// because no entities to refresh

int			con_totallines;		// total lines in console scrollback
int			con_backscroll;		// lines up from bottom to display
int			con_current;		// where next message will be printed
int			con_x;				// offset in current line for next print
char		*con_text=0;

cvar_t		con_notifytime = {"con_notifytime","3", "con_notifytime[time] Sets how long messages are displayed in seconds."};		//seconds
cvar_t		con_alpha = {"con_alpha","0.5", "con_alpha[0.0 to 1.0] Console alpha."}; // Manoel Kasimier - transparent console
cvar_t		con_logcenterprint = {"con_logcenterprint", "1", "con_logcenterprint[0/1] Toggle log centerprint to console."}; //qb:  from johnfitz

#define	NUM_CON_TIMES 4
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
// for transparent notify lines

int			con_vislines;

qboolean	con_debuglog;

char		con_lastcenterstring[1024]; //qb:  johnfitz
qboolean	con_initialized;
int			con_notifylines;		// scan lines to clear for notify lines

void M_OnScreenKeyboard_Reset (void); // Manoel Kasimier - on-screen keyboard

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
    if (key_dest == key_console)
    {
        if (cls.state == ca_connected)
        {
            key_dest = key_game;
            key_lines[edit_line][1] = 0;	// clear any typing
            key_linepos = 1;
        }
        else
            M_Main_f (); // edited
    }
    else
    {
        // Manoel Kasimier
        key_dest = key_console;
        // Manoel Kasimier - begin
        Vibration_Stop (0);
        Vibration_Stop (1);
    }
    // Manoel Kasimier - end

    SCR_EndLoadingPlaque ();
    memset (con_times, 0, sizeof(con_times));
    M_OnScreenKeyboard_Reset(); // Manoel Kasimier - on-screen keyboard
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
    if (con_text)
        memset (con_text, ' ', CON_TEXTSIZE);
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
    int		i;

    for (i=0 ; i<NUM_CON_TIMES ; i++)
        con_times[i] = 0;
}


/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
    key_dest = key_message;
    team_message = false;
}


/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
    key_dest = key_message;
    team_message = true;
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
    int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
    char	tbuf[CON_TEXTSIZE];

    width = (vid.width >> 3) - 2;

    if (width == con_linewidth)
        return;

    if (width < 1)			// video hasn't been initialized yet
    {
        width = 38;
        con_linewidth = width;
        con_totallines = CON_TEXTSIZE / con_linewidth;
        memset (con_text, ' ', CON_TEXTSIZE);
    }
    else
    {
        oldwidth = con_linewidth;
        con_linewidth = width;
        oldtotallines = con_totallines;
        con_totallines = CON_TEXTSIZE / con_linewidth;
        numlines = oldtotallines;

        if (con_totallines < numlines)
            numlines = con_totallines;

        numchars = oldwidth;

        if (con_linewidth < numchars)
            numchars = con_linewidth;

        memcpy (tbuf, con_text, CON_TEXTSIZE);
        memset (con_text, ' ', CON_TEXTSIZE);

        for (i=0 ; i<numlines ; i++)
        {
            for (j=0 ; j<numchars ; j++)
            {
                con_text[(con_totallines - 1 - i) * con_linewidth + j] =
                    tbuf[((con_current - i + oldtotallines) %
                          oldtotallines) * oldwidth + j];
            }
        }

        Con_ClearNotify ();
    }

    //qb: con_backscroll = 0;	Enhanced scrollback [Fett]


    con_current = con_totallines - 1;
}

/*
================
Con_Quakebar -- johnfitz -- returns a bar of the desired length, but never wider than the console

includes a newline, unless len >= con_linewidth.
================
*/
char *Con_Quakebar (int len)
{
    static char bar[42];
    int i;

    len = min(len, sizeof(bar) - 2);
    len = min(len, con_linewidth);

    bar[0] = '\35';
    for (i = 1; i < len - 1; i++)
        bar[i] = '\36';
    bar[len-1] = '\37';

    if (len < con_linewidth)
    {
        bar[len] = '\n';
        bar[len+1] = 0;
    }
    else
        bar[len] = 0;

    return bar;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
#define MAXGAMEDIRLEN	1000
    char	temp[MAXGAMEDIRLEN+1];
    char	*t2 = "/qconsole.log";

    con_debuglog = COM_CheckParm("-condebug");

    if (con_debuglog)
    {
        if (Q_strlen (com_gamedir) < (MAXGAMEDIRLEN - Q_strlen (t2)))
        {
            sprintf (temp, "%s%s", com_gamedir, t2);
            unlink (temp);
        }
    }

    con_text = Hunk_AllocName (CON_TEXTSIZE, "context");
    memset (con_text, ' ', CON_TEXTSIZE);
    con_linewidth = -1;
    Con_CheckResize ();

    Con_Printf ("Console initialized.\n");

//
// register our commands
//
    Cvar_RegisterVariable (&con_notifytime);
    Cvar_RegisterVariable (&con_alpha); // Manoel Kasimier - transparent console
    Cvar_RegisterVariable (&con_logcenterprint); //qb: from johnfitz

    Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
    Cmd_AddCommand ("messagemode", Con_MessageMode_f);
    Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
    Cmd_AddCommand ("clear", Con_Clear_f);
    con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
    if (con_backscroll)	con_backscroll++; //qb: Enhanced scrollback [Fett]
    con_x = 0;
    con_current++;
    memset (&con_text[(con_current%con_totallines)*con_linewidth]
            , ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print (char *txt)
{
    int		y;
    int		c;
    unsigned    l;
    static int	cr;
    int		mask;

#ifdef FLASH
    {
        AS3_Val as3Str = AS3_String(txt);
        AS3_Trace(txt);
        AS3_Release(as3Str);
    }
#endif

    con_backscroll = 0;
    if (txt[0] == 1)
    {
        mask = 128;		// go to colored text
        S_LocalSound ("misc/talk.wav");
        // play talk wav
        txt++;
    }
    else if (txt[0] == 2)
    {
        mask = 128;		// go to colored text
        txt++;
    }
    else
        mask = 0;


    while ( (c = *txt) )
    {
        // count word length
        for (l=0 ; l< con_linewidth ; l++)
            if ( txt[l] <= ' ')
                break;

        // word wrap
        if (l != con_linewidth && (con_x + l > con_linewidth) )
            con_x = 0;

        txt++;

        if (cr)
        {
            con_current--;
            cr = false;
        }


        if (!con_x)
        {
            Con_Linefeed ();
            // mark time for transparent overlay
            if (con_current >= 0)
                con_times[con_current % NUM_CON_TIMES] = realtime;
        }

        switch (c)
        {
        case '\n':
            con_x = 0;
            break;

        case '\r':
            con_x = 0;
            cr = 1;
            break;

        default:	// display character and advance
            y = con_current % con_totallines;
            con_text[y*con_linewidth+con_x] = c | mask;
            con_x++;
            if (con_x >= con_linewidth)
                con_x = 0;
            break;
        }

    }
}


/*
================
Con_DebugLog
================
*/
void Con_DebugLog(char *file, char *fmt, ...)
{
    va_list argptr;
    static char data[1024];
    int fd;

    va_start(argptr, fmt);
    vsprintf(data, fmt, argptr);
    va_end(argptr);
    fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
    write(fd, data, Q_strlen(data));
    close(fd);
}


/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
#define	MAXPRINTMSG	4096
// FIXME: make a buffer size safe vsprintf?
void Con_Printf_P (char *fmt, ...)
{
    va_list		argptr;
    char		msg[MAXPRINTMSG];
    static qboolean	inupdate;

    va_start (argptr,fmt);
    vsprintf (msg,fmt,argptr);
    va_end (argptr);

// also echo to debugging console
    Sys_Printf ("%s", msg);	// also echo to debugging console

// log all messages to file
    if (con_debuglog)
        Con_DebugLog(va("%s/qconsole.log",com_gamedir), "%s", msg);

    if (!con_initialized)
        return;

    if (cls.state == ca_dedicated)
        return;		// no graphics mode

// write it to the scrollable buffer
    Con_Print (msg);

// update the screen if the console is displayed
    if (cls.signon != SIGNONS && !scr_disabled_for_loading )
    {
        // protect against infinite loop if something in SCR_UpdateScreen calls
        // Con_Printd
        if (!inupdate)
        {
            inupdate = true;
            SCR_UpdateScreen ();
            inupdate = false;
        }
    }
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (char *fmt, ...)
{
    va_list		argptr;
    char		msg[MAXPRINTMSG];

    if (!developer.value)
        return;			// don't confuse non-developers with techie stuff...

    va_start (argptr,fmt);
    vsprintf (msg,fmt,argptr);
    va_end (argptr);

    Con_Printf ("%s", msg);
}



/*
==================
Con_Printf //qb: was safeprint... now all are safeprint

Okay to call even when the screen can't be updated
==================
*/
void Con_Printf (char *fmt, ...)
{
    va_list		argptr;
    char		msg[1024];
    int			temp;

    va_start (argptr,fmt);
    vsprintf (msg,fmt,argptr);
    va_end (argptr);

    temp = scr_disabled_for_loading;
    scr_disabled_for_loading = true;
    Con_Printf_P ("%s", msg); //qb: make all safeprints
    scr_disabled_for_loading = temp;
}


/*
================
Con_CenterPrintf -- qb:  johnfitz -- pad each line with spaces to make it appear centered
================
*/
void Con_CenterPrintf (int linewidth, char *fmt, ...)
{
    va_list	argptr;
    char	msg[MAXPRINTMSG]; //the original message
    char	line[MAXPRINTMSG]; //one line from the message
    char	spaces[21]; //buffer for spaces
    char	*src, *dst;
    int		len, s;

    va_start (argptr,fmt);
    vsprintf (msg,fmt,argptr);
    va_end (argptr);

    linewidth = min (linewidth, con_linewidth);
    for (src = msg; *src; )
    {
        dst = line;
        while (*src && *src != '\n')
            *dst++ = *src++;
        *dst = 0;
        if (*src == '\n')
            src++;

        len = Q_strlen(line);
        if (len < linewidth)
        {
            s = (linewidth-len)/2;
            memset (spaces, ' ', s);
            spaces[s] = 0;
            Con_Printf ("%s%s\n", spaces, line);
        }
        else
            Con_Printf ("%s\n", line);
    }
}

/*
==================
Con_LogCenterPrint - echo centerprint message to the console  //qb: from BJP
==================
*/
void Con_LogCenterPrint (char *str)
{
	if (!con_logcenterprint.value)
		return;

	if (!strcmp(str, con_lastcenterstring))
		return; //ignore duplicates

	if (cl.gametype == GAME_DEATHMATCH && con_logcenterprint.value != 2)
		return; //don't log in deathmatch

	strcpy (con_lastcenterstring, str);

	Con_Printf (Con_Quakebar(40));
	Con_CenterPrintf (40, "%s\n", str);
	Con_Printf (Con_Quakebar(40));
	Con_ClearNotify ();
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void M_OnScreenKeyboard_Draw (int y); // Manoel Kasimier - on-screen keyboard
void Con_DrawInput (void)
{
    int		y;
    int		i;
    char	*text;

    if (key_dest != key_console)// && !con_forcedup) // Manoel Kasimier - edited
        return;		// don't draw anything

    text = key_lines[edit_line];

// add the cursor frame
    text[key_linepos] = 10+((int)(realtime*con_cursorspeed)&1);

// fill out remainder with spaces
    for (i=key_linepos+1 ; i< con_linewidth ; i++)
        text[i] = ' ';

//	prestep if horizontally scrolling
    if (key_linepos >= con_linewidth)
        text += 1 + key_linepos - con_linewidth;

// draw it
    y = con_vislines-16;

    for (i=0 ; i<con_linewidth ; i++)
        Draw_Character ( (i+1)<<3, con_vislines - 16, text[i]);

// remove cursor
    key_lines[edit_line][key_linepos] = 0;

    M_OnScreenKeyboard_Draw(vid.height/2); // Manoel Kasimier - on-screen keyboard
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
    int		x, v;
    char	*text;
    int		i;
    float	time;

    v = 0; // Manoel Kasimier - screen positioning
    for (i= con_current-NUM_CON_TIMES+1 ; i<=con_current ; i++)
    {
        if (i < 0)
            continue;
        time = con_times[i % NUM_CON_TIMES];
        if (time == 0)
            continue;
        time = realtime - time;
        if (time > con_notifytime.value)
            continue;
        text = con_text + (i % con_totallines)*con_linewidth;

        clearnotify = 0;
        if (scr_viewsize.value < 100) // Manoel Kasimier
            scr_copytop = 1;

        for (x = 0 ; x < con_linewidth ; x++)
             Draw_Character ( ((x)<<3), v, text[x]); // Manoel Kasimier - screen positioning - edited

        v += 8;
    }


    if (key_dest == key_message)
    {
        clearnotify = 0;
        if (scr_viewsize.value < 100) // Manoel Kasimier
            scr_copytop = 1;

        x = 0;

        Draw_String (0, v, "say:", false); // Manoel Kasimier - screen positioning - edited
        while(chat_buffer[x])
        {
            Draw_Character ( ((x+4)<<3), v, chat_buffer[x]); // Manoel Kasimier - screen positioning - edited
            x++;
        }
        Draw_Character ( ((x+4)<<3), v, 10+((int)(realtime*con_cursorspeed)&1)); // Manoel Kasimier - screen positioning - edited
        v += 8;
        M_OnScreenKeyboard_Draw(48); // Manoel Kasimier - on-screen keyboard
    }

    if (v > con_notifylines)
        con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
void Con_DrawConsole (int lines, qboolean drawinput)
{
    int				i, x, y;
    int				rows;
    char			*text;
    int				j;
    int	sb; //qb: Enhanced scrollback [Fett]



    if (lines <= 0)
        return;

// draw the background
    Draw_ConsoleBackground (lines);

// draw the text
    con_vislines = lines;

    rows = ((lines-16)>>3) +1;		// rows of text to draw // Manoel Kasimier - edited
    y = lines - 16 - (rows<<3);	// may start slightly negative
    //qb: Enhanced scrollback [Fett] begin
    if (con_backscroll)
        sb=2;
    else
        sb=0;
    // Enhanced scrollback [Fett] end

    //qb: for (i= con_current - rows + 1 ; i<=con_current ; i++, y+=8 )
    for (i= con_current - rows + 1 ; i<=con_current - sb ; i++, y+=8) //qb: Enhanced scrollback [Fett]

    {
        j = i - con_backscroll;
        if (j<0)
            j = 0;
        text = con_text + (j % con_totallines)*con_linewidth;

        for (x=0 ; x<con_linewidth ; x++)
            Draw_Character ( (x+1)<<3, y, text[x]);
    }
    //qb: Enhanced scrollback [Fett] begin
    if (sb)	// are we scrolled back?
    {
        y+=8; // skip a line
        // draw arrows to show the buffer is backscrolled
        for (x=0 ; x<con_linewidth ; x+=4)
            Draw_Character ((x+1)<<3, y, '^');
    }
    // Enhanced scrollback [Fett] end

// draw the input prompt, user text, and cursor if desired
    if (drawinput)
        Con_DrawInput ();
}


/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
    double		t1, t2;

// during startup for sound / cd warnings
    Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

    Con_Printf (text);

    Con_Printf ("Press a key.\n");
    Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

    key_count = -2;		// wait for a key down and up
    key_dest = key_console;

    do
    {
        t1 = Sys_DoubleTime ();
        SCR_UpdateScreen ();
        Sys_SendKeyEvents ();
        t2 = Sys_DoubleTime ();
        realtime += t2-t1;		// make the cursor blink
    }
    while (key_count < 0);

    Con_Printf ("\n");
    key_dest = key_game;
    realtime = 0;				// put the cursor back to invisible
}

