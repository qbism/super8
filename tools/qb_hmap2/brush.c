// brush.c

#include "bsp5.h"

int			numbrushfaces;
mface_t		faces[MAX_FACES];		// beveled clipping hull can generate many extra

entity_t *CurrentEntity;

//============================================================================

/*
===========
AllocBrush
===========
*/
brush_t *AllocBrush( void )
{
	brush_t	*b;

	b = qmalloc( sizeof( brush_t ) );
	memset( b, 0, sizeof( brush_t ) );

	return b;
}

/*
===========
FreeBrush
===========
*/
void FreeBrush( brush_t *b ) {
	qfree( b );
}

//============================================================================

/*
=============================================================================

TURN BRUSHES INTO GROUPS OF FACES

=============================================================================
*/

vec3_t		brush_mins, brush_maxs;
face_t		*brush_faces;

entity_t *FindTargetEntity(char *targetname)
{
	int			entnum;

	for (entnum = 0;entnum < num_entities;entnum++)
		if (!strcmp(targetname, ValueForKey(&entities[entnum], "targetname")))
			return &entities[entnum];
	return NULL;
}

/*
=================
CreateBrushFaces
=================
*/
void CreateBrushFaces (void)
{
	int				i,j, k;
	vec_t			r;
	face_t			*f, *next;
	winding_t		*w;
	plane_t			clipplane, faceplane;
	mface_t			*mf;
	vec3_t			offset, point;

	offset[0] = offset[1] = offset[2] = 0;
	ClearBounds( brush_mins, brush_maxs );

	brush_faces = NULL;

	if (!strncmp(ValueForKey(CurrentEntity, "classname"), "rotate_", 7))
	{
		entity_t	*FoundEntity;
		char 		*searchstring;
		char		text[20];

		searchstring = ValueForKey (CurrentEntity, "target");
		FoundEntity = FindTargetEntity(searchstring);
		if (FoundEntity)
			GetVectorForKey(FoundEntity, "origin", offset);

		sprintf(text, "%g %g %g", offset[0], offset[1], offset[2]);
		SetKeyValue(CurrentEntity, "origin", text);
	}

	GetVectorForKey(CurrentEntity, "origin", offset);
	//printf("%i brushfaces at offset %f %f %f\n", numbrushfaces, offset[0], offset[1], offset[2]);

	for (i = 0;i < numbrushfaces;i++)
	{
		mf = &faces[i];

		//printf("plane %f %f %f %f\n", mf->plane.normal[0], mf->plane.normal[1], mf->plane.normal[2], mf->plane.dist);
		faceplane = mf->plane;
		w = BaseWindingForPlane (&faceplane);

		//VectorNegate( faceplane.normal, point );
		for (j = 0;j < numbrushfaces && w;j++)
		{
			clipplane = faces[j].plane;
			if( j == i/* || VectorCompare( clipplane.normal, point )*/ )
				continue;

			// flip the plane, because we want to keep the back side
			VectorNegate(clipplane.normal, clipplane.normal);
			clipplane.dist *= -1;

			w = ClipWindingEpsilon (w, &clipplane, ON_EPSILON, true);
		}

		if (!w)
		{
			//printf("----- skipped plane -----\n");
			continue;	// overcontrained plane
		}

		// this face is a keeper
		f = AllocFace ();
		f->winding = w;

		for (j = 0;j < w->numpoints;j++)
		{
			for (k = 0;k < 3;k++)
			{
				point[k] = w->points[j][k] - offset[k];
				r = Q_rint( point[k] );
				if ( fabs( point[k] - r ) < ZERO_EPSILON)
					w->points[j][k] = r;
				else
					w->points[j][k] = point[k];

				// check for incomplete brushes
				if( w->points[j][k] >= BOGUS_RANGE || w->points[j][k] <= -BOGUS_RANGE )
					break;
			}

			// remove this brush
			if (k < 3)
			{
				FreeFace (f);
				for (f = brush_faces; f; f = next)
				{
					next = f->next;
					FreeFace (f);
				}
				brush_faces = NULL;
				//printf("----- skipped brush -----\n");
				return;
			}

			AddPointToBounds( w->points[j], brush_mins, brush_maxs );
		}

		CheckWinding( w, mf->scriptline );

		faceplane.dist -= DotProduct(faceplane.normal, offset);
		f->texturenum = mf->texinfo;
		f->planenum = FindPlane (&faceplane, &f->planeside);
		f->next = brush_faces;
		brush_faces = f;
	}

	// Rotatable objects have to have a bounding box big enough
	// to account for all its rotations.
	if (DotProduct(offset, offset))
	{
		vec_t delta;

		delta = RadiusFromBounds( brush_mins, brush_maxs );

		for (k = 0;k < 3;k++)
		{
			brush_mins[k] = -delta;
			brush_maxs[k] = delta;
		}
	}

	//printf("%i : %f %f %f : %f %f %f\n", numbrushfaces, brush_mins[0], brush_mins[1], brush_mins[2], brush_maxs[0], brush_maxs[1], brush_maxs[2]);
}

