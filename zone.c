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

// Z_zone.c

#include "quakedef.h"

//qb- zone gone per mh idea... #define	DYNAMIC_SIZE	256*1024
//#define	ZONEID	0x1d4a11
//#define MINFRAGMENT	64

typedef struct memblock_s
{
    int		size;           // including the header and possibly tiny fragments
    int     tag;            // a tag of 0 is a free block
//qb	int     id;        		// should be ZONEID
    struct memblock_s       *next, *prev;
    int		pad;			// pad to 64 bit boundary
} memblock_t;

typedef struct
{
    int		size;		// total bytes malloced, including header
    memblock_t	blocklist;		// start / end cap for linked list
    memblock_t	*rover;
} memzone_t;

void Cache_FreeLow (int new_low_hunk);
void Cache_FreeHigh (int new_high_hunk);


/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

#define	HUNK_SENTINEL	0x1df001ed

typedef struct
{
    int		sentinel;
    int		size;		// including sizeof(hunk_t), -1 = not allocated
    char	name[8];
} hunk_t;

byte	*hunk_base;
int		hunk_size;

int		hunk_low_used;
int		hunk_high_used;

qboolean	hunk_tempactive;
int		hunk_tempmark;

void R_FreeTextures (void);

/*
==============
Hunk_Check

Run consistancy and sentinel trashing checks
==============
*/

