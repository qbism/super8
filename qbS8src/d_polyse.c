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
// d_polyset.c: routines for drawing sets of polygons sharing the same
// texture (used for Alias models)

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"

// TODO: put in span spilling to shrink list size
// !!! if this is changed, it must be changed in d_polysa.s too !!!
#define DPS_MAXSPANS			MAXHEIGHT+1
// 1 extra for spanpackage that marks end

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
    void			*pdest;
    short			*pz;
    int				count;
    byte			*ptex;
    int				sfrac, tfrac, light, zi;
} spanpackage_t;

typedef struct
{
    int		isflattop;
    int		numleftedges;
    int		*pleftedgevert0;
    int		*pleftedgevert1;
    int		*pleftedgevert2;
    int		numrightedges;
    int		*prightedgevert0;
    int		*prightedgevert1;
    int		*prightedgevert2;
} edgetable;

int	r_p0[6], r_p1[6], r_p2[6];

byte		*d_pcolormap;

int			d_aflatcolor;
int			d_xdenom;

edgetable	*pedgetable;

edgetable	edgetables[12] =
{
    {0, 1, r_p0, r_p2, NULL, 2, r_p0, r_p1, r_p2 },
    {0, 2, r_p1, r_p0, r_p2,   1, r_p1, r_p2, NULL},
    {1, 1, r_p0, r_p2, NULL, 1, r_p1, r_p2, NULL},
    {0, 1, r_p1, r_p0, NULL, 2, r_p1, r_p2, r_p0 },
    {0, 2, r_p0, r_p2, r_p1,   1, r_p0, r_p1, NULL},
    {0, 1, r_p2, r_p1, NULL, 1, r_p2, r_p0, NULL},
    {0, 1, r_p2, r_p1, NULL, 2, r_p2, r_p0, r_p1 },
    {0, 2, r_p2, r_p1, r_p0,   1, r_p2, r_p0, NULL},
    {0, 1, r_p1, r_p0, NULL, 1, r_p1, r_p2, NULL},
    {1, 1, r_p2, r_p1, NULL, 1, r_p0, r_p1, NULL},
    {1, 1, r_p1, r_p0, NULL, 1, r_p2, r_p0, NULL},
    {0, 1, r_p0, r_p2, NULL, 1, r_p0, r_p1, NULL},
};

// FIXME: some of these can become statics
int				a_sstepxfrac, a_tstepxfrac, r_lstepx, a_ststepxwhole;
int				r_sstepx, r_tstepx, r_lstepy, r_sstepy, r_tstepy;
int				r_zistepx, r_zistepy;
int				d_aspancount, d_countextrastep;

spanpackage_t			*a_spans;
spanpackage_t			*d_pedgespanpackage;
static int				ystart;
byte					*d_pdest, *d_ptex;
short					*d_pz;
int						d_sfrac, d_tfrac, d_light, d_zi;
int						d_ptexextrastep, d_sfracextrastep;
int						d_tfracextrastep, d_lightextrastep, d_pdestextrastep;
int						d_lightbasestep, d_pdestbasestep, d_ptexbasestep;
int						d_sfracbasestep, d_tfracbasestep;
int						d_ziextrastep, d_zibasestep;
int						d_pzextrastep, d_pzbasestep;

typedef struct
{
    int		quotient;
    int		remainder;
} adivtab_t;

static adivtab_t	adivtab[32*32] =
{
#include "adivtab.h"
};

byte	*skintable[MAX_LBM_HEIGHT];
int		skinwidth;
byte	*skinstart;

void D_PolysetDrawSpans8 (spanpackage_t *pspanpackage);
void D_PolysetCalcGradients (int skinwidth);
void D_DrawSubdiv (void);
void D_DrawNonSubdiv (void);
void D_PolysetRecursiveTriangle (int *p1, int *p2, int *p3);
void D_PolysetSetEdgeTable (void);
void D_RasterizeAliasPolySmooth (void);
void D_PolysetScanLeftEdge (int height);
void D_DrawNonSubdiv_C (void); // Manoel Kasimier - transparencies

