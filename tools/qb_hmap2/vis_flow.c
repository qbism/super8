#include "vis.h"

int c_chains;
int c_portalskip, c_leafskip;
int c_vistest, c_mighttest;

int active;

//=============================================================================

/*
==================
NewVisWinding
==================
*/
viswinding_t *NewVisWinding (int points)
{
	viswinding_t	*w;
	int			size;

	if (points > MAX_POINTS_ON_VISWINDING)
		Error ("NewWinding: %i points", points);

	size = (int)((size_t)((viswinding_t *)0)->points[points]);
	w = qmalloc (size);
	memset (w, 0, size);

	return w;
}

/*
==================
FreeVisWinding
==================
*/
void FreeVisWinding (viswinding_t *w)
{
	if (!w->original)
		qfree (w);
}

/*
==================
CopyVisWinding
==================
*/
viswinding_t *CopyVisWinding (viswinding_t *w)
{
	int			size;
	viswinding_t	*c;

	size = (int)((size_t)((viswinding_t *)0)->points[w->numpoints]);
	c = qmalloc (size);
	memcpy (c, w, size);
	c->original = false;
	return c;
}

/*
==================
ClipVisWinding

Clips the winding to the plane, returning the new winding on the positive side
Frees the input winding.
If keepon is true, an exactly on-plane winding will be saved, otherwise
it will be clipped away.
==================
*/
viswinding_t *ClipVisWinding (viswinding_t *in, plane_t *split, qboolean keepon)
{
	int				i, j;
	vec_t			dists[MAX_POINTS_ON_VISWINDING + 1];
	int				sides[MAX_POINTS_ON_VISWINDING + 1];
	int				counts[3];
	vec_t			dot;
	vec_t			*p1, *p2;
	vec3_t			mid;
	viswinding_t	*neww;
	int				maxpts;

	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dists[i] = dot = DotProduct (in->points[i], split->normal) - split->dist;
		if (dot > ON_EPSILON) sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON) sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (keepon && !counts[0] && !counts[1])
		return in;

	if (!counts[0])
	{
		FreeVisWinding (in);
		return NULL;
	}
	if (!counts[1])
		return in;

	maxpts = in->numpoints+4;	// can't use counts[0]+2 because
								// of fp grouping errors
	neww = NewVisWinding (maxpts);

	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->points[i];

		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

	// generate a split point
		p2 = in->points[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}

		VectorCopy (mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}

	if (neww->numpoints > maxpts)
		Error ("ClipVisWinding: points exceeded estimate");

// free the original winding
	FreeVisWinding (in);

	return neww;
}

void PlaneFromVisWinding (viswinding_t *w, plane_t *plane)
{
	vec3_t		v1, v2;

// calc plane
	VectorSubtract (w->points[2], w->points[1], v1);
	VectorSubtract (w->points[0], w->points[1], v2);
	CrossProduct (v2, v1, plane->normal);
	VectorNormalize (plane->normal);
	plane->dist = DotProduct (w->points[0], plane->normal);
}

/*
=================
VisWindingCentre
=================
*/
void VisWindingCentre( viswinding_t *w, vec3_t centre, vec_t *radius )
{
	int			i;
	vec3_t		edge;
	vec_t		dist, best;

	if( !w->numpoints )
		Error( "WindingCentre: no points" );

	VectorCopy( w->points[0], centre );
	for( i = 1; i < w->numpoints; i++ )
		VectorAdd( centre, w->points[i], centre );
	VectorScale( centre, (vec_t)1.0/w->numpoints, centre );

	if( radius ) {
		best = 0;
		for( i = 0; i < w->numpoints; i++ ) {
			VectorSubtract( centre, w->points[i], edge );
			dist = VectorLength( edge );
			if( dist > best )
				best = dist;
		}

	#ifdef PARANOID
		if( best == 0 )
			Error( "WindingCentre: winding has zero radius" );
	#endif

		*radius = best;
	}
}

void CheckStack (leaf_t *leaf, threaddata_t *thread)
{
	pstack_t	*p;

	for (p=thread->pstack_head.next ; p ; p=p->next)
		if (p->leaf == leaf)
			Error ("CheckStack: leaf recursion");
}


