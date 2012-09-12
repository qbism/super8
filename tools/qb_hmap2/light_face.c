
#include "light.h"

/*
===============================================================================

SAMPLE POINT DETERMINATION

This is a little tricky because the lightmap covers more area than the face.
If done in the straightforward fashion, some of the
sample points will be inside walls or on the other side of walls, causing
false shadows and light bleeds.

To solve this, I only consider a sample point valid if a line can be drawn
between it and the exact midpoint of the face.  If invalid, it is adjusted
towards the center until it is valid.

(this doesn't completely work)

===============================================================================
*/

// LordHavoc: increased from 18x18 samples to 256x256 samples
#define SINGLEMAP (256*256)

typedef struct
{
	vec3_t			v;
	qboolean		occluded;
	int				samplepos;	// the offset into the lightmap that this point contributes to
} lightpoint_t;

typedef struct
{
	vec3_t			c;
	vec3_t			n; // surface normal in tangentspace
} lightsample_t;

typedef struct
{
	vec_t			facedist;
	vec3_t			facenormal;
	vec3_t			facemid;

	int				numpoints;
	int				numsamples;

	int				maxsamples;
	lightpoint_t	*point;
	lightsample_t	*sample[MAXLIGHTMAPS];

	vec3_t			texorg;
	vec3_t			worldtotex[2];	// s = (world - texorg) . worldtotex[0]
	vec3_t			textoworld[2];	// world = texorg + s * textoworld[0]

	vec_t			exactmins[2], exactmaxs[2];

	int				texmins[2], texsize[2];
	int				lightstyles[MAXLIGHTMAPS];
	dface_t			*face;
} lightinfo_t;

static qboolean ranout = false; // FIXME: do this some other way?

/*
================
CalcFaceVectors

Fills in texorg, worldtotex. and textoworld
================
*/
static void CalcFaceVectors( lightinfo_t *l, const vec3_t faceorg )
{
	int			i;
	texinfo_t	*tex = &texinfo[l->face->texinfo];
	vec3_t		texnormal;
	vec_t		dist, len, distscale;

	// convert from float to vec_t
	VectorCopy( tex->vecs[0], l->worldtotex[0] );
	VectorCopy( tex->vecs[1], l->worldtotex[1] );

	// calculate a normal to the texture axis. points can be moved along this
	// without changing their S/T
	CrossProduct( tex->vecs[1], tex->vecs[0], texnormal );
	VectorNormalize( texnormal );

	// flip it towards plane normal
	distscale = DotProduct( texnormal, l->facenormal );
	if( !distscale )
	{
		printf( "Texture axis perpendicular to face at location %f %f %f\n", l->facemid[0], l->facemid[1], l->facemid[2] );
		distscale = 1;
	}
	if( distscale < 0 ) {
		distscale = -distscale;
		VectorNegate( texnormal, texnormal );
	}

	// distscale is the ratio of the distance along the texture normal to
	// the distance along the plane normal
	distscale = 1.0 / distscale;

	for( i = 0; i < 2; i++ ) {
		len = 1.0 / VectorLength( l->worldtotex[i] );
		len *= len;

		dist = DotProduct( l->worldtotex[i], l->facenormal);
		dist *= distscale;

		VectorMA( l->worldtotex[i], -dist, texnormal, l->textoworld[i] );
		VectorScale( l->textoworld[i], len, l->textoworld[i] );
	}

	// calculate texorg on the texture plane
	for( i = 0; i < 3; i++ )
		l->texorg[i] = -tex->vecs[0][3] * l->textoworld[0][i] - tex->vecs[1][3] * l->textoworld[1][i];

	VectorAdd( l->texorg, faceorg, l->texorg );

	// project back to the face plane
	dist = DotProduct( l->texorg, l->facenormal ) - l->facedist - 1;
	dist *= distscale;
	VectorMA( l->texorg, -dist, texnormal, l->texorg );
}

