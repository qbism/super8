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

#include "quakedef.h"
#include "r_local.h"

#define MAX_PARTICLES 8192	//qbism 16383 per qsb, was 2048	// default max # of particles at one time
#define ABSOLUTE_MIN_PARTICLES	1024 //qbism per qsb - was 512	//no fewer no matter the command line

extern cvar_t	sv_gravity;

int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int		ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

particle_t	*active_particles, *free_particles;
particle_t	*particles;
int			r_numparticles;

vec3_t			r_pright, r_pup, r_ppn;


/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
    int		i;

    i = COM_CheckParm ("-particles");

    if (i)
    {
        r_numparticles = (int)(Q_atoi(com_argv[i+1]));
        if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
            r_numparticles = ABSOLUTE_MIN_PARTICLES;
    }
    else
    {
        r_numparticles = MAX_PARTICLES;
    }

    particles = (particle_t *)
                Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");
}


void R_DarkFieldParticles (entity_t *ent)
{
    int			i, j, k;
    particle_t	*p;
    float		vel;
    vec3_t		dir;
    vec3_t		org;

    org[0] = ent->origin[0];
    org[1] = ent->origin[1];
    org[2] = ent->origin[2];
    for (i=-16 ; i<16 ; i+=8)
        for (j=-16 ; j<16 ; j+=8)
            for (k=0 ; k<32 ; k+=8)
            {
                if (!free_particles)
                    return;
                p = free_particles;
                free_particles = p->next;
                p->next = active_particles;
                active_particles = p;

                p->die = cl.time + 0.2 + (rand()&7) * 0.02;
                p->start_time = cl.time; // Manoel Kasimier
                p->color = 150 + rand()%6;
                p->type = pt_slowgrav;

                dir[0] = j*8;
                dir[1] = i*8;
                dir[2] = k*8;

                p->org[0] = org[0] + i + (rand()&3);
                p->org[1] = org[1] + j + (rand()&3);
                p->org[2] = org[2] + k + (rand()&3);

                VectorNormalize (dir);
                vel = 50 + (rand()&63);
                VectorScale (dir, vel, p->vel);
            }
}


/*
===============
R_EntityParticles
===============
*/

#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;
vec3_t	avelocity = {23, 7, 3};
float	partstep = 0.01;
float	timescale = 0.01;

void R_EntityParticles (entity_t *ent)
{
    int			count;
    int			i;
    particle_t	*p;
    float		angle;
    float		sr, sp, sy, cr, cp, cy;
    vec3_t		forward;
    float		dist;

    dist = 64;
    count = 50;

    if (!avelocities[0][0])
    {
        for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
            avelocities[0][i] = (rand()&255) * 0.01;
    }


    for (i=0 ; i<NUMVERTEXNORMALS ; i++)
    {
        angle = cl.time * avelocities[i][0];
        sy = sin(angle);
        cy = cos(angle);
        angle = cl.time * avelocities[i][1];
        sp = sin(angle);
        cp = cos(angle);
        angle = cl.time * avelocities[i][2];
        sr = sin(angle);
        cr = cos(angle);

        forward[0] = cp*cy;
        forward[1] = cp*sy;
        forward[2] = -sp;

        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;

        p->die = cl.time + 0.01;
        p->start_time = cl.time; // Manoel Kasimier
        p->color = 0x6f;
        p->type = pt_explode;

        p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength;
        p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength;
        p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength;
    }
}


/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles (void)
{
    int		i;

    free_particles = &particles[0];
    active_particles = NULL;

    for (i=0 ; i<r_numparticles ; i++)
        particles[i].next = &particles[i+1];
    particles[r_numparticles-1].next = NULL;
}


