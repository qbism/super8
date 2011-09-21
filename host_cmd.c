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

#include "quakedef.h"


extern cvar_t	pausable;

int	current_skill;

cvar_t sv_cheats = {"sv_cheats", "0"};//qbism sv_cheats similar to DP
qboolean allowcheats = false; //qbism sv_cheat similar to DP

void Mod_Print (void);

/*
==================
Host_Quit_f
==================
*/

extern void M_Menu_Quit_f (void);

void Host_Quit_f (void)
{
	/* // Manoel Kasimier - "quit" always quits - removed - begin
	if (key_dest != key_console && cls.state != ca_dedicated)
	{
	M_Menu_Quit_f ();
	return;
	}
	*/ // Manoel Kasimier - "quit" always quits - removed - end

	CL_Disconnect ();
	Host_ShutdownServer(false);
	// Manoel Kasimier - begin

	{
		vrect_t vrect;
		vrect.pnext = NULL;
		vrect.x = vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;

		Draw_Fill (0, 0, vid.width, vid.height, 0);
		VID_Update (&vrect);

		Draw_Fill (0, 0, vid.width, vid.height, 0);
		VID_Update (&vrect);
	}

	// Manoel Kasimier - end

	Sys_Quit ();
}


/*
==================
Host_Status_f
==================
*/
void Host_Status_f (void)
{
	client_t	*client;
	int			seconds;
	int			minutes;
	int			hours = 0;
	int			j;
	void		(*print) (char *fmt, ...);

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print = Con_Printf;
	}
	else
		print = SV_ClientPrintf;

	print ("host:    %s\n", Cvar_VariableString ("hostname"));
	print ("build: %i\n", VERSION);
	if (tcpipAvailable)
		print ("tcp/ip:  %s\n", my_tcpip_address);
	if (ipxAvailable)
		print ("ipx:     %s\n", my_ipx_address);
	print ("map:     %s\n", sv.name);
	print ("players: %i active (%i max)\n\n", net_activeconnections, svs.maxclients);
	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
	{
		if (!client->active)
			continue;
		seconds = (int)(net_time - client->netconnection->connecttime);
		minutes = seconds / 60;
		if (minutes)
		{
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		}
		else
			hours = 0;
		print ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j+1, client->name, (int)client->edict->v.frags, hours, minutes, seconds);
		print ("   %s\n", client->netconnection->address);
	}
}

// FrikaC - qcexec function - begin
/*
==================
Host_QC_Exec

Execute QC commands from the console
==================
*/
extern cvar_t sv_qcexec; // Manoel Kasimier
void Host_QC_Exec (void)
{
	dfunction_t *f;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}
	if (!(sv_qcexec.value || developer.value)) // Manoel Kasimier
		return;
	if ((f = ED_FindFunction(Cmd_Argv(1))) != NULL)
	{

		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram ((func_t)(f - pr_functions));
	}
	else
		Con_Printf("bad function \"%s\"\n", Cmd_Argv(1)); // Manoel Kasimier - edited

}
// FrikaC - qcexec function - end

