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
// d_scan.c
//
// Portable C scan-level rasterization code, all pixel depths.

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"

byte    *r_turb_pbase, *r_turb_pdest;
static fixed16_t   r_turb_s, r_turb_t, r_turb_sstep, r_turb_tstep;
int                *r_turb_turb;
int                r_turb_spancount;

extern pixel_t *warpbuf;
extern cvar_t vid_ddraw;

// mankrip - hi-res waterwarp - begin
int *intsintable_x = NULL,   *intsintable_y = NULL,   *warpcolumn = NULL,   *warprow = NULL;
byte *turbdest = NULL,   *turbsrc = NULL;
float  uwarpscale = 1.0f, vwarpscale = 1.0f;
// mankrip - hi-res waterwarp - end

void R_InitTurb (void)
{
    // mankrip - hi-res waterwarp - begin
   float
    ideal_width, ideal_height
    ,   ustep
    ,   uoffset // horizontal offset for the phase
    ,   uamp // horizontal warping amplitude
    ,   ustretch
    ,   u // source
    ,   vstep
    ,   voffset // vertical offset for the phase
    ,   vamp // vertical warping amplitude
    ,   vstretch
    ,   v // source
    ;
    int
    x // destination
    ,   y // destination
    ,   warpcolmem
    ,   warprowmem
    ;
    // widescreen
    ideal_height = (float)r_refdef.vrect.height;
    ideal_width = (float)r_refdef.vrect.width;

    uwarpscale = 2.0; //qb: needs to be integer for fisheye views to line-up.  // ideal_width  / MIN_VID_WIDTH;
    vwarpscale = 1.0; //qb: needs to be integer for fisheye views to line-up.  // //ideal_height / MIN_VID_HEIGHT;
    ustep = 1.0f / uwarpscale;
    vstep = 1.0f / vwarpscale;
    uoffset = (0.5f * ( (float)r_refdef.vrect.width  - ideal_width )) / uwarpscale;
    voffset = (0.5f * ( (float)r_refdef.vrect.height - ideal_height)) / vwarpscale;
    uamp = AMP2 * uwarpscale;
    vamp = AMP2 * vwarpscale;
    ustretch = ( (float)r_refdef.vrect.width  - uamp * 2.0f) / (float)r_refdef.vrect.width ; // screen compression - use ideal_width instead?
    vstretch = ( (float)r_refdef.vrect.height - vamp * 2.0f) / (float)r_refdef.vrect.height; // screen compression - use ideal_height instead?

    if (warprow)// || warpcolumn || intsintable_x || intsintable_y)
    {
        free (warprow);
        free (warpcolumn);
        free (intsintable_x);
        free (intsintable_y);
    }
    warprowmem = r_refdef.vrect.height + (int) (vamp * 2.0f + CYCLE * vwarpscale);
    intsintable_y = malloc (sizeof (int) * warprowmem);
    warprow       = malloc (sizeof (int) * warprowmem);
    for (v = 0.0f, y = 0 ; y < warprowmem ; v += vstep, y++)
    {
        // horizontal offset for waves
        // horizontal amplitude, vertical frequency
        // changes every line, remains the same every column
        intsintable_y[y] = uamp + sin ( (v - voffset) * 3.14159 * 2.0 / CYCLE) * uamp;
        //   warpcolumn[x] = (int) ( (float)x * ustretch);

        // vertical offset for waves
        // remains the same every line, changes every column
        //   intsintable_x[x] = vamp + sin ( (u - uoffset) * 3.14159 * 2.0 / CYCLE) * vamp;
        warprow[y]    = (int) ( (float)y * vstretch) * screenwidth;
    }
    warpcolmem = r_refdef.vrect.width + (int) (uamp * 2.0f + CYCLE * uwarpscale);
    intsintable_x = malloc (sizeof (int) * warpcolmem);
    warpcolumn    = malloc (sizeof (int) * warpcolmem);
    for (u = 0.0f, x = 0 ; x < warpcolmem ; u += ustep, x++)
    {
        // horizontal offset for waves
        // horizontal amplitude, vertical frequency
        // changes every line, remains the same every column
        //   intsintable_y[y] = uamp + sin ( (v - voffset) * 3.14159 * 2.0 / CYCLE) * uamp;
        warpcolumn[x] = (int) ( (float)x * ustretch);

        // vertical offset for waves
        // remains the same every line, changes every column
        intsintable_x[x] = vamp + sin ( (u - uoffset) * 3.14159 * 2.0 / CYCLE) * vamp;
        //   warprow[y]    = (int) ( (float)y * vstretch) * screenwidth;
    }
}
#define UNROLL_SPAN_SHIFT   5
#define UNROLL_SPAN_MAX   (1 << UNROLL_SPAN_SHIFT) // 32

/*
=============
D_WarpScreen //qb: from MK inside3d tute

// this performs a slight compression of the screen at the same time as
// the sine warp, to keep the edges from wrapping
=============
*/

