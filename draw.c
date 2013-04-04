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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"

byte identityTable[256];
byte translationTable[256];

typedef struct
{
    vrect_t	rect;
    int		width;
    int		height;
    byte	*ptexbytes;
    int		rowbytes;
} rectdesc_t;

static rectdesc_t	r_rectdesc;

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
    char		name[MAX_QPATH];
    cache_user_t	cache;
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;


qpic_t	*Draw_PicFromWad (char *name)
{
    return W_GetLumpName (name);
}

// mankrip - begin
extern int		LongSwap (int l);
extern short	ShortSwap (short l);
extern byte		BestColor (int r, int g, int b, int start, int stop);
qpic_t	*Draw_DecodePic (qpic_t *pic)
{
	satpic_t *p = (satpic_t *) pic;
	if (LongSwap (p->width) == 320 && LongSwap (p->height) == 200) //qb was 240
	{
		int i, r, g, b;
		byte c[256], *source;
		source = p->data;
		p->width	= LongSwap (p->width);
		p->height	= LongSwap (p->height);

		for (i=0 ; i<256 ; i++)
		{
			p->palette[i] = ShortSwap (p->palette[i]);
			r = ( ( (int) (p->palette[i]>>0) & 31) + 1) * 8 - 1;
			g = ( ( (int) (p->palette[i]>>5) & 31) + 1) * 8 - 1;
		//	g = ( ( (int) (p->palette[i]>>5) & 63) + 1) * 4 - 1;
			b = ( ( (int) (p->palette[i]>>10) & 31) + 1) * 8 - 1;
			c[i] = BestColor (r, g, b, 0, 255);
		}
		for (i=0 ; i < p->width * p->height ; i++)
			source[i] = c[source[i]];
	}
	if (p->width == 320 && p->height == 200) //qb was 240
		return (qpic_t *) ( (byte *) p + 512);
	return pic;
}
// mankrip - end
/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
    cachepic_t	*pic;
    int			i;
    qpic_t		*dat;

    for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
        if (!Q_strcmp (path, pic->name))
            break;

    if (i == menu_numcachepics)
    {
        if (menu_numcachepics == MAX_CACHED_PICS)
            Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
        menu_numcachepics++;
        Q_strcpy (pic->name, path);
    }

    dat = Cache_Check (&pic->cache);

    if (dat)
        return dat;

//
// load the pic from disk
//
    COM_LoadCacheFile (path, &pic->cache);

    dat = (qpic_t *)pic->cache.data;
    if (!dat)
    {
	//	Sys_Error ("Draw_CachePic: failed to load %s", path);
		Con_Printf ("Draw_CachePic: failed to load %s", path);
		return NULL;
    }

    SwapPic (dat);

    return dat;
}


/*
===============
Draw_Init
===============
*/

byte menumap[256][16];	//qb: from Engoo- Used for menu backgrounds and simple colormod


