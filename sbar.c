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
// sbar.c -- status bar code

#include "quakedef.h"

#define STAT_MINUS		10	// num frame for '-' stats digit
qpic_t		*sb_nums[2][11];
qpic_t		*sb_colon, *sb_slash;
qpic_t		*sb_ibar;
qpic_t		*sb_sbar;
qpic_t		*sb_scorebar;

qpic_t      *sb_weapons[7][8];   // 0 is active, 1 is owned, 2-5 are flashes
qpic_t      *sb_ammo[4];
qpic_t		*sb_sigil[4];
qpic_t		*sb_armor[3];
qpic_t		*sb_items[32];

qpic_t	*sb_faces[7][2];		// 0 is gibbed, 1 is dead, 2-6 are alive
// 0 is static, 1 is temporary animation
qpic_t	*sb_face_invis;
qpic_t	*sb_face_quad;
qpic_t	*sb_face_invuln;
qpic_t	*sb_face_invis_invuln;

qboolean	sb_showscores;

int			sb_lines;			// scan lines to draw
extern float scr_2d_scale_h, scr_2d_scale_v;

qpic_t      *rsb_invbar[2];
qpic_t      *rsb_weapons[5];
qpic_t      *rsb_items[2];
qpic_t      *rsb_ammo[3];
qpic_t      *rsb_teambord;		// PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
qpic_t      *hsb_weapons[7][5];   // 0 is active, 1 is owned, 2-5 are flashes
//MED 01/04/97 added array to simplify weapon parsing
int         hipweapons[4] = {HIT_LASER_CANNON_BIT,HIT_MJOLNIR_BIT,4,HIT_PROXIMITY_GUN_BIT};
//MED 01/04/97 added hipnotic items array
qpic_t      *hsb_items[2];

cvar_t	sbar_scale	= {"sbar_scale","0.8", "sbar_scale[0.0 - 1.0] Status bar and menu scale.", true}; //qb:
// Manoel Kasimier - begin
cvar_t	sbar_show_scores	= {"sbar_show_scores","0", "sbar_show_scores[0/1] Toggles display of status bar.", false}; //qb: let pros set w/ custom cfg
cvar_t	sbar_show_ammolist	= {"sbar_show_ammolist","1", "[0/1] Toggles display of status bar ammo list.", false};
cvar_t	sbar_show_weaponlist= {"sbar_show_weaponlist","1", "[0/1] Toggles display of status bar weapon list.", false};
cvar_t	sbar_show_keys		= {"sbar_show_keys","1", "[0/1] Toggles display of status bar keys.", false};
cvar_t	sbar_show_runes		= {"sbar_show_runes","1", "[0/1] Toggles display of status bar runes.", false};
cvar_t	sbar_show_powerups	= {"sbar_show_powerups","1", "[0/1] Toggles display of status bar powerups.", false};
cvar_t	sbar_show_armor		= {"sbar_show_armor","1", "[0/1] Toggles display of status bar armor.", false};
cvar_t	sbar_show_health	= {"sbar_show_health","1", "[0/1] Toggles display of status bar health.", false};
cvar_t	sbar_show_ammo		= {"sbar_show_ammo","1", "[0/1] Toggles display of status bar ammo.", false};
cvar_t	sbar_show_bg		= {"sbar_show_bg","0", "[0/1] Toggles display of status bar background.", true};
cvar_t	sbar                = {"sbar","1", "sbar[0-4] Status bar mode. 0 is off, 1-3 are 'classic' modes, 4 is a different mode.", true};

cvar_t	crosshair_color		= {"crosshair_color","12", "crosshair_color[0-17] 0-15 are built-in colors, 16 is crosshair_custom1, and 17 is crosshair_custom2.", true};
cvar_t	crosshair_custom16	= {"crosshair_custom16","244", "crosshair_custom16[palette index] Custom color slot for crosshair.  Set crosshair_color to 16.", true};
cvar_t	crosshair_custom17	= {"crosshair_custom17","251", "crosshair_custom17[palette index] Custom color slot for crosshair.  Set crosshair_color to 17.", true};
// Manoel Kasimier - end

void Sbar_MiniDeathmatchOverlay (void);
void Sbar_DeathmatchOverlay (void);

/*
===============
Sbar_ShowScores

Tab key down
===============
*/
void Sbar_ShowScores (void)
{
    sb_showscores = true;
}

/*
===============
Sbar_DontShowScores

Tab key up
===============
*/
void Sbar_DontShowScores (void)
{
    sb_showscores = false;
}

/*
===============
Sbar_Changed
===============
*/
void Sbar_Changed (void)
{
}

void CheckSbarScale (void)  //qb: bound sbar_scale value
{
    float sbsmin = MIN_VID_WIDTH/(float)vid.width + 0.01;
    if (sbar_scale.value < sbsmin)
    {
        //Con_Printf("minvalue %f \n", sbsmin);
        Cvar_SetValue("sbar_scale", sbsmin);
    }
    if (sbar_scale.value > 1.0)
        Cvar_SetValue("sbar_scale", 1.0);
}