/*
================
D_PolysetDraw
================
*/
void D_PolysetDraw_C (void) // Manoel Kasimier - transparencies - edited
{
    spanpackage_t	spans[DPS_MAXSPANS + 1 +
                          ((CACHE_SIZE - 1) / sizeof(spanpackage_t)) + 1];
    // one extra because of cache line pretouching

    a_spans = (spanpackage_t *)
              (((long)&spans[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));

    if (r_affinetridesc.drawtype)
    {
        D_DrawSubdiv ();
    }
    else
    {
        D_DrawNonSubdiv_C (); // Manoel Kasimier - transparencies - edited
    }
}

// Manoel Kasimier - transparencies

/*
================
D_PolysetDrawFinalVerts
================
*/
void D_PolysetDrawFinalVerts (finalvert_t *fv, int numverts)
{
    int		i, z;
    short	*zbuf;

    for (i=0 ; i<numverts ; i++, fv++)
    {
        // valid triangle coordinates for filling can include the bottom and
        // right clip edges, due to the fill rule; these shouldn't be drawn
        if ((fv->v[0] < r_refdef.vrectright) &&
                (fv->v[1] < r_refdef.vrectbottom))
        {
            z = fv->v[5]>>16;
            zbuf = zspantable[fv->v[1]] + fv->v[0];
            if (z >= *zbuf)
            {
                int		pix;

                *zbuf = z;
                pix = skintable[fv->v[3]>>16][fv->v[2]>>16];
                pix = ((byte *)acolormap)[pix + (fv->v[4] & 0xFF00) ];
                d_viewbuffer[d_scantable[fv->v[1]] + fv->v[0]] = pix;
            }
        }
    }
}
// Manoel Kasimier - transparencies


/*
================
D_DrawSubdiv
================
*/
void D_DrawSubdiv (void)
{
    mtriangle_t		*ptri;
    finalvert_t		*pfv, *index0, *index1, *index2;
    int				i;
    int				lnumtriangles;

    pfv = r_affinetridesc.pfinalverts;
    ptri = r_affinetridesc.ptriangles;
    lnumtriangles = r_affinetridesc.numtriangles;

    for (i=0 ; i<lnumtriangles ; i++)
    {
        index0 = pfv + ptri[i].vertindex[0];
        index1 = pfv + ptri[i].vertindex[1];
        index2 = pfv + ptri[i].vertindex[2];

        if (((index0->v[1]-index1->v[1]) *
                (index0->v[0]-index2->v[0]) -
                (index0->v[0]-index1->v[0]) *
                (index0->v[1]-index2->v[1])) >= 0)
        {
            continue;
        }

        d_pcolormap = &((byte *)acolormap)[index0->v[4] & 0xFF00];

        if (ptri[i].facesfront)
        {
            D_PolysetRecursiveTriangle(index0->v, index1->v, index2->v);
        }
        else
        {
            int		s0, s1, s2;

            s0 = index0->v[2];
            s1 = index1->v[2];
            s2 = index2->v[2];

            if (index0->flags & ALIAS_ONSEAM)
                index0->v[2] += r_affinetridesc.seamfixupX16;
            if (index1->flags & ALIAS_ONSEAM)
                index1->v[2] += r_affinetridesc.seamfixupX16;
            if (index2->flags & ALIAS_ONSEAM)
                index2->v[2] += r_affinetridesc.seamfixupX16;

            D_PolysetRecursiveTriangle(index0->v, index1->v, index2->v);

            index0->v[2] = s0;
            index1->v[2] = s1;
            index2->v[2] = s2;
        }
    }
}


/*
================
D_DrawNonSubdiv
================
*/
void D_DrawNonSubdiv_C (void) // Manoel Kasimier - transparencies - edited
{
    mtriangle_t		*ptri;
    finalvert_t		*pfv, *index0, *index1, *index2;
    int				i;
    int				lnumtriangles;

    pfv = r_affinetridesc.pfinalverts;
    ptri = r_affinetridesc.ptriangles;
    lnumtriangles = r_affinetridesc.numtriangles;

    for (i=0 ; i<lnumtriangles ; i++, ptri++)
    {
        index0 = pfv + ptri->vertindex[0];
        index1 = pfv + ptri->vertindex[1];
        index2 = pfv + ptri->vertindex[2];

        d_xdenom = (index0->v[1]-index1->v[1]) *
                   (index0->v[0]-index2->v[0]) -
                   (index0->v[0]-index1->v[0])*(index0->v[1]-index2->v[1]);

        if (d_xdenom >= 0)
        {
            continue;
        }

        r_p0[0] = index0->v[0];		// u
        r_p0[1] = index0->v[1];		// v
        r_p0[2] = index0->v[2];		// s
        r_p0[3] = index0->v[3];		// t
        r_p0[4] = index0->v[4];		// light
        r_p0[5] = index0->v[5];		// iz

        r_p1[0] = index1->v[0];
        r_p1[1] = index1->v[1];
        r_p1[2] = index1->v[2];
        r_p1[3] = index1->v[3];
        r_p1[4] = index1->v[4];
        r_p1[5] = index1->v[5];

        r_p2[0] = index2->v[0];
        r_p2[1] = index2->v[1];
        r_p2[2] = index2->v[2];
        r_p2[3] = index2->v[3];
        r_p2[4] = index2->v[4];
        r_p2[5] = index2->v[5];

        if (!ptri->facesfront)
        {
            if (index0->flags & ALIAS_ONSEAM)
                r_p0[2] += r_affinetridesc.seamfixupX16;
            if (index1->flags & ALIAS_ONSEAM)
                r_p1[2] += r_affinetridesc.seamfixupX16;
            if (index2->flags & ALIAS_ONSEAM)
                r_p2[2] += r_affinetridesc.seamfixupX16;
        }

        D_PolysetSetEdgeTable ();
        D_RasterizeAliasPolySmooth ();
    }
}

// Manoel Kasimier - transparencies

/*
================
D_PolysetRecursiveTriangle
================
*/
void D_PolysetRecursiveTriangle (int *lp1, int *lp2, int *lp3)
{
    int		*temp;
    int		d;
    int		new[6];
    int		z;
    short	*zbuf;

    d = lp2[0] - lp1[0];
    if (d < -1 || d > 1)
        goto split;
    d = lp2[1] - lp1[1];
    if (d < -1 || d > 1)
        goto split;

    d = lp3[0] - lp2[0];
    if (d < -1 || d > 1)
        goto split2;
    d = lp3[1] - lp2[1];
    if (d < -1 || d > 1)
        goto split2;

    d = lp1[0] - lp3[0];
    if (d < -1 || d > 1)
        goto split3;
    d = lp1[1] - lp3[1];
    if (d < -1 || d > 1)
    {
split3:
        temp = lp1;
        lp1 = lp3;
        lp3 = lp2;
        lp2 = temp;

        goto split;
    }

    return;			// entire tri is filled

split2:
    temp = lp1;
    lp1 = lp2;
    lp2 = lp3;
    lp3 = temp;

split:
// split this edge
    new[0] = (lp1[0] + lp2[0]) >> 1;
    new[1] = (lp1[1] + lp2[1]) >> 1;
    new[2] = (lp1[2] + lp2[2]) >> 1;
    new[3] = (lp1[3] + lp2[3]) >> 1;
    new[5] = (lp1[5] + lp2[5]) >> 1;

// draw the point if splitting a leading edge
    if (lp2[1] > lp1[1])
        goto nodraw;
    if ((lp2[1] == lp1[1]) && (lp2[0] < lp1[0]))
        goto nodraw;


    z = new[5]>>16;
    zbuf = zspantable[new[1]] + new[0];
    if (z >= *zbuf)
    {
        int		pix;

        *zbuf = z;
        pix = d_pcolormap[skintable[new[3]>>16][new[2]>>16]];
        d_viewbuffer[d_scantable[new[1]] + new[0]] = pix;
    }

nodraw:
// recursively continue
    D_PolysetRecursiveTriangle (lp3, lp1, new);
    D_PolysetRecursiveTriangle (lp3, new, lp2);
}


/*
================
D_PolysetUpdateTables
================
*/
void D_PolysetUpdateTables (void)
{
    int		i;
    byte	*s;

    if (r_affinetridesc.skinwidth != skinwidth ||
            r_affinetridesc.pskin != skinstart)
    {
        skinwidth = r_affinetridesc.skinwidth;
        skinstart = r_affinetridesc.pskin;
        s = skinstart;
        for (i=0 ; i<MAX_LBM_HEIGHT ; i++, s+=skinwidth)
            skintable[i] = s;
    }
}

/*
===================
D_PolysetScanLeftEdge
====================
*/
void D_PolysetScanLeftEdge_C (int height) // Manoel Kasimier - transparencies - edited
{

    do
    {
        d_pedgespanpackage->pdest = d_pdest;
        d_pedgespanpackage->pz = d_pz;
        d_pedgespanpackage->count = d_aspancount;
        d_pedgespanpackage->ptex = d_ptex;

        d_pedgespanpackage->sfrac = d_sfrac;
        d_pedgespanpackage->tfrac = d_tfrac;

        // FIXME: need to clamp l, s, t, at both ends?
        d_pedgespanpackage->light = d_light;
        d_pedgespanpackage->zi = d_zi;

        d_pedgespanpackage++;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_pdest += d_pdestextrastep;
            d_pz += d_pzextrastep;
            d_aspancount += d_countextrastep;
            d_ptex += d_ptexextrastep;
            d_sfrac += d_sfracextrastep;
            d_ptex += d_sfrac >> 16;

            d_sfrac &= 0xFFFF;
            d_tfrac += d_tfracextrastep;
            if (d_tfrac & 0x10000)
            {
                d_ptex += r_affinetridesc.skinwidth;
                d_tfrac &= 0xFFFF;
            }
            d_light += d_lightextrastep;
            d_zi += d_ziextrastep;
            errorterm -= erroradjustdown;
        }
        else
        {
            d_pdest += d_pdestbasestep;
            d_pz += d_pzbasestep;
            d_aspancount += ubasestep;
            d_ptex += d_ptexbasestep;
            d_sfrac += d_sfracbasestep;
            d_ptex += d_sfrac >> 16;
            d_sfrac &= 0xFFFF;
            d_tfrac += d_tfracbasestep;
            if (d_tfrac & 0x10000)
            {
                d_ptex += r_affinetridesc.skinwidth;
                d_tfrac &= 0xFFFF;
            }
            d_light += d_lightbasestep;
            d_zi += d_zibasestep;
        }
    }
    while (--height);
}