void R_ReadPointFile_f (void)
{
    FILE	*f;
    vec3_t	org;
    int		r;
    int		c;
    particle_t	*p;
    char	name[MAX_OSPATH];

    sprintf (name,"maps/%s.pts", sv.name);

    COM_FOpenFile (name, &f, NULL);	// 2001-09-12 Returning from which searchpath a file was loaded by Maddes
    if (!f)
    {
        Con_Printf ("couldn't open %s\n", name);
        return;
    }

    Con_Printf ("Reading %s...\n", name);
    c = 0;
    for ( ;; )
    {
        r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
        if (r != 3)
            break;
        c++;

        if (!free_particles)
        {
            Con_Printf ("Not enough free particles\n");
            break;
        }
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;

        p->die = 99999;
        p->start_time = 99999; // Manoel Kasimier
        p->color = (-c)&15;
        p->type = pt_static;
        VectorCopy (vec3_origin, p->vel);
        VectorCopy (org, p->org);
    }

    fclose (f);
    Con_Printf ("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
    vec3_t		org, dir;
    int			i, count, msgcount, color;

    for (i=0 ; i<3 ; i++)
        org[i] = MSG_ReadCoord ();
    for (i=0 ; i<3 ; i++)
        dir[i] = MSG_ReadChar () * (1.0/16);
    msgcount = MSG_ReadByte ();
    color = MSG_ReadByte ();

    if (msgcount == 255)
        count = 1024;
    else
        count = msgcount;

    R_RunParticleEffect (org, dir, color, count);
}


/*
===============
R_ParticleExplosion

===============
*/
void R_ParticleExplosion (vec3_t org)
{
    int			i, j;
    particle_t	*p;

    for (i=0 ; i<r_part_explo1_count.value ; i++)
    {
        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;

        p->die = cl.time + r_part_explo1_time.value;
        p->start_time = cl.time-1.4; // Manoel Kasimier
        p->color = ramp1[0];
        p->ramp = rand()&3;
        if (i & 1)
        {
            p->type = pt_explode;
            for (j=0 ; j<3 ; j++)
            {
                p->org[j] = org[j] + ((rand()%32)-16);
                p->vel[j] = (rand()%(int)r_part_explo1_vel.value)-(r_part_explo1_vel.value/2);
            }
        }
        else
        {
            p->type = pt_explode2;
            for (j=0 ; j<3 ; j++)
            {
                p->org[j] = org[j] + ((rand()%32)-16);
                p->vel[j] = (rand()%(int)r_part_explo1_vel.value)-(r_part_explo1_vel.value/2);
            }
        }
    }
}

/*
===============
R_ParticleExplosion2

===============
*/
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
    int			i, j;
    particle_t	*p;
    int			colorMod = 0;

    for (i=0; i<r_part_explo2_count.value; i++)
    {
        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;

        p->die = cl.time + r_part_explo1_time.value;
        p->start_time = cl.time; // Manoel Kasimier
        p->color = colorStart + (colorMod % colorLength);
        colorMod++;

        p->type = pt_blob;
        for (j=0 ; j<3 ; j++)
        {
            p->org[j] = org[j] + ((rand()%32)-16);
            p->vel[j] = (rand()%(int)r_part_explo2_vel.value)-(r_part_explo2_vel.value/2);
        }
    }
}

/*
===============
R_BlobExplosion

===============
*/
void R_BlobExplosion (vec3_t org)
{
    int			i, j;
    particle_t	*p;

    for (i=0 ; i<r_part_blob_count.value ; i++)
    {
        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
        p->die = cl.time + r_part_blob_time.value + (rand()&8)*0.05;
        p->start_time = cl.time; // Manoel Kasimier

        if (i & 1)
        {
            p->type = pt_blob;
            p->color = 66 + rand()%6;
            for (j=0 ; j<3 ; j++)
            {
                p->org[j] = org[j] + ((rand()%32)-16);
                p->vel[j] = (rand()%(int)r_part_blob_vel.value)-(r_part_blob_vel.value/2);
            }
        }
        else
        {
            p->type = pt_blob2;
            p->color = 150 + rand()%6;
            for (j=0 ; j<3 ; j++)
            {
                p->org[j] = org[j] + ((rand()%32)-16);
                p->vel[j] = (rand()%(int)r_part_blob_vel.value/2)-(r_part_blob_vel.value/4);
            }
        }
    }
}

//qbism - schtuff below this line is based on Engoo particles.
//--------------------------------------------------------------------
/*
===============
R_RunParticleEffect



===============
*/

