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
// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "r_local.h"
#ifdef _WIN32
#include "s_win32/movie_avi.h"
#endif

// only the refresh window will be updated unless these variables are flagged
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldfov; // edited
cvar_t		scr_viewsize = {"viewsize","100", true};
cvar_t		scr_fov = {"fov","91.25"};	// 10 - 170 //qb: 91.25 works best for fisheye
cvar_t		scr_conspeed = {"scr_conspeed","1000"};
cvar_t		scr_showpause = {"showpause","1"};
cvar_t		scr_centertime = {"scr_centertime","2"};
cvar_t		scr_printspeed = {"scr_printspeed","16"}; // 8 // Manoel Kasimier - edited
cvar_t      scr_fadecolor = {"scr_fadecolor","4", true};  //qb: background color for fadescreen2

qboolean	scr_initialized;		// ready to draw

qpic_t		*scr_ram;
qpic_t		*scr_net;
//qpic_t		*scr_turtle; // Manoel Kasimier - removed

int			scr_fullupdate;

int			clearconsole;
int			clearnotify;

viddef_t	vid;				// global video state

vrect_t		*pconupdate;
vrect_t		scr_vrect;

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;
qboolean	scr_skipupdate;

qboolean	block_drawing;

void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
    Con_LogCenterPrint (str); //qb: log centerprint - from BJB
    Q_strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
    scr_centertime_off = scr_centertime.value;
    scr_centertime_start = cl.time;

// count the number of lines for centering
    scr_center_lines = 1;
    while (*str)
    {
        if (*str == '\n')
            scr_center_lines++;
        str++;
    }
}

void SCR_EraseCenterString (void)
{
    int		y;
    int		h; // Manoel Kasimier

    if (scr_erase_center++ > vid.numpages)
    {
        scr_erase_lines = 0;
        return;
    }

    // Manoel Kasimier - svc_letterbox - begin
    if (cl.letterbox)
        y = scr_vrect.y + scr_vrect.height + 4;
    else
        // Manoel Kasimier - svc_letterbox - end
        if (scr_center_lines <= 4)
            y = vid.height*0.35;
        else
            y = 44;//28+16;//48; // Manoel Kasimier - edited

    scr_copytop = 1;
    // Manoel Kasimier - begin
    // limit the height to prevent crashes with huge centerprints
    h = 8*scr_erase_lines;
    if (y+h > vid.height)
        h = vid.height - y;
    Draw_Fill (0, y,vid.width, h, 0);
    // Manoel Kasimier - end
}

