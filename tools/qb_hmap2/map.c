// map.c

#include "bsp5.h"

// just for statistics
int			nummapbrushfaces;

int			nummapbrushes;
mbrush_t	mapbrushes[MAX_MAP_BRUSHES];

int			nummapplanes;
plane_t		mapplanes[MAX_MAP_PLANES];

int			nummiptex;
char		miptex[MAX_MAP_TEXINFO][128]; // LordHavoc: was [16]

int mapversion = 0;

//============================================================================

/*
===============
FindPlane

Returns a global plane number and the side that will be the front
===============
*/
int	FindPlane( plane_t *dplane, int *side )
{
	int			i;
	plane_t		*dp, pl;
	vec_t		dot;

	pl = *dplane;
	NormalizePlane( &pl );

	if( DotProduct( pl.normal, dplane->normal ) > 0 )
		*side = 0;
	else
		*side = 1;

	for( i = 0, dp = mapplanes; i < nummapplanes; i++, dp++ ) {
		if( DotProduct( dp->normal, pl.normal ) > 1.0 - ANGLE_EPSILON && fabs( dp->dist - pl.dist ) < DIST_EPSILON )
			return i; // regular match
	}

	if( nummapplanes == MAX_MAP_PLANES )
		Error( "FindPlane: nummapplanes == MAX_MAP_PLANES" );

	dot = VectorLength( dplane->normal );
	if( dot < 1.0 - ANGLE_EPSILON || dot > 1.0 + ANGLE_EPSILON )
		Error( "FindPlane: normalization error (%f %f %f, length %f)", dplane->normal[0], dplane->normal[1], dplane->normal[2], dot );

	mapplanes[nummapplanes] = pl;

	return nummapplanes++;
}

/*
===============
FindMiptex
===============
*/
int FindMiptex( char *name )
{
	int		i;

	for( i = 0; i < nummiptex; i++ ) {
		if( !strcmp( name, miptex[i] ) )
			return i;
	}

	if( nummiptex == MAX_MAP_TEXINFO )
		Error ("nummiptex == MAX_MAP_TEXINFO");

	strcpy( miptex[i], name );

	return nummiptex++;
}

/*
===============
FindTexinfo

Returns a global texinfo number
===============
*/
int	FindTexinfo( texinfo_t *t )
{
	int			i, j;
	texinfo_t	*tex;

	// set the special flag
	if( (miptex[t->miptex][0] == '*' && !waterlightmap) || !Q_strncasecmp (miptex[t->miptex], "sky", 3) )
		t->flags |= TEX_SPECIAL;

	tex = texinfo;
	for( i = 0; i < numtexinfo; i++, tex++ ) {
		if( t->miptex != tex->miptex )
			continue;
		if( t->flags != tex->flags )
			continue;

		for( j = 0; j < 8; j++ ) {
			if( t->vecs[0][j] != tex->vecs[0][j] )
				break;
		}
		if( j != 8 )
			continue;

		return i;
	}

	// allocate a new texture
	if( numtexinfo == MAX_MAP_TEXINFO )
		Error( "numtexinfo == MAX_MAP_TEXINFO" );

	texinfo[i] = *t;

	return numtexinfo++;
}

//============================================================================


typedef enum brushtype_e
{
	BRUSHTYPE_QUAKE,
	BRUSHTYPE_PATCHDEF2,
	BRUSHTYPE_BRUSHDEF3,
	BRUSHTYPE_PATCHDEF3
}
brushtype_t;

