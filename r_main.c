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
// r_main.c

#include "quakedef.h"
#include "r_local.h"

//define	PASSAGES

void MakeMy15to8();
void ParseWorldspawn (void);
void		*colormap;
//vec3_t		viewlightvec; // Manoel Kasimier - changed alias models lighting - removed
//alight_t	r_viewlighting = {128, 192, viewlightvec}; // Manoel Kasimier - changed alias models lighting - removed
float		r_time1;
int			r_numallocatededges;
//qbism remove     qboolean	r_drawpolys;
//qbism remove     qboolean	r_drawculledpolys;
//qbism remove     qboolean	r_worldpolysbacktofront;
qboolean	r_recursiveaffinetriangles = true;
int			r_pixbytes = 1;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;

qboolean	r_dowarp, r_dowarpold, r_viewchanged;

int			numbtofpolys;
btofpoly_t	*pbtofpolys;
mvertex_t	*r_pcurrentvertbase;

int			c_surf;
int			r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean	r_surfsonstack;
int			r_clipflags;

//qbism - from engoo
// COLOR Translation stuff
// Came straight out of image.c of Quake2 tools

byte	palmap[32][32][32];		// For fast 15-bit lookup
byte	palmapnofb[32][32][32];		// No fullbrights

#if !defined(FLASH)
byte		*r_stack_start;
#endif

byte		*r_warpbuffer;

qboolean	r_fov_greater_than_90;

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

//
// screen size info
//
refdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		screenwidth;

float	pixelAspect;
float	screenAspect;
float	verticalFieldOfView;
float	xOrigin, yOrigin;

mplane_t	screenedge[4];

byte	*warpbuffer = NULL; // Manoel Kasimier - hi-res waterwarp & buffered video
float ditherfog[DITHER_NUMRANDS]; //qbism - pseudorandom dither
//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

#define		VIEWMODNAME_LENGTH	256
char		viewmodname[VIEWMODNAME_LENGTH+1];
int			modcount;

int			*pfrustum_indexes[4];
int			r_frustum_indexes[4*6];

int		reinit_surfcache = 1;	// if 1, surface cache is currently empty and
// must be reinitialized for current cache size

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

float		r_aliastransition, r_resfudge;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

float	dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

void R_MarkLeaves (void);

cvar_t	r_draworder = {"r_draworder","0"};
cvar_t	r_speeds = {"r_speeds","0"};

//qbism fte stain cvars
cvar_t r_stainfadeamount = {"r_stainfadeamount", "1"};
cvar_t r_stainfadetime = {"r_stainfadetime", "1"};
cvar_t r_stains = {"r_stains", "0.75"}; //zero to one

cvar_t	r_timegraph = {"r_timegraph","0"};
cvar_t	r_graphheight = {"r_graphheight","10"};
cvar_t	r_clearcolor = {"r_clearcolor","2"};
cvar_t	r_waterwarp = {"r_waterwarp","1"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1", true}; // Manoel Kasimier - saved in the config file - edited
cvar_t	r_aliasstats = {"r_polymodelstats","0"};
cvar_t	r_dspeeds = {"r_dspeeds","0"};
cvar_t	r_drawflat = {"r_drawflat", "0"};
cvar_t	r_ambient = {"r_ambient", "0"};

cvar_t	r_coloredlights = {"r_coloredlights", "1", true}; //qbism
cvar_t	r_clintensity = {"r_clintensity", "1.2", true}; //qbism
cvar_t	r_clbaseweight = {"r_clbaseweight", "1.0", true}; //qbism- base pixel weight for color map blending
cvar_t	r_clcolorweight= {"r_clcolorweight", "0.7", true}; //qbism- color weight for color map blending
cvar_t	r_colmaprange= {"r_colmaprange", "1.3", true}; //qbism- colormap range for overbright

cvar_t r_fog = {"r_fog", "1", true}; //qbism-  draw fog?

cvar_t	r_reportsurfout = {"r_reportsurfout", "0"};
cvar_t	r_maxsurfs = {"r_maxsurfs", "0"};
cvar_t	r_numsurfs = {"r_numsurfs", "0"};
cvar_t	r_reportedgeout = {"r_reportedgeout", "0"};
cvar_t	r_maxedges = {"r_maxedges", "0"};
cvar_t	r_numedges = {"r_numedges", "0"};
cvar_t	r_aliastransadj = {"r_aliastransadj", "100"};

cvar_t	r_colmapred = {"r_colmapred", "0"};  //qbism - boost overall light tint for colormap.
cvar_t	r_colmapblue = {"r_colmapblue", "0"};  //qbism - boost overall light tint for colormap.
cvar_t	r_colmapgreen = {"r_colmapgreen", "0"};  //qbism - boost overall light tint for colormap.

//cvar_t	r_letterbox = {"r_letterbox","0"}; // Manoel Kasimier - r_letterbox
// Manoel Kasimier - changed alias models lighting - begin
cvar_t	r_light_vec_x = {"r_light_vec_x", "-1"};
cvar_t	r_light_vec_y = {"r_light_vec_y", "0"};
cvar_t	r_light_vec_z = {"r_light_vec_z", "-1"};
cvar_t	r_light_style = {"r_light_style", "1", true};
// Manoel Kasimier - changed alias models lighting - end
cvar_t	r_wateralpha = {"r_wateralpha","0.6666", true}; // Manoel Kasimier - translucent water
cvar_t	r_glassalpha = {"r_wateralpha","0.3333", true}; //qb: *glass
cvar_t	r_shadowhack = {"r_shadowhack", "0", false};
cvar_t	r_shadowhacksize = {"r_shadowhacksize", "2.7", true};

//qbism- particle cvars
cvar_t  r_part_scale = {"r_part_scale", "1.0", true};
cvar_t  r_part_blob_count = {"r_part_blob_count", "256", true};
cvar_t  r_part_blob_time = {"r_part_blob_time", "0.2", true};
cvar_t  r_part_blob_vel = {"r_part_blob_vel", "200", true};
cvar_t  r_part_explo1_count = {"r_part_explo1_count", "400", true};
cvar_t  r_part_explo1_time = {"r_part_explo1_time", "5", true};
cvar_t  r_part_explo1_vel = {"r_part_explo1_vel", "400", true};
cvar_t  r_part_explo2_count = {"r_part_explo2_count", "200", true};
cvar_t  r_part_explo2_time = {"r_part_explo2_time", "0.3", true};
cvar_t  r_part_explo2_vel = {"r_part_explo2_vel", "300", true};
cvar_t  r_part_sticky_time = {"r_part_sticky_time", "18", true};

//void CreatePassages (void); // Manoel Kasimier - removed
//void SetVisibilityByPassages (void); // Manoel Kasimier - removed

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
    int		x,y, m;
    byte	*dest;

// create a simple checkerboard texture for the default
    r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");

    r_notexture_mip->width = r_notexture_mip->height = 16;
    r_notexture_mip->offsets[0] = sizeof(texture_t);
    r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
    r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
    r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;

    for (m=0 ; m<4 ; m++)
    {
        dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
        for (y=0 ; y< (16>>m) ; y++)
            for (x=0 ; x< (16>>m) ; x++)
            {
                if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
                    *dest++ = 0;
                else
                    *dest++ = 0xff;
            }
    }
}

/*
===============
R_Init
===============
*/

void FogDitherInit(void)
{
    int i;
    float dfactor;
    dfactor = max(1.0-fog_density,0.01);
    for (i=0; i<DITHER_NUMRANDS; i++)
        ditherfog[i] = dfactor* 0.4 + (float)(rand()%10000)/100000.0;
}


