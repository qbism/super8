/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"
#include "r_local.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer, loadedfile_t *fileinfo);	// 2001-09-12 .ENT support by Maddes

void Mod_LoadAliasModel (model_t *mod, void *buffer);

//qbism - declare funcs from bjp
static qboolean AllFrames (texture_t *tx, texture_t *anims[], int max, qboolean Alternate);



model_t *Mod_LoadModel (model_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	2048 //qbism was 256
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// values for model_t's needload
#define NL_PRESENT		0
#define NL_NEEDS_LOADED	1
#define NL_UNREFERENCED	2

cvar_t	external_ent = {"external_ent","1"};	// 2001-09-12 .ENT support by Maddes
cvar_t	external_vis = {"external_vis","1"};	// 2001-12-28 .VIS support by Maddes

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
	Cvar_RegisterVariable (&external_ent);	// 2001-09-12 .ENT support by Maddes
	Cvar_RegisterVariable (&external_vis);	// 2001-12-28 .VIS support by Maddes
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;

	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed, model %s", mod->name);  //qbism- add mod name from bjp
	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;

	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->numleafs+7)>>3;
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

/*
=================
Mod_Alloc

Avoid putting large objects in Quake heap
=================
*/
static byte *Mod_Alloc (char *Function, int Size)  //qbism from bjp
{
	byte *Buf = malloc (Size);

	if (!Buf)
		Sys_Error ("%s: not enough memory (%d) for '%s'", Function, Size, loadname);

//Con_Printf ("Mod_Alloc: %s: %dk\n", Function, Size / 1024);
	return Buf;
}

/*
=================
Mod_AllocClear
=================
*/
static void Mod_AllocClear (model_t *mod)  //qbism from bjp
{
	// Clear Mod_Alloc pointers
	mod->lightdata = NULL;
	mod->visdata = NULL;
}

/*
=================
Mod_Free2
=================
*/
static void Mod_Free2 (byte **Ptr)
{
	if (*Ptr)
	{
		free (*Ptr);
		*Ptr = NULL;
	}
}

