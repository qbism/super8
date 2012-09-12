
#include "bsp5.h"

int		outleafs;

/*
===========
PointInLeaf

Vic: rewrote to be faster
LordHavoc: shortened a little
===========
*/
node_t	*PointInLeaf (node_t *node, vec3_t point)
{
	while (!node->contents)
		node = node->children[PlaneDiff( point, &mapplanes[node->planenum] ) <= 0];
	return node;
}

/*
===========
PlaceOccupant
===========
*/
qboolean PlaceOccupant (int num, vec3_t point, node_t *headnode)
{
	node_t	*n;

	n = PointInLeaf (headnode, point);
	if (n->contents == CONTENTS_SOLID)
		return false;
	n->occupied = num;
	return true;
}


/*
==============
MarkLeakTrail
==============
*/
portal_t	*prevleaknode;
FILE	*leakfilepts;
FILE	*leakfilelin;
void MarkLeakTrail (portal_t *n2)
{
	int			i;
	vec3_t		p1, p2, dir;
	vec_t		len;
	portal_t	*n1;

	n1 = prevleaknode;
	prevleaknode = n2;

	if (!n1)
		return;

	WindingCentre( n2->winding, p1, NULL );
	WindingCentre( n1->winding, p2, NULL );

	VectorSubtract (p2, p1, dir);
	len = VectorLength (dir);
	VectorNormalize (dir);

	if (!leakfilepts)
	{
		leakfilepts = fopen (filename_pts, "w");
		if (!leakfilepts)
			Error ("Couldn't open %s\n", filename_pts);
	}
	if (!leakfilelin)
	{
		leakfilelin = fopen (filename_lin, "w");
		if (!leakfilelin)
			Error ("Couldn't open %s\n", filename_lin);
		fprintf (leakfilelin, "%f %f %f\n", p1[0], p1[1], p1[2]);
	}

	if (leakfilepts)
	{
		while (len > 2)
		{
			fprintf (leakfilepts, "%f %f %f\n", p1[0], p1[1], p1[2]);
			for (i = 0;i < 3;i++)
				p1[i] += dir[i] * 2;
			len -= 2;
		}
	}

	if (leakfilelin)
		fprintf (leakfilelin, "%f %f %f\n", p2[0], p2[1], p2[2]);
}

/*
==================
RecursiveFillOutside

If fill is false, just check, don't fill
Returns true if an occupied leaf is reached
==================
*/
int		hit_occupied;
qboolean RecursiveFillOutside (node_t *l, int hullnum, qboolean fill)
{
	portal_t	*p;
	int			s;

	if (l->contents == CONTENTS_SOLID || l->contents == CONTENTS_SKY)
		return false;

	if (l->valid == valid)
		return false;

	if (l->occupied)
	{
		hit_occupied = l->occupied; // LordHavoc: this was missing from the released source... odd
		return true;
	}

	l->valid = valid;

// fill it and it's neighbors
	if (fill)
		l->contents = CONTENTS_SOLID;
	outleafs++;

	for (p = l->portals;p;)
	{
		s = (p->nodes[0] == l);

		if (RecursiveFillOutside (p->nodes[s], hullnum, fill) )
		{
			// leaked, so stop filling
			if (!hullnum)
				MarkLeakTrail (p);
			return true;
		}
		p = p->next[!s];
	}

	return false;
}

/*
==================
ClearOutFaces

==================
*/
void ClearOutFaces (node_t *node)
{
	face_t	**fp;

	if (node->planenum != PLANENUM_LEAF)
	{
		ClearOutFaces (node->children[0]);
		ClearOutFaces (node->children[1]);
		return;
	}
	if (node->contents != CONTENTS_SOLID)
		return;

	for( fp = node->markfaces; *fp; fp++ ) {
		// mark all the original faces that are removed
		FreeWinding( (*fp)->winding );
		(*fp)->winding = NULL;
	}
	node->faces = NULL;
}


//=============================================================================

/*
===========
FillOutside

===========
*/
qboolean FillOutside (tree_t *tree, int hullnum)
{
	int			i;
	int			s;
	qboolean	inside;
	vec3_t		origin;

	qprintf ("----- FillOutside ----\n");

	if (nofill)
	{
		printf ("skipped\n");
		return false;
	}

	inside = false;
	for (i=1 ; i<num_entities ; i++)
	{
		GetVectorForKey (&entities[i], "origin", origin);

		if (DotProduct (origin, origin) >= 0.1 && entities[i].brushes == NULL)
		{
			if (PlaceOccupant (i, origin, tree->headnode))
				inside = true;
		}
	}

	if (!inside)
	{
		printf ("Hullnum %i: No entities in empty space -- no filling performed\n", hullnum);
		return false;
	}

	s = !(outside_node.portals->nodes[1] == &outside_node);

// first check to see if an occupied leaf is hit
	outleafs = 0;
	valid++;

	prevleaknode = NULL;

	if (RecursiveFillOutside (outside_node.portals->nodes[s], hullnum, false))
	{
		if (leakfilepts)
			fclose(leakfilepts);
		leakfilepts = NULL;
		if (leakfilelin)
			fclose(leakfilelin);
		leakfilelin = NULL;
		if (!hullnum)
		{
			GetVectorForKey (&entities[hit_occupied], "origin", origin);

			qprintf ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
			qprintf ("reached occupant at: (%4.0f,%4.0f,%4.0f)\n", origin[0], origin[1], origin[2]);
			qprintf ("no filling performed\n");
			qprintf ("leak file written to %s\n", filename_pts);
			qprintf ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		}

		// remove faces from filled in leafs
		ClearOutFaces (tree->headnode);
		return false;
	}

	// now go back and fill things in
	valid++;
	RecursiveFillOutside (outside_node.portals->nodes[s], hullnum, true);

	// remove faces from filled in leafs
	ClearOutFaces (tree->headnode);

	qprintf ("%4i outleafs\n", outleafs);
	return true;
}
