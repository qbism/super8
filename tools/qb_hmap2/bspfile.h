
#ifndef BSPFILE_H
#define BSPFILE_H

// upper design bounds

#define	MAX_Q1MAP_HULLS			4
#define	MAX_MAP_HULLS			16

#define	MAX_MAP_MODELS			0x1000
#define	MAX_MAP_BRUSHES			0x100000
#define	MAX_MAP_ENTITIES		0x8000
#define	MAX_MAP_ENTSTRING		0x400000

#define	MAX_MAP_PLANES			0x200000
#define	MAX_MAP_NODES			0x200000
#define	MAX_MAP_CLIPNODES		0x800000
#define	MAX_MAP_LEAFS			0x400000
#define	MAX_MAP_VERTS			0x100000
#define	MAX_MAP_FACES			0x200000
#define	MAX_MAP_MARKSURFACES	0x400000
#define	MAX_MAP_TEXINFO			0x100000
#define	MAX_MAP_EDGES			0x400000
#define	MAX_MAP_SURFEDGES		0x800000
// these are pure byte limits on file size
#define	MAX_MAP_MIPTEX			0x1000000
#define	MAX_MAP_LIGHTING		0x1000000
#define	MAX_MAP_VISIBILITY		0x1000000

// used in tjunc
#define	MAX_WVERTS	MAX_MAP_VERTS
#define	MAX_WEDGES	MAX_MAP_VERTS
#define	NUM_HASH	MAX_MAP_VERTS

#define MAXLIGHTS 4096
#define LIGHTCHAINS (MAX_MAP_FACES * 4)

#define DEFAULTLIGHTLEVEL	300

// #define WRITE_LIGHTSFILE

#define MAP_DIRECTLIGHTS	MAX_MAP_ENTITIES

// key / value pair sizes

#define	MAX_KEY		32
#define	MAX_VALUE	1024

//=============================================================================

#define BSPVERSION	29
#define MCBSPVERSION 2
// BSP2 also exists, but its version is "BSP2" not a number

typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES	0
#define	LUMP_PLANES		1
#define	LUMP_TEXTURES	2
#define	LUMP_VERTEXES	3
#define	LUMP_VISIBILITY	4
#define	LUMP_NODES		5
#define	LUMP_TEXINFO	6
#define	LUMP_FACES		7
#define	LUMP_LIGHTING	8
#define	LUMP_CLIPNODES	9
#define	LUMP_LEAFS		10
#define	LUMP_MARKSURFACES 11
#define	LUMP_EDGES		12
#define	LUMP_SURFEDGES	13
#define	LUMP_MODELS		14

#define	HEADER_LUMPS	15

typedef struct
{
	int			numhulls;
	int			filehulls;
	float		hullsizes[MAX_MAP_HULLS][2][3];
} hullinfo_t;

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLS];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} dmodel_t;

typedef struct
{
	int			nummiptex;
	int			dataofs[4];		// [nummiptex]
} dmiptexlump_t;

#define	MIPLEVELS	4
typedef struct miptex_s
{
	char		name[16];
	unsigned	width, height;
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
} miptex_t;


typedef struct
{
	float	point[3];
} dvertex_t;


// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

typedef struct
{
	float	normal[3];
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;



#define	CONTENTS_EMPTY		-1
#define	CONTENTS_SOLID		-2
#define	CONTENTS_WATER		-3
#define	CONTENTS_SLIME		-4
#define	CONTENTS_LAVA		-5
#define	CONTENTS_SKY		-6

// !!! if this is changed, it must be changed in asm_i386.h too !!!

// MODIFIED FOR BSP2
typedef struct
{
	int			planenum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	float		mins[3];		// for sphere culling
	float		maxs[3];
	unsigned int	firstface;  //qb:  taniwha catch, was unsigned short
	unsigned int	numfaces;	//qb:  taniwha catch, was unsigned short // counting both sides
} dnode_t;

// MODIFIED FOR BSP2
typedef struct
{
	int			planenum;
	int			children[2];	// negative numbers are contents
} dclipnode_t;


typedef struct texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			miptex;
	int			flags;
} texinfo_t;
#define	TEX_SPECIAL		1		// sky or slime, no lightmap or 256 subdivision

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
//MODIFIED FOR BSP2
typedef struct
{
	unsigned int	v[2];		// vertex numbers
} dedge_t;