// mankrip - begin
void Sbar_SizeScreen (void)
{
    sb_lines = 0;
    if (sbar_show_bg.value)
        if (sbar.value > 0 && sbar.value < 4)
            sb_lines = sbar.value * 24.0 * scr_2d_scale_v;
}
// mankrip - end
/*
===============
Sbar_Init
===============
*/
void Sbar_Init (void)
{
    int		i;

    Cvar_RegisterVariableWithCallback(&sbar_scale, CheckSbarScale); //qb: added
    // mankrip - begin
    Cvar_RegisterVariable (&sbar_show_scores);
    Cvar_RegisterVariable (&sbar_show_ammolist);
    Cvar_RegisterVariable (&sbar_show_weaponlist);
    Cvar_RegisterVariable (&sbar_show_keys);
    Cvar_RegisterVariable (&sbar_show_runes);
    Cvar_RegisterVariable (&sbar_show_powerups);
    Cvar_RegisterVariable (&sbar_show_armor);
    Cvar_RegisterVariable (&sbar_show_health);
    Cvar_RegisterVariable (&sbar_show_ammo);
    Cvar_RegisterVariable (&sbar_show_bg);
    Cvar_RegisterVariable (&sbar);

    Cvar_RegisterVariable (&crosshair_color);
    Cvar_RegisterVariable (&crosshair_custom16); //qb: custom color
    Cvar_RegisterVariable (&crosshair_custom17); //qb: custom color

    Cvar_RegisterVariableWithCallback (&sbar, Sbar_SizeScreen);
    // mankrip - end

    for (i=0 ; i<10 ; i++)
    {
        sb_nums[0][i] = Draw_PicFromWad (va("num_%i",i));
        sb_nums[1][i] = Draw_PicFromWad (va("anum_%i",i));
    }

    sb_nums[0][10] = Draw_PicFromWad ("num_minus");
    sb_nums[1][10] = Draw_PicFromWad ("anum_minus");

    sb_colon = Draw_PicFromWad ("num_colon");
    sb_slash = Draw_PicFromWad ("num_slash");

    sb_weapons[0][0] = Draw_PicFromWad ("inv_shotgun");
    sb_weapons[0][1] = Draw_PicFromWad ("inv_sshotgun");
    sb_weapons[0][2] = Draw_PicFromWad ("inv_nailgun");
    sb_weapons[0][3] = Draw_PicFromWad ("inv_snailgun");
    sb_weapons[0][4] = Draw_PicFromWad ("inv_rlaunch");
    sb_weapons[0][5] = Draw_PicFromWad ("inv_srlaunch");
    sb_weapons[0][6] = Draw_PicFromWad ("inv_lightng");

    sb_weapons[1][0] = Draw_PicFromWad ("inv2_shotgun");
    sb_weapons[1][1] = Draw_PicFromWad ("inv2_sshotgun");
    sb_weapons[1][2] = Draw_PicFromWad ("inv2_nailgun");
    sb_weapons[1][3] = Draw_PicFromWad ("inv2_snailgun");
    sb_weapons[1][4] = Draw_PicFromWad ("inv2_rlaunch");
    sb_weapons[1][5] = Draw_PicFromWad ("inv2_srlaunch");
    sb_weapons[1][6] = Draw_PicFromWad ("inv2_lightng");

    for (i=0 ; i<5 ; i++)
    {
        sb_weapons[2+i][0] = Draw_PicFromWad (va("inva%i_shotgun",i+1));
        sb_weapons[2+i][1] = Draw_PicFromWad (va("inva%i_sshotgun",i+1));
        sb_weapons[2+i][2] = Draw_PicFromWad (va("inva%i_nailgun",i+1));
        sb_weapons[2+i][3] = Draw_PicFromWad (va("inva%i_snailgun",i+1));
        sb_weapons[2+i][4] = Draw_PicFromWad (va("inva%i_rlaunch",i+1));
        sb_weapons[2+i][5] = Draw_PicFromWad (va("inva%i_srlaunch",i+1));
        sb_weapons[2+i][6] = Draw_PicFromWad (va("inva%i_lightng",i+1));
    }

    sb_ammo[0] = Draw_PicFromWad ("sb_shells");
    sb_ammo[1] = Draw_PicFromWad ("sb_nails");
    sb_ammo[2] = Draw_PicFromWad ("sb_rocket");
    sb_ammo[3] = Draw_PicFromWad ("sb_cells");

    sb_armor[0] = Draw_PicFromWad ("sb_armor1");
    sb_armor[1] = Draw_PicFromWad ("sb_armor2");
    sb_armor[2] = Draw_PicFromWad ("sb_armor3");

    sb_items[0] = Draw_PicFromWad ("sb_key1");
    sb_items[1] = Draw_PicFromWad ("sb_key2");
    sb_items[2] = Draw_PicFromWad ("sb_invis");
    sb_items[3] = Draw_PicFromWad ("sb_invuln");
    sb_items[4] = Draw_PicFromWad ("sb_suit");
    sb_items[5] = Draw_PicFromWad ("sb_quad");

    sb_sigil[0] = Draw_PicFromWad ("sb_sigil1");
    sb_sigil[1] = Draw_PicFromWad ("sb_sigil2");
    sb_sigil[2] = Draw_PicFromWad ("sb_sigil3");
    sb_sigil[3] = Draw_PicFromWad ("sb_sigil4");

    sb_faces[4][0] = Draw_PicFromWad ("face1");
    sb_faces[4][1] = Draw_PicFromWad ("face_p1");
    sb_faces[3][0] = Draw_PicFromWad ("face2");
    sb_faces[3][1] = Draw_PicFromWad ("face_p2");
    sb_faces[2][0] = Draw_PicFromWad ("face3");
    sb_faces[2][1] = Draw_PicFromWad ("face_p3");
    sb_faces[1][0] = Draw_PicFromWad ("face4");
    sb_faces[1][1] = Draw_PicFromWad ("face_p4");
    sb_faces[0][0] = Draw_PicFromWad ("face5");
    sb_faces[0][1] = Draw_PicFromWad ("face_p5");

    sb_face_invis = Draw_PicFromWad ("face_invis");
    sb_face_invuln = Draw_PicFromWad ("face_invul2");
    sb_face_invis_invuln = Draw_PicFromWad ("face_inv2");
    sb_face_quad = Draw_PicFromWad ("face_quad");

    Cmd_AddCommand ("+showscores", Sbar_ShowScores);
    Cmd_AddCommand ("-showscores", Sbar_DontShowScores);

    sb_sbar = Draw_PicFromWad ("sbar");
    sb_ibar = Draw_PicFromWad ("ibar");
    sb_scorebar = Draw_PicFromWad ("scorebar");

//MED 01/04/97 added new hipnotic weapons
    if (hipnotic)
    {
        hsb_weapons[0][0] = Draw_PicFromWad ("inv_laser");
        hsb_weapons[0][1] = Draw_PicFromWad ("inv_mjolnir");
        hsb_weapons[0][2] = Draw_PicFromWad ("inv_gren_prox");
        hsb_weapons[0][3] = Draw_PicFromWad ("inv_prox_gren");
        hsb_weapons[0][4] = Draw_PicFromWad ("inv_prox");

        hsb_weapons[1][0] = Draw_PicFromWad ("inv2_laser");
        hsb_weapons[1][1] = Draw_PicFromWad ("inv2_mjolnir");
        hsb_weapons[1][2] = Draw_PicFromWad ("inv2_gren_prox");
        hsb_weapons[1][3] = Draw_PicFromWad ("inv2_prox_gren");
        hsb_weapons[1][4] = Draw_PicFromWad ("inv2_prox");

        for (i=0 ; i<5 ; i++)
        {
            hsb_weapons[2+i][0] = Draw_PicFromWad (va("inva%i_laser",i+1));
            hsb_weapons[2+i][1] = Draw_PicFromWad (va("inva%i_mjolnir",i+1));
            hsb_weapons[2+i][2] = Draw_PicFromWad (va("inva%i_gren_prox",i+1));
            hsb_weapons[2+i][3] = Draw_PicFromWad (va("inva%i_prox_gren",i+1));
            hsb_weapons[2+i][4] = Draw_PicFromWad (va("inva%i_prox",i+1));
        }

        hsb_items[0] = Draw_PicFromWad ("sb_wsuit");
        hsb_items[1] = Draw_PicFromWad ("sb_eshld");
    }

    if (rogue)
    {
        rsb_invbar[0] = Draw_PicFromWad ("r_invbar1");
        rsb_invbar[1] = Draw_PicFromWad ("r_invbar2");

        rsb_weapons[0] = Draw_PicFromWad ("r_lava");
        rsb_weapons[1] = Draw_PicFromWad ("r_superlava");
        rsb_weapons[2] = Draw_PicFromWad ("r_gren");
        rsb_weapons[3] = Draw_PicFromWad ("r_multirock");
        rsb_weapons[4] = Draw_PicFromWad ("r_plasma");

        rsb_items[0] = Draw_PicFromWad ("r_shield1");
        rsb_items[1] = Draw_PicFromWad ("r_agrav1");

// PGM 01/19/97 - team color border
        rsb_teambord = Draw_PicFromWad ("r_teambord");
// PGM 01/19/97 - team color border

        rsb_ammo[0] = Draw_PicFromWad ("r_ammolava");
        rsb_ammo[1] = Draw_PicFromWad ("r_ammomulti");
        rsb_ammo[2] = Draw_PicFromWad ("r_ammoplasma");
    }
    Draw_CachePic ("gfx/complete.lmp"); // mankrip
    Draw_CachePic ("gfx/inter.lmp"); // mankrip
}