/*
===================
D_PolysetSetUpForLineScan
====================
*/
void D_PolysetSetUpForLineScan(fixed8_t startvertu, fixed8_t startvertv,
                               fixed8_t endvertu, fixed8_t endvertv)
{
    double		dm, dn;
    int			tm, tn;
    adivtab_t	*ptemp;

// TODO: implement x86 version

    errorterm = -1;

    tm = endvertu - startvertu;
    tn = endvertv - startvertv;

    if (((tm <= 16) && (tm >= -15)) &&
            ((tn <= 16) && (tn >= -15)))
    {
        ptemp = &adivtab[((tm+15) << 5) + (tn+15)];
        ubasestep = ptemp->quotient;
        erroradjustup = ptemp->remainder;
        erroradjustdown = tn;
    }
    else
    {
        dm = (double)tm;
        dn = (double)tn;

        FloorDivMod (dm, dn, &ubasestep, &erroradjustup);

        erroradjustdown = dn;
    }
}

/*
================
D_PolysetCalcGradients
================
*/
void D_PolysetCalcGradients_C (int skinwidth) // Manoel Kasimier - transparencies - edited
{
    float	xstepdenominv, ystepdenominv, t0, t1;
    float	p01_minus_p21, p11_minus_p21, p00_minus_p20, p10_minus_p20;

    p00_minus_p20 = r_p0[0] - r_p2[0];
    p01_minus_p21 = r_p0[1] - r_p2[1];
    p10_minus_p20 = r_p1[0] - r_p2[0];
    p11_minus_p21 = r_p1[1] - r_p2[1];

    xstepdenominv = 1.0 / (float)d_xdenom;

    ystepdenominv = -xstepdenominv;

// ceil () for light so positive steps are exaggerated, negative steps
// diminished,  pushing us away from underflow toward overflow. Underflow is
// very visible, overflow is very unlikely, because of ambient lighting
    t0 = r_p0[4] - r_p2[4];
    t1 = r_p1[4] - r_p2[4];
    r_lstepx = (int)
               ceil((t1 * p01_minus_p21 - t0 * p11_minus_p21) * xstepdenominv);
    r_lstepy = (int)
               ceil((t1 * p00_minus_p20 - t0 * p10_minus_p20) * ystepdenominv);

    t0 = r_p0[2] - r_p2[2];
    t1 = r_p1[2] - r_p2[2];
    r_sstepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
                     xstepdenominv);
    r_sstepy = (int)((t1 * p00_minus_p20 - t0* p10_minus_p20) *
                     ystepdenominv);

    t0 = r_p0[3] - r_p2[3];
    t1 = r_p1[3] - r_p2[3];
    r_tstepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
                     xstepdenominv);
    r_tstepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
                     ystepdenominv);

    t0 = r_p0[5] - r_p2[5];
    t1 = r_p1[5] - r_p2[5];
    r_zistepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
                      xstepdenominv);
    r_zistepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
                      ystepdenominv);

    a_sstepxfrac = r_sstepx & 0xFFFF;
    a_tstepxfrac = r_tstepx & 0xFFFF;
    a_ststepxwhole = skinwidth * (r_tstepx >> 16) + (r_sstepx >> 16);
}



#if 0 //qbism 1 for gelmap test
byte gelmap[256];
void InitGel (byte *palette)
{
    int		i;
    int		r;

    for (i=0 ; i<256 ; i++)
    {
//		r = (palette[i*3]>>4);
        r = (palette[i*3] + palette[i*3+1] + palette[i*3+2])/(16*3);
        gelmap[i] = /* 64 */ 0 + r;
    }
}
#endif


/*
================
D_PolysetDrawSpans8
================
*/
void D_PolysetDrawSpans8 (spanpackage_t *pspanpackage)
{
    int		lcount;
    byte	*lpdest;
    byte	*lptex;
    int		lsfrac, ltfrac;
    int		llight;
    int		lzi;
    short	*lpz;

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
        {
            d_aspancount += ubasestep;
        }

        if (lcount)
        {
            lpdest = pspanpackage->pdest;
            lptex = pspanpackage->ptex;
            lpz = pspanpackage->pz;
            lsfrac = pspanpackage->sfrac;
            ltfrac = pspanpackage->tfrac;
            llight = pspanpackage->light;
            lzi = pspanpackage->zi;

            do
            {
                if ((lzi >> 16) >= *lpz)
                    if (*lptex != 255) // Manoel Kasimier - transparent pixels in alias models
                    {
                        *lpdest = ((byte *)acolormap)[*lptex + (llight & 0xFF00)];
                        //*lpdest = gelmap[*lpdest]; // gel mapping
                        *lpz = lzi >> 16;
                    }
                lpdest++;
                lzi += r_zistepx;
                lpz++;
                llight += r_lstepx;
                lptex += a_ststepxwhole;
                lsfrac += a_sstepxfrac;
                lptex += lsfrac >> 16;
                lsfrac &= 0xFFFF;
                ltfrac += a_tstepxfrac;
                if (ltfrac & 0x10000)
                {
                    lptex += r_affinetridesc.skinwidth;
                    ltfrac &= 0xFFFF;
                }
            }
            while (--lcount);
        }

        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}

