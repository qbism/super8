#include "bsp5.h"

qboolean transwater;

node_t	outside_node;		// portals outside the world face this

//=============================================================================

/*
===========
AllocPortal
===========
*/
portal_t *AllocPortal( void )
{
	portal_t	*p;

	p = qmalloc( sizeof( portal_t ) );
	memset( p, 0, sizeof( portal_t ) );

	return p;
}

/*
===========
FreePortal
===========
*/
void FreePortal( portal_t *p ) {
	qfree( p );
}

//=============================================================================

/*
=============
AddPortalToNodes
=============
*/
static void AddPortalToNodes (portal_t *p, node_t *front, node_t *back)
{
	if (p->nodes[0] || p->nodes[1])
		Error ("AddPortalToNode: allready included");

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;

	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}


/*
=============
RemovePortalFromNode
=============
*/
static void RemovePortalFromNode (portal_t *portal, node_t *l)
{
	portal_t	**pp, *t;

	// remove reference to the current portal
	pp = &l->portals;
	while (1)
	{
		t = *pp;
		if (!t)
			Error ("RemovePortalFromNode: portal not in leaf");

		if ( t == portal )
			break;

		if (t->nodes[0] == l)
			pp = &t->next[0];
		else if (t->nodes[1] == l)
			pp = &t->next[1];
		else
			Error ("RemovePortalFromNode: portal not bounding leaf");
	}

	if (portal->nodes[0] == l)
	{
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	}
	else if (portal->nodes[1] == l)
	{
		*pp = portal->next[1];
		portal->nodes[1] = NULL;
	}
}

//============================================================================

/*
================
CalcNodeBounds

Vic: proper node's bounding box calculation
================
*/
static void CalcNodeBounds (node_t *node)
{
	int			i;
	portal_t	*p;
	winding_t	*w;
	int			side;

	ClearBounds( node->mins, node->maxs );

	for (p = node->portals ; p ; p = p->next[side])
	{
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
		{
			Error ("CalcNodeBounds: mislinked portal");
			return; // hush compiler about side uninitialized
		}

		for (i = 0, w = p->winding; i < w->numpoints; i++)
			AddPointToBounds( w->points[i], node->mins, node->maxs );
	}
}

/*
================
MakeHeadnodePortals

The created portals will face the global outside_node
================
*/
static void MakeHeadnodePortals (node_t *node, const vec3_t mins, const vec3_t maxs)
{
	vec3_t		bounds[2];
	int			i, j, n;
	portal_t	*p, *portals[6];
	plane_t		bplanes[6], *pl;
	int			side;

	// pad with some space so there will never be null volume leafs
	for (i=0 ; i<3 ; i++)
	{
		bounds[0][i] = mins[i] - SIDESPACE;
		bounds[1][i] = maxs[i] + SIDESPACE;
	}

	outside_node.contents = CONTENTS_SOLID;
	outside_node.portals = NULL;

	for (i = 0;i < 3;i++)
	{
		for (j = 0;j < 2;j++)
		{
			n = j*3 + i;

			p = AllocPortal ();
			portals[n] = p;

			pl = &bplanes[n];
			memset (pl, 0, sizeof(*pl));
			if (j)
			{
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			}
			else
			{
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}
			p->planenum = FindPlane (pl, &side);
			p->winding = BaseWindingForPlane (pl);
			if (side)
				AddPortalToNodes (p, &outside_node, node);
			else
				AddPortalToNodes (p, node, &outside_node);
		}
	}

	// clip the basewindings by all the other planes
	for (i = 0;i < 6;i++)
	{
		for (j = 0;j < 6;j++)
		{
			if (j == i)
				continue;
			portals[i]->winding = ClipWinding (portals[i]->winding, &bplanes[j], true);
		}
	}
}

//============================================================================