/*
==============================================================================

BEVELED CLIPPING HULL GENERATION

This is done by brute force, and could easily get a lot faster if anyone cares.
==============================================================================
*/

// LordHavoc: these were 32 and 64 respectively
#define	MAX_HULL_POINTS	512
#define	MAX_HULL_EDGES	1024

int		num_hull_points;
vec3_t	hull_points[MAX_HULL_POINTS];
vec3_t	hull_corners[MAX_HULL_POINTS*8];
int		num_hull_edges;
int		hull_edges[MAX_HULL_EDGES][2];

/*
============
AddBrushPlane
=============
*/
void AddBrushPlane (plane_t *plane)
{
	int		i;
	plane_t	*pl;
	double	l;

	l = VectorLength (plane->normal);
	if (l < 1.0 - NORMAL_EPSILON || l > 1.0 + NORMAL_EPSILON)
		Error ("AddBrushPlane: bad normal (%f %f %f, length %f)", plane->normal[0], plane->normal[1], plane->normal[2], l);

	for (i=0 ; i<numbrushfaces ; i++)
	{
		pl = &faces[i].plane;
		if (VectorCompare (pl->normal, plane->normal) && fabs(pl->dist - plane->dist) < ON_EPSILON)
			return;
	}
	if (numbrushfaces == MAX_FACES)
		Error ("AddBrushPlane: numbrushfaces == MAX_FACES");

	faces[i].plane = *plane;
	faces[i].texinfo = faces[0].texinfo;

	numbrushfaces++;
}


/*
============
TestAddPlane

Adds the given plane to the brush description if all of the original brush
vertexes can be put on the front side
=============
*/
void TestAddPlane (plane_t *plane)
{
	int		i, c;
	vec_t	d;
	vec_t	*corner;
	plane_t	flip;
	vec3_t	inv;
	int		counts[3];
	plane_t	*pl;

	// see if the plane has allready been added
	for (i=0 ; i<numbrushfaces ; i++)
	{
		pl = &faces[i].plane;
		if (VectorCompare (plane->normal, pl->normal) && fabs(plane->dist - pl->dist) < ON_EPSILON)
			return;
		VectorNegate (plane->normal, inv);
		if (VectorCompare (inv, pl->normal) && fabs(plane->dist + pl->dist) < ON_EPSILON)
			return;
	}

	// check all the corner points
	counts[0] = counts[1] = counts[2] = 0;
	c = num_hull_points * 8;

	corner = hull_corners[0];
	for (i=0 ; i<c ; i++, corner += 3)
	{
		d = DotProduct (corner, plane->normal) - plane->dist;
		if (d < -ON_EPSILON)
		{
			if (counts[0])
				return;
			counts[1]++;
		}
		else if (d > ON_EPSILON)
		{
			if (counts[1])
				return;
			counts[0]++;
		}
		else
			counts[2]++;
	}

	// the plane is a seperator

	if (counts[0])
	{
		VectorNegate (plane->normal, flip.normal);
		flip.dist = -plane->dist;
		plane = &flip;
	}

	AddBrushPlane (plane);
}