// 66% transparency
void D_PolysetDrawSpans8_66 (spanpackage_t *pspanpackage) // Manoel Kasimier - transparencies - edited
{
    int		lcount;
    byte	*lpdest;
    byte	*lptex;
    int		lsfrac, ltfrac;
    int		llight;
    int		lzi;
    short	*lpz;

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
        {
            d_aspancount += ubasestep;
        }

        if (lcount)
        {
            lpdest = pspanpackage->pdest;
            lptex = pspanpackage->ptex;
            lpz = pspanpackage->pz;
            lsfrac = pspanpackage->sfrac;
            ltfrac = pspanpackage->tfrac;
            llight = pspanpackage->light;
            lzi = pspanpackage->zi;

            do
            {
                if ((lzi >> 16) >= *lpz)
                    if (*lptex != 255) // Manoel Kasimier - transparent pixels in alias models
                    {
                        // Manoel Kasimier - transparencies - begin
#define temp ((byte *)acolormap)[*lptex + (llight & 0xFF00)]
                        *lpdest = alphamap[*lpdest + temp*256];
                        // Manoel Kasimier - transparencies - end
                    }
                lpdest++;
                lzi += r_zistepx;
                lpz++;
                llight += r_lstepx;
                lptex += a_ststepxwhole;
                lsfrac += a_sstepxfrac;
                lptex += lsfrac >> 16;
                lsfrac &= 0xFFFF;
                ltfrac += a_tstepxfrac;
                if (ltfrac & 0x10000)
                {
                    lptex += r_affinetridesc.skinwidth;
                    ltfrac &= 0xFFFF;
                }
            }
            while (--lcount);
        }

        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}
// 33% transparency
void D_PolysetDrawSpans8_33 (spanpackage_t *pspanpackage) // Manoel Kasimier - transparencies - edited
{
    int		lcount;
    byte	*lpdest;
    byte	*lptex;
    int		lsfrac, ltfrac;
    int		llight;
    int		lzi;
    short	*lpz;

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
        {
            d_aspancount += ubasestep;
        }

        if (lcount)
        {
            lpdest = pspanpackage->pdest;
            lptex = pspanpackage->ptex;
            lpz = pspanpackage->pz;
            lsfrac = pspanpackage->sfrac;
            ltfrac = pspanpackage->tfrac;
            llight = pspanpackage->light;
            lzi = pspanpackage->zi;

            do
            {
                if ((lzi >> 16) >= *lpz)
                    if (*lptex != 255) // Manoel Kasimier - transparent pixels in alias models
                    {
                        // Manoel Kasimier - transparencies - begin
#define temp ((byte *)acolormap)[*lptex + (llight & 0xFF00)]
                        *lpdest = alphamap[temp + *lpdest*256];
                        // Manoel Kasimier - transparencies - end
                    }
                lpdest++;
                lzi += r_zistepx;
                lpz++;
                llight += r_lstepx;
                lptex += a_ststepxwhole;
                lsfrac += a_sstepxfrac;
                lptex += lsfrac >> 16;
                lsfrac &= 0xFFFF;
                ltfrac += a_tstepxfrac;
                if (ltfrac & 0x10000)
                {
                    lptex += r_affinetridesc.skinwidth;
                    ltfrac &= 0xFFFF;
                }
            }
            while (--lcount);
        }

        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}


// stippled alpha (50%)
void D_PolysetDrawSpans8_Stippled (spanpackage_t *pspanpackage) // Manoel Kasimier - transparencies - edited
{
    int		lcount;
    byte	*lpdest;
    byte	*lptex;
    int		lsfrac, ltfrac;
    int		llight;
    int		lzi;
    short	*lpz;

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
        {
            d_aspancount += ubasestep;
        }

        if (lcount)
        {
            int		teste = ( ((int)pspanpackage->pdest-(int)d_viewbuffer) / screenwidth) & 1; // Manoel Kasimier - stipple alpha
            lpdest = pspanpackage->pdest;
            lptex = pspanpackage->ptex;
            lpz = pspanpackage->pz;
            lsfrac = pspanpackage->sfrac;
            ltfrac = pspanpackage->tfrac;
            llight = pspanpackage->light;
            lzi = pspanpackage->zi;

            do
            {
                if ((lzi >> 16) >= *lpz)
                    if (*lptex != 255) // Manoel Kasimier - transparent pixels in alias models
                        // Manoel Kasimier - stipple alpha - begin
                        if (((int)lpdest + teste) & 1)
                        {
                            *lpdest = ((byte *)acolormap)[*lptex + (llight & 0xFF00)];
                            *lpz = lzi >> 16;
                        }
                // Manoel Kasimier - stipple alpha - end
                lpdest++;
                lzi += r_zistepx;
                lpz++;
                llight += r_lstepx;
                lptex += a_ststepxwhole;
                lsfrac += a_sstepxfrac;
                lptex += lsfrac >> 16;
                lsfrac &= 0xFFFF;
                ltfrac += a_tstepxfrac;
                if (ltfrac & 0x10000)
                {
                    lptex += r_affinetridesc.skinwidth;
                    ltfrac &= 0xFFFF;
                }
            }
            while (--lcount);
        }

        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}

// additive blending
void D_PolysetDrawSpans8_Add (spanpackage_t *pspanpackage) // Manoel Kasimier - transparencies - edited
{
    int		lcount;
    byte	*lpdest;
    byte	*lptex;
    int		lsfrac, ltfrac;
    int		llight;
    int		lzi;
    short	*lpz;

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
        {
            d_aspancount += ubasestep;
        }

        if (lcount)
        {
            lpdest = pspanpackage->pdest;
            lptex = pspanpackage->ptex;
            lpz = pspanpackage->pz;
            lsfrac = pspanpackage->sfrac;
            ltfrac = pspanpackage->tfrac;
            llight = pspanpackage->light;
            lzi = pspanpackage->zi;

            do
            {
                if ((lzi >> 16) >= *lpz)
                    if (*lptex != 255) // Manoel Kasimier - transparent pixels in alias models
                    {
                        // Manoel Kasimier - transparencies - begin
#define temp ((byte *)acolormap)[*lptex + (llight & 0xFF00)]
                        *lpdest = additivemap[temp + *lpdest*256];
                        // Manoel Kasimier - transparencies - end
                    }
                lpdest++;
                lzi += r_zistepx;
                lpz++;
                llight += r_lstepx;
                lptex += a_ststepxwhole;
                lsfrac += a_sstepxfrac;
                lptex += lsfrac >> 16;
                lsfrac &= 0xFFFF;
                ltfrac += a_tstepxfrac;
                if (ltfrac & 0x10000)
                {
                    lptex += r_affinetridesc.skinwidth;
                    ltfrac &= 0xFFFF;
                }
            }
            while (--lcount);
        }

        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}