/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
also sets exactmins[] and exactmaxs[]
================
*/
static void CalcFaceExtents( lightinfo_t *l, const vec3_t faceorg )
{
	int			i, j, e;
	dface_t		*s = l->face;
	dvertex_t	*v;
	texinfo_t	*tex = &texinfo[s->texinfo];
	vec_t		mins[2], maxs[2], val;

	mins[0] = mins[1] = BOGUS_RANGE;
	maxs[0] = maxs[1] = -BOGUS_RANGE;

	l->facemid[0] = l->facemid[1] = l->facemid[2] = 0;
	for( i = 0; i < s->numedges; i++ ) {
		e = dsurfedges[s->firstedge + i];
		if( e >= 0 )
			v = dvertexes + dedges[e].v[0];
		else
			v = dvertexes + dedges[-e].v[1];
		VectorAdd(l->facemid, v->point, l->facemid);

		for( j = 0; j < 2; j++ ) {
			val = DotProduct( v->point, tex->vecs[j] ) + tex->vecs[j][3];
			if( val < mins[j] )
				mins[j] = val;
			if( val > maxs[j] )
				maxs[j] = val;
		}
	}
	VectorMA(faceorg, (1.0 / s->numedges), l->facemid, l->facemid);

	for( i = 0; i < 2; i++ ) {
		l->exactmins[i] = mins[i];
		l->exactmaxs[i] = maxs[i];

		mins[i] = floor( mins[i] / 16 );
		maxs[i] = ceil( maxs[i] / 16 );

		l->texmins[i] = mins[i];
		l->texsize[i] = maxs[i] + 1 - mins[i];
		if( l->texsize[i] > 256 ) // LordHavoc: was 17, much much bigger allowed now
			Error( "Bad surface extents" );
	}
}

/*
================
CalcSamples
================
*/
static void CalcSamples( lightinfo_t *l )
{
	int				mapnum;

	l->numsamples = l->texsize[0] * l->texsize[1];
	if( (unsigned int)l->numsamples > (unsigned int)SINGLEMAP )
		Error( "Bad lightmap size: %i", l->numsamples );

	if( l->numsamples > l->maxsamples ) {
		l->maxsamples = l->numsamples;
		if( l->point )
			qfree( l->point );
		l->point = qmalloc( sizeof( lightpoint_t ) * l->maxsamples * (1<<extrasamplesbit)*(1<<extrasamplesbit) );

		for( mapnum = 0; mapnum < MAXLIGHTMAPS; mapnum++ ) {
			if( l->sample[mapnum] )
				qfree( l->sample[mapnum] );
			l->sample[mapnum] = qmalloc( sizeof( lightsample_t ) * l->maxsamples );
		}
	}

	// no need to clear because the lightinfo struct was cleared already
}

