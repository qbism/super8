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

//qb: replacement from mankrip inside3d post. r_aclip.c: clip routines for drawing Alias models directly to the screen

    #include "quakedef.h"
    #include "r_local.h"
    #include "d_local.h"

    static finalvert_t
       fv[2][8];
    //,   * clipfv // mankrip
    //   ;
    static auxvert_t
       av[8]
       ;
    // mankrip - begin
    #define HORIZONTAL 0 // for clipplane
    #define VERTICAL 1 // for clipplane
    static int
       * clipfvi0
    ,   * clipfvi1
    ,   clipplane // for clipscale
    ,   cliplimit // for clipscale
       ;
    static float
       clipscale
       ;
    static vec3_t
       clipv0
    ,   clipv1
       ;
    // mankrip - end



    /*
    ================
    R_Alias_clip_z

    pfv0 is the unclipped vertex, pfv1 is the z-clipped vertex
    ================
    */
    void R_Alias_clip_z (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out)
    {
       // mankrip - begin
       static auxvert_t
          avout
          ;
       if (pfv0->v[1] >= pfv1->v[1])
       {
          clipfvi0 = pfv0->v;
          clipfvi1 = pfv1->v;
          // copying these integer vectors also implicitely typecasts them
          VectorCopy (av[pfv0 - &fv[0][0]].fv, clipv0);
          VectorCopy (av[pfv1 - &fv[0][0]].fv, clipv1);
       }
       else
       {
          clipfvi0 = pfv1->v;
          clipfvi1 = pfv0->v;
          // copying these integer vectors also implicitely typecasts them
          VectorCopy (av[pfv1 - &fv[0][0]].fv, clipv0);
          VectorCopy (av[pfv0 - &fv[0][0]].fv, clipv1);
       }
       clipscale = (ALIAS_Z_CLIP_PLANE - clipv0[2]) / (clipv1[2] - clipv0[2]);

       avout.fv[0] = (int) (clipv0[0] + (clipv1[0] - clipv0[0]) * clipscale);
       avout.fv[1] = (int) (clipv0[1] + (clipv1[1] - clipv0[1]) * clipscale);
       avout.fv[2] = ALIAS_Z_CLIP_PLANE;

       out->v[2] =   clipfvi0[2] + (int) ( (float) (clipfvi1[2] - clipfvi0[2]) * clipscale);
       out->v[3] =   clipfvi0[3] + (int) ( (float) (clipfvi1[3] - clipfvi0[3]) * clipscale);
       out->v[4] =   clipfvi0[4] + (int) ( (float) (clipfvi1[4] - clipfvi0[4]) * clipscale);
       // mankrip - end

       R_AliasProjectFinalVert (out, &avout);
    }



    // mankrip - begin
    void R_Alias_clip_xy (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out)
    {
       // for left/right/top/bottom variations, only clipscale changes
       // r_refdef.vrect.x for left cliplimit, r_refdef.vrectright for right cliplimit
       // r_refdef.vrect.y for top cliplimit, r_refdef.vrectbottom for bottom cliplimit
       // clipplane should be either zero (horizontal) or 1 (vertical)

       if (pfv0->v[1] >= pfv1->v[1])
       {
          clipfvi0 = pfv0->v;
          clipfvi1 = pfv1->v;
       }
       else
       {
          clipfvi0 = pfv1->v;
          clipfvi1 = pfv0->v;
       }
       clipscale = (float) (cliplimit - clipfvi0[clipplane]) / (float) (clipfvi1[clipplane] - clipfvi0[clipplane]);
       out->v[0] = clipfvi0[0] + (int) ( (float) (clipfvi1[0] - clipfvi0[0]) * clipscale + 0.5f);
       out->v[1] = clipfvi0[1] + (int) ( (float) (clipfvi1[1] - clipfvi0[1]) * clipscale + 0.5f);
       out->v[2] = clipfvi0[2] + (int) ( (float) (clipfvi1[2] - clipfvi0[2]) * clipscale + 0.5f);
       out->v[3] = clipfvi0[3] + (int) ( (float) (clipfvi1[3] - clipfvi0[3]) * clipscale + 0.5f);
       out->v[4] = clipfvi0[4] + (int) ( (float) (clipfvi1[4] - clipfvi0[4]) * clipscale + 0.5f);
       out->v[5] = clipfvi0[5] + (int) ( (float) (clipfvi1[5] - clipfvi0[5]) * clipscale + 0.5f);
    }
    // mankrip - end



    int R_AliasClip (finalvert_t *in, finalvert_t *out, int flag, int count,
       void (*clip) (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out) )
    {
       static int
          i
       ,   j
       ,   k
       ,   flags
       ,   oldflags
          ;

       for (k = 0 , i = 0 , j = count - 1 ; i < count ; j = i, i++)
       {
          oldflags = in[j].flags & flag;
          flags    = in[i].flags & flag;

          if (flags && oldflags)
             continue;
          if (oldflags ^ flags)
          {
             clip (&in[j], &in[i], &out[k]);
             out[k].flags = 0;

             // check if it needs to be clipped again
             // mankrip - initial clipping is done right-to-left and bottom-to-top, so let's check the other way around now
             if (out[k].v[0] < r_refdef.vrect.x)      out[k].flags |= ALIAS_LEFT_CLIP;
             else
             if (out[k].v[0] > r_refdef.vrectright)   out[k].flags |= ALIAS_RIGHT_CLIP;
             if (out[k].v[1] < r_refdef.vrect.y)      out[k].flags |= ALIAS_TOP_CLIP;
             else
             if (out[k].v[1] > r_refdef.vrectbottom) out[k].flags |= ALIAS_BOTTOM_CLIP;

             k++;
          }
          if (!flags)
          {
             out[k] = in[i];
             k++;
          }
       }
       return k;
    }



    void R_AliasClipTriangle (mtriangle_t *ptri)
    {
       static int
          i
       ,   k
       ,   pingpong
          ;
       static unsigned
          clipflags
          ;
       static mtriangle_t
          mtri
          ;

       // copy vertexes and fix seam texture coordinates
       fv[0][0] = pfinalverts[ptri->vertindex[0]];
       fv[0][1] = pfinalverts[ptri->vertindex[1]];
       fv[0][2] = pfinalverts[ptri->vertindex[2]];
       // mankrip - begin
       if (!ptri->facesfront)
       {
          if (fv[0][0].flags & ALIAS_ONSEAM) fv[0][0].v[2] += r_affinetridesc.seamfixupX16;
          if (fv[0][1].flags & ALIAS_ONSEAM) fv[0][1].v[2] += r_affinetridesc.seamfixupX16;
          if (fv[0][2].flags & ALIAS_ONSEAM) fv[0][2].v[2] += r_affinetridesc.seamfixupX16;
       }
       // mankrip - end

       // clip
       clipflags = fv[0][0].flags | fv[0][1].flags | fv[0][2].flags;

       if (clipflags & ALIAS_Z_CLIP)
       {
          av[0] = pauxverts[ptri->vertindex[0]];
          av[1] = pauxverts[ptri->vertindex[1]];
          av[2] = pauxverts[ptri->vertindex[2]];

          k = R_AliasClip (fv[0], fv[1], ALIAS_Z_CLIP, 3, R_Alias_clip_z);
          if (k == 0)
             return;
          pingpong = 1;
          clipflags = fv[1][0].flags | fv[1][1].flags | fv[1][2].flags;
       }
       else
       {
          pingpong = 0;
          k = 3;
       }

       // mankrip - the viewmodel is always on the bottom of the screen, so let's clip the bottom first
       if (clipflags & ALIAS_BOTTOM_CLIP)
       {
          cliplimit = r_refdef.vrectbottom; // mankrip
          clipplane = VERTICAL; // mankrip
          k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1], ALIAS_BOTTOM_CLIP, k, R_Alias_clip_xy);
          if (k == 0)
             return;
          pingpong ^= 1;
       }

       // mankrip - weapons such as the axe are right-handed, so let's clip the right before the left
       if (clipflags & ALIAS_RIGHT_CLIP)
       {
          cliplimit = r_refdef.vrectright; // mankrip
          clipplane = HORIZONTAL; // mankrip
          k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1], ALIAS_RIGHT_CLIP, k, R_Alias_clip_xy);
          if (k == 0)
             return;
          pingpong ^= 1;
       }

       if (clipflags & ALIAS_TOP_CLIP)
       {
          cliplimit = r_refdef.vrect.y; // mankrip
          clipplane = VERTICAL; // mankrip
          k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1], ALIAS_TOP_CLIP, k, R_Alias_clip_xy);
          if (k == 0)
             return;
          pingpong ^= 1;
       }

       if (clipflags & ALIAS_LEFT_CLIP)
       {
          cliplimit = r_refdef.vrect.x; // mankrip
          clipplane = HORIZONTAL; // mankrip
          k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1], ALIAS_LEFT_CLIP, k, R_Alias_clip_xy);
          if (k == 0)
             return;
          pingpong ^= 1;
       }

       // mankrip - fix any rounding that went outside of the screen
       for (i = 0 ; i < k ; i++)
       {
          clipfvi0 = fv[pingpong][i].v;
          if (clipfvi0[0] > r_refdef.vrectright)
             clipfvi0[0] = r_refdef.vrectright;
          else
          if (clipfvi0[0] < r_refdef.vrect.x)
             clipfvi0[0] = r_refdef.vrect.x;

          if (clipfvi0[1] > r_refdef.vrectbottom)
             clipfvi0[1] = r_refdef.vrectbottom;
          else
          if (clipfvi0[1] < r_refdef.vrect.y)
             clipfvi0[1] = r_refdef.vrect.y;

          fv[pingpong][i].flags = 0;
       }

       // draw triangles
       mtri.facesfront = ptri->facesfront;
       r_affinetridesc.ptriangles = &mtri;
       r_affinetridesc.pfinalverts = fv[pingpong];

       // FIXME: do all at once as trifan?
       mtri.vertindex[0] = 0;
       for (i = 1 ; i < k - 1 ; i++)
       {
          mtri.vertindex[1] = i;
          mtri.vertindex[2] = i+1;
          D_PolysetDraw_C (); // mankrip - transparencies - edited
       }
    }