// shadow blending
void D_PolysetDrawSpans8_Shadow (spanpackage_t *pspanpackage) // Manoel Kasimier - EF_SHADOW - edited
{
    int		lcount;
    byte	*lpdest;
    int		lzi;
    short	*lpz;
    // Manoel Kasimier - begin
    int		dist;
    byte	*shadowmap;
    shadowmap = vid.colormap + ((32 + (int)(31.0*ENTALPHA_DECODE(currententity->alpha)) ) * 256);
    // Manoel Kasimier - end

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
            d_aspancount += ubasestep;

        if (lcount)
        {
            lpdest = pspanpackage->pdest;
            lpz = pspanpackage->pz;
            lzi = pspanpackage->zi;

            do
            {
                // Manoel Kasimier - begin
                dist = (lzi >> 16) - *lpz;
                // faster, looks worse
                //	if (dist >= 0)
                //		*lpdest = 0;
                // slower, looks better:
                if (dist >= 0 && dist < 8)
                    *lpdest = shadowmap[*lpdest];
                // Manoel Kasimier - end
                lpdest++;
                lzi += r_zistepx;
                lpz++;
            }
            while (--lcount);
        }

        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}

// Manoel Kasimier - EF_CELSHADING - begin
byte D_PolysetDraw_OutlinePixel_100 (byte lpdest, byte pixel)
{
//	return (byte)vid.colormap[pixel + ((32 + (int)(31.0*0.7) ) * 256)];
    return alphamap[pixel];
}
byte D_PolysetDraw_OutlinePixel_66 (byte lpdest, byte pixel)
{
//	return alphamap[lpdest + ((byte)vid.colormap[pixel + ((32 + (int)(31.0*0.7) ) * 256)])*256];
    return alphamap[lpdest + alphamap[pixel]*256];
}
byte D_PolysetDraw_OutlinePixel_33 (byte lpdest, byte pixel)
{
//	return alphamap[((byte)vid.colormap[pixel + ((32 + (int)(31.0*0.5) ) * 256)]) + lpdest*256];
    return alphamap[alphamap[pixel] + lpdest*256];
}
void D_PolysetDrawSpans8_Outline (spanpackage_t *pspanpackage)
{
    /*
    preencher o pixel 5 vezes, desse jeito:
     #
    ###
     #

    preencher pixels superiores/inferiores nesses casos:
    L = lateral
    A = acima/abaixo
           AAA
        AAL---LA
     AAL--------LA
    L-------------L
     AL--------LAA
       AL---LAA
         AAA

    sempre repetir a primeira e a última linha integralmente.
    ? fazer uma união dos conjuntos da primeira linha e da segunda linha, pra saber o comprimento máximo da linha a ser desenhada
    */
    int		lcount;
    byte	*lpdest;
    int		lzi;
    short	*lpz;
    int		firstline = true;
//	byte	*prevlpdest; // previous
    unsigned int	prevbegin=0, prevend=0, nextbegin, nextend; // previous
#if 0
    byte	(*DrawPixel)(byte, byte);
    if (r_drawoutlines == 1)
        DrawPixel = D_PolysetDraw_OutlinePixel_100;
    else if (r_drawoutlines == 6)
        DrawPixel = D_PolysetDraw_OutlinePixel_66;
    else /*if (r_drawoutlines == 3)*/
        DrawPixel = D_PolysetDraw_OutlinePixel_33;
#endif

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
            d_aspancount += ubasestep;

        if (lcount)
        {
            lpdest = pspanpackage->pdest;
            lpz = pspanpackage->pz;
            lzi = pspanpackage->zi;

            // one more pixel on the left side
            if ((lzi >> 16) > lpz[-1])
                lpdest[-1] = alphamap[*lpdest];//DrawPixel(lpdest[-1], *lpdest);
            // pixels on the top
            if (firstline)
            {
                // é possível o polígono ter uma linha só, ou seja, é possível (firstline == lastline),
                // então nesse caso é necessário desenhar tanto acima quanto abaixo.
                if (lpdest - vid.width >= vid.buffer)//(vid.buffer + vid.width*r_refdef.vrect.y + r_refdef.vrect.x))
                    do
                    {
                        if ((lzi >> 16) > lpz[-(signed int)(vid.width)])
                            lpdest[-(signed int)(vid.width)] = alphamap[*lpdest];//DrawPixel(lpdest[-vid.width], *lpdest);
                        // checar se a última linha sai pra fora da tela
                        //	if (lpdest + vid.width < (vid.buffer + vid.width*(r_refdef.vrect.y+r_refdef.vrect.height) - (vid.width-(r_refdef.vrect.x+r_refdef.vrect.width)) ))
                        if (pspanpackage[1].count == -999999) // last line
                            if ((lzi >> 16) > lpz[vid.width])
                                lpdest[vid.width] = alphamap[*lpdest];//DrawPixel(lpdest[vid.width], *lpdest);
                        lpdest++;
                        lzi += r_zistepx;
                        lpz++;
                    }
                    while (--lcount);
            }
            // pixels on the bottom
            else if (pspanpackage[1].count == -999999) // last line
            {
                //	if (lpdest + vid.width < (vid.buffer + vid.width*vid.height - vid.width))
                //	(r_refdef.vrect.y+r_refdef.vrect.height) -
                //	(vid.width-(r_refdef.vrect.x+r_refdef.vrect.width)) ))
                do
                {
                    if ((lzi >> 16) > lpz[vid.width])
                        lpdest[vid.width] = alphamap[*lpdest];//DrawPixel(lpdest[vid.width], *lpdest);
                    lpdest++;
                    lzi += r_zistepx;
                    lpz++;
                }
                while (--lcount);
            }
            // pixels on the center
            else
            {
                nextbegin = (unsigned int)pspanpackage[1].pdest - vid.width - 1;
                nextend = nextbegin + (unsigned int)(d_aspancount - pspanpackage[1].count) + 1;
                do
                {
                    // se o pixel atual começar  antes  do de cima, desenhar ele acima.
                    if ((unsigned int)lpdest < prevbegin)
                    {
                        if ((lzi >> 16) > lpz[-(signed int)(vid.width)])
                            lpdest[-(signed int)(vid.width)] = alphamap[*lpdest];//DrawPixel(lpdest[-vid.width], *lpdest);
                    }
                    // se o pixel atual terminar depois do de cima, desenhar ele acima.
                    else if ((unsigned int)lpdest > prevend)
                    {
                        if ((lzi >> 16) > lpz[-(signed int)(vid.width)])
                            lpdest[-(signed int)(vid.width)] = alphamap[*lpdest];//DrawPixel(lpdest[-vid.width], *lpdest);
                    }
                    // se o pixel atual começar  antes  do de baixo, desenhar ele abaixo.
                    if ((unsigned int)lpdest < nextbegin)
                    {
                        if ((lzi >> 16) > lpz[vid.width])
                            lpdest[vid.width] = alphamap[*lpdest];//DrawPixel(lpdest[vid.width], *lpdest);
                    }
                    // se o pixel atual terminar depois do de baixo, desenhar ele abaixo.
                    else if ((unsigned int)lpdest > nextend)
                    {
                        if ((lzi >> 16) > lpz[vid.width])
                            lpdest[vid.width] = alphamap[*lpdest];//DrawPixel(lpdest[vid.width], *lpdest);
                    }
                    lpdest++;
                    lzi += r_zistepx;
                    lpz++;
                }
                while (--lcount);
            }
            // one more pixel on the right side
            if (((lzi-r_zistepx) >> 16) > *lpz)
                *lpdest = alphamap[lpdest[-1]];//DrawPixel(*lpdest, lpdest[-1]);
            firstline = false;
        }

        prevbegin = ((unsigned int)pspanpackage->pdest + vid.width) - 1;
        if ((errorterm+erroradjustdown) >= 0)
            prevend = prevbegin + 1 + (unsigned int)(d_aspancount - d_countextrastep - pspanpackage->count);
        else
            prevend = prevbegin + 1 + (unsigned int)(d_aspancount - ubasestep - pspanpackage->count);
        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}