/*
==============
ClipToSeperators

Source, pass, and target are an ordering of portals.

Generates separating planes candidates by taking two points from source and one
point from pass, and clips target by them.

If target is totally clipped away, that portal can not be seen through.

Normal clip keeps target on the same side as pass, which is correct if the
order goes source, pass, target.  If the order goes pass, source, target then
flipclip should be set.
==============
*/
viswinding_t *ClipToSeperators (viswinding_t *source, viswinding_t *pass, viswinding_t *target, qboolean flipclip)
{
	int			i, j, k, l;
	plane_t		plane;
	vec3_t		v1, v2;
	vec_t		d, epcompare;
	vec_t		length;
//	int			counts[3];
	qboolean		fliptest;

// check all combinations
	for (i=0 ; i<source->numpoints ; i++)
	{
		l = (i+1)%source->numpoints;
		VectorSubtract (source->points[l], source->points[i], v1);

	// find a vertex of pass that makes a plane that puts all of the
	// vertexes of pass on the front side and all of the vertexes of
	// source on the back side
		for (j=0 ; j<pass->numpoints ; j++)
		{
			VectorSubtract (pass->points[j], source->points[i], v2);
			CrossProduct(v1, v2, plane.normal);

		// if points don't make a valid plane, skip it
			length = DotProduct(plane.normal, plane.normal);

			if (length < ON_EPSILON)
				continue;

			length = 1/sqrt(length);
			VectorScale( plane.normal, length, plane.normal );
			plane.dist = DotProduct (pass->points[j], plane.normal);

		//
		// find out which side of the generated seperating plane has the
		// source portal
		//
			fliptest = false;
			for (k=0 ; k<source->numpoints ; k++)
			{
				if (k == i || k == l)
					continue;
				d = DotProduct (source->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
				{	// source is on the negative side, so we want all
					// pass and target on the positive side
					fliptest = false;
					break;
				}
				else if (d > ON_EPSILON)
				{	// source is on the positive side, so we want all
					// pass and target on the negative side
					fliptest = true;
					break;
				}
			}
			if (k == source->numpoints)
				continue;		// planar with source portal

		//
		// flip the normal if the source portal is backwards
		//
			if (fliptest)
			{
				VectorInverse (plane.normal);
				plane.dist = -plane.dist;
			}

		//
		// if all of the pass portal points are now on the positive side,
		// this is the seperating plane
		//
			// LordHavoc: rewrote this to be faster
			for (k=0;k<j;k++)
			{
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON) goto cliptoseperatorscontinue;
				else if (d > ON_EPSILON) {epcompare = plane.dist - ON_EPSILON;goto cliptoseperatorsloop1;}
			}
			for (k=j+1;k<pass->numpoints;k++)
			{
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON) goto cliptoseperatorscontinue;
				else if (d > ON_EPSILON) {epcompare = plane.dist - ON_EPSILON;goto cliptoseperatorsloop2;}
			}
			continue;
			for (;k<j;k++)
			{
				if (DotProduct(pass->points[k], plane.normal) < epcompare) goto cliptoseperatorscontinue;
cliptoseperatorsloop1:
				;
			}
			for (k=j+1;k<pass->numpoints;k++)
			{
				if (DotProduct(pass->points[k], plane.normal) < epcompare) goto cliptoseperatorscontinue;
cliptoseperatorsloop2:
				;
			}
			/*
			// original code
			counts[0] = counts[1] = counts[2] = 0;
			for (k=0 ; k<pass->numpoints ; k++)
			{
				if (k==j)
					continue;
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
					break;
				else if (d > ON_EPSILON)
					counts[0]++;
				else
					counts[2]++;
			}
			if (k != pass->numpoints)
				continue;	// points on negative side, not a seperating plane

			if (!counts[0])
			{
				continue;	// planar with seperating plane
			}
			*/

		//
		// flip the normal if we want the back side
		//
			if (flipclip)
			{
				VectorInverse(plane.normal);
				plane.dist = -plane.dist;
			}

		//
		// clip target by the seperating plane
		//
			target = ClipVisWinding (target, &plane, false);
			if (!target)
				return NULL;		// target is not visible

			if (rvis)
				break;
cliptoseperatorscontinue:
			;
		}
	}

	return target;
}