void R_LoadPalette_f (void); //qbism - load an alternate palette
void R_LoadSky_f (void); // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
void R_Init (void)
{
    int i;
#if !defined(FLASH)
    int		dummy;

// get stack position so we can guess if we are going to overflow
    r_stack_start = (byte *)&dummy;
#endif

    MakeMy15to8(); //qbism - engoo doesn't use it yet, so might as well
    R_InitTurb ();

    Cmd_AddCommand ("loadpalette", R_LoadPalette_f);
    Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
    Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
    Cmd_AddCommand ("loadsky", R_LoadSky_f); // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
    Cmd_AddCommand ("fog", Fog_FogCommand_f); // qbism- fog commands from FitzQuake
    //set up global fog defaults
    fog_density = 0.0;
    fog_red = 0.17;
    fog_green = 0.17;
    fog_blue = 0.17;
    srand(8);  //leave nothing to chance.
    FogDitherInit();
    Cvar_RegisterVariable (&r_draworder);
    Cvar_RegisterVariable (&r_speeds);
    Cvar_RegisterVariable (&r_timegraph);
    Cvar_RegisterVariable (&r_graphheight);
    Cvar_RegisterVariable (&r_drawflat);
    Cvar_RegisterVariable (&r_ambient);
    Cvar_RegisterVariable (&r_coloredlights); //qbism
    Cvar_RegisterVariable (&r_clintensity); //qbism
    Cvar_RegisterVariable (&r_clbaseweight); //qbism
    Cvar_RegisterVariable (&r_clcolorweight); //qbism
    Cvar_RegisterVariable (&r_colmaprange); //qbism
    Cvar_RegisterVariable (&r_fog); //qbism
    Cvar_RegisterVariable (&r_clearcolor);
    Cvar_RegisterVariable (&r_waterwarp);
    Cvar_RegisterVariable (&r_fullbright);
    Cvar_RegisterVariable (&r_drawentities);
    Cvar_RegisterVariable (&r_drawviewmodel);
    Cvar_RegisterVariable (&r_aliasstats);
    Cvar_RegisterVariable (&r_dspeeds);
    Cvar_RegisterVariable (&r_reportsurfout);
    Cvar_RegisterVariable (&r_maxsurfs);
    Cvar_RegisterVariable (&r_numsurfs);
    Cvar_RegisterVariable (&r_reportedgeout);
    Cvar_RegisterVariable (&r_maxedges);

    Cvar_RegisterVariable (&r_colmapred); //qbism
    Cvar_RegisterVariable (&r_colmapblue);
    Cvar_RegisterVariable (&r_colmapgreen);

    Cvar_RegisterVariable (&r_skyname); // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
//	Cvar_RegisterVariableWithCallback (&r_letterbox, SCR_Adjust); // Manoel Kasimier - r_letterbox
    // Manoel Kasimier - changed alias models lighting - begin
    Cvar_RegisterVariable (&r_light_vec_x);
    Cvar_RegisterVariable (&r_light_vec_y);
    Cvar_RegisterVariable (&r_light_vec_z);
    Cvar_RegisterVariable (&r_light_style);
    // Manoel Kasimier - changed alias models lighting - end
    Cvar_RegisterVariable (&r_interpolation); // Manoel Kasimier - model interpolation
    Cvar_RegisterVariable (&r_wateralpha); // Manoel Kasimier - translucent water
    Cvar_RegisterVariable (&r_glassalpha); //qb: *glass
    Cvar_RegisterVariable (&sw_stipplealpha); // Manoel Kasimier
//    Cvar_RegisterVariable (&r_sprite_addblend); // Manoel Kasimier
    Cvar_RegisterVariable (&r_shadowhack); //qbism- engoo shadowhack
    Cvar_RegisterVariable (&r_shadowhacksize); //qbism

    //qbism ftestain cvars
    Cvar_RegisterVariable(&r_stains);
    Cvar_RegisterVariable(&r_stainfadetime);
    Cvar_RegisterVariable(&r_stainfadeamount);

    //qbism particle cvars
    Cvar_RegisterVariable(&r_part_scale);
    Cvar_RegisterVariable(&r_part_blob_count);
    Cvar_RegisterVariable(&r_part_blob_time);
    Cvar_RegisterVariable(&r_part_blob_vel);
    Cvar_RegisterVariable(&r_part_explo1_count);
    Cvar_RegisterVariable(&r_part_explo1_time);
    Cvar_RegisterVariable(&r_part_explo1_vel);
    Cvar_RegisterVariable(&r_part_explo2_count);
    Cvar_RegisterVariable(&r_part_explo2_time);
    Cvar_RegisterVariable(&r_part_explo2_vel);
    Cvar_RegisterVariable(&r_part_sticky_time);

    Cvar_SetValue ("r_maxedges", (float) 100000); //NUMSTACKEDGES //qbism was 60000
    Cvar_SetValue ("r_maxsurfs", (float) 100000); //NUMSTACKSURFACES //qbism was 60000

    view_clipplanes[0].leftedge = true;
    view_clipplanes[1].rightedge = true;
    view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
                                      view_clipplanes[3].leftedge = false;
    view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
                                       view_clipplanes[3].rightedge = false;

    r_refdef.xOrigin = XCENTERING;
    r_refdef.yOrigin = YCENTERING;

    R_InitParticles ();
    D_Init ();
}

// Manoel Kasimier - skyboxes - begin
// Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
void CL_ParseEntityLump (char *entdata)
{
    char *data;
    char key[128], value[4096];
    extern cvar_t r_skyname;

    data = entdata;

    if (!data)
        return;
    data = COM_Parse (data);
    if (!data || com_token[0] != '{')
        return;							// error

    while (1)
    {
        data = COM_Parse (data);
        if (!data)
            return;						// error
        if (com_token[0] == '}')
            break;						// end of worldspawn

        if (com_token[0] == '_')
            Q_strcpy(key, com_token + 1);
        else
            Q_strcpy(key, com_token);

        while (key[Q_strlen(key)-1] == ' ')
            key[Q_strlen(key)-1] = 0;		// remove trailing spaces

        data = COM_Parse (data);
        if (!data)
            return;						// error
        Q_strcpy (value, com_token);

        if (Q_strcmp (key, "sky") == 0 || Q_strcmp (key, "skyname") == 0 ||
                Q_strcmp (key, "qlsky") == 0)
            //	Cvar_Set ("r_skyname", value);
            // Manoel Kasimier - begin
        {
            Cbuf_AddText(va("wait;loadsky %s\n", value));
            //	R_LoadSky(value);
            return; // Manoel Kasimier
        }
        // Manoel Kasimier - end
        // more checks here..
        if (Q_strcmp (key, "palette") == 0) //qbism- use palette field in worldspawn.
        {
            Cbuf_AddText(va("wait;loadpalette %s\n", value));
            return;
        }
    }
    // Manoel Kasimier - begin
    if (r_skyname.string[0])
        Cbuf_AddText(va("wait;loadsky %s\n", r_skyname.string));
    //	R_LoadSky(r_skyname.string); // crashes the engine if the r_skyname cvar is set before the game boots
    else
        R_LoadSky("");
    // Manoel Kasimier - end
}
// Manoel Kasimier - skyboxes - end


/*
=============================================================================

COLORMAP GRABBING

=============================================================================
*/

//qbism - 15to8 stealed from engoo.