void ParseBrushFace (entity_t *ent, mbrush_t **brushpointer, brushtype_t brushtype)
{
	int			i, j, hltexdef, bpface, brushplane;
//	int			facecontents, faceflags, facevalue, q2brushface, q3brushface;
	vec_t		planepts[3][3], t1[3], t2[3], d, rotate, scale[2], vecs[2][4], ang, sinv, cosv, bp[2][3];
	mface_t		*f, *f2;
	plane_t	plane;
	texinfo_t	tx;
	mbrush_t	*b;

	if (brushtype == BRUSHTYPE_PATCHDEF2 || brushtype == BRUSHTYPE_PATCHDEF3)
		return;
	// read the three point plane definition
	if (strcmp (token, "(") )
		Error ("parsing brush on line %d\n", scriptline);
	GetToken (false);
	planepts[0][0] = atof(token);
	GetToken (false);
	planepts[0][1] = atof(token);
	GetToken (false);
	planepts[0][2] = atof(token);
	GetToken (false);
	if (!strcmp(token, ")"))
	{
		brushplane = false;
		GetToken (false);
		if (strcmp(token, "("))
			Error("parsing brush on line %d\n", scriptline);
		GetToken (false);
		planepts[1][0] = atof(token);
		GetToken (false);
		planepts[1][1] = atof(token);
		GetToken (false);
		planepts[1][2] = atof(token);
		GetToken (false);
		if (strcmp(token, ")"))
			Error("parsing brush on line %d\n", scriptline);

		GetToken (false);
		if (strcmp(token, "("))
			Error("parsing brush on line %d\n", scriptline);
		GetToken (false);
		planepts[2][0] = atof(token);
		GetToken (false);
		planepts[2][1] = atof(token);
		GetToken (false);
		planepts[2][2] = atof(token);
		GetToken (false);
		if (strcmp(token, ")"))
			Error("parsing brush on line %d\n", scriptline);

		// convert points to a plane
		VectorSubtract(planepts[0], planepts[1], t1);
		VectorSubtract(planepts[2], planepts[1], t2);
		CrossProduct(t1, t2, plane.normal);
		VectorNormalize(plane.normal);
		plane.dist = DotProduct(planepts[1], plane.normal);
	}
	else
	{
		// oh, it's actually a 4 value plane
		brushplane = true;
		plane.normal[0] = planepts[0][0];
		plane.normal[1] = planepts[0][1];
		plane.normal[2] = planepts[0][2];
		plane.dist = -atof(token);
		GetToken (false);
		if (strcmp(token, ")"))
			Error("parsing brush on line %d\n", scriptline);
	}

	// read the texturedef
	memset (&tx, 0, sizeof(tx));
	GetToken (false);
	bpface = false;
	hltexdef = false;
	if (!strcmp(token, "("))
	{
		// brush primitives, utterly insane
		bpface = true;
		// (
		GetToken(false);
		// (
		GetToken(false);
		bp[0][0] = atof(token);
		GetToken(false);
		bp[0][1] = atof(token);
		GetToken(false);
		bp[0][2] = atof(token);
		GetToken(false);
		// )
		GetToken(false);
		// (
		GetToken(false);
		bp[1][0] = atof(token);
		GetToken(false);
		bp[1][1] = atof(token);
		GetToken(false);
		bp[1][2] = atof(token);
		GetToken(false);
		// )
		GetToken (false);
		GetToken (false);
		tx.miptex = FindMiptex (token);
		rotate = 0;
		scale[0] = 1;
		scale[1] = 1;
	}
	else
	{
		// if the texture name contains a / then this is a q2/q3 brushface
		// strip off the path, wads don't use a path on texture names
		tx.miptex = FindMiptex (token);
		GetToken (false);
		if (!strcmp(token, "["))
		{
			hltexdef = true;
			// S vector
			GetToken(false);
			vecs[0][0] = (vec_t)atof(token);
			GetToken(false);
			vecs[0][1] = (vec_t)atof(token);
			GetToken(false);
			vecs[0][2] = (vec_t)atof(token);
			GetToken(false);
			vecs[0][3] = (vec_t)atof(token);
			// ]
			GetToken(false);
			// [
			GetToken(false);
			// T vector
			GetToken(false);
			vecs[1][0] = (vec_t)atof(token);
			GetToken(false);
			vecs[1][1] = (vec_t)atof(token);
			GetToken(false);
			vecs[1][2] = (vec_t)atof(token);
			GetToken(false);
			vecs[1][3] = (vec_t)atof(token);
			// ]
			GetToken(false);

			// rotation (unused - implicit in tex coords)
			GetToken(false);
			rotate = 0;
		}
		else
		{
			vecs[0][3] = (vec_t)atof(token); // LordHavoc: float coords
			GetToken (false);
			vecs[1][3] = (vec_t)atof(token); // LordHavoc: float coords
			GetToken (false);
			rotate = atof(token); // LordHavoc: float coords
		}

		GetToken (false);
		scale[0] = (vec_t)atof(token); // LordHavoc: was already float coords
		GetToken (false);
		scale[1] = (vec_t)atof(token); // LordHavoc: was already float coords

		bp[0][0] = 1;
		bp[0][1] = 0;
		bp[0][2] = 0;
		bp[1][0] = 0;
		bp[1][1] = 1;
		bp[1][2] = 0;
	}
	// q3 .map properties, currently unused but parsed
//	facecontents = 0;
//	faceflags = 0;
//	facevalue = 0;
//	q2brushface = false;
//	q3brushface = false;
	if (GetToken (false))
	{
//		q2brushface = true;
//		facecontents = atoi(token);
		if (GetToken (false))
		{
//			faceflags = atoi(token);
			if (GetToken (false))
			{
//				q2brushface = false;
//				q3brushface = true;
//				facevalue = atoi(token);
			}
		}
	}
	// skip trailing info (the 3 q3 .map parameters for example)
	while (GetToken (false));

	if (DotProduct(plane.normal, plane.normal) < 0.1)
	{
		printf ("WARNING: line %i: brush plane with no normal\n", scriptline);
		return;
	}

	scale[0] = 1.0 / scale[0];
	scale[1] = 1.0 / scale[1];

	if (bpface)
	{
		// calculate proper texture vectors from GTKRadiant/Doom3 brushprimitives matrix
		float a, ac, as, bc, bs;
		a = -atan2(plane.normal[2], sqrt(plane.normal[0]*plane.normal[0]+plane.normal[1]*plane.normal[1]));
		ac = cos(a);
		as = sin(a);
		a = atan2(plane.normal[1], plane.normal[0]);
		bc = cos(a);
		bs = sin(a);
		vecs[0][0] = -bs;
		vecs[0][1] = bc;
		vecs[0][2] = 0;
		vecs[1][0] = -as*bc;
		vecs[1][1] = -as*bs;
		vecs[1][2] = -ac;
		tx.vecs[0][0] = bp[0][0] * vecs[0][0] + bp[0][1] * vecs[1][0];
		tx.vecs[0][1] = bp[0][0] * vecs[0][1] + bp[0][1] * vecs[1][1];
		tx.vecs[0][2] = bp[0][0] * vecs[0][2] + bp[0][1] * vecs[1][2];
		tx.vecs[0][3] = bp[0][0] * vecs[0][3] + bp[0][1] * vecs[1][3] + bp[0][2];
		tx.vecs[1][0] = bp[1][0] * vecs[0][0] + bp[1][1] * vecs[1][0];
		tx.vecs[1][1] = bp[1][0] * vecs[0][1] + bp[1][1] * vecs[1][1];
		tx.vecs[1][2] = bp[1][0] * vecs[0][2] + bp[1][1] * vecs[1][2];
		tx.vecs[1][3] = bp[1][0] * vecs[0][3] + bp[1][1] * vecs[1][3] + bp[1][2];
	}
	else if (hltexdef)
	{
		// HL texture vectors are almost ready to go
		for (i = 0; i < 2; i++)
		{
			for (j = 0; j < 3; j++)
				tx.vecs[i][j] = vecs[i][j] * scale[i];
			tx.vecs[i][3] = vecs[i][3] /*+ DotProduct(origin, tx.vecs[i])*/;
// Sajt: ripped the commented out bit from the HL compiler code, not really sure what it is exactly doing
// 'origin': origin set on bmodel by origin brush or origin key
		}
	}
	else
	{
		// fake proper texture vectors from QuakeEd style

		// texture rotation around the plane normal
			 if (rotate ==  0) {sinv = 0;cosv = 1;}
		else if (rotate == 90) {sinv = 1;cosv = 0;}
		else if (rotate == 180) {sinv = 0;cosv = -1;}
		else if (rotate == 270) {sinv = -1;cosv = 0;}
		else {ang = rotate * (Q_PI / 180);sinv = sin(ang);cosv = cos(ang);}

		if (fabs(plane.normal[2]) < fabs(plane.normal[0]))
		{
			if (fabs(plane.normal[0]) < fabs(plane.normal[1]))
			{
				// Y primary
				VectorSet4(tx.vecs[0],  cosv*scale[0],  0,  sinv*scale[0], vecs[0][3]);
				VectorSet4(tx.vecs[1],  sinv*scale[1],  0, -cosv*scale[1], vecs[1][3]);
			}
			else
			{
				// X primary
				VectorSet4(tx.vecs[0],  0,  cosv*scale[0],  sinv*scale[0], vecs[0][3]);
				VectorSet4(tx.vecs[1],  0,  sinv*scale[1], -cosv*scale[1], vecs[1][3]);
			}
		}
		else if (fabs(plane.normal[2]) < fabs(plane.normal[1]))
		{
			// Y primary
			VectorSet4(tx.vecs[0],  cosv*scale[0],  0,  sinv*scale[0], vecs[0][3]);
			VectorSet4(tx.vecs[1],  sinv*scale[1],  0, -cosv*scale[1], vecs[1][3]);
		}
		else
		{
			// Z primary
			VectorSet4(tx.vecs[0],  cosv*scale[0],  sinv*scale[0], 0, vecs[0][3]);
			VectorSet4(tx.vecs[1],  sinv*scale[1], -cosv*scale[1], 0, vecs[1][3]);
		}
		//printf("plane + rotate scale = texture vectors:\n(%f %f %f %f) + [%f %f %f] =\n[%f %f %f %f] [%f %f %f %f]\n", plane.normal[0], plane.normal[1], plane.normal[2], plane.dist, rotate, scale[0], scale[1], tx.vecs[0][0], tx.vecs[0][1], tx.vecs[0][2], tx.vecs[0][3], tx.vecs[1][0], tx.vecs[1][1], tx.vecs[1][2], tx.vecs[1][3]);
	}

	for (i = 0;i < 2;i++)
	{
		for (j = 0;j < 4;j++)
		{
			if (tx.vecs[i][j] > -BOGUS_RANGE && tx.vecs[i][j] < BOGUS_RANGE)
				continue;
			printf( "WARNING: line %i: corrupt texture mapping vectors, using defaults\n", scriptline);
			cosv = 1;
			sinv = 0;
			scale[0] = 1;
			scale[1] = 1;
			vecs[0][3] = 0;
			vecs[1][3] = 0;
			if (fabs(plane.normal[2]) < fabs(plane.normal[0]))
			{
				if (fabs(plane.normal[0]) < fabs(plane.normal[1]))
				{
					// Y primary
					VectorSet4(tx.vecs[0],  cosv*scale[0],  0,  sinv*scale[0], vecs[0][3]);
					VectorSet4(tx.vecs[1],  sinv*scale[1],  0, -cosv*scale[1], vecs[1][3]);
				}
				else
				{
					// X primary
					VectorSet4(tx.vecs[0],  0,  cosv*scale[0],  sinv*scale[0], vecs[0][3]);
					VectorSet4(tx.vecs[1],  0,  sinv*scale[1], -cosv*scale[1], vecs[1][3]);
				}
			}
			else if (fabs(plane.normal[2]) < fabs(plane.normal[1]))
			{
				// Y primary
				VectorSet4(tx.vecs[0],  cosv*scale[0],  0,  sinv*scale[0], vecs[0][3]);
				VectorSet4(tx.vecs[1],  sinv*scale[1],  0, -cosv*scale[1], vecs[1][3]);
			}
			else
			{
				// Z primary
				VectorSet4(tx.vecs[0],  cosv*scale[0],  sinv*scale[0], 0, vecs[0][3]);
				VectorSet4(tx.vecs[1],  sinv*scale[1], -cosv*scale[1], 0, vecs[1][3]);
			}
			break;
		}
	}
	
	/*
	// LordHavoc: fix for CheckFace: point off plane errors in some maps (most notably QOOLE ones),
	// and hopefully preventing most 'portal clipped away' warnings
	VectorNormalize (plane.normal);
	for (j = 0;j < 3;j++)
		plane.normal[j] = (Q_rint((vec_t) plane.normal[j] * (vec_t) 8.0)) * (vec_t) (1.0 / 8.0);
	VectorNormalize (plane.normal);
	plane.dist = DotProduct (t3, plane.normal);
	d = (Q_rint(plane.dist * 8.0)) * (1.0 / 8.0);
	//if (fabs(d - plane.dist) >= (0.4 / 8.0))
	//	printf("WARNING: line %i: correcting minor math errors in brushface\n", scriptline);
	plane.dist = d;
	*/

	/*
	VectorNormalize (plane.normal);
	plane.dist = DotProduct (t3, plane.normal);

	VectorCopy(plane.normal, v);
	//for (j = 0;j < 3;j++)
	//	v[j] = (Q_rint((vec_t) v[j] * (vec_t) 32.0)) * (vec_t) (1.0 / 32.0);
	VectorNormalize (v);
	d = (Q_rint(DotProduct (t3, v) * 8.0)) * (1.0 / 8.0);

	// if deviation is too high, warn  (frequently happens on QOOLE maps)
	if (fabs(DotProduct(v, plane.normal) - 1.0) > (0.5 / 32.0)
	 || fabs(d - plane.dist) >= (0.25 / 8.0))
		printf("WARNING: line %i: minor misalignment of brushface\n"
			   "normal     %f %f %f (l: %f d: %f)\n"
			   "rounded to %f %f %f (l: %f d: %f r: %f)\n",
			   scriptline,
			   (vec_t) plane.normal[0], (vec_t) plane.normal[1], (vec_t) plane.normal[2], (vec_t) sqrt(DotProduct(plane.normal, plane.normal)), (vec_t) DotProduct (t3, plane.normal),
			   (vec_t) v[0], (vec_t) v[1], (vec_t) v[2], (vec_t) sqrt(DotProduct(v, v)), (vec_t) DotProduct(t3, v), (vec_t) d);
	//VectorCopy(v, plane.normal);
	//plane.dist = d;
	*/

	b = *brushpointer;
	if (b)
	{
		if (brushplane)
		{
			for (f2 = b->faces ; f2 ;f2=f2->next)
				if (VectorCompare(plane.normal, f2->plane.normal) && fabs(plane.dist - f2->plane.dist) < ON_EPSILON)
					break;
			if (f2)
			{
				printf ("WARNING: line %i: brush with duplicate plane\n", scriptline);
				return;
			}
		}
		else
		{
			// if the three points are all on a previous plane, it is a
			// duplicate plane
			for (f2 = b->faces ; f2 ; f2=f2->next)
			{
				for (i = 0;i < 3;i++)
				{
					d = DotProduct(planepts[i],f2->plane.normal) - f2->plane.dist;
					if (d < -ON_EPSILON || d > ON_EPSILON)
						break;
				}
				if (i==3)
					break;
			}
			if (f2)
			{
				printf ("WARNING: line %i: brush with duplicate plane\n", scriptline);
				return;
			}
		}
	}
	else
	{
		b = &mapbrushes[nummapbrushes];
		nummapbrushes++;
		b->next = ent->brushes;
		ent->brushes = b;
		b->scriptline = scriptline;
		*brushpointer = b;
	}

	f = qmalloc(sizeof(mface_t));
	f->next = b->faces;
	b->faces = f;
	f->scriptline = scriptline;
	f->plane = plane;
	f->texinfo = FindTexinfo (&tx);
	nummapbrushfaces++;
}