//
// Proto Particles have gravity and spew from origin making
// it a splash like effect instead of a leak like effect
// !!!!!!!!!!!!!!!!
// !HACK HACK HACK!
// !!!!!!!!!!!!!!!!
// Color overrides: Not exactly override of colors,
// but a stupid hack to allow other effects without
// doing something stupid to break the built-ins!
//
// > 256 = Smoke effect, no super velocity or gravity
// > 512 = Blood effect, splatter (todo! dynamically splatter ON THE SCREEN when touched on view then fade)
// > 768 = Rocks and debris, falling down, LONG life time
// > 1024 = !?
//
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
    int			i, j;
    particle_t	*p;

    for (i=0 ; i<count ; i++)
    {
        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
        if (count == 1024)
        {
            // rocket explosion
            p->die = cl.time + 5;
            p->start_time = cl.time-1.4; // Manoel Kasimier
            p->color = ramp1[0];
            p->ramp = rand()&3;

            if (i & 1)
            {
                p->type = pt_explode;
                for (j=0 ; j<3 ; j++)
                {
                    p->org[j] = org[j] + ((rand()%32)-16);
                    p->vel[j] = (rand()%512)-256;
                }
            }
            else
            {
                p->type = pt_explode2;
                for (j=0 ; j<3 ; j++)
                {
                    p->org[j] = org[j] + ((rand()%32)-16);
                    p->vel[j] = (rand()%512)-256;
                }
            }
        }
        else //bullet impact
        {
            p->start_time = cl.time; // Manoel Kasimier
            p->color = (color&~7) + (rand()&2);
            // p->color = (color&~35) + (rand()&2);
            //       if (p->color > 64 && p->color < 79)
            //     {
            //         p->type = pt_slowgrav;
            //         p->die = cl.time + 8;
            //    }
            if (color > 256)
            {
                p->die = cl.time + 0.1*(rand()%8);
                p->start_time = cl.time; // Manoel Kasimier
                p->type = pt_slowgrav;
                p->color = p->color - 256;
            }

            else if (color > 512)
            {
                p->die = cl.time + 0.1*(rand()%8);
                p->start_time = cl.time; // Manoel Kasimier
                p->type = pt_explode;
                p->color = p->color - 512;
            }

            else if (color > 768)
            {
                p->die = cl.time + 0.1*(rand()%8);
                p->start_time = cl.time; // Manoel Kasimier
                p->type = pt_explode;
                p->color = p->color - 768;
            }

            else if (color > 1024)
            {
                p->die = cl.time + 0.1*(rand()%8);
                p->start_time = cl.time; // Manoel Kasimier
                p->type = pt_explode;
                p->color = p->color - 1024;
            }

            else
            {
                p->die = cl.time + 0.1*(rand()%8);
                p->start_time = cl.time; // Manoel Kasimier
                p->type = pt_grav;
                for (j=0 ; j<3 ; j++)
                {
                    p->org[j] = org[j];
                    //	p->vel[j] = dir[j]*15 + (rand()%300)-150;
                    //		p->vel[j] = dir[j]*15 + (rand()%150)-75;
                    p->vel[j] = dir[j]*((rand()%22)) + (rand()%50)-25;
                }
            }
        }
    }
}

/*
===============
R_LavaSplash

===============
*/

void R_LavaSplash (vec3_t org)
{
    // more like gib splash!
    int			i, j, k;
    particle_t	*p;
    float		vel;
    vec3_t		dir;

    for (k=0 ; k<2 ; k++)
        for (i=-12 ; i<12 ; i++)
            for (j=-12 ; j<12 ; j++)

            {
                if (!free_particles)
                    return;
                p = free_particles;
                free_particles = p->next;
                p->next = active_particles;
                active_particles = p;

                p->die = cl.time + 2 + (rand()&31) * 4.02;
                p->start_time = cl.time; // Manoel Kasimier
                p->color = 68 + (rand()&7);
                p->type = pt_grav;

                dir[0] = j*448 + (rand()&27);
                dir[1] = i*448 + (rand()&27);
                dir[2] = 2356;

                p->org[0] = org[0];
                p->org[1] = org[1];
                p->org[2] = org[2];
                VectorNormalize (dir);
                vel = 50 + (rand()&63);
                VectorScale (dir, vel, p->vel);

            }

}


