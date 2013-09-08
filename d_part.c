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
// d_part.c: software driver module for drawing particles

#include "quakedef.h"
#include "d_local.h"

/*
==============
D_DrawParticle
==============
*/
void D_DrawParticle_33_C (particle_t *pparticle) // Manoel Kasimier
{
    vec3_t	local, transformed;
    float	zi;
    byte	*pdest;
    short	*pz;
    int		i, izi, pix, count, u, v;
    byte	*dottexture; // Manoel Kasimier

// transform point
    VectorSubtract (pparticle->org, r_origin, local);

    transformed[2] = DotProduct(local, r_ppn);
    if (transformed[2] < PARTICLE_Z_CLIP)
        return;
    transformed[0] = DotProduct(local, r_pright);
    transformed[1] = DotProduct(local, r_pup);

// project the point
// FIXME: preadjust xcenter and ycenter
    zi = 1.0 / transformed[2];
    u = (int)(xcenter + zi * transformed[0] + 0.5);
    v = (int)(ycenter - zi * transformed[1] + 0.5);

    izi = (int)(zi * 0x8000);

    //qb:  better pix calc from MK 1.4.  Code stripped down for square pixels and no aspect adjust.
    d_pix_shift = 8 - (int) ( ( (float) r_refdef.vrect.width) / ( (1.0f / 320.0f) * (float) vid.width));
    pix = (double) ( (int) ( (float) izi * fovscale * r_part_scale.value * ( (float) vid.width / 50.0 )) >> d_pix_shift);

    if (pix < d_pix_min)
        pix = d_pix_min;
    else if (pix > (d_pix_max))
        pix = (int)(d_pix_max);

    if ((v > (d_vrectbottom_particle - (pix << d_y_aspect_shift))) ||  // Manoel Kasimier - FOV-based scaling - fixed
            (u > (d_vrectright_particle - pix)) || // Manoel Kasimier - FOV-based scaling - fixed
            (v < d_vrecty) ||
            (u < d_vrectx))
    {
        return;
    }

    pz = d_pzbuffer + (d_zwidth * v) + u;
    pdest = d_viewbuffer + d_scantable[v] + u;


// Manoel Kasimier - begin
    if (pix == 1)
    {
        if (pz[0] <= izi)
        {
            pz[0] = izi;
            pdest[0] = alphamap[(int)(pparticle->color*256 + pdest[0])];
        }
    }
    else if (pix == 2)
    {
        if (pz[0] <= izi)
        {
            pz[0] = izi;
            pdest[0] = alphamap[(int)(pparticle->color*256 + pdest[0])];
            pz[1] = izi;
            pdest[1] = alphamap[(int)(pparticle->color*256 + pdest[1])];
            pz[d_zwidth] = izi;
            pdest[screenwidth] = alphamap[(int)(pparticle->color*256 + pdest[screenwidth])];
            pz[d_zwidth+1] = izi;
            pdest[screenwidth+1] = alphamap[(int)(pparticle->color*256 + pdest[screenwidth+1])];
        }
    }
    else
    {
        if (pz[pix/2] <= izi)
        {
            count = pix;

            for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
                for (i=0 ; i<pix ; i++)
                {
                    pz[i] = izi;
                    pdest[i] = alphamap[(int)(pparticle->color*256 + pdest[i])];

                }
        }
    }
    // Manoel Kasimier - end
}

