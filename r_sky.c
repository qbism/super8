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
// r_sky.c

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"

extern cvar_t r_palette;

int		iskyspeed = 8;
int		iskyspeed2 = 2;
float	skyspeed, skyspeed2;

float		skytime;

byte	*skyunderlay, *skyoverlay; // Manoel Kasimier - smooth sky
cvar_t	r_skyname = { "r_skyname", ""}; // Manoel Kasimier - skyboxes // Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (texture_t *mt)
{
    skyoverlay = (byte *)mt + mt->offsets[0]; // Manoel Kasimier - smooth sky
    skyunderlay = skyoverlay+128; // Manoel Kasimier - smooth sky
}


/*
=============
R_SetSkyFrame
==============
*/
void R_SetSkyFrame (void)
{
    int		g, s1, s2;
    float	temp;

    skyspeed = iskyspeed;
    skyspeed2 = iskyspeed2;

    g = GreatestCommonDivisor (iskyspeed, iskyspeed2);
    s1 = iskyspeed / g;
    s2 = iskyspeed2 / g;
    temp = SKYSIZE * s1 * s2;

    skytime = cl.ctime - ((int)(cl.ctime / temp) * temp);//DEMO_REWIND - qbism - translated to SW from GL Baker change
}

// Manoel Kasimier - skyboxes - begin
// Code taken from the ToChriS engine - Author: Vic (vic@quakesrc.org) (http://hkitchen.quakesrc.org/)
extern	mtexinfo_t		r_skytexinfo[6];
extern	qboolean		r_drawskybox;
byte					r_skypixels[6][1024*1024]; //qbism- up to 1024x1024
texture_t				r_skytextures[6];
char					skyname[MAX_QPATH];



