#include "cmdlib.h"
#include "mathlib.h"
#include "mem.h"
#include "winding.h"

#define WINDING_EPSILON		0.05

//===========================================================================

/*
==================
AllocWinding
==================
*/
winding_t *AllocWinding( int points )
{
	winding_t	*w;
	size_t		size;

	if( points > MAX_POINTS_ON_WINDING )
		Error( "AllocWinding: %i points", points );

	size = ( size_t )((winding_t *)0)->points[points];
	w = qmalloc( size );
	memset( w, 0, size );

	return w;
}

/*
==================
FreeWinding
==================
*/
void FreeWinding( winding_t *w )
{
	qfree( w );
}

/*
==================
PrintWinding
==================
*/
void PrintWinding(winding_t *w)
{
	int i;
	printf("%i", w->numpoints);
	for (i = 0;i < w->numpoints;i++)
		printf(", %f %f %f", w->points[i][0], w->points[i][1], w->points[i][2]);
	printf("\n");
}

/*
==================
CopyWinding
==================
*/
winding_t *CopyWinding( winding_t *w )
{
	winding_t	*c;
	size_t		size;

	size = ( size_t )((winding_t *)0)->points[w->numpoints];
	c = qmalloc( size );
	memcpy( c, w, size );

	return c;
}

/*
==================
CopyWindingExt
==================
*/
winding_t *CopyWindingExt( int numpoints, vec3_t *points )
{
	winding_t	*c;
	size_t		size;

	if( numpoints > MAX_POINTS_ON_WINDING )
		Error( "CopyWinding: %i points", numpoints );

	size = ( size_t )((winding_t *)0)->points[numpoints];
	c = malloc( size );
	c->numpoints = numpoints;
	memcpy( c->points, points, sizeof(*points)*numpoints );

	return c;
}

/*
==================
ReverseWinding
==================
*/
winding_t *ReverseWinding( winding_t *w )
{
	int		i;
	winding_t *neww;

	if( w->numpoints > MAX_POINTS_ON_WINDING )
		Error( "ReverseWinding: %i points", w->numpoints );

	neww = AllocWinding( w->numpoints );
	neww->numpoints = w->numpoints;
	for( i = 0; i < w->numpoints; i++ )		// add points backwards
		VectorCopy( w->points[w->numpoints - 1 - i], neww->points[i] );

#ifdef PARANOID
	CheckWinding( neww, 0 );
#endif

	return neww;
}

//===========================================================================

/*
=================
BaseWindingForPlane
=================
*/
winding_t *BaseWindingForPlane( plane_t *p )
{
	int			i, x;
	vec_t		max, v;
	vec3_t		org, vright, vup;
	winding_t	*w;

	// find the major axis
	max = -BOGUS_RANGE;
	x = -1;
	for( i = 0; i < 3; i++ ) {
		v = fabs( p->normal[i] );
		if( v > max ) {
			x = i;
			max = v;
		}
	}

	if( x == -1 )
		Error( "BaseWindingForPlane: no axis found" );

	VectorClear( vup );
	switch( x )
	{
		case 0:
		case 1:
			vup[2] = 1;
			break;
		default:
			vup[0] = 1;
			break;
	}

	v = DotProduct( vup, p->normal );
	VectorMA( vup, -v, p->normal, vup );
	VectorNormalize( vup );

	VectorScale( p->normal, p->dist, org );

	CrossProduct( vup, p->normal, vright );

	VectorScale( vup, BOGUS_RANGE, vup );
	VectorScale( vright, BOGUS_RANGE, vright );

	// project a really big	axis aligned box onto the plane
	w = AllocWinding( 4 );

	VectorSubtract( org, vright, w->points[0] );
	VectorAdd( w->points[0], vup, w->points[0] );

	VectorAdd( org, vright, w->points[1] );
	VectorAdd( w->points[1], vup, w->points[1] );

	VectorAdd( org, vright, w->points[2] );
	VectorSubtract( w->points[2], vup, w->points[2] );

	VectorSubtract( org, vright, w->points[3] );
	VectorSubtract( w->points[3], vup, w->points[3] );

	w->numpoints = 4;

	//PrintWinding(w);

	return w;
}