/*
=================
ParseBrush
=================
*/
void ParseBrush (entity_t *ent)
{
	brushtype_t	brushtype;
	mbrush_t	*b;

	b = NULL;

	brushtype = BRUSHTYPE_QUAKE;
	for (;;)
	{
		if (!GetToken (true))
			break;
		if (!strcmp (token, "patchDef2"))
		{
			brushtype = BRUSHTYPE_PATCHDEF2;
			printf("patches not supported, skipping patch on line %d\n", scriptline);
		}
		else if (!strcmp (token, "brushDef3"))
			brushtype = BRUSHTYPE_BRUSHDEF3;
		else if (!strcmp (token, "patchDef3"))
		{
			brushtype = BRUSHTYPE_PATCHDEF3;
			printf("patches not supported, skipping patch on line %d\n", scriptline);
		}
		else if (!strcmp (token, "{") && brushtype != BRUSHTYPE_QUAKE)
		{
			for (;;)
			{
				if (!GetToken(true))
					Error("parsing brush on line %d\n", scriptline);
				if (!strcmp (token, "}"))
					break;
				ParseBrushFace(ent, &b, brushtype);
			}
		}
		else if (!strcmp (token, "}"))
			break;
		else if (!strcmp (token, "("))
			ParseBrushFace(ent, &b, BRUSHTYPE_QUAKE);
		else
			Error("parsing brush on line %d, unknown token \"%s\"\n", scriptline, token);
	}
}