//=============================================================================

// drawing routines are relative to the status bar location

/*
=============
Sbar_DrawTransPic
=============
*/
void Sbar_DrawTransPic (int x, int y, qpic_t *pic)
{
    M_DrawTransPic (x, y + (200-SBAR_HEIGHT), pic, true); // mankrip
}

void Sbar_DrawTransPicMirror (int x, int y, qpic_t *pic)
{
    M_DrawTransPicMirror (x, y + (200-SBAR_HEIGHT), pic, true); // mankrip
}

/*
================
Sbar_DrawCharacter

Draws one solid graphics character
================
*/
void Sbar_DrawCharacter (int x, int y, int num)
{
    M_DrawCharacter (x + 4, y + 200 - SBAR_HEIGHT, num, true);
}

/*
================
Sbar_DrawString
================
*/
void Sbar_DrawString (int x, int y, char *str)
{
    Draw_String (x, y + 200 - SBAR_HEIGHT, str, true); // mankrip - edited
}


/*
=============
Sbar_itoa
=============
*/
int Sbar_itoa (int num, char *buf)
{
    char	*str;
    int		pow10;
    int		dig;

    str = buf;

    if (num < 0)
    {
        *str++ = '-';
        num = -num;
    }

    for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
        ;

    do
    {
        pow10 /= 10;
        dig = num/pow10;
        *str++ = '0'+dig;
        num -= dig*pow10;
    }
    while (pow10 != 1);

    *str = 0;

    return str-buf;
}


/*
=============
Sbar_DrawNum
=============
*/
void Sbar_DrawNum (int x, int y, int num, int digits, int color)
{
    char			str[12];
    char			*ptr;
    int				l, frame;

    l = Sbar_itoa (num, str);
    ptr = str;
    if (l > digits)
        ptr += (l-digits);
    if (l < digits)
        x += (digits-l)*24;

    while (*ptr)
    {
        if (*ptr == '-')
            frame = STAT_MINUS;
        else
            frame = *ptr -'0';

        Sbar_DrawTransPic (x,y,sb_nums[color][frame]);
        x += 24;
        ptr++;
    }
}

//=============================================================================

int		fragsort[MAX_SCOREBOARD];

char	scoreboardtext[MAX_SCOREBOARD][20];
int		scoreboardtop[MAX_SCOREBOARD];
int		scoreboardbottom[MAX_SCOREBOARD];
int		scoreboardcount[MAX_SCOREBOARD];
int		scoreboardlines;

/*
===============
Sbar_SortFrags
===============
*/
void Sbar_SortFrags (void)
{
    int		i, j, k;

// sort by frags
    scoreboardlines = 0;
    for (i=0 ; i<cl.maxclients ; i++)
    {
        if (cl.scores[i].name[0])
        {
            fragsort[scoreboardlines] = i;
            scoreboardlines++;
        }
    }

    for (i=0 ; i<scoreboardlines ; i++)
        for (j=0 ; j<scoreboardlines-1-i ; j++)
            if (cl.scores[fragsort[j]].frags < cl.scores[fragsort[j+1]].frags)
            {
                k = fragsort[j];
                fragsort[j] = fragsort[j+1];
                fragsort[j+1] = k;
            }
}

int	Sbar_ColorForMap (int m)
{
    return m < 128 ? m + 8 : m + 8;
}

/*
===============
Sbar_UpdateScoreboard
===============
*/
void Sbar_UpdateScoreboard (void)
{
    int		i, k;
    int		top, bottom;
    scoreboard_t	*s;

    Sbar_SortFrags ();

// draw the text
    memset (scoreboardtext, 0, sizeof(scoreboardtext));

    for (i=0 ; i<scoreboardlines; i++)
    {
        k = fragsort[i];
        s = &cl.scores[k];
        sprintf (&scoreboardtext[i][1], "%3i %s", s->frags, s->name);

        top = s->colors & 0xf0;
        bottom = (s->colors & 15) <<4;
        scoreboardtop[i] = Sbar_ColorForMap (top);
        scoreboardbottom[i] = Sbar_ColorForMap (bottom);
    }
}



