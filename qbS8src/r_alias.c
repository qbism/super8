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
// r_alias.c: routines for setting up to draw alias models

#include "quakedef.h"
#include "r_local.h"

#define LIGHT_MIN	5		// lowest light value we'll allow, to avoid the
//  need for inner-loop light clamping

mtriangle_t		*ptriangles;
affinetridesc_t	r_affinetridesc;

void			*acolormap;	// FIXME: should go away

trivertx_t		*r_apverts;

// Manoel Kasimier - model interpolation - begin
trivertx_t		*r_apverts1;
trivertx_t		*r_apverts2;
trivertx_t		*bboxmin1, *bboxmax1, *bboxmin2, *bboxmax2; // Manoel Kasimier
float			blend;
// Manoel Kasimier - model interpolation - end

// TODO: these probably will go away with optimized rasterization
mdl_t				*pmdl;
vec3_t				r_plightvec;
int					r_ambientlight;
float				r_shadelight;
aliashdr_t			*paliashdr;
finalvert_t			*pfinalverts;
auxvert_t			*pauxverts;
static float		ziscale;
static model_t		*pmodel;

static vec3_t		alias_forward, alias_right, alias_up;

static maliasskindesc_t	*pskindesc;

int				r_amodels_drawn;
int				a_skinwidth;
int				r_anumverts;

float	aliastransform[3][4];

typedef struct
{
    int	index0;
    int	index1;
} aedge_t;

static aedge_t	aedges[12] =
{
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 5}, {1, 4}, {2, 7}, {3, 6}
};

#define NUMVERTEXNORMALS	162

//qbism- dumped from r_2d.c (deleted)
cvar_t	r_interpolation = {"r_interpolation", "1"}; // Manoel Kasimier - model interpolation
cvar_t	r_particlealpha = {"r_particlealpha","0.2", true};
cvar_t	sw_stipplealpha = {"sw_stipplealpha","0", true}; // must be present in GLMakaqu because it's saved in the config file
cvar_t	r_sprite_addblend = {"r_sprite_addblend","1", true}; //qbism was 0

cvar_t		scr_left	= {"scr_left",	"1", true};
cvar_t		scr_right	= {"scr_right",	"1", true};
cvar_t		scr_top		= {"scr_top",	"1", true};
cvar_t		scr_bottom	= {"scr_bottom","1", true};


float	r_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv,
        stvert_t *pstverts);
void R_AliasSetUpTransform (int trivial_accept);
void R_AliasTransformVector (vec3_t in, vec3_t out);
void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av,
                                trivertx_t *pverts, stvert_t *pstverts);
void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av);
// Manoel Kasimier - model interpolation - begin
void R_AliasSetUpBlendedTransform (int trivial_accept);
void R_AliasTransformAndProjectFinalBlendedVerts (finalvert_t *fv,
        stvert_t *pstverts);
void R_AliasTransformFinalBlendedVert (finalvert_t *fv, auxvert_t *av,
                                       trivertx_t *pverts1, trivertx_t *pverts2, stvert_t *pstverts);

void VectorInterpolate (vec3_t v1, vec_t frac, vec3_t v2, vec3_t v)
{
    if (frac < (1.0/255.0))
    {
        VectorCopy (v1, v);
        return;
    }
    else if (frac > (1-(1.0/255.0)))
    {
        VectorCopy (v2, v);
        return;
    }

    v[0] = v1[0] + frac * (v2[0] - v1[0]);
    v[1] = v1[1] + frac * (v2[1] - v1[1]);
    v[2] = v1[2] + frac * (v2[2] - v1[2]);
}

void AngleInterpolate (vec3_t v1, vec_t frac, vec3_t v2, vec3_t v)
{
    vec3_t	d;
    int i;

    if (frac < (1.0/255.0))
    {
        VectorCopy (v1, v);
        return;
    }
    else if (frac > (1-(1.0/255.0)))
    {
        VectorCopy (v2, v);
        return;
    }

    for (i = 0; i < 3; i++)
    {
        d[i] = v2[i] - v1[i];

        if (d[i] > 180)
            d[i] -= 360;
        else if (d[i] < -180)
            d[i] += 360;
    }

    v[0] = v1[0] + frac * d[0];
    v[1] = v1[1] + frac * d[1];
    v[2] = v1[2] + frac * d[2];
}
// Manoel Kasimier - model interpolation - end


/*
================
R_AliasCheckBBox
================
*/
qboolean R_AliasCheckBBox (void)
{
    int					i, flags, numv;
    aliashdr_t			*pahdr;
    float				zi, basepts[8][3], v0, v1, frac;
    finalvert_t			*pv0, *pv1, viewpts[16];
    auxvert_t			*pa0, *pa1, viewaux[16];
    maliasframedesc_t	*pframedesc;
    qboolean			zclipped, zfullyclipped;
    unsigned			anyclip, allclip;
    int					minz;

// expand, rotate, and translate points into worldspace

    currententity->trivial_accept = 0;
    pmodel = currententity->model;
    pahdr = Mod_Extradata (pmodel);
    pmdl = (mdl_t *)((byte *)pahdr + pahdr->model);
    // Manoel Kasimier - model interpolation - begin
    if (r_interpolation.value)
    {
        if (currententity != &cl.viewent && currententity != &cl_entities[cl.viewentity])
            R_AliasSetUpBlendedTransform (0);
        else
            R_AliasSetUpTransform (0);
        // x worldspace coordinates
        basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] =	(float)min(bboxmin1->v[0], bboxmin2->v[0]);
        basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] =	(float)max(bboxmax1->v[0], bboxmax2->v[0]);

        // y worldspace coordinates
        basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] =	(float)min(bboxmin1->v[1], bboxmin2->v[1]);
        basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] =	(float)max(bboxmax1->v[1], bboxmax2->v[1]);

        // z worldspace coordinates
        basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] =	(float)min(bboxmin1->v[2], bboxmin2->v[2]);
        basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] = (float)max(bboxmax1->v[2], bboxmax2->v[2]);
    }
    else
    {
        // Manoel Kasimier - model interpolation - end
        R_AliasSetUpTransform (0);

// construct the base bounding box for this frame
        /*	// Manoel Kasimier - model interpolation - removed - begin
        	frame = currententity->frame;
        // TODO: don't repeat this check when drawing?
        	if ((frame >= pmdl->numframes) || (frame < 0))
        	{
        		Con_DPrintf ("R_AliasCheckBBox: No such frame %d %s\n", frame, // edited
        				pmodel->name);
        		frame = 0;
        	}
        */	// Manoel Kasimier - model interpolation - removed - end
        pframedesc = &pahdr->frames[currententity->frame]; // Manoel Kasimier - model interpolation - edited

// x worldspace coordinates
        basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] =
                                            (float)pframedesc->bboxmin.v[0];
        basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] =
                                            (float)pframedesc->bboxmax.v[0];

