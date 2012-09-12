#include "bsp5.h"

//============================================================================

qboolean func_water; //qb: pOX

/*
===========
AllocTree
===========
*/
tree_t *AllocTree( void )
{
    tree_t	*t;

    t = qmalloc( sizeof( tree_t ) );
    memset( t, 0, sizeof( tree_t ) );
    ClearBounds( t->mins, t->maxs );

    return t;
}

/*
===========
FreeTree
===========
*/
void FreeTree( tree_t *t )
{
    brush_t		*b, *next;

    for( b = t->brushes; b; b = next )
    {
        next = b->next;
        FreeBrush( b );
    }
    qfree( t );
}

//============================================================================

/*
===============
Tree_ProcessEntity
===============
*/
tree_t *Tree_ProcessEntity( entity_t *ent, int modnum, int hullnum )
{
    tree_t		*tree;
    qboolean	worldmodel;

    func_water = false; //qb: pOX
    if (!strcmp (ValueForKey(ent, "classname"), "func_water"))
    {
        if (!FloatForKey(ent, "_nomirror"))// allow it to be turned off selectively
            func_water = true;
             worldmodel = false;
    }
    else if (!strcmp (ValueForKey(ent, "classname"), "worldspawn"))
        worldmodel = true;
    else
    {
        worldmodel = false;

        if (verbose)
            PrintEntity (ent);
    }

    // allocate a tree to hold our entity
    tree = AllocTree ();

    // take the brushes and clip off all overlapping and contained faces,
    // leaving a perfect skin of the model with no hidden faces
    Brush_LoadEntity( ent, tree, hullnum, worldmodel );
    if( ent->brushes == NULL && hullnum == 0 )
    {
        PrintEntity( ent );
        printf( "WARNING: line %i: Entity with no valid brushes\n", ent->scriptline );
    }

    CSGFaces( tree );

    if( hullnum != 0 )
    {
        SolidBSP( tree, true );

        // assume non-world bmodels are simple
        if( worldmodel && !nofill )
        {
            PortalizeTree( tree );

            if( FillOutside( tree, hullnum ) )
            {
                GatherTreeFaces( tree );
                SolidBSP( tree, false );	// make a really good tree
            }
            FreeTreePortals( tree );
        }
    }
    else
    {
        // if not the world, make a good tree first
        // the world is just going to make a bad tree
        // because the outside filling will force a regeneration later
        SolidBSP( tree, worldmodel );	// SolidBSP generates a node tree

        // build all the portals in the bsp tree
        // some portals are solid polygons, and some are paths to other leafs

        // assume non-world bmodels are simple
        if( worldmodel && (!nofill || forcevis) )
        {
            PortalizeTree( tree );

            if( FillOutside( tree, 0 ) || forcevis )
            {
                FreeTreePortals( tree );

                // get the remaining faces together into surfaces again
                GatherTreeFaces( tree );

                // merge polygons
                MergeTreeFaces( tree );

                // make a really good tree
                SolidBSP( tree, false );

                // make the real portals for vis tracing
                PortalizeTree( tree );

                // save portal file for vis tracing
                WritePortalfile( tree );

                // fix tjunctions
                FixTJunctions( tree );
            }
            FreeTreePortals( tree );
        }
    }

    return tree;
}