void MakeMy15to8()
{
    int r, g, b, beastcolor, beefstcolor;
    for (r=0 ; r<256 ; r+=8)
    {
        for (g=0 ; g<256 ; g+=8)
        {
            for (b=0 ; b<256 ; b+=8)
            {
                beastcolor = BestColor (r, g, b, 0, 255);
                beefstcolor = BestColor (r, g, b, 0, 223);
                palmap[r>>3][g>>3][b>>3] = beastcolor;
                palmapnofb[r>>3][g>>3][b>>3] = beefstcolor;
            }
        }
    }
}



/*
===============
BestColor - qbism- from qlumpy
===============
*/
int BestColor (int r, int g, int b, int start, int stop)
{
    int	i;
    int	dr, dg, db;
    int	bestdistortion, distortion;
    int	bestcolor;
    byte	*pal;

    r = bound (0,r,254);
    g = bound (0,g,254);
    b = bound (0,b,254);
//
// let any color go to 0 as a last resort
//
    bestdistortion = ( (int)r + (int)g + (int)b )*2; //qbism  ( (int)r*r + (int)g*g + (int)b*b )*2;
    bestcolor = 0;

    pal = host_basepal + start*3;
    for (i=start ; i<= stop ; i++)
    {
        dr = abs(r - (int)pal[0]);
        dg = abs(g - (int)pal[1]);
        db = abs(b - (int)pal[2]);
        pal += 3;
        distortion = dr + dg + db; //qbism - more weight on value.  dr*dr + dg*dg + db*db;
        if (distortion < bestdistortion)
        {
            if (!distortion)
                return i;		// perfect match

            bestdistortion = distortion;
            bestcolor = i;
        }
    }

    return bestcolor;
}


void GrabAlphamap (void) //qbism- based on Engoo
{
    int c,l, r,g,b;
    float ay, ae;
    byte *colmap;

    ay = 0.666667;
    ae = 1.0 - ay;				// base pixels
    colmap = alphamap;

    for (l=0; l<256; l++)
    {
        for (c=0 ; c<256 ; c++)
        {
            r = (int)(((float)host_basepal[c*3]*ae)  + ((float)host_basepal[l*3] *ay));
            g = (int)(((float)host_basepal[c*3+1]*ae) + ((float)host_basepal[l*3+1] *ay));
            b = (int)(((float)host_basepal[c*3+2]*ae)  + ((float)host_basepal[l*3+2] *ay));
            *colmap++ =BestColor(r,g,b, 0, 255); // High quality color tables get best color
        }
    }
}

void GrabFogmap (void) //qbism- yet another lookup
{
    int c,l, r,g,b;
    float ay, ae;
    byte *colmap;

    colmap = fogmap;

    for (l=0; l<256; l++)
    {
        for (c=0 ; c<256 ; c++)
        {
            r = (host_basepal[c*3] + host_basepal[l*3] + (c/32.0));
            g = (host_basepal[c*3+1] + host_basepal[l*3+1] + (c/32.0));
            b = (host_basepal[c*3+2] + host_basepal[l*3+2] + (c/32.0));
            *colmap++ = BestColor(r,g,b, 0, 254); // High quality color tables get best color
        }
    }
}


void GrabLightcolormap (void) //qbism- for colored lighting, fullbrights show through
{
    int c,l, r,g,b;
    float rl, gl, bl;
    float ay, ae, bright;
    byte *colmap;

    if(r_coloredlights.value)
    {
        ae = bound (0, r_clbaseweight.value, 1.0);	    		// base pixels
        ay = bound (0, r_clcolorweight.value, 1.0);             //color
    }
    else
    {
        ae = 1.0;
        ay = 0.0;
    }

    colmap = lightcolormap;

    for (l=0; l<256; l++)
    {
        rl = host_basepal[l*3]*ae;
        gl = host_basepal[l*3+1]*ae;
        bl = host_basepal[l*3+2]*ae;

        for (c=0 ; c<256 ; c++)
        {
            if (l>223)
                *colmap++ = l;
            else
            {
                //bright = sqrt((rl+gl+bl)/(host_basepal[c*3]+host_basepal[c*3+1]+host_basepal[c*3+2]+1.0));
                r = rl + (float)host_basepal[c*3]*ay;//*bright;
                g = gl + (float)host_basepal[c*3+1]*ay;//*bright;
                b = bl + (float)host_basepal[c*3+2]*ay;//*bright;


                *colmap++ = BestColor(r,g,b, 0, 223);
            }
        }
    }
}



void GrabAdditivemap (void) //qbism- based on Engoo
{
    int c,l, red, green, blue;
    float ay, ae;
    byte *colmap;

    ay = 1.0;
    ae = 1.0;				// base pixels
    colmap = additivemap;

    for (l=0; l<256; l++)
    {
        for (c=0 ; c<256 ; c++)
        {
            red = (int)(((float)host_basepal[c*3]*ae)  + ((float)host_basepal[l*3] *ay));
            green = (int)(((float)host_basepal[c*3+1]*ae) + ((float)host_basepal[l*3+1] *ay));
            blue = (int)(((float)host_basepal[c*3+2]*ae)  + ((float)host_basepal[l*3+2] *ay));
            *colmap++ = BestColor(red,green,blue, 0, 254); // High quality color tables get best color
        }
    }
}



/*
==============
GrabColormap - qbism- from qlumpy

filename COLORMAP levels fullbrights
the first map is an identiy 0-255
the final map is all black except for the fullbrights
the remaining maps are evenly spread
fullbright colors start at the top of the palette.
==============
*/
void GrabColormap (void)  //qbism - fixed, was a little screwy
{
    int		l, c, red, green, blue;
    float	frac, cscale, fracscaled;
    float   rscaled, gscaled, bscaled;
    byte *colmap;

    colmap = host_colormap;

// identity lump
//   for (l=0 ; l<256 ; l++)
//       *colmap++ = l;

    cscale = (float)(255 - max(max(r_colmapred.value, r_colmapgreen.value),  r_colmapblue.value))
             / 255.0;

// shaded levels
    for (l=0; l<COLORLEVELS; l++)
    {
        frac = (float)l/(COLORLEVELS-1);
        frac = r_colmaprange.value - (frac );  //qbism was 1.0.... boost!
        //rscaled = r_colmapred.value*cscale;
        // gscaled = r_colmapgreen.value*cscale;
        // bscaled = r_colmapblue.value*cscale;

        for (c=0 ; c<256-PALBRIGHTS ; c++)
        {
            red = (int)((float)host_basepal[c*3]*frac); //+ rscaled);
            green = (int)((float)host_basepal[c*3+1]*frac); //+ gscaled);
            blue = (int)((float)host_basepal[c*3+2]*frac); // + bscaled);

//
// note: 254 instead of 255 because 255 is the transparent color, and we
// don't want anything remapping to that
//
            *colmap++ = BestColor(red,green,blue, 0, 254);
        }
        for ( ; c<256 ; c++)
        {
            red = (int)host_basepal[c*3];
            green = (int)host_basepal[c*3+1];
            blue = (int)host_basepal[c*3+2];

            *colmap++ = BestColor(red,green,blue, 0, 254);
        }
    }
}





/*
=============================================================================

MIPTEX GRABBING

=============================================================================
*/


