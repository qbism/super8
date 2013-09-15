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
// d_sprite.c: software top-level rasterization driver module for drawing
// sprites

#include "quakedef.h"
#include "d_local.h"

static int		sprite_height;
static int		minindex, maxindex;
static sspan_t	*sprite_spans;



static int			count, spancount, izistep;
static int			izi;
static byte		*pbase, *pdest;
static fixed16_t	s, t, snext, tnext, sstep, tstep;
static float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
static float		sdivz8stepu, tdivz8stepu, zi8stepu;
static byte		btemp;
static short		*pz;

/*
=====================
D_SpriteDrawSpans
=====================
*/

void D_SpriteDrawSpans (sspan_t *pspan)
{
    int			count, spancount, izistep;
    int			izi;
    byte		*pbase, *pdest;
    fixed16_t	s, t, snext, tnext, sstep, tstep;
    float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
    float		sdivzstepu, tdivzstepu, zistepu;
    byte		btemp;
    short		*pz;


    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

    pbase = cacheblock;

    sdivzstepu = d_sdivzstepu * 16;
    tdivzstepu = d_tdivzstepu * 16;
    zistepu = d_zistepu * 16;

// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u;
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        // Manoel Kasimier - begin
        count = pspan->count >> 4;
        spancount = pspan->count % 16;
        // Manoel Kasimier - end

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
        // we count on FP exceptions being turned off to avoid range problems
        izi = (int)(zi * 0x8000 * 0x10000);

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

        while (count-- > 0) // Manoel Kasimier
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

            // Manoel Kasimier - begin

            pdest += 16;
            pz += 16;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-16] <= (izi >> 16))
                {
                    pz[-16] = izi >> 16;
                    pdest[-16] = btemp;
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            }
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-15] <= (izi >> 16))
                {
                    pz[-15] = izi >> 16;
                    pdest[-15] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-14] <= (izi >> 16))
                {
                    pz[-14] = izi >> 16;
                    pdest[-14] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-13] <= (izi >> 16))
                {
                    pz[-13] = izi >> 16;
                    pdest[-13] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-12] <= (izi >> 16))
                {
                    pz[-12] = izi >> 16;
                    pdest[-12] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-11] <= (izi >> 16))
                {
                    pz[-11] = izi >> 16;
                    pdest[-11] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-10] <= (izi >> 16))
                {
                    pz[-10] = izi >> 16;
                    pdest[-10] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-9] <= (izi >> 16))
                {
                    pz[-9] = izi >> 16;
                    pdest[-9] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-8] <= (izi >> 16))
                {
                    pz[-8] = izi >> 16;
                    pdest[-8] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-7] <= (izi >> 16))
                {
                    pz[-7] = izi >> 16;
                    pdest[-7] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-6] <= (izi >> 16))
                {
                    pz[-6] = izi >> 16;
                    pdest[-6] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-5] <= (izi >> 16))
                {
                    pz[-5] = izi >> 16;
                    pdest[-5] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-4] <= (izi >> 16))
                {
                    pz[-4] = izi >> 16;
                    pdest[-4] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-3] <= (izi >> 16))
                {
                    pz[-3] = izi >> 16;
                    pdest[-3] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-2] <= (izi >> 16))
                {
                    pz[-2] = izi >> 16;
                    pdest[-2] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;
            btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
            if (btemp != 255)
            {
                if (pz[-1] <= (izi >> 16))
                {
                    pz[-1] = izi >> 16;
                    pdest[-1] = btemp;
                }
            }
            s += sstep;
            t += tstep;
            izi += izistep;

        }
        if (spancount > 0)
        {


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

            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - begin
            //qb: Duff's Device loop unroll per mh.
            pdest += spancount;
            switch (spancount)
            {
            case 16:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-16] <= (izi >> 16))
                    {
                        pz[-16] = izi >> 16;
                        pdest[-16] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 15:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-15] <= (izi >> 16))
                    {
                        pz[-15] = izi >> 16;
                        pdest[-15] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;

            case 14:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-14] <= (izi >> 16))
                    {
                        pz[-14] = izi >> 16;
                        pdest[-14] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 13:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-13] <= (izi >> 16))
                    {
                        pz[-13] = izi >> 16;
                        pdest[-13] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 12:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-12] <= (izi >> 16))
                    {
                        pz[-12] = izi >> 16;
                        pdest[-12] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 11:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-11] <= (izi >> 16))
                    {
                        pz[-11] = izi >> 16;
                        pdest[-11] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 10:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-10] <= (izi >> 16))
                    {
                        pz[-10] = izi >> 16;
                        pdest[-10] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 9:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-9] <= (izi >> 16))
                    {
                        pz[-9] = izi >> 16;
                        pdest[-9] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 8:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-8] <= (izi >> 16))
                    {
                        pz[-8] = izi >> 16;
                        pdest[-8] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 7:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-7] <= (izi >> 16))
                    {
                        pz[-7] = izi >> 16;
                        pdest[-7] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 6:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-6] <= (izi >> 16))
                    {
                        pz[-6] = izi >> 16;
                        pdest[-6] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 5:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-5] <= (izi >> 16))
                    {
                        pz[-5] = izi >> 16;
                        pdest[-5] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 4:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-4] <= (izi >> 16))
                    {
                        pz[-4] = izi >> 16;
                        pdest[-4] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 3:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-3] <= (izi >> 16))
                    {
                        pz[-3] = izi >> 16;
                        pdest[-3] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 2:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-2] <= (izi >> 16))
                    {
                        pz[-2] = izi >> 16;
                        pdest[-2] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;
            case 1:
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (pz[-1] <= (izi >> 16))
                    {
                        pz[-1] = izi >> 16;
                        pdest[-1] = btemp;
                    }
                }
                s += sstep;
                t += tstep;
                izi += izistep;

                break;
            }
            //qb: ( http://forums.inside3d.com/viewtopic.php?t=2717 ) - end
        }
        pspan++;
    }
    while (pspan->count != DS_SPAN_LIST_END);
}