// y worldspace coordinates
        basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] =
                                            (float)pframedesc->bboxmin.v[1];
        basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] =
                                            (float)pframedesc->bboxmax.v[1];

// z worldspace coordinates
        basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] =
                                            (float)pframedesc->bboxmin.v[2];
        basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] =
                                            (float)pframedesc->bboxmax.v[2];
    } // Manoel Kasimier - model interpolation

    zclipped = false;
    zfullyclipped = true;

    minz = 9999;
    for (i=0; i<8 ; i++)
    {
        R_AliasTransformVector  (&basepts[i][0], &viewaux[i].fv[0]);

        if (viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE)
        {
            // we must clip points that are closer than the near clip plane
            viewpts[i].flags = ALIAS_Z_CLIP;
            zclipped = true;
        }
        else
        {
            if (viewaux[i].fv[2] < minz)
                minz = viewaux[i].fv[2];
            viewpts[i].flags = 0;
            zfullyclipped = false;
        }
    }


    if (zfullyclipped)
    {
        return false;	// everything was near-z-clipped
    }

    numv = 8;

    if (zclipped)
    {
        // organize points by edges, use edges to get new points (possible trivial
        // reject)
        for (i=0 ; i<12 ; i++)
        {
            // edge endpoints
            pv0 = &viewpts[aedges[i].index0];
            pv1 = &viewpts[aedges[i].index1];
            pa0 = &viewaux[aedges[i].index0];
            pa1 = &viewaux[aedges[i].index1];

            // if one end is clipped and the other isn't, make a new point
            if (pv0->flags ^ pv1->flags)
            {
                frac = (ALIAS_Z_CLIP_PLANE - pa0->fv[2]) /
                       (pa1->fv[2] - pa0->fv[2]);
                viewaux[numv].fv[0] = pa0->fv[0] +
                                      (pa1->fv[0] - pa0->fv[0]) * frac;
                viewaux[numv].fv[1] = pa0->fv[1] +
                                      (pa1->fv[1] - pa0->fv[1]) * frac;
                viewaux[numv].fv[2] = ALIAS_Z_CLIP_PLANE;
                viewpts[numv].flags = 0;
                numv++;
            }
        }
    }

// project the vertices that remain after clipping
    anyclip = 0;
    allclip = ALIAS_XY_CLIP_MASK;

// TODO: probably should do this loop in ASM, especially if we use floats
    for (i=0 ; i<numv ; i++)
    {
        // we don't need to bother with vertices that were z-clipped
        if (viewpts[i].flags & ALIAS_Z_CLIP)
            continue;

        zi = 1.0 / viewaux[i].fv[2];

        // FIXME: do with chop mode in ASM, or convert to float
        v0 = (viewaux[i].fv[0] * xscale * zi) + xcenter;
        v1 = (viewaux[i].fv[1] * yscale * zi) + ycenter;

        flags = 0;

        if (v0 < r_refdef.fvrectx)
            flags |= ALIAS_LEFT_CLIP;
        if (v1 < r_refdef.fvrecty)
            flags |= ALIAS_TOP_CLIP;
        if (v0 > r_refdef.fvrectright)
            flags |= ALIAS_RIGHT_CLIP;
        if (v1 > r_refdef.fvrectbottom)
            flags |= ALIAS_BOTTOM_CLIP;

        anyclip |= flags;
        allclip &= flags;
    }

    if (allclip)
        return false;	// trivial reject off one side

    return true;
}


/*
================
R_AliasTransformVector
================
*/
void R_AliasTransformVector (vec3_t in, vec3_t out)
{
    out[0] = DotProduct(in, aliastransform[0]) + aliastransform[0][3];
    out[1] = DotProduct(in, aliastransform[1]) + aliastransform[1][3];
    out[2] = DotProduct(in, aliastransform[2]) + aliastransform[2][3];
}