/*
===============
Sbar_SoloScoreboard
===============
*/
char *timetos (int mytime);
void Sbar_SoloScoreboard (int x, int y, int vertical) // default 0, -48, 0
{
// mankrip - added and edited a lot of things in this function, I won't even bother myself commenting each one...
    // x has no effect if vertical is zero
    char	str[80];
//	int		minutes, seconds, tens, units; // mankrip - removed
    int		l;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_scores.value)
            return;

    sprintf (str,"Monsters:%3i /%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
    if (vertical)
        Sbar_DrawString (x, y - 8, str);
    else
        Sbar_DrawString (8, y + 4, str);

    sprintf (str,"Secrets :%3i /%3i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
    if (vertical)
        Sbar_DrawString (x, y, str);
    else
        Sbar_DrawString (8, y + 12, str);

// time
    /*// mankrip
    	minutes = cl.time / 60;
    	seconds = cl.time - 60*minutes;
    	tens = seconds / 10;
    	units = seconds - 10*tens;
    	sprintf (str,"Time :%3i:%i%i", minutes, tens, units);
    //*/// mankrip
    sprintf (str,"Time: %s", timetos((int)cl.time)); // mankrip
    if (vertical)
        Sbar_DrawString (x, y - 16, str);
    else
        Sbar_DrawString (184, y + 4, str);

// draw level name
    l = Q_strlen (cl.levelname);
    if (vertical)
        Sbar_DrawString (x, y - 24, cl.levelname);
    else
        Sbar_DrawString (232 - l*4, y + 12, cl.levelname);
}

//=============================================================================

//=============================================================================
//	mankrip - begin
//=============================================================================
/*
===============
Sbar_DrawAmmoList
===============
*/
void Sbar_DrawAmmoList (int x, int y, int vertical) // default 0, -24, 0
{
    // existe uma diferen�a de 4 pixels pra direita ao se desenhar os n�meros...
    int		i;
    char	num[12]; // mankrip - high values in the status bar - edited
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_ammolist.value)
            return;

    for (i=0 ; i<4 ; i++)
    {
        sprintf (num, "%3i",cl.stats[STAT_SHELLS+i] );
        if (vertical == 0)
        {
            if (num[0] != ' ')
                Sbar_DrawCharacter ( (6*i+1)*8 - 2 + x, y, 18 + num[0] - '0');
            if (num[1] != ' ')
                Sbar_DrawCharacter ( (6*i+2)*8 - 2 + x, y, 18 + num[1] - '0');
            if (num[2] != ' ')
                Sbar_DrawCharacter ( (6*i+3)*8 - 2 + x, y, 18 + num[2] - '0');
        }
        else
        {
            if (num[0] != ' ')
                Sbar_DrawCharacter (x	  , y - i*8, 18 + num[0] - '0');
            if (num[1] != ' ')
                Sbar_DrawCharacter (x +  8, y - i*8, 18 + num[1] - '0');
            if (num[2] != ' ')
                Sbar_DrawCharacter (x + 16, y - i*8, 18 + num[2] - '0');
        }
        /*
        (6		dist�ncia entre os valores, incluindo os n�meros (ex: 100---200---)
        *i		�ndice do valor (ex: shells=0, nails=1)
        +#)		desloca o caracter na quantidade de espa�os indicada pelo n�mero
        *8		transforma a posi��o horizontal em quantidade de caracteres
        - 2 + x
        //*/
    }
}

/*
===============
Sbar_DrawWeaponList
===============
*/
void Sbar_DrawWeaponList (int x, int y, int vertical) // default 0, -16, 0
{
    int		i;
    float	time;
    int		flashon;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_weaponlist.value)
            return;

// weapons
    for (i=0 ; i<7 ; i++)
        if (cl.items & (IT_SHOTGUN<<i) )
        {
            time = cl.item_gettime[i];
            flashon = (int)((cl.time - time)*10);
            if (flashon >= 10)
            {
                if ( cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN<<i)  )
                    flashon = 1;
                else
                    flashon = 0;
            }
            else
                flashon = (flashon%5) + 2;
            if (vertical == 0)
                Sbar_DrawTransPic (i*24 + x, y, sb_weapons[flashon][i]);
            else if (i == 6)
            {
                if (hipnotic)
                    Sbar_DrawTransPic (x-8, y - i*16, sb_weapons[flashon][i]);
                else
                    Sbar_DrawTransPic (x-24, y - i*16, sb_weapons[flashon][i]);
            }
            else
                Sbar_DrawTransPic (x, y - i*16, sb_weapons[flashon][i]);
        }

// MED 01/04/97
// hipnotic weapons
    if (hipnotic)
    {
        int grenadeflashing=0;
        for (i=0 ; i<4 ; i++)
            if (cl.items & (1<<hipweapons[i]) )
            {
                time = cl.item_gettime[hipweapons[i]];
                flashon = (int)((cl.time - time)*10);
                if (flashon >= 10)
                {
                    if ( cl.stats[STAT_ACTIVEWEAPON] == (1<<hipweapons[i])  )
                        flashon = 1;
                    else
                        flashon = 0;
                }
                else
                    flashon = (flashon%5) + 2;

                // check grenade launcher
                if (i==2)
                {
                    if (cl.items & HIT_PROXIMITY_GUN)
                        if (flashon)
                        {
                            grenadeflashing = 1;
                            {
                                if (vertical == 0)
                                    Sbar_DrawTransPic (96 + x, y, hsb_weapons[flashon][2]);
                                else
                                    Sbar_DrawTransPic (x, y - 64, hsb_weapons[flashon][2]);
                            }
                        }
                }
                else if (i==3)
                {
                    if (cl.items & (IT_SHOTGUN<<4))
                    {
                        if (flashon && !grenadeflashing)
                        {
                            if (vertical == 0)
                                Sbar_DrawTransPic (96 + x, y, hsb_weapons[flashon][3]);
                            else
                                Sbar_DrawTransPic (x, y - 64, hsb_weapons[flashon][3]);
                        }
                        else if (!grenadeflashing)
                        {
                            if (vertical == 0)
                                Sbar_DrawTransPic (96 + x, y, hsb_weapons[0][3]);
                            else
                                Sbar_DrawTransPic (x, y - 64, hsb_weapons[0][3]);
                        }
                    }
                    else
                    {
                        if (vertical == 0)
                            Sbar_DrawTransPic (96 + x, y, hsb_weapons[flashon][4]);
                        else
                            Sbar_DrawTransPic (x, y - 64, hsb_weapons[flashon][4]);
                    }
                }
                else
                {
                    if (vertical == 0)
                        Sbar_DrawTransPic (176 + (i*24) + x, y, hsb_weapons[flashon][i]);
                    else
                        Sbar_DrawTransPic (x, y - 112 - (i*16), hsb_weapons[flashon][i]);
                }
            }
    }
    else if (rogue) // check for powered up weapon.
        if ( cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
            for (i=0; i<5; i++)
                if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i))
                {
                    if (vertical == 0)
                        Sbar_DrawTransPic ((i+2)*24 + x, y, rsb_weapons[i]);
                    else if (i == 4)
                        Sbar_DrawTransPic (x-24, y - (i+2)*16, rsb_weapons[i]);
                    else
                        Sbar_DrawTransPic (x, y - (i+2)*16, rsb_weapons[i]);
                }
}

