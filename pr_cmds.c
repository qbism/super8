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

#include "quakedef.h"

#define	RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))
#define PR_MAX_TEMPSTRING 2048	// 2001-10-25 Enhanced temp string handling by Maddes

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/
// 2001-10-20 Extension System by LordHavoc  start
char *pr_extensions[] =
{
// add the extension names here, syntax: "extensionname",

    // Manoel Kasimier - begin
    "DP_CL_LOADSKY",	// console command "loadsky"?
    "DP_EF_ADDITIVE",
    "DP_EF_FULLBRIGHT",
    "DP_EF_NODRAW",
    "DP_ENT_ALPHA",
    "DP_ENT_GLOW",
    "DP_ENT_SCALE",
    "DP_GFX_SKYBOX",
    // Manoel Kasimier - end

    //qb: added extensions begin
    "DP_MOVETYPEBOUNCEMISSILE",
    "DP_MOVETYPEFOLLOW",
    "DP_SV_MODELFLAGS_AS_EFFECTS ",
    "DP_ENT_EXTERIORMODELTOCLIENT",
    "DP_SV_NODRAWTOCLIENT",  //based on Team Xlink
    "DP_SV_DRAWONLYTOCLIENT",  //based on Team Xlink
    //qb: added extensions end

//	"DP_REGISTERCVAR",	// 2001-09-18 New BuiltIn Function: cvar_create() by Maddes
    "DP_QC_CHANGEPITCH",// 2001-09-16 Quake 2 builtin functions by id/Maddes
    "DP_QC_COPYENTITY",	// 2001-11-15 DarkPlaces general builtin functions by LordHavoc
    "DP_QC_CVAR_STRING",// 2001-09-16 New BuiltIn Function: cvar_string() by Maddes
    "DP_QC_ETOS",		// 2001-09-16 Quake 2 builtin functions by id/Maddes
// 2001-11-15 DarkPlaces general builtin functions by LordHavoc  start
    "DP_QC_FINDCHAIN",
    "DP_QC_FINDCHAINFLOAT",
    "DP_QC_FINDFLOAT",
    "DP_QC_MINMAXBOUND ",
    "DP_QC_RANDOMVEC ",
    "DP_QC_SINCOSSQRTPOW ",
    "DP_QC_TRACEBOX ",
// 2001-11-15 DarkPlaces general builtin functions by LordHavoc  end
    "DP_QC_TRACETOSS",	// 2001-09-16 Quake 2 builtin functions by id/Maddes

    "DP_SND_DIRECTIONLESSATTNNONE",	// Manoel Kasimier - sounds with no attenuation full volume on both sides?

    "DP_SV_SETCOLOR",	// 2001-11-15 DarkPlaces general builtin functions by LordHavoc
    "DP_SV_ROTATINGBMODEL",
    "FRIK_FILE",
    "MK_EF_SHADOW",		// Manoel Kasimier

    /*
    "DP_EF_BLUE "
    "DP_EF_FLAME "
    "DP_EF_NOSHADOW "
    "DP_EF_RED "
    "DP_EF_STARDUST "
    "DP_ENT_CUSTOMCOLORMAP "
    "DP_ENT_LOWPRECISION "
    "DP_ENT_VIEWMODEL "
    "DP_GFX_EXTERNALTEXTURES "
    "DP_GFX_FOG "
    "DP_GFX_QUAKE3MODELTAGS "
    "DP_GFX_SKINFILES "
    "DP_HALFLIFE_MAP "
    "DP_HALFLIFE_MAP_CVAR "
    "DP_INPUTBUTTONS "			// ?
    "DP_LITSUPPORT "
    "DP_MONSTERWALK "
    "DP_QC_FS_SEARCH " // Black: same as in the menu qc
    "DP_QC_GETLIGHT "
    "DP_QC_GETSURFACE "
    "DP_QC_MULTIPLETEMPSTRINGS "
    "DP_QC_TRACE_MOVETYPE_HITMODEL "
    "DP_QC_TRACE_MOVETYPE_WORLDONLY "
    "DP_QC_VECTORVECTORS "
    "DP_QUAKE2_MODEL "
    "DP_QUAKE3_MODEL "
    "DP_REGISTERCVAR "
    "DP_SND_OGGVORBIS "
    "DP_SND_STEREOWAV "
    "DP_SOLIDCORPSE "
    "DP_SPRITE32 "
    "DP_SV_EFFECT "				// ?
    "DP_SV_EXTERIORMODELTOCLIENT "
    "DP_SV_PLAYERPHYSICS "
    "DP_SV_SLOWMO "
    "DP_TE_BLOOD "
    "DP_TE_BLOODSHOWER "
    "DP_TE_CUSTOMFLASH "
    "DP_TE_EXPLOSIONRGB "
    "DP_TE_FLAMEJET "
    "DP_TE_PARTICLECUBE "
    "DP_TE_PARTICLERAIN "
    "DP_TE_PARTICLESNOW "
    "DP_TE_PLASMABURN "
    "DP_TE_QUADEFFECTS1 "
    "DP_TE_SMALLFLASH "
    "DP_TE_SPARK "
    "DP_TE_STANDARDEFFECTBUILTINS "
    "DP_VIEWZOOM "
    "KRIMZON_SV_PARSECLIENTCOMMAND "
    "NEH_CMD_PLAY2 "
    "NEH_RESTOREGAME "
    "TENEBRAE_GFX_DLIGHTS "
    "TW_SV_STEPCONTROL "
    */
};

int pr_numextensions = sizeof(pr_extensions)/sizeof(pr_extensions[0]);

qboolean extension_find(char *name)
{
    int	i;

    for (i=0; i < pr_numextensions; i++)
    {
        if (!Q_strcasecmp(pr_extensions[i], name))
            return true;
    }
    return false;
}

/*
=================
PF_extension_find

returns true if the extension is supported by the server

float extension_find(string name)
=================
*/
void PF_extension_find (void)
{
    G_FLOAT(OFS_RETURN) = extension_find(G_STRING(OFS_PARM0));
}
// 2001-10-20 Extension System by LordHavoc  end

char	pr_varstring_temp[PR_MAX_TEMPSTRING];	// 2001-10-25 Enhanced temp string handling by Maddes

char *PF_VarString (int	first)
{
    int		i;
    /*	static char out[256];

    	out[0] = 0;
    	for (i=first ; i<pr_argc ; i++)
    	{
    		Q_strcat (out, G_STRING((OFS_PARM0+i*3)));
    	}
    	return out;
    */
// 2001-10-25 Enhanced temp string handling by Maddes  start
    unsigned int		maxlen; // signed/unsigned mismatch fixed
    char	*add;

    pr_varstring_temp[0] = 0;
    for (i=first ; i < pr_argc ; i++)
    {
        maxlen = PR_MAX_TEMPSTRING - Q_strlen(pr_varstring_temp) - 1;	// -1 is EndOfString
        add = G_STRING((OFS_PARM0+i*3));
        if (maxlen > Q_strlen(add))
        {
            Q_strcat (pr_varstring_temp, add);
        }
        else
        {
            strncat (pr_varstring_temp, add, maxlen);
            pr_varstring_temp[PR_MAX_TEMPSTRING-1] = 0;
            break;	// can stop here
        }
    }
    return pr_varstring_temp;
// 2001-10-25 Enhanced temp string handling by Maddes  end
}


