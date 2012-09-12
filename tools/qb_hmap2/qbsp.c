// bsp5.c

#include "bsp5.h"
#include "threads.h"

//
// command line flags
//
qboolean nofill;
qboolean notjunc;
qboolean noclip;
qboolean onlyents;
qboolean verbose;
qboolean allverbose;
qboolean forcevis;
qboolean waterlightmap;
qboolean option_solidbmodels;

int		subdivide_size;

int		valid;

char	filename_map[1024];
char	filename_bsp[1024];
char	filename_prt[1024];
char	filename_pts[1024];
char	filename_lin[1024];
char	filename_lit[1024];
char	filename_dlit[1024];
char	filename_lights[1024];

int		Vis_Main( int argc, char **argv );
int		Light_Main( int argc, char **argv );
int		Bsp2Prt_Main( int argc, char **argv );
int		BspInfo_Main( int argc, char **argv );

//===========================================================================

void qprintf( char *fmt, ... )
{
	va_list argptr;
	extern qboolean verbose;

	if( !verbose )
		return;		// only print if verbose

	va_start( argptr, fmt );
	vprintf( fmt, argptr );
	va_end( argptr );
}

//===========================================================================


/*
===============
ProcessEntity
===============
*/
void ProcessEntity (int entnum, int modnum, int hullnum)
{
	entity_t	*ent;
	tree_t		*tree;

	ent = &entities[entnum];
	if (!ent->brushes)
		return;

	tree = Tree_ProcessEntity (ent, modnum, hullnum);
	EmitNodePlanes (tree->headnode);

	if (hullnum != 0)
		EmitClipNodes (tree->headnode, modnum, hullnum);
	else
	{
		EmitNodeFaces (tree->headnode);
		EmitDrawNodes (tree->headnode);
	}

	FreeTree (tree);
}

/*
=================
UpdateEntLump
=================
*/
void UpdateEntLump (void)
{
	int		m, entnum;
	char	mod[80];

	m = 1;
	for (entnum = 1 ; entnum < num_entities ; entnum++)
    {
		if (!entities[entnum].brushes)
			continue;
		sprintf (mod, "*%i", m);
		SetKeyValue (&entities[entnum], "model", mod);
		m++;
    }

	UnparseEntities();
}

/*
=================
CreateSingleHull
=================
*/
void CreateSingleHull (int hullnum)
{
	int			entnum;
	int			modnum;

	// for each entity in the map file that has geometry
	verbose = true;	// print world
	for (entnum = 0, modnum = 0; entnum < num_entities; entnum++)
	{
		if (!entities[entnum].brushes)
			continue;

		ProcessEntity (entnum, modnum++, hullnum);
		if (!allverbose)
			verbose = false;	// don't print rest of entities
	}
}

