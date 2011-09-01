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
// cl_parse.c  -- parse a message received from the server

#include "quakedef.h"

//Dan East:
//We need to know the console char width for a modification below
extern int 		con_linewidth;

char *svc_strings[] =
{
    "svc_bad",
    "svc_nop",
    "svc_disconnect",
    "svc_updatestat",
    "svc_version",		// [long] server version
    "svc_setview",		// [short] entity number
    "svc_sound",			// <see code>
    "svc_time",			// [float] server time
    "svc_print",			// [string] null terminated string
    "svc_stufftext",		// [string] stuffed into client's console buffer
    // the string should be \n terminated
    "svc_setangle",		// [vec3] set the view angle to this absolute value

    "svc_serverinfo",		// [long] version
    // [string] signon string
    // [string]..[0]model cache [string]...[0]sounds cache
    // [string]..[0]item cache
    "svc_lightstyle",		// [byte] [string]
    "svc_updatename",		// [byte] [string]
    "svc_updatefrags",	// [byte] [short]
    "svc_clientdata",		// <shortbits + data>
    "svc_stopsound",		// <see code>
    "svc_updatecolors",	// [byte] [byte]
    "svc_particle",		// [vec3] <variable>
    "svc_damage",			// [byte] impact [byte] blood [vec3] from

    "svc_spawnstatic",
    "OBSOLETE svc_spawnbinary",
    "svc_spawnbaseline",

    "svc_temp_entity",		// <variable>
    "svc_setpause",
    "svc_signonnum",
    "svc_centerprint",
    "svc_killedmonster",
    "svc_foundsecret",
    "svc_spawnstaticsound",
    "svc_intermission",
    "svc_finale",			// [string] music [string] text
    "svc_cdtrack",			// [byte] track [byte] looptrack
    "svc_sellscreen",
    "svc_cutscene",
//qbism - from johnfitz -- new server messages
    "",	// 35
    "",	// 36
    "svc_skybox", // 37					// [string] skyname
    "", // 38
    "", // 39
    "svc_bf", // 40						// no data
    "svc_fog", // 41					// [byte] density [byte] red [byte] green [byte] blue [float] time
    "svc_spawnbaseline2", //42			// support for large modelindex, large framenum, alpha, using flags
    "svc_spawnstatic2", // 43			// support for large modelindex, large framenum, alpha, using flags
    "svc_spawnstaticsound2", //	44		// [coord3] [short] samp [byte] vol [byte] aten
    "", // 44
    "", // 45
    "", // 46
    "", // 47
    "", // 48
    "svc_say", // 49
//johnfitz
    /*50*/"svc_letterbox",
    /*51*/"svc_vibrate",
};

//=============================================================================

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
entity_t	*CL_EntityNum (int num)
{
    //johnfitz -- check minimum number too
    if (num < 0)
        Host_Error ("CL_EntityNum: %i is an invalid number",num);
    //john

    if (num >= cl.num_entities)
    {
        if (num >= MAX_EDICTS) //johnfitz -- no more MAX_EDICTS
            Host_Error ("CL_EntityNum: %i is an invalid number",num);
        while (cl.num_entities<=num)
        {
            cl_entities[cl.num_entities].colormap = vid.colormap;
            cl.num_entities++;
        }
    }

    return &cl_entities[num];
}


/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    int 	field_mask;
    float 	attenuation;
    int		i;

    field_mask = MSG_ReadByte();

    if (field_mask & SND_VOLUME)
        volume = MSG_ReadByte ();
    else
        volume = DEFAULT_SOUND_PACKET_VOLUME;

    if (field_mask & SND_ATTENUATION)
        attenuation = MSG_ReadByte () / 64.0;
    else
        attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

    //qbism: johnfitz begin
    if (field_mask & SND_LARGEENTITY)
    {
        ent = (unsigned short) MSG_ReadShort ();
        channel = MSG_ReadByte ();
    }
    else
    {
        channel = (unsigned short) MSG_ReadShort ();
        ent = channel >> 3;
        channel &= 7;
    }

    if (field_mask & SND_LARGESOUND)
        sound_num = (unsigned short) MSG_ReadShort ();
    else
        sound_num = MSG_ReadByte ();

    if (sound_num >= MAX_SOUNDS)
        Host_Error ("CL_ParseStartSoundPacket: %i > MAX_SOUNDS", sound_num);
    //qbism: johnfitz end

    if (ent > MAX_EDICTS) //qbism:  johnfitz -- no more MAX_EDICTS
        Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

    for (i=0 ; i<3 ; i++)
        pos[i] = MSG_ReadCoord ();

    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
