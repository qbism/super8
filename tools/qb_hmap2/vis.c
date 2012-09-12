// vis.c

#include "vis.h"
#include "threads.h"

#define	MAX_THREADS		4

int numportals;
int portalleafs;

portal_t *portals;
leaf_t *leafs;

int c_portaltest, c_portalpass, c_portalcheck;
int c_reused;

// past visfile
byte *vismap, *vismap_p, *vismap_end;
int originalvismapsize;

// [bitbytes*portalleafs]
byte *uncompressed;

// (portalleafs+63)>>3
int bitbytes;
int bitlongs;

qboolean fastvis;
qboolean verbose;
qboolean rvis;
qboolean noreuse;

vec_t farplane;

// LordHavoc: default to level 4 vis
int testlevel = 4;
// LordHavoc: optional ambient sounds
qboolean noambient, noambientwater, noambientslime, noambientlava, noambientsky;

//=============================================================================

/*
=============
GetNextPortal

Returns the next portal for a thread to work on
Returns the portals from the least complex, so the later ones can reuse
the earlier information.
=============
*/
portal_t *GetNextPortal (void)
{
	int		j;
	portal_t	*p, *tp;
	int		min;
	int		pass;

	min = 99999;
	p = NULL;

	for (pass = 0;pass < 256;pass++)
	{
		for (j=pass, tp = portals+j ; j<numportals*2 ; j += 256, tp += 256)
		{
			if (tp->nummightsee < min && tp->status == stat_none)
			{
				min = tp->nummightsee;
				p = tp;
			}
		}
	}


	if (p)
		p->status = stat_working;

	return p;
}

long portalizestarttime, portalschecked;

/*
==============
LeafThread
==============
*/
void *LeafThread (int thread)
{
	time_t oldtime, newtime;
	portal_t *p;

//printf ("Begining LeafThread: %i\n",(int)thread);
	oldtime = time(NULL);
	do
	{
		p = GetNextPortal ();
		if (!p)
			break;

		PortalFlow (p);

		portalschecked++;
		if (verbose)
			printf ("portal %4i of %4i mightsee:%4i  cansee:%4i\n", (int) portalschecked, (int) numportals * 2, (int) p->nummightsee, (int) p->numcansee);
		else
		{
			newtime = time(NULL);
			if (newtime != oldtime)
			{
				printf("\rportal %4i of %4i (%3i%%), estimated time left: %4i seconds    \b\b\b\b", (int) portalschecked, (int) numportals * 2, (int) (portalschecked*100/(numportals*2)), (int) ((numportals*2-portalschecked)*(newtime-portalizestarttime)/portalschecked));
				fflush(stdout);
				oldtime = newtime;
			}
		}
	} while (1);
	printf("\n");

//printf ("Completed LeafThread: %i\n",(int)thread);

	return NULL;
}

/*
===============
LeafFlow

Builds the entire visibility list for a leaf
===============
*/
int		totalvis;

void LeafFlow (int leafnum)
{
	leaf_t		*leaf;
	byte		*outbuffer;
	byte		compressed[MAX_MAP_LEAFS/8];
	int			i, j;
	int			numvis;
	byte		*dest;
	portal_t	*p;

//
// flow through all portals, collecting visible bits
//
	outbuffer = uncompressed + leafnum*bitbytes;
	leaf = &leafs[leafnum];
	for (i=0 ; i<leaf->numportals ; i++)
	{
		p = leaf->portals[i];
		if (p->status != stat_done)
			Error ("portal not done");
		for (j=0 ; j<bitbytes ; j++)
			outbuffer[j] |= p->visbits[j];
	}

	if (outbuffer[leafnum>>3] & (1<<(leafnum&7)))
		Error ("Leaf portals saw into leaf");

	outbuffer[leafnum>>3] |= (1<<(leafnum&7));

	numvis = 0;
	for (i=0 ; i<portalleafs ; i++)
		if (outbuffer[i>>3] & (1<<(i&3)))
			numvis++;

//
// compress the bit string
//
	if (verbose)
		printf ("leaf %4i : %4i visible\n", leafnum, numvis);
	totalvis += numvis;

	if( !noreuse ) {	// Vic: reuse old vis data
		int		i;
		byte	*data;

		data = uncompressed;
		for( i = 0; i < leafnum; i++, data += bitbytes ) {
			if( !memcmp( data, outbuffer, bitbytes ) ) {
				c_reused++;
				dleafs[leafnum+1].visofs = dleafs[i+1].visofs;
				return;
			}
		}
	}

#if 0
	i = bitbytes;
	memcpy (compressed, outbuffer, bitbytes);
#else
	i = CompressVis (outbuffer, compressed, bitbytes);
#endif


	dest = vismap_p;
	vismap_p += i;

	if (vismap_p > vismap_end)
		Error ("Vismap expansion overflow");

	dleafs[leafnum+1].visofs = dest-vismap;	// leaf 0 is a common solid

	memcpy (dest, compressed, i);
}