void Draw_Init (void)
{
	int		i, j, r;

    for (i = 0 ; i < 256 ; i++)
        identityTable[i] = (byte) i; //qb: MQ 1.6 hudscale

    draw_chars = W_GetLumpName ("conchars");
    draw_disc = W_GetLumpName ("disc");
    draw_backtile = W_GetLumpName ("backtile");

    r_rectdesc.width = draw_backtile->width;
    r_rectdesc.height = draw_backtile->height;
    r_rectdesc.ptexbytes = draw_backtile->data;
    r_rectdesc.rowbytes = draw_backtile->width;

    // qb: from Engoo- Make the menu background table
	// This has been extended to allow 16 others via r_menucolors
	for (i=0 ; i<256 ; i++)
	{
		r = (host_basepal[i*3] + host_basepal[i*3+1] + host_basepal[i*3+2])/(16*3);
		for (j=0; j<9; j++)
		menumap[i][j] = (j * 16) + r;
		for (j=14; j>7; j--)
		menumap[i][j] = (j * 16) + 15 - r;
		menumap[i][14] = 14*16 + r; // forward hack for the muzzleflash fire colors

		// and yes, color ramp #15 is left all black. any further is possibly reserved for slow
		// hexen 2 style menus which use a translucency table
	}
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
	byte			*dest;
	byte			*source;
	unsigned short	*pusdest;
	int				drawline;
	int				drawcol, draw_col; // mankrip
	int				row, col;

	num &= 255;

	if (y <= -8)
		return;			// totally off screen
	// mankrip - begin
	// if the character is totally off of the screen, don't print it
	else if (y >= (int)vid.height)
		return;
	if (x <= -8)
		return;
	else if (x >= (int)vid.width)
		return;
	// mankrip - end

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	if (y < 0)
	{	// clipped
		drawline = 8 + y;
		source -= 128*y;
		y = 0;
	}
	else if (y+8 >= vid.height)			// mankrip
		drawline = (int)vid.height - y;	// mankrip
	else
		drawline = 8;

	// mankrip - begin
	if (x < 0)
	{
		draw_col = drawcol = 8 + x;
		source -= x;
		x = 0;
	}
	else if (x+8 >= vid.width)
		draw_col = drawcol = (int)vid.width - x;
	else
		draw_col = drawcol = 8;
	// mankrip - end


	if (r_pixbytes == 1)
	{
		dest = vid.conbuffer + y*vid.conrowbytes + x;

		if (drawcol == 8) // mankrip
			while (drawline--)
			{
				if (source[0])
					dest[0] = source[0];
				if (source[1])
					dest[1] = source[1];
				if (source[2])
					dest[2] = source[2];
				if (source[3])
					dest[3] = source[3];
				if (source[4])
					dest[4] = source[4];
				if (source[5])
					dest[5] = source[5];
				if (source[6])
					dest[6] = source[6];
				if (source[7])
					dest[7] = source[7];
				source += 128;
				dest += vid.conrowbytes;
			}
		// mankrip - begin
		else
			while (drawline--)
			{
				while (drawcol--)
				{
					if (source[drawcol])
						dest[drawcol] = source[drawcol];
				}
				drawcol = draw_col;
				source += 128;
				dest += vid.conrowbytes;
			}
		// mankrip - end
	}
	else
	{
		// FIXME: pre-expand to native format?
		pusdest = (unsigned short *)((byte *)vid.conbuffer + y*vid.conrowbytes + (x<<1));

		if (drawcol == 8) // mankrip
			while (drawline--)
			{
				if (source[0])
					pusdest[0] = d_8to16table[source[0]];
				if (source[1])
					pusdest[1] = d_8to16table[source[1]];
				if (source[2])
					pusdest[2] = d_8to16table[source[2]];
				if (source[3])
					pusdest[3] = d_8to16table[source[3]];
				if (source[4])
					pusdest[4] = d_8to16table[source[4]];
				if (source[5])
					pusdest[5] = d_8to16table[source[5]];
				if (source[6])
					pusdest[6] = d_8to16table[source[6]];
				if (source[7])
					pusdest[7] = d_8to16table[source[7]];

				source += 128;
				pusdest += (vid.conrowbytes >> 1);
			}
		// mankrip - begin
		else
			while (drawline--)
			{
				while (drawcol--)
				{
					if (source[drawcol])
						pusdest[drawcol] = d_8to16table[source[drawcol]];
				}
				drawcol = draw_col;
				source += 128;
				pusdest += (vid.conrowbytes >> 1);
			}
		// mankrip - end
	}
}


/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
    while (*str)
    {
        //qb Draw_Character (x, y, *str);
        M_DrawCharacter (x, y, *str);
        str++;
        x += 8;
    }
}

#if 0
/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
    byte			*dest;
    byte			*source;
    int				drawline;
    int				row, col;

    if (!vid.direct)
        return;		// don't have direct FB access, so no debugchars...

    drawline = 8;

    row = num>>4;
    col = num&15;
    source = draw_chars + (row<<10) + (col<<3);

    dest = vid.direct + 312;

    while (drawline--)
    {
        dest[0] = source[0];
        dest[1] = source[1];
        dest[2] = source[2];
        dest[3] = source[3];
        dest[4] = source[4];
        dest[5] = source[5];
        dest[6] = source[6];
        dest[7] = source[7];
        source += 128;
        dest += 320;
    }
}
#endif



//=============================================================================

//								2D SPRITES

//=============================================================================

