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
#include "quakedef.h"
#include<dirent.h>
#include<stdlib.h>
#include<sys/types.h>

extern int min_vid_width;  //qbism- Dan East

#ifdef _WIN32
#define	NET_MENUS 1
#else
#define	NET_MENUS 0
#endif

cvar_t	savename = {"savename","QBS8____"}; // 8 uppercase characters


//qbism - needed for Rikku2000 maplist, but not included w/ mingw
#ifndef FLASH
int scandir(const char *dir, struct dirent ***namelist,
            int (*select)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **)) {
  DIR *d;
  struct dirent *entry;
  register int i=0;
  size_t entrysize;

  if ((d=opendir(dir)) == NULL)
     return(-1);

  *namelist=NULL;
  while ((entry=readdir(d)) != NULL)
  {
    if (select == NULL || (select != NULL && (*select)(entry)))
    {
      *namelist=(struct dirent **)realloc((void *)(*namelist),
                 (size_t)((i+1)*sizeof(struct dirent *)));
	if (*namelist == NULL) return(-1);
	entrysize=sizeof(struct dirent)-sizeof(entry->d_name)+strlen(entry->d_name)+1;
	(*namelist)[i]=(struct dirent *)malloc(entrysize);
	if ((*namelist)[i] == NULL) return(-1);
	memcpy((*namelist)[i], entry, entrysize);
	i++;
    }
  }
  if (closedir(d)) return(-1);
  if (i == 0) return(-1);
  if (compar != NULL)
    qsort((void *)(*namelist), (size_t)i, sizeof(struct dirent *), compar);

  return(i);
}

//qbism - alphasort def is in dirent.h of alchemy (cygwin)
int alphasort(const struct dirent **a, const struct dirent **b) {
  return(strcmpi((*a)->d_name, (*b)->d_name));
}
#endif

void SetSavename ()
{
    // savename must always be 8 uppercase alphanumeric characters
    // non-alphanumeric characters (for example, spaces) must be converted to underscores
    int		i;
    char	*s;
    // backup old string
    s = Q_calloc (Q_strlen(savename.string)+1);
    Q_strcpy (s, savename.string);
    // free the old value string
    free (savename.string);

    savename.string = Q_calloc (8+1);
    for (i=0; i<8; i++)
    {
        if (!s[i]) // string was shorter than 8 characters
            break;
        if ((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'A' && s[i] <= 'Z'))
            savename.string[i] = s[i];
        else if (s[i] >= 'a' && s[i] <= 'z')
            savename.string[i] = s[i]-('a'-'A');
        else
            savename.string[i] = '_';
    }
    for (; i<8; i++) // fill the remaining characters, if the string was shorter than 8 characters
        savename.string[i] = '_';
    savename.string[i] = 0; // null termination
    savename.value = 0;
    free(s);
}
cvar_t	help_pages = {"help_pages","6"};

extern	cvar_t	samelevel;

extern	cvar_t	sv_aim_h;
extern	cvar_t	sv_aim;
extern	cvar_t	scr_ofsy;
extern	cvar_t	crosshair;
extern	cvar_t	crosshair_color;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	cl_nobob;
extern	cvar_t	sv_idealpitchscale;
extern	cvar_t	sv_enable_use_button;
extern	cvar_t	chase_active;
extern	cvar_t	chase_back;
extern	cvar_t	chase_up;

extern	cvar_t	r_drawentities;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_waterwarp;
extern	cvar_t	d_mipscale;
extern	cvar_t	r_polyblend;
extern	cvar_t	sw_stipplealpha;
//extern	cvar_t	r_sprite_addblend;
extern	cvar_t	scr_fadecolor; //qbism -TODO- put this on menu



/* // Manoel Kasimier - removed - begin
cvar_t	axis_x_function;
cvar_t	axis_y_function;
cvar_t	axis_l_function;
cvar_t	axis_r_function;
*/ // Manoel Kasimier - removed - end
cvar_t	axis_x_scale;
cvar_t	axis_y_scale;
cvar_t	axis_l_scale;
cvar_t	axis_r_scale;
cvar_t	axis_x_threshold;
cvar_t	axis_y_threshold;
cvar_t	axis_l_threshold;
cvar_t	axis_r_threshold;
/*/// Manoel Kasimier
extern	cvar_t	axis_pitch_dz;
extern	cvar_t	axis_yaw_dz;
extern	cvar_t	axis_walk_dz;
extern	cvar_t	axis_strafe_dz;
/*/// Manoel Kasimier

extern	cvar_t	snd_stereo;
extern	cvar_t	snd_swapstereo;

extern	cvar_t	sbar_show_scores;
extern	cvar_t	sbar_show_ammolist;
extern	cvar_t	sbar_show_weaponlist;
extern	cvar_t	sbar_show_keys;
extern	cvar_t	sbar_show_runes;
extern	cvar_t	sbar_show_powerups;
extern	cvar_t	sbar_show_armor;
extern	cvar_t	sbar_show_health;
extern	cvar_t	sbar_show_ammo;
extern	cvar_t	sbar_show_bg;
extern	cvar_t	sbar;

extern cvar_t	scr_fov; //qbism

void Crosshair_Start (int x, int y);
void Host_WriteConfiguration (void);
// Manoel Kasimier - end

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

enum
{
    m_none=0,
    m_main,
    m_singleplayer, m_load, m_save, m_loadsmall, m_savesmall,
    m_gameoptions,
    m_maplist,  //qbism - Rikku2000 maplist
    m_options,
    m_setup,
    m_keys,
    m_controller, m_mouse,
    m_gameplay, m_audio,
    m_video, m_videomodes, m_videosize,
    m_developer,
    m_credits, m_help, m_popup,
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
    m_multiplayer, m_net, m_lanconfig,
    m_search, m_slist,
#endif
    m_nummenus // Manoel Kasimier - replaced cursor variables
} m_state; // Edited by Manoel Kasimier

int m_cursor[m_nummenus]; // Manoel Kasimier - replaced cursor variables

void M_Off (void); // Manoel Kasimier
void M_Main_f (void);
void M_SinglePlayer_f (void);
void M_LoadSmall_f (void); // Manoel Kasimier - small saves
void M_SaveSmall_f (void); // Manoel Kasimier - small saves
void M_Load_f (void);
void M_Save_f (void);
void M_GameOptions_f (void);
void M_Options_f (void);
void M_Setup_f (void);
void M_Keys_f (void);
void M_Controller_f (void);
void M_Mouse_f (void);
void M_Gameplay_f (void);
void M_Audio_f (void);
void M_Video_f (void);
void M_VideoSize_f (void); // Manoel Kasimier
void M_VideoModes_f (void); // Manoel Kasimier - for Windows version only
void M_Developer_f (void);
void M_Help_f (void);
void M_Quit_f (void);

  //qbism - Rikku2000 maplist
void M_Menu_MapList_f (void);
void M_MapList_Draw (void);
void M_MapList_Key (int key);

#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
void M_MultiPlayer_f (void);
void M_Net_f (void);
void M_LanConfig_f (void);
void M_Search_f (void);
void M_ServerList_f (void);
#endif

void M_Main_Draw (void);
void M_SinglePlayer_Draw (void);
void M_Save_Draw (void);
void M_GameOptions_Draw (void);
void M_Options_Draw (void);
void M_Setup_Draw (void);
void M_Keys_Draw (void);
void M_Controller_Draw (void);
void M_Mouse_Draw (void);
void M_Gameplay_Draw (void);
void M_Audio_Draw (void);
void M_Video_Draw (void);
void M_Developer_Draw (void);
void M_Help_Draw (void);
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
void M_MultiPlayer_Draw (void);
void M_Net_Draw (void);
void M_LanConfig_Draw (void);
void M_Search_Draw (void);
void M_ServerList_Draw (void);
#endif

void M_Main_Key (int key);
void M_SinglePlayer_Key (int key);
void M_Save_Key (int key);
void M_GameOptions_Key (int key);
void M_Options_Key (int key);
void M_Setup_Key (int key);
void M_Keys_Key (int key);
void M_Controller_Key (int key);
void M_Mouse_Key (int key);
void M_Gameplay_Key (int key);
void M_Audio_Key (int key);
void M_Video_Key (int key);
void M_Developer_Key (int key);
void M_Help_Key (int key);
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
void M_MultiPlayer_Key (int key);
void M_Net_Key (int key);
void M_LanConfig_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);
#endif

// Manoel Kasimier - begin
byte	m_inp_up, m_inp_down, m_inp_left, m_inp_right,
m_inp_ok, m_inp_cancel, m_inp_no, m_inp_yes,
m_inp_off, m_inp_backspace
;
void M_CheckInput (int key)
{
    m_inp_up = m_inp_down = m_inp_left = m_inp_right =
            m_inp_ok = m_inp_cancel = m_inp_no = m_inp_yes =
                                          m_inp_off = m_inp_backspace = false;
    // keyboard/mouse keys
    switch (key)
    {
    case K_UPARROW:
    case K_MWHEELUP:
        m_inp_up = true;
        break;

    case K_DOWNARROW:
    case K_MWHEELDOWN:
        m_inp_down = true;
        break;

    case K_LEFTARROW:
        m_inp_left = true;
        break;

    case K_RIGHTARROW:
    case K_MOUSE2:
        m_inp_right = true;
        break;

    case K_MOUSE1: // Mouse 1 enables both m_inp_left and m_inp_ok
        m_inp_left = true;
    case K_ENTER:
        m_inp_ok = true;
        break;

    case K_ESCAPE:
        m_inp_cancel = true;
    case 'n':
    case 'N':
        m_inp_no = true;
        break;

    case 'Y':
    case 'y':
        m_inp_yes = true;
        break;

    case K_BACKSPACE:
        m_inp_backspace = true;

#if 0
    case K_DEL:				// delete multiple bindings
    case K_SPACE:

    case K_MOUSE3:
#endif
    default:
        break;
    }
};
// Manoel Kasimier - end

qboolean	m_entersound;		// play after drawing a frame, so caching
// won't disrupt the sound
qboolean	m_recursiveDraw;

int			m_save_demonum;

int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define	IPXConfig		(m_net_cursor == 0)
#define	TCPIPConfig		(m_net_cursor == 1)

void M_ConfigureNetSubsystem(void);
#endif
static int fade_level = 0;


byte identityTable[256];
byte translationTable[256];

// Manoel Kasimier - precaching menu pics - begin
void M_Precache (void)
{
    int i;
// text box
    Draw_CachePic ("gfx/box_tl.lmp");
    Draw_CachePic ("gfx/box_tm.lmp");
    Draw_CachePic ("gfx/box_tr.lmp");
    Draw_CachePic ("gfx/box_ml.lmp");
    Draw_CachePic ("gfx/box_mm.lmp");
    Draw_CachePic ("gfx/box_mr.lmp");
    Draw_CachePic ("gfx/box_bl.lmp");
    Draw_CachePic ("gfx/box_bm.lmp");
    Draw_CachePic ("gfx/box_br.lmp");
    Draw_CachePic ("gfx/box_mm2.lmp");
// big "q" cursor
    for (i=1; i<7; i++)
        Draw_CachePic(va("gfx/menudot%i.lmp", i));
// left plaque
    Draw_CachePic ("gfx/qplaque.lmp");
// main menu
    Draw_CachePic ("gfx/ttl_main.lmp");
    Draw_CachePic ("gfx/mainmenu.lmp");
// single player
    Draw_CachePic ("gfx/ttl_sgl.lmp");
//	Draw_CachePic ("gfx/sp_menu.lmp");
// load/save
    Draw_CachePic ("gfx/p_load.lmp");
    Draw_CachePic ("gfx/p_save.lmp");
// options
    Draw_CachePic ("gfx/p_option.lmp");
    Draw_CachePic ("gfx/bigbox.lmp");
    Draw_CachePic ("gfx/menuplyr.lmp");
//	Draw_CachePic ("gfx/ttl_cstm.lmp");
//	Draw_CachePic ("gfx/vidmodes.lmp");
// help
//	for (i=0; i<6; i++)
//		Draw_CachePic(va("gfx/help%i.lmp", i));
// Multiplayer
    Draw_CachePic ("gfx/p_multi.lmp");
    Draw_CachePic ("gfx/mp_menu.lmp");
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
    Draw_CachePic ("gfx/netmen1.lmp");
    Draw_CachePic ("gfx/dim_modm.lmp");
    Draw_CachePic ("gfx/netmen2.lmp");
    Draw_CachePic ("gfx/dim_drct.lmp");
    Draw_CachePic ("gfx/netmen3.lmp");
    Draw_CachePic ("gfx/dim_ipx.lmp");
    Draw_CachePic ("gfx/netmen4.lmp");
    Draw_CachePic ("gfx/dim_tcp.lmp");
    Draw_CachePic ("gfx/netmen5.lmp");
#endif

    // sound precaches
    S_PrecacheSound ("misc/menu1.wav");
    S_PrecacheSound ("misc/menu2.wav");
    S_PrecacheSound ("misc/menu3.wav");

    Vibration_Stop (0);
    Vibration_Stop (1);
}
// Manoel Kasimier - precaching menu pics - end

void M_BuildTranslationTable(int top, int bottom)
{
    int		j;
    byte	*dest, *source;

    for (j = 0; j < 256; j++)
        identityTable[j] = j;
    dest = translationTable;
    source = identityTable;
    memcpy (dest, source, 256);

    if (top < 128)	// the artists made some backwards ranges.  sigh.
        memcpy (dest + TOP_RANGE, source + top, 16);
    else
        for (j=0 ; j<16 ; j++)
            dest[TOP_RANGE+j] = source[top+15-j];

    if (bottom < 128)
        memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
    else
        for (j=0 ; j<16 ; j++)
            dest[BOTTOM_RANGE+j] = source[bottom+15-j];
}

void M_DrawCharacter (int cx, int line, int num)
{
    Draw_Character ( cx + ((vid.width - /*320*/min_vid_width)>>1), line, num);
}

void M_Print (int cx, int cy, char *str)
{
    while (*str)
    {
        M_DrawCharacter (cx, cy, (*str)+128);
        cx += 8;
        str++;
    }
}

void M_PrintWhite (int cx, int cy, char *str)
{
    while (*str)
    {
        M_DrawCharacter (cx, cy, *str);
        cx += 8;
        str++;
    }
}

void M_DrawTransPic (int x, int y, qpic_t *pic)
{
    Draw_TransPic (x + ((vid.width - /*320*/min_vid_width)>>1), y, pic);
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
    qpic_t	*p;
    int		cx, cy;
    int		n;
    int		w = -width & 7; // Manoel Kasimier
    int		h = -lines & 7; // Manoel Kasimier

    // draw left side
    cx = x;
    cy = y;
    p = Draw_CachePic ("gfx/box_tl.lmp");
    M_DrawTransPic (cx, cy, p);
    p = Draw_CachePic ("gfx/box_ml.lmp");
    for (n = 0; n < lines; n+=8)//-------------
    {
        cy += 8;
        M_DrawTransPic (cx, cy, p);
    }
    p = Draw_CachePic ("gfx/box_bl.lmp");
    M_DrawTransPic (cx, cy+8 - h, p);//------------

    // draw middle
    cx += 8;
    while (width > 0)
    {
        cy = y;
        p = Draw_CachePic ("gfx/box_tm.lmp");
        M_DrawTransPic (cx, cy, p);
        p = Draw_CachePic ("gfx/box_mm.lmp");
        for (n = 0; n < lines; n+=8) //---------
        {
            cy += 8;
            if (n/8 == 1)//-----------
                p = Draw_CachePic ("gfx/box_mm2.lmp");
            M_DrawTransPic (cx, cy, p);
        }
        p = Draw_CachePic ("gfx/box_bm.lmp");
        M_DrawTransPic (cx, cy+8 - h, p);//-----------!!!!!!!!
        width -= 2*8;
        cx += 16;
    }

    // draw right side
    cy = y;
    p = Draw_CachePic ("gfx/box_tr.lmp");
    M_DrawTransPic (cx - w, cy, p); // Manoel Kasimier - crosshair - edited
    p = Draw_CachePic ("gfx/box_mr.lmp");
    for (n = 0; n < lines; n+=8)//--------
    {
        cy += 8;
        M_DrawTransPic (cx - w, cy, p); // Manoel Kasimier - crosshair - edited
    }
    p = Draw_CachePic ("gfx/box_br.lmp");
    M_DrawTransPic (cx - w, cy+8 - h, p); // Manoel Kasimier - crosshair - edited
}

void M_DrawTransPicTranslate (int x, int y, qpic_t *pic)
{
    Draw_TransPicTranslate (x + ((vid.width - /*320*/min_vid_width)>>1), y, pic, translationTable);
}

#define	SLIDER_RANGE	10
void M_DrawSlider (int x, int y, float range)
{
    int	i;

    if (range < 0)
        range = 0;
    if (range > 1)
        range = 1;
    M_DrawCharacter (x-8, y, 128);
    for (i=0 ; i<SLIDER_RANGE ; i++)
        M_DrawCharacter (x + i*8, y, 129);
    M_DrawCharacter (x+i*8, y, 130);
    M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
#if 0
    if (on)
        M_DrawCharacter (x, y, 131);
    else
        M_DrawCharacter (x, y, 129);
#endif
    if (on)
        M_Print (x, y, "on");
    else
        M_Print (x, y, "off");
}

