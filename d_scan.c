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

unsigned char	*r_turb_pbase, *r_turb_pdest;
static fixed16_t		r_turb_s, r_turb_t, r_turb_sstep, r_turb_tstep;
int				*r_turb_turb;
int				r_turb_spancount;
byte	*r_warpbuffer; // Manoel Kasimier - hi-res waterwarp & buffered video

extern cvar_t thread_warp;

void D_DrawTurbulent8Span (void);

// mankrip - hi-res waterwarp - begin
int *intsintable_x = NULL,   *intsintable_y = NULL,   *warpcolumn = NULL,   *warprow = NULL;
byte *turbdest = NULL,   *turbsrc = NULL;
float  uwarpscale = 1.0f, vwarpscale = 1.0f;
// mankrip - hi-res waterwarp - end

void R_InitTurb (void)
{
    // mankrip - hi-res waterwarp - begin
    extern cvar_t vid_mode;
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

//qb:  turbsrc = d_viewbuffer + r_refdef.vrect.y * screenwidth + r_refdef.vrect.x;
//qb:  turbdest = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;
    // mankrip - hi-res waterwarp - end
}
#define UNROLL_SPAN_SHIFT   5
#define UNROLL_SPAN_MAX   (1 << UNROLL_SPAN_SHIFT) // 32

/*
=============
D_WarpScreen //qb: from MK inside3d tute + add multihreading

// this performs a slight compression of the screen at the same time as
// the sine warp, to keep the edges from wrapping
=============
*/

typedef struct warpslice_s  //qb: for multithreading
{
    int rowstart, rowend;
    byte		*src, *dest;
    int  *turb_x, *turb_y;
} warpslice_t;


void WarpLoop (warpslice_t* ws)
{
    static byte *src, *dest, *tempdest;
    static int  *row, *col, *turb_x, *turb_x_temp, *turb_y;
    static int rollcount, spancount, y;

    src = ws->src;
    dest = ws->dest;
    turb_x = ws->turb_x;
    turb_y = ws->turb_y;

    for (y = ws->rowstart ; y < ws->rowend ; y++, dest += vid.width)
    {
        tempdest = dest;
        row = warprow + y;
        col = warpcolumn + turb_y[y];
        turb_x_temp = turb_x;
        rollcount  = r_refdef.vrect.width >> UNROLL_SPAN_SHIFT; // divided by 32
        spancount = r_refdef.vrect.width %  UNROLL_SPAN_MAX; // remainder of the above division (min zero, max 32)

        while (rollcount--)
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
            case 31:
                tempdest[31] = src[row[turb_x_temp[31]] + col[31]];
            case 30:
                tempdest[30] = src[row[turb_x_temp[30]] + col[30]];
            case 29:
                tempdest[29] = src[row[turb_x_temp[29]] + col[29]];
            case 28:
                tempdest[28] = src[row[turb_x_temp[28]] + col[28]];
            case 27:
                tempdest[27] = src[row[turb_x_temp[27]] + col[27]];
            case 26:
                tempdest[26] = src[row[turb_x_temp[26]] + col[26]];
            case 25:
                tempdest[25] = src[row[turb_x_temp[25]] + col[25]];
            case 24:
                tempdest[24] = src[row[turb_x_temp[24]] + col[24]];
            case 23:
                tempdest[23] = src[row[turb_x_temp[23]] + col[23]];
            case 22:
                tempdest[22] = src[row[turb_x_temp[22]] + col[22]];
            case 21:
                tempdest[21] = src[row[turb_x_temp[21]] + col[21]];
            case 20:
                tempdest[20] = src[row[turb_x_temp[20]] + col[20]];
            case 19:
                tempdest[19] = src[row[turb_x_temp[19]] + col[19]];
            case 18:
                tempdest[18] = src[row[turb_x_temp[18]] + col[18]];
            case 17:
                tempdest[17] = src[row[turb_x_temp[17]] + col[17]];
            case 16:
                tempdest[16] = src[row[turb_x_temp[16]] + col[16]];
            case 15:
                tempdest[15] = src[row[turb_x_temp[15]] + col[15]];
            case 14:
                tempdest[14] = src[row[turb_x_temp[14]] + col[14]];
            case 13:
                tempdest[13] = src[row[turb_x_temp[13]] + col[13]];
            case 12:
                tempdest[12] = src[row[turb_x_temp[12]] + col[12]];
            case 11:
                tempdest[11] = src[row[turb_x_temp[11]] + col[11]];
            case 10:
                tempdest[10] = src[row[turb_x_temp[10]] + col[10]];
            case  9:
                tempdest[ 9] = src[row[turb_x_temp[ 9]] + col[ 9]];
            case  8:
                tempdest[ 8] = src[row[turb_x_temp[ 8]] + col[ 8]];
            case  7:
                tempdest[ 7] = src[row[turb_x_temp[ 7]] + col[ 7]];
            case  6:
                tempdest[ 6] = src[row[turb_x_temp[ 6]] + col[ 6]];
            case  5:
                tempdest[ 5] = src[row[turb_x_temp[ 5]] + col[ 5]];
            case  4:
                tempdest[ 4] = src[row[turb_x_temp[ 4]] + col[ 4]];
            case  3:
                tempdest[ 3] = src[row[turb_x_temp[ 3]] + col[ 3]];
            case  2:
                tempdest[ 2] = src[row[turb_x_temp[ 2]] + col[ 2]];
            case  1:
                tempdest[ 1] = src[row[turb_x_temp[ 1]] + col[ 1]];
            case  0:
                tempdest[ 0] = src[row[turb_x_temp[ 0]] + col[ 0]];
                break;
            }
        }
    }
}

