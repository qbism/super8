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
// d_init.c: rasterization driver initialization

#include "quakedef.h"
#include "d_local.h"

#define NUM_MIPS	4

cvar_t	d_mipcap = {"d_mipcap", "0", "d_mipcap [0-3] max undetail texture level."};
cvar_t	d_mipscale = {"d_mipscale", "1", "d_mipscale [value] undetail distance."};
cvar_t	d_subdiv16 = {"d_subdiv16", "1", "d_subdiv16 [0/1] use division per 16 pixel instead of per 8 pixel. Faster, less accurate", true};

surfcache_t		*d_initial_rover;
qboolean		d_roverwrapped;
int				d_minmip;
float			d_scalemip[NUM_MIPS-1];

static float	basemip[NUM_MIPS-1] = {1.0, 0.5*0.8, 0.25*0.8};

void (*d_drawspans) (espan_t *pspan);
qboolean alphaspans;

/*
===============
D_Init
===============
*/
void D_Init (void)
{

    Cvar_RegisterVariable (&d_mipcap);
    Cvar_RegisterVariable (&d_mipscale);

    r_recursiveaffinetriangles = true;
    r_aliasuvscale = 1.0;
}



/*
===============
D_SetupFrame
===============
*/
void D_SetupFrame (void)
{
    int		i;

    d_viewbuffer = vid.buffer;
    screenwidth = vid.rowbytes;  //qb: was vid.width

    d_roverwrapped = false;
    d_initial_rover = sc_rover;

    d_minmip = d_mipcap.value;
    if (d_minmip > 3)
        d_minmip = 3;
    else if (d_minmip < 0)
        d_minmip = 0;

    for (i=0 ; i<(NUM_MIPS-1) ; i++)
        d_scalemip[i] = basemip[i] * d_mipscale.value;

    #if id386
    d_drawspans = D_DrawSpans16;
    #else
    d_drawspans = D_DrawSpans16_C;
    #endif

    d_aflatcolor = 0;
}


/*
===============
D_UpdateRects
===============
*/
void D_UpdateRects (vrect_t *prect)
{

// the software driver draws these directly to the vid buffer

    UNUSED(prect);
}