qboolean R_LoadSkybox (char *name)
{
    int		i;
    char	pathname[MAX_QPATH];
    byte	*pic;
    char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
    int		r_skysideimage[6] = {5, 2, 4, 1, 0, 3};
    int		width, height;

    if (!name || !name[0])
    {
        skyname[0] = 0;
        return false;
    }

//  the same skybox we are using now
//  qbism- on second thought, msy be a reason to reload it.   if (!Q_strcmp (name, skyname)) //qbism was stricmp
//  return true;


    Q_strncpy (skyname, name, sizeof(skyname)-1);

    for (i=0 ; i<6 ; i++)
    {

#ifdef __linux__
        snprintf (pathname, sizeof(pathname), "gfx/env/%s%s.pcx\0", skyname, suf[r_skysideimage[i]]); // Manoel Kasimier - edited
#else
        Q_snprintfz (pathname, sizeof(pathname), "gfx/env/%s%s.pcx\0", skyname, suf[r_skysideimage[i]]); //qbism- Q_snprintfz
#endif
        LoadPCX (pathname, &pic, &width, &height);

        if (!pic)
        {
#ifdef __linux__
            snprintf (pathname, sizeof(pathname), "gfx/env/%s%s.tga\0", skyname, suf[r_skysideimage[i]]); //qbism- MK 1.4a
#else
            Q_snprintfz (pathname, sizeof(pathname), "gfx/env/%s%s.tga\0", skyname, suf[r_skysideimage[i]]); //qbism- MK 1.4a
#endif
            LoadTGA_as8bit (pathname, &pic, &width, &height);
            if (!pic)
            {
                Con_Printf ("Couldn't load %s\n", pathname);
#ifdef __linux__
                snprintf (pathname, sizeof(pathname), "gfx/env/def_sky.pcx\0", skyname, suf[r_skysideimage[i]]); // Manoel Kasimier - edited
#else
                Q_snprintfz (pathname, sizeof(pathname), "gfx/env/def_sky.pcx\0", skyname, suf[r_skysideimage[i]]); //qbism- Q_snprintfz

#endif
                LoadPCX (pathname, &pic, &width, &height);

                if (!pic)
                {
                    Sys_Error ("Couldn't load sky, and\ncouldn't load default sky texture %s\n", pathname);  //qbism no sky = crash, so exit gracefully
                    return false;
                }
            }
        }
        // Manoel Kasimier - hi-res skyboxes - begin
        if (((width != 256) && (width != 512) && (width != 1024)) || ((height != 256) && (height != 512) && (height != 1024)))
        {
            free (pic);
            Con_Printf ("width for %s must be 256, 512, or 1024\n", pathname);
            Con_Printf ("Couldn't load %s\n", pathname);
#ifdef __linux__
            snprintf (pathname, sizeof(pathname), "gfx/env/def_sky.pcx\0", skyname, suf[r_skysideimage[i]]); // Manoel Kasimier - edited
#else
            Q_snprintfz (pathname, sizeof(pathname), "gfx/env/def_sky.pcx\0", skyname, suf[r_skysideimage[i]]); //qbism- Q_snprintfz

#endif
            LoadPCX (pathname, &pic, &width, &height);

            if (!pic)
            {
                Sys_Error ("Couldn't load sky, and\ncouldn't load default sky texture %s\n", pathname);  //qbism no sky = crash, so exit gracefully
                return false;
            }
        }

        // Manoel Kasimier - hi-res skyboxes - end

        r_skytexinfo[i].texture = &r_skytextures[i];
        r_skytexinfo[i].texture->width = width; // Manoel Kasimier - hi-res skyboxes - edited
        r_skytexinfo[i].texture->height = height; // Manoel Kasimier - hi-res skyboxes - edited
        r_skytexinfo[i].texture->offsets[0] = i;

        // Manoel Kasimier - hi-res skyboxes - begin
        {
            extern vec3_t box_256_vecs[6][2];
            extern vec3_t box_512_vecs[6][2];
            extern vec3_t box_1024_vecs[6][2]; //qbism added
            extern msurface_t *r_skyfaces;

            if (width == 1024)
                VectorCopy (box_1024_vecs[i][0], r_skytexinfo[i].vecs[0])
                else if (width == 512)
                    VectorCopy (box_512_vecs[i][0], r_skytexinfo[i].vecs[0])
                    else
                        VectorCopy (box_256_vecs[i][0], r_skytexinfo[i].vecs[0]);

            if (height == 1024)
                VectorCopy (box_1024_vecs[i][1], r_skytexinfo[i].vecs[1])
                    else if (height == 512)
                        VectorCopy (box_512_vecs[i][1], r_skytexinfo[i].vecs[1])
                        else
                            VectorCopy (box_256_vecs[i][1], r_skytexinfo[i].vecs[1]);

            r_skyfaces[i].texturemins[0] = -(width/2);
            r_skyfaces[i].texturemins[1] = -(height/2);
            r_skyfaces[i].extents[0] = width;
            r_skyfaces[i].extents[1] = height;
        }
        // Manoel Kasimier - hi-res skyboxes - end

        memcpy (r_skypixels[i], pic, width*height); // Manoel Kasimier - hi-res skyboxes - edited
        free (pic);
    }
    return true;
}


void R_LoadSky (char *s)
{
    if (s[0])
        r_drawskybox = R_LoadSkybox (s);
    else
        r_drawskybox = false;
}

void R_LoadSky_f (void)
{
    if (Cmd_Argc() != 2)
    {
        Con_Printf ("loadsky <name> : load a skybox\n");
        return;
    }
    R_LoadSky(Cmd_Argv(1)); // Manoel Kasimier
}
// Manoel Kasimier - skyboxes - end


//qbism - fog commands from FitzQuake, simplified
//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================

float fog_density, fog_red, fog_green, fog_blue;

float old_density, old_red, old_green, old_blue;

float fade_time; //duration of fade
float fade_done; //time when fade will be done

/*
=============
Fog_Update

update internal variables
=============
*/
void Fog_Update (float density, float red, float green, float blue, float time)
{
    //save previous settings for fade
    if (time > 0)
    {
        //check for a fade in progress
        if (fade_done > cl.time)
        {
            float f, d;

            f = (fade_done - cl.time) / fade_time;
            old_density = f * old_density + (1.0 - f) * fog_density;
            old_red = f * old_red + (1.0 - f) * fog_red;
            old_green = f * old_green + (1.0 - f) * fog_green;
            old_blue = f * old_blue + (1.0 - f) * fog_blue;
        }
        else
        {
            old_density = fog_density;
            old_red = fog_red;
            old_green = fog_green;
            old_blue = fog_blue;
        }
    }

    fog_density = density;
    fog_red = red;
    fog_green = green;
    fog_blue = blue;
    fade_time = time;
    fade_done = cl.time + time;
}