//=============================================================================
// Manoel Kasimier - begin
//=============================================================================
char *timetos (int mytime)
{
    int
    hours		= mytime/3600,
                  min_tens	= ((mytime%3600)/60)/10,
                                min_units	= ((mytime%3600)/60)%10,
                                        tens		= (mytime%60)/10,
                                                units		= (mytime%60)%10;
    return va("%i:%i%i:%i%i", hours, min_tens, min_units, tens, units);
}
char *skilltos(int i)
{
    if (i <= 0) return "Easy";
    if (i == 1) return "Normal";
    if (i == 2) return "Hard";
    if (i >= 3) return "Nightmare";
    return "";
}
void M_DrawPlaque (char *c, qboolean b)
{
    qpic_t	*p;
    if (b)
        M_DrawTransPic (16, 0, Draw_CachePic ("gfx/qplaque.lmp") );
    p = Draw_CachePic (c);
    M_DrawTransPic ( (/*320*/min_vid_width-p->width)/2, 0, p);
}
void M_DrawCursor (int x, int y, int itemindex)
{
    static int oldx, oldy, oldindex;
    static float cursortime;
    if ((oldx != x) || (oldy != y) || (oldindex != itemindex))
    {
        oldx = x;
        oldy = y;
        oldindex = itemindex;
        cursortime = realtime;
    }
    if ((cursortime + 0.5) >= realtime)
        M_DrawCharacter (x, y + itemindex*8, 13);
    else
        M_DrawCharacter (x, y + itemindex*8, 12+((int)(realtime*4)&1));
}
#define ALIGN_LEFT 0
#define ALIGN_RIGHT 8
#define ALIGN_CENTER 4
void M_PrintText (char *s, int x, int y, int w, int h, int alignment)
{
    int		l;
    int		line_w[64];
    int		xpos = x;
    char	*s_count = s;

    for (l=0; l<64; l++)
        line_w[l] = 0;

    l = 0;
    while (*s_count)
    {
        if (*s_count == 10)
            l++;
        else
            line_w[l]++;
        s_count++;
    }
    if (line_w[l]) // last line is empty
        l++;
    y += ((h - l)*4); // 0=top, 4=center, 8=bottom

    l = 0;
    while (*s)
    {
        if (*s == 10)
        {
            xpos = x;
            l++;
        }
        else
        {
            M_DrawCharacter (xpos + ((w - line_w[l])*alignment), y + (l*8), (*s)+128);
            xpos += 8;
        }
        s++;
    }
}
void ChangeCVar (char *cvar_s, float current, float interval, float min, float max, qboolean slider)
{
//	if (self.impulse == 21)
//		interval = interval * -1;
    current += interval;
    if (/*interval < 0 &&*/ current < min)
    {
        if (slider)
            current = min;
        else
            current = max;
    }
    else if (/*interval > 0 &&*/ current > max)
    {
        if (slider)
            current = max;
        else
            current = min;
    }
    Cvar_SetValue (cvar_s, current);
};
//=============================================================================
/* ON-SCREEN KEYBOARD */

// button definitions for on-screen keyboard
#define K_OSK_UP		K_AUX16
#define K_OSK_DOWN		K_AUX14
#define K_OSK_LEFT		K_AUX13
#define K_OSK_RIGHT		K_AUX15
#define K_OSK_BACKSPACE	K_AUX5
#define K_OSK_SPACE		K_AUX6
#define K_OSK_TAB		K_JOY3
#define K_OSK_ENTER		K_JOY4
#define K_OSK_INPUT		K_JOY1
#define K_OSK_QUIT		K_JOY2
#define	NUM_KEYCODES	52
int		keyboard_active;
int		keyboard_cursor;
int		keyboard_shift;