/*
==================
CheckWinding

Check for possible errors
==================
*/
void CheckWinding( winding_t *w, int scriptline )
{
	int		i, j;
	vec_t	*p1, *p2;
	vec_t	d, edgedist;
	vec3_t	dir, edgenormal;
	plane_t	facenormal;

	PlaneFromWinding ( w, &facenormal );

	if (w->numpoints < 3)
	{
		printf( "WARNING: line %i: CheckWinding: too few points to form a triangle at %f %f %f\n", scriptline, w->points[0][0], w->points[0][1], w->points[0][2] );
		w->numpoints = 0;
		return;
	}

	// try repeatedly if an error is corrected
	// (fixing one may expose another)
	do
	{
		for( i = 0; i < w->numpoints; i++ )
		{
			p1 = w->points[i];
			p2 = w->points[(i + 1) % w->numpoints];

			for( j = 0; j < 3; j++ )
				if( p1[j] >= BOGUS_RANGE || p1[j] <= -BOGUS_RANGE )
					Error( "WARNING: line %i: CheckWinding: BOGUS_RANGE: %f %f %f\n", scriptline, p1[0], p1[1], p1[2] );

			// check the point is on the face plane
			d = DotProduct( p1, facenormal.normal ) - facenormal.dist;
			if( d < -WINDING_EPSILON || d > WINDING_EPSILON )
			{
				printf( "WARNING: line %i: CheckWinding: point off plane at %f %f %f, attempting to fix\n", scriptline, p1[0], p1[1], p1[2] );
				VectorMA(p1, -d, facenormal.normal, p1);
				break;
			}

			// check the edge isn't degenerate
			VectorSubtract( p2, p1, dir );
			if( VectorLength( dir ) < WINDING_EPSILON )
			{
				printf( "WARNING: line %i: CheckWinding: healing degenerate edge at %f %f %f\n", scriptline, p2[0], p2[1], p2[2] );
				for (j = i + 1;j < w->numpoints;j++)
					VectorCopy(w->points[j - 1], w->points[j]);
				w->numpoints--;
				break;
			}

			// calculate edge plane to check for concavity
			CrossProduct( facenormal.normal, dir, edgenormal );
			VectorNormalize( edgenormal );
			edgedist = DotProduct( p1, edgenormal );

			// all other points must be on front side
			for( j = 0; j < w->numpoints; j++ )
			{
				d = DotProduct( w->points[j], edgenormal );
				if( d > edgedist + WINDING_EPSILON )
				{
					printf( "WARNING: line %i: CheckWinding: non-convex polygon at %f %f %f, attempting to heal\n", scriptline, w->points[j][0], w->points[j][1], w->points[j][2] );
					VectorMA(w->points[j], -d, edgenormal, w->points[j]);
					break;
				}
			}
			if (j < w->numpoints)
				break;
		}
	}
	while (i < w->numpoints);
}

/*
==================
WindingIsTiny
==================
*/
qboolean WindingIsTiny( winding_t *w )
{
	int		i, cnt;
	vec3_t	edge;
	vec_t	len;

#ifdef PARANOID
	if( !w || !w->numpoints )
		return true;
#endif

	cnt = 0;
	for( i = 0; i < w->numpoints; i++ ) {
		VectorSubtract( w->points[i], w->points[(i+1)%w->numpoints], edge );
		len = VectorLength( edge );
		if( len > 0.3 ) {
			if( ++cnt == 3 )
				return false;
		}
	}

	return true;
}

/*
=================
WindingArea
=================
*/
vec_t WindingArea( winding_t *w )
{
	int		i;
	vec_t	total;
	vec3_t	v1, v2, cross;

	// triangle area is vectorlength(crossproduct(edge0, edge1)) * 0.5
	total = 0;
	for( i = 2; i < w->numpoints; i++ ) {
		VectorSubtract( w->points[i-1], w->points[0], v1 );
		VectorSubtract( w->points[i  ], w->points[0], v2 );
		CrossProduct( v1, v2, cross );
		total += VectorLength( cross );
	}

	return total * 0.5;
}

/*
=================
WindingCentre
=================
*/
void WindingCentre( winding_t *w, vec3_t centre, vec_t *radius )
{
	int			i;
	vec3_t		edge;
	vec_t		dist, best;

	if( !w->numpoints )
		Error( "WindingCentre: no points" );

	VectorCopy( w->points[0], centre );
	for( i = 1; i < w->numpoints; i++ )
		VectorAdd( centre, w->points[i], centre );
	VectorScale( centre, (vec_t)1.0/w->numpoints, centre );

	if( radius ) {
		best = 0;
		for( i = 0; i < w->numpoints; i++ ) {
			VectorSubtract( centre, w->points[i], edge );
			dist = VectorLength( edge );
			if( dist > best )
				best = dist;
		}

	#ifdef PARANOID
		if( best == 0 )
			Error( "WindingCentre: winding has zero radius" );
	#endif

		*radius = best;
	}
}