/*
================
CutNodePortals_r
================
*/
static void CutNodePortals_r (node_t *node)
{
	plane_t 	*plane, clipplane;
	node_t		*f, *b, *other_node;
	portal_t	*p, *new_portal, *next_portal;
	winding_t	*w, *frontwinding, *backwinding;
	int			side;

	// Vic: properly calculate the bounding box
	CalcNodeBounds (node);

	//
	// separate the portals on node into it's children
	//
	if (node->contents)
		return;			// at a leaf, no more dividing

	plane = &mapplanes[node->planenum];

	f = node->children[0];
	b = node->children[1];

	//
	// create the new portal by taking the full plane winding for the cutting plane
	// and clipping it by all of the planes from the other portals
	//
	w = BaseWindingForPlane (&mapplanes[node->planenum]);
	side = 0;	// shut up compiler warning
	for (p = node->portals ; p ; p = p->next[side])
	{
		clipplane = mapplanes[p->planenum];
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
		{
			clipplane.dist = -clipplane.dist;
			VectorNegate (clipplane.normal, clipplane.normal);
			side = 1;
		}
		else
			Error ("CutNodePortals_r: mislinked portal");

		w = ClipWinding (w, &clipplane, true);
		if (!w)
		{
			printf ("WARNING: CutNodePortals_r:new portal was clipped away\n");
			break;
		}
	}

	if (w)
	{
		// if the plane was not clipped on all sides, there was an error
		new_portal = AllocPortal ();
		new_portal->planenum = node->planenum;
		new_portal->winding = w;
		AddPortalToNodes (new_portal, f, b);
	}

	// partition the portals
	for (p = node->portals ; p ; p = next_portal)
	{
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Error ("CutNodePortals_r: mislinked portal");
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode (p, p->nodes[0]);
		RemovePortalFromNode (p, p->nodes[1]);

		// cut the portal into two portals, one on each side of the cut plane
		DivideWindingEpsilon( p->winding, plane, &frontwinding, &backwinding, ON_EPSILON );

		if (!frontwinding)
		{
			if (side == 0)
				AddPortalToNodes (p, b, other_node);
			else
				AddPortalToNodes (p, other_node, b);
			continue;
		}
		if (!backwinding)
		{
			if (side == 0)
				AddPortalToNodes (p, f, other_node);
			else
				AddPortalToNodes (p, other_node, f);
			continue;
		}

		// the winding is split
		new_portal = AllocPortal ();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding (p->winding);
		p->winding = frontwinding;

		if (side == 0)
		{
			AddPortalToNodes (p, f, other_node);
			AddPortalToNodes (new_portal, b, other_node);
		}
		else
		{
			AddPortalToNodes (p, other_node, f);
			AddPortalToNodes (new_portal, other_node, b);
		}
	}

	CutNodePortals_r (f);
	CutNodePortals_r (b);
}


/*
==================
PortalizeTree

Builds the exact polyhedrons for the nodes and leafs
==================
*/
void PortalizeTree (tree_t *tree)
{
	qprintf ("----- portalize ----\n");

	MakeHeadnodePortals (tree->headnode, tree->mins, tree->maxs);
	CutNodePortals_r (tree->headnode);
}

/*
==================
FreeNodePortals_r
==================
*/
static void FreeNodePortals_r (node_t *node)
{
	portal_t	*p, *nextp;

	if (!node->contents)
	{
		FreeNodePortals_r (node->children[0]);
		FreeNodePortals_r (node->children[1]);
	}

	for (p=node->portals ; p ; p=nextp)
	{
		if (p->nodes[0] == node)
			nextp = p->next[0];
		else
			nextp = p->next[1];
		RemovePortalFromNode (p, p->nodes[0]);
		RemovePortalFromNode (p, p->nodes[1]);
		FreeWinding (p->winding);
		FreePortal (p);
	}
}

/*
==================
FreeTreePortals
==================
*/
void FreeTreePortals (tree_t *tree) {
	FreeNodePortals_r( tree->headnode );
}

/*
==============================================================================

PORTAL FILE GENERATION

==============================================================================
*/

#define	PORTALFILE	"PRT1"

static	FILE	*pf;
static	int		num_visleafs;				// leafs the player can be in
static	int		num_visportals;