int	keycodes[] =
{
    '0','1','2','3','4','5','6','7','8','9','-','+','/','|', 92,'_',
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p',
    'q','r','s','t','u','v','w','x','y','z','"',';','.','?','~', K_SPACE,

    '<','>','(',')','[',']','{','}','@','#','$','%','^','&','*','=',
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z', 39,':',',','!','`', K_SPACE,

    K_BACKSPACE, K_SHIFT, K_TAB, K_ENTER
};

void M_OnScreenKeyboard_Reset (void)
{
    keyboard_active = keyboard_cursor = keyboard_shift = 0;
    m_inp_up = m_inp_down = m_inp_left = m_inp_right = m_inp_ok = m_inp_cancel = false;
}

void M_OnScreenKeyboard_Draw (int y)
{
    int i;
    if (!keyboard_active)
        return;

    M_DrawTextBox (16, y, 33*8, 5*8);
    for (i=0; i<47; i++)
        M_PrintWhite (36 + i*16 - (256*(i/16)), y+12+(8*(i/16)), Key_KeynumToString (keycodes[i+keyboard_shift]));
    i=96;
    M_Print (36			, y+36, Key_KeynumToString (keycodes[i++]));
    M_Print (36+11*8	, y+36, Key_KeynumToString (keycodes[i++]));
    M_Print (36+18*8	, y+36, Key_KeynumToString (keycodes[i++]));
    M_Print (36+23*8	, y+36, Key_KeynumToString (keycodes[i++]));

    if (keyboard_cursor < 48)
    {
        M_DrawCharacter (27+ keyboard_cursor*16 - (256*(keyboard_cursor/16)), y+12+(8*(keyboard_cursor/16)), 16);
        M_DrawCharacter (44+ keyboard_cursor*16 - (256*(keyboard_cursor/16)), y+12+(8*(keyboard_cursor/16)), 17);
    }
    else if (keyboard_cursor == 48)
        M_DrawCharacter (28			, y+36, 12+((int)(realtime*4)&1));
    else if (keyboard_cursor == 49)
        M_DrawCharacter (28+11*8	, y+36, 12+((int)(realtime*4)&1));
    else if (keyboard_cursor == 50)
        M_DrawCharacter (28+18*8	, y+36, 12+((int)(realtime*4)&1));
    else if (keyboard_cursor == 51)
        M_DrawCharacter (28+23*8	, y+36, 12+((int)(realtime*4)&1));
}

int M_OnScreenKeyboard_Key (int key)
{
    if (!keyboard_active)
    {
        if (key == K_OSK_INPUT)
        {
            M_OnScreenKeyboard_Reset();
            keyboard_active	= 1;
        }
        else
            return key;
    }
    else
    {
        m_inp_up = m_inp_down = m_inp_left = m_inp_right = m_inp_ok = m_inp_cancel = false;
        switch (key)
        {
        case K_OSK_QUIT:
            M_OnScreenKeyboard_Reset();
            break;

        case K_OSK_LEFT:
            if (--keyboard_cursor < 0)
                keyboard_cursor = NUM_KEYCODES-1;
            break;
        case K_OSK_RIGHT:
            if (++keyboard_cursor >= NUM_KEYCODES)
                keyboard_cursor = 0;
            break;
        case K_OSK_UP:
            if (keyboard_cursor >= 48)
                keyboard_cursor = 48;
            keyboard_cursor-=16;
            if (keyboard_cursor < 0)
                keyboard_cursor = NUM_KEYCODES-4;
            break;
        case K_OSK_DOWN:
            if (keyboard_cursor < 32)
                keyboard_cursor+=16;
            else if (keyboard_cursor < 48)
                keyboard_cursor = NUM_KEYCODES-4;
            else
                keyboard_cursor = 0;
            break;

        case K_OSK_BACKSPACE:
            return K_BACKSPACE;
        case K_OSK_SPACE:
            return K_SPACE;
        case K_OSK_INPUT:
            if (keyboard_cursor < 48)
                return keycodes[keyboard_cursor+keyboard_shift];
            else if (keyboard_cursor == 49)
                keyboard_shift = (!keyboard_shift)*48;
            else
                return keycodes[keyboard_cursor+48];
        case K_OSK_TAB:
            return K_TAB;
        case K_OSK_ENTER:
            return K_ENTER;
        default:
            return key;
        }
    }
    return 0;
}

//=============================================================================
/* POP-UP MENU */
int			m_prevstate;
qboolean	wasInMenus;
char		*popup_message;
char		*popup_command;

void M_PopUp_f (char *s, char *cmd)
{
    if (m_state != m_popup)
    {
        wasInMenus = (key_dest == key_menu);
        key_dest = key_menu;
        m_prevstate = m_state; // set previously active menu
        m_state = m_popup;
    }
    m_entersound = true;
    popup_message = s;
    if (Q_strlen(cmd))
    {
        popup_command = Q_calloc (Q_strlen(cmd)+1);
        Q_strcpy (popup_command, cmd);
    }
    else
        popup_command = NULL;
}

void M_PopUp_Draw (void)
{
    if (wasInMenus)
    {
        m_state = m_prevstate;
        m_recursiveDraw = true;
        M_Draw ();
        m_state = m_popup;
    }
    //qbism M_DrawTextBox (56, (vid.height - 48) / 2, 24*8, 4*8);
    M_PrintText (popup_message, 64, (vid.height + 8) / 2, 24, 4, ALIGN_CENTER); //qbism was vid.height - 32
}

void M_PopUp_Key (int key)
{
    if (m_inp_yes && popup_command)
        Cbuf_AddText(popup_command);
    else if (!m_inp_no)
        return;

    if (wasInMenus)
    {
        m_state = m_prevstate;
        m_entersound = true;
    }
    else
    {
        fade_level = 0; // BlackAura - Menu background fading
        key_dest = key_game;
        m_state = m_none;
    }
    if (popup_command)
        free(popup_command);
}

void M_Off (void) // turns the menu off
{
    key_dest = key_game;
    m_state = m_none;
    cls.demonum = m_save_demonum;
    if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
        CL_NextDemo ();
}
//=============================================================================
// Manoel Kasimier - end
//=============================================================================

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
    m_entersound = true;

    if (key_dest == key_menu)
    {
        if (m_state != m_main)
        {
            M_Main_f ();
            return;
        }

        key_dest = key_game;
        m_state = m_none;
        return;
    }
    if (key_dest == key_console)
    {
        Con_ToggleConsole_f ();
    }
    else
    {
        M_Main_f ();
    }
}

//=============================================================================
/* MAIN MENU */

#define	MAIN_ITEMS	5


void M_Main_f (void)
{
    if (key_dest != key_menu)
    {
        m_save_demonum = cls.demonum;
        cls.demonum = -1;
    }
    key_dest = key_menu;
    m_state = m_main;
    m_entersound = true;
    M_Precache(); // Manoel Kasimier - precaching menu pics
}


void M_Main_Draw (void)
{
    M_DrawPlaque ("gfx/ttl_main.lmp", true); // Manoel Kasimier

    M_DrawTransPic (72, 28, Draw_CachePic ("gfx/mainmenu.lmp") );

    M_DrawTransPic (54, 28 + m_cursor[m_state] * 20, Draw_CachePic( va("gfx/menudot%i.lmp", 1+((int)(realtime * 10)%6) ) ) ); // edited by Manoel Kasimier
}


void M_Main_Key (int key)
{
    // The DC_B shouldn't turn the main menu off
    if (key==K_ESCAPE)
    {
#ifdef FLASH
        //For FLASH, we write config.cfg every time we leave the main menu.
        //This is because we cant do it when we quit (as is done originally),
        //as you cant quit a flash app

        Host_WriteConfiguration();
#endif
        M_Off ();
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= MAIN_ITEMS)
            m_cursor[m_state] = 0;
    }
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0)
            m_cursor[m_state] = MAIN_ITEMS - 1;
    }
    else if (m_inp_ok)
    {
        switch (m_cursor[m_state]) // Manoel Kasimier - replaced cursor variables
        {
        case 0:
            M_SinglePlayer_f ();
            break;

        case 1:
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
            M_MultiPlayer_f ();
#else
            M_GameOptions_f (); // Manoel Kasimier
#endif
            break;

        case 2:
            M_Options_f ();
            break;

        case 3:
            M_Help_f ();
            break;

        case 4:
            M_Quit_f ();
            break;
        }
    }
}

//=============================================================================
/* SINGLE PLAYER MENU */  //qbism- switched back to original menu

#define	SINGLEPLAYER_ITEMS	3

int		m_singleplayer_cursor;
qboolean	m_singleplayer_confirm;

void M_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
	m_singleplayer_confirm = false;
}

void M_SinglePlayer_Draw (void)
{
	int	f;
	qpic_t	*p;

	if (m_singleplayer_confirm)
	{
		M_PrintWhite (64, 11*8, "Are you sure you want to");
		M_PrintWhite (64, 12*8, "    start a new game?");
		return;
	}

	M_DrawTransPic (16, 0 /*4*/, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawTransPic ((320 - p->width) >> 1, 0 /*4*/, p);
	M_DrawTransPic (72, 28 /*32*/, Draw_CachePic("gfx/sp_menu.lmp"));

	f = (int)(host_time*10) % 6;
	M_DrawTransPic (54, 28 /*32*/ + m_singleplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));
}

static void StartNewGame (void)
{
	key_dest = key_game;
	if (sv.active)
		Cbuf_AddText ("disconnect\n");
	Cbuf_AddText ("maxplayers 1\n");
	Cvar_SetValue (&teamplay, 0);
	Cvar_SetValue (&coop, 0);
	Cbuf_AddText ("map start\n");
}

void M_SinglePlayer_Key (int key)
{
	if (m_singleplayer_confirm)
	{
		if (key == 'n' || key == K_ESCAPE)
		{
			m_singleplayer_confirm = false;
			m_entersound = true;
		}
		else if (key == 'y' || key == K_ENTER)
		{
			StartNewGame ();
		}
		return;
	}

	switch (key)
	{
	case K_ESCAPE:
		M_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_singleplayer_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;
		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active)
				m_singleplayer_confirm = true;
			else
				StartNewGame ();
			break;

		case 1:
			M_Load_f ();
			break;

		case 2:
			M_Save_f ();
			break;
		}
	}
}

//=============================================================================
/* LOAD/SAVE MENU */

// -------------------------------------------------
// BlackAura (08-12-2002) - Replacing fscanf (start)
// -------------------------------------------------
static void DCM_ScanString(FILE *file, char *string)
{
    char newchar;
    fread(&newchar, 1, 1, file);
    while(newchar != '\n')
    {
        *string++ = newchar;
        fread(&newchar, 1, 1, file);
    }
    *string++ = '\0';
}

static int DCM_ScanInt(FILE *file)
{
    char sbuf[32768];
    DCM_ScanString(file, sbuf);
    return Q_atoi(sbuf);
}
// -------------------------------------------------
//  BlackAura (08-12-2002) - Replacing fscanf (end)
// -------------------------------------------------

int		liststart[4]; // Manoel Kasimier [m_state - m_load]
int		load_cursormax; // Manoel Kasimier
#define	MAX_SAVEGAMES		100 // Manoel Kasimier - edited
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
char	*m_fileinfo[MAX_SAVEGAMES]; // Manoel Kasimier
int		loadable[MAX_SAVEGAMES];

void M_ScanSaves (qboolean smallsave) // Manoel Kasimier - edited
{
    int		i, j;
    char	name[MAX_OSPATH];
    FILE	*f;
    int		version;
    // Manoel Kasimier - begin
    int		i1, i2;
    char	*s;
    char	t_enemies[16], k_enemies[16], t_secrets[16], f_secrets[16], str[64], fileinfo[64];
    // Manoel Kasimier - end

    for (i=0 ; i<MAX_SAVEGAMES ; i++)
    {
        strcpy (m_filenames[i], "- empty -"); // Manoel Kasimier - edited
        loadable[i] = false;

        // Manoel Kasimier - begin
        if (m_fileinfo[i])
            free(m_fileinfo[i]);
        m_fileinfo[i] = NULL;

        sprintf (name, "%s/%s.%c%i%i", com_gamedir, savename.string, smallsave?'G':'S', i/10, i%10);
#ifdef FLASH
        as3ReadFileSharedObject(name);
#endif
        // Manoel Kasimier - end

        f = fopen (name, "r");
        if (!f)
            continue;

        // Manoel Kasimier - begin
        // set everything to "0"
        t_enemies[0] = k_enemies[0] = t_secrets[0] = f_secrets[0] = '0';
        t_enemies[1] = k_enemies[1] = t_secrets[1] = f_secrets[1] = 0;
        // Manoel Kasimier - end

        //  BlackAura (08-12-2002) - Replacing fscanf
        // fscanf (f, "%i\n", &version);
        // fscanf (f, "%79s\n", name);
        version = DCM_ScanInt(f);
        DCM_ScanString(f, name);
        strncpy (m_filenames[i], name, sizeof(m_filenames[i])-1 -17); // Manoel Kasimier - edited

        // change _ back to space
        for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
            if (m_filenames[i][j] == '_')
                m_filenames[i][j] = ' ';

        // Manoel Kasimier - begin
        // skill
        for (i1=0 ; i1 < 17; i1++)
            i2 = DCM_ScanInt(f);
        s = skilltos(i2);

        DCM_ScanString(f, fileinfo); // map
        strcat (fileinfo, va("\n%s\n", s)); // add skill
        if (!smallsave)
        {
            strcat (fileinfo, timetos(DCM_ScanInt(f))); // add time
            for (i2=0 ; i2 < 200 ; i2++)
            {
                DCM_ScanString(f, str);
                s = COM_Parse (str);
                if (com_token[0] == '}')
                    break;
                if (!Q_strcmp(com_token, "total_secrets"))
                {
                    s = COM_Parse (s);
                    Q_strcpy(t_secrets, com_token);
                    continue;
                }
                if (!Q_strcmp(com_token, "total_monsters"))
                {
                    s = COM_Parse (s);
                    Q_strcpy(t_enemies, com_token);
                    continue;
                }
                if (!Q_strcmp(com_token, "found_secrets"))
                {
                    s = COM_Parse (s);
                    Q_strcpy(f_secrets, com_token);
                    continue;
                }
                if (!Q_strcmp(com_token, "killed_monsters"))
                {
                    s = COM_Parse (s);
                    Q_strcpy(k_enemies, com_token);
                    break;
                }
            }
            Q_strcat(fileinfo, va("\n%s / %s", k_enemies, t_enemies));
            Q_strcat(fileinfo, va("\n%s / %s", f_secrets, t_secrets));
        }
        m_fileinfo[i] = Q_calloc(Q_strlen(fileinfo)+1);
        Q_strcpy(m_fileinfo[i], fileinfo);
        // Manoel Kasimier - end

        loadable[i] = true;
        fclose (f);
    }
}

// Manoel Kasimier - begin
void M_RefreshSaveList (void)
{
    if (key_dest == key_menu && (m_state == m_savesmall || m_state == m_save))
    {
        M_ScanSaves (m_state == m_savesmall);
        m_entersound = true;
    }
}
// info panel
void M_DrawFileInfo (int i, int y)
{
    if (m_state == m_loadsmall || m_state == m_savesmall)
    {
        M_DrawTextBox (72, y, 20*8, 2*8);
        M_PrintText ("map\nskill", 88, y+8, 20, 2, ALIGN_LEFT);
        M_PrintText (m_fileinfo[i], 160, y+8, 20, 2, ALIGN_LEFT);
    }
    else
    {
        M_DrawTextBox (72, y, 20*8, 5*8);
        M_PrintText ("map\nskill\ntime\nenemies\nsecrets", 88, y+8, 20, 5, ALIGN_LEFT);
        M_PrintText (m_fileinfo[i], 160, y+8, 20, 5, ALIGN_LEFT);
    }
}

// merged save/load menu functions

void M_Save_Common_f (int menustate)
{
    int		i, i2;
    qboolean saving = (menustate == m_save || menustate == m_savesmall);

    if (key_dest == key_menu && m_state == menustate)
    {
        M_Off ();
        return;
    }
    if (saving)
    {
        if (!sv.active)
            return;
        if (svs.maxclients != 1)
            return;
    }
    M_ScanSaves (menustate == m_savesmall || menustate == m_loadsmall);
    for (i=i2=0; i< MAX_SAVEGAMES; i++)
        if (loadable[i] || (saving && (loadable[i-1] || i == 0)))
            i2++;
    if (!saving && i2 == 0)
    {
        M_PopUp_f((menustate==m_loadsmall) ? "There isn't any save\nto load." : "There isn't any state\nto load.", "");
        return;
    }
    m_entersound = true;
    key_dest = key_menu;
    m_state = menustate;
    if (m_cursor[m_state] >= i2)
        m_cursor[m_state] = i2 - 1;
    if (m_cursor[m_state] < 0)
        m_cursor[m_state] = 0;
    M_Precache(); // Manoel Kasimier - precaching menu pics
}
void M_Save_f (void)
{
    if (cl.stats[STAT_HEALTH] <= 0) // the player is dead
        return;
    if (cl.intermission)
        return;
    M_Save_Common_f(m_save);
}
void M_SaveSmall_f (void)
{
    M_Save_Common_f(m_savesmall);
}
void M_Load_f (void)
{
    M_Save_Common_f(m_load);
}
void M_LoadSmall_f (void)
{
    M_Save_Common_f(m_loadsmall);
}

void M_Save_Draw (void)
{
    int		i, i2, numsaves;
    qboolean saving = (m_state == m_save || m_state == m_savesmall);
    qboolean smallsave = (m_state == m_loadsmall || m_state == m_savesmall);

    M_DrawPlaque (saving ? "gfx/p_save.lmp" : "gfx/p_load.lmp", true);

    numsaves = (int)((vid.height - 28 - 8*7) / 8);
    if (smallsave)
        numsaves += 3;

    for (i=i2=0 ; i< MAX_SAVEGAMES; i++)
        if (loadable[i] || (saving && (loadable[i-1] || i == 0)))
        {
            if ((i >= liststart[m_state - m_load]) && (i2 - liststart[m_state - m_load] >= 0) && (i2 - liststart[m_state - m_load] < numsaves))
            {
                M_Print (56 + (8*(i < 10)), 28 + 8*(i2 - liststart[m_state - m_load]), va("%i", i));
                if (loadable[i] == true)
                    M_Print		 (96, 28 + 8*(i2 - liststart[m_state - m_load]), m_filenames[i]);
                else
                    M_PrintWhite (96, 28 + 8*(i2 - liststart[m_state - m_load]), m_filenames[i]);
            }
            i2++;
        }
    load_cursormax = i2;

    if (load_cursormax >= numsaves)
        if (liststart[m_state - m_load] > load_cursormax - numsaves)
            liststart[m_state - m_load] = load_cursormax - numsaves;

    for (i=i2=0 ; i< MAX_SAVEGAMES; i++)
        if (loadable[i] || (saving && (loadable[i-1] || i == 0)))
        {
            if (m_cursor[m_state] == i2)
            {
                if (loadable[i])
                    M_DrawFileInfo(i, 28 + (numsaves*8));
                break;
            }
            i2++;
        }
    M_DrawCursor (80, 28, (m_cursor[m_state] - liststart[m_state - m_load])); // Manoel Kasimier
}
// Manoel Kasimier - end

void M_Save_Key (int k)
{
    // Manoel Kasimier - begin
    qboolean saving = (m_state == m_save || m_state == m_savesmall);
    int		i, i2, numsaves;
    numsaves = (int)((vid.height - (28) - 8*7) / 8) - 1;
    if (m_state == m_savesmall || m_state == m_loadsmall)
        numsaves += 3;
    // Manoel Kasimier - end

    if (m_inp_cancel)
        M_SinglePlayer_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        // Manoel Kasimier - begin
        if (--m_cursor[m_state] < 0)
        {
            m_cursor[m_state] = load_cursormax-1;
            liststart[m_state - m_load] = m_cursor[m_state] - numsaves;
        }
        if (m_cursor[m_state] < liststart[m_state - m_load])
            liststart[m_state - m_load] = m_cursor[m_state];
        if (liststart[m_state - m_load] < 0)
            liststart[m_state - m_load] = 0;
        // Manoel Kasimier - end
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        // Manoel Kasimier - begin
        if (++m_cursor[m_state] >= load_cursormax)
            liststart[m_state - m_load] = m_cursor[m_state] = 0;
        if (m_cursor[m_state] > liststart[m_state - m_load] + numsaves)
            liststart[m_state - m_load] = m_cursor[m_state] - numsaves;
        // Manoel Kasimier - end
    }
    else if (m_inp_ok)
    {
        // Manoel Kasimier - begin
        for (i=i2=0 ; i< MAX_SAVEGAMES; i++)
            if (loadable[i] || (saving && (loadable[i-1] || i == 0)))
            {
                if (m_cursor[m_state] == i2)
                {
                    if (saving)
                    {
                        // the "menu_main;menu_save" part is a hack to refresh the list of saves
                        if (loadable[i] == false)
                        {
                            Cbuf_AddText (va ("save%s %s.%c%i%i\nmenu_main;menu_save%s\n", (m_state==m_savesmall)?"small":"", savename.string, (m_state==m_savesmall)?'G':'S', i/10, i%10, (m_state==m_savesmall)?"small":""));
                            m_state = m_none; //qbism - exit this menu
                            key_dest = key_game; //qbism
                        }
                              else
                            M_PopUp_f ((m_state==m_savesmall)?"Overwrite saved game?":"Overwrite saved state?", va ("save%s %s.%c%i%i\nmenu_main;menu_save%s\n",
                                       (m_state==m_savesmall)?"small":"", savename.string, (m_state==m_savesmall)?'G':'S', i/10, i%10, (m_state==m_savesmall)?"small":""));
                        return;
                    }
                    else
                    {
                        // Manoel Kasimier - end
                        // Host_Loadgame_f can't bring up the loading plaque because too much
                        // stack space has been used, so do it now
                        SCR_BeginLoadingPlaque ();
                        Cbuf_AddText (va ("load%s %s.%c%i%i\n", (m_state==m_loadsmall)?"small":"", savename.string, (m_state==m_loadsmall)?'G':'S', i/10, i%10) );
                        fade_level = 0; // BlackAura - Menu background fading
                        m_state = m_none;
                        key_dest = key_game;
                        return;
                        // Manoel Kasimier - begin
                    }
                }
                i2++;
            }
        // Manoel Kasimier - end
    }
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
    char	*description;
    int		firstLevel;
    int		levels;
} episode_t;

typedef struct
{
    char	*name;
    char	*description;
} level_t;

level_t		id1levels[] = // Manoel Kasimier - renamed
{
    {"start", "Entrance"},				// 0

    {"e1m1", "Slipgate Complex"},		// 1
    {"e1m2", "Castle of the Damned"},
    {"e1m3", "The Necropolis"},
    {"e1m4", "The Grisly Grotto"},
    {"e1m5", "Gloom Keep"},
    {"e1m6", "The Door To Chthon"},
    {"e1m7", "The House of Chthon"},
    {"e1m8", "Ziggurat Vertigo"},

    {"e2m1", "The Installation"},		// 9
    {"e2m2", "Ogre Citadel"},
    {"e2m3", "Crypt of Decay"},
    {"e2m4", "The Ebon Fortress"},
    {"e2m5", "The Wizard's Manse"},
    {"e2m6", "The Dismal Oubliette"},
    {"e2m7", "Underearth"},

    {"e3m1", "Termination Central"},	// 16
    {"e3m2", "The Vaults of Zin"},
    {"e3m3", "The Tomb of Terror"},
    {"e3m4", "Satan's Dark Delight"},
    {"e3m5", "Wind Tunnels"},
    {"e3m6", "Chambers of Torment"},
    {"e3m7", "The Haunted Halls"},

    {"e4m1", "The Sewage System"},		// 23
    {"e4m2", "The Tower of Despair"},
    {"e4m3", "The Elder God Shrine"},
    {"e4m4", "The Palace of Hate"},
    {"e4m5", "Hell's Atrium"},
    {"e4m6", "The Pain Maze"},
    {"e4m7", "Azure Agony"},
    {"e4m8", "The Nameless City"},

    {"end", "Shub-Niggurath's Pit"},	// 31

    {"dm1", "Place of Two Deaths"},		// 32
    {"dm2", "Claustrophobopolis"},
    {"dm3", "The Abandoned Base"},
    {"dm4", "The Bad Place"},
    {"dm5", "The Cistern"},
    {"dm6", "The Dark Zone"}
};

//MED 01/06/97 added hipnotic levels
level_t     hipnoticlevels[] =
{
    {"start", "Command HQ"},				// 0

    {"hip1m1", "The Pumping Station"},	// 1
    {"hip1m2", "Storage Facility"},
    {"hip1m3", "The Lost Mine"},
    {"hip1m4", "Research Facility"},
    {"hip1m5", "Military Complex"},

    {"hip2m1", "Ancient Realms"},		// 6
    {"hip2m2", "The Black Cathedral"},
    {"hip2m3", "The Catacombs"},
    {"hip2m4", "The Crypt"},
    {"hip2m5", "Mortum's Keep"},
    {"hip2m6", "The Gremlin's Domain"},

    {"hip3m1", "Tur Torment"},			// 12
    {"hip3m2", "Pandemonium"},
    {"hip3m3", "Limbo"},
    {"hip3m4", "The Gauntlet"},

    {"hipend", "Armagon's Lair"},		// 16

    {"hipdm1", "The Edge of Oblivion"}	// 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t		roguelevels[] =
{
    {"start",	"Split Decision"},
    {"r1m1",	"Deviant's Domain"},
    {"r1m2",	"Dread Portal"},
    {"r1m3",	"Judgement Call"},
    {"r1m4",	"Cave of Death"},
    {"r1m5",	"Towers of Wrath"},
    {"r1m6",	"Temple of Pain"},
    {"r1m7",	"Tomb of the Overlord"},
    {"r2m1",	"Tempus Fugit"},
    {"r2m2",	"Elemental Fury I"},
    {"r2m3",	"Elemental Fury II"},
    {"r2m4",	"Curse of Osiris"},
    {"r2m5",	"Wizard's Keep"},
    {"r2m6",	"Blood Sacrifice"},
    {"r2m7",	"Last Bastion"},
    {"r2m8",	"Source of Evil"},
    {"ctf1",    "Division of Change"}
};

episode_t	id1episodes[] = // Manoel Kasimier - renamed
{
    {"Welcome to Quake", 0, 1},
    {"Doomed Dimension", 1, 8},
    {"Realm of Black Magic", 9, 7},
    {"Netherworld", 16, 7},
    {"The Elder World", 23, 8},
    {"Final Level", 31, 1},
    {"Deathmatch Arena", 32, 6}
};

//MED 01/06/97  added hipnotic episodes
episode_t   hipnoticepisodes[] =
{
    {"Scourge of Armagon", 0, 1},
    {"Fortress of the Dead", 1, 5},
    {"Dominion of Darkness", 6, 6},
    {"The Rift", 12, 4},
    {"Final Level", 16, 1},
    {"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t	rogueepisodes[] =
{
    {"Introduction", 0, 1},
    {"Hell's Fortress", 1, 7},
    {"Corridors of Time", 8, 8},
    {"Deathmatch Arena", 16, 1}
};

// Manoel Kasimier - begin
level_t		levels[1024];
int			nummaps;

episode_t	episodes[16];
int			numepisodes = -1;

void M_Maps_Clear (void)
{
    int i;

    if ((key_dest == key_menu) && (m_state == m_gameoptions))
        return;
    if (nummaps)
        for (i=0; i<=nummaps; i++)
        {
            free (levels[i].name);
            free (levels[i].description);
        }
    if (numepisodes >= 0)
        for (i=0; i<=numepisodes; i++)
            free (episodes[i].description);
    nummaps = 0;
    numepisodes = -1;
}

void M_AddEpisode (void)
{
    if (Cmd_Argc() != 2)
    {
        Con_Printf ("menu_addepisode <name>\n");
        return;
    }
    if (numepisodes >= 0)
        if (episodes[numepisodes].levels == 0)
        {
            Con_Printf (va("menu_addepisode: first you must add map(s) to episode \"%s\"\n"), episodes[numepisodes].description);
            return;
        }
    if ((key_dest == key_menu) && (m_state == m_gameoptions))
        return;

    numepisodes++;

    episodes[numepisodes].description = Q_calloc (strlen(Cmd_Argv(1))+1);
    strcpy (episodes[numepisodes].description, Cmd_Argv(1));
    episodes[numepisodes].firstLevel = nummaps;
}

void M_AddMap (void)
{
    char	*s;

    if (Cmd_Argc() != 3)
    {
        Con_Printf ("menu_addmap <filename> <description>\n");
        return;
    }
    if (numepisodes < 0)
    {
        Con_Printf ("menu_addmap: you must add an episode first\n");
        return;
    }
    if ((key_dest == key_menu) && (m_state == m_gameoptions))
        return;

    s = Q_calloc (strlen(Cmd_Argv(1))+1);
    strcpy (s, Cmd_Argv(1));
    levels[nummaps].name = s;

    s = Q_calloc (strlen(Cmd_Argv(2))+1);
    strcpy (s, Cmd_Argv(2));
    levels[nummaps].description = s;

    nummaps++;
    episodes[numepisodes].levels++;
}

void M_SetDefaultEpisodes (episode_t *myepisodes, int epcount, level_t *mylevels, int levcount)
{
    int i;
    numepisodes = epcount;
    nummaps = levcount;
    for (i=0; i<=numepisodes; i++)
    {
        episodes[i].description = Q_calloc (strlen(myepisodes[i].description)+1);
        strcpy (episodes[i].description, myepisodes[i].description);
        episodes[i].firstLevel = myepisodes[i].firstLevel;
        episodes[i].levels = myepisodes[i].levels;
    }
    for (i=0; i<=nummaps; i++)
    {
        levels[i].name			= Q_calloc (strlen(mylevels[i].name)+1);
        strcpy (levels[i].name			, mylevels[i].name);
        levels[i].description	= Q_calloc (strlen(mylevels[i].description)+1);
        strcpy (levels[i].description	, mylevels[i].description);
    }
}

int o_deathmatch = 1;
int o_teamplay;
int o_skill = 1;
int o_fraglimit = 10;
int o_timelimit;
int o_samelevel;
// Manoel Kasimier - end

int	startepisode;
int	startlevel;
int maxplayers;
qboolean m_serverInfoMessage = false;
double m_serverInfoMessageTime;

void M_GameOptions_f (void)
{
    // Manoel Kasimier - begin
    if (key_dest == key_menu && m_state == m_gameoptions)
    {
        M_Off ();
        return;
    }
    // Manoel Kasimier - end
    key_dest = key_menu;
    m_state = m_gameoptions;
    m_entersound = true;
    if (maxplayers == 0)
        maxplayers = svs.maxclients;
    if (maxplayers < 2)
        maxplayers = svs.maxclientslimit;
    // Manoel Kasimier - begin
    startepisode = startlevel = 0;
    if (nummaps)
    {
        if (episodes[numepisodes].levels == 0) // remove last episode if empty
        {
            free (episodes[numepisodes].description);
            numepisodes--;
        }
    }
    else if (hipnotic)
        M_SetDefaultEpisodes(hipnoticepisodes, 5, hipnoticlevels, 17);
    else if (rogue)
        M_SetDefaultEpisodes(rogueepisodes, 3, roguelevels, 16);
    else
    {
        if (registered.value)
            M_SetDefaultEpisodes(id1episodes, 6, id1levels, 37);
        else
            M_SetDefaultEpisodes(id1episodes, 1, id1levels, 8);
    }
    // Manoel Kasimier - end
}

#define	NUM_GAMEOPTIONS	10 // Manoel Kasimier - edited

void M_GameOptions_Draw (void)
{
    int y = 20; // Manoel Kasimier
    char *msg;

    M_DrawPlaque ("gfx/p_multi.lmp", true); // Manoel Kasimier

    // Manoel Kasimier - edited - begin
    M_Print (160, y+=8, "begin game");
    M_Print (0, y+=8, va("      Max players   %i", maxplayers));
    M_Print (0, y+=8, "        Game Type");
    M_Print (160, y, !o_deathmatch ? "Cooperative" : va("Deathmatch %i", o_deathmatch));
    M_Print (0, y+=8, "         Teamplay");
    // Manoel Kasimier - edited - end
    if (rogue)
        switch(o_teamplay)
        {
        case 1:
            msg = "No Friendly Fire";
            break;
        case 2:
            msg = "Friendly Fire";
            break;
        case 3:
            msg = "Tag";
            break;
        case 4:
            msg = "Capture the Flag";
            break;
        case 5:
            msg = "One Flag CTF";
            break;
        case 6:
            msg = "Three Team CTF";
            break;
        default:
            msg = "Off";
            break;
        }
    else
        switch(o_teamplay)
        {
        case 1:
            msg = "No Friendly Fire";
            break;
        case 2:
            msg = "Friendly Fire";
            break;
        default:
            msg = "Off";
            break;
        }
    M_Print (160, y, msg);

    // Manoel Kasimier - begin
    M_Print (0, y+=8, "            Skill");
    M_Print (160, y, skilltos(o_skill));
    M_Print (0, y+=8, "       Frag Limit");
    M_Print (160, y, !o_fraglimit ? "none" : va("%i frags", o_fraglimit));
    M_Print (0, y+=8, "       Time Limit");
    M_Print (160, y, !o_timelimit ? "none" : va("%i minutes", o_timelimit));
    M_Print (0, y+=8, "       Same Level");
    M_DrawCheckbox (160, y, o_samelevel);
    // Manoel Kasimier - end
    // Manoel Kasimier - edited - begin
    M_Print (0, y+=8, "          Episode");
    M_Print (160, y, episodes[startepisode].description);
    M_Print (0, y+=8, "            Level");
    M_Print (160, y, levels[episodes[startepisode].firstLevel + startlevel].description);
    M_PrintWhite (160, y+=8, levels[episodes[startepisode].firstLevel + startlevel].name);
    // Manoel Kasimier - edited - end

    // Manoel Kasimier - begin
    M_DrawTextBox (56, vid.height - 32, 24*8, 2*8);
    M_PrintText ("Changes take effect\nwhen starting a new game", 64, vid.height - 24, 24, 2, ALIGN_CENTER);

    M_DrawCursor (144, 28, m_cursor[m_state]);
    // Manoel Kasimier - end
}

void M_GameOptions_Change (int dir)
{
    // Manoel Kasimier - begin
    int c = m_cursor[m_state];
    int i = 1;

    S_LocalSound ("misc/menu3.wav");

    if (c == i++)
    {
        // Manoel Kasimier - end
        maxplayers += dir;
        if (maxplayers > svs.maxclientslimit)
        {
            maxplayers = svs.maxclientslimit;
            m_serverInfoMessage = true;
            m_serverInfoMessageTime = realtime;
        }
        if (maxplayers < 2)
            maxplayers = 2;
        // Manoel Kasimier - begin
    }
    if (c == i++)
    {
        if ((o_deathmatch += dir) < 0)
            o_deathmatch = 3;
        if (o_deathmatch > 3)
            o_deathmatch = 0;
    }
    if (c == i++)
    {
        if ((o_teamplay += dir) < 0)
            o_teamplay = (rogue ? 6 : 2);
        if (o_teamplay > (rogue ? 6 : 2))
            o_teamplay = 0;
    }
    if (c == i++)
    {
        if ((o_skill += dir) < 0)
            o_skill = 3;
        if (o_skill > 3)
            o_skill = 0;
    }
    if (c == i++)
    {
        if ((o_fraglimit += dir*5) < 0)
            o_fraglimit = 100;
        if (o_fraglimit > 100)
            o_fraglimit = 0;
    }
    if (c == i++)
    {
        if ((o_timelimit += dir*5) < 0)
            o_timelimit = 60;
        if (o_timelimit > 60)
            o_timelimit = 0;
    }
    if (c == i++) o_samelevel = !o_samelevel;
    if (c == i++)
    {
        // Manoel Kasimier - end
        startepisode += dir;
        if (startepisode < 0)
            startepisode = numepisodes; // Manoel Kasimier - edited
        if (startepisode > numepisodes) // Manoel Kasimier - edited
            startepisode = 0;
        startlevel = 0;
        // Manoel Kasimier - begin
    }
    if (c == i++)
    {
        // Manoel Kasimier - end
        startlevel += dir;
        if (startlevel < 0)
            startlevel = episodes[startepisode].levels - 1; // Manoel Kasimier - edited
        if (startlevel >= episodes[startepisode].levels) // Manoel Kasimier - edited
            startlevel = 0;
    } // Manoel Kasimier
}

void M_GameOptions_Key (int key)
{
    if (m_inp_cancel)
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
        M_Net_f ();
#else
        M_Main_f (); // Manoel Kasimier
#endif
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0) // Manoel Kasimier
            m_cursor[m_state] = NUM_GAMEOPTIONS-1; // Manoel Kasimier - edited
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= NUM_GAMEOPTIONS) // Manoel Kasimier
            m_cursor[m_state] = 0; // Manoel Kasimier - edited
    }
    else if (m_inp_left)
        M_GameOptions_Change (-1);
    else if (m_inp_right)
        M_GameOptions_Change (1);
    if (m_inp_ok)
    {
        if (m_cursor[m_state] == 0) // Manoel Kasimier - edited
        {
            // Manoel Kasimier - begin
            char *s;
            s = Q_calloc (Q_strlen(levels[episodes[startepisode].firstLevel + startlevel].name)+1);
            Q_strcpy (s, levels[episodes[startepisode].firstLevel + startlevel].name);
            // run "listen 0", so host_netport will be re-examined
            if (sv.active)
                M_PopUp_f("Are you sure you want to\nstart a new game?",
                          va("disconnect\nlisten 0\nmaxplayers %u\ncoop %i\ndeathmatch %i\nteamplay %i\nskill %i\nfraglimit %i\ntimelimit %i\nsamelevel %i\nmap %s\n",
                             maxplayers, !o_deathmatch, o_deathmatch, o_teamplay, o_skill, o_fraglimit, o_timelimit, o_samelevel, s));
            else
                Cbuf_AddText(va("disconnect\nlisten 0\nmaxplayers %u\ncoop %i\ndeathmatch %i\nteamplay %i\nskill %i\nfraglimit %i\ntimelimit %i\nsamelevel %i\nmap %s\n",
                                maxplayers, !o_deathmatch, o_deathmatch, o_teamplay, o_skill, o_fraglimit, o_timelimit, o_samelevel, s));
            free(s);
            // Manoel Kasimier - end
        }
        else if (!m_inp_left) // Manoel Kasimier
            M_GameOptions_Change (1);
    }
}

//=============================================================================
/* MAPLIST MENU */   //qbism - Rikku2000 maplist

#define MAX_FILE_LIST 15

int initState = 1;
int numFiles = 0;
int minFile = 0;
int maxFile = MAX_FILE_LIST;
struct dirent **nombres;
char listaFiles[MAX_FILE_LIST][256];
char nombreFile[MAX_FILE_LIST];
int arrowY = 35;
int posArrow = 0;
int numFileList = 0;
int lastArrowY = 0;

void DeleteArray (char listaFiles[MAX_FILE_LIST][256], int numFileList) {
   int i;

   for (i = 0; i < numFileList; i++)
      strcpy(listaFiles[i], "0");
}

void FileNames (char listaFiles[MAX_FILE_LIST][256], int numFileList) {
   int y = 35;
   int i;

   for(i = 0; i < numFileList; i++) {
      M_Print (74, y, listaFiles[i]);

      y += 8;
   }
}

int isFile(const struct dirent *nombre) {
   int isFile = 0;

   char *extension = strrchr(nombre->d_name, '.');

   if (strcmp(extension, ".bsp") == 0)
      isFile = 1;

   return isFile;
}

void M_Menu_MapList_f (void)
{
   key_dest = key_menu;
   m_state = m_maplist;
   m_entersound = true;

   if(initState) {
      numFiles = scandir(va("%s/maps", GAMENAME), &nombres, isFile, alphasort);

      if (numFiles < MAX_FILE_LIST) {
         maxFile = numFiles;
         numFileList = numFiles;
      } else
         numFileList = MAX_FILE_LIST;

      DeleteArray(listaFiles, numFileList);

      int x = 0;
      int i;

      for (i = minFile; i < maxFile; i++){
         strcpy(listaFiles[x], nombres[i]->d_name);
         x++;
      }

      initState = 0;
   }
}

void M_MapList_Draw (void)
{
   qpic_t   *p;
   int      x;

   M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
   p = Draw_CachePic ("gfx/p_multi.lmp");
   M_DrawTransPic ( (320-p->width)/2, 4, p);

   M_DrawTextBox (56, 27, 23, 15);
      FileNames(listaFiles, numFileList);

   M_DrawCharacter (64, arrowY, 12 + ((int)(realtime * 4) & 1));
}

void M_MapList_Key (int key)
{
   switch (key)
   {
   case K_ESCAPE:
      M_GameOptions_f ();
      break;

   case K_UPARROW:
      S_LocalSound ("misc/menu1.wav");
      arrowY -= 8;
      posArrow--;

      if (posArrow < 0) {
         if (minFile != 0) {
            minFile--;
            maxFile--;

            DeleteArray(listaFiles, numFileList);

            int x = 0;
            int i;

            for (i = minFile; i < maxFile; i++) {
               strcpy(listaFiles[x], nombres[i]->d_name);
               x++;
            }
         }

         arrowY = 35;
         posArrow = 0;
      }
      break;

   case K_DOWNARROW:
      S_LocalSound ("misc/menu1.wav");
      lastArrowY = arrowY;
      arrowY += 8;

      if (posArrow > numFileList-2) {
         if(maxFile+1 <= numFiles) {
            minFile++;
            maxFile++;

            DeleteArray(listaFiles, numFileList);

            int x = 0;
            int i;

            for (i = minFile; i < maxFile; i++) {
               strcpy(listaFiles[x], nombres[i]->d_name);
               x++;
            }
         }

         arrowY = lastArrowY;
      } else
         posArrow++;
      break;

   case K_ENTER:
      S_LocalSound ("misc/menu2.wav");
      strcpy(nombreFile, listaFiles[posArrow]);
      Cbuf_AddText (va("map %s\n", strtok(nombreFile, ".bsp")));
      break;
   }
}

//=============================================================================
/* OPTIONS MENU */

#define	OPTIONS_ITEMS	10
#define QUICKHACK	0

void M_Options_f (void)
{
    // Manoel Kasimier - begin
    if (key_dest == key_menu && m_state == m_options)
    {
        M_Off ();
        return;
    }
    // Manoel Kasimier - end
    key_dest = key_menu;
    m_state = m_options;
    m_entersound = true;
    M_Precache(); // Manoel Kasimier - precaching menu pics
}

void M_Options_Draw (void)
{
    // Manoel Kasimier - begin
    int y = 20;

    M_DrawPlaque ("gfx/p_option.lmp", true);

    M_Print (16, y += 8, "          Player setup    ...");
    // Manoel Kasimier - end
    M_Print (16, y += 8, "    Customize controls    ..."); // Manoel Kasimier - edited
    // Manoel Kasimier - begin
    M_Print (16, y += 8, "                 Mouse    ...");
    M_Print (16, y += 8, "              Gameplay    ...");
    M_Print (16, y += 8, "                 Audio    ...");
    M_Print (16, y += 8, "                 Video    ...");
    // Manoel Kasimier - end
    M_Print (16, y += 8, "         Go to console"); // Manoel Kasimier - edited
    M_Print (16, y += 8, "            Save setup"); // Manoel Kasimier - edited
    M_Print (16, y += 8, "            Load setup"); // Manoel Kasimier - edited
    M_Print (16, y += 8, "     Reset to defaults"); // Manoel Kasimier - edited

    M_DrawCursor (200, 28, m_cursor[m_state]); // Manoel Kasimier
}


void M_Options_Key (int k)
{
    if (m_inp_cancel)
        M_Main_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0) // Manoel Kasimier
            m_cursor[m_state] = OPTIONS_ITEMS-1; // Manoel Kasimier - edited
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= OPTIONS_ITEMS) // Manoel Kasimier
            m_cursor[m_state] = 0; // Manoel Kasimier - edited
    }
    else if (m_inp_ok)
    {
        switch (m_cursor[m_state]) // Manoel Kasimier - replaced cursor variables
        {
            // Manoel Kasimier - begin
        case 0:
            M_Setup_f ();
            break;
            // Manoel Kasimier - end
        case 1:
            M_Keys_f ();
            break;
            // Manoel Kasimier - begin
        case 2+QUICKHACK:
            M_Mouse_f ();
            break;
        case 3+QUICKHACK:
            M_Gameplay_f ();
            break;
        case 4+QUICKHACK:
           M_Audio_f ();
            break;
        case 5+QUICKHACK:
            M_Video_f ();
            break;
            // Manoel Kasimier - end
        case 6+QUICKHACK:
            m_state = m_none;
            Con_ToggleConsole_f ();
            break;
            // Manoel Kasimier - begin
        case 7+QUICKHACK:
            m_entersound = true;
            Host_WriteConfiguration ();
            break;
        case 8+QUICKHACK:
            M_PopUp_f("Do you wish to load the\nsaved settings?", "exec config.cfg\n"); // Manoel Kasimier
            break;
        case 9+QUICKHACK:
            M_PopUp_f("Do you wish to load the\ndefault settings?", "exec default.cfg\n"); // Manoel Kasimier
            break;
            // Manoel Kasimier - end
        default:
            break;
        }
    }
    else if (m_inp_backspace)
        if (m_cursor[m_state] == 7) // "Go to console" option
            M_Developer_f ();
}

//=============================================================================
/* SETUP MENU */

#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
int		setup_cursor_table[] = {8, 24, 48, 72, 104, 112}; // Manoel Kasimier - edited
#else
int		setup_cursor_table[] = {8, 32, 56, 88, 96}; // Manoel Kasimier - edited
#endif

#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
char	setup_hostname[16]; // Commented out by Manoel Kasimier
#endif
char	setup_myname[16];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;
int		setup_crosshair; // Manoel Kasimier
int		setup_crosshair_color; // Manoel Kasimier

#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
#define	NUM_SETUP_CMDS	6
#else
#define	NUM_SETUP_CMDS	5
#endif

void M_Setup_f (void)
{
    key_dest = key_menu;
    m_state = m_setup;
    m_entersound = true;
    Q_strcpy(setup_myname, cl_name.string);
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
    Q_strcpy(setup_hostname, hostname.string);
#endif
    setup_top = setup_oldtop = ((int)cl_color.value) >> 4;
    setup_bottom = setup_oldbottom = ((int)cl_color.value) & 15;
    setup_crosshair = crosshair.value; // Manoel Kasimier
    setup_crosshair_color = crosshair_color.value; // Manoel Kasimier
    M_OnScreenKeyboard_Reset(); // Manoel Kasimier
    M_Precache(); // Manoel Kasimier - precaching menu pics
}

void M_Setup_Draw (void)
{
    int i=0, i1, i2;
    int y = 28 - 4; // Manoel Kasimier

    M_DrawPlaque ("gfx/p_option.lmp", true); // Manoel Kasimier

    // Manoel Kasimier - edited - begin
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
    M_Print (64, y + setup_cursor_table[i], "Hostname");
    M_DrawTextBox (160, y + setup_cursor_table[i] - 8, 16*8, 1*8);
    M_Print (168, y + setup_cursor_table[i++], setup_hostname);
#endif
    M_Print (64, y + setup_cursor_table[i], "Your name");
    M_DrawTextBox (160, y + setup_cursor_table[i] - 8, 16*8, 1*8);
    M_Print (168, y + setup_cursor_table[i++], setup_myname);

    M_Print (64, y + setup_cursor_table[i++]/*32*/, "Shirt color");
    M_Print (64, y + setup_cursor_table[i]/*56*/, "Pants color");

    M_DrawTransPic (160, y + setup_cursor_table[i]-40/*16*/, Draw_CachePic ("gfx/bigbox.lmp"));
    M_BuildTranslationTable(setup_top*16, setup_bottom*16);
    M_DrawTransPicTranslate (172, y + setup_cursor_table[i++]-32/*24*/, Draw_CachePic ("gfx/menuplyr.lmp"));
    // Manoel Kasimier - edited - end
    // Manoel Kasimier - crosshair - begin
    M_Print (64, y + setup_cursor_table[i++]/*88*/, "Crosshair");
    M_Print (64, y + setup_cursor_table[i]/*96*/, "Color");
//	M_DrawTextBox (160, y + setup_cursor_table[i]-16/*80*/, 2*8 - 1, 2*8 - 1);
//	M_DrawTextBox (160, y + setup_cursor_table[i]-16/*80*/, ceil(1.5*(vid.width/320))*8 - 1, ceil(1.5*(vid.height<480 ? vid.height/200 : vid.height/240))*8 - 1);
    if (!setup_crosshair_color)
        Draw_Fill  (169 + ((vid.width - /*320*/min_vid_width)>>1), y + setup_cursor_table[i]-7/*80*/, 4+11*(vid.width/ /*320*/min_vid_width), 4+11*(vid.height<480 ? vid.height/200 : vid.height/240), 8);
    i1 = crosshair.value;
    i2 = crosshair_color.value;
    crosshair.value = setup_crosshair;
    crosshair_color.value = setup_crosshair_color;
    Crosshair_Start(171 + ((vid.width - /*320*/min_vid_width)>>1), y + setup_cursor_table[i]-5/*91*/);
    crosshair.value = i1;
    crosshair_color.value = i2;
    // Manoel Kasimier - crosshair - end

    M_DrawCursor (56, y + setup_cursor_table [m_cursor[m_state]], 0); // Manoel Kasimier

    // Manoel Kasimier - edited - begin
    if (m_cursor[m_state] == 0)
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
        M_DrawCharacter (168 + 8*strlen(setup_hostname), y + setup_cursor_table [m_cursor[m_state]], 10+((int)(realtime*4)&1));

    if (m_cursor[m_state] == 1)
#endif
        M_DrawCharacter (168 + 8*strlen(setup_myname  ), y + setup_cursor_table [m_cursor[m_state]], 10+((int)(realtime*4)&1));
    // Manoel Kasimier - edited - end
//24
    M_OnScreenKeyboard_Draw (setup_cursor_table [m_cursor[m_state]] + 12+20/*28 + 16*/); // Manoel Kasimier
}


void M_Setup_Key (int k)
{
    int			l;

    if (m_cursor[m_state] == 0) // Manoel Kasimier
        k = M_OnScreenKeyboard_Key (k); // Manoel Kasimier

    if (m_inp_off)
        m_inp_cancel = true;

    if (m_inp_cancel)
    {
        // Manoel Kasimier - update player setup on exit - begin
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
        if (Q_strcmp(cl_name.string, setup_hostname) != 0)
            Cbuf_AddText ( va ("hostname \"%s\"\n", setup_hostname) );
#endif
        if (Q_strcmp(cl_name.string, setup_myname) != 0)
            Cbuf_AddText ( va ("name \"%s\"\n", setup_myname) );
        if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
            Cbuf_AddText( va ("color %i %i\n", setup_top, setup_bottom) );
        Cvar_SetValue ("crosshair", setup_crosshair);
        Cvar_SetValue ("crosshair_color", setup_crosshair_color);
        if (m_inp_off)
            M_Off ();
        else
            M_Options_f ();
        // Manoel Kasimier - update player setup on exit - end
//		M_MultiPlayer_f (); // Commented out by Manoel Kasimier
    }
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0) // Manoel Kasimier
            m_cursor[m_state] = NUM_SETUP_CMDS-1; // Manoel Kasimier - edited
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= NUM_SETUP_CMDS) // Manoel Kasimier
            m_cursor[m_state] = 0; // Manoel Kasimier - edited
    }
    else if (m_inp_left)
    {
        if (m_cursor[m_state] < NUM_SETUP_CMDS-4) // Manoel Kasimier
            return;
        S_LocalSound ("misc/menu3.wav");
        if (m_cursor[m_state] == NUM_SETUP_CMDS-4) // Manoel Kasimier
            setup_top = setup_top - 1;
        if (m_cursor[m_state] == NUM_SETUP_CMDS-3) // Manoel Kasimier
            setup_bottom = setup_bottom - 1;
        // Manoel Kasimier - begin
        if (m_cursor[m_state] == NUM_SETUP_CMDS-2)
        {
            setup_crosshair -= 1;
            if (setup_crosshair < 0)
                setup_crosshair = 5;
        }
        if (m_cursor[m_state] == NUM_SETUP_CMDS-1)
        {
            setup_crosshair_color -= 1;
            if (setup_crosshair_color < 0)
                setup_crosshair_color = 17;
        }
        // Manoel Kasimier - end
    }
    else if (m_inp_right)
    {
        if (m_cursor[m_state] < NUM_SETUP_CMDS-4) // Manoel Kasimier
            return;
forward:
        S_LocalSound ("misc/menu3.wav");
        if (m_cursor[m_state] == NUM_SETUP_CMDS-4) // Manoel Kasimier
            setup_top = setup_top + 1;
        if (m_cursor[m_state] == NUM_SETUP_CMDS-3) // Manoel Kasimier
            setup_bottom = setup_bottom + 1;
        // Manoel Kasimier - begin
        if (m_cursor[m_state] == NUM_SETUP_CMDS-2)
        {
            setup_crosshair += 1;
            if (setup_crosshair > 5)
                setup_crosshair = 0;
        }
        if (m_cursor[m_state] == NUM_SETUP_CMDS-1)
        {
            setup_crosshair_color += 1;
            if (setup_crosshair_color > 17)
                setup_crosshair_color = 0;
        }
        // Manoel Kasimier - end
    }
    else if (m_inp_ok)
    {
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
        if (m_cursor[m_state] == 0 || m_cursor[m_state] == 1) // Manoel Kasimier
#else
        if (m_cursor[m_state] == 0) // Manoel Kasimier
#endif
            return;
        if (m_cursor[m_state] > 0 && m_cursor[m_state] < NUM_SETUP_CMDS) // Manoel Kasimier
            goto forward;
    }
    else if (k == K_BACKSPACE)
    {
        if (m_cursor[m_state] == 0) // Manoel Kasimier - edited
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
        {
            if (strlen(setup_hostname))
                setup_hostname[strlen(setup_hostname)-1] = 0;
        }

        if (m_cursor[m_state] == 1) // Manoel Kasimier - edited
#endif
        {
            if (strlen(setup_myname))
                setup_myname[strlen(setup_myname)-1] = 0;
        }
    }
    else if (k > 31 && k < 128)
    {
        if (m_cursor[m_state] == 0) // Manoel Kasimier - edited
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
        {
            l = strlen(setup_hostname);
            if (l < 15)
            {
                setup_hostname[l+1] = 0;
                setup_hostname[l] = k;
            }
        }
        if (m_cursor[m_state] == 1) // Manoel Kasimier - edited
#endif
        {
            l = strlen(setup_myname);
            if (l < 15)
            {
                setup_myname[l+1] = 0;
                setup_myname[l] = k;
            }
        }
    }
    if (m_cursor[m_state] != 0) // Manoel Kasimier
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
        if (m_cursor[m_state] != 1) // Manoel Kasimier
#endif
            M_OnScreenKeyboard_Reset(); // Manoel Kasimier

    if (setup_top > 13)
        setup_top = 0;
    if (setup_top < 0)
        setup_top = 13;
    if (setup_bottom > 13)
        setup_bottom = 0;
    if (setup_bottom < 0)
        setup_bottom = 13;
}

//=============================================================================
/* KEYS MENU */

// Manoel Kasimier - begin
#define	MAXKEYS 16
#define MAXCMDS 256

qboolean dont_bind[256];
char	*bindnames[256][2];
int		numcommands;
int		m_keys_pages;
//int		m_keys_page_start[16]; // first command on the page
int		cmd_position[256];//[NUMCOMMANDS];
int		cmd_for_position[256];
int		keys_for_cmd[MAXCMDS];
int		keyliststart;
// Manoel Kasimier - end
int		bind_grab;

// Manoel Kasimier - begin
void M_Keys_Bindable (void)
{
    if (Cmd_Argc() != 3)
    {
        Con_Printf ("bindable <key> <0/1>\n");
        return;
    }
    dont_bind[Key_StringToKeynum(Cmd_Argv(1))] = !Q_atof(Cmd_Argv(2));
}
// Manoel Kasimier - end

void M_UnbindCommand (char *command)
{
    // unbinds all keys bound to the specified command
    int		j;
    int		l;
    char	*b;

    l = strlen(command);

    for (j=0 ; j<256 ; j++)
    {
        if (dont_bind[j]) // Manoel Kasimier
            continue; // Manoel Kasimier
        b = keybindings[j];
        if (b) // Manoel Kasimier - edited
            if (!Q_strcmp (b, command))//, l) ) // Manoel Kasimier - edited
                Key_SetBinding (j, "");
        // Manoel Kasimier - begin
        b = shiftbindings[j];
        if (b)
            if (!Q_strcmp (b, command))//, l) )
                Key_SetShiftBinding (j, "");
        // Manoel Kasimier - end
    }
}

void M_FindKeysForCommand (char *command, int *keys) // Manoel Kasimier - edited
{
    // Finds how many keys are bound to the specified command
    int		count;
    int		j;
    int		l;
    char	*b;

    // Manoel Kasimier - begin
    for (count=0 ; count<MAXKEYS ; count++)
        keys[count] = -1;
    // Manoel Kasimier - end

    l = strlen(command);
    for (count=j=0 ; (j<256) || (count == MAXKEYS) ; j++) // Manoel Kasimier - edited
    {
        // look at each key
        b = keybindings[j];
        // Manoel Kasimier - edited - begin
        if (b) // if there is any command bound to this key,
            if (!Q_strcmp (b, command))//, l) ) //strncmp // compare the string bound to the key with the string of the command
            {
                keys[count] = j; // puts the number of the key into the slot of the array
                // Manoel Kasimier - edited - end
                count++;		 // select the next slot in the array
                if (count == MAXKEYS) // Manoel Kasimier - edited
                    break;
            }
        // Manoel Kasimier - begin
        // same stuff, now for shifted keys
        b = shiftbindings[j];
        if (b)
            if (!Q_strcmp (b, command))//, l) ) //strncmp
            {
                keys[count] = -j; // dirty hack, using negative integers to identify the shifted keys
                count++;
            }
        // Manoel Kasimier - end
    }
}

// Manoel Kasimier - begin
void M_FillKeyList (void)
{
    int		i, i2, position;
    int		keys[MAXKEYS];
    position = 0;

    // finds how many keys are set for each command

    // it assumes that at least one key is bound, for practicity purposes, because the menu
    // always displays at least one line (either the key or the "---") for each command
    for (i=0 ; i<MAXCMDS ; i++)
        keys_for_cmd[i] = 1;
    for (i=0 ; i<numcommands ; i++)
    {
        cmd_for_position[position] = i;
        M_FindKeysForCommand (bindnames[i][0], keys);
        for (i2=1 ; i2<MAXKEYS ; i2++)
        {
            if (keys[i2] != -1)
            {
                keys_for_cmd[i]++;
                cmd_for_position[position + i2] = i;
            }
            else break;
        }
        cmd_position[i] = position; // the position of the command in the menu
        position += keys_for_cmd[i];
    }
}

void M_Keys_Clear (void)
{
    int i;
    if ((key_dest == key_menu) && (m_state == m_keys))
        return;
    for (i=0; i<numcommands; i++)
    {
        free (bindnames [i][0]);
        free (bindnames [i][1]);
    }
    keyliststart = m_cursor[m_state] = numcommands = 0;
}

void M_Keys_AddCmd (void)
{
    char	*s;

    if (Cmd_Argc() != 3)
    {
        Con_Printf ("menu_keys_addcmd <command> <description>\n");
        return;
    }
    if ((key_dest == key_menu) && (m_state == m_keys))
        return;

    s = Q_calloc (strlen(Cmd_Argv(1))+1);
    strcpy (s, Cmd_Argv(1));
    bindnames [numcommands][0] = s;

    s = Q_calloc (strlen(Cmd_Argv(2))+1);
    strcpy (s, Cmd_Argv(2));
    bindnames [numcommands][1] = s;

    numcommands++;
}
// Manoel Kasimier - end

// Manoel Kasimier - reordered the default commands
char *defaultbindnames[][2] = // Manoel Kasimier - renamed
{
    {"+shift",			"function shift"}, // Manoel Kasimier
    {"+use",			"use"}, // Manoel Kasimier
    {"+attack", 		"attack"},
    {"impulse 10", 		"next weapon"},
    {"impulse 12",		"previous weapon"}, // BlackAura (11-12-2002) - Next/prev weapons

// Manoel Kasimier - begin
    {"impulse 1",		"Axe"},
    {"impulse 2",		"Shotgun"},
    {"impulse 3",		"Super shotgun"},
    {"impulse 4",		"Nailgun"},
    {"impulse 5",		"Super nailgun"},
    {"impulse 6",		"Grenade launcher"},
    {"impulse 7",		"Rocket launcher"},
    {"impulse 8",		"Thunderbolt"},
// Manoel Kasimier - end
    {"+jump", 			"jump / swim up"},
    {"+moveup",			"swim up"},
    {"+movedown",		"swim down"},
    {"+lookup", 		"look up"},
    {"+lookdown", 		"look down"},
    {"+left", 			"turn left"},
    {"+right", 			"turn right"},
    {"+forward", 		"walk forward"},
    {"+back", 			"backpedal"},
    {"+moveleft", 		"step left"},
    {"+moveright", 		"step right"},
    {"+strafe", 		"sidestep"},
    {"+speed", 			"run"},
    {"+klook", 			"keyboard look"},
    {"centerview", 		"center view"},
    {"+showscores",		"show scores"} // Manoel Kasimier
};
#define	NUMCOMMANDS (sizeof(defaultbindnames)/sizeof(defaultbindnames[0]))

// Manoel Kasimier - begin
void M_Keys_SetDefaultCmds (void)
{
    int i;

    keyliststart = m_cursor[m_state] = 0;
    numcommands = NUMCOMMANDS;
    for (i=0; i<numcommands; i++)
    {
        bindnames[i][0] = Q_calloc (strlen(defaultbindnames[i][0])+1);
        strcpy (bindnames[i][0], defaultbindnames[i][0]);
        bindnames[i][1] = Q_calloc (strlen(defaultbindnames[i][1])+1);
        strcpy (bindnames[i][1], defaultbindnames[i][1]);
    }
}
// Manoel Kasimier - end

void M_Keys_f (void)
{
    // Manoel Kasimier - begin
    if (key_dest == key_menu && m_state == m_keys)
    {
        M_Off ();
        return;
    }
    // Manoel Kasimier - end
    key_dest = key_menu;
    m_state = m_keys;
    m_entersound = true;
    // Manoel Kasimier - begin
    bind_grab = false;
    if (numcommands == 0)
        M_Keys_SetDefaultCmds();
    // Manoel Kasimier - end
    M_Precache(); // Manoel Kasimier - precaching menu pics
}

void M_Keys_Draw (void)
{
    // Manoel Kasimier - begin
    int		i, i2;
    int		keys[MAXKEYS];
    int	/*	x,*/ y;
    int		top, bottom;

    M_DrawPlaque ("gfx/p_option.lmp", true); // ttl_cstm Manoel Kasimier

//	m_keys_pages = 1; // for testing purposes

    if (m_keys_pages > 0)
        M_Print (128, 28, "Page 1/1");
    top = 28 + (m_keys_pages > 0)*16;
    bottom = vid.height - 48; // vid.height, textbox height

    M_DrawTextBox (0, bottom + 8, 37*8, 3*8);
    if (bind_grab)
        M_PrintText ("Hold Function Shift for \"FS + button\"\nPress a key or button for this action\nPress Start or ESC to cancel", 12, bottom + 16, 37, 3, ALIGN_CENTER);
    else
        M_PrintText ("A button or Enter to change\nX button or Backspace to clear\nDelete to clear all keys for command", 12, bottom + 16, 37, 3, ALIGN_CENTER);

    // search for known bindings
    y = top - (keyliststart*8) - 8;
    // Manoel Kasimier - end
    for (i=0 ; i<numcommands; i++) // Manoel Kasimier - edited
    {
        M_FindKeysForCommand (bindnames[i][0], keys);
        // Manoel Kasimier - begin
        if (((y+16) > top) && (y+7 < bottom))
        {
            // prints the description of the command
            M_Print (56, y += 8, bindnames[i][1]);
            // prints the first key bound to this command
            if (keys[0] == -1)
                M_PrintWhite (196, y, "---");
            else if (keys[0] < 0)
                M_Print (196, y, va("FS + %s", Key_KeynumToString (-keys[0])));
            else
                M_Print (196, y, Key_KeynumToString (keys[0]));
        }
        else
            y += 8;
        // prints the other keys bound to this command
        for (i2=1 ; i2<MAXKEYS ; i2++)
            if ((keys[i2] != -1) && (y+7 < bottom))
            {
                if ((y + 16) > top)
                {
                    if (keys[i2] < 0)
                        M_Print (196, y += 8, va("FS + %s", Key_KeynumToString (-keys[i2])));
                    else
                        M_Print (196, y += 8, Key_KeynumToString (keys[i2]));
                }
                else
                    y += 8;
            }
            else break;
        // Manoel Kasimier - end
    }

    M_FillKeyList(); // Manoel Kasimier
    if (bind_grab)
        M_DrawCharacter (186, top + (cmd_position[cmd_for_position[m_cursor[m_state]]] - keyliststart)*8, '='); // Manoel Kasimier
    else
        M_DrawCursor (186, top, (m_cursor[m_state] - keyliststart)); // Manoel Kasimier
}

void M_Keys_Key (int k)
{
    char	cmd[80];
    int		keys[MAXKEYS]; // Manoel Kasimier
    int		menulines = (vid.height - screen_top - 28 - (m_keys_pages>0)*16 - 48) / 8; // Manoel Kasimier

    M_FillKeyList(); // Manoel Kasimier

    if (bind_grab)
    {
        // defining a key
        S_LocalSound ("misc/menu3.wav"); // Edited by Manoel Kasimier
        // Manoel Kasimier - begin
        if (k>='A' && k<='Z')
            k = k + 'a' - 'A'; // convert upper case characters to lower case
        // Manoel Kasimier - end
        if ((k == K_ESCAPE) || m_inp_off) // Manoel Kasimier - edited
            bind_grab = false;
        // Manoel Kasimier - begin
        else if (keybindings[k] && !Q_strcmp(keybindings[k], "+shift"))
            return;
        else if (dont_bind[k] == false) // (k != '`')
        {
            if (shift_function)
                sprintf (cmd, "bindshift \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[cmd_for_position[m_cursor[m_state]]][0]);
            else
                // Manoel Kasimier - end
                sprintf (cmd, "bind \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[cmd_for_position[m_cursor[m_state]]][0]); // Manoel Kasimier - edited
            Cbuf_InsertText (cmd);
            bind_grab = false;
        }
        return;
    }

    if (m_inp_off)
        M_Off ();
    else if (m_inp_cancel)
        M_Options_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        // Manoel Kasimier - begin
        if (--m_cursor[m_state] < 0)
        {
            m_cursor[m_state] = cmd_position[numcommands-1] + keys_for_cmd[numcommands-1] -1;
            keyliststart = m_cursor[m_state] - menulines;
        }
        if (m_cursor[m_state] < keyliststart)
            keyliststart = m_cursor[m_state];
        if (keyliststart < 0)
            keyliststart = 0;
        // Manoel Kasimier - end
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        // Manoel Kasimier - begin
        if (++m_cursor[m_state] >= cmd_position[numcommands-1] + keys_for_cmd[numcommands-1])
            keyliststart = m_cursor[m_state] = 0;
        if (m_cursor[m_state] > keyliststart + menulines)
            keyliststart = m_cursor[m_state] - menulines;
        // Manoel Kasimier - end
    }
    else if (m_inp_ok)
    {
        S_LocalSound ("misc/menu3.wav"); // Edited by Manoel Kasimier
        bind_grab = true;
        if (keyliststart > cmd_position[cmd_for_position[m_cursor[m_state]]]) // Manoel Kasimier
            keyliststart = cmd_position[cmd_for_position[m_cursor[m_state]]]; // Manoel Kasimier
    }
    else if (m_inp_backspace) // delete binding
    {
        // Manoel Kasimier - begin
        S_LocalSound ("misc/menu3.wav");
        M_FindKeysForCommand (bindnames[cmd_for_position[m_cursor[m_state]]][0], keys);
        k = keys[m_cursor[m_state] - cmd_position[cmd_for_position[m_cursor[m_state]]]];
        if (dont_bind[k * -(k < 0)])
            return;
        if (k < 0)
            Key_SetShiftBinding (-k, "");
        else
            Key_SetBinding (k, "");
        M_FillKeyList();
        if (m_cursor[m_state] >= cmd_position[numcommands-1] + keys_for_cmd[numcommands-1])
        {
            m_cursor[m_state]  = cmd_position[numcommands-1] + keys_for_cmd[numcommands-1] -1;
            keyliststart = m_cursor[m_state] - menulines;
        }
        // Manoel Kasimier - end
    }
    else if (k == K_DEL) // delete multiple bindings
    {
        S_LocalSound ("misc/menu3.wav"); // Manoel Kasimier - edited
        M_UnbindCommand (bindnames[cmd_for_position[m_cursor[m_state]]][0]); // Manoel Kasimier - edited
    }
}