void CL_KeepaliveMessage (void)
{
    float	    time;
    static float lastmsg;
    int		    ret;
    sizebuf_t	old;
    byte		olddata[16384]; //qbism- was 8192, could overrun as discovered by mh

    if (sv.active)
        return;		// no need if server is local
    if (cls.demoplayback)
        return;

// read messages from server, should just be nops
    old = net_message;
    memcpy (olddata, net_message.data, net_message.cursize);

    do
    {
        ret = CL_GetMessage ();
        switch (ret)
        {
        default:
            Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed");
        case 0:
            break;	// nothing waiting
        case 1:
            Host_Error ("CL_KeepaliveMessage: received a message");
            break;
        case 2:
            if (MSG_ReadByte() != svc_nop)
                Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
            break;
        }
    }
    while (ret);

    net_message = old;
    memcpy (net_message.data, olddata, net_message.cursize);

// check time
    time = Sys_DoubleTime ();
    if (time - lastmsg < 5)
        return;
    lastmsg = time;

// write out a nop
    Con_Printf ("--> client to server keepalive\n");

    MSG_WriteChar (&cls.message, clc_nop);
    NET_SendMessage (cls.netcon, &cls.message);
    SZ_Clear (&cls.message);
}


#ifdef HTTP_DOWNLOAD
/*
   =====================
   CL_WebDownloadProgress
   Callback function for webdownloads.
   Since Web_Get only returns once it's done, we have to do various things here:
   Update download percent, handle input, redraw UI and send net packets.
   =====================
*/
static int CL_WebDownloadProgress( double percent )
{
	static double time, oldtime, newtime;

	cls.download.percent = percent;
		CL_KeepaliveMessage();

	newtime = Sys_DoubleTime ();
	time = newtime - oldtime;

	Host_Frame (time);

	oldtime = newtime;

	return cls.download.disconnect; // abort if disconnect received
}
#endif



/*
==================
CL_ParseServerInfo
==================
*/
static void CL_ParseServerInfo (void) //qbism - w/ bjp modifications
{
	char	*str, tempname[MAX_QPATH];
	int		i, nummodels, numsounds;
    char	model_precache[MAX_MODELS][MAX_QPATH];
    char	sound_precache[MAX_SOUNDS][MAX_QPATH];
    byte	tmp[256]; //qbism- for Dan East pocketquake


#ifdef HTTP_DOWNLOAD //qbism - from Baker's proquake
	extern cvar_t cl_web_download;
	extern cvar_t cl_web_download_url;
	extern int Web_Get( const char *url, const char *referer, const char *name, int resume, int max_downloading_time, int timeout, int ( *_progress )(double) );
#endif

    Con_DPrintf ("Serverinfo packet received.\n");
    if (cls.signon > 0)
        Host_Error("Repeat serverinfo packet at signon %i", cls.signon);
//
// wipe the client_state_t struct
//
    CL_ClearState ();

// parse protocol version number

    current_protocol = MSG_ReadLong ("ReadLong CL_ParseServerInfo");
    if((current_protocol != PROTOCOL_NETQUAKE) && (current_protocol != PROTOCOL_QBS8)) // qbism added
    {
        Con_Printf ("\n"); //becuase there's no newline after serverinfo print
        Host_Error/*Con_Printf*/ ("Server returned version %i, not %i, %i, or %i.\n",
                                  current_protocol, PROTOCOL_NETQUAKE, PROTOCOL_QBS8);
        return;
    }
    else Con_Printf("Protocol version: %i", current_protocol);

// parse maxclients
    cl.maxclients = MSG_ReadByte ();
    if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
    {
        Con_Printf("Bad maxclients (%u) from server\n", cl.maxclients);
        return;
    }
    cl.scores = Hunk_AllocName (cl.maxclients*sizeof(*cl.scores), "scores");

// parse gametype
    cl.gametype = MSG_ReadByte ();

// parse signon message
    str = MSG_ReadString ();
    Q_strncpy (cl.levelname, str, sizeof(cl.levelname)-1);

    //Dan East: The original code hardcoded a console char width of 38 characters.  I've
    //modified this to dynamically match the console width.
    if (con_linewidth>=sizeof(tmp)) i=sizeof(tmp)-1;
    else i=con_linewidth;
    tmp[i--]='\0';
    tmp[i--]='\37';
    while (i) tmp[i--]='\36';
    tmp[i]='\35';

    Con_Printf("\n\n");
    Con_Printf(tmp);

    //Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
    Con_Printf ("%c%s\n", 2, str);

//qbism:  johnfitz -- tell user which protocol this is
    Con_Printf ("Using protocol %i\n", current_protocol);

//
// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it
//

// precache models
    memset (cl.model_precache, 0, sizeof(cl.model_precache));
    for (nummodels=1 ; ; nummodels++)
    {
        str = MSG_ReadString ();
        if (!str[0])
            break;
        if (nummodels==MAX_MODELS)
        {
            Con_Printf ("Server sent too many model precaches\n");
            return;
        }
        Q_strcpy (model_precache[nummodels], str);
        Mod_TouchModel (str);
    }

// precache sounds
    memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
    for (numsounds=1 ; ; numsounds++)
    {
        str = MSG_ReadString ();
        if (!str[0])
            break;
        if (numsounds==MAX_SOUNDS)
        {
            Con_Printf ("Server sent too many sound precaches\n");
            return;
        }
        Q_strcpy (sound_precache[numsounds], str);
        S_TouchSound (str);
    }

	{
		char	mapname[MAX_QPATH];
		COM_StripExtension (COM_SkipPath(model_precache[1]), mapname);
	}

// now we try to load everything else until a cache allocation fails
    for (i=1 ; i<nummodels ; i++)
    {
        cl.model_precache[i] = Mod_ForName (model_precache[i], false);
        if (cl.model_precache[i] == NULL)
        {
#ifdef HTTP_DOWNLOAD
			if (!cls.demoplayback && cl_web_download.value && cl_web_download_url.string)
			{
				char url[1024];
				qboolean success = false;
				char download_tempname[MAX_OSPATH],download_finalname[MAX_OSPATH];
				char folder[MAX_QPATH];
				char name[MAX_QPATH];
				extern char server_name[MAX_QPATH];
				extern int net_hostport;

				//Create the FULL path where the file should be written
				snprintf (download_tempname, sizeof(download_tempname), "%s/%s.tmp", com_gamedir, model_precache[i]);

				//determine the proper folder and create it, the OS will ignore if already exsists
				COM_GetFolder(model_precache[i],folder);// "progs/","maps/"
				snprintf (name, sizeof(name), "%s/%s", com_gamedir, folder);
				Sys_mkdir (name);

				Con_Printf( "Web downloading: %s from %s%s\n", model_precache[i], cl_web_download_url.string, model_precache[i]);

				//assign the url + path + file + extension we want
				snprintf( url, sizeof( url ), "%s%s", cl_web_download_url.string, model_precache[i]);

				cls.download.web = true;
				cls.download.disconnect = false;
				cls.download.percent = 0.0;

				SCR_EndLoadingPlaque ();

				//let libCURL do it's magic!!
				success = Web_Get(url, NULL, download_tempname, false, 600, 30, CL_WebDownloadProgress);

				cls.download.web = false;

				free(url);
				free(name);
				free(folder);

				if (success)
				{
					Con_Printf("Web download successful: %s\n", download_tempname);
					//Rename the .tmp file to the final precache filename
					snprintf (download_finalname, sizeof(download_finalname), "%s/%s", com_gamedir, model_precache[i]);
					rename (download_tempname, download_finalname);

					free(download_tempname);
					free(download_finalname);

						Cbuf_AddText (va("connect %s:%u\n",server_name,net_hostport));//reconnect after each success
						return;
					}
				else
				{
					remove (download_tempname);
					Con_Printf( "Web download of %s failed\n", download_tempname );
					return;
				}

				free(download_tempname);

				if( cls.download.disconnect )//if the user type disconnect in the middle of the download
				{
					cls.download.disconnect = false;
					CL_Disconnect_f();
					return;
				}
			} else
#endif
			{
            Con_Printf("Model %s not found\n", model_precache[i]);
            return;
        }
		}
        CL_KeepaliveMessage ();
    }

    for (i=1 ; i<numsounds ; i++)
    {
        cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);
        CL_KeepaliveMessage ();
    }

