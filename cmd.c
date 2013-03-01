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

// cmd.c -- Quake script command processing module

#include "quakedef.h"
#include "stdio.h"

#ifdef FLASH
#undef _WIN32 //qb: FIXME - how to not def _win32 in CodeBlocks
#endif

#ifdef _WIN32
#include "s_win32/winquake.h"
#endif

void Cmd_ForwardToServer (void);

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s
{
    struct cmdalias_s	*next;
    char	name[MAX_ALIAS_NAME];
    char	*value;
} cmdalias_t;

cmdalias_t	*cmd_alias;

int trashtest;
int *trashspot;

qboolean	cmd_wait;

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
    cmd_wait = true;
}

/*
=============================================================================

COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
    SZ_Alloc (&cmd_text, 8192);		// space for commands and script files
}


/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
    int		l;

    l = Q_strlen (text);

    if (cmd_text.cursize + l >= cmd_text.maxsize)
    {
        Con_Printf ("Cbuf_AddText: overflow\n");
        return;
    }

    SZ_Write (&cmd_text, text, Q_strlen (text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
    char	*temp;
    int		templen;

    // copy off any commands still remaining in the exec buffer
    templen = cmd_text.cursize;
    if (templen)
    {
        temp = Q_calloc (templen);
        memcpy (temp, cmd_text.data, templen);
        SZ_Clear (&cmd_text);
    }
    else
        temp = NULL;	// shut up compiler

    // add the entire text of the file
    Cbuf_AddText (text);

    // add the copied off data
    if (templen)
    {
        SZ_Write (&cmd_text, temp, templen);
        free (temp);
    }
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
    int		i;
    char	*text;
    char	line[1024];
    int		quotes;

    while (cmd_text.cursize)
    {
        // find a \n or ; line break
        text = (char *)cmd_text.data;

        quotes = 0;
        for (i=0 ; i< cmd_text.cursize ; i++)
        {
            if (text[i] == '"')
                quotes++;
            if ( !(quotes&1) &&  text[i] == ';')
                break;	// don't break if inside a quoted string
            if (text[i] == '\n')
                break;
        }


        memcpy (line, text, i);
        line[i] = 0;

        // delete the text from the command buffer and move remaining commands down
        // this is necessary because commands (exec, alias) can insert data at the
        // beginning of the text buffer

        if (i == cmd_text.cursize)
            cmd_text.cursize = 0;
        else
        {
            i++;
            cmd_text.cursize -= i;
            memcpy (text, text+i, cmd_text.cursize);
        }

        // execute the command line
        Cmd_ExecuteString (line, src_command);
        //Con_DPrintf("Cmd_ExecuteString: %s \n", line);  //qb: qc parse debug

        if (cmd_wait)
        {
            // skip out while text still remains in buffer, leaving it
            // for next frame
            cmd_wait = false;
            break;
        }
    }
}

/*
==============================================================================

SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f //qb: from Fitzquake, brilliant!
-- johnfitz -- rewritten to read the "cmdline" cvar, for use with dynamic mod loading

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f (void)
{
    char	cmds[256];
    int		i, j, plus;

    plus = true;
    j = 0;
    for (i=0; cmdline.string[i]; i++)
    {
        if (cmdline.string[i] == '+')
        {
            plus = true;
            if (j > 0)
            {
                cmds[j-1] = ';';
                cmds[j++] = ' ';
            }
        }
        else if (cmdline.string[i] == '-' &&
                 (i==0 || cmdline.string[i-1] == ' ')) //johnfitz -- allow hypenated map names with +map
            plus = false;
        else if (plus)
            cmds[j++] = cmdline.string[i];
    }
    cmds[j] = 0;

    Cbuf_InsertText (cmds);
}


/*
===============
Cmd_Exec_f
===============
*/
#define	MAX_ARGS		80 // Manoel Kasimier - config.cfg replacement
static	char		*cmd_argv[MAX_ARGS]; // Manoel Kasimier - config.cfg replacement
void Cmd_Exec_f (void)
{
    char	*f;
    int		mark;
    loadedfile_t	*fileinfo;	// 2001-09-12 Returning information about loaded file by Maddes

    if (Cmd_Argc () != 2)
    {
        Con_Printf ("exec <filename> : execute a script file\n");
        return;
    }

    mark = Hunk_LowMark ();
#ifdef FLASH
    as3ReadFileSharedObject(va("%s/%s", com_gamedir, Cmd_Argv(1)));//config.cfg is stored in the flash shared objects
#endif
    // Manoel Kasimier - config.cfg replacement - begin
    if (!Q_strcmp(Cmd_Argv(1), "config.cfg"))
    {
        int h;
        COM_OpenFile ("super8.cfg", &h, NULL);
        if (h != -1)
        {
            Q_strcpy(cmd_argv[1], "super8.cfg"); // "config.cfg" and "super8.cfg" have the same size, so there's no need to realloc the buffer
            COM_CloseFile (h);
        }
    }
    //	if (!f)
    // Manoel Kasimier - config.cfg replacement - end
    // 2001-09-12 Returning information about loaded file by Maddes  start
    /*
    f = (char *)COM_LoadHunkFile (Cmd_Argv(1));
    if (!f)
    */
    fileinfo = COM_LoadHunkFile (Cmd_Argv(1));
    if (!fileinfo)
        // 2001-09-12 Returning information about loaded file by Maddes  end
    {
        Con_Printf ("couldn't exec %s\n",Cmd_Argv(1));
        return;
    }
    f = (char *)fileinfo->data;	// 2001-09-12 Returning information about loaded file by Maddes
    Con_DPrintf ("execing %s\n",Cmd_Argv(1)); // edited

    Cbuf_InsertText ("\n"); // Manoel Kasimier - for files that doesn't end with a newline
    Cbuf_InsertText (f);
    // Manoel Kasimier - begin
    // skip a frame to prevent possible crashes when the engine is started with the "-map" parameter
    //	if (!Q_strcmp(Cmd_Argv(1), "quake.rc"))
    Cbuf_InsertText ("wait\n");
    // Manoel Kasimier - end
    Hunk_FreeToLowMark (mark);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
    int		i;

    for (i=1 ; i<Cmd_Argc() ; i++)
        Con_Printf ("%s ",Cmd_Argv(i));
    Con_Printf ("\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/

char *CopyString (char *in)
{
    char	*out;

    out = Q_calloc (Q_strlen(in)+1);
    Q_strcpy (out, in);
    return out;
}

void Cmd_Alias_f (void)
{
    cmdalias_t	*a;
    char		cmd[1024];
    int			i, c;
    char		*s;

    if (Cmd_Argc() == 1)
    {
        Con_Printf ("Current alias commands:\n");
        for (a = cmd_alias ; a ; a=a->next)
            Con_Printf ("%s : %s\n", a->name, a->value);
        return;
    }

    s = Cmd_Argv(1);
    if (Q_strlen(s) >= MAX_ALIAS_NAME)
    {
        Con_Printf ("Alias name is too long\n");
        return;
    }

    // if the alias allready exists, reuse it
    for (a = cmd_alias ; a ; a=a->next)
    {
        if (!Q_strcmp(s, a->name))
        {
            free (a->value);
            break;
        }
    }

    if (!a)
    {
        a = Q_calloc (sizeof(cmdalias_t));
        a->next = cmd_alias;
        cmd_alias = a;
    }
    Q_strcpy (a->name, s);

    // copy the rest of the command line
    cmd[0] = 0;		// start out with a null string
    c = Cmd_Argc();
    for (i=2 ; i< c ; i++)
    {
        Q_strcat (cmd, Cmd_Argv(i));
        if (i != c)
            Q_strcat (cmd, " ");
    }
    Q_strcat (cmd, "\n");

    a->value = CopyString (cmd);
}


// Manoel Kasimier - auto-completion of aliases - begin
/*
============
Cmd_CompleteAlias
============
*/
char *Cmd_CompleteAlias (char *partial)
{
    cmdalias_t		*a;
    int				len;

    len = Q_strlen(partial);

    if (!len)
        return NULL;

    // check aliases
    for (a = cmd_alias ; a ; a=a->next)
        if (!Q_strncmp (partial,a->name, len))
            return a->name;

    return NULL;
}
// Manoel Kasimier - auto-completion of aliases - end

/*
=============================================================================

COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
    struct cmd_function_s	*next;
    char					*name;
    xcommand_t				function;
} cmd_function_t;


#define	MAX_ARGS		80

static	int			cmd_argc;
static	char		*cmd_argv[MAX_ARGS];
static	char		*cmd_null_string = "";
static	char		*cmd_args = NULL;

cmd_source_t	cmd_source;


static	cmd_function_t	*cmd_functions;		// possible commands to execute


/*
============
Cvar_List_f   -- johnfitz
============
*/
void Cvar_List_f (void) //qb: from Fitzquake
{
    cvar_t	*cvar;
    char 	*partial;
    int		len, count;

    if (Cmd_Argc() > 1)
    {
        partial = Cmd_Argv (1);
        len = Q_strlen(partial);
    }
    else
    {
        partial = NULL;
        len = 0;
    }

    count=0;
    for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
    {
        if (partial && Q_strncmp (partial,cvar->name, len))
        {
            continue;
        }
        Con_Printf ("%s%s %s \"%s\"\n",
                    cvar->archive ? "*" : " ",
                    cvar->server ? "s" : " ",
                    cvar->name,
                    cvar->string);
        count++;
    }

    Con_Printf ("%i cvars", count);
    if (partial)
    {
        Con_Printf (" beginning with \"%s\"", partial);
    }
    Con_Printf ("\n");
}


/*
============
CmdList_f  -- johnfitz
============
*/
void Cmd_List_f (void) //qb: from Fitzquake
{
    cmd_function_t	*cmd;
    char 			*partial;
    int				len, count;

    if (Cmd_Argc() > 1)
    {
        partial = Cmd_Argv (1);
        len = Q_strlen(partial);
    }
    else
    {
        partial = NULL;
        len = 0;
    }

    count=0;
    for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
    {
        if (partial && Q_strncmp (partial,cmd->name, len))
        {
            continue;
        }
        Con_Printf ("   %s\n", cmd->name);
        count++;
    }

    Con_Printf ("%i commands", count);
    if (partial)
    {
        Con_Printf (" beginning with \"%s\"", partial);
    }
    Con_Printf ("\n");
}

/*
===============
Cmd_Unalias_f -- johnfitz
===============
*/
void Cmd_Unalias_f (void) //qb: from Fitzquake
{
    cmdalias_t	*a, *prev;

    switch (Cmd_Argc())
    {
    default:
    case 1:
        Con_Printf("unalias <name> : delete alias\n");
        break;
    case 2:
        for (prev = a = cmd_alias; a; a = a->next)
        {
            if (!strcmp(Cmd_Argv(1), a->name))
            {
                prev->next = a->next;
                free (a->value);
                free (a);
                prev = a;
                return;
            }
            prev = a;
        }
        break;
    }
}

/*
===============
Cmd_Unaliasall_f -- johnfitz
===============
*/
void Cmd_Unaliasall_f (void) //qb: from Fitzquake
{
    cmdalias_t	*blah;

    while (cmd_alias)
    {
        blah = cmd_alias->next;
        free(cmd_alias->value);
        free(cmd_alias);
        cmd_alias = blah;
    }
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
    //
    // register our commands
    //

    Cmd_AddCommand ("cmdlist", Cmd_List_f);	// qb: these from  Fitzquake
    Cmd_AddCommand ("unalias", Cmd_Unalias_f);
    Cmd_AddCommand ("unaliasall", Cmd_Unaliasall_f);
    Cmd_AddCommand ("cvarlist", Cvar_List_f);

    Cmd_AddCommand ("stuffcmds",Cmd_StuffCmds_f);
    Cmd_AddCommand ("exec",Cmd_Exec_f);
    Cmd_AddCommand ("echo",Cmd_Echo_f);
    Cmd_AddCommand ("alias",Cmd_Alias_f);
    Cmd_AddCommand ("cmd", Cmd_ForwardToServer);
    Cmd_AddCommand ("wait", Cmd_Wait_f);
}

/*
============
Cmd_Argc
============
*/
int		Cmd_Argc (void)
{
    return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char	*Cmd_Argv (int arg)
{
    if ( /*(unsigned)*/arg >= cmd_argc ) // signed/unsigned mismatch fixed
        return cmd_null_string;
    return cmd_argv[arg];
}

/*
============
Cmd_Args
============
*/
char		*Cmd_Args (void)
{
    return cmd_args;
}


/*
============
Cmd_TokenizeString //qb: remove zone

Parses the given string into command line tokens.
============
*/
void Cmd_TokenizeString (char *text)
{
    // the maximum com_token is 1024 so the command buffer will never be larger than this
    static char argbuf[MAX_ARGS * (1024 + 1)];
    char *currarg = argbuf;

    cmd_argc = 0;
    cmd_args = NULL;

    while (1)
    {
        // skip whitespace up to a /n
        while (*text && *text <= ' ' && *text != '\n')
            text++;

        if (*text == '\n')
        {
            // a newline seperates commands in the buffer
            text++;
            break;
        }

        if (!*text) return;
        if (cmd_argc == 1) cmd_args = text;
        if (!(text = COM_Parse (text))) return;

        if (cmd_argc < MAX_ARGS)
        {
            cmd_argv[cmd_argc] = currarg;
            Q_strcpy (cmd_argv[cmd_argc], com_token);
            currarg += Q_strlen (com_token) + 1;
            cmd_argc++;
        }
    }
}

/*
============
Cmd_AddCommand
============
*/
void	Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
    cmd_function_t	*cmd;

    if (host_initialized)	// because hunk allocation would get stomped
        Sys_Error ("Cmd_AddCommand after host_initialized");

    // fail if the command is a variable name
    if (Cvar_VariableString(cmd_name)[0])
    {
        Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
        return;
    }

    // fail if the command already exists
    for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
    {
        if (!Q_strcmp (cmd_name, cmd->name))
        {
            Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
            return;
        }
    }

    cmd = Hunk_Alloc (sizeof(cmd_function_t));
    cmd->name = cmd_name;
    cmd->function = function;
    cmd->next = cmd_functions;
    cmd_functions = cmd;
}

/*
============
Cmd_Exists
============
*/
qboolean	Cmd_Exists (char *cmd_name)
{
    cmd_function_t	*cmd;

    for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
    {
        if (!Q_strcmp (cmd_name,cmd->name))
            return true;
    }

    return false;
}


/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand (char *partial)
{
    cmd_function_t	*cmd;
    int		len;

    if (!(len = Q_strlen(partial)))
        return NULL;

    // check functions
    for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
        if (!Q_strncasecmp(partial, cmd->name, len))
            return cmd->name;

    return NULL;
}

//qb: qrack command line begin

/*
============
Cmd_CompleteCountPossible
============
*/
int Cmd_CompleteCountPossible (char *partial)
{
    cmd_function_t	*cmd;
    int		len, c = 0;

    if (!(len = Q_strlen(partial)))
        return 0;

    for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
        if (!Q_strncasecmp(partial, cmd->name, len))
            c++;

    return c;
}
static void FindCommonSubString (char *s)
{
    if (!compl_clen)
    {
        Q_strncpyz (compl_common, s, sizeof(compl_common));
        compl_clen = Q_strlen (compl_common);
    }
    else
    {
        while (compl_clen > compl_len && Q_strncasecmp(s, compl_common, compl_clen))
            compl_clen--;
    }
}

static void CompareParams (void)
{
    int	i;

    compl_clen = 0;

    for (i=0 ; i<num_files ; i++)
        FindCommonSubString (filelist[i].name);

    if (compl_clen)
        compl_common[compl_clen] = 0;
}

static void PrintEntries (void);
void EraseDirEntries (void);

//qb: qrack list maps begin
static qboolean CheckRealBSP (char *bspname)
{
    if (!Q_strcmp(bspname, "b_batt0.bsp") ||
            !Q_strcmp(bspname, "b_batt1.bsp") ||
            !Q_strcmp(bspname, "b_bh10.bsp") ||
            !Q_strcmp(bspname, "b_bh100.bsp") ||
            !Q_strcmp(bspname, "b_bh25.bsp") ||
            !Q_strcmp(bspname, "b_explob.bsp") ||
            !Q_strcmp(bspname, "b_nail0.bsp") ||
            !Q_strcmp(bspname, "b_nail1.bsp") ||
            !Q_strcmp(bspname, "b_rock0.bsp") ||
            !Q_strcmp(bspname, "b_rock1.bsp") ||
            !Q_strcmp(bspname, "b_shell0.bsp") ||
            !Q_strcmp(bspname, "b_shell1.bsp") ||
            !Q_strcmp(bspname, "b_exbox2.bsp"))
        return false;

    return true;
}

static void toLower (char* str)		// for strings
{
    char	*s;
    int	i;

    i = 0;
    s = str;

    while (*s)
    {
        if (*s >= 'A' && *s <= 'Z')
            *(str + i) = *s + 32;
        i++;
        s++;
    }
}


static void AddNewEntry (char *fname, int ftype, long fsize)
{
    int	i, pos;

    filelist = Q_realloc (filelist, (num_files + 1) * sizeof(direntry_t));

#ifdef _WIN32
    toLower (fname);
    // else don't convert, linux is case sensitive
#endif

    // inclusion sort
    for (i=0 ; i<num_files ; i++)
    {
        if (ftype < filelist[i].type)
            continue;
        else if (ftype > filelist[i].type)
            break;

        if (Q_strcmp(fname, filelist[i].name) < 0)
            break;
    }
    pos = i;
    for (i=num_files ; i>pos ; i--)
        filelist[i] = filelist[i-1];

    filelist[i].name = Q_strdup (fname);
    filelist[i].type = ftype;
    filelist[i].size = fsize;

    num_files++;
}

static void AddNewEntry_unsorted (char *fname, int ftype, long fsize)
{
    filelist = Q_realloc (filelist, (num_files + 1) * sizeof(direntry_t));

#ifdef _WIN32
    toLower (fname);
    // else don't convert, linux is case sensitive
#endif
    filelist[num_files].name = Q_strdup (fname);
    filelist[num_files].type = ftype;
    filelist[num_files].size = fsize;

    num_files++;
}

void EraseDirEntries (void)
{
    if (filelist)
    {
        Q_free (filelist);
        filelist = NULL;
        num_files = 0;
    }
}

static qboolean CheckEntryName (char *ename)
{
    int	i;

    for (i=0 ; i<num_files ; i++)
        if (!Q_strcasecmp(ename, filelist[i].name))
            return true;

    return false;
}

#define SLASHJMP(x, y)			\
	if (!(x = Q_strrchr(y, '/')))	\
	x = y;			\
	else				\
	*++x

/*
=================
ReadDir			-- by joe
=================
*/
int	num_files = 0;
void ReadDir (char *path, char *the_arg)
{
    //qb: FIXME for other ports: windows only
#ifdef _WIN32
    HANDLE		h;
    WIN32_FIND_DATA	fd;
    if (path[Q_strlen(path)-1] == '/')
        path[Q_strlen(path)-1] = 0;

    if (!(RDFlags & RD_NOERASE))
        EraseDirEntries ();

    h = FindFirstFile (va("%s/%s", path, the_arg), &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        if (RDFlags & RD_MENU_DEMOS)
        {
            AddNewEntry ("Error reading directory", 3, 0);
            num_files = 1;
        }
        else if (RDFlags & RD_COMPLAIN)
        {
            Con_Printf ("No such file\n");
        }
        goto end;
    }

    if (RDFlags & RD_MENU_DEMOS && !(RDFlags & RD_MENU_DEMOS_MAIN))
    {
        AddNewEntry ("..", 2, 0);
        num_files = 1;
    }

    do
    {
        int	fdtype;
        long	fdsize;
        char	filename[MAX_FILELENGTH];


        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!(RDFlags & (RD_MENU_DEMOS | RD_GAMEDIR)) || !Q_strcmp(fd.cFileName, ".") || !Q_strcmp(fd.cFileName, ".."))
                continue;

            fdtype = 1;
            fdsize = 0;
            Q_strncpyz (filename, fd.cFileName, sizeof(filename));
        }
        else
        {
            char	ext[8];

            if (RDFlags & RD_GAMEDIR)
                continue;

            Q_strncpyz (ext, COM_FileExtension(fd.cFileName), sizeof(ext));

            if (RDFlags & RD_MENU_DEMOS && Q_strcasecmp(ext, "dem") && Q_strcasecmp(ext, "dz"))
                continue;

            fdtype = 0;
            fdsize = fd.nFileSizeLow;
            if (Q_strcasecmp(ext, "dz") && RDFlags & (RD_STRIPEXT | RD_MENU_DEMOS))
            {
                COM_StripExtension (fd.cFileName, filename);
                if (RDFlags & RD_SKYBOX)
                    filename[Q_strlen(filename)-2] = 0;	// cut off skybox_ext
            }
            else
            {
                Q_strncpyz (filename, fd.cFileName, sizeof(filename));
            }

            if (CheckEntryName(filename))
                continue;	// file already on list
        }

        AddNewEntry (filename, fdtype, fdsize);
    }
    while (FindNextFile(h, &fd));
    FindClose (h);

    if (!num_files)
    {
        if (RDFlags & RD_MENU_DEMOS)
        {
            AddNewEntry ("[ no files ]", 3, 0);
            num_files = 1;
        }
        else if (RDFlags & RD_COMPLAIN)
        {
            Con_Printf ("No such file\n");
        }
    }

end:
    RDFlags = 0;
#endif
}


