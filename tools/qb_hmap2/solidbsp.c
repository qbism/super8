// solidbsp.c

#include "bsp5.h"

int		leaffaces;
int		c_nodefaces;
int		c_subdivides;
int		splitnodes;

int		c_solid, c_empty, c_water, c_slime, c_lava, c_sky;

qboolean		usemidsplit;

//============================================================================

/*
===========
AllocNode
===========
*/
node_t *AllocNode( void )
{
	node_t	*n;

	n = qmalloc( sizeof( node_t ) );
	memset( n, 0, sizeof( node_t ) );

	return n;
}

/*
===========
FreeNode
===========
*/
void FreeNode( node_t *n ) {
	qfree( n );
}

//============================================================================

/*
==================
ChooseMidPlaneFromList

The clipping hull BSP doesn't worry about avoiding splits
==================
*/
surface_t *ChooseMidPlaneFromList( surface_t *surfaces, vec3_t mins, vec3_t maxs )
{
	int			j, l;
	surface_t	*p, *bestsurface;
	vec_t		bestvalue, value, dist;
	plane_t		*plane;
	
	// pick the plane that splits the least
	bestvalue = 8*(double)BOGUS_RANGE*(double)BOGUS_RANGE;
	bestsurface = NULL;

	for( p = surfaces; p; p = p->next ) {
		if( p->onnode )
			continue;

		plane = &mapplanes[p->planenum];

		// check for axis aligned surfaces
		l = plane->type;
		if( l > PLANE_Z )
			continue;

		// calculate the split metric along axis l, smaller values are better
		value = 0;
		dist = plane->dist;

		for( j = 0; j < 3; j++ ) {
			if( j == l ) {
				value += (maxs[l] - dist) * (maxs[l] - dist);
				value += (dist - mins[l]) * (dist - mins[l]);
				continue;
			}
			value += 2 * (maxs[j] - mins[j]) * (maxs[j] - mins[j]);
		}

		if( value < bestvalue ) { // currently the best!
			bestvalue = value;
			bestsurface = p;
		}
    }

	if( !bestsurface ) {
		for( p = surfaces; p; p = p->next ) {
			if( !p->onnode )
				return p;		// first valid surface
		}
		Error( "ChooseMidPlaneFromList: no valid planes" );
    }

	return bestsurface;
}

/*
==================
ChoosePlaneFromList

The real BSP hueristic
==================
*/
surface_t *ChoosePlaneFromList( surface_t *surfaces, vec3_t mins, vec3_t maxs )
{
	int			j, k, l;
	surface_t	*p, *p2, *bestsurface;
	vec_t		bestvalue, bestdistribution, value, dist;
	plane_t		*plane;
	face_t		*f;

	// pick the plane that splits the least
	bestvalue = 8*(double)BOGUS_RANGE*(double)BOGUS_RANGE;
	bestsurface = NULL;
	bestdistribution = 9e30;

	for( p = surfaces; p; p = p->next ) {
		if( p->onnode )
			continue;

		k = 0;
		plane = &mapplanes[p->planenum];

		for( p2 = surfaces; p2; p2 = p2->next ) {
			if( p2 == p || p2->onnode )
				continue;

			for( f = p2->faces; f; f = f->next ) {
				if( WindingOnPlaneSide( f->winding, plane ) == SIDE_CROSS ) {
					if( ++k >= bestvalue )
						break;
				}
			}
			if( k > bestvalue )
				break;
		}

		if( k > bestvalue )
			continue;

		// if equal numbers, axial planes win, then decide on spatial subdivision
		if( k < bestvalue || (k == bestvalue && plane->type < PLANE_ANYX) ) {
			// check for axis aligned surfaces
			l = plane->type;

			if( l <= PLANE_Z ) {	// axial aligned
				// calculate the split metric along axis l
				value = 0;
				dist = plane->dist;

				for( j = 0; j < 3; j++ ) {
					if( j == l ) {
						value += (maxs[l] - dist) * (maxs[l] - dist);
						value += (dist - mins[l]) * (dist - mins[l]);
						continue;
					}
					value += 2 * (maxs[j] - mins[j]) * (maxs[j] - mins[j]);
				}

				if( value > bestdistribution && k == bestvalue )
					continue;
				bestdistribution = value;
			}

			// currently the best!
			bestvalue = k;
			bestsurface = p;
		}
	}

	return bestsurface;
}