/*
=================
Mod_Free
=================
*/
static void Mod_Free (model_t *mod)
{
	Mod_Free2 (&mod->lightdata);
	Mod_Free2 (&mod->visdata);
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t	*mod;


	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++) {
		mod->needload = NL_UNREFERENCED;
//FIX FOR CACHE_ALLOC ERRORS:
		if (mod->type == mod_sprite) mod->cache.data = NULL;

		Mod_Free (mod);
	}
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (char *name)  //qbism - with bjp modifications
{
	int		i;
	model_t	*mod;
	model_t	*avail = NULL;

	if (!name[0])
		Sys_Error ("Mod_FindName: NULL name");

//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!Q_strcmp (mod->name, name) )
			break;
		if (mod->needload == NL_UNREFERENCED)
			if (!avail || mod->type != mod_alias)
				avail = mod;
	}

	if (i == mod_numknown)
	{
		int len;

		if (mod_numknown == MAX_MOD_KNOWN)
		{
			if (avail)
			{
				mod = avail;
				if (mod->type == mod_alias)
					if (Cache_Check (&mod->cache))
						Cache_Free (&mod->cache);
			}
			else
				Sys_Error ("Mod_FindName: mod_numknown == MAX_MOD_KNOWN (%d)", MAX_MOD_KNOWN);
		}
		else
			mod_numknown++;

		len = strlen (name);

		if (len >= sizeof(mod->name))
			Sys_Error ("Mod_FindName: name '%s' too long (%d, max = %d)", name, len, sizeof(mod->name) - 1);

		Q_strcpy (mod->name, name);
		mod->needload = NL_NEEDS_LOADED;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel (char *name)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	if (mod->needload == NL_PRESENT)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

static int mod_filesize;

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, qboolean crash)
{
	unsigned *buf;
	byte	stackbuf[1024];		// avoid dirtying the cache heap
	loadedfile_t	*fileinfo;	// 2001-09-12 Returning information about loaded file by Maddes

	if (mod->type == mod_alias)
	{
		if (Cache_Check (&mod->cache))
		{
			mod->needload = NL_PRESENT;
			return mod;
		}
	}
	else
	{
		if (mod->needload == NL_PRESENT)
			return mod;
	}
	Con_DPrintf ("Loading %s\n",mod->name); // Manoel Kasimier

//
// because the world is so huge, load it one piece at a time
//

//
// load the file
//
// 2001-09-12 Returning information about loaded file by Maddes  start
/*
	buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
	if (!buf)
*/
	fileinfo = COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
	if (!fileinfo)
// 2001-09-12 Returning information about loaded file by Maddes  end
	{
		// If last added mod_known, get rid of it
		if (mod == &mod_known[mod_numknown - 1])
			--mod_numknown;

		if (crash)
			Host_Error ("Mod_LoadModel: %s not found", mod->name);

        Con_DPrintf ("Mod_LoadModel: %s not found",mod->name);  //qbism- list all not found, in debug mode.
		return NULL;
	}
	buf = (unsigned *)fileinfo->data;	// 2001-09-12 Returning information about loaded file by Maddes
	mod_filesize = com_filesize; // Save off

//
// allocate a new model
//
	COM_FileBase (mod->name, loadname);

	loadmodel = mod;

//
// fill it in
//

// call the apropriate loader
	mod->needload = NL_PRESENT;

	switch (LittleLong(*(unsigned *)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;

	default:
		Mod_LoadBrushModel (mod, buf, fileinfo);	// 2001-09-12 .ENT support by Maddes
		break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

static int MissTex;  //qbism - from bjp
byte	*mod_base;

/*
=================
Mod_ChkLumpSize
=================
*/
static void Mod_ChkLumpSize (char *Function, lump_t *l, int Size)
{
	if (l->filelen % Size)
		Sys_Error ("%s: lump size %d is not a multiple of %d in %s", Function, l->filelen, Size, loadmodel->name);
}

/*
=================
WithinBounds
=================
*/
static qboolean WithinBounds (int SItem, int Items, int Limit, int *ErrItem)  //qbism - from bjp
{
	int EItem = 0;

	if (SItem < 0)
		EItem = SItem;
	else if (Items < 0)
		EItem = Items;
	else if (SItem + Items < 0 || SItem + Items > Limit)
		EItem = SItem + Items;

	if (ErrItem)
		*ErrItem = EItem;

	return EItem == 0;
}

/*
=================
Mod_ChkBounds
=================
*/
static void Mod_ChkBounds (char *Function, char *Object, int SItem, int Items, int Limit)  //qbism - from bjp
{
	int ErrItem;

	if (!WithinBounds (SItem, Items, Limit, &ErrItem))
		Sys_Error ("%s: %s is out of bounds (%d, max = %d) in %s", Function, Object, ErrItem, Limit, loadmodel->name);
}

/*
=================
Mod_ChkFileSize
=================
*/
static void Mod_ChkFileSize (char *Function, char *Object, int SItem, int Items)  //qbism - from bjp
{
	int ErrItem;

	if (!WithinBounds (SItem, Items, mod_filesize, &ErrItem))
		Sys_Error ("%s: %s is outside file (%d, max = %d) in %s", Function, Object, ErrItem, mod_filesize, loadmodel->name);
}

/*
=================
Mod_ChkType
=================
*/
static void Mod_ChkType (char *Function, char *Object, char *ObjNum, int Type, int Type1, int Type2)  //qbism - from bjp
{
	if (Type != Type1 && Type != Type2)
	{
		// Should be an error but ...
		Con_Printf ("\002%s: ", Function);
		Con_Printf ("invalid %s %d in %s in %s\n", Object, Type, ObjNum, loadmodel->name);
//		Sys_Error ("%s: invalid %s %d in %s in %s", Function, Object, Type, ObjNum, loadmodel->name);
	}
}


static int  MissFrameNo;
static char MissFrameName[16 +1];

static qboolean AllFrames (texture_t *tx, texture_t *anims[], int max, qboolean Alternate)
{
	int i, frames = 0;

	for (i=0 ; i<max ; i++)
	{
		if (anims[i])
		    ++frames;
		else
		{
			// Avoid unnecessary warnings
			if (i != MissFrameNo || strcmp(tx->name, MissFrameName))
			{
				MissFrameNo = i;
				strcpy (MissFrameName, tx->name);
				Con_Printf ("\002Mod_LoadTextures: ");
				Con_Printf ("missing %sframe %i of '%s'\n", Alternate ? "alternate" : "", i, tx->name);
			}
		}
	}

	return frames == max;
}

/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	int		i, j, pixels, num, max, altmax;
	miptex_t	*mt;
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	dmiptexlump_t *m;
	char		texname[16 + 1];

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		Con_Printf ("\x02Mod_LoadTextures: ");
		Con_Printf ("no textures in %s\n", loadmodel->name);
		return;
	}

	// Check bounds for nummiptex var
	Mod_ChkBounds ("Mod_LoadTextures", "nummiptex", 0, sizeof(int), l->filelen);

	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	m->nummiptex = LittleLong (m->nummiptex);

	// Check bounds for dataofs array
	if (m->nummiptex > 0)
		Mod_ChkBounds ("Mod_LoadTextures", "miptex lump", sizeof(int), m->nummiptex * sizeof(int), l->filelen);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures) , "modtex1");

	MissTex = 0;

	for (i=0 ; i<m->nummiptex ; i++)
	{
		int size, copypixels;

		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
		{
			++MissTex;
			continue;
		}
		mt = (miptex_t *)((byte *)m + m->dataofs[i]);

		// Check bounds for miptex entry
		Mod_ChkBounds ("Mod_LoadTextures", "miptex entry", (byte *)mt - (mod_base + l->fileofs), sizeof(miptex_t), l->filelen);

		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		// Make sure tex name is terminated
		memset (texname, 0, sizeof(texname));
		memcpy (texname, mt->name, sizeof(texname) - 1);

		if (mt->width <= 0 || mt->height <= 0 || (mt->width & 15) || (mt->height & 15))
			Sys_Error ("Mod_LoadTextures: texture '%s' is not 16 aligned (%dx%d) in %s", texname, mt->width, mt->height, loadmodel->name);

		// Check bounds for miptex objects
		for (j=0 ; j<MIPLEVELS ; j++)
		{
			int ErrItem, Offset = (byte *)mt - (mod_base + l->fileofs) + mt->offsets[j];
			int MipSize = mt->width * mt->height / (1 << j * 2);

			if (!WithinBounds(Offset, MipSize, l->filelen, &ErrItem))
			{
				Con_Printf ("\x02Mod_LoadTextures: ");
				Con_Printf ("miptex object %d for '%s' is outside texture lump (%d, max = %d) in %s\n", j, texname, ErrItem, l->filelen, loadmodel->name);
				mt->offsets[j] = sizeof (miptex_t); // OK?
			}
		}

		pixels = copypixels = mt->width*mt->height/64*85;

		size = (byte *)(mt + 1) + pixels - (mod_base + l->fileofs);

		// Check bounds for pixels
		if (size > l->filelen)
		{
			Con_Printf ("\x02Mod_LoadTextures: ");
			Con_Printf ("pixels for '%s' is outside texture lump (%d, max = %d) in %s\n", texname, size, l->filelen, loadmodel->name);
			copypixels -= size - l->filelen; // Prevent access violation
		}

		tx = Hunk_AllocName (sizeof(texture_t) +pixels, "modtex2");
		loadmodel->textures[i] = tx;

		strcpy (tx->name, texname);
		tx->width = mt->width;
		tx->height = mt->height;
		for (j=0 ; j<MIPLEVELS ; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures
		memcpy ( tx+1, mt+1, copypixels);

		if (!isDedicated) //no texture uploading for dedicated server
		{
			if (!Q_strncmp(tx->name,"sky",3))
			R_InitSky (tx);
	}
	}

//
// sequence the animations
//
	MissFrameNo = 0;
	memset (MissFrameName, 0, sizeof(MissFrameName));

	for (i=0 ; i<m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// allready sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Sys_Error ("Bad animating texture '%s'", tx->name);

		for (j=i+1 ; j<m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (Q_strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Sys_Error ("Bad animating texture '%s'", tx->name);
		}

		// Check if all frames are present
		if (!AllFrames (tx, anims, max, false) || !AllFrames (tx, altanims, altmax, true))
			continue; // Disable animation sequence

#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];

			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];

			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Mod_Alloc ("Mod_LoadLighting", l->filelen);
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Mod_Alloc ("Mod_LoadVisibility", l->filelen);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l, loadedfile_t *brush_fileinfo)	// 2001-09-12 .ENT support by Maddes
{
// 2001-09-12 .ENT support by Maddes  start
	char	entfilename[1024];
	loadedfile_t	*fileinfo;
	searchpath_t	*s_check;

	loadmodel->entities = NULL;

	if (external_ent.value)
	{
		// check for a .ENT file
		Q_strcpy(entfilename, loadmodel->name);
		COM_StripExtension(entfilename, entfilename);
		Q_strcat(entfilename, ".ent");

		fileinfo = COM_LoadHunkFile (entfilename);
		if (fileinfo && fileinfo->filelen)
		{
			// .ENT file only allowed from same directory of map file or another directory before in the searchpath
			s_check = COM_GetDirSearchPath(brush_fileinfo->path);	// get last searchpath of map directory
			// Manoel Kasimier - begin
			if (s_check->pack) // it's a damn pack file, so let's find the searchpath of the directory where it is
			{
				char	brushfilename[1024];
				int i;
				searchpath_t	*s_temp;

				Q_strcpy(brushfilename, s_check->pack->filename);
				// remove the packfile from the path
				for (i=Q_strlen(brushfilename); i > 0 ; i--)
					if (brushfilename[i] == '/' || brushfilename[i] == '\\')
					{
						brushfilename[i] = 0;
						break;
					}
				// find the searchpath of the directory
				s_temp = s_check;
				while ((s_temp = s_temp->next))
					if (!s_temp->pack)
						if (!Q_strcmp(brushfilename, s_temp->filename))
						{
							s_check = s_temp;
							break;
						}
			}
			// Manoel Kasimier - end
			while ( (s_check = s_check->next) )		// next searchpath
			{
				if (s_check == fileinfo->path)	// found .ENT searchpath = after map directory
				{
					Con_DPrintf("%s not allowed from %s as %s is from %s\n",
						entfilename,
						fileinfo->path->pack ? fileinfo->path->pack->filename : fileinfo->path->filename,
						loadmodel->name,
						brush_fileinfo->path->pack ? brush_fileinfo->path->pack->filename : brush_fileinfo->path->filename);
					break;
				}
			}

			if (!s_check)	// .ENT searchpath not found = before map directory
			{
				Con_DPrintf("Loaded %s\n", entfilename);//, fileinfo->path->pack ? fileinfo->path->pack->filename : fileinfo->path->filename);
				loadmodel->entities = fileinfo->data;
				return;
			}
		}

		// no .ENT found, use the original entity list
	}
// 2001-09-12 .ENT support by Maddes  end
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}
	loadmodel->entities = Hunk_AllocName ( l->filelen + 1, "modent"); // +1 for extra NUL below
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
	loadmodel->entities[l->filelen] = 0;

	if (loadmodel->entities[l->filelen - 1] != 0)
	{
		Con_Printf ("\x02Mod_LoadEntities: ");
		Con_Printf ("missing NUL in entity lump in %s\n", loadmodel->name);
	}
}