#define MAXWARPTHREADS          16   //qb: for multithreading

void D_WarpScreen (void)
{
    // mankrip - hi-res waterwarp - begin
    static byte    *dest, *src;
    static int   *turb_x, *turb_y;
    static int i;
    const float    timeoffset = (float) ( (int) (cl.time * SPEED) & (CYCLE - 1)); // turbulence phase offset

#ifdef R_THREADED
    static warpslice_t ws[MAXWARPTHREADS];  //qb:  threads
    static pthread_t warpthread[MAXWARPTHREADS];
#endif

    static byte  *tempdest;
    static int  *row, *col, *turb_x_temp;
    static int count, spancount, y, numthreads;

    //R_InitTurb (); //qb: fixme? calling this here because vid.recalc_refdef doesn't seem to always be set properly

    turb_x = intsintable_x + (int) (timeoffset * uwarpscale);
    turb_y = intsintable_y + (int) (timeoffset * vwarpscale);

    src = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;
    dest= r_warpbuffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;

#ifdef R_THREADED
    numthreads = min(thread_warp.value,MAXWARPTHREADS);
    if(numthreads > 1)
    {
        for (i=0; i<numthreads; i++)
        {
            ws[i].src = src;
            ws[i].dest = dest + vid.width * i * (r_refdef.vrect.height/numthreads);
            ws[i].turb_x = turb_x;
            ws[i].turb_y = turb_y;
            ws[i].rowstart= i*(r_refdef.vrect.height/numthreads);
            if (i+1 == numthreads)
                ws[i].rowend = r_refdef.vrect.height;
            else
                ws[i].rowend = (i+1)*(r_refdef.vrect.height/numthreads);
            pthread_create(&warpthread[i], NULL, WarpLoop, &ws[i]);
        }

        //wait for threads to finish
        for (i=0; i<numthreads; i++)
        {
            pthread_join(warpthread[i], NULL);
        }
        // mankrip - hi-res waterwarp - end
    }
    else
#endif

    {
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
    }
//qb: swap buffer with video
    src = vid.buffer;
    vid.buffer = r_warpbuffer;
    vid.direct = vid.buffer;
    vid.conbuffer = vid.buffer;
    r_warpbuffer = src;

    /*
        //qb: copy buffer to video
        src = r_warpbuffer + scr_vrect.y * vid.width + scr_vrect.x;
        dest = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;
        for (i=0;
                i<scr_vrect.height;
                i++, src += vid.width, dest += vid.rowbytes)
            memcpy(dest, src, scr_vrect.width);
    */

}

/*
=============
D_DrawTurbulent8Span
=============
*/
void D_DrawTurbulent8Span (void)
{
//	int		sturb, tturb;

    do
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


/*
=============
Turbulent8
=============
*/
void Turbulent8 (espan_t *pspan)
{
    static int				count;  //qb: do more static.
    static int				izi, izistep, izistep2, sturb, tturb, teste; // Manoel Kasimier - translucent water
    static short			*pz; // Manoel Kasimier - translucent water
    static fixed16_t		snext, tnext;
    static float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
    static float			sdivz16stepu, tdivz16stepu, zi16stepu;

    r_turb_turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));

    r_turb_sstep = 0;	// keep compiler happy
    r_turb_tstep = 0;	// ditto

    r_turb_pbase = (unsigned char *)cacheblock;

    sdivz16stepu = d_sdivzstepu * 16;
    tdivz16stepu = d_tdivzstepu * 16;
    zi16stepu = d_zistepu * 16;

    // Manoel Kasimier - translucent water - begin
// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);
    izistep2 = izistep*2;
    // Manoel Kasimier - translucent water - end

    do
    {
        r_turb_pdest = (unsigned char *)((byte *)d_viewbuffer +
                                         (screenwidth * pspan->v) + pspan->u);
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u; // Manoel Kasimier - translucent water

        count = pspan->count;

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
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
                sdivz += sdivz16stepu;
                tdivz += tdivz16stepu;
                zi += zi16stepu;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 16)
                    snext = 16;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 16)
                    tnext = 16;	// guard against round-off error on <0 steps

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
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 16)
                    snext = 16;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 16)
                    tnext = 16;	// guard against round-off error on <0 steps

                if (r_turb_spancount > 1)
                {
                    r_turb_sstep = (snext - r_turb_s) / (r_turb_spancount - 1);
                    r_turb_tstep = (tnext - r_turb_t) / (r_turb_spancount - 1);
                }
            }

            r_turb_s = r_turb_s & ((CYCLE<<16)-1);
            r_turb_t = r_turb_t & ((CYCLE<<16)-1);

            // Manoel Kasimier - translucent water - begin
            if (r_drawwater)
            {
                if (r_wateralpha.value <= 0.24) // <25%
                {
                    teste = ((((int)r_turb_pdest-(int)d_viewbuffer) / screenwidth)+1) & 1;
                    if (teste) // 25% transparency
                    {
stipple:
                        if (!(((int)r_turb_pdest + teste) & 1)) // if we are in the wrong pixel,
                        {
                            if (r_turb_spancount == 1)
                                goto end_of_loop;
                            // this is not working the way it should. it's drawing 1 pixel outside of the screen.
                            // advance one pixel
                            *r_turb_pdest++;
                            izi += izistep;
                            pz++;
                            r_turb_s += r_turb_sstep;
                            r_turb_t += r_turb_tstep;
                            r_turb_spancount = (r_turb_spancount/2); // -1
                        }
                        else
                            r_turb_spancount = (r_turb_spancount+1)/2;
                        // multiply steps by 2
                        r_turb_sstep*=2;
                        r_turb_tstep*=2;
                        do
                        {
#define sturb2 (((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63)
#define tturb2 (((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63)
                            if (*pz <= (izi >> 16))
                                *r_turb_pdest = *(r_turb_pbase + (tturb2<<6) + sturb2);
                            // advance two pixels
                            r_turb_pdest += 2;
                            pz += 2;
                            izi += izistep2;
                            r_turb_s += r_turb_sstep;
                            r_turb_t += r_turb_tstep;
                        }
                        while (--r_turb_spancount > 0);
                    }
                }
                else if (r_wateralpha.value <= 0.41) // 33%
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
                    teste = ((((int)r_turb_pdest-(int)d_viewbuffer) / screenwidth)+1) & 1;
                    goto stipple;
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
                D_DrawTurbulent8Span ();

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


static int          count, spancount;
static byte         *pbase, *pdest;
static fixed16_t    s, t, snext, tnext, sstep, tstep;
static float        sdivz, tdivz, zi, z, du, dv, spancountminus1;
static float        sdivzstepu, tdivzstepu, zistepu;
static int		    izi, izistep; // mankrip
static short		*pz; // mankrip

//qbism: pointer to pbase and macroize idea from mankrip
#define WRITEPDEST(i) { pdest[i] = *(pbase + (s >> 16) + (t >> 16) * cachewidth); s+=sstep; t+=tstep;}

void D_DrawSpans16 (espan_t *pspan) //qb: up it from 8 to 16.  This + unroll = big speed gain!
{
    sstep = 0;   // keep compiler happy
    tstep = 0;   // ditto

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


//qb: Makaqu 1.5 begin

void D_DrawSpans16_Blend (espan_t *pspan) // mankrip
{

    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

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
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
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

            // mankrip - begin
            pdest += 16;
            pz += 16;
            if (pz[-16] <= (izi >> 16)) pdest[-16] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-16] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-15] <= (izi >> 16)) pdest[-15] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-15] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-14] <= (izi >> 16)) pdest[-14] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-14] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-13] <= (izi >> 16)) pdest[-13] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-13] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-12] <= (izi >> 16)) pdest[-12] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-12] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-11] <= (izi >> 16)) pdest[-11] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-11] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-10] <= (izi >> 16)) pdest[-10] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-10] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -9] <= (izi >> 16)) pdest[ -9] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -9] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -8] <= (izi >> 16)) pdest[ -8] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -8] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -7] <= (izi >> 16)) pdest[ -7] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -7] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -6] <= (izi >> 16)) pdest[ -6] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -6] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -5] <= (izi >> 16)) pdest[ -5] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -5] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -4] <= (izi >> 16)) pdest[ -4] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -4] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -3] <= (izi >> 16)) pdest[ -3] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -3] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -2] <= (izi >> 16)) pdest[ -2] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -2] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -1] <= (izi >> 16)) pdest[ -1] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -1] * 256];
            izi += izistep;
            // mankrip - end

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
                if (pz[-16] <= (izi >> 16)) pdest[-16] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-16] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 15:
                if (pz[-15] <= (izi >> 16)) pdest[-15] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-15] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 14:
                if (pz[-14] <= (izi >> 16)) pdest[-14] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-14] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 13:
                if (pz[-13] <= (izi >> 16)) pdest[-13] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-13] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 12:
                if (pz[-12] <= (izi >> 16)) pdest[-12] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-12] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 11:
                if (pz[-11] <= (izi >> 16)) pdest[-11] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-11] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 10:
                if (pz[-10] <= (izi >> 16)) pdest[-10] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-10] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  9:
                if (pz[ -9] <= (izi >> 16)) pdest[ -9] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -9] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  8:
                if (pz[ -8] <= (izi >> 16)) pdest[ -8] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -8] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  7:
                if (pz[ -7] <= (izi >> 16)) pdest[ -7] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -7] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  6:
                if (pz[ -6] <= (izi >> 16)) pdest[ -6] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -6] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  5:
                if (pz[ -5] <= (izi >> 16)) pdest[ -5] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -5] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  4:
                if (pz[ -4] <= (izi >> 16)) pdest[ -4] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -4] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  3:
                if (pz[ -3] <= (izi >> 16)) pdest[ -3] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -3] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  2:
                if (pz[ -2] <= (izi >> 16)) pdest[ -2] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -2] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  1:
                if (pz[ -1] <= (izi >> 16)) pdest[ -1] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -1] * 256];
                break;
            }
        }
        // mankrip - end
    }
    while ((pspan = pspan->pnext) != NULL);
}


