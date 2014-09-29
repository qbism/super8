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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h" //qb: ftestain

extern shadow_t		cl_shadows[MAX_SHADOWS];

drawsurf_t	r_drawsurf;

int             coloredlights; //qb: sanity check ala engoo
static int      lightleft, lightright, lightleftstep, lightrightstep;
static int	    sourcesstep, blocksize, sourcetstep;
static int		blockdivshift;
void			*prowdestbase;
byte	        *pbasesource;
static int		surfrowbytes;	// used by ASM files
unsigned		*r_lightptr;
unsigned		*r_colorptr; //qb: color
static int		r_stepback;
static int		r_lightwidth;
int		r_numhblocks, r_numvblocks;
byte	*r_source, *r_sourcemax;
byte    *vidcolmap; //qb: color
unsigned		blocklights[18*18];
unsigned		blockcolors[18*18]; //qb: color

const byte dithercolor[] =  //qb: fast randomish dither
{ 0, 2, 1, 3, 2, 3, 1, 0, 3, 0, 2, 1, 1, 2, 0, 3, 0, 3, 1, 2, 1, 2, 3, 0, 0, 1, 3, 2, 3, 2, 0, 1, 3, 2, 0, 1, 0, 3, 1, 2, 0};

void R_DrawSurfaceBlockColor8_mip0 (void);
void R_DrawSurfaceBlockColor8_mip1 (void);
void R_DrawSurfaceBlockColor8_mip2 (void);
void R_DrawSurfaceBlockColor8_mip3 (void);

void R_DrawSurfaceBlock8_mip0 (void);
void R_DrawSurfaceBlock8_mip1 (void);
void R_DrawSurfaceBlock8_mip2 (void);
void R_DrawSurfaceBlock8_mip3 (void);

static void	(*surfmiptableColor[4])(void) = //qb
{
    R_DrawSurfaceBlockColor8_mip0,
    R_DrawSurfaceBlockColor8_mip1,
    R_DrawSurfaceBlockColor8_mip2,
    R_DrawSurfaceBlockColor8_mip3
};

static void	(*surfmiptable[4])(void) =
{
    R_DrawSurfaceBlock8_mip0,
    R_DrawSurfaceBlock8_mip1,
    R_DrawSurfaceBlock8_mip2,
    R_DrawSurfaceBlock8_mip3
};

//qb: ftestain begin
extern cvar_t r_stains;
extern cvar_t r_stainfadetime;
extern cvar_t r_stainfadeamount;

#define	LMBLOCK_WIDTH		128
#define	LMBLOCK_HEIGHT		128
#define MAX_STAINMAPS		2000 //qb: was 64

typedef byte stmap;
stmap stainmaps[MAX_STAINMAPS*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];	//added to lightmap for added (hopefully) speed.
int			allocated[MAX_STAINMAPS][LMBLOCK_WIDTH];

//radius, x y z, a
void R_StainSurf (msurface_t *surf, float *parms)
{
    int			sd, td;
    float		dist, rad, minlight;
    vec3_t		impact, local;
    int			s, t;
    int			i;
    int			smax, tmax;
    float amm;
    int lim;
    mtexinfo_t	*tex;

    stmap *stainbase;

    lim = 255 - (r_stains.value*255);

    smax = (surf->extents[0]>>4)+1;
    tmax = (surf->extents[1]>>4)+1;
    tex = surf->texinfo;

    stainbase = stainmaps + surf->lightmaptexturenum*LMBLOCK_WIDTH*LMBLOCK_HEIGHT;
    stainbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s);

    rad = *parms;
    dist = DotProduct ((parms+1), surf->plane->normal) - surf->plane->dist;
    rad -= fabs(dist);
    minlight = 0;
    if (rad < minlight)	//not hit
        return;
    minlight = rad - minlight;

    for (i=0 ; i<3 ; i++)
    {
        impact[i] = (parms+1)[i] - surf->plane->normal[i]*dist;
    }

    local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
    local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

    local[0] -= surf->texturemins[0];
    local[1] -= surf->texturemins[1];

    for (t = 0 ; t<tmax ; t++)
    {
        td = local[1] - t*16;
        if (td < 0)
            td = -td;
        for (s=0 ; s<smax ; s++)
        {
            sd = local[0] - s*16;
            if (sd < 0)
                sd = -sd;
            if (sd > td)
                dist = sd + (td>>1);
            else
                dist = td + (sd>>1);
            if (dist < minlight)
            {
                amm = stainbase[(s)] - (dist - rad)*parms[4];
                stainbase[(s)] = bound(lim, amm, 255);

                surf->stained = true;
            }
        }
        stainbase += LMBLOCK_WIDTH;
    }

    if (surf->stained)
    {
        for (i = 0; i < 4; i++)
            if (surf->cachespots[i])
                surf->cachespots[i]->dlight = -1;
    }
}

