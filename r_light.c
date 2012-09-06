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
// r_light.c

#include "quakedef.h"
#include "r_local.h"


/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
	int			i,j,k;

//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int)(cl.ctime*10);  //DEMO_REWIND_HARD_ANIMS_TIME qbism - Baker change
	for (j=0 ; j<MAX_LIGHTSTYLES ; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}
		k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k*22;
		d_lightstylevalue[j] = k;
	}
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void R_MarkLights (dlight_t *light, int num, mnode_t *node)  //qbism- adapted from MH tute - increased dlights
{
   mplane_t   *splitplane;
   float      dist;
   msurface_t   *surf;
   int         i;
   vec3_t      origin_for_ent; //qbism- i3d: mankrip - dynamic lights on moving brush models fix

   if (node->contents < 0)
      return;

   splitplane = node->plane;
   VectorSubtract (light->origin, currententity->origin, origin_for_ent); //qbism- i3d: mankrip - dynamic lights on moving brush models fix
   dist = DotProduct (origin_for_ent, splitplane->normal) - splitplane->dist; //qbism- i3d mankrip - dynamic lights on moving brush models fix - edited

   if (dist > light->radius)
   {
      R_MarkLights (light, num, node->children[0]);
      return;
   }

   if (dist < -light->radius)
   {
      R_MarkLights (light, num, node->children[1]);
      return;
   }

   // mark the polygons
   surf = cl.worldmodel->surfaces + node->firstsurface;

   for (i = 0; i < node->numsurfaces; i++, surf++)
   {
      if (surf->dlightframe != r_framecount)
      {
         memset (surf->dlightbits, 0, sizeof (surf->dlightbits));
         surf->dlightframe = r_framecount;
      }

      surf->dlightbits[num >> 5] |= 1 << (num & 31);
   }

   R_MarkLights (light, num, node->children[0]);
   R_MarkLights (light, num, node->children[1]);
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (mnode_t *headnode)  //qbism- from MH tute - increased dlights
{
   int i;
   dlight_t *l = cl_dlights;

   //qbism - moved to r_main // currententity = &cl_entities[0]; // mankrip - dynamic lights on moving brush models fix
   for (i = 0; i < MAX_DLIGHTS; i++, l++)
   {
      if (l->die < cl.time || (l->radius <= 0))
         continue;

      R_MarkLights (l, i, headnode);
   }
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

byte *pointcolormap;

int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	int			r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	unsigned	scale;
	int			maps;

	if (node->contents < 0)
		return -1;		// didn't hit anything

// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;

	if ( (back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end);

	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;

// go down front side
	r = RecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something

	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing

// check for impact on this node

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		pointcolormap = surf->colorsamples;
		r = 0;
		if (lightmap)
		{

			lightmap += dt * ((surf->extents[0]>>4)+1) + ds;
			pointcolormap += dt * ((surf->extents[0]>>4)+1) + ds;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				r += *lightmap * scale;
				lightmap += ((surf->extents[0]>>4)+1) *
						((surf->extents[1]>>4)+1);
			}

			r >>= 8;
		}

		return r;
	}

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end);
}

int blendedlightpoint = 0; //qbism interpolate light point

int R_LightPoint (vec3_t p)
{
	vec3_t		end;
	int			r;

	if (!cl.worldmodel->lightdata)
		return 255;

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = RecursiveLightPoint (cl.worldmodel->nodes, p, end);

	if (r == -1)
		r = 0;

	if (r < r_refdef.ambientlight)
		r = r_refdef.ambientlight;

    blendedlightpoint = (blendedlightpoint*7 + r) /8; //qbism - quicky soften of bright/dark transitions

	return blendedlightpoint;
}

