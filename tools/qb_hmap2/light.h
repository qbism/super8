
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "mem.h"

typedef enum lighttype_e
{
	LIGHTTYPE_MINUSX,		// id light/arghlite/tyrlite mode 0, equation 1-x
	LIGHTTYPE_RECIPX,		// tyrlite mode 1, equation 1/x
	LIGHTTYPE_RECIPXX,		// hlight/tyrlite mode 2, realistic, equation 1/(x*x)
	LIGHTTYPE_NONE,			// tyrlite mode 3, no fade, equation 1
	LIGHTTYPE_SUN,			// sun light from sky polygons, equation 1
	LIGHTTYPE_MINUSXX,		// tenebrae/doom3-like, 1-(x*x)
	LIGHTTYPE_TOTAL			// total number of light types
} lighttype_t;

typedef struct
{
	lighttype_t	type;

	vec3_t	origin;			// location of light (unused on LIGHTTYPE_SUN)
	vec_t	radius;			// used by all light types except LIGHTTYPE_SUN
	vec3_t	color;			// color of light in output scale (128 for example, or a huge number for LIGHTTYPE_RECIPX and LIGHTTYPE_RECIPXX)
	vec_t	angle;			// cone angle for spotlights
	int		light;			// light radius from quake light entity
	int		style;			// which style the light belongs to

	vec3_t	spotdir;		// spotlight cone direction
	vec_t	spotcone;		// spotlight cone cosine (DotProduct(spotdir, lightdir) compare value)
	vec_t	clampradius;	// confine light to this radius, without affecting its attenuation behavior (fades to black at this radius)
} directlight_t;

typedef struct lightchain_s
{
	directlight_t *light;
	struct lightchain_s *next;
} lightchain_t;

typedef struct
{
	int			startcontents;
	int			endcontents;
	vec_t		fraction;
	vec3_t		start;
	vec3_t		end;
	vec3_t		impact;
	vec3_t		filter;
	plane_t		plane;
	qboolean	hitsky;
} lightTrace_t;

//===============================================================================

// light.c
extern	int			c_occluded;
extern	qboolean	relight;

extern	int			num_directlights;
extern	directlight_t directlights[MAP_DIRECTLIGHTS];

extern	int			extrasamplesbit;	// power of 2 extra sampling (0 = 1x1 sampling, 1 = 2x2 sampling, 2 = 4x4 sampling, etc)
extern	vec_t		extrasamplesscale;	// 1.0 / pointspersample (extrasamples related)

extern	vec_t		globallightscale;

extern	int			harshshade;

extern	int			minlight;
extern	int			ambientlight;

// light_face.c
void LightFace( dface_t *f, const lightchain_t *lightchain, const directlight_t **novislight, int novislights, const vec3_t faceorg );

// light_trace.c
dleaf_t *Light_PointInLeaf( const vec3_t point );
int Light_PointContents( const vec3_t p );
void Light_TraceLine( lightTrace_t *trace, const vec3_t start, const vec3_t end, qboolean hitsky );