void D_WarpScreen (void)
{
    // mankrip - hi-res waterwarp - begin
    static byte    *dest, *src;
    static int   *turb_x, *turb_y;
    static int i;
    const float    timeoffset = (float) ( (int) (cl.time * SPEED * 2) & (CYCLE - 1)); // turbulence phase offset  //qb: double speed

    static byte  *tempdest;
    static int  *row, *col, *turb_x_temp;
    static int count, spancount, y;

    turb_x = intsintable_x + (int) (timeoffset * uwarpscale);
    turb_y = intsintable_y + (int) (timeoffset * vwarpscale);

    src = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;
    dest= warpbuf + scr_vrect.y * vid.width + scr_vrect.x;

    for (y = 0 ; y < r_refdef.vrect.height ; y++, dest += vid.width)
    {
        tempdest = dest;
        row = warprow + y;
        col = warpcolumn + turb_y[y];
        turb_x_temp = turb_x;
        count     = r_refdef.vrect.width >> UNROLL_SPAN_SHIFT; // divided by 32
        spancount = r_refdef.vrect.width %  UNROLL_SPAN_MAX; // remainder of the above division (min zero, max 32)

        while (count--)
        {
            tempdest[31] = src[row[turb_x_temp[31]] + col[31]];
            tempdest[30] = src[row[turb_x_temp[30]] + col[30]];
            tempdest[29] = src[row[turb_x_temp[29]] + col[29]];
            tempdest[28] = src[row[turb_x_temp[28]] + col[28]];
            tempdest[27] = src[row[turb_x_temp[27]] + col[27]];
            tempdest[26] = src[row[turb_x_temp[26]] + col[26]];
            tempdest[25] = src[row[turb_x_temp[25]] + col[25]];
            tempdest[24] = src[row[turb_x_temp[24]] + col[24]];
            tempdest[23] = src[row[turb_x_temp[23]] + col[23]];
            tempdest[22] = src[row[turb_x_temp[22]] + col[22]];
            tempdest[21] = src[row[turb_x_temp[21]] + col[21]];
            tempdest[20] = src[row[turb_x_temp[20]] + col[20]];
            tempdest[19] = src[row[turb_x_temp[19]] + col[19]];
            tempdest[18] = src[row[turb_x_temp[18]] + col[18]];
            tempdest[17] = src[row[turb_x_temp[17]] + col[17]];
            tempdest[16] = src[row[turb_x_temp[16]] + col[16]];
            tempdest[15] = src[row[turb_x_temp[15]] + col[15]];
            tempdest[14] = src[row[turb_x_temp[14]] + col[14]];
            tempdest[13] = src[row[turb_x_temp[13]] + col[13]];
            tempdest[12] = src[row[turb_x_temp[12]] + col[12]];
            tempdest[11] = src[row[turb_x_temp[11]] + col[11]];
            tempdest[10] = src[row[turb_x_temp[10]] + col[10]];
            tempdest[ 9] = src[row[turb_x_temp[ 9]] + col[ 9]];
            tempdest[ 8] = src[row[turb_x_temp[ 8]] + col[ 8]];
            tempdest[ 7] = src[row[turb_x_temp[ 7]] + col[ 7]];
            tempdest[ 6] = src[row[turb_x_temp[ 6]] + col[ 6]];
            tempdest[ 5] = src[row[turb_x_temp[ 5]] + col[ 5]];
            tempdest[ 4] = src[row[turb_x_temp[ 4]] + col[ 4]];
            tempdest[ 3] = src[row[turb_x_temp[ 3]] + col[ 3]];
            tempdest[ 2] = src[row[turb_x_temp[ 2]] + col[ 2]];
            tempdest[ 1] = src[row[turb_x_temp[ 1]] + col[ 1]];
            tempdest[ 0] = src[row[turb_x_temp[ 0]] + col[ 0]];

            tempdest += UNROLL_SPAN_MAX;
            turb_x_temp += UNROLL_SPAN_MAX;
            col += UNROLL_SPAN_MAX;
        }
        if (spancount)
        {
            switch (spancount)
            {
            // from UNROLL_SPAN_MAX to 1
            case 32:
                tempdest[31] = src[row[turb_x_temp[31]] + col[31]];
            case 31:
                tempdest[30] = src[row[turb_x_temp[30]] + col[30]];
            case 30:
                tempdest[29] = src[row[turb_x_temp[29]] + col[29]];
            case 29:
                tempdest[28] = src[row[turb_x_temp[28]] + col[28]];
            case 28:
                tempdest[27] = src[row[turb_x_temp[27]] + col[27]];
            case 27:
                tempdest[26] = src[row[turb_x_temp[26]] + col[26]];
            case 26:
                tempdest[25] = src[row[turb_x_temp[25]] + col[25]];
            case 25:
                tempdest[24] = src[row[turb_x_temp[24]] + col[24]];
            case 24:
                tempdest[23] = src[row[turb_x_temp[23]] + col[23]];
            case 23:
                tempdest[22] = src[row[turb_x_temp[22]] + col[22]];
            case 22:
                tempdest[21] = src[row[turb_x_temp[21]] + col[21]];
            case 21:
                tempdest[20] = src[row[turb_x_temp[20]] + col[20]];
            case 20:
                tempdest[19] = src[row[turb_x_temp[19]] + col[19]];
            case 19:
                tempdest[18] = src[row[turb_x_temp[18]] + col[18]];
            case 18:
                tempdest[17] = src[row[turb_x_temp[17]] + col[17]];
            case 17:
                tempdest[16] = src[row[turb_x_temp[16]] + col[16]];
            case 16:
                tempdest[15] = src[row[turb_x_temp[15]] + col[15]];
            case 15:
                tempdest[14] = src[row[turb_x_temp[14]] + col[14]];
            case 14:
                tempdest[13] = src[row[turb_x_temp[13]] + col[13]];
            case 13:
                tempdest[12] = src[row[turb_x_temp[12]] + col[12]];
            case 12:
                tempdest[11] = src[row[turb_x_temp[11]] + col[11]];
            case 11:
                tempdest[10] = src[row[turb_x_temp[10]] + col[10]];
            case 10:
                tempdest[ 9] = src[row[turb_x_temp[ 9]] + col[ 9]];
            case  9:
                tempdest[ 8] = src[row[turb_x_temp[ 8]] + col[ 8]];
            case  8:
                tempdest[ 7] = src[row[turb_x_temp[ 7]] + col[ 7]];
            case  7:
                tempdest[ 6] = src[row[turb_x_temp[ 6]] + col[ 6]];
            case  6:
                tempdest[ 5] = src[row[turb_x_temp[ 5]] + col[ 5]];
            case  5:
                tempdest[ 4] = src[row[turb_x_temp[ 4]] + col[ 4]];
            case  4:
                tempdest[ 3] = src[row[turb_x_temp[ 3]] + col[ 3]];
            case  3:
                tempdest[ 2] = src[row[turb_x_temp[ 2]] + col[ 2]];
            case  2:
                tempdest[ 1] = src[row[turb_x_temp[ 1]] + col[ 1]];
            case  1:
                tempdest[ 0] = src[row[turb_x_temp[ 0]] + col[ 0]];
                break;
            }
        }
    }

    //qb: copy buffer to video
    src = warpbuf + scr_vrect.y * vid.width + scr_vrect.x;
    dest = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;
    for (i=0;
            i<r_refdef.vrect.height;
            i++, src += vid.width, dest += vid.rowbytes)
        memcpy(dest, src, r_refdef.vrect.width);
}


