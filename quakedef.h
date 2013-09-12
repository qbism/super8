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
// quakedef.h -- primary header for client

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#ifdef WIN32
#include "C:\CodeBlocks\MinGW\include\pthread.h"  //qb: multithreaded functions
#endif

//#define	GLTEST			// experimental stuff

#define	QUAKE_GAME			// as opposed to utilities

#define	D3DQUAKE_VERSION	0
#define	WINQUAKE_VERSION	7
#define	LINUX_VERSION		0

//define	PARANOID			// speed sapping error checking
#define	GAMENAME	"id1"

#ifdef FLASH
#include "AS3.h"

void trace(char *fmt, ...);

FILE* as3OpenWriteFile(const char* filename);
void as3UpdateFileSharedObject(const char* filename);
void as3ReadFileSharedObject(const char* filename);

//For Flash, we need to swap round the arguments for atan2
#define myAtan2(opp, adj) atan2(adj, opp)

#else
#define trace(a, ...)

#define myAtan2(opp, adj) atan2(opp, adj)

#endif

#define      HISTORY_FILE_NAME   "command_history.txt"  //qb: Baker/ezQuake command history
#define		MAXCMDLINE	256
#define      CMDLINES   64

#define CACHE_SIZE	32		// used to align key data structures

#define UNUSED(x)	(x = x)	// for pesky compiler / lint warnings

#define	MINIMUM_MEMORY	0x800000	// qb: was 0x550000
#define	MINIMUM_MEMORY_LEVELPAK	(MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS	50

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#define	MAX_QPATH		96		//qb: was 64 //max length of a quake game pathname
#define	MAX_OSPATH		160		//qb: was 128 //max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	MAX_MSGLEN		32767 //qb: 32767 Super8 max, 65535 per qsb - was 8000		// max length of a reliable message
#define	MAX_DATAGRAM	32767 //max for wifi, 32767 Super8 max - was 1024		// max length of unreliable message
#define	DATAGRAM_MTU	1400 //qb:  johnfitz -- if client is nonlocal

//
// per-level limits
//

#define	MAX_EDICTS		8192			//qb: 8192 per qsb - was 600.    More than 8192 requires protocol change.
#define	MAX_LIGHTSTYLES	64
#define	MAX_MODELS		2048	//qb: 4096 per qsb, protocol was changed.
#define	MAX_SOUNDS		2048	//qb: 4096 per qsb, was 256, protocol has been changed.

#define	SAVEGAME_COMMENT_LENGTH	39

#define	MAX_STYLESTRING	64

#define COLORLEVELS 64 //qb: number of color map levels. 64.
#define PALBRIGHTS 32 //qb: number of fullbrights at end of palette. 32.

//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		32
#define	STAT_HEALTH			0
#define	STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster


// stock defines

#define	IT_SHOTGUN				1
#define	IT_SUPER_SHOTGUN		2
#define	IT_NAILGUN				4
#define	IT_SUPER_NAILGUN		8
#define	IT_GRENADE_LAUNCHER		16
#define	IT_ROCKET_LAUNCHER		32
#define	IT_LIGHTNING			64
//#define IT_SUPER_LIGHTNING      128
//#define IT_HOOK  qb: from ROOk
#define IT_SHELLS               256
#define IT_NAILS                512
#define IT_ROCKETS              1024
#define IT_CELLS                2048
#define IT_AXE                  4096
#define IT_ARMOR1               8192
#define IT_ARMOR2               16384
#define IT_ARMOR3               32768
#define IT_SUPERHEALTH          65536
#define IT_KEY1                 131072
#define IT_KEY2                 262144
#define	IT_INVISIBILITY			524288
#define	IT_INVULNERABILITY		1048576
#define	IT_SUIT					2097152
#define	IT_QUAD					4194304
#define IT_SIGIL1               (1<<28)
#define IT_SIGIL2               (1<<29)
#define IT_SIGIL3               (1<<30)
#define IT_SIGIL4               (1<<31)

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              128
#define RIT_NAILS               256
#define RIT_ROCKETS             512
#define RIT_CELLS               1024
#define RIT_AXE                 2048
#define RIT_LAVA_NAILGUN        4096
#define RIT_LAVA_SUPER_NAILGUN  8192
#define RIT_MULTI_GRENADE       16384
#define RIT_MULTI_ROCKET        32768
#define RIT_PLASMA_GUN          65536
#define RIT_ARMOR1              8388608
#define RIT_ARMOR2              16777216
#define RIT_ARMOR3              33554432
#define RIT_LAVA_NAILS          67108864
#define RIT_PLASMA_AMMO         134217728
#define RIT_MULTI_ROCKETS       268435456
#define RIT_SHIELD              536870912
#define RIT_ANTIGRAV            1073741824
#define RIT_SUPERHEALTH         2147483648

//MED 01/04/97 added hipnotic defines
//===========================================
//hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<(23+2))
#define HIT_EMPATHY_SHIELDS (1<<(23+3))

//===========================================

//qb: stain fade types defines
#define FADE_STAIN      1
#define FADE_SHADOW     2
#define FADE_SLIMETRAIL 3

#define	MAX_SCOREBOARD		64 //qb: per bjp, was 16
#define	MAX_SCOREBOARDNAME	32

#define	MAX_RAW_SAMPLES	8192 //qb: QS
#define WAV_FORMAT_PCM	1 //qb: QS

//qb: for M_PrintText
#define ALIGN_LEFT 0
#define ALIGN_RIGHT 8
#define ALIGN_CENTER 4

//qb: for Draw2Dimage_ScaledMappedTranslatedTransparent
#define JUSTIFY_BOTTOM 1
#define JUSTIFY_CENTER 2

