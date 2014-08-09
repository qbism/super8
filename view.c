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

// view.c -- player eye positioning

#include "quakedef.h"
#include "r_local.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

//qb - Aardappel fisheye begin
void R_RenderView_Fisheye();
cvar_t  ffov = {"ffov", "129", "ffov[value] fisheye ffov.", true};
cvar_t  r_fisheye = {"r_fisheye", "0", "r_fisheye[0/1] Toggle fisheye view.", true}; //qb added
cvar_t  r_fishaccel = {"r_fishaccel", "0", "r_fishaccel[value] Accelleration of fisheye fov change.  Bind keys to positive and negative values for zoom effects.", false}; //qb:  for cheeezy zoom effect
int r_fviews;
//qb - Aardappel fisheye end

int  vibration_update[2];

cvar_t	scr_ofsx = {"scr_ofsx","0", "scr_ofsx[value] video screen x offset", false};
cvar_t	scr_ofsy = {"scr_ofsy","0", "scr_ofsy[value] video screen y offset", true}; // Manoel Kasimier - saved in the config file - edited
cvar_t	scr_ofsz = {"scr_ofsz","0", "scr_ofsz[value] video screen x offset", false};

cvar_t	v_fragtilt = {"v_fragtilt", "1", "v_fragtilt[0/1] Toggle tilt view when fragged."}; // Manoel Kasimier
cvar_t	v_fragtiltangle = {"v_fragtiltangle", "80", "v_fragtiltangle[angle] view angle when fragged."}; // Manoel Kasimier
cvar_t	cl_rollspeed = {"cl_rollspeed", "200", "cl_rollspeed[time] How quickly to tilt view to roll angle."};
cvar_t	cl_rollangle = {"cl_rollangle", "2.0", "cl_rollangle[angle] Angle to tilt view when turning."};

cvar_t	cl_bob = {"cl_bob","0.007", "cl_bob[value] Amount of view bob when moving.", false};
cvar_t	cl_bobcycle = {"cl_bobcycle","0.6", "cl_bobcycle[value] View bob frequency.", false};
cvar_t	cl_bobup = {"cl_bobup","0.5", "cl_bobup[0.0 - 1.0] Proportion view bob up.", false};
cvar_t  cl_bobmodel = {"cl_bobmodel", "3", "cl_bobmodel[1-7] various types of weapon bob.", true};
cvar_t  cl_bobmodel_speed = {"cl_bobmodel_speed", "7", "cl_bobmodel_speed[1-20] speed of weapon bob.", true};
cvar_t  cl_bobmodel_side = {"cl_bobmodel_side", "0.3", "cl_bobmodel_side[0.0-1.0] side-to-side weapon bob speed.", true};
cvar_t  cl_bobmodel_up = {"cl_bobmodel_up", "0.15", "cl_bobmodel_up[0.0-1.0] up-and-down weapon bob speed.", true};

cvar_t	v_kicktime = {"v_kicktime", "0.5", "v_kicktime[time] Time length of view kick.", false};
cvar_t	v_kickroll = {"v_kickroll", "0.6", "v_kickroll[angle] View roll for kick.", false};
cvar_t	v_kickpitch = {"v_kickpitch", "0.6", "v_kickpitch[angle] View pitch for kick.", false};
cvar_t  v_gunkick = {"v_gunkick", "1", "v_gunkick[0/1] Toggles view 'kick' from recoil.", true}; //qb - from directq

cvar_t	v_iyaw_cycle = {"v_iyaw_cycle", "2", "v_iyaw_cycle[time] Yaw cycle when v_idlescale is active.", false};
cvar_t	v_iroll_cycle = {"v_iroll_cycle", "0.5", "v_iroll_cycle[time] Roll cycle when v_idlescale is active.", false};
cvar_t	v_ipitch_cycle = {"v_ipitch_cycle", "1", "v_ipitch_cycle[time] Pitch cycle when v_idlescale is active.", false};
cvar_t	v_iyaw_level = {"v_iyaw_level", "0.3", "v_iyaw_level[value] Yaw level when v_idlescale is active.", false};
cvar_t	v_iroll_level = {"v_iroll_level", "0.1", "v_iroll_level[value] Roll level when v_idlescale is active.", false};
cvar_t	v_ipitch_level = {"v_ipitch_level", "0.3", "v_ipitch_level[value] Pitch level when v_idlescale is active.", false};

cvar_t	v_idlescale = {"v_idlescale", "0", "v_idlescale [value] Scale of view idle effect.  0 is off. Otherwise generates view drift when player is idle.", false};

cvar_t	crosshair = {"crosshair", "2", "crosshair[0-5] Crosshair type.  0 is off.", true};
cvar_t	cl_crossx = {"cl_crossx", "0", "cl_crossx[value] Crosshair center x offset.", false};
cvar_t	cl_crossy = {"cl_crossy", "0", "cl_crossy[value] Crosshair center y offset.", false};

//qb replace gl_polyblend with r_polyblend
cvar_t	r_polyblend = {"r_polyblend", "1", "r_polyblend[0/1] Toggle full-screen color tint effects, like powerups and underwater.", false}; // Manoel Kasimier - r_polyblend
int	r_polyblend_old; // Manoel Kasimier - r_polyblend

cvar_t	cl_nobob = {"cl_nobob","0", "cl_nobob[0/1] Toggle to turn off view bobbing.", true}; // Manoel Kasimier - cl_nobob

float	v_dmg_time, v_dmg_roll, v_dmg_pitch;

float fisheye_accel; //qb


/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
vec3_t	forward, right, up;