/*
=================
PlaneFromWinding
=================
*/
void PlaneFromWinding( winding_t *w, plane_t *plane )
{
	vec3_t	v1, v2;

	if( w->numpoints < 3 )
		Error( "PlaneFromWinding: %i points at %f %f %f", w->numpoints, w->points[0], w->points[1], w->points[2] );

	// calc plane
	VectorSubtract( w->points[2], w->points[1], v1 );
	VectorSubtract( w->points[0], w->points[1], v2 );
	CrossProduct( v2, v1, plane->normal );
	VectorNormalize( plane->normal );
	plane->dist = DotProduct( w->points[0], plane->normal );
}

/*
==================
WindingOnPlaneSide
==================
*/
int WindingOnPlaneSide( winding_t *w, plane_t *plane )
{
	int i;
	vec_t dist;
	qboolean back, front;

	back = front = false;
	for( i = 0; i < w->numpoints; i++ ) {
		dist = DotProduct( w->points[i], plane->normal ) - plane->dist;

		if( dist < -WINDING_EPSILON ) {
			if( front )
				return SIDE_CROSS;
			back = true;
		} else if( dist > WINDING_EPSILON ) {
			if( back )
				return SIDE_CROSS;
			front = true;
		}
	}

	if( back )
		return SIDE_BACK;
	if ( front )
		return SIDE_FRONT;
	return SIDE_ON;
}