/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}
	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;
    	if (!allowcheats)
	{
		SV_ClientPrintf("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	sv_player->v.flags = (int)sv_player->v.flags ^ FL_GODMODE;
	if (!((int)sv_player->v.flags & FL_GODMODE) )
		SV_ClientPrintf ("godmode OFF\n");
	else
		SV_ClientPrintf ("godmode ON\n");
}

void Host_Notarget_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (!allowcheats)
	{
		SV_ClientPrintf("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}
	sv_player->v.flags = (int)sv_player->v.flags ^ FL_NOTARGET;
	if (!((int)sv_player->v.flags & FL_NOTARGET) )
		SV_ClientPrintf ("notarget OFF\n");
	else
		SV_ClientPrintf ("notarget ON\n");
}

qboolean noclip_anglehack;

void Host_Noclip_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (!allowcheats)
	{
		SV_ClientPrintf("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}
	if (sv_player->v.movetype != MOVETYPE_NOCLIP)
	{
		noclip_anglehack = true;
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		SV_ClientPrintf ("noclip ON\n");
	}
	else
	{
		noclip_anglehack = false;
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf ("noclip OFF\n");
	}
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
void Host_Fly_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

    if (!allowcheats)
	{
		SV_ClientPrintf("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}
	if (sv_player->v.movetype != MOVETYPE_FLY)
	{
		sv_player->v.movetype = MOVETYPE_FLY;
		SV_ClientPrintf ("flymode ON\n");
	}
	else
	{
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf ("flymode OFF\n");
	}
}


/*
==================
Host_Ping_f

==================
*/
void Host_Ping_f (void)
{
	int		i, j;
	float	total;
	client_t	*client;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	SV_ClientPrintf ("Client ping times:\n");
	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		total = 0;
		for (j=0 ; j<NUM_PING_TIMES ; j++)
			total+=client->ping_times[j];
		total /= NUM_PING_TIMES;
		SV_ClientPrintf ("%4i %s\n", (int)(total*1000), client->name);
	}
}

/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/


/*
======================
Host_Map_f

handle a
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
void Host_Map_f (void)
{
	int		i;
	char	name[MAX_QPATH];

	if (cmd_source != src_command)
		return;
	cls.demonum = -1;		// stop demo loop in case this fails

	CL_Disconnect ();
	Host_ShutdownServer(false);

	key_dest = key_game;			// remove console or menu
	SCR_BeginLoadingPlaque ();

	cls.mapstring[0] = 0;
	for (i=0 ; i<Cmd_Argc() ; i++)
	{
		Q_strcat (cls.mapstring, Cmd_Argv(i));
		Q_strcat (cls.mapstring, " ");
	}
	Q_strcat (cls.mapstring, "\n");

	svs.serverflags = 0;			// haven't completed an episode yet
	allowcheats = sv_cheats.value;

	Q_strcpy (name, Cmd_Argv(1));

	SV_SpawnServer (name);

	if (!sv.active)
		return;

	if (cls.state != ca_dedicated)
	{
		Q_strcpy (cls.spawnparms, "");

		for (i=2 ; i<Cmd_Argc() ; i++)
		{
			Q_strcat (cls.spawnparms, Cmd_Argv(i));
			Q_strcat (cls.spawnparms, " ");
		}

		Cmd_ExecuteString ("connect local", src_command);
	}
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
void Host_Changelevel_f (void)
{

	char	level[MAX_QPATH];
	int		i;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("changelevel <levelname> : continue game on a new level\n");
		return;
	}
	if (!sv.active || cls.demoplayback)
	{
		Con_Printf ("Only the server may changelevel\n");
		return;
	}

	//qbism:  johnfitz begin -- check for client having map before anything else //qbism
	sprintf (level, "maps/%s.bsp", Cmd_Argv(1));
	if (COM_OpenFile (level, &i, NULL) == -1)
		Host_Error ("cannot find map %s", level);
	//qbism:  johnfitz end

	SV_SaveSpawnparms ();
	Q_strcpy (level, Cmd_Argv(1));
	// Manoel Kasimier - map transition lists - begin
	{
		char	str[MAX_QPATH*3];
		int		i, r;
		FILE	*f;

		if (pr_global_struct->deathmatch)
			f = fopen (va("%s/maps/maps_dm.txt", com_gamedir), "r");
		else
			f = fopen (va("%s/maps/maps_sp.txt", com_gamedir), "r");
		if (f)
		{
			while (!feof(f))
			{
				for (i=0 ; true ; i++)
				{
					r = fgetc (f);
					if (r == EOF || !r || r == '\n')
						break;
					if (i >= sizeof(str)-1) // end of string
						continue;
					if (i > 0)
						if (str[i-1] == '/' && r == '/') // start of comment
							str[i-1] = 0;
					str[i] = r;
				}
				str[i] = 0;
				Cmd_TokenizeString(str);
				if (Cmd_Argc() < 3)
					continue;
				if (!Q_strcmp(Cmd_Argv(0), sv.name))
				{
					if (!Q_strcmp(Cmd_Argv(1), level))
					{
						Q_strcpy (level, Cmd_Argv(2));
						break;
					}
					else if (!Q_strcmp(Cmd_Argv(1), "*"))
					{
						Q_strcpy (level, Cmd_Argv(2));
						break;
					}
				}
			}
			fclose (f);
		}
	}

	// Manoel Kasimier - map transition lists - end
	allowcheats = sv_cheats.value;
	SV_SpawnServer (level);
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
void Host_Restart_f (void)
{
	char	mapname[MAX_QPATH];

	if (cls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;
	Q_strcpy (mapname, sv.name);	// must copy out, because it gets cleared
	// in sv_spawnserver
allowcheats = sv_cheats.value;
	SV_SpawnServer (mapname);
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
void Host_Reconnect_f (void)
{
	SCR_BeginLoadingPlaque ();
	cls.signon = 0;		// need new connection messages
}

/*
=====================
Host_Connect_f

User command to connect to server
=====================
*/
void Host_Connect_f (void)
{
	char	name[MAX_QPATH];

	cls.demonum = -1;		// stop demo loop in case this fails
	if (cls.demoplayback)
	{
		CL_StopPlayback ();
		CL_Disconnect ();
	}
	Q_strcpy (name, Cmd_Argv(1));
	CL_EstablishConnection (name);
	Host_Reconnect_f ();
}


/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#define	SAVEGAME_VERSION	5

/*
===============
Host_SavegameComment

Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current
===============
*/
void Host_SavegameComment (char *text)
{
	int		i;
	char	kills[20];

	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		text[i] = ' ';
	memcpy (text, cl.levelname, min(strlen(cl.levelname),22)); //qbism- johnfitz -- only copy 22 chars.
	sprintf (kills,"kills:%3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy (text+22, kills, Q_strlen(kills));
	// convert space to _ to make stdio happy
	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		if (text[i] == ' ')
			text[i] = '_';
	text[SAVEGAME_COMMENT_LENGTH] = '\0';
}

// -------------------------------------------------
// BlackAura (08-12-2002) - Replacing fscanf (start)
// -------------------------------------------------
/*static*/ void DC_ScanString(FILE *file, char *string)
{
	char newchar;
	fread(&newchar, 1, 1, file);
	while(newchar != '\n')
	{
		*string++ = newchar;
		fread(&newchar, 1, 1, file);
	}
	*string++ = '\0';
}

/*static*/ int DC_ScanInt(FILE *file)
{
	char sbuf[32768];
	DC_ScanString(file, sbuf);
	return Q_atoi(sbuf);
}

/*static*/ float DC_ScanFloat(FILE *file)
{
	char sbuf[32768];
	DC_ScanString(file, sbuf);
	return Q_atof(sbuf);
}
// -------------------------------------------------
//  BlackAura (08-12-2002) - Replacing fscanf (end)
// -------------------------------------------------

//======================================
//
// Manoel Kasimier - small saves - begin
//
//======================================
char *MK_cleanftos (float f); // Manoel Kasimier - reduced savegames

/*
====================
Host_SmallSavegame_f
====================
*/
void Host_SmallSavegame_f (void)
{
	char	name[256];
	FILE	*f;
	int		i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (cmd_source != src_command)
		return;

	if (!sv.active)
	{
		Con_Printf ("Not playing a local game.\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Printf ("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1)
	{
		Con_Printf ("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("savesmall <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && (svs.clients[i].edict->v.health <= 0) )
		{
			Con_Printf ("Can't savegame with a dead player\n");
			return;
		}
	}

	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));

	COM_DefaultExtension (name, ".GAM");

	Con_Printf ("Saving game to %s...\n", name);
#ifdef FLASH
	f = as3OpenWriteFile(name);
#else
	f = fopen (name, "wb"); // Manoel Kasimier - reduced savegames - edited
#endif

	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	fprintf (f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (comment);
	fprintf (f, "%s\n", comment);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		fprintf (f, "%s\n", MK_cleanftos(svs.clients->spawn_parms[i])); // Manoel Kasimier - reduced savegames
	fprintf (f, "%d\n", current_skill);
	fprintf (f, "%s\n", sv.name);
	fprintf (f, "%i\n", svs.serverflags); // serverflags

	fclose (f);
	Con_Printf ("done.\n");
}


/*
====================
Host_SmallLoadgame_f
====================
*/
float	smallsave_parms[NUM_SPAWN_PARMS];
void Host_SmallLoadgame_f (void)
{
	int		i;
	char	name[MAX_QPATH];
	FILE	*f;
	char	str[32768];
	int		version;

	if (cmd_source != src_command)
		return;

	cls.demonum = -1;		// stop demo loop in case this fails

	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".GAM");
	Con_Printf ("Loading game from %s...\n", name);
	f = fopen (name, "r");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}
	CL_Disconnect ();
	Host_ShutdownServer(false);

	version = DC_ScanInt(f); // Replacing fscanf
	if (version != SAVEGAME_VERSION)
	{
		fclose (f);
		Con_Printf ("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}
	Cvar_SetValue ("deathmatch", 0);
	Cvar_SetValue ("coop", 0);
	Cvar_SetValue ("teamplay", 0);

	DC_ScanString(f, str); // Replacing fscanf
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		smallsave_parms[i] = DC_ScanFloat(f); // Replacing fscanf
	current_skill = DC_ScanInt(f); // fscanf (f, "%i\n", &current_skill); // Replacing fscanf
	Cvar_SetValue ("skill", current_skill);

	DC_ScanString(f, name); // Replacing fscanf
	svs.serverflags = DC_ScanInt(f); // fscanf (f, "%i\n", &svs.serverflags); // Replacing fscanf
	fclose (f);
    allowcheats = sv_cheats.value;
	SV_SpawnServer (name);
	if (!sv.active)
		return;
	if (cls.state != ca_dedicated)
	{
		CL_EstablishConnection ("local");
		Host_Reconnect_f ();
	}
}
//====================================
//
// Manoel Kasimier - small saves - end
//
//====================================


/*
===============
Host_Savegame_f
===============
*/

void Host_Savegame_f (void)
{
	char	name[256];
	FILE	*f;
	int		i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (cmd_source != src_command)
		return;

	if (!sv.active)
	{
		Con_Printf ("Not playing a local game.\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Printf ("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1)
	{
		Con_Printf ("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && (svs.clients[i].edict->v.health <= 0) )
		{
			Con_Printf ("Can't savegame with a dead player\n");
			return;
		}
	}
	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".SAV"); // Manoel Kasimier - upper-case savegame filenames - edited
	Con_Printf ("Saving game to %s...\n", name);
#ifdef FLASH
	f = as3OpenWriteFile(name);
#else
	f = fopen (name, "wb"); // Manoel Kasimier - reduced savegames - edited
#endif
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	fprintf (f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (comment);
	fprintf (f, "%s\n", comment);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		fprintf (f, "%s\n", MK_cleanftos(svs.clients->spawn_parms[i])); // Manoel Kasimier - reduced savegames
	fprintf (f, "%d\n", current_skill);
	fprintf (f, "%s\n", sv.name);

	fprintf (f, "%s\n", MK_cleanftos(sv.time)); // Manoel Kasimier - reduced savegames
	fprintf (f, "%i\n", svs.serverflags); // Manoel Kasimier - serverflags bugfix

	// write the light styles

	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.lightstyles[i])
			fprintf (f, "%s\n", sv.lightstyles[i]);
		else
			fprintf (f,"m\n");
	}


	ED_WriteGlobals (f);
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ED_Write (f, EDICT_NUM(i));
		fflush (f);
	}
	fclose (f);
	Con_Printf ("done.\n");
}


/*
===============
Host_Loadgame_f
===============
*/
void Host_Loadgame_f (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;
	char	mapname[MAX_QPATH];
	float	time, tfloat;
	char	str[32768], *start;
	int		i;//, r; // Manoel Kasimier - removed
	char	r;		// Manoel Kasimier - VMU saves
	edict_t	*ent;
	int		entnum;
	int		version;
	float			spawn_parms[NUM_SPAWN_PARMS];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("load <savename> : load a game\n");
		return;
	}

	cls.demonum = -1;		// stop demo loop in case this fails

	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".SAV"); // Manoel Kasimier - upper-case savegame filenames - edited

	// we can't call SCR_BeginLoadingPlaque, because too much stack space has
	// been used.  The menu calls it before stuffing loadgame command
	//	SCR_BeginLoadingPlaque ();

	Con_Printf ("Loading game from %s...\n", name);
	f = fopen (name, "r");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	version = DC_ScanInt(f); // BlackAura - Replacing fscanf
	//	fscanf (f, "%i\n", &version);
	if (version != SAVEGAME_VERSION)
	{
		fclose (f);
		Con_Printf ("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}
	DC_ScanString(f, str); // BlackAura - Replacing fscanf
	//	fscanf (f, "%s\n", str);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		spawn_parms[i] = DC_ScanFloat(f); // BlackAura - Replacing fscanf
	//	fscanf (f, "%f\n", &spawn_parms[i]);

	// this silliness is so we can load 1.06 save files, which have float skill values
	tfloat = DC_ScanFloat(f); // BlackAura (08-12-2002) - Replacing fscanf
	//	fscanf (f, "%f\n", &tfloat);
	current_skill = (int)(tfloat + 0.1);
	Cvar_SetValue ("skill", (float)current_skill);

	Cvar_SetValue ("deathmatch", 0);
	Cvar_SetValue ("coop", 0);
	Cvar_SetValue ("teamplay", 0);

	DC_ScanString(f, mapname); // BlackAura (08-12-2002) - Replacing fscanf
	//	fscanf (f, "%s\n",mapname);
	time = DC_ScanFloat(f); // BlackAura (08-12-2002) - Replacing fscanf
	svs.serverflags = DC_ScanInt(f); // Replacing fscanf
	CL_Disconnect_f ();
    allowcheats = sv_cheats.value;
	SV_SpawnServer (mapname);
	if (!sv.active)
	{
		Con_Printf ("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

	// load the light styles

	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		DC_ScanString(f, str); // BlackAura (08-12-2002) - Replacing fscanf
		//	fscanf (f, "%s\n", str);
		sv.lightstyles[i] = Hunk_Alloc (Q_strlen(str)+1);
		Q_strcpy (sv.lightstyles[i], str);
	}

	// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	while (!feof(f))
	{
		for (i=0 ; i<sizeof(str)-1 ; i++)
		{
			r = fgetc (f);
			if (r == EOF || !r)
				break;
			str[i] = r;
			if (r == '}')
			{
				i++;
				break;
			}
		}
		if (i == sizeof(str)-1)
			Sys_Error ("Loadgame buffer overflow");
		str[i] = 0;
		start = str;
		start = COM_Parse(str);
		if (!com_token[0])
			break;		// end of file
		if (Q_strcmp(com_token,"{"))
		{
			Host_Error ("First token isn't a brace"); // Manoel Kasimier - edited
			return; // Manoel Kasimier
		}

		if (entnum == -1)
		{	// parse the global vars
			ED_ParseGlobals (start);
		}
		else
		{	// parse an edict

			ent = EDICT_NUM(entnum);
			memset (&ent->v, 0, progs->entityfields * 4);
			ent->free = false;
			ED_ParseEdict (start, ent);

			// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	sv.num_edicts = entnum;
	sv.time = time;

	fclose (f);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	if (cls.state != ca_dedicated)
	{
		CL_EstablishConnection ("local");
		Host_Reconnect_f ();
	}
}



//============================================================================



/*
======================
Host_Name_f
======================
*/
void Host_Name_f (void)
{
	char	*newName;
	unsigned int		i;	// 2001-09-09 Names with carriage return fix by Charles "Psykotik" Pence

	if (Cmd_Argc () == 1)
	{
		Con_Printf ("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}
	if (Cmd_Argc () == 2)
		newName = Cmd_Argv(1);
	else
		newName = Cmd_Args();
	newName[15] = 0;

// 2001-09-09 Names with carriage return fix by Charles "Psykotik" Pence
	for (i = 0 ; i < Q_strlen(newName) ; i++)
	{
		if (newName[i] == '\n')
		{
			newName[i] = '_';
		}
	}
// 2001-09-09 Names with carriage return fix by Charles "Psykotik" Pence
	if (cmd_source == src_command)
	{
		if (Q_strcmp(cl_name.string, newName) == 0)
			return;
		Cvar_Set ("_cl_name", newName);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (svs.maxclients > 1) // Manoel Kasimier
	if (host_client->name[0] && Q_strcmp(host_client->name, "unconnected") )
		if (Q_strcmp(host_client->name, newName) != 0)
			Con_Printf ("%s renamed to %s\n", host_client->name, newName);
	Q_strcpy (host_client->name, newName);
	host_client->edict->v.netname = host_client->name - pr_strings;

// send notification to all clients

	MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, host_client->name);
}

void Host_Version_f (void)
{
	Con_Printf ("Version %i\n", VERSION);  //qbism - changed to integer to match build #
	Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
}


//qbism TTS?
void SV_ClientSayf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	vsprintf (string, fmt,argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_say);
	MSG_WriteString (&host_client->message, string);
}


void Host_Say(qboolean teamonly)
{
	client_t *client;
	client_t *save;
	int		j;
	char	*p;
	char	text[64];
	qboolean	fromServer = false;

	if (cmd_source == src_command)
	{
		if (cls.state == ca_dedicated)
		{
			fromServer = true;
			teamonly = false;
		}
		else
		{
			Cmd_ForwardToServer ();
			return;
		}
	}

	if (Cmd_Argc () < 2)
		return;

	save = host_client;

	p = Cmd_Args();
	// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[Q_strlen(p)-1] = 0;
	}

	// turn on color set 1
	if (!fromServer)
		sprintf (text, "%c%s: ", 1, save->name);
	else
		sprintf (text, "%c<%s> ", 1, hostname.string);

	j = sizeof(text) - 2 - Q_strlen(text);  // -2 for /n and null terminator
	if (Q_strlen(p) > j)
		p[j] = 0;

	Q_strcat (text, p);
	Q_strcat (text, "\n");

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client || !client->active || !client->spawned)
			continue;
		if (teamplay.value && teamonly && client->edict->v.team != save->edict->v.team)
			continue;
		host_client = client;
		SV_ClientSayf("%s", text); //qbism TTS
	}
	host_client = save;

	Sys_Printf("%s", &text[1]);
}


void Host_Say_f(void)
{
	Host_Say(false);
}


void Host_Say_Team_f(void)
{
	Host_Say(true);
}


void Host_Tell_f(void)
{
	// 2001-09-09 Tell command accepts player number by Maddes  start
	char		*who;
	char		*message = NULL;
	qboolean	byNumber = false;
	// 2001-09-09 Tell command accepts player number by Maddes  end
	client_t *client;
	client_t *save;
	int		j;
	// 2001-09-09 Tell command accepts player number by Maddes  start
	/*
	char	*p;
	char	text[64];
	*/
	// 2001-09-09 Tell command accepts player number by Maddes  end

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	// 2001-09-09 Tell command accepts player number by Maddes  start
	if (Cmd_Argc () < 3)
	{
		SV_ClientPrintf ("Syntax: tell <playername|# playernumber> <message>\n");
		return;
	}

	if (Q_strcmp(Cmd_Argv(1), "#") == 0)	// user stated a player number
	{
		if (Cmd_Argc() < 4)
		{
			SV_ClientPrintf ("Syntax: tell # <playernumber> <message>\n");
			return;
		}
		j = Q_atof(Cmd_Argv(2)) - 1;
		if (j < 0 || j >= svs.maxclients)
			return;
		client = &svs.clients[j];
		if (!client->active || !client->spawned)
			return;
		byNumber = true;
	}
	else
	{
		// 2001-09-09 Tell command accepts player number by Maddes  end
		if (Cmd_Argc () < 3)
			return;

		// 2001-09-09 Tell command accepts player number by Maddes  start
		/*
		Q_strcpy(text, host_client->name);
		Q_strcat(text, ": ");

		p = Cmd_Args();

		// remove quotes if present
		if (*p == '"')
		{
		p++;
		p[Q_strlen(p)-1] = 0;
		}

		// check length & truncate if necessary
		j = sizeof(text) - 2 - Q_strlen(text);  // -2 for /n and null terminator
		if (Q_strlen(p) > j)
		p[j] = 0;

		Q_strcat (text, p);
		Q_strcat (text, "\n");

		save = host_client;
		*/
		// 2001-09-09 Tell command accepts player number by Maddes  end
		for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
		{
			if (!client->active || !client->spawned)
				continue;
			if (Q_strcasecmp(client->name, Cmd_Argv(1)))
				continue;
			// 2001-09-09 Tell command accepts player number by Maddes  start
			/*
			host_client = client;
			SV_ClientPrintf("%s", text);
			*/
			// 2001-09-09 Tell command accepts player number by Maddes  end
			break;
		}
		// 2001-09-09 Tell command accepts player number by Maddes  start
	}

	if (j >= svs.maxclients)
	{
		return;
	}

	// can't tell yourself!
	if (host_client == client)
		return;

	who = host_client->name;

	message = COM_Parse(Cmd_Args());
	if (byNumber)
	{
		message++;							// skip the #
		while (*message == ' ')				// skip white space
			message++;
		message += Q_strlen(Cmd_Argv(2));	// skip the number
	}
	while (*message && *message == ' ')
		message++;

	save = host_client;

	host_client = client;
	SV_ClientPrintf ("%s: %s\n", who, message);
	// 2001-09-09 Tell command accepts player number by Maddes  end
	host_client = save;
}


/*
==================
Host_Color_f
==================
*/
void Host_Color_f(void)
{
	int		top, bottom;
	int		playercolor;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"color\" is \"%i %i\"\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 0x0f);
		Con_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	playercolor = top*16 + bottom;

	if (cmd_source == src_command)
	{
		Cvar_SetValue ("_cl_color", playercolor);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	host_client->colors = playercolor;
	host_client->edict->v.team = bottom + 1;

	// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
}

/*
==================
Host_Kill_f
==================
*/
void Host_Kill_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	// 2001-09-09 Kill command does not work for zombie players fix by Maddes  start
	//	if (sv_player->v.health <= 0)
	if ((sv_player->v.health <= 0) && (sv_player->v.deadflag != DEAD_NO))
		// 2001-09-09 Kill command does not work for zombie players fix by Maddes  end
	{
		SV_ClientPrintf ("Can't suicide -- allready dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientKill);
}


/*
==================
Host_Pause_f
==================
*/
void Host_Pause_f (void)
{

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}
	if (!pausable.value)
		SV_ClientPrintf ("Pause not allowed.\n");
	else
	{
		sv.paused ^= 1;

		if (svs.maxclients > 1 || key_dest != key_game) // Manoel Kasimier
		{ // Manoel Kasimier
			if (sv.paused)
			{
				SV_BroadcastPrintf ("%s paused the game\n", pr_strings + sv_player->v.netname);
			}
			else
			{
				SV_BroadcastPrintf ("%s unpaused the game\n",pr_strings + sv_player->v.netname);
			}
		} // Manoel Kasimier

		// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte (&sv.reliable_datagram, sv.paused);
	}
}

//===========================================================================


/*
==================
Host_PreSpawn_f
==================
*/
void Host_PreSpawn_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	SZ_Write (&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 2);
	Con_DPrintf ("Host prespawn message 2 sent\n");
	host_client->sendsignon = true;
}

/*
==================
Host_Spawn_f
==================
*/
void Host_Spawn_f (void)
{
	int		i;
	client_t	*client;
	edict_t	*ent;

	if (cmd_source == src_command)
	{
		Con_Printf ("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("Spawn not valid -- allready spawned\n");
		return;
	}
	// run the entrance script
	if (sv.loadgame)
	{	// loaded games are fully inited allready
		// if this is the last client to be connected, unpause
		sv.paused = false;
	}
	else
	{
		// set up the edict
		ent = host_client->edict;

		memset (&ent->v, 0, progs->entityfields * 4);
		ent->v.colormap = NUM_FOR_EDICT(ent);
		ent->v.team = (host_client->colors & 15) + 1;
		ent->v.netname = host_client->name - pr_strings;

		// copy spawn parms out of the client_t

		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			// Manoel Kasimier - small saves - begin
			if (smallsave_parms[i])
			{
				(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i] = smallsave_parms[i];
				smallsave_parms[i] = 0;
			}
			else
				// Manoel Kasimier - small saves - end
				(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram (pr_global_struct->ClientConnect);

		if ((Sys_DoubleTime() - host_client->netconnection->connecttime) <= sv.time)
			Sys_Printf ("%s entered the game\n", host_client->name);

		PR_ExecuteProgram (pr_global_struct->PutClientInServer);
	}


	// send all current names, colors, and frag counts
	SZ_Clear (&host_client->message);

	// send time of update
	MSG_WriteByte (&host_client->message, svc_time);
	MSG_WriteFloat (&host_client->message, sv.time);

	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		MSG_WriteByte (&host_client->message, svc_updatename);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, client->name);
		MSG_WriteByte (&host_client->message, svc_updatefrags);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteShort (&host_client->message, client->old_frags);
		MSG_WriteByte (&host_client->message, svc_updatecolors);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteByte (&host_client->message, client->colors);
	}

	// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		MSG_WriteByte (&host_client->message, svc_lightstyle);
		MSG_WriteByte (&host_client->message, (char)i);
		MSG_WriteString (&host_client->message, sv.lightstyles[i]);
	}

	//
	// send some stats
	//
	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->killed_monsters);


	//
	// send a fixangle
	// Never send a roll angle, because savegames can catch the server
	// in a state where it is expecting the client to correct the angle
	// and it won't happen if the game was just loaded, so you wind up
	// with a permanent head tilt
	ent = EDICT_NUM( 1 + (host_client - svs.clients) );
	MSG_WriteByte (&host_client->message, svc_setangle);
	for (i=0 ; i < 2 ; i++)
		MSG_WriteAngle (&host_client->message, ent->v.angles[i] );
	MSG_WriteAngle (&host_client->message, 0 );

	SV_WriteClientdataToMessage (sv_player, &host_client->message);

	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 3);
	Con_DPrintf ("Host prespawn message 3 sent\n");
	host_client->sendsignon = true;
}

/*
==================
Host_Begin_f
==================
*/
void Host_Begin_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
}

//===========================================================================


/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
void Host_Kick_f (void)
{
	char		*who;
	char		*message = NULL;
	client_t	*save;
	int			i;
	qboolean	byNumber = false;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
	}
	else if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	save = host_client;

	if (Cmd_Argc() > 2 && Q_strcmp(Cmd_Argv(1), "#") == 0)
	{
		i = Q_atof(Cmd_Argv(2)) - 1;
		if (i < 0 || i >= svs.maxclients)
			return;
		if (!svs.clients[i].active)
			return;
		host_client = &svs.clients[i];
		byNumber = true;
	}
	else
	{
		for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
		{
			if (!host_client->active)
				continue;
			if (Q_strcasecmp(host_client->name, Cmd_Argv(1)) == 0)
				break;
		}
	}

	if (i < svs.maxclients)
	{
		if (cmd_source == src_command)
		{	// 1999-12-24 explicit brackets by Maddes
			if (cls.state == ca_dedicated)
				who = "Console";
			else
				who = cl_name.string;
		}	// 1999-12-24 explicit brackets by Maddes
		else
			who = save->name;

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc() > 2)
		{
			message = COM_Parse(Cmd_Args());
			if (byNumber)
			{
				message++;							// skip the #
				while (*message == ' ')				// skip white space
					message++;
				message += Q_strlen(Cmd_Argv(2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf ("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf ("Kicked by %s\n", who);
		SV_DropClient (false);
	}

	host_client = save;
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

/*
==================
Host_Give_f
==================
*/
void Host_Give_f (void)
{
	char	*t;
	int		v;
	eval_t	*val;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (!allowcheats)
	{
		SV_ClientPrintf("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	t = Cmd_Argv(1);
	v = atoi (Cmd_Argv(2));

	switch (t[0])
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		// MED 01/04/97 added hipnotic give stuff
		if (hipnotic)
		{
			if (t[0] == '6')
			{
				if (t[1] == 'a')
					sv_player->v.items = (int)sv_player->v.items | HIT_PROXIMITY_GUN;
				else
					sv_player->v.items = (int)sv_player->v.items | IT_GRENADE_LAUNCHER;
			}
			else if (t[0] == '9')
				sv_player->v.items = (int)sv_player->v.items | HIT_LASER_CANNON;
			else if (t[0] == '0')
				sv_player->v.items = (int)sv_player->v.items | HIT_MJOLNIR;
			else if (t[0] >= '2')
				sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		else
		{
			if (t[0] >= '2')
				sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		break;

	case 'a':
		if (rogue)
		{
			val = GetEdictFieldValue(sv_player, "armorvalue1");
			if (val)
				val->_float = v;
		}

		sv_player->v.armorvalue = v;
		break;
	case 's':
		if (rogue)
		{
			val = GetEdictFieldValue(sv_player, "ammo_shells1");
			if (val)
				val->_float = v;
		}

		sv_player->v.ammo_shells = v;
		break;
	case 'n':
		if (rogue)
		{
			val = GetEdictFieldValue(sv_player, "ammo_nails1");
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_nails = v;
			}
		}
		else
		{
			sv_player->v.ammo_nails = v;
		}
		break;
	case 'l':
		if (rogue)
		{
			val = GetEdictFieldValue(sv_player, "ammo_lava_nails");
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_nails = v;
			}
		}
		break;
	case 'r':
		if (rogue)
		{
			val = GetEdictFieldValue(sv_player, "ammo_rockets1");
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_rockets = v;
			}
		}
		else
		{
			sv_player->v.ammo_rockets = v;
		}
		break;
	case 'm':
		if (rogue)
		{
			val = GetEdictFieldValue(sv_player, "ammo_multi_rockets");
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_rockets = v;
			}
		}
		break;
	case 'h':
		sv_player->v.health = v;
		break;
	case 'c':
		if (rogue)
		{
			val = GetEdictFieldValue(sv_player, "ammo_cells1");
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_cells = v;
			}
		}
		else
		{
			sv_player->v.ammo_cells = v;
		}
		break;
	case 'p':
		if (rogue)
		{
			val = GetEdictFieldValue(sv_player, "ammo_plasma");
			if (val)
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_cells = v;
			}
		}
		break;
	}
}

edict_t	*FindViewthing (void)
{
	int		i;
	edict_t	*e;

	for (i=0 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		if ( !Q_strcmp (pr_strings + e->v.classname, "viewthing") )
			return e;
	}
	Con_Printf ("No viewthing on map\n");
	return NULL;
}

/*
==================
Host_Viewmodel_f
==================
*/
void Host_Viewmodel_f (void)
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = Mod_ForName (Cmd_Argv(1), false);
	if (!m)
	{
		Con_Printf ("Can't load %s\n", Cmd_Argv(1));
		return;
	}

	e->v.frame = 0;
	cl.model_precache[(int)e->v.modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f (void)
{
	edict_t	*e;
	int		f;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes-1;

	e->v.frame = f;
}


void PrintFrameName (model_t *m, int frame)
{
	aliashdr_t 			*hdr;
	maliasframedesc_t	*pframedesc;

	hdr = (aliashdr_t *)Mod_Extradata (m);
	if (!hdr)
		return;
	pframedesc = &hdr->frames[frame];

	Con_Printf ("frame %i: %s\n", frame, pframedesc->name);
}

/*
==================
Host_Viewnext_f
==================
*/
void Host_Viewnext_f (void)
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame + 1;
	if (e->v.frame >= m->numframes)
		e->v.frame = m->numframes - 1;

	PrintFrameName (m, e->v.frame);
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f (void)
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame - 1;
	if (e->v.frame < 0)
		e->v.frame = 0;

	PrintFrameName (m, e->v.frame);
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


/*
==================
Host_Startdemos_f
==================
*/
void Host_Startdemos_f (void)
{
	int		i, c;
	// Manoel Kasimier - begin
	int		i2=0;
	static int democount;
	FILE	*f;
	char	name[256];
	// Manoel Kasimier - end

	if (cls.state == ca_dedicated)
	{
		if (!sv.active)
			Cbuf_AddText ("map start\n");
		return;
	}

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	// Manoel Kasimier - begin
	else if (!c)
	{
		for (i=1 ; i<democount+1 ; i++)
		{
			Con_Printf (cls.demos[i-1]);
			Con_Printf ("\n");
		}
		Con_Printf ("%i demo(s) in loop\n", democount);
		return;
	}
	// clear demo list
	for (i=0 ; i<democount ; i++)
		//	Z_Free (cls.demos[i]);
		cls.demos[i][0] = 0;
	// don't add invalid demos to the list
	for (i=1 ; i<c+1 ; i++)
	{
		Q_strcpy (name, Cmd_Argv(i));
		COM_DefaultExtension (name, ".dem");
		COM_FOpenFile (name, &f, NULL);
		if (!f)
		{
			i2++;
			Con_Printf (va("startdemos: couldn't find %s\n", name));
			continue;
		}
		fclose (f);
		f = NULL;
		Q_strncpy (cls.demos[i-i2-1], Cmd_Argv(i), sizeof(cls.demos[0])-1);
	}
	democount = c-i2;
	// Manoel Kasimier - end

	//	for (i=1 ; i<c+1 ; i++) // Manoel Kasimier - removed
	//		Q_strncpy (cls.demos[i-1], Cmd_Argv(i), sizeof(cls.demos[0])-1); // Manoel Kasimier - removed

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback)
	{
		cls.demonum = 0;
		CL_NextDemo ();
	}
	else
		cls.demonum = -1;
}


/*
==================
Host_Demos_f

Return to looping demos
==================
*/
void Host_Demos_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (cls.demonum == -1)
		cls.demonum = 0;//1; // Manoel Kasimier - edited
	CL_Disconnect_f ();
	CL_NextDemo ();
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
void Host_Stopdemo_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (!cls.demoplayback)
		return;
	CL_StopPlayback ();
	CL_Disconnect ();
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
	Cmd_AddCommand ("status", Host_Status_f);
	Cmd_AddCommand ("quit", Host_Quit_f);
	Cmd_AddCommand ("god", Host_God_f);
	Cmd_AddCommand ("notarget", Host_Notarget_f);
	Cmd_AddCommand ("fly", Host_Fly_f);
	Cmd_AddCommand ("map", Host_Map_f);
	Cmd_AddCommand ("restart", Host_Restart_f);
	Cmd_AddCommand ("changelevel", Host_Changelevel_f);
	Cmd_AddCommand ("connect", Host_Connect_f);
	Cmd_AddCommand ("reconnect", Host_Reconnect_f);
	Cmd_AddCommand ("name", Host_Name_f);
	Cmd_AddCommand ("noclip", Host_Noclip_f);
	Cmd_AddCommand ("version", Host_Version_f);
	Cmd_AddCommand ("say", Host_Say_f);
	Cmd_AddCommand ("say_team", Host_Say_Team_f);
	Cmd_AddCommand ("tell", Host_Tell_f);
	Cmd_AddCommand ("color", Host_Color_f);
	Cmd_AddCommand ("kill", Host_Kill_f);
	Cmd_AddCommand ("pause", Host_Pause_f);
	Cmd_AddCommand ("spawn", Host_Spawn_f);
	Cmd_AddCommand ("begin", Host_Begin_f);
	Cmd_AddCommand ("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand ("kick", Host_Kick_f);
	Cmd_AddCommand ("ping", Host_Ping_f);
	Cmd_AddCommand ("load", Host_Loadgame_f);
	Cmd_AddCommand ("save", Host_Savegame_f);
	Cmd_AddCommand ("loadsmall", Host_SmallLoadgame_f); // Manoel Kasimier - small saves
	Cmd_AddCommand ("savesmall", Host_SmallSavegame_f); // Manoel Kasimier - small saves
	Cmd_AddCommand ("give", Host_Give_f);

	Cmd_AddCommand ("startdemos", Host_Startdemos_f);
	Cmd_AddCommand ("demos", Host_Demos_f);
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f);

	Cmd_AddCommand ("viewmodel", Host_Viewmodel_f);
	Cmd_AddCommand ("viewframe", Host_Viewframe_f);
	Cmd_AddCommand ("viewnext", Host_Viewnext_f);
	Cmd_AddCommand ("viewprev", Host_Viewprev_f);

	Cmd_AddCommand ("mcache", Mod_Print);
	Cmd_AddCommand ("qcexec", Host_QC_Exec); // FrikaC - qcexec function
}