/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)  //qbism - with bjp modifications
{
	dvertex_t	*in;
	mvertex_t	*out;
	int		i, j, count;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadVertexes", l, sizeof(*in));
	count = l->filelen / sizeof(*in);

	if (count > 65535)
	{
		Con_Printf ("\x02Mod_LoadVertexes: ");
		Con_Printf ("excessive vertexes (%d, normal max = %d) in %s\n", count, 65535, loadmodel->name);
	}

	out = Hunk_AllocName ((count+8) *sizeof(*out), "modvert"); //+8 for skyboxes

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j = 0; j < 3; ++j)
		{
			out->position[j] = LittleFloat (in->point[j]);

			// Check bounds
			if (fabs(out->position[j]) > 65536)
//				Sys_Error ("Mod_LoadVertexes: vertex way out of bounds (%.0f, max = %d) in %s", out->position[j], 4096, loadmodel->name);
			{
				Con_Printf ("\x02Mod_LoadVertexes: ");
				Con_Printf ("vertex %d,%d way out of bounds (%.0f, max = %d) in %s\n", i, j, out->position[j], 4096, loadmodel->name);
				out->position[j] = 0; // OK?
			}
		}
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int		i, j, count, firstface;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadSubmodels", l, sizeof(*in));
	count = l->filelen / sizeof(*in);

	if (count > MAX_MODELS) // Why not MAX_MAP_MODELS?
		Sys_Error ("Mod_LoadSubmodels: too many models (%d, max = %d) in %s", count, MAX_MODELS, loadmodel->name);

	if (count > 256) // Old limit
	{
		Con_Printf ("\x02Mod_LoadSubmodels: ");
		Con_Printf ("excessive models (%d, normal max = %d) in %s\n", count, 256, loadmodel->name);
	}

	out = Hunk_AllocName ( count*sizeof(*out), "modsub");

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);

		firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);

		// Check bounds
		Mod_ChkBounds ("Mod_LoadSubmodels", "face", firstface, out->numfaces, loadmodel->numsurfaces);

		out->firstface = firstface;
	}

	//qbism: johnfitz -- check world visleafs -- adapted from bjp
	out = loadmodel->submodels;

	if (out->visleafs > MAX_MAP_LEAFS)
		Sys_Error ("Mod_LoadSubmodels: too many visleafs (%d, max = %d) in %s", out->visleafs, MAX_MAP_LEAFS, loadmodel->name);

	if (out->visleafs > 8192)
		Con_DPrintf ("%i visleafs exceeds standard limit of 8192.\n", out->visleafs);
	//johnfitz
}


/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, j, count;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadEdges", l, sizeof(*in));
	count = l->filelen / sizeof(*in);
	Con_DPrintf("LoadEgdges count: %i\n", count); //qbism added
	out = Hunk_AllocName ( (count + 13) * sizeof(*out), "modedges"); // Manoel Kasimier - skyboxes - extra for skybox // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)  //qbism - this loop from bjp
	{
		for (j = 0; j < 2; ++j)
		{
			out->v[j] = (unsigned short)LittleShort (in->v[j]);

			if (i == 0 && j == 0)
				continue; // Why is sometimes this edge invalid?

			// Check bounds
			if (out->v[j] >= loadmodel->numvertexes)
			{
				Con_Printf ("\x02Mod_LoadEdges: ");
				Con_Printf ("vertex %d in edge %d out of bounds (%d, max = %d) in %s\n", j, i, out->v[j], loadmodel->numvertexes, loadmodel->name);
				out->v[j] = 0; // OK?
			}
		}
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	int		miptex;
	float	len1, len2;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadTexinfo", l, sizeof(*in));
	count = l->filelen / sizeof(*in);

	if (count > 32767)
	{
		Con_Printf ("\x02Mod_LoadTexinfo: ");
		Con_Printf ("excessive texinfo (%d, normal max = %d) in %s\n", count, 32767, loadmodel->name);
	}

	out = Hunk_AllocName ((count+6)*sizeof(*out), "modtxinf");

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
		len1 = Length (out->vecs[0]);
		len2 = Length (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;
#if 0
		if (len1 + len2 < 0.001)
			out->mipadjust = 1;		// don't crash
		else
			out->mipadjust = 1 / floor( (len1+len2)/2 + 0.1 );
#endif

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			Mod_ChkBounds ("Mod_LoadTexinfo", "miptex", miptex, 1, loadmodel->numtextures);
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}

	if (MissTex > 0)
	{
		Con_Printf ("\x02Mod_LoadTexinfo: ");
		Con_Printf ("%d texture%s missing in %s\n", MissTex, MissTex == 1 ? " is" : "s are", loadmodel->name);
	}
}

#define MAX_SURF_EXTENTS 256

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > MAX_SURF_EXTENTS)
		{
			Con_Printf ("\002CalcSurfaceExtents: ");
			Con_Printf ("excessive surface extents (%d, normal max = %d), texture %s in %s\n", s->extents[i], MAX_SURF_EXTENTS, tex->texture->name, loadmodel->name);
			s->extents[i] = MAX_SURF_EXTENTS; // Kludge, but seems to work
		}
	}
}