/*
=============
AveragePixels  qbism- from qlumpy
=============
*/
/*qbism- to do, or not...

byte	pixdata[256];
int		d_red, d_green, d_blue;

byte AveragePixels (int count)
{
    int		r,g,b;
    int		i;
    int		vis;
    int		pix;
    int		dr, dg, db;
    int		bestdistortion, distortion;
    int		bestcolor;
    byte	*pal;
    int		fullbright;
    int		e;

    vis = 0;
    r = g = b = 0;
    fullbright = 0;
    for (i=0 ; i<count ; i++)
    {
        pix = pixdata[i];
        if (pix == 255)
            fullbright = 2;
        else if (pix >= 240)
        {
            return pix;
            if (!fullbright)
            {
                fullbright = true;
                r = 0;
                g = 0;
                b = 0;
            }
        }
        else
        {
            if (fullbright)
                continue;
        }

        r += host_basepal[pix*3];
        g += host_basepal[pix*3+1];
        b += host_basepal[pix*3+2];
        vis++;
    }

    if (fullbright == 2)
        return 255;

    r /= vis;
    g /= vis;
    b /= vis;

    if (!fullbright)
    {
        r += d_red;
        g += d_green;
        b += d_blue;
    }

//
// find the best color
//
    bestdistortion = r*r + g*g + b*b;
    bestcolor = 0;
    if (fullbright)
    {
        i = 240;
        e = 255;
    }
    else
    {
        i = 0;
        e = 240;
    }

    for ( ; i< e ; i++)
    {
        pix = i;	//pixdata[i];

        pal = host_basepal + pix*3;

        dr = r - (int)pal[0];
        dg = g - (int)pal[1];
        db = b - (int)pal[2];

        distortion = dr*dr + dg*dg + db*db;
        if (distortion < bestdistortion)
        {
            if (!distortion)
            {
                d_red = d_green = d_blue = 0;	// no distortion yet
                return pix;		// perfect match
            }

            bestdistortion = distortion;
            bestcolor = pix;
        }
    }

    if (!fullbright)
    {
        // error diffusion
        pal = host_basepal + bestcolor*3;
        d_red = r - (int)pal[0];
        d_green = g - (int)pal[1];
        d_blue = b - (int)pal[2];
    }

    return bestcolor;
}
*/

/*
==============
GrabMip

filename MIP x y width height
must be multiples of sixteen
==============
*/
/*qbism- to do, or not...
void GrabMip (void)
{
	int             x,y,xl,yl,xh,yh,w,h;
	byte            *screen_p, *source;
	int             linedelta;
	miptex_t		*qtex;
	int				miplevel, mipstep;
	int				xx, yy, pix;
	int				count;

	GetToken (false);
	xl = atoi (token);
	GetToken (false);
	yl = atoi (token);
	GetToken (false);
	w = atoi (token);
	GetToken (false);
	h = atoi (token);

	if ( (w & 15) || (h & 15) )
		Error ("line %i: miptex sizes must be multiples of 16", scriptline);

	xh = xl+w;
	yh = yl+h;

	qtex = (miptex_t *)lump_p;
	qtex->width = LittleLong(w);
	qtex->height = LittleLong(h);
	strcpy (qtex->name, lumpname);

	lump_p = (byte *)&qtex->offsets[4];

	screen_p = byteimage + yl*byteimagewidth + xl;
	linedelta = byteimagewidth - w;

	source = lump_p;
	qtex->offsets[0] = LittleLong(lump_p - (byte *)qtex);

	for (y=yl ; y<yh ; y++)
	{
		for (x=xl ; x<xh ; x++)
		{
			pix = *screen_p;
			*screen_p++ = 0;
			if (pix == 255)
				pix = 0;
			*lump_p++ = pix;
		}
		screen_p += linedelta;
	}

//
// subsample for greater mip levels
//
	d_red = d_green = d_blue = 0;	// no distortion yet

	for (miplevel = 1 ; miplevel<4 ; miplevel++)
	{
		qtex->offsets[miplevel] = LittleLong(lump_p - (byte *)qtex);

		mipstep = 1<<miplevel;
		for (y=0 ; y<h ; y+=mipstep)
		{

			for (x = 0 ; x<w ; x+= mipstep)
			{
				count = 0;
				for (yy=0 ; yy<mipstep ; yy++)
					for (xx=0 ; xx<mipstep ; xx++)
					{
						pixdata[count] = source[ (y+yy)*w + x + xx ];
						count++;
					}
				*lump_p++ = AveragePixels (count);
			}
		}
	}


}

*/




/*
===============
R_LoadPalette
===============
*/

int R_LoadPalette (char *name) //qbism - load an alternate palette
{
    loadedfile_t	*fileinfo;
    char	pathname[MAX_QPATH];

    Q_snprintfz (pathname, sizeof(pathname), "gfx/%s.lmp", name);

    fileinfo = COM_LoadHunkFile (pathname);
    if (!fileinfo)
    {
        Con_Printf("Palette %s not found.\n", name);
        return 0;
    }
    memcpy (host_basepal, fileinfo->data, 768);
    //qbism - if color 255 (transparent) isn't the default color, use it for lighting tint.
    if ((host_basepal[765] != 159) && (host_basepal[766] != 91) && (host_basepal[767] != 83))
    {
        r_colmapred.value = host_basepal[765];
        r_colmapgreen.value = host_basepal[766];
        r_colmapblue.value = host_basepal[767];
    }
    else
    {
        r_colmapred.value = 0;
        r_colmapgreen.value = 0;
        r_colmapblue.value = 0;
    }

    GrabColormap();
    GrabAlphamap();
    GrabLightcolormap();
    GrabAdditivemap();
    GrabFogmap();
    VID_SetPalette (host_basepal);
    Con_Printf("Palette %s loaded.\n", name);
    return 1;
}

/*
===============
R_LoadPalette_f
===============
*/
void R_LoadPalette_f (void) //qbism - load an alternate palette
{
    if (Cmd_Argc() != 2)
    {
        Con_Printf ("loadpalette <name> : load a color palette\n");
        return;
    }
    R_LoadPalette(Cmd_Argv(1));
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
    int		i;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
    for (i=0 ; i<cl.worldmodel->numleafs ; i++)
        cl.worldmodel->leafs[i].efrags = NULL;

    CL_ParseEntityLump (cl.worldmodel->entities); // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
    r_viewleaf = NULL;
    R_ClearParticles ();
    R_BuildLightmaps(); //qbism ftestain
    ParseWorldspawn (); //qbism- based on Fitzquake
    R_InitSkyBox (); // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
    r_cnumsurfs = r_maxsurfs.value;

    if (r_cnumsurfs <= MINSURFACES)
        r_cnumsurfs = MINSURFACES;

    if (r_cnumsurfs > NUMSTACKSURFACES)
    {
        surfaces = Hunk_AllocName (r_cnumsurfs * sizeof(surf_t), "surfaces");
        surface_p = surfaces;
        surf_max = &surfaces[r_cnumsurfs];
        r_surfsonstack = false;
        // surface 0 doesn't really exist; it's just a dummy because index 0
        // is used to indicate no edge attached to surface
        surfaces--;
    }
    else
    {
        r_surfsonstack = true;
    }

    r_maxedgesseen = 0;
    r_maxsurfsseen = 0;

    r_numallocatededges = r_maxedges.value;

    if (r_numallocatededges < MINEDGES)
        r_numallocatededges = MINEDGES;

    if (r_numallocatededges <= NUMSTACKEDGES)
    {
        auxedges = NULL;
    }
    else
    {
        auxedges = Hunk_AllocName (r_numallocatededges * sizeof(edge_t),
                                   "edges");
    }

    r_dowarpold = false;
    r_viewchanged = false;
    V_BonusFlash_f(); //qbism- hack to remove cshift hangover, plus looks good on respawn.  Is there a better place for this?

#ifdef PASSAGES
    CreatePassages ();
#endif
}