static int          count, spancount;
static byte         *pbase, *pdest;
static fixed16_t    s, t, snext, tnext, sstep, tstep;
static float        sdivz, tdivz, zi, z, du, dv, spancountminus1;
static float        sdivzstepu, tdivzstepu, zistepu;
static int          izi, izistep; // mankrip
static short        *pz; // mankrip
static int cw_local;  //qb: using a static is faster, based on assembly output


/*
=============
Turbulent8
=============
*/
void Turbulent8 (espan_t *pspan)
{
    static int     izistep2, sturb, tturb, teste; // Manoel Kasimier - translucent water

    r_turb_turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));
    r_turb_pbase = (byte *)cacheblock;

    sdivzstepu = d_sdivzstepu * 16;
    tdivzstepu = d_tdivzstepu * 16;
    zistepu = d_zistepu * 16;

    // Manoel Kasimier - translucent water - begin
// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);
    izistep2 = izistep*2;
    // Manoel Kasimier - translucent water - end

    do
    {
        r_turb_pdest = (byte *)((byte *)d_viewbuffer +
                                (screenwidth * pspan->v) + pspan->u);
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u; // Manoel Kasimier - translucent water

        count = pspan->count;

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
        // we count on FP exceptions being turned off to avoid range problems // Manoel Kasimier - translucent water
        izi = (int)(zi * 0x8000 * 0x10000); // Manoel Kasimier - translucent water

        r_turb_s = (int)(sdivz * z) + sadjust;
        if (r_turb_s > bbextents)
            r_turb_s = bbextents;
        else if (r_turb_s < 0)
            r_turb_s = 0;

        r_turb_t = (int)(tdivz * z) + tadjust;
        if (r_turb_t > bbextentt)
            r_turb_t = bbextentt;
        else if (r_turb_t < 0)
            r_turb_t = 0;

        do
        {
            // calculate s and t at the far end of the span
            if (count >= 16)
                r_turb_spancount = 16;
            else
                r_turb_spancount = count;

            count -= r_turb_spancount;

            if (count)
            {
                // calculate s/z, t/z, zi->fixed s and t at far end of span,
                // calculate s and t steps across span by shifting
                sdivz += sdivzstepu;
                tdivz += tdivzstepu;
                zi += zistepu;
                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point

                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 16)
                    snext = 16; // prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 16)
                    tnext = 16; // guard against round-off error on <0 steps

                r_turb_sstep = (snext - r_turb_s) >> 4;
                r_turb_tstep = (tnext - r_turb_t) >> 4;
            }
            else
            {
                // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
                // can't step off polygon), clamp, calculate s and t steps across
                // span by division, biasing steps low so we don't run off the
                // texture
                spancountminus1 = (float)(r_turb_spancount - 1);
                sdivz += d_sdivzstepu * spancountminus1;
                tdivz += d_tdivzstepu * spancountminus1;
                zi += d_zistepu * spancountminus1;
                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 16)
                    snext = 16; // prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 16)
                    tnext = 16; // guard against round-off error on <0 steps

                if (r_turb_spancount > 1)
                {
                    r_turb_sstep = (snext - r_turb_s) / (r_turb_spancount - 1);
                    r_turb_tstep = (tnext - r_turb_t) / (r_turb_spancount - 1);
                }
            }

            r_turb_s = r_turb_s & ((CYCLE<<16)-1);
            r_turb_t = r_turb_t & ((CYCLE<<16)-1);

            // Manoel Kasimier - translucent water - begin
            if (r_overdraw)
            {
                if (r_wateralpha.value <= 0.41) // 33%
                {
                    do
                    {
                        if (*pz <= (izi >> 16))
                        {
                            sturb = ((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63;
                            tturb = ((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63;
#define temp *(r_turb_pbase + (tturb<<6) + sturb)
                            *r_turb_pdest = alphamap[temp + *r_turb_pdest*256];
                        }
                        *r_turb_pdest++;
                        izi += izistep;
                        pz++;
                        r_turb_s += r_turb_sstep;
                        r_turb_t += r_turb_tstep;
                    }
                    while (--r_turb_spancount > 0);
                }
                else if (r_wateralpha.value < 0.61 || !alphamap) // 50%
                {
                    do
                    {
                        if (*pz <= (izi >> 16))
                        {
                            sturb = ((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63;
                            tturb = ((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63;
#define temp *(r_turb_pbase + (tturb<<6) + sturb)
                            *r_turb_pdest = alpha50map[temp + *r_turb_pdest*256];
                        }
                        *r_turb_pdest++;
                        izi += izistep;
                        pz++;
                        r_turb_s += r_turb_sstep;
                        r_turb_t += r_turb_tstep;
                    }
                    while (--r_turb_spancount > 0);

                }
                else //if (r_wateralpha.value >= 0.66 && alphamap) // 66%
                {
                    do
                    {
                        if (*pz <= (izi >> 16))
                        {
                            sturb = ((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63;
                            tturb = ((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63;
#define temp *(r_turb_pbase + (tturb<<6) + sturb)
                            *r_turb_pdest = alphamap[*r_turb_pdest + temp*256];
                        }
                        *r_turb_pdest++;
                        izi += izistep;
                        pz++;
                        r_turb_s += r_turb_sstep;
                        r_turb_t += r_turb_tstep;
                    }
                    while (--r_turb_spancount > 0);
                }
            }
            else
                // Manoel Kasimier - translucent water - end
            {
                do //qb: just move D_DrawTurbulent8Span stuff here
                {
#define sturb (((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63) // Manoel Kasimier - edited
#define tturb (((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63) // Manoel Kasimier - edited
                    *r_turb_pdest++ = *(r_turb_pbase + (tturb<<6) + sturb);
                    r_turb_s += r_turb_sstep;
                    r_turb_t += r_turb_tstep;
                }
                while (--r_turb_spancount > 0);
#undef sturb
#undef tturb
            }

end_of_loop: // Manoel Kasimier - translucent water
            r_turb_s = snext;
            r_turb_t = tnext;

        }
        while (count > 0);

    }
    while ((pspan = pspan->pnext) != NULL);
}



/*
=============
D_DrawSpans
=============
*/

/*==============================================
//unrolled- mh, MK, qbism
//============================================*/


//qbism: pointer to pbase and macroize idea from mankrip
#define WRITEPDEST(i) { pdest[i] = *(pbase + (s >> 16) + (t >> 16) * cw_local); s+=sstep; t+=tstep;} //qb: using a static is faster

void D_DrawSpans16_C (espan_t *pspan) //qb: up it from 8 to 16.  This + unroll = big speed gain!
{
    cw_local = cachewidth; //qb: static is faster, based on assembly output
    pbase = (byte *)cacheblock;
    sdivzstepu = d_sdivzstepu * 16;
    tdivzstepu = d_tdivzstepu * 16;
    zistepu = d_zistepu * 16;

    do
    {
        pdest = (byte *)((byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u);
        count = pspan->count >> 4;
        spancount = pspan->count % 16;

        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;   // prescale to 16.16 fixed-point

        s = bound( 0, (int) (sdivz * z) + sadjust, bbextents);
        t = bound( 0, (int) (tdivz * z) + tadjust, bbextentt);

        while (count-- > 0) // Manoel Kasimier
        {
            sdivz += sdivzstepu;
            tdivz += tdivzstepu;
            zi += zistepu;
            z = (float)0x10000 / zi;   // prescale to 16.16 fixed-point

            snext = bound (16, (int) (sdivz * z) + sadjust, bbextents);
            tnext = bound (16, (int) (tdivz * z) + tadjust, bbextentt);

            sstep = (snext - s) >> 4;
            tstep = (tnext - t) >> 4;
            pdest += 16;

            WRITEPDEST(-16);
            WRITEPDEST(-15);
            WRITEPDEST(-14);
            WRITEPDEST(-13);
            WRITEPDEST(-12);
            WRITEPDEST(-11);
            WRITEPDEST(-10);
            WRITEPDEST(-9);
            WRITEPDEST(-8);
            WRITEPDEST(-7);
            WRITEPDEST(-6);
            WRITEPDEST(-5);
            WRITEPDEST(-4);
            WRITEPDEST(-3);
            WRITEPDEST(-2);
            WRITEPDEST(-1);

            s = snext;
            t = tnext;
        }
        if (spancount > 0)
        {
            spancountminus1 = (float)(spancount - 1);
            sdivz += d_sdivzstepu * spancountminus1;
            tdivz += d_tdivzstepu * spancountminus1;
            zi += d_zistepu * spancountminus1;
            z = (float)0x10000 / zi;   // prescale to 16.16 fixed-point

            snext = bound(16, (int)(sdivz * z) + sadjust, bbextents);
            tnext = bound(16, (int)(tdivz * z) + tadjust, bbextentt);

            if (spancount > 1)
            {
                sstep = (snext - s) / (spancount - 1);
                tstep = (tnext - t) / (spancount - 1);
            }

            pdest += spancount;

            switch (spancount)
            {
            case 16:
                WRITEPDEST(-16);
            case 15:
                WRITEPDEST(-15);
            case 14:
                WRITEPDEST(-14);
            case 13:
                WRITEPDEST(-13);
            case 12:
                WRITEPDEST(-12);
            case 11:
                WRITEPDEST(-11);
            case 10:
                WRITEPDEST(-10);
            case  9:
                WRITEPDEST(-9);
            case  8:
                WRITEPDEST(-8);
            case  7:
                WRITEPDEST(-7);
            case  6:
                WRITEPDEST(-6);
            case  5:
                WRITEPDEST(-5);
            case  4:
                WRITEPDEST(-4);
            case  3:
                WRITEPDEST(-3);
            case  2:
                WRITEPDEST(-2);
            case  1:
                WRITEPDEST(-1);
                break;
            }
        }
    }
    while ( (pspan = pspan->pnext) != NULL);
}

//qb: fence textures  //fencemap[fencepix*256 + pdest[i]];
#define WRITEFENCE(i) {    fencepix = *(pbase + (s >> 16) + (t >> 16) * cw_local);     \
            if (pz[i] <= (izi >> 16) && fencepix != 255){ pdest[i] = fencepix; pz[i] = (izi >> 16);}  \
            izi += izistep;  s += sstep;  t += tstep;    }
void D_DrawSpans16_Fence (espan_t *pspan)
{
    static byte fencepix;
    cw_local = cachewidth;
    pbase = (byte *)cacheblock;


    //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - begin
    sdivzstepu = d_sdivzstepu * 16;
    tdivzstepu = d_tdivzstepu * 16;
    zistepu = d_zistepu * 16;
    //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - end

    // mankrip - begin
    // we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);
    // mankrip - end

    do
    {
        pdest = (byte *)((byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u);
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u; // mankrip

        count = pspan->count >> 4; // mh
        spancount = pspan->count % 16; // mankrip

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
        // we count on FP exceptions being turned off to avoid range problems // mankrip
        izi = (int) (zi * 0x8000 * 0x10000); // mankrip

        s = (int)(sdivz * z) + sadjust;
        if (s > bbextents)
            s = bbextents;
        else if (s < 0)
            s = 0;

        t = (int)(tdivz * z) + tadjust;
        if (t > bbextentt)
            t = bbextentt;
        else if (t < 0)
            t = 0;

        while (count--) // mankrip
        {
            // calculate s/z, t/z, zi->fixed s and t at far end of span,
            // calculate s and t steps across span by shifting
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - begin
            sdivz += sdivzstepu;
            tdivz += tdivzstepu;
            zi += zistepu;
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - end
            z = (float)0x10000 / zi;   // prescale to 16.16 fixed-point

            snext = (int) (sdivz * z) + sadjust;
            if (snext > bbextents)
                snext = bbextents;
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - begin
            else if (snext <= 16)
                snext = 16;   // prevent round-off error on <0 steps causing overstepping & running off the edge of the texture
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - end

            tnext = (int) (tdivz * z) + tadjust;
            if (tnext > bbextentt)
                tnext = bbextentt;
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - begin
            else if (tnext < 16)
                tnext = 16;   // guard against round-off error on <0 steps

            sstep = (snext - s) >> 4;
            tstep = (tnext - t) >> 4;
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - end

            pdest += 16;
            pz += 16;
            WRITEFENCE(-16);
            WRITEFENCE(-15);
            WRITEFENCE(-14);
            WRITEFENCE(-13);
            WRITEFENCE(-12);
            WRITEFENCE(-11);
            WRITEFENCE(-10);
            WRITEFENCE(-9);
            WRITEFENCE(-8);
            WRITEFENCE(-7);
            WRITEFENCE(-6);
            WRITEFENCE(-5);
            WRITEFENCE(-4);
            WRITEFENCE(-3);
            WRITEFENCE(-2);
            WRITEFENCE(-1);

            s = snext;
            t = tnext;
            // mankrip - begin
        }
        if (spancount > 0)
        {
            // mankrip - end

            // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so can't step off polygon),
            // clamp, calculate s and t steps across span by division, biasing steps low so we don't run off the texture
            spancountminus1 = (float)(spancount - 1);
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - begin
            sdivz += d_sdivzstepu * spancountminus1;
            tdivz += d_tdivzstepu * spancountminus1;
            zi += d_zistepu * spancountminus1;
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - end
            z = (float)0x10000 / zi;   // prescale to 16.16 fixed-point
            snext = (int)(sdivz * z) + sadjust;
            if (snext > bbextents)
                snext = bbextents;
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - begin
            else if (snext < 16)
                snext = 16;   // prevent round-off error on <0 steps from causing overstepping & running off the edge of the texture
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - end

            tnext = (int)(tdivz * z) + tadjust;
            if (tnext > bbextentt)
                tnext = bbextentt;
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - begin
            else if (tnext < 16)
                tnext = 16;   // guard against round-off error on <0 steps
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - end

            if (spancount > 1)
            {
                sstep = (snext - s) / (spancount - 1);
                tstep = (tnext - t) / (spancount - 1);
            }

            //qb: Duff's Device loop unroll per mh.
            pdest += spancount;
            // mankrip - begin
            pz += spancount;
            switch (spancount)
            {
            case 16:
                WRITEFENCE(-16);
            case 15:
                WRITEFENCE(-15);
            case 14:
                WRITEFENCE(-14);
            case 13:
                WRITEFENCE(-13);
            case 12:
                WRITEFENCE(-12);
            case 11:
                WRITEFENCE(-11);
            case 10:
                WRITEFENCE(-10);
            case  9:
                WRITEFENCE(-9);
            case  8:
                WRITEFENCE(-8);
            case  7:
                WRITEFENCE(-7);
            case  6:
                WRITEFENCE(-6);
            case  5:
                WRITEFENCE(-5);
            case  4:
                WRITEFENCE(-4);
            case  3:
                WRITEFENCE(-3);
            case  2:
                WRITEFENCE(-2);
            case  1:
                WRITEFENCE(-1);
                break;
            }
        }
        // mankrip - end
    }
    while ((pspan = pspan->pnext) != NULL);
}


//qb: Makaqu 1.5 begin

#define WRITEBLEND(i) { if (pz[i] <= (izi >> 16))  \
    { pdest[i] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cw_local) + pdest[i] * 256]; } \
            izi += izistep; s += sstep; t += tstep; }

void D_DrawSpans16_Blend (espan_t *pspan)
{
    cw_local = cachewidth;
    pbase = (byte *)cacheblock;

    sdivzstepu = d_sdivzstepu * 16;
    tdivzstepu = d_tdivzstepu * 16;
    zistepu = d_zistepu * 16;
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)((byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u);
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        count = pspan->count >> 4;
        spancount = pspan->count % 16;
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;
        izi = (int) (zi * 0x8000 * 0x10000);

        s = (int)(sdivz * z) + sadjust;
        if (s > bbextents)
            s = bbextents;
        else if (s < 0)
            s = 0;

        t = (int)(tdivz * z) + tadjust;
        if (t > bbextentt)
            t = bbextentt;
        else if (t < 0)
            t = 0;

        while (count--)
        {
            sdivz += sdivzstepu;
            tdivz += tdivzstepu;
            zi += zistepu;
            z = (float)0x10000 / zi;

            snext = (int) (sdivz * z) + sadjust;
            if (snext > bbextents)
                snext = bbextents;
            else if (snext <= 16)
                snext = 16;

            tnext = (int) (tdivz * z) + tadjust;
            if (tnext > bbextentt)
                tnext = bbextentt;
            else if (tnext < 16)
                tnext = 16;

            sstep = (snext - s) >> 4;
            tstep = (tnext - t) >> 4;

            pdest += 16;
            pz += 16;
            WRITEBLEND(-16);
            WRITEBLEND(-15);
            WRITEBLEND(-14);
            WRITEBLEND(-13);
            WRITEBLEND(-12);
            WRITEBLEND(-11);
            WRITEBLEND(-10);
            WRITEBLEND(-9);
            WRITEBLEND(-8);
            WRITEBLEND(-7);
            WRITEBLEND(-6);
            WRITEBLEND(-5);
            WRITEBLEND(-4);
            WRITEBLEND(-3);
            WRITEBLEND(-2);
            WRITEBLEND(-1);

            s = snext;
            t = tnext;
        }
        if (spancount > 0)
        {
            spancountminus1 = (float)(spancount - 1);
            sdivz += d_sdivzstepu * spancountminus1;
            tdivz += d_tdivzstepu * spancountminus1;
            zi += d_zistepu * spancountminus1;
            z = (float)0x10000 / zi;
            snext = (int)(sdivz * z) + sadjust;
            if (snext > bbextents)
                snext = bbextents;
            else if (snext < 16)
                snext = 16;

            tnext = (int)(tdivz * z) + tadjust;
            if (tnext > bbextentt)
                tnext = bbextentt;
            else if (tnext < 16)
                tnext = 16;

            if (spancount > 1)
            {
                sstep = (snext - s) / (spancount - 1);
                tstep = (tnext - t) / (spancount - 1);
            }

            pdest += spancount;
            pz += spancount;
            switch (spancount)
            {
            case 16:
                WRITEBLEND(-16);
            case 15:
                WRITEBLEND(-15);
            case 14:
                WRITEBLEND(-14);
            case 13:
                WRITEBLEND(-13);
            case 12:
                WRITEBLEND(-12);
            case 11:
                WRITEBLEND(-11);
            case 10:
                WRITEBLEND(-10);
            case  9:
                WRITEBLEND(-9);
            case  8:
                WRITEBLEND(-8);
            case  7:
                WRITEBLEND(-7);
            case  6:
                WRITEBLEND(-6);
            case  5:
                WRITEBLEND(-5);
            case  4:
                WRITEBLEND(-4);
            case  3:
                WRITEBLEND(-3);
            case  2:
                WRITEBLEND(-2);
            case  1:
                WRITEBLEND(-1);
                break;
            }
        }
    }
    while ((pspan = pspan->pnext) != NULL);
}


#define WRITEBLEND50(i) { if (pz[i] <= (izi >> 16))  \
    { pdest[i] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cw_local) + pdest[i] * 256]; } \
            izi += izistep; s += sstep; t += tstep; }

void D_DrawSpans16_Blend50 (espan_t *pspan)
{
    cw_local = cachewidth;
    pbase = (byte *)cacheblock;

    sdivzstepu = d_sdivzstepu * 16;
    tdivzstepu = d_tdivzstepu * 16;
    zistepu = d_zistepu * 16;
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)((byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u);
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        count = pspan->count >> 4;
        spancount = pspan->count % 16;
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;
        izi = (int) (zi * 0x8000 * 0x10000);

        s = (int)(sdivz * z) + sadjust;
        if (s > bbextents)
            s = bbextents;
        else if (s < 0)
            s = 0;

        t = (int)(tdivz * z) + tadjust;
        if (t > bbextentt)
            t = bbextentt;
        else if (t < 0)
            t = 0;

        while (count--)
        {
            sdivz += sdivzstepu;
            tdivz += tdivzstepu;
            zi += zistepu;
            z = (float)0x10000 / zi;

            snext = (int) (sdivz * z) + sadjust;
            if (snext > bbextents)
                snext = bbextents;
            else if (snext <= 16)
                snext = 16;

            tnext = (int) (tdivz * z) + tadjust;
            if (tnext > bbextentt)
                tnext = bbextentt;
            else if (tnext < 16)
                tnext = 16;

            sstep = (snext - s) >> 4;
            tstep = (tnext - t) >> 4;

            pdest += 16;
            pz += 16;
            WRITEBLEND50(-16);
            WRITEBLEND50(-15);
            WRITEBLEND50(-14);
            WRITEBLEND50(-13);
            WRITEBLEND50(-12);
            WRITEBLEND50(-11);
            WRITEBLEND50(-10);
            WRITEBLEND50(-9);
            WRITEBLEND50(-8);
            WRITEBLEND50(-7);
            WRITEBLEND50(-6);
            WRITEBLEND50(-5);
            WRITEBLEND50(-4);
            WRITEBLEND50(-3);
            WRITEBLEND50(-2);
            WRITEBLEND50(-1);

            s = snext;
            t = tnext;
        }
        if (spancount > 0)
        {
            spancountminus1 = (float)(spancount - 1);
            sdivz += d_sdivzstepu * spancountminus1;
            tdivz += d_tdivzstepu * spancountminus1;
            zi += d_zistepu * spancountminus1;
            z = (float)0x10000 / zi;
            snext = (int)(sdivz * z) + sadjust;
            if (snext > bbextents)
                snext = bbextents;
            else if (snext < 16)
                snext = 16;

            tnext = (int)(tdivz * z) + tadjust;
            if (tnext > bbextentt)
                tnext = bbextentt;
            else if (tnext < 16)
                tnext = 16;

            if (spancount > 1)
            {
                sstep = (snext - s) / (spancount - 1);
                tstep = (tnext - t) / (spancount - 1);
            }

            pdest += spancount;
            pz += spancount;
            switch (spancount)
            {
            case 16:
                WRITEBLEND50(-16);
            case 15:
                WRITEBLEND50(-15);
            case 14:
                WRITEBLEND50(-14);
            case 13:
                WRITEBLEND50(-13);
            case 12:
                WRITEBLEND50(-12);
            case 11:
                WRITEBLEND50(-11);
            case 10:
                WRITEBLEND50(-10);
            case  9:
                WRITEBLEND50(-9);
            case  8:
                WRITEBLEND50(-8);
            case  7:
                WRITEBLEND50(-7);
            case  6:
                WRITEBLEND50(-6);
            case  5:
                WRITEBLEND50(-5);
            case  4:
                WRITEBLEND50(-4);
            case  3:
                WRITEBLEND50(-3);
            case  2:
                WRITEBLEND50(-2);
            case  1:
                WRITEBLEND50(-1);
                break;
            }
        }
    }
    while ((pspan = pspan->pnext) != NULL);
}


#define WRITEBLENDBACK(i) { if (pz[i] <= (izi >> 16))  \
    { pdest[i] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cw_local) * 256 + pdest[i]]; } \
            izi += izistep; s += sstep; t += tstep; }


