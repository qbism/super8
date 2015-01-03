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
// d_edge.c

#include "quakedef.h"
#include "d_local.h"

static int	miplevel;

float		scale_for_mip;
int			ubasestep, errorterm, erroradjustup, erroradjustdown;
int			vstartscan;

// FIXME: should go away
extern void			R_RotateBmodel (void);
extern void			R_TransformFrustum (void);

vec3_t		transformed_modelorg;

/*
=============
D_MipLevelForScale
=============
*/
int D_MipLevelForScale (float scale)
{
    int		lmiplevel;

    if (scale >= d_scalemip[0] )
        lmiplevel = 0;
    else if (scale >= d_scalemip[1] )
        lmiplevel = 1;
    else if (scale >= d_scalemip[2] )
        lmiplevel = 2;
    else
        lmiplevel = 3;

    if (lmiplevel < d_minmip)
        lmiplevel = d_minmip;

    return lmiplevel;
}


/*
==============
D_DrawSolidSurface
==============
*/

// FIXME: clean this up

void D_DrawSolidSurface (espan_t *pspan, int color)
{
    byte	*pdest;
    int		u, u2, pix;

    pix = (color<<24) | (color<<16) | (color<<8) | color;
    do
    {
        pdest = (byte *)d_viewbuffer + screenwidth*pspan->v;
        u = pspan->u;
        u2 = pspan->u + pspan->count - 1;
        ((byte *)pdest)[u] = pix;

        if (u2 - u < 8)
        {
            for (u++ ; u <= u2 ; u++)
                ((byte *)pdest)[u] = pix;
        }
        else
        {
            for (u++ ; u & 3 ; u++)
                ((byte *)pdest)[u] = pix;

            u2 -= 4;
            for ( ; u <= u2 ; u+=4)
                *(int *)((byte *)pdest + u) = pix;
            u2 += 4;
            for ( ; u <= u2 ; u++)
                ((byte *)pdest)[u] = pix;
        }
    }
    while ((pspan = pspan->pnext) != NULL);
}


/*
==============
D_CalcGradients
==============
*/
void D_CalcGradients (msurface_t *pface)
{
    float		mipscale;
    vec3_t		p_temp1;
    vec3_t		p_saxis, p_taxis;
    float		t;

    mipscale = 1.0 / (float)(1 << miplevel);

    TransformVector (pface->texinfo->vecs[0], p_saxis);
    TransformVector (pface->texinfo->vecs[1], p_taxis);

    t = xscaleinv * mipscale;
    d_sdivzstepu = p_saxis[0] * t;
    d_tdivzstepu = p_taxis[0] * t;

    t = yscaleinv * mipscale;
    d_sdivzstepv = -p_saxis[1] * t;
    d_tdivzstepv = -p_taxis[1] * t;

    d_sdivzorigin = p_saxis[2] * mipscale - xcenter * d_sdivzstepu -
                    ycenter * d_sdivzstepv;
    d_tdivzorigin = p_taxis[2] * mipscale - xcenter * d_tdivzstepu -
                    ycenter * d_tdivzstepv;

    VectorScale (transformed_modelorg, mipscale, p_temp1);

    t = 0x10000*mipscale;
    sadjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
              ((pface->texturemins[0] << 16) >> miplevel) + pface->texinfo->vecs[0][3]*t;
    tadjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
              ((pface->texturemins[1] << 16) >> miplevel) + pface->texinfo->vecs[1][3]*t;

// -1 (-epsilon) so we never wander off the edge of the texture
    bbextents = ((pface->extents[0] << 16) >> miplevel) - 1;
    bbextentt = ((pface->extents[1] << 16) >> miplevel) - 1;
}