void D_DrawParticle_50_C (particle_t *pparticle) //qb
{
    vec3_t	local, transformed;
    float	zi;
    byte	*pdest;
    short	*pz;
    int		i, izi, pix, count, u, v;
    byte	*dottexture; // Manoel Kasimier

// transform point
    VectorSubtract (pparticle->org, r_origin, local);

    transformed[2] = DotProduct(local, r_ppn);
    if (transformed[2] < PARTICLE_Z_CLIP)
        return;
    transformed[0] = DotProduct(local, r_pright);
    transformed[1] = DotProduct(local, r_pup);

// project the point
// FIXME: preadjust xcenter and ycenter
    zi = 1.0 / transformed[2];
    u = (int)(xcenter + zi * transformed[0] + 0.5);
    v = (int)(ycenter - zi * transformed[1] + 0.5);

    izi = (int)(zi * 0x8000);
    d_pix_shift = 8 - (int) ( ( (float) r_refdef.vrect.width) / ( (1.0f / 320.0f) * (float) vid.width));
    pix = (double) ( (int) ( (float) izi * fovscale * r_part_scale.value * ( (float) vid.width / 50.0 )) >> d_pix_shift);

    if (pix < d_pix_min)
        pix = d_pix_min;
    else if (pix > (d_pix_max))
        pix = (int)(d_pix_max);

    if ((v > (d_vrectbottom_particle - (pix << d_y_aspect_shift))) ||  // Manoel Kasimier - FOV-based scaling - fixed
            (u > (d_vrectright_particle - pix)) || // Manoel Kasimier - FOV-based scaling - fixed
            (v < d_vrecty) ||
            (u < d_vrectx))
    {
        return;
    }

    pz = d_pzbuffer + (d_zwidth * v) + u;
    pdest = d_viewbuffer + d_scantable[v] + u;

// Manoel Kasimier - begin
    if (pix == 1)
    {
        if (pz[0] <= izi)
        {
            pz[0] = izi;
            pdest[0] = alpha50map[(int)(pparticle->color + pdest[0]*256)];
        }
    }
    else if (pix == 2)
    {
        if (pz[0] <= izi)
        {
            pz[0] = izi;
            pdest[0] = alpha50map[(int)(pparticle->color + pdest[0]*256)];
            pz[1] = izi;
            pdest[1] = alpha50map[(int)(pparticle->color + pdest[1]*256)];
            pz[d_zwidth] = izi;
            pdest[screenwidth] = alpha50map[(int)(pparticle->color + pdest[screenwidth]*256)];
            pz[d_zwidth+1] = izi;
            pdest[screenwidth+1] = alpha50map[(int)(pparticle->color + pdest[screenwidth+1]*256)];
        }
    }
    else
    {
        if (pz[pix/2] <= izi)
        {
            count = pix;

            for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
                for (i=0 ; i<pix ; i++)
                {
                    pz[i] = izi;
                    pdest[i] = alpha50map[(int)(pdest[i] + pparticle->color*256)];

                }
        }
    }
    // Manoel Kasimier - end
}



void D_DrawParticle_66_C (particle_t *pparticle) // Manoel Kasimier
{
    vec3_t	local, transformed;
    float	zi;
    byte	*pdest;
    short	*pz;
    int		i, izi, pix, count, u, v;
    byte	*dottexture; // Manoel Kasimier

// transform point
    VectorSubtract (pparticle->org, r_origin, local);

    transformed[2] = DotProduct(local, r_ppn);
    if (transformed[2] < PARTICLE_Z_CLIP)
        return;
    transformed[0] = DotProduct(local, r_pright);
    transformed[1] = DotProduct(local, r_pup);

// project the point
// FIXME: preadjust xcenter and ycenter
    zi = 1.0 / transformed[2];
    u = (int)(xcenter + zi * transformed[0] + 0.5);
    v = (int)(ycenter - zi * transformed[1] + 0.5);

    izi = (int)(zi * 0x8000);
    d_pix_shift = 8 - (int) ( ( (float) r_refdef.vrect.width) / ( (1.0f / 320.0f) * (float) vid.width));
    pix = (double) ( (int) ( (float) izi * fovscale * r_part_scale.value * ( (float) vid.width / 50.0 )) >> d_pix_shift);

    if (pix < d_pix_min)
        pix = d_pix_min;
    else if (pix > (d_pix_max))
        pix = (int)(d_pix_max);

    if ((v > (d_vrectbottom_particle - (pix << d_y_aspect_shift))) ||  // Manoel Kasimier - FOV-based scaling - fixed
            (u > (d_vrectright_particle - pix)) || // Manoel Kasimier - FOV-based scaling - fixed
            (v < d_vrecty) ||
            (u < d_vrectx))
    {
        return;
    }

    pz = d_pzbuffer + (d_zwidth * v) + u;
    pdest = d_viewbuffer + d_scantable[v] + u;

// Manoel Kasimier - begin
    if (pix == 1)
    {
        if (pz[0] <= izi)
        {
            pz[0] = izi;
            pdest[0] = alphamap[(int)(pparticle->color + pdest[0]*256)];
        }
    }
    else if (pix == 2)
    {
        if (pz[0] <= izi)
        {
            pz[0] = izi;
            pdest[0] = alphamap[(int)(pparticle->color + pdest[0]*256)];
            pz[1] = izi;
            pdest[1] = alphamap[(int)(pparticle->color + pdest[1]*256)];
            pz[d_zwidth] = izi;
            pdest[screenwidth] = alphamap[(int)(pparticle->color + pdest[screenwidth]*256)];
            pz[d_zwidth+1] = izi;
            pdest[screenwidth+1] = alphamap[(int)(pparticle->color + pdest[screenwidth+1]*256)];
        }
    }
    else
    {
        if (pz[pix/2] <= izi)
        {
            count = pix;

            for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
                for (i=0 ; i<pix ; i++)
                {
                    pz[i] = izi;
                    pdest[i] = alphamap[(int)(pdest[i] + pparticle->color*256)];

                }
        }
    }
    // Manoel Kasimier - end
}


