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
// r_shared.h: general refresh-related stuff shared between the refresh and the
// driver

// FIXME: clean up and move into d_iface.h

#ifndef _R_SHARED_H_
#define _R_SHARED_H_

#define	MAXVERTS	16					// max points in a surface polygon
#define MAXWORKINGVERTS	(MAXVERTS+4)	// max points in an intermediate
										//  polygon (while processing)
#define	MAXHEIGHT		1080 //qb: was 1024.  Why not more?
#define	MAXWIDTH		1920 //qb: was 1280
#define MAXDIMENSION	((MAXHEIGHT > MAXWIDTH) ? MAXHEIGHT : MAXWIDTH)

#define SIN_BUFFER_SIZE	(MAXDIMENSION+CYCLE)

//===================================================================

extern void	R_DrawLine (polyvert_t *polyvert0, polyvert_t *polyvert1);

extern int		cachewidth;
extern byte	*cacheblock;
extern int		screenwidth;

extern	float	pixelAspect;

extern int		r_drawnpolycount;

extern cvar_t	r_clearcolor;

//qb: particle cvars
extern cvar_t r_part_scale;         //global particle scale
extern cvar_t r_part_blob_count;    //quantity of particles for blob explosion
extern cvar_t r_part_blob_time;     //life span of blob particles
extern cvar_t r_part_blob_vel;      //velocity of blob particles
extern cvar_t r_part_explo1_count;
extern cvar_t r_part_explo1_time;
extern cvar_t r_part_explo1_vel;
extern cvar_t r_part_explo2_count;
extern cvar_t r_part_explo2_time;
extern cvar_t r_part_explo2_vel;
extern cvar_t r_part_sticky_time;   //new type for gib trail or similar, sticks to walls and floors

extern int	sintable[SIN_BUFFER_SIZE];

extern	vec3_t	vup, base_vup;
extern	vec3_t	vpn, base_vpn;
extern	vec3_t	vright, base_vright;
extern	entity_t		*currententity;

//qb: increased to solve dropped faces in maps with large open spaces like ALK08DM
#define NUMSTACKEDGES		8192  //qb: 8192 per qsb - was 2400
#define	MINEDGES			NUMSTACKEDGES
#define NUMSTACKSURFACES	8192  //qb: 8192 per qsb - was 800
#define MINSURFACES			NUMSTACKSURFACES
#define	MAXSPANS			8192 //qb: was 3000.

typedef struct espan_s
{
	int				u, v, count;
	struct espan_s	*pnext;
} espan_t;

// FIXME: compress, make a union if that will help
// insubmodel is only 1, flags is fewer than 32, spanstate could be a byte
typedef struct surf_s
{
	struct surf_s	*next;			// active surface stack in r_edge.c
	struct surf_s	*prev;			// used in r_edge.c for active surf stack
	struct espan_s	*spans;			// pointer to linked list of spans to draw
	int			key;				// sorting key (BSP order)
	int			last_u;				// set during tracing
	int			spanstate;			// 0 = not in span
									// 1 = in span
									// -1 = in inverted span (end before
									//  start)
	int			flags;				// currentface flags
	void		*data;				// associated data like msurface_t
	entity_t	*entity;
	float		nearzi;				// nearest 1/z on surface, for mipmapping
	qboolean	insubmodel;
	float		d_ziorigin, d_zistepu, d_zistepv;

	int			pad[2];				// to 64 bytes
} surf_t;

extern	surf_t	*surfaces, *surface_p, *surf_max;

// surfaces are generated in back to front order by the bsp, so if a surf
// pointer is greater than another one, it should be drawn in front
// surfaces[1] is the background, and is used as the active surface stack.
// surfaces[0] is a dummy, because index 0 is used to indicate no surface
//  attached to an edge_t

//===================================================================

extern vec3_t	sxformaxis[4];	// s axis transformed into viewspace
extern vec3_t	txformaxis[4];	// t axis transformed into viewspac

extern vec3_t	modelorg, base_modelorg;

extern	float	xcenter, ycenter;
extern	float	xscale, yscale;
extern	float	xscaleinv, yscaleinv;
extern	float	xscaleshrink, yscaleshrink;

extern	int d_lightstylevalue[256]; // 8.8 frac of base light value

extern void TransformVector (vec3_t in, vec3_t out);
extern void SetUpForLineScan(fixed8_t startvertu, fixed8_t startvertv,
	fixed8_t endvertu, fixed8_t endvertv);

//extern int	r_skymade; // Manoel Kasimier - smooth sky - removed
//extern void R_MakeSky (void); // Manoel Kasimier - smooth sky - removed

extern int	ubasestep, errorterm, erroradjustup, erroradjustdown;

// flags in finalvert_t.flags
#define ALIAS_LEFT_CLIP				0x0001
#define ALIAS_TOP_CLIP				0x0002
#define ALIAS_RIGHT_CLIP			0x0004
#define ALIAS_BOTTOM_CLIP			0x0008
#define ALIAS_Z_CLIP				0x0010
#define ALIAS_ONSEAM				0x0020	// also defined in modelgen.h;
											//  must be kept in sync
#define ALIAS_XY_CLIP_MASK			0x000F

typedef struct edge_s
{
	fixed16_t		u;
	fixed16_t		u_step;
	struct edge_s	*prev, *next;
	unsigned short	surfs[2];
	struct edge_s	*nextremove;
	float			nearzi;
	medge_t			*owner;
} edge_t;

#endif	// _R_SHARED_H_