//combination of R_AddDynamicLights and R_MarkLights
void R_Q1BSP_StainNode (mnode_t *node, float *parms)
{
    mplane_t	*splitplane;
    float		dist;
    msurface_t	*surf;
    int			i;

    if (node->contents < 0)
        return;

    splitplane = node->plane;
    dist = DotProduct ((parms+1), splitplane->normal) - splitplane->dist;

    if (dist > (*parms))
    {
        R_Q1BSP_StainNode (node->children[0], parms);
        return;
    }
    if (dist < (-*parms))
    {
        R_Q1BSP_StainNode (node->children[1], parms);
        return;
    }

// mark the polygons
    surf = cl.worldmodel->surfaces + node->firstsurface;
    for (i=0 ; i<node->numsurfaces ; i++, surf++)
    {
        if (surf->flags&~(SURF_DRAWTURB|SURF_PLANEBACK))
            continue;
        R_StainSurf(surf, parms);
    }

    R_Q1BSP_StainNode (node->children[0], parms);
    R_Q1BSP_StainNode (node->children[1], parms);
}

void R_AddStain(vec3_t org, float tint, float radius)
{
    entity_t *pe;
    int i;
    float parms[5];

    if (r_stains.value <= 0 || !cl.worldmodel)
        return;
    parms[0] = radius;
    parms[1] = org[0];
    parms[2] = org[1];
    parms[3] = org[2];
    parms[4] = tint;

    R_Q1BSP_StainNode(cl.worldmodel->nodes+cl.worldmodel->hulls[0].firstclipnode, parms);

    //now stain bsp models other than world.
    for (i=1 ; i< cl.num_entities ; i++)	//0 is world...
    {
        pe = &cl_entities[i];
        if (pe->model && pe->model->surfaces == cl.worldmodel->surfaces)
        {
            parms[1] = org[0] - pe->origin[0];
            parms[2] = org[1] - pe->origin[1];
            parms[3] = org[2] - pe->origin[2];
            R_Q1BSP_StainNode(pe->model->nodes+pe->model->hulls[0].firstclipnode, parms);
        }
    }
}

void R_WipeStains(void)
{
    memset(stainmaps, 255, sizeof(stainmaps));
}

void R_LessenStains(void)
{
    int i, j;
    msurface_t	*surf;

    int			smax, tmax;
    int			s, t;
    stmap *stain;
    int stride;
    int amount, limit;

    static float time;

    if (r_stains.value <= 0 || cl.paused)
    {
        time = 0;
        return;
    }

    time += host_frametime;
    if (time < r_stainfadetime.value)
        return;
    time-=r_stainfadetime.value;

    amount = r_stainfadeamount.value;
    limit = 255 - amount;

    surf = cl.worldmodel->surfaces;
    for (i=0 ; i<cl.worldmodel->numsurfaces ; i++, surf++)
    {
        if (surf->stained)
        {
            for (j = 0; j < 4; j++)
                if (surf->cachespots[j])
                    surf->cachespots[j]->dlight = -1;

            smax = (surf->extents[0]>>4)+1;
            tmax = (surf->extents[1]>>4)+1;

            stain = stainmaps + surf->lightmaptexturenum*LMBLOCK_WIDTH*LMBLOCK_HEIGHT;
            stain += (surf->light_t * LMBLOCK_WIDTH + surf->light_s);

            stride = (LMBLOCK_WIDTH-smax);

            surf->stained = false;

            for (t = 0 ; t<tmax ; t++, stain+=stride)
            {
                for (s=0 ; s<smax ; s++)
                {
                    if (*stain < limit)
                    {
                        *stain += amount;
                        surf->stained=true;
                    }
                    else	//reset to 255.
                        *stain = 255;

                    stain++;
                }
            }
        }
    }

}



// returns a texture number and the position inside it
int SWAllocBlock (int w, int h, int *x, int *y)
{
    int		i, j;
    int		best, best2;
    int		texnum;

    if (!w || !h)
        Sys_Error ("AllocBlock: bad size");

    for (texnum=0 ; texnum<MAX_STAINMAPS ; texnum++)
    {
        best = LMBLOCK_HEIGHT;

        for (i=0 ; i<LMBLOCK_WIDTH-w ; i++)
        {
            best2 = 0;

            for (j=0 ; j<w ; j++)
            {
                if (allocated[texnum][i+j] >= best)
                    break;
                if (allocated[texnum][i+j] > best2)
                    best2 = allocated[texnum][i+j];
            }
            if (j == w)
            {
                // this is a valid spot
                *x = i;
                *y = best = best2;
            }
        }

        if (best + h > LMBLOCK_HEIGHT)
            continue;

        for (i=0 ; i<w ; i++)
            allocated[texnum][*x + i] = best + h;

        return texnum;
    }

    Sys_Error ("AllocBlock: full");
    return 0;
}