void D_PolysetDrawSpans8_Cel (spanpackage_t *pspanpackage)
{
    int		lcount;
    byte	*lpdest;
    byte	*lptex;
    int		lsfrac, ltfrac;
    int		llight;
    int		lzi;
    short	*lpz;

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
        {
            d_aspancount += ubasestep;
        }

        if (lcount)
        {
            lpdest = pspanpackage->pdest;
            lptex = pspanpackage->ptex;
            lpz = pspanpackage->pz;
            lsfrac = pspanpackage->sfrac;
            ltfrac = pspanpackage->tfrac;
            llight = pspanpackage->light;
            lzi = pspanpackage->zi;

            do
            {
                if ((lzi >> 16) >= *lpz)
                    if (*lptex != 255) // Manoel Kasimier - transparent pixels in alias models
                    {
                        *lpdest = ((byte *)acolormap)[*lptex + (llight & 0xFF00)];
                        //	*lpdest = ((byte *)acolormap)[*lptex + ((llight/16) & 0xFF00)*16];
                        *lpz = lzi >> 16;
                    }
                lpdest++;
                lzi += r_zistepx;
                lpz++;
                llight += r_lstepx;
                lptex += a_ststepxwhole;
                lsfrac += a_sstepxfrac;
                lptex += lsfrac >> 16;
                lsfrac &= 0xFFFF;
                ltfrac += a_tstepxfrac;
                if (ltfrac & 0x10000)
                {
                    lptex += r_affinetridesc.skinwidth;
                    ltfrac &= 0xFFFF;
                }
            }
            while (--lcount);
        }

        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}
// Manoel Kasimier - EF_CELSHADING - end

// Manoel Kasimier - EF_REFLECTIVE - begin
void D_PolysetDrawSpans8_Metal (spanpackage_t *pspanpackage)
{
    int		lcount;
    byte	*lpdest;
    byte	*lptex;
    int		lsfrac, ltfrac;
    int		llight;
    int		lzi;
    short	*lpz;

    do
    {
        lcount = d_aspancount - pspanpackage->count;

        errorterm += erroradjustup;
        if (errorterm >= 0)
        {
            d_aspancount += d_countextrastep;
            errorterm -= erroradjustdown;
        }
        else
        {
            d_aspancount += ubasestep;
        }

        if (lcount)
        {
            lpdest = pspanpackage->pdest;
            lptex = pspanpackage->ptex;
            lpz = pspanpackage->pz;
            lsfrac = pspanpackage->sfrac;
            ltfrac = pspanpackage->tfrac;
            llight = pspanpackage->light;
            lzi = pspanpackage->zi;

            do
            {
                if ((lzi >> 16) >= *lpz)
                    if (*lptex != 255) // Manoel Kasimier - transparent pixels in alias models
                    {
                        *lpdest = ((byte *)acolormap)[*lptex + (llight & 0xAA00)]; //metal
                        *lpz = lzi >> 16;
                    }
                lpdest++;
                lzi += r_zistepx;
                lpz++;
                llight += r_lstepx;
                lptex += a_ststepxwhole;
                lsfrac += a_sstepxfrac;
                lptex += lsfrac >> 16;
                lsfrac &= 0xFFFF;
                ltfrac += a_tstepxfrac;
                if (ltfrac & 0x10000)
                {
                    lptex += r_affinetridesc.skinwidth;
                    ltfrac &= 0xFFFF;
                }
            }
            while (--lcount);
        }

        pspanpackage++;
    }
    while (pspanpackage->count != -999999);
}
// Manoel Kasimier - EF_REFLECTIVE - end

/*
================
D_PolysetFillSpans8
================
*/
void D_PolysetFillSpans8 (spanpackage_t *pspanpackage)
{
    int				color;

// FIXME: do z buffering

    color = d_aflatcolor++;

    while (1)
    {
        int		lcount;
        byte	*lpdest;

        lcount = pspanpackage->count;

        if (lcount == -1)
            return;

        if (lcount)
        {
            lpdest = pspanpackage->pdest;

            do
            {
                *lpdest++ = color;
            }
            while (--lcount);
        }

        pspanpackage++;
    }
}