// Manoel Kasimier - begin
//=============================================================================
/* CONTROLLER OPTIONS MENU */
#define	CONTROLLER_ITEMS	8

void M_Controller_f (void)
{
    key_dest = key_menu;
    m_state = m_controller;
    m_entersound = true;
}
void M_Controller_Draw (void)
{
    int y = 20;

    M_DrawPlaque ("gfx/p_option.lmp", true);

    M_Print (16, y+=8, "       Axis X Deadzone");
    M_DrawSlider (220, y, axis_x_threshold.value * 2);
    M_Print (16, y+=8, "       Axis Y Deadzone");
    M_DrawSlider (220, y, axis_y_threshold.value * 2);
    M_Print (16, y+=8, "    Trigger L Deadzone");
    M_DrawSlider (220, y, axis_l_threshold.value * 2);
    M_Print (16, y+=8, "    Trigger R Deadzone");
    M_DrawSlider (220, y, axis_r_threshold.value * 2);
    M_Print (16, y+=8, "    Axis X Sensitivity");
    M_DrawSlider (220, y, axis_x_scale.value - 0.5);
    M_Print (16, y+=8, "    Axis Y Sensitivity");
    M_DrawSlider (220, y, axis_y_scale.value - 0.5);
    M_Print (16, y+=8, "         L Sensitivity");
    M_DrawSlider (220, y, axis_l_scale.value - 0.5);
    M_Print (16, y+=8, "         R Sensitivity");
    M_DrawSlider (220, y, axis_r_scale.value - 0.5);

    M_DrawCursor (200, 28, m_cursor[m_state]);
}
void M_Controller_Change (int dir)
{
    int c = m_cursor[m_state];
    int i = 0;

    S_LocalSound ("misc/menu3.wav");

    // threshold
    if (c == i++) ChangeCVar("dc_axisx_threshold", axis_x_threshold.value, dir * 0.05, 0, 0.5, true);
    if (c == i++) ChangeCVar("dc_axisy_threshold", axis_y_threshold.value, dir * 0.05, 0, 0.5, true);
    if (c == i++) ChangeCVar("dc_axisl_threshold", axis_l_threshold.value, dir * 0.05, 0, 0.5, true);
    if (c == i++) ChangeCVar("dc_axisr_threshold", axis_r_threshold.value, dir * 0.05, 0, 0.5, true);
    // sensitivity
    if (c == i++) ChangeCVar("dc_axisx_scale", axis_x_scale.value, dir * 0.1, 0.5, 1.5, true);
    if (c == i++) ChangeCVar("dc_axisy_scale", axis_y_scale.value, dir * 0.1, 0.5, 1.5, true);
    if (c == i++) ChangeCVar("dc_axisl_scale", axis_l_scale.value, dir * 0.1, 0.5, 1.5, true);
    if (c == i++) ChangeCVar("dc_axisr_scale", axis_r_scale.value, dir * 0.1, 0.5, 1.5, true);
}
void M_Controller_Key (int k)
{
    if (m_inp_cancel)
        M_Options_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0)
            m_cursor[m_state] = CONTROLLER_ITEMS-1;
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= CONTROLLER_ITEMS)
            m_cursor[m_state] = 0;
    }
    else if (m_inp_left)
        M_Controller_Change (-1);
    else if (m_inp_right || m_inp_ok)
        M_Controller_Change (1);
}
//=============================================================================
/* MOUSE CONFIG MENU */
#define	MOUSE_ITEMS	5

