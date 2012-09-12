/*  Copyright (C) 1996-1997 Id Software, Inc.
    Copyright (C) 1999-2012 other authors as noted in code comments

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.   */

// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"
#include "r_local.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer, loadedfile_t *fileinfo);	// 2001-09-12 .ENT support by Maddes
//void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);
loadedfile_t *COM_LoadFile (char *path, int usehunk);

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
        Sys_Error ("Mod_Extradata: caching failed");
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
    {
        // no vis info, so make all visible
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
    }
    while (out - decompressed < row);

    return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
    if (leaf == model->leafs)
        return mod_novis;
    return Mod_DecompressVis (leaf->compressed_vis, model);
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


    for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
    {
        mod->needload = NL_UNREFERENCED;
//FIX FOR CACHE_ALLOC ERRORS:
        if (mod->type == mod_sprite) mod->cache.data = NULL;
    }
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (char *name)
{
    int		i;
    model_t	*mod;
    model_t	*avail = NULL;

    if (!name[0])
        Sys_Error ("Mod_ForName: NULL name");

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
                Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
        }
        else
            mod_numknown++;
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
        if (crash)
            Sys_Error ("Mod_LoadModel: %s not found", mod->name); //was "Mod_NumForName"

        Con_DPrintf ("Mod_LoadModel: %s not found",mod->name);  //qbism- list all not found, in debug mode.
        return NULL;
    }
    buf = (unsigned *)fileinfo->data;	// 2001-09-12 Returning information about loaded file by Maddes

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

byte	*mod_base;


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

    if (!l->filelen)
    {
        loadmodel->textures = NULL;
        return;
    }
    m = (dmiptexlump_t *)(mod_base + l->fileofs);

    m->nummiptex = LittleLong (m->nummiptex);

    loadmodel->numtextures = m->nummiptex;
    loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures) , "modtex1"); //qbism per bjp, was loadname

    for (i=0 ; i<m->nummiptex ; i++)
    {
        m->dataofs[i] = LittleLong(m->dataofs[i]);
        if (m->dataofs[i] == -1)
            continue;
        mt = (miptex_t *)((byte *)m + m->dataofs[i]);
        mt->width = LittleLong (mt->width);
        mt->height = LittleLong (mt->height);
        for (j=0 ; j<MIPLEVELS ; j++)
            mt->offsets[j] = LittleLong (mt->offsets[j]);

        if ( (mt->width & 15) || (mt->height & 15) )
            Sys_Error ("Texture %s is not 16 aligned", mt->name);
        pixels = mt->width*mt->height/64*85;
        tx = Hunk_AllocName (sizeof(texture_t) +pixels, "modtex1"); //qbism per bjp, was loadname
        loadmodel->textures[i] = tx;

        memcpy (tx->name, mt->name, sizeof(tx->name));
        tx->width = mt->width;
        tx->height = mt->height;
        for (j=0 ; j<MIPLEVELS ; j++)
            tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
        // the pixels immediately follow the structures
        memcpy ( tx+1, mt+1, pixels);

        if (!Q_strncmp(mt->name,"sky",3))
            R_InitSky (tx);
    }

//
// sequence the animations
//
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
            Sys_Error ("Bad animating texture %s", tx->name);

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
                Sys_Error ("Bad animating texture %s", tx->name);
        }