/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l, int lightdatasize)
{
	dface_t		*in;
	msurface_t 	*out;
	int		i, count, surfnum, texinfo, firstsurfedge;
	int			planenum, side;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadFaces", l, sizeof(*in));
	count = l->filelen / sizeof(*in);

	if (count > 32767)
	{
		Con_Printf ("\x02Mod_LoadFaces: ");
		Con_Printf ("excessive faces (%d, normal max = %d) in %s\n", count, 32767, loadmodel->name);
	}

	out = Hunk_AllocName ((count+6)*sizeof(*out), "modface");

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		firstsurfedge = LittleLong (in->firstedge);
		out->numedges = LittleShort(in->numedges);

		// Check bounds
		Mod_ChkBounds ("MOD_LoadFaces", "surfedge", firstsurfedge, out->numedges, loadmodel->numsurfedges);

		out->firstedge = firstsurfedge;
		out->flags = 0;

		planenum = LittleShort(in->planenum);

		Mod_ChkBounds ("MOD_LoadFaces", "plane", planenum, 1, loadmodel->numplanes);

		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		texinfo = LittleShort (in->texinfo);

		Mod_ChkBounds ("MOD_LoadFaces", "texinfo", texinfo, 1, loadmodel->numtexinfo);

		out->texinfo = loadmodel->texinfo + texinfo;

		CalcSurfaceExtents (out);

	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);

		if (i != -1)
		{
			if (!WithinBounds (i, 1, lightdatasize, NULL))
			{
				Con_Printf ("\002MOD_LoadFaces: ");
				Con_Printf ("lightofs in face %d out of bounds (%d, max = %d) in %s\n", surfnum, i, lightdatasize, loadmodel->name);
				i = -1;
			}
		}

		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + i;

	// set the drawing flags flag

		if (!Q_strncmp(out->texinfo->texture->name,"sky",3))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			continue;
		}

		if (out->texinfo->texture->name[0] == '*')		// turbulent
		{
			// Manoel Kasimier - translucent water - begin
			if (Q_strncmp(out->texinfo->texture->name,"*lava",5)) //lava should be opaque
				out->flags |= SURF_DRAWTRANSLUCENT;
			// Manoel Kasimier - translucent water - end
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			continue;
		}
	}
}

static unsigned RecursLevel;

/*
=================
Mod_SetParent
=================
*/
void static Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	if (!node || ++RecursLevel > 4096) // 512 seems enough for huge maps and 8192 might create stack overflow
		Sys_Error ("Mod_SetParent: %s in %s", !node ? "invalid node" : "excessive tree depth", loadmodel->name);

	node->parent = parent;
	if (node->contents >= 0)
	{
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

	--RecursLevel;
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)  //qbism - with bjp modifications
{
	int	i, j, count, p, firstface;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadNodes", l, sizeof(*in));
	count = l->filelen / sizeof(*in);

	// Check nodes
	if (count > 32767)
//		Sys_Error ("MOD_LoadNodes: too many nodes (%d, max = %d) in %s", count, 32767, loadmodel->name);
	{
		Con_Printf ("\x02Mod_LoadNodes: ");
		Con_Printf ("excessive nodes (%d, normal max = %d) in %s\n", count, 32767, loadmodel->name);
	}

	out = Hunk_AllocName ( count*sizeof(*out), "modnode");

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);

		Mod_ChkBounds ("MOD_LoadNodes", "plane", p, 1, loadmodel->numplanes);

		out->plane = loadmodel->planes + p;

		firstface = (unsigned short)LittleShort (in->firstface);
		out->numsurfaces = (unsigned short)LittleShort (in->numfaces);

		// Check bounds
		Mod_ChkBounds ("MOD_LoadNodes", "face", firstface, out->numsurfaces, loadmodel->numsurfaces);

		out->firstsurface = firstface;

		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort (in->children[j]);

			if (p < -loadmodel->numleafs) //qbism- compare to Fitzquake method/hack
				p += 65536; // Gross hack to connect as much as possible of the crippled bsp tree

			if (p >= 0)
			{
				if (p >= loadmodel->numnodes)
//					Sys_Error ("MOD_LoadNodes: node out of bounds (%d, max = %d)", p, loadmodel->numnodes);
					p = loadmodel->numnodes - 1;

				out->children[j] = loadmodel->nodes + p;
			}
				else
				{
				if ((-1 - p) >= loadmodel->numleafs)
//					Sys_Error ("MOD_LoadNodes: leaf out of bounds (%d, max = %d)", (-1 - p), loadmodel->numleafs);
					p = -loadmodel->numleafs;

				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
			}
		}
	}

	RecursLevel = 0;
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

void Mod_ProcessLeafs (dleaf_t *in, int filelen);	// 2001-12-28 .VIS support by Maddes
/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadLeafs", l, sizeof(*in)); //qbism- from bjp
	Mod_ProcessLeafs (in, l->filelen);
}


void Mod_ProcessLeafs (dleaf_t *in, int filelen)
{
	mleaf_t 	*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Sys_Error ("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);
	count = filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), "USE_LEAF");
// 2001-12-28 .VIS support by Maddes  end

	//qbism:  check leafs limit from johnfitz
	if (count > 32767)
		Host_Error ("Mod_LoadLeafs: %i leafs exceeds limit of 32767.\n", count);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
            (unsigned short)LittleShort(in->firstmarksurface); //qbism:  johnfitz -- unsigned short
		out->nummarksurfaces = (unsigned short)LittleShort(in->nummarksurfaces); //qbism:  johnfitz -- unsigned short

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}


/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in;
	mclipnode_t *out; //qbism:  johnfitz -- was dclipnode_t
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), "modcnode");

	//qbism:  johnfitz -- warn about exceeding old limits
	if (count > 32767)
		Con_DPrintf("%i clipnodes exceeds standard limit of 32767.\n", count);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);

		//qbism:  johnfitz -- bounds check
		if (out->planenum < 0 || out->planenum >= loadmodel->numplanes)
			Host_Error ("Mod_LoadClipnodes: planenum out of bounds");

		//qbism:  johnfitz begin -- support clipnodes > 32k
		out->children[0] = (unsigned short)LittleShort(in->children[0]);
		out->children[1] = (unsigned short)LittleShort(in->children[1]);
		if (out->children[0] >= count)
			out->children[0] -= 65536;
		if (out->children[1] >= count)
			out->children[1] -= 65536;
		//qbism:  johnfitz end
	}
}