// local state
    cl_entities[0].model = cl.worldmodel = cl.model_precache[1];
    vibration_update[0] = vibration_update[1] = false; // Manoel Kasimier

    R_NewMap ();

	Hunk_Check ();		// make sure nothing is hurt

    noclip_anglehack = false;		// noclip is turned off at start
}


/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
int	bitcounts[16];

void CL_ParseUpdate (int bits)
{
    int			i;
    model_t		*model;
    int			modnum;
    qboolean	forcelink;
    entity_t	*ent;
    int			num;

    if (cls.signon == SIGNONS - 1)
    {
        // first update is the final signon stage
        cls.signon = SIGNONS;
        CL_SignonReply ();
    }

    if (bits & U_MOREBITS)
    {
        i = MSG_ReadByte ();
        bits |= (i<<8);
    }

//qbism - FITZQUAKE and QBS8 protocols
    if (bits & U_EXTEND1)
        bits |= MSG_ReadByte() << 16;
    if (bits & U_EXTEND2)
        bits |= MSG_ReadByte() << 24;

    if (bits & U_LONGENTITY)
        num = MSG_ReadShort ();
    else
        num = MSG_ReadByte ();

    ent = CL_EntityNum (num);

    for (i=0 ; i<16 ; i++)
        if (bits&(1<<i))
            bitcounts[i]++;

    if (ent->msgtime != cl.mtime[1])
        forcelink = true;	// no previous frame to lerp from
    else
        forcelink = false;

    ent->msgtime = cl.mtime[0];

    if (bits & U_MODEL)
    {
        modnum = MSG_ReadByte ();
        if (modnum >= MAX_MODELS)
            Host_Error ("CL_ParseModel: bad modnum");
    }
    else
        modnum = ent->baseline.modelindex;

    if (bits & U_FRAME)
        ent->frame = MSG_ReadByte ();
    else
        ent->frame = ent->baseline.frame;

    if (bits & U_COLORMAP)
        i = MSG_ReadByte();
    else
        i = ent->baseline.colormap;
    if (!i)
        ent->colormap = vid.colormap;
    else
    {
        if (i > cl.maxclients)
            Sys_Error ("i >= cl.maxclients");
        ent->colormap = cl.scores[i-1].translations;
    }
    if (bits & U_SKIN)
        ent->skinnum = MSG_ReadByte();
    else
        ent->skinnum = ent->baseline.skin;

    if (bits & U_EFFECTS)
    {
        if (current_protocol == PROTOCOL_QBS8)
        {
            if (bits & U_EFFECTS2)
                ent->effects = MSG_ReadLong("CL_ParseUpdate"); //qbism
            else
                ent->effects = MSG_ReadByte();
            ent->baseline.effects = ent->effects;  //qbism - also on server side
        }
        else ent->effects = MSG_ReadByte();
    }
    else  ent->effects = ent->baseline.effects;

// shift the known values for interpolation
    VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
    VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);

    if (bits & U_ORIGIN1)
        ent->msg_origins[0][0] = MSG_ReadCoord ();
    else
        ent->msg_origins[0][0] = ent->baseline.origin[0];
    if (bits & U_ANGLE1)
        ent->msg_angles[0][0] = MSG_ReadAngle();
    else
        ent->msg_angles[0][0] = ent->baseline.angles[0];

    if (bits & U_ORIGIN2)
        ent->msg_origins[0][1] = MSG_ReadCoord ();
    else
        ent->msg_origins[0][1] = ent->baseline.origin[1];
    if (bits & U_ANGLE2)
        ent->msg_angles[0][1] = MSG_ReadAngle();
    else
        ent->msg_angles[0][1] = ent->baseline.angles[1];

    if (bits & U_ORIGIN3)
        ent->msg_origins[0][2] = MSG_ReadCoord ();
    else
        ent->msg_origins[0][2] = ent->baseline.origin[2];
    if (bits & U_ANGLE3)
        ent->msg_angles[0][2] = MSG_ReadAngle();
    else
        ent->msg_angles[0][2] = ent->baseline.angles[2];

    if ( bits & U_NOLERP )
        ent->forcelink = true;