/*
==============
D_DrawSurfaces
==============
*/
void D_DrawSurfaces (void)
{
    surf_t			*s;
    espan_t			*pspans;
    msurface_t		*pface;
    surfcache_t		*pcurrentcache;
    vec3_t			world_transformed_modelorg;
    vec3_t			local_modelorg;

    currententity = &cl_entities[0];
    TransformVector (modelorg, transformed_modelorg);
    VectorCopy (transformed_modelorg, world_transformed_modelorg);

// TODO: could preset a lot of this at mode set time
    if (r_drawflat.value)
    {
        for (s = &surfaces[1] ; s<surface_p ; s++)
        {
            if (! (pspans = s->spans))
                continue;
            // mankrip - begin
            if (r_overdraw && (s->flags & SURF_DRAWBACKGROUND)) // shouldn't overdraw
                continue;
            // mankrip - end

            d_zistepu = s->d_zistepu;
            d_zistepv = s->d_zistepv;
            d_ziorigin = s->d_ziorigin;

            D_DrawSolidSurface (pspans, (int)s->data & 0xFF);
            D_DrawZSpans (pspans); // mankrip - edited
        }
    }
    else
    {
        for (s = &surfaces[1] ; s<surface_p ; s++)
        {
            if (! (pspans = s->spans))
                continue;

             if (r_overdraw && (!(s->flags & SURF_DRAWTRANSLUCENT)))
                 continue;

            r_drawnpolycount++;

            d_zistepu = s->d_zistepu;
            d_zistepv = s->d_zistepv;
            d_ziorigin = s->d_ziorigin;

            if (s->flags & SURF_DRAWSKY)
            {
                D_DrawSkyScans8 (pspans); // mankrip
                D_DrawZSpans (pspans); // mankrip - edited
            }

            else if (s->flags & SURF_DRAWBACKGROUND)
            {
                // mankrip - translucent water - begin
                if (r_overdraw)
                    continue;
                // mankrip - translucent water - end

                // set up a gradient for the background surface that places it
                // effectively at infinity distance from the viewpoint
                d_zistepu = 0;
                d_zistepv = 0;
                d_ziorigin = -0.9;

                D_DrawSolidSurface (pspans, (int)r_clearcolor.value & 0xFF);
                D_DrawZSpans (pspans); // mankrip - edited
            }
            else if (s->flags & (SURF_DRAWTURB|SURF_DRAWTRANSLUCENT))
            {
                pface = s->data;
                miplevel = 0;
                if (s->insubmodel)
                {
                    // FIXME: we don't want to do all this for every polygon!
                    // TODO: store once at start of frame
                    currententity = s->entity;	//FIXME: make this passed in to R_RotateBmodel ()
                    VectorSubtract (r_origin, currententity->origin, local_modelorg);
                    TransformVector (local_modelorg, transformed_modelorg);

                    R_RotateBmodel ();	// FIXME: don't mess with the frustum, make entity passed in
                }
                D_CalcGradients (pface);

                if (s->flags & SURF_DRAWTURB)
                {
                    cacheblock = (pixel_t *) ((byte *)pface->texinfo->texture + pface->texinfo->texture->offsets[0]);
                    cachewidth = 64;
                    Turbulent8 (pspans);
                }
                else
                {
					pcurrentcache = D_CacheSurface (pface, miplevel);
                    cacheblock = (pixel_t *)pcurrentcache->data;
                    cachewidth = pcurrentcache->width;

                    if (s->flags & SURF_DRAWFENCE)
                        D_DrawSpans16_Fence(pspans);
                    else if (s->flags & SURF_DRAWGLASS33)
                        D_DrawSpans16_Blend(pspans);
                    else if (s->flags & SURF_DRAWGLASS50)
                        D_DrawSpans16_Blend50(pspans);
                    else if (s->flags & SURF_DRAWGLASS66)
                        D_DrawSpans16_BlendBackwards(pspans);
                   // else D_DrawSpans16_Blend(pspans); //qb: catchall
                }

                if (!r_overdraw) // mankrip - translucent water
                    D_DrawZSpans (pspans); // mankrip - edited

                if (s->insubmodel)
                {
                    // restore the old drawing state
                    // FIXME: we don't want to do this every time!
                    // TODO: speed up
                    //
                    currententity = &cl_entities[0];
                    VectorCopy (world_transformed_modelorg,
                                transformed_modelorg);
                    VectorCopy (base_vpn, vpn);
                    VectorCopy (base_vup, vup);
                    VectorCopy (base_vright, vright);
                    VectorCopy (base_modelorg, modelorg);
                    R_TransformFrustum ();
                }
            }
            // mankrip - skyboxes - begin
            // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
            else if (s->flags & SURF_DRAWSKYBOX)
            {
                extern byte	r_skypixels[6][1024*1024]; //qb: increased to 1024x1024

                pface = s->data;
                miplevel = 0;
                cacheblock = (byte *)(r_skypixels[pface->texinfo->texture->offsets[0]]);
                cachewidth = pface->texinfo->texture->width; // mankrip - hi-res skyboxes - edited

                d_zistepu = s->d_zistepu;
                d_zistepv = s->d_zistepv;
                d_ziorigin = s->d_ziorigin;

                D_CalcGradients (pface);

                //	(*d_drawspans) (pspans);  //mankrip
#if id386
                D_DrawSpans16 (pspans);
#else
                D_DrawSpans16_C (pspans);
#endif

                // set up a gradient for the background surface that places it
                // effectively at infinity distance from the viewpoint
                d_zistepu = 0;
                d_zistepv = 0;
                d_ziorigin = -0.9;

                D_DrawZSpans (pspans); // mankrip - edited
            }
            // Manoel Kasimier - skyboxes - end
            else if (!r_overdraw)
            {
                if (s->insubmodel)
                {
                    // FIXME: we don't want to do all this for every polygon!
                    // TODO: store once at start of frame
                    currententity = s->entity;	//FIXME: make this passed in to
                    // R_RotateBmodel ()
                    VectorSubtract (r_origin, currententity->origin, local_modelorg);
                    TransformVector (local_modelorg, transformed_modelorg);

                    R_RotateBmodel ();	// FIXME: don't mess with the frustum,
                    // make entity passed in
                }

                pface = s->data;
                miplevel = D_MipLevelForScale (s->nearzi * scale_for_mip * pface->texinfo->mipadjust);

                // FIXME: make this passed in to D_CacheSurface
                pcurrentcache = D_CacheSurface (pface, miplevel);

                cacheblock = (pixel_t *)pcurrentcache->data;
                cachewidth = pcurrentcache->width;

                D_CalcGradients (pface);

               // if (r_overdraw)
               //     D_DrawSpans16_Blend (pspans); // mankrip //qb: catchall

				//else
				(*d_drawspans) (pspans);

                D_DrawZSpans (pspans);

                if (s->insubmodel)
                {
                    // restore the old drawing state
                    // FIXME: we don't want to do this every time!
                    // TODO: speed up
                    currententity = &cl_entities[0];
                    VectorCopy (world_transformed_modelorg,
                                transformed_modelorg);
                    VectorCopy (base_vpn, vpn);
                    VectorCopy (base_vup, vup);
                    VectorCopy (base_vright, vright);
                    VectorCopy (base_modelorg, modelorg);
                    R_TransformFrustum ();
                }
            }
        }
    }
}



