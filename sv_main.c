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
// sv_main.c -- server main program

#include "quakedef.h"
#include "version.h" //qbism - generated from bat

int current_protocol = PROTOCOL_QBS8; //qbism
extern qboolean		pr_alpha_supported; //johnfitz

server_t		sv;
server_static_t	svs;

cvar_t  sv_progs = {"sv_progs", "progs.dat" }; //qbism: enginex
char	localmodels[MAX_MODELS][6];			// inline model names for precache //qbism: was 5

//============================================================================


cvar_t sv_qcexec = {"sv_qcexec","0", false, true}; // Manoel Kasimier - qcexec

/*
===============
SV_Protocol_f  //qbism:  fitzquake
===============
*/
void SV_Protocol_f (void)
{
    int i;

    switch (Cmd_Argc())
    {
    case 1:
        Con_Printf ("\"sv_protocol\" is \"%i\"\n", current_protocol);
        break;
    case 2:
        i = atoi(Cmd_Argv(1));
        if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_QBS8)
            Con_Printf ("sv_protocol must be %i or %i\n", PROTOCOL_NETQUAKE, PROTOCOL_QBS8);
        else
        {
            current_protocol = i;
            if (sv.active)
                Con_Printf ("changes will not take effect until the next level load.\n");
        }
        break;
    default:
        Con_Printf ("usage: sv_protocol <protocol>\n");
        break;
    }
}

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
    int		i;
    extern	cvar_t	sv_maxvelocity;
    extern	cvar_t	sv_cheats;  //qbism
    extern	cvar_t	sv_gravity;
    extern	cvar_t	sv_nostep;
    extern	cvar_t	sv_friction;
    extern	cvar_t	sv_edgefriction;
    extern	cvar_t	sv_stopspeed;
    extern	cvar_t	sv_maxspeed;
    extern	cvar_t	sv_accelerate;
    extern	cvar_t	sv_idealpitchscale;
    extern	cvar_t	sv_aim_h; // Manoel Kasimier - horizontal autoaim
    extern	cvar_t	sv_aim;
    extern	cvar_t	sv_enable_use_button; // Manoel Kasimier - +USE fix

    Cvar_RegisterVariable (&sv_progs); //qbism:  fitzquake
    Cvar_RegisterVariable (&sv_cheats); //qbism
    Cvar_RegisterVariable (&sv_maxvelocity);
    Cvar_RegisterVariable (&sv_gravity);
    Cvar_RegisterVariable (&sv_friction);
    Cvar_RegisterVariable (&sv_edgefriction);
    Cvar_RegisterVariable (&sv_stopspeed);
    Cvar_RegisterVariable (&sv_maxspeed);
    Cvar_RegisterVariable (&sv_accelerate);
    Cvar_RegisterVariable (&sv_idealpitchscale);
    Cvar_RegisterVariable (&sv_aim_h); // Manoel Kasimier - horizontal autoaim
    Cvar_RegisterVariable (&sv_aim);
    Cvar_RegisterVariable (&sv_nostep);
    Cvar_RegisterVariable (&sv_enable_use_button); // Manoel Kasimier - +USE fix
    Cvar_RegisterVariable (&sv_qcexec); // Manoel Kasimier - qcexec

    Cmd_AddCommand ("sv_protocol", &SV_Protocol_f); //qbism: johnfitz

    for (i=0 ; i<MAX_MODELS ; i++)
        sprintf (localmodels[i], "*%i", i);
}

/*
===============
SV_IsPaused
===============
*/
qboolean SV_IsPaused (void) //qbism from bjp
{
    return sv.paused || (svs.maxclients == 1 && key_dest != key_game);
}

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
    int		i, v;

    if (sv.datagram.cursize > MAX_DATAGRAM-16)
        return;
    MSG_WriteByte (&sv.datagram, svc_particle);
    MSG_WriteCoord (&sv.datagram, org[0]);
    MSG_WriteCoord (&sv.datagram, org[1]);
    MSG_WriteCoord (&sv.datagram, org[2]);
    for (i=0 ; i<3 ; i++)
    {
        v = dir[i]*16;
        if (v > 127)
            v = 127;
        else if (v < -128)
            v = -128;
        MSG_WriteChar (&sv.datagram, v);
    }
    MSG_WriteByte (&sv.datagram, count);
    MSG_WriteByte (&sv.datagram, color);
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

==================
*/
void SV_StartSound (edict_t *entity, int channel, char *sample, byte volume, float attenuation)
{
    int         sound_num, field_mask, i, ent;

//qbism - don't check volume range anymore, it's a byte
//   if (volume < 0 || volume > 255)

    if (attenuation < 0 || attenuation > 4)
    {
        Con_Printf ("SV_StartSound: attenuation = %g, max = %d\n", attenuation, 4);
        return;  //qbism - return instead of error - bjp
    }

    if (channel < 0 || channel > 7)
    {
        Con_Printf ("SV_StartSound: channel = %i, max = %d\n", channel, 7);
        return;  //qbism - return instead of error - bjp
    }

    if (sv.datagram.cursize > MAX_DATAGRAM-16)
        return;

// find precache number for sound
    for (sound_num=1 ; sound_num<MAX_SOUNDS && sv.sound_precache[sound_num] ; sound_num++)
        if (!strcmp(sample, sv.sound_precache[sound_num]))
            break;

    if ( sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num] )
    {
        Con_Printf ("SV_StartSound: %s not precacheed\n", sample);
        return;
    }

    ent = NUM_FOR_EDICT(entity);

    field_mask = 0;
    if (volume != DEFAULT_SOUND_PACKET_VOLUME)
        field_mask |= SND_VOLUME;
    if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
        field_mask |= SND_ATTENUATION;

    //qbism:  johnfitz begin
    if (ent >= 8192)
    {
        if (current_protocol == PROTOCOL_NETQUAKE)
            return; //don't send any info protocol can't support
        else
            field_mask |= SND_LARGEENTITY;
    }
    if (sound_num >= 256 || channel >= 8)  //qbism channel >7 is sys_error above
    {
        if (current_protocol == PROTOCOL_NETQUAKE)
            return; //don't send any info protocol can't support
        else
            field_mask |= SND_LARGESOUND;
    }
    //qbism:  johnfitz end

