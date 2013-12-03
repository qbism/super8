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
#include "version.h" //qb: displayed on demo playback
#include "quakedef.h"

//qb: jqavi - JoeQuake avi from Baker tutorial
#ifdef _WIN32
#include "s_win32/movie_avi.h"
#endif

void CL_FinishTimeDemo (void);

//DEMO_REWIND qb:  Baker change
typedef struct framepos_s
{
	long				baz;
	struct framepos_s	*next;
} framepos_t;

framepos_t	*dem_framepos = NULL;
qboolean	start_of_demo = false;
qboolean	bumper_on = false;
//DEMO_REWIND

void CL_UpdateDemoStat (int stat);

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

//qb: added from proquake 3.50
//plus Baker single player fix http://forums.inside3d.com/viewtopic.php?t=4567
// JPG 1.05 - support for recording demos after connecting to the server
byte	demo_head[3][MAX_MSGLEN];
int		demo_head_size[2];

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
    if (!cls.demoplayback)
        return;

    fclose (cls.demofile);

    cls.demoplayback = false;
    cls.demofile = NULL;
    cls.state = ca_disconnected;

    if (cls.timedemo)
        CL_FinishTimeDemo ();

#ifdef _WIN32 //qb: jqavi
    Movie_StopPlayback ();
#endif
    current_protocol = PROTOCOL_QBS8; //qb: revert back to standard protocol
}