/*
=================
Mod_MakeHull0

Duplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	mclipnode_t *out; //qbism:  johnfitz -- was dclipnode_t
	int			i, j, count;
	hull_t		*hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName ( count*sizeof(*out), "modhull0");

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)  //qbism - with bjp modifications
{
	int		i, j, count;
	unsigned short	*in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadMarksurfaces", l, sizeof(*in));
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), "modmsurf");

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	//qbism:  johnfitz -- warn mappers about exceeding old limits
	if (count > 32767)
		Con_DPrintf("%i marksurfaces exceeds standard limit of 32767.\n", count);

	for ( i=0 ; i<count ; i++)
	{
		j = (unsigned short)LittleShort(in[i]);
		Mod_ChkBounds ("Mod_LoadMarksurfaces", "face", j, 1, loadmodel->numsurfaces);
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l) //qbism - with bjp check
{
	int		i, count;
	int		*in, *out;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadSurfedges", l, sizeof(*in));
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count+24)*sizeof(*out), "modsedge"); // Manoel Kasimier - skyboxes - extra for skybox // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
	{
		out[i] = LittleLong (in[i]);

		// Check bounds
		if (abs(out[i]) >= loadmodel->numedges)
		{
			Con_Printf ("\x02Mod_LoadSurfedges: ");
			Con_Printf ("edge in surfedge %d out of bounds (%d, max = %d) in %s\n", i, out[i], loadmodel->numedges, loadmodel->name);
			out[i] = 0; // OK?
		}
	}
}

/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;

	in = (void *)(mod_base + l->fileofs);
	Mod_ChkLumpSize ("Mod_LoadPlanes", l, sizeof(*in));
	count = l->filelen / sizeof(*in);

	if (count > 32767)
	{
		Con_Printf ("\x02Mod_LoadPlanes: ");
		Con_Printf ("excessive planes (%d, normal max = %d) in %s\n", count, 32767, loadmodel->name);
	}

	//qbism - bjp is out = Hunk_AllocName ( count*2*sizeof(*out), "modplan");  ...why *2 ?
	out = Hunk_AllocName ( (count+6)*sizeof(*out), "modplane"); // Manoel Kasimier - skyboxes - extra for skybox
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length (corner);
}

// 2001-12-28 .VIS support by Maddes  start
/*
=================
Mod_LoadExternalVisibility
=================
*/
void Mod_LoadExternalVisibility (int fhandle)
{
	long	filelen;

	// get visibility data length
	filelen = 0;
	Sys_FileRead (fhandle, &filelen, 4);
	filelen = LittleLong(filelen);

	Con_DPrintf("...%i bytes visibility data\n", filelen);

	// load visibility data
	if (!filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName ( filelen, "EXT_VIS");
	loadmodel->visdata = Mod_Alloc ("Mod_LoadVisibility", filelen);
	Sys_FileRead (fhandle, loadmodel->visdata, filelen);
}



/*
=================
Mod_LoadExternalLeafs
=================
*/
void Mod_LoadExternalLeafs (int fhandle)
{
	dleaf_t 	*in;
	long	filelen;

	// get leaf data length
	filelen = 0;
	Sys_FileRead (fhandle, &filelen, 4);
	filelen = LittleLong(filelen);

	Con_DPrintf("...%i bytes leaf data\n", filelen);

	// load leaf data
	if (!filelen)
	{
		loadmodel->leafs = NULL;
		loadmodel->numleafs = 0;
		return;
	}
	in = Hunk_AllocName (filelen, "EXT_LEAF");
	Sys_FileRead (fhandle, in, filelen);

	Mod_ProcessLeafs (in, filelen);
}

int Mod_FindExternalVIS (loadedfile_t *brush_fileinfo)
{
	char	visfilename[1024];
	char	vispathname[MAX_OSPATH]; // Manoel Kasimier
	int		fhandle;
	int		len, i, pos;
	searchpath_t	*s_vis;
	vispatch_t	header;
	char	mapname[VISPATCH_MAPNAME_LENGTH+5];	// + ".vis" + EoS

	fhandle = -1;

	if (external_vis.value)
	{
		// check for a .VIS file
		Q_strcpy(visfilename, loadmodel->name);
		COM_StripExtension(visfilename, visfilename);
		Q_strcat(visfilename, ".vis");

		len = COM_OpenFile (visfilename, &fhandle, &s_vis);
		if (fhandle == -1)	// check for a .VIS file with map's directory name (e.g. ID1.VIS)
		{
		//	Q_strcpy(visfilename, "maps/"); // Manoel Kasimier - removed
		//	Q_strcat(visfilename, COM_SkipPath(brush_fileinfo->path->filename)); // Manoel Kasimier - removed

			// Manoel Kasimier - begin
			// wrong: quake\gamedir\maps\gamedir.vis - don't load from the "maps" directory, to prevent mistakes if there's a map named "gamedir.bsp"
			// right: quake\gamedir\gamedir.vis - we'll look for the VIS packs directly into the game directory
			if (brush_fileinfo->path->pack)
			{
				Q_strcpy(visfilename, brush_fileinfo->path->pack->filename);
				// remove the packfile from the path
				for (i=Q_strlen(visfilename); i > 0 ; i--)
					if (visfilename[i] == '/' || visfilename[i] == '\\')
					{
						visfilename[i] = 0;
						break;
					}
			//	Q_strcpy(vispathname, visfilename);
				Q_strcpy(visfilename, COM_SkipPath(visfilename));
			}
			else
			{
			//	Q_strcpy(vispathname, brush_fileinfo->path->filename);
				Q_strcpy(visfilename, COM_SkipPath(brush_fileinfo->path->filename));
			}
		/*	// make sure it is in lowercase
			for (i=Q_strlen(visfilename); i > 0 ; i--)
				if (visfilename[i] >= 'A' && visfilename[i] <= 'Z')
					visfilename[i] += ('a' - 'A');
		*/	// Manoel Kasimier - end

			Q_strcat(visfilename, ".vis");
			len = COM_OpenFile (visfilename, &fhandle, &s_vis);
		}
		// Manoel Kasimier - begin
		else if (fhandle >= 0)
		{
			Q_strcpy(vispathname, brush_fileinfo->path->pack ? brush_fileinfo->path->pack->filename : brush_fileinfo->path->filename);
			// get the full path to the game dir where the .bsp file is
			for (i=Q_strlen(vispathname); i > 0 ; i--)
				if (vispathname[i] == '/' || vispathname[i] == '\\')
				{
					vispathname[i] = 0;
					break;
				}
			Q_strcpy(visfilename, s_vis->pack ? s_vis->pack->filename : s_vis->filename);
			// get the full path to the game dir where the .vis file is
			for (i=Q_strlen(visfilename); i > 0 ; i--)
				if (visfilename[i] == '/' || visfilename[i] == '\\')
				{
					visfilename[i] = 0;
					break;
				}
			if (Q_strcasecmp(vispathname, visfilename)) // different game directories
			{
				COM_CloseFile(fhandle);
				return -1;
			}
		}
		// Manoel Kasimier - end

		if (fhandle >= 0)
		{
			// check file for size
			if (len <= 0)
			{
				COM_CloseFile(fhandle);
				fhandle = -1;
			}
		}

		if (fhandle >= 0)
		{
			// search map in visfile
			Q_strncpy(mapname, loadname, VISPATCH_MAPNAME_LENGTH);
			mapname[VISPATCH_MAPNAME_LENGTH] = 0;
			Q_strcat(mapname, ".bsp");

			pos = 0;
			while ((i = Sys_FileRead (fhandle, &header, sizeof(struct vispatch_s))) == sizeof(struct vispatch_s))
			{
				header.filelen = LittleLong(header.filelen);

				pos += i;

				if (!Q_strncasecmp (header.mapname, mapname, VISPATCH_MAPNAME_LENGTH))	// found
				{
					break;
				}

				pos += header.filelen;

				Sys_FileSeek(fhandle, pos);
			}

			if (i != sizeof(struct vispatch_s))
			{
				COM_CloseFile(fhandle);
				fhandle = -1;
			}
		}

		if (fhandle >= 0)
		{
			Con_DPrintf("Loaded %s for %s from %s\n", visfilename, mapname, s_vis->pack ? s_vis->pack->filename : s_vis->filename);
		}
	}

	return fhandle;
}
// 2001-12-28 .VIS support by Maddes  end
static char *LumpDesc[] =
{
	"entities",
	"planes",
	"textures",
	"vertexes",
	"visdata",
	"nodes",
	"texinfo",
	"faces",
	"lighting",
	"clipnodes",
	"leafs",
	"marksurfaces",
	"edges",
	"surfedges",
	"models",
};

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer, loadedfile_t *brush_fileinfo)	// 2001-09-12 .ENT support by Maddes
{
	int			i, j;
	dheader_t	*header;
	dmodel_t 	*bm;
	int			fhandle;	// 2001-12-28 .VIS support by Maddes

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	// Check header is inside file buffer
	Mod_ChkFileSize ("Mod_LoadBrushModel", "header", 0, sizeof(dheader_t));

	i = LittleLong (header->version);
	if (i != BSPVERSION)
//		Sys_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);
	// MrG - incorrect BSP version is no longer fatal - begin
	{
		Con_Printf("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)\n", mod->name, i, BSPVERSION);
		mod->numvertexes=-1;	// HACK
		return;
	}
	// MrG - incorrect BSP version is no longer fatal - end

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	// Check all lumps are OK and inside file buffer
	for (i = 0; i < HEADER_LUMPS; ++i)
		Mod_ChkFileSize ("Mod_LoadBrushModel", LumpDesc[i], header->lumps[i].fileofs, header->lumps[i].filelen);

// load into heap

	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES], header->lumps[LUMP_LIGHTING].filelen);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