/*
================
R_AliasPreparePoints

General clipped case
================
*/
void R_AliasPreparePoints (void)
{
    int			i;
    stvert_t	*pstverts;
    finalvert_t	*fv;
    auxvert_t	*av;
    mtriangle_t	*ptri;
    finalvert_t	*pfv[3];
    int			r_drawoutlines_old = r_drawoutlines; // Manoel Kasimier - EF_CELSHADING
    r_drawoutlines = false; // Manoel Kasimier - EF_CELSHADING

    pstverts = (stvert_t *)((byte *)paliashdr + paliashdr->stverts);
    r_anumverts = pmdl->numverts;
    fv = pfinalverts;
    av = pauxverts;

    for (i=0 ; i<r_anumverts ; i++, fv++, av++, r_apverts++, r_apverts1++, r_apverts2++, pstverts++) // Manoel Kasimier - model interpolation - edited
    {
        // Manoel Kasimier - model interpolation - begin
        if (r_interpolation.value)
            R_AliasTransformFinalBlendedVert (fv, av, r_apverts1, r_apverts2, pstverts);
        else
            // Manoel Kasimier - model interpolation - end
            R_AliasTransformFinalVert (fv, av, r_apverts, pstverts);
        if (av->fv[2] < ALIAS_Z_CLIP_PLANE)
            fv->flags |= ALIAS_Z_CLIP;
        else
        {
            R_AliasProjectFinalVert (fv, av);

            if (fv->v[0] < r_refdef.aliasvrect.x)
                fv->flags |= ALIAS_LEFT_CLIP;
            if (fv->v[1] < r_refdef.aliasvrect.y)
                fv->flags |= ALIAS_TOP_CLIP;
            if (fv->v[0] > r_refdef.aliasvrectright)
                fv->flags |= ALIAS_RIGHT_CLIP;
            if (fv->v[1] > r_refdef.aliasvrectbottom)
                fv->flags |= ALIAS_BOTTOM_CLIP;
        }
    }

//
// clip and draw all triangles
//
    r_affinetridesc.numtriangles = 1;

    ptri = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
    for (i=0 ; i<pmdl->numtris ; i++, ptri++)
    {
        celtri_drawn = false; // Manoel Kasimier - EF_CELSHADING
        pfv[0] = &pfinalverts[ptri->vertindex[0]];
        pfv[1] = &pfinalverts[ptri->vertindex[1]];
        pfv[2] = &pfinalverts[ptri->vertindex[2]];

        if ( pfv[0]->flags & pfv[1]->flags & pfv[2]->flags & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP) )
            continue;		// completely clipped

        if ( ! ( (pfv[0]->flags | pfv[1]->flags | pfv[2]->flags) &
                 (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP) ) )
        {
            // totally unclipped
            r_affinetridesc.pfinalverts = pfinalverts;
            r_affinetridesc.ptriangles = ptri;
            D_PolysetDraw_C ();
            // Manoel Kasimier - end
        }
        else
        {
            // partially clipped
            R_AliasClipTriangle (ptri);
        }
        ptri->drawn = celtri_drawn; // Manoel Kasimier - EF_CELSHADING
    }

    // Manoel Kasimier - EF_CELSHADING - begin
    if (!((int)developer.value & 32)) // for betatesting
        if (r_drawoutlines_old)
        {
            mtriangle_t	mtri; // Manoel Kasimier
            ptri = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
            r_drawoutlines = r_drawoutlines_old;
            for (i=0 ; i<pmdl->numtris ; i++, ptri++)
            {
                // Manoel Kasimier - begin
                if (ptri->drawn) // already drawn, it's probably front-faced
                    continue;
                mtri.facesfront = ptri->facesfront;
                mtri.vertindex[0] = ptri->vertindex[2];
                mtri.vertindex[1] = ptri->vertindex[1];
                mtri.vertindex[2] = ptri->vertindex[0];
                // Manoel Kasimier - end
                pfv[0] = &pfinalverts[mtri.vertindex[0]];
                pfv[1] = &pfinalverts[mtri.vertindex[1]];
                pfv[2] = &pfinalverts[mtri.vertindex[2]];

                if ( pfv[0]->flags & pfv[1]->flags & pfv[2]->flags & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP) )
                    continue;		// completely clipped

                if ( ! ( (pfv[0]->flags | pfv[1]->flags | pfv[2]->flags) &
                         (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP) ) )
                {
                    // totally unclipped
                    r_affinetridesc.pfinalverts = pfinalverts;
                    r_affinetridesc.ptriangles = &mtri;
                    D_PolysetDraw_C (); // Manoel Kasimier
                }
                else
                {
                    // partially clipped
                    R_AliasClipTriangle (&mtri);
                }
            }
        }
    // Manoel Kasimier - EF_CELSHADING - end
}


/*
================
R_AliasSetUpTransform
================
*/
void R_AliasSetUpTransform (int trivial_accept)
{
    int				i;
    float			rotationmatrix[3][4], t2matrix[3][4];
    static float	tmatrix[3][4];
    static float	viewmatrix[3][4];
    vec3_t			angles;

// TODO: should really be stored with the entity instead of being reconstructed
// TODO: should use a look-up table
// TODO: could cache lazily, stored in the entity

    if (!r_interpolation.value || (currententity == &cl.viewent) || currententity == &cl_entities[cl.viewentity]) // Manoel Kasimier - model interpolation
    {
        // Manoel Kasimier - model interpolation
        angles[ROLL] = currententity->angles[ROLL];
        angles[PITCH] = -currententity->angles[PITCH];
        angles[YAW] = currententity->angles[YAW];
        AngleVectors (angles, alias_forward, alias_right, alias_up);
    } // Manoel Kasimier - model interpolation

    // Manoel Kasimier - QC Scale - begin

    tmatrix[0][0] = pmdl->scale[0] * currententity->scale2 * currententity->scalev[0];
    tmatrix[1][1] = pmdl->scale[1] * currententity->scale2 * currententity->scalev[1];
    tmatrix[2][2] = pmdl->scale[2] * currententity->scale2 * currententity->scalev[2];

    tmatrix[0][3] = pmdl->scale_origin[0] * currententity->scale2 * currententity->scalev[0];
    tmatrix[1][3] = pmdl->scale_origin[1] * currententity->scale2 * currententity->scalev[1];
    tmatrix[2][3] = pmdl->scale_origin[2] * currententity->scale2 * currententity->scalev[2];

    // Manoel Kasimier - QC Scale - end

// TODO: can do this with simple matrix rearrangement

    for (i=0 ; i<3 ; i++)
    {
        t2matrix[i][0] = alias_forward[i];
        t2matrix[i][1] = -alias_right[i];
        t2matrix[i][2] = alias_up[i];
    }

    t2matrix[0][3] = -modelorg[0];
    t2matrix[1][3] = -modelorg[1];
    t2matrix[2][3] = -modelorg[2];

// FIXME: can do more efficiently than full concatenation
    R_ConcatTransforms (t2matrix, tmatrix, rotationmatrix);

// TODO: should be global, set when vright, etc., set
    VectorCopy (vright, viewmatrix[0]);
    VectorCopy (vup, viewmatrix[1]);
    VectorInverse (viewmatrix[1]);
    VectorCopy (vpn, viewmatrix[2]);

//	viewmatrix[0][3] = 0;
//	viewmatrix[1][3] = 0;
//	viewmatrix[2][3] = 0;

    R_ConcatTransforms (viewmatrix, rotationmatrix, aliastransform);

// do the scaling up of x and y to screen coordinates as part of the transform
// for the unclipped case (it would mess up clipping in the clipped case).
// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
// correspondingly so the projected x and y come out right
// FIXME: make this work for clipped case too?
    if (trivial_accept)
    {
        for (i=0 ; i<4 ; i++)
        {
            aliastransform[0][i] *= aliasxscale *
                                    (1.0 / ((float)0x8000 * 0x10000));
            aliastransform[1][i] *= aliasyscale *
                                    (1.0 / ((float)0x8000 * 0x10000));
            aliastransform[2][i] *= 1.0 / ((float)0x8000 * 0x10000);

        }
    }
}