void D_SpriteDrawSpans_66 (sspan_t *pspan) // Manoel Kasimier - transparencies
{
    int			count, spancount, izistep;
    int			izi;
    byte		*pbase, *pdest;
    fixed16_t	s, t, snext, tnext, sstep, tstep;
    float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
    float		sdivz8stepu, tdivz8stepu, zi8stepu;
    byte		btemp;
    short		*pz;


    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

    pbase = cacheblock;

    sdivz8stepu = d_sdivzstepu * 8;
    tdivz8stepu = d_tdivzstepu * 8;
    zi8stepu = d_zistepu * 8;

// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u;
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        count = pspan->count;

        if (count <= 0)
            goto NextSpan;

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
        // we count on FP exceptions being turned off to avoid range problems
        izi = (int)(zi * 0x8000 * 0x10000);

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

        do
        {
            // calculate s and t at the far end of the span
            if (count >= 8)
                spancount = 8;
            else
                spancount = count;

            count -= spancount;

            if (count)
            {
                // calculate s/z, t/z, zi->fixed s and t at far end of span,
                // calculate s and t steps across span by shifting
                sdivz += sdivz8stepu;
                tdivz += tdivz8stepu;
                zi += zi8stepu;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                sstep = (snext - s) >> 3;
                tstep = (tnext - t) >> 3;
            }
            else
            {
                // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
                // can't step off polygon), clamp, calculate s and t steps across
                // span by division, biasing steps low so we don't run off the
                // texture
                spancountminus1 = (float)(spancount - 1);
                sdivz += d_sdivzstepu * spancountminus1;
                tdivz += d_tdivzstepu * spancountminus1;
                zi += d_zistepu * spancountminus1;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                if (spancount > 1)
                {
                    sstep = (snext - s) / (spancount - 1);
                    tstep = (tnext - t) / (spancount - 1);
                }
            }

            do
            {
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                    if (*pz <= (izi >> 16))
                        *pdest = alphamap[*pdest + btemp*256]; // Manoel Kasimier - transparencies

                izi += izistep;
                pdest++;
                pz++;
                s += sstep;
                t += tstep;
            }
            while (--spancount > 0);

            s = snext;
            t = tnext;

        }
        while (count > 0);

NextSpan:
        pspan++;

    }
    while (pspan->count != DS_SPAN_LIST_END);
}