// 2001-12-28 .VIS support by Maddes  start
	loadmodel->visdata = NULL;
	loadmodel->leafs = NULL;
	loadmodel->numleafs = 0;

	fhandle = Mod_FindExternalVIS (brush_fileinfo);
	if (fhandle >= 0)
	{
		Mod_LoadExternalVisibility (fhandle);
		Mod_LoadExternalLeafs (fhandle);
	}

	if ((loadmodel->visdata == NULL)
	    || (loadmodel->leafs == NULL)
	    || (loadmodel->numleafs == 0))
	{
		if (fhandle >= 0)
		{
			Con_Printf("External VIS data are invalid!!!\n");
		}
// 2001-12-28 .VIS support by Maddes  end
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
// 2001-12-28 .VIS support by Maddes  start
	}

	if (fhandle >= 0)
	{
		COM_CloseFile(fhandle);
	}
// 2001-12-28 .VIS support by Maddes  end
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES], brush_fileinfo);	// 2001-09-12 .ENT support by Maddes
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;		// regular and alternate animation
	mod->flags = 0;

//
// set up the submodels (FIXME: this is confusing)
//
	// johnfitz -- okay, so that i stop getting confused every time i look at this loop, here's how it works:
	// we're looping through the submodels starting at 0.  Submodel 0 is the main model, so we don't have to
	// worry about clobbering data the first time through, since it's the same data.  At the end of the loop,
	// we create a new copy of the data to use the next time through.
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			sprintf (name, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			Q_strcpy (loadmodel->name, name);
			Mod_AllocClear (loadmodel);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/



/*
=================
Mod_LoadAliasFrame //qbism - with bjp modifications
=================
*/
void * Mod_LoadAliasFrame (void * pin, int *pframeindex, int numv,
	trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	trivertx_t		*pframe, *pinframe;
	int				i, j;
	daliasframe_t	*pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	// Check frame is inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasFrame", "frame", (byte *)pdaliasframe - mod_base, sizeof(daliasframe_t));

	Q_strcpy (name, pdaliasframe->name);

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about
	// endianness
		pbboxmin->v[i] = pdaliasframe->bboxmin.v[i];
		pbboxmax->v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	// Check trivertexes are inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasFrame", "trivertexes", (byte *)pinframe - mod_base, numv * sizeof(trivertx_t));

	pframe = Hunk_AllocName (numv * sizeof(*pframe), "modfrm1");

	*pframeindex = (byte *)pframe - (byte *)pheader;

	for (j=0 ; j<numv ; j++)
	{
		int		k;

	// these are all byte values, so no need to deal with endianness
		pframe[j].lightnormalindex = pinframe[j].lightnormalindex;

		for (k=0 ; k<3 ; k++)
		{
			pframe[j].v[k] = pinframe[j].v[k];
		}
	}

	pinframe += numv;

	return (void *)pinframe;
}


/*
=================
Mod_LoadAliasGroup //qbism - with bjp modifications
=================
*/
void * Mod_LoadAliasGroup (void * pin, int *pframeindex, int numv,
	trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	daliasgroup_t		*pingroup;
	maliasgroup_t		*paliasgroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (daliasgroup_t *)pin;

	// Check group is inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasGroup", "group", (byte *)pingroup - mod_base, sizeof(daliasgroup_t));

	numframes = LittleLong (pingroup->numframes);

	paliasgroup = Hunk_AllocName (sizeof (maliasgroup_t) +
			(numframes - 1) * sizeof (paliasgroup->frames[0]), "modfrm2");

	paliasgroup->numframes = numframes;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		pbboxmin->v[i] = pingroup->bboxmin.v[i];
		pbboxmax->v[i] = pingroup->bboxmax.v[i];
	}

	*pframeindex = (byte *)paliasgroup - (byte *)pheader;

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	// Check intervals are inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasGroup", "intervals", (byte *)pin_intervals - mod_base, numframes * sizeof(daliasinterval_t));

	poutintervals = Hunk_AllocName (numframes * sizeof (float), "modfint");

	paliasgroup->intervals = (byte *)poutintervals - (byte *)pheader;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadAliasGroup: interval %f <= 0 in %s", *poutintervals, name);

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		paliasgroup->frames[i].numframes = numframes; // Manoel Kasimier - model interpolation

		ptemp = Mod_LoadAliasFrame (ptemp,
									&paliasgroup->frames[i].frame,
									numv,
									&paliasgroup->frames[i].bboxmin,
									&paliasgroup->frames[i].bboxmax,
									pheader, name);
	}

	return ptemp;
}


