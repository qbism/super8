#include "bsp5.h"

/*
==================
BspInfo_PrintInfo
==================
*/
static void BspInfo_PrintInfo( const char *filename )
{
	int length;
	FILE *file;

	// get file length
	file = SafeOpenRead( filename_bsp );
	length = Q_filelength( file );
	fclose( file );

	// print file length
	printf( "%s: %i bytes\n", filename_bsp, length );
	printf( "\n" );

	LoadBSPFile( filename_bsp );

	// print .bsp info
	PrintBSPFileSizes ();
}

/*
==================
BspInfo_Main
==================
*/
int BspInfo_Main( int argc, char **argv )
{
	if( argc < 2 ) {
		Error ("%s",
"usage: hmap2 -bspinfo bspfile\n"
"Prints information about a .bsp file\n"
		);
	}

	// init memory
	Q_InitMem ();

	BspInfo_PrintInfo( argv[argc-1] );

	// free allocated memory
	Q_ShutdownMem ();

	return 0;
}
