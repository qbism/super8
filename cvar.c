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
// cvar.c -- dynamic variable tracking

#include "quakedef.h"

cvar_t	*cvar_vars;
char	*cvar_null_string = "";

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;

	for (var=cvar_vars ; var ; var=var->next)
		if (!Q_strcmp (var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float	Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return Q_atof (var->string);
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}


/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t		*cvar;
	int			len;

	len = Q_strlen(partial);

	if (!len)
		return NULL;

// check functions
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!Q_strncmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}


//qb: qrack command line begin
/*
============
Cvar_CompleteCountPossible
============
*/
int Cvar_CompleteCountPossible (char *partial)
{
	cvar_t	*cvar;
	int	len, c = 0;

	if (!(len = Q_strlen(partial)))
		return 0;

	// check partial match
	for (cvar = cvar_vars ; cvar ; cvar = cvar->next)
		if (!Q_strncasecmp(partial, cvar->name, len))
			c++;

	return c;
}

//qb: qrack command line end

/*
============
Cvar_Set
============
*/

cvar_t *Cvar_SetWithNoCallback (char *var_name, char *value)
{
	cvar_t	*var;
	qboolean changed;

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// there is an error in C code if this happens
		Con_DPrintf ("Cvar_Set: variable %s not found\n", var_name); // edited
		return 0;
	}

	changed = Q_strcmp(var->string, value);

	Q_free (var->string);	// free the old value string

	var->string = Q_calloc ("Cvar_SetWithNoCallback", Q_strlen(value)+1);
	Q_strcpy (var->string, value);
	var->value = Q_atof (var->string);
	if (var->server && changed)
	{
		if (sv.active)
		if (svs.maxclients > 1) // Manoel Kasimier
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);
	}
	return var;
}

void Cvar_Set (char *var_name, char *value)
{
	cvar_t	*var;

	var = Cvar_SetWithNoCallback(var_name, value);

	//Heffo - Cvar Callback Function
	if(var->Cvar_Changed)
		var->Cvar_Changed();
	//Heffo - Cvar Callback Function
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (char *var_name, float value)
{
	//char	val[32];
	//sprintf (val, "%f", value);
	//Cvar_Set (var_name, val);
	Cvar_Set (var_name, COM_NiceFloatString(value));
}


/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable (cvar_t *variable)
{
	char	*oldstr;

// first check to see if it has allready been defined
	if (Cvar_FindVar (variable->name))
	{
		Con_Printf ("Can't register variable %s, already defined\n", variable->name);
		return;
	}

// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

// copy the value off, because future sets will free it
	oldstr = variable->string;
	variable->string = Q_calloc ("Cvar_RegisterVariable", Q_strlen(variable->string)+1);
	Q_strcpy (variable->string, oldstr);
	variable->value = Q_atof (variable->string);

	//qb:  remember default
	variable->defaultstring = Q_calloc ("Cvar_RegisterVariable", Q_strlen(variable->string)+1);
	Q_strcpy (variable->defaultstring, variable->string);


// link the variable in
	variable->next = cvar_vars;
	cvar_vars = variable;
}


//Manoel Kasimier & Heffo - Cvar Callback Function - begin
void Cvar_RegisterVariableWithCallback (cvar_t *variable, void *function)
{
// first check to see if it has allready been defined
	if (Cvar_FindVar (variable->name))
	{
		Con_Printf ("Can't register variable %s, allready defined\n", variable->name);
		return;
	}

// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}
	Cvar_RegisterVariable (variable);
	variable->Cvar_Changed = function;
}
//Manoel Kasimier & Heffo - Cvar Callback Function - end
/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean	Cvar_Command (void)
{
	cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;

// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Current value is %s\n", v->string);
		Con_Printf ("Default value is %s\n", v->defaultstring); //qb: default
		Con_Printf ("Usage: %s\n",  v->helpstring);  //qb: help
		if (v->archive)
            Con_Printf ("Value gets saved in super8.cfg: YES\n\n");
        else
            Con_Printf ("Value gets saved in super8.cfg: NO\n\n");
		return true;
	}

	Cvar_Set (v->name, Cmd_Argv(1));
	return true;
}


/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
qboolean HaveSemicolon (char *s); // Manoel Kasimier - reduced config file
void Cvar_WriteVariables (FILE *f)
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
		if (var->archive)
		// Manoel Kasimier - reduced config file - begin
		{
			Cmd_TokenizeString(var->string);
			if (Cmd_Argc() == 1 && !HaveSemicolon(var->string))
				fprintf (f, "%s %s\n", var->name, var->string);
			else
		// Manoel Kasimier - reduced config file - end
			fprintf (f, "%s \"%s\"\n", var->name, var->string);
		} // Manoel Kasimier - reduced config file
}