/*
=============
Fog_ParseServerMessage

handle an SVC_FOG message from server
=============
*/
void Fog_ParseServerMessage (void)
{
    float density, red, green, blue, time;

    density = MSG_ReadByte() / 255.0;
    red = MSG_ReadByte() / 255.0;
    green = MSG_ReadByte() / 255.0;
    blue = MSG_ReadByte() / 255.0;
    time = max(0.0, MSG_ReadShort() / 100.0);

    Fog_Update (density, red, green, blue, time);
}
/*
=============
Fog_FogCommand_f

handle the 'fog' console command
=============
*/
void Fog_FogCommand_f (void)
{
    switch (Cmd_Argc())
    {
    default:
    case 1:
        Con_Printf("usage:\n");
        Con_Printf("   fog <density>\n");
        Con_Printf("   fog <red> <green> <blue>\n");
        Con_Printf("   fog <density> <red> <green> <blue>\n");
        Con_Printf("current values:\n");
        Con_Printf("   \"density\" is \"%f\"\n", fog_density);
        Con_Printf("   \"red\" is \"%f\"\n", fog_red);
        Con_Printf("   \"green\" is \"%f\"\n", fog_green);
        Con_Printf("   \"blue\" is \"%f\"\n", fog_blue);
        break;
    case 2:
        Fog_Update(max(0.0, atof(Cmd_Argv(1))),
                   fog_red,
                   fog_green,
                   fog_blue,
                   0.0);
        break;
    case 3: //TEST
        Fog_Update(max(0.0, atof(Cmd_Argv(1))),
                   fog_red,
                   fog_green,
                   fog_blue,
                   atof(Cmd_Argv(2)));
        break;
    case 4:
        Fog_Update(fog_density,
                   CLAMP(0.0, atof(Cmd_Argv(1)), 1.0),
                   CLAMP(0.0, atof(Cmd_Argv(2)), 1.0),
                   CLAMP(0.0, atof(Cmd_Argv(3)), 1.0),
                   0.0);
        break;
    case 5:
        Fog_Update(max(0.0, atof(Cmd_Argv(1))),
                   CLAMP(0.0, atof(Cmd_Argv(2)), 1.0),
                   CLAMP(0.0, atof(Cmd_Argv(3)), 1.0),
                   CLAMP(0.0, atof(Cmd_Argv(4)), 1.0),
                   0.0);
        break;
    case 6: //TEST
        Fog_Update(max(0.0, atof(Cmd_Argv(1))),
                   CLAMP(0.0, atof(Cmd_Argv(2)), 1.0),
                   CLAMP(0.0, atof(Cmd_Argv(3)), 1.0),
                   CLAMP(0.0, atof(Cmd_Argv(4)), 1.0),
                   atof(Cmd_Argv(5)));
        break;
    }
}

/*
=============
ParseWorldspawn based on Fog_ParseWorldspawn

called at map load
=============
*/
void ParseWorldspawn (void)
{
    char key[128], value[4096];
    char *data;

    data = COM_Parse(cl.worldmodel->entities);
    if (!data)
        return; // error
    if (com_token[0] != '{')
        return; // error
    while (1)
    {
        data = COM_Parse(data);
        if (!data)
            return; // error
        if (com_token[0] == '}')
            break; // end of worldspawn
        if (com_token[0] == '_')
            strcpy(key, com_token + 1);
        else
            strcpy(key, com_token);
        while (key[strlen(key)-1] == ' ') // remove trailing spaces
            key[strlen(key)-1] = 0;
        data = COM_Parse(data);
        if (!data)
            return; // error
        strcpy(value, com_token);

        if (!strcmp("fog", key))
        {
            sscanf(value, "%f %f %f %f", &fog_density, &fog_red, &fog_green, &fog_blue);
        }
/*        if (!strcmp("skyboxsomething", key)) //qbism - add more keys?  Don't match commands or vars!
        {
            sscanf(value, "%s", &r_skyname.value);
        }
        if (!strcmp("palettesomething", key))
        {
            sscanf(value, "%s", &r_palette.value);
        }
        */
    }
}