/*
=================
FindFilesInPak

Search for files inside a PAK file		-- by joe
=================
*/
int	pak_files = 0;
void FindFilesInPak (char *the_arg)
{
    int		i;
    searchpath_t	*search;
    pack_t		*pak;
    char		*myarg;

    SLASHJMP(myarg, the_arg);
    for (search = com_searchpaths ; search ; search = search->next)
    {
        if (search->pack)
        {
            char	*s, *p, ext[8], filename[MAX_FILELENGTH];

            // look through all the pak file elements
            pak = search->pack;
            for (i=0 ; i<pak->numfiles ; i++)
            {
                s = pak->files[i].name;
                Q_strncpyz (ext, COM_FileExtension(s), sizeof(ext));
                if (!Q_strcasecmp(ext, COM_FileExtension(myarg)))
                {
                    SLASHJMP(p, s);
                    if (!Q_strcasecmp(ext, "bsp") && !CheckRealBSP(p))
                        continue;
                    if (!Q_strncasecmp(s, the_arg, Q_strlen(the_arg)-5) ||
                            (*myarg == '*' && !Q_strncasecmp(s, the_arg, Q_strlen(the_arg)-5-compl_len)))
                    {
                        COM_StripExtension (p, filename);
                        if (CheckEntryName(filename))
                            continue;
                        AddNewEntry_unsorted (filename, 0, pak->files[i].filelen);
                        pak_files++;
                    }
                }
            }
        }
    }
}

