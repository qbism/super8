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
// host.c -- coordinates spawning and killing of local servers

#include "quakedef.h"
#include "r_local.h"
#ifdef _WIN32 //qbism jqavi
#include "s_win32/movie_avi.h"
#endif

/*

A server can allways be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

// 2001-10-20 TIMESCALE extension by Tomaz/Maddes  start
double	host_org_frametime;
cvar_t	host_timescale = {"host_timescale", "0"};
// 2001-10-20 TIMESCALE extension by Tomaz/Maddes  end

void Palette_Init (void);
void BuildGammaTable (float g);
void GrabLightcolormap (void);

quakeparms_t host_parms;

qboolean	host_initialized;		// true if into command execution

double		host_frametime;
double		host_time;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run
int			host_framecount;

int			host_hunklevel;

int			minimum_memory;

client_t	*host_client;			// current client

jmp_buf 	host_abortserver;

byte		*host_basepal;
byte		*host_colormap;//qbism
byte        *alphamap, *additivemap, *fogmap; //qbism moved here
byte        *lightcolormap; //qbism

cvar_t	r_skyalpha = {"r_skyalpha","0.5"}; //0.6 Manoel Kasimier - translucent sky

cvar_t  r_palette =  {"r_palette", "s8pal", true}; //qbism- the default palette to load

cvar_t	host_framerate = {"host_framerate","0"};	// set for slow motion
cvar_t	host_speeds = {"host_speeds","0"};			// set for running times

cvar_t	sys_ticrate = {"sys_ticrate","0.05"};
cvar_t	serverprofile = {"serverprofile","0"};

cvar_t	fraglimit = {"fraglimit","0",false,true};
cvar_t	timelimit = {"timelimit","0",false,true};
cvar_t	teamplay = {"teamplay","0",false,true};

cvar_t	samelevel = {"samelevel","0"};
cvar_t	noexit = {"noexit","0",false,true};

cvar_t	developer = {"developer","0"};

cvar_t	skill = {"skill","1"};						// 0 - 3
cvar_t	deathmatch = {"deathmatch","0"};			// 0, 1, or 2
cvar_t	coop = {"coop","0"};			// 0 or 1

cvar_t	pausable = {"pausable","1"};

cvar_t	temp1 = {"temp1","0"};


/*
================
Host_EndGame
================
*/
void Host_EndGame (char *message, ...)
{
    va_list		argptr;
    char		string[1024];

    va_start (argptr,message);
    vsprintf (string,message,argptr);
    va_end (argptr);
    Con_DPrintf ("Host_EndGame: %s\n",string);

    if (sv.active)
        Host_ShutdownServer (false);

    if (cls.state == ca_dedicated)
        Sys_Error ("Host_EndGame: %s\n",string);	// dedicated servers exit

    if (cls.demonum != -1)
        CL_NextDemo ();
    else
        CL_Disconnect ();

    longjmp (host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (char *error, ...)
{
    va_list		argptr;
    char		string[1024];
    static	qboolean inerror = false;

    if (inerror)
        Sys_Error ("Host_Error: recursively entered");
    inerror = true;

    SCR_EndLoadingPlaque ();		// reenable screen updates

    va_start (argptr,error);
    vsprintf (string,error,argptr);
    va_end (argptr);
    Con_Printf ("Host_Error: %s\n",string);

    if (sv.active)
        Host_ShutdownServer (false);

    if (cls.state == ca_dedicated)
        Sys_Error ("Host_Error: %s\n",string);	// dedicated servers exit

    CL_Disconnect ();
    cls.demonum = -1;

    inerror = false;

    longjmp (host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void	Host_FindMaxClients (void)
{
    int		i;

    svs.maxclients = 1;

    i = COM_CheckParm ("-dedicated");
    if (i)
    {
        cls.state = ca_dedicated;
        if (i != (com_argc - 1))
        {
            svs.maxclients = Q_atoi (com_argv[i+1]);
        }
        else
            svs.maxclients = 16;  //qbism was 8
    }
    else
        cls.state = ca_disconnected;

    i = COM_CheckParm ("-listen");
    if (i)
    {
        if (cls.state == ca_dedicated)
            Sys_Error ("Only one of -dedicated or -listen can be specified");
        if (i != (com_argc - 1))
            svs.maxclients = Q_atoi (com_argv[i+1]);
        else
            svs.maxclients = 16;  //qbism was 8
    }
    if (svs.maxclients < 1)
        svs.maxclients = 16;  //qbism was 8
    else if (svs.maxclients > MAX_SCOREBOARD)
        svs.maxclients = MAX_SCOREBOARD;

    svs.maxclientslimit = svs.maxclients;
    if (svs.maxclientslimit < 4)
        svs.maxclientslimit = 16;  //qbism was 4
    svs.clients = Hunk_AllocName (svs.maxclientslimit*sizeof(client_t), "clients");

    if (svs.maxclients > 1)
        Cvar_SetValue ("deathmatch", 1.0);
    else
        Cvar_SetValue ("deathmatch", 0.0);
}

// Manoel Kasimier - Deathmatch/Coop not at the same time - begin
void Host_CheckDeathmatchValue (void)
{
    if (deathmatch.value && coop.value)
        Cvar_SetValue("coop", 0);
}
void Host_CheckCoopValue (void)
{
    if (deathmatch.value && coop.value)
        Cvar_SetValue("deathmatch", 0);
}
// Manoel Kasimier - Deathmatch/Coop not at the same time - end
// Manoel Kasimier - TIMESCALE extension - begin
void Host_CheckTimescaleValue (void)
{
    if (host_timescale.value < 0.0)
        Cvar_SetValue("host_timescale", 0.0);
    else if (host_timescale.value > 10.0)
        Cvar_SetValue("host_timescale", 10.0);
}
// Manoel Kasimier - TIMESCALE extension - end
/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal (void)
{
    Host_InitCommands ();

    // 2001-10-20 TIMESCALE extension by Tomaz/Maddes  start
    Cvar_RegisterVariableWithCallback (&host_timescale, Host_CheckTimescaleValue); // Manoel Kasimier
    // 2001-10-20 TIMESCALE extension by Tomaz/Maddes  end
    Cvar_RegisterVariable (&r_skyalpha); // Manoel Kasimier - translucent sky

    Cvar_RegisterVariable (&r_palette); // qbism - default palette

    Cvar_RegisterVariable (&host_framerate);
    Cvar_RegisterVariable (&host_speeds);
    Cvar_RegisterVariable (&host_timescale); //qbism - from johnfitz

    Cvar_RegisterVariable (&sys_ticrate);
    Cvar_RegisterVariable (&serverprofile);

    Cvar_RegisterVariable (&fraglimit);
    Cvar_RegisterVariable (&timelimit);
    Cvar_RegisterVariable (&teamplay);
    Cvar_RegisterVariable (&samelevel);
    Cvar_RegisterVariable (&noexit);
    Cvar_RegisterVariable (&skill);
    Cvar_RegisterVariable (&developer);
    Cvar_RegisterVariableWithCallback (&deathmatch, Host_CheckDeathmatchValue); // Manoel Kasimier - Deathmatch/Coop not at the same time - edited
    Cvar_RegisterVariableWithCallback (&coop, Host_CheckCoopValue); // Manoel Kasimier - Deathmatch/Coop not at the same time - edited

    Cvar_RegisterVariable (&pausable);

    Cvar_RegisterVariable (&temp1);

    Host_FindMaxClients ();

    host_time = 1.0;		// so a think at time 0 won't get called
}


/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration (void)
{
    FILE	*f;

    // dedicated servers initialize the host but don't parse and set the
    // config.cfg cvars
    if (host_initialized && !isDedicated) // fixed
    {
#ifdef FLASH
        f = as3OpenWriteFile(va("%s/super8.cfg",com_gamedir));
#else
        f = fopen (va("%s/super8.cfg",com_gamedir), "wb"); // Manoel Kasimier - config.cfg replacement - edited
#endif

        if (!f)
        {
            Con_Printf ("Couldn't write super8.cfg.\n"); // Manoel Kasimier - config.cfg replacement - edited
            return;
        }

        Key_WriteBindings (f);
        Cvar_WriteVariables (f);

        fclose (f);
    }
}


/*
=================
SV_ClientPrintf

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf (char *fmt, ...)
{
    va_list		argptr;
    char		string[1024];

    va_start (argptr,fmt);
    vsprintf (string, fmt,argptr);
    va_end (argptr);

    MSG_WriteByte (&host_client->message, svc_print);
    MSG_WriteString (&host_client->message, string);
}


/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (char *fmt, ...)
{
    va_list		argptr;
    char		string[1024];
    int			i;

    va_start (argptr,fmt);
    vsprintf (string, fmt,argptr);
    va_end (argptr);

    for (i=0 ; i<svs.maxclients ; i++)
        if (svs.clients[i].active && svs.clients[i].spawned)
        {
            MSG_WriteByte (&svs.clients[i].message, svc_print);
            MSG_WriteString (&svs.clients[i].message, string);
        }
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands (char *fmt, ...)
{
    va_list		argptr;
    char		string[1024];

    va_start (argptr,fmt);
    vsprintf (string, fmt,argptr);
    va_end (argptr);

    MSG_WriteByte (&host_client->message, svc_stufftext);
    MSG_WriteString (&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient (qboolean crash)
{
    int		saveSelf;
    int		i;
    client_t *client;

    if (!crash)
    {
        // send any final messages (don't check for errors)
        if (NET_CanSendMessage (host_client->netconnection))
        {
            MSG_WriteByte (&host_client->message, svc_disconnect);
            NET_SendMessage (host_client->netconnection, &host_client->message);
        }

        if (host_client->edict && host_client->spawned)
        {
            // call the prog function for removing a client
            // this will set the body to a dead frame, among other things
            saveSelf = pr_global_struct->self;
            pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
            PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
            pr_global_struct->self = saveSelf;
        }

        Sys_Printf ("Client %s removed\n",host_client->name);
    }

    // break the net connection
    NET_Close (host_client->netconnection);
    host_client->netconnection = NULL;

    // free the client (the body stays around)
    host_client->active = false;
    host_client->name[0] = 0;
    host_client->old_frags = -999999;
    net_activeconnections--;

    // send notification to all clients
    for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
    {
        if (!client->active)
            continue;
        MSG_WriteByte (&client->message, svc_updatename);
        MSG_WriteByte (&client->message, host_client - svs.clients);
        MSG_WriteString (&client->message, "");
        MSG_WriteByte (&client->message, svc_updatefrags);
        MSG_WriteByte (&client->message, host_client - svs.clients);
        MSG_WriteShort (&client->message, 0);
        MSG_WriteByte (&client->message, svc_updatecolors);
        MSG_WriteByte (&client->message, host_client - svs.clients);
        MSG_WriteByte (&client->message, 0);
    }
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(qboolean crash)
{
    int		i;
    int		count;
    sizebuf_t	buf;
    byte	message[4];
    double	start;

    if (!sv.active)
        return;

    sv.active = false;

    // stop all client sounds immediately
    if (cls.state == ca_connected)
        CL_Disconnect ();

    // flush any pending messages - like the score!!!
    start = Sys_DoubleTime();
    do
    {
        count = 0;
        for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
        {
            if (host_client->active && host_client->message.cursize)
            {
                if (NET_CanSendMessage (host_client->netconnection))
                {
                    NET_SendMessage(host_client->netconnection, &host_client->message);
                    SZ_Clear (&host_client->message);
                }
                else
                {
                    NET_GetMessage(host_client->netconnection);
                    count++;
                }
            }
        }
        if ((Sys_DoubleTime() - start) > 3.0)
            break;
    }
    while (count);

    // make sure all the clients know we're disconnecting
    buf.data = message;
    buf.maxsize = 4;
    buf.cursize = 0;
    MSG_WriteByte(&buf, svc_disconnect);
    count = NET_SendToAll(&buf, 5);
    if (count)
        Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

    for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
        if (host_client->active)
            SV_DropClient(crash);

    //
    // clear structures
    //
    memset (&sv, 0, sizeof(sv));
    memset (svs.clients, 0, svs.maxclientslimit*sizeof(client_t));
}


/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory (void)
{
    Con_DPrintf ("Clearing memory\n");
    D_FlushCaches ();
    Mod_ClearAll ();
    if (host_hunklevel)
        Hunk_FreeToLowMark (host_hunklevel);

    cls.signon = 0;
    memset (&sv, 0, sizeof(sv));
    memset (&cl, 0, sizeof(cl));
}


//============================================================================


/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/

float frame_timescale = 1.0f; //DEMO_REWIND - qbism - Baker change

qboolean Host_FilterTime (float time)
{
    double	fps;
    static double lastmaxfps;

    realtime += time;

    fps = max(10, cl_maxfps.value);

    if (!cls.timedemo && realtime - oldrealtime < 1.0/fps)
        return false;		// framerate is too high

#ifdef _WIN32 //qbism jqavi
    if (Movie_IsActive())
        host_frametime = Movie_FrameTime ();
    else
#endif

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

//qbism - fitzquake host_framerate + DEMO_REWIND Baker change
    frame_timescale = 1;
    if (cls.demoplayback && cls.demospeed && !cls.timedemo /* && !cls.capturedemo && cls.demonum == -1 */) //qb: allow for all but timedemo
    {
        host_frametime *= cls.demospeed;
        frame_timescale = cls.demospeed;
    }
    else if (host_timescale.value > 0 && !(cls.demoplayback && cls.demospeed && !cls.timedemo && !cls.capturedemo && cls.demonum == -1) )
    {
        host_frametime *= host_timescale.value;
        frame_timescale = host_timescale.value;
    }
    else if (host_framerate.value > 0)
        host_frametime = host_framerate.value;
    else // don't allow really long or short frames
        host_frametime = CLAMP (0.001, host_frametime, 0.1); //johnfitz -- use CLAMP

    return true;
}


/*
===================
Host_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
void Host_GetConsoleCommands (void)
{
    char	*cmd;

    while (1)
    {
        cmd = Sys_ConsoleInput ();
        if (!cmd)
            break;
        Cbuf_AddText (cmd);
    }
}


/*
==================
Host_ServerFrame //qbism- framerate independent physics from mh

==================
*/


void Host_ServerFrame (void)
{
    // run the world state
    pr_global_struct->frametime = host_frametime;

    // set the time and clear the general datagram
    SV_ClearDatagram ();

    // check for new clients
    SV_CheckForNewClients ();

    // read client messages
    SV_RunClients ();

    // move things around and think
    // always pause in single player if in console or menus
    if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
        SV_Physics ();

    // send all messages to the clients
    SV_SendClientMessages ();
}


/*
==================
Host_Frame

Runs all active servers
==================
*/
void _Host_Frame (float time)
{
    static double		time1 = 0;
    static double		time2 = 0;
    static double		time3 = 0;
    int			pass1, pass2, pass3;

    if (setjmp (host_abortserver) )
        return;			// something bad happened, or the server disconnected

    // keep the random time dependent
    rand ();

//qbism per mh - run input before possible host_filtertime return, improves responsiveness
// get new key events
    Sys_SendKeyEvents ();

// allow mice or other external controllers to add commands
    IN_Commands ();

// decide the simulation time
    if (!Host_FilterTime (time))
        return;         // don't run too fast, or packets will flood out

    // process console commands
    Cbuf_Execute ();

    NET_Poll();

    // if running the server locally, make intentions now
    if (sv.active)
        CL_SendCmd ();

    //-------------------
    //
    // server operations
    //
    //-------------------

    // check for commands typed to the host
    Host_GetConsoleCommands ();

    if (sv.active)
        Host_ServerFrame ();

    //-------------------
    //
    // client operations
    //
    //-------------------

    // if running the server remotely, send intentions now after
    // the incoming messages have been read
    if (!sv.active)
        CL_SendCmd ();

    host_time += host_frametime;

    // fetch results from server
    if (cls.state == ca_connected)
    {
        CL_ReadFromServer ();
    }

    // update video
    if (host_speeds.value)
        time1 = Sys_DoubleTime ();

    SCR_UpdateScreen ();

    if (host_speeds.value)
        time2 = Sys_DoubleTime ();

    // update audio
    if (cls.signon == SIGNONS)
    {
        S_Update (r_origin, vpn, vright, vup);
        CL_DecayLights ();
    }
    else
        S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

    CDAudio_Update();

    if (host_speeds.value)
    {
        pass1 = (time1 - time3)*1000;
        time3 = Sys_DoubleTime ();
        pass2 = (time2 - time1)*1000;
        pass3 = (time3 - time2)*1000;
        Con_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
                    pass1+pass2+pass3, pass1, pass2, pass3);
    }

    host_framecount++;
}

void Host_Frame (float time)
{
    double	time1, time2;
    static double	timetotal;
    static int		timecount;
    int		i, c, m;

    if (!serverprofile.value)
    {
        _Host_Frame (time);
        return;
    }

    time1 = Sys_DoubleTime ();
    _Host_Frame (time);
    time2 = Sys_DoubleTime ();

    timetotal += time2 - time1;
    timecount++;

    if (timecount < 1000)
        return;

    m = timetotal*1000/timecount;
    timecount = 0;
    timetotal = 0;
    c = 0;
    for (i=0 ; i<svs.maxclients ; i++)
    {
        if (svs.clients[i].active)
            c++;
    }

    Con_Printf ("serverprofile: %2i clients %2i msec\n",  c,  m);
}

//============================================================================


void Palette_Init (void) //qbism - idea from Engoo
{
    char path[128];
    loadedfile_t	*fileinfo;	// 2001-09-12 Returning information about loaded file by Maddes

    sprintf (path,"gfx/%s.lmp",r_palette.string);
    fileinfo = COM_LoadHunkFile (path);
    if (!fileinfo)
    {
        Con_Printf("Palette file %s not found.");
        fileinfo = COM_LoadHunkFile ("gfx/palette.lmp");
        if (!fileinfo)
            Sys_Error ("Couldn't load %s or gfx/palette.lmp", r_palette.value);
    }

    host_basepal = fileinfo->data;

//qbism - colormap, alphamap, and addivemap are generated
    host_colormap = Q_malloc(256*COLORLEVELS);
    GrabColormap();
    {
        int i;
        for (i=0; i<COLORLEVELS; i++)
            memcpy(colormap_cel+i*256, host_colormap+(i-(i%16))*256, 256); // 4 shades
    }
    alphamap = Q_malloc(256*256);
    GrabAlphamap();
    fogmap = Q_malloc(256*256);
    GrabFogmap();
    lightcolormap = Q_malloc(256*256);
    //qbism- need to do this AFTER r_init.... GrabLightcolormap();

    additivemap = Q_malloc(256*256);
    GrabAdditivemap();
}


/*
====================
Host_Init
====================
*/
pixel_t colormap_cel[256*COLORLEVELS]; // Manoel Kasimier - EF_CELSHADING
void M_Credits_f (void); // Manoel Kasimier
void SCR_Adjust (void); // Manoel Kasimier - screen positioning
void Host_Init (quakeparms_t *parms)
{
    loadedfile_t	*fileinfo;	// 2001-09-12 Returning information about loaded file by Maddes

    if (standard_quake)
        minimum_memory = MINIMUM_MEMORY;
    else
        minimum_memory = MINIMUM_MEMORY_LEVELPAK;
    if (COM_CheckParm ("-minmemory"))
        parms->memsize = minimum_memory;

    host_parms = *parms;

    if (parms->memsize < minimum_memory)
        Sys_Error ("Only %4.1f megs of memory available, can't execute game", parms->memsize / (float)0x100000);

    com_argc = parms->argc;
    com_argv = parms->argv;

    Memory_Init (parms->membase, parms->memsize);

    Cbuf_Init ();
    Cmd_Init ();
    Con_Init ();
    V_Init ();
    Chase_Init ();
//    Host_InitVCR (parms);  //qbism deleted
    COM_Init (parms->basedir);
    Host_InitLocal ();
    W_LoadWadFile ("gfx.wad");
    Key_Init ();
    M_Init ();
    PR_Init ();
    Mod_Init ();
    NET_Init ();
    SV_Init ();

    Con_DPrintf ("Exe: "__TIME__" "__DATE__"\n"); // edited
    Con_DPrintf ("%4.1f megabyte heap\n",parms->memsize/ (1024*1024.0)); // edited

    R_InitTextures ();		// needed even for dedicated servers

    if (cls.state != ca_dedicated)
    {
        Palette_Init ();  //qbism

#ifndef _WIN32 // on non win32, mouse comes before video for security reasons
        IN_Init ();
#endif
        VID_Init (host_basepal);

        Draw_Init ();
        SCR_Init ();
        R_Init ();
#ifndef	_WIN32
        // on Win32, sound initialization has to come before video initialization, so we
        // can put up a popup if the sound hardware is in use
        S_Init ();
#endif	// _WIN32

        CDAudio_Init ();
        Sbar_Init ();
        CL_Init ();
#ifdef _WIN32 // on non win32, mouse comes before video for security reasons
        IN_Init ();
#endif
        GrabLightcolormap(); //qbism
    }

    Con_DPrintf ("\n"); // Manoel Kasimier

    //SCR_Adjust(); // Manoel Kasimier - screen positioning
    //M_Credits_f(); // Manoel Kasimier //qbism: go straight to demo
    Cbuf_InsertText ("exec quake.rc\n"); //qbism: go straight to demo

    Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
    host_hunklevel = Hunk_LowMark ();

    host_initialized = true;

    Sys_Printf ("========Quake Initialized=========\n");
}


/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
    static qboolean isdown = false;

    if (isdown)
    {
        printf ("recursive shutdown\n");
        return;
    }
    isdown = true;

    // keep Con_Printf from trying to update the screen
    scr_disabled_for_loading = true;
    Host_WriteConfiguration ();
    if (con_initialized) History_Shutdown (); //qbism- Baker/ezQuake- command history
    CDAudio_Shutdown ();
    NET_Shutdown ();
    S_Shutdown();
    IN_Shutdown ();

    if (cls.state != ca_dedicated)
    {
        VID_Shutdown();
    }
}