void Draw2Dimage_ScaledMappedTranslatedTransparent (int x, int y, byte *source, int sourcewidth, int sourceheight, int xpadding, int ypadding, int w, int h, qboolean hflip, byte *translation, byte *color_blending_map, qboolean blendbackwards)//, vflip, rotate)
{
    byte  * dest;
    byte  tbyte;
    unsigned short   * pusdest;
    int    v, u, vscale, uscale;

    x += (360-320)/2; //qb: square aspect is 320x180 or 360x202.5  ...sigh...
     if (xpadding < 0)
        Sys_Error ("Draw2Dimage_ScaledMappedTranslatedTransparent: xpadding < 0");
    if (ypadding < 0)
        Sys_Error ("Draw2Dimage_ScaledMappedTranslatedTransparent: ypadding < 0");
    if (xpadding >= sourcewidth)
        Sys_Error ("Draw2Dimage_ScaledMappedTranslatedTransparent: xpadding >= sourcewidth");
    if (ypadding >= sourceheight)
        Sys_Error ("Draw2Dimage_ScaledMappedTranslatedTransparent: ypadding >= sourceheight");

    if (w > sourcewidth)
        Sys_Error ("Draw2Dimage_ScaledMappedTranslatedTransparent: w > sourcewidth");
    if (h > sourceheight)
        Sys_Error ("Draw2Dimage_ScaledMappedTranslatedTransparent: h > sourceheight");

    if (xpadding + w > sourcewidth)
        w = sourcewidth - xpadding;
    if (ypadding + h > sourceheight)
        h = sourceheight - ypadding;

    // return if offscreen
    if (x + w * scr_2d_scale_h < scr_2d_scale_h - 1)
        return;
    if (y + h * scr_2d_scale_v < scr_2d_scale_v - 1)
        return;
    if (x * scr_2d_scale_h > (signed)vid.width)
        return;
    if (y * scr_2d_scale_v > (signed)vid.height)
        return;

    // crop left
    if (x * scr_2d_scale_h < 0)
    {
        x = 0;
    }
    // crop right
    if ( (unsigned) ((x + w) * scr_2d_scale_h) > vid.width)
        w = ( (signed) (vid.width) - (x * scr_2d_scale_h)) / scr_2d_scale_h;
    // crop top
    if (y * scr_2d_scale_v < 0)
    {
        int yoffset = y;
        h += yoffset;
        ypadding -= yoffset;
        y = 0;
    }
    // crop bottom
    if ( (unsigned) ((y + h) * scr_2d_scale_v) > vid.height)
        h = ( (signed) (vid.height) - (y * scr_2d_scale_v)) / scr_2d_scale_v;

    if (!translation)
        translation = identityTable;

    source += (sourcewidth * ypadding) + xpadding;

    if (r_pixbytes == 1)
    {
        for (v = 0 ; v < h ; v++)
        {
            for (u = 0 ; u < w ; u++)
                if (((tbyte = source[u]) != TRANSPARENT_COLOR) && tbyte)  //qb: fixme gfx - both 0 and 255...
                {
                    dest = vid.buffer
                           + ( (int)((y + v) * scr_2d_scale_v - 1) * vid.rowbytes) // v offset
                           + (int)((x + (hflip ? w - u - 1 : u)) * scr_2d_scale_h - 1); // h offset
                    for (vscale = scr_2d_scale_v+1; vscale ; vscale--) //qb:  special effect: skip rows or columns
                        for (uscale = scr_2d_scale_h+1; uscale ; uscale--)  //qb: need that +1
                            if (color_blending_map)
                            {
                                if (blendbackwards)
                                {
                                    dest[vscale * vid.rowbytes + uscale] =
                                        color_blending_map[dest[vscale * vid.rowbytes + uscale] * 256 + translation[tbyte]];
                                }
                                else
                                {
                                    dest[vscale * vid.rowbytes + uscale] =
                                        color_blending_map[dest[vscale * vid.rowbytes + uscale] + translation[tbyte] * 256];
                                }
                            }
                            else
                            {
                                dest[
                                    vscale * vid.rowbytes // v offset
                                    + uscale // h offset
                                ] = translation[tbyte];
                            }
                }
            source += sourcewidth;
        }
    }
    else
    {
        // FIXME: pretranslate at load time?
        pusdest = (unsigned short *)vid.buffer + y * (vid.rowbytes >> 1) + x;

        for (v = 0 ; v < h; v++)
        {
            for (u = 0 ; u < w ; u++)
                if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
                    pusdest[u] = d_8to16table[tbyte];

            pusdest += vid.rowbytes >> 1;
            source += sourcewidth;
        }
    }
}