void D_SpriteDrawSpans_33 (sspan_t *pspan) // Manoel Kasimier - transparencies
{
    int			count, spancount, izistep;
    int			izi;
    byte		*pbase, *pdest;
    fixed16_t	s, t, snext, tnext, sstep, tstep;
    float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
    float		sdivz8stepu, tdivz8stepu, zi8stepu;
    byte		btemp;
    short		*pz;


    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

    pbase = cacheblock;

    sdivz8stepu = d_sdivzstepu * 8;
    tdivz8stepu = d_tdivzstepu * 8;
    zi8stepu = d_zistepu * 8;

// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u;
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        count = pspan->count;

        if (count <= 0)
            goto NextSpan;

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
        // we count on FP exceptions being turned off to avoid range problems
        izi = (int)(zi * 0x8000 * 0x10000);

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

        do
        {
            // calculate s and t at the far end of the span
            if (count >= 8)
                spancount = 8;
            else
                spancount = count;

            count -= spancount;

            if (count)
            {
                // calculate s/z, t/z, zi->fixed s and t at far end of span,
                // calculate s and t steps across span by shifting
                sdivz += sdivz8stepu;
                tdivz += tdivz8stepu;
                zi += zi8stepu;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                sstep = (snext - s) >> 3;
                tstep = (tnext - t) >> 3;
            }
            else
            {
                // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
                // can't step off polygon), clamp, calculate s and t steps across
                // span by division, biasing steps low so we don't run off the
                // texture
                spancountminus1 = (float)(spancount - 1);
                sdivz += d_sdivzstepu * spancountminus1;
                tdivz += d_tdivzstepu * spancountminus1;
                zi += d_zistepu * spancountminus1;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                if (spancount > 1)
                {
                    sstep = (snext - s) / (spancount - 1);
                    tstep = (tnext - t) / (spancount - 1);
                }
            }

            do
            {
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                    if (*pz <= (izi >> 16))
                        *pdest = alphamap[btemp + *pdest*256]; // Manoel Kasimier - transparencies

                izi += izistep;
                pdest++;
                pz++;
                s += sstep;
                t += tstep;
            }
            while (--spancount > 0);

            s = snext;
            t = tnext;

        }
        while (count > 0);

NextSpan:
        pspan++;

    }
    while (pspan->count != DS_SPAN_LIST_END);
}

void D_SpriteDrawSpans_Stippled (sspan_t *pspan) // Manoel Kasimier - transparencies
{
    int			count, spancount, izistep;
    int			izi;
    byte		*pbase, *pdest;
    fixed16_t	s, t, snext, tnext, sstep, tstep;
    float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
    float		sdivz8stepu, tdivz8stepu, zi8stepu;
    byte		btemp;
    short		*pz;
    int			teste; // Manoel Kasimier - stipple alpha


    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

    pbase = cacheblock;

    sdivz8stepu = d_sdivzstepu * 8;
    tdivz8stepu = d_tdivzstepu * 8;
    zi8stepu = d_zistepu * 8;

// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u;
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;
        teste = ( ((int)pdest-(int)d_viewbuffer) / screenwidth) & 1; // Manoel Kasimier - stipple alpha

        count = pspan->count;

        if (count <= 0)
            goto NextSpan;

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
        // we count on FP exceptions being turned off to avoid range problems
        izi = (int)(zi * 0x8000 * 0x10000);

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

        do
        {
            // calculate s and t at the far end of the span
            if (count >= 8)
                spancount = 8;
            else
                spancount = count;

            count -= spancount;

            if (count)
            {
                // calculate s/z, t/z, zi->fixed s and t at far end of span,
                // calculate s and t steps across span by shifting
                sdivz += sdivz8stepu;
                tdivz += tdivz8stepu;
                zi += zi8stepu;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                sstep = (snext - s) >> 3;
                tstep = (tnext - t) >> 3;
            }
            else
            {
                // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
                // can't step off polygon), clamp, calculate s and t steps across
                // span by division, biasing steps low so we don't run off the
                // texture
                spancountminus1 = (float)(spancount - 1);
                sdivz += d_sdivzstepu * spancountminus1;
                tdivz += d_tdivzstepu * spancountminus1;
                zi += d_zistepu * spancountminus1;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                if (spancount > 1)
                {
                    sstep = (snext - s) / (spancount - 1);
                    tstep = (tnext - t) / (spancount - 1);
                }
            }

            do
            {
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                {
                    if (*pz <= (izi >> 16))
                    {
                        if (((int)pdest + teste) & 1) // Manoel Kasimier - stipple alpha
                        {
                            // Manoel Kasimier - stipple alpha
                            *pz = izi >> 16;
                            *pdest = btemp;
                        } // Manoel Kasimier - stipple alpha
                    }
                }

                izi += izistep;
                pdest++;
                pz++;
                s += sstep;
                t += tstep;
            }
            while (--spancount > 0);

            s = snext;
            t = tnext;

        }
        while (count > 0);

NextSpan:
        pspan++;

    }
    while (pspan->count != DS_SPAN_LIST_END);
}