/*
==================
RecursiveLeafFlow

Flood fill through the leafs
If src_portal is NULL, this is the originating leaf
==================
*/
void RecursiveLeafFlow (int leafnum, threaddata_t *thread, pstack_t *prevstack)
{
	pstack_t	stack;
	portal_t	*p;
	plane_t		backplane;
	viswinding_t *source, *target;
	leaf_t 		*leaf;
	int			i, j;
	long		*test, *might, *vis;
	qboolean		more;

	c_chains++;

	leaf = &leafs[leafnum];
	CheckStack (leaf, thread);

// mark the leaf as visible
	if (! (thread->leafvis[leafnum>>3] & (1<<(leafnum&7)) ) )
	{
		thread->leafvis[leafnum>>3] |= 1<<(leafnum&7);
		thread->base->numcansee++;
	}

	prevstack->next = &stack;
	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;
	stack.mightsee = qmalloc(bitlongs * sizeof(long));
	might = (long *)stack.mightsee;
	vis = (long *)thread->leafvis;

// check all portals for flowing into other leafs
	for (i=0 ; i<leaf->numportals ; i++)
	{
		p = leaf->portals[i];

		if ( ! (prevstack->mightsee[p->leaf>>3] & (1<<(p->leaf&7)) ) )
		{
			c_leafskip++;
			continue;	// can't possibly see it
		}

	// if the portal can't see anything we haven't allready seen, skip it
		if (p->status == stat_done)
		{
			c_vistest++;
			test = (long *)p->visbits;
		}
		else
		{
			c_mighttest++;
			test = (long *)p->mightsee;
		}
		more = false;
		for (j=0 ; j<bitlongs ; j++)
		{
			might[j] = ((long *)prevstack->mightsee)[j] & test[j];
			if (might[j] & ~vis[j])
				more = true;
		}

		if (!more)
		{	// can't see anything new
			c_portalskip++;
			continue;
		}

// get plane of portal, point normal into the neighbor leaf
		stack.portalplane = p->plane;
		VectorNegate (p->plane.normal, backplane.normal);
		backplane.dist = -p->plane.dist;

		if (VectorCompare (prevstack->portalplane.normal, backplane.normal) )
			continue;	// can't go out a coplanar face

		c_portalcheck++;

		stack.portal = p;
		stack.next = NULL;

		target = ClipVisWinding (p->winding, &thread->pstack_head.portalplane, false);
		if (!target)
			continue;

		if (!prevstack->pass)
		{	// the second leaf can only be blocked if coplanar
			stack.source = prevstack->source;
			stack.pass = target;
			RecursiveLeafFlow (p->leaf, thread, &stack);
			FreeVisWinding (target);
			continue;
		}

		target = ClipVisWinding (target, &prevstack->portalplane, false);
		if (!target)
			continue;

		source = CopyVisWinding (prevstack->source);

		source = ClipVisWinding (source, &backplane, false);
		if (!source)
		{
			FreeVisWinding (target);
			continue;
		}

		c_portaltest++;

		if (testlevel > 0)
		{
			target = ClipToSeperators (source, prevstack->pass, target, false);
			if (!target)
			{
				FreeVisWinding (source);
				continue;
			}
		}

		if (testlevel > 1)
		{
			target = ClipToSeperators (prevstack->pass, source, target, true);
			if (!target)
			{
				FreeVisWinding (source);
				continue;
			}
		}

		if (testlevel > 2)
		{
			source = ClipToSeperators (target, prevstack->pass, source, false);
			if (!source)
			{
				FreeVisWinding (target);
				continue;
			}
		}

		if (testlevel > 3)
		{
			source = ClipToSeperators (prevstack->pass, target, source, true);
			if (!source)
			{
				FreeVisWinding (target);
				continue;
			}
		}

		stack.source = source;
		stack.pass = target;

		c_portalpass++;

	// flow through it for real
		RecursiveLeafFlow (p->leaf, thread, &stack);

		FreeVisWinding (source);
		FreeVisWinding (target);
	}

	qfree (stack.mightsee);
}