#define	ANIM_CYCLE	2
        // link them all together
        for (j=0 ; j<max ; j++)
        {
            tx2 = anims[j];
            if (!tx2)
                Sys_Error ("Missing frame %i of %s",j, tx->name);
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
            if (!tx2)
                Sys_Error ("Missing frame %i of %s",j, tx->name);
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
void Mod_LoadLighting (lump_t *l)  //qbism- colored lit load modified from Engoo
{
    int		i, j, k;
    int		r, g, b;
    float   weight, wlout;
    byte	*out, *lout, *data;
    char	litname[1024];
    loadedfile_t	*fileinfo;	// 2001-09-12 Returning information about loaded file by Maddes

    if (!l->filelen)
    {
        loadmodel->lightdata = NULL;
        return;
    }
    loadmodel->lightdata = Hunk_AllocName ( l->filelen, "modlight");
    memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
    if (cls.state != ca_dedicated)
    {
        strcpy(litname, loadmodel->name);
        COM_StripExtension(loadmodel->name, litname);
        COM_DefaultExtension(litname, ".lit");    //qbism- indexed colored
        fileinfo = COM_LoadFile(litname,0); //qbism- don't load into hunk
        if (fileinfo && ((l->filelen*3 +8) == fileinfo->filelen))
        {
            Con_DPrintf("%s loaded from %s\n", litname, fileinfo->path->pack ? fileinfo->path->pack->filename : fileinfo->path->filename);

            data = fileinfo->data;
            if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
            {
                i = LittleLong(((int *)data)[1]);
                if (i == 1)
                {
                    loadmodel->colordata = Hunk_AllocName (l->filelen+18, "modcolor"); //qbism- need some padding
                    k=8;
                    out = loadmodel->colordata;
                    lout = loadmodel->lightdata;
                    while(k <= fileinfo->filelen)
                    {
                        r = data[k++];
                        g = data[k++];
                        b = data[k++];
                        weight= r_clintensity.value/(1+r+b+g);  //qbism- flatten out the color
                        wlout = *lout*(1-r_clintensity.value);
                        *out++ = BestColor((int)(r*r*weight)+wlout, (int)(g*g*weight)+wlout, (int)(b*b*weight)+wlout, 0, 254);
                        *lout++ = max((r+g+b)/3, *lout);  //avoid large differences if colored lights don't align w/ standard.
                    }
                    Q_free(fileinfo);
                    return;
                }
                else
                    Con_Printf("Unknown .LIT file version (%d)\n", i);
            }
            else
                Con_Printf("Corrupt .LIT file (old version?), ignoring\n");
        }
        //qbism- no lit.  Still need to have something.
        loadmodel->colordata = Hunk_AllocName (l->filelen+18, "modcolor"); //qbism- need some padding
        memset (loadmodel->colordata, 1, l->filelen+18);  //qbism- fill w/ color index
    }

    /*   loadmodel->colordata = Hunk_AllocName (l->filelen+18, "modcolor"); //qbism- need some padding
       out = loadmodel->colordata;
       lout = loadmodel->lightdata;
       for(i=0 ; i < sizeof(loadmodel->lightdata); i++)
       {
           j= *lout++;
           *out++ = BestColor(j,j,j,0,254);
       }
    */
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
// 2001-12-28 .VIS support by Maddes  start
//	loadmodel->visdata = Hunk_AllocName ( l->filelen, loadname);
    loadmodel->visdata = Hunk_AllocName ( l->filelen, "INT_VIS");
// 2001-12-28 .VIS support by Maddes  end
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
// 2001-09-12 .ENT support by Maddes  start
//	loadmodel->entities = Hunk_AllocName ( l->filelen, loadname);
    loadmodel->entities = Hunk_AllocName ( l->filelen, "INT_ENT");
// 2001-09-12 .ENT support by Maddes  end
    memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
    dvertex_t	*in;
    mvertex_t	*out;
    int			i, count;

    in = (void *)(mod_base + l->fileofs);
    if (l->filelen % sizeof(*in))
        Sys_Error ("Mod_LoadVertexes: funny lump size in %s",loadmodel->name);
    count = l->filelen / sizeof(*in);
//	out = Hunk_AllocName ( count*sizeof(*out), loadname);
    out = Hunk_AllocName ( (count+8)*sizeof(*out), "modvert"); // Manoel Kasimier - skyboxes - extra for skybox // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)

    loadmodel->vertexes = out;
    loadmodel->numvertexes = count;

    for ( i=0 ; i<count ; i++, in++, out++)
    {
        out->position[0] = LittleFloat (in->point[0]);
        out->position[1] = LittleFloat (in->point[1]);
        out->position[2] = LittleFloat (in->point[2]);
    }
}



/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
    medge_t *out;
    int 	i, count;
    size_t	structsize = loadmodel->isbsp2 ? 8 : 4;  //qb: from DP


    if (l->filelen % structsize)
        Sys_Error ("Mod_LoadEdges: funny lump size in %s",loadmodel->name);
    count = l->filelen / structsize;
//	out = Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);
    out = Hunk_AllocName ( (count + 13) * sizeof(*out), "modedges"); // Manoel Kasimier - skyboxes - extra for skybox // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)

    loadmodel->edges = out;
    loadmodel->numedges = count;

    if (loadmodel->isbsp2)
    {
        dedge_t_BSP2 *in = (void *)(mod_base + l->fileofs);
        for ( i=0 ; i<count ; i++, in++, out++)
        {
            out->v[0] = (unsigned int)LittleShort(in->v[0]);
            out->v[1] = (unsigned int)LittleShort(in->v[1]);
        }
    }
    else
    {
        dedge_t *in = (void *)(mod_base + l->fileofs);
        for ( i=0 ; i<count ; i++, in++, out++)
        {
            out->v[0] = (unsigned short)LittleShort(in->v[0]);
            out->v[1] = (unsigned short)LittleShort(in->v[1]);
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
    if (l->filelen % sizeof(*in))
        Sys_Error ("Mod_LoadTexinfo: funny lump size in %s",loadmodel->name);
    count = l->filelen / sizeof(*in);
//	out = Hunk_AllocName ( count*sizeof(*out), loadname);
    out = Hunk_AllocName ( (count+6)*sizeof(*out), "modtxinf"); // Manoel Kasimier - skyboxes - extra for skybox // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
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
            if (miptex >= loadmodel->numtextures)
                Sys_Error ("miptex >= loadmodel->numtextures");
            out->texture = loadmodel->textures[miptex];
            if (!out->texture)
            {
                out->texture = r_notexture_mip; // texture not found
                out->flags = 0;
            }
        }
    }
}

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

    mins[0] = mins[1] = 9999999;
    maxs[0] = maxs[1] = -9999999;

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
        if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 256)
            Sys_Error ("Bad surface extents");
    }
}