void Hunk_Check (void) //qb - from mh
{
    hunk_t	*h;
    hunk_t   *hprev = NULL;

    for (h = (hunk_t *)hunk_base ; (byte *)h != hunk_base + hunk_low_used ; )
    {
        if (h->sentinel != HUNK_SENTINEL)
            Sys_Error ("Hunk_Check: trahsed sentinal after %s", hprev ? hprev->name : "hunk base");
        if (h->size < 16 || h->size + (byte *)h - hunk_base > hunk_size)
            Sys_Error ("Hunk_Check: bad size after %s", hprev ? hprev->name : "hunk base");

        hprev = h;
        h = (hunk_t *)((byte *)h+h->size);
    }
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void Hunk_Print (qboolean all)
{
    hunk_t	*h, *next, *endlow, *starthigh, *endhigh;
    int		count, sum;
    int		totalblocks;
    char	name[9];

    name[8] = 0;
    count = 0;
    sum = 0;
    totalblocks = 0;

    h = (hunk_t *)hunk_base;
    endlow = (hunk_t *)(hunk_base + hunk_low_used);
    starthigh = (hunk_t *)(hunk_base + hunk_size - hunk_high_used);
    endhigh = (hunk_t *)(hunk_base + hunk_size);

    Con_Printf ("          :%8i total hunk size\n", hunk_size);
    Con_Printf ("-------------------------\n");

    while (1)
    {
        //
        // skip to the high hunk if done with low hunk
        //
        if ( h == endlow )
        {
            Con_Printf ("-------------------------\n");
            Con_Printf ("          :%8i REMAINING\n", hunk_size - hunk_low_used - hunk_high_used);
            Con_Printf ("-------------------------\n");
            h = starthigh;
        }

        //
        // if totally done, break
        //
        if ( h == endhigh )
            break;

        //
        // run consistancy checks
        //
        if (h->sentinel != HUNK_SENTINEL)
            Sys_Error ("Hunk_Check: trashed sentinel");
        if (h->size < 16 || h->size + (byte *)h - hunk_base > hunk_size)
            Sys_Error ("Hunk_Check: bad size");

        next = (hunk_t *)((byte *)h+h->size);
        count++;
        totalblocks++;
        sum += h->size;

        //
        // print the single block
        //
        memcpy (name, h->name, 8);
        if (all)
            Con_Printf ("%8p :%8i %8s\n",h, h->size, name);

        //
        // print the total
        //
        if (next == endlow || next == endhigh ||
                strncmp (h->name, next->name, 8) )
        {
            if (!all)
                Con_Printf ("          :%8i %8s (TOTAL)\n",sum, name);
            count = 0;
            sum = 0;
        }

        h = next;
    }

    Con_Printf ("-------------------------\n");
    Con_Printf ("%8i total blocks\n", totalblocks);

}

// 2001-09-20 Hunklist command by Maddes  start
/*
===================
Hunk_Print_f
===================
*/
void Hunk_Print_f(void)
{
    qboolean	showall;

    showall = 0;
    if (Cmd_Argc() > 1)
    {
        showall = Q_atoi(Cmd_Argv(1));
    }

    Hunk_Print(showall);
}
// 2001-09-20 Hunklist command by Maddes  end

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName (int size, char *name)
{
    hunk_t	*h;

#ifdef PARANOID
    Hunk_Check ();
#endif

    if (size < 0)
        Sys_Error ("Hunk_Alloc: bad size: %i \nname:  %s", size, name); //qb might as well report name, too

    size = sizeof(hunk_t) + ((size+15)&~15);

    if (hunk_size - hunk_low_used - hunk_high_used < size)
        Sys_Error ("Hunk_Alloc: failed on %i bytes \nname:  %s",size, name); //qb might as well report name, too

    h = (hunk_t *)(hunk_base + hunk_low_used);
    hunk_low_used += size;

    Cache_FreeLow (hunk_low_used);

    memset (h, 0, size);

    h->size = size;
    h->sentinel = HUNK_SENTINEL;
    Q_strncpy (h->name, name, 8);

    return (void *)(h+1);
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc (int size)
{
    return Hunk_AllocName (size, "unknown");
}

int	Hunk_LowMark (void)
{
    return hunk_low_used;
}

void Hunk_FreeToLowMark (int mark)
{
    if (mark < 0 || mark > hunk_low_used)
        Sys_Error ("Hunk_FreeToLowMark: bad mark %i", mark);
    memset (hunk_base + mark, 0, hunk_low_used - mark);
    hunk_low_used = mark;
}

int	Hunk_HighMark (void)
{
    if (hunk_tempactive)
    {
        hunk_tempactive = false;
        Hunk_FreeToHighMark (hunk_tempmark);
    }

    return hunk_high_used;
}

void Hunk_FreeToHighMark (int mark)
{
    if (hunk_tempactive)
    {
        hunk_tempactive = false;
        Hunk_FreeToHighMark (hunk_tempmark);
    }
    if (mark < 0 || mark > hunk_high_used)
        Sys_Error ("Hunk_FreeToHighMark: bad mark %i", mark);
    memset (hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
    hunk_high_used = mark;
}


/*
===================
Hunk_HighAllocName
===================
*/
void *Hunk_HighAllocName (int size, char *name)
{
    hunk_t	*h;

    if (size < 0)
        Sys_Error ("Hunk_HighAllocName: bad size: %i", size);

    if (hunk_tempactive)
    {
        Hunk_FreeToHighMark (hunk_tempmark);
        hunk_tempactive = false;
    }

#ifdef PARANOID
    Hunk_Check ();
#endif

    size = sizeof(hunk_t) + ((size+15)&~15);

    if (hunk_size - hunk_low_used - hunk_high_used < size)
    {
        Sys_Error ("Hunk_HighAlloc: failed on %i bytes\n",size);  //qb- was Con_Printf, IIRC
        //return NULL;
    }

    hunk_high_used += size;
    Cache_FreeHigh (hunk_high_used);

    h = (hunk_t *)(hunk_base + hunk_size - hunk_high_used);

    memset (h, 0, size);
    h->size = size;
    h->sentinel = HUNK_SENTINEL;
    Q_strncpy (h->name, name, 8);

    return (void *)(h+1);
}


/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
*/
void *Hunk_TempAlloc (int size)
{
    void	*buf;

    size = (size+15)&~15;

    if (hunk_tempactive)
    {
        Hunk_FreeToHighMark (hunk_tempmark);
        hunk_tempactive = false;
    }

    hunk_tempmark = Hunk_HighMark ();

    buf = Hunk_HighAllocName (size, "temp");

    hunk_tempactive = true;

    return buf;
}

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

typedef struct cache_system_s
{
    int						size;		// including this header
    cache_user_t			*user;
    char					name[16];
    struct cache_system_s	*prev, *next;
    struct cache_system_s	*lru_prev, *lru_next;	// for LRU flushing
} cache_system_t;

cache_system_t *Cache_TryAlloc (int size, qboolean nobottom);

cache_system_t	cache_head;

/*
===========
Cache_Move
===========
*/
void Cache_Move ( cache_system_t *c)
{
    cache_system_t		*new;

// we are clearing up space at the bottom, so only allocate it late
    new = Cache_TryAlloc (c->size, true);
    if (new)
    {
//		Con_Printf ("cache_move ok\n");

        memcpy ( new+1, c+1, c->size - sizeof(cache_system_t) );
        new->user = c->user;
        memcpy (new->name, c->name, sizeof(new->name));
        Cache_Free (c->user);
        new->user->data = (void *)(new+1);
    }
    else
    {
//		Con_Printf ("cache_move failed\n");

        Cache_Free (c->user);		// tough luck...
    }
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeLow (int new_low_hunk)
{
    cache_system_t	*c;

    while (1)
    {
        c = cache_head.next;
        if (c == &cache_head)
            return;		// nothing in cache at all
        if ((byte *)c >= hunk_base + new_low_hunk)
            return;		// there is space to grow the hunk
        Cache_Move ( c );	// reclaim the space
    }
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeHigh (int new_high_hunk)
{
    cache_system_t	*c, *prev;

    prev = NULL;
    while (1)
    {
        c = cache_head.prev;
        if (c == &cache_head)
            return;		// nothing in cache at all
        if ( (byte *)c + c->size <= hunk_base + hunk_size - new_high_hunk)
            return;		// there is space to grow the hunk
        if (c == prev)
            Cache_Free (c->user);	// didn't move out of the way
        else
        {
            Cache_Move (c);	// try to move it
            prev = c;
        }
    }
}

void Cache_UnlinkLRU (cache_system_t *cs)
{
    if (!cs->lru_next || !cs->lru_prev)
        Sys_Error ("Cache_UnlinkLRU: NULL link");

    cs->lru_next->lru_prev = cs->lru_prev;
    cs->lru_prev->lru_next = cs->lru_next;

    cs->lru_prev = cs->lru_next = NULL;
}

void Cache_MakeLRU (cache_system_t *cs)
{
    if (cs->lru_next || cs->lru_prev)
        Sys_Error ("Cache_MakeLRU: active link");

    cache_head.lru_next->lru_prev = cs;
    cs->lru_next = cache_head.lru_next;
    cs->lru_prev = &cache_head;
    cache_head.lru_next = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
cache_system_t *Cache_TryAlloc (int size, qboolean nobottom)
{
    cache_system_t	*cs, *new;

// is the cache completely empty?

    if (!nobottom && cache_head.prev == &cache_head)
    {
        if (hunk_size - hunk_high_used - hunk_low_used < size)
            Sys_Error ("Cache_TryAlloc: %i is greater then free hunk", size);

        new = (cache_system_t *) (hunk_base + hunk_low_used);
        memset (new, 0, sizeof(*new));
        new->size = size;

        cache_head.prev = cache_head.next = new;
        new->prev = new->next = &cache_head;

        Cache_MakeLRU (new);
        return new;
    }

// search from the bottom up for space

    new = (cache_system_t *) (hunk_base + hunk_low_used);
    cs = cache_head.next;

    do
    {
        if (!nobottom || cs != cache_head.next)
        {
            if ( (byte *)cs - (byte *)new >= size)
            {
                // found space
                memset (new, 0, sizeof(*new));
                new->size = size;

                new->next = cs;
                new->prev = cs->prev;
                cs->prev->next = new;
                cs->prev = new;

                Cache_MakeLRU (new);

                return new;
            }
        }

        // continue looking
        new = (cache_system_t *)((byte *)cs + cs->size);
        cs = cs->next;

    }
    while (cs != &cache_head);

// try to allocate one at the very end
    if ( hunk_base + hunk_size - hunk_high_used - (byte *)new >= size)
    {
        memset (new, 0, sizeof(*new));
        new->size = size;

        new->next = &cache_head;
        new->prev = cache_head.prev;
        cache_head.prev->next = new;
        cache_head.prev = new;

        Cache_MakeLRU (new);

        return new;
    }

    return NULL;		// couldn't allocate
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush (void)
{
    while (cache_head.next != &cache_head)
        Cache_Free ( cache_head.next->user );	// reclaim the space
}


/*
============
Cache_Print

============
*/
void Cache_Print (void)
{
    cache_system_t	*cd;

    for (cd = cache_head.next ; cd != &cache_head ; cd = cd->next)
    {
        Con_Printf ("%8i : %s\n", cd->size, cd->name);
    }
}

/*
============
Cache_Report

============
*/
void Cache_Report (void)
{
    Con_DPrintf ("%4.1f megabyte data cache\n", (hunk_size - hunk_high_used - hunk_low_used) / (float)(1024*1024) );
}


/*
============
Cache_Init

============
*/
void Cache_Init (void)
{
    cache_head.next = cache_head.prev = &cache_head;
    cache_head.lru_next = cache_head.lru_prev = &cache_head;

    Cmd_AddCommand ("flush", Cache_Flush);
}

/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void Cache_Free (cache_user_t *c)
{
    cache_system_t	*cs;

    if (!c->data)
        Sys_Error ("Cache_Free: not allocated");

    cs = ((cache_system_t *)c->data) - 1;

    cs->prev->next = cs->next;
    cs->next->prev = cs->prev;
    cs->next = cs->prev = NULL;

    c->data = NULL;

    Cache_UnlinkLRU (cs);
}



/*
==============
Cache_Check
==============
*/
void *Cache_Check (cache_user_t *c)
{
    cache_system_t	*cs;

    if (!c->data)
        return NULL;

    cs = ((cache_system_t *)c->data) - 1;

// move to head of LRU
    Cache_UnlinkLRU (cs);
    Cache_MakeLRU (cs);

    return c->data;
}


/*
==============
Cache_Alloc
==============
*/
void *Cache_Alloc (cache_user_t *c, int size, char *name)
{
    cache_system_t	*cs;

    if (c->data)
        Sys_Error ("Cache_Alloc: allready allocated");

    if (size <= 0)
        Sys_Error ("Cache_Alloc: size %i", size);

    size = (size + sizeof(cache_system_t) + 15) & ~15;

// find memory for it
    while (1)
    {
        cs = Cache_TryAlloc (size, false);
        if (cs)
        {
            strncpy (cs->name, name, sizeof(cs->name)-1);
            c->data = (void *)(cs+1);
            cs->user = c;
            break;
        }

        // free the least recently used cahedat
        if (cache_head.lru_prev == &cache_head)
            Sys_Error ("Cache_Alloc: out of memory");
        // not enough memory at all
        Cache_Free ( cache_head.lru_prev->user );
    }

    return Cache_Check (c);
}

//============================================================================


/*
========================
Memory_Init
========================
*/
void Memory_Init (void *buf, int size)
{
    hunk_base = buf;
    hunk_size = size;
    hunk_low_used = 0;
    hunk_high_used = 0;

    Cache_Init ();
    Cmd_AddCommand ("hunklist", Hunk_Print_f);	// 2001-09-20 Hunklist command by Maddes
    Cmd_AddCommand ("cachelist", Cache_Print);	// 2001-09-20 Cachelist command by Maddes
}


//qb jqavi begin
/*
===================
Q_malloc

Use it instead of malloc so that if memory allocation fails,
the program exits with a message saying there's not enough memory
instead of crashing after trying to use a NULL pointer
===================
*/
void *Q_malloc (size_t size, char *caller)
{
    void   *p;

    if (!(p = malloc(size)))
        Sys_Error ("Q_malloc(): %s: Failed to allocate %lu bytes. Check disk space", caller, size);

    return p;
}

/*
===================
Q_calloc
===================
*/
void *Q_calloc (char *str, size_t n)
{
    void   *p;

    if (!(p = calloc(1, n)))  //qb- always 1 !
        Sys_Error ("Q_calloc(): %s: Failed to allocate %lu bytes. Check disk space", str, n);

    return p;
}

/*
===================
Q_realloc
===================
*/
void *Q_realloc (void *ptr, size_t size)
{
    void   *p;

    if (!(p = realloc(ptr, size)))
        Sys_Error ("Q_realloc(): Failed to allocate %lu bytes. Check disk space", size);

    return p;
}

/*
===================
Q_strdup
===================
*/
void *Q_strdup (const char *str)
{
	unsigned long size;
	char *d;

	size = strlen(str);
	size++;

    d = (char *)malloc (size); // Allocate memory
    if (d)
        strcpy (d,str);                            // Copy string if okay
    else
		Sys_Error ("Q_strdup:  Failed to allocate %lu bytes. Check disk space", size);
    return d;
}

//qb jqavi end