float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
    float	sign;
    float	side;
    float	value;

    AngleVectors (angles, forward, right, up);
    if (cl_nobob.value) // Manoel Kasimier - cl_nobob
        return 0; // Manoel Kasimier - cl_nobob
    side = DotProduct (velocity, right);
    sign = side < 0 ? -1 : 1;
    side = fabs(side);

    value = cl_rollangle.value;
//	if (cl.inwater)
//		value *= 6;

    if (side < cl_rollspeed.value)
        side = side * value / cl_rollspeed.value;
    else
        side = value;

    return side*sign;

}


/*
===============
V_CalcBob

===============
*/
float V_CalcBob (void)
{
    float	bob;
    double	cycle; //qb: was float

    if (cl_bobcycle.value <= 0)
        cl_bobcycle.value = 0.01; //qb: don't divide by zero.
    if (cl_bobup.value <= 0)
        cl_bobup.value = 0.01; //qb: don't divide by zero.

    cycle = cl.ctime - (int)(cl.ctime/cl_bobcycle.value)*cl_bobcycle.value;//DEMO_REWIND - qb - Baker change
    cycle = cycle/cl_bobcycle.value;

    if (cycle < cl_bobup.value)
        cycle = M_PI * cycle / cl_bobup.value;
    else
        cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)

    bob = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
//Con_Printf ("speed: %5.1f\n", Length(cl.velocity));
    bob = bob*0.3 + bob*0.7*sin(cycle);
    if (bob > 4)
        bob = 4;
    else if (bob < -7)
        bob = -7;
    return bob;

}


//=============================================================================


cvar_t	v_centermove = {"v_centermove", "0.15", "v_centermove [value] How far the player must move forward before the view recenters when lookspring is active.", false};
cvar_t	v_centerspeed = {"v_centerspeed","200", "v_centerspeed[value]  How quickly view recenters when lookspring is active."}; // 500 // Manoel Kasimier - edited


void V_StartPitchDrift (void)
{
#if 1
    if (cl.laststop == cl.time)
    {
        return;		// something else is keeping it from drifting
    }
#endif
    if (cl.nodrift || !cl.pitchvel)
    {
        cl.pitchvel = v_centerspeed.value;
        cl.nodrift = false;
        cl.driftmove = 0;
    }
}

void V_StopPitchDrift (void)
{
    cl.laststop = cl.time;
    cl.nodrift = true;
    cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when
===============
*/
void V_DriftPitch (void)
{
    float		delta, move;

    if (noclip_anglehack || !cl.onground || cls.demoplayback )
    {
        cl.driftmove = 0;
        cl.pitchvel = 0;
        return;
    }

// don't count small mouse motion
    if (cl.nodrift)
    {
        if ( fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
            cl.driftmove = 0;
        else
            cl.driftmove += host_frametime;

        if ( cl.driftmove > v_centermove.value)
        {
            V_StartPitchDrift ();
        }
        return;
    }

    delta = cl.idealpitch - cl.viewangles[PITCH];

    if (!delta)
    {
        cl.pitchvel = 0;
        return;
    }

    move = host_frametime * cl.pitchvel;
    cl.pitchvel += host_frametime * v_centerspeed.value;

//Con_Printf ("move: %f (%f)\n", move, host_frametime);

    if (delta > 0)
    {
        if (move > delta)
        {
            cl.pitchvel = 0;
            move = delta;
        }
        cl.viewangles[PITCH] += move;
    }
    else if (delta < 0)
    {
        if (move > -delta)
        {
            cl.pitchvel = 0;
            move = -delta;
        }
        cl.viewangles[PITCH] -= move;
    }
}





/*
==============================================================================

						PALETTE FLASHES

==============================================================================
*/


cshift_t	cshift_empty = { {130,80,50}, 0 };
cshift_t	cshift_water = { {130,80,50}, 128 };
cshift_t	cshift_slime = { {0,25,5}, 150 };
cshift_t	cshift_lava = { {255,80,0}, 150 };

cvar_t		v_gamma = {"gamma", "0.95", "gamma [value] Sets the darkness level. Lower values are brighter. Recommended range is 0.7 to 1.3.", true};

byte		gammatable[256];	// palette is sent through this

void BuildGammaTable (float g)
{
    int		i, inf;

    if (g == 1.0)
    {
        for (i=0 ; i<256 ; i++)
            gammatable[i] = i;
        return;
    }

    for (i=0 ; i<256 ; i++)
    {
        inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
        if (inf < 0)
            inf = 0;
        if (inf > 255)
            inf = 255;
        gammatable[i] = inf;
    }
}

/*
=================
V_CheckGamma
=================
*/
qboolean V_CheckGamma (void)
{
//qb- just always do it.
//   static float oldgammavalue;

//   if (v_gamma.value == oldgammavalue)
//       return false;
//   oldgammavalue = v_gamma.value;

    BuildGammaTable (v_gamma.value);
    vid.recalc_refdef = 1;				// force a surface cache flush

    return true;
}



/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
    int		armor, blood;
    vec3_t	from;
    int		i;
    vec3_t	forward, right, up;
    entity_t	*ent;
    float	side;
    float	count;

    armor = MSG_ReadByte ();
    blood = MSG_ReadByte ();
    for (i=0 ; i<3 ; i++)
        from[i] = MSG_ReadCoord ();

    count = blood*0.5 + armor*0.5;
    if (count < 10)
        count = 10;

    cl.faceanimtime = cl.time + 0.2;		// but sbar face into pain frame

    cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
    if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
        cl.cshifts[CSHIFT_DAMAGE].percent = 0;
    if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
        cl.cshifts[CSHIFT_DAMAGE].percent = 150;

    if (armor > blood)
    {
        cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
    }
    else if (armor)
    {
        cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
    }
    else
    {
        cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
    }

//
// calculate view angle kicks
//
    ent = &cl_entities[cl.viewentity];

    VectorSubtract (from, ent->origin, from);
    VectorNormalize (from);

    AngleVectors (ent->angles, forward, right, up);

    side = DotProduct (from, right);
    v_dmg_roll = count*side*v_kickroll.value;

    side = DotProduct (from, forward);
    v_dmg_pitch = count*side*v_kickpitch.value;

    v_dmg_time = v_kicktime.value;
}


/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
    cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
    cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
    cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
    cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
    cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
    cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
    cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
    cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
    switch (contents)
    {
    case CONTENTS_EMPTY:
    case CONTENTS_SOLID:
    case CONTENTS_SKY: //qb: from johnfitz -- no blend in sky
        cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
        break;
    case CONTENTS_LAVA:
        cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
        break;
    case CONTENTS_SLIME:
        cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
        break;
    default:
        cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
    }
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
    if (cl.items & IT_QUAD)
    {
        cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
        cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
        cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
        cl.cshifts[CSHIFT_POWERUP].percent = 30;
    }
    else if (cl.items & IT_SUIT)
    {
        cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
        cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
        cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
        cl.cshifts[CSHIFT_POWERUP].percent = 20;
    }
    else if (cl.items & IT_INVISIBILITY)
    {
        cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
        cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
        cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
        cl.cshifts[CSHIFT_POWERUP].percent = 100;
    }
    else if (cl.items & IT_INVULNERABILITY)
    {
        cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
        cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
        cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
        cl.cshifts[CSHIFT_POWERUP].percent = 30;
    }
    else
        cl.cshifts[CSHIFT_POWERUP].percent = 0;
}


