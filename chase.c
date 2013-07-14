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
// chase.c -- chase camera code

#include "quakedef.h"

cvar_t	chase_back = {"chase_back", "100", "chase_back [distance] Camera location behind player if chase_active = 1 or 2.", true};
cvar_t	chase_up = {"chase_up", "16", "chase_up [distance] Camera location behind player if chase_active = 1 or 2.", true};
cvar_t	chase_right = {"chase_right", "0", "chase_right [distance] Camera location behind player, when chase_active = 1 or 2.", true};

//qb:  custom chase by frag.machine
cvar_t	chase_active = {"chase_active", "0", "chase_active[ 0/1/2] Third-person cam.\n[1] Camera faces player direction.\n[2] Fixed camera direction.", true};
cvar_t  chase_roll = {"chase_roll", "0", "chase_roll [angle] Camera roll when chase_active = 2.", true};
cvar_t  chase_yaw = {"chase_yaw", "180", "chase_yaw [angle] Camera yaw when chase_active = 2.", true};
cvar_t  chase_pitch = {"chase_pitch", "45", "chase_pitch [angle] Camera pitch when chase_active = 2.", true};

vec3_t	chase_dest;
vec3_t	chase_dest_angles;

void Chase_Init (void)
{
    Cvar_RegisterVariable (&chase_back);
    Cvar_RegisterVariable (&chase_up);
    Cvar_RegisterVariable (&chase_right);
    Cvar_RegisterVariable (&chase_active);
    Cvar_RegisterVariable (&chase_roll); //qb: custom chase
    Cvar_RegisterVariable (&chase_yaw);
    Cvar_RegisterVariable (&chase_pitch);
}

void TraceLine (vec3_t start, vec3_t end, vec3_t impact)
{
    trace_t	trace;

    memset (&trace, 0, sizeof(trace));
    SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

    VectorCopy (trace.endpos, impact);
}


void Chase_Update (void)  //qb:  custom chase by frag.machine
{
    int     i;
    float   dist;
    vec3_t  forward, up, right;
    vec3_t  dest, stop;

    if ((int)chase_active.value != 2)
    {
        // if can't see player, reset
        AngleVectors (cl.viewangles, forward, right, up);

        // calc exact destination
        for (i=0 ; i<3 ; i++)
            chase_dest[i] = r_refdef.vieworg[i]
                            - forward[i]*chase_back.value
                            - right[i]*chase_right.value;
        chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;

        // find the spot the player is looking at
        VectorMA (r_refdef.vieworg, 4096, forward, dest);
        TraceLine (r_refdef.vieworg, dest, stop);

        // calculate pitch to look at the same spot from camera
        VectorSubtract (stop, r_refdef.vieworg, stop);
        dist = DotProduct (stop, forward);
        if (dist < 1)
            dist = 1;

        r_refdef.viewangles[PITCH] = -atan(stop[2] / dist) / M_PI * 180;

        TraceLine(r_refdef.vieworg, chase_dest, stop);
        if (Length(stop) != 0)
            VectorCopy(stop, chase_dest);

        // move towards destination
        VectorCopy (chase_dest, r_refdef.vieworg);
    }
    else
    {
        chase_dest[0] = r_refdef.vieworg[0] + chase_back.value;
        chase_dest[1] = r_refdef.vieworg[1] + chase_right.value;
        chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;

        // this is from the chasecam fix - start
        TraceLine (r_refdef.vieworg, chase_dest, stop);
        if (Length (stop) != 0)
        {
            VectorCopy (stop, chase_dest);
        }
        // this is from the chasecam fix - end

        VectorCopy (chase_dest, r_refdef.vieworg);
        r_refdef.viewangles[ROLL] = chase_roll.value;
        r_refdef.viewangles[YAW] = chase_yaw.value;
        r_refdef.viewangles[PITCH] = chase_pitch.value;
    }
}
