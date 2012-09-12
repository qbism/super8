
// LordHavoc: raised MAX_FACES From 32 to 256
#define	MAX_FACES		256

typedef struct mface_s
{
	struct mface_s	*next;
	plane_t			plane;
	int				texinfo;
	// LordHavoc: better error reporting
	int				scriptline;
} mface_t;

typedef struct mbrush_s
{
	struct mbrush_s	*next;
	mface_t *faces;
	// LordHavoc: better error reporting
	int				scriptline;
} mbrush_t;

extern	int			nummapbrushes;
extern	mbrush_t	mapbrushes[MAX_MAP_BRUSHES];

extern	int			nummapplanes;
extern	plane_t		mapplanes[MAX_MAP_PLANES];

extern	int			nummiptex;
extern	char		miptex[MAX_MAP_TEXINFO][128]; // LordHavoc: was [16]

void 	LoadMapFile (char *filename);

int		FindPlane( plane_t *dplane, int *side );
int		FindMiptex (char *name);