void M_Mouse_f (void)
{
    key_dest = key_menu;
    m_state = m_mouse;
    m_entersound = true;
}
void M_Mouse_Draw (void)
{
    int y = 20;

    M_DrawPlaque ("gfx/p_option.lmp", true);

    M_Print (16, y+=8, "           Sensitivity");
    M_DrawSlider (220, y, (sensitivity.value - 1)/10);
    M_Print (16, y+=8, "            Walk Speed");
    M_DrawSlider (220, y, m_forward.value - 0.5);
    M_Print (16, y+=8, "          Strafe Speed");
    M_DrawSlider (220, y, m_side.value - 0.5);
    M_Print (16, y+=8, "          Invert Mouse");
    M_DrawCheckbox (220, y, m_pitch.value < 0);
    M_Print (16, y+=8, "            Mouse Look");
    M_DrawCheckbox (220, y, m_look.value);

    M_DrawCursor (200, 28, m_cursor[m_state]);
}
void M_Mouse_Change (int dir)
{
    int c = m_cursor[m_state];
    int i = 0;

    S_LocalSound ("misc/menu3.wav");

    if (c == i++) ChangeCVar("sensitivity", sensitivity.value, dir * 0.5, 1, 11, true);
    if (c == i++) ChangeCVar("m_forward", m_forward.value, dir * 0.1, 0.5, 1.5, true);
    if (c == i++) ChangeCVar("m_side", m_side.value, dir * 0.1, 0.5, 1.5, true);
    if (c == i++) Cvar_SetValue ("m_pitch", -m_pitch.value);
    if (c == i++) Cvar_SetValue ("m_look", !m_look.value);
}
void M_Mouse_Key (int k)
{
    if (m_inp_cancel)
        M_Options_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0)
            m_cursor[m_state] = MOUSE_ITEMS-1;
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= MOUSE_ITEMS)
            m_cursor[m_state] = 0;
    }
    else if (m_inp_left)
        M_Mouse_Change (-1);
    else if (m_inp_right || m_inp_ok)
        M_Mouse_Change (1);
}
//=============================================================================
/* GAMEPLAY OPTIONS MENU */
#define	GAMEPLAY_ITEMS	12