/*
=================
Mod_LoadAliasSkin
=================
*/
void * Mod_LoadAliasSkin (void * pin, int *pskinindex, int skinsize,
	aliashdr_t *pheader)
{
	int		i;
	byte		*pskin, *pinskin;
	unsigned short	*pusskin;

	pskin = Hunk_AllocName (skinsize * r_pixbytes, "modskin2");
	pinskin = (byte *)pin;

	// Check skin is inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasSkin", "skin", (byte *)pinskin - mod_base, skinsize);

	*pskinindex = (byte *)pskin - (byte *)pheader;

	if (r_pixbytes == 1)
	{
		memcpy (pskin, pinskin, skinsize);
	}
	else if (r_pixbytes == 2)
	{
		pusskin = (unsigned short *)pskin;

		for (i=0 ; i<skinsize ; i++)
			pusskin[i] = d_8to16table[pinskin[i]];
	}
	else
	{
		Sys_Error ("Mod_LoadAliasSkin: driver set invalid r_pixbytes %d in %s",
				 r_pixbytes, loadmodel->name);
	}

	pinskin += skinsize;

	return ((void *)pinskin);
}

/*
=================
Mod_LoadAliasSkinGroup
=================
*/
void * Mod_LoadAliasSkinGroup (void * pin, int *pskinindex, int skinsize,
	aliashdr_t *pheader)
{
	daliasskingroup_t		*pinskingroup;
	maliasskingroup_t		*paliasskingroup;
	int						i, numskins;
	daliasskininterval_t	*pinskinintervals;
	float					*poutskinintervals;
	void					*ptemp;

	pinskingroup = (daliasskingroup_t *)pin;

	// Check group is inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasSkinGroup", "group", (byte *)pinskingroup - mod_base, sizeof(daliasskingroup_t));

	numskins = LittleLong (pinskingroup->numskins);

	paliasskingroup = Hunk_AllocName (sizeof (maliasskingroup_t) +
			(numskins - 1) * sizeof (paliasskingroup->skindescs[0]),
			"modskin3");

	paliasskingroup->numskins = numskins;

	*pskinindex = (byte *)paliasskingroup - (byte *)pheader;

	pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

	// Check intervals are inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasSkinGroup", "intervals", (byte *)pinskinintervals - mod_base, numskins * sizeof(daliasskininterval_t));

	poutskinintervals = Hunk_AllocName (numskins * sizeof (float), "modsint");

	paliasskingroup->intervals = (byte *)poutskinintervals - (byte *)pheader;

	for (i=0 ; i<numskins ; i++)
	{
		*poutskinintervals = LittleFloat (pinskinintervals->interval);
		if (*poutskinintervals <= 0)
			Sys_Error ("Mod_LoadAliasSkinGroup: interval %f <= 0 in %s", *poutskinintervals, loadmodel->name);

		poutskinintervals++;
		pinskinintervals++;
	}

	ptemp = (void *)pinskinintervals;

	for (i=0 ; i<numskins ; i++)
	{
		ptemp = Mod_LoadAliasSkin (ptemp,
				&paliasskingroup->skindescs[i].skin, skinsize, pheader);
	}

	return ptemp;
}


/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i;
	mdl_t				*pmodel, *pinmodel;
	stvert_t			*pstverts, *pinstverts;
	aliashdr_t			*pheader;
	mtriangle_t			*ptri;
	dtriangle_t			*pintriangles;
	int					version, numframes, numskins;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	maliasskindesc_t	*pskindesc;
	int					skinsize;
	int					start, end, total;

	start = Hunk_LowMark ();

	mod_base = (byte *)buffer;
	pinmodel = (mdl_t *)buffer;

	// Check version+header is inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasModel", "model header", 0, sizeof(int) + sizeof(mdl_t));

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error ("Mod_LoadAliasModel: %s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t) + (LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]) +
			sizeof (mdl_t) +
			LittleLong (pinmodel->numverts) * sizeof (stvert_t) +
			LittleLong (pinmodel->numtris) * sizeof (mtriangle_t);

	pheader = Hunk_AllocName (size, "modmdl");
	pmodel = (mdl_t *) ((byte *)&pheader[1] +
			(LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]));

	//qbism from DP-  convert model flags to EF flags (MF_ROCKET becomes EF_ROCKET, etc)
	i = LittleLong (pinmodel->flags);
	mod->flags = ((i & 255) << 24) | (i & 0x00FFFF00);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pmodel->boundingradius = LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->numskins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("Mod_LoadAliasModel: model %s has a skin taller than %d (%d)", mod->name,
				   MAX_LBM_HEIGHT, pmodel->skinheight);


	pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
		Sys_Error ("Mod_LoadAliasModel: model %s has no vertices", mod->name);

	if (pmodel->numverts > MAXALIASVERTS)
		Sys_Error ("Mod_LoadAliasModel: model %s has too many vertices (%d, max = %d)", mod->name, pmodel->numverts, MAXALIASVERTS);


	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
		Sys_Error ("Mod_LoadAliasModel: model %s has no triangles", mod->name);

	pmodel->numframes = LittleLong (pinmodel->numframes);
	pmodel->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pmodel->scale[i] = LittleFloat (pinmodel->scale[i]);
		pmodel->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pmodel->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	numskins = pmodel->numskins;
	numframes = pmodel->numframes;

//	if (pmodel->skinwidth & 0x03)
//		Sys_Error ("Mod_LoadAliasModel: skinwidth %d not multiple of 4 in %s", pmodel->skinwidth, mod->name);

	pheader->model = (byte *)pmodel - (byte *)pheader;

//
// load the skins
//
	skinsize = pmodel->skinheight * pmodel->skinwidth;

	if (numskins < 1)
		Sys_Error ("Mod_LoadAliasModel: invalid # of skins %d in %s", numskins, mod->name);

	pskintype = (daliasskintype_t *)&pinmodel[1];

	pskindesc = Hunk_AllocName (numskins * sizeof (maliasskindesc_t),
								"modskin1");

	pheader->skindesc = (byte *)pskindesc - (byte *)pheader;

	for (i=0 ; i<numskins ; i++)
	{
		aliasskintype_t	skintype;
		char		Str[256];

		sprintf (Str, "skin %d", i);
		Mod_ChkFileSize ("Mod_LoadAliasModel", Str, (byte *)pskintype - mod_base, sizeof(aliasskintype_t));

		skintype = LittleLong (pskintype->type);
		pskindesc[i].type = skintype;

		Mod_ChkType ("Mod_LoadAliasModel", "skintype", Str, skintype, ALIAS_SKIN_SINGLE, ALIAS_SKIN_GROUP);

		if (skintype == ALIAS_SKIN_SINGLE)
		{
			pskintype = (daliasskintype_t *)
					Mod_LoadAliasSkin (pskintype + 1,
									   &pskindesc[i].skin,
									   skinsize, pheader);
		}
		else
		{
			pskintype = (daliasskintype_t *)
					Mod_LoadAliasSkinGroup (pskintype + 1,
											&pskindesc[i].skin,
											skinsize, pheader);
		}
	}

