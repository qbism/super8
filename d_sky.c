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
// d_sky.c

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"

#define SKY_SPAN_SHIFT	5
#define SKY_SPAN_MAX	(1 << SKY_SPAN_SHIFT)

static float	timespeed1, timespeed2; // Manoel Kasimier - smooth sky

/*
=================
D_Sky_uv_To_st
=================
*/
void D_Sky_uv_To_st (int u, int v, fixed16_t *s, fixed16_t *t
					, fixed16_t *s2, fixed16_t *t2) // Manoel Kasimier - smooth sky
{
	float	wu, wv;//, temp; // Manoel Kasimier - edited
	vec3_t	end;

	// ToChriS - begin
	wu = (u - xcenter) / xscale;
	wv = (ycenter - v) / yscale;

	end[0] = vpn[0] + wu*vright[0] + wv*vup[0];
	end[1] = vpn[1] + wu*vright[1] + wv*vup[1];
	end[2] = vpn[2] + wu*vright[2] + wv*vup[2];
	// ToChriS - end
	end[2] *= 3;
	VectorNormalize (end);
	*s = (int)((timespeed1 + 6*(SKYSIZE/2-1)*end[0]) * 0x10000); // Manoel Kasimier - smooth sky - edited
	*t = (int)((timespeed1 + 6*(SKYSIZE/2-1)*end[1]) * 0x10000); // Manoel Kasimier - smooth sky - edited
	*s2 = (int)((timespeed2 + 6*(SKYSIZE/2-1)*end[0]) * 0x10000); // Manoel Kasimier - smooth sky
	*t2 = (int)((timespeed2 + 6*(SKYSIZE/2-1)*end[1]) * 0x10000); // Manoel Kasimier - smooth sky
}

// Manoel Kasimier - smooth sky - begin
unsigned char D_DrawSkyPixel33 (unsigned char pixel1, unsigned char pixel2)
{return pixel2 ? alphamap[pixel2 + pixel1*256] : pixel1;}

unsigned char D_DrawSkyPixel66 (unsigned char pixel1, unsigned char pixel2)
{return pixel2 ? alphamap[pixel1 + pixel2*256] : pixel1;}

unsigned char D_DrawSkyPixelOpaque (unsigned char pixel1, unsigned char pixel2)
{return pixel2 ? pixel2 : pixel1;}

unsigned char D_DrawSkyPixelTransparent (unsigned char pixel1, unsigned char pixel2)
{return pixel1;}
// Manoel Kasimier - smooth sky - end

/*
=================
D_DrawSkyScans8
=================
*/
void D_DrawSkyScans8 (espan_t *pspan)
{
	int				count, spancount, u, v;
	unsigned char	*pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	fixed16_t		s2, t2, snext2, tnext2, sstep2, tstep2; // Manoel Kasimier - smooth sky
	int				spancountminus1;
	// Manoel Kasimier - smooth sky - begin
	unsigned char	(*D_DrawSkyPixel)(unsigned char pixel1, unsigned char pixel2);

	if (r_skyalpha.value > 0)
	{
		if (r_skyalpha.value < 0.5)
			D_DrawSkyPixel = D_DrawSkyPixel33;
		else if (r_skyalpha.value < 1.0)
			D_DrawSkyPixel = D_DrawSkyPixel66;
		else
			D_DrawSkyPixel = D_DrawSkyPixelOpaque;
	}
	else
		D_DrawSkyPixel = D_DrawSkyPixelTransparent;

	timespeed1=skytime*skyspeed;
	timespeed2=timespeed1*2.0;
	sstep2 = 0;
	tstep2 = 0;
	// Manoel Kasimier - smooth sky - end
	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	do
	{
		pdest = (unsigned char *)((byte *)d_viewbuffer +
				(screenwidth * pspan->v) + pspan->u);

		count = pspan->count;

	// calculate the initial s & t
		u = pspan->u;
		v = pspan->v;
		D_Sky_uv_To_st (u, v, &s, &t, &s2, &t2); // Manoel Kasimier - smooth sky - edited

		do
		{
			if (count >= SKY_SPAN_MAX)
				spancount = SKY_SPAN_MAX;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				u += spancount;

			// calculate s and t at far end of span,
			// calculate s and t steps across span by shifting
				D_Sky_uv_To_st (u, v, &snext, &tnext, &snext2, &tnext2); // Manoel Kasimier - smooth sky - edited

				sstep = (snext - s) >> SKY_SPAN_SHIFT;
				tstep = (tnext - t) >> SKY_SPAN_SHIFT;
				sstep2 = (snext2 - s2) >> SKY_SPAN_SHIFT; // Manoel Kasimier - smooth sky
				tstep2 = (tnext2 - t2) >> SKY_SPAN_SHIFT; // Manoel Kasimier - smooth sky
			}
			else
			{
			// calculate s and t at last pixel in span,
			// calculate s and t steps across span by division
				spancountminus1 = (float)(spancount - 1);

				if (spancountminus1 > 0)
				{
					u += spancountminus1;
					D_Sky_uv_To_st (u, v, &snext, &tnext, &snext2, &tnext2); // Manoel Kasimier - smooth sky - edited

					sstep = (snext - s) / spancountminus1;
					tstep = (tnext - t) / spancountminus1;
					sstep2 = (snext2 - s2) / spancountminus1; // Manoel Kasimier - smooth sky
					tstep2 = (tnext2 - t2) / spancountminus1; // Manoel Kasimier - smooth sky
				}
			}

			do
			{
				// Manoel Kasimier - smooth sky - begin
				*pdest++ = D_DrawSkyPixel(skyunderlay[((t  & R_SKY_TMASK) >> 8) + ((s  & R_SKY_SMASK) >> 16)],
										skyoverlay [((t2 & R_SKY_TMASK) >> 8) + ((s2 & R_SKY_SMASK) >> 16)]);
				// Manoel Kasimier - smooth sky - end
				s += sstep;
				t += tstep;
				s2 += sstep2; // Manoel Kasimier - smooth sky
				t2 += tstep2; // Manoel Kasimier - smooth sky
			} while (--spancount > 0);

			s = snext;
			t = tnext;
			s2 = snext2; // Manoel Kasimier - smooth sky
			t2 = tnext2; // Manoel Kasimier - smooth sky

		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
}
