#ifndef __WINDING_H__
#define __WINDING_H__

typedef struct
{
	int		numpoints;
	vec3_t	points[8];			// variable sized
} winding_t;

#define MAX_POINTS_ON_WINDING	64

winding_t	*AllocWinding (int points);
void		FreeWinding (winding_t *w);
void		PrintWinding(winding_t *w);
winding_t	*CopyWinding (winding_t *w);
winding_t	*CopyWindingExt( int numpoints, vec3_t *points );
winding_t	*ReverseWinding( winding_t *w );
winding_t	*BaseWindingForPlane( plane_t *p );
void		CheckWinding( winding_t *w, int scriptline );
qboolean	WindingIsTiny( winding_t *w );
vec_t		WindingArea( winding_t *w );
void		WindingCentre( winding_t *w, vec3_t centre, vec_t *radius );
void		PlaneFromWinding( winding_t *w, plane_t *plane );
int			WindingOnPlaneSide( winding_t *w, plane_t *plane );
winding_t	*ClipWinding( winding_t *in, plane_t *split, qboolean keepon );
winding_t	*ClipWindingEpsilon( winding_t *in, plane_t *split, vec_t epsilon, qboolean keepon );
void		DivideWinding( winding_t *in, plane_t *split, winding_t **front, winding_t **back );
void		DivideWindingEpsilon( winding_t *in, plane_t *split, winding_t **front, winding_t **back, vec_t epsilon );
winding_t	*TryMergeWinding( winding_t *w1, winding_t *w2, vec3_t planenormal );

#endif //__WINDING_H__
