
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "mem.h"

//=============================================================================

int			nummodels;
dmodel_t	dmodels[MAX_MAP_MODELS];

int			visdatasize;
byte		dvisdata[MAX_MAP_VISIBILITY];

int			lightdatasize;
byte		dlightdata[MAX_MAP_LIGHTING];

// LordHavoc: stored in .lit file
int			rgblightdatasize;
byte		drgblightdata[MAX_MAP_LIGHTING*3];

// LordHavoc: stored in .dlit file
int			nmaplightdatasize;
byte		dnmaplightdata[MAX_MAP_LIGHTING*3];


int			texdatasize;
byte		dtexdata[MAX_MAP_MIPTEX]; // (dmiptexlump_t)

int			entdatasize;
char		dentdata[MAX_MAP_ENTSTRING];

int			numleafs;
dleaf_t		dleafs[MAX_MAP_LEAFS];

int			numplanes;
dplane_t	dplanes[MAX_MAP_PLANES];

int			numvertexes;
dvertex_t	dvertexes[MAX_MAP_VERTS];

int			numnodes;
dnode_t		dnodes[MAX_MAP_NODES];

int			numtexinfo;
texinfo_t	texinfo[MAX_MAP_TEXINFO];

int			numfaces;
dface_t		dfaces[MAX_MAP_FACES];

int			numclipnodes;
dclipnode_t	dclipnodes[MAX_MAP_CLIPNODES];

int			numedges;
dedge_t		dedges[MAX_MAP_EDGES];

int			nummarksurfaces;
// MODIFIED FOR BSP2
unsigned int	dmarksurfaces[MAX_MAP_MARKSURFACES];

int			numsurfedges;
int			dsurfedges[MAX_MAP_SURFEDGES];

hullinfo_t	hullinfo;

qboolean	ismcbsp;

//=============================================================================

typedef struct
{
	int		initialized;
	int		maxsize;
	byte	*start;
	byte	*index;
} swappedbuffer_t;

void SB_LoadFile (swappedbuffer_t *sbuf, char *filename)
{
	FILE	*f;

	f = SafeOpenRead (filename);
	sbuf->maxsize = Q_filelength (f);
	sbuf->start = qmalloc (sbuf->maxsize + 1);
	sbuf->start[sbuf->maxsize] = 0;
	SafeRead (f, sbuf->start, sbuf->maxsize);
	fclose (f);

	sbuf->index = sbuf->start;
	sbuf->initialized = true;
}

void SB_Alloc (swappedbuffer_t *sbuf, int maxsize)
{
	sbuf->maxsize = maxsize;
	sbuf->start = qmalloc (maxsize);
	sbuf->index = sbuf->start;
	sbuf->initialized = true;
}

void SB_Free (swappedbuffer_t *sbuf)
{
	if (!sbuf->initialized)
		return;

	qfree (sbuf->start);
	memset (sbuf, 0, sizeof(*sbuf));
}

byte SB_ReadByte (swappedbuffer_t *sbuf)
{
	byte b;
	b = *sbuf->index++;
	return b;
}

short SB_ReadShort (swappedbuffer_t *sbuf)
{
	short s;
	s = sbuf->index[0] + 256*sbuf->index[1];
	sbuf->index += 2;
	return s;
}

int SB_ReadInt (swappedbuffer_t *sbuf)
{
	int i;
	i = sbuf->index[0] + 256*sbuf->index[1] + 65536*sbuf->index[2] + 16777216*sbuf->index[3];
	sbuf->index += 4;
	return i;
}

float SB_ReadFloat (swappedbuffer_t *sbuf)
{
	union
	{
		int		i;
		float	f;
	} u;

	u.i = SB_ReadInt (sbuf);
	return u.f;
}

void SB_ReadData (swappedbuffer_t *sbuf, void *d, int n)
{
	memcpy (d, sbuf->index, n);
	sbuf->index += n;
}

void SB_ZeroFill (swappedbuffer_t *sbuf, int n)
{
	while (n--)
		*sbuf->index++ = 0;
}

void SB_WriteByte (swappedbuffer_t *sbuf, byte b)
{
	*sbuf->index++ = b;
}

void SB_WriteShort (swappedbuffer_t *sbuf, short i)
{
	sbuf->index[0] = i & 0xFF;
	sbuf->index[1] = (i >> 8) & 0xFF;
	sbuf->index += 2;
}

void SB_WriteInt (swappedbuffer_t *sbuf, int i)
{
	sbuf->index[0] = i & 0xFF;
	sbuf->index[1] = (i >> 8) & 0xFF;
	sbuf->index[2] = (i >> 16) & 0xFF;
	sbuf->index[3] = (i >> 24) & 0xFF;
	sbuf->index += 4;
}

void SB_WriteFloat (swappedbuffer_t *sbuf, float f)
{
	union
	{
		int		i;
		float	f;
	} u;

	u.f = f;
	SB_WriteInt (sbuf, u.i);
}

void SB_WriteData (swappedbuffer_t *sbuf, void *d, int n)
{
	memcpy (sbuf->index, d, n);
	sbuf->index += n;
}

void SB_WriteString (swappedbuffer_t *sbuf, char *s)
{
	while (*s)
		*sbuf->index++ = *s++;
	*sbuf->index++ = 0;
}

void SB_SeekAbsolute (swappedbuffer_t *sbuf, int index)
{
	sbuf->index = sbuf->start + index;
}

int SB_Tell (swappedbuffer_t *sbuf)
{
	return sbuf->index - sbuf->start;
}