void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	Draw_UpdateAlignment (1, 1);
	// mankrip - svc_letterbox - begin
	if (cl.letterbox)
	{
		y = (scr_vrect.y + scr_vrect.height + 4) / scr_2d_scale_v;
		Draw_UpdateAlignment (1, 0);
	}
	else
	// mankrip - svc_letterbox - end
	if (scr_center_lines <= 4)
		y = 200*0.35; // mankrip - edited
	else
		y = 44;//28+16;//48; // mankrip - edited

	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (320 - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
		{
			M_DrawCharacter (x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
    scr_copytop = 1;
    if (scr_center_lines > scr_erase_lines)
        scr_erase_lines = scr_center_lines;

    scr_centertime_off -= host_frametime;

    if (scr_centertime_off <= 0 && !cl.intermission)
        return;
    if (key_dest != key_game)
        return;

    SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
    float   a;
    float   x;

    if (fov_x < 1 || fov_x > 179)
        Sys_Error ("Bad fov: %f", fov_x);

    x = width/tan(fov_x/360*M_PI);

    a = atan (height/x);

    a = a*360/M_PI;

    return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
    vrect_t		vrect;
//	float		size; // Manoel Kasimier - removed

    scr_fullupdate = 0;		// force a background redraw
    vid.recalc_refdef = 0;

// force the status bar to redraw
    Sbar_Changed ();

//========================================

    r_refdef.fov_x = scr_fov.value;
    r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

    fovscale = 90.0f/scr_fov.value; // Manoel Kasimier - FOV-based scaling
    /*/// Manoel Kasimier - removed - begin
    // intermission is always full screen
    	if (cl.intermission)
    		size = 120;
    	else
    		size = scr_viewsize.value;

    	if (size >= 120)
    		sb_lines = 0;		// no status bar at all
    	else if (size >= 110)
    		sb_lines = 24;		// no inventory
    	else
    		sb_lines = 24+16+8;
    /*/// Manoel Kasimier - removed - end

// these calculations mirror those in R_Init() for r_refdef, but take no
// account of water warping
    vrect.x = 0;
    vrect.y = 0;
    vrect.width = vid.width;
    vrect.height = vid.height;

    R_SetVrect (&vrect, &scr_vrect, sb_lines);

// guard against going from one mode to another that's less than half the
// vertical resolution
    if (scr_con_current > vid.height)
        scr_con_current = vid.height;

// notify the refresh of the change
    R_ViewChanged (&vrect, sb_lines);
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
    Cvar_SetValue ("viewsize",scr_viewsize.value+5); // Manoel Kasimier - edited
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
    Cvar_SetValue ("viewsize",scr_viewsize.value-5); // Manoel Kasimier - edited
}


void SCR_SbarUp_f (void)  //qb:
{
    sbar.value +=1;
    if (sbar.value >4) sbar.value = 4;
}
void SCR_SbarDown_f (void)  //qb:
{
    sbar.value -=1;
    if (sbar.value <0) sbar.value = 0;
}


/*
==================
SCR_Adjust
==================
*/
void SCR_AdjustFOV (void)
{
// bound field of view //qb: change to 30 - 140
    if (r_fisheye.value) //qb:  set it yourself, for now
        return;
    if (scr_fov.value < 30)
        Cvar_Set ("fov","30");
    if (scr_fov.value > 140)
        Cvar_Set ("fov","140");
    scr_ofsx.value = (pow(scr_fov.value, 2)-scr_fov.value*70)/2000 -3; //qb: compensate view model position
    scr_ofsz.value = (pow(scr_fov.value, 2)-scr_fov.value*70)/2000 -1;//qb: compensate view model position
    if (oldfov != scr_fov.value)
    {
        oldfov = scr_fov.value;
        vid.recalc_refdef = true;
    }
}

void SCR_Adjust (void)
{
    static float	oldscr_viewsize;

// bound viewsize
    // Manoel Kasimier - edited - begin
    if (scr_viewsize.value < 50)	// 30
        Cvar_Set ("viewsize","50");	// 30
    else if (scr_viewsize.value > 100)	// 120
        Cvar_Set ("viewsize","100");// 120
    // Manoel Kasimier - edited - end
    if (scr_viewsize.value != oldscr_viewsize)
    {
        oldscr_viewsize = scr_viewsize.value;
        vid.recalc_refdef = 1;
    }
}

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{

    Cvar_RegisterVariableWithCallback (&scr_fov, SCR_AdjustFOV); // Manoel Kasimier
    Cvar_RegisterVariableWithCallback (&scr_viewsize, SCR_Adjust); // Manoel Kasimier
//	Cvar_RegisterVariable (&scr_fov);
//	Cvar_RegisterVariable (&scr_viewsize);
    Cvar_RegisterVariable (&scr_conspeed);
    Cvar_RegisterVariable (&scr_centertime);
    Cvar_RegisterVariable (&scr_printspeed);
    Cvar_RegisterVariable (&scr_fadecolor); //qb:

//
// register our commands
//
    Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
    Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
    Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

    Cmd_AddCommand ("sbarup",SCR_SbarUp_f);  //qb:
    Cmd_AddCommand ("sbardown",SCR_SbarDown_f);  //qb:

    scr_ram = Draw_PicFromWad ("ram");
    scr_net = Draw_PicFromWad ("net");
//	scr_turtle = Draw_PicFromWad ("turtle"); // Manoel Kasimier - removed

#ifdef _WIN32 //qb: jqavi
    Movie_Init ();
#endif
    scr_initialized = true;
}


/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
    if (realtime - cl.last_received_message < 0.3)
        return;
    if (cls.demoplayback)
        return;

    Draw_UpdateAlignment (0, 0); // mankrip
    M_DrawTransPic (scr_vrect.x+64, scr_vrect.y, scr_net); // Manoel Kasimier - edited
}

//qb: improved drawfps adapted from Proquake

/* JPG - draw frames per second
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS (void)
{
    int x;
    static double last_realtime = 0.0;
    static int last_framecount = 0;
    static float fps = 0; //qb: let's see higher precision, better for tweaking
    char st[10];

    if (realtime - last_realtime > 2.0) //qb: was 1, for better averaging
    {
        fps = (host_framecount - last_framecount) / (realtime - last_realtime) + 1.0;
        last_framecount = host_framecount;
        last_realtime = realtime;
    }

    sprintf(st, "%f FPS", fps);

    x = 300; //qb
    if (scr_vrect.y > 0)
        Draw_Fill(x, 0, Q_strlen(st) * 8, 8, 0);
    Draw_UpdateAlignment (2, 0);
    Draw_String(x, 0, st);
}


/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
    if (!scr_showpause.value)		// turn off for screenshots
        return;

    if (!cl.paused)
        return;
    Draw_UpdateAlignment (1, 2);
    M_DrawPlaque ("gfx/pause.lmp", false); // Manoel Kasimier
}



/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
    qpic_t	*pic;

    if (!scr_drawloading)
        return;

    pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_UpdateAlignment (1, 1); // mankrip
	M_DrawTransPic ( (min_vid_width - pic->width) / 2, (min_vid_height - pic->height - (float)sb_lines / scr_2d_scale_v) / 2, pic);
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
    float conspeed; //qb: from goldquake
    //qb: from johnfitz -- let's hack away the problem of slow console when host_timescale is <0
// DEMO_REWIND - qb: Baker change
    float timescale;

    Con_CheckResize ();

    if (scr_drawloading)
        return;		// never a console with loading plaque

// decide on the height of the console
    con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

    if (con_forcedup)
    {
        scr_conlines = vid.height;		// full screen
        scr_con_current = scr_conlines;
    }
    else if (key_dest == key_console)
        scr_conlines = vid.height/2;	// half screen
    else
        scr_conlines = 0;				// none visible

    timescale = (host_timescale.value > 0) ? host_timescale.value : 1; //qb: DEMO_REWIND by Baker - johnfitz -- timescale

    conspeed = scr_conspeed.value * (vid.height / 200.0f) * host_frametime;
    if (cls.timedemo || cls.capturedemo)
        frame_timescale = .00001; // Make it open or close instantly in timedemo

    if (scr_conlines < scr_con_current)
    {
        scr_con_current -= scr_conspeed.value*host_frametime/frame_timescale; //qb: DEMO_REWIND by Baker - johnfitz -- timescale

        if (scr_conlines > scr_con_current)
            scr_con_current = scr_conlines;

    }
    else if (scr_conlines > scr_con_current)
    {
        scr_con_current += scr_conspeed.value*host_frametime/frame_timescale; //qb: DEMO_REWIND by Baker - johnfitz -- timescale
        if (scr_conlines < scr_con_current)
            scr_con_current = scr_conlines;
    }

    if (clearconsole++ < vid.numpages)
    {
        scr_copytop = 1;
        if (con_alpha.value == 1.0) // Manoel Kasimier - transparent console
            Draw_Fill (0, (int)scr_con_current, vid.width, vid.height-(int)scr_con_current, 0); // Manoel Kasimier - edited
        else // Manoel Kasimier - transparent console
            Draw_Fill (0, 0, vid.width, vid.height, 0); // Manoel Kasimier - transparent console
        Sbar_Changed ();
    }
    else if (clearnotify++ < vid.numpages && scr_vrect.y > 0)
    {
        scr_copytop = 1;
        Draw_Fill (0, 0, vid.width, con_notifylines, 0);
    }
    else
        con_notifylines = 0;
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
    if (scr_con_current)
    {
        scr_copyeverything = 1;
        Con_DrawConsole (scr_con_current, true);
        clearconsole = 0;
    }
    else
    {
        if (key_dest == key_game || key_dest == key_message)
            Con_DrawNotify ();	// only draw notify in game
    }
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

typedef struct
{
    byte	manufacturer;
    byte	version;
    byte	encoding;
    byte	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    byte	reserved;
    byte	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    byte	filler[58];
    byte	data;			// unbounded
} pcx_t;

/*
=============
LoadTGA //qb: MK 1.4a
=============
*/

#pragma pack(push,1) //qb: thanks to Spike
typedef struct //_TargaHeader
{
    char			id_length, colormap_type, image_type;
    unsigned short	colormap_index, colormap_length;
    char			colormap_size;
    unsigned short	x_origin, y_origin, width, height;
    char			pixel_size, attributes;
} TargaHeader;
#pragma pack(pop)

void LoadTGA_as8bit (char *filename, byte **pic, int *width, int *height)
{
    TargaHeader		*filedata;					//pcx_t	*pcx;
    byte			*input, *output;			//*out,

    int				row, column;				//int		x, y;
    int				columns, rows, numPixels;	//int		dataByte, runLength;
    byte			*pixbuf;					//byte	*pix;
    loadedfile_t	*fileinfo;

    *pic = NULL;

    fileinfo = COM_LoadTempFile (filename);
    if (!fileinfo)
        return;

    // parse the file
    filedata = (TargaHeader *) (fileinfo->data);

    if (filedata->image_type != 2 && filedata->image_type != 10)
        Sys_Error ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");

    if (filedata->colormap_type != 0 || (filedata->pixel_size != 32 && filedata->pixel_size != 24))
        Sys_Error ("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\non \"%s\"\n", filename);

    columns   = LittleShort (filedata->width);
    rows   = LittleShort (filedata->height);
    numPixels = columns * rows;
    *width = filedata->width;
    *height = filedata->height;

    *pic = output = Q_malloc (numPixels);
    input = (byte *) filedata + sizeof (TargaHeader) + filedata->id_length; // skip TARGA image comment

    if (filedata->image_type == 2) // Uncompressed, RGB images
    {
        byte red, green, blue,alphabyte;
        int k = 0;
        for(row = rows - 1 ; row >= 0 ; row--)
        {
            pixbuf = output + row * columns;
            for (column = 0 ; column < columns ; column++)
            {
                switch (filedata->pixel_size)
                {
                case 24:
                    blue		= input[k++];
                    green		= input[k++];
                    red			= input[k++];
                    *pixbuf++ = BestColor (red, green, blue, 0, 254);
                    break;
                case 32:
                    blue		= input[k++];
                    green		= input[k++];
                    red			= input[k++];
                    alphabyte	= input[k++];
                    *pixbuf++ =  BestColor (red, green, blue, 0, 254);
                    break;
                }
            }
        }
    }

    else if (filedata->image_type == 10) // Runlength encoded RGB images
    {
        byte red, green, blue, alphabyte, packetHeader, packetSize, bestcol;
        int j, k=0;

        for (row = rows - 1 ; row >= 0 ; row--)
        {
            pixbuf = output + row * columns;
            for (column = 0 ; column < columns ; )
            {
                packetHeader = input[k++];
                packetSize = 1 + (packetHeader & 0x7f);
                if (packetHeader & 0x80) // run-length packet
                {
                    switch (filedata->pixel_size)
                    {
                    case 24:
                        blue		= input[k++];
                        green		= input[k++];
                        red			= input[k++];
                        bestcol = BestColor (red, green, blue, 0, 254);
                        break;
                    case 32:
                        blue		= input[k++];
                        green		= input[k++];
                        red			= input[k++];
                        alphabyte	= input[k++];
                        bestcol =  BestColor (red, green, blue, 0, 254);
                        break;
                    }
                    for(j = 0 ; j < packetSize ; j++)
                    {
                        *pixbuf++ = bestcol;
                        column++;
                        if (column == columns) // run spans across rows
                        {
                            column = 0;
                            if (row > 0)
                                row--;
                            else
                                goto breakOut;
                            pixbuf = output + row * columns;
                        }
                    }
                }
                else // non run-length packet
                {
                    for(j = 0 ; j < packetSize ; j++)
                    {
                        switch (filedata->pixel_size)
                        {
                        case 24:
                            blue		= input[k++];
                            green		= input[k++];
                            red			= input[k++];
                            *pixbuf++   = BestColor (red, green, blue, 0, 254);
                            break;
                        case 32:
                            blue		= input[k++];
                            green		= input[k++];
                            red			= input[k++];
                            alphabyte	= input[k++];
                            *pixbuf++   = BestColor (red, green, blue, 0, 254);
                            break;
                        }
                        column++;
                        if (column == columns) // pixel packet run spans across rows
                        {
                            column = 0;
                            if (row > 0)
                                row--;
                            else
                                goto breakOut;
                            pixbuf = output + row * columns;
                        }
                    }
                }
            }
breakOut:
            ;
        }
    }

}

// Manoel Kasimier - skyboxes - begin
// Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
/*
============
LoadPCX
============
*/
void LoadPCX (char *filename, byte **pic, int *width, int *height)
{
    pcx_t	*pcx;
    byte	*pcxbuf, *out, *pix;
    int		x, y;
    int		dataByte, runLength;
    loadedfile_t	*fileinfo; // Manoel Kasimier

    *pic = NULL;
    // Manoel Kasimier - begin
//	pcxbuf = COM_LoadTempFile (filename); // removed
//	if (!pcxbuf) // removed
    fileinfo = COM_LoadTempFile (filename);
    if (!fileinfo)
        // Manoel Kasimier - end
        return;
    pcxbuf = (char *)fileinfo->data; // Manoel Kasimier

//
// parse the PCX file
//
    pcx = (pcx_t *)pcxbuf;
    pcx->xmax = LittleShort (pcx->xmax);
    pcx->xmin = LittleShort (pcx->xmin);
    pcx->ymax = LittleShort (pcx->ymax);
    pcx->ymin = LittleShort (pcx->ymin);
    pcx->hres = LittleShort (pcx->hres);
    pcx->vres = LittleShort (pcx->vres);
    pcx->bytes_per_line = LittleShort (pcx->bytes_per_line);
    pcx->palette_type = LittleShort (pcx->palette_type);

    pix = &pcx->data;

    if (pcx->manufacturer != 0x0a
            || pcx->version != 5
            || pcx->encoding != 1
            || pcx->bits_per_pixel != 8) // Manoel Kasimier - edited
        //	|| pcx->xmax >= 640 // Manoel Kasimier - removed
        //	|| pcx->ymax >= 480) // Manoel Kasimier - removed
    {
        Con_Printf ("Bad pcx file\n");
        return;
    }

    if (width)
        *width = pcx->xmax+1;
    if (height)
        *height = pcx->ymax+1;

    *pic = out = Q_malloc ((pcx->xmax+1) * (pcx->ymax+1));

    for (y=0 ; y<=pcx->ymax ; y++, out += pcx->xmax+1)
    {
        for (x=0 ; x<=pcx->xmax ; )
        {
            dataByte = *pix++;

            if((dataByte & 0xC0) == 0xC0)
            {
                runLength = dataByte & 0x3F;
                dataByte = *pix++;
            }
            else
                runLength = 1;

            while(runLength-- > 0)
                out[x++] = dataByte;
        }
    }
}
// Manoel Kasimier - skyboxes - end

/*
==============
WritePCXfile
==============
*/
void WritePCXfile (char *filename, byte *data, int width, int height,
                   int rowbytes, byte *palette)
{
    int		i, j, length;
    pcx_t	*pcx;
    byte		*pack;

    pcx = Hunk_TempAlloc (width*height*2+1000);
    if (pcx == NULL)
    {
        Con_Printf("SCR_ScreenShot_f: not enough memory\n");
        return;
    }

    pcx->manufacturer = 0x0a;	// PCX id
    pcx->version = 5;			// 256 color
    pcx->encoding = 1;		// uncompressed
    pcx->bits_per_pixel = 8;		// 256 color
    pcx->xmin = 0;
    pcx->ymin = 0;
    pcx->xmax = LittleShort((short)(width-1));
    pcx->ymax = LittleShort((short)(height-1));
    pcx->hres = LittleShort((short)width);
    pcx->vres = LittleShort((short)height);
    memset (pcx->palette,0,sizeof(pcx->palette));
    pcx->color_planes = 1;		// chunky image
    pcx->bytes_per_line = LittleShort((short)width);
    pcx->palette_type = LittleShort(2);		// not a grey scale
    memset (pcx->filler,0,sizeof(pcx->filler));

// pack the image
    pack = &pcx->data;

    for (i=0 ; i<height ; i++)
    {
        for (j=0 ; j<width ; j++)
        {
            if ( (*data & 0xc0) != 0xc0)
                *pack++ = *data++;
            else
            {
                *pack++ = 0xc1;
                *pack++ = *data++;
            }
        }

        data += rowbytes - width;
    }

// write the palette
    *pack++ = 0x0c;	// palette ID byte
    for (i=0 ; i<768 ; i++)
        *pack++ = *palette++;

// write output file
    length = pack - (byte *)pcx;
    COM_WriteFile (filename, pcx, length);
}



/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f (void)
{
#ifndef FLASH //qb:
    int     i;
    char		pcxname[80];
    char		checkname[MAX_OSPATH];

//
// find a file name to save it to
//
    Q_strcpy(pcxname,"qbs8_000.pcx"); //qb: screenshots dir

    for (i=0 ; i<=999 ; i++)
    {
        pcxname[5] = i/100 + '0';
        pcxname[6] = i/10 + '0';
        pcxname[7] = i%10 + '0';
        sprintf (checkname, "%s/%s", com_gamedir, pcxname);
        if (Sys_FileTime(checkname) == -1)
            break;	// file doesn't exist
    }
    if (i==1000)
    {
        Con_Printf ("SCR_ScreenShot_f: Too many PCX files in directory.\n");
        return;
    }

//
// save the pcx file
//
    WritePCXfile (pcxname, vid.buffer, vid.width, vid.height, vid.rowbytes,
                  host_basepal);

    Con_Printf ("Wrote %s\n", pcxname);
#endif // FLASH
}


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
    S_StopAllSounds (true);

    if (cls.state != ca_connected)
        return;
    if (cls.signon != SIGNONS)
        return;

    Vibration_Stop (0); // Manoel Kasimier
    Vibration_Stop (1); // Manoel Kasimier
// redraw with no console and the loading plaque
    Con_ClearNotify ();
    scr_centertime_off = 0;
    scr_con_current = 0;
    scr_drawloading = true;
    scr_fullupdate = 0;
    Sbar_Changed ();
    SCR_UpdateScreen (); //qb: debug
    scr_drawloading = false;

    scr_disabled_for_loading = true;
    scr_disabled_time = realtime;
    scr_fullupdate = 0;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
    scr_disabled_for_loading = false;
    scr_fullupdate = 0;
    Con_ClearNotify ();
}