void R_CreateSurfaceLightmap (msurface_t *surf)
{
    int		smax, tmax;

    if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
        return;
    if (surf->texinfo->flags & (TEX_SPECIAL))
        return;

    smax = (surf->extents[0]>>4)+1;
    tmax = (surf->extents[1]>>4)+1;

    surf->lightmaptexturenum = SWAllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
}

void R_BuildLightmaps(void)
{
    int i;
    msurface_t	*surf;

    memset(allocated, 0, sizeof(allocated));

    R_WipeStains();

    surf = cl.worldmodel->surfaces;
    for (i=0 ; i<cl.worldmodel->numsurfaces ; i++, surf++)
    {
//		if ( cl.worldmodel->surfaces[i].flags & SURF_DRAWSKY )
//			P_EmitSkyEffectTris(cl.worldmodel, &cl.worldmodel->surfaces[i]);
        R_CreateSurfaceLightmap(surf);
    }
}
//qb: ftestain end


/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (void)
{
    msurface_t *surf;
    int			lnum;
    int			sd, td;
    float		dist, rad, minlight;
    vec3_t		impact, local;
    vec3_t      origin_for_ent; //qb: from inside3d post: mankrip - dynamic lights on moving brush models fix
    int			s, t;
    int			i;
    int			smax, tmax;
    mtexinfo_t	*tex;

    surf = r_drawsurf.surf;
    smax = (surf->extents[0]>>4)+1;
    tmax = (surf->extents[1]>>4)+1;
    tex = surf->texinfo;

    for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
    {
        if (!(surf->dlightbits[lnum >> 5] & (1 << (lnum & 31)))) continue;  //qb: from MH

        rad = cl_dlights[lnum].radius;
        VectorSubtract (cl_dlights[lnum].origin, currententity->origin, origin_for_ent); //qb: from inside3d post:   mankrip - dynamic lights on moving brush models fix
        dist = DotProduct (origin_for_ent, surf->plane->normal) - //qb: from inside3d post:   mankrip - dynamic lights on moving brush models fix - edited
               surf->plane->dist;
        rad -= fabs(dist);
        minlight = cl_dlights[lnum].minlight;
        if (rad < minlight)
            continue;
        minlight = rad - minlight;

        for (i=0 ; i<3 ; i++)
            impact[i] = origin_for_ent[i] -  //qb: from inside3d post:   mankrip - dynamic lights on moving brush models fix - edited
                        surf->plane->normal[i]*dist;
 //       {
 //           surf->plane->normal[i]*dist;
 //       }

        local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
        local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

        local[0] -= surf->texturemins[0];
        local[1] -= surf->texturemins[1];

        for (t = 0 ; t<tmax ; t++)
        {
            td = local[1] - t*16;
            if (td < 0)
                td = -td;
            for (s=0 ; s<smax ; s++)
            {
                sd = local[0] - s*16;
                if (sd < 0)
                    sd = -sd;
                if (sd > td)
                    dist = sd + (td>>1);
                else
                    dist = td + (sd>>1);
                if (dist < minlight)
                {
                    unsigned temp;
                    temp = (rad - dist)*256;
                    i = t*smax + s;
                    if (!cl_dlights[lnum].dark)
                        blocklights[i] += temp;
                    else
                    {
                        if (blocklights[i] > temp)
                            blocklights[i] -= temp;
                        else
                            blocklights[i] = 0;
                    }
                    if(coloredlights == 1)
                    {
                        blockcolors[i]= cl_dlights[lnum].color;
                    }
                }
            }
        }
    }
}