/*
=============
LoadBSPFile
=============
*/
void	LoadBSPFile (char *filename)
{
	int				i, j;
	swappedbuffer_t	sb;
	lump_t			lumps[HEADER_LUMPS], *lump;
	qboolean isbsp2 = false;
	ismcbsp = false;

// load file into buffer
	SB_LoadFile(&sb, filename);

// check header
	if (!memcmp(sb.index, "MCBSPpad", 8))
	{
		ismcbsp = true;
		SB_SeekAbsolute(&sb, 8);
		i = SB_ReadInt (&sb);
		if (i != MCBSPVERSION)
			Error ("%s is version %i, should be %i", filename, i, MCBSPVERSION);
	}
	else if (!memcmp(sb.index, "BSP2", 4))
	{
		isbsp2 = true;
		SB_SeekAbsolute(&sb, 4);
	}
	else
	{
		i = SB_ReadInt( &sb);
		if (i != BSPVERSION)
			Error ("%s is version %i, should be %i", filename, i, BSPVERSION);
	}

// hull 0 is always point-sized
	VectorClear (hullinfo.hullsizes[0][0]);
	VectorClear (hullinfo.hullsizes[0][1]);

	if (ismcbsp)
	{
		hullinfo.numhulls = SB_ReadInt (&sb);
		hullinfo.filehulls = hullinfo.numhulls;
		for (i = 1; i < hullinfo.numhulls; i++)
		{
			hullinfo.hullsizes[i][0][0] = SB_ReadFloat (&sb);
			hullinfo.hullsizes[i][0][1] = SB_ReadFloat (&sb);
			hullinfo.hullsizes[i][0][2] = SB_ReadFloat (&sb);
			hullinfo.hullsizes[i][1][0] = SB_ReadFloat (&sb);
			hullinfo.hullsizes[i][1][1] = SB_ReadFloat (&sb);
			hullinfo.hullsizes[i][1][2] = SB_ReadFloat (&sb);
		}
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

	for (i = 0; i < HEADER_LUMPS; i++)
	{
		lumps[i].fileofs = SB_ReadInt (&sb);
		lumps[i].filelen = SB_ReadInt (&sb);
	}

// read lumps (sigh...)
	lump = &lumps[LUMP_PLANES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	numplanes = lump->filelen / 20;
	for (i = 0; i < numplanes; i++)
	{
		dplanes[i].normal[0] = SB_ReadFloat (&sb);
		dplanes[i].normal[1] = SB_ReadFloat (&sb);
		dplanes[i].normal[2] = SB_ReadFloat (&sb);
		dplanes[i].dist = SB_ReadFloat (&sb);
		dplanes[i].type = SB_ReadInt (&sb);
	}

	lump = &lumps[LUMP_LEAFS];
	SB_SeekAbsolute (&sb, lump->fileofs);
	if (isbsp2)
	{
		numleafs = lump->filelen / 44;
		for (i = 0; i < numleafs; i++)
		{
			dleafs[i].contents = SB_ReadInt (&sb);
			dleafs[i].visofs = SB_ReadInt (&sb);
			dleafs[i].mins[0] = SB_ReadFloat (&sb);
			dleafs[i].mins[1] = SB_ReadFloat (&sb);
			dleafs[i].mins[2] = SB_ReadFloat (&sb);
			dleafs[i].maxs[0] = SB_ReadFloat (&sb);
			dleafs[i].maxs[1] = SB_ReadFloat (&sb);
			dleafs[i].maxs[2] = SB_ReadFloat (&sb);
			dleafs[i].firstmarksurface = SB_ReadInt (&sb);
			dleafs[i].nummarksurfaces = SB_ReadInt (&sb);
			for (j = 0; j < NUM_AMBIENTS; j++)
				dleafs[i].ambient_level[j] = SB_ReadByte (&sb);
		}
	}
	else
	{
		numleafs = lump->filelen / 28;
		for (i = 0; i < numleafs; i++)
		{
			dleafs[i].contents = SB_ReadInt (&sb);
			dleafs[i].visofs = SB_ReadInt (&sb);
			dleafs[i].mins[0] = SB_ReadShort (&sb);
			dleafs[i].mins[1] = SB_ReadShort (&sb);
			dleafs[i].mins[2] = SB_ReadShort (&sb);
			dleafs[i].maxs[0] = SB_ReadShort (&sb);
			dleafs[i].maxs[1] = SB_ReadShort (&sb);
			dleafs[i].maxs[2] = SB_ReadShort (&sb);
			dleafs[i].firstmarksurface = SB_ReadShort (&sb);
			dleafs[i].nummarksurfaces = SB_ReadShort (&sb);
			for (j = 0; j < NUM_AMBIENTS; j++)
				dleafs[i].ambient_level[j] = SB_ReadByte (&sb);
		}
	}

	lump = &lumps[LUMP_VERTEXES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	numvertexes = lump->filelen / 12;
	for (i = 0; i < numvertexes; i++)
	{
		dvertexes[i].point[0] = SB_ReadFloat (&sb);
		dvertexes[i].point[1] = SB_ReadFloat (&sb);
		dvertexes[i].point[2] = SB_ReadFloat (&sb);
	}

	lump = &lumps[LUMP_NODES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	if (isbsp2)
	{
		numnodes = lump->filelen / 44;
		for (i = 0; i < numnodes; i++)
		{
			dnodes[i].planenum = SB_ReadInt (&sb);
			dnodes[i].children[0] = SB_ReadInt (&sb);
			dnodes[i].children[1] = SB_ReadInt (&sb);
			dnodes[i].mins[0] = SB_ReadFloat (&sb);
			dnodes[i].mins[1] = SB_ReadFloat (&sb);
			dnodes[i].mins[2] = SB_ReadFloat (&sb);
			dnodes[i].maxs[0] = SB_ReadFloat (&sb);
			dnodes[i].maxs[1] = SB_ReadFloat (&sb);
			dnodes[i].maxs[2] = SB_ReadFloat (&sb);
			dnodes[i].firstface = SB_ReadInt (&sb);
			dnodes[i].numfaces = SB_ReadInt (&sb);
		}
	}
	else
	{
		numnodes = lump->filelen / 24;
		for (i = 0; i < numnodes; i++)
		{
			dnodes[i].planenum = SB_ReadInt (&sb);
			dnodes[i].children[0] = SB_ReadShort (&sb);
			dnodes[i].children[1] = SB_ReadShort (&sb);
			dnodes[i].mins[0] = SB_ReadShort (&sb);
			dnodes[i].mins[1] = SB_ReadShort (&sb);
			dnodes[i].mins[2] = SB_ReadShort (&sb);
			dnodes[i].maxs[0] = SB_ReadShort (&sb);
			dnodes[i].maxs[1] = SB_ReadShort (&sb);
			dnodes[i].maxs[2] = SB_ReadShort (&sb);
			dnodes[i].firstface = SB_ReadShort (&sb);
			dnodes[i].numfaces = SB_ReadShort (&sb);
		}
	}

	lump = &lumps[LUMP_TEXINFO];
	SB_SeekAbsolute (&sb, lump->fileofs);
	numtexinfo = lump->filelen / 40;
	for (i = 0; i < numtexinfo; i++)
	{
		texinfo[i].vecs[0][0] = SB_ReadFloat (&sb);
		texinfo[i].vecs[0][1] = SB_ReadFloat (&sb);
		texinfo[i].vecs[0][2] = SB_ReadFloat (&sb);
		texinfo[i].vecs[0][3] = SB_ReadFloat (&sb);
		texinfo[i].vecs[1][0] = SB_ReadFloat (&sb);
		texinfo[i].vecs[1][1] = SB_ReadFloat (&sb);
		texinfo[i].vecs[1][2] = SB_ReadFloat (&sb);
		texinfo[i].vecs[1][3] = SB_ReadFloat (&sb);
		texinfo[i].miptex = SB_ReadInt (&sb);
		texinfo[i].flags = SB_ReadInt (&sb);
	}

	lump = &lumps[LUMP_FACES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	if (isbsp2)
	{
		numfaces = lump->filelen / 28;
		for (i = 0; i < numfaces; i++)
		{
			dfaces[i].planenum = SB_ReadInt (&sb);
			dfaces[i].side = SB_ReadInt (&sb);
			dfaces[i].firstedge = SB_ReadInt (&sb);
			dfaces[i].numedges = SB_ReadInt (&sb);
			dfaces[i].texinfo = SB_ReadInt (&sb);
			for (j = 0; j < MAXLIGHTMAPS; j++)
				dfaces[i].styles[j] = SB_ReadByte (&sb);
			dfaces[i].lightofs = SB_ReadInt (&sb);
		}
	}
	else
	{
		numfaces = lump->filelen / 20;
		for (i = 0; i < numfaces; i++)
		{
			dfaces[i].planenum = SB_ReadShort (&sb);
			dfaces[i].side = SB_ReadShort (&sb);
			dfaces[i].firstedge = SB_ReadInt (&sb);
			dfaces[i].numedges = SB_ReadShort (&sb);
			dfaces[i].texinfo = SB_ReadShort (&sb);
			for (j = 0; j < MAXLIGHTMAPS; j++)
				dfaces[i].styles[j] = SB_ReadByte (&sb);
			dfaces[i].lightofs = SB_ReadInt (&sb);
		}
	}

	lump = &lumps[LUMP_CLIPNODES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	if (isbsp2)
	{
		numclipnodes = lump->filelen / 12;
		for (i = 0; i < numclipnodes; i++)
		{
			dclipnodes[i].planenum = SB_ReadInt (&sb);
			dclipnodes[i].children[0] = SB_ReadInt (&sb);
			dclipnodes[i].children[1] = SB_ReadInt (&sb);
		}
	}
	else
	{
		numclipnodes = lump->filelen / 8;
		for (i = 0; i < numclipnodes; i++)
		{
			dclipnodes[i].planenum = SB_ReadInt (&sb);
			dclipnodes[i].children[0] = SB_ReadShort (&sb);
			dclipnodes[i].children[1] = SB_ReadShort (&sb);
		}
	}

	lump = &lumps[LUMP_MARKSURFACES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	if (isbsp2)
	{
		nummarksurfaces = lump->filelen / 4;
		for (i = 0; i < nummarksurfaces; i++)
			dmarksurfaces[i] = SB_ReadInt (&sb);
	}
	else
	{
		nummarksurfaces = lump->filelen / 2;
		for (i = 0; i < nummarksurfaces; i++)
			dmarksurfaces[i] = SB_ReadShort (&sb);
	}

	lump = &lumps[LUMP_SURFEDGES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	numsurfedges = lump->filelen / 4;
	for (i = 0; i < numsurfedges; i++)
		dsurfedges[i] = SB_ReadInt (&sb);

	lump = &lumps[LUMP_EDGES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	if (isbsp2)
	{
		numedges = lump->filelen / 8;
		for (i = 0; i < numedges; i++)
		{
			dedges[i].v[0] = SB_ReadInt (&sb);
			dedges[i].v[1] = SB_ReadInt (&sb);
		}
	}
	else
	{
		numedges = lump->filelen / 4;
		for (i = 0; i < numedges; i++)
		{
			dedges[i].v[0] = SB_ReadShort (&sb);
			dedges[i].v[1] = SB_ReadShort (&sb);
		}
	}

	lump = &lumps[LUMP_MODELS];
	SB_SeekAbsolute (&sb, lump->fileofs);
	nummodels = lump->filelen / (48+4*hullinfo.filehulls);
	for (i = 0; i < nummodels; i++)
	{
		dmodels[i].mins[0] = SB_ReadFloat (&sb);
		dmodels[i].mins[1] = SB_ReadFloat (&sb);
		dmodels[i].mins[2] = SB_ReadFloat (&sb);
		dmodels[i].maxs[0] = SB_ReadFloat (&sb);
		dmodels[i].maxs[1] = SB_ReadFloat (&sb);
		dmodels[i].maxs[2] = SB_ReadFloat (&sb);
		dmodels[i].origin[0] = SB_ReadFloat (&sb);
		dmodels[i].origin[1] = SB_ReadFloat (&sb);
		dmodels[i].origin[2] = SB_ReadFloat (&sb);
		for (j = 0; j < hullinfo.filehulls; j++)
			dmodels[i].headnode[j] = SB_ReadInt (&sb);
		dmodels[i].visleafs = SB_ReadInt (&sb);
		dmodels[i].firstface = SB_ReadInt (&sb);
		dmodels[i].numfaces = SB_ReadInt (&sb);
	}

	lump = &lumps[LUMP_LIGHTING];
	SB_SeekAbsolute (&sb, lump->fileofs);
	if (ismcbsp)
	{
		rgblightdatasize = lump->filelen;
		SB_ReadData (&sb, drgblightdata, rgblightdatasize);
	}
	else
	{
		lightdatasize = lump->filelen;
		SB_ReadData (&sb, dlightdata, lightdatasize);
	}

	lump = &lumps[LUMP_VISIBILITY];
	SB_SeekAbsolute (&sb, lump->fileofs);
	visdatasize = lump->filelen;
	SB_ReadData (&sb, dvisdata, visdatasize);

	lump = &lumps[LUMP_ENTITIES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	entdatasize = lump->filelen;
	SB_ReadData (&sb, dentdata, entdatasize);

	lump = &lumps[LUMP_TEXTURES];
	SB_SeekAbsolute (&sb, lump->fileofs);
	texdatasize = lump->filelen;
	SB_ReadData (&sb, dtexdata, texdatasize);

// finish up
	SB_Free(&sb);
}

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void WriteBSPFile (char *filename, qboolean litonly)
{
	int				i, j;
	FILE			*f;
	swappedbuffer_t	sb;
	qboolean		isbsp2
	 =  dmodels[0].mins[0] < -32768.0f
	 || dmodels[0].mins[1] < -32768.0f
	 || dmodels[0].mins[2] < -32768.0f
	 || dmodels[0].maxs[0] >= 32768.0f
	 || dmodels[0].maxs[1] >= 32768.0f
	 || dmodels[0].maxs[2] >= 32768.0f
	 || nummarksurfaces >= 32768
	 || numvertexes > 32768
	 || numnodes >= 32768
	 || numleafs >= 32768
	 || numplanes >= 32768
	 || numtexinfo >= 32768
	 || numclipnodes >= 32768;

	if (!litonly)
	{
		int		index;
		int		bspsize;
		lump_t	lumps[HEADER_LUMPS], *lump;

	// allocate as much memory is needed for the buffer -- sorry about this! Please do something about this!
		if (ismcbsp)
		{
			bspsize = 8+4+4+(hullinfo.numhulls-1)*24;
			bspsize += sizeof(lumps)+20*numplanes+(24+NUM_AMBIENTS)*numleafs+12*numvertexes;
			bspsize += 24*numnodes+40*numtexinfo+(16+MAXLIGHTMAPS)*numfaces+8*numclipnodes;
			bspsize += 2*nummarksurfaces+4*numsurfedges+4*numedges;
			bspsize += (48+4*MAX_MAP_HULLS)*nummodels+rgblightdatasize;
			bspsize += visdatasize+entdatasize+texdatasize;
		}
		else if (isbsp2)
		{
			bspsize = 4;
			bspsize += sizeof(lumps)+20*numplanes+(40+NUM_AMBIENTS)*numleafs+12*numvertexes;
			bspsize += 44*numnodes+40*numtexinfo+(24+MAXLIGHTMAPS)*numfaces+12*numclipnodes;
			bspsize += 4*nummarksurfaces+4*numsurfedges+8*numedges;
			bspsize += (48+4*MAX_Q1MAP_HULLS)*nummodels+lightdatasize;
			bspsize += visdatasize+entdatasize+texdatasize;
		}
		else
		{
			bspsize = 4;
			bspsize += sizeof(lumps)+20*numplanes+(24+NUM_AMBIENTS)*numleafs+12*numvertexes;
			bspsize += 24*numnodes+40*numtexinfo+(16+MAXLIGHTMAPS)*numfaces+8*numclipnodes;
			bspsize += 2*nummarksurfaces+4*numsurfedges+4*numedges;
			bspsize += (48+4*MAX_Q1MAP_HULLS)*nummodels+lightdatasize;
			bspsize += visdatasize+entdatasize+texdatasize;
		}
		bspsize += 512;	// extra case for safety and to compensate for the 4-byte padding of the lumps

		SB_Alloc (&sb, bspsize);
		printf ("Allocated %f MB (%d bytes) for file buffer\n", bspsize*(1.0f/(1024.0f*1024.0f)), bspsize);

	// write header
		if (ismcbsp)
			SB_ZeroFill (&sb, 8+4+4+(hullinfo.numhulls - 1)*24);	// filled in later
		else if (isbsp2)
			SB_ZeroFill (&sb, 4); // filled in later
		else
			SB_ZeroFill (&sb, 4); // filled in later

		SB_ZeroFill (&sb, sizeof(lumps));	// filled in later

	// write lumps and pad each one to a multiple of 4 bytes
		lump = &lumps[LUMP_PLANES];
		lump->fileofs = SB_Tell(&sb);
		for (i = 0; i < numplanes; i++)
		{
			SB_WriteFloat (&sb, dplanes[i].normal[0]);
			SB_WriteFloat (&sb, dplanes[i].normal[1]);
			SB_WriteFloat (&sb, dplanes[i].normal[2]);
			SB_WriteFloat (&sb, dplanes[i].dist);
			SB_WriteInt (&sb, dplanes[i].type);
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_LEAFS];
		lump->fileofs = SB_Tell(&sb);
		if (isbsp2)
		{
			for (i = 0; i < numleafs; i++)
			{
				SB_WriteInt (&sb, dleafs[i].contents);
				SB_WriteInt (&sb, dleafs[i].visofs);
				SB_WriteFloat (&sb, dleafs[i].mins[0]);
				SB_WriteFloat (&sb, dleafs[i].mins[1]);
				SB_WriteFloat (&sb, dleafs[i].mins[2]);
				SB_WriteFloat (&sb, dleafs[i].maxs[0]);
				SB_WriteFloat (&sb, dleafs[i].maxs[1]);
				SB_WriteFloat (&sb, dleafs[i].maxs[2]);
				SB_WriteInt (&sb, dleafs[i].firstmarksurface);
				SB_WriteInt (&sb, dleafs[i].nummarksurfaces);
				for (j = 0; j < NUM_AMBIENTS; j++)
					SB_WriteByte (&sb, dleafs[i].ambient_level[j]);
			}
		}
		else
		{
			for (i = 0; i < numleafs; i++)
			{
				SB_WriteInt (&sb, dleafs[i].contents);
				SB_WriteInt (&sb, dleafs[i].visofs);
				SB_WriteShort (&sb, dleafs[i].mins[0]);
				SB_WriteShort (&sb, dleafs[i].mins[1]);
				SB_WriteShort (&sb, dleafs[i].mins[2]);
				SB_WriteShort (&sb, dleafs[i].maxs[0]);
				SB_WriteShort (&sb, dleafs[i].maxs[1]);
				SB_WriteShort (&sb, dleafs[i].maxs[2]);
				SB_WriteShort (&sb, dleafs[i].firstmarksurface);
				SB_WriteShort (&sb, dleafs[i].nummarksurfaces);
				for (j = 0; j < NUM_AMBIENTS; j++)
					SB_WriteByte (&sb, dleafs[i].ambient_level[j]);
			}
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_VERTEXES];
		lump->fileofs = SB_Tell(&sb);
		for (i = 0; i < numvertexes; i++)
		{
			SB_WriteFloat (&sb, dvertexes[i].point[0]);
			SB_WriteFloat (&sb, dvertexes[i].point[1]);
			SB_WriteFloat (&sb, dvertexes[i].point[2]);
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_NODES];
		lump->fileofs = SB_Tell(&sb);
		if (isbsp2)
		{
			for (i = 0; i < numnodes; i++)
			{
				SB_WriteInt (&sb, dnodes[i].planenum);
				SB_WriteInt (&sb, dnodes[i].children[0]);
				SB_WriteInt (&sb, dnodes[i].children[1]);
				SB_WriteFloat (&sb, dnodes[i].mins[0]);
				SB_WriteFloat (&sb, dnodes[i].mins[1]);
				SB_WriteFloat (&sb, dnodes[i].mins[2]);
				SB_WriteFloat (&sb, dnodes[i].maxs[0]);
				SB_WriteFloat (&sb, dnodes[i].maxs[1]);
				SB_WriteFloat (&sb, dnodes[i].maxs[2]);
				SB_WriteInt (&sb, dnodes[i].firstface);
				SB_WriteInt (&sb, dnodes[i].numfaces);
			}
		}
		else
		{
			for (i = 0; i < numnodes; i++)
			{
				SB_WriteInt (&sb, dnodes[i].planenum);
				SB_WriteShort (&sb, dnodes[i].children[0]);
				SB_WriteShort (&sb, dnodes[i].children[1]);
				SB_WriteShort (&sb, dnodes[i].mins[0]);
				SB_WriteShort (&sb, dnodes[i].mins[1]);
				SB_WriteShort (&sb, dnodes[i].mins[2]);
				SB_WriteShort (&sb, dnodes[i].maxs[0]);
				SB_WriteShort (&sb, dnodes[i].maxs[1]);
				SB_WriteShort (&sb, dnodes[i].maxs[2]);
				SB_WriteShort (&sb, dnodes[i].firstface);
				SB_WriteShort (&sb, dnodes[i].numfaces);
			}
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_TEXINFO];
		lump->fileofs = SB_Tell(&sb);
		for (i = 0; i < numtexinfo; i++)
		{
			SB_WriteFloat (&sb, texinfo[i].vecs[0][0]);
			SB_WriteFloat (&sb, texinfo[i].vecs[0][1]);
			SB_WriteFloat (&sb, texinfo[i].vecs[0][2]);
			SB_WriteFloat (&sb, texinfo[i].vecs[0][3]);
			SB_WriteFloat (&sb, texinfo[i].vecs[1][0]);
			SB_WriteFloat (&sb, texinfo[i].vecs[1][1]);
			SB_WriteFloat (&sb, texinfo[i].vecs[1][2]);
			SB_WriteFloat (&sb, texinfo[i].vecs[1][3]);
			SB_WriteInt (&sb, texinfo[i].miptex);
			SB_WriteInt (&sb, texinfo[i].flags);
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_FACES];
		lump->fileofs = SB_Tell(&sb);
		if (isbsp2)
		{
			for (i = 0; i < numfaces; i++)
			{
				SB_WriteInt (&sb, dfaces[i].planenum);
				SB_WriteInt (&sb, dfaces[i].side);
				SB_WriteInt (&sb, dfaces[i].firstedge);
				SB_WriteInt (&sb, dfaces[i].numedges);
				SB_WriteInt (&sb, dfaces[i].texinfo);
				for (j = 0; j < MAXLIGHTMAPS; j++)
					SB_WriteByte (&sb, dfaces[i].styles[j]);
				SB_WriteInt (&sb, dfaces[i].lightofs);
			}
		}
		else
		{
			for (i = 0; i < numfaces; i++)
			{
				SB_WriteShort (&sb, dfaces[i].planenum);
				SB_WriteShort (&sb, dfaces[i].side);
				SB_WriteInt (&sb, dfaces[i].firstedge);
				SB_WriteShort (&sb, dfaces[i].numedges);
				SB_WriteShort (&sb, dfaces[i].texinfo);
				for (j = 0; j < MAXLIGHTMAPS; j++)
					SB_WriteByte (&sb, dfaces[i].styles[j]);
				SB_WriteInt (&sb, dfaces[i].lightofs);
			}
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_CLIPNODES];
		lump->fileofs = SB_Tell(&sb);
		if (isbsp2)
		{
			for (i = 0; i < numclipnodes; i++)
			{
				SB_WriteInt (&sb, dclipnodes[i].planenum);
				SB_WriteInt (&sb, dclipnodes[i].children[0]);
				SB_WriteInt (&sb, dclipnodes[i].children[1]);
			}
		}
		else
		{
			for (i = 0; i < numclipnodes; i++)
			{
				SB_WriteInt (&sb, dclipnodes[i].planenum);
				SB_WriteShort (&sb, dclipnodes[i].children[0]);
				SB_WriteShort (&sb, dclipnodes[i].children[1]);
			}
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_MARKSURFACES];
		lump->fileofs = SB_Tell(&sb);
		if (isbsp2)
		{
			for (i = 0; i < nummarksurfaces; i++)
				SB_WriteInt (&sb, dmarksurfaces[i]);
		}
		else
		{
			for (i = 0; i < nummarksurfaces; i++)
				SB_WriteShort (&sb, dmarksurfaces[i]);
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_SURFEDGES];
		lump->fileofs = SB_Tell(&sb);
		for (i = 0; i < numsurfedges; i++)
			SB_WriteInt (&sb, dsurfedges[i]);
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_EDGES];
		lump->fileofs = SB_Tell(&sb);
		if (isbsp2)
		{
			for (i = 0; i < numedges; i++)
			{
				SB_WriteInt (&sb, dedges[i].v[0]);
				SB_WriteInt (&sb, dedges[i].v[1]);
			}
		}
		else
		{
			for (i = 0; i < numedges; i++)
			{
				SB_WriteShort (&sb, dedges[i].v[0]);
				SB_WriteShort (&sb, dedges[i].v[1]);
			}
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_MODELS];
		lump->fileofs = SB_Tell (&sb);
		for (i = 0; i < nummodels; i++)
		{
			SB_WriteFloat (&sb, dmodels[i].mins[0]);
			SB_WriteFloat (&sb, dmodels[i].mins[1]);
			SB_WriteFloat (&sb, dmodels[i].mins[2]);
			SB_WriteFloat (&sb, dmodels[i].maxs[0]);
			SB_WriteFloat (&sb, dmodels[i].maxs[1]);
			SB_WriteFloat (&sb, dmodels[i].maxs[2]);
			SB_WriteFloat (&sb, dmodels[i].origin[0]);
			SB_WriteFloat (&sb, dmodels[i].origin[1]);
			SB_WriteFloat (&sb, dmodels[i].origin[2]);
			for (j = 0; j < hullinfo.filehulls; j++)
				SB_WriteInt (&sb, dmodels[i].headnode[j]);
			SB_WriteInt (&sb, dmodels[i].visleafs);
			SB_WriteInt (&sb, dmodels[i].firstface);
			SB_WriteInt (&sb, dmodels[i].numfaces);
		}
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_LIGHTING];
		lump->fileofs = SB_Tell (&sb);
		if (ismcbsp)
			SB_WriteData (&sb, drgblightdata, rgblightdatasize);
		else
			SB_WriteData (&sb, dlightdata, lightdatasize);
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_VISIBILITY];
		lump->fileofs = SB_Tell (&sb);
		SB_WriteData (&sb, dvisdata, visdatasize);
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_ENTITIES];
		lump->fileofs = SB_Tell (&sb);
		SB_WriteData (&sb, dentdata, entdatasize);
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

		lump = &lumps[LUMP_TEXTURES];
		lump->fileofs = SB_Tell (&sb);
		SB_WriteData (&sb, dtexdata, texdatasize);
		lump->filelen = SB_Tell(&sb) - lump->fileofs;
		SB_ZeroFill (&sb, ((lump->filelen + 3) & ~3) - lump->filelen);

	// go back and update the header
		index = SB_Tell (&sb);
		SB_SeekAbsolute (&sb, 0);

		if (ismcbsp)
		{
			SB_WriteData (&sb, "MCBSPpad", 8);
			SB_WriteInt (&sb, MCBSPVERSION);
			SB_WriteInt (&sb, hullinfo.numhulls);
			for (i = 1; i < hullinfo.numhulls; i++)
			{
				SB_WriteFloat (&sb, hullinfo.hullsizes[i][0][0]);
				SB_WriteFloat (&sb, hullinfo.hullsizes[i][0][1]);
				SB_WriteFloat (&sb, hullinfo.hullsizes[i][0][2]);
				SB_WriteFloat (&sb, hullinfo.hullsizes[i][1][0]);
				SB_WriteFloat (&sb, hullinfo.hullsizes[i][1][1]);
				SB_WriteFloat (&sb, hullinfo.hullsizes[i][1][2]);
			}
		}
		else if (isbsp2)
		{
			SB_WriteData (&sb, "BSP2", 4);
		}
		else
		{
			SB_WriteInt (&sb, BSPVERSION);
		}

		for (i = 0; i < HEADER_LUMPS; i++)
		{
			SB_WriteInt (&sb, lumps[i].fileofs);
			SB_WriteInt (&sb, lumps[i].filelen);
		}

		SB_SeekAbsolute (&sb, index);

	// open a file and dump the buffer into it
		f = SafeOpenWrite (filename);
		SafeWrite (f, sb.start, (sb.index - sb.start));
		fclose (f);

	// finish up
		SB_Free (&sb);
	}

	if (!ismcbsp && rgblightdatasize)
	{
		FILE *litfile;
		litfile = SafeOpenWrite(filename_lit);
		if (litfile)
		{
			fputc('Q', litfile);
			fputc('L', litfile);
			fputc('I', litfile);
			fputc('T', litfile);
			fputc(1, litfile);
			fputc(0, litfile);
			fputc(0, litfile);
			fputc(0, litfile);
			SafeWrite(litfile, drgblightdata, rgblightdatasize);
			fclose(litfile);
		}
		else
			printf("unable to write \"%s\"\n", filename_lit);
	}

	if (nmaplightdatasize)
	{
		FILE *litfile;
		litfile = SafeOpenWrite(filename_dlit);
		if (litfile)
		{
			fputc('Q', litfile);
			fputc('L', litfile);
			fputc('I', litfile);
			fputc('T', litfile);
			fputc(1, litfile);
			fputc(0, litfile);
			fputc(0, litfile);
			fputc(0, litfile);
			SafeWrite(litfile, dnmaplightdata, nmaplightdatasize);
			fclose(litfile);
		}
		else
			printf("unable to write \"%s\"\n", filename_dlit);
	}
}

//============================================================================

/*
=============
PrintBSPFileSizes

Dumps info about current file
=============
*/
void PrintBSPFileSizes (void)
{
	if (ismcbsp)
		printf ("%5i models       %6i\n", nummodels, (int)(nummodels*sizeof(dmodel_t)));
	else
		printf ("%5i models       %6i\n", nummodels, (int)(nummodels*64));
	printf ("%5i planes       %6i\n", numplanes, (int)(numplanes*sizeof(dplane_t)));
	printf ("%5i vertexes     %6i\n", numvertexes, (int)(numvertexes*sizeof(dvertex_t)));
	printf ("%5i nodes        %6i\n", numnodes, (int)(numnodes*sizeof(dnode_t)));
	printf ("%5i texinfo      %6i\n", numtexinfo, (int)(numtexinfo*sizeof(texinfo_t)));
	printf ("%5i faces        %6i\n", numfaces, (int)(numfaces*sizeof(dface_t)));
	printf ("%5i clipnodes    %6i\n", numclipnodes, (int)(numclipnodes*sizeof(dclipnode_t)));
	printf ("%5i leafs        %6i\n", numleafs, (int)(numleafs*sizeof(dleaf_t)));
	printf ("%5i marksurfaces %6i\n", nummarksurfaces, (int)(nummarksurfaces*sizeof(dmarksurfaces[0])));
	printf ("%5i surfedges    %6i\n", numsurfedges, (int)(numsurfedges*sizeof(dmarksurfaces[0])));
	printf ("%5i edges        %6i\n", numedges, (int)(numedges*sizeof(dedge_t)));
	if (!texdatasize)
		printf ("    0 textures          0\n");
	else
		printf ("%5i textures     %6i\n", ((dmiptexlump_t*)dtexdata)->nummiptex, texdatasize);
	printf ("      lightdata    %6i\n", lightdatasize);
	printf ("      visdata      %6i\n", visdatasize);
	printf ("      entdata      %6i\n", entdatasize);
}

//============================================================================

/*
===============
CompressVis
===============
*/
int CompressVis (byte *vis, byte *dest, int visrow)
{
	int		j;
	int		rep;
	byte	*dest_p;

	dest_p = dest;

	for (j=0 ; j<visrow ; j++)
	{
		*dest_p++ = vis[j];
		if (vis[j])
			continue;

		rep = 1;
		for (j++; j<visrow ; j++)
			if (vis[j] || rep == 255)
				break;
			else
				rep++;
		*dest_p++ = rep;
		j--;
	}

	return dest_p - dest;
}

/*
===============
DecompressVis
===============
*/
void DecompressVis(byte *in, byte *out, int size)
{
	byte *end = out + size;
	int n;
	while (out < end)
	{
		n = *in++;
		if (n)
			*out++ = n;
		else
		{
			n = *in++;
			while (n--)
				*out++ = 0;
		}
	}
}

//============================================================================

int			num_entities = 0;
entity_t	entities[MAX_MAP_ENTITIES];

void PrintEntity (entity_t *ent)
{
	epair_t	*ep;

	for (ep=ent->epairs ; ep ; ep=ep->next)
		printf ("%20s : %s\n", ep->key, ep->value);
}


char *ValueForKey (entity_t *ent, char *key)
{
	epair_t	*ep;

	for (ep=ent->epairs ; ep ; ep=ep->next)
		if (!strcmp (ep->key, key) )
			return ep->value;
		return "";
}

vec_t FloatForKey (entity_t *ent, char *key)
{
	epair_t	*ep;

	for (ep=ent->epairs ; ep ; ep=ep->next)
		if (!strcmp (ep->key, key) )
			return atof( ep->value );

	return 0;
}

void SetKeyValue (entity_t *ent, char *key, char *value)
{
	epair_t	*ep;

	for (ep=ent->epairs ; ep ; ep=ep->next)
		if (!strcmp (ep->key, key) )
		{
			qfree (ep->value);
			ep->value = copystring(value);
			return;
		}
	ep = qmalloc (sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring(key);
	ep->value = copystring(value);
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair (void)
{
	epair_t	*e;

	e = qmalloc (sizeof(epair_t));
	memset (e, 0, sizeof(epair_t));

	if (strlen(token) >= MAX_KEY-1)
		Error ("ParseEpair: token too long");
	e->key = copystring(token);
	GetToken (false);
	if (strlen(token) >= MAX_VALUE-1)
		Error ("ParseEpair: token too long");
	e->value = copystring(token);

	return e;
}

entity_t *FindEntityWithKeyPair( char *key, char *value )
{
	entity_t *ent;
	epair_t	*ep;
	int i;

	for (i=0 ; i<num_entities ; i++)
	{
		ent = &entities[ i ];
		for (ep=ent->epairs ; ep ; ep=ep->next)
		{
			if (!strcmp (ep->key, key) )
			{
				if ( !strcmp( ep->value, value ) )
					return ent;
				break;
			}
		}
	}
	return NULL;
}

void GetVectorForKey (entity_t *ent, char *key, vec3_t vec)
{
	char	*k;
	double	v1, v2, v3;

	k = ValueForKey (ent, key);
	v1 = v2 = v3 = 0;
	// scanf into doubles, then assign, so it is vec_t size independent
	sscanf (k, "%lf %lf %lf", &v1, &v2, &v3);
	vec[0] = v1;
	vec[1] = v2;
	vec[2] = v3;
}

/*
==================
ParseEntities
==================
*/
void ParseEntities (void)
{
	char 		*data;
	entity_t	*entity;
	char		key[MAX_KEY];
	epair_t		*epair;

	data = dentdata;

	//
	// start parsing
	//
	num_entities = 0;

	// go through all the entities
	while (1)
	{
		// parse the opening brace
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Error ("LoadEntities: found %s when expecting {", com_token);

		if (num_entities == MAX_MAP_ENTITIES)
			Error ("LoadEntities: MAX_MAP_ENTITIES");

		entity = &entities[num_entities++];

		// go through all the keys in this entity
		while (1)
		{
			int		c;

			// parse key
			data = COM_Parse (data);
			if (!data)
				Error ("LoadEntities: EOF without closing brace");
			if (!strcmp(com_token,"}"))
				break;

			strcpy (key, com_token);

			// parse value
			data = COM_Parse (data);
			if (!data)
				Error ("LoadEntities: EOF without closing brace");
			c = com_token[0];
			if (c == '}')
				Error ("LoadEntities: closing brace without data");

			epair = malloc (sizeof(epair_t));
			epair->key = copystring( key );
			epair->value = copystring( com_token );
			epair->next = entity->epairs;
			entity->epairs = epair;
		}
	}
}

/*
==================
UnparseEntity
==================
*/
static void UnparseEntity( entity_t *ent, char *buf, char **end )
{
	epair_t		*ep;
	char		line[16384];

	ep = ent->epairs;
	if( !ep )
		return;

	strcat (*end,"{\n");
	*end += 2;

	for ( ; ep ; ep=ep->next)
	{
		sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
		strcat (*end, line);
		*end += strlen(line);
	}
	strcat (*end,"}\n");
	*end += 2;

	if (*end > buf + MAX_MAP_ENTSTRING)
		Error ("Entity text too long");
}

/*
==================
UnparseEntities
==================
*/
void UnparseEntities (void)
{
	int			i;
	entity_t	*ent;
	char		*buf, *end;

	buf = dentdata;
	end = buf;
	*end = 0;

	// Vic: write "worldspawn" as the very first entity (engines might depend on it)
	ent = FindEntityWithKeyPair( "classname", "worldspawn" );
	if( ent )
		UnparseEntity( ent, buf, &end );

	for (i=0 ; i<num_entities ; i++) {
		if( &entities[i] != ent )
			UnparseEntity( &entities[i], buf, &end );
	}

	entdatasize = end - buf + 1;
}