/*
============
MoveEntityBrushesIntoWorld

Moves entity's brushes into tree.
============
*/
void MoveEntityBrushesIntoWorld( entity_t *ent )
{
	entity_t *world;
	mbrush_t *b, *next;

	world = &entities[0];
	if( ent == world )
		return;

	for( b = ent->brushes; b; b = next ) {
		next = b->next;
		b->next = world->brushes;
		world->brushes = b;
	}

	ent->brushes = NULL;
}

/*
================
ParseEntity
================
*/
qboolean ParseEntity (void)
{
	epair_t *e, *next;
	entity_t *ent;

	if( !GetToken( true ) )
		return false;
	if (!strcmp(token, "Version"))
	{
		GetToken(true);
		mapversion = atof(token);
		GetToken(true);
	}
	if( strcmp( token, "{" ) )
		Error( "ParseEntity: line %i: { not found", scriptline );

	if( num_entities == MAX_MAP_ENTITIES )
		Error( "num_entities == MAX_MAP_ENTITIES" );

	ent = &entities[num_entities++];
	ent->epairs = NULL;
	ent->scriptline = scriptline; // LordHavoc: better error reporting

	do {
		fflush( stdout );

		if( !GetToken( true ) )
			Error( "ParseEntity: line %i: EOF without closing brace", scriptline );

		if( !strcmp (token, "}") )
			break;
		else if( !strcmp( token, "{" ) )  {
			ParseBrush( ent );
		} else {
			e = ParseEpair ();
			e->next = ent->epairs;
			ent->epairs = e;
		}
	} while( 1 );

	if( !strcmp( ValueForKey( ent, "classname" ), "func_group" ) ) {
		MoveEntityBrushesIntoWorld( ent );

		for( e = ent->epairs; e; e = next ) {
			next = e->next;
			qfree( e );
		}

		num_entities--;
	}

	return true;
}

/*
================
LoadMapFile
================
*/
void LoadMapFile (char *filename)
{
	void	*buf;

	num_entities = 0;

	nummapbrushfaces = 0;
	nummapbrushes = 0;
	nummapplanes = 0;
	nummiptex = 0;
	mapversion = 0;

	LoadFile (filename, &buf);

	StartTokenParsing (buf);

	while (ParseEntity ());

	qfree (buf);

	qprintf ("--- LoadMapFile ---\n");
	qprintf ("%s\n", filename);
	qprintf ("%5i faces\n", nummapbrushfaces);
	qprintf ("%5i brushes\n", nummapbrushes);
	qprintf ("%5i entities\n", num_entities);
	qprintf ("%5i textures\n", nummiptex);
	qprintf ("%5i texinfo\n", numtexinfo);
}
