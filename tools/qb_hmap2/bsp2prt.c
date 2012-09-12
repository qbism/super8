#include "bsp5.h"

/*
==================
Bsp2Prt_BuildTree_r
==================
*/
static node_t *Bsp2Prt_BuildTree_r( int nodenum )
{
	node_t		*n;

	if( nodenum < 0 ) {
		dleaf_t *leaf = &dleafs[-1-nodenum];

		n = AllocNode ();
		n->planenum = PLANENUM_LEAF;
		n->contents = leaf->contents;
	} else {
		int		side;
		plane_t plane;
		dnode_t *node = &dnodes[nodenum];

		n = AllocNode ();
		plane.dist = dplanes[node->planenum].dist;
		VectorCopy( dplanes[node->planenum].normal, plane.normal );
		n->planenum = FindPlane( &plane, &side );

		if( side )
			printf( "bsp2prt: Bad node plane (non-optimized)\n" );

		n->children[0] = Bsp2Prt_BuildTree_r( node->children[0] );
		n->children[1] = Bsp2Prt_BuildTree_r( node->children[1] );
	}

	return n;
}

/*
==================
Bsp2Prt_GetWorldBounds
==================
*/
static void Bsp2Prt_GetWorldBounds( vec3_t mins, vec3_t maxs )
{
	int			i, j, e;
	dface_t		*face;
	dvertex_t	*v;
	vec3_t		point;

	ClearBounds( mins, maxs );

	for( i = 0, face = dfaces; i < dmodels[0].numfaces; i++, face++ ) {
		for( j = 0; j < face->numedges; j++ ) {
			e = dsurfedges[face->firstedge + j];
			if( e >= 0 )
				v = dvertexes + dedges[e].v[0];
			else
				v = dvertexes + dedges[-e].v[1];

			VectorCopy( v->point, point );
			AddPointToBounds( point, mins, maxs );
		}
	}
}

/*
==================
Bsp2Prt_ProcessFile
==================
*/
static void Bsp2Prt_ProcessFile( void )
{
	tree_t		*tree;

	LoadBSPFile( filename_bsp );

	tree = AllocTree ();

	Bsp2Prt_GetWorldBounds( tree->mins, tree->maxs );

	tree->headnode = Bsp2Prt_BuildTree_r( 0 );

	PortalizeTree( tree );
	WritePortalfile( tree );
	FreeTreePortals( tree );

	FreeTree( tree );
}

/*
==================
Bsp2Prt_Main
==================
*/
int Bsp2Prt_Main( int argc, char **argv )
{
	int			i;
	double		start, end;

	transwater = true;

	for( i = 1; i < argc; i++ ) {
		if (!strcmp (argv[i],"-nowater"))
			transwater = false;
		else if( argv[i][0] == '-' )
			Error( "Unknown option \"%s\"", argv[i] );
		else
			break;
	}

	if( i != argc - 1 )
		Error( "%s",
"usage: hmap2 -bsp2prt [options] sourcefile\n"
"Makes a .prt file from a .bsp, to allow it to be vised\n"
"\n"
"What the options do:\n"
"-nowater    disable watervis; r_wateralpha in glquake will not work right\n"
		);

	// init memory
	Q_InitMem ();

	// do it!
	start = I_DoubleTime ();

	end = I_DoubleTime ();
	Bsp2Prt_ProcessFile ();
	printf( "%5.2f seconds elapsed\n\n", end - start );

	// print memory stats
	Q_PrintMem ();

	// free allocated memory
	Q_ShutdownMem ();

	return 0;
}