/*
==================
PaddedPrint
==================
*/
#define	COLUMNWIDTH	20
#define	MINCOLUMNWIDTH	18	// the last column may be slightly smaller

extern	int	con_x;

static void PaddedPrint (char *s)
{
    int		nextcolx = 0;

    if (con_x)
        nextcolx = (int)((con_x + COLUMNWIDTH) / COLUMNWIDTH) * COLUMNWIDTH;

    if (nextcolx > con_linewidth - MINCOLUMNWIDTH
            || (con_x && nextcolx + Q_strlen(s) >= con_linewidth))
        Con_Printf ("\n");

    if (con_x)
        Con_Printf (" ");
    while (con_x % COLUMNWIDTH)
        Con_Printf (" ");
    Con_Printf ("%s", s);
}

static void PrintEntries (void)
{
    int	i, filectr;

    filectr = pak_files ? (num_files - pak_files) : 0;

    for (i=0 ; i<num_files ; i++)
    {
        if (!filectr-- && pak_files)
        {
            if (con_x)
            {
                Con_Printf ("\n");
                Con_Printf ("\x02" "inside pack file:\n");
            }
            else
            {
                Con_Printf ("\x02" "inside pack file:\n");
            }
        }
        PaddedPrint (filelist[i].name);
    }

    if (con_x)
        Con_Printf ("\n");
}