/*
=============
V_UpdatePalette
=============
*/

void V_UpdatePalette (void)
{
    int		i, j;
    qboolean	new;
    byte	*basepal, *newpal;
    byte	pal[768];
    int		r,g,b,shiftpercent;
    qboolean force;

    V_CalcPowerupCshift ();

    new = false;

    for (i=0 ; i<NUM_CSHIFTS ; i++)
    {
        if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
        {
            new = true;
            cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
        }
        for (j=0 ; j<3 ; j++)
            if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
            {
                new = true;
                cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
            }
    }

// drop the damage value
    cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime*150;
    if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
        cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
    cl.cshifts[CSHIFT_BONUS].percent -= host_frametime*100;
    if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
        cl.cshifts[CSHIFT_BONUS].percent = 0;

    force = V_CheckGamma ();
// Manoel Kasimier - r_polyblend begin
    if (!force) // no gamma changes
    {
        if (r_polyblend_old == r_polyblend.value) // r_polyblend hasn't changed
        {
            if (!r_polyblend_old) // no pallete changes allowed
                return;
            else if (!new) // no pallette changes
                return;
        }
    }
    r_polyblend_old = r_polyblend.value;
    /* Manoel Kasimier - r_polyblend end
    	if (!new && !force)
    		return;
    // Manoel Kasimier - r_polyblend */

    basepal = host_basepal;
    newpal = pal;

    for (i=0 ; i<256 ; i++)
    {
        r = basepal[0];
        g = basepal[1];
        b = basepal[2];
        basepal += 3;

        if (r_polyblend.value != 0) // Manoel Kasimier - r_polyblend
            for (j=0 ; j<NUM_CSHIFTS ; j++)
            {
                shiftpercent = cl.cshifts[j].percent; //qb
                r += (shiftpercent*(cl.cshifts[j].destcolor[0]))>>8; //qb - was (cl.cshifts[j].destcolor[0]-r)
                g += (shiftpercent*(cl.cshifts[j].destcolor[1]))>>8;
                b += (shiftpercent*(cl.cshifts[j].destcolor[2]))>>8;
            }
        newpal[0] = gammatable[min(r, 255)];  //qb - was just r
        newpal[1] = gammatable[min(g, 255)];
        newpal[2] = gammatable[min(b, 255)];
        newpal += 3;

    }
    VID_ShiftPalette (pal);
}


/*
==============================================================================

						VIEW RENDERING

==============================================================================
*/

float angledelta (float a)
{
    a = anglemod(a);
    if (a > 180)
        a -= 360;
    return a;
}