/*
================
PortalSidesVisible

FIXME: add support for detail brushes
================
*/
qboolean PortalSidesVisible (portal_t *p)
{
	if (p->nodes[0]->contents == p->nodes[1]->contents)
		return true;
	if (transwater && p->nodes[0]->contents != CONTENTS_SOLID && p->nodes[1]->contents != CONTENTS_SOLID && p->nodes[0]->contents != CONTENTS_SKY && p->nodes[1]->contents != CONTENTS_SKY)
		return true;
	return false;
}

/*
================
NumberLeafs_r
================
*/
static void NumberLeafs_r (node_t *node)
{
	portal_t	*p;

	if (!node->contents)
	{	// decision node
		node->visleafnum = -99;
		NumberLeafs_r (node->children[0]);
		NumberLeafs_r (node->children[1]);
		return;
	}

	if (node->contents == CONTENTS_SOLID)
	{	// solid block, viewpoint never inside
		node->visleafnum = -1;
		return;
	}

	node->visleafnum = num_visleafs++;

	for (p = node->portals ; p ; )
	{
		if (p->nodes[0] == node)		// only write out from first leaf
		{
			if (p->winding && PortalSidesVisible (p))
				num_visportals++;
			p = p->next[0];
		}
		else
			p = p->next[1];
	}
}

// Vic: proper float output
static void WriteFloatToPortalFile (vec_t f)
{
	int i;

	i = Q_rint( f );
	if( f == i )
		fprintf( pf, "%i", i );
	else
		fprintf( pf, "%f", f );
}

static void WritePortalFile_r (node_t *node)
{
	int		i;
	portal_t	*p;
	winding_t	*w;
	plane_t		*pl, plane2;

	if (!node->contents)
	{
		WritePortalFile_r (node->children[0]);
		WritePortalFile_r (node->children[1]);
		return;
	}

	if (node->contents == CONTENTS_SOLID)
		return;

	for (p = node->portals ; p ; )
	{
		w = p->winding;
		// LordHavoc: transparent water support
		if (w && p->nodes[0] == node && PortalSidesVisible (p))
		{
			// write out to the file

			// sometimes planes get turned around when they are very near
			// the changeover point between different axis. interpret the
			// plane the same way vis will, and flip the side orders if needed
			pl = &mapplanes[p->planenum];
			PlaneFromWinding (w, &plane2);
			if ( DotProduct (pl->normal, plane2.normal) < 0.99 ) // backwards...
				fprintf (pf,"%i %i %i ",w->numpoints, p->nodes[1]->visleafnum, p->nodes[0]->visleafnum);
			else
				fprintf (pf,"%i %i %i ",w->numpoints, p->nodes[0]->visleafnum, p->nodes[1]->visleafnum);

			// Vic: proper float output
			for (i=0 ; i<w->numpoints ; i++)
			{
				fprintf (pf, "(");
				WriteFloatToPortalFile (w->points[i][0]);
				fprintf (pf, " ");
				WriteFloatToPortalFile (w->points[i][1]);
				fprintf (pf, " ");
				WriteFloatToPortalFile (w->points[i][2]);
				fprintf (pf, ")");
				fprintf (pf, i == w->numpoints-1 ? "\n" : " ");
			}
		}

		if (p->nodes[0] == node)
			p = p->next[0];
		else
			p = p->next[1];
	}
}



/*
================
WritePortalfile
================
*/
void WritePortalfile (tree_t *tree)
{
	// set the visleafnum field in every leaf and count the total number of portals
	num_visleafs = 0;
	num_visportals = 0;
	NumberLeafs_r (tree->headnode);

	// write the file
	printf ("writing %s\n", filename_prt);
	pf = fopen (filename_prt, "w");
	if (!pf)
		Error ("Error opening %s", filename_prt);

	fprintf (pf, "%s\n", PORTALFILE);
	fprintf (pf, "%i\n", num_visleafs);
	fprintf (pf, "%i\n", num_visportals);

	WritePortalFile_r (tree->headnode);

	fclose (pf);
}
