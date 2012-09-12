#include "cmdlib.h"

char	token[MAXTOKEN];
qboolean	unget;
char	*script_p;
int		scriptline;

void StartTokenParsing (char *data)
{
	scriptline = 1;
	script_p = data;
	unget = false;
}

qboolean GetToken (qboolean crossline)
{
	char    *token_p, *temp;

	if (unget)                         // is a token already waiting?
		return true;

	// return something even if the caller ignores a return false
	token[0] = 0;

	//
	// skip space
	//
	for (;;)
	{
		if (*script_p == 0)
			return false; 
		else if (*script_p == '\n')
		{
			if (!crossline)
				return false;
			script_p++;
			scriptline++;
		}
		else if (*script_p <= ' ')
			script_p++;
		else if (script_p[0] == '/' && script_p[1] == '/')
		{
			while (*script_p && *script_p != '\n')
				script_p++;
		}
		else if (script_p[0] == '/' && script_p[1] == '*')
		{
			temp = script_p;
			for (;;)
			{
				if (*script_p == 0)
					break;
				else if (script_p[0] == '*' && script_p[1] == '/')
				{
					script_p += 2;
					break;
				}
				else if (*script_p == '\n')
				{
					if (!crossline)
					{
						script_p = temp;
						return false;
					}
					script_p++;
					scriptline++;
				}
				else
					script_p++;
			}
		}
		else
			break;
	}

	//
	// copy token
	//
	token_p = token;

	if (*script_p == '"')
	{
		script_p++;
		while ( *script_p != '"' )
		{
			if (!*script_p)
				Error ("EOF inside quoted token");
			*token_p++ = *script_p++;
			if (token_p > &token[MAXTOKEN-1])
				Error ("Token too large on line %i",scriptline);
		}
		script_p++;
	}
	else while ( *script_p > 32 )
	{
		*token_p++ = *script_p++;
		if (token_p > &token[MAXTOKEN-1])
			Error ("Token too large on line %i",scriptline);
	}

	*token_p = 0;

	return true;
}

void UngetToken (void)
{
	unget = true;
}