/*
============
AddHullPoint

Doesn't add if duplicated
=============
*/
int AddHullPoint (vec3_t p, int hullnum)
{
	int		i;
	vec_t	*c;
	int		x,y,z;

	for (i=0 ; i<num_hull_points ; i++)
		if (VectorCompare (p, hull_points[i]))
			return i;

	VectorCopy (p, hull_points[num_hull_points]);

	c = hull_corners[i*8];

	for (x=0 ; x<2 ; x++)
		for (y=0 ; y<2 ; y++)
			for (z=0; z<2 ; z++)
			{
				c[0] = p[0] - hullinfo.hullsizes[hullnum][x^1][0];
				c[1] = p[1] - hullinfo.hullsizes[hullnum][y^1][1];
				c[2] = p[2] - hullinfo.hullsizes[hullnum][z^1][2];
				c += 3;
			}

	if (num_hull_points == MAX_HULL_POINTS)
		Error ("AddHullPoint: MAX_HULL_POINTS");

	num_hull_points++;

	return i;
}


/*
============
AddHullEdge

Creates all of the hull planes around the given edge, if not done allready
=============
*/
void AddHullEdge (vec3_t p1, vec3_t p2, int hullnum)
{
	int		pt1, pt2;
	int		i;
	int		a, b, c, d, e;
	vec3_t	edgevec, planeorg, planevec;
	plane_t	plane;
	vec_t	length;

	pt1 = AddHullPoint (p1, hullnum);
	pt2 = AddHullPoint (p2, hullnum);

	for (i=0 ; i<num_hull_edges ; i++)
		if ((hull_edges[i][0] == pt1 && hull_edges[i][1] == pt2) || (hull_edges[i][0] == pt2 && hull_edges[i][1] == pt1))
			return;	// already added

	if (num_hull_edges == MAX_HULL_EDGES)
		Error ("AddHullEdge: MAX_HULL_EDGES");

	hull_edges[num_hull_edges][0] = pt1;
	hull_edges[num_hull_edges][1] = pt2;
	num_hull_edges++;

	VectorSubtract(p1, p2, edgevec);
	VectorNormalize(edgevec);

	for (a=0 ; a<3 ; a++)
	{
		b = (a+1)%3;
		c = (a+2)%3;

		planevec[a] = 1;
		planevec[b] = 0;
		planevec[c] = 0;
		CrossProduct(planevec, edgevec, plane.normal);
		length = VectorNormalize(plane.normal);

		/* If this edge is almost parallel to the hull edge, skip it. */
		if (length < ANGLE_EPSILON)
			continue;

		for (d = 0 ; d < 2 ; d++)
		{
			for (e = 0 ; e < 2 ; e++)
			{
				VectorCopy(p1, planeorg);
				planeorg[b] -= hullinfo.hullsizes[hullnum][d^1][b];
				planeorg[c] -= hullinfo.hullsizes[hullnum][e^1][c];
				plane.dist = DotProduct(planeorg, plane.normal);

				TestAddPlane(&plane);
			}
		}
	}
}