void M_Gameplay_f (void)
{
    key_dest = key_menu;
    m_state = m_gameplay;
    m_entersound = true;
}
void M_Gameplay_Draw (void)
{
    int y = 20;

    M_DrawPlaque ("gfx/p_option.lmp", true);

    M_Print (16, y+=8, "        Aim Assistance");// M_DrawCheckbox (220, y, sv_aim.value < 1);
    if (sv_aim_h.value < 1)
        M_Print (220, y, "on");
    else if (sv_aim.value < 1)
        M_Print (220, y, "vertical");
    else
        M_Print (220, y, "off");
    M_Print (16, y+=8, "       Weapon Position");
    if (scr_ofsy.value > 0)
        M_Print (220, y, "Left");
    else if (scr_ofsy.value < 0)
        M_Print (220, y, "Right");
    else
        M_Print (220, y, "Center");
    M_Print (16, y+=8, "    View Player Weapon");
    M_DrawCheckbox (220, y, r_drawviewmodel.value);
    M_Print (16, y+=8, "               Bobbing");
    M_DrawCheckbox (220, y, !cl_nobob.value);
    M_Print (16, y+=8, "            Slope Look");
    M_DrawCheckbox (220, y, sv_idealpitchscale.value > 0);
    M_Print (16, y+=8, "      Auto Center View");
    M_DrawCheckbox (220, y, lookspring.value);
    M_Print (16, y+=8, "            Lookstrafe");
    M_DrawCheckbox (220, y, lookstrafe.value);
    M_Print (16, y+=8, "            Always Run");
    M_DrawCheckbox (220, y, cl_forwardspeed.value > 200);
    M_Print (16, y+=8, "    Allow \"use\" button");
    M_DrawCheckbox (220, y, sv_enable_use_button.value);
    M_Print (16, y+=8, "             Body View");
    M_DrawCheckbox (220, y, chase_active.value);
    M_Print (16, y+=8, "    Body View Distance");
    M_DrawSlider (220, y, (chase_back.value - 50) / 100);
    M_Print (16, y+=8, "      Body View Height");
    M_DrawSlider (220, y, chase_up.value / 50);

    M_DrawCursor (200, 28, m_cursor[m_state]);
}
void M_Gameplay_Change (int dir)
{
    int c = m_cursor[m_state];
    int i = 0;

    S_LocalSound ("misc/menu3.wav");

    if (c == i++)
    {
//		(sv_aim.value < 1) ? Cvar_SetValue ("sv_aim", 1) : Cvar_SetValue ("sv_aim", 0.93);
        if (dir == 1)
        {
            if (sv_aim_h.value < 1) // on
            {
                Cvar_SetValue ("sv_aim_h", 1);
                Cvar_SetValue ("sv_aim", 1);
            }
            else if (sv_aim.value < 1) // vertical
                Cvar_SetValue ("sv_aim_h", 0.98);
            else // off
                Cvar_SetValue ("sv_aim", 0.93);
        }
        else
        {
            if (sv_aim_h.value < 1) // on
            {
                Cvar_SetValue ("sv_aim_h", 1);
                Cvar_SetValue ("sv_aim", 0.93);
            }
            else if (sv_aim.value < 1) // vertical
                Cvar_SetValue ("sv_aim", 1);
            else // off
                Cvar_SetValue ("sv_aim_h", 0.98);
        }
    }
    if (c == i++) ChangeCVar("scr_ofsy", scr_ofsy.value, dir * -7, -7, 7, false);
    if (c == i++) Cvar_SetValue ("r_drawviewmodel", !r_drawviewmodel.value);
    if (c == i++) Cvar_SetValue ("cl_nobob", !cl_nobob.value);
    if (c == i++) Cvar_SetValue ("sv_idealpitchscale", !sv_idealpitchscale.value * 0.8);
    if (c == i++) Cvar_SetValue ("lookspring", !lookspring.value);
    if (c == i++) Cvar_SetValue ("lookstrafe", !lookstrafe.value);
    if (c == i++) // always run
    {
        if (cl_forwardspeed.value > 200)
        {
            Cvar_SetValue ("cl_forwardspeed", 200);
            Cvar_SetValue ("cl_backspeed", 200);
        }
        else
        {
            Cvar_SetValue ("cl_forwardspeed", 400);
            Cvar_SetValue ("cl_backspeed", 400);
        }
    }
    if (c == i++) Cvar_SetValue ("sv_enable_use_button", !sv_enable_use_button.value);
    if (c == i++) Cvar_SetValue ("chase_active", !chase_active.value);
    if (c == i++) ChangeCVar("chase_back", chase_back.value, dir * 10, 50, 150, true);
    if (c == i++) ChangeCVar("chase_up", chase_up.value, dir * 5, 0, 50, true);
}
void M_Gameplay_Key (int k)
{
    if (m_inp_cancel)
        M_Options_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0)
            m_cursor[m_state] = GAMEPLAY_ITEMS-1;
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= GAMEPLAY_ITEMS)
            m_cursor[m_state] = 0;
    }
    else if (m_inp_left)
        M_Gameplay_Change (-1);
    else if (m_inp_right || m_inp_ok)
        M_Gameplay_Change (1);
}
//=============================================================================
/* AUDIO OPTIONS MENU */

#ifndef _WIN32
#define	AUDIO_ITEMS	12
#else
#define	AUDIO_ITEMS	11
#endif
byte cd_cursor;

void M_Audio_f (void)
{
    key_dest = key_menu;
    m_state = m_audio;
    m_entersound = true;
    cd_cursor = playTrack;
}

void M_Audio_Draw (void)
{
    int y = 20;

    M_DrawPlaque ("gfx/p_option.lmp", true);

    M_Print (16, y+=8, "          Stereo Sound");
    M_DrawCheckbox (220, y, snd_stereo.value);
    M_Print (16, y+=8, "     Swap L/R Channels");
    M_DrawCheckbox (220, y, snd_swapstereo.value);
    M_Print (16, y+=8, "          Sound Volume");
    M_DrawSlider (220, y, volume.value);
#ifndef _WIN32
    M_Print (16, y+=8, "       CD Music Volume");
    M_DrawSlider (220, y, bgmvolume.value);
#endif
    M_Print (16, y+=8, "              CD Music");
    M_DrawCheckbox (220, y, cd_enabled.value);
    M_Print (16, y+=8, "            Play Track");
    if (cdValid)
    {
        (cd_cursor == playTrack) ? M_Print (220, y, va("%u", cd_cursor)) : M_PrintWhite (220, y, va("%u", cd_cursor));
        M_Print (244, y, va("/ %u", maxTrack));
    }
    else if (maxTrack)
        M_Print (220, y, "No tracks");
    else
        M_Print (220, y, "Drive empty");
    M_Print (16, y+=8, "                  Loop");
    M_DrawCheckbox (220, y, playLooping);
    M_Print (16, y+=8, "                 Pause");
    M_DrawCheckbox (220, y, !playing && wasPlaying);
    M_Print (16, y+=8, "                  Stop");
    M_Print (16, y+=8, "            Open Drive");
    M_Print (16, y+=8, "           Close Drive");
    M_Print (16, y+=8, "        Reset CD Audio");

    M_DrawCursor (200, 28, m_cursor[m_state]);
}
void M_Audio_Change (int dir)
{
    int c = m_cursor[m_state];
    int i = 0;

    S_LocalSound ("misc/menu3.wav");

    if (c == i++) Cvar_SetValue ("snd_stereo", !snd_stereo.value);
    if (c == i++) Cvar_SetValue ("snd_swapstereo", !snd_swapstereo.value);
    if (c == i++) ChangeCVar("volume", volume.value, dir * 0.1, 0, 1, true);
#ifndef _WIN32
    if (c == i++) ChangeCVar("bgmvolume", bgmvolume.value, dir * 0.1, 0, 1, true);
#endif
    if (c == i++) Cvar_SetValue ("cd_enabled", !cd_enabled.value);
    if (c == i++)
    {
        if (!cdValid)
            return;
        do
        {
            cd_cursor += dir;
            if (cd_cursor > maxTrack)
                cd_cursor = 1;
            if (cd_cursor < 1)
                cd_cursor = maxTrack;
        }
        while (audioTrack[cd_cursor] == false);
    }
    if (c == i++) playLooping = !playLooping;
    if (c == i++) (!playing && wasPlaying) ? CDAudio_Resume() : CDAudio_Pause();
    if (c == i++) CDAudio_Stop();
    if (c == i++) Cbuf_AddText ("cd eject\n");
    if (c == i++) Cbuf_AddText ("cd close\n");
    if (c == i++) Cbuf_AddText ("cd reset\n");

}
void M_Audio_Key (int k)
{
    if (m_inp_cancel)
        M_Options_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0)
            m_cursor[m_state] = AUDIO_ITEMS-1;
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= AUDIO_ITEMS)
            m_cursor[m_state] = 0;
    }
#ifndef _WIN32
    else if (m_cursor[m_state] == 5 && m_inp_ok && cdValid && audioTrack[cd_cursor])
#else
    else if (m_cursor[m_state] == 4 && m_inp_ok && cdValid && audioTrack[cd_cursor])
#endif
        CDAudio_Play(cd_cursor, playLooping);
    else if (m_inp_left)
        M_Audio_Change (-1);
    else if (m_inp_right || m_inp_ok)
        M_Audio_Change (1);
}
// Manoel Kasimier - end

//=============================================================================
/* VIDEO MENU */

#define	VIDEO_ITEMS	18 //qbism

void M_Video_f (void)
{
    if (key_dest == key_menu && m_state == m_video)
    {
        M_Off ();
        return;
    }
    key_dest = key_menu;
    m_state = m_video;
    m_entersound = true;
}
// Manoel Kasimier - end


void M_Video_Draw (void)
{
    // Manoel Kasimier - begin
    int y = 19;

    M_DrawPlaque ("gfx/p_option.lmp", true);

    M_Print (16, y+=8, "           Video modes    ...");
    M_Print (16, y+=8, "    FOV- field of view");
    M_DrawSlider (220, y, (scr_fov.value - 30.0) / 110.0); //qbism
    M_Print (16, y+=8, "            Brightness");
    M_DrawSlider (220, y, (1.0 - v_gamma.value) / 0.5);
    M_Print (16, y+=8, "            Status bar");
    M_DrawSlider (220, y, sbar.value / 4.0);
    M_Print (16, y+=8, "            Background");
    M_DrawCheckbox (220, y, sbar_show_bg.value);
    M_Print (16, y+=8, "          Level status");
    M_DrawCheckbox (220, y, sbar_show_scores.value);
    M_Print (16, y+=8, "           Weapon list");
    M_DrawCheckbox (220, y, sbar_show_weaponlist.value);
    M_Print (16, y+=8, "             Ammo list");
    M_DrawCheckbox (220, y, sbar_show_ammolist.value);
    M_Print (16, y+=8, "                  Keys");
    M_DrawCheckbox (220, y, sbar_show_keys.value);
    M_Print (16, y+=8, "                 Runes");
    M_DrawCheckbox (220, y, sbar_show_runes.value);
    M_Print (16, y+=8, "              Powerups");
    M_DrawCheckbox (220, y, sbar_show_powerups.value);
    M_Print (16, y+=8, "                 Armor");
    M_DrawCheckbox (220, y, sbar_show_armor.value);
    M_Print (16, y+=8, "                Health");
    M_DrawCheckbox (220, y, sbar_show_health.value);
    M_Print (16, y+=8, "                  Ammo");
    M_DrawCheckbox (220, y, sbar_show_ammo.value);
    M_Print (16, y+=8, "      Classic lighting");
    M_DrawCheckbox (220, y, !r_light_style.value);
    M_Print (16, y+=8, "         Stipple alpha");
    M_DrawCheckbox (220, y, sw_stipplealpha.value);
    M_Print (16, y+=8, "         Water opacity");
    M_DrawSlider (220, y, r_wateralpha.value);

    M_DrawCursor (200, 28, m_cursor[m_state]);
    // Manoel Kasimier - end
}
// Manoel Kasimier - begin
void M_Video_Change (int dir)
{
    int c = m_cursor[m_state];
    int i = 1; //qbism changed

    S_LocalSound ("misc/menu3.wav");

    if (c == i++) ChangeCVar("fov", scr_fov.value, dir * 5, 30, 140, false); //qbism
    if (c == i++) ChangeCVar("gamma", v_gamma.value, dir * -0.05, 0.5, 1, true);
    if (c == i++) ChangeCVar("sbar", sbar.value, dir, 0, 4, true);
    if (c == i++) Cvar_SetValue ("sbar_show_bg", !sbar_show_bg.value);
    if (c == i++) Cvar_SetValue ("sbar_show_scores", !sbar_show_scores.value);
    if (c == i++) Cvar_SetValue ("sbar_show_weaponlist", !sbar_show_weaponlist.value);
    if (c == i++) Cvar_SetValue ("sbar_show_ammolist", !sbar_show_ammolist.value);
    if (c == i++) Cvar_SetValue ("sbar_show_keys", !sbar_show_keys.value);
    if (c == i++) Cvar_SetValue ("sbar_show_runes", !sbar_show_runes.value);
    if (c == i++) Cvar_SetValue ("sbar_show_powerups", !sbar_show_powerups.value);
    if (c == i++) Cvar_SetValue ("sbar_show_armor", !sbar_show_armor.value);
    if (c == i++) Cvar_SetValue ("sbar_show_health", !sbar_show_health.value);
    if (c == i++) Cvar_SetValue ("sbar_show_ammo", !sbar_show_ammo.value);
    if (c == i++) Cvar_SetValue ("r_light_style", !r_light_style.value);
    if (c == i++) Cvar_SetValue ("sw_stipplealpha", !sw_stipplealpha.value);
    if (c == i++) ChangeCVar("r_wateralpha", r_wateralpha.value, dir*0.166667, 0, 1, true);
}
// Manoel Kasimier - end
void M_Video_Key (int k) // int key Edited by Manoel Kasimier
{
//	(*vid_menukeyfn) (key);
    // Manoel Kasimier - begin
    if (m_inp_cancel)
        M_Options_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0)
            m_cursor[m_state] = VIDEO_ITEMS-1;
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= VIDEO_ITEMS)
            m_cursor[m_state] = 0;
    }
    else if (m_cursor[m_state] == 0 && m_inp_ok)

        M_VideoModes_f ();