//=============================================================================

char	*scr_notifystring;
qboolean	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
    char	*start;
    int		l;
    int		j;
    int		x, y;

    start = scr_notifystring;

    y = vid.height*0.35;
	Draw_UpdateAlignment (0, 0); // mankrip
    do
    {
        // scan the width of the line
        for (l=0 ; l<40 ; l++)
            if (start[l] == '\n' || !start[l])
                break;
        x = (vid.width - l*8)/2;
        for (j=0 ; j<l ; j++, x+=8)
            Draw_Character (x, y, start[j]);

        y += 8;

        while (*start && *start != '\n')
            start++;

        if (!*start)
            break;
        start++;		// skip the \n
    }
    while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
int SCR_ModalMessage (char *text)
{
#ifdef FLASH
    return true;	//For Flash we receive key messages via the Main.as file, between calls to the Alchemy C source.
    //We therefore cant check what the user response was, so we just assume that it was 'yes'.
#endif

    if (cls.state == ca_dedicated)
        return true;

    scr_notifystring = text;

// draw a fresh screen
    scr_fullupdate = 0;
    scr_drawdialog = true;
    SCR_UpdateScreen ();
    scr_drawdialog = false;

    S_ClearBuffer ();		// so dma doesn't loop current sound

    do
    {
        key_count = -1;		// wait for a key down and up
        Sys_SendKeyEvents ();
    }
    while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

    scr_fullupdate = 0;
    SCR_UpdateScreen ();

    return key_lastpress == 'y';
}