/*
==================
CalcPortalVis
==================
*/
void CalcPortalVis (void)
{
	int		i;

// fastvis just uses mightsee for a very loose bound
	if (fastvis)
	{
		for (i=0 ; i<numportals*2 ; i++)
		{
			portals[i].visbits = portals[i].mightsee;
			portals[i].status = stat_done;
		}
		return;
	}

	portalizestarttime = time(NULL);

	LeafThread (0);

	if (verbose)
	{
		printf ("portalcheck: %i  portaltest: %i  portalpass: %i\n",c_portalcheck, c_portaltest, c_portalpass);
		printf ("c_vistest: %i  c_mighttest: %i\n",c_vistest, c_mighttest);
	}
}

/*
==================
CalcVis
==================
*/
void CalcVis (void)
{
	int		i;

	BasePortalVis ();

	CalcPortalVis ();

//
// assemble the leaf vis lists by oring and compressing the portal lists
//
	for (i=0 ; i<portalleafs ; i++)
		LeafFlow (i);

	printf ("average leafs visible: %i\n", totalvis / portalleafs);
	fflush(stdout);
}

/*
==============================================================================

PASSAGE CALCULATION (not used yet...)

==============================================================================
*/

int		count_sep;

sep_t *Findpassages (viswinding_t *source, viswinding_t *pass)
{
	int			i, j, k, l;
	plane_t		plane;
	vec3_t		v1, v2;
	double		d;
	double		length;
	int			counts[3];
	qboolean	fliptest;
	sep_t		*sep, *list;

	list = NULL;

	// hush warnings about uninitialized plane.type (which isn't used here)
	memset(&plane, 0, sizeof(plane));


// check all combinations
	for (i=0 ; i<source->numpoints ; i++)
	{
		l = (i+1)%source->numpoints;
		VectorSubtract (source->points[l] , source->points[i], v1);

	// fing a vertex of pass that makes a plane that puts all of the
	// vertexes of pass on the front side and all of the vertexes of
	// source on the back side
		for (j=0 ; j<pass->numpoints ; j++)
		{
			VectorSubtract (pass->points[j], source->points[i], v2);
			CrossProduct(v1, v2, plane.normal);

		// if points don't make a valid plane, skip it
			length = DotProduct( plane.normal, plane.normal );

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
				VectorNegate (plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}

		//
		// if all of the pass portal points are now on the positive side,
		// this is the seperating plane
		//
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
				continue;	// planar with pass portal

		//
		// save this out
		//
			count_sep++;

			sep = qmalloc(sizeof(*sep));
			sep->next = list;
			list = sep;
			sep->plane = plane;
		}
	}

	return list;
}



/*
============
CalcPassages
============
*/
void CalcPassages (void)
{
	int		i, j, k;
	int		count, count2;
	leaf_t	*l;
	portal_t	*p1, *p2;
	sep_t		*sep;
	passage_t		*passages;

	printf ("building passages...\n");

	count = count2 = 0;
	for (i=0 ; i<portalleafs ; i++)
	{
		l = &leafs[i];

		for (j=0 ; j<l->numportals ; j++)
		{
			p1 = l->portals[j];
			for (k=0 ; k<l->numportals ; k++)
			{
				if (k==j)
					continue;

				count++;
				p2 = l->portals[k];

			// definately can't see into a coplanar portal
				if (PlaneCompare (&p1->plane, &p2->plane) )
					continue;

				count2++;

				sep = Findpassages (p1->winding, p2->winding);
				if (!sep)
				{
//					Error ("No seperating planes found in portal pair");
					count_sep++;
					sep = qmalloc(sizeof(*sep));
					sep->next = NULL;
					sep->plane = p1->plane;
				}
				passages = qmalloc(sizeof(*passages));
				passages->planes = sep;
				passages->from = p1->leaf;
				passages->to = p2->leaf;
				passages->next = l->passages;
				l->passages = passages;
			}
		}
	}

	printf ("numpassages: %i (%i)\n", count2, count);
	printf ("total passages: %i\n", count_sep);
}