/*
===============
R_TeleportSplash

===============
*/
void R_TeleportSplash (vec3_t org)
{
    int			i, j, k;
    particle_t	*p;
    float		vel;
    vec3_t		dir;

    for (i=-16 ; i<16 ; i+=4)
        for (j=-16 ; j<16 ; j+=4)
            for (k=-24 ; k<32 ; k+=4)
            {
                if (!free_particles)
                    return;
                p = free_particles;
                free_particles = p->next;
                p->next = active_particles;
                active_particles = p;

                p->die = cl.time + 0.2 + (rand()&7) * 0.02;
                p->start_time = cl.time; // Manoel Kasimier
                p->color = 7 + (rand()&7);
                p->type = pt_slowgrav;

                dir[0] = j*8;
                dir[1] = i*8;
                dir[2] = k*8;

                p->org[0] = org[0] + i + (rand()&3);
                p->org[1] = org[1] + j + (rand()&3);
                p->org[2] = org[2] + k + (rand()&3);

                VectorNormalize (dir);
                vel = 50 + (rand()&63);
                VectorScale (dir, vel, p->vel);
            }
}

void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
    vec3_t		vec;
    float		len;
    int			j;
    particle_t	*p;
    int			dec;
    static int	tracercount;

    VectorSubtract (end, start, vec);
    len = VectorNormalize (vec);
    if (type < 128)
        dec = 3;
    else 	if (type == 7)
        dec = 1;
    else
    {
        dec = 1;
        type -= 128;
    }
    VectorScale (vec, dec, vec); // Manoel Kasimier

    while (len > 0)
    {
        len -= dec;

        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;

        VectorCopy (vec3_origin, p->vel);
        p->die = cl.time + 2;
         p->start_time = cl.time; // Manoel Kasimier
        switch (type)
        {
        case 0:	// rocket trail

            if ((rand()&35) < 1)  //qbism -added occasional falling ash
            {
                p->type = pt_grav;
                p->color = 32;
                for (j=0 ; j<3 ; j++)
                    p->org[j] = start[j] + ((rand()&6)-3);
                break;
            }
            else
            {
                p->type = pt_fire;
                p->ramp = (rand()&3);
                p->color = ramp3[(int)p->ramp];
                p->alpha = 0.25;
            }

            for (j=0 ; j<3 ; j++)
                p->org[j] = start[j] + ((rand()&6)-3);
            break;

        case 1:	// smoke smoke
            p->ramp = (rand()&3) + 2;
            p->color = ramp3[(int)p->ramp];
            p->type = pt_fire;
            for (j=0 ; j<3 ; j++)
                p->org[j] = start[j] + ((rand()&6)-3);
            break;

        case 2:	// GIB
            p->color = 67 + (rand()&3);
            for (j=0 ; j<3 ; j++)
            {
                p->org[j] = start[j] + ((rand()&6)-3);
                p->vel[j] = (float)((rand()%24)-12); //qbism- add scatter velocity, for wall sticking

            }
            p->type = pt_sticky;
            p->die = cl.time + r_part_sticky_time.value; //qbism - stick around
            p->start_time = cl.time; // Manoel Kasimier
            break;

        case 3:
        case 5:	// tracer
            p->die = cl.time + 0.5;
            p->start_time = cl.time; // Manoel Kasimier
            p->type = pt_static;
            if (type == 3)
                p->color = 52 + ((tracercount&4)<<1);
            else
                p->color = 230 + ((tracercount&4)<<1);

            tracercount++;

            VectorCopy (start, p->org);
            if (tracercount & 1)
            {
                p->vel[0] = (27+rand()%4)*vec[1];
                p->vel[1] = (27+rand()%4)*-vec[0];
            }
            else
            {
                p->vel[0] = (27+rand()%4)*-vec[1];
                p->vel[1] = (27+rand()%4)*vec[0];
            }
            break;

        case 4:	// slight blood
            //p->type = pt_grav;
            p->color = 67 + (rand()&3);
            for (j=0 ; j<3 ; j++)
            {
                p->org[j] = start[j] + ((rand()%6)-3);
                p->vel[j] = (float)((rand()%16)-8);
            }
            p->type = pt_sticky;
            len -= 3;
            p->die = cl.time + 16;
            p->start_time = cl.time; // Manoel Kasimier
            break;

        case 6:	// voor trail
            p->color = 9*16 + 8 + (rand()&3);
            p->type = pt_static;
            p->die = cl.time + 0.3;
            p->start_time = cl.time; // Manoel Kasimier
            for (j=0 ; j<3 ; j++)
                p->org[j] = start[j] + ((rand()&15)-8);
            break;
        case 7:	// rail trail smoke
            p->ramp = (rand()&3) + 2;
            p->color = ramp3[(int)p->ramp];
            p->alpha = 1;
            p->alphavel = -3.7 - rand()%1;
            p->type = pt_staticfade;
            for (j=0 ; j<3 ; j++)
                p->org[j] = start[j] + ((rand()&6)-3);
            break;
        }


        VectorAdd (start, vec, start);
    }
}