// Manoel Kasimier - model interpolation - begin
/*
================
R_AliasSetUpBlendedTransform
================
*/
void R_AliasSetUpBlendedTransform (int trivial_accept)
{
    float			timepassed;
    float			lerp;
    vec3_t			t;
    // Manoel Kasimier - begin
    int				i;
    static float	viewmatrix[3][4];
    float rotationmatrix[3][4];
    // Manoel Kasimier - end

// TODO: should really be stored with the entity instead of being reconstructed
// TODO: should use a look-up table
// TODO: could cache lazily, stored in the entity

    // orientation interpolation (Euler angles, yuck!)
    timepassed = (float)cl.time - currententity->rotate_start_time; // edited
    if (currententity->rotate_start_time == 0 || timepassed > 1)
    {
        currententity->rotate_start_time = (float)cl.time; // edited
        VectorCopy (currententity->angles, currententity->angles1);
        VectorCopy (currententity->angles, currententity->angles2);
    }
    if (!VectorCompare (currententity->angles, currententity->angles2))
    {
        currententity->rotate_start_time = (float)cl.time; // edited
        VectorCopy (currententity->angles2, currententity->angles1);
        VectorCopy (currententity->angles, currententity->angles2);
        lerp = 0;
    }
    else
    {
        lerp = timepassed * 10.0f; // edited
        if (lerp > 1) lerp = 1; // edited
    }

    AngleInterpolate (currententity->angles1, lerp, currententity->angles2, t);
    t[PITCH] = -t[PITCH];
    AngleVectors (t, alias_forward, alias_right, alias_up);

    // positional interpolation
    timepassed = (float)cl.time - currententity->translate_start_time; // edited

    if (currententity->translate_start_time == 0 || timepassed > 1)
    {
        currententity->translate_start_time = (float)cl.time; // edited
        VectorCopy (currententity->origin, currententity->origin1);
        VectorCopy (currententity->origin, currententity->origin2);
    }
    if (!VectorCompare (currententity->origin, currententity->origin2))
    {
        currententity->translate_start_time = (float)cl.time; // edited
        VectorCopy (currententity->origin2, currententity->origin1);
        VectorCopy (currententity->origin, currententity->origin2);
        lerp = 0;
    }
    else
    {
        lerp = timepassed * 10.0f; // edited
        if (lerp > 1) lerp = 1; // edited
    }

    VectorInterpolate (currententity->origin1, lerp, currententity->origin2, t);
    VectorSubtract (r_origin, t, modelorg);

    // Manoel Kasimier - begin
    for (i = 0; i < 3; i++)
    {
        rotationmatrix[i][0] = alias_forward[i] * pmdl->scale[0];
        rotationmatrix[i][1] = -alias_right[i] * pmdl->scale[1];
        rotationmatrix[i][2] = alias_up[i] * pmdl->scale[2];
        rotationmatrix[i][3] = alias_forward[i] * pmdl->scale_origin[0] -
                               alias_right[i] * pmdl->scale_origin[1] +
                               alias_up[i] * pmdl->scale_origin[2] - modelorg[i];
    }

    R_ConcatTransforms (viewmatrix, rotationmatrix, aliastransform);
    R_AliasSetUpTransform (trivial_accept);
    // Manoel Kasimier - end
}
// Manoel Kasimier - model interpolation - end

/*
================
R_AliasTransformFinalVert
================
*/
void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av,
                                trivertx_t *pverts, stvert_t *pstverts)
{
    int		temp;
    float	lightcos, *plightnormal;

    av->fv[0] = DotProduct(pverts->v, aliastransform[0]) +
                aliastransform[0][3];
    av->fv[1] = DotProduct(pverts->v, aliastransform[1]) +
                aliastransform[1][3];
    av->fv[2] = DotProduct(pverts->v, aliastransform[2]) +
                aliastransform[2][3];

    fv->v[2] = pstverts->s;
    fv->v[3] = pstverts->t;

    fv->flags = pstverts->onseam;

// lighting
    plightnormal = r_avertexnormals[pverts->lightnormalindex];
    lightcos = DotProduct (plightnormal, r_plightvec);
    temp = r_ambientlight;

    if (lightcos < 0)
    {
        temp += (int)(r_shadelight * lightcos);

        // clamp; because we limited the minimum ambient and shading light, we
        // don't have to clamp low light, just bright
        if (temp < 0)
            temp = 0;
    }

    fv->v[4] = temp;
}