void R_AddShadows (void) //qb: engoo shadowhack
{
    msurface_t *surf;
    int			lnum;
    int			sd, td;
    float		dist, rad, minlight;
    vec3_t		impact, local;
    int			s, t;
    int			i;
    int			smax, tmax;
    mtexinfo_t	*tex;

    surf = r_drawsurf.surf;
    smax = (surf->extents[0]>>4)+1;
    tmax = (surf->extents[1]>>4)+1;
    tex = surf->texinfo;

    for (lnum=0 ; lnum<MAX_SHADOWS ; lnum++)
    {
        if ( !(surf->shadowbits & (1<<lnum) ) )
            continue;		// not lit by this light

        rad = cl_shadows[lnum].radius;
        dist = DotProduct (cl_shadows[lnum].origin, surf->plane->normal) -
               surf->plane->dist;
        rad -= fabs(dist);
        minlight = cl_shadows[lnum].minlight;
        if (rad < minlight)
            continue;
        minlight = rad - minlight;

        for (i=0 ; i<3 ; i++)
        {
            impact[i] = cl_shadows[lnum].origin[i] -
                        surf->plane->normal[i]*dist;
        }

        local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
        local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

        local[0] -= surf->texturemins[0];
        local[1] -= surf->texturemins[1];

        for (t = 0 ; t<tmax ; t++)
        {
            td = local[1] - t*16;
            if (td < 0)
                td = -td;
            for (s=0 ; s<smax ; s++)
            {
                sd = local[0] - s*16;
                if (sd < 0)
                    sd = -sd;
                if (sd > td)
                    dist = sd + (td>>1);
                else
                    dist = td + (sd>>1);
                if (dist < minlight)

                {
                    unsigned temp;
                    temp = (rad - dist)*256;
                    i = t*smax + s;
                    //if (!cl_shadows[lnum].dark)
                    //	blocklights[i] += temp;
                    //			else
                    {
                        if (blocklights[i] > temp)
                            blocklights[i] -= temp;
                        else
                            blocklights[i] = 0;
                    }
                }

                //				blocklights[t*smax + s] += (rad - dist)*256;


            }
        }
    }
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/

void R_BuildLightMap (void)
{
    int			smax, tmax;
    int			t, i, surfsize;
    byte		*lightmap;
    byte		*colormap;  //qb: indexed colored
    int			r,g,b;
    float       coladd, colbase;
    unsigned	scale;
    int			maps;
    msurface_t	*surf;

    surf = r_drawsurf.surf;

    smax = (surf->extents[0]>>4)+1;
    tmax = (surf->extents[1]>>4)+1;
    surfsize = smax*tmax;
    lightmap = surf->samples;
    if (coloredlights == 1)
        colormap = surf->colorsamples; //qb:

    if (r_fullbright.value || !cl.worldmodel->lightdata)
    {
        for (i=0 ; i<surfsize ; i++)
            blocklights[i] = 31<<8;//0; // Manoel Kasimier - fullbright BSP textures fix
        return;
    }

// clear to ambient
    for (i=0 ; i<surfsize ; i++)
        blocklights[i] = r_refdef.ambientlight<<8;


// add all the lightmaps
    if (lightmap)
        for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
                maps++)
        {
            scale = r_drawsurf.lightadj[maps];	// 8.8 fraction
            for (i=0 ; i<surfsize ; i++)
            {
                if (coloredlights == 1)
                {
                    colbase = blocklights[i]/(blocklights[i] + lightmap[i] * scale + 0.1);
                    coladd = 1.0-colbase;
                    r = host_basepal[blockcolors[i]*3]*colbase + host_basepal[colormap[i]*3] * coladd * 0.85;
                    g = host_basepal[blockcolors[i]*3+1]*colbase + host_basepal[colormap[i]*3+1] * coladd *0.75;
                    b = host_basepal[blockcolors[i]*3+2]*colbase + host_basepal[colormap[i]*3+2] * coladd * 0.65;
                    blockcolors[i] = palmap[r>>2][g>>2][b>>2]; //qb: need to blend somehow
                }
                blocklights[i] += lightmap[i] * scale;
            }
            lightmap += surfsize;	// skip to next lightmap
            if (coloredlights == 1)
                colormap += surfsize;  //qb: skip to next colormap
        }

// add all the dynamic lights
    if (surf->dlightframe == r_framecount)
        R_AddDynamicLights ();

    if (surf->shadowframe == r_framecount)
    {
        R_AddShadows ();
    }

// and shadows



//qb: ftestains begin
    if (surf->stained)
    {
        stmap *stain;
        int sstride;
        int x, y;

        stain = stainmaps + surf->lightmaptexturenum*LMBLOCK_WIDTH*LMBLOCK_HEIGHT;
        stain += (surf->light_t * LMBLOCK_WIDTH + surf->light_s);

        sstride = LMBLOCK_WIDTH - smax;

        i=0;

        for (x = 0; x < tmax; x++, stain+=sstride)
        {
            for (y = 0; y < smax; y++, i++, stain++)
            {
                t = (255*256*256-127*256-(int)blocklights[i]*(*stain)) >> (16 - VID_CBITS);

                if (t < (1 << 6))
                    t = (1 << 6);

                blocklights[i] = t;
            }
        }
        return;
    }
//qb: ftestains end

// bound, invert, and shift
    for (i=0 ; i<surfsize ; i++)
    {
        t = (255*256 - (int)blocklights[i]) >> (8 - VID_CBITS);

        if (t < (1 << 6))
            t = (1 << 6);

        blocklights[i] = t;
    }
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
    int		relative;
    int		count;

    if (currententity->frame)
    {
        if (base->alternate_anims)
            base = base->alternate_anims;
    }

    if (!base->anim_total)
        return base;

    relative = (int)(cl.ctime*10) % base->anim_total; //DEMO_REWIND - qb: Baker change

    count = 0;
    while (base->anim_min > relative || base->anim_max <= relative)
    {
        base = base->anim_next;
        if (!base)
            Sys_Error ("R_TextureAnimation: broken cycle");
        if (++count > 100)
            Sys_Error ("R_TextureAnimation: infinite cycle");
    }

    return base;
}