void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up)
{
    float		d;

    // this rotate and negat guarantees a vector
    // not colinear with the original
    right[1] = -forward[0];
    right[2] = forward[1];
    right[0] = forward[2];

    d = DotProduct (right, forward);
    VectorMA (right, -d, forward, right);
    VectorNormalize (right);
    CrossProduct (right, forward, up);
}


/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
    particle_t		*p, *kill;
    mleaf_t         *l;
    float			grav, grav2, percent;
    int				i;
    float			time2, time3;
    float			time1;
    float			dvel;
    float			frametime;

// hexen 2
    vec3_t		diff;
    qboolean	in_solid;

    VectorScale (vright, xscaleshrink, r_pright);
    VectorScale (vup, yscaleshrink, r_pup);
    VectorCopy (vpn, r_ppn);

    frametime = fabs(cl.time - cl.oldtime); //DEMO_REWIND - qbism - Baker change (no, it is not supposed to be 'ctime')
    time3 = frametime * 17;
    time2 = frametime * 13; // 15;
    time1 = frametime * 10;
    grav = frametime * sv_gravity.value * 0.05;
    dvel = 4*frametime;

    for ( ;; )
    {
        kill = active_particles;
        if (kill && kill->die < cl.time)
        {
            active_particles = kill->next;
            kill->next = free_particles;
            free_particles = kill;
            continue;
        }
        break;
    }

    for (p=active_particles ; p ; p=p->next)
    {
        for ( ;; )
        {
            kill = p->next;
            if (kill && kill->die < cl.time)
            {
                p->next = kill->next;
                kill->next = free_particles;
                free_particles = kill;
                continue;
            }
            break;
        }
        // Manoel Kasimier - begin

        float alpha = 1.0 - ((cl.time - p->start_time) / (p->die - cl.time));
        if (alpha <= 0.41)
            D_DrawParticle_33_C (p);
        else if (alpha <= 0.76)
            D_DrawParticle_66_C (p);
        else
            D_DrawParticle_C (p);  // renamed to not conflict with the asm version

        p->org[0] += p->vel[0] * frametime;
        p->org[1] += p->vel[1] * frametime;
        p->org[2] += p->vel[2] * frametime;

        switch (p->type)
        {
        case pt_static:
            break;
        case pt_fire:
            p->ramp += time1;
            if (p->ramp >= 6)
                p->die = -1;
            else
                p->color = ramp3[(int)p->ramp];
            p->vel[2] += grav;
            break;

        case pt_explode:
            p->ramp += time2;
//			p->splatter = 0;
            if (p->ramp >=8)
                p->die = -1;
            else
                p->color = ramp1[(int)p->ramp];
            for (i=0 ; i<3 ; i++)
                p->vel[i] += p->vel[i]*dvel;
            p->vel[2] -= grav;
            break;

        case pt_explode2:
            p->ramp += time3;
//			p->splatter = 0;
            if (p->ramp >=8)
                p->die = -1;
            else
                p->color = ramp2[(int)p->ramp];
            for (i=0 ; i<3 ; i++)
                p->vel[i] -= p->vel[i]*frametime;
            p->vel[2] -= grav;
            break;

        case pt_blob:
            for (i=0 ; i<3 ; i++)
                p->vel[i] += p->vel[i]*dvel;
            p->vel[2] -= grav;
            break;

        case pt_blob2:
            for (i=0 ; i<2 ; i++)
                p->vel[i] -= p->vel[i]*dvel;
            p->vel[2] -= grav;
            break;

        case pt_grav:
            p->vel[2] -= grav * 4;
            break;

        case pt_slowgrav:
            p->vel[2] -= grav * 2;
            break;

        case pt_fastgrav:
            p->vel[2] -= grav * 8;
            break;

		case pt_smoke:
			p->alpha = 1;
			p->vel[2] += grav;
			break;

        case pt_decel:
			p->alpha = 1;
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]* -dvel;
			break;

        case pt_staticfade:
            p->alpha + frametime*p->alphavel;

            if (p->alpha <= 0)
                p->die = -1;
            break;

		case pt_staticfadeadd:
			p->alpha += frametime*p->alphavel;
			if (p->alpha <= 0)
				p->die = -1;
			break;

        case pt_sticky: //pt_blood in engoo... could be anything sticky
            VectorScale(p->vel, frametime, diff);
            VectorAdd(p->org, diff, p->org);

            // WHERE THE HITTING HAPPENS
            // if hit solid, go to last position,
            // no velocity, fade out.
            l = Mod_PointInLeaf (p->org, cl.worldmodel);
            if (l->contents != CONTENTS_EMPTY) // || in_solid == true
            {
                // still have small prob of snow melting on emitter
                VectorScale(diff, 0.2, p->vel);
                i = 6;
                while (l->contents != CONTENTS_EMPTY)
                {
                    VectorNormalize(p->vel);
                    p->org[0] -= p->vel[0]*3;
                    p->org[1] -= p->vel[1]*3;
                    p->org[2] -= p->vel[2]*3;
                    i--; //no infinite loops
                    if (!i)
                    {
                        p->die = -1;	//should never happen now!
                        break;
                    }
                    l = Mod_PointInLeaf (p->org, cl.worldmodel);
                }
                p->vel[0] = p->vel[1] = p->vel[2] = 0;
                p->ramp = 0;
                p->type = pt_static;
            }
            else
            {
                p->alpha = 0.5;
                p->vel[2] -= grav * 2;
            }
            break;
        }
    }
}