/*
===============
R_SetVrect
===============
*/
extern cvar_t temp1;
void R_SetVrect (vrect_t *pvrectin, vrect_t *pvrect, int lineadj)
{
    int		h;
    float	size;

    size = scr_viewsize.value > 100 ? 100 : scr_viewsize.value;
    if (cl.intermission)
    {
        size = 100;
        lineadj = 0;
    }
    size /= 100.0; // fixed

    h = pvrectin->height - lineadj;
    pvrect->width = pvrectin->width * size;
    if (pvrect->width < 96)
    {
        size = 96.0 / pvrectin->width;
        pvrect->width = 96;	// min for icons
    }
    pvrect->width &= ~7;
    pvrect->height = pvrectin->height * size;
    if (pvrect->height > h)//pvrectin->height - lineadj) // optimized
        pvrect->height = h;//pvrectin->height - lineadj; // optimized

    pvrect->height &= ~1;

    pvrect->x = (pvrectin->width - pvrect->width)/2;
    pvrect->y = (h - pvrect->height)/2;

    // Manoel Kasimier - screen positioning - begin
    {
        int hspace = (int)(vid.width  / 40);//80);
        int vspace = (int)(vid.height / 30);//60);

        if (screen_left > hspace)
        {
            pvrect->x += screen_left - hspace;
            pvrect->width -= screen_left - hspace;
        }

        if (screen_right > hspace)
            pvrect->width -= screen_right - hspace;

        /*	if ((scr_con_current > screen_top) && (scr_con_current < vid.height))
        	{
        		pvrect->y += scr_con_current;
        		pvrect->height -= scr_con_current;
        	}
        	else	*/
        if (screen_top > vspace)
        {
            pvrect->y += screen_top - vspace;
            pvrect->height -= screen_top - vspace;
        }

        if (sb_lines)
        {
            pvrect->height -= screen_bottom;
            vid.aspect = ((float)(pvrect->height + sb_lines + min(vspace, screen_bottom)) / (float)pvrect->width) * (320.0 / 240.0);
        }
        else
        {
            if (screen_bottom > vspace)
                pvrect->height -= screen_bottom - vspace;
            vid.aspect = ((float)pvrect->height / (float)pvrect->width) * (320.0 / 240.0);
        }
        // Manoel Kasimier - svc_letterbox - start
        if (cl.letterbox)
        {
            int letterbox_height = (int)((float)pvrect->height * (cl.letterbox/2.0));
            pvrect->y += letterbox_height; // - sb_lines/2
            pvrect->height -= letterbox_height*2;
        }
        // Manoel Kasimier - svc_letterbox - end
    }
    // Manoel Kasimier - stereo 3D - begin
}


/*
===============
R_ViewChanged

Called every time the vid structure or r_refdef changes.
Guaranteed to be called before the first refresh
===============
*/
void R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect)
{
    int		i;
    float	res_scale;

    r_viewchanged = true;

    R_SetVrect (pvrect, &r_refdef.vrect, lineadj);

    r_refdef.horizontalFieldOfView = 2.0 * tan (r_refdef.fov_x/360*M_PI);
    r_refdef.fvrectx = (float)r_refdef.vrect.x;
    r_refdef.fvrectx_adj = (float)r_refdef.vrect.x - 0.5;
    r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x<<20) + (1<<19) - 1;
    r_refdef.fvrecty = (float)r_refdef.vrect.y;
    r_refdef.fvrecty_adj = (float)r_refdef.vrect.y - 0.5;
    r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
    r_refdef.vrectright_adj_shift20 = (r_refdef.vrectright<<20) + (1<<19) - 1;
    r_refdef.fvrectright = (float)r_refdef.vrectright;
    r_refdef.fvrectright_adj = (float)r_refdef.vrectright - 0.5;
    r_refdef.vrectrightedge = (float)r_refdef.vrectright - 0.99;
    r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
    r_refdef.fvrectbottom = (float)r_refdef.vrectbottom;
    r_refdef.fvrectbottom_adj = (float)r_refdef.vrectbottom - 0.5;

    r_refdef.aliasvrect.x = (int)(r_refdef.vrect.x * r_aliasuvscale);
    r_refdef.aliasvrect.y = (int)(r_refdef.vrect.y * r_aliasuvscale);
    r_refdef.aliasvrect.width = (int)(r_refdef.vrect.width * r_aliasuvscale);
    r_refdef.aliasvrect.height = (int)(r_refdef.vrect.height * r_aliasuvscale);
    r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
                               r_refdef.aliasvrect.width;
    r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
                                r_refdef.aliasvrect.height;

//qbism: Aardappel fisheye
    if(r_fisheye.value)
    {
        pixelAspect = (float)r_refdef.vrect.height/(float)r_refdef.vrect.width;
        screenAspect = r_refdef.vrect.width*pixelAspect /r_refdef.vrect.height;
    }
    else
    {
        pixelAspect = aspect;
        screenAspect = r_refdef.vrect.width*pixelAspect /r_refdef.vrect.height;
    }
    xOrigin = r_refdef.xOrigin;
    yOrigin = r_refdef.yOrigin;

// 320*200 1.0 pixelAspect = 1.6 screenAspect
// 320*240 1.0 pixelAspect = 1.3333 screenAspect
// proper 320*200 pixelAspect = 0.8333333

    verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

// values for perspective projection
// if math were exact, the values would range from 0.5 to to range+0.5
// hopefully they wll be in the 0.000001 to range+.999999 and truncate
// the polygon rasterization will never render in the first row or column
// but will definately render in the [range] row and column, so adjust the
// buffer origin to get an exact edge to edge fill
    xcenter = ((float)r_refdef.vrect.width * XCENTERING) +
              r_refdef.vrect.x - 0.5;
    aliasxcenter = xcenter * r_aliasuvscale;
    ycenter = ((float)r_refdef.vrect.height * YCENTERING) +
              r_refdef.vrect.y - 0.5;
    aliasycenter = ycenter * r_aliasuvscale;

    xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
    aliasxscale = xscale * r_aliasuvscale;
    xscaleinv = 1.0 / xscale;
    yscale = xscale * pixelAspect;
    aliasyscale = yscale * r_aliasuvscale;
    yscaleinv = 1.0 / yscale;
    xscaleshrink = (r_refdef.vrect.width-6)/r_refdef.horizontalFieldOfView;
    yscaleshrink = xscaleshrink*pixelAspect;

// left side clip
    screenedge[0].normal[0] = -1.0 / (xOrigin*r_refdef.horizontalFieldOfView);
    screenedge[0].normal[1] = 0;
    screenedge[0].normal[2] = 1;
    screenedge[0].type = PLANE_ANYZ;

// right side clip
    screenedge[1].normal[0] =
        1.0 / ((1.0-xOrigin)*r_refdef.horizontalFieldOfView);
    screenedge[1].normal[1] = 0;
    screenedge[1].normal[2] = 1;
    screenedge[1].type = PLANE_ANYZ;

// top side clip
    screenedge[2].normal[0] = 0;
    screenedge[2].normal[1] = -1.0 / (yOrigin*verticalFieldOfView);
    screenedge[2].normal[2] = 1;
    screenedge[2].type = PLANE_ANYZ;

// bottom side clip
    screenedge[3].normal[0] = 0;
    screenedge[3].normal[1] = 1.0 / ((1.0-yOrigin)*verticalFieldOfView);
    screenedge[3].normal[2] = 1;
    screenedge[3].type = PLANE_ANYZ;

    for (i=0 ; i<4 ; i++)
        VectorNormalize (screenedge[i].normal);

    res_scale = sqrt ((double)(r_refdef.vrect.width * r_refdef.vrect.height) /
                      (320.0 * 152.0)) *
                (2.0 / r_refdef.horizontalFieldOfView);

    if (scr_fov.value <= 90.0)
        r_fov_greater_than_90 = false;
    else
        r_fov_greater_than_90 = true;
    D_ViewChanged ();
}


