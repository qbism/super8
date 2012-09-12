// mathlib.c -- math primitives

#include "cmdlib.h"
#include "mathlib.h"

vec3_t vec3_origin = { 0, 0, 0 };

vec_t VectorNormalize( vec3_t v )
{
	vec_t length;

	length = VectorLength( v );
	if( length ) {
		vec_t ilength;

		ilength = 1.0 / length;
		VectorScale( v, ilength, v );
	}

	return length;
}

vec_t Q_rint( vec_t n )
{
	if (n >= 0)
		return ( vec_t )(( int )(n + 0.5));
	else
		return ( vec_t )(( int )(n - 0.5));
}

/*
=================
ClearBounds
=================
*/
void ClearBounds( vec3_t mins, vec3_t maxs )
{
	int		i;

	for( i = 0; i < 3; i++ ) {
		mins[i] = BOGUS_RANGE;
		maxs[i] = -BOGUS_RANGE;
	}
}

/*
=================
AddPointToBounds
=================
*/
void AddPointToBounds( vec3_t v, vec3_t mins, vec3_t maxs )
{
	int		i;

	for( i = 0; i < 3; i++ ) {
		if (v[i] < mins[i])
			mins[i] = v[i];
		if (v[i] > maxs[i])
			maxs[i] = v[i];
	}
}

/*
=================
RadiusFromBounds
=================
*/
vec_t RadiusFromBounds( vec3_t mins, vec3_t maxs )
{
	vec_t mind, maxd;

	if( mins[0] < mins[1] )
	    mind = mins[0];
	else
	    mind = mins[1];
	if( mins[2] < mind )
	    mind = mins[2];
	mind = fabs( mind );
	
	if( maxs[0] > maxs[1] )
	    maxd = maxs[0];
	else
	    maxd = maxs[1];
	if( maxs[2] > maxd )
	    maxd = maxs[2];
	maxd = fabs( maxd );
		    
	if( mind > maxd )
		return mind;
	return maxd;
}

/*
=================
PlaneTypeForNormal
=================
*/
int	PlaneTypeForNormal( vec3_t normal )
{
	vec_t	ax, ay, az;

	// NOTE: should these have an epsilon around 1.0?
	if( normal[0] == 1.0 )
		return PLANE_X;
	if( normal[1] == 1.0 )
		return PLANE_Y;
	if( normal[2] == 1.0 )
		return PLANE_Z;
	if( normal[0] == -1.0 || normal[1] == -1.0 || normal[2] == -1.0 )
		Error ("PlaneTypeForNormal: not a canonical vector (%f %f %f)", normal[0], normal[1], normal[2]);

	ax = fabs( normal[0] );
	ay = fabs( normal[1] );
	az = fabs( normal[2] );

	if( ax >= ay && ax >= az )
		return PLANE_ANYX;
	if( ay >= ax && ay >= az )
		return PLANE_ANYY;
	return PLANE_ANYZ;
}

/*
=================
NormalizePlane
=================
*/
void NormalizePlane( plane_t *dp )
{
	vec_t	ax, ay, az;

	if( dp->normal[0] == -1.0 ) {
		dp->normal[0] = 1.0;
		dp->dist = -dp->dist;
	} else if( dp->normal[1] == -1.0 ) {
		dp->normal[1] = 1.0;
		dp->dist = -dp->dist;
	} else if( dp->normal[2] == -1.0 ) {
		dp->normal[2] = 1.0;
		dp->dist = -dp->dist;
	}

	if( dp->normal[0] == 1.0 ) {
		dp->type = PLANE_X;
		VectorSet(dp->normal, 1, 0, 0);
		return;
	}
	if( dp->normal[1] == 1.0 ) {
		dp->type = PLANE_Y;
		VectorSet(dp->normal, 0, 1, 0);
		return;
	}
	if( dp->normal[2] == 1.0 ) {
		dp->type = PLANE_Z;
		VectorSet(dp->normal, 0, 0, 1);
		return;
	}

	ax = fabs( dp->normal[0] );
	ay = fabs( dp->normal[1] );
	az = fabs( dp->normal[2] );

	if( ax >= ay && ax >= az )
		dp->type = PLANE_ANYX;
	else if( ay >= ax && ay >= az )
		dp->type = PLANE_ANYY;
	else
		dp->type = PLANE_ANYZ;

	if( dp->normal[dp->type - PLANE_ANYX] < 0 ) {
		VectorNegate( dp->normal, dp->normal );
		dp->dist = -dp->dist;
	}
}

/*
=================
PlaneCompare
=================
*/
qboolean PlaneCompare( plane_t *p1, plane_t *p2 )
{
	int		i;

	if( fabs( p1->dist - p2->dist ) > 0.01)
		return false;

	for( i = 0; i < 3; i++ )
		if( fabs(p1->normal[i] - p2->normal[i] ) > 0.001 )
			return false;

	return true;
}