////qbism - below this line is sadly not used yet /////////

/*
===============
R_PlasmaExplosion
used by TE_TEI_PLASMAHIT
===============
*/
void R_PlasmaExplosion (vec3_t org, int colorStart, int colorLength, int count)
{
    int			i, j;
    particle_t	*p;
    int			colorMod = 0;

    for (i=0; i<count; i++)
    {
        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
        // TODO: Implement Spark Trail type
        p->die = cl.time + 0.4;
        p->start_time = cl.time; // Manoel Kasimier
        p->color = colorStart + (colorMod % colorLength);
        colorMod++;

        p->type = pt_decel;
        for (j=0 ; j<3 ; j++)
        {
            p->org[j] = org[j] + ((rand()%4)-2);
            p->vel[j] = (rand()%1500)-750;
        }
    }
}

// --------------------
// Used by TE_TEI_SMOKE
// --------------------

void R_Smoke (vec3_t org, vec3_t dir, int color, int count)
{
    int			i, j;
    particle_t	*p;

    for (i=0 ; i<count ; i++)
    {
        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
        if (count == 1024)
        {
            // rocket explosion
            p->die = cl.time + 5;
            p->start_time = cl.time; // Manoel Kasimier
            p->color = ramp1[0];
            p->ramp = rand()&3;
            if (i & 1)
            {
                p->type = pt_smoke;
                for (j=0 ; j<3 ; j++)
                {
                    p->org[j] = org[j] + ((rand()%32)-16);
                    p->vel[j] = (rand()%512)-256;
                }
            }
            else
            {
                p->type = pt_smoke;
                for (j=0 ; j<3 ; j++)
                {
                    p->org[j] = org[j] + ((rand()%32)-16);
                    p->vel[j] = (rand()%512)-256;
                }
            }
        }
        else
        {
            p->die = cl.time + 0.1*(rand()%5);
            p->start_time = cl.time; // Manoel Kasimier
            p->color = (color&~7) + (rand()&7);
            p->type = pt_smoke;
            for (j=0 ; j<3 ; j++)
            {
                p->org[j] = org[j] + ((rand()%6)-3);
                p->vel[j] = (rand()%50)-25;
            }
        }
    }
}


// --------------------
// Used for the engine splash calls (by r_particle_splash)
// --------------------