//    if (current_protocol != PROTOCOL_NETQUAKE)  //qbism - from Fitzquake
//   {
    if (bits & U_ALPHA)
        ent->alpha = MSG_ReadByte();
    else
        ent->alpha = ent->baseline.alpha;
    if (bits & U_FRAME2)
        ent->frame = (ent->frame & 0x00FF) | (MSG_ReadByte() << 8);
    if (bits & U_MODEL2)
        modnum = (modnum & 0x00FF) | (MSG_ReadByte() << 8);
    if (bits & U_LERPFINISH)  //qbism- for potential PROTOCOL_FITZQUAKE support, not implemented yet, so dump.
    {
        MSG_ReadByte(); //ent->lerpfinish = ent->msgtime + ((float)(MSG_ReadByte()) / 255);
        //ent->lerpflags |= LERP_FINISH;
    }
//   }
//   else ent->alpha = ent->baseline.alpha;

//       if (current_protocol == PROTOCOL_QBS8)  //qbism
//       {
    // Tomaz - QC Scale Glow Begin
    if (bits & U_SCALE)
        ent->scale2 = MSG_ReadFloat();
    else ent->scale2 = 1.0f;

    if (bits & U_SCALEV)
    {
        for (i=0 ; i<3 ; i++)
            ent->scalev[i] = MSG_ReadFloat();
    }
    else
        ent->scalev[0] = ent->scalev[1] = ent->scalev[2] = 1.0f;

    if (bits & U_GLOW_SIZE)
        ent->glow_size = MSG_ReadFloat();
    else
        ent->glow_size = 0;
    // Tomaz - QC Scale Glow End

    // Manoel Kasimier - QC frame_interval - begin
    if (bits & U_FRAMEINTERVAL)
        ent->frame_interval = MSG_ReadFloat();
    else
        ent->frame_interval = 0.1f;
    // Manoel Kasimier - QC frame_interval - end
//       }

    model = cl.model_precache[modnum];
    if (model != ent->model)
    {
        ent->model = model;
        // automatic animation (torches, etc) can be either all together
        // or randomized
        if (model)
        {
            if (model->synctype == ST_RAND)
                ent->syncbase = (float)(rand()&0x7fff) / 0x7fff;
            else
                ent->syncbase = 0.0;
        }
        else
            forcelink = true;	// hack to make null model players work
    }

    if ( forcelink )
    {
        // didn't have an update last message
        VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
        VectorCopy (ent->msg_origins[0], ent->origin);
        VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
        VectorCopy (ent->msg_angles[0], ent->angles);
        ent->forcelink = true;
    }
}

/*
==================
CL_ParseBaseline  //qbism-  from Fitzquake
==================
*/