/*
=================
CreateHulls
=================
*/
void CreateHulls (void)
{
	int	i;

// hull 0 is always point-sized
	VectorClear (hullinfo.hullsizes[0][0]);
	VectorClear (hullinfo.hullsizes[0][1]);
	CreateSingleHull (0);

	// commanded to ignore the hulls altogether
	if (noclip)
    {
		hullinfo.numhulls = 1;
		hullinfo.filehulls = (ismcbsp ? 1 : 4);
		return;
    }

	// create the hulls sequentially
	printf ("building hulls sequentially...\n");

	// get the hull sizes
	if (ismcbsp)
	{
		entity_t	*world;
		char		keymins[16], keymaxs[16];
		vec3_t		v;
		int			i;

	// read hull values from _hull# fields in worldspawn
		world = FindEntityWithKeyPair ("classname", "worldspawn");

		for (hullinfo.numhulls = 1; hullinfo.numhulls < MAX_MAP_HULLS; hullinfo.numhulls++)
		{
			sprintf (keymins, "_hull%d_mins", hullinfo.numhulls);
			sprintf (keymaxs, "_hull%d_maxs", hullinfo.numhulls);

			if ((ValueForKey(world, keymins))[0] && (ValueForKey(world, keymaxs))[0])
			{
				GetVectorForKey (world, keymins, v);
				VectorCopy (v, hullinfo.hullsizes[hullinfo.numhulls][0]);
				GetVectorForKey (world, keymaxs, v);
				VectorCopy (v, hullinfo.hullsizes[hullinfo.numhulls][1]);
			}
			else
				break;
		}
		hullinfo.filehulls = hullinfo.numhulls;

		printf ("Map has %d hulls:\n", hullinfo.numhulls);
		for (i = 0; i < hullinfo.numhulls; i++)
			printf ("%2d: %.1f %.1f %.1f, %.1f %.1f %.1f\n", i, hullinfo.hullsizes[i][0][0], hullinfo.hullsizes[i][0][1], hullinfo.hullsizes[i][0][2], hullinfo.hullsizes[i][1][0], hullinfo.hullsizes[i][1][1], hullinfo.hullsizes[i][1][2]);
	}
	else
	{
		hullinfo.numhulls = 3;
		hullinfo.filehulls = 4;
		VectorSet (hullinfo.hullsizes[1][0], -16, -16, -24);
		VectorSet (hullinfo.hullsizes[1][1], 16, 16, 32);
		VectorSet (hullinfo.hullsizes[2][0], -32, -32, -24);
		VectorSet (hullinfo.hullsizes[2][1], 32, 32, 64);
	}

	for (i = 1; i < hullinfo.numhulls; i++)
		CreateSingleHull (i);
}

/*
=================
ProcessFile
=================
*/
void ProcessFile (char *sourcebase, char *filename_bsp1)
{
	if (!onlyents)
	{
		remove (filename_bsp);
		remove (filename_prt);
		remove (filename_pts);
		remove (filename_lin);
		remove (filename_lit);
		remove (filename_dlit);
		remove (filename_lights);
	}

	// load brushes and entities
	LoadMapFile (sourcebase);
	if (onlyents)
	{
		LoadBSPFile (filename_bsp);
		UpdateEntLump ();
		WriteBSPFile (filename_bsp, false);
		return;
	}

	// init the tables to be shared by all models
	BeginBSPFile ();

	// the clipping hulls will be written out to text files by forked processes
	CreateHulls ();

	UpdateEntLump ();

	WriteMiptex ();

	FinishBSPFile ();
}