void R_Splash (vec3_t org, vec3_t dir, int color, int count, int power)
{
    int			i, j;
    particle_t	*p;

    for (i=0 ; i<count ; i++)
    {
        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
        if (count == 1024)
        {
            // rocket explosion
            p->die = cl.time + 5;
            p->start_time = cl.time; // Manoel Kasimier
            p->color = ramp1[0];
            p->ramp = rand()&3;
            if (i & 1)
            {
                p->type = pt_smoke;
                for (j=0 ; j<3 ; j++)
                {
                    p->org[j] = org[j] + ((rand()%32)-16);
                    p->vel[j] = (rand()%512)-256;
                }
            }
            else
            {
                p->type = pt_smoke;
                for (j=0 ; j<3 ; j++)
                {
                    p->org[j] = org[j] + ((rand()%32)-16);
                    p->vel[j] = (rand()%512)-256;
                }
            }
        }
        else
        {
            p->die = cl.time + 0.4*(rand()%5);
            p->start_time = cl.time; // Manoel Kasimier
            p->color = color;
            p->type = pt_blob;
            for (j=0 ; j<3 ; j++)
            {
                p->org[j] = org[j];
                p->vel[j] = (rand()%350)-(125);
            }
        }
    }
}

// LEI - lame particle beam
// (TODO: Quake2 polygon beam)
void R_ParticleBeam (vec3_t start, vec3_t end, int thecol)
{
    vec3_t		vec;
    float		len;
    int			j;
    particle_t	*p;
    int			dec;
    static int	tracercount;
    int	wat;
    //int	thecol;



    VectorSubtract (end, start, vec);
    len = VectorNormalize (vec);
    dec = 1;

    while (len > 0)
    {
        len -= dec;

        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;

        VectorCopy (vec3_origin, p->vel);
        p->die = cl.time;
        p->start_time = cl.time; // Manoel Kasimier


        wat = rand()&2;
        {
            if (wat == 2) 	// Outer Glow
            {
                p->color = thecol;
                p->type = pt_staticfadeadd;
                p->die = cl.time + 2;
                p->start_time = cl.time; // Manoel Kasimier
                p->alpha = 0.6;
                p->alphavel = -0.7;
                for (j=0 ; j<3 ; j++)
                    p->org[j] = start[j] + ((rand()&4)-2);
            }
            else
            {
                // Inner Line
                p->color = thecol;
                p->type = pt_staticfadeadd;
                p->die = cl.time + 2;
                p->start_time = cl.time; // Manoel Kasimier
                p->alpha = 1;
                p->alphavel = -0.9;
                for (j=0 ; j<3 ; j++)
                    p->org[j] = start[j];
            }
        }


        VectorAdd (start, vec, start);
    }
}


/*
===============
R_RailTrail
Used by TE_RAILTRAIL
i'm a quake2 function short and stout
===============
*/

void R_RailTrail (vec3_t start, vec3_t end)
{
    vec3_t		move;
    vec3_t		vec;
    float		len;
    int			j;
    particle_t	*p;
    float		dec;
    vec3_t		right, up;
    int			i;
    float		d, c, s;
    vec3_t		dir;
    byte		clr = 40;

    VectorCopy (start, move);
    VectorSubtract (end, start, vec);
    len = VectorNormalize (vec);

    MakeNormalVectors (vec, right, up);

    for (i=0 ; i<len ; i++)
    {
        if (!free_particles)
            return;

        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;

        p->die = cl.time + 2.0;
        p->start_time = cl.time; // Manoel Kasimier
        VectorClear (p->vel);

        d = i * 0.1;
        c = cos(d);
        s = sin(d);

        VectorScale (right, c, dir);
        VectorMA (dir, s, up, dir);
        p->type = pt_staticfadeadd;
        p->alpha = 1.0;
        //	p->alphavel = -1.0 / (1+frand()*0.2);
        p->alphavel = -1.0;

        p->color = clr + (rand()&7);
        for (j=0 ; j<3 ; j++)
        {
            p->org[j] = move[j] + dir[j]*3;
            p->vel[j] = dir[j]*6;
        }

        VectorAdd (move, vec, move);
    }

    dec = 0.75;
    VectorScale (vec, dec, vec);
    VectorCopy (start, move);
}