/*
===============
R_DrawSurface
===============
*/
void R_DrawSurface (void)
{
    byte	*basetptr;
    int				smax, tmax, twidth;
    int				u;
    int				soffset, basetoffset, texwidth;
    int				horzblockstep;
    byte	*pcolumndest;
    void			(*pblockdrawer)(void);
    texture_t		*mt;

    vidcolmap = (byte *)vid.colormap; //qb
// calculate the lightings
    R_BuildLightMap ();

    surfrowbytes = r_drawsurf.rowbytes;

    mt = r_drawsurf.texture;

    r_source = (byte *)mt + mt->offsets[r_drawsurf.surfmip];

// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
// from a source range of 0 - 255

    texwidth = mt->width >> r_drawsurf.surfmip;

    blocksize = 16 >> r_drawsurf.surfmip;
    blockdivshift = 4 - r_drawsurf.surfmip;
    r_lightwidth = (r_drawsurf.surf->extents[0]>>4)+1;

    r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
    r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

//==============================

    if (r_pixbytes == 1)
    {
        if (coloredlights == 1)
            pblockdrawer = surfmiptableColor[r_drawsurf.surfmip];
        else
            pblockdrawer = surfmiptable[r_drawsurf.surfmip];
        // TODO: only needs to be set when there is a display settings change
        horzblockstep = blocksize;
    }
    else
    {
        pblockdrawer = R_DrawSurfaceBlock16;
        // TODO: only needs to be set when there is a display settings change
        horzblockstep = blocksize << 1;
    }

    smax = mt->width >> r_drawsurf.surfmip;
    twidth = texwidth;
    tmax = mt->height >> r_drawsurf.surfmip;
    sourcetstep = texwidth;
    r_stepback = tmax * twidth;

    r_sourcemax = r_source + (tmax * smax);

    soffset = r_drawsurf.surf->texturemins[0];
    basetoffset = r_drawsurf.surf->texturemins[1];

// << 16 components are to guarantee positive values for %
    soffset = ((soffset >> r_drawsurf.surfmip) + (smax << 16)) % smax;
    basetptr = &r_source[((((basetoffset >> r_drawsurf.surfmip)
                            + (tmax << 16)) % tmax) * twidth)];

    pcolumndest = r_drawsurf.surfdat;

    for (u=0 ; u<r_numhblocks; u++)
    {
        r_lightptr = blocklights + u;
        r_colorptr = blockcolors + u; //qb:

        prowdestbase = pcolumndest;

        pbasesource = basetptr + soffset;

        (*pblockdrawer)();

        soffset = soffset + blocksize;
        if (soffset >= smax)
            soffset = 0;

        pcolumndest += horzblockstep;
    }
}


//=============================================================================


/*
================
R_DrawSurfaceBlockColor8  //qb: combine standard lightmap and indexed color lightmap
================
*/

//qbism: macroize idea from mankrip
#define WRITEPROWDEST(i) { prowdest[i] = vidcolmap[((light += lightstep) & 0xFF00)+ lightcolormap[psource[i] + blend[dithercolor[(byte)light%41]]]];}
#define WRITEPROWLAST { prowdest[0] = vidcolmap[(light & 0xFF00)+ lightcolormap[psource[0] + blend[dithercolor[(byte)light%41]]]];}
    static int		lightstep, light;
    static int      colorL, colorR, colorL2, colorR2, blend[4]; //qb: indexed lit
    static byte	    *psource, *prowdest;