/*
==================
main

==================
*/
int main (int argc, char **argv)
{
	int		i;
	double	start, end;

	myargc = argc;
	myargv = argv;

	//	malloc_debug (15);
	printf( "hmap2 by LordHavoc and Vic\n");
	printf( "based on id Software's quake qbsp, light and vis utilities source code\n" );
	printf( "\n" );

	//
	// check command line flags
	//

	if (argc == 1)
		goto error;

	// create all the filenames pertaining to this map
	strcpy(filename_map, argv[argc-1]);ReplaceExtension(filename_map, ".bsp", ".map", ".map");
	strcpy(filename_bsp, filename_map);ReplaceExtension(filename_bsp, ".map", ".bsp", ".bsp");
	strcpy(filename_prt, filename_bsp);ReplaceExtension(filename_prt, ".bsp", ".prt", ".prt");
	strcpy(filename_pts, filename_bsp);ReplaceExtension(filename_pts, ".bsp", ".pts", ".pts");
	strcpy(filename_lin, filename_bsp);ReplaceExtension(filename_lin, ".bsp", ".lin", ".lin");
	strcpy(filename_lit, filename_bsp);ReplaceExtension(filename_lit, ".bsp", ".lit", ".lit");
	strcpy(filename_dlit, filename_bsp);ReplaceExtension(filename_dlit, ".bsp", ".dlit", ".dlit");
	strcpy(filename_lights, filename_bsp);ReplaceExtension(filename_lights, ".bsp", ".lights", ".lights");

	if (!strcmp(filename_map, filename_bsp))
		Error("filename_map \"%s\" == filename_bsp \"%s\"\n", filename_map, filename_bsp);

	for (i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i],"-threads"))
		{
			i++;
			if( i >= argc )
				Error( "no value was given to -numthreads\n" );
			numthreads = atoi (argv[i]);
		}
	}

	ThreadSetDefault ();

	i = 1;
	ismcbsp = false;
	if (!strcmp (argv[i], "-mc"))
	{
		printf ("Using Martial Concert bsp format\n");
		ismcbsp = true;
		i++;
	}

	if (argc == i)
		goto error;

	if (!strcmp (argv[i], "-bsp2prt"))
		return Bsp2Prt_Main (argc-i, argv+i);
	else if (!strcmp (argv[i], "-bspinfo"))
		return BspInfo_Main (argc-i, argv+i);
	else if (!strcmp (argv[i], "-vis"))
		return Vis_Main (argc-i, argv+i);
	else if (!strcmp (argv[i], "-light"))
		return Light_Main (argc-i, argv+i);

	nofill = false;
	notjunc = false;
	noclip = false;
	onlyents = false;
	verbose = true;
	allverbose = false;
	transwater = true;
	forcevis = true;
	waterlightmap = true;
	subdivide_size = 240;
	option_solidbmodels = false;

	for (; i < argc; i++)
	{
		if (argv[i][0] != '-')
			break;
		else if (!strcmp (argv[i],"-nowater"))
			transwater = false;
		else if (!strcmp (argv[i],"-notjunc"))
			notjunc = true;
		else if (!strcmp (argv[i],"-nofill"))
			nofill = true;
		else if (!strcmp (argv[i],"-noclip"))
			noclip = true;
		else if (!strcmp (argv[i],"-onlyents"))
			onlyents = true;
		else if (!strcmp (argv[i],"-verbose"))
			allverbose = true;
		else if (!strcmp (argv[i],"-nowaterlightmap"))
			waterlightmap = false;
		else if (!strcmp (argv[i],"-subdivide"))
		{
			subdivide_size = atoi(argv[i+1]);
			i++;
		}
		else if (!strcmp (argv[i],"-darkplaces"))
		{
			// produce 256x256 texel lightmaps
			subdivide_size = 4080;
		}
		else if (!strcmp (argv[i],"-noforcevis"))
			forcevis = false;
		else if (!strcmp (argv[i],"-solidbmodels"))
			option_solidbmodels = true;
		else if (!strcmp (argv[i],"-wadpath"))
			i++; // handled later in wad lookups
		else
			Error ("Unknown option '%s'", argv[i]);
	}

	if (i != argc - 1)
error:
		Error ("%s",
"usage: hmap2 [options] sourcefile\n"
"Compiles .map to .bsp, does not compile vis or lighting data\n"
"\n"
"other utilities available:\n"
"-bsp2prt    bsp2prt utility, run -bsp2prt as the first parameter for more\n"
"-bspinfo    bspinfo utility, run -bspinfo as the first parameter for more\n"
"-light      lighting utility, run -light as the first parameter for more\n"
"-vis        vis utility, run -vis as the first parameter for more\n"
"\n"
"What the options do:\n"
"-nowater    disable watervis; r_wateralpha in glquake will not work right\n"
"-notjunc    disable tjunction fixing; glquake will have holes between polygons\n"
"-nofill     disable sealing of map and vis, used for ammoboxes\n"
"-onlyents   patchs entities in existing .bsp, for relighting\n"
"-verbose    show more messages\n"
"-darkplaces allow really big polygons\n"
"-noforcevis don't make a .prt if the map leaks\n"
"-nowaterlightmap disable darkplaces lightmapped water feature\n"
"-notex      store blank textures instead of real ones, smaller bsp if zipped\n"
"-solidbmodels use qbsp behavior of making water/sky submodels solid\n"
		);

	printf("inputfile: %s\n", filename_map);
	printf("outputfile: %s\n", filename_bsp);

	// init memory
	Q_InitMem ();

	//
	// do it!
	//
	start = I_DoubleTime ();
	ProcessFile (filename_map, filename_bsp);
	end = I_DoubleTime ();
	printf ("%5.1f seconds elapsed\n\n", end-start);

	// print memory stats
	Q_PrintMem ();

	// free allocated memory
	Q_ShutdownMem ();

	return 0;
}