/*
================
R_AliasTransformAndProjectFinalVerts
================
*/
void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, stvert_t *pstverts)
{
    int			i, temp;
    float		lightcos, *plightnormal, zi;
    trivertx_t	*pverts;

    pverts = r_apverts;

    for (i=0 ; i<r_anumverts ; i++, fv++, pverts++, pstverts++)
    {
        // transform and project
        zi = 1.0 / (DotProduct(pverts->v, aliastransform[2]) +
                    aliastransform[2][3]);

        // x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
        // scaled up by 1/2**31, and the scaling cancels out for x and y in the
        // projection
        fv->v[5] = zi;

        fv->v[0] = ((DotProduct(pverts->v, aliastransform[0]) +
                     aliastransform[0][3]) * zi) + aliasxcenter;
        fv->v[1] = ((DotProduct(pverts->v, aliastransform[1]) +
                     aliastransform[1][3]) * zi) + aliasycenter;

        fv->v[2] = pstverts->s;
        fv->v[3] = pstverts->t;
        fv->flags = pstverts->onseam;

        // lighting
        plightnormal = r_avertexnormals[pverts->lightnormalindex];
        lightcos = DotProduct (plightnormal, r_plightvec);
        temp = r_ambientlight;

        if (lightcos < 0)
        {
            temp += (int)(r_shadelight * lightcos);

            // clamp; because we limited the minimum ambient and shading light, we
            // don't have to clamp low light, just bright
            if (temp < 0)
                temp = 0;
        }

        fv->v[4] = temp;
    }
}



// Manoel Kasimier - model interpolation - begin
/*
================
R_AliasTransformFinalBlendedVert
================
*/
void R_AliasTransformFinalBlendedVert (finalvert_t *fv, auxvert_t *av,
                                       trivertx_t *pverts1, trivertx_t *pverts2, stvert_t *pstverts)
{
    vec3_t	v, v1;
    float	lightcos1, lightcos2; // Manoel Kasimier
    int		lightcos; // Manoel Kasimier

    v[0] = (float)pverts2->v[0];
    v[1] = (float)pverts2->v[1];
    v[2] = (float)pverts2->v[2];
    v1[0] = (float)pverts1->v[0];
    v1[1] = (float)pverts1->v[1];
    v1[2] = (float)pverts1->v[2];
    VectorInterpolate (v1, blend, v, v);

    R_AliasTransformVector (v, av->fv);

    fv->v[2] = pstverts->s;
    fv->v[3] = pstverts->t;

    fv->flags = pstverts->onseam;

// lighting
    // Manoel Kasimier - begin
    lightcos1 = DotProduct (r_avertexnormals[pverts1->lightnormalindex], r_plightvec);
    lightcos2 = DotProduct (r_avertexnormals[pverts2->lightnormalindex], r_plightvec);
    lightcos = (int)((r_shadelight*lightcos1) + blend * ((r_shadelight*lightcos2) - (r_shadelight*lightcos1)));

    if (lightcos < 0)
    {
        // clamp; because we limited the minimum ambient and shading light, we
        // don't have to clamp low light, just bright
        fv->v[4] = r_ambientlight + lightcos;
        if (fv->v[4] < 0)
            fv->v[4] = 0;
    }
    else
        fv->v[4] = r_ambientlight;
    // Manoel Kasimier - end
}

/*
================
R_AliasTransformAndProjectFinalBlendedVerts
================
*/
void R_AliasTransformAndProjectFinalBlendedVerts (finalvert_t *fv, stvert_t *pstverts)
{
    int			i;
    float		zi;
    trivertx_t	*pverts1 = r_apverts1,
                 *pverts2 = r_apverts2;
    vec3_t		v, v1;
    float		lightcos1, lightcos2; // Manoel Kasimier
    int			lightcos; // Manoel Kasimier

    for (i=0 ; i<r_anumverts ; i++, fv++, pverts1++, pverts2++, pstverts++)
    {
        v[0] = (float)pverts2->v[0];
        v[1] = (float)pverts2->v[1];
        v[2] = (float)pverts2->v[2];
        v1[0] = (float)pverts1->v[0];
        v1[1] = (float)pverts1->v[1];
        v1[2] = (float)pverts1->v[2];
        VectorInterpolate (v1, blend, v, v);

        // transform and project
        zi = 1.0f / (DotProduct(v, aliastransform[2]) + // edited
                     aliastransform[2][3]);

        // x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
        // scaled up by 1/2**31, and the scaling cancels out for x and y in the
        // projection
        fv->v[5] = (int)zi; // compiler warning fix

        fv->v[0] = (int)(((DotProduct(v, aliastransform[0]) + // compiler warning fix
                           aliastransform[0][3]) * zi) + aliasxcenter); // compiler warning fix
        fv->v[1] = (int)(((DotProduct(v, aliastransform[1]) + // compiler warning fix
                           aliastransform[1][3]) * zi) + aliasycenter); // compiler warning fix

        fv->v[2] = pstverts->s;
        fv->v[3] = pstverts->t;
        fv->flags = pstverts->onseam;

        // lighting
        // Manoel Kasimier - begin
        lightcos1 = DotProduct (r_avertexnormals[pverts1->lightnormalindex], r_plightvec);
        lightcos2 = DotProduct (r_avertexnormals[pverts2->lightnormalindex], r_plightvec);
        lightcos = (int)((r_shadelight*lightcos1) + blend * ((r_shadelight*lightcos2) - (r_shadelight*lightcos1)));

        if (lightcos < 0)
        {
            // clamp; because we limited the minimum ambient and shading light, we
            // don't have to clamp low light, just bright
            fv->v[4] = r_ambientlight + lightcos;
            if (fv->v[4] < 0)
                fv->v[4] = 0;
            continue;
        }
        fv->v[4] = r_ambientlight;
        // Manoel Kasimier - end
    }
}
// Manoel Kasimier - model interpolation - end