void R_DrawSurfaceBlockColor8_mip0 (void)
{
    int v,i;
    psource = pbasesource;
    prowdest = prowdestbase;

    for (v=0 ; v<r_numvblocks ; v++)
    {
        lightleft = r_lightptr[0];
        lightright = r_lightptr[1];
        r_lightptr += r_lightwidth;
        lightleftstep = (r_lightptr[0] - lightleft) >> 4;
        lightrightstep = (r_lightptr[1] - lightright) >> 4;

        colorL = r_colorptr[0];
        colorR = r_colorptr[1];
        r_colorptr += r_lightwidth;
        colorL2 = r_colorptr[0];
        colorR2 = r_colorptr[1];
        blend[0] = alpha50map[colorL+colorL2*256]*256;
        blend[1] = alpha50map[colorR+colorR2*256]*256;
        blend[2] = alpha50map[colorL+colorR*256]*256;
        blend[3] = alpha50map[colorL2+colorR2*256]*256;
        for (i=0 ; i<16 ; i++)
        {
            lightstep = (lightleft - lightright) >> 4;
            light = lightright;

            WRITEPROWDEST(15);
            WRITEPROWDEST(14);
            WRITEPROWDEST(13);
            WRITEPROWDEST(12);
            WRITEPROWDEST(11);
            WRITEPROWDEST(10);
            WRITEPROWDEST(9);
            WRITEPROWDEST(8);
            WRITEPROWDEST(7);
            WRITEPROWDEST(6);
            WRITEPROWDEST(5);
            WRITEPROWDEST(4);
            WRITEPROWDEST(3);
            WRITEPROWDEST(2);
            WRITEPROWDEST(1);
            WRITEPROWLAST;
            //prowdest[1] = vidcolmap[((light += lightstep) & 0xFF00) + lightcolormap[psource[1] + blend[dithercolor[(byte)light%41]]]];
            //prowdest[0] =                vidcolmap[(light & 0xFF00) + lightcolormap[psource[0] + blend[dithercolor[(byte)light%41]]]];

            psource += sourcetstep;
            lightright += lightrightstep;
            lightleft += lightleftstep;
            prowdest += surfrowbytes;
        }
        if (psource >= r_sourcemax)
            psource -= r_stepback;
    }
}


void R_DrawSurfaceBlockColor8_mip1 (void)
{
    int v,i;
    psource = pbasesource;
    prowdest = prowdestbase;

    for (v=0 ; v<r_numvblocks ; v++)
    {
        lightleft = r_lightptr[0];
        lightright = r_lightptr[1];
        r_lightptr += r_lightwidth;
        lightleftstep = (r_lightptr[0] - lightleft) >> 3;
        lightrightstep = (r_lightptr[1] - lightright) >> 3;

        colorL = r_colorptr[0];
        colorR = r_colorptr[1];
        r_colorptr += r_lightwidth;
        colorL2 = r_colorptr[0];
        colorR2 = r_colorptr[1];
        blend[0] = alpha50map[colorL+colorL2*256]*256;
        blend[1] = alpha50map[colorR+colorR2*256]*256;
        blend[2] = alpha50map[colorL+colorR*256]*256;
        blend[3] = alpha50map[colorL2+colorR2*256]*256;
        for (i=0 ; i<8 ; i++)
        {
            lightstep = (lightleft - lightright) >> 3;
            light = lightright;

            WRITEPROWDEST(7);
            WRITEPROWDEST(6);
            WRITEPROWDEST(5);
            WRITEPROWDEST(4);
            WRITEPROWDEST(3);
            WRITEPROWDEST(2);
            WRITEPROWDEST(1);
            WRITEPROWLAST;

            psource += sourcetstep;
            lightright += lightrightstep;
            lightleft += lightleftstep;
            prowdest += surfrowbytes;
        }
        if (psource >= r_sourcemax)
            psource -= r_stepback;
    }
}


void R_DrawSurfaceBlockColor8_mip2(void)
{
    int v,i;
    psource = pbasesource;
    prowdest = prowdestbase;

    for (v=0 ; v<r_numvblocks ; v++)
    {
        lightleft = r_lightptr[0];
        lightright = r_lightptr[1];
        r_lightptr += r_lightwidth;
        lightleftstep = (r_lightptr[0] - lightleft) >> 2;
        lightrightstep = (r_lightptr[1] - lightright) >> 2;

        colorL = r_colorptr[0];
        colorR = r_colorptr[1];
        r_colorptr += r_lightwidth;
        colorL2 = r_colorptr[0];
        colorR2 = r_colorptr[1];
        blend[0] = alpha50map[colorL+colorL2*256]*256;
        blend[1] = alpha50map[colorR+colorR2*256]*256;
        blend[2] = alpha50map[colorL+colorR*256]*256;
        blend[3] = alpha50map[colorL2+colorR2*256]*256;

        for (i=0 ; i<4 ; i++)
        {
            lightstep = (lightleft - lightright) >> 2;
            light = lightright;
            WRITEPROWDEST(3);
            WRITEPROWDEST(2);
            WRITEPROWDEST(1);
            WRITEPROWLAST;

            psource += sourcetstep;
            lightright += lightrightstep;
            lightleft += lightleftstep;
            prowdest += surfrowbytes;
        }
        if (psource >= r_sourcemax)
            psource -= r_stepback;
    }
}