/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle (void)
{
    float	yaw, pitch, move;
    static float oldyaw = 0;
    static float oldpitch = 0;

    yaw = r_refdef.viewangles[YAW];
    pitch = -r_refdef.viewangles[PITCH];

    yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
    if (yaw > 10)
        yaw = 10;
    if (yaw < -10)
        yaw = -10;
    pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
    if (pitch > 10)
        pitch = 10;
    if (pitch < -10)
        pitch = -10;
    move = host_frametime*20;
    if (yaw > oldyaw)
    {
        if (oldyaw + move < yaw)
            yaw = oldyaw + move;
    }
    else
    {
        if (oldyaw - move > yaw)
            yaw = oldyaw - move;
    }

    if (pitch > oldpitch)
    {
        if (oldpitch + move < pitch)
            pitch = oldpitch + move;
    }
    else
    {
        if (oldpitch - move > pitch)
            pitch = oldpitch - move;
    }

    oldyaw = yaw;
    oldpitch = pitch;

    cl.viewent.angles[YAW] = r_refdef.viewangles[YAW] + yaw;
    cl.viewent.angles[PITCH] = - (r_refdef.viewangles[PITCH] + pitch);

    cl.viewent.angles[ROLL] -= v_idlescale.value * sin(cl.ctime*v_iroll_cycle.value) * v_iroll_level.value;//DEMO_REWIND - qb - Baker change
    cl.viewent.angles[PITCH] -= v_idlescale.value * sin(cl.ctime*v_ipitch_cycle.value) * v_ipitch_level.value;
    cl.viewent.angles[YAW] -= v_idlescale.value * sin(cl.ctime*v_iyaw_cycle.value) * v_iyaw_level.value;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (void)
{
    entity_t	*ent;

    ent = &cl_entities[cl.viewentity];

// absolutely bound refresh reletive to entity clipping hull
// so the view can never be inside a solid wall

    if (r_refdef.vieworg[0] < ent->origin[0] - 14)
        r_refdef.vieworg[0] = ent->origin[0] - 14;
    else if (r_refdef.vieworg[0] > ent->origin[0] + 14)
        r_refdef.vieworg[0] = ent->origin[0] + 14;
    if (r_refdef.vieworg[1] < ent->origin[1] - 14)
        r_refdef.vieworg[1] = ent->origin[1] - 14;
    else if (r_refdef.vieworg[1] > ent->origin[1] + 14)
        r_refdef.vieworg[1] = ent->origin[1] + 14;
    if (r_refdef.vieworg[2] < ent->origin[2] - 22)
        r_refdef.vieworg[2] = ent->origin[2] - 22;
    else if (r_refdef.vieworg[2] > ent->origin[2] + 30)
        r_refdef.vieworg[2] = ent->origin[2] + 30;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (void)
{
    r_refdef.viewangles[ROLL] += v_idlescale.value * sin(cl.ctime*v_iroll_cycle.value) * v_iroll_level.value; //DEMO_REWIND - qb - Baker change
    r_refdef.viewangles[PITCH] += v_idlescale.value * sin(cl.ctime*v_ipitch_cycle.value) * v_ipitch_level.value;
    r_refdef.viewangles[YAW] += v_idlescale.value * sin(cl.ctime*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (void)
{
    float		side;

    side = V_CalcRoll (cl_entities[cl.viewentity].angles, cl.velocity);
    r_refdef.viewangles[ROLL] += side;

    if (v_dmg_time > 0)
    {
        // Manoel Kasimier - begin
        vec3_t		v_dmg;
        float		dmg;
        v_dmg[0] = v_dmg_time/v_kicktime.value*v_dmg_pitch;
        v_dmg[1] = 0;
        v_dmg[2] = v_dmg_time/v_kicktime.value*v_dmg_roll;
        dmg = Length(v_dmg);
        if (dmg >= 1.0 && cl_vibration.value != 2)
        {
            vibration_update[0] = true;
//			Con_Printf("%f\n", dmg);
        }
        // Manoel Kasimier - end
        r_refdef.viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
        r_refdef.viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
        v_dmg_time -= host_frametime;
    }
    // Manoel Kasimier - begin
    // é menor que zero, então mudamos o v_dmg_time pra -666
    // pra que o Vibration_Stop só seja ativado neste frame
    else if (v_dmg_time != -666.0) // hack to stop the vibration in this frame only
    {
        v_dmg_time = -666.0;
        if (cl_vibration.value != 2)
            Vibration_Stop (0);
    }
    // Manoel Kasimier - end

    if (cl.stats[STAT_HEALTH] <= 0 && !chase_active.value && v_fragtilt.value)
    {
        r_refdef.viewangles[ROLL] = v_fragtiltangle.value;	// dead view angle
        return;
    }

}


/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef (void)
{
    entity_t	*ent, *view;
    float		old;
    // Manoel Kasimier - begin
    // reset damage angle, so the view angle won't be wrong when the next map starts
    if (v_dmg_time != -666.0)
    {
        v_dmg_time = -666.0;
        Vibration_Stop (0);
        Vibration_Stop (1);
    }
    // Manoel Kasimier - end
// ent is the player model (visible when out of body)
    ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
    view = &cl.viewent;

    VectorCopy (ent->origin, r_refdef.vieworg);
    VectorCopy (ent->angles, r_refdef.viewangles);
    view->model = NULL;

// allways idle in intermission
    old = v_idlescale.value;
    v_idlescale.value = 1;
    V_AddIdle ();
    v_idlescale.value = old;
}

/*
==================
V_CalcRefdef

==================
*/
void V_CalcRefdef (void)
{
    entity_t	*ent, *view;
    int			i;
    vec3_t		forward, right, up;
    vec3_t		angles;
    float		bob, s;
    static float oldz = 0;
    vec3_t kickangle; //qb: v_gunkick from directq
    float  gunbobtime; //qb: engoo/sajt
    double xyspeed;
    float bspeed;

     V_DriftPitch ();

// ent is the player model (visible when out of body)
    ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
    view = &cl.viewent;


// transform the view offset by the model's matrix to get the offset from
// model origin for the view
    ent->angles[YAW] = cl.viewangles[YAW];	// the model should face
    // the view dir
    ent->angles[PITCH] = -cl.viewangles[PITCH];	// the model should face
    // the view dir

    if (!cl_nobob.value) // Manoel Kasimier - cl_nobob
        bob = V_CalcBob ();
    else bob = 0; // Manoel Kasimier - cl_nobob

// refresh position
    VectorCopy (ent->origin, r_refdef.vieworg);
    r_refdef.vieworg[2] += cl.viewheight + bob;

// never let it sit exactly on a node line, because a water plane can
// dissapear when viewed with the eye exactly on it.
// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
    r_refdef.vieworg[0] += 1.0/32;
    r_refdef.vieworg[1] += 1.0/32;
    r_refdef.vieworg[2] += 1.0/32;

    VectorCopy (cl.viewangles, r_refdef.viewangles);
    V_CalcViewRoll ();
    V_AddIdle ();

// offsets
    angles[PITCH] = -ent->angles[PITCH];	// because entity pitches are
    //  actually backward
    angles[YAW] = ent->angles[YAW];
    angles[ROLL] = ent->angles[ROLL];

    AngleVectors (angles, forward, right, up);

    // Manoel Kasimier - begin
    if (chase_active.value)
        for (i=0 ; i<3 ; i++)
            r_refdef.vieworg[i] += forward[i] + right[i] + up[i];
    else
        // Manoel Kasimier - end
        for (i=0 ; i<3 ; i++)
            r_refdef.vieworg[i] += scr_ofsx.value*forward[i]
                                   + scr_ofsy.value*right[i]
                                   + scr_ofsz.value*up[i];


    V_BoundOffsets ();

// set up gun position
    VectorCopy (cl.viewangles, view->angles);

    CalcGunAngle ();

    VectorCopy (ent->origin, view->origin);
    view->origin[2] += cl.viewheight;

    for (i=0 ; i<3 ; i++)
    {
        view->origin[i] += forward[i]*bob*0.4;
    }
    view->origin[2] += bob;
    if (!cl_nobob.value) // Manoel Kasimier - cl_nobob
        view->origin[2] += 2; // Manoel Kasimier - cl_nobob

    view->model = cl.model_precache[cl.stats[STAT_WEAPON]];
    view->frame = cl.stats[STAT_WEAPONFRAME];
    view->colormap = vid.colormap;

    // set up the refresh position
    VectorScale (cl.punchangle, v_gunkick.value, kickangle);
    if (v_gunkick.value) VectorAdd (r_refdef.viewangles, kickangle, r_refdef.viewangles);

// smooth out stair step ups
    if (cl.onground && ent->origin[2] - oldz > 0)
    {
        float steptime;

        steptime = cl.time - cl.oldtime;
        if (steptime < 0)
            steptime = 0;

        oldz += steptime * 80;
        if (oldz > ent->origin[2])
            oldz = ent->origin[2];
        if (ent->origin[2] - oldz > 12)
            oldz = ent->origin[2] - 12;
        r_refdef.vieworg[2] += oldz - ent->origin[2];
        view->origin[2] += oldz - ent->origin[2];
    }
    else
        oldz = ent->origin[2];

    if (chase_active.value)
        Chase_Update ();

    //qb:  simplified from engoo...
    //	  -----------------------------------------
    // ** SAJT'S KEGENDARY BOBMODEL CODE STARTS HERE! **
    //    -----------------------------------------
    if (cl_bobmodel.value)
    {
       gunbobtime = cl.time;
        s = gunbobtime * cl_bobmodel_speed.value;

        bspeed = xyspeed * 0.01f;
        xyspeed = sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);
        bspeed = bound(0, xyspeed, 400) * 0.01f;

        AngleVectors(view->angles, forward, right, up);


        // leilei - The engine now has other hacked types of DP bobbing
        // to capture other 'feels'.
        if (cl_bobmodel.value == 2)
        {
            // Arc2  bobbing
            bob = bspeed * cl_bobmodel_side.value * (cos(s));
            VectorMA(view->origin, bob, right, view->origin);
            bob = bspeed * cl_bobmodel_up.value * cos(s * 2);
            VectorMA(view->origin, bob, up, view->origin);
        }
        else if (cl_bobmodel.value == 3)
        {
            // Figure 8  bobbing
            bob = bspeed * cl_bobmodel_side.value * sin(s);
            VectorMA(view->origin, bob, right, view->origin);
            bob = bspeed * cl_bobmodel_up.value * sin(s * 2);
            VectorMA(view->origin, bob, up, view->origin);
        }
        else if (cl_bobmodel.value == 4)
        {
            // -ish Bobbing
            bob = bspeed * cl_bobmodel_side.value * sin(s) * 0.3;
            VectorMA(view->origin, bob, right, view->origin);
            bob = bspeed * cl_bobmodel_up.value * sin(s * 2);

            VectorMA(view->origin, bob, up, view->origin);

        }
        else if (cl_bobmodel.value == 5)
        {
            // A unique bob.
            // This one pushes the weapon toward the right
            // and down so it appears 'aiming' when no longer
            // moving. This will tick players off for those who
            // use the gun's center as an aiming reticle.
            //		t = t * 0.01; // soften the time!
            bob = bspeed * cl_bobmodel_side.value * (sin(s) + 6);
            VectorMA(view->origin, bob, right, view->origin);
            bob = bspeed * cl_bobmodel_up.value * (cos(s * 2) - 3);
            VectorMA(view->origin, bob, up, view->origin);

        }
        else if (cl_bobmodel.value == 6)
        {
            // A variation, but in figure 8

            bob = bspeed * cl_bobmodel_side.value * (sin(s) + 6);
            VectorMA(view->origin, bob, right, view->origin);

            bob = bspeed * cl_bobmodel_up.value * (sin(s * 2) - 4);
            VectorMA(view->origin, bob, up, view->origin);

        }
        else if (cl_bobmodel.value == 7)
        {

            bob = bspeed * cl_bobmodel_side.value * cos(s);
            VectorMA(view->origin, bob, right, view->origin);
            bob = bspeed * cl_bobmodel_up.value * cos(s * 2);
            VectorMA(view->origin, bob, up, view->origin);
        }
        else
        {
            // Default Darkplaces bobbing
            bob = bspeed * cl_bobmodel_side.value * sin(s);
            VectorMA(view->origin, bob, right, view->origin);
            bob = bspeed * cl_bobmodel_up.value * cos(s * 2);
            VectorMA(view->origin, bob, up, view->origin);
        }
    }
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void V_RenderView (void)
{
     if (con_forcedup)
        return;

    if (!sv_freezephysics.value)// || !sv_cheats.value) //qb
        R_LessenStains();  //qb ftestain

    if (cl.intermission)
    {
        // intermission / finale rendering
        V_CalcIntermissionRefdef ();
    }
    else
    {
        if (!cl.paused /* && (sv.maxclients > 1 || key_dest == key_game) */ )
            V_CalcRefdef ();
    }
    if (r_fisheye.value)
    {
        fisheye_accel += r_fishaccel.value;
        ffov.value += fisheye_accel;
        ffov.value = bound (30, ffov.value, 10000);
        R_RenderView_Fisheye ();//qb Aardappel fisheye
    }
    else
    {
               R_RenderView ();
    }
}

//============================================================================

/*
=============
V_Init
=============
*/
void V_Init (void)
{
    Cmd_AddCommand ("v_cshift", V_cshift_f);
    Cmd_AddCommand ("bf", V_BonusFlash_f);
    Cmd_AddCommand ("centerview", V_StartPitchDrift);

    Cvar_RegisterVariable (&v_centermove);
    Cvar_RegisterVariable (&v_centerspeed);

    Cvar_RegisterVariable (&v_iyaw_cycle);
    Cvar_RegisterVariable (&v_iroll_cycle);
    Cvar_RegisterVariable (&v_ipitch_cycle);
    Cvar_RegisterVariable (&v_iyaw_level);
    Cvar_RegisterVariable (&v_iroll_level);
    Cvar_RegisterVariable (&v_ipitch_level);

    Cvar_RegisterVariable (&v_idlescale);
    Cvar_RegisterVariable (&crosshair);
    Cvar_RegisterVariable (&cl_crossx);
    Cvar_RegisterVariable (&cl_crossy);
    Cvar_RegisterVariable (&r_polyblend); // Manoel Kasimier - r_polyblend
    Cvar_RegisterVariable (&cl_nobob); // Manoel Kasimier - cl_nobob
    Cvar_RegisterVariable (&v_fragtilt); // Manoel Kasimier
    Cvar_RegisterVariable (&v_fragtiltangle); // Manoel Kasimier

    Cvar_RegisterVariable (&scr_ofsx);
    Cvar_RegisterVariable (&scr_ofsy);
    Cvar_RegisterVariable (&scr_ofsz);
    Cvar_RegisterVariable (&cl_rollspeed);
    Cvar_RegisterVariable (&cl_rollangle);
    Cvar_RegisterVariable (&cl_bob);
    Cvar_RegisterVariable (&cl_bobcycle);
    Cvar_RegisterVariable (&cl_bobup);
    Cvar_RegisterVariable (&cl_bobmodel);
    Cvar_RegisterVariable (&cl_bobmodel_speed);
    Cvar_RegisterVariable (&cl_bobmodel_side);
    Cvar_RegisterVariable (&cl_bobmodel_up);

    Cvar_RegisterVariable (&v_kicktime);
    Cvar_RegisterVariable (&v_kickroll);
    Cvar_RegisterVariable (&v_kickpitch);
    Cvar_RegisterVariable (&v_gunkick); //qb- from directq

    BuildGammaTable (1.0);	// no gamma yet
    Cvar_RegisterVariable (&v_gamma);

//qb Aardappel fisheye begin
    Cvar_RegisterVariable (&ffov);
    Cvar_RegisterVariable (&r_fisheye); //qb added
    Cvar_RegisterVariable (&r_fishaccel); //qb
//qb Aardappel fisheye end
}

//qb fisheye from Aardappel begin
typedef byte B;

#define BOX_FRONT  0
#define BOX_BEHIND 2
#define BOX_LEFT   3
#define BOX_RIGHT  1
#define BOX_TOP    4
#define BOX_BOTTOM 5

#define PI 3.141592654

#define DEG(x) (x / PI * 180.0)
#define RAD(x) (x * PI / 180.0)

struct my_coords
{
    double x, y, z;
};

struct my_angles
{
    double yaw, pitch, roll;
};

void x_rot(struct my_coords *c, double pitch);
void y_rot(struct my_coords *c, double yaw);
void z_rot(struct my_coords *c, double roll);
void my_get_angles(struct my_coords *in_o, struct my_coords *in_u, struct my_angles *a);

// get_ypr()

void get_ypr(double yaw, double pitch, double roll, int side, struct my_angles *a)
{
    struct my_coords o, u;

    // get 'o' (observer) and 'u' ('this_way_up') depending on box side

    switch(side)
    {
    case BOX_FRONT:
        //printf("(FRONT)");
        o.x =  0.0;
        o.y =  0.0;
        o.z =  1.0;
        u.x =  0.0;
        u.y =  1.0;
        u.z =  0.0;
        break;
    case BOX_BEHIND:
        //printf("(BEHIND)");
        o.x =  0.0;
        o.y =  0.0;
        o.z = -1.0;
        u.x =  0.0;
        u.y =  1.0;
        u.z =  0.0;
        break;
    case BOX_LEFT:
        //printf("(LEFT)");
        o.x = -1.0;
        o.y =  0.0;
        o.z =  0.0;
        u.x = -1.0;
        u.y =  1.0;
        u.z =  0.0;
        break;
    case BOX_RIGHT:
        //printf("(RIGHT)");
        o.x =  1.0;
        o.y =  0.0;
        o.z =  0.0;
        u.x =  0.0;
        u.y =  1.0;
        u.z =  0.0;
        break;
    case BOX_TOP:
        //printf("(TOP)");
        o.x =  0.0;
        o.y = -1.0;
        o.z =  0.0;
        u.x =  0.0;
        u.y =  0.0;
        u.z = -1.0;
        break;
    case BOX_BOTTOM:
        //printf("(BOTTOM)");
        o.x =  0.0;
        o.y =  1.0;
        o.z =  0.0;
        u.x =  0.0;
        u.y =  0.0;
        u.z = -1.0;
        break;
    }

    //printf(" - [inputs: yaw = %.4f, pitch = %.4f, roll = %.4f]\n", yaw, pitch, roll);

    z_rot(&o, roll);
    z_rot(&u, roll);
    x_rot(&o, pitch);
    x_rot(&u, pitch);
    y_rot(&o, yaw);
    y_rot(&u, yaw);

    my_get_angles(&o, &u, a);

    /* normalise angles */

    while (a->yaw   <   0.0) a->yaw   += 360.0;
    while (a->yaw   > 360.0) a->yaw   -= 360.0;
    while (a->pitch <   0.0) a->pitch += 360.0;
    while (a->pitch > 360.0) a->pitch -= 360.0;
    while (a->roll  <   0.0) a->roll  += 360.0;
    while (a->roll  > 360.0) a->roll  -= 360.0;

    //printf("get_ypr -> %.4f, %.4f, %.4f\n", a->yaw, a->pitch, a->roll);
}

/* my_get_angles */

void my_get_angles(struct my_coords *in_o, struct my_coords *in_u, struct my_angles *a)
{
    double rad_yaw, rad_pitch;
    struct my_coords o, u;

    a->pitch = 0.0;
    a->yaw = 0.0;
    a->roll = 0.0;

    // make a copy of the coords

    o.x = in_o->x;
    o.y = in_o->y;
    o.z = in_o->z;
    u.x = in_u->x;
    u.y = in_u->y;
    u.z = in_u->z;

    //printf("%.4f, %.4f, %.4f - \n", o.x, o.y, o.z);

    // special case when looking straight up or down

    if ((o.x == 0.0) && (o.z == 0.0))
    {
        // printf("special!\n");
        a->yaw   = 0.0;
        if (o.y > 0.0)
        {
            a->pitch = -90.0;    // down
            a->roll = 180.0 - DEG(atan2(u.x, u.z));
        }
        else
        {
            a->pitch =  90.0;    // up
            a->roll = DEG(atan2(u.x, u.z));
        }
        return;
    }

    /******************************************************************************/

    // get yaw angle and then rotate o and u so that yaw = 0

    rad_yaw = atan2(-o.x, o.z);
    a->yaw  = DEG(rad_yaw);

    y_rot(&o, -rad_yaw);
    y_rot(&u, -rad_yaw);

    //printf("%.4f, %.4f, %.4f - stage 1\n", o.x, o.y, o.z);

    // get pitch and then rotate o and u so that pitch = 0

    rad_pitch = atan2(-o.y, o.z);
    a->pitch  = DEG(rad_pitch);

    x_rot(&o, -rad_pitch);
    x_rot(&u, -rad_pitch);

    //printf("%.4f, %.4f, %.4f - stage 2\n", u.x, u.y, u.z);

    // get roll

    a->roll = DEG(-atan2(u.x, u.y));

    //printf("yaw = %.4f, pitch = %.4f, roll = %.4f\n", a->yaw, a->pitch, a->roll);
}

/*******************************************************************************/

/* x_rot (pitch) */

void x_rot(struct my_coords *c, double pitch)
{
    double nx, ny, nz;

    nx = c->x;
    ny = (c->y * cos(pitch)) - (c->z * sin(pitch));
    nz = (c->y * sin(pitch)) + (c->z * cos(pitch));

    c->x = nx;
    c->y = ny;
    c->z = nz;

    /*printf("x_rot: %.4f, %.4f, %.4f\n", c->x, c->y, c->z);*/
}

/* y_rot (yaw) */

void y_rot(struct my_coords *c, double yaw)
{
    double nx, ny, nz;

    nx = (c->x * cos(yaw)) - (c->z * sin(yaw));
    ny = c->y;
    nz = (c->x * sin(yaw)) + (c->z * cos(yaw));

    c->x = nx;
    c->y = ny;
    c->z = nz;
}

/* z_rot (roll) */

void z_rot(struct my_coords *c, double roll)
{
    double nx, ny, nz;

    nx = (c->x * cos(roll)) - (c->y * sin(roll));
    ny = (c->x * sin(roll)) + (c->y * cos(roll));
    nz = c->z;

    c->x = nx;
    c->y = ny;
    c->z = nz;
}

void rendercopy(int *dest)
{
    int *p = (int*)vid.buffer;
    int x, y;
    R_RenderView();
    for(y = 0; y<vid.height; y++)
    {
        for(x = 0; x<(vid.width/4); x++,dest++) *dest = p[x];
        p += (vid.rowbytes/4);
    };
};

void renderlookup(B **offs, B* bufs)
{
    B *p = (B*)vid.buffer;
    int x, y;
    for(y = 0; y<vid.height; y++)
    {
        for(x = 0; x<vid.width; x++,offs++) p[x] = **offs;
        p += vid.rowbytes;
    };
};


void fisheyelookuptable(B **buf, int width, int height, B *scrp, double fov)
{
    int x, y;
    for(y = 0; y<height; y++) for(x = 0; x<width; x++)
        {
            double dx = x-width/2;
            double dy = -(y-height/2);
            double yaw = sqrt(dx*dx+dy*dy)*fov/((double)width);
            double roll = -atan2(dy,dx);
            double sx = sin(yaw) * cos(roll);
            double sy = sin(yaw) * sin(roll);
            double sz = cos(yaw);

            // determine which side of the box we need
            double abs_x = fabs(sx);
            double abs_y = fabs(sy);
            double abs_z = fabs(sz);
            int side;
            double xs=0, ys=0;
            if (abs_x > abs_y)
            {
                if (abs_x > abs_z)
                {
                    side = ((sx > 0.0) ? BOX_RIGHT : BOX_LEFT);
                }
                else
                {
                    side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND);
                }
            }
            else
            {
                if (abs_y > abs_z)
                {
                    side = ((sy > 0.0) ? BOX_TOP   : BOX_BOTTOM);
                }
                else
                {
                    side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND);
                }
            }

#define RC(x) ((x / 2.06) + 0.5)
#define R2(x) ((x / 2.03) + 0.5)

            // scale up our vector [x,y,z] to the box
            switch(side)
            {
            case BOX_FRONT:
                xs = RC( sx /  sz);
                ys = R2( sy /  sz);
                break;
            case BOX_BEHIND:
                xs = RC(-sx / -sz);
                ys = R2( sy / -sz);
                break;
            case BOX_LEFT:
                xs = RC( sz / -sx);
                ys = R2( sy / -sx);
                break;
            case BOX_RIGHT:
                xs = RC(-sz /  sx);
                ys = R2( sy /  sx);
                break;
            case BOX_TOP:
                xs = RC( sx /  sy);
                ys = R2( sz / -sy);
                break; //bot
            case BOX_BOTTOM:
                xs = RC(-sx /  sy);
                ys = R2( sz / -sy);
                break; //top??
            }

            if (xs <  0.0) xs = 0.0;
            if (xs >= 1.0) xs = 0.999;
            if (ys <  0.0) ys = 0.0;
            if (ys >= 1.0) ys = 0.999;
            *buf++=scrp+(((int)(xs*(double)width))+
                         ((int)(ys*(double)height))*width)+
                   side*width*height;
        };
};

void renderside(B* bufs, double yaw, double pitch, double roll, int side)
{
    struct my_angles a;
    get_ypr(RAD(yaw), RAD(pitch), RAD(roll), side, &a);
    if (side == BOX_RIGHT)
    {
        a.roll = -a.roll;
        a.pitch = -a.pitch;
    }
    if (side == BOX_LEFT)
    {
        a.roll = -a.roll;
        a.pitch = -a.pitch;
    }
    if (side == BOX_TOP)
    {
        a.yaw += 180.0;
        a.pitch = 180.0 - a.pitch;
    }
    r_refdef.viewangles[YAW] = a.yaw;
    r_refdef.viewangles[PITCH] = a.pitch;
    r_refdef.viewangles[ROLL] = a.roll;
    rendercopy((int *)bufs);
};

void R_RenderView_Fisheye()
{
    int width = vid.width; //r_refdef.vrect.width;
    int height = vid.height; //r_refdef.vrect.height;
    int scrsize = width*height;
    int fov = (int)ffov.value;
    double yaw = r_refdef.viewangles[YAW];
    double pitch = r_refdef.viewangles[PITCH];
    double roll = 0;//r_refdef.viewangles[ROLL];
    static int pwidth = -1;
    static int pheight = -1;
    static int pfov = -1;
    static int pviews = -1;
    static B *scrbufs = NULL;
    static B **offs = NULL;

    if (fov < 86) r_fviews = 1;
    else if (fov < 139) r_fviews = 3;
    else if (fov < 229) r_fviews = 5;
    else r_fviews = 6;

    if(fov<1) fov = 1;

    if(pwidth!=width || pheight!=height || pfov!=fov)
    {
        if(scrbufs) Q_free(scrbufs);
        if(offs) Q_free(offs);
        scrbufs = (B*)Q_malloc(scrsize*6, "srcbufs"); // front|right|back|left|top|bottom
        offs = (B**)Q_malloc(scrsize*sizeof(B*), "offs");
        if(!scrbufs || !offs) Sys_Error ("R_RenderView_Fisheye: Memory allocation error.") ; // the rude way
        pwidth = width;
        pheight = height;
        pfov = fov;
        fisheyelookuptable(offs,width,height,scrbufs,((double)fov)*PI/180.0);
    };

    if(r_fviews!=pviews)
    {
        int i;
        pviews = r_fviews;
        for(i = 0; i<scrsize*6; i++) scrbufs[i] = 0;
    };

    switch(r_fviews)
    {
    case 6:
        renderside(scrbufs+scrsize*2,yaw,pitch,roll, BOX_BEHIND);
    case 5:
        renderside(scrbufs+scrsize*5,yaw,pitch,roll, BOX_BOTTOM);
    case 4:
        renderside(scrbufs+scrsize*4,yaw,pitch,roll, BOX_TOP);
    case 3:
        renderside(scrbufs+scrsize*3,yaw,pitch,roll, BOX_LEFT);
    case 2:
        renderside(scrbufs+scrsize,  yaw,pitch,roll, BOX_RIGHT);
    default:
        renderside(scrbufs,          yaw,pitch,roll, BOX_FRONT);
    };

    r_refdef.viewangles[YAW] = yaw;
    r_refdef.viewangles[PITCH] = pitch;
    r_refdef.viewangles[ROLL] = roll;
    renderlookup(offs,scrbufs);
};
//qb fisheye end