/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
static void CalcPoints( lightinfo_t *l )
{
	int j, s, t, w, h, realw, realh, stepbit;
	vec_t starts, startt, us, ut;
//	vec_t mids, midt;
	vec3_t facemid, base;
	lightTrace_t tr;
	lightpoint_t *point;
	/*
	int i;
	vec3_t v;
	vec_t nudgeScale = 1.0 / 8.0;
	vec_t nudgeFractions[2][9] =
	{
		{ 0, 0, 0, 1, 1, 1, -1, -1, -1 },
		{ 0, 1, -1, 0, 1, -1, 0, 1, -1 },
	};
	int numNudgeFractions = sizeof( nudgeFractions[0] ) / sizeof( nudgeFractions[0][0] );
	*/

//
// fill in point array
// the points are biased towards the center of the surface
// to help avoid edge cases just inside walls
//
//	mids = (l->exactmaxs[0] + l->exactmins[0]) * 0.5;
//	midt = (l->exactmaxs[1] + l->exactmins[1]) * 0.5;

	// put the base and facemid a little above the surface
	VectorMA( l->texorg, (1.0 / 32.0), l->facenormal, base );
	VectorMA( l->facemid, (1.0 / 32.0), l->facenormal, facemid );

	realw = l->texsize[0];
	realh = l->texsize[1];
	starts = l->texmins[0] * 16;
	startt = l->texmins[1] * 16;

	stepbit = 4 - extrasamplesbit;

	w = realw << extrasamplesbit;
	h = realh << extrasamplesbit;

	if( stepbit < 4 ) {
		starts -= 1 << stepbit;
		startt -= 1 << stepbit;
	}

	point = l->point;
	l->numpoints = w * h;
	for( t = 0; t < h; t++ ) {
		for( s = 0; s < w; s++, point++ ) {
			us = starts + (s << stepbit);
			ut = startt + (t << stepbit);

			point->occluded = false;
			point->samplepos = (t >> extrasamplesbit) * realw + (s >> extrasamplesbit);

			// calculate texture point
			for( j = 0; j < 3; j++ )
				point->v[j] = base[j] + l->textoworld[0][j] * us + l->textoworld[1][j] * ut;

			/*
			// nudge around the sample point
			for( i = 0; i < numNudgeFractions; i++ ) {
				// calculate texture point
				for( j = 0; j < 3; j++ )
					v[j] = point->v[j] + nudgeScale * (l->textoworld[0][j] * nudgeFractions[0][i] + l->textoworld[1][j] * nudgeFractions[1][i]);

				if( Light_PointContents( v ) != CONTENTS_SOLID ) {
					VectorCopy( v, point->v );
					break;	// got it
				}
			}

			if( i == numNudgeFractions ) */ {
				// ok, give it one last chance
				// note: at this point we already know that
				// the starting sample point is in solid
				//VectorSubtract(point->v, facemid, v);
				//VectorNormalize(v);
				//VectorMA(point->v, (0.0 / 32.0), v, v);
				Light_TraceLine( &tr, facemid, point->v, false );

				// adjust to impact position
				// (which is nudged out of the impact surface by 1/32 already)
				VectorCopy( tr.impact, point->v );

				if( Light_PointContents( point->v ) == CONTENTS_SOLID ) {
					c_occluded++;
					point->occluded = true;
				}
			}
		}
	}
}


/*
===============================================================================

FACE LIGHTING

===============================================================================
*/

/*
=================
SingleLightFace_FindMapNum
=================
*/
static int SingleLightFace_FindMapNum( lightinfo_t *l, int style )
{
	int mapnum;

	for( mapnum = 0; mapnum < MAXLIGHTMAPS; mapnum++ ) {
		if( l->lightstyles[mapnum] == style )
			break;

		if( l->lightstyles[mapnum] == 255 ) {
			if( relight )
				return MAXLIGHTMAPS;
			l->lightstyles[mapnum] = style;
			break;
		}
	}

	if( mapnum == MAXLIGHTMAPS )
		printf( "WARNING: Too many light styles on a face\n" );
	return mapnum;
}