/*
===============
PortalFlow

===============
*/
void PortalFlow (portal_t *p)
{
	threaddata_t	data;

	if (p->status != stat_working)
		Error ("PortalFlow: reflowed");
	p->status = stat_working;

	p->visbits = qmalloc (bitlongs * sizeof(long));
	memset (p->visbits, 0, bitlongs * sizeof(long));

	memset (&data, 0, sizeof(data));
	data.leafvis = p->visbits;
	data.base = p;

	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	data.pstack_head.mightsee = p->mightsee;

	RecursiveLeafFlow (p->leaf, &data, &data.pstack_head);

	p->status = stat_done;
}


/*
===============================================================================

This is a rough first-order aproximation that is used to trivially reject some
of the final calculations.

===============================================================================
*/

byte	*portalsee;
int		c_leafsee, c_portalsee;

void SimpleFlood (portal_t *srcportal, int leafnum)
{
	int		i;
	leaf_t	*leaf;
	portal_t	*p;

	if (srcportal->mightsee[leafnum>>3] & (1<<(leafnum&7)) )
		return;
	srcportal->mightsee[leafnum>>3] |= (1<<(leafnum&7));
	c_leafsee++;

	leaf = &leafs[leafnum];

	for (i=0 ; i<leaf->numportals ; i++)
	{
		p = leaf->portals[i];
		if ( !portalsee[ p - portals ] )
			continue;
		SimpleFlood (srcportal, p->leaf);
	}
}


/*
==============
BasePortalVis
==============
*/
void BasePortalVis (void)
{
	int i, j, k;
	portal_t *tp, *p;
	plane_t *plane;
	vec_t d;
	viswinding_t *w;
	time_t oldtime, newtime;
	vec3_t backnormal, dist;

	portalsee = qmalloc (numportals*2);
	oldtime = time(NULL);
	for (i=0, p = portals ; i<numportals*2 ; i++, p++)
	{
		p->mightsee = qmalloc (bitlongs * sizeof(long));
		memset (p->mightsee, 0, bitlongs * sizeof(long));

		c_portalsee = 0;
		memset (portalsee, 0, numportals*2);

		VectorNegate (p->plane.normal, backnormal);

		for (j=0, tp = portals ; j<numportals*2 ; j++, tp++)
		{
			if (j == i)
				continue;
			if (VectorCompare (backnormal, tp->plane.normal))
				continue;

			if (farplane > 0)
			{
				VectorSubtract (tp->origin, p->origin, dist);
				if (VectorLength (dist) - tp->radius - p->radius > farplane)
					continue;
			}

			w = tp->winding;
			plane = &p->plane;
			for (k=0 ; k<w->numpoints ; k++)
			{
				if (plane->type < 3)
					d = w->points[k][plane->type] - plane->dist;
				else
					d = DotProduct (w->points[k], plane->normal) - plane->dist;
				if (d > ON_EPSILON)
					break;
			}
			if (k == w->numpoints)
				continue;	// no points on front

			w = p->winding;
			plane = &tp->plane;
			for (k=0 ; k<w->numpoints ; k++)
			{
				if (plane->type < 3)
					d = w->points[k][plane->type] - plane->dist;
				else
					d = DotProduct (w->points[k], plane->normal) - plane->dist;
				if (d < -ON_EPSILON)
					break;
			}
			if (k == w->numpoints)
				continue;	// no points on back

			portalsee[j] = 1;
			c_portalsee++;
		}

		c_leafsee = 0;
		SimpleFlood (p, p->leaf);
		p->nummightsee = c_leafsee;
//		printf ("portal:%4i  c_leafsee:%4i \n", i, c_leafsee);
		newtime = time(NULL);
		if (newtime != oldtime)
		{
			printf ("\rbasevis: portal%5i of%5i", i, numportals*2);
			fflush(stdout);
			newtime = oldtime;
		}
	}
	printf ("\rbasevis done                \n");
	fflush(stdout);
	qfree (portalsee);
}