/*
================
D_RasterizeAliasPolySmooth
================
*/
extern cvar_t sw_stipplealpha; // Manoel Kasimier
void D_RasterizeAliasPolySmooth (void)
{
    int				initialleftheight, initialrightheight;
    int				*plefttop, *prighttop, *pleftbottom, *prightbottom;
    int				working_lstepx, originalcount;

    plefttop = pedgetable->pleftedgevert0;
    prighttop = pedgetable->prightedgevert0;

    pleftbottom = pedgetable->pleftedgevert1;
    prightbottom = pedgetable->prightedgevert1;

    initialleftheight = pleftbottom[1] - plefttop[1];
    initialrightheight = prightbottom[1] - prighttop[1];

//
// set the s, t, and light gradients, which are consistent across the triangle
// because being a triangle, things are affine
//
//	D_PolysetCalcGradients (r_affinetridesc.skinwidth); // Manoel Kasimier - transparencies - removed
    // Manoel Kasimier - transparencies - begin
    D_PolysetCalcGradients_C (r_affinetridesc.skinwidth);
    // Manoel Kasimier - transparencies - end

//
// rasterize the polygon
//

//
// scan out the top (and possibly only) part of the left edge
//
    d_pedgespanpackage = a_spans;

    ystart = plefttop[1];
    d_aspancount = plefttop[0] - prighttop[0];

    d_ptex = (byte *)r_affinetridesc.pskin + (plefttop[2] >> 16) +
             (plefttop[3] >> 16) * r_affinetridesc.skinwidth;
    {
        d_sfrac = plefttop[2] & 0xFFFF;
        d_tfrac = plefttop[3] & 0xFFFF;
    }
    d_light = plefttop[4];
    d_zi = plefttop[5];

    d_pdest = (byte *)d_viewbuffer +
              ystart * screenwidth + plefttop[0];
    d_pz = d_pzbuffer + ystart * d_zwidth + plefttop[0];

    if (initialleftheight == 1)
    {
        d_pedgespanpackage->pdest = d_pdest;
        d_pedgespanpackage->pz = d_pz;
        d_pedgespanpackage->count = d_aspancount;
        d_pedgespanpackage->ptex = d_ptex;

        d_pedgespanpackage->sfrac = d_sfrac;
        d_pedgespanpackage->tfrac = d_tfrac;

        // FIXME: need to clamp l, s, t, at both ends?
        d_pedgespanpackage->light = d_light;
        d_pedgespanpackage->zi = d_zi;

        d_pedgespanpackage++;
    }
    else
    {
        D_PolysetSetUpForLineScan(plefttop[0], plefttop[1],
                                  pleftbottom[0], pleftbottom[1]);

        {
            d_pzbasestep = d_zwidth + ubasestep;
            d_pzextrastep = d_pzbasestep + 1;
        }

        d_pdestbasestep = screenwidth + ubasestep;
        d_pdestextrastep = d_pdestbasestep + 1;

        // TODO: can reuse partial expressions here

        // for negative steps in x along left edge, bias toward overflow rather than
        // underflow (sort of turning the floor () we did in the gradient calcs into
        // ceil (), but plus a little bit)
        if (ubasestep < 0)
            working_lstepx = r_lstepx - 1;
        else
            working_lstepx = r_lstepx;

        d_countextrastep = ubasestep + 1;
        d_ptexbasestep = ((r_sstepy + r_sstepx * ubasestep) >> 16) +
                         ((r_tstepy + r_tstepx * ubasestep) >> 16) *
                         r_affinetridesc.skinwidth;
        {
            d_sfracbasestep = (r_sstepy + r_sstepx * ubasestep) & 0xFFFF;
            d_tfracbasestep = (r_tstepy + r_tstepx * ubasestep) & 0xFFFF;
        }
        d_lightbasestep = r_lstepy + working_lstepx * ubasestep;
        d_zibasestep = r_zistepy + r_zistepx * ubasestep;

        d_ptexextrastep = ((r_sstepy + r_sstepx * d_countextrastep) >> 16) +
                          ((r_tstepy + r_tstepx * d_countextrastep) >> 16) *
                          r_affinetridesc.skinwidth;
        {
            d_sfracextrastep = (r_sstepy + r_sstepx*d_countextrastep) & 0xFFFF;
            d_tfracextrastep = (r_tstepy + r_tstepx*d_countextrastep) & 0xFFFF;
        }
        d_lightextrastep = d_lightbasestep + working_lstepx;
        d_ziextrastep = d_zibasestep + r_zistepx;
        D_PolysetScanLeftEdge_C (initialleftheight);
        // Manoel Kasimier - transparencies - end
    }

//
// scan out the bottom part of the left edge, if it exists
//
    if (pedgetable->numleftedges == 2)
    {
        int		height;

        plefttop = pleftbottom;
        pleftbottom = pedgetable->pleftedgevert2;

        height = pleftbottom[1] - plefttop[1];

// TODO: make this a function; modularize this function in general

        ystart = plefttop[1];
        d_aspancount = plefttop[0] - prighttop[0];
        d_ptex = (byte *)r_affinetridesc.pskin + (plefttop[2] >> 16) +
                 (plefttop[3] >> 16) * r_affinetridesc.skinwidth;
        d_sfrac = 0;
        d_tfrac = 0;
        d_light = plefttop[4];
        d_zi = plefttop[5];

        d_pdest = (byte *)d_viewbuffer + ystart * screenwidth + plefttop[0];
        d_pz = d_pzbuffer + ystart * d_zwidth + plefttop[0];

        if (height == 1)
        {
            d_pedgespanpackage->pdest = d_pdest;
            d_pedgespanpackage->pz = d_pz;
            d_pedgespanpackage->count = d_aspancount;
            d_pedgespanpackage->ptex = d_ptex;

            d_pedgespanpackage->sfrac = d_sfrac;
            d_pedgespanpackage->tfrac = d_tfrac;

            // FIXME: need to clamp l, s, t, at both ends?
            d_pedgespanpackage->light = d_light;
            d_pedgespanpackage->zi = d_zi;

            d_pedgespanpackage++;
        }
        else
        {
            D_PolysetSetUpForLineScan(plefttop[0], plefttop[1],
                                      pleftbottom[0], pleftbottom[1]);

            d_pdestbasestep = screenwidth + ubasestep;
            d_pdestextrastep = d_pdestbasestep + 1;
            {
                d_pzbasestep = d_zwidth + ubasestep;
                d_pzextrastep = d_pzbasestep + 1;
            }

            if (ubasestep < 0)
                working_lstepx = r_lstepx - 1;
            else
                working_lstepx = r_lstepx;

            d_countextrastep = ubasestep + 1;
            d_ptexbasestep = ((r_sstepy + r_sstepx * ubasestep) >> 16) +
                             ((r_tstepy + r_tstepx * ubasestep) >> 16) *
                             r_affinetridesc.skinwidth;
            {
                d_sfracbasestep = (r_sstepy + r_sstepx * ubasestep) & 0xFFFF;
                d_tfracbasestep = (r_tstepy + r_tstepx * ubasestep) & 0xFFFF;
            }
            d_lightbasestep = r_lstepy + working_lstepx * ubasestep;
            d_zibasestep = r_zistepy + r_zistepx * ubasestep;

            d_ptexextrastep = ((r_sstepy + r_sstepx * d_countextrastep) >> 16) +
                              ((r_tstepy + r_tstepx * d_countextrastep) >> 16) *
                              r_affinetridesc.skinwidth;
            {
                d_sfracextrastep = (r_sstepy+r_sstepx*d_countextrastep) & 0xFFFF;
                d_tfracextrastep = (r_tstepy+r_tstepx*d_countextrastep) & 0xFFFF;
            }
            d_lightextrastep = d_lightbasestep + working_lstepx;
            d_ziextrastep = d_zibasestep + r_zistepx;
            D_PolysetScanLeftEdge_C (height);
            // Manoel Kasimier - transparencies - end
        }
    }

// scan out the top (and possibly only) part of the right edge, updating the
// count field
    d_pedgespanpackage = a_spans;

    D_PolysetSetUpForLineScan(prighttop[0], prighttop[1],
                              prightbottom[0], prightbottom[1]);
    d_aspancount = 0;
    d_countextrastep = ubasestep + 1;
    originalcount = a_spans[initialrightheight].count;
    a_spans[initialrightheight].count = -999999; // mark end of the spanpackages

    // Manoel Kasimier - EF_REFLECTIVE - begin
    if (currententity->effects & EF_REFLECTIVE && !r_drawoutlines)
        D_PolysetDrawSpans8_Metal (a_spans);
    // Manoel Kasimier - EF_REFLECTIVE - end
    // Manoel Kasimier - EF_CELSHADING - begin
    else if (currententity->effects & EF_CELSHADING)
    {
        if (r_drawoutlines)
            D_PolysetDrawSpans8_Outline (a_spans);
        else
        {
            celtri_drawn = true;
            D_PolysetDrawSpans8_Cel (a_spans);
        }
    }
    else
        // Manoel Kasimier - EF_CELSHADING - end
        // Manoel Kasimier - EF_ADDITIVE - begin
        if (currententity->effects & EF_ADDITIVE && additivemap)
            D_PolysetDrawSpans8_Add (a_spans);
    // Manoel Kasimier - EF_ADDITIVE - end
    // Manoel Kasimier - EF_SHADOW - begin
        else if (currententity->effects & EF_SHADOW)
            D_PolysetDrawSpans8_Shadow (a_spans);
    // Manoel Kasimier - EF_SHADOW - end
        else if (currententity->alpha == ENTALPHA_DEFAULT) // Manoel Kasimier - transparencies
            D_PolysetDrawSpans8 (a_spans);
    // Manoel Kasimier - transparencies - begin
        else if (!alphamap || sw_stipplealpha.value)
            D_PolysetDrawSpans8_Stippled (a_spans);
        else if (ENTALPHA_DECODE(currententity->alpha) < 0.5)
            D_PolysetDrawSpans8_33 (a_spans);
        else
            D_PolysetDrawSpans8_66 (a_spans);
    // Manoel Kasimier - transparencies - end

// scan out the bottom part of the right edge, if it exists
    if (pedgetable->numrightedges == 2)
    {
        int				height;
        spanpackage_t	*pstart;

        pstart = a_spans + initialrightheight;
        pstart->count = originalcount;

        d_aspancount = prightbottom[0] - prighttop[0];

        prighttop = prightbottom;
        prightbottom = pedgetable->prightedgevert2;

        height = prightbottom[1] - prighttop[1];

        D_PolysetSetUpForLineScan(prighttop[0], prighttop[1],
                                  prightbottom[0], prightbottom[1]);

        d_countextrastep = ubasestep + 1;
        a_spans[initialrightheight + height].count = -999999;
        // mark end of the spanpackages

        // Manoel Kasimier - EF_REFLECTIVE - begin
        if (currententity->effects & EF_REFLECTIVE && !r_drawoutlines)
            D_PolysetDrawSpans8_Metal (pstart);
        // Manoel Kasimier - EF_REFLECTIVE - end
        // Manoel Kasimier - EF_CELSHADING - begin
        else if (currententity->effects & EF_CELSHADING)
        {
            if (r_drawoutlines)
                D_PolysetDrawSpans8_Outline (pstart);
            else
                D_PolysetDrawSpans8_Cel (pstart);
        }
        else
            // Manoel Kasimier - EF_CELSHADING - end
            // Manoel Kasimier - EF_ADDITIVE - begin
            if (currententity->effects & EF_ADDITIVE && additivemap)
                D_PolysetDrawSpans8_Add (pstart);
        // Manoel Kasimier - EF_ADDITIVE - end
        // Manoel Kasimier - EF_SHADOW - begin
            else if (currententity->effects & EF_SHADOW)
                D_PolysetDrawSpans8_Shadow (pstart);
        // Manoel Kasimier - EF_SHADOW - end
            else if (currententity->alpha == ENTALPHA_DEFAULT) // Manoel Kasimier - transparencies
                D_PolysetDrawSpans8 (pstart);
        // Manoel Kasimier - transparencies - begin
            else if (!alphamap || sw_stipplealpha.value)
                D_PolysetDrawSpans8_Stippled (pstart);
            else if (ENTALPHA_DECODE(currententity->alpha) < 0.5f)
                D_PolysetDrawSpans8_33 (pstart);
            else
                D_PolysetDrawSpans8_66 (pstart);
        // Manoel Kasimier - transparencies - end
    }
}


/*
================
D_PolysetSetEdgeTable
================
*/
void D_PolysetSetEdgeTable (void)
{
    int			edgetableindex;

    edgetableindex = 0;	// assume the vertices are already in
    //  top to bottom order

//
// determine which edges are right & left, and the order in which
// to rasterize them
//
    if (r_p0[1] >= r_p1[1])
    {
        if (r_p0[1] == r_p1[1])
        {
            if (r_p0[1] < r_p2[1])
                pedgetable = &edgetables[2];
            else
                pedgetable = &edgetables[5];

            return;
        }
        else
        {
            edgetableindex = 1;
        }
    }

    if (r_p0[1] == r_p2[1])
    {
        if (edgetableindex)
            pedgetable = &edgetables[8];
        else
            pedgetable = &edgetables[9];

        return;
    }
    else if (r_p1[1] == r_p2[1])
    {
        if (edgetableindex)
            pedgetable = &edgetables[10];
        else
            pedgetable = &edgetables[11];

        return;
    }

    if (r_p0[1] > r_p2[1])
        edgetableindex += 2;

    if (r_p1[1] > r_p2[1])
        edgetableindex += 4;

    pedgetable = &edgetables[edgetableindex];
}