//qbism	else if (m_cursor[m_state] == 1 && m_inp_ok)
    else if (m_inp_left)
        M_Video_Change (-1);
    else if (m_inp_right || m_inp_ok)
        M_Video_Change (1);
    // Manoel Kasimier - end
}


//=============================================================================
/* VIDEO MODES MENU */
// Manoel Kasimier - re-added the video modes menu for the PC version
void M_VideoModes_f (void)
{
    key_dest = key_menu;
    m_state = m_videomodes;
    m_entersound = true;
}

// Manoel Kasimier - begin
//=============================================================================
/* DEVELOPER MENU */

#define	DEVELOPER_ITEMS	16

void M_Developer_f (void)
{
    if (key_dest == key_menu && m_state == m_developer)
    {
        M_Off ();
        return;
    }
    key_dest = key_menu;
    m_state = m_developer;
    m_entersound = true;
}


void M_Developer_Draw (void)
{
    int y = 20;

    M_DrawPlaque ("gfx/p_option.lmp", false);

    M_Print (16, y+=8, "              Show FPS");
    M_DrawCheckbox (220, y, cl_showfps.value);
    M_Print (16, y+=8, "                   fog");
    M_DrawSlider (220, y, (developer.value-100.0)/255.0);
    M_Print (16, y+=8, "        developer mode");
    M_DrawCheckbox (220, y, developer.value);
    M_Print (16, y+=8, "       registered mode");
    M_DrawCheckbox (220, y, registered.value);
    M_Print (16, y+=8, "         draw entities");
    M_DrawCheckbox (220, y, r_drawentities.value);
    M_Print (16, y+=8, "           full bright");
    M_DrawCheckbox (220, y, r_fullbright.value);
    M_Print (16, y+=8, "underwater warp effect");
    M_DrawCheckbox (220, y, r_waterwarp.value);
    M_Print (16, y+=8, "  screen color effects");
    M_DrawCheckbox (220, y, r_polyblend.value);
    M_Print (16, y+=8, "              timedemo");
    M_Print (16, y+=8, "                noclip");
    M_Print (16, y+=8, "                   fly");
    M_Print (16, y+=8, "              notarget");
    M_Print (16, y+=8, "                   god");
    M_Print (16, y+=8, "             impulse 9");

    M_DrawCursor (200, 28, m_cursor[m_state]);
}
void M_Developer_Change (int dir)
{
    int c = m_cursor[m_state];
    int i = 0;

    S_LocalSound ("misc/menu3.wav");

    if (c == i++) Cvar_SetValue ("cl_showfps", !cl_showfps.value);
    if (c == i++) ChangeCVar("developer", developer.value, dir, 100, 355, false);
    if (c == i++) Cvar_SetValue ("developer", !developer.value);
    if (c == i++) Cvar_SetValue ("registered", !registered.value);
    if (c == i++) Cvar_SetValue ("r_drawentities", !r_drawentities.value);
    if (c == i++) Cvar_SetValue ("r_fullbright", !r_fullbright.value);
    if (c == i++) Cvar_SetValue ("r_waterwarp", !r_waterwarp.value);
    if (c == i++) Cvar_SetValue ("r_polyblend", !r_polyblend.value);
    if (c == i++)
    {
        M_Off();
        Cbuf_AddText ("timedemo demo1.dem\n");
    }
    if (c == i++) Cbuf_AddText ("noclip\n");
    if (c == i++) Cbuf_AddText ("fly\n");
    if (c == i++) Cbuf_AddText ("notarget\n");
    if (c == i++) Cbuf_AddText ("god\n");
    if (c == i++)
    {
        M_Off();
        Cbuf_AddText ("impulse 9\n");
    }
}
void M_Developer_Key (int k)
{
    if (m_inp_cancel)
        M_Options_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0)
            m_cursor[m_state] = DEVELOPER_ITEMS-1;
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= DEVELOPER_ITEMS)
            m_cursor[m_state] = 0;
    }
    else if (m_inp_left)
        M_Developer_Change (-1);
    else if (m_inp_right || m_inp_ok)
        M_Developer_Change (1);
}
// Manoel Kasimier - end
//=============================================================================
/* HELP MENU */
double m_credits_starttime;
void M_Credits_f (void)
{
    if (key_dest != key_menu)
    {
        m_save_demonum = cls.demonum;
        cls.demonum = -1;
    }
    key_dest = key_menu;
    m_state = m_credits;
    m_cursor[m_state] = 1;//help_pages.value;
    m_credits_starttime = realtime;
}

void M_Credits_Key (int key)
{
    if (m_cursor[m_state] == 1)
    {
        m_cursor[m_state] += 1;
        m_credits_starttime = realtime;
    }
    else if (m_cursor[m_state] == 2)
    {
        m_cursor[m_state] += 1;
        m_credits_starttime = realtime-8.0;//8.5
        Cbuf_InsertText ("exec quake.rc\n");
    }
    else if (m_cursor[m_state] == 3)
        M_Off ();
}

void M_Help_f (void)
{
    // Manoel Kasimier - begin
    int i;
    if (key_dest == key_menu && m_state == m_help)
    {
        M_Off ();
        return;
    }
    for (i=0; i<help_pages.value; i++)
        Draw_CachePic(va("gfx/help%i.lmp", i));
    m_cursor[m_state] = 0;
    // Manoel Kasimier - end
    key_dest = key_menu;
    m_state = m_help;
    m_entersound = true;
}

void M_Help_Draw (void)
{
    // Manoel Kasimier - begin
    qpic_t	*p;
    int y = r_refdef.vrect.y + ((r_refdef.vrect.height-200)/2);
    if (y < 0)
        y = 0;
    if (y+200 > vid.height)
        y = vid.height - 200;

    if (m_state == m_credits)
    {
        if ((int)(realtime-m_credits_starttime) > 9)
            M_Credits_Key(0);
        if (m_cursor[m_state] == 1)
            goto credits1;
        else if (m_cursor[m_state] == 2)
            goto credits2;
        return;
    }

    if (m_cursor[m_state] < help_pages.value)
    {
        p = Draw_CachePic ( va("gfx/help%i.lmp", m_cursor[m_state]));
        M_DrawTransPic (0, y, p);
    }
    else
    {
        if (m_cursor[m_state] == help_pages.value)
        {
credits1:
            M_DrawTextBox (0, y, 38*8, 23*8);
            M_Print		(16+4,	y+=12,	"     qbism Super 8 engine  ");
            M_Print		(16+4,	y+=8,	"             qbism.com  ");
            M_PrintWhite(16,	y+=16,	"           code sources:           ");
            M_PrintWhite(16+4,	y+=12,	"originally based on Makaqu     ");
            M_PrintWhite(16,	y+=8,	" Programmed by Manoel Kasimier");
            M_PrintWhite(16+4,	y+=12,	"and ToChriS Quake by Victor Luchitz");
            M_PrintWhite(16,	y+=16,	"FlashQuake port by Michael Rennie");
            M_PrintWhite(16,	y+=8,	"FlashProQuake port by Baker");
            M_PrintWhite(16,	y+=8,	"joequake engine by Jozsef Szalontai");
            M_PrintWhite(16,	y+=8,	"qrack engine coded by R00k");
            M_PrintWhite(16,	y+=8,	"fteqw engine by Spike and FTE Team");
            M_PrintWhite(16,	y+=8,	"FitzQuake coded by John Fitz");
            M_PrintWhite(16,	y+=8,	"DarkPlaces engine by Lord Havoc");
            M_PrintWhite(16,	y+=8,	"GoldQuake engine by Sajt");
            M_PrintWhite(16,	y+=8,	"enhanced WinQuake by Bengt Jardrup ");
            M_PrintWhite(16,	y+=8,	"Tutorials- JTR, Fett, Baker and MH");

            M_PrintWhite(16,	y+=16,	"Thanks to any other GPL coders!");
        }
        else
        {
credits2:
            M_DrawTextBox (0, y, 38*8, 23*8);
            M_PrintWhite(16+4,	y+=12, "     Quake engine version 1.09     ");

            M_PrintWhite(16,	y+=12, "Programming");
            M_Print		(16,	 y+=8, " John Carmack");
            M_Print		(16,	 y+=8, " Michael Abrash");
            M_Print		(16,	 y+=8, " John Cash");
            M_Print		(16,	 y+=8, " Dave 'Zoid' Kirsch");

            M_PrintWhite(16,	y+=12, "Quake is a trademark of Id Software,");
            M_PrintWhite(16,	 y+=8, "inc., (c)1996-1997 Id Software, inc.");
            M_PrintWhite(16,	 y+=8, "All rights reserved.");

            M_PrintWhite(16,	y+=16, "This engine is distributed in the");
            M_PrintWhite(16,	 y+=8, "hope that it will be useful, but");
            M_PrintWhite(16,	 y+=8, "WITHOUT ANY WARRANTY.");
            M_PrintWhite(16,	 y+=8, "Its code is licensed under the terms");
            M_PrintWhite(16,	 y+=8, "of the GNU General Public License.");
            M_PrintWhite(16,	 y+=8, "If you distribute modified binary");
            M_PrintWhite(16,	 y+=8, "versions of this engine, you must");
            M_PrintWhite(16,	 y+=8, "also make its source code available.");
            M_PrintWhite(16,	 y+=8, "See the GNU General Public License");
            M_PrintWhite(16,	 y+=8, "for more details.");
        }
    }
    // Manoel Kasimier - end
}

void M_Help_Key (int key)
{
    if (m_inp_cancel)
        M_Main_f ();
    else if (m_inp_down || m_inp_right)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_cursor[m_state] >= (help_pages.value+2))
            m_cursor[m_state] = 0;
    }
    else if (m_inp_up || m_inp_left)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_cursor[m_state] < 0)
            m_cursor[m_state] = help_pages.value+1;
    }

}

//=============================================================================
/* QUIT MENU */

char *quitMessage =  //qbism- show credits
    "Press Y to Quit.\n\
Press N to Reconsider.\n\
\n\
qbism Super8 engine\n\
\n\
Originally based on Makaqu\n\
Also thanks to code from ToChriS Quake,\n\
FlashQuake port, joequake engine,\n\
Qrack, FTEQW, FitzQuake, DarkPlaces,\n\
GoldQuake, and BJP's WinQuake\n\
Tutorials and support from inside3d.com";

void M_Quit_f (void)
{
    M_PopUp_f (quitMessage, "quit"); // Manoel Kasimier
}
// fazer menu c/ opções de reiniciar o mapa atual, resetar o jogo, resetar o console
// e sair pra BIOS
// usar arch_reboot pra resetar o DC
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
//void _ (){}

//=============================================================================
/* MULTIPLAYER MENU */

#define	MULTIPLAYER_ITEMS	3
int m_multiplayer_cursor;

void M_MultiPlayer_f (void)
{
    key_dest = key_menu;
    m_state = m_multiplayer;
    m_entersound = true;
}

void M_MultiPlayer_Draw (void)
{
    M_DrawPlaque ("gfx/p_multi.lmp", true); // Manoel Kasimier

    M_DrawTransPic (72, 28/*32*/, Draw_CachePic ("gfx/mp_menu.lmp") ); // Manoel Kasimier - edited

    M_DrawTransPic (54, 28/*32*/ + m_multiplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", 1+((int)(realtime * 10)%6) ) ) ); // Manoel Kasimier - edited

    if (ipxAvailable || tcpipAvailable)
        return;
    M_PrintWhite ((/*320*/min_vid_width/2) - ((27*8)/2), 148, "No Communications Available");
}

void M_MultiPlayer_Key (int key)
{
    // Manoel Kasimier - begin
    if (m_inp_cancel)
        M_Main_f ();
    else if (m_inp_up)
    {
        S_LocalSound ("misc/menu1.wav");
        if (--m_multiplayer_cursor < 0)
            m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
    }
    else if (m_inp_down)
    {
        S_LocalSound ("misc/menu1.wav");
        if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
            m_multiplayer_cursor = 0;
    }
    else if (m_inp_ok)
        // Manoel Kasimier - end
    {
        m_entersound = true;
        switch (m_multiplayer_cursor)
        {
        case 0:
            if (ipxAvailable || tcpipAvailable)
                M_Net_f ();
            break;
        case 1:
            if (ipxAvailable || tcpipAvailable)
                M_Net_f ();
            break;
        case 2:
            M_Setup_f ();
            break;
        }
    }
}

//=============================================================================
/* NET MENU */

int	m_net_cursor;
int m_net_items;
int m_net_saveHeight;

qboolean m_net_recursive = false;

void M_Net_f (void)
{
    qboolean m_net_direct = false;

    // either go direct to the appropriate menu or use the protocol selection menu.
    // the protocol selection menu will only show if 2 or more protocols are available.
    // it also shows if none are available to inform the user of this fact.
    if (ipxAvailable && !tcpipAvailable) m_net_direct = true;
    if (!ipxAvailable && tcpipAvailable) m_net_direct = true;

    if (m_net_direct)
    {
        if (!m_net_recursive)
        {
            // automatically select the correct protocol and go to the appropriate menu
            m_net_recursive = true;
            m_net_cursor = ipxAvailable ? 2 : 3;
            M_LanConfig_f ();
            return;
        }
        else
        {
            m_net_recursive = false;
            M_MultiPlayer_f ();
            return;
        }
    }
    else m_net_recursive = false;

    key_dest = key_menu;
    m_state = m_net;
    m_entersound = true;
    m_net_items = 4;

    if (m_net_cursor >= m_net_items)
        m_net_cursor = 0;

    m_net_cursor--;
    M_Net_Key (K_DOWNARROW);
}


void M_Net_Draw (void)
{
    int		f;
    qpic_t	*p;

    M_DrawPlaque ("gfx/p_multi.lmp", true); // Manoel Kasimier

    f = 28;//32; // Manoel Kasimier


#ifdef _WIN32
        p = NULL;
#else
        p = Draw_CachePic ("gfx/dim_modm.lmp");
#endif

    if (p)
        M_DrawTransPic (72, f, p);

    f += 19;

#ifdef _WIN32
        p = NULL;
#else
        p = Draw_CachePic ("gfx/dim_drct.lmp");
#endif

    if (p)
        M_DrawTransPic (72, f, p);

    f += 19;
    if (ipxAvailable)
        p = Draw_CachePic ("gfx/netmen3.lmp");
    else
        p = Draw_CachePic ("gfx/dim_ipx.lmp");
    M_DrawTransPic (72, f, p);

    f += 19;
    if (tcpipAvailable)
        p = Draw_CachePic ("gfx/netmen4.lmp");
    else
        p = Draw_CachePic ("gfx/dim_tcp.lmp");
    M_DrawTransPic (72, f, p);

    if (m_net_items == 5)	// JDC, could just be removed
    {
        f += 19;
        p = Draw_CachePic ("gfx/netmen5.lmp");
        M_DrawTransPic (72, f, p);
    }
    M_DrawTransPic (54, 28/*32*/ + m_net_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", 1+((int)(realtime * 10)%6) ) ) ); // Manoel Kasimier - edited
}


void M_Net_Key (int k)
{
    // Manoel Kasimier - begin
    if (m_inp_cancel)
        M_MultiPlayer_f ();
    // Manoel Kasimier - end
again:
    switch (k)
    {
    case K_DOWNARROW:
    case K_DPAD1_DOWN:
        S_LocalSound ("misc/menu1.wav");
        if (++m_net_cursor >= m_net_items)
            m_net_cursor = 0;
        break;

    case K_UPARROW:
    case K_DPAD1_UP:
        S_LocalSound ("misc/menu1.wav");
        if (--m_net_cursor < 0)
            m_net_cursor = m_net_items - 1;
        break;

    case K_ENTER:
    case K_DC_A:
        m_entersound = true;

        switch (m_net_cursor)
        {
        case 0:
            M_LanConfig_f ();
            break;

        case 1:
            M_LanConfig_f ();
            break;

        case 2:
// multiprotocol
            break;
        }
    }

    if (m_net_cursor == 0 && !ipxAvailable)
        goto again;
    if (m_net_cursor == 1 && !tcpipAvailable)
        goto again;
}

//=============================================================================
/* LAN CONFIG MENU */

int		lanConfig_cursor = -1;
//int		lanConfig_cursor_table [] = {72, 92, 124};
int		lanConfig_cursor_table [] = {40, 60, 92};
#define NUM_LANCONFIG_CMDS	3

int 	lanConfig_port;
char	lanConfig_portname[6];
char	lanConfig_joinname[22];

void M_LanConfig_f (void)
{
    key_dest = key_menu;
    m_state = m_lanconfig;
    m_entersound = true;
    if (lanConfig_cursor == -1)
    {
        if (JoiningGame && TCPIPConfig)
            lanConfig_cursor = 2;
        else
            lanConfig_cursor = 1;
    }
    if (StartingGame && lanConfig_cursor == 2)
        lanConfig_cursor = 1;
    lanConfig_port = DEFAULTnet_hostport;
    sprintf(lanConfig_portname, "%u", lanConfig_port);

    m_return_onerror = false;
    m_return_reason[0] = 0;
}


void M_LanConfig_Draw (void)
{
    qpic_t	*p;
    int		basex;
    char	*startJoin;
    char	*protocol;

    int		f = 28; // Manoel Kasimier
    M_DrawPlaque ("gfx/p_multi.lmp", true); // Manoel Kasimier

//	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
    p = Draw_CachePic ("gfx/p_multi.lmp");
    basex = (/*320*/min_vid_width-p->width)/2;
//	M_DrawTransPic (basex, 4, p);

    if (StartingGame)
        startJoin = "New Game";
    else
        startJoin = "Join Game";
    if (IPXConfig)
        protocol = "IPX";
    else
        protocol = "TCP/IP";
    M_Print (basex, f/*32*/, va ("%s - %s", startJoin, protocol));
    basex += 8;

    M_Print (basex, f+20/*52*/, "Address:");
    if (IPXConfig)
        M_Print (basex+9*8, f+20/*52*/, my_ipx_address);
    else
        M_Print (basex+9*8, f+20/*52*/, my_tcpip_address);

    M_Print (basex, f+lanConfig_cursor_table[0], "Port");
    M_DrawTextBox (basex+8*8, f+lanConfig_cursor_table[0]-8, 6*8, 1*8);
    M_Print (basex+9*8, f+lanConfig_cursor_table[0], lanConfig_portname);

    if (JoiningGame)
    {
        M_Print (basex, f+lanConfig_cursor_table[1], "Search for local games...");
        M_Print (basex, f+76/*108*/, "Join game at:");
        M_DrawTextBox (basex+8, f+lanConfig_cursor_table[2]-8, 22*8, 1*8);
        M_Print (basex+16, f+lanConfig_cursor_table[2], lanConfig_joinname);
    }
    else
    {
        M_DrawTextBox (basex, f+lanConfig_cursor_table[1]-8, 2*8, 1*8);
        M_Print (basex+8, f+lanConfig_cursor_table[1], "OK");
    }

    M_DrawCharacter (basex-8, f+lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

    if (lanConfig_cursor == 0)
        M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), f+lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

    if (lanConfig_cursor == 2)
        M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), f+lanConfig_cursor_table [2], 10+((int)(realtime*4)&1));

    if (*m_return_reason)
        M_PrintWhite (basex, f+116/*148*/, m_return_reason);
}