void D_SpriteDrawSpans_Add (sspan_t *pspan) // Manoel Kasimier - transparencies
{
    int			count, spancount, izistep;
    int			izi;
    byte		*pbase, *pdest;
    fixed16_t	s, t, snext, tnext, sstep, tstep;
    float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
    float		sdivz8stepu, tdivz8stepu, zi8stepu;
    byte		btemp;
    short		*pz;


    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

    pbase = cacheblock;

    sdivz8stepu = d_sdivzstepu * 8;
    tdivz8stepu = d_tdivzstepu * 8;
    zi8stepu = d_zistepu * 8;

// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u;
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        count = pspan->count;

        if (count <= 0)
            goto NextSpan;

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
        // we count on FP exceptions being turned off to avoid range problems
        izi = (int)(zi * 0x8000 * 0x10000);

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

        do
        {
            // calculate s and t at the far end of the span
            if (count >= 8)
                spancount = 8;
            else
                spancount = count;

            count -= spancount;

            if (count)
            {
                // calculate s/z, t/z, zi->fixed s and t at far end of span,
                // calculate s and t steps across span by shifting
                sdivz += sdivz8stepu;
                tdivz += tdivz8stepu;
                zi += zi8stepu;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                sstep = (snext - s) >> 3;
                tstep = (tnext - t) >> 3;
            }
            else
            {
                // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
                // can't step off polygon), clamp, calculate s and t steps across
                // span by division, biasing steps low so we don't run off the
                // texture
                spancountminus1 = (float)(spancount - 1);
                sdivz += d_sdivzstepu * spancountminus1;
                tdivz += d_tdivzstepu * spancountminus1;
                zi += d_zistepu * spancountminus1;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                if (spancount > 1)
                {
                    sstep = (snext - s) / (spancount - 1);
                    tstep = (tnext - t) / (spancount - 1);
                }
            }

            do
            {
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                    if (*pz <= (izi >> 16))
                        *pdest = additivemap[btemp + *pdest*256]; // Manoel Kasimier - transparencies

                izi += izistep;
                pdest++;
                pz++;
                s += sstep;
                t += tstep;
            }
            while (--spancount > 0);

            s = snext;
            t = tnext;

        }
        while (count > 0);

NextSpan:
        pspan++;

    }
    while (pspan->count != DS_SPAN_LIST_END);
}

