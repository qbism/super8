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

#ifndef __MODEL__
#define __MODEL__

#ifdef INCLUDELIBS

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "cmdlib.h"
#include "scriplib.h"
#include "trilib.h"
#include "lbmlib.h"
#include "mathlib.h"

#endif

#include "spritegn.h"

#define ALIAS_VERSION	6

#define ALIAS_ONSEAM				0x0020

// must match definition in spritegn.h
#ifndef SYNCTYPE_T
#define SYNCTYPE_T
typedef enum {ST_SYNC=0, ST_RAND } synctype_t;
#endif

typedef enum { ALIAS_SINGLE=0, ALIAS_GROUP } aliasframetype_t;

typedef enum { ALIAS_SKIN_SINGLE=0, ALIAS_SKIN_GROUP } aliasskintype_t;


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

#define	MD2MAX_TRIANGLES	4096
#define MD2MAX_VERTS		2048
#define MD2MAX_FRAMES		512
#define MD2MAX_SKINS		32
#define	MD2MAX_SKINNAME		64

#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_DRAWSKYBOX		0x80		// Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
#define SURF_DRAWTRANSLUCENT	0x100	// Manoel Kasimier - translucent water
#define SURF_DRAWFENCE		0x200   //qb: textures with holes
#define SURF_DRAWGLASS33	0x400   //qb: glass (non-turbulent transparent)
#define SURF_DRAWGLASS50	0x800   //qb: glass (non-turbulent transparent)
#define SURF_DRAWGLASS66	0x1000   //qb: glass (non-turbulent transparent)
#define SURF_NOTEXTURE  	0x2000   //qb: from FQ

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/
/*qb: moved to model effects
#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail
*/

//
// in memory representation
//
// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vec3_t		position;
} mvertex_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


// plane_t structure
typedef struct mplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for texture axis selection and fast side tests
	byte	signbits;		// signx + signy<<1 + signz<<1
	byte	pad[2];
} mplane_t;

typedef struct texture_s
{
	char		name[16];
	unsigned	width, height;
	int			anim_total;				// total tenths in sequence ( 0 = no)
	int			anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s *anim_next;		// in the animation sequence
	struct texture_s *alternate_anims;	// bmodels in frmae 1 use these
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
} texture_t;


//qb:  johnfitz begin -- for clipnodes>32k
typedef struct mclipnode_s
{
	int			planenum;
	int			children[2]; // negative numbers are contents
} mclipnode_t;
//qb:  johnfitz end

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
	mclipnode_t	*clipnodes; //qb:  johnfitz -- was dclipnode_t
	mplane_t	*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	unsigned int	v[2]; //qb: upped for bsp2
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct
{
	float		vecs[2][4];
	float		mipadjust;
	texture_t	*texture;
	int			flags;
} mtexinfo_t;

typedef struct msurface_s
{
	int			visframe;		// should be drawn when node is crossed

	int			dlightframe;
	int			dlightbits[(MAX_DLIGHTS + 31) >> 5];  //qb: from MH - increase max_dlights
	int			lightmaptexturenum;  //qb: ftestain
	int			light_s, light_t;	//qb: ftestain lightmap coordinates
	qboolean    stained; //qb: ftestain

	int			shadowframe; //qb: engoo shadowhack
	int			shadowbits;

	mplane_t	*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges

// surface generation data
	struct surfcache_s	*cachespots[MIPLEVELS];

	short		texturemins[2];
	short		extents[2];

	mtexinfo_t	*texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	byte		*samples;		// [numstyles*surfsize]
	byte		*colorsamples;		//qb: [numstyles*surfsize]

} msurface_t;


typedef struct mnode_s
{
// common with leaf
	int			contents;		// 0, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];

	unsigned int		firstsurface;
	unsigned int		numsurfaces;
} mnode_t;


typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];

	struct mnode_s	*parent;

