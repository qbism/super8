// surfaces.c

#include "bsp5.h"

/*

a surface has all of the faces that could be drawn on a given plane
the outside filling stage can remove some of them so a better bsp can be generated

*/

//===========================================================================

/*
===========
AllocFace
===========
*/
face_t *AllocFace( void )
{
	face_t	*f;

	f = qmalloc( sizeof( face_t ) );
	memset( f, 0, sizeof( face_t ) );
	f->planenum = -1;
	f->outputnumber = -1;

	return f;
}

/*
===========
FreeFace
===========
*/
void FreeFace( face_t *f )
{
	if( f->winding )
		FreeWinding( f->winding );
	qfree( f );
}

/*
==================
NewFaceFromFace

Duplicates the non point information of a face,
used by SplitFace and MergeFace.
==================
*/
face_t *NewFaceFromFace( face_t *in )
{
	face_t	*newf;

	newf = AllocFace ();
	newf->planenum = in->planenum;
	newf->texturenum = in->texturenum;
	newf->planeside = in->planeside;
	newf->original = in->original;
	newf->contents[0] = in->contents[0];
	newf->contents[1] = in->contents[1];

	return newf;
}

//===========================================================================

/*
==================
SplitFace
==================
*/
void SplitFace( face_t *in, plane_t *split, face_t **front, face_t **back )
{
	winding_t	*frontw, *backw;

	if( !in->winding )
		Error( "SplitFace: freed face" );

	DivideWindingEpsilon( in->winding, split, &frontw, &backw, ON_EPSILON );

	if( !backw ) {
		*front = in;
		*back = NULL;
		return;
	}
	if( !frontw ) {
		*front = NULL;
		*back = in;
		return;
	}

	if( frontw->numpoints < 3 || backw->numpoints < 3 )
		Error( "SplitFace: numpoints < 3" );

	*front = NewFaceFromFace( in );
	(*front)->winding = frontw;

	*back = NewFaceFromFace( in );
	(*back)->winding = backw;

	// free the original face now that is is represented by the fragments
	FreeFace( in );
}

/*
================
SubdivideFace

If the face is > subdivide_size in either texture direction,
carve a valid sized piece off and insert the remainder in the next link
================
*/
int SubdivideFace( face_t *f, face_t **prevptr )
{
	int			i, axis;
	int			subdivides;
	vec_t		mins, maxs, v;
	plane_t		plane;
	winding_t	*w;
	face_t		*front, *back, *next;
	texinfo_t	*tex = &texinfo[f->texturenum];

	// special (non-surface cached) faces don't need subdivision
	if( tex->flags & TEX_SPECIAL )
		return 0;

	subdivides = 0;
	for( axis = 0; axis < 2 ; axis++ ) {
		while( 1 ) {
			mins = BOGUS_RANGE;
			maxs = -BOGUS_RANGE;

			w = f->winding;
			for( i = 0; i < w->numpoints; i++ ) {
				v = DotProduct( w->points[i], tex->vecs[axis] );
				if( v < mins )
					mins = v;
				if( v > maxs )
					maxs = v;
			}

			if( maxs - mins <= subdivide_size )
				break;

			// split it
			VectorCopy( tex->vecs[axis], plane.normal );
			v = VectorNormalize( plane.normal );
			if( !v )
				Error( "SubdivideFace: zero length normal" );
			plane.dist = (mins + subdivide_size - 16) / v;

			next = f->next;
			SplitFace( f, &plane, &front, &back );

			if( !front || !back )
				Error( "SubdivideFace: didn't split the polygon" );

			*prevptr = back;
			back->next = front;
			front->next = next;
			f = back;
			subdivides++;
		}
	}

	return subdivides;
}

//===========================================================================

/*
=============
TryMergeFaces

Returns NULL if the faces couldn't be merged, or the new face.
The originals will NOT be freed.
=============
*/
static face_t *TryMergeFaces( face_t *f1, face_t *f2 )
{
	face_t		*newf;
	vec3_t		planenormal;
	plane_t		*plane;
	winding_t	*neww;

	if( !f1->winding || !f2->winding ||
		f1->planeside != f2->planeside ||
		f1->texturenum != f2->texturenum ||
		f1->contents[0] != f2->contents[0] ||
		f1->contents[1] != f2->contents[1] )
		return NULL;

	plane = &mapplanes[f1->planenum];
	VectorCopy( plane->normal, planenormal );
	if( f1->planeside )
		VectorNegate( planenormal, planenormal );

	neww = TryMergeWinding( f1->winding, f2->winding, planenormal );
	if( !neww )
		return NULL;

	newf = NewFaceFromFace( f1 );
	newf->winding = neww;

	return newf;
}