void M_LanConfig_Key (int key)
{
    int		l;

    // Manoel Kasimier - begin
    if (m_inp_cancel)
        M_Net_f ();
    // Manoel Kasimier - end
    switch (key)
    {
    case K_UPARROW:
    case K_DPAD1_UP:
        S_LocalSound ("misc/menu1.wav");
        lanConfig_cursor--;
        if (lanConfig_cursor < 0)
            lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
        break;

    case K_DOWNARROW:
    case K_DPAD1_DOWN:
        S_LocalSound ("misc/menu1.wav");
        lanConfig_cursor++;
        if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
            lanConfig_cursor = 0;
        break;

    case K_ENTER:
    case K_DC_A:
        if (lanConfig_cursor == 0)
            break;

        m_entersound = true;

        M_ConfigureNetSubsystem ();

        if (lanConfig_cursor == 1)
        {
            if (StartingGame)
            {
                M_GameOptions_f ();
                break;
            }
            M_Search_f();
            break;
        }

        if (lanConfig_cursor == 2)
        {
            m_return_state = m_state;
            m_return_onerror = true;
            // BlackAura - Menu background fading
            fade_level = 0;
            key_dest = key_game;
            m_state = m_none;
            Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
            break;
        }

        break;

    case K_BACKSPACE:
    case K_DC_X: // Edited by Manoel Kasimier
        if (lanConfig_cursor == 0)
        {
            if (strlen(lanConfig_portname))
                lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
        }

        if (lanConfig_cursor == 2)
        {
            if (strlen(lanConfig_joinname))
                lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
        }
        break;

    default:
        if (key < 32 || key > 127)
            break;

        if (lanConfig_cursor == 2)
        {
            l = strlen(lanConfig_joinname);
            if (l < 21)
            {
                lanConfig_joinname[l+1] = 0;
                lanConfig_joinname[l] = key;
            }
        }

        if (key < '0' || key > '9')
            break;
        if (lanConfig_cursor == 0)
        {
            l = strlen(lanConfig_portname);
            if (l < 5)
            {
                lanConfig_portname[l+1] = 0;
                lanConfig_portname[l] = key;
            }
        }
    }

    if (StartingGame && lanConfig_cursor == 2)
    {
        if ((key == K_UPARROW)||(key == K_DPAD1_UP))
            lanConfig_cursor = 1;
        else
            lanConfig_cursor = 0;
    }
    l =  Q_atoi(lanConfig_portname);
    if (l > 65535)
        l = lanConfig_port;
    else
        lanConfig_port = l;
    sprintf(lanConfig_portname, "%u", lanConfig_port);
}

//=============================================================================
/* SEARCH MENU */

qboolean	searchComplete = false;
double		searchCompleteTime;

void M_Search_f (void)
{
    key_dest = key_menu;
    m_state = m_search;
    m_entersound = false;
    slistSilent = true;
    slistLocal = false;
    searchComplete = false;
    NET_Slist_f();

}


void M_Search_Draw (void)
{
    int x;

    M_DrawPlaque ("gfx/p_multi.lmp", false); // Manoel Kasimier

    x = (/*320*/min_vid_width/2) - ((12*8)/2) + 4;
    M_DrawTextBox (x-8, 32, 12*8, 1*8);
    M_Print (x, 40, "Searching...");

    if(slistInProgress)
    {
        NET_Poll();
        return;
    }

    if (! searchComplete)
    {
        searchComplete = true;
        searchCompleteTime = realtime;
    }

    if (hostCacheCount)
    {
        M_ServerList_f ();
        return;
    }

    M_PrintWhite ((/*320*/min_vid_width/2) - ((22*8)/2), 64, "No Quake servers found");
    if ((realtime - searchCompleteTime) < 3.0)
        return;

    M_LanConfig_f ();
}


void M_Search_Key (int key)
{
}

//=============================================================================
/* SLIST MENU */

int		slist_cursor;
qboolean slist_sorted;

void M_ServerList_f (void)
{
    key_dest = key_menu;
    m_state = m_slist;
    m_entersound = true;
    slist_cursor = 0;
    m_return_onerror = false;
    m_return_reason[0] = 0;
    slist_sorted = false;
}


void M_ServerList_Draw (void)
{
    int		n;
    char	string [64];

    M_DrawPlaque ("gfx/p_multi.lmp", false); // Manoel Kasimier

    if (!slist_sorted)
    {
        if (hostCacheCount > 1)
        {
            int	i,j;
            hostcache_t temp;
            for (i = 0; i < hostCacheCount; i++)
                for (j = i+1; j < hostCacheCount; j++)
                    if (Q_strcmp(hostcache[j].name, hostcache[i].name) < 0)
                    {
                        memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
                        memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
                        memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
                    }
        }
        slist_sorted = true;
    }

    for (n = 0; n < hostCacheCount; n++)
    {
        if (hostcache[n].maxusers)
            sprintf(string, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
        else
            sprintf(string, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
        M_Print (16, 32 + 8*n, string);
    }
    M_DrawCharacter (0, 32 + slist_cursor*8, 12+((int)(realtime*4)&1));

    if (*m_return_reason)
        M_PrintWhite (16, 148, m_return_reason);
}


void M_ServerList_Key (int k)
{
    // Manoel Kasimier - begin
    if (m_inp_cancel)
        M_LanConfig_f ();
    // Manoel Kasimier - end
    switch (k)
    {
    case K_SPACE:
        M_Search_f ();
        break;

    case K_UPARROW:
    case K_DPAD1_UP:
    case K_LEFTARROW:
    case K_DPAD1_LEFT:
        S_LocalSound ("misc/menu1.wav");
        slist_cursor--;
        if (slist_cursor < 0)
            slist_cursor = hostCacheCount - 1;
        break;

    case K_DOWNARROW:
    case K_DPAD1_DOWN:
    case K_RIGHTARROW:
    case K_DPAD1_RIGHT:
        S_LocalSound ("misc/menu1.wav");
        slist_cursor++;
        if (slist_cursor >= hostCacheCount)
            slist_cursor = 0;
        break;

    case K_ENTER:
    case K_DC_A:
        S_LocalSound ("misc/menu2.wav");
        m_return_state = m_state;
        m_return_onerror = true;
        slist_sorted = false;
        // BlackAura - Menu background fading
        fade_level = 0;
        key_dest = key_game;
        m_state = m_none;
        Cbuf_AddText ( va ("connect \"%s\"\n", hostcache[slist_cursor].cname) );
        break;

    default:
        break;
    }

}

void M_ConfigureNetSubsystem(void)
{
// enable/disable net systems to match desired config

    Cbuf_AddText ("stopdemo\n");

    if (IPXConfig || TCPIPConfig)
        net_hostport = lanConfig_port;
}
#endif


//=============================================================================
/* Menu Subsystem */

// BlackAura (28-12-2002) - Fade menu background
void M_FadeBackground()
{
    Draw_FadeScreen ();
}

void M_Draw (void)
{
    scr_copyeverything = 1;

    if (m_state == m_none || key_dest != key_menu)
    {
        // BlackAura (28-12-2002) - Fade menu background
        if(fade_level > 0)
        {
            scr_copyeverything = 1;
            //	fade_level -= 1; // Manoel Kasimier - edited
            //	if(fade_level < 0)
            fade_level = 0;
            scr_fullupdate = 1;
            Draw_FadeScreen ();
        }

        return;
    }

    if (!m_recursiveDraw)
    {
        scr_copyeverything = 1;

        if (scr_con_current)
        {
            Draw_ConsoleBackground (vid.height);
            S_ExtraUpdate ();
        }
        else
            // BlackAura (28-12-2002) - Fade menu background
            M_FadeBackground();

        scr_fullupdate = 0;
    }
    else
    {
        m_recursiveDraw = false;
    }

    switch (m_state)
    {
    case m_none:
        break;

    case m_main:
        M_Main_Draw ();
        break;

    case m_singleplayer:
        M_SinglePlayer_Draw ();
        break;

    case m_loadsmall: // Manoel Kasimier - small saves
    case m_savesmall: // Manoel Kasimier - small saves
    case m_load:
    case m_save:
        M_Save_Draw ();
        break;

    case m_gameoptions:
        M_GameOptions_Draw ();
        break;

    case m_options:
        M_Options_Draw ();
        break;

    case m_setup:
        M_Setup_Draw ();
        break;

    case m_keys:
        M_Keys_Draw ();
        break;

// Manoel Kasimier - begin
    case m_controller:
        M_Controller_Draw ();
        break;

    case m_mouse:
        M_Mouse_Draw ();
        break;

    case m_gameplay:
        M_Gameplay_Draw ();
        break;

    case m_audio:
        M_Audio_Draw ();
        break;

    case m_developer:
        M_Developer_Draw ();
        break;

    case m_popup:
        M_PopUp_Draw ();
        break;

    case m_videomodes:
        (*vid_menudrawfn) ();
        break;

        // Manoel Kasimier - end

    case m_video:
        M_Video_Draw ();
        break;

    case m_credits: // Manoel Kasimier
    case m_help:
        M_Help_Draw ();
        break;

    case m_maplist:
        M_MapList_Draw ();
        break;

#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
    case m_multiplayer:
        M_MultiPlayer_Draw ();
        break;

    case m_net:
        M_Net_Draw ();
        break;

    case m_lanconfig:
        M_LanConfig_Draw ();
        break;

    case m_search:
        M_Search_Draw ();
        break;

    case m_slist:
        M_ServerList_Draw ();
        break;
#endif
    default:
        break;
    }
    if (m_entersound)
    {
        S_LocalSound ("misc/menu2.wav");
        m_entersound = false;
    }

    S_ExtraUpdate ();
}


void M_Keydown (int key)
{
    M_CheckInput (key); // sets input
    // Manoel Kasimier - menus which aren't turned off by m_inp_off
    switch (m_state)
    {
        // Manoel Kasimier - begin
    case m_popup: // must be verified before all other menus!
        M_PopUp_Key (key);
        return;

    case m_credits:
        M_Credits_Key (key);
        return;
        // Manoel Kasimier - end

    case m_setup:
        M_Setup_Key (key);
        return;

    case m_keys:
        M_Keys_Key (key);
        return;

   case m_maplist:
        M_MapList_Key (key);
        return;

#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
    case m_search:
        M_Search_Key (key);
        return;
#endif
    default:
        break;
    }

    // Manoel Kasimier - begin
    if (m_inp_off)
    {
        M_Off ();
        return;
    }
#ifdef _WIN32
    if (key == '-')
    {
        Cbuf_AddText ("vid_minimize\n");
        return;
    }
#endif
    // Manoel Kasimier - end

    switch (m_state)
    {
    case m_none:
        return;

    case m_main:
        M_Main_Key (key);
        return;

    case m_singleplayer:
        M_SinglePlayer_Key (key);
        return;

    case m_loadsmall: // Manoel Kasimier - small saves
    case m_savesmall: // Manoel Kasimier - small saves
    case m_load:
    case m_save:
        M_Save_Key (key);
        return;

    case m_options:
        M_Options_Key (key);
        return;

// Manoel Kasimier - begin
    case m_controller:
        M_Controller_Key (key);
        return;

    case m_mouse:
        M_Mouse_Key (key);
        return;

    case m_gameplay:
        M_Gameplay_Key (key);
        return;

    case m_audio:
        M_Audio_Key (key);
        return;

    case m_developer:
        M_Developer_Key (key);
        return;

    case m_videomodes:
        (*vid_menukeyfn) (key);
        return;

    case m_video:
        M_Video_Key (key);
        return;

    case m_help:
        M_Help_Key (key);
        return;

    case m_gameoptions:
        M_GameOptions_Key (key);
        return;
#if NET_MENUS // Manoel Kasimier - removed multiplayer menus
    case m_multiplayer:
        M_MultiPlayer_Key (key);
        return;

    case m_net:
        M_Net_Key (key);
        return;

    case m_lanconfig:
        M_LanConfig_Key (key);
        return;

    case m_slist:
        M_ServerList_Key (key);
        return;
#endif
    default:
        break;
    }
}


void M_Init (void)
{
    // Manoel Kasimier - setting default menu items - begin
    int i;

    M_Precache(); // Manoel Kasimier - precaching menu pics
    M_Keys_SetDefaultCmds();
    if (hipnotic)
        M_SetDefaultEpisodes(hipnoticepisodes, 5, hipnoticlevels, 17);
    else if (rogue)
        M_SetDefaultEpisodes(rogueepisodes, 3, roguelevels, 16);
    else
    {
        if (registered.value)
            M_SetDefaultEpisodes(id1episodes, 6, id1levels, 37);
        else
            M_SetDefaultEpisodes(id1episodes, 1, id1levels, 8);
    }
    // Manoel Kasimier - setting default menu items - end

    // Manoel Kasimier - registering stuff - begin
    Cvar_RegisterVariable (&help_pages);
    Cvar_RegisterVariableWithCallback (&savename, SetSavename);

    for (i=0 ; i<MAX_SAVEGAMES ; i++)
        m_fileinfo[i] = NULL;

    dont_bind['~'] = true;
    dont_bind['`'] = true;
    Cmd_AddCommand ("menu_keys_clear", M_Keys_Clear);
    Cmd_AddCommand ("menu_keys_addcmd", M_Keys_AddCmd);
    Cmd_AddCommand ("bindable", M_Keys_Bindable);
    Cmd_AddCommand ("menu_clearmaps", M_Maps_Clear);
    Cmd_AddCommand ("menu_addmap", M_AddMap);
    Cmd_AddCommand ("menu_addepisode", M_AddEpisode);
    Cmd_AddCommand ("menu_loadsmall", M_LoadSmall_f);
    Cmd_AddCommand ("menu_savesmall", M_SaveSmall_f);
    // Manoel Kasimier - registering stuff - end

    Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

    Cmd_AddCommand ("menu_main", M_Main_f);
    Cmd_AddCommand ("menu_singleplayer", M_SinglePlayer_f);
    Cmd_AddCommand ("menu_load", M_Load_f);
    Cmd_AddCommand ("menu_save", M_Save_f);

    Cmd_AddCommand ("menu_multiplayer", M_GameOptions_f); // edited by Manoel Kasimier
    Cmd_AddCommand ("menu_setup", M_Setup_f);
    Cmd_AddCommand ("menu_options", M_Options_f);
    Cmd_AddCommand ("menu_keys", M_Keys_f);
    Cmd_AddCommand ("menu_video", M_Video_f);
    Cmd_AddCommand ("menu_developer", M_Developer_f); // Manoel Kasimier
    Cmd_AddCommand ("help", M_Help_f);
    Cmd_AddCommand ("menu_quit", M_Quit_f);
    Cmd_AddCommand ("menu_maplist", M_Menu_MapList_f);
}