// leaf specific
	byte		*compressed_vis;
	efrag_t		*efrags;

	msurface_t	**firstmarksurface;
	unsigned short			nummarksurfaces;  //qb: was int
	int			key;			// BSP sequence number for leaf's contents
	byte		ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct {
	byte	v[3];
	byte	lightnormalindex;
} trivertx_t;

typedef struct {
	trivertx_t	bboxmin;	// lightnormal isn't used
	trivertx_t	bboxmax;	// lightnormal isn't used
	char		name[16];	// frame name from grabbing
} daliasframe_t;

typedef struct {
	int			numframes;
	trivertx_t	bboxmin;	// lightnormal isn't used
	trivertx_t	bboxmax;	// lightnormal isn't used
} daliasgroup_t;


typedef struct
{
	aliasframetype_t	type;
	int					firstpose;
	int					numposes;
	float				interval;
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;

	vec3_t		scale;
	vec3_t		scale_origin;

	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct
{
	aliasskintype_t		type;
	void				*pcachespot;
	int					skin;
} maliasskindesc_t;

typedef struct
{
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	int					numframes; // Manoel Kasimier - model interpolation
} maliasgroupframedesc_t;

typedef struct maliasgroup_s // Manoel Kasimier - model interpolation - edited
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

typedef struct
{
	int					numskins;
	int					intervals;
	maliasskindesc_t	skindescs[1];
} maliasskingroup_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
	int					drawn; // Manoel Kasimier - EF_CELSHADING
	int					xyz_index[3];
	int					st_index[3];
	int	pad[2];
} mtriangle_t;

extern int celtri_drawn; // Manoel Kasimier - EF_CELSHADING

//===================================================================

//
// Whole model
//

typedef enum {mod_brush, mod_sprite, mod_alias} modtype_t;

typedef struct
{
	char	searchpath[MAX_OSPATH];
} loadinfo_t; //qb: model search path, from FQ Mark V


typedef struct model_s
{
	char		name[MAX_QPATH];
	qboolean	needload;		// bmodels and sprites don't cache normally
	modtype_t	type;
	int			numframes;
	synctype_t	synctype;
	int			flags;

//
// volume occupied by the model graphics
//
	vec3_t		mins, maxs;
    float radius;

//
// brush model
//
	int			firstmodelsurface, nummodelsurfaces;

	int			numsubmodels;
	dmodel_t	*submodels;

	int			numplanes;
	mplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			numclipnodes;
    mclipnode_t	*clipnodes; //qb:  johnfitz -- was dclipnode_t

	int    nummarksurfaces;
	msurface_t	    **marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	int			numtextures;
	texture_t	**textures;

	byte		*visdata;
	byte		*lightdata;
	byte		*colordata; //qb: indexed colored
	byte		*entities;

    loadinfo_t	loadinfo;  //qb: model search path
//
// additional model data
//
	cache_user_t	cache;		// only access through Mod_Extradata
	int				dontshadow;  //qb: shadowhack from engoo
} model_t;

typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;
} mdl_t;

// TODO: could be shorts

typedef struct {
	int		onseam;
	int		s;
	int		t;
} stvert_t;

typedef struct dtriangle_s {
	int					facesfront;
	int					vertindex[3];
} dtriangle_t;

#define DT_FACES_FRONT				0x0010

// This mirrors trivert_t in trilib.h, is present so Quake knows how to
// load this data

typedef struct {
	int			numskins;
} daliasskingroup_t;

typedef struct {
	float	interval;
} daliasinterval_t;

typedef struct {
	float	interval;
} daliasskininterval_t;

typedef struct {
	aliasframetype_t	type;
} daliasframetype_t;

typedef struct {
	aliasskintype_t	type;
} daliasskintype_t;


//qb: md2 from FTEQW
typedef struct {
	int		s;
	int		t;
} mstvert_t;


typedef struct {
	byte	v[3];
	byte	lightnormalindex;
} dtrivertx_t;
typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numstverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;
} mmdl_t;