/*
================
R_AliasProjectFinalVert
================
*/
void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av)
{
    float	zi;

// project points
    zi = 1.0 / av->fv[2];

    fv->v[5] = zi * ziscale;

    fv->v[0] = (av->fv[0] * aliasxscale * zi) + aliasxcenter;
    fv->v[1] = (av->fv[1] * aliasyscale * zi) + aliasycenter;
}


/*
================
R_AliasPrepareUnclippedPoints
================
*/
void R_AliasPrepareUnclippedPoints (void)
{
    stvert_t	*pstverts;
    finalvert_t	*fv;

    pstverts = (stvert_t *)((byte *)paliashdr + paliashdr->stverts);
    r_anumverts = pmdl->numverts;
// FIXME: just use pfinalverts directly?
    fv = pfinalverts;

    // Manoel Kasimier - model interpolation - begin
    if (r_interpolation.value)
        R_AliasTransformAndProjectFinalBlendedVerts (fv, pstverts);
    else
        // Manoel Kasimier - model interpolation - end
        R_AliasTransformAndProjectFinalVerts (fv, pstverts);

    if (r_affinetridesc.drawtype)
        D_PolysetDrawFinalVerts (fv, r_anumverts);

    r_affinetridesc.pfinalverts = pfinalverts;
    r_affinetridesc.ptriangles = (mtriangle_t *)
                                 ((byte *)paliashdr + paliashdr->triangles);
    r_affinetridesc.numtriangles = pmdl->numtris;

    // Manoel Kasimier - begin
    D_PolysetDraw_C ();
    // Manoel Kasimier - end
}

/*
===============
R_AliasSetupSkin
===============
*/
void R_AliasSetupSkin (void)
{
    int					skinnum;
    int					i, numskins;
    maliasskingroup_t	*paliasskingroup;
    float				*pskinintervals, fullskininterval;
    float				skintargettime, skintime;

    skinnum = currententity->skinnum;
    if ((skinnum >= pmdl->numskins) || (skinnum < 0))
    {
        Con_DPrintf ("R_AliasSetupSkin: no such skin # %d\n", skinnum);
        skinnum = 0;
    }

    pskindesc = ((maliasskindesc_t *)
                 ((byte *)paliashdr + paliashdr->skindesc)) + skinnum;
    a_skinwidth = pmdl->skinwidth;

    if (pskindesc->type == ALIAS_SKIN_GROUP)
    {
        paliasskingroup = (maliasskingroup_t *)((byte *)paliashdr +
                                                pskindesc->skin);
        pskinintervals = (float *)
                         ((byte *)paliashdr + paliasskingroup->intervals);
        numskins = paliasskingroup->numskins;
        fullskininterval = pskinintervals[numskins-1];

        skintime = cl.time + currententity->syncbase;

        // when loading in Mod_LoadAliasSkinGroup, we guaranteed all interval
        // values are positive, so we don't have to worry about division by 0
        skintargettime = skintime -
                         ((int)(skintime / fullskininterval)) * fullskininterval;

        for (i=0 ; i<(numskins-1) ; i++)
        {
            if (pskinintervals[i] > skintargettime)
                break;
        }

        pskindesc = &paliasskingroup->skindescs[i];
    }

    r_affinetridesc.pskindesc = pskindesc;
    r_affinetridesc.pskin = (void *)((byte *)paliashdr + pskindesc->skin);
    r_affinetridesc.skinwidth = a_skinwidth;
    r_affinetridesc.seamfixupX16 =  (a_skinwidth >> 1) << 16;
    r_affinetridesc.skinheight = pmdl->skinheight;
}