// directed messages go only to the entity the are targeted on
    MSG_WriteByte (&sv.datagram, svc_sound);
    MSG_WriteByte (&sv.datagram, field_mask);
    if (field_mask & SND_VOLUME)
        MSG_WriteByte (&sv.datagram, volume);
    if (field_mask & SND_ATTENUATION)
        MSG_WriteByte (&sv.datagram, attenuation*64);

    //qbism:  johnfitz begin
    if (field_mask & SND_LARGEENTITY)
    {
        MSG_WriteShort (&sv.datagram, ent);
        MSG_WriteByte (&sv.datagram, channel);
    }
    else
        MSG_WriteShort (&sv.datagram, (ent<<3) | channel);
    if (field_mask & SND_LARGESOUND)
        MSG_WriteShort (&sv.datagram, sound_num);
    else
        MSG_WriteByte (&sv.datagram, sound_num);
    //qbism:  johnfitz end

    for (i=0 ; i<3 ; i++)
        MSG_WriteCoord (&sv.datagram, entity->v.origin[i]+0.5*(entity->v.mins[i]+entity->v.maxs[i]));
}

/*  //qbism added SV_StartLocalSound
==================
SV_LocalSound
Plays a local sound to a specific client, others can't hear it.
==================
*/

void SV_LocalSound (client_t *client, char *sample, char volume)
{
    int sound_num;
    int field_mask;

// find precache number for sound
    for (sound_num=1 ; sound_num<MAX_SOUNDS && sv.sound_precache[sound_num] ; sound_num++)
        if (!strcmp(sample, sv.sound_precache[sound_num]))
            break;

    if ( sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num] )
    {
        Con_Printf ("SV_StartSound: %s not precacheed\n", sample);
        return;
    }

    field_mask = 0;
    if (volume != DEFAULT_SOUND_PACKET_VOLUME)
        field_mask |= SND_VOLUME;

    if (sound_num >= 256)  //qbism channel >7 is sys_error above
    {
        if (current_protocol == PROTOCOL_NETQUAKE)
            return; //don't send any info protocol can't support
        else
            field_mask |= SND_LARGESOUND;
    }

