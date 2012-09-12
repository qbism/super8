#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>

typedef unsigned char byte;

//definitions from bspfile.h
#define	LUMP_TEXTURES		2
#define	LUMP_TEXINFO		6
#define	LUMP_FACES			7
#define	LUMP_LEAFS			10
#define	LUMP_MARKSURFACES	11
#define	LUMP_MODELS			14

typedef struct
{
	int			fileofs, filelen;
} lump_t;

#define	HEADER_LUMPS 15
typedef struct
{
	int			version;
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

#define	NUM_AMBIENTS 4
typedef struct
{
	int					contents;
	int					visofs;
	short				mins[3];
	short				maxs[3];
	unsigned short		firstmarksurface;
	unsigned short		nummarksurfaces;
	byte				ambient_level[NUM_AMBIENTS];
} dleaf_t;

#define	MAXLIGHTMAPS 4
typedef struct
{
	short		planenum;
	short		side;
	int			firstedge;
	short		numedges;
	short		texinfo;
	byte		styles[MAXLIGHTMAPS];
	int			lightofs;
} dface_t;

typedef struct texinfo_s
{
	float		vecs[2][4];
	int			miptex;
	int			flags;
} texinfo_t;

typedef struct
{
	int			nummiptex;
	int			dataofs[4]; //array is actually variable-size based on nummiptex
} dmiptexlump_t;

#define	MIPLEVELS 4
typedef struct miptex_s
{
	char		name[16];
	unsigned	width, height;
	unsigned	offsets[MIPLEVELS];
} miptex_t;

#define	MAX_MAP_HULLS 4
typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLS];
	int			visleafs;
	int			firstface, numfaces;
} dmodel_t;


/*
============
StripExtension
============
*/
void StripExtension (char *in, char *out) //qb: from Quake source
{
    while (*in && *in != '.')
        *out++ = *in++;
    *out = 0;
}


/*
===================
ProcessBSP - returns the number of removed faces

TODO: make endian-safe
===================
*/
int ProcessBSP (byte *data, unsigned int filesize)
{
	dheader_t		*header;
	dleaf_t			*leafs, *leaf;
	unsigned short	*marksurfaces, *leafmarks;
	dface_t			*faces, *face, *modfaces;
	texinfo_t		*texinfos, *texinfo;
	dmiptexlump_t	*miptexlump;
	miptex_t		*textures, *texture;
	dmodel_t		*models, *model;
	char			*name;
	int				numleafs, i, j, k, skipcount, nummodels;

	header = (dheader_t *)data;
	leafs = (dleaf_t *)(data + header->lumps[LUMP_LEAFS].fileofs);
	numleafs = header->lumps[LUMP_LEAFS].filelen / sizeof(dleaf_t);
	marksurfaces = (unsigned short *)(data + header->lumps[LUMP_MARKSURFACES].fileofs);
	faces = (dface_t *)(data + header->lumps[LUMP_FACES].fileofs);
	texinfos = (texinfo_t *)(data + header->lumps[LUMP_TEXINFO].fileofs);
	miptexlump = (dmiptexlump_t *)(data + header->lumps[LUMP_TEXTURES].fileofs);
	textures = (miptex_t *)(data + header->lumps[LUMP_TEXTURES].fileofs);
	models = (dmodel_t *)(data + header->lumps[LUMP_MODELS].fileofs);
	nummodels = header->lumps[LUMP_MODELS].filelen / sizeof(dmodel_t);
	skipcount = 0;

// loop through all leafs, editing marksurfaces lists (this takes care of the worldmodel)
	for (i=0,leaf=leafs; i<numleafs; i++,leaf++)
	{
		leafmarks = marksurfaces + leaf->firstmarksurface;
		for (j=0; j<leaf->nummarksurfaces; )
		{
			face = faces + leafmarks[j];
			texinfo = texinfos + face->texinfo;
			texture = (miptex_t *)((byte *)textures + miptexlump->dataofs[texinfo->miptex]);
			name = texture->name;

			if (!strcmp(name,"skip") ||
				!strcmp(name,"*waterskip") ||
				!strcmp(name,"*slimeskip") ||
				!strcmp(name,"*lavaskip"))
			{
				// copy each remaining marksurface to previous slot
				for (k=j;k<leaf->nummarksurfaces-1; k++)
					leafmarks[k] = leafmarks[k+1];

				// reduce marksurface count by one
				leaf->nummarksurfaces--;

				skipcount++;
			}
			else
				j++;
		}
	}

// loop through all the models, editing surface lists (this takes care of brush entities)
	for (i=0,model=models; i<nummodels; i++,model++)
	{
		if (i==0) continue; // model 0 is worldmodel

		modfaces = faces + model->firstface;
		for (j=0; j<model->numfaces; )
		{
			face = modfaces + j;
			texinfo = texinfos + face->texinfo;
			texture = (miptex_t *)((byte *)textures + miptexlump->dataofs[texinfo->miptex]);
			name = texture->name;

			if (!strcmp(name,"skip") ||
				!strcmp(name,"*waterskip") ||
				!strcmp(name,"*slimeskip") ||
				!strcmp(name,"*lavaskip"))
			{
				// copy each remaining face to previous slot
				for (k=j;k<model->numfaces-1; k++)
					modfaces[k] = modfaces[k+1];

				// reduce face count by one
				model->numfaces--;

				skipcount++;
			}
			else
				j++;
		}
	}

	return skipcount;
}

/*
===================
ReadBSP - returns a pointer to the data in memory, and sets the value of "size"
===================
*/
byte *ReadBSP (char *name, unsigned int *size)
{
	FILE *in;
	byte *data;
	unsigned int filesize;
    //char filename[256];

	StripExtension(name, name);
		//qb: no try, only do... add ".bsp"
		if ((in = fopen(strcat(name,".bsp"), "rb")) == NULL)
		{
			printf ("Error: Couldn't open file: %s", name);
			exit(1);
		}


	//get filesize
	fseek (in, 0, SEEK_END);
	filesize = ftell(in);
	fseek (in, 0, SEEK_SET);

	//load into buffer
	data = malloc(filesize);
	fread(data, sizeof(byte), filesize, in);
	fclose(in);

	*size = filesize;
	return data;
}

/*
===================
WriteBSP
===================
*/
void WriteBSP (char *name, byte *data, unsigned int filesize)
{
	FILE *out;

	if ((out = fopen(name, "wb")) == NULL)
	{
		printf ("Error: Couldn't write file: %s", name);
		exit(1);
	}

	fwrite(data, sizeof(byte), filesize, out);
	fclose(out);
}


/*
===================
StripPath
===================
*/
const char *StripPath (const char *path)
{
	const char *c;

	for (c = path + strlen (path); c > path; c--)
		if (*c == '\\')
			return c+1;

	return path;
}

/*
===================
main
===================
*/
void main (int argc, char **argv)
{
	byte *data;
	int skipcount;
	unsigned int filesize;
	char filename[1024];

	printf("----- newskip version 1.0 -----\n");
	printf("copyright 2007 John Fitzgibbons\n\n");

	if (argc < 2)
	{
		printf ("Usage: newskip <filename>");
		exit(1);
	}

	strncpy (filename, argv[1], sizeof(filename)-1);

	printf("Filename: %s\n", StripPath (filename));

	data = ReadBSP(filename, &filesize);
	skipcount = ProcessBSP(data, filesize);
    StripExtension(filename, filename);//qb: from Quake source
    strcat(filename,".bsp"); //qb: always switch to bsp
	WriteBSP (filename, data, filesize);
	free (data);

	printf("Removed %i faces.\n", skipcount);
}