//=============================================================================

/*
============
LoadPortals
============
*/
void LoadPortals (char *name)
{
	int			i, j;
	portal_t	*p;
	leaf_t		*l;
	char		magic[80];
	FILE		*f;
	int			numpoints;
	viswinding_t *w;
	int			leafnums[2];
	plane_t		plane;
	vec3_t		origin;
	vec_t		radius;

	if (!strcmp(name,"-"))
		f = stdin;
	else
	{
		f = fopen(name, "r");
		if (!f)
			Error ("LoadPortals: couldn't read %s\n",name);
	}

	if (fscanf (f,"%79s\n%i\n%i\n",magic, &portalleafs, &numportals) != 3)
		Error ("LoadPortals: failed to read header");
	if (strcmp(magic,PORTALFILE))
		Error ("LoadPortals: not a portal file");

	printf ("%4i portalleafs\n", portalleafs);
	printf ("%4i numportals\n", numportals);

	bitbytes = (portalleafs+7)>>3;
	bitlongs = (portalleafs+(8*sizeof(long)-1))/(8*sizeof(long));

// each file portal is split into two memory portals
	portals = qmalloc(2*numportals*sizeof(portal_t));
	memset (portals, 0, 2*numportals*sizeof(portal_t));

	leafs = qmalloc(portalleafs*sizeof(leaf_t));
	memset (leafs, 0, portalleafs*sizeof(leaf_t));

	originalvismapsize = portalleafs*((portalleafs+7)/8);

	vismap = vismap_p = dvisdata;
	vismap_end = vismap + MAX_MAP_VISIBILITY;

	for (i=0, p=portals ; i<numportals ; i++)
	{
		if (fscanf (f, "%i %i %i ", &numpoints, &leafnums[0], &leafnums[1])
			!= 3)
			Error ("LoadPortals: reading portal %i", i);
		if (numpoints > MAX_POINTS_ON_VISWINDING)
			Error ("LoadPortals: portal %i has too many points", i);
		if ((unsigned)leafnums[0] > (unsigned)portalleafs
		 || (unsigned)leafnums[1] > (unsigned)portalleafs)
			Error ("LoadPortals: reading portal %i", i);

		w = p->winding = NewVisWinding (numpoints);
		w->original = true;
		w->numpoints = numpoints;

		for (j=0 ; j<numpoints ; j++)
		{
			double	v[3];
			int		k;

			// scanf into double, then assign to vec_t
			if (fscanf (f, "(%lf %lf %lf ) ", &v[0], &v[1], &v[2]) != 3)
				Error ("LoadPortals: reading portal %i", i);
			for (k=0 ; k<3 ; k++)
				w->points[j][k] = v[k];
		}
		fscanf (f, "\n");

	// calc plane
		PlaneFromVisWinding (w, &plane);

	// calc origin and radius
		VisWindingCentre (w, origin, &radius);

	// create forward portal
		l = &leafs[leafnums[0]];
		if (l->numportals == MAX_PORTALS_ON_LEAF)
			Error ("Leaf with too many portals");
		l->portals[l->numportals] = p;
		l->numportals++;

		p->winding = w;
		VectorCopy (origin, p->origin);
		p->radius = radius;
		VectorNegate (plane.normal, p->plane.normal);
		p->plane.dist = -plane.dist;
		if (p->plane.normal[0] == 1)
			p->plane.type = PLANE_X;
		else if (p->plane.normal[1] == 1)
			p->plane.type = PLANE_Y;
		else if (p->plane.normal[2] == 1)
			p->plane.type = PLANE_Z;
		else
			p->plane.type = PLANE_ANYX;
		p->leaf = leafnums[1];
		p++;

	// create backwards portal
		l = &leafs[leafnums[1]];
		if (l->numportals == MAX_PORTALS_ON_LEAF)
			Error ("Leaf with too many portals");
		l->portals[l->numportals] = p;
		l->numportals++;

		p->winding = w;
		VectorCopy (origin, p->origin);
		p->radius = radius;
		p->plane = plane;
		if (p->plane.normal[0] == 1)
			p->plane.type = PLANE_X;
		else if (p->plane.normal[1] == 1)
			p->plane.type = PLANE_Y;
		else if (p->plane.normal[2] == 1)
			p->plane.type = PLANE_Z;
		else
			p->plane.type = PLANE_ANYX;
		p->leaf = leafnums[0];
		p++;
	}

	fclose (f);
}