/*
================
R_AliasSetupLighting
================
*/
void R_AliasSetupLighting (/*alight_t *plighting*/) // Manoel Kasimier - edited
{
    // Manoel Kasimier - Model interpolation - begin
    float		lightvec[3] = {-1, 0, 0};
    vec3_t		dist;
    float		add;
    int			lnum;
    dlight_t	*dl;
    vec3_t		t;
    int			shadelight;

    VectorCopy(currententity->origin, t);
    r_ambientlight = R_LightPoint (t);
    if (currententity == &cl.viewent)
        if (r_ambientlight < 24)
            r_ambientlight = 24;		// always give some light on gun
    shadelight = r_ambientlight;

    // add dynamic lights
    for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
    {
        dl = &cl_dlights[lnum];
        if (!dl->radius) // viewmodel only
            continue; // viewmodel only
        if (dl->die < cl.time)
            continue;
        if (dl->dark)
            continue;

        VectorSubtract (t, dl->origin, dist);
        add = dl->radius - Length(dist);
        if (add > 0)
            r_ambientlight += (int)add;
    }
    // clamp lighting so it doesn't overbright as much, and
    // guarantee that no vertex will ever be lit below LIGHT_MIN,
    // so we don't have to clamp off the bottom
    if (r_ambientlight > 128)
        r_ambientlight = 128;
    if (r_ambientlight + shadelight > 192)
        shadelight = 192 - r_ambientlight;
    else if (r_ambientlight < LIGHT_MIN)
        r_ambientlight = LIGHT_MIN;

    r_ambientlight = (255 - r_ambientlight) << VID_CBITS;
//	if (r_ambientlight < LIGHT_MIN)
//		r_ambientlight = LIGHT_MIN;

    if (shadelight < 0)
        r_shadelight = 0;
    else
        r_shadelight = (float)shadelight * VID_GRADES;

    // rotate the lighting vector into the model's frame of reference
    r_plightvec[0] = DotProduct (lightvec, alias_forward);
    r_plightvec[1] = -DotProduct (lightvec, alias_right);
    r_plightvec[2] = DotProduct (lightvec, alias_up);

    // Manoel Kasimier - Model interpolation - end
}
// Manoel Kasimier - begin
void R_AliasSetupLighting_new ()
{
    float		lightvec[3];
    vec3_t		dist;
    float		add;
    int			lnum;
    dlight_t	*dl;
    vec3_t		t;

    lightvec[0] = r_light_vec_x.value;
    lightvec[1] = r_light_vec_y.value;
    lightvec[2] = r_light_vec_z.value;

    VectorCopy(currententity->origin, t);
    r_shadelight = R_LightPoint (t);
    if (currententity == &cl.viewent)
        if (r_shadelight < 24)
            r_shadelight = 24;		// always give some light on gun

    // add dynamic lights
    for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
    {
        dl = &cl_dlights[lnum];
        if (!dl->radius)
            continue;
        if (dl->die < cl.time)
            continue;

        VectorSubtract (t, dl->origin, dist);
        add = dl->radius - Length(dist);
        // dl->radius: 200 normal, 350 explosion
        if (add > 0)
        {
            float scale = 1.0-(Length(dist)/dl->radius);
            scale *= scale; // scale ^ 2

            r_shadelight += add*(scale);

            VectorScale (dist, scale, dist);
            // limitar o lightvec
            /*
            	{
            	float teste = Length(lightvec);
            	if (teste > scale)
            		VectorScale (lightvec, scale/teste, lightvec);
            	}
            //*/

            if (dl->dark)
            {
                VectorSubtract(lightvec, dist, lightvec);
            }
            else
                VectorAdd(lightvec, dist, lightvec);
        }
    }
    r_ambientlight = (255 - LIGHT_MIN) << VID_CBITS;

//qbism- doesn't happen	if (r_shadelight < 0)
//		r_shadelight = 0;
//	else
    r_shadelight = (float)(r_shadelight * VID_GRADES);

    // rotate the lighting vector into the model's frame of reference
    r_plightvec[0] = DotProduct (lightvec, alias_forward);
    r_plightvec[1] = -DotProduct (lightvec, alias_right);
    r_plightvec[2] = DotProduct (lightvec, alias_up);
}
void R_AliasSetupLighting_fullbright (void)
{
    r_ambientlight = 128;
    r_ambientlight = (255 - r_ambientlight) << VID_CBITS;
    r_shadelight = 0;
}
// Manoel Kasimier - end

/*
=================
R_AliasSetupFrame

set r_apverts
=================
*/
void R_AliasSetupFrame (void)
{
    int				frame;
    int				pose; // Manoel Kasimier - model interpolation
    int				i, numframes;
    maliasgroup_t	*paliasgroup;
    float			*pintervals, fullinterval, targettime, time;

    frame = currententity->frame;
    if ((frame >= pmdl->numframes) || (frame < 0))
    {
        Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
        frame = 0;
    }

    if (paliashdr->frames[frame].type == ALIAS_SINGLE)
    {
        paliasgroup = NULL; // Manoel Kasimier - model interpolation
        pose = frame; // Manoel Kasimier - model interpolation
        r_apverts = (trivertx_t *)
                    ((byte *)paliashdr + paliashdr->frames[frame].frame);
        //	return; // Manoel Kasimier - model interpolation - removed
    }
    else // Manoel Kasimier - model interpolation
    {
        // Manoel Kasimier - model interpolation

        paliasgroup = (maliasgroup_t *)
                      ((byte *)paliashdr + paliashdr->frames[frame].frame);
        pintervals = (float *)((byte *)paliashdr + paliasgroup->intervals);
        numframes = paliasgroup->numframes;
        fullinterval = pintervals[numframes-1];

        time = cl.time + currententity->syncbase;

//
// when loading in Mod_LoadAliasGroup, we guaranteed all interval values
// are positive, so we don't have to worry about division by 0
//
        targettime = time - ((int)(time / fullinterval)) * fullinterval;

        for (i=0 ; i<(numframes-1) ; i++)
        {
            if (pintervals[i] > targettime)
                break;
        }

        pose = i; // Manoel Kasimier - model interpolation

        r_apverts = (trivertx_t *)
                    ((byte *)paliashdr + paliasgroup->frames[i].frame);
        // Manoel Kasimier - model interpolation - begin
    }
    currententity->pose1 = currententity->pose2 = pose;
    currententity->framegroup1 = currententity->framegroup2 = paliasgroup; // Manoel Kasimier - edited
    currententity->frame_start_time = (float)cl.time;
    // Manoel Kasimier - model interpolation - end
}