// directed messages go only to the entity the are targeted on
    MSG_WriteByte (&client->message, svc_localsound);
    MSG_WriteByte (&client->message, field_mask);
    if (field_mask & SND_VOLUME)
        MSG_WriteByte (&client->message, volume);

    if (field_mask & SND_LARGESOUND)
        MSG_WriteShort (&client->message, sound_num);
    else
        MSG_WriteByte (&client->message, sound_num);
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo (client_t *client)
{
    char			**s;
    char			message[2048];

    if (client->sendsignon == true)  //qbism - don't send serverinfo twice.
    {
        Con_DPrintf ("Warning, attempted to send duplicate serverinfo.\n");
        return;
    }

    MSG_WriteByte (&client->message, svc_print);
    sprintf (message, "QBISM SERVER BUILD %s\n", BUILDVERSION); //johnfitz -- include fitzquake version
    MSG_WriteString (&client->message,message);

    MSG_WriteByte (&client->message, svc_serverinfo);

    MSG_WriteLong (&client->message, current_protocol);

    MSG_WriteByte (&client->message, svs.maxclients);

    if (!coop.value && deathmatch.value)
        MSG_WriteByte (&client->message, GAME_DEATHMATCH);
    else
        MSG_WriteByte (&client->message, GAME_COOP);

    sprintf (message, pr_strings+sv.edicts->v.message);

    MSG_WriteString (&client->message,message);

    for (s = sv.model_precache+1 ; *s ; s++)
        MSG_WriteString (&client->message, *s);
    MSG_WriteByte (&client->message, 0);

    for (s = sv.sound_precache+1 ; *s ; s++)
        MSG_WriteString (&client->message, *s);
    MSG_WriteByte (&client->message, 0);

// send music
    MSG_WriteByte (&client->message, svc_cdtrack);
    MSG_WriteByte (&client->message, sv.edicts->v.sounds);
    MSG_WriteByte (&client->message, sv.edicts->v.sounds);

// set view
    MSG_WriteByte (&client->message, svc_setview);
    MSG_WriteShort (&client->message, NUM_FOR_EDICT(client->edict));

    MSG_WriteByte (&client->message, svc_signonnum);
    MSG_WriteByte (&client->message, 1);

    client->sendsignon = true;
    client->spawned = false;		// need prespawn, spawn, etc
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient (int clientnum)
{
    edict_t			*ent;
    client_t		*client;
    int				edictnum;
    struct qsocket_s *netconnection;
    int				i;
    float			spawn_parms[NUM_SPAWN_PARMS];

    client = svs.clients + clientnum;

    Con_DPrintf ("Client %s connected\n", client->netconnection->address);

    edictnum = clientnum+1;

    ent = EDICT_NUM(edictnum);

// set up the client_t
    netconnection = client->netconnection;

    if (sv.loadgame)
        memcpy (spawn_parms, client->spawn_parms, sizeof(spawn_parms));
    memset (client, 0, sizeof(*client));
    client->netconnection = netconnection;

    Q_strcpy (client->name, "unconnected");
    client->active = true;
    client->spawned = false;
    client->edict = ent;
    client->message.data = client->msgbuf;
    client->message.maxsize = sizeof(client->msgbuf);
    client->message.allowoverflow = true;		// we can catch it

#ifdef IDGODS
    client->privileged = IsID(&client->netconnection->addr);
#else
    client->privileged = false;
#endif

    if (sv.loadgame)
        memcpy (client->spawn_parms, spawn_parms, sizeof(spawn_parms));
    else
    {
        // call the progs to get default spawn parms for the new client
        PR_ExecuteProgram (pr_global_struct->SetNewParms);
        for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
            client->spawn_parms[i] = (&pr_global_struct->parm1)[i];
    }

    SV_SendServerinfo (client);

    Con_DPrintf ("client server spawned.\n");
}


/*
===================
SV_CheckForNewClients

===================
*/
void SV_CheckForNewClients (void)
{
    struct qsocket_s	*ret;
    int				i;

//
// check for new connections
//
    while (1)
    {
        ret = NET_CheckNewConnections ();
        if (!ret)
            break;

        //
        // init a new client structure
        //
        for (i=0 ; i<svs.maxclients ; i++)
            if (!svs.clients[i].active)
                break;
        if (i == svs.maxclients)
            Sys_Error ("Host_CheckForNewClients: no free clients");

        svs.clients[i].netconnection = ret;
        SV_ConnectClient (i);

        net_activeconnections++;
    }
}



/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
==================
SV_ClearDatagram

==================
*/
void SV_ClearDatagram (void)
{
    SZ_Clear (&sv.datagram);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

int		fatbytes;
byte	fatpvs[MAX_MAP_LEAFS/8];

void SV_AddToFatPVS (vec3_t org, mnode_t *node)
{
    int		i;
    byte	*pvs;
    mplane_t	*plane;
    float	d;

    while (1)
    {
        // if this is a leaf, accumulate the pvs bits
        if (node->contents < 0)
        {
            if (node->contents != CONTENTS_SOLID)
            {
                pvs = Mod_LeafPVS ( (mleaf_t *)node, sv.worldmodel);
                for (i=0 ; i<fatbytes ; i++)
                    fatpvs[i] |= pvs[i];
            }
            return;
        }

        plane = node->plane;
        d = DotProduct (org, plane->normal) - plane->dist;
        if (d > 8)
            node = node->children[0];
        else if (d < -8)
            node = node->children[1];
        else
        {
            // go down both
            SV_AddToFatPVS (org, node->children[0]);
            node = node->children[1];
        }
    }
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
byte *SV_FatPVS (vec3_t org)
{
    fatbytes = (sv.worldmodel->numleafs+31)>>3;
    memset (fatpvs, 0, fatbytes);
    SV_AddToFatPVS (org, sv.worldmodel->nodes);
    return fatpvs;
}

//=============================================================================







/*
===============
SV_FindTouchedLeafs
qbism- 'Fix - Large Brushmodel Flickering/Not Visible' by MH elaborated by Reckless, from inside3d

moved to here, deferred find until we hit actual sending to client, and
added test vs client pvs in finding.

===============
*/
void SV_FindTouchedLeafs (edict_t *ent, mnode_t *node, byte *pvs)
{
    mplane_t   *splitplane;
    int         sides;
    int         leafnum;

loc0:
    ;
    // ent already touches a leaf
    if (ent->touchleaf) return;

    // hit solid
    if (node->contents == CONTENTS_SOLID) return;

    // add an efrag if the node is a leaf
    // this is used for sending ents to the client so it needs to stay
    if (node->contents < 0)
    {
loc1:
        ;
        leafnum = ((mleaf_t *) node) - sv.worldmodel->leafs - 1;

        if ((pvs[leafnum >> 3] & (1 << (leafnum & 7))))
        {
            ent->touchleaf = true;
        }
        return;
    }

    // NODE_MIXED
    splitplane = node->plane;
    sides = BOX_ON_PLANE_SIDE (ent->v.absmin, ent->v.absmax, splitplane);

    // recurse down the contacted sides, start dropping out if we hit anything
    if ((sides & 1) && !ent->touchleaf && node->children[0]->contents != CONTENTS_SOLID)
    {
        if (!(sides & 2) && node->children[0]->contents < 0)
        {
            node = node->children[0];
            goto loc1;
        }
        else if (!(sides & 2))
        {
            node = node->children[0];
            goto loc0;
        }
        else
        {
            SV_FindTouchedLeafs (ent, node->children[0], pvs);
        }
    }

    if ((sides & 2) && !ent->touchleaf && node->children[1]->contents != CONTENTS_SOLID)
    {
        // test for a leaf and drop out if so, otherwise it's a node so go round again
        node = node->children[1];

        if (node->contents < 0)
        {
            goto loc1;
        }
        else
        {
            goto loc0;   // SV_FindTouchedLeafs (ent, node, pvs);
        }
    }
}

/*
=============
SV_WriteEntitiesToClient

=============
*/
void SV_WriteEntitiesToClient (edict_t	*clent, sizebuf_t *msg)
{
    unsigned int	i;  //qbism
    int		e;
    int		bits;
    byte	*pvs;
    vec3_t	org;
    float	miss;
    edict_t	*ent;
    // Tomaz - QC Alpha Scale Glow Begin
    float	alpha=1;
    float	scale=1;
    vec3_t	scalev= {0,0,0};
    float	glow_size=0;
    float   glow_red, glow_green, glow_blue;

    // Tomaz - QC Alpha Scale Glow End
    float	frame_interval=0.1f; // Manoel Kasimier - QC frame_interval
    int clentnum; //Team XLink DP_SV_DRAWONLYTOCLIENT & DP_SV_NODRAWTOCLIENT //qbism
    eval_t *val; //Team XLink DP_SV_DRAWONLYTOCLIENT & DP_SV_NODRAWTOCLIENT


// find the client's PVS
    VectorAdd (clent->v.origin, clent->v.view_ofs, org);
    pvs = SV_FatPVS (org);

    clentnum = EDICT_TO_PROG(clent); // LordHavoc: for comparison purposes //Team XLink DP_SV_DRAWONLYTOCLIENT & DP_SV_NODRAWTOCLIENT

// send over all entities (excpet the client) that touch the pvs
    ent = NEXT_EDICT(sv.edicts);
    for (e=1 ; e<sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
    {
        if (ent != clent)	// clent is ALWAYS sent
        {
            // don't send if flagged for NODRAW and there are no lighting effects
            if (ent->v.effects == EF_NODRAW)
                continue;

// ignore if not touching a PV leaf
            if (ent != clent)	// clent is ALWAYS sent
            {
// ignore ents without visible models
                if (!ent->v.modelindex || !pr_strings[ent->v.model])
                    continue;
                //qbism based on Team XLink DP_SV_DRAWONLYTOCLIENT & DP_SV_NODRAWTOCLIENT Start
                if ((val = GetEdictFieldValue(ent, "drawonlytoclient")) && val->edict && val->edict != clentnum)
                    continue;

                if ((val = GetEdictFieldValue(ent, "nodrawtoclient")) && val->edict == clentnum)
                    continue;
                //Team XLink DP_SV_DRAWONLYTOCLIENT & DP_SV_NODRAWTOCLIENT End

                if ((!chase_active.value && (val = GetEdictFieldValue(ent, "exteriormodeltoclient"))) && val->edict == clentnum)
                    continue;

                //qbism- 'Fix - Large Brushmodel Flickering/Not Visible' by MH elaborated by Reckless, from inside3d
                // link to PVS leafs - deferred to here so that we can compare leafs that are touched to the PVS.
                // this is less optimal on one hand as it now needs to be done separately for each client, rather than once
                // only (covering all clients), but more optimal on the other as it only needs to hit one leaf and will
                // start dropping out of the recursion as soon as it does so.  on balance it should be more optimal overall.
                ent->touchleaf = false;

                SV_FindTouchedLeafs (ent, sv.worldmodel->nodes, pvs);

                // if the entity didn't touch any leafs in the pvs don't send it to the client
                if (!ent->touchleaf) continue; //qbism fixme, do sv_novis?  if (!ent->touchleaf && !sv_novis.integer)
            }


            //johnfitz -- max size for protocol 15 is 18 bytes, not 16 as originally
            //assumed here.  And, for protocol 85 the max size is actually 24 bytes.
            if (msg->cursize + 24 > msg->maxsize)
            {
                Con_Printf ("Packet overflow!\n");
            }
            //johnfitz
        }

// send an update
        bits = 0;

        for (i=0 ; i<3 ; i++)
        {
            miss = ent->v.origin[i] - ent->baseline.origin[i];
            if ( miss < -0.1 || miss > 0.1 )
                bits |= U_ORIGIN1<<i;
        }

        if ( ent->v.angles[0] != ent->baseline.angles[0] )
            bits |= U_ANGLE1;

        if ( ent->v.angles[1] != ent->baseline.angles[1] )
            bits |= U_ANGLE2;

        if ( ent->v.angles[2] != ent->baseline.angles[2] )
            bits |= U_ANGLE3;

        if (ent->v.movetype == MOVETYPE_STEP)
            bits |= U_NOLERP;	// don't mess up the step animation

        if (ent->baseline.colormap != ent->v.colormap)
            bits |= U_COLORMAP;

        if (ent->baseline.skin != ent->v.skin)
            bits |= U_SKIN;

        if (ent->baseline.frame != ent->v.frame)
            bits |= U_FRAME;


        if ((current_protocol == PROTOCOL_QBS8) && (val = GetEdictFieldValue(ent, "modelflags")))  //qbism from DP
        {
            i= (unsigned int)ent->v.effects;
            i |= ((unsigned int)val->_float & 0xff) << 24;
            ent->v.effects = i;
        }

        if (ent->baseline.effects != ent->v.effects)
        {
            // Manoel Kasimier
            bits |= U_EFFECTS;
            // Manoel Kasimier - begin
            if (current_protocol == PROTOCOL_QBS8)
            {
                ent->baseline.effects = ent->v.effects; //qbism reset baseline, also on client side.
                if (ent->v.effects >= 256)
                    bits |= U_EFFECTS2;
            }
        }
        // Manoel Kasimier - end

        if (ent->baseline.modelindex != ent->v.modelindex)
            bits |= U_MODEL;
        // Tomaz - QC Alpha Scale Glow Begin (qbism- modified)
        if (current_protocol == PROTOCOL_QBS8)
        {
            eval_t  *val;
            if (val = GetEdictFieldValue(ent, "alpha"))
            {
                ent->alpha = ENTALPHA_ENCODE(val->_float); //qbism ent->alpha
                //qbism:  fitzquake  don't send invisible entities unless they have effects
                if (ent->alpha == ENTALPHA_ZERO && !ent->v.effects)
                    continue;
                //johnfitz
                bits |= U_ALPHA;
            }
            else ent->alpha = ENTALPHA_DEFAULT;

            if (val = GetEdictFieldValue(ent, "scale"))
            {
                scale = val->_float;
                if (scale > 4)
                    scale = 4;
                if (scale > 0)
                    bits |= U_SCALE;
            }
            if ((val = GetEdictFieldValue(ent, "scalev")))
            {
                for (i=0 ; i<3 ; i++)
                    scalev[i] = val->vector[i];
                if (scalev[0] || scalev[1] || scalev[2])
                    bits |= U_SCALEV;
            }
            if ((val = GetEdictFieldValue(ent, "glow_size")))
            {
                glow_size = val->_float;
                if (glow_size)
                {
                    bound(-250, glow_size, 250);
                    if (glow_size > 250)
                        glow_size = 250;
                    else if (glow_size < -250)
                        glow_size = -250;
                    bits |= U_GLOW_SIZE;

                }
            }
            if (val = GetEdictFieldValue(ent, "glow_red"))
            {
                glow_red = (byte)(val->_float*255.0);
                bits |= U_GLOW_RED;
            }
            if (val = GetEdictFieldValue(ent, "glow_green"))
            {
                glow_red = (byte)(val->_float*255.0);
                bits |= U_GLOW_GREEN;
            }
            if (val = GetEdictFieldValue(ent, "glow_blue"))
            {
                glow_red = (byte)(val->_float*255.0);
                bits |= U_GLOW_BLUE;
            }
            // Tomaz - QC Alpha Scale Glow End

            // Manoel Kasimier - QC frame_interval - begin
            if ((val = GetEdictFieldValue(ent, "frame_interval")))
            {
                frame_interval = val->_float;
                if (frame_interval)
                    bits |= U_FRAMEINTERVAL;
            }
            // Manoel Kasimier - QC frame_interval - end
        }
        // Tomaz - QC Alpha Scale Glow End
        if (e >= 256)
            bits |= U_LONGENTITY;

        if (bits >= 256)
            bits |= U_MOREBITS;
        // Tomaz - QC Control Begin
        if (bits >= 65536)
            bits |= U_EXTEND1;
        // Tomaz - QC Control End
        // Manoel Kasimier - begin
        if (bits >= 1<<24)
            bits |= U_EXTEND2;
        // Manoel Kasimier - end

        //
        // write the message
        //
        MSG_WriteByte (msg,bits | U_SIGNAL);

        if (bits & U_MOREBITS)
            MSG_WriteByte (msg, bits>>8);
        // Tomaz - QC Control Begin
        if (bits & U_EXTEND1)
            MSG_WriteByte (msg, bits>>16);
        // Tomaz - QC Control End
        // Manoel Kasimier - begin
        if (bits & U_EXTEND2)
            MSG_WriteByte (msg, bits>>24);
        // Manoel Kasimier - end
        if (bits & U_LONGENTITY)
            MSG_WriteShort (msg,e);
        else
            MSG_WriteByte (msg,e);

        if (bits & U_MODEL)
        {
            if (current_protocol == PROTOCOL_QBS8)
                MSG_WriteShort (msg,	ent->v.modelindex);
            else MSG_WriteByte (msg,	ent->v.modelindex);
        }
        if (bits & U_FRAME)
            MSG_WriteByte (msg, ent->v.frame);
        if (bits & U_COLORMAP)
            MSG_WriteByte (msg, ent->v.colormap);
        if (bits & U_SKIN)
            MSG_WriteByte (msg, ent->v.skin);
        if (bits & U_EFFECTS)
        {
            if (current_protocol == PROTOCOL_QBS8)
            {
                if(bits & U_EFFECTS2) MSG_WriteLong(msg, ent->v.effects); //qbism changed for modelflags
                else MSG_WriteByte (msg, ent->v.effects);
            }
            // Manoel Kasimier - begin
            else
                MSG_WriteByte (msg, ent->v.effects - ((int)ent->v.effects & (EF_REFLECTIVE|EF_SHADOW|EF_CELSHADING)) );
        }
        // Manoel Kasimier - end
        if (bits & U_ORIGIN1)
            MSG_WriteCoord (msg, ent->v.origin[0]);
        if (bits & U_ANGLE1)
            MSG_WriteAngle(msg, ent->v.angles[0]);
        if (bits & U_ORIGIN2)
            MSG_WriteCoord (msg, ent->v.origin[1]);
        if (bits & U_ANGLE2)
            MSG_WriteAngle(msg, ent->v.angles[1]);
        if (bits & U_ORIGIN3)
            MSG_WriteCoord (msg, ent->v.origin[2]);
        if (bits & U_ANGLE3)
            MSG_WriteAngle(msg, ent->v.angles[2]);
        // Tomaz - QC Alpha Scale Glow Begin
        if (bits & U_ALPHA)
            MSG_WriteByte(msg, ent->alpha);
        if (bits & U_SCALE)
            MSG_WriteFloat(msg, scale);
        if (bits & U_SCALEV)
            for (i=0 ; i<3 ; i++)
                MSG_WriteFloat (msg, scalev[i]);
        if (bits & U_GLOW_SIZE)
            MSG_WriteShort(msg, glow_size); //qbism - was writefloat
        if (bits & U_GLOW_RED) //qbism
            MSG_WriteByte(msg, glow_red);  //qbism, unlike Tomazquake, sending 0-255 RGB values
        if (bits & U_GLOW_GREEN)
            MSG_WriteByte(msg, glow_green);
        if (bits & U_GLOW_BLUE)
            MSG_WriteByte(msg, glow_blue);
    }
    // Tomaz - QC Alpha Scale Glow End
    // Manoel Kasimier - QC frame_interval - begin
    if (bits & U_FRAMEINTERVAL)
        if (current_protocol == PROTOCOL_QBS8)  //qbism
            MSG_WriteFloat(msg, frame_interval);
    // Manoel Kasimier - QC frame_interval - end
}


/*
=============
SV_CleanupEnts

=============
*/
void SV_CleanupEnts (void)
{
    int		e;
    edict_t	*ent;

    ent = NEXT_EDICT(sv.edicts);
    for (e=1 ; e<sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
    {
        ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
    }

}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg)
{
    int		bits;
    int		i;
    edict_t	*other;
    int		items;
    eval_t	*val;

//
// send a damage message
//
    if (ent->v.dmg_take || ent->v.dmg_save)
    {
        other = PROG_TO_EDICT(ent->v.dmg_inflictor);
        MSG_WriteByte (msg, svc_damage);
        MSG_WriteByte (msg, ent->v.dmg_save);
        MSG_WriteByte (msg, ent->v.dmg_take);
        for (i=0 ; i<3 ; i++)
            MSG_WriteCoord (msg, other->v.origin[i] + 0.5*(other->v.mins[i] + other->v.maxs[i]));

        ent->v.dmg_take = 0;
        ent->v.dmg_save = 0;
    }

//
// send the current viewpos offset from the view entity
//
    SV_SetIdealPitch ();		// how much to look up / down ideally

// a fixangle might get lost in a dropped packet.  Oh well.
    if ( ent->v.fixangle )
    {
        MSG_WriteByte (msg, svc_setangle);
        for (i=0 ; i < 3 ; i++)
            MSG_WriteAngle (msg, ent->v.angles[i] );
        ent->v.fixangle = 0;
    }

    bits = 0;

    if (ent->v.view_ofs[2] != DEFAULT_VIEWHEIGHT)
        bits |= SU_VIEWHEIGHT;

    if (ent->v.idealpitch)
        bits |= SU_IDEALPITCH;

// stuff the sigil bits into the high bits of items for sbar, or else
// mix in items2

    val = GetEdictFieldValue(ent, "items2");

    if (val)
        items = (int)ent->v.items | ((int)val->_float << 23);
    else
        items = (int)ent->v.items | ((int)pr_global_struct->serverflags << 28);

    bits |= SU_ITEMS;

    if ( (int)ent->v.flags & FL_ONGROUND)
        bits |= SU_ONGROUND;

    if ( ent->v.waterlevel >= 2)
        bits |= SU_INWATER;

    for (i=0 ; i<3 ; i++)
    {
        if (ent->v.punchangle[i])
            bits |= (SU_PUNCH1<<i);
        if (ent->v.velocity[i])
            bits |= (SU_VELOCITY1<<i);
    }

    if (ent->v.weaponframe)
        bits |= SU_WEAPONFRAME;

    if (ent->v.armorvalue)
        bits |= SU_ARMOR;

//	if (ent->v.weapon)
    bits |= SU_WEAPON;

// send the data

    MSG_WriteByte (msg, svc_clientdata);
    MSG_WriteShort (msg, bits);

    if (bits & SU_VIEWHEIGHT)
        MSG_WriteChar (msg, ent->v.view_ofs[2]);

    if (bits & SU_IDEALPITCH)
        MSG_WriteChar (msg, ent->v.idealpitch);

    for (i=0 ; i<3 ; i++)
    {
        if (bits & (SU_PUNCH1<<i))
            // Manoel Kasimier - 16-bit angles - begin
        {
            if (current_protocol == PROTOCOL_QBS8)
                MSG_WriteAngle (msg, ent->v.punchangle[i]);
            else
                // Manoel Kasimier - 16-bit angles - end
                MSG_WriteChar (msg, ent->v.punchangle[i]);
        } // Manoel Kasimier - 16-bit angles
        if (bits & (SU_VELOCITY1<<i))
            MSG_WriteChar (msg, ent->v.velocity[i]/16);
    }

// [always sent]	if (bits & SU_ITEMS)
    MSG_WriteLong (msg, items);

    if (bits & SU_WEAPONFRAME)
        MSG_WriteByte (msg, ent->v.weaponframe);
    if (bits & SU_ARMOR)
        MSG_WriteByte (msg, ent->v.armorvalue);
    if (bits & SU_WEAPON)
    {
        if (current_protocol == PROTOCOL_QBS8)
        {
            MSG_WriteShort (msg, SV_ModelIndex(pr_strings+ent->v.weaponmodel));
        }
        else MSG_WriteByte (msg, SV_ModelIndex(pr_strings+ent->v.weaponmodel));
    }

    MSG_WriteShort (msg, ent->v.health);
    // Manoel Kasimier - high values in the status bar - begin
    if(current_protocol == PROTOCOL_QBS8)
    {
        MSG_WriteShort (msg, ent->v.currentammo);
        MSG_WriteShort (msg, ent->v.ammo_shells);
        MSG_WriteShort (msg, ent->v.ammo_nails);
        MSG_WriteShort (msg, ent->v.ammo_rockets);
        MSG_WriteShort (msg, ent->v.ammo_cells);
    }
    else
    {
        // Manoel Kasimier - high values in the status bar - end
        MSG_WriteByte (msg, ent->v.currentammo);
        MSG_WriteByte (msg, ent->v.ammo_shells);
        MSG_WriteByte (msg, ent->v.ammo_nails);
        MSG_WriteByte (msg, ent->v.ammo_rockets);
        MSG_WriteByte (msg, ent->v.ammo_cells);
    } // Manoel Kasimier - high values in the status bar

    if (standard_quake)
    {
        MSG_WriteByte (msg, ent->v.weapon);
    }
    else
    {
        for(i=0; i<32; i++)
        {
            if ( ((int)ent->v.weapon) & (1<<i) )
            {
                MSG_WriteByte (msg, i);
                break;
            }
        }
    }
}

/*
=======================
SV_SendClientDatagram
=======================
*/
qboolean SV_SendClientDatagram (client_t *client)
{
    byte		buf[MAX_DATAGRAM];
    sizebuf_t	msg;

    msg.data = buf;
    msg.maxsize = sizeof(buf);
    msg.cursize = 0;

    //qbism:  from johnfitz -- if client is nonlocal, use smaller max size so packets aren't fragmented
    if (Q_strcmp (client->netconnection->address, "LOCAL") != 0)
        msg.maxsize = DATAGRAM_MTU;


    MSG_WriteByte (&msg, svc_time);
    MSG_WriteFloat (&msg, sv.time);

// add the client specific data to the datagram
    SV_WriteClientdataToMessage (client->edict, &msg);

    SV_WriteEntitiesToClient (client->edict, &msg);

// copy the server datagram if there is space
    if (msg.cursize + sv.datagram.cursize < msg.maxsize)
        SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);

// send the datagram
    if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
    {
        SV_DropClient (true);// if the message couldn't send, kick off
        return false;
    }

    return true;
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
void SV_UpdateToReliableMessages (void)
{
    int			i, j;
    client_t *client;

// check for changes to be sent over the reliable streams
    for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
    {
        if (host_client->old_frags != host_client->edict->v.frags)
        {
            for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
            {
                if (!client->active)
                    continue;
                MSG_WriteByte (&client->message, svc_updatefrags);
                MSG_WriteByte (&client->message, i);
                MSG_WriteShort (&client->message, host_client->edict->v.frags);
            }

            host_client->old_frags = host_client->edict->v.frags;
        }
    }

    for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
    {
        if (!client->active)
            continue;
        SZ_Write (&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
    }

    SZ_Clear (&sv.reliable_datagram);
}


/*
=======================
SV_SendNop

Send a nop message without trashing or sending the accumulated client
message buffer
=======================
*/
void SV_SendNop (client_t *client)
{
    sizebuf_t	msg;
    byte		buf[4];

    msg.data = buf;
    msg.maxsize = sizeof(buf);
    msg.cursize = 0;

    MSG_WriteByte (&msg, svc_nop);

    if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
        SV_DropClient (true);	// if the message couldn't send, kick off
    client->last_message = realtime;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
    int			i;

// update frags, names, etc
    SV_UpdateToReliableMessages ();

// build individual updates
    for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
    {
        if (!host_client->active)
            continue;

        if (host_client->spawned)
        {
            if (!SV_SendClientDatagram (host_client))
                continue;
        }
        else
        {
            // the player isn't totally in the game yet
            // send small keepalive messages if too much time has passed
            // send a full message when the next signon stage has been requested
            // some other message data (name changes, etc) may accumulate
            // between signon stages
            if (!host_client->sendsignon)
            {
                if (realtime - host_client->last_message > 5)
                    SV_SendNop (host_client);
                continue;	// don't send out non-signon messages
            }
        }

        // check for an overflowed message.  Should only happen
        // on a very fucked up connection that backs up a lot, then
        // changes level
        if (host_client->message.overflowed)
        {
            SV_DropClient (true);
            host_client->message.overflowed = false;
            continue;
        }

        if (host_client->message.cursize || host_client->dropasap)
        {
            if (!NET_CanSendMessage (host_client->netconnection))
            {
//				I_Printf ("can't write\n");
                continue;
            }

            if (host_client->dropasap)
                SV_DropClient (false);	// went to another level
            else
            {
                if (NET_SendMessage (host_client->netconnection
                                     , &host_client->message) == -1)
                    SV_DropClient (true);	// if the message couldn't send, kick off
                SZ_Clear (&host_client->message);
                host_client->last_message = realtime;
                host_client->sendsignon = false;
            }
        }
    }


// clear muzzle flashes
    SV_CleanupEnts ();
}


/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex (char *name)
{
    int		i;

    if (!name || !name[0])
        return 0;

    for (i=0 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
        if (!Q_strcmp(sv.model_precache[i], name))
            return i;
    if (i==MAX_MODELS || !sv.model_precache[i])
        Sys_Error ("SV_ModelIndex: model %s not precached", name);
    return i;
}

/*
================
SV_CreateBaseline

================
*/
void SV_CreateBaseline (void)
{
    int			i;
    edict_t			*svent;
    int				entnum;

    for (entnum = 0; entnum < sv.num_edicts ; entnum++)
    {
        // get the current server version
        svent = EDICT_NUM(entnum);
        if (svent->free)
            continue;
        if (entnum > svs.maxclients && !svent->v.modelindex)
            continue;

        //
        // create entity baseline
        //
        VectorCopy (svent->v.origin, svent->baseline.origin);
        VectorCopy (svent->v.angles, svent->baseline.angles);
        svent->baseline.frame = svent->v.frame;
        svent->baseline.skin = svent->v.skin;
        if (entnum > 0 && entnum <= svs.maxclients)
        {
            svent->baseline.colormap = entnum;
            svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");  //qbism FIXME if multiple player models
        }
        else
        {
            svent->baseline.colormap = 0;
            svent->baseline.modelindex =
                SV_ModelIndex(pr_strings + svent->v.model);

        }

        //
        // add to the message
        //
        MSG_WriteByte (&sv.signon,svc_spawnbaseline);
        MSG_WriteShort (&sv.signon,entnum);

        if (current_protocol == PROTOCOL_QBS8)
        {
            svent->baseline.alpha = svent->alpha;  //qbism
            MSG_WriteShort (&sv.signon, svent->baseline.modelindex);
            MSG_WriteByte (&sv.signon, svent->baseline.alpha); //qbism
        }

        else
        {
            svent->baseline.alpha = ENTALPHA_DEFAULT; //qbism
            MSG_WriteByte (&sv.signon, svent->baseline.modelindex);
        }

        MSG_WriteByte (&sv.signon, svent->baseline.frame);
        MSG_WriteByte (&sv.signon, svent->baseline.colormap);
        MSG_WriteByte (&sv.signon, svent->baseline.skin);
        for (i=0 ; i<3 ; i++)
        {
            MSG_WriteCoord(&sv.signon, svent->baseline.origin[i]);
            MSG_WriteAngle(&sv.signon, svent->baseline.angles[i]);
        }
    }
}


/*
================
SV_SendReconnect

Tell all the clients that the server is changing levels
================
*/
void SV_SendReconnect (void)
{
    char	data[128];
    sizebuf_t	msg;

    msg.data = data;
    msg.cursize = 0;
    msg.maxsize = sizeof(data);

    MSG_WriteByte (&msg, svc_stufftext);
    MSG_WriteString (&msg, "reconnect\n");
    NET_SendToAll (&msg, 5);

    if (cls.state != ca_dedicated)

        Cmd_ExecuteString ("reconnect\n", src_command);
}


/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
    int		i, j;

    svs.serverflags = pr_global_struct->serverflags;

    for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
    {
        if (!host_client->active)
            continue;

        // call the progs to get default spawn parms for the new client
        pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
        PR_ExecuteProgram (pr_global_struct->SetChangeParms);
        for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
            host_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
    }
}


/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
extern float		scr_centertime_off;

void SV_SpawnServer (char *server)
{
    edict_t		*ent;
    int			i;
    // let's not have any servers with no name
    if (hostname.string[0] == 0)
        Cvar_Set ("hostname", "UNNAMED");
    scr_centertime_off = 0;

    Con_DPrintf ("SpawnServer: %s\n",server);
    svs.changelevel_issued = false;		// now safe to issue another

//
// tell all connected clients that we are going to a new level
//
    if (sv.active)
    {
        SV_SendReconnect ();
    }

//
// make cvars consistant
//
    if (coop.value)
        Cvar_SetValue ("deathmatch", 0);
    current_skill = (int)(skill.value + 0.5);
    current_skill = bound(0, current_skill, 3);

    Cvar_SetValue ("skill", (float)current_skill);

//
// set up the new server
//
    Host_ClearMemory ();

    memset (&sv, 0, sizeof(sv));

    Q_strcpy (sv.name, server);

// load progs to get entity field count
    PR_LoadProgs ();

// allocate server memory
    sv.edicts = Hunk_AllocName (MAX_EDICTS*pr_edict_size, "edicts");

    sv.datagram.maxsize = sizeof(sv.datagram_buf);
    sv.datagram.cursize = 0;
    sv.datagram.data = sv.datagram_buf;

    sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
    sv.reliable_datagram.cursize = 0;
    sv.reliable_datagram.data = sv.reliable_datagram_buf;

    sv.signon.maxsize = sizeof(sv.signon_buf);
    sv.signon.cursize = 0;
    sv.signon.data = sv.signon_buf;

// leave slots at start for clients only
    sv.num_edicts = svs.maxclients+1;
    for (i=0 ; i<svs.maxclients ; i++)
    {
        ent = EDICT_NUM(i+1);
        svs.clients[i].edict = ent;
    }

    sv.state = ss_loading;
    sv.paused = false;

    sv.time = 1.0;

    Q_strcpy (sv.name, server);
    sprintf (sv.modelname,"maps/%s.bsp", server);
    sv.worldmodel = Mod_ForName (sv.modelname, false);
    if (!sv.worldmodel || (sv.worldmodel->numvertexes == -1)) // MrG - incorrect BSP version is no longer fatal - edited
    {
        sv.active = false;
        Host_Error/*Con_Printf*/ ("Couldn't spawn server %s\n", sv.modelname); // Manoel Kasimier - edited
        return;
    }
    R_LoadPalette("palette"); //qbism- cleanse the palette.
    R_LoadPalette(server); //qbism - load palette matching the map name, if there is one.
    sv.models[1] = sv.worldmodel;
//
// clear world interaction links
//
    SV_ClearWorld ();

    sv.sound_precache[0] = pr_strings;

    sv.model_precache[0] = pr_strings;
    sv.model_precache[1] = sv.modelname;
    for (i=1 ; i<sv.worldmodel->numsubmodels ; i++)
    {
        sv.model_precache[1+i] = localmodels[i];
        sv.models[i+1] = Mod_ForName (localmodels[i], false);
    }

//
// load the rest of the entities
//
    ent = EDICT_NUM(0);
    memset (&ent->v, 0, progs->entityfields * 4);
    ent->free = false;
    ent->v.model = sv.worldmodel->name - pr_strings;
    ent->v.modelindex = 1;		// world model
    ent->v.solid = SOLID_BSP;
    ent->v.movetype = MOVETYPE_PUSH;

    if (coop.value)
        pr_global_struct->coop = coop.value;
    else
        pr_global_struct->deathmatch = deathmatch.value;

    pr_global_struct->mapname = sv.name - pr_strings;

// serverflags are for cross level information (sigils)
    pr_global_struct->serverflags = svs.serverflags;

    ED_LoadFromFile (sv.worldmodel->entities);

    sv.active = true;

// all setup is completed, any further precache statements are errors
    sv.state = ss_active;

// run two frames to allow everything to settle
    host_org_frametime =	// 2001-10-20 TIMESCALE extension by Tomaz/Maddes
        host_frametime = 0.1;
    SV_Physics ();
    SV_Physics ();

// create a baseline for more efficient communications
    SV_CreateBaseline ();

    //qbism:  johnfitz -- warn if signon buffer larger than standard server can handle
    if (sv.signon.cursize > 8000-2) //max size that will fit into 8000-sized client->message buffer with 2 extra bytes on the end
        Con_DPrintf ("%i byte signon buffer exceeds standard limit of 7998.\n", sv.signon.cursize);

// send serverinfo to all connected clients
    for (i=0,host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
        if (host_client->active)
            SV_SendServerinfo (host_client);

    Con_DPrintf ("host_client server spawned.\n");
}