/*
===========
Vis_Main
===========
*/
int Vis_Main( int argc, char **argv )
{
	int			i;
	double		start, end;

	printf ("---- vis ----\n");

	fastvis = false;
	verbose = false;
	rvis = true;
	noambientslime = true;
	noreuse = false;
	farplane = 0;

	if (ismcbsp)
		noambient = true;

	for (i=1 ; i<argc ; i++)
	{
		if (!strcmp(argv[i], "-norvis"))
		{
			rvis = false;
			printf ("rvis optimization disabled\n");
		}
		else if (!strcmp(argv[i], "-fast"))
		{
			printf ("fastvis = true\n");
			fastvis = true;
		}
		else if (!strcmp(argv[i], "-level"))
		{
			testlevel = atoi(argv[i+1]);
			printf ("testlevel = %i\n", testlevel);
			i++;
		}
		else if (!strcmp(argv[i], "-v"))
		{
			printf ("verbose = true\n");
			verbose = true;
		}
		else if (!strcmp(argv[i], "-noreuse"))
		{
			printf ("vis rows reusage disabled\n");
			noreuse = true;
		}
		else if (!strcmp(argv[i], "-noambient"))
		{
			noambient = true;
			printf ("all ambient sounds disabled\n");
		}
		else if (!strcmp(argv[i], "-noambientwater"))
		{
			noambientwater = true;
			printf ("ambient water sounds disabled\n");
		}
		else if (!strcmp(argv[i], "-ambientslime"))
		{
			noambientslime = false;
			printf ("ambient slime sounds enabled\n");
		}
		else if (!strcmp(argv[i], "-noambientlava"))
		{
			noambientlava = true;
			printf ("ambient lava sounds disabled\n");
		}
		else if (!strcmp(argv[i], "-noambientsky"))
		{
			noambientsky = true;
			printf ("ambient sky sounds disabled\n");
		}
		else if (!strcmp(argv[i], "-farplane"))
		{
			farplane = atoi (argv[i+1]);
			printf ("farplane = %f\n", farplane);
			i++;
		}
		else if (argv[i][0] == '-')
			Error ("Unknown option \"%s\"", argv[i]);
		else
			break;
	}

	if (i != argc - 1)
	{
		Error ("%s",
"usage: hmap2 -vis [options] bspfile\n"
"Compiles visibility data in a .bsp, needs a .prt file\n"
"\n"
"What the options do:\n"
"-level 0-4      quality, default 4\n"
"-fast           fast but bad quality vis\n"
"-v              verbose\n"
"-norvis         disable rvis optimization, 0.001% better quality and 30% slower\n"
"-ambientslime   do not convert slime channel to water (requires engine support)\n"
"-noambient      disable ambient sounds (water bubbling, wind, etc)\n"
"-noambientwater disable ambient water sounds (water)\n"
"-noambientslime disable ambient slime sounds (water, or -ambientslime)\n"
"-noambientlava  disable ambient lava sounds (unused by quake)\n"
"-noambientsky   disable ambient sky sounds (wind)\n"
"-noreuse        disable merging of identical vis data (less compression)\n"
"-farplane       limit visible distance (warning: not a good idea without fog)\n"
		);
	}

	// init memory
	Q_InitMem ();

	start = I_DoubleTime ();

	LoadBSPFile (filename_bsp);
	LoadPortals (filename_prt);

	uncompressed = qmalloc(bitlongs*portalleafs*sizeof(long));
	memset (uncompressed, 0, bitlongs*portalleafs*sizeof(long));

//	CalcPassages ();

	CalcVis ();

	printf ("row size: %i\n",bitbytes);
	printf ("c_reused: %i\n",c_reused);
	printf ("c_chains: %i\n",c_chains);

	visdatasize = vismap_p - dvisdata;
	printf ("reused bytes: %i\n",c_reused*bitbytes);
	printf ("visdatasize:%i compressed from %i\n", visdatasize, originalvismapsize);

	if (!noambient)
		CalcAmbientSounds ();

	WriteBSPFile (filename_bsp, false);

//	unlink (portalfile);

	end = I_DoubleTime ();
	printf ("%5.2f seconds elapsed\n\n", end-start);

	// print memory stats
	Q_PrintMem ();

	// free allocated memory
	Q_ShutdownMem ();

	return 0;
}

