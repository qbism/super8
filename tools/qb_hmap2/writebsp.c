
#include "bsp5.h"

int		firstface;
int		*planemapping;

//===========================================================================

/*
==================
FindFinalPlane

Used to find plane index numbers for clip nodes read from child processes
==================
*/
int FindFinalPlane( vec3_t normal, vec_t dist, int type )
{
	int			i;
	dplane_t	*dplane;

	for( i = 0, dplane = dplanes; i < numplanes; i++, dplane++ ) {
		if( dplane->type != type
			|| dplane->dist != dist
			|| dplane->normal[0] != normal[0]
			|| dplane->normal[1] != normal[1]
			|| dplane->normal[2] != normal[2])
			continue;
		return i;
	}

	// new plane
	if( numplanes == MAX_MAP_PLANES )
		Error( "numplanes == MAX_MAP_PLANES" );

	dplane->type = type;
	dplane->dist = dist;
	VectorCopy( normal, dplane->normal );

	return numplanes++;
}

/*
==================
EmitNodePlanes
==================
*/
static void EmitNodePlanes_r( node_t *node )
{
	if( node->planenum == PLANENUM_LEAF )
		return;

	if( planemapping[node->planenum] == -1 ) {	// a new plane
		plane_t *plane;

		plane = &mapplanes[node->planenum];
		planemapping[node->planenum] = FindFinalPlane( plane->normal, plane->dist, plane->type );
	}

	node->outputplanenum = planemapping[node->planenum];

	EmitNodePlanes_r( node->children[0] );
	EmitNodePlanes_r( node->children[1] );
}

/*
==================
EmitNodePlanes
==================
*/
void EmitNodePlanes( node_t *nodes )
{
//	qprintf( "--- EmitNodePlanes ---\n" );

	EmitNodePlanes_r( nodes );
}

//===========================================================================

/*
==================
EmitClipNodes_r
==================
*/
static int EmitClipNodes_r( node_t *node )
{
	int			i, c;
	dclipnode_t	*cn;
	int			num;

	// FIXME: free more stuff?
	if( node->planenum == PLANENUM_LEAF ) {
		num = node->contents;
		FreeNode( node );
		return num;
	}

	c = numclipnodes;
	cn = &dclipnodes[numclipnodes++];	// emit a clipnode
	cn->planenum = node->outputplanenum;
	for( i = 0; i < 2; i++ )
		cn->children[i] = EmitClipNodes_r( node->children[i] );

	FreeNode( node );

	return c;
}

/*
==================
EmitClipNodes

Called after the clipping hull is completed. Generates a disk format
representation and frees the original memory.
==================
*/
void EmitClipNodes( node_t *nodes, int modnum, int hullnum )
{
//	qprintf( "--- EmitClipNodes ---\n" );

	dmodels[modnum].headnode[hullnum] = numclipnodes;
	EmitClipNodes_r( nodes );
}

//===========================================================================

/*
==================
EmitVertex
==================
*/
void EmitVertex( vec3_t point )
{
	dvertex_t	*vert;

	if( numvertexes == MAX_MAP_VERTS )
		Error( "numvertexes == MAX_MAP_VERTS" );

	vert = &dvertexes[numvertexes++];	// emit a vertex
	VectorCopy( point, vert->point );
}

/*
==================
EmitEdge
==================
*/
void EmitEdge( int v1, int v2 )
{
	dedge_t		*edge;

	if( numedges == MAX_MAP_EDGES )
		Error( "numedges == MAX_MAP_EDGES" );

	edge = &dedges[numedges++];	// emit an edge
	edge->v[0] = v1;
	edge->v[1] = v2;
}

//===========================================================================