/*
=================
PF_errror

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
void PF_error (void)
{
    char	*s;
    edict_t	*ed;

    s = PF_VarString(0);
    Con_Printf ("======SERVER ERROR in %s:\n%s\n"
                ,pr_strings + pr_xfunction->s_name,s);
    ed = PROG_TO_EDICT(pr_global_struct->self);
    ED_Print (ed);

    Host_Error ("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void PF_objerror (void)
{
    char	*s;
    edict_t	*ed;

    s = PF_VarString(0);
    Con_Printf ("======OBJECT ERROR in %s:\n%s\n"
                ,pr_strings + pr_xfunction->s_name,s);
    ed = PROG_TO_EDICT(pr_global_struct->self);
    ED_Print (ed);
    ED_Free (ed);

//	Host_Error ("Program error");	// 2001-12-16 Do not stop server on objerror
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors (void)
{
    AngleVectors (G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
void PF_setorigin (void)
{
    edict_t	*e;
    float	*org;

    e = G_EDICT(OFS_PARM0);
    org = G_VECTOR(OFS_PARM1);
    VectorCopy (org, e->v.origin);
    SV_LinkEdict (e, false);
}


void SetMinMaxSize (edict_t *e, float *min, float *max, qboolean rotate)
{
    float	*angles;
    vec3_t	rmin, rmax;
    float	bounds[2][3];
    float	xvector[2], yvector[2];
    float	a;
    vec3_t	base, transformed;
    int		i, j, k, l;

    for (i=0 ; i<3 ; i++)
        if (min[i] > max[i])
            PR_RunError ("backwards mins/maxs");

    rotate = false;		// FIXME: implement rotation properly again

    if (!rotate)
    {
        VectorCopy (min, rmin);
        VectorCopy (max, rmax);
    }
    else
    {
        // find min / max for rotations
        angles = e->v.angles;

        a = angles[1]/180 * M_PI;

        xvector[0] = cos(a);
        xvector[1] = sin(a);
        yvector[0] = -sin(a);
        yvector[1] = cos(a);

        VectorCopy (min, bounds[0]);
        VectorCopy (max, bounds[1]);

        rmin[0] = rmin[1] = rmin[2] = 9999;
        rmax[0] = rmax[1] = rmax[2] = -9999;

        for (i=0 ; i<= 1 ; i++)
        {
            base[0] = bounds[i][0];
            for (j=0 ; j<= 1 ; j++)
            {
                base[1] = bounds[j][1];
                for (k=0 ; k<= 1 ; k++)
                {
                    base[2] = bounds[k][2];

                    // transform the point
                    transformed[0] = xvector[0]*base[0] + yvector[0]*base[1];
                    transformed[1] = xvector[1]*base[0] + yvector[1]*base[1];
                    transformed[2] = base[2];

                    for (l=0 ; l<3 ; l++)
                    {
                        if (transformed[l] < rmin[l])
                            rmin[l] = transformed[l];
                        if (transformed[l] > rmax[l])
                            rmax[l] = transformed[l];
                    }
                }
            }
        }
    }

// set derived values
    VectorCopy (rmin, e->v.mins);
    VectorCopy (rmax, e->v.maxs);
    VectorSubtract (max, min, e->v.size);

    SV_LinkEdict (e, false);
}

/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize (void)
{
    edict_t	*e;
    float	*min, *max;

    e = G_EDICT(OFS_PARM0);
    min = G_VECTOR(OFS_PARM1);
    max = G_VECTOR(OFS_PARM2);
    SetMinMaxSize (e, min, max, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
void PF_setmodel (void)
{
    edict_t	*e;
    char	*m, **check;
    model_t	*mod;
    int		i;

    e = G_EDICT(OFS_PARM0);
    m = G_STRING(OFS_PARM1);

// check to see if model was properly precached
    for (i=0, check = sv.model_precache ; *check ; i++, check++)
        if (!Q_strcmp(*check, m))
            break;

    if (!*check)
        PR_RunError ("setmodel:  no precache: %s\n", m);

    e->v.model = m - pr_strings;
    e->v.modelindex = i; //SV_ModelIndex (m);

    mod = sv.models[ (int)e->v.modelindex];  // Mod_ForName (m, true);

    if (mod)
			SetMinMaxSize (e, mod->mins, mod->maxs, true);
    else
        SetMinMaxSize (e, vec3_origin, vec3_origin, true);
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
void PF_bprint (void)
{
    char		*s;

    s = PF_VarString(0);
    SV_BroadcastPrintf ("%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint (void)
{
    char		*s;
    client_t	*client;
    int			entnum;

    entnum = G_EDICTNUM(OFS_PARM0);
    s = PF_VarString(1);

    if (entnum < 1 || entnum > svs.maxclients)
    {
        Con_Printf ("tried to sprint to a non-client\n");
        return;
    }

    client = &svs.clients[entnum-1];

    MSG_WriteByte (&client->message,svc_print);
    MSG_WriteString (&client->message, s );
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF_centerprint (void)
{
    char		*s;
    client_t	*client;
    int			entnum;

    entnum = G_EDICTNUM(OFS_PARM0);
    s = PF_VarString(1);

    if (entnum < 1 || entnum > svs.maxclients)
    {
        Con_Printf ("tried to sprint to a non-client\n");
        return;
    }

    client = &svs.clients[entnum-1];

    MSG_WriteByte (&client->message,svc_centerprint);
    MSG_WriteString (&client->message, s );
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
void PF_normalize (void)
{
    float	*value1;
    vec3_t	newvalue;
    float	new;

    value1 = G_VECTOR(OFS_PARM0);

    new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
    new = sqrt(new);

    if (new == 0)
        newvalue[0] = newvalue[1] = newvalue[2] = 0;
    else
    {
        new = 1/new;
        newvalue[0] = value1[0] * new;
        newvalue[1] = value1[1] * new;
        newvalue[2] = value1[2] * new;
    }

    VectorCopy (newvalue, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
void PF_vlen (void)
{
    float	*value1;
    float	new;

    value1 = G_VECTOR(OFS_PARM0);

    new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
    new = sqrt(new);

    G_FLOAT(OFS_RETURN) = new;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
void PF_vectoyaw (void)
{
    float	*value1;
    float	yaw;

    value1 = G_VECTOR(OFS_PARM0);

    if (value1[1] == 0 && value1[0] == 0)
        yaw = 0;
    else
    {
        yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);

        //yaw = /*(int)*/ (atan2(value1[1], value1[0]) * 180 / M_PI); // Manoel Kasimier - 16-bit angles - edited
        if (yaw < 0)
            yaw += 360;
    }

    G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
