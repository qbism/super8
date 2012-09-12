// csg4.c

#include "bsp5.h"

/*

NOTES
-----
Brushes that touch still need to be split at the cut point to make a tjunction

*/

static	face_t	*inside, *outside;
static	int		numcsgbrushfaces;
static	int		numcsgfaces;
static	int		numcsgmergefaces;

extern qboolean func_water; //qb:  pOX

/*
=================
ClipInside

Clips all of the faces in the inside list, possibly moving them to the
outside list or spliting it into a piece in each list.

Faces exactly on the plane will stay inside unless overdrawn by later brush

frontside is the side of the plane that holds the outside list
=================
*/
static void ClipInside( int splitplane, int frontside, qboolean precedence )
{
	face_t	*f, *next;
	face_t	*frags[2];
	face_t	*insidelist;
	plane_t *split = &mapplanes[splitplane];

	insidelist = NULL;
	for( f = inside; f; f = next ) {
		next = f->next;

		if( f->planenum == splitplane ) {
			// exactly on, handle special
			if( frontside != f->planeside || precedence ) {
				// allways clip off opposite faceing
				frags[frontside] = NULL;
				frags[!frontside] = f;
			} else {
				// leave it on the outside
				frags[frontside] = f;
				frags[!frontside] = NULL;
			}
		} else {
			// proper split
			SplitFace( f, split, &frags[0], &frags[1] );
		}

		if( frags[frontside] ) {
			frags[frontside]->next = outside;
			outside = frags[frontside];
		}
		if( frags[!frontside] ) {
			frags[!frontside]->next = insidelist;
			insidelist = frags[!frontside];
		}
	}

	inside = insidelist;
}


/*
==================
SaveOutside

Saves all of the faces in the outside list to the bsp plane list
==================
*/
static void SaveOutside( tree_t *tree, qboolean mirror )
{
	int		planenum;
	face_t	*f, *next, *newf;

	for( f = outside; f; f = next ) {
		next = f->next;
		numcsgfaces++;
		planenum = f->planenum;

		if( mirror ) {
			newf = NewFaceFromFace( f );
			newf->planeside = f->planeside ^ 1;	// reverse side
			newf->contents[0] = f->contents[1];
			newf->contents[1] = f->contents[0];
			newf->winding = ReverseWinding( f->winding );
			tree->validfaces[planenum] = MergeFaceToList_r( newf, tree->validfaces[planenum] );
		}

		tree->validfaces[planenum] = MergeFaceToList_r( f, tree->validfaces[planenum]);
		tree->validfaces[planenum] = FreeMergeListScraps( tree->validfaces[planenum] );
	}
}

/*
==================
FreeInside

Free all the faces that got clipped out
==================
*/
static void FreeInside( int contents )
{
	face_t	*f, *next;

	for( f = inside; f; f = next ) {
		next = f->next;

		if( contents == CONTENTS_SOLID ) {
			FreeFace (f);
			continue;
		}

		f->contents[0] = contents;
		f->next = outside;
		outside = f;
	}
}

//==========================================================================

/*
===========
AllocSurface
===========
*/
surface_t *AllocSurface( void )
{
	surface_t	*s;

	s = qmalloc( sizeof( surface_t ) );
	memset( s, 0, sizeof( surface_t ) );

	return s;
}

/*
===========
FreeSurface
===========
*/
void FreeSurface( surface_t *s ) {
	qfree( s );
}

//==========================================================================

/*
==================
BuildSurfaces

Returns a chain of all the external surfaces with one or more visible
faces.
==================
*/
void BuildSurfaces( tree_t *tree )
{
	int			i;
	face_t		**f;
	face_t		*count;
	surface_t	*s;
	surface_t	*surfhead;

	surfhead = NULL;
	for( i = 0, f = tree->validfaces; i < nummapplanes; i++, f++ ) {
		if( !*f )
			continue;	// nothing left on this plane

		// create a new surface to hold the faces on this plane
		s = AllocSurface ();
		s->planenum = i;
		s->next = surfhead;
		surfhead = s;
		s->faces = *f;

		for( count = s->faces; count; count = count->next )
			numcsgmergefaces++;

		CalcSurfaceInfo( s );	// bounding box and flags
	}

	tree->surfaces = surfhead;
}

//==========================================================================

/*
==================
CopyFacesToOutside
==================
*/
static void CopyFacesToOutside( const brush_t *b )
{
	face_t		*f, *newf;

	outside = NULL;
	for( f = b->faces; f; f = f->next ) {
		if( !f->winding )
			continue;

		numcsgbrushfaces++;
		newf = AllocFace ();
		newf->texturenum = f->texturenum;
		newf->planenum = f->planenum;
		newf->planeside = f->planeside;
		newf->original = f->original;
		newf->winding = CopyWinding( f->winding );
		newf->next = outside;
		newf->contents[0] = CONTENTS_EMPTY;
		newf->contents[1] = b->contents;
		outside = newf;
	}
}

/*
==================
CSGFaces

Builds a list of surfaces containing all of the faces
==================
*/
void CSGFaces( tree_t *tree )
{
	int			i;
	qboolean	overwrite;
	brush_t		*b1, *b2;
	face_t		*f;

	qprintf( "---- CSGFaces ----\n" );

	memset( tree->validfaces, 0, sizeof( tree->validfaces ) );

	numcsgfaces = numcsgbrushfaces = numcsgmergefaces = 0;

	// do the solid faces
	for( b1 = tree->brushes; b1; b1 = b1->next ) {
		// set outside to a copy of the brush's faces
		CopyFacesToOutside( b1 );

		overwrite = false;
		for( b2 = tree->brushes; b2; b2 = b2->next ) {
			// see if b2 needs to clip a chunk out of b1
			if( b1 == b2 ) 	{
				overwrite = true;	// later brushes now overwrite
				continue;
			}

			// check bounding box first
			for( i = 0; i < 3; i++ ) {
				if( b1->mins[i] > b2->maxs[i] || b1->maxs[i] < b2->mins[i] )
					break;
			}
			if( i < 3 )
				continue;

			// divide faces by the planes of the new brush
			inside = outside;
			outside = NULL;

			for( f = b2->faces; f; f = f->next )
				ClipInside( f->planenum, f->planeside, overwrite );

			// these faces are continued in another brush, so get rid of them
			if( b1->contents == CONTENTS_SOLID && b2->contents <= CONTENTS_WATER )
				FreeInside( b2->contents );
			else
				FreeInside( CONTENTS_SOLID );
		}

		// all of the faces left in outside are real surface faces
//		if( b1->contents != CONTENTS_SOLID )
	   		if (b1->contents != CONTENTS_SOLID || func_water)//qb;  pOx - mirror insides for func_water entities
			SaveOutside( tree, true );	// mirror faces for inside view
		else
			SaveOutside( tree, false );
	}

	BuildSurfaces( tree );

	qprintf( "%5i brushfaces\n", numcsgbrushfaces );
	qprintf( "%5i csgfaces\n", numcsgfaces );
	qprintf( "%5i mergedfaces\n", numcsgmergefaces );
}