/*
=================
SingleLightFace_Sun
=================
*/
static void SingleLightFace_Sun( const directlight_t *light, lightinfo_t *l )
{
	int				i, mapnum;
	vec_t			shade, f;
	vec3_t			endpos, c;
	lightpoint_t	*point;
	lightsample_t	*sample;
	lightTrace_t	tr;

	shade = -DotProduct( light->spotdir, l->facenormal );
	if( shade <= 0 )
		return;		// ignore backfaces

	// LordHavoc: FIXME: decide this 0.5 bias based on shader properties (some are dull, some are shiny)
	if (!harshshade)
		shade = (shade * 0.5 + 0.5);

	// mapnum won't be allocated until some light hits the surface
	mapnum = -1;

	for( i = 0, point = l->point; i < l->numpoints; i++, point++ ) {
		if( point->occluded )
			continue;

		// LordHavoc: changed to be more realistic (entirely different lighting model)
		// LordHavoc: FIXME: use subbrightness on all lights, simply to have some distance culling
		VectorMA( point->v, -131072, light->spotdir, endpos );

		// if trace hits solid don't cast sun
		Light_TraceLine( &tr, point->v, endpos, true );
		if( tr.fraction < 1 && tr.endcontents == CONTENTS_SOLID )
			continue;

		c[0] = shade * light->color[0] * tr.filter[0];
		c[1] = shade * light->color[1] * tr.filter[1];
		c[2] = shade * light->color[2] * tr.filter[2];

		if( DotProduct(c, c) < (1.0 / 32.0) )
			continue;	// ignore colors too dim

		// if there is some light, alloc a style for it
		if( mapnum < 0 )
			if( (mapnum = SingleLightFace_FindMapNum( l, light->style )) >= MAXLIGHTMAPS )
				return;

		// accumulate the lighting
		sample = &l->sample[mapnum][point->samplepos];
		VectorMA( sample->c, extrasamplesscale, c, sample->c );
		// accumulate the weighted average surface normal
		f = -VectorLength(c) * extrasamplesscale;
		VectorMA(sample->n, f, light->spotdir, sample->n);
	}
}

/*
================
SingleLightFace
================
*/
static void SingleLightFace( const directlight_t *light, lightinfo_t *l )
{
	int				i, mapnum;
	vec_t			add, dist, idist, dist2, f, iradius;
	vec3_t			incoming, c;
	lightpoint_t	*point;
	lightsample_t	*sample;
	lightTrace_t	tr;

	if( light->type == LIGHTTYPE_SUN ) {
		SingleLightFace_Sun( light, l );
		return;
	}

	dist = DotProduct( light->origin, l->facenormal ) - l->facedist;
	if( dist < 0 )
		return;		// don't bother with lights behind the surface
	else if( dist > light->radius )
		return;		// don't bother with light too far away

	// mapnum won't be allocated until some light hits the surface
	mapnum = -1;

	iradius = 1.0 / light->radius;

	for( i = 0, point = l->point; i < l->numpoints; i++, point++ ) {
		if( point->occluded )
			continue;

		VectorSubtract( light->origin, point->v, incoming );
		dist = VectorLength( incoming );
		if( !dist || dist > light->clampradius )
			continue;

		idist = 1.0 / dist;
		VectorScale( incoming, idist, incoming );

		// spotlight
		if( light->spotcone && DotProduct( light->spotdir, incoming ) > light->spotcone)
			continue;

		dist2 = dist * iradius;

		switch( light->type ) {
			case LIGHTTYPE_MINUSX:
				add = (1.0 - (dist2        ));
				break;
			case LIGHTTYPE_MINUSXX:
				add = (1.0 - (dist2 * dist2));
				break;
			case LIGHTTYPE_RECIPX:
				add = (1.0 / (dist2        ));
				break;
			case LIGHTTYPE_RECIPXX:
				add = (1.0 / (dist2 * dist2));
				break;
			default:
				add = 1.0;
				break;
		}

		if( add <= 0 )
			continue;

		// LordHavoc: FIXME: decide this 0.5 bias based on shader properties (some are dull, some are shiny)
		if (!harshshade)
			add = add * (DotProduct( incoming, l->facenormal ) * 0.5 + 0.5);
		else
			add = add * (DotProduct( incoming, l->facenormal ));
		if( add <= 0 )
			continue;

		Light_TraceLine( &tr, point->v, light->origin, false );
		if( tr.startcontents == CONTENTS_SOLID || tr.fraction < 1 )
			continue;

		c[0] = add * light->color[0] * tr.filter[0];
		c[1] = add * light->color[1] * tr.filter[1];
		c[2] = add * light->color[2] * tr.filter[2];

		if( DotProduct( c, c ) < (1.0 / 32.0) )
			continue;	// ignore colors too dim

		// if there is some light, alloc a style for it
		if( mapnum < 0 )
			if( (mapnum = SingleLightFace_FindMapNum( l, light->style )) >= MAXLIGHTMAPS )
				return;

		// accumulate the lighting
		sample = &l->sample[mapnum][point->samplepos];
		VectorMA( sample->c, extrasamplesscale, c, sample->c );
		// accumulate the weighted average light direction (deluxemap)
		f = VectorLength(c) * extrasamplesscale;
		VectorMA(sample->n, f, incoming, sample->n);
	}
}