//
// set base s and t vertices
//
	pstverts = (stvert_t *)&pmodel[1];
	pinstverts = (stvert_t *)pskintype;

	// Check vertices are inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasModel", "vertices", (byte *)pinstverts - mod_base, pmodel->numverts * sizeof(stvert_t));

	pheader->stverts = (byte *)pstverts - (byte *)pheader;

	for (i=0 ; i<pmodel->numverts ; i++)
	{
		int s = LittleLong (pinstverts[i].s);
		int t = LittleLong (pinstverts[i].t);

//		if ((s & 0xFFFF0000) || (t & 0xFFFF0000) || t > MAX_LBM_HEIGHT) // Is this correct?
//			Sys_Error ("Mod_LoadAliasModel: invalid s/t values (%d/%d, max = %d/%d) for vertice %d in %s", s, t, 65535, MAX_LBM_HEIGHT, i, mod->name);
		if ((t & 0xFFFF0000) || t > MAX_LBM_HEIGHT)
		{
//			Sys_Error ("Mod_LoadAliasModel: invalid t value (%d, max = %d) for vertice %d in %s", t, MAX_LBM_HEIGHT, i, mod->name);
			Con_Printf ("\002Mod_LoadAliasModel: ");
			Con_Printf ("invalid t value (%d, max = %d) for vertice %d in %s\n", t, MAX_LBM_HEIGHT, i, mod->name);
			t = 0; // OK?
		}

		pstverts[i].onseam = LittleLong (pinstverts[i].onseam);
	// put s and t in 16.16 format
		pstverts[i].s = s << 16;
		pstverts[i].t = t << 16;
	}

//
// set up the triangles
//
	ptri = (mtriangle_t *)&pstverts[pmodel->numverts];
	pintriangles = (dtriangle_t *)&pinstverts[pmodel->numverts];

	// Check triangles are inside file buffer
	Mod_ChkFileSize ("Mod_LoadAliasModel", "triangles", (byte *)pintriangles - mod_base, pmodel->numtris * sizeof(dtriangle_t));

	pheader->triangles = (byte *)ptri - (byte *)pheader;

	for (i=0 ; i<pmodel->numtris ; i++)
	{
		int		j;

		ptri[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j=0 ; j<3 ; j++)
		{
			ptri[i].vertindex[j] =
					LittleLong (pintriangles[i].vertindex[j]);

			if (ptri[i].vertindex[j] < 0 || ptri[i].vertindex[j] >= MAXALIASVERTS)
				Sys_Error ("Mod_LoadAliasModel: invalid ptri[%d].vertindex[%d] (%d, max = %d) in %s", i, j, ptri[i].vertindex[j], MAXALIASVERTS, mod->name);
		}
	}

//
// load the frames
//
	if (numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: invalid # of frames %d in %s", numframes, mod->name);

	pframetype = (daliasframetype_t *)&pintriangles[pmodel->numtris];

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;
		char		 Str[256];

		sprintf (Str, "frame %d", i);
		Mod_ChkFileSize ("Mod_LoadAliasModel", Str, (byte *)pframetype - mod_base, sizeof(aliasframetype_t));

		frametype = LittleLong (pframetype->type);
		pheader->frames[i].type = frametype;

		Mod_ChkType ("Mod_LoadAliasModel", "frametype", Str, frametype, ALIAS_SINGLE, ALIAS_GROUP);

		if (frametype == ALIAS_SINGLE)
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasFrame (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
		else
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasGroup (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
	}

	mod->type = mod_alias;

// FIXME: do this right
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;

//
// move the complete, relocatable alias model to the cache
//
	end = Hunk_LowMark ();
	total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}

//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					i, width, height, size, origin[2];
	unsigned short		*ppixout;
	byte				*ppixin;
	char		str[256];

	pinframe = (dspriteframe_t *)pin;

	// Check sprite frame is inside file buffer
	sprintf (str, "frame %d", framenum);
	Mod_ChkFileSize ("Mod_LoadSpriteFrame", str, (byte *)pinframe - mod_base, sizeof(dspriteframe_t));

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	Mod_ChkFileSize ("Mod_LoadSpriteFrame", str, (byte *)(pinframe + 1) - mod_base, size);
	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t) + size*r_pixbytes, "modspr2");

	memset (pspriteframe, 0, sizeof (mspriteframe_t) + size);
	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	if (r_pixbytes == 1)
	{
		memcpy (&pspriteframe->pixels[0], (byte *)(pinframe + 1), size);
	}
	else if (r_pixbytes == 2)
	{
		ppixin = (byte *)(pinframe + 1);
		ppixout = (unsigned short *)&pspriteframe->pixels[0];

		for (i=0 ; i<size ; i++)
			ppixout[i] = d_8to16table[ppixin[i]];
	}
	else
	{
		Sys_Error ("Mod_LoadSpriteFrame: driver set invalid r_pixbytes %d in %s",
				 r_pixbytes, loadmodel->name);
	}

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (sizeof (mspritegroup_t) +
				(numframes - 1) * sizeof (pspritegroup->frames[0]), "modspr3");

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), "modint");

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadSpriteGroup: interval %f <= 0 in %s", *poutintervals, loadmodel->name);

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;

	mod_base = (byte *)buffer;
	pin = (dsprite_t *)buffer;

	// Check version+header is inside file buffer
	Mod_ChkFileSize ("Mod_LoadSpriteModel", "sprite header", 0, sizeof(int) + sizeof(dsprite_t));

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION)
		Sys_Error ("Mod_LoadSpriteModel: %s has wrong version number "
				 "(%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, "modspr1");

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

//
// load the frames
//
	if (numframes < 1)
		Sys_Error ("Mod_LoadSpriteModel: invalid # of frames %d in %s", numframes, mod->name);

	mod->numframes = numframes;
	mod->flags = 0;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;
		char		  Str[256];

		sprintf (Str, "frame %d", i);
		Mod_ChkFileSize ("Mod_LoadSpriteModel", Str, (byte *)pframetype - mod_base, sizeof(spriteframetype_t));

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		Mod_ChkType ("Mod_LoadSpriteModel", "frametype", Str, frametype, SPR_SINGLE, SPR_GROUP);

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteFrame (pframetype + 1,
										 &psprite->frames[i].frameptr, i);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteGroup (pframetype + 1,
										 &psprite->frames[i].frameptr, i);
		}
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int	i, total;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	total = 0;
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_Printf ("%8p : %s",mod->cache.data, mod->name);
		if (mod->needload & NL_UNREFERENCED)
			Con_Printf (" (!R)");
		if (mod->needload & NL_NEEDS_LOADED)
			Con_Printf (" (!P)");
		Con_Printf ("\n");
	}
}