void PF_vectoangles (void)
{
    float	*value1;
    float	forward;
    float	yaw, pitch;

    value1 = G_VECTOR(OFS_PARM0);

    if (value1[1] == 0 && value1[0] == 0)
    {
        yaw = 0;
        if (value1[2] > 0)
            pitch = 90;
        else
            pitch = 270;
    }
    else
    {
        yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);

        //yaw = /*(int)*/ (atan2(value1[1], value1[0]) * 180 / M_PI); // Manoel Kasimier - 16-bit angles - edited
        if (yaw < 0)
            yaw += 360;

        forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);

        pitch = (atan2(value1[2], forward) * 180 / M_PI);

        if (pitch < 0)
            pitch += 360;
    }

    G_FLOAT(OFS_RETURN+0) = pitch;
    G_FLOAT(OFS_RETURN+1) = yaw;
    G_FLOAT(OFS_RETURN+2) = 0;
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
void PF_random (void)
{
    float		num;

    num = (rand ()&0x7fff) / ((float)0x7fff);

    G_FLOAT(OFS_RETURN) = num;
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
void PF_particle (void)
{
    float		*org, *dir;
    float		color;
    float		count;

    org = G_VECTOR(OFS_PARM0);
    dir = G_VECTOR(OFS_PARM1);
    color = G_FLOAT(OFS_PARM2);
    count = G_FLOAT(OFS_PARM3);
    SV_StartParticle (org, dir, color, count);
}


/*
=================
PF_ambientsound

=================
*/
void PF_ambientsound (void)
{
    char		**check;
    char		*samp;
    float		*pos;
    float 		vol, attenuation;
    int			i, soundnum;

    pos = G_VECTOR (OFS_PARM0);
    samp = G_STRING(OFS_PARM1);
    vol = G_FLOAT(OFS_PARM2);
    attenuation = G_FLOAT(OFS_PARM3);

// check to see if samp was properly precached
    for (soundnum=0, check = sv.sound_precache ; *check ; check++, soundnum++)
        if (!Q_strcmp(*check,samp))
            break;

    if (!*check)
    {
        Con_Printf ("ambient sound:  no precache: %s\n", samp);
        return;
    }
 // qb: soundnum as byte for compatibility (Marcher) Ambients should be low number, anyway.
   if (current_protocol != PROTOCOL_NETQUAKE)
    {
        if (soundnum >MAX_SOUNDS)
        {
            Con_Printf ("ambient sound: soundnum > MAX_SOUNDS.\n");
            return;
        }
    }
    else if (soundnum >256)
    {
        Con_Printf ("ambient sound: soundnum >256.\n");
        return;
    }


// add an svc_spawnambient command to the level signon packet


if (current_protocol == PROTOCOL_NETQUAKE || (soundnum < 256))
    MSG_WriteByte (&sv.signon,svc_spawnstaticsound);
else
    MSG_WriteByte (&sv.signon,svc_spawnstaticsound_large); //qb: for mods that stuffcmd byte (Marcher, grrr...)
    for (i=0 ; i<3 ; i++)
        MSG_WriteCoord(&sv.signon, pos[i]);

    if (current_protocol != PROTOCOL_NETQUAKE && (soundnum > 255))
        MSG_WriteShort (&sv.signon, soundnum); //qb: more ambient sounds, breaks some mods
    else
    MSG_WriteByte (&sv.signon, soundnum);
    MSG_WriteByte (&sv.signon, vol*255);
    MSG_WriteByte (&sv.signon, attenuation*64);
}

/*
=================
PF_sound

Each entity can have eight independent sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
void PF_sound (void)
{
    char		*sample;
    int			channel;
    edict_t		*entity;
    int 		volume;
    float attenuation;

    entity = G_EDICT(OFS_PARM0);
    channel = G_FLOAT(OFS_PARM1);
    sample = G_STRING(OFS_PARM2);
    volume = G_FLOAT(OFS_PARM3) * 255;
    attenuation = G_FLOAT(OFS_PARM4);

    if (volume < 0 || volume > 255)
        Sys_Error ("SV_StartSound: volume = %i", volume);

    if (attenuation < 0 || attenuation > 4)
        Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

    if (channel < 0 || channel > 7)
        Sys_Error ("SV_StartSound: channel = %i", channel);

    SV_StartSound (entity, channel, sample, volume, attenuation);
}


//qb: plays sound to an individual client... I hear voices...
void PF_localsound (void)
{
    char		*sample;
    char 		volume;
    client_t	*client;
    int			entnum;

    entnum = G_EDICTNUM(OFS_PARM0);
    if (entnum < 1 || entnum > svs.maxclients)
    {
        Con_Printf ("tried to send local sound to a non-client\n");
        return;
    }

    client = &svs.clients[entnum-1];
    sample = G_STRING(OFS_PARM1);
    volume = (char)(G_FLOAT(OFS_PARM2) * 255);

    SV_LocalSound (client, sample, volume);
}

/*
=================
PF_break

break()
=================
*/
void PF_break (void)
{
    Con_Printf ("break statement\n");
    *(int *)-4 = 0;	// dump to debugger
//	PR_RunError ("break statement");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
void PF_traceline (void)
{
    float	*v1, *v2;
    trace_t	trace;
    int		nomonsters;
    edict_t	*ent;

    v1 = G_VECTOR(OFS_PARM0);
    v2 = G_VECTOR(OFS_PARM1);
    nomonsters = G_FLOAT(OFS_PARM2);
    ent = G_EDICT(OFS_PARM3);

    trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

    pr_global_struct->trace_allsolid = trace.allsolid;
    pr_global_struct->trace_startsolid = trace.startsolid;
    pr_global_struct->trace_fraction = trace.fraction;
    pr_global_struct->trace_inwater = trace.inwater;
    pr_global_struct->trace_inopen = trace.inopen;
    VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
    VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
    pr_global_struct->trace_plane_dist =  trace.plane.dist;
    if (trace.ent)
        pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
    else
        pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}


// 2001-09-16 Quake 2 builtin functions by id/Maddes
extern trace_t SV_Trace_Toss (edict_t *ent, edict_t *ignore);

void PF_TraceToss (void)
{
    trace_t	trace;
    edict_t	*ent;
    edict_t	*ignore;

    ent = G_EDICT(OFS_PARM0);
    ignore = G_EDICT(OFS_PARM1);

    trace = SV_Trace_Toss (ent, ignore);

    pr_global_struct->trace_allsolid = trace.allsolid;
    pr_global_struct->trace_startsolid = trace.startsolid;
    pr_global_struct->trace_fraction = trace.fraction;
    pr_global_struct->trace_inwater = trace.inwater;
    pr_global_struct->trace_inopen = trace.inopen;
    VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
    VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
    pr_global_struct->trace_plane_dist =  trace.plane.dist;
    if (trace.ent)
        pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
    else
        pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}
// 2001-09-16 Quake 2 builtin functions by id/Maddes


/*
=================
PF_checkpos

Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/

byte	checkpvs[MAX_MAP_LEAFS/8];

int PF_newcheckclient (int check)
{
    int		i;
    byte	*pvs;
    edict_t	*ent;
    mleaf_t	*leaf;
    vec3_t	org;

// cycle to the next one

    if (check < 1)
        check = 1;
    if (check > svs.maxclients)
        check = svs.maxclients;

    if (check == svs.maxclients)
        i = 1;
    else
        i = check + 1;

    for ( ;  ; i++)
    {
        if (i == svs.maxclients+1)
            i = 1;

        ent = EDICT_NUM(i);

        if (i == check)
            break;	// didn't find anything else

        if (ent->free)
            continue;
        if (ent->v.health <= 0)
            continue;
        if ((int)ent->v.flags & FL_NOTARGET)
            continue;

        // anything that is a client, or has a client as an enemy
        break;
    }

// get the PVS for the entity
    VectorAdd (ent->v.origin, ent->v.view_ofs, org);
    leaf = Mod_PointInLeaf (org, sv.worldmodel);
    pvs = Mod_LeafPVS (leaf, sv.worldmodel);
    memcpy (checkpvs, pvs, (sv.worldmodel->numleafs+7)>>3 );

    return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
#define	MAX_CHECK	16
int c_invis, c_notvis;
void PF_checkclient (void)
{
    edict_t	*ent, *self;
    mleaf_t	*leaf;
    int		l;
    vec3_t	view;

// find a new check if on a new frame
    if (sv.time - sv.lastchecktime >= 0.1)
    {
        sv.lastcheck = PF_newcheckclient (sv.lastcheck);
        sv.lastchecktime = sv.time;
    }

// return check if it might be visible
    ent = EDICT_NUM(sv.lastcheck);
    if (ent->free || ent->v.health <= 0)
    {
        RETURN_EDICT(sv.edicts);
        return;
    }

// if current entity can't possibly see the check entity, return 0
    self = PROG_TO_EDICT(pr_global_struct->self);
    VectorAdd (self->v.origin, self->v.view_ofs, view);
    leaf = Mod_PointInLeaf (view, sv.worldmodel);
    l = (leaf - sv.worldmodel->leafs) - 1;
    if ( (l<0) || !(checkpvs[l>>3] & (1<<(l&7)) ) )
    {
        c_notvis++;
        RETURN_EDICT(sv.edicts);
        return;
    }

// might be able to see it
    c_invis++;
    RETURN_EDICT(ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
void PF_stuffcmd (void)
{
    int		entnum;
    char	*str;
    client_t	*old;

    entnum = G_EDICTNUM(OFS_PARM0);
    if (entnum < 1 || entnum > svs.maxclients)
        PR_RunError ("Parm 0 not a client");
    str = G_STRING(OFS_PARM1);

    old = host_client;
    host_client = &svs.clients[entnum-1];
    Host_ClientCommands ("%s", str);
    host_client = old;
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
void PF_localcmd (void)
{
    char	*str;

    str = G_STRING(OFS_PARM0);
    Cbuf_AddText (str);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
void PF_cvar (void)
{
    char	*str;

    str = G_STRING(OFS_PARM0);

    G_FLOAT(OFS_RETURN) = Cvar_VariableValue (str);
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
void PF_cvar_set (void)
{
    char	*var, *val;

    var = G_STRING(OFS_PARM0);
    val = G_STRING(OFS_PARM1);

    Cvar_Set (var, val);
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
void PF_findradius (void)
{
    edict_t	*ent, *chain;
    float	rad;
    float	*org;
    vec3_t	eorg;
    int		i, j;

    chain = (edict_t *)sv.edicts;

    org = G_VECTOR(OFS_PARM0);
    rad = G_FLOAT(OFS_PARM1);

    ent = NEXT_EDICT(sv.edicts);
    for (i=1 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent))
    {
        if (ent->free)
            continue;
        if (ent->v.solid == SOLID_NOT)
            continue;
        for (j=0 ; j<3 ; j++)
            eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j])*0.5);
        if (Length(eorg) > rad)
            continue;

        ent->v.chain = EDICT_TO_PROG(chain);
        chain = ent;
    }

    RETURN_EDICT(chain);
}


/*
=========
PF_dprint
=========
*/
void PF_dprint (void)
{
    Con_DPrintf ("%s",PF_VarString(0));
}

//char	pr_string_temp[128];
char	pr_string_temp[PR_MAX_TEMPSTRING];	// 2001-10-25 Enhanced temp string handling by Maddes

void PF_ftos (void)
{
    float	v;
    int	i;	// 2000-01-14 Maximum precision for FTOS by Maddes
    v = G_FLOAT(OFS_PARM0);

    if (v == (int)v)
        sprintf (pr_string_temp, "%d",(int)v);
    else
// 2000-01-14 Maximum precision for FTOS by Maddes  start
    {
//		sprintf (pr_string_temp, "%5.1f",v);
        sprintf (pr_string_temp, "%1f", v);
        for (i=Q_strlen(pr_string_temp)-1 ; i>0 && pr_string_temp[i]=='0' && pr_string_temp[i-1]!='.' ; i--)
        {
            pr_string_temp[i] = 0;
        }
    }
// 2000-01-14 Maximum precision for FTOS by Maddes  end
    G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_fabs (void)
{
    float	v;
    v = G_FLOAT(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = fabs(v);
}

void PF_vtos (void)
{
    sprintf (pr_string_temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
    G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}


void PF_etos (void)
{
    sprintf (pr_string_temp, "entity %i", G_EDICTNUM(OFS_PARM0));
    G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}


void PF_Spawn (void)
{
    edict_t	*ed;
    ed = ED_Alloc();
    RETURN_EDICT(ed);
}

void PF_Remove (void)
{
    edict_t	*ed;

    ed = G_EDICT(OFS_PARM0);
    ED_Free (ed);
}


// entity (entity start, .string field, string match) find = #5;
void PF_Find (void)
{
    int		e;
    int		f;
    char	*s, *t;
    edict_t	*ed;

    e = G_EDICTNUM(OFS_PARM0);
    f = G_INT(OFS_PARM1);
    s = G_STRING(OFS_PARM2);
    if (!s)
        PR_RunError ("PF_Find: bad search string");

    for (e++ ; e < sv.num_edicts ; e++)
    {
        ed = EDICT_NUM(e);
        if (ed->free)
            continue;
        t = E_STRING(ed,f);
        if (!t)
            continue;
        if (!Q_strcmp(t,s))
        {
            RETURN_EDICT(ed);
            return;
        }
    }

    RETURN_EDICT(sv.edicts);
}


void PR_CheckEmptyString (char *s)
{
    if (s[0] <= ' ')
        PR_RunError ("Bad string");
}

void PF_precache_file (void)
{
    // precache_file is only used to copy files with qcc, it does nothing
    G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound (void)
{
    char	*s;
    int		i;

    if (sv.state != ss_loading)
        PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

    s = G_STRING(OFS_PARM0);
    G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
    PR_CheckEmptyString (s);

    for (i=0 ; i<MAX_SOUNDS ; i++)
    {
        if (!sv.sound_precache[i])
        {
            sv.sound_precache[i] = s;
            return;
        }
        if (!Q_strcmp(sv.sound_precache[i], s))
            return;
    }
    PR_RunError ("PF_precache_sound: overflow");
}

void PF_precache_model (void)
{
    char	*s;
    int		i;

    if (sv.state != ss_loading)
        PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

    s = G_STRING(OFS_PARM0);
    G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
    PR_CheckEmptyString (s);

    for (i=0 ; i<MAX_MODELS ; i++)
    {
        if (!sv.model_precache[i])
        {
            sv.model_precache[i] = s;
            sv.models[i] = Mod_ForName (s, true);
            return;
        }
        if (!Q_strcmp(sv.model_precache[i], s))
            return;
    }
    PR_RunError ("PF_precache_model: overflow");
}


void PF_coredump (void)
{
    ED_PrintEdicts ();
}

void PF_traceon (void)
{
    pr_trace = true;
}

void PF_traceoff (void)
{
    pr_trace = false;
}

void PF_eprint (void)
{
    ED_PrintNum (G_EDICTNUM(OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
void PF_walkmove (void)
{
    edict_t	*ent;
    float	yaw, dist;
    vec3_t	move;
    dfunction_t	*oldf;
    int 	oldself;

    ent = PROG_TO_EDICT(pr_global_struct->self);
    yaw = G_FLOAT(OFS_PARM0);
    dist = G_FLOAT(OFS_PARM1);

    if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
    {
        G_FLOAT(OFS_RETURN) = 0;
        return;
    }

    yaw = yaw*M_PI*2 / 360;

    move[0] = cos(yaw)*dist;
    move[1] = sin(yaw)*dist;
    move[2] = 0;

// save program state, because SV_movestep may call other progs
    oldf = pr_xfunction;
    oldself = pr_global_struct->self;

    G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);


// restore program state
    pr_xfunction = oldf;
    pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
void PF_droptofloor (void)
{
    edict_t		*ent;
    vec3_t		end;
    trace_t		trace;

    ent = PROG_TO_EDICT(pr_global_struct->self);

    VectorCopy (ent->v.origin, end);
    end[2] -= 256;

    trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

    if (trace.fraction == 1 || trace.allsolid)
        G_FLOAT(OFS_RETURN) = 0;
    else
    {
        VectorCopy (trace.endpos, ent->v.origin);
        SV_LinkEdict (ent, false);
        ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
        ent->v.groundentity = EDICT_TO_PROG(trace.ent);
        G_FLOAT(OFS_RETURN) = 1;
    }
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
void PF_lightstyle (void)
{
    int		style;
    char	*val;
    client_t	*client;
    int			j;

    style = G_FLOAT(OFS_PARM0);
    val = G_STRING(OFS_PARM1);

// change the string in sv
    sv.lightstyles[style] = val;

// send message to all clients on this server
    if (sv.state != ss_active)
        return;

    for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
        if (client->active || client->spawned)
        {
            MSG_WriteByte (&client->message, svc_lightstyle);
            MSG_WriteByte (&client->message,style);
            MSG_WriteString (&client->message, val);
        }
}

void PF_rint (void)
{
    float	f;
    f = G_FLOAT(OFS_PARM0);
    if (f > 0)
        G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
    else
        G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
}
void PF_floor (void)
{
    G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}
void PF_ceil (void)
{
    G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom (void)
{
    edict_t	*ent;

    ent = G_EDICT(OFS_PARM0);

    G_FLOAT(OFS_RETURN) = SV_CheckBottom (ent);
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents (void)
{
    float	*v;

    v = G_VECTOR(OFS_PARM0);

    G_FLOAT(OFS_RETURN) = SV_PointContents (v);
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
void PF_nextent (void)
{
    int		i;
    edict_t	*ent;

    i = G_EDICTNUM(OFS_PARM0);
    while (1)
    {
        i++;
        if (i == sv.num_edicts)
        {
            RETURN_EDICT(sv.edicts);
            return;
        }
        ent = EDICT_NUM(i);
        if (!ent->free)
        {
            RETURN_EDICT(ent);
            return;
        }
    }
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
cvar_t	sv_aim_h = {"sv_aim_h", "1", "sv_aim_h[0.0 to 1.0] Horizontal aim, 0 is full automatic 1 is full player control.", true}; // Manoel Kasimier - horizontal autoaim
cvar_t	sv_aim = {"sv_aim", "1", "sv_aim[0.0 to 1.0] Vertical aim, 0 is full automatic 1 is full player control.", true}; // Manoel Kasimier - saving aim value

void PF_aim (void)
{
    edict_t	*ent, *check, *bestent;
    vec3_t	start, dir, end, bestdir;
    int		i, j;
    trace_t	tr;
    float	dist, bestdist;
    float	speed;

    ent = G_EDICT(OFS_PARM0);
    speed = G_FLOAT(OFS_PARM1);

    VectorCopy (ent->v.origin, start);
    start[2] += 20;

// try sending a trace straight
    VectorCopy (pr_global_struct->v_forward, dir);
    VectorMA (start, 2048, dir, end);
    tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
    if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM
            && (!teamplay.value || ent->v.team <=0 || ent->v.team != tr.ent->v.team) )
    {
        VectorCopy (pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
        return;
    }


// try all possible entities
    VectorCopy (dir, bestdir);
    // Manoel Kasimier - horizontal autoaim - begin
    if (sv_aim_h.value < 1)
        bestdist = sv_aim_h.value;
    else
        // Manoel Kasimier - horizontal autoaim - end
        bestdist = sv_aim.value;
    bestent = NULL;

    check = NEXT_EDICT(sv.edicts);
    for (i=1 ; i<sv.num_edicts ; i++, check = NEXT_EDICT(check) )
    {
        if (check->v.takedamage != DAMAGE_AIM)
            continue;
        if (check == ent)
            continue;
        if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
            continue;	// don't aim at teammate
        for (j=0 ; j<3 ; j++)
            end[j] = check->v.origin[j]
                     + 0.5*(check->v.mins[j] + check->v.maxs[j]);
        VectorSubtract (end, start, dir);
        VectorNormalize (dir);
        dist = DotProduct (dir, pr_global_struct->v_forward);
        if (dist < bestdist)
            continue;	// to far to turn
        tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
        if (tr.ent == check)
        {
            // can shoot at this one
            bestdist = dist;
            bestent = check;
        }
    }

    if (bestent)
    {
        VectorSubtract (bestent->v.origin, ent->v.origin, dir);
        dist = DotProduct (dir, pr_global_struct->v_forward);
        VectorScale (pr_global_struct->v_forward, dist, end);
        // Manoel Kasimier - horizontal autoaim - begin
        if (sv_aim_h.value < 1)
        {
            end[0] = dir[0];
            end[1] = dir[1];
        }
        // Manoel Kasimier - horizontal autoaim - end
        end[2] = dir[2];
        VectorNormalize (end);
        VectorCopy (end, G_VECTOR(OFS_RETURN));
    }
    else
    {
        VectorCopy (bestdir, G_VECTOR(OFS_RETURN));
    }
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw (void)
{
    edict_t		*ent;
    float		ideal, current, move, speed;

    ent = PROG_TO_EDICT(pr_global_struct->self);
    current = anglemod( ent->v.angles[1] );
    ideal = ent->v.ideal_yaw;
    speed = ent->v.yaw_speed;

    if (current == ideal)
        return;
    move = ideal - current;
    if (ideal > current)
    {
        if (move >= 180)
            move = move - 360;
    }
    else
    {
        if (move <= -180)
            move = move + 360;
    }
    if (move > 0)
    {
        if (move > speed)
            move = speed;
    }
    else
    {
        if (move < -speed)
            move = -speed;
    }

    ent->v.angles[1] = anglemod (current + move);
}

// 2001-09-16 Quake 2 builtin functions by id/Maddes
/*
==============
PF_changepitch
==============
*/
void PF_changepitch (void)
{
    edict_t		*ent;
    float		ideal, current, move, speed;
    eval_t		*val;	// 2001-09-16 PF_changepitch entity check by LordHavoc

    ent = G_EDICT(OFS_PARM0);
    current = anglemod( ent->v.angles[0] );

// 2001-09-16 PF_changepitch entity check by LordHavoc  start
// 2001-11-15 Better GetEdictFieldValue performance by LordHavoc/Maddes  start
    val = GetEdictFieldValue(ent, "idealpitch");
//	val = GETEDICTFIELDVALUE(ent, pr_field_idealpitch);
// 2001-11-15 Better GetEdictFieldValue performance by LordHavoc/Maddes  end
    if (val)
        ideal = val->_float;
    else
    {
        PR_RunError ("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch");
        return;
    }
// 2001-11-15 Better GetEdictFieldValue performance by LordHavoc/Maddes  start
    val = GetEdictFieldValue(ent, "pitch_speed");
//	val = GETEDICTFIELDVALUE(ent, pr_field_pitch_speed);
// 2001-11-15 Better GetEdictFieldValue performance by LordHavoc/Maddes  end
    if (val)
        speed = val->_float;
    else
    {
        PR_RunError ("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch");
        return;
    }
// 2001-09-16 PF_changepitch entity check by LordHavoc  end

    if (current == ideal)
        return;
    move = ideal - current;
    if (ideal > current)
    {
        if (move >= 180)
            move = move - 360;
    }
    else
    {
        if (move <= -180)
            move = move + 360;
    }
    if (move > 0)
    {
        if (move > speed)
            move = speed;
    }
    else
    {
        if (move < -speed)
            move = -speed;
    }

    ent->v.angles[0] = anglemod (current + move);
}
// 2001-09-16 Quake 2 builtin functions by id/Maddes

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

sizebuf_t *WriteDest (void)
{
    int		entnum;
    int		dest;
    edict_t	*ent;

    dest = G_FLOAT(OFS_PARM0);
    switch (dest)
    {
    case MSG_BROADCAST:
        return &sv.datagram;

    case MSG_ONE:
        ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
        entnum = NUM_FOR_EDICT(ent);
        if (entnum < 1 || entnum > svs.maxclients)
            PR_RunError ("WriteDest: not a client");
        return &svs.clients[entnum-1].message;

    case MSG_ALL:
        return &sv.reliable_datagram;

    case MSG_INIT:
        return &sv.signon;

    default:
        PR_RunError ("WriteDest: bad destination");
        break;
    }

    return NULL;
}

void PF_WriteByte (void)
{
    MSG_WriteByte (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteChar (void)
{
    MSG_WriteByte (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteShort (void)
{
    MSG_WriteShort (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteLong (void)
{
    MSG_WriteLong (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteAngle (void)
{
    MSG_WriteAngle (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteCoord (void)
{
    MSG_WriteCoord (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteString (void)
{
    MSG_WriteString (WriteDest(), G_STRING(OFS_PARM1));
}


void PF_WriteEntity (void)
{
    MSG_WriteShort (WriteDest(), G_EDICTNUM(OFS_PARM1));
}

//=============================================================================

int SV_ModelIndex (char *name);

void PF_makestatic (void)
{
    edict_t	*ent;
    int		i;
    float	scale=1;
    vec3_t	scalev= {0,0,0};
    short	glow_size=0;
    byte    glow_red,glow_green,glow_blue;
    int		bits=0;
    eval_t *val;

    ent = G_EDICT(OFS_PARM0);

    MSG_WriteByte (&sv.signon,svc_spawnstatic);

    if (current_protocol == PROTOCOL_QBS8)
    {
        if (val = GetEdictFieldValue(ent, "alpha"))
        {
            ent->alpha = ENTALPHA_ENCODE(val->_float); //qb: ent->alpha
            //qb:  fitzquake  don't send invisible entities unless they have effects
            if (ent->alpha == ENTALPHA_ZERO && !ent->v.effects)
                return;
        }
        MSG_WriteShort (&sv.signon, SV_ModelIndex(pr_strings + ent->v.model));
        MSG_WriteByte (&sv.signon, ent->alpha); //qb:
    }
    else MSG_WriteByte (&sv.signon, SV_ModelIndex(pr_strings + ent->v.model));

    MSG_WriteByte (&sv.signon, ent->v.frame);
    MSG_WriteByte (&sv.signon, ent->v.colormap);
    MSG_WriteByte (&sv.signon, ent->v.skin);

    for (i=0 ; i<3 ; i++)
    {
        MSG_WriteCoord(&sv.signon, ent->v.origin[i]);
        MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
    }
    // Manoel Kasimier - QC Alpha Scale Glow - begin
    if (current_protocol == PROTOCOL_QBS8)
    {
        if ((val = GetEdictFieldValue(ent, "scale")))
        {
            scale = val->_float;
            bits |= U_SCALE;
        }
        if ((val = GetEdictFieldValue(ent, "scalev")))
        {
            for (i=0 ; i<3 ; i++)
                scalev[i] = val->vector[i];
            bits |= U_SCALEV;
        }
        if ((val = GetEdictFieldValue(ent, "glow_size")))
        {
            glow_size = val->_float;
            bits |= U_GLOW_SIZE;
        }
        if ((val = GetEdictFieldValue(ent, "glow_red")))
        {
            glow_red = (byte)(val->_float);
            bits |= U_GLOW_RED;
        }
        if ((val = GetEdictFieldValue(ent, "glow_green")))
        {
            glow_green = (byte)(val->_float);
            bits |= U_GLOW_GREEN;
        }
        if ((val = GetEdictFieldValue(ent, "glow_blue")))
        {
            glow_blue = (byte)(val->_float);
            bits |= U_GLOW_BLUE;
        }
        // write the message
        MSG_WriteLong(&sv.signon, bits);
        if (bits & U_SCALE)
            MSG_WriteFloat (&sv.signon, scale);
        if (bits & U_SCALEV)
            for (i=0 ; i<3 ; i++)
                MSG_WriteFloat (&sv.signon, scalev[i]);
        if (bits & U_GLOW_SIZE)
            MSG_WriteShort (&sv.signon, glow_size);
        if (bits & U_GLOW_RED)
            MSG_WriteByte (&sv.signon, glow_red);
        if (bits & U_GLOW_GREEN)
            MSG_WriteByte (&sv.signon, glow_green);
        if (bits & U_GLOW_BLUE)
            MSG_WriteByte (&sv.signon, glow_blue);
        MSG_WriteShort(&sv.signon, ent->v.effects);
    }
    // Manoel Kasimier - QC Alpha Scale Glow - end

// throw the entity away now
    ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void PF_setspawnparms (void)
{
    edict_t	*ent;
    int		i;
    client_t	*client;

    ent = G_EDICT(OFS_PARM0);
    i = NUM_FOR_EDICT(ent);
    if (i < 1 || i > svs.maxclients)
        PR_RunError ("Entity is not a client");

    // copy spawn parms out of the client_t
    client = svs.clients + (i-1);

    for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
        (&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void PF_changelevel (void)
{
    char	*s;

// make sure we don't issue two changelevels
    if (svs.changelevel_issued)
        return;
    svs.changelevel_issued = true;

    s = G_STRING(OFS_PARM0);
    Cbuf_AddText (va("changelevel %s\n",s));
}


// 2001-09-16 Quake 2 builtin functions: sin, cos, sqrt, etos by id/Maddes
void PF_sin (void)
{
    G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}

void PF_cos (void)
{
    G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}

void PF_sqrt (void)
{
    G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}


void PF_Fixme (void)
{
    PR_RunError ("unimplemented bulitin");
}

// 2001-09-16 New BuiltIn Function: cmd_find() by Maddes  start
/*
=================
PF_cmd_find

float cmd_find (string)
=================
*/
void PF_cmd_find (void)
{
    char	*cmdname;
    float	result;

    cmdname = G_STRING(OFS_PARM0);

    result = Cmd_Exists (cmdname);

    G_FLOAT(OFS_RETURN) = result;
}
// 2001-09-16 New BuiltIn Function: cmd_find() by Maddes  end
// 2001-09-16 New BuiltIn Function: cvar_find() by Maddes  start
/*
=================
PF_cvar_find

float cvar_find (string)
=================
*/
void PF_cvar_find (void)
{
    char	*varname;
    float	result;

    varname = G_STRING(OFS_PARM0);

    result = 0;
    if (Cvar_FindVar (varname))
    {
        result = 1;
    }

    G_FLOAT(OFS_RETURN) = result;
}
// 2001-09-16 New BuiltIn Function: cvar_find() by Maddes  end
// 2001-09-16 New BuiltIn Function: cvar_string() by Maddes  start
/*
=================
PF_cvar_string

string cvar_string (string)
=================
*/
void PF_cvar_string (void)
{
    char	*varname;
    cvar_t	*var;

    varname = G_STRING(OFS_PARM0);
    var = Cvar_FindVar (varname);
    if (!var)
    {
        Con_DPrintf ("Cvar_String: variable \"%s\" not found\n", varname);	// 2001-09-09 Made 'Cvar not found' a developer message by Maddes
        G_INT(OFS_RETURN) = OFS_NULL;
    }
    else
    {
        G_INT(OFS_RETURN) = var->string - pr_strings;
    }
}
// 2001-09-16 New BuiltIn Function: cvar_string() by Maddes  end
// 2001-09-16 New BuiltIn Function: WriteFloat() by Maddes  start
/*
PF_WriteFloat

void (float to, float f) WriteFloat
*/
void PF_WriteFloat (void)
{
    MSG_WriteFloat (WriteDest(), G_FLOAT(OFS_PARM1));
}
// 2001-09-16 New BuiltIn Function: WriteFloat() by Maddes  end
// 2001-09-25 New BuiltIn Function: etof() by Maddes  start
/*
=================
PF_etof

float etof (entity)
=================
*/
void PF_etof (void)
{
    G_FLOAT(OFS_RETURN) = G_EDICTNUM(OFS_PARM0);
}
// 2001-09-25 New BuiltIn Function: etof() by Maddes  end

// 2001-09-25 New BuiltIn Function: ftoe() by Maddes  start
/*
=================
PF_ftoe

entity ftoe (float)
=================
*/
void PF_ftoe (void)
{
    edict_t		*e;

    e = EDICT_NUM(G_FLOAT(OFS_PARM0));
    RETURN_EDICT(e);
}
// 2001-09-25 New BuiltIn Function: ftoe() by Maddes  end
// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes  start
/*
=================
PF_strzone

string strzone (string)
=================
*/
void PF_strzone (void)
{
    char *m, *p;
    m = G_STRING(OFS_PARM0);
    p = Q_calloc("PF_strzone", Q_strlen(m) + 1);
    Q_strcpy(p, m);

    G_INT(OFS_RETURN) = p - pr_strings;
}

/*
=================
PF_strunzone

string strunzone (string)
=================
*/
void PF_strunzone (void)
{
    free(G_STRING(OFS_PARM0));
    G_INT(OFS_PARM0) = OFS_NULL; // empty the def
};

/*
=================
PF_strlen

float Q_strlen (string)
=================
*/
void PF_strlen (void)
{
    char *p = G_STRING(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = Q_strlen(p);
}

/*
=================
PF_strcat

string Q_strcat (string, string)
=================
*/

void PF_strcat (void)
{
    char *s1, *s2;
    int		maxlen;	// 2001-10-25 Enhanced temp string handling by Maddes

    s1 = G_STRING(OFS_PARM0);
    s2 = PF_VarString(1);

// 2001-10-25 Enhanced temp string handling by Maddes  start
    pr_string_temp[0] = 0;
    if (Q_strlen(s1) < PR_MAX_TEMPSTRING)
    {
        Q_strcpy(pr_string_temp, s1);
    }
    else
    {
        Q_strncpy(pr_string_temp, s1, PR_MAX_TEMPSTRING);
        pr_string_temp[PR_MAX_TEMPSTRING-1] = 0;
    }

    maxlen = PR_MAX_TEMPSTRING - Q_strlen(pr_string_temp) - 1;	// -1 is EndOfString
    if (maxlen > 0)
    {
        if (maxlen > Q_strlen(s2))
        {
            Q_strcat (pr_string_temp, s2);
        }
        else
        {
            strncat (pr_string_temp, s2, maxlen);
            pr_string_temp[PR_MAX_TEMPSTRING-1] = 0;
        }
    }
// 2001-10-25 Enhanced temp string handling by Maddes  end

    G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_substring

string substring (string, float, float)
=================
*/
void PF_substring (void)
{
    int		offset, length;
    int		maxoffset;		// 2001-10-25 Enhanced temp string handling by Maddes
    char	*p;

    p = G_STRING(OFS_PARM0);
    offset = (int)G_FLOAT(OFS_PARM1); // for some reason, Quake doesn't like G_INT
    length = (int)G_FLOAT(OFS_PARM2);

    // cap values
    maxoffset = Q_strlen(p);
    if (offset > maxoffset)
    {
        offset = maxoffset;
    }
    if (offset < 0)
        offset = 0;
// 2001-10-25 Enhanced temp string handling by Maddes  start
    if (length >= PR_MAX_TEMPSTRING)
        length = PR_MAX_TEMPSTRING-1;
// 2001-10-25 Enhanced temp string handling by Maddes  end
    if (length < 0)
        length = 0;

    p += offset;
    Q_strncpy(pr_string_temp, p, length);
    pr_string_temp[length]=0;

    G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_stof

float stof (string)
=================
*/
// thanks Zoid, taken from QuakeWorld
void PF_stof (void)
{
    char	*s;

    s = G_STRING(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = Q_atof(s);
}

/*
=================
PF_stov

vector stov (string)
=================
*/
void PF_stov (void)
{
    char *v;
    int i;
    vec3_t d;

    v = G_STRING(OFS_PARM0);

    for (i=0; i<3; i++)
    {
        while(v && (v[0] == ' ' || v[0] == '\'')) //skip unneeded data
            v++;
        d[i] = Q_atof(v);
        while (v && v[0] != ' ') // skip to next space
            v++;
    }
    VectorCopy (d, G_VECTOR(OFS_RETURN));
}
// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes  end

// 2001-09-20 QuakeC file access by FrikaC/Maddes  start
/*
=================
PF_fopen

float fopen (string,float)
=================
*/
void PF_fopen (void) //qb: updated from mh FRIK_FILE tute
{
    char *p = G_STRING(OFS_PARM0);
    int fmode = G_FLOAT (OFS_PARM1);
    int h = 0;

    switch (fmode)
    {
    case 0: // read
        Sys_FileOpenRead (va("%s/%s",com_gamedir, p), &h);
        break;

//  case 1: // append -- sane version
//     h = Sys_FileOpenAppend (va("%s/%s", com_gamedir, p));
//     break;

    default: // write
        h = Sys_FileOpenWrite (va("%s/%s", com_gamedir, p));
        break;
    }

    // always common now
    G_FLOAT (OFS_RETURN) = (float) h;
}




/*
=================
PF_fclose

void fclose (float)
=================
*/
void PF_fclose (void)
{
    int h = (int)G_FLOAT(OFS_PARM0);
    Sys_FileClose(h);
}

/*
=================
PF_fgets

string fgets (float)
=================
*/
void PF_fgets (void)
{
    // reads one line (up to a \n) into a string
    int		h;
    int		i;
    int		count;
    char	buffer;

    h = (int)G_FLOAT(OFS_PARM0);

    count = Sys_FileRead(h, &buffer, 1);
    if (count && buffer == '\r')	// carriage return
    {
        count = Sys_FileRead(h, &buffer, 1);	// skip
    }
    if (!count)	// EndOfFile
    {
        G_INT(OFS_RETURN) = OFS_NULL;	// void string
        return;
    }

    i = 0;
    while (count && buffer != '\n')
    {
        if (i < PR_MAX_TEMPSTRING-1)	// no place for character in temp string
        {
            pr_string_temp[i++] = buffer;
        }

        // read next character
        count = Sys_FileRead(h, &buffer, 1);
        if (count && buffer == '\r')	// carriage return
        {
            count = Sys_FileRead(h, &buffer, 1);	// skip
        }
    };
    pr_string_temp[i] = 0;

    G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_fputs

void fputs (float,string)
=================
*/
void PF_fputs (void)
{
    // writes to file, like bprint
    float handle = G_FLOAT(OFS_PARM0);
    char *str = PF_VarString(1);
    Sys_FileWrite (handle, str, Q_strlen(str));
}
// 2001-09-20 QuakeC file access by FrikaC/Maddes  end

// 2001-11-15 DarkPlaces general builtin functions by LordHavoc  start
/*
=================
PF_fmin

Returns the minimum of two or more supplied floats

float fmin(float f1, float f2, ...)
=================
*/
void PF_fmin (void)
{
    // LordHavoc: 3+ argument enhancement suggested by FrikaC
    if (pr_argc == 2)
        G_FLOAT(OFS_RETURN) = min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
    else if (pr_argc >= 3)
    {
        int i;
        float f = G_FLOAT(OFS_PARM0);
        for (i = 1; i < pr_argc; i++)
            if (G_FLOAT((OFS_PARM0+i*3)) < f)
                f = G_FLOAT((OFS_PARM0+i*3));
        G_FLOAT(OFS_RETURN) = f;
    }
    else
        PR_RunError("fmin: must supply at least 2 floats\n");
}

/*
=================
PF_fmax

Returns the maximum of two or more supplied floats

float fmax(float f1, float f2, ...)
=================
*/
void PF_fmax (void)
{
    // LordHavoc: 3+ argument enhancement suggested by FrikaC
    if (pr_argc == 2)
        G_FLOAT(OFS_RETURN) = max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
    else if (pr_argc >= 3)
    {
        int i;
        float f = G_FLOAT(OFS_PARM0);
        for (i = 1; i < pr_argc; i++)
            if (G_FLOAT((OFS_PARM0+i*3)) > f)
                f = G_FLOAT((OFS_PARM0+i*3));
        G_FLOAT(OFS_RETURN) = f;
    }
    else
        PR_RunError("fmax: must supply at least 2 floats\n");
}

/*
=================
PF_fbound

Returns number bounded by supplied range

float fbound(float min, float f, float max)
=================
*/
void PF_fbound (void)
{
    G_FLOAT(OFS_RETURN) = bound(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
}

/*
=================
PF_fpow

Returns base raised to power exp (base^exp)

float fpow(float base, float exp)
=================
*/
void PF_fpow (void)
{
    G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}

/*
=================
PF_findfloat

Loops through all entities beginning with the "start" entity, and checks the named entity field for a match.

entity findfloat(entity start, .string fld, float match)
=================
*/
// LordHavoc: added this for searching float, int, and entity reference fields
void PF_FindFloat (void)
{
    int		e;
    int		f;
    float	s;
    edict_t	*ed;

    e = G_EDICTNUM(OFS_PARM0);
    f = G_INT(OFS_PARM1);
    s = G_FLOAT(OFS_PARM2);

    for (e++ ; e < sv.num_edicts ; e++)
    {
        ed = EDICT_NUM(e);
        if (ed->free)
            continue;
        if (E_FLOAT(ed,f) == s)
        {
            RETURN_EDICT(ed);
            return;
        }
    }

    RETURN_EDICT(sv.edicts);
}

/*
=================
PF_tracebox

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entities, and also slide box entities
if the tryents flag is set(?).

void(vector v1, vector mins, vector maxs, vector v2, float nomonsters, entity forent) tracebox
=================
*/
// LordHavoc: added this for my own use, VERY useful, similar to traceline
void PF_tracebox (void)
{
    float	*v1, *v2, *m1, *m2;
    trace_t	trace;
    int		nomonsters;
    edict_t	*ent;

    v1 = G_VECTOR(OFS_PARM0);
    m1 = G_VECTOR(OFS_PARM1);
    m2 = G_VECTOR(OFS_PARM2);
    v2 = G_VECTOR(OFS_PARM3);
    nomonsters = G_FLOAT(OFS_PARM4);
    ent = G_EDICT(OFS_PARM5);

    trace = SV_Move (v1, m1, m2, v2, nomonsters, ent);

    pr_global_struct->trace_allsolid = trace.allsolid;
    pr_global_struct->trace_startsolid = trace.startsolid;
    pr_global_struct->trace_fraction = trace.fraction;
    pr_global_struct->trace_inwater = trace.inwater;
    pr_global_struct->trace_inopen = trace.inopen;
    VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
    VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
    pr_global_struct->trace_plane_dist =  trace.plane.dist;
    if (trace.ent)
        pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
    else
        pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

/*
=================
PF_randomvec

Returns a vector of length < 1

vector randomvec()
=================
*/
void PF_randomvec (void)
{
    vec3_t		temp;
    do
    {
        temp[0] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
        temp[1] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
        temp[2] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
    }
    while (DotProduct(temp, temp) >= 1);
    VectorCopy (temp, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_copyentity

copies data from one entity to another

copyentity(src, dst)
=================
*/
void PF_copyentity (void)
{
    edict_t *in, *out;
    in = G_EDICT(OFS_PARM0);
    out = G_EDICT(OFS_PARM1);
    memcpy(out, in, pr_edict_size);
}

/*
=================
PF_setcolor

sets the color of a client and broadcasts the update to all connected clients

setcolor(clientent, value)
=================
*/
void PF_setcolor (void)
{
    client_t	*client;
    int			entnum, i;

    entnum = G_EDICTNUM(OFS_PARM0);
    i = G_FLOAT(OFS_PARM1);

    if (entnum < 1 || entnum > svs.maxclients)
    {
        Con_DPrintf ("PROGS.DAT tried to setcolor a non-client\n");
        return;
    }

    client = &svs.clients[entnum-1];
    client->colors = i;
    client->edict->v.team = (i & 15) + 1;

    MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
    MSG_WriteByte (&sv.reliable_datagram, entnum - 1);
    MSG_WriteByte (&sv.reliable_datagram, i);
}

// chained search for strings in entity fields
// entity(.string field, string match) findchain = #402;
void PF_findchain (void)
{
    int		i;
    int		f;
    char	*s, *t;
    edict_t	*ent, *chain;

    chain = (edict_t *)sv.edicts;

    f = G_INT(OFS_PARM0);
    s = G_STRING(OFS_PARM1);
    if (!s || !s[0])
    {
        RETURN_EDICT(sv.edicts);
        return;
    }

    ent = NEXT_EDICT(sv.edicts);
    for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT(ent))
    {
        if (ent->free)
            continue;
        t = E_STRING(ent,f);
        if (!t)
            continue;
        if (Q_strcmp(t,s))
            continue;

        ent->v.chain = EDICT_TO_PROG(chain);
        chain = ent;
    }

    RETURN_EDICT(chain);
}

// LordHavoc: chained search for float, int, and entity reference fields
// entity(.string field, float match) findchainfloat = #403;
void PF_findchainfloat (void)
{
    int		i;
    int		f;
    float	s;
    edict_t	*ent, *chain;

    chain = (edict_t *)sv.edicts;

    f = G_INT(OFS_PARM0);
    s = G_FLOAT(OFS_PARM1);

    ent = NEXT_EDICT(sv.edicts);
    for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT(ent))
    {
        if (ent->free)
            continue;
        if (E_FLOAT(ent,f) != s)
            continue;

        ent->v.chain = EDICT_TO_PROG(chain);
        chain = ent;
    }

    RETURN_EDICT(chain);
}
// 2001-11-15 DarkPlaces general builtin functions by LordHavoc  end

// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  start
/*
=================
PF_builtin_find

float builtin_find (string)
=================
*/
void PF_builtin_find (void)
{
    int		j;
    float	funcno;
    char	*funcname;

    funcno = 0;
    funcname = G_STRING(OFS_PARM0);

    // search function name
    for ( j=1 ; j < pr_ebfs_numbuiltins ; j++)
    {
        if ((pr_ebfs_builtins[j].funcname) && (!(Q_strcasecmp(funcname,pr_ebfs_builtins[j].funcname))))
        {
            break;	// found
        }
    }

    if (j < pr_ebfs_numbuiltins)
    {
        funcno = pr_ebfs_builtins[j].funcno;
    }

    G_FLOAT(OFS_RETURN) = funcno;
}
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end


// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  start
// for builtin function definitions see Quake Standards Group at http://www.quakesrc.org/
builtin_t *pr_builtins;
int pr_numbuiltins;
ebfs_builtin_t pr_ebfs_builtins[] =
{
    {   0, NULL, PF_Fixme },		// has to be first entry as it is needed for initialization in PR_LoadProgs()
    {   1, "makevectors", PF_makevectors },	// void(entity e)	makevectors 		= #1;
    {   2, "setorigin", PF_setorigin },		// void(entity e, vector o) setorigin	= #2;
    {   3, "setmodel", PF_setmodel },		// void(entity e, string m) setmodel	= #3;
    {   4, "setsize", PF_setsize },			// void(entity e, vector min, vector max) setsize = #4;
//	{   5, "fixme", PF_Fixme },				// void(entity e, vector min, vector max) setabssize = #5;
    {   6, "break", PF_break },				// void() break						= #6;
    {   7, "random", PF_random },			// float() random						= #7;
    {   8, "sound", PF_sound },				// void(entity e, float chan, string samp) sound = #8;
    {   9, "normalize", PF_normalize },		// vector(vector v) normalize			= #9;
    {  10, "error", PF_error },				// void(string e) error				= #10;
    {  11, "objerror", PF_objerror },		// void(string e) objerror				= #11;
    {  12, "vlen", PF_vlen },				// float(vector v) vlen				= #12;
    {  13, "vectoyaw", PF_vectoyaw },		// float(vector v) vectoyaw		= #13;
    {  14, "spawn", PF_Spawn },				// entity() spawn						= #14;
    {  15, "remove", PF_Remove },			// void(entity e) remove				= #15;
    {  16, "traceline", PF_traceline },		// float(vector v1, vector v2, float tryents) traceline = #16;
    {  17, "checkclient", PF_checkclient },	// entity() clientlist					= #17;
    {  18, "find", PF_Find },				// entity(entity start, .string fld, string match) find = #18;
    {  19, "precache_sound", PF_precache_sound },	// void(string s) precache_sound		= #19;
    {  20, "precache_model", PF_precache_model },	// void(string s) precache_model		= #20;
    {  21, "stuffcmd", PF_stuffcmd },		// void(entity client, string s)stuffcmd = #21;
    {  22, "findradius", PF_findradius },	// entity(vector org, float rad) findradius = #22;
    {  23, "bprint", PF_bprint },			// void(string s) bprint				= #23;
    {  24, "sprint", PF_sprint },			// void(entity client, string s) sprint = #24;
    {  25, "dprint", PF_dprint },			// void(string s) dprint				= #25;
    {  26, "ftos", PF_ftos },				// void(string s) ftos				= #26;
    {  27, "vtos", PF_vtos },				// void(string s) vtos				= #27;
    {  28, "coredump", PF_coredump },
    {  29, "traceon", PF_traceon },
    {  30, "traceoff", PF_traceoff },
    {  31, "eprint", PF_eprint },			// void(entity e) debug print an entire entity
    {  32, "walkmove", PF_walkmove },		// float(float yaw, float dist) walkmove
//	{  33, "fixme", PF_Fixme},				// float(float yaw, float dist)
    {  34, "droptofloor", PF_droptofloor },
    {  35, "lightstyle", PF_lightstyle },
    {  36, "rint", PF_rint },
    {  37, "floor", PF_floor },
    {  38, "ceil", PF_ceil },
//	{  39, "fixme", PF_Fixme },
    {  40, "checkbottom", PF_checkbottom },
    {  41, "pointcontents", PF_pointcontents },
//	{  42, "fixme", PF_Fixme },
    {  43, "fabs", PF_fabs },
    {  44, "aim", PF_aim },
    {  45, "cvar", PF_cvar },
    {  46, "localcmd", PF_localcmd },
    {  47, "nextent", PF_nextent },
    {  48, "particle", PF_particle },
    {  49, "ChangeYaw", PF_changeyaw },
    {  50, "localsound", PF_localsound }, //qb: plays an S_LocalSound
    {  51, "vectoangles", PF_vectoangles },

    {  52, "WriteByte", PF_WriteByte },
    {  53, "WriteChar", PF_WriteChar },
    {  54, "WriteShort", PF_WriteShort },
    {  55, "WriteLong", PF_WriteLong },
    {  56, "WriteCoord", PF_WriteCoord },
    {  57, "WriteAngle", PF_WriteAngle },
    {  58, "WriteString", PF_WriteString },
    {  59, "WriteEntity", PF_WriteEntity },

// 2001-09-16 Quake 2 builtin functions: sin, cos, sqrt, etos by id/Maddes  start
    {  60, "sin", PF_sin },
    {  61, "cos", PF_cos },
    {  62, "sqrt", PF_sqrt },
    {  63, "changepitch", PF_changepitch },	// 2001-09-16 Quake 2 builtin functions by id/Maddes
    {  64, "TraceToss", PF_TraceToss },		// 2001-09-16 Quake 2 builtin functions by id/Maddes
    {  65, "etos", PF_etos },
// 2001-09-16 Quake 2 builtin functions: sin, cos, sqrt, etos by id/Maddes  end

    {  67, "movetogoal", SV_MoveToGoal },
    {  68, "precache_file", PF_precache_file },
    {  69, "makestatic", PF_makestatic },

    {  70, "changelevel", PF_changelevel },
//	{  71, "fixme", PF_Fixme },

    {  72, "cvar_set", PF_cvar_set },
    {  73, "centerprint", PF_centerprint },

    {  74, "ambientsound", PF_ambientsound },

    {  75, "precache_model2", PF_precache_model },
    {  76, "precache_sound2", PF_precache_sound },	// precache_sound2 is different only for qcc
    {  77, "precache_file2", PF_precache_file },

    {  78, "setspawnparms", PF_setspawnparms },

    {  81, "stof", PF_stof },	// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes

// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  start
    {  90, "tracebox", PF_tracebox },
    {  91, "randomvec", PF_randomvec },
//	{  92, "getlight", PF_GetLight },	// not implemented yet
//	{  93, "cvar_create", PF_cvar_create },		// 2001-09-18 New BuiltIn Function: cvar_create() by Maddes
    {  94, "fmin", PF_fmin },
    {  95, "fmax", PF_fmax },
    {  96, "fbound", PF_fbound },
    {  97, "fpow", PF_fpow },
    {  98, "findfloat", PF_FindFloat },
    { PR_DEFAULT_FUNCNO_EXTENSION_FIND, "extension_find", PF_extension_find },	// 2001-10-20 Extension System by Lord Havoc/Maddes
//	{   0, "registercvar", PF_cvar_create },	// 0 indicates that this entry is just for remapping (because of name change)
    {   0, "checkextension", PF_extension_find },
// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  end

    { PR_DEFAULT_FUNCNO_BUILTIN_FIND, "builtin_find", PF_builtin_find },	// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes

    { 101, "cmd_find", PF_cmd_find },		// 2001-09-16 New BuiltIn Function: cmd_find() by Maddes
    { 102, "cvar_find", PF_cvar_find },		// 2001-09-16 New BuiltIn Function: cvar_find() by Maddes
    { 103, "cvar_string", PF_cvar_string },	// 2001-09-16 New BuiltIn Function: cvar_string() by Maddes

// not implemented yet
    /*
    	{ 105, "cvar_free", PF_cvar_free },		// 2001-09-18 New BuiltIn Function: cvar_free() by Maddes

    	{ 106, "NVS_InitSVCMsg", PF_NVS_InitSVCMsg },	// 2000-05-02 NVS SVC by Maddes
    */

    { 107, "WriteFloat", PF_WriteFloat },	// 2001-09-16 New BuiltIn Function: WriteFloat() by Maddes
    { 108, "etof", PF_etof },	// 2001-09-25 New BuiltIn Function: etof() by Maddes
    { 109, "ftoe", PF_ftoe },	// 2001-09-25 New BuiltIn Function: ftoe() by Maddes

// 2001-09-20 QuakeC file access by FrikaC/Maddes  start
    { 110, "fopen", PF_fopen },
    { 111, "fclose", PF_fclose },
    { 112, "fgets", PF_fgets },
    { 113, "fputs", PF_fputs },
    {   0, "open", PF_fopen },		// 0 indicates that this entry is just for remapping (because of name and number change)
    {   0, "close", PF_fclose },
    {   0, "read", PF_fgets },
    {   0, "write", PF_fputs },
// 2001-09-20 QuakeC file access by FrikaC/Maddes  end

// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes  start
    { 114, "Q_strlen", PF_strlen },
    { 115, "Q_strcat", PF_strcat },
    { 116, "substring", PF_substring },
    { 117, "stov", PF_stov },
    { 118, "strzone", PF_strzone },
    { 119, "strunzone", PF_strunzone },
    {   0, "zone", PF_strzone },		// 0 indicates that this entry is just for remapping (because of name and number change)
    {   0, "unzone", PF_strunzone },
// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes  end

// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  start
    { 400, "copyentity", PF_copyentity },
    { 401, "setcolor", PF_setcolor },
    { 402, "findchain", PF_findchain },
    { 403, "findchainfloat", PF_findchainfloat },
// not implemented yet
    /*
    	{ 404, "effect", PF_... },
    	{ 405, "te_blood", PF_... },
    	{ 406, "te_bloodshower", PF_... },
    	{ 407, "te_explosionrgb", PF_... },
    	{ 408, "te_particlecube", PF_... },
    	{ 409, "te_particlerain", PF_... },
    	{ 410, "te_particlesnow", PF_... },
    	{ 411, "te_spark", PF_... },
    	{ 412, "te_gunshotquad", PF_... },
    	{ 413, "te_spikequad", PF_... },
    	{ 414, "te_superspikequad", PF_... },
    	{ 415, "te_explosionquad", PF_... },
    	{ 416, "te_smallflash", PF_... },
    	{ 417, "te_customflash", PF_... },
    	{ 418, "te_gunshot", PF_... },
    	{ 419, "te_spike", PF_... },
    	{ 420, "te_superspike", PF_... },
    	{ 421, "te_explosion", PF_... },
    	{ 422, "te_tarexplosion", PF_... },
    	{ 423, "te_wizspike", PF_... },
    	{ 424, "te_knightspike", PF_... },
    	{ 425, "te_lavasplash", PF_... },
    	{ 426, "te_teleport", PF_... },
    	{ 427, "te_explosion2", PF_... },
    	{ 428, "te_lightning1", PF_... },
    	{ 429, "te_lightning2", PF_... },
    	{ 430, "te_lightning3", PF_... },
    	{ 431, "te_beam", PF_... },
    	{ 432, "vectorvectors", PF_... },
    */
// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  end
};

int pr_ebfs_numbuiltins = sizeof(pr_ebfs_builtins)/sizeof(pr_ebfs_builtins[0]);
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end