void D_SpriteDrawSpans_Shadow (sspan_t *pspan) // Manoel Kasimier - transparencies
{
    int			count, spancount, izistep;
    int			izi;
    byte		*pbase, *pdest;
    fixed16_t	s, t, snext, tnext, sstep, tstep;
    float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
    float		sdivz8stepu, tdivz8stepu, zi8stepu;
    byte		btemp;
    short		*pz;
    float intensity; // Manoel Kasimier


    sstep = 0;	// keep compiler happy
    tstep = 0;	// ditto

    pbase = cacheblock;

    sdivz8stepu = d_sdivzstepu * 8;
    tdivz8stepu = d_tdivzstepu * 8;
    zi8stepu = d_zistepu * 8;

// we count on FP exceptions being turned off to avoid range problems
    izistep = (int)(d_zistepu * 0x8000 * 0x10000);

    do
    {
        pdest = (byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u;
        pz = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

        count = pspan->count;

        if (count <= 0)
            goto NextSpan;

        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
        du = (float)pspan->u;
        dv = (float)pspan->v;

        sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
        tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
        zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
        z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
        // we count on FP exceptions being turned off to avoid range problems
        izi = (int)(zi * 0x8000 * 0x10000);

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

        do
        {
            // calculate s and t at the far end of the span
            if (count >= 8)
                spancount = 8;
            else
                spancount = count;

            count -= spancount;

            if (count)
            {
                // calculate s/z, t/z, zi->fixed s and t at far end of span,
                // calculate s and t steps across span by shifting
                sdivz += sdivz8stepu;
                tdivz += tdivz8stepu;
                zi += zi8stepu;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                sstep = (snext - s) >> 3;
                tstep = (tnext - t) >> 3;
            }
            else
            {
                // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
                // can't step off polygon), clamp, calculate s and t steps across
                // span by division, biasing steps low so we don't run off the
                // texture
                spancountminus1 = (float)(spancount - 1);
                sdivz += d_sdivzstepu * spancountminus1;
                tdivz += d_tdivzstepu * spancountminus1;
                zi += d_zistepu * spancountminus1;
                z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
                snext = (int)(sdivz * z) + sadjust;
                if (snext > bbextents)
                    snext = bbextents;
                else if (snext < 8)
                    snext = 8;	// prevent round-off error on <0 steps from
                //  from causing overstepping & running off the
                //  edge of the texture

                tnext = (int)(tdivz * z) + tadjust;
                if (tnext > bbextentt)
                    tnext = bbextentt;
                else if (tnext < 8)
                    tnext = 8;	// guard against round-off error on <0 steps

                if (spancount > 1)
                {
                    sstep = (snext - s) / (spancount - 1);
                    tstep = (tnext - t) / (spancount - 1);
                }
            }

            do
            {
                btemp = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
                if (btemp != 255)
                    // Manoel Kasimier - begin
                {
                    intensity = (1.0 - ((float)((izi >> 16)- *pz)/20.0));
                    if (intensity == 1 && currententity->alpha == ENTALPHA_DEFAULT)
                        *pdest = 0;
                    else if (intensity > 0 && intensity <= 1)
                        *pdest = (byte)vid.colormap[*pdest + ((32 + (int)(31.0*intensity*ENTALPHA_DECODE(currententity->alpha))) * 256)];
                    // Manoel Kasimier - end
                }
                // Manoel Kasimier - end

                izi += izistep;
                pdest++;
                pz++;
                s += sstep;
                t += tstep;
            }
            while (--spancount > 0);

            s = snext;
            t = tnext;

        }
        while (count > 0);

NextSpan:
        pspan++;

    }
    while (pspan->count != DS_SPAN_LIST_END);
}


/*
=====================
D_SpriteScanLeftEdge
=====================
*/
void D_SpriteScanLeftEdge (void)
{
    int			i, v, itop, ibottom, lmaxindex;
    emitpoint_t	*pvert, *pnext;
    sspan_t		*pspan;
    float		du, dv, vtop, vbottom, slope;
    fixed16_t	u, u_step;

    pspan = sprite_spans;
    i = minindex;
    if (i == 0)
        i = r_spritedesc.nump;

    lmaxindex = maxindex;
    if (lmaxindex == 0)
        lmaxindex = r_spritedesc.nump;

    vtop = ceil (r_spritedesc.pverts[i].v);

    do
    {
        pvert = &r_spritedesc.pverts[i];
        pnext = pvert - 1;

        vbottom = ceil (pnext->v);

        if (vtop < vbottom)
        {
            du = pnext->u - pvert->u;
            dv = pnext->v - pvert->v;
            slope = du / dv;
            u_step = (int)(slope * 0x10000);
            // adjust u to ceil the integer portion
            u = (int)((pvert->u + (slope * (vtop - pvert->v))) * 0x10000) +
                (0x10000 - 1);
            itop = (int)vtop;
            ibottom = (int)vbottom;

            for (v=itop ; v<ibottom ; v++)
            {
                pspan->u = u >> 16;
                pspan->v = v;
                u += u_step;
                pspan++;
            }
        }

        vtop = vbottom;

        i--;
        if (i == 0)
            i = r_spritedesc.nump;

    }
    while (i != lmaxindex);
}


/*
=====================
D_SpriteScanRightEdge
=====================
*/
void D_SpriteScanRightEdge (void)
{
    int			i, v, itop, ibottom;
    emitpoint_t	*pvert, *pnext;
    sspan_t		*pspan;
    float		du, dv, vtop, vbottom, slope, uvert, unext, vvert, vnext;
    fixed16_t	u, u_step;

    pspan = sprite_spans;
    i = minindex;

    vvert = r_spritedesc.pverts[i].v;
    if (vvert < r_refdef.fvrecty_adj)
        vvert = r_refdef.fvrecty_adj;
    if (vvert > r_refdef.fvrectbottom_adj)
        vvert = r_refdef.fvrectbottom_adj;

    vtop = ceil (vvert);

    do
    {
        pvert = &r_spritedesc.pverts[i];
        pnext = pvert + 1;

        vnext = pnext->v;
        if (vnext < r_refdef.fvrecty_adj)
            vnext = r_refdef.fvrecty_adj;
        if (vnext > r_refdef.fvrectbottom_adj)
            vnext = r_refdef.fvrectbottom_adj;

        vbottom = ceil (vnext);

        if (vtop < vbottom)
        {
            uvert = pvert->u;
            if (uvert < r_refdef.fvrectx_adj)
                uvert = r_refdef.fvrectx_adj;
            if (uvert > r_refdef.fvrectright_adj)
                uvert = r_refdef.fvrectright_adj;

            unext = pnext->u;
            if (unext < r_refdef.fvrectx_adj)
                unext = r_refdef.fvrectx_adj;
            if (unext > r_refdef.fvrectright_adj)
                unext = r_refdef.fvrectright_adj;

            du = unext - uvert;
            dv = vnext - vvert;
            slope = du / dv;
            u_step = (int)(slope * 0x10000);
            // adjust u to ceil the integer portion
            u = (int)((uvert + (slope * (vtop - vvert))) * 0x10000) +
                (0x10000 - 1);
            itop = (int)vtop;
            ibottom = (int)vbottom;

            for (v=itop ; v<ibottom ; v++)
            {
                pspan->count = (u >> 16) - pspan->u;
                u += u_step;
                pspan++;
            }
        }

        vtop = vbottom;
        vvert = vnext;

        i++;
        if (i == r_spritedesc.nump)
            i = 0;

    }
    while (i != maxindex);

    pspan->count = DS_SPAN_LIST_END;	// mark the end of the span list
}


/*
=====================
D_SpriteCalculateGradients
=====================
*/
void D_SpriteCalculateGradients (void)
{
    vec3_t		p_normal, p_saxis, p_taxis, p_temp1;
    float		distinv;

    TransformVector (r_spritedesc.vpn, p_normal);
    TransformVector (r_spritedesc.vright, p_saxis);
    TransformVector (r_spritedesc.vup, p_taxis);
    VectorInverse (p_taxis);

    distinv = 1.0 / (-DotProduct (modelorg, r_spritedesc.vpn));

    // Manoel Kasimier - QC Scale - begin
    p_saxis[0] /= (currententity->scale2 * currententity->scalev[0]);
    p_saxis[1] /= (currententity->scale2 * currententity->scalev[0]);
    p_saxis[2] /= (currententity->scale2 * currententity->scalev[0]);

    p_taxis[0] /= (currententity->scale2 * currententity->scalev[1]);
    p_taxis[1] /= (currententity->scale2 * currententity->scalev[1]);
    p_taxis[2] /= (currententity->scale2 * currententity->scalev[1]);
    // Manoel Kasimier - QC Scale - end

    d_sdivzstepu = p_saxis[0] * xscaleinv;
    d_tdivzstepu = p_taxis[0] * xscaleinv;

    d_sdivzstepv = -p_saxis[1] * yscaleinv;
    d_tdivzstepv = -p_taxis[1] * yscaleinv;

    d_zistepu = p_normal[0] * xscaleinv * distinv;
    d_zistepv = -p_normal[1] * yscaleinv * distinv;

    d_sdivzorigin = p_saxis[2] - xcenter * d_sdivzstepu -
                    ycenter * d_sdivzstepv;
    d_tdivzorigin = p_taxis[2] - xcenter * d_tdivzstepu -
                    ycenter * d_tdivzstepv;
    d_ziorigin = p_normal[2] * distinv - xcenter * d_zistepu -
                 ycenter * d_zistepv;

    TransformVector (modelorg, p_temp1);

    sadjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
              (-(cachewidth >> 1) << 16);
    tadjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
              (-(sprite_height >> 1) << 16);

// -1 (-epsilon) so we never wander off the edge of the texture
    bbextents = (cachewidth << 16) - 1;
    bbextentt = (sprite_height << 16) - 1;
}


/*
=====================
D_DrawSprite
=====================
*/
void D_DrawSprite (void)
{
    int			i, nump;
    float		ymin, ymax;
    emitpoint_t	*pverts;
    sspan_t		spans[MAXHEIGHT+1];

    sprite_spans = spans;

// find the top and bottom vertices, and make sure there's at least one scan to
// draw
    ymin = 999999.9;
    ymax = -999999.9;
    pverts = r_spritedesc.pverts;

    for (i=0 ; i<r_spritedesc.nump ; i++)
    {
        if (pverts->v < ymin)
        {
            ymin = pverts->v;
            minindex = i;
        }

        if (pverts->v > ymax)
        {
            ymax = pverts->v;
            maxindex = i;
        }

        pverts++;
    }

    ymin = ceil (ymin);
    ymax = ceil (ymax);

    if (ymin >= ymax)
        return;		// doesn't cross any scans at all

    cachewidth = r_spritedesc.pspriteframe->width;
    sprite_height = r_spritedesc.pspriteframe->height;
    cacheblock = (byte *)&r_spritedesc.pspriteframe->pixels[0];

// copy the first vertex to the last vertex, so we don't have to deal with
// wrapping
    nump = r_spritedesc.nump;
    pverts = r_spritedesc.pverts;
    pverts[nump] = pverts[0];

    D_SpriteCalculateGradients ();
    D_SpriteScanLeftEdge ();
    D_SpriteScanRightEdge ();
    // Manoel Kasimier - EF_ADDITIVE - begin
    if (currententity->effects & EF_ADDITIVE && additivemap)
        D_SpriteDrawSpans_Add (sprite_spans);
    // Manoel Kasimier - EF_ADDITIVE - end
    // Manoel Kasimier - EF_SHADOW - begin
    else if (currententity->effects & EF_SHADOW)
        D_SpriteDrawSpans_Shadow (sprite_spans);
    // Manoel Kasimier - EF_SHADOW - end
    // Manoel Kasimier - transparencies - begin
    else if (currententity->alpha != ENTALPHA_DEFAULT)
    {
        if (sw_stipplealpha.value)
            D_SpriteDrawSpans_Stippled (sprite_spans);
        else if (ENTALPHA_DECODE(currententity->alpha) < 0.5)
            D_SpriteDrawSpans_33 (sprite_spans);
        else D_SpriteDrawSpans_66 (sprite_spans);
    }
//	else if (r_sprite_addblend.value && additivemap)
//		D_SpriteDrawSpans_Add (sprite_spans);
    else D_SpriteDrawSpans (sprite_spans);
}