/*
============
LightFace
============
*/
lightinfo_t l; // if this is made multithreaded again, this should be inside the function
void LightFace( dface_t *f, const lightchain_t *lightchain, const directlight_t **novislight, int novislights, const vec3_t faceorg )
{
	int				i, j, size;
	byte			*out, *lit, *nmap;
	lightsample_t	*sample;
	vec3_t			tangentvectors[3]; // [0] is along S texcoord, [1] is along T texcoord, [2] is surface normal
	vec_t			dist;

	//memset (&l, 0, sizeof(l));
	l.face = f;

	// some surfaces don't need lightmaps
	if( relight ) {
		if( f->lightofs == -1 )
			return;
	} else {
		for( i = 0; i < MAXLIGHTMAPS; i++ )
			f->styles[i] = l.lightstyles[i] = 255;
		f->lightofs = -1;

		// return if it's not a lightmapped surface
		if( texinfo[f->texinfo].flags & TEX_SPECIAL )
			return;
		// return if we've run out of lightmap data space
		if (ranout)
			return;
	}

	// rotate plane
	VectorCopy( dplanes[f->planenum].normal, l.facenormal );
	l.facedist = dplanes[f->planenum].dist;

	if( f->side ) {
		VectorNegate( l.facenormal, l.facenormal );
		l.facedist = -l.facedist;
	}

	CalcFaceExtents( &l, faceorg );
	CalcFaceVectors( &l, faceorg );
	CalcSamples( &l );
	CalcPoints( &l );

	// clear all the samples to 0
	for( i = 0; i < MAXLIGHTMAPS; i++ )
		memset( l.sample[i], 0, sizeof( lightsample_t ) * l.numsamples );

	// if -minlight or -ambientlight is used we always allocate style 0
	if( minlight > 0 || ambientlight > 0 )
		l.lightstyles[0] = 0;

	if( relight ) {
		// reserve the correct light styles
		for( i = 0; i < MAXLIGHTMAPS; i++ )
			l.lightstyles[i] = f->styles[i];
	}

	// cast all lights
	while( lightchain ) {
		SingleLightFace( lightchain->light, &l );
		lightchain = lightchain->next;
	}

	while( novislights-- )
		SingleLightFace( *novislight++, &l );

	// apply ambientlight if needed
	if( ambientlight > 0 )
	{
		vec_t d;
		vec3_t v;
		v[0] = ambientlight;
		v[1] = ambientlight;
		v[2] = ambientlight;
		d = VectorLength(v);
		for( i = 0; i < l.numsamples; i++ )
		{
			VectorAdd(l.sample[0][i].c, v, l.sample[0][i].c);
			// accumulate ambientlight normal as directly infront of the face
			VectorMA(l.sample[0][i].n, d, l.facenormal, l.sample[0][i].n);
		}
	}

	// apply minlight if needed
	if( minlight > 0 )
	{
		vec_t d;
		vec3_t v;
		for( i = 0; i < l.numsamples; i++ )
		{
			for( j = 0; j < 3; j++ )
			{
				v[j] = minlight - l.sample[0][i].c[j];
				if (v[j] < 0)
					v[j] = 0;
			}
			VectorAdd(l.sample[0][i].c, v, l.sample[0][i].c);
			// accumulate minlight normal as directly infront of the face
			d = VectorLength(v);
			VectorMA(l.sample[0][i].n, d, l.facenormal, l.sample[0][i].n);
		}
	}

	// save out the values
	if( relight ) {
		// relighting an existing map without changing it's lightofs table
		for( i = 0; i < MAXLIGHTMAPS && f->styles[i] != 255; i++ );

		size = l.numsamples * i;
		if( f->lightofs < 0 || f->lightofs + size > lightdatasize )
			Error( "LightFace: Error while trying to relight map: invalid lightofs value, %i must be >= 0 && < %i\n", f->lightofs, lightdatasize );
	} else {
		// creating lightofs table from scratch
		for( i = 0; i < MAXLIGHTMAPS && l.lightstyles[i] != 255; i++ );

		size = l.numsamples * i;
		if( !size )
			return;	// no light styles

		if( lightdatasize + size > MAX_MAP_LIGHTING ) {
			if( !ranout ) {
				printf( "LightFace: ran out of lightmap dataspace" );
				ranout = true;
			}
			return;
		}

		for( i = 0; i < MAXLIGHTMAPS; i++ )
			f->styles[i] = l.lightstyles[i];
		f->lightofs = lightdatasize;
		lightdatasize += size;
	}

	rgblightdatasize = lightdatasize * 3;
	nmaplightdatasize = lightdatasize * 3;

	out = dlightdata + f->lightofs;
	lit = drgblightdata + f->lightofs * 3;
	nmap = dnmaplightdata + f->lightofs * 3;

	// calculate tangent vectors for deluxemap normalmap building
	VectorCopy(l.textoworld[0], tangentvectors[0]);
	VectorNegate(l.textoworld[1], tangentvectors[1]);
	VectorCopy(l.facenormal, tangentvectors[2]);

	// project onto plane
	dist = -DotProduct(tangentvectors[0], l.facenormal);
	VectorMA(tangentvectors[0], dist, l.facenormal, tangentvectors[0]);

	// project onto plane
	dist = -DotProduct(tangentvectors[1], l.facenormal);
	VectorMA(tangentvectors[1], dist, l.facenormal, tangentvectors[1]);

	// normalize
	VectorNormalize(tangentvectors[0]);
	VectorNormalize(tangentvectors[1]);

	for( i = 0; i < MAXLIGHTMAPS && f->styles[i] != 255; i++ ) {
		for( j = 0, sample = l.sample[i]; j < l.numsamples; j++, sample++ ) {
			int red, green, blue, white, tn[3];
			vec3_t n;
			red   = (int)sample->c[0];
			green = (int)sample->c[1];
			blue  = (int)sample->c[2];
			white = (int)((sample->c[0] + sample->c[1] + sample->c[2]) * (1.0 / 3.0));
			// output tangentspace normalmap instead of modelspace, this is
			// done by doing a matrix multiply on the normal vector with the
			// tangent matrix
			// additionally we renormalize the modelspace normal here by
			// multiplying by 128 (a scale used to remap into the 0-255 color
			// range expected in normalmaps) divided by the current length of
			// the normal
			VectorCopy(sample->n, n);
			VectorNormalize(n);
			//if (VectorLength(n) < 0.01)
			//	VectorCopy(l.tangentvectors[2], n);
			tn[0] = (int)(DotProduct(n, tangentvectors[0]) * 128 + 128);
			tn[1] = (int)(DotProduct(n, tangentvectors[1]) * 128 + 128);
			tn[2] = (int)(DotProduct(n, tangentvectors[2]) * 128 + 128);
			//tn[0] = (int)(n[0] * 128 + 128);
			//tn[1] = (int)(n[1] * 128 + 128);
			//tn[2] = (int)(n[2] * 128 + 128);

			*lit++ = bound( 0, red, 255 );
			*lit++ = bound( 0, green, 255 );
			*lit++ = bound( 0, blue, 255 );
			*out++ = bound( 0, white, 255 );
			*nmap++ = bound(0, tn[0], 255);
			*nmap++ = bound(0, tn[1], 255);
			*nmap++ = bound(0, tn[2], 255);
		}
	}
}