/*
==================
CompleteCommand

Advanced command completion

Main body and many features imported from ZQuake	-- joe
==================
*/
void CompleteCommand (void)
{
    int	c, v;
    char	*s, *cmd;

    s = key_lines[edit_line] + 1;
    if (!(compl_len = Q_strlen(s)))
        return;
    compl_clen = 0;

    c = Cmd_CompleteCountPossible (s);
    v = Cvar_CompleteCountPossible (s);

    if (c + v > 1)
    {
        Con_Printf ("\n");

        if (c)
        {
            cmd_function_t	*cmd;

            Con_Printf ("\x02" "commands:\n");
            // check commands
            for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
            {
                if (!Q_strncasecmp(s, cmd->name, compl_len))
                {
                    Con_Printf ("%s\n",cmd->name);
                    FindCommonSubString (cmd->name);
                }
            }
            Con_Printf ("\n");
        }

        if (v)
        {
            cvar_t		*var;

            Con_Printf ("\x02" "variables:\n");
            // check variables
            for (var = cvar_vars ; var ; var = var->next)
            {
                if (!Q_strncasecmp(s, var->name, compl_len))
                {
                    Con_Printf ("%s\n",var->name);
                    FindCommonSubString (var->name);
                }
            }
            Con_Printf ("\n");
        }
    }

    if (c + v == 1)
    {
        if (!(cmd = Cmd_CompleteCommand(s)))
            cmd = Cvar_CompleteVariable (s);
    }
    else if (compl_clen)
    {
        compl_common[compl_clen] = 0;
        cmd = compl_common;
    }
    else
        return;

    Q_strcpy (key_lines[edit_line]+1, cmd);
    key_linepos = Q_strlen(cmd) + 1;
    if (c + v == 1)
        key_lines[edit_line][key_linepos++] = ' ';
    key_lines[edit_line][key_linepos] = 0;
}

