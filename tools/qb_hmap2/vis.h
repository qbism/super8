// vis.h

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "mem.h"

#define	MAX_PORTALS	32768

#define	PORTALFILE	"PRT1"

typedef struct
{
	qboolean	original;			// don't free, it's part of the portal
	int			numpoints;
	vec3_t		points[8];			// variable sized
} viswinding_t;

#define MAX_POINTS_ON_VISWINDING	64

viswinding_t	*NewVisWinding (int points);
void			FreeVisWinding (viswinding_t *w);
viswinding_t	*ClipVisWinding (viswinding_t *in, plane_t *split, qboolean keepon);
viswinding_t	*CopyVisWinding (viswinding_t *w);
void			PlaneFromVisWinding (viswinding_t *w, plane_t *plane);
void			VisWindingCentre( viswinding_t *w, vec3_t centre, vec_t *radius );


typedef enum {stat_none, stat_working, stat_done} vstatus_t;
typedef struct
{
	plane_t			plane;		// normal pointing into neighbor
	vec3_t			origin;
	vec_t			radius;
	int				leaf;		// neighbor
	viswinding_t	*winding;
	vstatus_t		status;
	byte			*visbits;
	byte			*mightsee;
	int				nummightsee;
	int				numcansee;
} portal_t;

typedef struct seperating_plane_s
{
	struct seperating_plane_s *next;
	plane_t			plane;		// from portal is on positive side
} sep_t;


typedef struct passage_s
{
	struct passage_s	*next;
	int				from, to;	// leaf numbers
	sep_t			*planes;
} passage_t;

// LordHavoc: a friend ran into this limit (was 128)
#define	MAX_PORTALS_ON_LEAF		512
typedef struct leaf_s
{
	int				numportals;
	passage_t		*passages;
	portal_t		*portals[MAX_PORTALS_ON_LEAF];
} leaf_t;


typedef struct pstack_s
{
	struct pstack_s	*next;
	leaf_t			*leaf;
	portal_t		*portal;		// portal exiting
	viswinding_t	*source, *pass;
	plane_t			portalplane;
	byte			*mightsee;		// bit string
} pstack_t;

typedef struct
{
	byte			*leafvis;		// bit string
	portal_t		*base;
	pstack_t		pstack_head;
} threaddata_t;

extern	int			numportals;
extern	int			portalleafs;

extern	portal_t	*portals;
extern	leaf_t		*leafs;

extern	int			c_portaltest, c_portalpass, c_portalcheck;
extern	int			c_portalskip, c_leafskip;
extern	int			c_vistest, c_mighttest;
extern	int			c_chains;
extern	int			c_reused;

extern	byte		*vismap, *vismap_p, *vismap_end;	// past visfile

extern	int			testlevel;

extern	vec_t		farplane;

extern	qboolean	rvis;
extern	qboolean	noreuse;
extern	qboolean	noambientwater, noambientslime, noambientlava, noambientsky;

extern	byte		*uncompressed;
extern	int			bitbytes;
extern	int			bitlongs;

void LeafFlow (int leafnum);
void BasePortalVis (void);

void PortalFlow (portal_t *p);

void CalcAmbientSounds (void);