#define	MAX_SKINS	32
typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;

	int					model;
	int					stverts;
	int					skindesc;

	int					numposes;
	int					poseverts;
	int					posedata;	// numposes*poseverts trivert_t

	int					baseposedata; //original verts for triangles to reference
	int					triangles; //we need tri data for shadow volumes

	int					commands;	// gl command list with embedded s/t
	int					gl_texturenum[MAX_SKINS][4];
	int					texels[MAX_SKINS];
	maliasframedesc_t	frames[1];	// variable sized
} aliashdr_t;

#define ALIAS_Z_CLIP_PLANE	5
#define	MAXALIASFRAMES	256
#define	MAXALIASTRIS	2048

typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;
} dmdl_t;

// TODO: could be shorts

typedef struct {
	int		onseam;
	int		s;
	int		t;
} dstvert_t;

typedef struct {
	short		s;
	short		t;
} dmd2stvert_t;

typedef struct dmd2triangle_s {
	short					xyz_index[3];
	short					st_index[3];
} dmd2triangle_t;


#define MD2IDALIASHEADER		(('2'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD2ALIAS_VERSION	8

#define	MD2MAX_TRIANGLES	4096
#define MD2MAX_VERTS		2048
#define MD2MAX_FRAMES		512
#define MD2MAX_SKINS		32
#define	MD2MAX_SKINNAME		64
// sanity checking size
#define MD2MAX_SIZE	(1024*4200)

typedef struct
{
	short	s;
	short	t;
} md2stvert_t;

typedef struct
{
	short	index_xyz[3];
	short	index_st[3];
} md2triangle_t;

typedef struct
{
	byte	v[3];			// scaled qbyte to fit in frame mins/maxs
	byte	lightnormalindex;
} md2trivertx_t;

#define MD2TRIVERTX_V0   0
#define MD2TRIVERTX_V1   1
#define MD2TRIVERTX_V2   2
#define MD2TRIVERTX_LNI  3
#define MD2TRIVERTX_SIZE 4

typedef struct
{
	float		scale[3];	// multiply qbyte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing
	md2trivertx_t	verts[1];	// variable sized
} md2frame_t;


// the glcmd format:
// a positive integer starts a tristrip command, followed by that many
// vertex structures.
// a negative integer starts a trifan command, followed by -x vertexes
// a zero indicates the end of the command list.
// a vertex consists of a floating point s, a floating point t,
// and an integer vertex index.


typedef struct
{
	int			ident;
	int			version;

	int			skinwidth;
	int			skinheight;
	int			framesize;		// qbyte size of each frame

	int			num_skins;
	int			num_xyz;
	int			num_st;			// greater than num_xyz for seams
	int			num_tris;
	int			num_glcmds;		// dwords in strip/fan command list
	int			num_frames;

	int			ofs_skins;		// each skin is a MAX_SKINNAME string
	int			ofs_st;			// qbyte offset from start for stverts
	int			ofs_tris;		// offset for dtriangles
	int			ofs_frames;		// offset for first frame
	int			ofs_glcmds;
	int			ofs_end;		// end of file

	int			gl_texturenum[MAX_SKINS];
} md2_t;

#define IDPOLYHEADER	(('O'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD2IDALIASHEADER        (('2'<<24)+('P'<<16)+('D'<<8)+'I')

														// little-endian "IDPO"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/


/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s
{
	int		width;
	int		height;
	void	*pcachespot;			// remove?
	float	up, down, left, right;
	byte	pixels[4];
} mspriteframe_t;

typedef struct
{
	int				numframes;
	float			*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t	type;
	mspriteframe_t		*frameptr;
} mspriteframedesc_t;

typedef struct
{
	int					type;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	float				beamlength;		// remove?
	void				*cachespot;		// remove?
	mspriteframedesc_t	frames[1];
} msprite_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qboolean crash);
void	*Mod_Extradata (model_t *mod);	// handles caching
void	Mod_TouchModel (char *name);
void Mod_SetExtraFlags (model_t *mod);
mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
byte	*Mod_LeafPVS (mleaf_t *leaf, model_t *model);

extern	cvar_t	external_ent;	// 2001-09-12 .ENT support by Maddes


#endif	// __MODEL__