//qb:  flag faces
void Mod_FlagFaces ( msurface_t *out)
{
    int i;
    if (!Q_strncmp(out->texinfo->texture->name,"sky",3))	// sky
    {
        out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
        return;
    }

    if (!Q_strncmp(out->texinfo->texture->name,"*",1))		// turbulent
    {
        if (!Q_strncmp(out->texinfo->texture->name, "*glass",6))
        {
            out->flags |= (SURF_DRAWTRANSLUCENT| SURF_DRAWTILED);
        }
        else
        {
            // Manoel Kasimier - translucent water - begin
            if (Q_strncmp(out->texinfo->texture->name,"*lava",5)) // lava should be opaque
                //	if (Q_strncmp(out->texinfo->texture->name,"*teleport",9)) // teleport should be opaque
                out->flags |= SURF_DRAWTRANSLUCENT;
            // Manoel Kasimier - translucent water - end
            out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
            for (i=0 ; i<2 ; i++)
            {
                out->extents[i] = 16384;
                out->texturemins[i] = -8192;
            }
            return;

        }
    }
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
    // dface_t		*in;
    msurface_t 	*out;
    int			i, count, surfnum;
    int			planenum, side;
    size_t structsize = loadmodel->isbsp2 ? 28 : 20; //qb: bsp2 from DP


    // in = (void *)(mod_base + l->fileofs);
    if (l->filelen % structsize)
        Sys_Error ("Mod_LoadFaces: funny lump size in %s",loadmodel->name);
    count = l->filelen / structsize;
//	out = Hunk_AllocName ( count*sizeof(*out), loadname);
    out = Hunk_AllocName ( (count+6)*sizeof(*out), "modface"); // Manoel Kasimier - skyboxes - extra for skybox // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
    loadmodel->surfaces = out;
    loadmodel->numsurfaces = count;

    if (loadmodel->isbsp2)  //qb: bsp2 based on DP
    {
        dface_t_BSP2 *in;
        in = (void *)(mod_base + l->fileofs);
        for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
        {
            out->firstedge = LittleLong(in->firstedge);
            out->numedges = LittleLong(in->numedges);
            out->flags = 0;
            planenum = LittleLong(in->planenum);
            side = LittleLong(in->side);
            if (side)
                out->flags |= SURF_PLANEBACK;

            out->plane = loadmodel->planes + planenum;
            out->texinfo = loadmodel->texinfo + LittleLong(in->texinfo);

            CalcSurfaceExtents (out);

            // lighting info

            for (i=0 ; i<MAXLIGHTMAPS ; i++)
                out->styles[i] = in->styles[i];
            i = LittleLong(in->lightofs);
            if (i == -1)
            {
                out->samples = NULL;
                out->colorsamples = NULL;
            }

            else
            {
                out->samples = loadmodel->lightdata + i;
                out->colorsamples = loadmodel->colordata + i; //qbism

            }
            Mod_FlagFaces (out);
        }
    }
    else
    {
        dface_t *in;
        in = (void *)(mod_base + l->fileofs);
        for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
        {
            out->firstedge = LittleLong(in->firstedge);
            out->numedges = LittleShort(in->numedges);
            out->flags = 0;
            planenum = LittleShort(in->planenum);
            side = LittleShort(in->side);
            if (side)
                out->flags |= SURF_PLANEBACK;

            out->plane = loadmodel->planes + planenum;
            out->texinfo = loadmodel->texinfo + LittleShort(in->texinfo);
            CalcSurfaceExtents (out);

            // lighting info

            for (i=0 ; i<MAXLIGHTMAPS ; i++)
                out->styles[i] = in->styles[i];
            i = LittleLong(in->lightofs);
            if (i == -1)
            {
                out->samples = NULL;
                out->colorsamples = NULL;
            }

            else
            {
                out->samples = loadmodel->lightdata + i;
                out->colorsamples = loadmodel->colordata + i; //qbism

            }
            Mod_FlagFaces(out);
        }
    }
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
    node->parent = parent;
    if (node->contents < 0)
        return;
    Mod_SetParent (node->children[0], node);
    Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
    int			i, j, count, p;
    //dnode_t		*in;
    mnode_t 	*out;
    size_t structsize = loadmodel->isbsp2 ? 44 : 24; //qb:  BSP2 from DP

    //in = (void *)(mod_base + l->fileofs);
    if (l->filelen % structsize)
        Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
    count = l->filelen / structsize;
    if (count == 0)
        Sys_Error("Mod_LoadNodes: missing BSP tree in %s",loadmodel->name);  //qb: from DP
    out = Hunk_AllocName ( count*sizeof(*out), "modnode");

    loadmodel->nodes = out;
    loadmodel->numnodes = count;

    if(loadmodel->isbsp2)
    {
        dnode_t_BSP2		*in;
        in = (void *)(mod_base + l->fileofs);
        for ( i=0 ; i<count ; i++, in++, out++)
        {
            for (j=0 ; j<3 ; j++)
            {
                out->minmaxs[j] = LittleFloat (in->mins[j]);
                out->minmaxs[3+j] = LittleFloat (in->maxs[j]);
            }

            p = LittleLong(in->planenum);
            out->plane = loadmodel->planes + p;

            out->firstsurface = (unsigned short)LittleShort (in->firstface); //qbism:  from johnfitz -- explicit cast as unsigned short
            out->numsurfaces = (unsigned short)LittleShort (in->numfaces); //qbism:  johnfitz -- explicit cast as unsigned short

            for (j=0 ; j<2 ; j++)
            {
                p = LittleLong(in->children[j]);
                if (p < count)
                    out->children[j] = loadmodel->nodes + p;
                else
                {
                    if (p < loadmodel->numleafs)
                        out->children[j] = (mnode_t *)(loadmodel->leafs + p);
                    else
                    {
                        Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
                        out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
                    }
                }
            }
        }
    }
    else
    {
        dnode_t		*in;
        in = (void *)(mod_base + l->fileofs);
        for ( i=0 ; i<count ; i++, in++, out++)
        {
            for (j=0 ; j<3 ; j++)
            {
                out->minmaxs[j] = (float)LittleShort (in->mins[j]);
                out->minmaxs[3+j] = (float)LittleShort (in->maxs[j]);
            }

            p = LittleLong(in->planenum);
            out->plane = loadmodel->planes + p;

            out->firstsurface = (unsigned short)LittleShort (in->firstface); //qbism:  from johnfitz -- explicit cast as unsigned short
            out->numsurfaces = (unsigned short)LittleShort (in->numfaces); //qbism:  johnfitz -- explicit cast as unsigned short

            for (j=0 ; j<2 ; j++)
            {
                //qbism:  johnfitz begin -- hack to handle nodes > 32k, adapted from darkplaces
                p = (unsigned short)LittleShort(in->children[j]);
                if (p < count)
                    out->children[j] = loadmodel->nodes + p;
                else
                {
                    p = 65535 - p; //note this uses 65535 intentionally, -1 is leaf 0
                    if (p < loadmodel->numleafs)
                        out->children[j] = (mnode_t *)(loadmodel->leafs + p);
                    else
                    {
                        Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
                        out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
                    }
                }
                //qbism:  johnfitz end
            }
        }
    }

    Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

void Mod_ProcessLeafs (dleaf_t *in, int filelen);	// 2001-12-28 .VIS support by Maddes
void Mod_ProcessLeafs_BSP2 (dleaf_t_BSP2 *in, int filelen); //qb: bsp2
/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
    if(loadmodel->isbsp2)
    {
        dleaf_t_BSP2 	*in;
        in = (void *)(mod_base + l->fileofs);
        Mod_ProcessLeafs_BSP2 (in, l->filelen);
    }
    else
    {
        dleaf_t 	*in;
        in = (void *)(mod_base + l->fileofs);
        Mod_ProcessLeafs (in, l->filelen);
    }
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
            out->minmaxs[j] = (float) LittleShort (in->mins[j]);
            out->minmaxs[3+j] = (float) LittleShort (in->maxs[j]);
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


void Mod_ProcessLeafs_BSP2 (dleaf_t_BSP2 *in, int filelen)  //qb: bsp2
{
    mleaf_t 	*out;
    int			i, j, count, p;

    if (filelen % sizeof(*in))
        Sys_Error ("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);
    count = filelen / sizeof(*in);
    out = Hunk_AllocName ( count*sizeof(*out), "USE_LEAF");

    loadmodel->leafs = out;
    loadmodel->numleafs = count;

    for ( i=0 ; i<count ; i++, in++, out++)
    {
        for (j=0 ; j<3 ; j++)
        {
            out->minmaxs[j] = LittleFloat (in->mins[j]);
            out->minmaxs[3+j] =  LittleFloat (in->maxs[j]);
        }

        p = LittleLong(in->contents);
        out->contents = p;

        out->firstmarksurface = loadmodel->marksurfaces +
                                (unsigned short)LittleShort(in->firstmarksurface);
        out->nummarksurfaces = (unsigned short)LittleShort(in->nummarksurfaces);

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
    //dclipnode_t *in;
    mclipnode_t *out; //qbism:  johnfitz -- was dclipnode_t
    int			i, count;
    hull_t		*hull;
    size_t structsize = loadmodel->isbsp2 ? 12 : 8;

    //in = (void *)(mod_base + l->fileofs);
    if (l->filelen % structsize)
        Sys_Error ("Mod_LoadClipnodes: funny lump size in %s",loadmodel->name);
    count = l->filelen / structsize;
    out = Hunk_AllocName ( count*sizeof(*out), "modcnode");

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

    if(loadmodel->isbsp2)
    {
        dclipnode_t_BSP2 *in;
        in = (void *)(mod_base + l->fileofs);
        for (i=0 ; i<count ; i++, out++, in++)
        {
            out->planenum = LittleLong(in->planenum);

            //qbism:  johnfitz -- bounds check
            if (out->planenum < 0 || out->planenum >= loadmodel->numplanes)
                Host_Error ("Mod_LoadClipnodes: planenum out of bounds");

            out->children[0] = LittleFloat(in->children[0]);
            out->children[1] = LittleFloat(in->children[1]);

        }
    }
    else
    {
        dclipnode_t *in;
        in = (void *)(mod_base + l->fileofs);
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
void Mod_LoadMarksurfaces (lump_t *l)
{
    int		i, j, count;
    // short		*in;
    msurface_t **out;
    size_t structsize = loadmodel->isbsp2 ? 4 : 2;

    //in = (void *)(mod_base + l->fileofs);
    if (l->filelen % structsize)
        Sys_Error ("Mod_LoadMarksurfaces: funny lump size in %s",loadmodel->name);
    count = l->filelen / structsize;
    out = Hunk_AllocName ( count*sizeof(*out), "modmsurf");

    loadmodel->marksurfaces = out;
    loadmodel->nummarksurfaces = count;

    if(loadmodel->isbsp2)
    {
        int		*in;
        in = (void *)(mod_base + l->fileofs);
        for ( i=0 ; i<count ; i++)
        {
            j = LittleLong(in[i]);
            if (j >= loadmodel->numsurfaces)
                Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
            out[i] = loadmodel->surfaces + j;
        }
    }
    else
    {
        short		*in;
        in = (void *)(mod_base + l->fileofs);
        for ( i=0 ; i<count ; i++)
        {
            j = (unsigned short)LittleShort(in[i]); //qbism:  johnfitz -- explicit cast as unsigned short
            if (j >= loadmodel->numsurfaces)
                Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
            out[i] = loadmodel->surfaces + j;
        }
    }
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{
    int		i, count;
    int		*in, *out;

    in = (void *)(mod_base + l->fileofs);
    if (l->filelen % sizeof(*in))
        Sys_Error ("Mod_LoadSurfedges: funny lump size in %s",loadmodel->name);
    count = l->filelen / sizeof(*in);
//	out = Hunk_AllocName ( count*sizeof(*out), loadname);
    out = Hunk_AllocName ( (count+24)*sizeof(*out), "modsedge"); // Manoel Kasimier - skyboxes - extra for skybox // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)

    loadmodel->surfedges = out;
    loadmodel->numsurfedges = count;

    for ( i=0 ; i<count ; i++)
        out[i] = LittleLong (in[i]);
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
    if (l->filelen % sizeof(*in))
        Sys_Error ("Mod_LoadPlanes: funny lump size in %s",loadmodel->name);
    count = l->filelen / sizeof(*in);
//	out = Hunk_AllocName ( count*2*sizeof(*out), loadname);
    out = Hunk_AllocName ( (count+6)*sizeof(*out), "modplane"); // Manoel Kasimier - skyboxes - extra for skybox // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)

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
    Sys_FileRead (fhandle, loadmodel->visdata, filelen);
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
    int			i, j, count;

    in = (void *)(mod_base + l->fileofs);
    if (l->filelen % sizeof(*in))
        Sys_Error ("Mod_LoadSubmodels: funny lump size in %s",loadmodel->name);
    count = l->filelen / sizeof(*in);
    out = Hunk_AllocName ( count*sizeof(*out), "modmodel");

    loadmodel->submodels = out;
    loadmodel->numsubmodels = count;

    for ( i=0 ; i<count ; i++, in++, out++)
    {
        for (j=0 ; j<3 ; j++)
        {
            // spread the mins / maxs by a pixel
            out->mins[j] = LittleFloat (in->mins[j]) - 1;
            out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
            out->origin[j] = LittleFloat (in->origin[j]);
        }
        for (j=0 ; j<MAX_MAP_HULLS ; j++)
            out->headnode[j] = LittleLong (in->headnode[j]);
        out->visleafs = LittleLong (in->visleafs);
        out->firstface = LittleLong (in->firstface);
        out->numfaces = LittleLong (in->numfaces);
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
    loadmodel->isbsp2 = false;

    i = LittleLong (header->version);
    if (i == 844124994 ) //qb: 'BSP2'
        loadmodel->isbsp2 = true;
    else
    if ( i != BSPVERSION)
//		Sys_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);
        // MrG - incorrect BSP version is no longer fatal - begin
    {
        Con_Printf("Mod_LoadBrushModel: %s has wrong version number (%i should be %i or BSP2)\n", mod->name, i, BSPVERSION);
        mod->numvertexes=-1;	// HACK
        return;
    }
    // MrG - incorrect BSP version is no longer fatal - end

// swap all the lumps
    mod_base = (byte *)header;

    for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
        ((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap

    Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
    Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
    Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
    Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
    Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
    Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
    Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
    Mod_LoadFaces (&header->lumps[LUMP_FACES]);
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
        {
            // duplicate the basic information
            char	name[10];

            sprintf (name, "*%i", i+1);
            loadmodel = Mod_FindName (name);
            *loadmodel = *mod;
            Q_strcpy (loadmodel->name, name);
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
Mod_LoadAliasFrame
=================
*/
void * Mod_LoadAliasFrame (void * pin, int *pframeindex, int numv,
                           trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
    trivertx_t		*pframe, *pinframe;
    int				i, j;
    daliasframe_t	*pdaliasframe;

    pdaliasframe = (daliasframe_t *)pin;

    Q_strcpy (name, pdaliasframe->name);

    for (i=0 ; i<3 ; i++)
    {
        // these are byte values, so we don't have to worry about
        // endianness
        pbboxmin->v[i] = pdaliasframe->bboxmin.v[i];
        pbboxmax->v[i] = pdaliasframe->bboxmax.v[i];
    }

    pinframe = (trivertx_t *)(pdaliasframe + 1);
    pframe = Hunk_AllocName (numv * sizeof(*pframe), "modframe");

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
Mod_LoadAliasGroup
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

    numframes = LittleLong (pingroup->numframes);

    paliasgroup = Hunk_AllocName (sizeof (maliasgroup_t) +
                                  (numframes - 1) * sizeof (paliasgroup->frames[0]), "modgroup");

    paliasgroup->numframes = numframes;

    for (i=0 ; i<3 ; i++)
    {
        // these are byte values, so we don't have to worry about endianness
        pbboxmin->v[i] = pingroup->bboxmin.v[i];
        pbboxmax->v[i] = pingroup->bboxmax.v[i];
    }

    *pframeindex = (byte *)paliasgroup - (byte *)pheader;

    pin_intervals = (daliasinterval_t *)(pingroup + 1);

    poutintervals = Hunk_AllocName (numframes * sizeof (float), "modfram2");

    paliasgroup->intervals = (byte *)poutintervals - (byte *)pheader;

    for (i=0 ; i<numframes ; i++)
    {
        *poutintervals = LittleFloat (pin_intervals->interval);
        if (*poutintervals <= 0.0)
            Sys_Error ("Mod_LoadAliasGroup: interval<=0");

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
    byte	*pskin, *pinskin;
    unsigned short	*pusskin;

    pskin = Hunk_AllocName (skinsize * r_pixbytes, loadname);
    pinskin = (byte *)pin;
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
        Sys_Error ("Mod_LoadAliasSkin: driver set invalid r_pixbytes: %d\n",
                   r_pixbytes);
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

    numskins = LittleLong (pinskingroup->numskins);

    paliasskingroup = Hunk_AllocName (sizeof (maliasskingroup_t) +
                                      (numskins - 1) * sizeof (paliasskingroup->skindescs[0]),
                                      loadname);

    paliasskingroup->numskins = numskins;

    *pskinindex = (byte *)paliasskingroup - (byte *)pheader;

    pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

    poutskinintervals = Hunk_AllocName (numskins * sizeof (float),"numskin");

    paliasskingroup->intervals = (byte *)poutskinintervals - (byte *)pheader;

    for (i=0 ; i<numskins ; i++)
    {
        *poutskinintervals = LittleFloat (pinskinintervals->interval);
        if (*poutskinintervals <= 0)
            Sys_Error ("Mod_LoadAliasSkinGroup: interval<=0");

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

    pinmodel = (mdl_t *)buffer;

    version = LittleLong (pinmodel->version);
    if (version != ALIAS_VERSION)
        Sys_Error ("%s has wrong version number (%i should be %i)",
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

    pheader = Hunk_AllocName (size, loadname);
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
        Sys_Error ("model %s has a skin taller than %d", mod->name,
                   MAX_LBM_HEIGHT);

    pmodel->numverts = LittleLong (pinmodel->numverts);

    if (pmodel->numverts <= 0)
        Sys_Error ("model %s has no vertices", mod->name);

    if (pmodel->numverts > MAXALIASVERTS)
        Sys_Error ("model %s has too many vertices", mod->name);

    pmodel->numtris = LittleLong (pinmodel->numtris);

    if (pmodel->numtris <= 0)
        Sys_Error ("model %s has no triangles", mod->name);

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

    if (pmodel->skinwidth & 0x03)
        Sys_Error ("Mod_LoadAliasModel: skinwidth not multiple of 4");

    pheader->model = (byte *)pmodel - (byte *)pheader;

//
// load the skins
//
    skinsize = pmodel->skinheight * pmodel->skinwidth;

    if (numskins < 1)
        Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

    pskintype = (daliasskintype_t *)&pinmodel[1];

    pskindesc = Hunk_AllocName (numskins * sizeof (maliasskindesc_t),
                                loadname);

    pheader->skindesc = (byte *)pskindesc - (byte *)pheader;

    for (i=0 ; i<numskins ; i++)
    {
        aliasskintype_t	skintype;

        skintype = LittleLong (pskintype->type);
        pskindesc[i].type = skintype;

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

    pheader->stverts = (byte *)pstverts - (byte *)pheader;

    for (i=0 ; i<pmodel->numverts ; i++)
    {
        pstverts[i].onseam = LittleLong (pinstverts[i].onseam);
        // put s and t in 16.16 format
        pstverts[i].s = LittleLong (pinstverts[i].s) << 16;
        pstverts[i].t = LittleLong (pinstverts[i].t) << 16;
    }

//
// set up the triangles
//
    ptri = (mtriangle_t *)&pstverts[pmodel->numverts];
    pintriangles = (dtriangle_t *)&pinstverts[pmodel->numverts];

    pheader->triangles = (byte *)ptri - (byte *)pheader;

    for (i=0 ; i<pmodel->numtris ; i++)
    {
        int		j;

        ptri[i].facesfront = LittleLong (pintriangles[i].facesfront);

        for (j=0 ; j<3 ; j++)
        {
            ptri[i].vertindex[j] =
                LittleLong (pintriangles[i].vertindex[j]);
        }
    }

//
// load the frames
//
    if (numframes < 1)
        Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

    pframetype = (daliasframetype_t *)&pintriangles[pmodel->numtris];

    for (i=0 ; i<numframes ; i++)
    {
        aliasframetype_t	frametype;

        frametype = LittleLong (pframetype->type);
        pheader->frames[i].type = frametype;

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
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe)
{
    dspriteframe_t		*pinframe;
    mspriteframe_t		*pspriteframe;
    int					i, width, height, size, origin[2];
    unsigned short		*ppixout;
    byte				*ppixin;

    pinframe = (dspriteframe_t *)pin;

    width = LittleLong (pinframe->width);
    height = LittleLong (pinframe->height);
    size = width * height;

    pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t) + size*r_pixbytes,
                                   loadname);

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
        Sys_Error ("Mod_LoadSpriteFrame: driver set invalid r_pixbytes: %d\n",
                   r_pixbytes);
    }

    return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe)
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
                                   (numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

    pspritegroup->numframes = numframes;

    *ppframe = (mspriteframe_t *)pspritegroup;

    pin_intervals = (dspriteinterval_t *)(pingroup + 1);

    poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

    pspritegroup->intervals = poutintervals;

    for (i=0 ; i<numframes ; i++)
    {
        *poutintervals = LittleFloat (pin_intervals->interval);
        if (*poutintervals <= 0.0)
            Sys_Error ("Mod_LoadSpriteGroup: interval<=0");

        poutintervals++;
        pin_intervals++;
    }

    ptemp = (void *)pin_intervals;

    for (i=0 ; i<numframes ; i++)
    {
        ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i]);
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

    pin = (dsprite_t *)buffer;

    version = LittleLong (pin->version);
    if (version != SPRITE_VERSION)
        Sys_Error ("%s has wrong version number "
                   "(%i should be %i)", mod->name, version, SPRITE_VERSION);

    numframes = LittleLong (pin->numframes);

    size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

    psprite = Hunk_AllocName (size, "modsprt2");

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
        Sys_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

    mod->numframes = numframes;
    mod->flags = 0;

    pframetype = (dspriteframetype_t *)(pin + 1);

    for (i=0 ; i<numframes ; i++)
    {
        spriteframetype_t	frametype;

        frametype = LittleLong (pframetype->type);
        psprite->frames[i].type = frametype;

        if (frametype == SPR_SINGLE)
        {
            pframetype = (dspriteframetype_t *)
                         Mod_LoadSpriteFrame (pframetype + 1,
                                              &psprite->frames[i].frameptr);
        }
        else
        {
            pframetype = (dspriteframetype_t *)
                         Mod_LoadSpriteGroup (pframetype + 1,
                                              &psprite->frames[i].frameptr);
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
    int		i;
    model_t	*mod;

    Con_Printf ("Cached models:\n");
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