void CL_ParseBaseline (entity_t *ent, int version) //johnfitz -- added argument
{
    int	i;
    int bits; //johnfitz

    //johnfitz -- PROTOCOL_FITZQUAKE
    bits = (version == 2) ? MSG_ReadByte() : 0;
    ent->baseline.modelindex = (bits & B_LARGEMODEL) ? MSG_ReadShort() : MSG_ReadByte();
    ent->baseline.frame = (bits & B_LARGEFRAME) ? MSG_ReadShort() : MSG_ReadByte();
    //johnfitz

    ent->baseline.colormap = MSG_ReadByte();
    ent->baseline.skin = MSG_ReadByte();
    for (i=0 ; i<3 ; i++)
    {
        ent->baseline.origin[i] = MSG_ReadCoord ();
        ent->baseline.angles[i] = MSG_ReadAngle ();
    }

    ent->baseline.alpha = (bits & B_ALPHA) ? MSG_ReadByte() : ENTALPHA_DEFAULT; //johnfitz -- PROTOCOL_FITZQUAKE
}

/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata (void ) //qbism read bits in function similar to johnfitz
{
    int		i, j, bits;
    float newpunchangle, oldpunchangle = Length(cl.punchangle); // Manoel Kasimier

    bits = (unsigned short)MSG_ReadShort (); //qbism:  johnfitz -- read bits here instead of in CL_ParseServerMessage()

    //qbism- from johnfitz -- PROTOCOL_FITZQUAKE
    if (bits & SU_EXTEND1)
        bits |= (MSG_ReadByte() << 16);
    if (bits & SU_EXTEND2)
        bits |= (MSG_ReadByte() << 24);
    //johnfitz

    if (bits & SU_VIEWHEIGHT)
        cl.viewheight = MSG_ReadChar ();
    else
        cl.viewheight = DEFAULT_VIEWHEIGHT;

    if (bits & SU_IDEALPITCH)
        cl.idealpitch = MSG_ReadChar ();
    else
        cl.idealpitch = 0;

    VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
    for (i=0 ; i<3 ; i++)
    {
        if (bits & (SU_PUNCH1<<i) )
            // Manoel Kasimier - 16-bit angles - begin
        {
            if (current_protocol == PROTOCOL_QBS8)
                cl.punchangle[i] = MSG_ReadAngle();
            else
                // Manoel Kasimier - 16-bit angles - end
                cl.punchangle[i] = MSG_ReadChar();
        } // Manoel Kasimier - 16-bit angles
        else
            cl.punchangle[i] = 0;
        if (bits & (SU_VELOCITY1<<i) )
            cl.mvelocity[0][i] = MSG_ReadChar()*16;
        else
            cl.mvelocity[0][i] = 0;
    }

    // Manoel Kasimier - begin
    newpunchangle = Length(cl.punchangle);
    if (newpunchangle && !cl.intermission)
    {
        if (oldpunchangle <= newpunchangle && cl_vibration.value != 2) // increasing
        {
            vibration_update[0] = true;
//			Con_Printf("%f\n", newpunchangle);
        }
    }
    // Manoel Kasimier - end

    i = MSG_ReadLong ("ReadLong CL_ParseClientdata");

    if (cl.items != i)
    {
        // set flash times
        Sbar_Changed ();
        for (j=0 ; j<32 ; j++)
            if ( (i & (1<<j)) && !(cl.items & (1<<j)))
                cl.item_gettime[j] = cl.time;
        cl.items = i;
    }

    cl.onground = (bits & SU_ONGROUND) != 0;
    cl.inwater = (bits & SU_INWATER) != 0;

    if (bits & SU_WEAPONFRAME)
        cl.stats[STAT_WEAPONFRAME] = MSG_ReadByte ();
    else
        cl.stats[STAT_WEAPONFRAME] = 0;

    if (bits & SU_ARMOR)
        i = MSG_ReadByte ();
    else
        i = 0;
    if (cl.stats[STAT_ARMOR] != i)
    {
        cl.stats[STAT_ARMOR] = i;
        Sbar_Changed ();
    }

    if (bits & SU_WEAPON)
        i = MSG_ReadByte ();
    else
        i = 0;
    if (cl.stats[STAT_WEAPON] != i)
    {
        cl.stats[STAT_WEAPON] = i;
        Sbar_Changed ();
    }

    i = MSG_ReadShort ();
    if (cl.stats[STAT_HEALTH] != i)
    {
        cl.stats[STAT_HEALTH] = i;
        Sbar_Changed ();
    }

    i = MSG_ReadByte ();
    if (cl.stats[STAT_AMMO] != i)
    {
        cl.stats[STAT_AMMO] = i;
        Sbar_Changed ();
    }

    for (i=0 ; i<4 ; i++)
    {
        j = MSG_ReadByte ();
        if (cl.stats[STAT_SHELLS+i] != j)
        {
            cl.stats[STAT_SHELLS+i] = j;
            Sbar_Changed ();
        }
    }

    i = MSG_ReadByte ();

    if (standard_quake)
    {
        if (cl.stats[STAT_ACTIVEWEAPON] != i)
        {
            cl.stats[STAT_ACTIVEWEAPON] = i;
            Sbar_Changed ();
        }
    }
    else
    {
        if (cl.stats[STAT_ACTIVEWEAPON] != (1<<i))
        {
            cl.stats[STAT_ACTIVEWEAPON] = (1<<i);
            Sbar_Changed ();
        }
    }
    //johnfitz -- PROTOCOL_FITZQUAKE
    if (bits & SU_WEAPON2)
        cl.stats[STAT_WEAPON] |= (MSG_ReadByte() << 8);
    if (bits & SU_ARMOR2)
        cl.stats[STAT_ARMOR] |= (MSG_ReadByte() << 8);
    if (bits & SU_AMMO2)
        cl.stats[STAT_AMMO] |= (MSG_ReadByte() << 8);
    if (bits & SU_SHELLS2)
        cl.stats[STAT_SHELLS] |= (MSG_ReadByte() << 8);
    if (bits & SU_NAILS2)
        cl.stats[STAT_NAILS] |= (MSG_ReadByte() << 8);
    if (bits & SU_ROCKETS2)
        cl.stats[STAT_ROCKETS] |= (MSG_ReadByte() << 8);
    if (bits & SU_CELLS2)
        cl.stats[STAT_CELLS] |= (MSG_ReadByte() << 8);
    if (bits & SU_WEAPONFRAME2)
        cl.stats[STAT_WEAPONFRAME] |= (MSG_ReadByte() << 8);
    if (bits & SU_WEAPONALPHA)
        cl.viewent.alpha = MSG_ReadByte();
    else
        cl.viewent.alpha = ENTALPHA_DEFAULT;
    //johnfitz
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation (int slot)
{
    int		i, j;
    int		top, bottom;
    byte	*dest, *source;

    if (slot > cl.maxclients)
        Sys_Error ("CL_NewTranslation: slot > cl.maxclients");
    dest = cl.scores[slot].translations;
    source = vid.colormap;
    memcpy (dest, vid.colormap, sizeof(cl.scores[slot].translations));
    top = cl.scores[slot].colors & 0xf0;
    bottom = (cl.scores[slot].colors &15)<<4;

    for (i=0 ; i<VID_GRADES ; i++, dest += 256, source+=256)
    {
        if (top < 128)	// the artists made some backwards ranges.  sigh.
            memcpy (dest + TOP_RANGE, source + top, 16);
        else
            for (j=0 ; j<16 ; j++)
                dest[TOP_RANGE+j] = source[top+15-j];

        if (bottom < 128)
            memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
        else
            for (j=0 ; j<16 ; j++)
                dest[BOTTOM_RANGE+j] = source[bottom+15-j];
    }
}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic (int version) //qbism- from johnfitz -- added a parameter
{
    entity_t *ent;

   // mh - extended static entities begin
   ent = (entity_t *) Hunk_Alloc (sizeof (entity_t));
   CL_ParseBaseline (ent, version); //qbism- from johnfitz -- added second parameter
   // mh - extended static entities end

// copy it to the current state
    ent->model = cl.model_precache[ent->baseline.modelindex];
    ent->frame = ent->baseline.frame;
    ent->colormap = vid.colormap;
    ent->skinnum = ent->baseline.skin;
    ent->effects = ent->baseline.effects;
    ent->alpha = ent->baseline.alpha; //johnfitz -- alpha
    // Manoel Kasimier - model interpolation - begin
    ent->pose1 = ent->pose2 = ent->frame;
    ent->frame_start_time = cl.time;
    ent->translate_start_time = 0;
    ent->rotate_start_time = 0;
    // Manoel Kasimier - model interpolation - end

    // Manoel Kasimier - QC Glow Scale - begin
    ent->glow_size = 0;
    ent->scale2 = 1.0f;
    ent->scalev[0] = ent->scalev[1] = ent->scalev[2] = 1.0f;

    // Manoel Kasimier - QC Glow Scale - end

    VectorCopy (ent->baseline.origin, ent->origin);
    VectorCopy (ent->baseline.angles, ent->angles);
    R_AddEfrags (ent);
}


/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (int version) //qbism- from johnfitz -- added argument
{
    vec3_t		org;
    int			sound_num, vol, atten;
    int			i;

    for (i=0 ; i<3 ; i++)
        org[i] = MSG_ReadCoord ();

    //qbism- from johnfitz -- PROTOCOL_FITZQUAKE
    if (version == 2)
        sound_num = MSG_ReadShort ();
    else
        sound_num = MSG_ReadByte ();
    //johnfitz

    vol = MSG_ReadByte ();
    atten = MSG_ReadByte ();

    S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}


#define SHOWNET(x) if(cl_shownet.value==2)Con_Printf ("%3i:%s\n", msg_readcount-1, x);

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
    int			cmd;
    int			i;
    char		*str; //qbism:  johnfitz
    int			lastcmd=0; //qbism:  johnfitz

//
// if recording demos, copy the message out
//
    if (cl_shownet.value == 1)
        Con_Printf ("%i ",net_message.cursize);
    else if (cl_shownet.value == 2)
        Con_Printf ("------------------\n");

    cl.onground = false;	// unless the server says otherwise
//
// parse the message
//
    MSG_BeginReading ();

    while (1)
    {
        if (msg_badread)
            Host_Error ("CL_ParseServerMessage: Bad server message from %s\n", msg_badread);

        cmd = MSG_ReadByte ();

        if (cmd == -1)
        {
            SHOWNET("END OF MESSAGE");
            return;		// end of message
        }

        // if the high bit of the command byte is set, it is a fast update
        if (cmd & 128) //U_SIGNAL
        {
            SHOWNET("fast update");
            CL_ParseUpdate (cmd&127);
            continue;
        }

        SHOWNET(svc_strings[cmd]);

        // other commands
        switch (cmd)
        {
        default:
            Host_Error ("CL_ParseServerMessage: Illegible server message, previous was %s\n", svc_strings[lastcmd]); //qbism:  johnfitz -- added svc_strings[lastcmd]
            break;

        case svc_nop:
//			Con_Printf ("svc_nop\n");
            break;

        case svc_time:
            cl.mtime[1] = cl.mtime[0];
            cl.mtime[0] = MSG_ReadFloat ();
            break;

        case svc_clientdata:
            CL_ParseClientdata ();  //qbism- removed bits per johnfitz
            break;

        case svc_version:
            // Manoel Kasimier - 16-bit angles - edited - begin
            current_protocol = MSG_ReadLong ("ReadLong CL_ParseServerMessage1");
            if((current_protocol != PROTOCOL_NETQUAKE) && (current_protocol != PROTOCOL_QBS8)) // added
                Host_Error ("CL_ParseServerMessage: Server is unknown protocol %i", current_protocol); //qbism
            // Manoel Kasimier - 16-bit angles - edited - end
            break;

        case svc_disconnect:
            Host_EndGame ("Server disconnected\n");

        case svc_print:
            Con_Printf ("%s", MSG_ReadString ());
            break;

        case svc_say:  //qbism svc_say TODO - find a simple TTS library...
            Con_Printf ("%s", MSG_ReadString ());
            break;

        case svc_centerprint:
            //qbism:  johnfitz begin -- log centerprints to console
            str = MSG_ReadString ();
            SCR_CenterPrint (str);
            Con_LogCenterPrint (str);
            //qbism:  johnfitz end
            break;


        case svc_stufftext:
            Cbuf_AddText (MSG_ReadString ());
            break;

        case svc_damage:
            V_ParseDamage ();
            break;

        case svc_serverinfo:
            CL_ParseServerInfo ();
            vid.recalc_refdef = true;	// leave intermission full screen
            break;

        case svc_setangle:
            for (i=0 ; i<3 ; i++)
                cl.viewangles[i] = MSG_ReadAngle ();
            break;

        case svc_setview:
            cl.viewentity = MSG_ReadShort ();
            break;

        case svc_lightstyle:
            i = MSG_ReadByte ();
            if (i >= MAX_LIGHTSTYLES)
                Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
            Q_strcpy (cl_lightstyle[i].map,  MSG_ReadString());
            cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
            break;

        case svc_sound:
            CL_ParseStartSoundPacket();
            break;


        case svc_stopsound:
            i = MSG_ReadShort();
            S_StopSound(i>>3, i&7);
            break;


        case svc_updatename:
            Sbar_Changed ();
            i = MSG_ReadByte ();
            if (i >= cl.maxclients)
                Host_Error ("CL_ParseServerMessage: svc_updatename > MAX_SCOREBOARD");
            Q_strcpy (cl.scores[i].name, MSG_ReadString ());
            break;

        case svc_updatefrags:
            Sbar_Changed ();
            i = MSG_ReadByte ();
            if (i >= cl.maxclients)
                Host_Error ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
            cl.scores[i].frags = MSG_ReadShort ();
            break;

        case svc_updatecolors:
            Sbar_Changed ();
            i = MSG_ReadByte ();
            if (i >= cl.maxclients)
                Host_Error ("CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD");
            cl.scores[i].colors = MSG_ReadByte ();
            CL_NewTranslation (i);
            break;

        case svc_particle:
            R_ParseParticleEffect ();
            break;

        case svc_spawnbaseline:
            i = MSG_ReadShort ();
            // must use CL_EntityNum() to force cl.num_entities up
            CL_ParseBaseline (CL_EntityNum(i), 1); //qbism- from johnfitz -- added second parameter
            break;
        case svc_spawnstatic:
            CL_ParseStatic (1);
            break;
        case svc_temp_entity:
            CL_ParseTEnt ();
            break;

        case svc_setpause:
        {
            cl.paused = MSG_ReadByte ();

            if (cl.paused)
            {
                CDAudio_Pause ();

#ifdef _WIN32
                VID_HandlePause (true);
#endif

                Vibration_Stop (0); // Manoel Kasimier
                Vibration_Stop (1); // Manoel Kasimier
            }
            else
            {
                CDAudio_Resume ();
#ifdef _WIN32
                VID_HandlePause (false);
#endif
            }
        }
        break;

        case svc_signonnum:
            i = MSG_ReadByte ();
            if (i <= cls.signon)
                Host_Error ("Received signon %i when at %i", i, cls.signon);
            cls.signon = i;

            CL_SignonReply ();
            break;

        case svc_killedmonster:
            cl.stats[STAT_MONSTERS]++;
            break;

        case svc_foundsecret:
            cl.stats[STAT_SECRETS]++;
            break;

        case svc_updatestat:
            i = MSG_ReadByte ();
            if (i < 0 || i >= MAX_CL_STATS)
                Sys_Error ("svc_updatestat: %i is invalid", i);
            cl.stats[i] = MSG_ReadLong ("ReadLong CL_ParseServerMessage2");
            break;

        case svc_spawnstaticsound:
            CL_ParseStaticSound (1);
            break;

        case svc_cdtrack:
            cl.cdtrack = MSG_ReadByte ();
            cl.looptrack = MSG_ReadByte ();
            if ( (cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1) )
                CDAudio_Play ((byte)cls.forcetrack, true);
            else
                CDAudio_Play ((byte)cl.cdtrack, true);
            break;

        case svc_intermission:
            cl.letterbox = 0; // Manoel Kasimier - svc_letterbox
            cl.intermission = 1;
            //cl.completed_time = cl.time;//qbism: intermission bugfix -- by joe
            cl.completed_time = cl.mtime[0];
            vid.recalc_refdef = true;	// go to full screen
            break;

        case svc_finale:
            cl.letterbox = 0; // Manoel Kasimier - svc_letterbox
            cl.intermission = 2;
            cl.completed_time = cl.time;
            vid.recalc_refdef = true;	// go to full screen
            //qbism:  johnfitz -- log centerprints to console
            str = MSG_ReadString ();
            Con_DPrintf("svc_finale: cl.intermission = 2\n");
            SCR_CenterPrint (str);
            Con_LogCenterPrint (str);
            //qbism:  johnfitz
            break;

        case svc_cutscene:
            cl.letterbox = 0; // Manoel Kasimier - svc_letterbox
            cl.intermission = 3;
            cl.completed_time = cl.time;
            vid.recalc_refdef = true;	// go to full screen
            //qbism:  johnfitz -- log centerprints to console
            str = MSG_ReadString ();
            SCR_CenterPrint (str);
            Con_LogCenterPrint (str);
            break;

        case svc_sellscreen:
            Cmd_ExecuteString ("help", src_command);
            break;

            //qbism- from johnfitz -- new svc types
        case svc_skybox:
            R_LoadSky (MSG_ReadString());
            break;

        case svc_bf:
            Cmd_ExecuteString ("bf", src_command);
            break;

        case svc_fog:
            //qbism- not implemented yet, so dump
            MSG_ReadByte(); //density = MSG_ReadByte() / 255.0;
            MSG_ReadByte(); //red = MSG_ReadByte() / 255.0;
            MSG_ReadByte(); //green = MSG_ReadByte() / 255.0;
            MSG_ReadByte(); //blue = MSG_ReadByte() / 255.0;
            MSG_ReadShort(); //time = max(0.0, MSG_ReadShort() / 100.0);
            break;

        case svc_spawnbaseline2: //PROTOCOL_FITZQUAKE
            i = MSG_ReadShort ();
            // must use CL_EntityNum() to force cl.num_entities up
            CL_ParseBaseline (CL_EntityNum(i), 2);
            break;

        case svc_spawnstatic2: //PROTOCOL_FITZQUAKE
            CL_ParseStatic (2);
            break;

        case svc_spawnstaticsound2: //PROTOCOL_FITZQUAKE
            CL_ParseStaticSound (2);
            break;
            //johnfitz


            // Manoel Kasimier - svc_letterbox - begin
        case svc_letterbox:
            cl.letterbox = (float)(MSG_ReadByte ())/100.0;
            if (cl.letterbox < 0) cl.letterbox = 0;
            if (cl.letterbox > 1) cl.letterbox = 1;
            if (cl.letterbox)
                cl.intermission = 0;
            vid.recalc_refdef = true;
            break;
            // Manoel Kasimier - svc_letterbox - end

            // Manoel Kasimier - begin
        case svc_vibrate:
        {
            byte s, e1, e2, d;
            int player;
            s = MSG_ReadByte ();
            e1 = MSG_ReadByte ();
            e2 = MSG_ReadByte ();
            d = MSG_ReadByte ();
            player = MSG_ReadByte ();
            if (player > 1)
                break; // invalid player

            vibration_update[player] = true;
        }
        break;
        // Manoel Kasimier - end
        }
        lastcmd = cmd;
    }
}