int		RDFlags = 0;
direntry_t	*filelist = NULL;

#define	READDIR_ALL_PATH(p)							\
	for (search = com_searchpaths ; search ; search = search->next)		\
{									\
	if (!search->pack)						\
{								\
	RDFlags |= (RD_STRIPEXT | RD_NOERASE);			\
	if (skybox)						\
	RDFlags |= RD_SKYBOX;				\
	ReadDir (va("%s/%s", search->filename, subdir), p);	\
}								\
}

/*
============
Cmd_CompleteParameter	-- by joe

parameter completion for various commands
============
*/
void Cmd_CompleteParameter (char *partial, char *attachment)
{
    char		*s, *param, stay[MAX_QPATH], subdir[MAX_QPATH] = "", param2[MAX_QPATH];
    qboolean	skybox = false;

    Q_strncpyz (stay, partial, sizeof(stay));

    // we don't need "<command> + space(s)" included
    param = Q_strrchr (stay, ' ') + 1;
    if (!*param)		// no parameter was written in, so quit
        return;

    compl_len = Q_strlen (param);
    Q_strcat (param, attachment);

    if (!Q_strcmp(attachment, "*.bsp"))
    {
        Q_strncpyz (subdir, "maps/", sizeof(subdir));
    }
    if (!Q_strcmp(attachment, "*.mp3"))
    {
        Q_strncpyz (subdir, "music/", sizeof(subdir));
    }

    if (strstr(stay, "gamedir ") == stay)
    {
        RDFlags |= RD_GAMEDIR;
        ReadDir (com_basedir, param);

        pak_files = 0;	// so that previous pack searches are cleared
    }
    else if (strstr(stay, "load ") == stay || strstr(stay, "printtxt ") == stay)
    {
        RDFlags |= RD_STRIPEXT;
        ReadDir (com_gamedir, param);

        pak_files = 0;	// same here
    }
    else
    {
        searchpath_t	*search;

        EraseDirEntries ();
        pak_files = 0;

        READDIR_ALL_PATH(param);
        /* qb: omit
        if (!Q_strcmp(param + Q_strlen(param)-3, "tga"))
        {
        Q_strncpyz (param2, param, Q_strlen(param)-3);
        Q_strcat (param2, "png");
        READDIR_ALL_PATH(param2);
        FindFilesInPak (va("%s%s", subdir, param2));
        }
        else */
        if (!Q_strcmp(param + Q_strlen(param)-3, "dem"))
        {
            Q_strncpyz (param2, param, Q_strlen(param)-3);
            Q_strcat (param2, "dz");
            READDIR_ALL_PATH(param2);
            FindFilesInPak (va("%s%s", subdir, param2));
        }
        FindFilesInPak (va("%s%s", subdir, param));
    }

    if (!filelist)
        return;

    s = strchr (partial, ' ') + 1;
    // just made this to avoid printing the filename twice when there's only one match
    if (num_files == 1)
    {
        *s = '\0';
        Q_strcat (partial, filelist[0].name);
        key_linepos = Q_strlen(partial) + 1;
        key_lines[edit_line][key_linepos] = 0;
        return;
    }

    CompareParams ();

    Con_Printf ("]%s\n", partial);
    PrintEntries ();

    *s = '\0';
    Q_strcat (partial, compl_common);
    key_linepos = Q_strlen(partial) + 1;
    key_lines[edit_line][key_linepos] = 0;
}