//=============================================================================


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
    vrect_t		vrect;

    if (scr_skipupdate || block_drawing)
        return;

    scr_copytop = 0;
    scr_copyeverything = 0;

    if (scr_disabled_for_loading)
    {
        if (realtime - scr_disabled_time > 60)
        {
            scr_disabled_for_loading = false;
            Con_Printf ("load failed.\n");
        }
        else
            return;
    }

    if (cls.state == ca_dedicated)
        return;				// stdout only

    if (!scr_initialized || !con_initialized)
        return;				// not initialized yet

    // Manoel Kasimier - removi código daqui

    // Manoel Kasimier - start
    {
        static byte old_sb_lines;
        if (sb_lines != old_sb_lines)
        {
            old_sb_lines = sb_lines;
            vid.recalc_refdef = true;
        }
    }
    // Manoel Kasimier - end

    if (vid.recalc_refdef)
    {
        // something changed, so reorder the screen
        SCR_CalcRefdef ();
    }
//
// do 3D refresh drawing, and then update the screen
//
    if (scr_fullupdate++ < vid.numpages)
    {
        // clear the entire screen
        scr_copyeverything = 1;
        Draw_Fill (0,0,vid.width,vid.height, 0); // Manoel Kasimier - edited
        //	Sbar_Changed ();
    }

    pconupdate = NULL;

    SCR_SetUpToDrawConsole ();
    SCR_EraseCenterString ();

    V_RenderView ();

    if (scr_drawdialog)
    {
       Sbar_Draw ();
        Draw_FadeScreen ();
        SCR_DrawNotifyString ();
        scr_copyeverything = true;
    }
    else if (scr_drawloading)
    {
       SCR_DrawLoading ();
//		Sbar_Draw (); // Manoel Kasimier - removed
    }
    else if (cl.intermission == 1 && key_dest == key_game)
    {
       Sbar_IntermissionOverlay ();
    }
    else if (cl.intermission == 2 && key_dest == key_game)
    {
        Sbar_FinaleOverlay ();
        SCR_CheckDrawCenterString ();
    }
    else if (cl.intermission == 3 && key_dest == key_game)
    {
       SCR_CheckDrawCenterString ();
    }
    else
    {
        //qb: removed	SCR_DrawRam ();
        SCR_DrawNet ();
        //	SCR_DrawTurtle (); // Manoel Kasimier - removed
        SCR_DrawPause ();
        SCR_CheckDrawCenterString ();
        Sbar_Draw ();
        SCR_DrawConsole ();
        M_Draw ();
        if (cl_showfps.value) SCR_DrawFPS ();	// 2001-11-31 FPS display by QuakeForge/Muff
    }

    if (pconupdate)
    {
        D_UpdateRects (pconupdate);
    }

    V_UpdatePalette ();

#ifdef _WIN32 //qb: jqavi
    Movie_UpdateScreen ();
#endif

// update one of three areas
    if (scr_copyeverything)
    {
        vrect.x = 0;
        vrect.y = 0;
        vrect.width = vid.width;
        vrect.height = vid.height;
        vrect.pnext = 0;

        VID_Update (&vrect);
    }
    else if (scr_copytop)
    {
        vrect.x = 0;
        vrect.y = 0;
        vrect.width = vid.width;
        vrect.height = vid.height - sb_lines;
        vrect.pnext = 0;

        VID_Update (&vrect);
    }
    else
    {
        vrect.x = scr_vrect.x;
        vrect.y = scr_vrect.y;
        vrect.width = scr_vrect.width;
        vrect.height = scr_vrect.height;
        vrect.pnext = 0;

        VID_Update (&vrect);
    }
}


/*
==================
SCR_UpdateWholeScreen
==================
*/
void SCR_UpdateWholeScreen (void)
{
    scr_fullupdate = 0;
    SCR_UpdateScreen ();
}