/*
===============
MergeFaceToList_r
===============
*/
face_t *MergeFaceToList_r( face_t *face, face_t *list )
{
	face_t	*newf, *f;

	for( f = list; f; f = f->next ) {
		newf = TryMergeFaces( face, f );
		if( !newf )
			continue;

		FreeFace( face );
		FreeWinding( f->winding );
		f->winding = NULL;		// merged out

		return MergeFaceToList_r( newf, list );
    }

	// didn't merge, so add at start
	face->next = list;
	return face;
}

/*
===============
FreeMergeListScraps
===============
*/
face_t *FreeMergeListScraps( face_t *merged )
{
	face_t	*head, *next;

	for( head = NULL; merged ; merged = next) {
		next = merged->next;

		if( !merged->winding ) {
			FreeFace( merged );
			continue;
		}

		merged->next = head;
		head = merged;
    }

	return head;
}

/*
===============
MergePlaneFaces
===============
*/
static int MergePlaneFaces( surface_t *plane )
{
	int		count;
	face_t	*f1, *next;
	face_t	*merged;

	merged = NULL;
	for( f1 = plane->faces; f1; f1 = next ) {
		next = f1->next;
		merged = MergeFaceToList_r( f1, merged );
    }

	// chain all of the non-empty faces to the plane
	plane->faces = FreeMergeListScraps( merged );

	for( f1 = plane->faces, count = 0; f1; f1 = f1->next, count++ );

	return count;
}

/*
============
MergeTreeFaces
============
*/
void MergeTreeFaces( tree_t *tree )
{
	surface_t	*surf;
	int			mergefaces;

	printf( "---- MergeTreeFaces ----\n" );

	mergefaces = 0;
	for( surf = tree->surfaces; surf; surf = surf->next )
		mergefaces += MergePlaneFaces( surf );

	printf( "%i mergefaces\n", mergefaces );
}

//===========================================================================

/*
================
GatherNodeFaces_r
================
*/
static void GatherNodeFaces_r( node_t *node, face_t **validfaces )
{
	face_t	*f, *next;

	if( node->planenum != PLANENUM_LEAF ) {		// decision node
		for( f = node->faces; f; f = next ) {
			next = f->next;

			if( !f->winding ) {	// face was removed outside
				FreeFace( f );
				continue;
			}

			f->next = validfaces[f->planenum];
			validfaces[f->planenum] = f;
		}

		GatherNodeFaces_r( node->children[0], validfaces );
		GatherNodeFaces_r( node->children[1], validfaces );
    }

	FreeNode( node );
}

/*
================
GatherTreeFaces

Frees the current node tree and returns a new chain of
the surfaces that have inside faces.
================
*/
void GatherTreeFaces( tree_t *tree )
{
	memset( tree->validfaces, 0, sizeof( tree->validfaces ) );

	GatherNodeFaces_r( tree->headnode, tree->validfaces );

	BuildSurfaces( tree );
}

//===========================================================================

typedef struct hashvert_s
{
	vec3_t		point;
	int			num;
	int			numplanes;		// for corner determination
	int			planenums[2];
	int			numedges;
	struct hashvert_s *next;
} hashvert_t;

static	hashvert_t	hvertex[MAX_MAP_VERTS];
static	hashvert_t	*hvert_p;

static	hashvert_t	*hashverts[NUM_HASH];

static	face_t		*edgefaces[MAX_MAP_EDGES][2];
static	int			firstmodeledge;

static	vec3_t		hash_min, hash_scale;

static void InitHash( vec3_t mins, vec3_t maxs )
{
	int		i;
	vec3_t	size;
	vec_t	scale;
	int		newsize[2];

	memset( hashverts, 0, sizeof( hashverts ) );

	for( i = 0; i < 3; i++ ) {
		hash_min[i] = mins[i];
		size[i] = maxs[i] - mins[i];
    }

	scale = sqrt( size[0] * size[1] / NUM_HASH );

	newsize[0] = size[0] / scale;
	newsize[1] = size[1] / scale;

	hash_scale[0] = newsize[0] / size[0];
	hash_scale[1] = newsize[1] / size[1];
	hash_scale[2] = newsize[1];

	hvert_p = hvertex;
}

static unsigned HashVec( vec3_t vec )
{
	unsigned	h;

	h =	hash_scale[0] * (vec[0] - hash_min[0]) * hash_scale[2] + hash_scale[1] * (vec[1] - hash_min[1]);
	return h % NUM_HASH;
}