/*
===============
Sbar_DrawKeys
===============
*/
void Sbar_DrawKeys (int x, int y) // default 192, -16
{
    int		i;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_keys.value)
            return;
    if (!hipnotic)
        for (i=0 ; i<2 ; i++)
            if (cl.items & (1<<(17+i)))
                Sbar_DrawTransPic (x + i*16, y, sb_items[i]);
}

/*
===============
Sbar_DrawPowerUps
===============
*/
void Sbar_DrawPowerUps (int x, int y) // default 224, -16
{
    int		i;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_powerups.value)
            return;
    for (i=0 ; i<4 ; i++)
        if (cl.items & (1<<(19+i)))
            Sbar_DrawTransPic (x + i*16, y, sb_items[i+2]);
}

/*
===============
Sbar_DrawPowerups_Rogue
===============
*/
void Sbar_DrawPowerups_Rogue (int x, int y) // default 288, -16
{
    int		i;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_powerups.value)
            return;
    if (rogue)
        for (i=0 ; i<2 ; i++)
            if (cl.items & (1<<(29+i)))
                Sbar_DrawTransPic (x + i*16, y, rsb_items[i]);
}

/*
===============
Sbar_DrawPowerups_Hipnotic
===============
*/
void Sbar_DrawPowerups_Hipnotic (int x, int y) // default 288, -16
{
    int		i;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_powerups.value)
            return;
    if (hipnotic)
        for (i=0 ; i<2 ; i++)
            if (cl.items & (1<<(24+i)))
                Sbar_DrawTransPic (x + i*16, y, hsb_items[i]);
}

/*
===============
Sbar_DrawRunes
===============
*/
void Sbar_DrawRunes (int x, int y) // default 288, -16
{
    int		i;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_runes.value)
            return;
    if (!rogue && !hipnotic)
        for (i=0 ; i<4 ; i++)
            if (cl.items & (1<<(28+i)))
                Sbar_DrawTransPic (x + i*8, y, sb_sigil[i]);
}
//=============================================================================
//	mankrip - end
//=============================================================================

/*
===============
Sbar_DrawFrags
===============
*/
void Sbar_DrawFrags (void)
{
    int				i, k, l;
    int				top, bottom;
    int				x, y, f;
    int				xofs;
    char			num[12];
    scoreboard_t	*s;

    if (cl.maxclients == 1) // mankrip
        return; // mankrip

    Sbar_SortFrags ();

// draw the text
    l = scoreboardlines <= 4 ? scoreboardlines : 4;

    x = 23;
    xofs = 202;
    y = 200 - SBAR_HEIGHT  + 23; // mankrip - edited

    for (i=0 ; i<l ; i++)
    {
        k = fragsort[i];
        s = &cl.scores[k];
        if (!s->name[0])
            continue;

        // draw background
        top = s->colors & 0xf0;
        bottom = (s->colors & 15)<<4;
        top = Sbar_ColorForMap (top);
        bottom = Sbar_ColorForMap (bottom);

        //qb: fixme - needs its own special function for scalable hud...
       // M_Draw_Fill (xofs - (xofs *scr_2d_scale_h)/4 + 220, y - (y * scr_2d_scale_v)/2 + 228, 28, 4, top); //qb: adjust for scaling - mankrip - edited
        //M_Draw_Fill (xofs - (xofs *scr_2d_scale_h)/4 + 220, y - (y * scr_2d_scale_v)/2 + 232, 28, 3, bottom); //qb: adjust for scaling -  mankrip - edited

        // draw number
        f = s->frags;
        sprintf (num, "%3i",f);

        Sbar_DrawCharacter ( (x+1)*8 , -24, num[0]);
        Sbar_DrawCharacter ( (x+2)*8 , -24, num[1]);
        Sbar_DrawCharacter ( (x+3)*8 , -24, num[2]);

        if (k == cl.viewentity - 1)
        {
            Sbar_DrawCharacter (x*8+2, -24, 16);
            Sbar_DrawCharacter ( (x+4)*8-4, -24, 17);
        }
        x+=4;
    }
}

//=============================================================================

//=============================================================================
//	mankrip - begin
//=============================================================================
/*
===============
Sbar_DrawArmor
===============
*/
void Sbar_DrawArmor (int x, int x2, int y) // default 0, 24, 0
{
    int i = 0;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_armor.value)
            return;
    if (cl.items & IT_INVULNERABILITY)
    {
        Sbar_DrawTransPic (x, y, draw_disc);
        Sbar_DrawNum (x2, y, 666, 3, 1);
    }
    else
    {
        if (rogue)
        {
            if (cl.items & RIT_ARMOR3)
                Sbar_DrawTransPic (x, y, sb_armor[2]);
            else if (cl.items & RIT_ARMOR2)
                Sbar_DrawTransPic (x, y, sb_armor[1]);
            else if (cl.items & RIT_ARMOR1)
                Sbar_DrawTransPic (x, y, sb_armor[0]);
            else
                i = 1;
        }
        else
        {
            if (cl.items & IT_ARMOR3)
                Sbar_DrawTransPic (x, y, sb_armor[2]);
            else if (cl.items & IT_ARMOR2)
                Sbar_DrawTransPic (x, y, sb_armor[1]);
            else if (cl.items & IT_ARMOR1)
                Sbar_DrawTransPic (x, y, sb_armor[0]);
            else
                i = 1;
        }
        if (!i || sbar.value < 4 || cl.stats[STAT_ARMOR])
            Sbar_DrawNum (x2, y, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
    }
}
//=============================================================================
//	mankrip - end
//=============================================================================