/*
==================
ClipWindingEpsilon
==================
*/
winding_t *ClipWindingEpsilon( winding_t *in, plane_t *split, vec_t epsilon, qboolean keepon )
{
	int			i, j;
	vec_t		dists[MAX_POINTS_ON_WINDING + 1];
	int			sides[MAX_POINTS_ON_WINDING + 1];
	int			counts[3];
	vec_t		dot;
	vec_t		*p1, *p2;
	vec3_t		mid;
	winding_t	*neww;
	int			maxpts;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for( i = 0; i < in->numpoints; i++ )
	{
		dists[i] = dot = DotProduct( in->points[i], split->normal ) - split->dist;
		if( dot > epsilon )
			sides[i] = SIDE_FRONT;
		else if( dot < -epsilon )
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if( keepon && !counts[SIDE_FRONT] && !counts[SIDE_BACK] )
		return in;

	if( !counts[SIDE_FRONT] ) {
		FreeWinding( in );
		return NULL;
	}
	if( !counts[SIDE_BACK] )
		return in;

	maxpts = in->numpoints + 4;	// can't use counts[0]+2 because of fp grouping errors
	neww = AllocWinding( maxpts );

	for( i = 0; i < in->numpoints; i++ ) {
		p1 = in->points[i];

		if( sides[i] == SIDE_ON ) {
			VectorCopy( p1, neww->points[neww->numpoints] );
			neww->numpoints++;
			continue;
		}

		if( sides[i] == SIDE_FRONT ) {
			VectorCopy( p1, neww->points[neww->numpoints] );
			neww->numpoints++;
		}

		if( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		// generate a split point
		p2 = in->points[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i] - dists[i+1]);
		for( j = 0; j < 3; j++ ) {
			// avoid round off error when possible
			if( split->normal[j] == 1 )
				mid[j] = split->dist;
			else if( split->normal[j] == -1 )
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}

		VectorCopy( mid, neww->points[neww->numpoints] );
		neww->numpoints++;
	}

	if( neww->numpoints > maxpts )
		Error( "ClipWinding: points exceeded estimate" );

	// free the original winding
	FreeWinding( in );

	return neww;
}

/*
==================
ClipWinding
==================
*/
winding_t *ClipWinding( winding_t *in, plane_t *split, qboolean keepon ) {
	return ClipWindingEpsilon( in, split, WINDING_EPSILON, keepon );
}

/*
==================
DivideWindingEpsilon

Divides a winding by a plane, producing one or two windings. The
original winding is not damaged or freed. If only on one side, the
returned winding will be the input winding. If on both sides, two
new windings will be created.
==================
*/
void DivideWindingEpsilon( winding_t *in, plane_t *split, winding_t **front, winding_t **back, vec_t epsilon )
{
	int			i, j;
	vec_t		dists[MAX_POINTS_ON_WINDING + 1];
	int			sides[MAX_POINTS_ON_WINDING + 1];
	int			counts[3];
	vec_t		dot;
	vec_t		*p1, *p2;
	vec3_t		mid;
	winding_t	*f, *b;
	int			maxpts;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for( i = 0; i < in->numpoints; i++ ) {
		dists[i] = dot = DotProduct (in->points[i], split->normal) - split->dist;
		if( dot > epsilon )
			sides[i] = SIDE_FRONT;
		else if( dot < -epsilon )
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*front = *back = NULL;

	if( !counts[SIDE_FRONT] ) {
		*back = in;
		return;
	}
	if( !counts[SIDE_BACK] ) {
		*front = in;
		return;
	}

	maxpts = in->numpoints + 4;	// can't use counts[0]+2 because of fp grouping errors

	*front = f = AllocWinding( maxpts );
	*back = b = AllocWinding( maxpts );

	for( i = 0; i < in->numpoints; i++ ) {
		p1 = in->points[i];

		if( sides[i] == SIDE_ON ) {
			VectorCopy( p1, f->points[f->numpoints] );
			f->numpoints++;
			VectorCopy( p1, b->points[b->numpoints] );
			b->numpoints++;
			continue;
		}

		if( sides[i] == SIDE_FRONT ) {
			VectorCopy( p1, f->points[f->numpoints] );
			f->numpoints++;
		} else if( sides[i] == SIDE_BACK ) {
			VectorCopy( p1, b->points[b->numpoints] );
			b->numpoints++;
		}

		if( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		// generate a split point
		p2 = in->points[(i+1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i+1]);
		for( j = 0;j  < 3; j++) {
			// avoid round off error when possible
			if( split->normal[j] == 1 )
				mid[j] = split->dist;
			else if( split->normal[j] == -1 )
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy( mid, f->points[f->numpoints] );
		f->numpoints++;
		VectorCopy( mid, b->points[b->numpoints] );
		b->numpoints++;
	}

	if( f->numpoints > maxpts || b->numpoints > maxpts )
		Error ("ClipWinding: points exceeded estimate");
}

/*
==================
DivideWinding
==================
*/
void DivideWinding( winding_t *in, plane_t *split, winding_t **front, winding_t **back ) {
	DivideWindingEpsilon( in, split, front, back, WINDING_EPSILON );
}

/*
=============
TryMergeWinding

If two polygons share a common edge and the edges that meet at the
common points are both inside the other polygons, merge them

Returns NULL if the windings couldn't be merged, or the new winding.
The originals will NOT be freed.
=============
*/

winding_t *TryMergeWinding( winding_t *w1, winding_t *w2, vec3_t planenormal )
{
	int			i, j = 0, k, l;
	vec_t		*p1 = NULL, *p2 = NULL, *p3, *p4, *back;
	winding_t	*neww;
	vec3_t		normal, delta;
	vec_t		dot;
	qboolean	keep1, keep2;

	//
	// find a common edge
	//
	for( i = 0; i < w1->numpoints; i++ ) {
		p1 = w1->points[i];
		p2 = w1->points[(i + 1) % w1->numpoints];

		for( j = 0; j < w2->numpoints; j++ ) {
			p3 = w2->points[j];
			p4 = w2->points[(j + 1) % w2->numpoints];

			for( k = 0; k < 3; k++ ) {
				if( fabs( p1[k] - p4[k] ) > EQUAL_EPSILON ||
					fabs( p2[k] - p3[k] ) > EQUAL_EPSILON )
					break;
			}

			if( k == 3 )
				break;
		}

		if( j < w2->numpoints )
			break;
    }

	if( i == w1->numpoints )
		return NULL;			// no matching edges

	//
	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	//
	back = w1->points[(i + w1->numpoints - 1) % w1->numpoints];
	VectorSubtract( p1, back, delta );
	CrossProduct( planenormal, delta, normal );
	VectorNormalize( normal );

	back = w2->points[(j + 2) % w2->numpoints];
	VectorSubtract( back, p1, delta );
	dot = DotProduct( delta, normal );
	if( dot > CONTINUOUS_EPSILON )
		return NULL;			// not a convex polygon
	keep1 = dot < -CONTINUOUS_EPSILON;

	back = w1->points[(i + 2) % w1->numpoints];
	VectorSubtract( back, p2, delta );
	CrossProduct( planenormal, delta, normal );
	VectorNormalize( normal );

	back = w2->points[(j + w2->numpoints - 1) % w2->numpoints];
	VectorSubtract( back, p2, delta );
	dot = DotProduct( delta, normal );
	if( dot > CONTINUOUS_EPSILON )
		return NULL;			// not a convex polygon
	keep2 = dot < -CONTINUOUS_EPSILON;

	//
	// build the new polygon
	//
	neww = AllocWinding( w1->numpoints + w2->numpoints - 4 + keep1 + keep2 );

	// copy first polygon
	for( k = (i + 1) % w1->numpoints; k != i; k = (k + 1)%w1->numpoints ) {
		if( k == (i + 1) % w1->numpoints && !keep2 )
			continue;

		VectorCopy( w1->points[k], neww->points[neww->numpoints] );
		neww->numpoints++;
    }

	// copy second polygon
	for( l = (j + 1) % w2->numpoints; l != j; l = (l + 1) % w2->numpoints ) {
		if( l == (j + 1) % w2->numpoints && !keep1 )
			continue;

		VectorCopy( w2->points[l], neww->points[neww->numpoints] );
		neww->numpoints++;
    }

	return neww;
}