//============================================================================

/*
=============
GetVertex
=============
*/
static int GetVertex( vec3_t in, int planenum )
{
	int			i;
	int			h;
	hashvert_t	*hv;
	vec3_t		vert;

	for( i = 0; i < 3; i++ ) {
		h = Q_rint( in[i] );

		if( fabs( in[i] - h ) < 0.001 )
			vert[i] = h;
		else
			vert[i] = in[i];
    }

	h = HashVec( vert );
	for( hv = hashverts[h]; hv; hv = hv->next ) {
		if( fabs( hv->point[0] - vert[0] ) < POINT_EPSILON
			&& fabs( hv->point[1] - vert[1] ) < POINT_EPSILON
			&& fabs( hv->point[2] - vert[2] ) < POINT_EPSILON ) {
			hv->numedges++;

			if( hv->numplanes == 3 )
				return hv->num;			// allready known to be a corner

			for( i = 0; i < hv->numplanes; i++ ) {
				if( hv->planenums[i] == planenum )
					return hv->num;		// allready know this plane
			}

			if( hv->numplanes != 2 )
				hv->planenums[hv->numplanes] = planenum;
			hv->numplanes++;
			return hv->num;
		}
    }

	hv = hvert_p;
	hv->numedges = 1;
	hv->numplanes = 1;
	hv->planenums[0] = planenum;
	hv->next = hashverts[h];
	hv->num = numvertexes;
	VectorCopy( vert, hv->point );
	hashverts[h] = hv;
	hvert_p++;

	EmitVertex( vert );

	return hv->num;
}

/*
==================
EmitFaceEdge

Don't allow four way edges
==================
*/
static int EmitFaceEdge( vec3_t p1, vec3_t p2, face_t *f )
{
	int		i;
	int		v1, v2;
	dedge_t	*edge;

	v1 = GetVertex( p1, f->planenum );
	v2 = GetVertex( p2, f->planenum );
	for( i = firstmodeledge; i < numedges; i++ ) {
		edge = &dedges[i];

		if( v1 == edge->v[1] && v2 == edge->v[0] && !edgefaces[i][1]
			&& edgefaces[i][0]->contents[0] == f->contents[0] ) {
			edgefaces[i][1] = f;
			return -i;
		}
    }

	edgefaces[i][0] = f;

	EmitEdge( v1, v2 );

	return i;
}

/*
==============
EmitNodeFaces_r
==============
*/
static void EmitNodeFaces_r( node_t *node )
{
	int			i;
	dface_t		*f;
	face_t		*face;
	winding_t	*w;

	if( node->planenum == PLANENUM_LEAF )
		return;

	node->firstface = numfaces;
	for( face = node->faces; face; face = face->next ) {
		if( numfaces == MAX_MAP_FACES )
			Error( "numfaces == MAX_MAP_FACES" );

		f = &dfaces[numfaces];	// emit a face
		f->planenum = node->outputplanenum;
		f->side = face->planeside;
		f->texinfo = face->texturenum;
		f->lightofs = -1;
		for( i = 0; i < MAXLIGHTMAPS; i++ )
			f->styles[i] = 255;

		// add the face and mergable neighbors to it
		f->firstedge = numsurfedges;
		for( i = 0, w = face->winding; i < w->numpoints; i++ ) {
			if( numsurfedges == MAX_MAP_SURFEDGES )
				Error( "numsurfedges == MAX_MAP_SURFEDGES" );
			dsurfedges[numsurfedges++] = EmitFaceEdge( w->points[i], w->points[(i+1) % w->numpoints], face );
		}
		f->numedges = numsurfedges - f->firstedge;

		face->outputnumber = numfaces++;
	}

	node->numfaces = numfaces - node->firstface;

	EmitNodeFaces_r( node->children[0] );
	EmitNodeFaces_r( node->children[1] );
}

/*
================
EmitNodeFaces
================
*/
void EmitNodeFaces( node_t *headnode )
{
//	vec_t	radius;
//	vec3_t	maxs, mins;

//	qprintf( "--- EmitNodeFaces ---\n" );

	// origin points won't allways be inside the map, so extend the hash area
//	radius = RadiusFromBounds( headnode->mins, headnode->maxs );
//	VectorSet( maxs, radius, radius, radius );
//	VectorSet( mins, -radius, -radius, -radius );

	InitHash( headnode->mins, headnode->maxs );

	firstmodeledge = numedges;
	EmitNodeFaces_r( headnode );
}