//qb: qrack end

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void	Cmd_ExecuteString (char *text, cmd_source_t src)
{
    cmd_function_t	*cmd;
    cmdalias_t		*a;

    cmd_source = src;
    Cmd_TokenizeString (text);

    // execute the command line
    if (!Cmd_Argc())
        return;		// no tokens

    // check functions
    for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
    {
        if (!Q_strcasecmp (cmd_argv[0],cmd->name))
        {
            cmd->function ();
            return;
        }
    }

    // check alias
    for (a=cmd_alias ; a ; a=a->next)
    {
        if (!Q_strcasecmp (cmd_argv[0], a->name))
        {
            Cbuf_InsertText (a->value);
            return;
        }
    }

    // check cvars
    if (!Cvar_Command ())
        Con_Printf ("Unknown command \"%s\"\n", Cmd_Argv(0));

}

//qb: talk macros and location begin - from Baker's FQ Mark V
#define MAX_LOCATIONS 64
typedef struct
{
	vec3_t mins_corner;		// min xyz corner
	vec3_t maxs_corner;		// max xyz corner
	vec_t sd;				// sum of dimensions
	char name[32];
} location_t;

static location_t	locations[MAX_LOCATIONS];
static int			numlocations = 0;

/*
===============
LOC_LoadLocations

Load the locations for the current level from the location file
===============
*/
void LOC_LoadLocations (void)
{
	FILE *f;
	char		*ch;
	char		locs_filename[64];
	char		base_map_name[64];
	char		buff[256];
	location_t *thisloc;
	int i;
	float temp;

	numlocations = 0;

	COM_StripExtension (cl.worldmodel->name + 5 /* +5 = skip "maps/" */, base_map_name);
	sprintf (locs_filename, "locs/%s.loc", base_map_name);

	if (COM_FOpenFile (locs_filename, &f, cl.worldmodel->loadinfo.searchpath) == -1)
		return;

	thisloc = locations;
	while (!feof(f) && numlocations < MAX_LOCATIONS)
	{
		if (fscanf(f, "%f, %f, %f, %f, %f, %f, ", &thisloc->mins_corner[0], &thisloc->mins_corner[1], &thisloc->mins_corner[2], &thisloc->maxs_corner[0], &thisloc->maxs_corner[1], &thisloc->maxs_corner[2]) == 6)
		{
			thisloc->sd = 0;
			for (i = 0 ; i < 3 ; i++)
			{
				if (thisloc->mins_corner[i] > thisloc->maxs_corner[i])
				{
					temp = thisloc->mins_corner[i];
					thisloc->mins_corner[i] = thisloc->maxs_corner[i];
					thisloc->maxs_corner[i] = temp;
				}
				thisloc->sd += thisloc->maxs_corner[i] - thisloc->mins_corner[i];
			}
			thisloc->mins_corner[2] -= 32.0;
			thisloc->maxs_corner[2] += 32.0;
			fgets(buff, 256, f);

			ch = strrchr(buff, '\n');	if (ch)		*ch = 0;	// Eliminate trailing newline characters
			ch = strrchr(buff, '\"');	if (ch)		*ch = 0;	// Eliminate trailing quotes

			for (ch = buff ; *ch == ' ' || *ch == '\t' || *ch == '\"' ; ch++);	// Go through the string and forward past any spaces, tabs or double quotes to find start of the name

			strcpy (thisloc->name, ch);
			thisloc = &locations[++numlocations];
		}
		else
			fgets(buff, 256, f);
	}

	fclose(f);
}