void R_DrawSurfaceBlockColor8_mip3 (void)
{
    int v,i;
    psource = pbasesource;
    prowdest = prowdestbase;

    for (v=0 ; v<r_numvblocks ; v++)
    {
        lightleft = r_lightptr[0];
        lightright = r_lightptr[1];
        r_lightptr += r_lightwidth;
        lightleftstep = (r_lightptr[0] - lightleft) >> 1;
        lightrightstep = (r_lightptr[1] - lightright) >> 1;

        colorL = r_colorptr[0];
        colorR = r_colorptr[1];
        r_colorptr += r_lightwidth;
        colorL2 = r_colorptr[0];
        colorR2 = r_colorptr[1];
        blend[0] = alpha50map[colorL+colorL2*256]*256;
        blend[1] = alpha50map[colorR+colorR2*256]*256;
        blend[2] = alpha50map[colorL+colorR*256]*256;
        blend[3] = alpha50map[colorL2+colorR2*256]*256;
        for (i=0; i<2 ; i++)
        {
            lightstep = (lightleft - lightright) >> 1;
            light = lightright;

            WRITEPROWDEST(1);
            WRITEPROWLAST;
            psource += sourcetstep;
            lightright += lightrightstep;
            lightleft += lightleftstep;
            prowdest += surfrowbytes;
        }
        if (psource >= r_sourcemax)
            psource -= r_stepback;
    }
}


/*
================
R_DrawSurfaceBlock8  //qb: good old grays
================
*/

void R_DrawSurfaceBlock8_mip0 (void)
{
    int   v, i;

    psource = pbasesource;
    prowdest = prowdestbase;

    for (v=0 ; v<r_numvblocks ; v++)
    {
        lightleft = r_lightptr[0];
        lightright = r_lightptr[1];
        r_lightptr += r_lightwidth;
        lightleftstep = (r_lightptr[0] - lightleft) >> 4;
        lightrightstep = (r_lightptr[1] - lightright) >> 4;

        for (i=0 ; i<16 ; i++)
        {
            lightstep = (lightleft - lightright) >> 4;
            light = lightright;

            prowdest[15] = vidcolmap[((light += lightstep) & 0xFF00) + psource[15]];
            prowdest[14] = vidcolmap[((light += lightstep) & 0xFF00) + psource[14]];
            prowdest[13] = vidcolmap[((light += lightstep) & 0xFF00) + psource[13]];
            prowdest[12] = vidcolmap[((light += lightstep) & 0xFF00) + psource[12]];
            prowdest[11] = vidcolmap[((light += lightstep) & 0xFF00) + psource[11]];
            prowdest[10] = vidcolmap[((light += lightstep) & 0xFF00) + psource[10]];
            prowdest[9] = vidcolmap[((light += lightstep) & 0xFF00) + psource[9]];
            prowdest[8] = vidcolmap[((light += lightstep) & 0xFF00) + psource[8]];
            prowdest[7] = vidcolmap[((light += lightstep) & 0xFF00) + psource[7]];
            prowdest[6] = vidcolmap[((light += lightstep) & 0xFF00) + psource[6]];
            prowdest[5] = vidcolmap[((light += lightstep) & 0xFF00) + psource[5]];
            prowdest[4] = vidcolmap[((light += lightstep) & 0xFF00) + psource[4]];
            prowdest[3] = vidcolmap[((light += lightstep) & 0xFF00) + psource[3]];
            prowdest[2] = vidcolmap[((light += lightstep) & 0xFF00) + psource[2]];
            prowdest[1] = vidcolmap[((light += lightstep) & 0xFF00) + psource[1]];
            prowdest[0] = vidcolmap[(light & 0xFF00) + psource[0]];

            psource += sourcetstep;
            lightright += lightrightstep;
            lightleft += lightleftstep;
            prowdest += surfrowbytes;
        }

        if (psource >= r_sourcemax)
            psource -= r_stepback;
    }
}