/*
==================
EmitLeaf
==================
*/
static void EmitLeaf( node_t *node )
{
	face_t		**fp, *f;
	dleaf_t		*leaf_p;

	if( numleafs == MAX_MAP_LEAFS )
		Error( "numleafs == MAX_MAP_LEAFS" );

	leaf_p = &dleafs[numleafs++];	// emit a leaf
	leaf_p->contents = node->contents;
	leaf_p->visofs = -1;			// no vis info yet

	// write bounding box info
	VectorCopy( node->mins, leaf_p->mins );
	VectorCopy( node->maxs, leaf_p->maxs );

	// write the marksurfaces
	leaf_p->firstmarksurface = nummarksurfaces;

	for( fp = node->markfaces; *fp ; fp++ ) {
		for( f = *fp; f; f = f->original ) {	// grab tjunction split faces
			if( nummarksurfaces == MAX_MAP_MARKSURFACES )
				Error( "nummarksurfaces == MAX_MAP_MARKSURFACES" );
			if( f->outputnumber == -1 )
				Error( "f->outputnumber == -1" );
			dmarksurfaces[nummarksurfaces++] = f->outputnumber;		// emit a marksurface
		}
	}

	leaf_p->nummarksurfaces = nummarksurfaces - leaf_p->firstmarksurface;
}

/*
==================
EmitDrawNodes_r
==================
*/
void EmitDrawNodes_r( node_t *node )
{
	int			i;
	dnode_t		*n;

	if( numnodes == MAX_MAP_NODES )
		Error( "numnodes == MAX_MAP_NODES" );

	n = &dnodes[numnodes++];		// emit a node
	n->planenum = node->outputplanenum;
	n->firstface = node->firstface;
	n->numfaces = node->numfaces;
	VectorCopy( node->mins, n->mins );
	VectorCopy( node->maxs, n->maxs );

	// recursively output the other nodes
	for( i = 0; i < 2; i++ ) {
		if( node->children[i]->planenum == PLANENUM_LEAF ) {
			n->children[i] = -1;

			if( node->children[i]->contents != CONTENTS_SOLID ) {
				n->children[i] = -(numleafs + 1);
				EmitLeaf( node->children[i] );
			}
		} else {
			n->children[i] = numnodes;
			EmitDrawNodes_r( node->children[i] );
		}
	}
}

/*
==================
EmitDrawNodes
==================
*/
void EmitDrawNodes( node_t *headnode )
{
	int			i;
	dmodel_t	*bm;
	int			firstleaf;

//	qprintf( "--- EmitDrawNodes ---\n" );

	if( nummodels == MAX_MAP_MODELS )
		Error( "nummodels == MAX_MAP_MODELS" );

	bm = &dmodels[nummodels++];		// emit a model
	bm->headnode[0] = numnodes;
	bm->firstface = firstface;
	bm->numfaces = numfaces - firstface;
	firstface = numfaces;

	firstleaf = numleafs;
	if( headnode->contents < 0 )
		EmitLeaf( headnode );
	else
		EmitDrawNodes_r( headnode );
	bm->visleafs = numleafs - firstleaf;

	for( i = 0; i < 3; i++ ) {
		bm->mins[i] = headnode->mins[i] + SIDESPACE + 1;	// remove the padding
		bm->maxs[i] = headnode->maxs[i] - SIDESPACE - 1;
	}
}

//=============================================================================

/*
==================
BeginBSPFile
==================
*/
void BeginBSPFile( void )
{
	qprintf( "--- BeginBSPFile ---\n" );

	planemapping = qmalloc( sizeof( *planemapping ) * MAX_MAP_PLANES );
	memset( planemapping, -1, sizeof( *planemapping ) * MAX_MAP_PLANES );

	// edge 0 is not used, because 0 can't be negated
	numedges = 1;

	// leaf 0 is common solid with no faces
	numleafs = 1;
	dleafs[0].contents = CONTENTS_SOLID;
	dleafs[0].visofs = -1; // thanks to Vic for suggesting this line

	firstface = 0;
}

/*
==================
FinishBSPFile
==================
*/
void FinishBSPFile( void )
{
	printf( "--- BSP file information ---\n" );
	PrintBSPFileSizes ();

	qprintf( "--- WriteBSPFile: %s ---\n", filename_bsp );
	WriteBSPFile( filename_bsp, false );

	qfree( planemapping );
}