/*
============
ExpandBrush
=============
*/
void ExpandBrush (int hullnum)
{
	int			i, x, s;
	vec3_t		corner;
	winding_t	*w;
	plane_t		plane;

	int				j, k, numwindings;
	vec_t			r;
	winding_t		**windings;
	plane_t			clipplane, faceplane;
	mface_t			*mf;
	vec3_t			point;
	vec3_t		mins, maxs;

	if (!numbrushfaces)
		return;

	num_hull_points = 0;
	num_hull_edges = 0;

	ClearBounds( mins, maxs );

	// generate windings and bounds data
	numwindings = 0;
	windings = calloc(numbrushfaces, sizeof(*windings));
	for (i = 0;i < numbrushfaces;i++)
	{
		mf = &faces[i];
		windings[i] = NULL;

		faceplane = mf->plane;
		w = BaseWindingForPlane (&faceplane);

		for (j = 0;j < numbrushfaces && w;j++)
		{
			clipplane = faces[j].plane;
			if( j == i )
				continue;

			// flip the plane, because we want to keep the back side
			VectorNegate(clipplane.normal, clipplane.normal);
			clipplane.dist *= -1;

			w = ClipWindingEpsilon (w, &clipplane, ON_EPSILON, true);
		}

		if (!w)
			continue;	// overcontrained plane

		for (j = 0;j < w->numpoints;j++)
		{
			for (k = 0;k < 3;k++)
			{
				point[k] = w->points[j][k];
				r = Q_rint( point[k] );
				if ( fabs( point[k] - r ) < ZERO_EPSILON)
					w->points[j][k] = r;
				else
					w->points[j][k] = point[k];

				// check for incomplete brushes
				if( w->points[j][k] >= BOGUS_RANGE || w->points[j][k] <= -BOGUS_RANGE )
					return;
			}

			AddPointToBounds( w->points[j], mins, maxs );
		}

		windings[i] = w;
	}

	// add all of the corner offsets
	for (i = 0;i < numwindings;i++)
	{
		w = windings[i];
		for (j = 0;j < w->numpoints;j++)
			AddHullPoint(w->points[j], hullnum);
	}

	// expand the face planes
	for (i = 0;i < numbrushfaces;i++)
	{
		mf = &faces[i];
		for (x=0 ; x<3 ; x++)
		{
			if (mf->plane.normal[x] > 0)
				corner[x] = -hullinfo.hullsizes[hullnum][0][x];
			else if (mf->plane.normal[x] < 0)
				corner[x] = -hullinfo.hullsizes[hullnum][1][x];
		}
		mf->plane.dist += DotProduct (corner, mf->plane.normal);
	}

	// add any axis planes not contained in the brush to bevel off corners
	for (x=0 ; x<3 ; x++)
		for (s=-1 ; s<=1 ; s+=2)
		{
			// add the plane
			VectorClear (plane.normal);
			plane.normal[x] = s;
			if (s == -1)
				plane.dist = -mins[x] + hullinfo.hullsizes[hullnum][1][x];
			else
				plane.dist = maxs[x] + -hullinfo.hullsizes[hullnum][0][x];
			AddBrushPlane (&plane);
		}

	// add all of the edge bevels
	for (i = 0;i < numwindings;i++)
	{
		w = windings[i];
		for (j = 0;j < w->numpoints;j++)
			AddHullEdge(w->points[j], w->points[(j+1)%w->numpoints], hullnum);
	}

	// free the windings as we no longer need them
	for (i = 0;i < numwindings;i++)
		if (windings[i])
			FreeWinding(windings[i]);
	free(windings);
}

//============================================================================