void D_DrawSpans16_BlendBackwards (espan_t *pspan)
{
    cw_local = cachewidth;
    pbase = (byte *)cacheblock;

    sdivzstepu = d_sdivzstepu * 16;
    tdivzstepu = d_tdivzstepu * 16;
    zistepu = d_zistepu * 16;
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)((byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u);
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        count = pspan->count >> 4;
        spancount = pspan->count % 16;
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;
        izi = (int) (zi * 0x8000 * 0x10000);

        s = (int)(sdivz * z) + sadjust;
        if (s > bbextents)
            s = bbextents;
        else if (s < 0)
            s = 0;

        t = (int)(tdivz * z) + tadjust;
        if (t > bbextentt)
            t = bbextentt;
        else if (t < 0)
            t = 0;

        while (count--)
        {
            sdivz += sdivzstepu;
            tdivz += tdivzstepu;
            zi += zistepu;
            z = (float)0x10000 / zi;

            snext = (int) (sdivz * z) + sadjust;
            if (snext > bbextents)
                snext = bbextents;
            else if (snext <= 16)
                snext = 16;

            tnext = (int) (tdivz * z) + tadjust;
            if (tnext > bbextentt)
                tnext = bbextentt;
            else if (tnext < 16)
                tnext = 16;

            sstep = (snext - s) >> 4;
            tstep = (tnext - t) >> 4;

            pdest += 16;
            pz += 16;
            WRITEBLENDBACK(-16);
            WRITEBLENDBACK(-15);
            WRITEBLENDBACK(-14);
            WRITEBLENDBACK(-13);
            WRITEBLENDBACK(-12);
            WRITEBLENDBACK(-11);
            WRITEBLENDBACK(-10);
            WRITEBLENDBACK(-9);
            WRITEBLENDBACK(-8);
            WRITEBLENDBACK(-7);
            WRITEBLENDBACK(-6);
            WRITEBLENDBACK(-5);
            WRITEBLENDBACK(-4);
            WRITEBLENDBACK(-3);
            WRITEBLENDBACK(-2);
            WRITEBLENDBACK(-1);

            s = snext;
            t = tnext;
        }
        if (spancount > 0)
        {
            spancountminus1 = (float)(spancount - 1);
            sdivz += d_sdivzstepu * spancountminus1;
            tdivz += d_tdivzstepu * spancountminus1;
            zi += d_zistepu * spancountminus1;
            z = (float)0x10000 / zi;
            snext = (int)(sdivz * z) + sadjust;
            if (snext > bbextents)
                snext = bbextents;
            else if (snext < 16)
                snext = 16;

            tnext = (int)(tdivz * z) + tadjust;
            if (tnext > bbextentt)
                tnext = bbextentt;
            else if (tnext < 16)
                tnext = 16;

            if (spancount > 1)
            {
                sstep = (snext - s) / (spancount - 1);
                tstep = (tnext - t) / (spancount - 1);
            }

            pdest += spancount;
            pz += spancount;
            switch (spancount)
            {
            case 16:
                WRITEBLENDBACK(-16);
            case 15:
                WRITEBLENDBACK(-15);
            case 14:
                WRITEBLENDBACK(-14);
            case 13:
                WRITEBLENDBACK(-13);
            case 12:
                WRITEBLENDBACK(-12);
            case 11:
                WRITEBLENDBACK(-11);
            case 10:
                WRITEBLENDBACK(-10);
            case  9:
                WRITEBLENDBACK(-9);
            case  8:
                WRITEBLENDBACK(-8);
            case  7:
                WRITEBLENDBACK(-7);
            case  6:
                WRITEBLENDBACK(-6);
            case  5:
                WRITEBLENDBACK(-5);
            case  4:
                WRITEBLENDBACK(-4);
            case  3:
                WRITEBLENDBACK(-3);
            case  2:
                WRITEBLENDBACK(-2);
            case  1:
                WRITEBLENDBACK(-1);
                break;
            }
        }
    }
    while ((pspan = pspan->pnext) != NULL);
}



/*
=============
D_DrawZSpans
=============
*/

#if	!id386
void D_DrawZSpans (espan_t *pspan)
{
    static int                          count, doublecount, izistep;
    static int                          izi;
    static unsigned short                       *pdest;
    static unsigned             ltemp;
    static double                       zi;
    static float                        du, dv;

// FIXME: check for clamping/range problems
// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        count = pspan->count;

        // calculate the initial 1/z
        du = (float)pspan->u;
        dv = (float)pspan->v;

        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        // we count on FP exceptions being turned off to avoid range problems
        izi = (int)(zi * 0x8000 * 0x10000);

        if ((long)pdest & 0x02)
        {
            *pdest++ = (short)(izi >> 16);
            izi += izistep;
            count--;
        }

        if ((doublecount = count >> 1) > 0)
        {
            do
            {
                ltemp = izi >> 16;
                izi += izistep;
                ltemp |= izi & 0xFFFF0000;
                izi += izistep;
                *(int *)pdest = ltemp;
                pdest += 2;
            }
            while (--doublecount > 0);
        }

        if (count & 1)
            *pdest = (short)(izi >> 16);

    }
    while ((pspan = pspan->pnext) != NULL);
}
#endif