/*
================
R_DrawSurfaceBlock8_mip1
================
*/
void R_DrawSurfaceBlock8_mip1 (void)
{
    int		v, i;

    psource = pbasesource;
    prowdest = prowdestbase;

    for (v=0 ; v<r_numvblocks ; v++)
    {
        lightleft = r_lightptr[0];
        lightright = r_lightptr[1];
        r_lightptr += r_lightwidth;
        lightleftstep = (r_lightptr[0] - lightleft) >> 3;
        lightrightstep = (r_lightptr[1] - lightright) >> 3;

        for (i=0 ; i<8 ; i++)
        {
            lightstep = (lightleft - lightright) >> 3;
            light = lightright;

            prowdest[7] = vidcolmap[((light += lightstep) & 0xFF00) + psource[7]];
            prowdest[6] = vidcolmap[((light += lightstep) & 0xFF00) + psource[6]];
            prowdest[5] = vidcolmap[((light += lightstep) & 0xFF00) + psource[5]];
            prowdest[4] = vidcolmap[((light += lightstep) & 0xFF00) + psource[4]];
            prowdest[3] = vidcolmap[((light += lightstep) & 0xFF00) + psource[3]];
            prowdest[2] = vidcolmap[((light += lightstep) & 0xFF00) + psource[2]];
            prowdest[1] = vidcolmap[((light += lightstep) & 0xFF00) + psource[1]];
            prowdest[0] = vidcolmap[(light & 0xFF00) + psource[0]];

            psource += sourcetstep;
            lightright += lightrightstep;
            lightleft += lightleftstep;
            prowdest += surfrowbytes;
        }

        if (psource >= r_sourcemax)
            psource -= r_stepback;
    }
}


/*
================
R_DrawSurfaceBlock8_mip2
================
*/

void R_DrawSurfaceBlock8_mip2 (void)
{
    int		v, i;

    psource = pbasesource;
    prowdest = prowdestbase;

    for (v=0 ; v<r_numvblocks ; v++)
    {
        lightleft = r_lightptr[0];
        lightright = r_lightptr[1];
        r_lightptr += r_lightwidth;
        lightleftstep = (r_lightptr[0] - lightleft) >> 2;
        lightrightstep = (r_lightptr[1] - lightright) >> 2;

        for (i=0 ; i<4 ; i++)
        {
            lightstep = (lightleft - lightright) >> 2;
            light = lightright;

            prowdest[3] = vidcolmap[((light += lightstep) & 0xFF00) + psource[3]];
            prowdest[2] = vidcolmap[((light += lightstep) & 0xFF00) + psource[2]];
            prowdest[1] = vidcolmap[((light += lightstep) & 0xFF00) + psource[1]];
            prowdest[0] = vidcolmap[(light & 0xFF00) + psource[0]];

            psource += sourcetstep;
            lightright += lightrightstep;
            lightleft += lightleftstep;
            prowdest += surfrowbytes;
        }
        if (psource >= r_sourcemax)
            psource -= r_stepback;
    }
}


/*
================
R_DrawSurfaceBlock8_mip3
================
*/

void R_DrawSurfaceBlock8_mip3 (void)
{
    int		v, i;

    psource = pbasesource;
    prowdest = prowdestbase;

    for (v=0 ; v<r_numvblocks ; v++)
    {
        lightleft = r_lightptr[0];
        lightright = r_lightptr[1];
        r_lightptr += r_lightwidth;
        lightleftstep = (r_lightptr[0] - lightleft) >> 1;
        lightrightstep = (r_lightptr[1] - lightright) >> 1;

        for (i=0 ; i<2 ; i++)
        {
            lightstep = (lightleft - lightright) >> 1;
            light = lightright;

            prowdest[1] = vidcolmap[((light += lightstep) & 0xFF00) + psource[1]];
            prowdest[0] = vidcolmap[(light & 0xFF00) + psource[0]];

            psource += sourcetstep;
            lightright += lightrightstep;
            lightleft += lightleftstep;
            prowdest += surfrowbytes;
        }
        if (psource >= r_sourcemax)
            psource -= r_stepback;
    }
}


/*
================
R_DrawSurfaceBlock16

FIXME: make this work
================
*/
void R_DrawSurfaceBlock16 (void)
{
    int				k;
    byte	*psource;
    int				lighttemp, lightstep, light;
    unsigned short	*prowdest;

    prowdest = (unsigned short *)prowdestbase;
    for (k=0 ; k<blocksize ; k++)
    {
        unsigned short	*pdest;
        byte	pix;
        int				b;

        psource = pbasesource;
        lighttemp = lightright - lightleft;
        lightstep = lighttemp >> blockdivshift;

        light = lightleft;
        pdest = prowdest;

        for (b=0; b<blocksize; b++)
        {
            pix = *psource;
            *pdest = vid.colormap16[(light & 0xFF00) + pix];
            psource += sourcesstep;
            pdest++;
            light += lightstep;
        }

        pbasesource += sourcetstep;
        lightright += lightrightstep;
        lightleft += lightleftstep;
        prowdest = (unsigned short *)((long)prowdest + surfrowbytes);
    }

    prowdestbase = prowdest;
}

