// mathlib.h

#ifndef __MATHLIB__
#define __MATHLIB__

#include <math.h>

#if _MSC_VER
# pragma warning(disable : 4244)     // MIPS
# pragma warning(disable : 4136)     // X86
# pragma warning(disable : 4051)     // ALPHA
#endif

#define DOUBLEVEC_T

#ifdef DOUBLEVEC_T
typedef double vec_t;
#else
typedef float vec_t;
#endif
typedef vec_t vec3_t[3];

// LordHavoc: these epsilons came from tyrqbsp by Tyrann
// Misc EPSILON values
// FIXME: document which to use where
//        use float.h stuff for better choices?
#define TIGHTER_EPSILONS
#ifdef  TIGHTER_EPSILONS
# define NORMAL_EPSILON     ((vec_t)0.000001)
# define ANGLE_EPSILON      ((vec_t)0.000001)
# define DIST_EPSILON       ((vec_t)0.0001)
# define ZERO_EPSILON       ((vec_t)0.0001)
# define POINT_EPSILON      ((vec_t)0.0001)
# define ON_EPSILON         ((vec_t)0.0001)
# define EQUAL_EPSILON      ((vec_t)0.0001)
# define CONTINUOUS_EPSILON ((vec_t)0.0001)
# define T_EPSILON          ((vec_t)0.0001)
#else
# define NORMAL_EPSILON     ((vec_t)0.00001)
# define ANGLE_EPSILON      ((vec_t)0.00001)
# define DIST_EPSILON       ((vec_t)0.01)
# define ZERO_EPSILON       ((vec_t)0.001)
# define POINT_EPSILON      ((vec_t)0.01)
# define ON_EPSILON         ((vec_t)0.05)
# define EQUAL_EPSILON      ((vec_t)0.001)
# define CONTINUOUS_EPSILON ((vec_t)0.001)
# define T_EPSILON          ((vec_t)0.01)
#endif

extern vec3_t vec3_origin;

typedef struct
{
	vec3_t	normal;
	vec_t	dist;
	int		type;
} plane_t;

#define	SIDE_FRONT		0
#define	SIDE_ON			2
#define	SIDE_BACK		1
#define	SIDE_CROSS		-2

#define	Q_PI			3.14159265358979323846

#ifndef PLANE_X

// 0-2 are axial planes
# define	PLANE_X			0
# define	PLANE_Y			1
# define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
# define	PLANE_ANYX		3
# define	PLANE_ANYY		4
# define	PLANE_ANYZ		5

#endif

// LordHavoc: moved BOGUS_RANGE from bsp5.h and increased from 18000 to 1000000000
#define	BOGUS_RANGE		1000000000

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define bound(a,b,c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))
#define clamp(a,b,c) ((b) >= (c) ? (a)=(b) : (a) < (b) ? (a)=(b) : (a) > (c) ? (a)=(c) : (a))

#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSet(a,x,y,z) ((a)[0]=(x),(a)[1]=(y),(a)[2]=(z))
#define VectorSet4(a,x,y,z,w) ((a)[0]=(x),(a)[1]=(y),(a)[2]=(z),(a)[3]=(w))
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c) ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorLength(v) (sqrt(((v)[0]*v[0]) + ((v)[1]*v[1]) + ((v)[2]*(v)[2])))
#define VectorCompare(v1, v2) (fabs((v1)[0]-(v2)[0]) < EQUAL_EPSILON && \
				fabs((v1)[1]-(v2)[1]) < EQUAL_EPSILON && \
				fabs((v1)[2]-(v2)[2]) < EQUAL_EPSILON)
#define VectorMA(va, scale, vb, vc) ((vc)[0]=(va)[0]+(scale)*(vb)[0],(vc)[1]=(va)[1]+(scale)*(vb)[1],(vc)[2]=(va)[2]+(scale)*(vb)[2])
#define CrossProduct(v1, v2, cross) ((cross)[0]=(v1)[1]*(v2)[2]-(v1)[2]*(v2)[1],(cross)[1]=(v1)[2]*(v2)[0]-(v1)[0]*(v2)[2],(cross)[2]=(v1)[0]*(v2)[1]-(v1)[1]*(v2)[0])
#define VectorClear(v) ((v)[0]=(v)[1]=(v)[2]=0)
#define VectorNegate(a,b) ((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorInverse(v) ((v)[0]=-(v)[0],(v)[1]=-(v)[1],(v)[2]=-(v)[2])
#define VectorScale(v, scale, out) ((out)[0]=(v)[0]*(scale),(out)[1]=(v)[1]*(scale),(out)[2]=(v)[2]*(scale))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

vec_t VectorNormalize( vec3_t v );

vec_t Q_rint( vec_t n );

void ClearBounds( vec3_t mins, vec3_t maxs );
void AddPointToBounds( vec3_t v, vec3_t mins, vec3_t maxs );
vec_t RadiusFromBounds( vec3_t mins, vec3_t maxs );

int	PlaneTypeForNormal( vec3_t normal );
void NormalizePlane( plane_t *dp );
qboolean PlaneCompare( plane_t *p1, plane_t *p2 );

#endif