void D_DrawSpans16_Blend50 (espan_t *pspan) //qb
{
    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

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
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
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

            // mankrip - begin
            pdest += 16;
            pz += 16;
            if (pz[-16] <= (izi >> 16)) pdest[-16] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-16] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-15] <= (izi >> 16)) pdest[-15] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-15] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-14] <= (izi >> 16)) pdest[-14] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-14] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-13] <= (izi >> 16)) pdest[-13] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-13] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-12] <= (izi >> 16)) pdest[-12] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-12] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-11] <= (izi >> 16)) pdest[-11] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-11] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-10] <= (izi >> 16)) pdest[-10] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-10] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -9] <= (izi >> 16)) pdest[ -9] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -9] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -8] <= (izi >> 16)) pdest[ -8] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -8] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -7] <= (izi >> 16)) pdest[ -7] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -7] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -6] <= (izi >> 16)) pdest[ -6] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -6] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -5] <= (izi >> 16)) pdest[ -5] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -5] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -4] <= (izi >> 16)) pdest[ -4] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -4] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -3] <= (izi >> 16)) pdest[ -3] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -3] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -2] <= (izi >> 16)) pdest[ -2] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -2] * 256];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -1] <= (izi >> 16)) pdest[ -1] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -1] * 256];
            izi += izistep;
            // mankrip - end

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
                if (pz[-16] <= (izi >> 16)) pdest[-16] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-16] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 15:
                if (pz[-15] <= (izi >> 16)) pdest[-15] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-15] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 14:
                if (pz[-14] <= (izi >> 16)) pdest[-14] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-14] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 13:
                if (pz[-13] <= (izi >> 16)) pdest[-13] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-13] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 12:
                if (pz[-12] <= (izi >> 16)) pdest[-12] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-12] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 11:
                if (pz[-11] <= (izi >> 16)) pdest[-11] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-11] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 10:
                if (pz[-10] <= (izi >> 16)) pdest[-10] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[-10] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  9:
                if (pz[ -9] <= (izi >> 16)) pdest[ -9] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -9] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  8:
                if (pz[ -8] <= (izi >> 16)) pdest[ -8] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -8] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  7:
                if (pz[ -7] <= (izi >> 16)) pdest[ -7] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -7] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  6:
                if (pz[ -6] <= (izi >> 16)) pdest[ -6] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -6] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  5:
                if (pz[ -5] <= (izi >> 16)) pdest[ -5] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -5] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  4:
                if (pz[ -4] <= (izi >> 16)) pdest[ -4] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -4] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  3:
                if (pz[ -3] <= (izi >> 16)) pdest[ -3] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -3] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  2:
                if (pz[ -2] <= (izi >> 16)) pdest[ -2] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -2] * 256];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  1:
                if (pz[ -1] <= (izi >> 16)) pdest[ -1] = alpha50map[*(pbase + (s >> 16) + (t >> 16) * cachewidth) + pdest[ -1] * 256];
                break;
            }
        }
        // mankrip - end
    }
    while ((pspan = pspan->pnext) != NULL);
}


