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
// vid_null.c -- null video driver to aid porting efforts
//This has been customized for FLASH

#include "../quakedef.h"
#include "../d_local.h"

viddef_t	vid;				// global video state

/*qbism- surcache size formula from D_SurfaceCacheForRes

	size = SURFCACHE_SIZE_AT_320X200; which is 600x1024 = 614400
	pix = width*height;
	if (pix > 64000)
		size += (pix-64000)*3;
*/


#define SURFCACHE_SIZE_AT_768X400	1344000 //qbism was 640X480
#define SURFCACHE_SIZE_AT_640X360	1113600 //qbism

#define	BASEWIDTH	640
#define	BASEHEIGHT	360
int min_vid_width = BASEWIDTH; //qbism
byte	vid_buffer[BASEWIDTH*BASEHEIGHT];
short	zbuffer[BASEWIDTH*BASEHEIGHT];
byte	surfcache[SURFCACHE_SIZE_AT_640X360];

unsigned _vidBuffer4b[BASEWIDTH*BASEHEIGHT];//This is created from the palette every frame, and is suitable for FLASH BitmapData

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];

void	VID_SetPalette (unsigned char *pal)
{
	int i;
	unsigned r,g,b;
	unsigned v;
	unsigned	*table;

//
// 8 8 8 encoding
//
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
//		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff;	// 255 is transparent
}

void	VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette(palette);
}

void	VID_Init (unsigned char *palette)
{
	vid.width = vid.conwidth = BASEWIDTH;
	vid.height = vid.conheight = BASEHEIGHT;
	vid.maxwarpwidth = vid.width; // Manoel Kasimier - hi-res waterwarp
	vid.maxwarpheight = vid.height; // Manoel Kasimier - hi-res waterwarp	vid.aspect = 1.0;
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = BASEWIDTH;

	d_pzbuffer = zbuffer;
	D_InitCaches (surfcache, sizeof(surfcache));
	vid.recalc_refdef = 1; //qbism calculate fovscale
}

void	VID_Shutdown (void)
{
}

void	VID_Update (vrect_t *rects)
{
	int i;
	//Create the buffer suitable for AS3 using the palette

	for(i = 0; i < sizeof(_vidBuffer4b)/sizeof(_vidBuffer4b[0]); i++)
	{
		_vidBuffer4b[i] = d_8to24table[vid_buffer[i]];
	}
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}

// direct draw software compatability stuff

void VID_HandlePause (qboolean pause)
{
}