/*
===============
Sbar_DrawFace
===============
*/
void Sbar_DrawFace (int x, int y) // default 112, 0
{
    int		f, anim;

// PGM 01/19/97 - team color drawing
// PGM 03/02/97 - fixed so color swatch only appears in CTF modes
    if (rogue && (cl.maxclients != 1) && (teamplay.value>3) && (teamplay.value<7))
    {
        int				top, bottom;
        int				xofs;
        char			num[12];
        scoreboard_t	*s;

        s = &cl.scores[cl.viewentity - 1];
        // draw background
        top = s->colors & 0xf0;
        bottom = (s->colors & 15)<<4;
        top = Sbar_ColorForMap (top);
        bottom = Sbar_ColorForMap (bottom);

        xofs = x + 2; // 113 mankrip - edited

        Sbar_DrawTransPic (x, 0, rsb_teambord); // 112 mankrip - edited
        M_Draw_Fill (xofs, 200 - SBAR_HEIGHT +  3, 20, 9, top   ); // mankrip - edited
        M_Draw_Fill (xofs, 200 - SBAR_HEIGHT + 12, 20, 9, bottom); // mankrip - edited

        // draw number
        f = s->frags;
        sprintf (num, "%3i",f);

        if (top==8)
        {
            if (num[0] != ' ')
                Sbar_DrawCharacter(x - 3, 3, 18 + num[0] - '0'); // 109 mankrip - edited
            if (num[1] != ' ')
                Sbar_DrawCharacter(x + 4, 3, 18 + num[1] - '0'); // 116 mankrip - edited
            if (num[2] != ' ')
                Sbar_DrawCharacter(x + 11, 3, 18 + num[2] - '0'); // 123 mankrip - edited
        }
        else
        {
            Sbar_DrawCharacter ( x - 3, 3, num[0]); // 109 mankrip - edited
            Sbar_DrawCharacter ( x + 4, 3, num[1]); // 116 mankrip - edited
            Sbar_DrawCharacter ( x + 11, 3, num[2]); // 123 mankrip - edited
        }
        return;
    }
// PGM 01/19/97 - team color drawing

    // mankrip - begin
    if (cl.stats[STAT_HEALTH] <= 0)
    {
        Sbar_DrawTransPic (x, y, sb_faces[0][0]);
        return;
    }
    // mankrip - end

    if ( (cl.items & (IT_INVISIBILITY | IT_INVULNERABILITY)) ==
            (IT_INVISIBILITY | IT_INVULNERABILITY) )
    {
        Sbar_DrawTransPic (x, y, sb_face_invis_invuln); // mankrip - edited
        return;
    }
    if (cl.items & IT_QUAD)
    {
        Sbar_DrawTransPic (x, y, sb_face_quad ); // mankrip - edited
        return;
    }
    if (cl.items & IT_INVISIBILITY)
    {
        Sbar_DrawTransPic (x, y, sb_face_invis ); // mankrip - edited
        return;
    }
    if (cl.items & IT_INVULNERABILITY)
    {
        Sbar_DrawTransPic (x, y, sb_face_invuln); // mankrip - edited
        return;
    }

    if (cl.stats[STAT_HEALTH] >= 100)
        f = 4;
    else
        f = cl.stats[STAT_HEALTH] / 20;

    if (cl.time <= cl.faceanimtime)
        anim = 1;
    else
        anim = 0;
    Sbar_DrawTransPic (x, y, sb_faces[f][anim]); // mankrip - edited
}

//=============================================================================
//	mankrip - begin
//=============================================================================
/*
===============
Sbar_DrawHealth
===============
*/
void Sbar_DrawHealth (int x, int x2, int y) // default 112, 136, 0
{
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_health.value)
            return;
    // face
    Sbar_DrawFace (x, y);
    // health
    Sbar_DrawNum (x2, y, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);
}

/*
===============
Sbar_DrawHipKeys
===============
*/
void Sbar_DrawHipKeys (int x, int y) // default 209, 3
{
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_keys.value)
            return;
    if (hipnotic) // keys (hipnotic only)
    {
        if (cl.items & IT_KEY1)
            Sbar_DrawTransPic (x, y, sb_items[0]);
        if (cl.items & IT_KEY2)
            Sbar_DrawTransPic (x, y + 9, sb_items[1]);
    }
}

/*
===============
Sbar_DrawAmmo
===============
*/
void Sbar_DrawAmmo (int x, int x2, int y) // default 224, 248, 0
{
    int i = 0;
    if (sbar.value > 3)
        if (!sb_showscores && !sbar_show_ammo.value)
            return;
    if (rogue)
    {
        if (cl.items & RIT_SHELLS)
            Sbar_DrawTransPic (x, y, sb_ammo[0]);
        else if (cl.items & RIT_NAILS)
            Sbar_DrawTransPic (x, y, sb_ammo[1]);
        else if (cl.items & RIT_ROCKETS)
            Sbar_DrawTransPic (x, y, sb_ammo[2]);
        else if (cl.items & RIT_CELLS)
            Sbar_DrawTransPic (x, y, sb_ammo[3]);
        else if (cl.items & RIT_LAVA_NAILS)
            Sbar_DrawTransPic (x, y, rsb_ammo[0]);
        else if (cl.items & RIT_PLASMA_AMMO)
            Sbar_DrawTransPic (x, y, rsb_ammo[1]);
        else if (cl.items & RIT_MULTI_ROCKETS)
            Sbar_DrawTransPic (x, y, rsb_ammo[2]);
        else
            i = 1;
    }
    else
    {
        if (cl.items & IT_SHELLS)
            Sbar_DrawTransPic (x, y, sb_ammo[0]);
        else if (cl.items & IT_NAILS)
            Sbar_DrawTransPic (x, y, sb_ammo[1]);
        else if (cl.items & IT_ROCKETS)
            Sbar_DrawTransPic (x, y, sb_ammo[2]);
        else if (cl.items & IT_CELLS)
            Sbar_DrawTransPic (x, y, sb_ammo[3]);
        else
            i = 1;
    }
    if (!i || sbar.value < 4 || cl.stats[STAT_AMMO])
        Sbar_DrawNum (x2, y, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
}


// Manoel Kasimier - crosshair - begin
// crosshair alpha values: 0=transparent, 1=opaque, 3=33% transparent, 6=66% transparent
byte	crosshair_tex[5][11][11] =
{
    {
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,0,6,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0},
        {0,0,3,6,1,1,1,6,3,0,0},
        {0,0,0,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,6,0,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
    },

    {
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,0,6,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,3,6,1,0,0,0,1,6,3,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,6,0,0,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
    },

    {
        {0,0,0,3,6,1,6,3,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {3,0,0,0,0,0,0,0,0,0,3},
        {6,0,0,0,0,0,0,0,0,0,6},
        {1,0,0,0,0,6,0,0,0,0,1},
        {6,0,0,0,0,0,0,0,0,0,6},
        {3,0,0,0,0,0,0,0,0,0,3},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,3,6,1,6,3,0,0,0},
    },

    {
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,1,6,3,0,0,0,3,6,1,0},
        {0,6,0,0,0,0,0,0,0,6,0},
        {0,3,0,0,0,0,0,0,0,3,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,3,0,0,0,0,0,0,0,3,0},
        {0,6,0,0,0,0,0,0,0,6,0},
        {0,1,6,3,0,0,0,3,6,1,0},
        {0,0,0,0,0,0,0,0,0,0,0},
    },

    {
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
    },
};
void Crosshair_Draw (int x, int y, int color)
{
    int		pixel_value, pixel_color,
            countx, county, xdest, ydest,
            xmult=vid.width/MIN_VID_WIDTH,
            ymult=xmult; //qb;
    byte	*pdest, num=crosshair.value-1;
    pdest = vid.buffer + y*vid.rowbytes + x;

    for (county=0 ; county<11 ; county++, pdest+=vid.rowbytes*ymult)
        for (countx=0 ; countx<11 ; countx++)
            if ((pixel_value = crosshair_tex[num][county][countx]))
            {
                if (pixel_value == 6)
                    pixel_color = alphamap[(int)(pdest[countx*xmult] + color*256)];
                else if (pixel_value == 3)
                    pixel_color = alphamap[(int)(color + pdest[countx*xmult]*256)];
                else
                    pixel_color = color;

                for (xdest=0 ; xdest < xmult ; xdest++)
                    for (ydest=0 ; ydest < ymult ; ydest++)
                        pdest[countx*xmult+xdest + vid.rowbytes*ydest] = pixel_color;
            }
}
void Crosshair_Start (int x, int y)
{
    byte color = (byte)crosshair_color.value;
    if (!crosshair.value || color > 17)
        return;
    // custom colors
    if (color == 16) //qb: set by cvar
        Crosshair_Draw (x, y, crosshair_custom16.value);
    else if (color == 17) //qb: set by cvar
        Crosshair_Draw (x, y, crosshair_custom17.value);
    // palette colors
    else if (color > 8 && color < 15)
        Crosshair_Draw (x, y, (color-1)*16);
    else if (color)
        Crosshair_Draw (x, y, color*16-1);
    else //if (!color) // black
        Crosshair_Draw (x, y, 0);
}