/*
==================
SelectPartition

Selects a surface from a linked list of surfaces to split the group on
returns NULL if the surface list can not be divided any more (a leaf)
==================
*/
surface_t *SelectPartition( surface_t *surfaces )
{
	int			i;
	vec3_t		mins, maxs;
	surface_t	*p, *bestsurface;

	// calculate a bounding box of the entire surfaceset
	ClearBounds( mins, maxs );

	// count onnode surfaces
	i = 0;
	bestsurface = NULL;
	for( p = surfaces; p; p = p->next ) {
		AddPointToBounds( p->mins, mins, maxs );
		AddPointToBounds( p->maxs, mins, maxs );

		if( !p->onnode ) {
			i++;
			bestsurface = p;
		}
	}

	if( !i )
		return NULL;
	else if( i == 1 )
		return bestsurface;	// this is a final split

	if( usemidsplit ) // do fast way for clipping hull
		return ChooseMidPlaneFromList( surfaces, mins, maxs );

	// do slow way to save poly splits for drawing hull
	return ChoosePlaneFromList( surfaces, mins, maxs );
}

//============================================================================

/*
=================
CalcSurfaceInfo

Calculates the bounding box
=================
*/
void CalcSurfaceInfo( surface_t *surf )
{
	int			i;
	face_t		*f;
	winding_t	*w;

	if( !surf->faces )
		Error( "CalcSurfaceInfo: surface without a face" );

	// calculate a bounding box
	ClearBounds( surf->mins, surf->maxs );

	for( f = surf->faces; f; f = f->next ) {
		if( f->contents[0] >= 0 || f->contents[1] >= 0 )
			Error( "Bad contents" );

		for( i = 0, w = f->winding; i < w->numpoints; i++ )
			AddPointToBounds( w->points[i], surf->mins, surf->maxs );
	}
}

/*
==================
DividePlane
==================
*/
void DividePlane( surface_t *in, plane_t *split, surface_t **front, surface_t **back )
{
	face_t		*facet, *next;
	face_t		*frontlist, *backlist;
	face_t		*frontfrag, *backfrag;
	surface_t	*news;
	plane_t		*inplane;

	inplane = &mapplanes[in->planenum];

	// parallel case is easy
	if( VectorCompare( inplane->normal, split->normal ) ) {
		// check for exactly on node
		if( inplane->dist == split->dist ) {	// divide the facets to the front and back sides
			news = AllocSurface ();
			*news = *in;

			facet = in->faces;
			in->faces = NULL;
			news->faces = NULL;
			in->onnode = news->onnode = true;

			for( ; facet; facet = next ) {
				next = facet->next;

				if( facet->planeside ) {
					facet->next = news->faces;
					news->faces = facet;
				} else {
					facet->next = in->faces;
					in->faces = facet;
				}
			}

			if( in->faces )
				*front = in;
			else
				*front = NULL;
			if( news->faces )
				*back = news;
			else
				*back = NULL;
			return;
		}

		if( inplane->dist > split->dist ) {
			*front = in;
			*back = NULL;
		} else {
			*front = NULL;
			*back = in;
		}
		return;
	}

	// do a real split.  may still end up entirely on one side
	// OPTIMIZE: use bounding box for fast test
	frontlist = NULL;
	backlist = NULL;

	for( facet = in->faces; facet; facet = next ) {
		next = facet->next;
		SplitFace( facet, split, &frontfrag, &backfrag );

		if( frontfrag ) {
			frontfrag->next = frontlist;
			frontlist = frontfrag;
		}
		if( backfrag ) {
			backfrag->next = backlist;
			backlist = backfrag;
		}
	}

	// if nothing actually got split, just move the in plane
	if( frontlist == NULL ) {
		*front = NULL;
		*back = in;
		in->faces = backlist;
		return;
	}

	if( backlist == NULL ) {
		*front = in;
		*back = NULL;
		in->faces = frontlist;
		return;
	}

	// stuff got split, so allocate one new plane and reuse in
	news = AllocSurface ();
	*news = *in;
	news->faces = backlist;
	*back = news;

	in->faces = frontlist;
	*front = in;

	// recalc bboxes and flags
	CalcSurfaceInfo( news );
	CalcSurfaceInfo( in );
}