void M_DrawCharacter (int x, int y, int num)
{
	Draw2Dimage_ScaledMappedTranslatedTransparent (x, y, draw_chars + ( (num & 240) << 6) + ( (num & 15) << 3), 128, 128, 0, 0, 8, 8, false, identityTable, NULL, false);
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	byte	*dest, *source, tbyte;
	unsigned short	*pusdest;
	int				v, u;

	if (!pic) return; // mankrip

	if (x < 0 || (unsigned)(x + pic->width) > vid.width ||
		y < 0 || (unsigned)(y + pic->height) > vid.height)
		Sys_Error ("Draw_TransPic: bad coordinates");

	source = pic->data;

	if (r_pixbytes == 1)
	{
		dest = vid.buffer + y * vid.rowbytes + x;

		if (pic->width & 7)
		{	// general
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;

				dest += vid.rowbytes;
				source += pic->width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<pic->height ; v++)
			{
				for (u=0 ; u<pic->width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						dest[u+1] = tbyte;
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						dest[u+2] = tbyte;
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						dest[u+3] = tbyte;
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						dest[u+4] = tbyte;
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						dest[u+5] = tbyte;
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						dest[u+6] = tbyte;
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						dest[u+7] = tbyte;
				}
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
	}
	else
	{
	// FIXME: pretranslate at load time?
		pusdest = (unsigned short *)vid.buffer + y * (vid.rowbytes >> 1) + x;

		for (v=0 ; v<pic->height ; v++)
		{
			for (u=0 ; u<pic->width ; u++)
				if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
					pusdest[u] = d_8to16table[tbyte];

			pusdest += vid.rowbytes >> 1;
			source += pic->width;
		}
	}
}
// mankrip - begin
void Draw_TransPicMirror (int x, int y, qpic_t *pic)
{
    byte	*dest, *source, tbyte;
//	unsigned short	*pusdest;
    int				v, u;

	if (!pic) return; // mankrip

    if (x < 0 || (unsigned)(x + pic->width) > vid.width ||
            y < 0 || (unsigned)(y + pic->height) > vid.height)
        Sys_Error (va("Draw_TransPic: bad coordinates (%i, %i)", x, y));

    source = pic->data;

    if (r_pixbytes == 1)
    {
        dest = vid.buffer + y * vid.rowbytes + x;

        if (pic->width & 7)
        {
            // general
            for (v=0 ; v<pic->height ; v++)
            {
                for (u=0 ; u<pic->width ; u++)
                    if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
                        dest[pic->width-1-u] = tbyte;

                dest += vid.rowbytes;
                source += pic->width;
            }
        }
        else
        {
            // unwound
            for (v=0 ; v<pic->height ; v++)
            {
                for (u=0 ; u<pic->width ; u+=8)
                {
                    if ( (tbyte=source[pic->width-u-1]) != TRANSPARENT_COLOR)
                        dest[u] = tbyte;
                    if ( (tbyte=source[pic->width-u-2]) != TRANSPARENT_COLOR)
                        dest[u+1] = tbyte;
                    if ( (tbyte=source[pic->width-u-3]) != TRANSPARENT_COLOR)
                        dest[u+2] = tbyte;
                    if ( (tbyte=source[pic->width-u-4]) != TRANSPARENT_COLOR)
                        dest[u+3] = tbyte;
                    if ( (tbyte=source[pic->width-u-5]) != TRANSPARENT_COLOR)
                        dest[u+4] = tbyte;
                    if ( (tbyte=source[pic->width-u-6]) != TRANSPARENT_COLOR)
                        dest[u+5] = tbyte;
                    if ( (tbyte=source[pic->width-u-7]) != TRANSPARENT_COLOR)
                        dest[u+6] = tbyte;
                    if ( (tbyte=source[pic->width-u-8]) != TRANSPARENT_COLOR)
                        dest[u+7] = tbyte;
                }
                dest += vid.rowbytes;
                source += pic->width;
            }
        }
    }
}
// mankrip - end


/*
=============
Draw_TransPicTranslate
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
    byte	*dest, *source, tbyte;
    unsigned short	*pusdest;
    int				v, u;

	if (!pic) return; // mankrip

    if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
            (unsigned)(y + pic->height) > vid.height)
    {
        Sys_Error ("Draw_TransPic: bad coordinates");
    }

    source = pic->data;

    if (r_pixbytes == 1)
    {
        dest = vid.buffer + y * vid.rowbytes + x;

        if (pic->width & 7)
        {
            // general
            for (v=0 ; v<pic->height ; v++)
            {
                for (u=0 ; u<pic->width ; u++)
                    if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
                        dest[u] = translation[tbyte];

                dest += vid.rowbytes;
                source += pic->width;
            }
        }
        else
        {
            // unwound
            for (v=0 ; v<pic->height ; v++)
            {
                for (u=0 ; u<pic->width ; u+=8)
                {
                    if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
                        dest[u] = translation[tbyte];
                    if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
                        dest[u+1] = translation[tbyte];
                    if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
                        dest[u+2] = translation[tbyte];
                    if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
                        dest[u+3] = translation[tbyte];
                    if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
                        dest[u+4] = translation[tbyte];
                    if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
                        dest[u+5] = translation[tbyte];
                    if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
                        dest[u+6] = translation[tbyte];
                    if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
                        dest[u+7] = translation[tbyte];
                }
                dest += vid.rowbytes;
                source += pic->width;
            }
        }
    }
    else
    {
        // FIXME: pretranslate at load time?
        pusdest = (unsigned short *)vid.buffer + y * (vid.rowbytes >> 1) + x;

        for (v=0 ; v<pic->height ; v++)
        {
            for (u=0 ; u<pic->width ; u++)
            {
                tbyte = source[u];

                if (tbyte != TRANSPARENT_COLOR)
                {
                    pusdest[u] = d_8to16table[tbyte];
                }
            }

            pusdest += vid.rowbytes >> 1;
            source += pic->width;
        }
    }
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
    int				x, y, v;
    byte			*src, *dest;
    unsigned short	*pusdest;
    int				f, fstep;
    qpic_t			*conback;
//	char			ver[100]; // removed



    extern cvar_t	con_alpha; // mankrip - transparent console

    if (!con_alpha.value && !con_forcedup) // mankrip - transparent console
        return; // mankrip - transparent console
    conback = Draw_CachePic ("gfx/conback.lmp");

    /* // mankrip
    // hack the version number directly into the pic
    #ifdef _WIN32
    	sprintf (ver, "(WinQuake) %4.2f", (float)VERSION);
    	dest = conback->data + 320*186 + 320 - 11 - 8*Q_strlen(ver);
    #elif defined(X11)
    	sprintf (ver, "(X11 Quake %2.2f) %4.2f", (float)X11_VERSION, (float)VERSION);
    	dest = conback->data + 320*186 + 320 - 11 - 8*Q_strlen(ver);
    #elif defined(__linux__)
    	sprintf (ver, "(Linux Quake %2.2f) %4.2f", (float)LINUX_VERSION, (float)VERSION);
    	dest = conback->data + 320*186 + 320 - 11 - 8*Q_strlen(ver);
    #else
    	dest = conback->data + 320 - 43 + 320*186;
    	sprintf (ver, "%4.2f", VERSION);
    #endif

    	for (x=0 ; x<Q_strlen(ver) ; x++)
    		Draw_CharToConback (ver[x], dest+(x<<3));
    */ // mankrip
// draw the pic
    fstep = conback->width*0x10000/vid.conwidth; // mankrip - hi-res console background - edited
    if (r_pixbytes == 1)
    {
        dest = vid.conbuffer;

        // mankrip - transparent console - begin
        if (con_alpha.value < 0.5 && !con_forcedup)
            for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
            {
                // mankrip - hi-res console background - edited - begin
                v = (vid.conheight - lines + y)*conback->height/vid.conheight;
                src = conback->data + v*conback->width;
                // mankrip - hi-res console background - edited - end
                f = 0;
                for (x=0 ; x<vid.conwidth ; x++) //qb: get rid of x4
                {
                    dest[x] = alphamap[src[f>>16] + dest[x]*256];
                    f += fstep;
                }
                /*qb: get rid of x4
                				for (x=0 ; x<vid.conwidth ; x+=4)
                				{
                					dest[x] = alphamap[src[f>>16] + dest[x]*256];
                					f += fstep;
                					dest[x+1] = alphamap[src[f>>16] + dest[x+1]*256];
                					f += fstep;
                					dest[x+2] = alphamap[src[f>>16] + dest[x+2]*256];
                					f += fstep;
                					dest[x+3] = alphamap[src[f>>16] + dest[x+3]*256];
                					f += fstep;
                				} */
            }
        else if (con_alpha.value < 1 && !con_forcedup)
            for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
            {
                // mankrip - hi-res console background - edited - begin
                v = (vid.conheight - lines + y)*conback->height/vid.conheight;
                src = conback->data + v*conback->width;
                // mankrip - hi-res console background - edited - end
                f = 0;
                for (x=0 ; x<vid.conwidth ; x++) //qb: get rid of x4
                {
                    dest[x] = alphamap[src[f>>16] + dest[x]*256];
                    f += fstep;
                }
            }
        else
            // mankrip - transparent console - end
            for (y=0 ; y<lines ; y++, dest += vid.conrowbytes)
            {
                // mankrip - hi-res console background - edited - begin
                v = (vid.conheight - lines + y)*conback->height/vid.conheight;
                src = conback->data + v*conback->width;
                if (vid.conwidth == conback->width)
                    // mankrip - hi-res console background - edited - end
                    memcpy (dest, src, vid.conwidth);
                else
                {
                    f = 0;
                    for (x=0 ; x<vid.conwidth ; x++) //qb: get rid of x4
                    {
                        dest[x] = src[f>>16];
                        f += fstep;
                    }
                }
            }
    }
    else
    {
        pusdest = (unsigned short *)vid.conbuffer;

        for (y=0 ; y<lines ; y++, pusdest += (vid.conrowbytes >> 1))
        {
            // FIXME: pre-expand to native format?
            // FIXME: does the endian switching go away in production?
            v = (vid.conheight - lines + y)*conback->height/vid.conheight; // mankrip - hi-res console background - edited
            src = conback->data + v*conback->width; // mankrip - hi-res console background - edited
            f = 0;
            //	fstep = 320*0x10000/vid.conwidth; // removed
            for (x=0 ; x<vid.conwidth ; x++)
            {
                pusdest[x] = d_8to16table[src[f>>16]]; //qb: get rid of x4
                f += fstep;
            }
        }
    }
}


