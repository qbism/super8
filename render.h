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

// refresh.h -- public interface to refresh functions

#define	MAXCLIPPLANES	11

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;

// Manoel Kasimier - r_light_style - begin
extern cvar_t r_light_vec_x;
extern cvar_t r_light_vec_y;
extern cvar_t r_light_vec_z;
extern cvar_t r_light_style;
// Manoel Kasimier - r_light_style - end

extern cvar_t  r_fisheye; //qb: fisheye added
extern cvar_t	scr_fov;

extern cvar_t r_interpolation; // Manoel Kasimier - model interpolation
extern cvar_t r_wateralpha; // Manoel Kasimier - translucent water
extern cvar_t r_glassalpha; //qb: *glass
byte r_foundwater, r_drawwater; // Manoel Kasimier - translucent water
extern byte       *alphamap, *additivemap, *fogmap; // Manoel Kasimier - transparencies
extern byte       *lightcolormap;  //qb: light colors

typedef struct entity_s
{
	qboolean				forcelink;		// model changed

	int						update_type;

	entity_state_t			baseline;		// to fill in defaults in updates

	double					msgtime;		// time of last update
	vec3_t					msg_origins[2];	// last two updates (0 is newest)
	vec3_t					origin;
	vec3_t					msg_angles[2];	// last two updates (0 is newest)
	vec3_t					angles;
	struct model_s			*model;			// NULL = no model
	struct efrag_s			*efrag;			// linked list of efrags
	int						frame;
	float					syncbase;		// for client-side animations
	byte					*colormap;
	int						effects;		// light, particals, etc
	int						modelflags;		//qb: for DP model flag override
	int						skinnum;		// for Alias models
	int						visframe;		// last frame this entity was
											//  found in an active leaf

	int						dlightframe;	// dynamic lighting
	//qb: not used here...  int						dlightbits;

// FIXME: could turn these into a union
	int						trivial_accept;
	struct mnode_s			*topnode;		// for bmodels, first world node
											//  that splits bmodel, or NULL if
											//  not split
	// Manoel Kasimier - model interpolation - begin
	struct model_s			*lastmodel;
	int			shadowsize;	//qb: engoo// leilei - shadow size hack

	qboolean				reset_frame_interpolation; // Manoel Kasimier
	float                   frame_start_time;
	float                   frame_interval; // Manoel Kasimier - QC frame_interval
	struct maliasgroup_s	*framegroup1; // Manoel Kasimier
	int                     pose1;
	struct maliasgroup_s	*framegroup2; // Manoel Kasimier - renamed
	int                     pose2;
	float                   translate_start_time;
	vec3_t                  origin1;
	vec3_t                  origin2;
	float                   rotate_start_time;
	vec3_t                  angles1;
	vec3_t                  angles2;
	// Manoel Kasimier - model interpolation - end
	// Tomaz - QC Alpha Scale Glow Begin
	byte					alpha;
	float					scale_start_time;  //qb: fixme - also convert these to bytes (if kept...)
//	float					scale1;
	float					scale2;
	vec3_t                  scalev;
	int	    	glow_size;  //qb: changed to int (passed as short)
	byte		glow_red;   //qb: change to byte
	byte		glow_green;
	byte		glow_blue;
	// Tomaz - QC Alpha Scale Glow End
	float distance; //qb: from reckless, depth sorting
} entity_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh
									// FIXME: not need vrect next field here?
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	float		vrectrightedge;			// rightmost right edge we care about,
										//  for use in edge list
	float		fvrectx, fvrecty;		// for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int			vrect_x_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj;
										// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible
										// 2.0 = 90 degrees
	float		xOrigin;			// should probably allways be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	float		fov_x, fov_y;

	int			ambientlight;
} refdef_t;


//
// refresh
//
extern	int		reinit_surfcache;


extern	refdef_t	r_refdef;
extern vec3_t	r_origin, vpn, vright, vup;

extern	struct texture_s	*r_notexture_mip;

float fovscale; // Manoel Kasimier - FOV-based scaling

void R_Init (void);
void R_InitTextures (void);
void R_InitEfrags (void);
void R_RenderView (void);		//qb: do lerp once in fisheye // must set r_refdef first
void R_ViewChanged (vrect_t *pvrect, int lineadj);
								// called whenever r_refdef or vid change
void R_InitSky (struct texture_s *mt);	// called at level load
void R_LoadSky (char *s); // Manoel Kasimier - skyboxes
void LoadPCX (char *filename, byte **pic, int *width, int *height); // Manoel Kasimier - skyboxes
void LoadTGA_as8bit (char *filename, byte **pic, int *width, int *height); // Manoel Kasimier //qb: MK 1.4


void R_AddEfrags (entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

void R_NewMap (void);


void R_ParseParticleEffect (void);
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail (vec3_t start, vec3_t end, int type);

void R_DarkFieldParticles (entity_t *ent);
void R_EntityParticles (entity_t *ent);
void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);

//
// surface cache related
//
extern	int		reinit_surfcache;	// if 1, surface cache is currently empty and
extern qboolean	r_cache_thrash;	// set if thrashing the surface cache

int	D_SurfaceCacheForRes (int width, int height);
void D_FlushCaches (void);
void D_DeleteSurfaceCache (void);
void D_InitCaches (void *buffer, int size);
void R_SetVrect (vrect_t *pvrect, vrect_t *pvrectin, int lineadj);

//qb: ftestain
void R_AddStain(vec3_t org, float tint, float radius);
void R_LessenStains(void);