/*
==================
LinkConvexFaces

Determines the contents of the leaf and creates the final list of
original faces that have some fragment inside this leaf
==================
*/
void LinkConvexFaces( surface_t *planelist, node_t *leafnode )
{
	int			i, count;
	face_t		*f, *next;
	surface_t	*surf, *pnext;

	leafnode->faces = NULL;
	leafnode->contents = 0;
	leafnode->planenum = -1;

	count = 0;
	for( surf = planelist; surf; surf = surf->next ) {
		for( f = surf->faces; f; f = f->next ) {
			count++;
			if( !leafnode->contents )
				leafnode->contents = f->contents[0];
			else if( leafnode->contents != f->contents[0] )
				Error ("LinkConvexFaces: Mixed face contents in leafnode");
		}
	}

	if( !leafnode->contents )
		leafnode->contents = CONTENTS_SOLID;

	switch( leafnode->contents ) {
		case CONTENTS_EMPTY:
			c_empty++;
			break;
		case CONTENTS_SOLID:
			c_solid++;
			break;
		case CONTENTS_WATER:
			c_water++;
			break;
		case CONTENTS_SLIME:
			c_slime++;
			break;
		case CONTENTS_LAVA:
			c_lava++;
			break;
		case CONTENTS_SKY:
			c_sky++;
			break;
		default:
			Error ("LinkConvexFaces: bad contents number");
	}

	// write the list of faces, and free the originals
	leaffaces += count;
	leafnode->markfaces = qmalloc( sizeof(face_t *) * (count + 1) );
	for( i = 0, surf = planelist; surf; surf = pnext ) {
		pnext = surf->next;
		for( f = surf->faces; f; f = next, i++ ) {
			next = f->next;
			leafnode->markfaces[i] = f->original;
			FreeFace( f );
		}
		FreeSurface( surf );
	}
	leafnode->markfaces[i] = NULL;	// sentinel
}

/*
==================
LinkNodeFaces

Returns a duplicated list of all faces on surface
==================
*/
face_t *LinkNodeFaces( surface_t *surface )
{
	face_t	*f, *list, *newf, **prevptr;

	list = NULL;

	// subdivide
	prevptr = &surface->faces;
	while( 1 ) {
		f = *prevptr;
		if( !f )
			break;
		c_subdivides += SubdivideFace( f, prevptr );
		f = *prevptr;
		prevptr = &f->next;
    }

	// copy
	for( f = surface->faces; f; f = f->next, c_nodefaces++ ) {
		newf = NewFaceFromFace( f );
		newf->winding = CopyWinding( f->winding );
		f->original = newf;
		newf->next = list;
		list = newf;
    }

	return list;
}

/*
==================
PartitionSurfaces
==================
*/
void PartitionSurfaces( surface_t *surfaces, node_t *node )
{
	surface_t	*split, *p, *next;
	surface_t	*frontlist, *backlist;
	surface_t	*frontfrag, *backfrag;
	plane_t		*splitplane;
	
	split = SelectPartition (surfaces);
	if( !split )  {	// this is a leaf node
		node->planenum = PLANENUM_LEAF;
		LinkConvexFaces( surfaces, node );
		return;
    }

	splitnodes++;
	node->faces = LinkNodeFaces( split );
	node->children[0] = AllocNode ();
	node->children[1] = AllocNode ();
	node->planenum = split->planenum;

	splitplane = &mapplanes[split->planenum];

	// multiple surfaces, so split all the polysurfaces into front and back lists
	frontlist = NULL;
	backlist = NULL;

	for( p = surfaces; p; p = next ) {
		next = p->next;
		DividePlane( p, splitplane, &frontfrag, &backfrag );

		if( frontfrag ) {
			if( !frontfrag->faces )
				Error( "surface with no faces" );
			frontfrag->next = frontlist;
			frontlist = frontfrag;
		}
		if( backfrag ) {
			if( !backfrag->faces )
				Error ("surface with no faces");
			backfrag->next = backlist;
			backlist = backfrag;
		}
    }

	PartitionSurfaces( frontlist, node->children[0] );
	PartitionSurfaces( backlist, node->children[1] );
}

/*
==================
SolidBSP
==================
*/
void SolidBSP( tree_t *tree, qboolean midsplit )
{
	int		i;
	node_t	*headnode;

	qprintf( "----- SolidBSP -----\n" );

	headnode = AllocNode ();
	usemidsplit = midsplit;

	// calculate a bounding box for the entire model
	for( i = 0; i < 3; i++ ) {
		headnode->mins[i] = tree->mins[i] - SIDESPACE;
		headnode->maxs[i] = tree->maxs[i] + SIDESPACE;
    }

	// recursively partition everything
	splitnodes = 0;
	leaffaces = 0;
	c_nodefaces = 0;
	c_solid = c_empty = c_water = c_slime = c_lava = c_sky = 0;
	c_subdivides = 0;

	PartitionSurfaces( tree->surfaces, tree->headnode = headnode );

	qprintf( "%5i split nodes\n", splitnodes );
	qprintf( "%5i solid leafs\n", c_solid );
	qprintf( "%5i empty leafs\n", c_empty );
	qprintf( "%5i water leafs\n", c_water );
	qprintf( "%5i slime leafs\n", c_slime );
	qprintf( "%5i lava  leafs\n", c_lava );
	qprintf( "%5i sky   leafs\n", c_sky );
	qprintf( "%5i leaffaces\n",leaffaces );
	qprintf( "%5i nodefaces\n", c_nodefaces );
	qprintf( "%5i subdivides\n", c_subdivides );
}
