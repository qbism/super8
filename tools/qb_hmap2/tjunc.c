// tjunc.c

#include "bsp5.h"


typedef struct wvert_s
{
	vec_t	t;
	struct wvert_s *prev, *next;
} wvert_t;

typedef struct wedge_s
{
	struct wedge_s *next;
	vec3_t	dir;
	vec3_t	origin;
	wvert_t	head;
} wedge_t;

int		numwedges, numwverts;
int		c_tjuncs;
int		c_tjuncfaces;
int		c_rotated;
int		c_degenerateEdges;
int		c_degenerateFaces;

wvert_t	*wverts;
wedge_t	*wedges;

//============================================================================

wedge_t	*wedge_hash[NUM_HASH];

static	vec3_t	hash_min, hash_scale;

static	void InitHash (vec3_t mins, vec3_t maxs)
{
	vec3_t	size;
	vec_t	volume;
	vec_t	scale;
	int		newsize[2];

	VectorCopy (mins, hash_min);
	VectorSubtract (maxs, mins, size);
	memset (wedge_hash, 0, sizeof(wedge_hash));

	volume = size[0]*size[1];

	scale = sqrt(volume / NUM_HASH);

	newsize[0] = size[0] / scale;
	newsize[1] = size[1] / scale;

	hash_scale[0] = newsize[0] / size[0];
	hash_scale[1] = newsize[1] / size[1];
	hash_scale[2] = newsize[1];
}

static	unsigned HashVec (vec3_t vec)
{
	unsigned	h;

	h =	hash_scale[0] * (vec[0] - hash_min[0]) * hash_scale[2]
		+ hash_scale[1] * (vec[1] - hash_min[1]);
	if ( h >= NUM_HASH)
		return NUM_HASH - 1;
	return h;
}

//============================================================================

// Vic: changed this to qboolean
qboolean CanonicalVector (vec3_t vec)
{
	vec_t length;

	length = VectorLength (vec);

	// Vic: ignore degenerate edges
	if (length < 0.1)
		return false;

	length = (vec_t)1.0 / length;
	vec[0] *= length;
	vec[1] *= length;
	vec[2] *= length;

	if (vec[0] > EQUAL_EPSILON)
		return true;
	else if (vec[0] < -EQUAL_EPSILON)
	{
		VectorNegate (vec, vec);
		return true;
	}
	else
		vec[0] = 0;

	if (vec[1] > EQUAL_EPSILON)
		return true;
	else if (vec[1] < -EQUAL_EPSILON)
	{
		VectorNegate (vec, vec);
		return true;
	}
	else
		vec[1] = 0;

	if (vec[2] > EQUAL_EPSILON)
		return true;
	else if (vec[2] < -EQUAL_EPSILON)
	{
		VectorNegate (vec, vec);
		return true;
	}
	else
		vec[2] = 0;

	Error ("CanonicalVector: degenerate");
	return false;
}

wedge_t	*FindEdge (vec3_t p1, vec3_t p2, vec_t *t1, vec_t *t2)
{
	vec3_t	origin;
	vec3_t	dir;
	wedge_t	*w;
	vec_t	temp;
	int		h;

	VectorSubtract (p2, p1, dir);

	// ignore degenerate edges
	if (!CanonicalVector (dir))
	{
		c_degenerateEdges++;
		return NULL;
	}

	*t1 = DotProduct (p1, dir);
	*t2 = DotProduct (p2, dir);

	VectorMA (p1, -*t1, dir, origin);

	if (*t1 > *t2)
	{
		temp = *t1;
		*t1 = *t2;
		*t2 = temp;
	}

	h = HashVec (origin);

	for (w = wedge_hash[h] ; w ; w=w->next)
	{
		temp = w->origin[0] - origin[0];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;
		temp = w->origin[1] - origin[1];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;
		temp = w->origin[2] - origin[2];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;

		temp = w->dir[0] - dir[0];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;
		temp = w->dir[1] - dir[1];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;
		temp = w->dir[2] - dir[2];
		if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
			continue;

		return w;
	}

	if (numwedges == MAX_WEDGES)
		Error ("FindEdge: numwedges == MAX_WEDGES");
	w = &wedges[numwedges];
	numwedges++;

	w->next = wedge_hash[h];
	wedge_hash[h] = w;

	VectorCopy (origin, w->origin);
	VectorCopy (dir, w->dir);
	w->head.next = w->head.prev = &w->head;
	w->head.t = 99999;
	return w;
}