/* JPG - need to fix up the demo message
==============
CL_FixMsg
==============
*/
void CL_FixMsg (int fix)
{
    char s1[7] = "coop 0";
    char s2[7] = "cmd xs";
    char *s;
    int match = 0;
    int c, i;

    s = fix ? s1 : s2;

    MSG_BeginReading ();
    while (1)
    {
        if (msg_badread)
            return;
        if (MSG_ReadByte () != svc_stufftext)
            return;

        while (1)
        {
            c = MSG_ReadChar();
            if (c == -1 || c == 0)
                break;
            if (c == s[match])
            {
                match++;
                if (match == 6)
                {
                    for (i = 0 ; i < 6 ; i++)
                        net_message.data[msg_readcount - 6 + i] ^= s1[i] ^ s2[i];
                    match = 0;
                }
            }
            else
                match = 0;
        }
    }
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (void)
{
    int		len;
    int		i;
    float	f;

    len = LittleLong (net_message.cursize);
    fwrite (&len, 4, 1, cls.demofile);
    for (i=0 ; i<3 ; i++)
    {
        f = LittleFloat (cl.viewangles[i]);
        fwrite (&f, 4, 1, cls.demofile);
    }
    CL_FixMsg(1); // JPG - some demo things are bad
    fwrite (net_message.data, net_message.cursize, 1, cls.demofile);
    CL_FixMsg(0); // JPG - some demo things are bad
    fflush (cls.demofile);
}

//DEMO_REWIND qb: Baker change
void PushFrameposEntry (long fbaz)
{
	framepos_t	*newf;

	newf = malloc (sizeof(framepos_t)); // Demo rewind
	newf->baz = fbaz;

	if (!dem_framepos)
	{
		newf->next = NULL;
		start_of_demo = false;
	}
	else
	{
		newf->next = dem_framepos;
	}
	dem_framepos = newf;
}

static void EraseTopEntry (void)
{
	framepos_t	*top;

	top = dem_framepos;
	dem_framepos = dem_framepos->next;
	free (top);
}//DEMO_REWIND

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage (void)
{
    int		r, i;
    float	f;

	if (cl.paused & 2) //PAUSE_DEMO - qb: Baker change
		return 0;

    if	(cls.demoplayback)
    {
        //DEMO_REWIND qb: Baker change
		if (start_of_demo && cls.demorewind)
			return 0;

		if (cls.signon < SIGNONS)	// clear stuffs if new demo
			while (dem_framepos)
				EraseTopEntry ();//DEMO_REWIND
        // decide if it is time to grab the next message
        if (cls.signon == SIGNONS)	// allways grab until fully connected
        {
            if (cls.timedemo)
            {
                if (host_framecount == cls.td_lastframe)
                    return 0;		// allready read this frame's message
                cls.td_lastframe = host_framecount;
                // if this is the second frame, grab the real td_starttime
                // so the bogus time on the first frame doesn't count
                if (host_framecount == cls.td_startframe + 1)
                    cls.td_starttime = realtime;
            }
            //DEMO_REWIND qb: Baker change
			else if (!cls.demorewind && cl.ctime <= cl.mtime[0])
				return 0;		// don't need another message yet
			else if (cls.demorewind && cl.ctime >= cl.mtime[0])
				return 0;

			// joe: fill in the stack of frames' positions
			// enable on intermission or not...?
			// NOTE: it can't handle fixed intermission views!
			if (!cls.demorewind /*&& !cl.intermission*/)
				PushFrameposEntry (ftell(cls.demofile)); //DEMO_REWIND
        }

        // get the next message
        fread (&net_message.cursize, 4, 1, cls.demofile);
        VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
        for (i=0 ; i<3 ; i++)
        {
            r = fread (&f, 4, 1, cls.demofile);
            cl.mviewangles[0][i] = LittleFloat (f);
        }

        net_message.cursize = LittleLong (net_message.cursize);
        if (net_message.cursize > MAX_MSGLEN)
            Sys_Error ("Demo message > MAX_MSGLEN");
        r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
        if (r != 1)
        {
            CL_StopPlayback ();
            return 0;
        }
//DEMO_REWIND qb: Baker change
		// joe: get out framestack's top entry
		if (cls.demorewind /*&& !cl.intermission*/)
		{
			if (dem_framepos/* && dem_framepos->baz*/)	// Baker: in theory, if this occurs we ARE at the start of the demo with demo rewind on
			{
				fseek (cls.demofile, dem_framepos->baz, SEEK_SET);
				EraseTopEntry (); // Baker: we might be able to improve this better but not right now.
			}
			if (!dem_framepos)
				bumper_on = start_of_demo = true;
		}//DEMO_REWIND
        return 1;
    }

    while (1)
    {
        r = NET_GetMessage (cls.netcon);

        if (r != 1 && r != 2)
            return r;

        // discard nop keepalive message
        if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
            Con_Printf ("<-- server to client keepalive\n");
        else
            break;
    }

    if (cls.demorecording)
        CL_WriteDemoMessage ();

    // JPG 1.05 - support for recording demos after connecting
    if (cls.signon < 2)
    {
        memcpy(demo_head[cls.signon], net_message.data, net_message.cursize);
        demo_head_size[cls.signon] = net_message.cursize;

        if (!cls.signon)
        {
            char *ch;
            int len;

            len = strlen(demo_head[0]);
            ch = strstr(demo_head[0] + len + 1, "qbism server build");
            if (ch)
                memcpy(ch, va("QBISM SERVER BUILD %s\n", BUILDVERSION), 28);
            else
            {
                ch = demo_head[0] + demo_head_size[0];
                *ch++ = svc_print;
                ch += 1 + sprintf(ch, "\02\n   demo recorded with SUPER8 BUILD %s\n", BUILDVERSION);
                demo_head_size[0] = ch - (char *) demo_head[0];
            }
        }
    }

    return r;
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
    if (cmd_source != src_command)
        return;

    if (!cls.demorecording)
    {
        Con_Printf ("Not recording a demo.\n");
        return;
    }

// write a disconnect message to the demo file
    SZ_Clear (&net_message);
    MSG_WriteByte (&net_message, svc_disconnect);
    CL_WriteDemoMessage ();

// finish up
    fclose (cls.demofile);
    cls.demofile = NULL;
    cls.demorecording = false;
    Con_Printf ("Completed demo\n");
}

void CL_Clear_Demos_Queue (void) //qb: from FQ Mark V
{
	int i;
	for (i = 0;i < MAX_DEMOS; i ++)	// Clear demo loop queue
		cls.demos[i][0] = 0;
	cls.demonum = -1;				// Set next demo to none
}


/*
====================
CL_UpdateDemoStat

qb: from MH improvement to 'record any time'
====================
*/
void CL_UpdateDemoStat (int stat)
{
    if (stat < 0 || stat >= MAX_CL_STATS) return;

    MSG_WriteByte (&net_message, svc_updatestat);
    MSG_WriteByte (&net_message, stat);
    MSG_WriteLong (&net_message, cl.stats[stat]);
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void CL_Record_f (void)
{
    int		c;
    char	name[MAX_OSPATH];
    int		track;

    if (cmd_source != src_command)
        return;

    c = Cmd_Argc();
    if (c != 2 && c != 3 && c != 4)
    {
        Con_Printf ("record <demoname> [<map> [cd track]]\n");
        return;
    }

    if (strstr(Cmd_Argv(1), ".."))
    {
        Con_Printf ("Relative pathnames are not allowed.\n");
        return;
    }

    // JPG 3.00 - consecutive demo bug
    if (cls.demorecording)
        CL_Stop_f();

    /* JPG 1.05 - got rid of this because recording after connecting is now supported
    if (c == 2 && cls.state == ca_connected)
    {
        Con_Printf("Can not record - already connected to server\nClient demo recording must be started before connecting\n");
        return;
    }
    */

    // JPG 1.05 - replaced it with this
    if (c == 2 && cls.state == ca_connected && cls.signon < 2)
    {
        Con_Printf("Can't record - try again when connected\n");
        return;
    }

// write the forced cd track number, or -1
    if (c == 4)
    {
        track = atoi(Cmd_Argv(3));
        Con_Printf ("Forcing CD track to %i\n", cls.forcetrack);
    }
    else
        track = -1;

    sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
    CL_Clear_Demos_Queue (); //qb: from FQ Mark V timedemo is a very intentional action
//
// start the map up
//
    if (c > 2)
        Cmd_ExecuteString ( va("map %s", Cmd_Argv(2)), src_command);

//
// open the demo file
//
    COM_DefaultExtension (name, ".dem");

    Con_Printf ("recording to %s.\n", name);
    cls.demofile = fopen (name, "wb");
    if (!cls.demofile)
    {
        Con_Printf ("ERROR: couldn't open.\n");
        return;
    }

    cls.forcetrack = track;
    fprintf (cls.demofile, "%i\n", cls.forcetrack);

    cls.demorecording = true;

    // JPG 1.05 - initialize the demo file if we're already connected
    if (c == 2 && cls.state == ca_connected)
    {
        byte *data = net_message.data;
        int cursize = net_message.cursize;
        int i;

        for (i = 0 ; i < 2 ; i++)
        {
            net_message.data = demo_head[i];
            net_message.cursize = demo_head_size[i];
            CL_WriteDemoMessage();
        }

        net_message.data = demo_head[2];
        SZ_Clear (&net_message);

        // current names, colors, and frag counts
        for (i=0 ; i < cl.maxclients ; i++)
        {
            MSG_WriteByte (&net_message, svc_updatename);
            MSG_WriteByte (&net_message, i);
            MSG_WriteString (&net_message, cl.scores[i].name);
            MSG_WriteByte (&net_message, svc_updatefrags);
            MSG_WriteByte (&net_message, i);
            MSG_WriteShort (&net_message, cl.scores[i].frags);
            MSG_WriteByte (&net_message, svc_updatecolors);
            MSG_WriteByte (&net_message, i);
            MSG_WriteByte (&net_message, cl.scores[i].colors);
        }

        // send all current light styles
        for (i = 0 ; i < MAX_LIGHTSTYLES ; i++)
        {
            MSG_WriteByte (&net_message, svc_lightstyle);
            MSG_WriteByte (&net_message, i);
            MSG_WriteString (&net_message, cl_lightstyle[i].map);
        }

        CL_UpdateDemoStat (STAT_TOTALSECRETS);
        CL_UpdateDemoStat (STAT_TOTALMONSTERS);
        CL_UpdateDemoStat (STAT_SECRETS);
        CL_UpdateDemoStat (STAT_MONSTERS);

         // view entity
        MSG_WriteByte (&net_message, svc_setview);
        MSG_WriteShort (&net_message, cl.viewentity);

        // signon
        MSG_WriteByte (&net_message, svc_signonnum);
        MSG_WriteByte (&net_message, 3);

        CL_WriteDemoMessage();

        // restore net_message
        net_message.data = data;
        net_message.cursize = cursize;
    }
}


/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDemo_f (void)
{
    int length; // BlackAura (11/08/2004) - Cache demo files in memory
    char	name[256];
    int c;
    qboolean neg = false;

    if (cmd_source != src_command)
        return;

    if (Cmd_Argc() != 2)
    {
        Con_Printf ("play <demoname> : plays a demo\n");
        return;
    }

//
// disconnect from server
//
    CL_Disconnect ();

    //DEMO_REWIND qb: Baker change
	// Revert
	cls.demorewind = false;
	cls.demospeed = 0; // 0 = Don't use
	bumper_on = false; //DEMO_REWIND

    // BlackAura (11/08/2004) - stop playing music
    CDAudio_Stop();

//
// open the demo file
//

    Q_strcpy (name, Cmd_Argv(1));
    COM_DefaultExtension (name, ".dem");

    Con_Printf ("Playing demo from %s.\n", name);
    length = COM_FOpenFile (name, &cls.demofile, NULL);	// 2001-09-12 Returning from which searchpath a file was loaded by Maddes
    if (!cls.demofile)
    {
        Con_Printf ("ERROR: couldn't open.\n");
        cls.demonum = -1;		// stop demo loop
        return;
    }
    cls.demoplayback = true;
    cls.state = ca_connected;
    cls.forcetrack = 0;

    // BlackAura (08-12-2002) - Changed getc to fgetc
    while ((c = fgetc(cls.demofile)) != '\n')
    {
        if (c == '-')
            neg = true;
        else
            cls.forcetrack = cls.forcetrack * 10 + (c - '0');
    }

    if (neg)
        cls.forcetrack = -cls.forcetrack;
// ZOID, fscanf is evil
//	fscanf (cls.demofile, "%i\n", &cls.forcetrack);
    if( !R_LoadPalette(r_palette.string)) //qb: load custom palette if it exists.
        R_LoadPalette("palette"); //qb: default to standard palette.
}

/*
====================
CL_FinishTimeDemo

====================
*/
void CL_FinishTimeDemo (void)
{
    int		frames;
    float	time;

    cls.timedemo = false;

// the first frame didn't count
    frames = (host_framecount - cls.td_startframe) - 1;
    time = realtime - cls.td_starttime;
    if (!time)
        time = 1;
    Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
    if (cmd_source != src_command)
        return;

    if (Cmd_Argc() != 2)
    {
        Con_Printf ("timedemo <demoname> : gets demo speeds\n");
        return;
    }
	if (key_dest != key_game) //qb: close console from FQ Mark V
		key_dest = key_game;

	CL_Clear_Demos_Queue (); //qb: from FQ Mark V - timedemo is a very intentional action
    CL_PlayDemo_f ();

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted

    cls.timedemo = true;
    cls.td_startframe = host_framecount;
    cls.td_lastframe = -1;		// get a new message this frame
}