/*
==============
R_DrawRect8
==============
*/
void R_DrawRect8 (vrect_t *prect, int rowbytes, byte *psrc,
                  int transparent)
{
    byte	t;
    int		i, j, srcdelta, destdelta;
    byte	*pdest;

    pdest = vid.buffer + (prect->y * vid.rowbytes) + prect->x;

    srcdelta = rowbytes - prect->width;
    destdelta = vid.rowbytes - prect->width;

    if (transparent)
    {
        for (i=0 ; i<prect->height ; i++)
        {
            for (j=0 ; j<prect->width ; j++)
            {
                t = *psrc;
                if (t != TRANSPARENT_COLOR)
                {
                    *pdest = t;
                }

                psrc++;
                pdest++;
            }

            psrc += srcdelta;
            pdest += destdelta;
        }
    }
    else
    {
        for (i=0 ; i<prect->height ; i++)
        {
            memcpy (pdest, psrc, prect->width);
            psrc += rowbytes;
            pdest += vid.rowbytes;
        }
    }
}


/*
==============
R_DrawRect16
==============
*/
void R_DrawRect16 (vrect_t *prect, int rowbytes, byte *psrc,
                   int transparent)
{
    byte			t;
    int				i, j, srcdelta, destdelta;
    unsigned short	*pdest;

// FIXME: would it be better to pre-expand native-format versions?

    pdest = (unsigned short *)vid.buffer +
            (prect->y * (vid.rowbytes >> 1)) + prect->x;

    srcdelta = rowbytes - prect->width;
    destdelta = (vid.rowbytes >> 1) - prect->width;

    if (transparent)
    {
        for (i=0 ; i<prect->height ; i++)
        {
            for (j=0 ; j<prect->width ; j++)
            {
                t = *psrc;
                if (t != TRANSPARENT_COLOR)
                {
                    *pdest = d_8to16table[t];
                }

                psrc++;
                pdest++;
            }

            psrc += srcdelta;
            pdest += destdelta;
        }
    }
    else
    {
        for (i=0 ; i<prect->height ; i++)
        {
            for (j=0 ; j<prect->width ; j++)
            {
                *pdest = d_8to16table[*psrc];
                psrc++;
                pdest++;
            }

            psrc += srcdelta;
            pdest += destdelta;
        }
    }
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
    int				width, height, tileoffsetx, tileoffsety;
    byte			*psrc;
    vrect_t			vr;

    r_rectdesc.rect.x = x;
    r_rectdesc.rect.y = y;
    r_rectdesc.rect.width = w;
    r_rectdesc.rect.height = h;

    vr.y = r_rectdesc.rect.y;
    height = r_rectdesc.rect.height;

    tileoffsety = vr.y % r_rectdesc.height;

    while (height > 0)
    {
        vr.x = r_rectdesc.rect.x;
        width = r_rectdesc.rect.width;

        if (tileoffsety != 0)
            vr.height = r_rectdesc.height - tileoffsety;
        else
            vr.height = r_rectdesc.height;

        if (vr.height > height)
            vr.height = height;

        tileoffsetx = vr.x % r_rectdesc.width;

        while (width > 0)
        {
            if (tileoffsetx != 0)
                vr.width = r_rectdesc.width - tileoffsetx;
            else
                vr.width = r_rectdesc.width;

            if (vr.width > width)
                vr.width = width;

            psrc = r_rectdesc.ptexbytes +
                   (tileoffsety * r_rectdesc.rowbytes) + tileoffsetx;

            if (r_pixbytes == 1)
            {
                R_DrawRect8 (&vr, r_rectdesc.rowbytes, psrc, 0);
            }
            else
            {
                R_DrawRect16 (&vr, r_rectdesc.rowbytes, psrc, 0);
            }

            vr.x += vr.width;
            width -= vr.width;
            tileoffsetx = 0;	// only the left tile can be left-clipped
        }

        vr.y += vr.height;
        height -= vr.height;
        tileoffsety = 0;		// only the top tile can be top-clipped
    }
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
    byte			*dest;
    unsigned short	*pusdest;
    unsigned		uc;
    int				u, v;

    // mankrip - begin
    if (w > vid.width - x)
        w = vid.width - x;
    if (h > vid.height - y)
        h = vid.height - y;
    // mankrip - end
    if (r_pixbytes == 1)
    {
        dest = vid.buffer + y*vid.rowbytes + x;
        for (v=0 ; v<h ; v++, dest += vid.rowbytes)
            for (u=0 ; u<w ; u++)
                dest[u] = c;
    }
    else
    {
        uc = d_8to16table[c];

        pusdest = (unsigned short *)vid.buffer + y * (vid.rowbytes >> 1) + x;
        for (v=0 ; v<h ; v++, pusdest += (vid.rowbytes >> 1))
            for (u=0 ; u<w ; u++)
                pusdest[u] = uc;
    }
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)  //qb: from engoo
{
	int			x,y;
	byte		*pbuf;
	int		mycol;

	mycol = (int)scr_fadecolor.value;
	S_ExtraUpdate ();

	for (y=0 ; y<vid.height ; y++)
	{
		int	t;

		pbuf = (byte *)(vid.buffer + vid.rowbytes*y);
		t = (y & 1) << 1;

		for (x=0 ; x<vid.width ; x++)
		{
			// Classic 0.8-1.06 look
			if (mycol < 15){
				pbuf[x] = menumap[pbuf[x]][mycol];	// new menu tint
			}
			else
			{
			// stupid v1.08 look:
			if ((x & 3) != t)
				pbuf[x] = 0;
			}
		}
	}

	S_ExtraUpdate ();
}