/*
===============
LoadBrush

Converts a mapbrush to a bsp brush
===============
*/
brush_t *LoadBrush (mbrush_t *mb, int brushnum, int hullnum, qboolean worldmodel)
{
	brush_t		*b;
	int			contents;
	char		*name;
	mface_t		*f;

	// figure out the brush contents
	contents = CONTENTS_SOLID;
	for (f = mb->faces;f;f = f->next)
	{
		name = miptex[texinfo[f->texinfo].miptex];
		// nodraw brushes do not produce anything
		if (!Q_strcasecmp(name, "nodraw"))
			continue;
		if (!Q_strcasecmp(name, "common/nodraw"))
			continue;
		if (!Q_strcasecmp(name, "textures/common/nodraw"))
			continue;
		// textures which don't show up in the drawing hull
		if (hullnum == 0 && !Q_strcasecmp(name, "clip"))
			return NULL;
		if (hullnum == 0 && !Q_strcasecmp(name, "textures/common/clip"))
			return NULL;
		if (hullnum == 0 && !Q_strcasecmp(name, "textures/common/full_clip"))
			return NULL;
		if (!Q_strcasecmp(name, "textures/editor/visportal"))
			return NULL;
		if (name[0] == '*' && (worldmodel || !option_solidbmodels))
		{
			if (hullnum)
				return NULL; // water brushes don't show up in clipping hulls
			if (!Q_strncasecmp(name+1,"lava",4))
				contents = CONTENTS_LAVA;
			else if (!Q_strncasecmp(name+1,"slime",5))
				contents = CONTENTS_SLIME;
			else
				contents = CONTENTS_WATER;
		}
		else if (!Q_strncasecmp(name, "sky",3) && hullnum == 0)
			contents = CONTENTS_SKY;
	}

	//
	// create the faces
	//
	brush_faces = NULL;

	numbrushfaces = 0;
	for (f=mb->faces ; f ; f=f->next)
	{
		faces[numbrushfaces] = *f;
		// cliphulls do not use textures
		if (hullnum)
			faces[numbrushfaces].texinfo = 0;
		numbrushfaces++;
	}

	if (hullnum)
		ExpandBrush (hullnum);

	CreateBrushFaces ();

	if (!brush_faces)
	{
		printf ("WARNING: line %i: couldn't create faces for brush %i in entity %i (incomplete brush?)\n", mb->scriptline, brushnum, (int)(CurrentEntity - entities));
		return NULL;
	}

	//
	// create the brush
	//
	b = AllocBrush ();
	b->contents = contents;
	b->faces = brush_faces;
	VectorCopy (brush_mins, b->mins);
	VectorCopy (brush_maxs, b->maxs);
	// debugging code
	//printf("mapbrush\n");
	//for (f=mb->faces ; f ; f=f->next)
	//	printf("face %f %f %f %f \"%s\"\n", f->plane.normal[0], f->plane.normal[1], f->plane.normal[2], f->plane.dist, miptex[texinfo[f->texinfo].miptex]);
	//printf("bspbrush %i\n", numbrushfaces);
	//face_t		*face;
	//for (face=b->faces ; face ; face=face->next)
	//	printf("bspface %f %f %f %f\n", mapplanes[face->planenum].normal[0], mapplanes[face->planenum].normal[1], mapplanes[face->planenum].normal[2], mapplanes[face->planenum].dist);

	return b;
}

//=============================================================================

/*
============
Brush_LoadEntity
============
*/
void Brush_LoadEntity (entity_t *ent, tree_t *tree, int hullnum, qboolean worldmodel)
{
	mbrush_t	*mbr;
	int			brushnum, numbrushes;
	brush_t		*b, *next, *water, *other;

	numbrushes = 0;
	other = water = NULL;

	qprintf ("--- Brush_LoadEntity ---\n");

	CurrentEntity = ent;

	for (mbr = ent->brushes, brushnum = 0; mbr; mbr=mbr->next, brushnum++)
	{
		b = LoadBrush (mbr, brushnum, hullnum, worldmodel);
		if (!b)
			continue;

		numbrushes++;

		if (b->contents != CONTENTS_SOLID)
		{
			b->next = water;
			water = b;
		}
		else
		{
			b->next = other;
			other = b;
		}

		AddPointToBounds (b->mins, tree->mins, tree->maxs);
		AddPointToBounds (b->maxs, tree->mins, tree->maxs);
	}

	// add all of the water textures at the start
	for (b=water ; b ; b=next)
	{
		next = b->next;
		b->next = other;
		other = b;
	}

	tree->brushes = other;

	qprintf( "%i brushes read\n", numbrushes );
}