/*
===============
AddVert

===============
*/

void AddVert (wedge_t *w, vec_t t)
{
	wvert_t	*v, *newv;

	v = w->head.next;
	do
	{
		if (fabs(v->t - t) < T_EPSILON)
			return;
		if (v->t > t)
			break;
		v = v->next;
	} while (1);

// insert a new wvert before v
	if (numwverts == MAX_WVERTS)
		Error ("AddVert: numwverts == MAX_WVERTS");

	newv = &wverts[numwverts];
	numwverts++;

	newv->t = t;
	newv->next = v;
	newv->prev = v->prev;
	v->prev->next = newv;
	v->prev = newv;
}


/*
===============
AddEdge

===============
*/
void AddEdge (vec3_t p1, vec3_t p2)
{
	wedge_t	*w;
	vec_t	t1, t2;

	w = FindEdge(p1, p2, &t1, &t2);
	if (w)
	{
		AddVert (w, t1);
		AddVert (w, t2);
	}
}

//============================================================================

face_t	*newlist;

#define MAX_VERTS_ON_FACE		32
#define MAX_VERTS_ON_SUPERFACE	8192

typedef struct
{
	int numpoints;
	vec3_t points[MAX_VERTS_ON_SUPERFACE];
	face_t original;
} superface_t;

superface_t *superface;

void FaceFromSuperface (face_t *original)
{
	int			i, numpts;
	face_t		*newf, *chain;
	vec3_t		dir, test;
	vec_t		v;
	int			firstcorner, lastcorner;

	chain = NULL;
	do
	{
		if( superface->numpoints < 3 ) {
			c_degenerateFaces++;
			return;
		}

		if (superface->numpoints <= MAX_VERTS_ON_FACE)
		{	// the face is now small enough without more cutting
			// so copy it back to the original
			*original = superface->original;
			original->winding = CopyWindingExt( superface->numpoints, superface->points );
			original->original = chain;
			original->next = newlist;
			newlist = original;
			return;
		}

		c_tjuncfaces++;

restart:
	// find the last corner
		VectorSubtract (superface->points[superface->numpoints-1], superface->points[0], dir);
		VectorNormalize (dir);
		for (lastcorner=superface->numpoints-1 ; lastcorner > 0 ; lastcorner--)
		{
			VectorSubtract (superface->points[lastcorner-1], superface->points[lastcorner], test);
			VectorNormalize (test);
			v = DotProduct (test, dir);
			if (v < 0.9999 || v > 1.00001)
			{
				break;
			}
		}

	// find the first corner
		VectorSubtract (superface->points[1], superface->points[0], dir);
		VectorNormalize (dir);
		for (firstcorner=1 ; firstcorner < superface->numpoints-1 ; firstcorner++)
		{
			VectorSubtract (superface->points[firstcorner+1], superface->points[firstcorner], test);
			VectorNormalize (test);
			v = DotProduct (test, dir);
			if (v < 0.9999 || v > 1.00001)
				break;
		}

		if (firstcorner+2 >= MAX_VERTS_ON_FACE)
		{
			c_rotated++;
		// rotate the point winding
			VectorCopy (superface->points[0], test);
			for (i=1 ; i<superface->numpoints ; i++)
				VectorCopy (superface->points[i], superface->points[i-1]);
			VectorCopy (test, superface->points[superface->numpoints-1]);
			goto restart;
		}


	// cut off as big a piece as possible, less than MAXPOINTS, and not
	// past lastcorner
		newf = NewFaceFromFace (&superface->original);
		newf->original = chain;
		chain = newf;
		newf->next = newlist;
		newlist = newf;

		if (superface->numpoints - firstcorner <= MAX_VERTS_ON_FACE)
			numpts = firstcorner + 2;
		else if (lastcorner+2 < MAX_VERTS_ON_FACE && superface->numpoints - lastcorner <= MAX_VERTS_ON_FACE)
			numpts = lastcorner + 2;
		else
			numpts = MAX_VERTS_ON_FACE;

		if( numpts < 3 ) {
			c_degenerateFaces++;
			return;
		}
		newf->winding = CopyWindingExt( numpts, superface->points );

		for (i=newf->winding->numpoints-1 ; i<superface->numpoints ; i++)
			VectorCopy (superface->points[i], superface->points[i-(newf->winding->numpoints-2)]);
		superface->numpoints -= (newf->winding->numpoints-2);
	} while (1);
}