#include "common.h"
#include "bspfile.h"
#include "vid.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"

typedef struct
{
    vec3_t	origin;
    vec3_t	angles;
    unsigned short 	modelindex; //qb: johnfitz -- was int
    unsigned short 	frame; //johnfitz -- was int
    unsigned char 	colormap; //johnfitz -- was int
    unsigned char 	skin; //johnfitz -- was int
    int		effects;
    byte   alpha; //qb:
    unsigned short nodrawtoclient; //qb: Team Xlink DP_SV_NODRAWTOCLIENT
    unsigned short drawonlytoclient; //Team Xlink DP_SV_DRAWONLYTOCLIENT
    unsigned short exteriormodeltoclient; //qb:
} entity_state_t;

#include "wad.h"
#include "draw.h"
#include "cvar.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"
#include "sound.h"
#include "render.h"
#include "client.h"
#include "progs.h"
#include "server.h"

#include "model.h"
#include "d_iface.h"

#include "input.h"
#include "world.h"
#include "keys.h"
#include "console.h"
#include "view.h"
#include "menu.h"
#include "cdaudio.h"

//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

typedef struct
{
    char	*basedir;
    char	*cachedir;		// for development over ISDN lines
    int		argc;
    char	**argv;
    void	*membase;
    int		memsize;
} quakeparms_t;


//=============================================================================



extern qboolean noclip_anglehack;
extern	kbutton_t	in_forward, in_forward2, in_back;

extern	portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];  //qb QS

//
// host
//
extern	quakeparms_t host_parms;

extern qboolean bumper_on;  //DEMO_REWIND qb: Baker change
extern	qboolean	host_initialized;		// true if into command execution
extern	double		host_frametime;
extern	byte		*host_basepal;
extern	byte		*host_colormap;
extern	int			host_framecount;	// incremented every frame, never reset
extern	double		realtime;			// not bounded in any way, changed at
// start of every frame, never reset

extern double	newtime;

extern int min_vid_width; //qb: Dan East

//qb: qrack complete command begin
extern	char	key_lines[CMDLINES][MAXCMDLINE];
extern	int	edit_line;
extern	int	key_linepos;

static	char	compl_common[MAX_FILELENGTH];
static	int	compl_len;
static	int	compl_clen;

//qb: qrack complete command end

extern	unsigned  con_linewidth;

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (quakeparms_t *parms);
void Host_Shutdown(char *status);
void Host_Error (char *error, ...);
void Host_EndGame (char *message, ...);
void Host_Frame (float time);
void Host_Quit_f (void);
void Host_ClientCommands (char *fmt, ...);
void Host_ShutdownServer (qboolean crash);

//qb: add declarations
void COM_CreatePath (char *path);
void Sys_InitDoubleTime (void);
void COM_CreatePath (char *path);
void LOC_LoadLocations (void);
void Sys_InitDoubleTime (void);
void History_Shutdown (void);
void CRC_Init(unsigned short *crcvalue);
void CRC_ProcessByte(unsigned short *crcvalue, byte data);
unsigned short CRC_Value(unsigned short crcvalue);
void SV_LocalSound (client_t *client, char *sample, char volume);
int R_LoadPalette (char *name);
void Fog_ParseServerMessage (void);
qboolean R_LoadSkybox (char *name);
void CL_Clear_Demos_Queue (void);
qboolean SV_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace);  //qb: move externs out of c files.
void M_Video_f (void); //qb: from Manoel Kasimier - edited
void M_Print (int cx, int cy, char *str);
void M_PrintWhite (int cx, int cy, char *str);
void M_DrawCharacter (int cx, int line, int num, qboolean sscale);

extern qboolean		msg_suppress_1;		// suppresses resolution and cache size console output
//  an fullscreen DIB focus gain/loss
extern int			current_skill;		// skill level for currently loaded level (in case
//  the user changes the cvar while the level is
//  running, this reflects the level actually in use)

extern int  stretched; //qb: video stretch, also used by pcx and avi capture.
extern int	coloredlights; //qb: colored lights on, set at map load.

extern qboolean		isDedicated;
extern	int			sb_lines;			// scan lines to draw
extern	float		scr_2d_scale_h, scr_2d_scale_v;
extern int min_vid_width, min_vid_height;  //qb: Dan East

extern int		minimum_memory;
extern byte     palmapnofb[32][32][32];
extern byte	    palmap[32][32][32];
extern byte		*draw_chars;

extern cvar_t   cmdline;
extern cvar_t	developer;
extern cvar_t	host_timescale;
extern cvar_t	chase_active;
extern cvar_t   snd_speed; //qb
extern cvar_t   vid_mode; //qb
extern cvar_t	sys_ticrate;
extern cvar_t	sys_nostdout;
extern cvar_t   r_palette;
extern cvar_t	r_shadowhack; //qb: engoo shadowhack
extern cvar_t   r_shadowhacksize; //qb
extern cvar_t   m_look; // Manoel Kasimier - m_look
extern cvar_t	bgmvolume;  //qb: QS
extern cvar_t	vid_stretch_by_2;

//
// chase
//
void Chase_Init (void);
void Chase_Update (void);

dfunction_t *ED_FindFunction (char *name);	// FrikaC - qcexec function
// 2001-10-20 TIMESCALE extension by Tomaz/Maddes  start
extern	double	host_org_frametime;extern int current_protocol; //qb

#ifdef WEBDL    //qb: sometimes works, needs more testing
extern cvar_t cl_web_download; //qb: R00k / Baker tute
extern cvar_t cl_web_download_url;

extern int Web_Get( const char *url, const char *referer, const char *name, int resume,
                    int max_downloading_time, int timeout, int ( *_progress )(double) );
#endif