void D_DrawParticle_C (particle_t *pparticle) // Manoel Kasimier
{
    vec3_t	local, transformed;
    float	zi;
    byte	*pdest;
    short	*pz;
    int		i, izi, pix, count, u, v;
    byte	*dottexture; // Manoel Kasimier

// transform point
    VectorSubtract (pparticle->org, r_origin, local);

    transformed[2] = DotProduct(local, r_ppn);
    if (transformed[2] < PARTICLE_Z_CLIP)
        return;
    transformed[0] = DotProduct(local, r_pright);
    transformed[1] = DotProduct(local, r_pup);

// project the point
// FIXME: preadjust xcenter and ycenter
    zi = 1.0 / transformed[2];
    u = (int)(xcenter + zi * transformed[0] + 0.5);
    v = (int)(ycenter - zi * transformed[1] + 0.5);

    izi = (int)(zi * 0x8000);
    d_pix_shift = 8 - (int) ( ( (float) r_refdef.vrect.width) / ( (1.0f / 320.0f) * (float) vid.width));
    pix = (double) ( (int) ( (float) izi * fovscale * r_part_scale.value * ( (float) vid.width / 50.0 )) >> d_pix_shift);

    if (pix < d_pix_min)
        pix = d_pix_min;
    else if (pix > (d_pix_max))
        pix = (int)(d_pix_max);

    if ((v > (d_vrectbottom_particle - (pix << d_y_aspect_shift))) ||  // Manoel Kasimier - FOV-based scaling - fixed
            (u > (d_vrectright_particle - pix)) || // Manoel Kasimier - FOV-based scaling - fixed
            (v < d_vrecty) ||
            (u < d_vrectx))
    {
        return;
    }

    pz = d_pzbuffer + (d_zwidth * v) + u;
    pdest = d_viewbuffer + d_scantable[v] + u;

// Manoel Kasimier - begin
    if (pix == 1)
    {
        if (pz[0] <= izi)
        {
            pz[0] = izi;
            pdest[0] = pparticle->color;
        }
    }
    else if (pix == 2)
    {
        if (pz[0] <= izi)
        {
            pz[0] = izi;
            pdest[0] = pparticle->color;
            pz[1] = izi;
            pdest[1] = pparticle->color;
            pz[d_zwidth] = izi;
            pdest[screenwidth] = pparticle->color;
            pz[d_zwidth+1] = izi;
            pdest[screenwidth+1] = pparticle->color;
        }
    }
    else
    {
        if (pz[pix/2] <= izi)
        {
            count = pix;

            for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
                for (i=0 ; i<pix ; i++)
                {
                    pz[i] = izi;
                    pdest[i] =  pparticle->color;

                }
        }
    }
    // Manoel Kasimier - end
}
