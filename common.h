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
// comndef.h  -- general definitions

#if !defined BYTE_DEFINED
typedef unsigned char 		byte;
#define BYTE_DEFINED 1
#endif

#undef true
#undef false

typedef enum {false, true}	qboolean;

//============================================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Sys_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte	*data;
	unsigned int		maxsize; //qb: allow 65535 max
	unsigned int		cursize;
} sizebuf_t;

void SZ_Alloc (sizebuf_t *buf, int startsize);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

//qb:  fs_handle_t from quakespasm
/* The following FS_*() stdio replacements are necessary if one is
 * to perform non-sequential reads on files reopened on pak files
 * because we need the bookkeeping about file start/end positions.
 * Allocating and filling in the fshandle_t structure is the users'
 * responsibility when the file is initially opened. */

typedef struct _fshandle_t
{
	FILE *file;
	qboolean pak;	/* is the file read from a pak */
	long start;	/* file or data start position */
	long length;	/* file or data size */
	long pos;	/* current position relative to start */
} fshandle_t;

size_t FS_fread(void *ptr, size_t size, size_t nmemb, fshandle_t *fh);
int FS_fseek(fshandle_t *fh, long offset, int whence);
long FS_ftell(fshandle_t *fh);
void FS_rewind(fshandle_t *fh);
int FS_feof(fshandle_t *fh);
int FS_ferror(fshandle_t *fh);
int FS_fclose(fshandle_t *fh);
char *FS_fgets(char *s, int size, fshandle_t *fh);

void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (int)&(((t *)0)->m)))

//============================================================================

#ifndef NULL
#define NULL ((void *)0)
#endif

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT	((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT 	((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

//============================================================================

extern	qboolean		host_bigendian;

extern	short	(*BigShort) (short l);
extern	short	(*LittleShort) (short l);
extern	int	(*BigLong) (int l);
extern	int	(*LittleLong) (int l);
extern	float	(*BigFloat) (float l);
extern	float	(*LittleFloat) (float l);

//============================================================================

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);
void MSG_WriteAngle16 (sizebuf_t *sb, float f);

extern	int			msg_readcount;
extern	qboolean	msg_badread;

void MSG_BeginReading (void);
int MSG_ReadChar (void);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);

float MSG_ReadCoord (void);
float MSG_ReadAngle (void);
float MSG_ReadAngle16 (void);

//============================================================================

void Q_strcpy (char *dest, char *src);
void Q_strncpy (char *dest, char *src, int count);
unsigned int Q_strlen (char *str);
char *Q_strrchr (char *s, char c);
void Q_strcat (char *dest, char *src);
int Q_strcmp (char *s1, char *s2);
int Q_strncmp (char *s1, char *s2, int count);
int Q_strcasecmp (char *s1, char *s2);
int Q_strncasecmp (char *s1, char *s2, int n);
int	Q_atoi (char *str);
float Q_atof (char *str);

//============================================================================

extern	char		com_token[1024];
extern	qboolean	com_eof;

char *COM_Parse (char *data);


extern	int		com_argc;
extern	char	**com_argv;

extern	int	file_from_pak;	//qb: QS - global indicating that file came from a pak


int COM_CheckParm (char *parm);
void COM_Init (char *path);
void COM_InitArgv (int argc, char **argv);

char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
void COM_FileBase (char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);

char	*va(char *format, ...);
// does a varargs printf into a temp buffer


//============================================================================

// 2001-09-12 Returning from which searchpath a file was loaded by Maddes  start
// copied from common.c
/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

//
// in memory
//
typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	int		handle;
	int		numfiles;
	packfile_t	*files;
} pack_t;

//
// on disk
//
typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK	2048

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;	// only one of filename / pack will be used
	struct searchpath_s	*next;
} searchpath_t;
// 2001-09-12 Returning from which searchpath a file was loaded by Maddes  end

// 2001-09-12 Returning information about loaded file by Maddes  start
// new structure for passing back a loaded file
typedef struct loadedfile_s
{
	byte			*data;		// memory the file is loaded to (directly before this structure)
	int				filelen;	// length of the file
	searchpath_t	*path;		// 2001-09-12 Returning from which searchpath a file was loaded by Maddes
} loadedfile_t;
// 2001-09-12 Returning information about loaded file by Maddes  end

extern int com_filesize;
struct cache_user_s;

extern	char	com_gamedir[MAX_OSPATH];

//qb: qrack begin
extern	char	com_basedir[MAX_OSPATH];	// by joe

extern searchpath_t	*com_searchpaths;

//qb: qrack end


void COM_WriteFile (char *filename, void *data, int len);
// 2001-09-12 Returning from which searchpath a file was loaded by Maddes  start
//int COM_OpenFile (char *filename, int *hndl);
//int COM_FOpenFile (char *filename, FILE **file);
int COM_OpenFile (char *filename, int *hndl, searchpath_t **foundpath);
int COM_FOpenFile (char *filename, FILE **file, searchpath_t **foundpath);
// 2001-09-12 Returning from which searchpath a file was loaded by Maddes  end
void COM_CloseFile (int h);

// 2001-09-12 Returning information about loaded file by Maddes  start
/*
byte *COM_LoadStackFile (char *path, void *buffer, int bufsize);
byte *COM_LoadTempFile (char *path);
byte *COM_LoadHunkFile (char *path);
void COM_LoadCacheFile (char *path, struct cache_user_s *cu);
*/
loadedfile_t *COM_LoadStackFile (char *path, void *buffer, int bufsize);
loadedfile_t *COM_LoadTempFile (char *path);
loadedfile_t *COM_LoadHunkFile (char *path);
loadedfile_t *COM_LoadCacheFile (char *path, struct cache_user_s *cu);
// 2001-09-12 Returning information about loaded file by Maddes  end


extern	struct cvar_s	registered;

extern qboolean		standard_quake, rogue, hipnotic;

searchpath_t *COM_GetDirSearchPath(searchpath_t *startsearch);	// 2001-09-12 Finding the last searchpath of a directory

// Manoel Kasimier - begin
void M_PopUp_f (char *s, char *cmd);
void M_RefreshSaveList (void);
// Manoel Kasimier - end

//qb: jqavi begin
#ifdef _WIN32
#define   vsnprintf _vsnprintf
#endif

void Q_strncpyz (char *dest, char *src, size_t size);
void Q_snprintfz (char *dest, size_t size, char *fmt, ...);

void COM_ForceExtension (char *path, char *extension);
//qb: jqavi end

void COM_GetFolder (char *in, char *out);//qb: R00k /Baker tute