//=============================================================================
//	mankrip - end
//=============================================================================


/*
===============
Sbar_Draw
===============
*/
void Sbar_Draw (void)
{
    if (scr_con_current == vid.height)
        return;		// console is full screen

    // mankrip - svc_letterbox - start
    if (cl.letterbox)
    {
        if (cl.letterbox == 1) // hack to ensure the whole screen will be black
            Draw_Fill(0, vid.height/2, vid.width, 1, 0);
        return;
    }
    // mankrip - svc_letterbox - end

    // Manoel Kasimier - crosshair - start
    if (crosshair.value)
    {
        if (crosshair.value < 0 || crosshair.value > 5)
            crosshair.value = 0;
        Crosshair_Start(cl_crossx.value + scr_vrect.x + scr_vrect.width/2 - 6*(vid.width/MIN_VID_WIDTH),
                        cl_crossy.value + scr_vrect.y + scr_vrect.height/2 - 6*(vid.width/MIN_VID_WIDTH)); // Manoel Kasimier - crosshair
    }
    // Manoel Kasimier - crosshair - end

    scr_copyeverything = 1;

// mankrip - begin

    Sbar_SizeScreen ();

    if ( (sb_showscores || sbar.value > 0) && sbar.value < 4)
    {
        if (sbar_show_bg.value)
            Draw_TileClear (scr_vrect.x, vid.height - sb_lines, scr_vrect.width, sb_lines);
#if 0
        else
        {
            Draw_Fill (0, vid.height - sb_lines - screen_bottom, scr_vrect.x, sb_lines, 0); // left
            Draw_Fill (scr_vrect.x+scr_vrect.width, vid.height - sb_lines - screen_bottom, vid.width-(scr_vrect.x+scr_vrect.width), sb_lines, 0); // right
            Draw_Fill (scr_vrect.x, scr_vrect.y+scr_vrect.height, scr_vrect.width, vid.height-(scr_vrect.y+scr_vrect.height)-screen_bottom, 0); // bottom
        }
#endif
        if (sbar.value > 1 || sb_showscores) // inventory bar
        {
            if (sbar.value > 2 || sb_showscores) // score bar
            {
                if (sbar_show_bg.value)
                    Sbar_DrawTransPic (0, -48, sb_scorebar);
                Sbar_SoloScoreboard (0, -48, 0);
            }
            if (sbar_show_bg.value)
            {
                if (rogue)
                {
                    if ( cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
                        Sbar_DrawTransPic (0, -24, rsb_invbar[0]);
                    else
                        Sbar_DrawTransPic (0, -24, rsb_invbar[1]);
                }
                else
                    Sbar_DrawTransPic (0, -24, sb_ibar);
            }

            Sbar_DrawAmmoList (0, -24, 0); // 0, -24, 0
            Sbar_DrawWeaponList (0, -16, 0); // 0, -16, 0
            if (rogue)
                Sbar_DrawKeys (192, -16); // 192, -16
            else
                Sbar_DrawKeys (192, -16); // 192, -16
            if (rogue || hipnotic)
                Sbar_DrawPowerUps (224, -16); // 224, -16
            else
                Sbar_DrawPowerUps (224, -16); // 224, -16

            if (rogue)
                Sbar_DrawPowerups_Rogue (288, -16); // 288, -16
            else if (hipnotic)
                Sbar_DrawPowerups_Hipnotic (288, -16); // 288, -16
            else
                Sbar_DrawRunes (288, -16); // sigils 288, -16

            Sbar_DrawFrags ();
        }
        // status bar
        if (sbar_show_bg.value)
            Sbar_DrawTransPic (0, 0, sb_sbar);
        Sbar_DrawArmor (0, 24, 0); // 0, 24, 0
        Sbar_DrawHealth (112, 136, 0); // 112, 136, 0
        Sbar_DrawHipKeys (209, 3); // keys (hipnotic only) 209, 3
        Sbar_DrawAmmo (224, 248, 0); // 224, 248, 0
    }
    else if (sbar.value == 4)
    {
#define SBAR_PADDING 4
        if (rogue || hipnotic)
            Sbar_SoloScoreboard (SBAR_PADDING, -49, 1);
        else
            Sbar_SoloScoreboard (SBAR_PADDING, -33, 1);

        Sbar_DrawAmmoList	(320 - SBAR_PADDING - 28, -9, 1);
        Sbar_DrawWeaponList	(320 - SBAR_PADDING - 24, -48, 1);

        if (rogue)
            Sbar_DrawKeys (320 - SBAR_PADDING - 56, -16);
        else
            Sbar_DrawKeys (320 - SBAR_PADDING - 88, -16);

        if (rogue || hipnotic)
        {
            Sbar_DrawPowerUps (SBAR_PADDING, -40);
            Sbar_DrawPowerups_Rogue		(64 + SBAR_PADDING, -40);
            Sbar_DrawPowerups_Hipnotic	(64 + SBAR_PADDING, -40);
        }
        else
            Sbar_DrawPowerUps (320 - SBAR_PADDING - 88, -32);

        Sbar_DrawRunes (320 - SBAR_PADDING - 56, -16); // sigils

//		Sbar_DrawFrags (); // TO DO

        Sbar_DrawArmor	(SBAR_PADDING, 24 + SBAR_PADDING, -24);
        Sbar_DrawHealth	(SBAR_PADDING, 24 + SBAR_PADDING, 0);
        Sbar_DrawHipKeys (320 - SBAR_PADDING - 40, -18); // keys (hipnotic only)
        Sbar_DrawAmmo (320 - SBAR_PADDING - 24, 320 - SBAR_PADDING - 96, 0);
    }

    if (cl.gametype == GAME_DEATHMATCH)
        if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
            Sbar_DeathmatchOverlay ();
// mankrip - end
}

//=============================================================================

/*
==================
Sbar_DeathmatchOverlay

==================
*/
void Sbar_DeathmatchOverlay (void)
{
    int				i, k, l;
    int				top, bottom;
    int				x, y, f;
    char			num[12];
    float           oldsbarscale; //qb: do it big
    scoreboard_t	*s;

    //qb: big and sort of lazy..
    oldsbarscale = sbar_scale.value;
    sbar_scale.value = 1.0;
    scr_2d_scale_h = vid.width / (MIN_VID_WIDTH/sbar_scale.value);
    scr_2d_scale_v =  vid.height / (MIN_VID_HEIGHT/sbar_scale.value);

    scr_copyeverything = 1;
    scr_fullupdate = 0;


    M_DrawPlaque ("gfx/ranking.lmp", false); // mankrip

// scores
    Sbar_SortFrags ();

// draw the text
    l = scoreboardlines;

    x = 80; // mankrip - edited
    y = 36; //qb: also edited, was 36 - mankrip - edited
    for (i=0 ; i<l ; i++)
    {
        k = fragsort[i];
        s = &cl.scores[k];
        if (!s->name[0])
            continue;

        // draw background
        top = s->colors & 0xf0;
        bottom = (s->colors & 15)<<4;
        top = Sbar_ColorForMap (top);
        bottom = Sbar_ColorForMap (bottom);

        M_Draw_Fill (x+20, y + 1    , 40, 4, top);
        M_Draw_Fill (x+20, y + 1 + 4, 40, 4, bottom);

        // draw number
        f = s->frags;
        sprintf (num, "%3i",f);

        M_DrawCharacter ( x+8 , y, num[0], true);
        M_DrawCharacter ( x+16 , y, num[1], true);
        M_DrawCharacter ( x+24 , y, num[2], true);

        if (k == cl.viewentity - 1)
            M_DrawCharacter ( x - 8, y, 12, true);

#if 0
        {
            int				total;
            int				n, minutes, tens, units;

            // draw time
            total = cl.completed_time - s->entertime;
            minutes = (int)total/60;
            n = total - minutes*60;
            tens = n/10;
            units = n%10;

            sprintf (num, "%3i:%i%i", minutes, tens, units);

            Draw_String ( x+48 , y, num);
        }
#endif

        // draw name
        Draw_String (x+64, y, s->name, true);

        y += 10;
    }
    sbar_scale.value =oldsbarscale;
        scr_2d_scale_h = vid.width / (360.0/sbar_scale.value);
    if (scr_2d_scale_h < 1)
        scr_2d_scale_h = 1;
    scr_2d_scale_v =  vid.height / (200.0/sbar_scale.value);
    if (scr_2d_scale_v < 1)
        scr_2d_scale_v = 1;
}



/*
==================
Sbar_IntermissionNumber

==================
*/
void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color)
{
    char			str[12];
    char			*ptr;
    int				l, frame;

    l = Sbar_itoa (num, str);
    ptr = str;
    if (l > digits)
        ptr += (l-digits);
    if (l < digits)
        x += (digits-l)*24;

    while (*ptr)
    {
        if (*ptr == '-')
            frame = STAT_MINUS;
        else
            frame = *ptr -'0';

        M_DrawTransPic (x,y,sb_nums[color][frame], true);
        x += 24;
        ptr++;
    }
}