/*
===============
FixFaceEdges

===============
*/
void FixFaceEdges (face_t *f)
{
	int		i, j, k;
	wedge_t	*w;
	wvert_t	*v;
	vec_t	t1, t2;

	if( f->winding->numpoints > MAX_VERTS_ON_SUPERFACE )
		Error( "FixFaceEdges: f->winding->numpoints > MAX_VERTS_ON_SUPERFACE" );

	superface->original = *f;
	superface->numpoints = f->winding->numpoints;
	memcpy( superface->points, f->winding->points, sizeof(vec3_t)*f->winding->numpoints );
	FreeWinding( f->winding );

	// LordHavoc: FIXME: rewrite this mess to find points on edges,
	// rather than finding edges that run into polygons
restart:
	for (i=0 ; i < superface->numpoints ; i++)
	{
		j = (i+1)%superface->numpoints;

		w = FindEdge (superface->points[i], superface->points[j], &t1, &t2);
		if (!w)
			continue;

		for (v=w->head.next ; v->t < t1 + T_EPSILON ; v = v->next)
		{
		}

		if (v->t < t2-T_EPSILON)
		{
			c_tjuncs++;
		// insert a new vertex here
			for (k = superface->numpoints ; k> j ; k--)
				VectorCopy (superface->points[k-1], superface->points[k]);
			VectorMA (w->origin, v->t, w->dir, superface->points[j]);
			superface->numpoints++;
			goto restart;
		}
	}

	// the face might be split into multiple faces because of too many edges
	FaceFromSuperface( f );
}

//============================================================================

void tjunc_find_r (node_t *node)
{
	int			i, j;
	face_t		*f;
	winding_t	*w;

	if (node->planenum == PLANENUM_LEAF)
		return;

	for (f=node->faces ; f ; f=f->next) {
		w = f->winding;
		for( i = 0, j = 1; i < w->numpoints; i++, j = (i + 1) % w->numpoints )
			 AddEdge( w->points[i], w->points[j] );
	}

	tjunc_find_r (node->children[0]);
	tjunc_find_r (node->children[1]);
}

void tjunc_fix_r (node_t *node)
{
	face_t	*f, *next;

	if (node->planenum == PLANENUM_LEAF)
		return;

	newlist = NULL;

	for (f=node->faces ; f ; f=next)
	{
		next = f->next;
		FixFaceEdges (f);
	}

	node->faces = newlist;

	tjunc_fix_r (node->children[0]);
	tjunc_fix_r (node->children[1]);
}

/*
===========
FixTJunctions
===========
*/
void FixTJunctions(tree_t *tree)
{
	vec_t	radius;
	vec3_t	maxs, mins;

	qprintf ("---- tjunc ----\n");

	if (notjunc)
	{
		printf ("skipped\n");
		return;
	}

//
// identify all points on common edges
//

// origin points won't allways be inside the map, so extend the hash area
	radius = RadiusFromBounds( tree->mins, tree->maxs );
	VectorSet( maxs, radius, radius, radius );
	VectorSet( mins, -radius, -radius, -radius );

	InitHash (mins, maxs);

	numwedges = numwverts = 0;
	wverts = qmalloc( sizeof(*wverts) * MAX_WVERTS );
	wedges = qmalloc( sizeof(*wverts) * MAX_WEDGES );
	superface = qmalloc (sizeof(superface_t));

	tjunc_find_r (tree->headnode);

	qprintf ("%i world edges  %i edge points\n", numwedges, numwverts);

//
// add extra vertexes on edges where needed
//
	c_tjuncs = c_tjuncfaces = c_degenerateEdges = c_degenerateFaces = c_rotated = 0;

	tjunc_fix_r (tree->headnode);

	qfree( wverts );
	qfree( wedges );
	qfree (superface);

	// Vic: report number of degenerate edges
	qprintf ("%i degenerate edges\n", c_degenerateEdges);
	qprintf ("%i degenerate faces\n", c_degenerateFaces);
	qprintf ("%i edges added by tjunctions\n", c_tjuncs);
	qprintf ("%i faces added by tjunctions\n", c_tjuncfaces);
	qprintf ("%i naturally ordered\n", c_tjuncfaces - c_rotated);
	qprintf ("%i rotated orders\n", c_rotated);
}

