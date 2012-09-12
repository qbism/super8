
#include "vis.h"

/*

Some textures (sky, water, slime, lava) are considered ambient sound emiters.
Find an aproximate distance to the nearest emiter of each class for each leaf.

*/

/*
====================
SurfaceBBox
====================
*/
void SurfaceBBox (dface_t *s, vec3_t mins, vec3_t maxs)
{
	int		i;
	int		e;
	int		vi;
	vec3_t	v;

	ClearBounds( mins, maxs );

	// Vic: old buggy code
//	mins[0] = mins[1] = BOGUS_RANGE;
//	maxs[0] = maxs[1] = -BOGUS_RANGE;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = dsurfedges[s->firstedge+i];
		if (e >= 0)
			vi = dedges[e].v[0];
		else
			vi = dedges[-e].v[1];

		VectorCopy( dvertexes[vi].point, v );
		AddPointToBounds( v, mins, maxs );
	}
}

/*
====================
CalcAmbientSounds
====================
*/
void CalcAmbientSounds (void)
{
	int		i, j, k, l;
	dleaf_t	*leaf, *hit;
	byte	*vis;
	dface_t	*surf;
	vec3_t	mins, maxs;
	vec_t	d, maxd;
	int		ambient_type;
	texinfo_t	*info;
	miptex_t	*miptex;
	int		ofs;
	vec_t	dists[NUM_AMBIENTS];
	vec_t	vol;

	printf ("---- ambient ----\n");

	if (noambientslime)
		printf ("defaulting slime to water\n");

	for (i=0 ; i< portalleafs ; i++)
	{
		leaf = &dleafs[i+1];

	//
	// clear ambients
	//
		for (j=0 ; j<NUM_AMBIENTS ; j++)
			dists[j] = BOGUS_RANGE;

		vis = &uncompressed[i*bitbytes];

		for (j=0 ; j< portalleafs ; j++)
		{
			if ( !(vis[j>>3] & (1<<(j&7))) )
				continue;

		//
		// check this leaf for sound textures
		//	
			hit = &dleafs[j+1];

			for (k=0 ; k< hit->nummarksurfaces ; k++)
			{
				surf = &dfaces[dmarksurfaces[hit->firstmarksurface + k]];
				info = &texinfo[surf->texinfo];
				ofs = ((dmiptexlump_t *)dtexdata)->dataofs[info->miptex];
				miptex = (miptex_t *)(&dtexdata[ofs]);

				// LordHavoc: optimized this to reduce wasted Q_strncasecmp calls
				if ((miptex->name[0] == 's' || miptex->name[0] == 'S') && (miptex->name[1] == 'k' || miptex->name[1] == 'K') && (miptex->name[2] == 'y' || miptex->name[2] == 'Y'))
				{
					if (noambientsky)
						continue;
					ambient_type = AMBIENT_SKY;
				}
				else if (miptex->name[0] == '*')
				{
					if ( !Q_strncasecmp (miptex->name, "*water", 6) )
					{
						if (noambientwater)
							continue;
						ambient_type = AMBIENT_WATER;
					}
					else if ( !Q_strncasecmp (miptex->name, "*slime", 6) )
					{
						if (noambientwater)
							continue;
						if (noambientslime)
							ambient_type = AMBIENT_WATER;
						else
							ambient_type = AMBIENT_SLIME;
					}
					else if ( !Q_strncasecmp (miptex->name, "*lava", 6) )
					{
						if (noambientlava)
							continue;
						ambient_type = AMBIENT_LAVA;
					}
					else if ( !Q_strncasecmp (miptex->name, "*04water", 8) )
					{
						if (noambientwater)
							continue;
						ambient_type = AMBIENT_WATER;
					}
					else
						continue;
				}
				else
					continue;

			// find distance from source leaf to polygon
				SurfaceBBox (surf, mins, maxs);
				maxd = 0;
				for (l=0 ; l<3 ; l++)
				{
					if (mins[l] > leaf->maxs[l])
						d = mins[l] - leaf->maxs[l];
					else if (maxs[l] < leaf->mins[l])
						d = leaf->mins[l] - mins[l];
					else
						d = 0;
					if (d > maxd)
						maxd = d;
				}

				// Vic: old buggy code
//				maxd = 0.25;
				if (maxd < dists[ambient_type])
					dists[ambient_type] = maxd;
			}
		}

		for (j=0 ; j<NUM_AMBIENTS ; j++)
		{
			if (dists[j] < 100)
				vol = 1.0;
			else
			{
				// Vic: old buggy code
//				vol = 1.0 - dists[2]*0.002;
				vol = 1.0 - dists[j]*0.002;
				if (vol < 0)
					vol = 0;
				else if (vol > 1.0)
					vol = 1.0;		// Vic
			}
			leaf->ambient_level[j] = vol*255;
		}
	}
}