#define	MAXLIGHTMAPS	4

// MODIFIED FOR BSP2
typedef struct
{
	int			planenum;
	int			side;

	int			firstedge;		// we must support > 64k edges
	int			numedges;
	int			texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} dface_t;



#define	AMBIENT_WATER	0
#define	AMBIENT_SKY		1
#define	AMBIENT_SLIME	2
#define	AMBIENT_LAVA	3

#define	NUM_AMBIENTS			4		// automatic ambient sounds

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info

//MODIFIED FOR BSP2
typedef struct
{
	int			contents;
	int			visofs;				// -1 = no visibility info

	float		mins[3];			// for frustum culling
	float		maxs[3];

	unsigned int		firstmarksurface;
	unsigned int		nummarksurfaces;

	byte		ambient_level[NUM_AMBIENTS];
} dleaf_t;

//============================================================================

// the utilities get to be lazy and just use large static arrays

extern	int			nummodels;
extern	dmodel_t	dmodels[MAX_MAP_MODELS];

extern	int			visdatasize;
extern	byte		dvisdata[MAX_MAP_VISIBILITY];

extern	int			lightdatasize;
extern	byte		dlightdata[MAX_MAP_LIGHTING];

// LordHavoc: stored in .lit file
extern	int			rgblightdatasize;
extern	byte		drgblightdata[MAX_MAP_LIGHTING*3];

// LordHavoc: stored in .dlit file
extern	int			nmaplightdatasize;
extern	byte		dnmaplightdata[MAX_MAP_LIGHTING*3];

extern	int			texdatasize;
extern	byte		dtexdata[MAX_MAP_MIPTEX]; // (dmiptexlump_t)

extern	int			entdatasize;
extern	char		dentdata[MAX_MAP_ENTSTRING];

extern	int			numleafs;
extern	dleaf_t		dleafs[MAX_MAP_LEAFS];

extern	int			numplanes;
extern	dplane_t	dplanes[MAX_MAP_PLANES];

extern	int			numvertexes;
extern	dvertex_t	dvertexes[MAX_MAP_VERTS];

extern	int			numnodes;
extern	dnode_t		dnodes[MAX_MAP_NODES];

extern	int			numtexinfo;
extern	texinfo_t	texinfo[MAX_MAP_TEXINFO];

extern	int			numfaces;
extern	dface_t		dfaces[MAX_MAP_FACES];

extern	int			numclipnodes;
extern	dclipnode_t	dclipnodes[MAX_MAP_CLIPNODES];

extern	int			numedges;
extern	dedge_t		dedges[MAX_MAP_EDGES];

extern	int			nummarksurfaces;

extern	int			numsurfedges;
extern	int			dsurfedges[MAX_MAP_SURFEDGES];

extern	hullinfo_t	hullinfo;

extern qboolean		ismcbsp;

//qb: added here// MODIFIED FOR BSP2
extern unsigned int	dmarksurfaces[MAX_MAP_MARKSURFACES];

typedef struct epair_s
{
	struct epair_s	*next;
	char	*key;
	char	*value;
} epair_t;

typedef struct
{
	epair_t		*epairs;
	struct mbrush_s *brushes;
	int			scriptline; // LordHavoc: better error reporting
} entity_t;

extern	int			num_entities;
extern	entity_t	entities[MAX_MAP_ENTITIES];

void	PrintEntity (entity_t *ent);
char 	*ValueForKey (entity_t *ent, char *key);
vec_t	FloatForKey (entity_t *ent, char *key);
void	SetKeyValue (entity_t *ent, char *key, char *value);
entity_t *FindEntityWithKeyPair( char *key, char *value );
void 	GetVectorForKey (entity_t *ent, char *key, vec3_t vec);
epair_t *ParseEpair (void);
void	ParseEntities (void);
void	UnparseEntities (void);

int		CompressVis (byte *vis, byte *dest, int visrow);
void	DecompressVis(byte *in, byte *out, int size);

void LoadBSPFile (char *filename);
void WriteBSPFile (char *filename, qboolean litonly);
void PrintBSPFileSizes (void);

#endif

