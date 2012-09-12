
#include "bsp5.h"

typedef struct
{
	char	identification[4]; // "WAD2"
	int		numlumps;
	int		infotableofs;
}
wadinfo_t;

typedef struct
{
	int		filepos;
	int		disksize;
	int		size; // uncompressed
	char	type;
	char	compression;
	char	pad1, pad2;
	char	name[16]; // must be null terminated
}
lumpinfo_t;

#define	TYP_NONE		0
#define	TYP_LABEL		1

#define	TYP_LUMPY		64				// 64 + grab command number
#define	TYP_PALETTE		64
#define	TYP_QTEX		65
#define	TYP_QPIC		66
#define	TYP_SOUND		67
#define	TYP_MIPTEX		68

typedef struct
{
	FILE *file;
	int filepos;
	int size;
	char name[16];
}
miptexfile_t;

#define MAX_MIPTEXFILES 32768

miptexfile_t miptexfile[MAX_MIPTEXFILES];
int nummiptexfiles = 0;

void CleanupName (char *in, char *out)
{
	int		i;

	for (i = 0;i < 15 && in[i];i++)
		out[i] = toupper(in[i]);

	for (;i < 16;i++)
		out[i] = 0;
}

int MipTexUsed(char *texname)
{
	int i;
	for (i = 0;i < nummiptex;i++)
		if (!Q_strcasecmp(miptex[i], texname))
			return true;
	return false;
}

miptexfile_t *FindMipTexFile(char *texname)
{
	int i;

	// search for exact match, then a match with first path element stripped, then next path element, and so on...
	while (*texname)
	{
		for (i = 0;i < nummiptexfiles;i++)
			if (!Q_strcasecmp(miptexfile[i].name, texname))
				return miptexfile + i;
		if (strchr(texname, '/'))
			texname = strchr(texname, '/') + 1;
		else
			break;
	}
	return NULL;
}

void AddAnimatingTextures (void)
{
	int		base;
	int		i, j;
	char	name[128];

	base = nummiptex;

	for (i = 0;i < base;i++)
	{
		if (miptex[i][0] != '+')
			continue;
		strncpy (name, miptex[i], sizeof(name) - 1);
		name[sizeof(name)-1] = 0;

		for (j = 0;j < 20;j++)
		{
			if (j < 10)
				name[1] = j + '0';
			else
				name[1] = j + 'A' - 10;		// alternate animation

			if (!MipTexUsed(name) && FindMipTexFile(name))
				FindMiptex (name);
		}
	}
}

void getwadname(char *out, char **inpointer)
{
	char *in;
	in = *inpointer;
	while (*in && (*in == ';' || *in <= ' '))
		in++;
	if (*in == '"')
	{
		// quoted name
		in++;
		while (*in && *in != '"')
			*out++ = *in++;
		if (*in == '"')
			in++;
		// FIXME: error if quoted string is not properly terminated?
	}
	else
	{
		// normal name
		while (*in && *in != ';')
			*out++ = *in++;
	}
	*out++ = 0;
	*inpointer = in;
}

int loadwad(char *filename)
{
	int i, oldnummiptexfiles = nummiptexfiles;
	wadinfo_t wadinfo;
	lumpinfo_t lumpinfo;
	FILE *file;
	file = fopen(filename, "rb");
	if (!file)
	{
		printf("unable to open wad \"%s\"\n", filename);
		return -1;
	}
	if (fread(&wadinfo, sizeof(wadinfo_t), 1, file) < 1)
	{
		printf("error reading wad header in \"%s\"\n", filename);
		fclose(file);
		return -1;
	}
	// FIXME: support halflife WAD3?
	if (memcmp(wadinfo.identification, "WAD2", 4))
	{
		printf("\"%s\" is not a quake WAD2 archive\n", filename);
		fclose(file);
		return -1;
	}

	wadinfo.numlumps = LittleLong(wadinfo.numlumps);
	wadinfo.infotableofs = LittleLong(wadinfo.infotableofs);
	if (fseek(file, wadinfo.infotableofs, SEEK_SET))
	{
		printf("error seeking in wad \"%s\"\n", filename);
		fclose(file);
		return -1;
	}

	for (i = 0;i < wadinfo.numlumps;i++)
	{
		if (fread(&lumpinfo, sizeof(lumpinfo_t), 1, file) < 1)
		{
			printf("error reading lump header in wad \"%s\"\n", filename);
			fclose(file);
			return -1;
		}
		lumpinfo.filepos = LittleLong(lumpinfo.filepos);
		lumpinfo.disksize = LittleLong(lumpinfo.disksize);
		lumpinfo.size = LittleLong(lumpinfo.size);
		CleanupName(lumpinfo.name, lumpinfo.name);
		// LordHavoc: some wad editors write 0 size
		// disabled the size check because some wad editors write garbage for the lump size
		if (lumpinfo.compression)// || (lumpinfo.size && lumpinfo.size != lumpinfo.disksize))
		{
			printf("lump \"%s\" in wad \"%s\" is compressed??\n", lumpinfo.name, filename);
			continue;
		}
		if (lumpinfo.type == TYP_MIPTEX)
		{
			miptexfile[nummiptexfiles].file = file;
			miptexfile[nummiptexfiles].filepos = lumpinfo.filepos;
			miptexfile[nummiptexfiles].size = lumpinfo.disksize;
			memcpy(miptexfile[nummiptexfiles].name, lumpinfo.name, 16);
			nummiptexfiles++;
		}
	}
	printf("loaded wad \"%s\" (%i miptex found)\n", filename, nummiptexfiles - oldnummiptexfiles);
	return nummiptexfiles - oldnummiptexfiles;
}