/*
===============
LOC_GetLocation

Get the name of the location of a point
===============
*/
// return the nearest rectangle if you aren't in any (manhattan distance)
char *LOC_GetLocation (vec3_t worldposition)
{
	location_t *thisloc;
	location_t *bestloc;
	float dist, bestdist;

	if (numlocations == 0)
		return "(Not loaded)";


	bestloc = NULL;
	bestdist = 999999;
	for (thisloc = locations ; thisloc < locations + numlocations ; thisloc++)
	{
		dist =	fabs(thisloc->mins_corner[0] - worldposition[0]) + fabs(thisloc->maxs_corner[0] - worldposition[0]) +
				fabs(thisloc->mins_corner[1] - worldposition[1]) + fabs(thisloc->maxs_corner[1] - worldposition[1]) +
				fabs(thisloc->mins_corner[2] - worldposition[2]) + fabs(thisloc->maxs_corner[2] - worldposition[2]) - thisloc->sd;

		if (dist < .01)
			return thisloc->name;

		if (dist < bestdist)
		{
			bestdist = dist;
			bestloc = thisloc;
		}
	}
	if (bestloc)
		return bestloc->name;
	return "somewhere";
}



char *Weapons_String (void)
{
	static char weapons_string[256];

	memset (weapons_string, 0, sizeof(weapons_string));

	strcat (weapons_string, "(");
	if (cl.items & IT_ROCKET_LAUNCHER)  strcat (weapons_string, "RL ");
	if (cl.items & IT_GRENADE_LAUNCHER) strcat (weapons_string, "GL ");
	if (cl.items & IT_LIGHTNING)  		strcat (weapons_string, "LG ");

	if (strlen (weapons_string) == 1) // No good weapons.
	{
		if (cl.items & IT_SUPER_NAILGUN)  	strcat (weapons_string, "SNG ");
		if (cl.items & IT_SUPER_SHOTGUN)  	strcat (weapons_string, "SSG ");
		if (cl.items & IT_SUPER_NAILGUN)  	strcat (weapons_string, "NG ");
	}

	// Not even bad weapons
	if (strlen (weapons_string) == 1) 		strcat (weapons_string, "none");

	strcat (weapons_string, ")");

	return weapons_string;
}