// Manoel Kasimier - model interpolation - begin
/*
=================
R_AliasSetupBlendedFrame

set r_apverts1, r_apverts2 and blend
=================
*/
void R_AliasSetupBlendedFrame (void)
{
    entity_t		*e = currententity;
    int				frame = e->frame, pose;
    maliasgroup_t	*paliasgroup;
    float			interval; // Manoel Kasimier

    if ((frame >= pmdl->numframes) || (frame < 0))
    {
        Con_DPrintf ("R_AliasSetupBlendedFrame: no such frame %d %s\n", frame, pmodel->name); // edited
        frame = 0;
    }

    if (paliashdr->frames[frame].type == ALIAS_SINGLE)
    {
        paliasgroup = NULL;
        pose = frame;
        // Manoel Kasimier - QC frame_interval - begin
        if (e->frame_interval > 0)
            interval = e->frame_interval;
        else if (e->frame_interval < 0)
            interval = 0;
        else // if (!e->frame_interval)
            interval = 0.1f;
        // Manoel Kasimier - QC frame_interval - end
    }
    else
    {
        // Manoel Kasimier - begin
        /*
        o pintervals[] contém o intervalo de tempo acumulado até o frame indicado
        */
        float			*pintervals, fullinterval, targettime, time;
        int				numframes;
        paliasgroup = (maliasgroup_t *)	((byte *)paliashdr + paliashdr->frames[frame].frame);
        pintervals = (float *)((byte *)paliashdr + paliasgroup->intervals);
        numframes = paliasgroup->numframes;
        fullinterval = pintervals[numframes-1];
        time = (float)cl.time + currententity->syncbase;
        targettime = time - ((int)(time / fullinterval)) * fullinterval;
        for (pose=0 ; pose<(numframes-1) ; pose++)
        {
            if (pintervals[pose] > targettime)
                break;
        }
        interval = (pose > e->pose1) ? pintervals[pose] - pintervals[pose-1] : pintervals[pose];
        // Manoel Kasimier - end
    }

    if (e->lastmodel == e->model && interval)// && (e->framegroup2 == paliasgroup)) // Manoel Kasimier - edited
    {
        if ((pose != e->pose2) || (paliasgroup != e->framegroup2)) // Manoel Kasimier - edited
        {
            if (e->reset_frame_interpolation == true)
            {
                e->framegroup1 = paliasgroup; // Manoel Kasimier
                e->pose1 = pose;
                e->reset_frame_interpolation = false;
            }
            else
            {
                e->framegroup1 = e->framegroup2; // Manoel Kasimier
                e->pose1 = e->pose2;
            }
            e->framegroup2 = paliasgroup; // Manoel Kasimier
            e->pose2 = pose;

            e->frame_start_time = (float)cl.time;
            blend = 0;
        }
        else
            blend = ((float)cl.time - e->frame_start_time) / interval; // Manoel Kasimier - edited
    }
    else
    {
        e->pose1 = e->pose2 = pose;
        e->lastmodel = e->model;
        e->frame_start_time = (float)cl.time;
        e->framegroup1 = e->framegroup2 = paliasgroup; // Manoel Kasimier - edited
        blend = 0;
    }

    if (blend > 1.0f) // Manoel Kasimier - edited
        blend = 1.0f;

    // FIXME ?
    else if (blend < 0) // Manoel Kasimier - edited
    {
        Con_DPrintf ("R_AliasSetupBlendedFrame: %f blend\n", blend);
        blend = 0;
    }

    // Manoel Kasimier - begin
    if (e->framegroup1)
    {
        r_apverts1	= (trivertx_t *) ((byte *)paliashdr + e->framegroup1->frames[e->pose1].frame);
        bboxmin1	= &e->framegroup1->frames[e->pose1].bboxmin;
        bboxmax1	= &e->framegroup1->frames[e->pose1].bboxmax;
    }
    else
    {
        r_apverts1	= (trivertx_t *) ((byte *)paliashdr + paliashdr->frames[e->pose1].frame);
        bboxmin1	= &paliashdr->frames[e->pose1].bboxmin;
        bboxmax1	= &paliashdr->frames[e->pose1].bboxmax;
    }

    if (paliasgroup)
    {
        r_apverts2	= (trivertx_t *) ((byte *)paliashdr + paliasgroup->frames[e->pose2].frame);
        bboxmin2	= &paliasgroup->frames[e->pose2].bboxmin;
        bboxmax2	= &paliasgroup->frames[e->pose2].bboxmax;
    }
    else
    {
        r_apverts2	= (trivertx_t *) ((byte *)paliashdr + paliashdr->frames[e->pose2].frame);
        bboxmin2	= &paliashdr->frames[e->pose2].bboxmin;
        bboxmax2	= &paliashdr->frames[e->pose2].bboxmax;
    }
    // Manoel Kasimier - end
}
// Manoel Kasimier - model interpolation - end

/*
================
R_AliasDrawModel
================
*/
void R_AliasDrawModel (/* alight_t *plighting */) // Manoel Kasimier - edited
{
    finalvert_t		finalverts[MAXALIASVERTS +
                               ((CACHE_SIZE - 1) / sizeof(finalvert_t)) + 1];
    auxvert_t		auxverts[MAXALIASVERTS];

    r_amodels_drawn++;

// cache align
    pfinalverts = (finalvert_t *)
                  (((long)&finalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
    pauxverts = &auxverts[0];

    paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);
    pmdl = (mdl_t *)((byte *)paliashdr + paliashdr->model);

    // Manoel Kasimier - model interpolation - begin
    if (r_interpolation.value)
        R_AliasSetupBlendedFrame();
    else
        R_AliasSetupFrame ();

    // see if the bounding box lets us trivially reject, also sets trivial accept status
    if (!R_AliasCheckBBox ())
    {
        currententity->reset_frame_interpolation = true;
        return;
    }

    if (r_interpolation.value && currententity != &cl.viewent && currententity != &cl_entities[cl.viewentity])
        R_AliasSetUpBlendedTransform (currententity->trivial_accept);
    else
        R_AliasSetUpTransform (currententity->trivial_accept);
    // Manoel Kasimier - model interpolation - end
    R_AliasSetupSkin ();
    // Manoel Kasimier - begin
    if (currententity->effects & EF_FULLBRIGHT)
        R_AliasSetupLighting_fullbright();
    else if (r_light_style.value)
        R_AliasSetupLighting_new();
    else
        // Manoel Kasimier - end
        R_AliasSetupLighting (/* plighting */); // Manoel Kasimier - edited

    if (!currententity->colormap)
        Sys_Error ("R_AliasDrawModel: !currententity->colormap");

    r_affinetridesc.drawtype = (currententity->trivial_accept == 3) &&
                               r_recursiveaffinetriangles;

    if (r_affinetridesc.drawtype)
    {
        D_PolysetUpdateTables ();		// FIXME: precalc...
    }

    acolormap = currententity->colormap;

    if (currententity != &cl.viewent)
        ziscale = (float)0x8000 * (float)0x10000;
    else
        ziscale = (float)0x8000 * (float)0x10000 * 3.0;

    if (currententity->trivial_accept)
        R_AliasPrepareUnclippedPoints ();
    else
        R_AliasPreparePoints ();
}