void D_DrawSpans16_BlendBackwards (espan_t *pspan)
{
    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

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
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
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

            // mankrip - begin
            pdest += 16;
            pz += 16;
            if (pz[-16] <= (izi >> 16)) pdest[-16] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-16]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-15] <= (izi >> 16)) pdest[-15] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-15]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-14] <= (izi >> 16)) pdest[-14] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-14]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-13] <= (izi >> 16)) pdest[-13] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-13]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-12] <= (izi >> 16)) pdest[-12] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-12]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-11] <= (izi >> 16)) pdest[-11] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-11]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[-10] <= (izi >> 16)) pdest[-10] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-10]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -9] <= (izi >> 16)) pdest[ -9] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -9]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -8] <= (izi >> 16)) pdest[ -8] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -8]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -7] <= (izi >> 16)) pdest[ -7] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -7]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -6] <= (izi >> 16)) pdest[ -6] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -6]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -5] <= (izi >> 16)) pdest[ -5] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -5]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -4] <= (izi >> 16)) pdest[ -4] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -4]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -3] <= (izi >> 16)) pdest[ -3] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -3]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -2] <= (izi >> 16)) pdest[ -2] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -2]];
            izi += izistep;
            s += sstep;
            t += tstep;
            if (pz[ -1] <= (izi >> 16)) pdest[ -1] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -1]];
            izi += izistep;
            // mankrip - end

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
                if (pz[-16] <= (izi >> 16)) pdest[-16] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-16]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 15:
                if (pz[-15] <= (izi >> 16)) pdest[-15] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-15]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 14:
                if (pz[-14] <= (izi >> 16)) pdest[-14] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-14]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 13:
                if (pz[-13] <= (izi >> 16)) pdest[-13] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-13]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 12:
                if (pz[-12] <= (izi >> 16)) pdest[-12] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-12]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 11:
                if (pz[-11] <= (izi >> 16)) pdest[-11] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-11]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case 10:
                if (pz[-10] <= (izi >> 16)) pdest[-10] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[-10]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  9:
                if (pz[ -9] <= (izi >> 16)) pdest[ -9] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -9]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  8:
                if (pz[ -8] <= (izi >> 16)) pdest[ -8] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -8]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  7:
                if (pz[ -7] <= (izi >> 16)) pdest[ -7] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -7]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  6:
                if (pz[ -6] <= (izi >> 16)) pdest[ -6] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -6]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  5:
                if (pz[ -5] <= (izi >> 16)) pdest[ -5] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -5]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  4:
                if (pz[ -4] <= (izi >> 16)) pdest[ -4] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -4]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  3:
                if (pz[ -3] <= (izi >> 16)) pdest[ -3] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -3]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  2:
                if (pz[ -2] <= (izi >> 16)) pdest[ -2] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -2]];
                izi += izistep;
                s += sstep;
                t += tstep;
            case  1:
                if (pz[ -1] <= (izi >> 16)) pdest[ -1] = alphamap[*(pbase + (s >> 16) + (t >> 16) * cachewidth) * 256 + pdest[ -1]];
                break;
            }
        }
        // mankrip - end
    }
    while ((pspan = pspan->pnext) != NULL);
}



/*
=============
D_DrawZSpans
=============
*/

void D_DrawZSpans (espan_t *pspan)
{
    static int				count, doublecount, izistep;
    static int				izi;
    static short			*pdest;
    static unsigned		ltemp;
    static double			zi;
    static float			du, dv;

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