char *Powerups_String (void)
{
	static char powerups_string[256];

	memset (powerups_string, 0, sizeof(powerups_string));

	if (cl.items & IT_QUAD)				strcat (powerups_string, "[QUAD] ");
	if (cl.items & IT_INVULNERABILITY)  strcat (powerups_string, "[PENT] ");
	if (cl.items & IT_INVISIBILITY)		strcat (powerups_string, "[RING] ");

	return powerups_string;
}

char *Time_String (void)
{
	static char level_time_string[256];
	int minutes = cl.time / 60;
	int seconds = cl.time - (60 * minutes);
	minutes &= 511;

	memset (level_time_string, 0, sizeof(level_time_string));

	sprintf (level_time_string, "%i:%02i", minutes, seconds);

	return level_time_string;
}


char *Expand_Talk_Macros (char *string)
{
	static char modified_string[256];
	int		readpos = 0;
	int		writepos = 0;
	char	*insert_point;
	char	letter;

	memset (modified_string, 0, sizeof(modified_string));

	// Byte by byte copy cmd_args into the buffer.

	for ( ; string[readpos] && writepos < 100; )
	{
		if (string[readpos] != '%')
		{
			modified_string[writepos] = string[readpos];
			readpos ++;
			writepos ++;
			continue; // Not a macro to replace or some invalid macro
		}

		// We found a macro
		readpos ++; // Skip the percent
		letter = string[readpos];
		readpos ++; // Skip writing the letter too
		insert_point = &modified_string[writepos];

			 if (letter == 'a')  writepos += sprintf(insert_point, "%i", cl.stats[STAT_ARMOR]);
		else if (letter == 'c')  writepos += sprintf(insert_point, "%i", cl.stats[STAT_CELLS]);
		else if (letter == 'd')  writepos += sprintf(insert_point, "%s", LOC_GetLocation(cl.death_location));
		else if (letter == 'h')  writepos += sprintf(insert_point, "%i", cl.stats[STAT_HEALTH]);
		else if (letter == 'l')  writepos += sprintf(insert_point, "%s", LOC_GetLocation(cl_entities[cl.viewentity].origin));
		else if (letter == 'p')	 writepos += sprintf(insert_point, "%s", Powerups_String (/*cl.items*/));
		else if (letter == 'r')  writepos += sprintf(insert_point, "%i", cl.stats[STAT_ROCKETS]);
		else if (letter == 't')  writepos += sprintf(insert_point, "%s", Time_String (/*cl.time*/));
		else if (letter == 'w')  writepos += sprintf(insert_point, "%s", Weapons_String (/*cl.items*/));
		else					 writepos += sprintf(insert_point, "(invalid macro '%c')", letter);
	}

	modified_string[writepos] = 0; // Null terminate the copy
	return modified_string;
}
//qb: talk macros and location end


/*
===================
Cmd_ForwardToServer

Sends the entire command line over to the server
===================
*/
void Cmd_ForwardToServer (void)
{
    if (cls.state != ca_connected)
    {
        Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
        return;
    }

    if (cls.demoplayback)
        return;		// not really connected

    MSG_WriteByte (&cls.message, clc_stringcmd);
    if (Q_strcasecmp(Cmd_Argv(0), "cmd") != 0)
    {
        SZ_Print (&cls.message, Cmd_Argv(0));
        SZ_Print (&cls.message, " ");
    }

    //qb: talk macro begin
    if (Cmd_Argc() > 1 && (Q_strcasecmp(Cmd_Argv(0), "say") == 0 || !Q_strcasecmp(Cmd_Argv(0), "say_team") == 0) )
	{
		char *new_text = Expand_Talk_Macros (Cmd_Args());  // Func also writes out the new message

		SZ_Print (&cls.message, new_text);
		return;
	}
	else //qb: talk macro end
    if (Cmd_Argc() > 1)
        SZ_Print (&cls.message, Cmd_Args());
    else
        SZ_Print (&cls.message, "\n");
}


/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/

int Cmd_CheckParm (char *parm)
{
    int i;

    if (!parm)
        Sys_Error ("Cmd_CheckParm: NULL");

    for (i = 1; i < Cmd_Argc (); i++)
        if (! Q_strcasecmp (parm, Cmd_Argv (i)))
            return i;

    return 0;
}