/*
==================
Sbar_IntermissionOverlay

==================
*/
void Sbar_IntermissionOverlay (void)
{
    int		num;
    int		y = 28; // mankrip

    scr_copyeverything = 1;
    scr_fullupdate = 0;

    if (cl.gametype == GAME_DEATHMATCH)
    {
        Sbar_DeathmatchOverlay ();
        return;
    }

    // mankrip - begin
    M_DrawPlaque ("gfx/complete.lmp", false);
    M_DrawTransPic (0, y, Draw_CachePic ("gfx/inter.lmp"), true);
    // mankrip - end

// time
// mankrip - edited - begin
    num = cl.completed_time/60;
    Sbar_IntermissionNumber (160, y+=8/*64*/, num, 3, 0);
    num = cl.completed_time - num*60;
    M_DrawTransPic (234, y/*64*/, sb_colon, true);
    M_DrawTransPic (246, y/*64*/, sb_nums[0][num/10], true);
    M_DrawTransPic (266, y/*64*/, sb_nums[0][num%10], true);

    Sbar_IntermissionNumber (160, y+=40/*104*/, cl.stats[STAT_SECRETS], 3, 0);
    M_DrawTransPic			(232, y/*104*/, sb_slash, true);
    Sbar_IntermissionNumber (240, y/*104*/, cl.stats[STAT_TOTALSECRETS], 3, 0);

    Sbar_IntermissionNumber (160, y+=40/*144*/, cl.stats[STAT_MONSTERS], 3, 0);
    M_DrawTransPic			(232, y/*144*/, sb_slash, true);
    Sbar_IntermissionNumber (240, y/*144*/, cl.stats[STAT_TOTALMONSTERS], 3, 0);
// mankrip - edited - end
}


/*
==================
Sbar_FinaleOverlay

==================
*/
void Sbar_FinaleOverlay (void)
{
    scr_copyeverything = 1;
    // mankrip - begin
    M_DrawPlaque ("gfx/finale.lmp", false);
    // mankrip - end
}