/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
    byte	*vis;
    mnode_t	*node;
    int		i;

    if (r_oldviewleaf == r_viewleaf)
        return;

    r_visframecount++;
    r_oldviewleaf = r_viewleaf;

    vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

    for (i=0 ; i<cl.worldmodel->numleafs ; i++)
    {
        if (vis[i>>3] & (1<<(i&7)))
        {
            node = (mnode_t *)&cl.worldmodel->leafs[i+1];
            do
            {
                if (node->visframe == r_visframecount)
                    break;
                node->visframe = r_visframecount;
                node = node->parent;
            }
            while (node);
        }
    }
}


/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntity (int i) // Manoel Kasimier
{
    if (i != -666) // Manoel Kasimier
        currententity = cl_visedicts[i];
    if (currententity == &cl_entities[cl.viewentity])
        // 2000-01-09 ChaseCam fix by FrikaC  start
    {
        if (!chase_active.value)
        {
            // 2000-01-09 ChaseCam fix by FrikaC  end
            return;	// don't draw the player
            // 2000-01-09 ChaseCam fix by FrikaC  start
        }
        else
        {
//				currententity->angles[0] *= 0.3;
            currententity->angles[0] = 0;
            currententity->angles[2] = 0;
        }
    }
    // 2000-01-09 ChaseCam fix by FrikaC  end

    switch (currententity->model->type)
    {
    case mod_sprite:
        VectorCopy (currententity->origin, r_entorigin);
        VectorSubtract (r_origin, r_entorigin, modelorg);
        R_DrawSprite ();
        break;

    case mod_alias:
        VectorCopy (currententity->origin, r_entorigin);
        VectorSubtract (r_origin, r_entorigin, modelorg);
        R_AliasDrawModel (/* &lighting */); // Manoel Kasimier - changed alias models lighting - edited

        break;

    default:
        break;
    }
}
void R_DrawEntitiesOnList (void)
{
    int			i; // Manoel Kasimier

    if (!r_drawentities.value)
        return;

    for (i=0 ; i<cl_numvisedicts ; i++)
    {
//		cl_visedicts[i]->effects -= cl_visedicts[i]->effects & (EF_CELSHADING|EF_REFLECTIVE); // for betatesting
        // Manoel Kasimier - begin
        if (cl_visedicts[i]->alpha != ENTALPHA_DEFAULT			// Manoel Kasimier - QC Alpha
                || cl_visedicts[i]->effects & (EF_ADDITIVE|EF_SHADOW|EF_CELSHADING))	// Manoel Kasimier
            continue; // skip translucent objects
        R_DrawEntity (i);
        // Manoel Kasimier - end
    }
    // Manoel Kasimier - EF_CELSHADING - begin
    for (i=0 ; i<cl_numvisedicts ; i++)
    {
        vec3_t dist;
        float distlen;
        if (!(cl_visedicts[i]->effects & EF_CELSHADING))
            continue;
        cl_visedicts[i]->colormap = colormap_cel;
        VectorSubtract (r_refdef.vieworg, cl_visedicts[i]->origin, dist);
        distlen = Length(dist);
#if 0
        if (distlen < 250.0)
#endif
            r_drawoutlines = 1;
#if 0
        else if (distlen < 350.0)
            r_drawoutlines = 6;
        else if (distlen < 420.0) // 450
            r_drawoutlines = 3;
        else
            r_drawoutlines = false;
#endif

        R_DrawEntity (i);
    }
    r_drawoutlines = false;
    // Manoel Kasimier - EF_CELSHADING - end
    // Manoel Kasimier - EF_SHADOW - begin
#if 0
    if (!((int)developer.value & 16)) // for betatesting
#endif
        for (i=0 ; i<cl_numvisedicts ; i++)
        {
            if (!(cl_visedicts[i]->effects & EF_SHADOW))
                continue;
            R_DrawEntity (i);
        }
    // Manoel Kasimier - EF_SHADOW - end
    // Manoel Kasimier - QC Alpha - begin
    for (i=0 ; i<cl_numvisedicts ; i++)
    {
        if(cl_visedicts[i]->alpha == ENTALPHA_DEFAULT
                && !(cl_visedicts[i]->effects & EF_ADDITIVE))	// Manoel Kasimier - additive rendering
            continue;
        if (cl_visedicts[i]->effects & EF_SHADOW)
            continue;
        R_DrawEntity (i);
    }
    // Manoel Kasimier - QC Alpha - end
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (int opaque)
{
    /* // Manoel Kasimier - changed alias models lighting - removed - begin
    // FIXME: remove and do real lighting
    float		lightvec[3] = {-1, 0, 0};
    int			j;
    int			lnum;
    vec3_t		dist;
    float		add;
    dlight_t	*dl;
    */ // Manoel Kasimier - changed alias models lighting - removed - end

    currententity = &cl.viewent;
    // Manoel Kasimier - edited - begin
    if ((!currententity->model)
            || (cl.stats[STAT_HEALTH] <= 0)
//	|| ((cl.items & IT_INVISIBILITY) && (r_framecount & 1)) // Manoel Kasimier - stipple alpha - removed
            || (chase_active.value)
            || (!r_drawviewmodel.value) // || r_fov_greater_than_90)
       )
        // Manoel Kasimier - edited - end
    {
        currententity->reset_frame_interpolation = true; // Manoel Kasimier - model interpolation
        return;
    }
    VectorCopy (currententity->origin, r_entorigin);
    VectorSubtract (r_origin, r_entorigin, modelorg);
    // Manoel Kasimier - QC Alpha Scale Begin
    if (cl.items & IT_INVISIBILITY)
    {
        if (cl_entities[cl.viewentity].alpha == ENTALPHA_DEFAULT)
            currententity->alpha = 96;
        else
            currententity->alpha = cl_entities[cl.viewentity].alpha;
    }
    else
        currententity->alpha = cl_entities[cl.viewentity].alpha;
    currententity->effects = cl_entities[cl.viewentity].effects;
    if (opaque && (currententity->alpha != ENTALPHA_DEFAULT || (currententity->effects & EF_ADDITIVE)))
        return;
    if (!opaque && currententity->alpha == ENTALPHA_DEFAULT && !(currententity->effects & EF_ADDITIVE))
        return;
    currententity->scale2 = cl_entities[cl.viewentity].scale2;
    VectorCopy (cl_entities[cl.viewentity].scalev, currententity->scalev);
    // Manoel Kasimier - QC Alpha Scale End

    if (r_dspeeds.value) // Manoel Kasimier
        dv_time1 = Sys_DoubleTime (); // Manoel Kasimier - draw viewmodel time

    // Manoel Kasimier - EF_CELSHADING - begin
    if (currententity->effects & EF_CELSHADING)
    {
        r_drawoutlines = 1;
        R_AliasDrawModel ();
        r_drawoutlines = false;
    }
    else
        // Manoel Kasimier - EF_CELSHADING - end
        R_AliasDrawModel (/* &r_viewlighting */); // Manoel Kasimier - changed alias models lighting - edited
    if (r_dspeeds.value) // Manoel Kasimier
        dv_time2 = Sys_DoubleTime (); // Manoel Kasimier - draw viewmodel time
}

/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox (model_t *clmodel, float *minmaxs)
{
    int			i, *pindex, clipflags;
    vec3_t		acceptpt, rejectpt;
    double		d;

    clipflags = 0;

    if (currententity->angles[0] || currententity->angles[1]
            || currententity->angles[2])
    {
        for (i=0 ; i<4 ; i++)
        {
            d = DotProduct (currententity->origin, view_clipplanes[i].normal);
            d -= view_clipplanes[i].dist;

            if (d <= -clmodel->radius)
                return BMODEL_FULLY_CLIPPED;

            if (d <= clmodel->radius)
                clipflags |= (1<<i);
        }
    }
    else
    {
        for (i=0 ; i<4 ; i++)
        {
            // generate accept and reject points
            // FIXME: do with fast look-ups or integer tests based on the sign bit
            // of the floating point values

            pindex = pfrustum_indexes[i];

            rejectpt[0] = minmaxs[pindex[0]];
            rejectpt[1] = minmaxs[pindex[1]];
            rejectpt[2] = minmaxs[pindex[2]];

            d = DotProduct (rejectpt, view_clipplanes[i].normal);
            d -= view_clipplanes[i].dist;

            if (d <= 0)
                return BMODEL_FULLY_CLIPPED;

            acceptpt[0] = minmaxs[pindex[3+0]];
            acceptpt[1] = minmaxs[pindex[3+1]];
            acceptpt[2] = minmaxs[pindex[3+2]];

            d = DotProduct (acceptpt, view_clipplanes[i].normal);
            d -= view_clipplanes[i].dist;

            if (d <= 0)
                clipflags |= (1<<i);
        }
    }

    return clipflags;
}


//qbism R_DepthSortAliasEntities and R_SortAliasEntities from reckless
/*
=============
R_DepthSortAliasEntities

sort entities by depth.
=============
*/
int R_DepthSortAliasEntities (const void *a, const void *b)
{
    static int   offset;
    entity_t   *enta = *((entity_t **) a);
    entity_t   *entb = *((entity_t **) b);

    // sort back to front
    if (enta->distance > entb->distance)
    {
        offset = 1;
    }
    else if (enta->distance < entb->distance)
    {
        offset = -1;
    }
    else
    {
        offset = 0;
    }
    return offset;
}

/*
=============
R_SortAliasEntities
=============
*/
void R_SortAliasEntities (void)
{
    int    i;

    // if there's only one then the list is already sorted!
    if (cl_numvisedicts > 1)
    {
        // evaluate distances
        for (i = 0; i < cl_numvisedicts; i++)
        {
            entity_t *ent = cl_visedicts[i];

            // set distance from viewer - no need to sqrt them as the order will be the same
            ent->distance = (ent->origin[0] - r_origin[0]) * (ent->origin[0] - r_origin[0]) +
                            (ent->origin[1] - r_origin[1]) * (ent->origin[1] - r_origin[1]) +
                            (ent->origin[2] - r_origin[2]) * (ent->origin[2] - r_origin[2]);
        }

        if (cl_numvisedicts == 2)
        {
            // trivial case - 2 entities
            if (cl_visedicts[0]->distance < cl_visedicts[1]->distance)
            {
                entity_t *tmpent = cl_visedicts[1];

                // reorder correctly
                cl_visedicts[1] = cl_visedicts[0];
                cl_visedicts[0] = tmpent;
            }
        }
        else
        {
            // general case - depth sort the transedicts from back to front
            qsort ((void *) cl_visedicts, cl_numvisedicts, sizeof (entity_t *), R_DepthSortAliasEntities);
        }
    }
}


/*
=============
R_DrawBEntitiesOnList
=============
*/
void R_DrawBEntitiesOnList (void)
{
    int			i, j, k, clipflags;
    vec3_t		oldorigin;
    model_t		*clmodel;
    float		minmaxs[6];

    if (!r_drawentities.value)
        return;

    VectorCopy (modelorg, oldorigin);
    insubmodel = true;

    for (i=0 ; i<cl_numvisedicts ; i++)
    {
        currententity = cl_visedicts[i];

        switch (currententity->model->type)
        {
        case mod_brush:

            clmodel = currententity->model;

            // see if the bounding box lets us trivially reject, also sets
            // trivial accept status
            for (j=0 ; j<3 ; j++)
            {
                minmaxs[j] = currententity->origin[j] +
                             clmodel->mins[j];
                minmaxs[3+j] = currententity->origin[j] +
                               clmodel->maxs[j];
            }

            clipflags = R_BmodelCheckBBox (clmodel, minmaxs);

            if (clipflags != BMODEL_FULLY_CLIPPED)
            {
                VectorCopy (currententity->origin, r_entorigin);
                VectorSubtract (r_origin, r_entorigin, modelorg);
                // FIXME: is this needed?
                //	VectorCopy (modelorg, r_worldmodelorg); // Manoel Kasimier - removed

                r_pcurrentvertbase = clmodel->vertexes;

                // FIXME: stop transforming twice
                R_RotateBmodel ();

                // calculate dynamic lighting for bmodel if it's not an
                // instanced model
                if (clmodel->firstmodelsurface != 0)
                    R_PushDlights (clmodel->nodes + clmodel->hulls[0].firstclipnode); //qbism - from MH

                r_pefragtopnode = NULL;

                for (j=0 ; j<3 ; j++)
                {
                    r_emins[j] = minmaxs[j];
                    r_emaxs[j] = minmaxs[3+j];
                }

                R_SplitEntityOnNode2 (cl.worldmodel->nodes);

                if (r_pefragtopnode)
                {
                    currententity->topnode = r_pefragtopnode;

                    if (r_pefragtopnode->contents >= 0)
                    {
                        // not a leaf; has to be clipped to the world BSP
                        r_clipflags = clipflags;
                        R_DrawSolidClippedSubmodelPolygons (clmodel);
                    }
                    else
                    {
                        // falls entirely in one leaf, so we just put all the
                        // edges in the edge list and let 1/z sorting handle
                        // drawing order
                        R_DrawSubmodelPolygons (clmodel, clipflags);
                    }

                    currententity->topnode = NULL;
                }


                // put back world rotation and frustum clipping
                // FIXME: R_RotateBmodel should just work off base_vxx
                VectorCopy (base_vpn, vpn);
                VectorCopy (base_vup, vup);
                VectorCopy (base_vright, vright);
                VectorCopy (base_modelorg, modelorg);
                VectorCopy (oldorigin, modelorg);
                R_TransformFrustum ();
            }

            break;

        default:
            break;
        }
    }

    insubmodel = false;
}


/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing (void)
{
    edge_t	ledges[NUMSTACKEDGES +
                   ((CACHE_SIZE - 1) / sizeof(edge_t)) + 1];
    surf_t	lsurfs[NUMSTACKSURFACES +
                   ((CACHE_SIZE - 1) / sizeof(surf_t)) + 1];

    if (auxedges)
    {
        r_edges = auxedges;
    }
    else
    {
        r_edges =  (edge_t *)
                   (((long)&ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
    }

    if (r_surfsonstack)
    {
        surfaces =  (surf_t *)
                    (((long)&lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
        surf_max = &surfaces[r_cnumsurfs];
        // surface 0 doesn't really exist; it's just a dummy because index 0
        // is used to indicate no edge attached to surface
        surfaces--;
    }

    R_BeginEdgeFrame ();

    if (r_dspeeds.value)
    {
        rw_time1 = Sys_DoubleTime ();
    }

    R_RenderWorld ();

    if (r_dspeeds.value)
    {
        rw_time2 = Sys_DoubleTime ();
        db_time1 = rw_time2;
    }

    R_DrawBEntitiesOnList ();

    if (r_dspeeds.value)
    {
        db_time2 = Sys_DoubleTime ();
        se_time1 = db_time2;
    }

    if (!r_dspeeds.value)
    {
        S_ExtraUpdate ();	// don't let sound get messed up if going slow
    }

    R_ScanEdges ();
}


/*
================
R_RenderView

r_refdef must be set before the first call
================
*/

void R_RenderView (void) //qbism- so can only setup frame once, for fisheye and stereo.
{
    int		dummy;
    int		delta;
    //qbism - for fog
    int			x, y, level, fogindex, dither, xref;
    float   density_factor;
    byte		*pbuf;
    short		*pz;
    extern short		*d_pzbuffer;
    extern unsigned int	d_zwidth;
    extern int			d_scantable[1024];

    //This causes problems for Flash when not using -O3
#if !defined(FLASH)
    delta = (byte *)&dummy - r_stack_start;
    if (delta < -0x10000 || delta > 0x10000) //qbism was 10000. D_SQ_calloc is 0x10000.  Does it matter?
        Sys_Error ("R_RenderView: called without enough stack");
#endif

    if ( Hunk_LowMark() & 3 )
        Sys_Error ("Hunk is missaligned");

    if ( (long)(&dummy) & 3 )
        Sys_Error ("Stack is missaligned");

    if ( (long)(&r_warpbuffer) & 3 )
        Sys_Error ("Globals are missaligned");

    //byte	warpbuffer[WARP_WIDTH * WARP_HEIGHT]; // Manoel Kasimier - hi-res waterwarp & buffered video - removed
    // Manoel Kasimier - hi-res waterwarp & buffered video - begin

    if (warpbuffer)
        Q_free(warpbuffer);
    warpbuffer = Q_malloc(vid.rowbytes*vid.height);
    // Manoel Kasimier - hi-res waterwarp & buffered video - end

    r_warpbuffer = warpbuffer;

    if (r_timegraph.value || r_speeds.value || r_dspeeds.value)
        r_time1 = Sys_DoubleTime ();

    R_SetupFrame ();
    currententity = &cl_entities[0];
        R_PushDlights (cl.worldmodel->nodes);  //qbism - moved here from view.c

#ifdef PASSAGES
    SetVisibilityByPassages ();
#else
    R_MarkLeaves ();	// done here so we know if we're in water
#endif

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c

    if (!cl_entities[0].model || !cl.worldmodel)
        Sys_Error ("R_RenderView: NULL worldmodel");

    if (!r_dspeeds.value)
    {
        S_ExtraUpdate ();	// don't let sound get messed up if going slow
    }

    r_foundwater = r_drawwater = false; // Manoel Kasimier - translucent water
#if 0
    if (((int)developer.value & 64)) // Manoel Kasimier - for betatesting
    {
        extern short		*d_pzbuffer;
        memset (d_pzbuffer, 0, vid.width * vid.height * sizeof(d_pzbuffer));
        memset (vid.buffer, 8, vid.width * vid.height);
        memset (r_warpbuffer, 8, vid.width * vid.height);
    }
    else
#endif
        R_EdgeDrawing ();

    if (!r_dspeeds.value)
    {
        S_ExtraUpdate ();	// don't let sound get messed up if going slow
    }

    if (r_dspeeds.value)
    {
        se_time2 = Sys_DoubleTime (); // scan edges time
//		de_time1 = se_time2; // draw entities time
    }
//   R_DrawViewModel (true); qbism - move after particles

    if (r_dspeeds.value) // Manoel Kasimier
        de_time1 = Sys_DoubleTime (); // Manoel Kasimier - draw entities time
        CL_UpdateTEnts (); // Manoel Kasimier

    R_SortAliasEntities(); //qbism from reckless
    R_DrawEntitiesOnList ();


    if (r_dspeeds.value)
    {
        de_time2 = Sys_DoubleTime (); // draw entities time
        dp_time1 = de_time2; // draw particles time
    }

    R_DrawParticles ();

    if (r_dspeeds.value)
        dp_time2 = Sys_DoubleTime (); // draw particles time

    // Manoel Kasimier - translucent water - begin
    if (r_foundwater)
    {
        r_drawwater = true;
        R_EdgeDrawing ();
    }
    // Manoel Kasimier - translucent water - end

    R_DrawViewModel (true); //qbism draw after particles.  it's worth the overdraw.
    R_DrawViewModel (false); // Manoel Kasimier

    //qbism- originally based on Makaqu 1.3 fog.  added global fog, dithering, optimizing
    static float previous_fog_density;
    if (fog_density && r_fog.value)
    {
        dither=0;
        if(previous_fog_density != fog_density)
            FogDitherInit(); //dither includes density factor, so regenerate when it changes
        fogindex = 32*256 + palmapnofb[(int)(fog_red*164)>>3][(int)(fog_green*164) >>3][(int)(fog_blue*164)>>3]; //qbism -fractional value, bright fog is harsh
        for (y=0 ; y<r_refdef.vrect.height; y++)
        {
            pbuf = r_warpbuffer + d_scantable[y+r_refdef.vrect.y];
            pz = d_pzbuffer + (d_zwidth * (y+r_refdef.vrect.y));
            for (x=0 ; x<r_refdef.vrect.width; x++)
            {
                xref = x+r_refdef.vrect.x;
                level = (int)( *(pz + xref) * ditherfog[dither++ % DITHER_NUMRANDS]);
                if (level < 32 && level >= 1) //qb:  0 is sky.  Assumption is that mapper's sky selection looks appropriately foggy.
                    *(pbuf + xref) = fogmap[*(pbuf + xref) + (int)vid.colormap[fogindex + level*256]*256];
            }
        }
        previous_fog_density = fog_density;
    }

    // Manoel Kasimier - buffered video (bloody hack) - begin
#ifdef _WIN32
    if (r_dowarp)
        D_WarpScreen ();
    else // copy buffer to video
    {
        int		i;
        byte	*src = r_warpbuffer + scr_vrect.y * vid.width + scr_vrect.x;
        byte	*dest = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;
        for (i=0 ; i<scr_vrect.height ; i++, src += vid.width, dest += vid.rowbytes)
            memcpy(dest, src, scr_vrect.width);
    }
#else
    // Manoel Kasimier - buffered video (bloody hack) - end
    if (r_dowarp)
        D_WarpScreen ();
#endif // Manoel Kasimier - buffered video (bloody hack)

    V_SetContentsColor (r_viewleaf->contents);
        if (r_timegraph.value)
            R_TimeGraph ();

        if (r_aliasstats.value)
            R_PrintAliasStats ();

        if (r_speeds.value)
            R_PrintTimes ();

        if (r_dspeeds.value)
            R_PrintDSpeeds ();

        if (r_reportsurfout.value && r_outofsurfaces)
            Con_Printf ("Short %d surfaces\n", r_outofsurfaces);

        if (r_reportedgeout.value && r_outofedges)
            Con_Printf ("Short roughly %d edges\n", r_outofedges * 2 / 3);
}

/*
================
R_InitTurb
================
*/
void R_InitTurb (void)
{
    int		i;

    for (i=0 ; i<(SIN_BUFFER_SIZE) ; i++)
    {
        sintable[i] = AMP + sin(i*3.14159*2/CYCLE)*AMP;
        intsintable[i] = AMP2 + sin(i*3.14159*2/CYCLE)*AMP2;	// AMP2, not 20
    }
}