int ReadMipTexFile(miptexfile_t *m, byte *out)
{
	if (fseek(m->file, m->filepos, SEEK_SET))
	{
		printf("error seeking to data for lump \"%s\"\n", m->name);
		return true;
	}
	if (fread(out, m->size, 1, m->file) < 1)
	{
		printf("error reading data for lump \"%s\"\n", m->name);
		return true;
	}
	return false;
}

static char wadname[2048], wadfilename[2048];
void WriteMiptex (void)
{
	int i, success, blanktex;
	byte *miptex_data;
	dmiptexlump_t *miptex_lumps;
	char *path, *currentpath;
	miptexfile_t *m;

	blanktex = CheckParm("-notex") != 0;

	path = ValueForKey (&entities[0], "_wad");
	if (!path || !path[0])
	{
		path = ValueForKey (&entities[0], "wad");
		if (!path || !path[0])
		{
			printf ("WriteMiptex: no wads specified in \"wad\" key in worldspawn\n");
			//texdatasize = 0;
			//return;
		}
	}

	nummiptexfiles = 0;

	currentpath = path;
	while (*currentpath)
	{
		getwadname(wadname, &currentpath);
		if (wadname[0])
		{
			success = false;
			// try prepending each -wadpath on the commandline to the wad name
			for (i = 1;i < myargc;i++)
			{
				if (!Q_strcasecmp("-wadpath", myargv[i]))
				{
					i++;
					if (i < myargc)
					{
						sprintf(wadfilename, "%s%s", myargv[i], wadname);
						if ((success = loadwad(wadfilename) >= 0))
							break;
					}
				}
			}
			if (!success)
			{
				// if the map name has a path, we can try loading the wad from there
				ExtractFilePath(filename_bsp, wadfilename);
				if (wadfilename[0])
				{
					strcat(wadfilename, wadname);
					if (!(success = loadwad(wadfilename) >= 0))
					{
						// try the parent directory
						ExtractFilePath(filename_bsp, wadfilename);
						strcat(wadfilename, "../");
						strcat(wadfilename, wadname);
						success = loadwad(wadfilename) >= 0;
					}
				}
			}
			if (!success)
			{
				// try the wadname itself
				success = loadwad(wadname) >= 0;
			}
			if (!success)
				printf("Could not find wad \"%s\" using -wadpath options or in the same directory as the map or it's parent directory, so there!\n", wadname);
		}
	}

	AddAnimatingTextures ();

	miptex_lumps = (dmiptexlump_t *)dtexdata;
	miptex_data = (byte *)&miptex_lumps->dataofs[nummiptex];
	miptex_lumps->nummiptex = nummiptex;
	for (i=0 ; i < nummiptex ; i++)
	{
		printf("miptex used: %s", miptex[i]);
		m = FindMipTexFile(miptex[i]);
		if (m)
		{
			if (miptex_data + m->size - dtexdata >= MAX_MAP_MIPTEX)
			{
				miptex_lumps->dataofs[i] = -1;
				printf(" (MAX_MAP_MIPTEX exceeded)\n");
			}
			else if (ReadMipTexFile(m, miptex_data))
			{
				miptex_lumps->dataofs[i] = -1;
				printf(" (READ ERROR)\n");
			}
			else
			{
				miptex_lumps->dataofs[i] = miptex_data - (byte *)miptex_lumps;
				if (blanktex && m->size > (int)sizeof(miptex_t))
					memset(miptex_data + sizeof(miptex_t), 255, m->size - sizeof(miptex_t));
				printf("\n");
				miptex_data += m->size;
			}
		}
		else
		{
			if (miptex_data + (sizeof(miptex_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2) - dtexdata >= MAX_MAP_MIPTEX)
			{
				miptex_lumps->dataofs[i] = -1;
				printf(" (MAX_MAP_MIPTEX exceeded)\n");
			}
			else
			{
				miptex_t blankmiptex;
				char *s;
				int j, x, y;
				printf(" (NOT FOUND)\n");
				miptex_lumps->dataofs[i] = miptex_data - (byte *)miptex_lumps;
				s = miptex[i];
				if (strrchr(s, '/'))
					s = strrchr(s, '/') + 1;
				memset(blankmiptex.name, 0, sizeof(blankmiptex.name));
				strncpy(blankmiptex.name, s, sizeof(blankmiptex.name) - 1);
				blankmiptex.width = LittleLong(16);
				blankmiptex.height = LittleLong(16);
				blankmiptex.offsets[0] = LittleLong(sizeof(miptex_t));
				blankmiptex.offsets[1] = LittleLong(sizeof(miptex_t) + 16 * 16);
				blankmiptex.offsets[2] = LittleLong(sizeof(miptex_t) + 16 * 16 + 8 * 8);
				blankmiptex.offsets[3] = LittleLong(sizeof(miptex_t) + 16 * 16 + 8 * 8 + 4 * 4);
				memcpy(miptex_data, &blankmiptex, sizeof(miptex_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2);
				miptex_data += sizeof(miptex_t);
				for (j = 0;j < 4;j++)
				{
					for (y = 0;y < (16 >> j);y++)
					{
						for (x = 0;x < (16 >> j);x++)
						{
							if ((x ^ y) & (8 >> j))
								*miptex_data++ = 10;
							else
								*miptex_data++ = 6;
						}
					}
				}
				//memset(miptex_data, 15, 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2);
				//miptex_data += 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2;
			}
		}
	}

	texdatasize = miptex_data - dtexdata;
}

