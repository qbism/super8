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
// vid_win.c -- Win32 video driver  //qbism - based on MGL-less version by MH

#include "../quakedef.h"
#include "winquake.h"
#include "../d_local.h"
#include "resource.h"
int min_vid_width=320; //qbism- Dan East (for very low-res display)

// compatibility
qboolean		DDActive;


#define MAX_MODE_LIST	30
#define VID_ROW_SIZE	3
#define VID_WINDOWED_MODES 3 //qbism

extern int		Minimized;

HWND		mainwindow;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

static DEVMODE	gdevmode;
static qboolean	startwindowed = 0, windowed_mode_set;
static int		firstupdate = 1;
static qboolean	vid_initialized = false, vid_palettized;
static int		vid_fulldib_on_focus_mode;
static qboolean	force_minimized, in_mode_set, is_mode0x13, force_mode_set;
static int		windowed_mouse;
static qboolean	palette_changed, syscolchg, vid_mode_set, hide_window, pal_is_nostatic;
static HICON	hIcon;

viddef_t	vid;				// global video state

#define MODE_WINDOWED			0
#define MODE_SETTABLE_WINDOW	2
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + VID_WINDOWED_MODES) //qbism was 3

// Note that 0 is MODE_WINDOWED
cvar_t		vid_mode = {"vid_mode", "0", false};
// Note that 0 is MODE_WINDOWED
cvar_t		_vid_default_mode = {"_vid_default_mode", "0", true};
// Note that 3 is MODE_FULLSCREEN_DEFAULT
cvar_t		_vid_default_mode_win = {"_vid_default_mode_win", "3", true};
cvar_t		vid_wait = {"vid_wait", "0"};
cvar_t		vid_nopageflip = {"vid_nopageflip", "0", true};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t		vid_config_x = {"vid_config_x", "800", true};
cvar_t		vid_config_y = {"vid_config_y", "600", true};
cvar_t		vid_stretch_by_2 = {"vid_stretch_by_2", "0", true};
cvar_t		_windowed_mouse = {"_windowed_mouse", "1", true};
cvar_t		vid_fullscreen_mode = {"vid_fullscreen_mode", "3", true};
cvar_t		vid_windowed_mode = {"vid_windowed_mode", "0", true};
cvar_t		block_switch = {"block_switch", "0", true};
cvar_t		vid_window_x = {"vid_window_x", "0", true};
cvar_t		vid_window_y = {"vid_window_y", "0", true};

int			vid_modenum = NO_MODE;
int			vid_testingmode, vid_realmode;
double		vid_testendtime;
int			vid_default = MODE_WINDOWED;
static int	windowed_default;

modestate_t	modestate = MS_UNINIT;

static byte		*vid_surfcache;
static int		vid_surfcachesize;
static int		VID_highhunkmark;

unsigned char	vid_curpal[256*3];

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];

int     mode;

typedef struct
{
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			fullscreen;
	char		modedesc[13];
} vmode_t;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;
static vmode_t	*pcurrentmode;

int		aPage;					// Current active display page
int		vPage;					// Current visible display page
int		waitVRT = true;			// True to wait for retrace on flip

static vmode_t	badmode;

static byte	backingbuf[48*24];

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate (BOOL fActive, BOOL minimize);

// video commands
int VID_NumModes (void);
vmode_t *VID_GetModePtr (int modenum);
void VID_TestMode_f (void);
void VID_ForceMode_f (void);
void VID_Minimize_f (void);
void VID_Fullscreen_f (void);
void VID_Windowed_f (void);
void VID_DescribeModes_f (void);
void VID_DescribeMode_f (void);
void VID_NumModes_f (void);
void VID_DescribeCurrentMode_f (void);
char *VID_GetExtModeDescription (int mode);
char *VID_GetModeDescription (int mode);
char *VID_GetModeDescription2 (int mode);
char *VID_GetModeDescriptionMemCheck (int mode);
void VID_CheckModedescFixup (int mode);



typedef struct dibinfo
{
	BITMAPINFOHEADER	header;
	RGBQUAD				acolors[256];
} dibinfo_t;


static HGDIOBJ previously_selected_GDI_obj = NULL;
HBITMAP hDIBSection;
unsigned char *pDIBBase = NULL;
HDC hdcDIBSection = NULL;
HDC maindc = NULL;


void VID_ShutdownDIB (void)
{
	if (hdcDIBSection)
	{
		SelectObject (hdcDIBSection, previously_selected_GDI_obj);
		DeleteDC (hdcDIBSection);
		hdcDIBSection = NULL;
	}

	if (hDIBSection)
	{
		DeleteObject (hDIBSection);
		hDIBSection = NULL;
		pDIBBase = NULL;
	}

	if (maindc)
	{
		// if maindc exists mainwindow must also be valid
		ReleaseDC (mainwindow, maindc);
		maindc = NULL;
	}
}


void VID_CreateDIB (int width, int height, unsigned char *palette, qboolean windowed)
{
	dibinfo_t   dibheader;
	BITMAPINFO *pbmiDIB = (BITMAPINFO *) &dibheader;
	int i;

	maindc = GetDC (mainwindow);
	memset (&dibheader, 0, sizeof (dibheader));

	// fill in the bitmap info
	pbmiDIB->bmiHeader.biSize          = sizeof (BITMAPINFOHEADER);
	pbmiDIB->bmiHeader.biWidth         = width;
	pbmiDIB->bmiHeader.biHeight        = -height;	// top-down
	pbmiDIB->bmiHeader.biPlanes        = 1;
	pbmiDIB->bmiHeader.biBitCount      = 8;
	pbmiDIB->bmiHeader.biCompression   = BI_RGB;
	pbmiDIB->bmiHeader.biSizeImage     = 0;
	pbmiDIB->bmiHeader.biXPelsPerMeter = 0;
	pbmiDIB->bmiHeader.biYPelsPerMeter = 0;
	pbmiDIB->bmiHeader.biClrUsed       = 256;
	pbmiDIB->bmiHeader.biClrImportant  = 256;

	// create the DIB section
	hDIBSection = CreateDIBSection (maindc,
							pbmiDIB,
							DIB_RGB_COLORS,
							&pDIBBase,
							NULL,
							0);

	// set video buffers
	if (pbmiDIB->bmiHeader.biHeight > 0)
	{
		// bottom up
		vid.buffer = pDIBBase + (height - 1) * width;
		vid.rowbytes = -width;
	}
	else
	{
		// top down
		vid.buffer = pDIBBase;
		vid.rowbytes = width; //qbism was vid.width
	}

	// set the rest of the buffers we need (why not just use one single buffer instead of all this crap? oh well, it's Quake...)
	vid.conbuffer = vid.direct = vid.buffer;

	// more crap for the console
	vid.conrowbytes = vid.rowbytes;

	// clear the buffer
	memset (pDIBBase, 0xff, width * height);

	if ((hdcDIBSection = CreateCompatibleDC (maindc)) == NULL)
		Sys_Error ("DIB_Init() - CreateCompatibleDC failed\n");

	if ((previously_selected_GDI_obj = SelectObject (hdcDIBSection, hDIBSection)) == NULL)
		Sys_Error ("DIB_Init() - SelectObject failed\n");
}


/*
================
VID_RememberWindowPos
================
*/
void VID_RememberWindowPos (void)
{
	RECT	rect;

	if (GetWindowRect (mainwindow, &rect))
	{
		if ((rect.left < GetSystemMetrics (SM_CXSCREEN)) &&
				(rect.top < GetSystemMetrics (SM_CYSCREEN))  &&
				(rect.right > 0)                             &&
				(rect.bottom > 0))
		{
			Cvar_SetValue ("vid_window_x", (float) rect.left);
			Cvar_SetValue ("vid_window_y", (float) rect.top);
		}
	}
}


/*
================
VID_CheckWindowXY
================
*/
void VID_CheckWindowXY (void)
{
	if (((int) vid_window_x.value > (GetSystemMetrics (SM_CXSCREEN) - 160)) ||
			((int) vid_window_y.value > (GetSystemMetrics (SM_CYSCREEN) - 120)) ||
			((int) vid_window_x.value < 0)									   ||
			((int) vid_window_y.value < 0))
	{
		Cvar_SetValue ("vid_window_x", 0.0);
		Cvar_SetValue ("vid_window_y", 0.0);
	}
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;
	IN_UpdateClipCursor ();
}


/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int		i;

	// send an up event for each key, to make sure the server clears them all
	for (i = 0; i < 256; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}


/*
================
VID_CheckAdequateMem
================
*/
qboolean VID_CheckAdequateMem (int width, int height)
{
	// there will always be enough ;)
	return true;
}


/*
================
VID_AllocBuffers
================
*/
qboolean VID_AllocBuffers (int width, int height)
{
	int		tsize, tbuffersize;

	tbuffersize = width * height * sizeof (*d_pzbuffer);
	tsize = D_SurfaceCacheForRes (width, height);
	tbuffersize += tsize;

	vid_surfcachesize = tsize;

	if (d_pzbuffer)
	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (VID_highhunkmark);
		d_pzbuffer = NULL;
	}

	VID_highhunkmark = Hunk_HighMark ();
	d_pzbuffer = Hunk_HighAllocName (tbuffersize, "video");
	vid_surfcache = (byte *) d_pzbuffer +
					width * height * sizeof (*d_pzbuffer);

	return true;
}


void initFatalError (void)
{
	exit (EXIT_FAILURE);
}


void registerAllMemDrivers (void)
{
}


void VID_InitModes (HINSTANCE hInstance)
{
	WNDCLASS		wc;
	HDC				hdc;
	int				i;
	hIcon = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON2));
	/* Register the frame class */
	wc.style         = CS_OWNDC;
	wc.lpfnWndProc   = (WNDPROC) MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "qbismSuper8"; //qbism

	if (!RegisterClass (&wc))
		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;
	modelist[0].width = 800;
	modelist[0].height = 480;
	strcpy (modelist[0].modedesc, "800x480");
	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].fullscreen = 0;

	modelist[1].type = MS_WINDOWED;
	modelist[1].width = 1024;
	modelist[1].height = 600;
	strcpy (modelist[1].modedesc, "1024x600");
	modelist[1].modenum = MODE_WINDOWED + 1;
	modelist[1].fullscreen = 0;

	modelist[2].type = MS_WINDOWED;
	modelist[2].width = 1280; //qbism
	modelist[2].height = 720;
	strcpy (modelist[2].modedesc, "custom window");
	modelist[2].modenum = MODE_WINDOWED + 2;
	modelist[2].fullscreen = 0;

	// automatically stretch the default mode up if > 640x480 desktop resolution
	hdc = GetDC (NULL);

	if ((GetDeviceCaps (hdc, HORZRES) > modelist[1].width) && !COM_CheckParm ("-noautostretch"))  //qbism - was 800
	{
		vid_default = MODE_WINDOWED + 2;
	}
	else if ((GetDeviceCaps (hdc, HORZRES) > modelist[0].width) && !COM_CheckParm ("-noautostretch"))  //qbism - was 640
	{
		vid_default = MODE_WINDOWED + 1;
	}
	else
	{
		vid_default = MODE_WINDOWED;
	}

	windowed_default = vid_default;
	ReleaseDC (NULL, hdc);
	nummodes = VID_WINDOWED_MODES;	// reserve space for windowed mode  //qbism was 3
}


/*
=================
VID_GetDisplayModes
=================
*/
void VID_GetDisplayModes (void)
{
	DEVMODE	devmode;
	int		i, j, modenum, cmodes, existingmode, originalnummodes, lowestres;
	int		numlowresmodes, bpp, done;
	int		cstretch, istretch, mstretch;
	BOOL	stat;

	// enumerate > 8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;
	lowestres = 99999;

	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmPelsWidth <= MAXWIDTH) &&
				(devmode.dmPelsHeight <= MAXHEIGHT) &&
				(devmode.dmPelsWidth >= 320) && //qbism was 640
				(devmode.dmPelsHeight >= 200) && //qbism was 480
				(nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL |
							   DM_PELSWIDTH |
							   DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].fullscreen = 1;
				sprintf (modelist[nummodes].modedesc, "%dx%d",
						 devmode.dmPelsWidth, devmode.dmPelsHeight);

				// see is the mode already there
				// (same dimensions but different refresh rate)
				for (i = originalnummodes, existingmode = 0; i < nummodes; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width) &&
							(modelist[nummodes].height == modelist[i].height))
					{
						existingmode = 1;
						break;
					}
				}

				// if it's not add it to the list
				if (!existingmode)
				{
					if (modelist[nummodes].width < lowestres)
						lowestres = modelist[nummodes].width;

					nummodes++;
				}
			}
		}

		modenum++;
	}
	while (stat);

	if (nummodes != originalnummodes)
		vid_default = MODE_FULLSCREEN_DEFAULT;
	else Con_Printf ("No fullscreen DIB modes found\n");
}


void VID_DestroyWindow (void)
{
	if (modestate == MS_FULLDIB)
		ChangeDisplaySettings (NULL, CDS_FULLSCREEN);

	VID_ShutdownDIB ();
}


qboolean VID_SetWindowedMode (int modenum)
{
	HDC				hdc;
	//qbism not used qboolean		stretched;
	int				lastmodestate;
	LONG			wlong;

	if (!windowed_mode_set)
	{
		if (COM_CheckParm ("-resetwinpos"))
		{
			Cvar_SetValue ("vid_window_x", 0.0);
			Cvar_SetValue ("vid_window_y", 0.0);
		}

		windowed_mode_set;
	}

	VID_CheckModedescFixup (modenum);
	lastmodestate = modestate;
	VID_DestroyWindow ();

	WindowRect.top = WindowRect.left = 0;
	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;
	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;
	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
				  WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPSIBLINGS |
				  WS_CLIPCHILDREN;
	ExWindowStyle = 0;
	AdjustWindowRectEx (&WindowRect, WindowStyle, FALSE, 0);

	// the first time we're called to set the mode, create the window we'll use
	// for the rest of the session
	if (!vid_mode_set)
	{
		mainwindow = CreateWindowEx (
						 ExWindowStyle,
			 "qbismSuper8",
			 "qbismSuper8", //qbism edited
						 WindowStyle,
						 0, 0,
						 WindowRect.right - WindowRect.left,
						 WindowRect.bottom - WindowRect.top,
						 NULL,
						 NULL,
						 global_hInstance,
						 NULL);

		if (!mainwindow)
			Sys_Error ("Couldn't create DIB window");

		vid_mode_set = true;
	}
	else
	{
		SetWindowLong (mainwindow, GWL_STYLE, WindowStyle | WS_VISIBLE);
		SetWindowLong (mainwindow, GWL_EXSTYLE, ExWindowStyle);
	}

	if (!SetWindowPos (mainwindow,
					   NULL,
					   0, 0,
					   WindowRect.right - WindowRect.left,
					   WindowRect.bottom - WindowRect.top,
					   SWP_NOCOPYBITS | SWP_NOZORDER |
					   SWP_HIDEWINDOW))
	{
		Sys_Error ("Couldn't resize DIB window");
	}

	if (hide_window)
		return true;

	// position and show the DIB window
	VID_CheckWindowXY ();
	SetWindowPos (mainwindow, NULL, (int) vid_window_x.value,
				  (int) vid_window_y.value, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	if (force_minimized)
		ShowWindow (mainwindow, SW_MINIMIZE);
	else
		ShowWindow (mainwindow, SW_SHOWDEFAULT);

	UpdateWindow (mainwindow);
	modestate = MS_WINDOWED;
	vid_fulldib_on_focus_mode = 0;

	vid.numpages = 1;

//	vid.maxwarpwidth = WARP_WIDTH; //qbism from Manoel Kasimier - hi-res waterwarp - removed
//	vid.maxwarpheight = WARP_HEIGHT; //qbism from Manoel Kasimier - hi-res waterwarp - removed
	vid.height = vid.conheight = DIBHeight;
	vid.width = vid.conwidth = DIBWidth;
	vid.maxwarpwidth = vid.width; //qbism from  Manoel Kasimier - hi-res waterwarp
	vid.maxwarpheight = vid.height; //qbism from  Manoel Kasimier - hi-res waterwarp

	vid.aspect = ((float) vid.height / (float) vid.width) *
				 (320.0 / 240.0);

	SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);

	return true;
}


qboolean VID_SetFullDIBMode (int modenum)
{
	HDC				hdc;
	int				lastmodestate;
	VID_DestroyWindow ();

	gdevmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
	gdevmode.dmPelsWidth = modelist[modenum].width;
	gdevmode.dmPelsHeight = modelist[modenum].height;
	gdevmode.dmSize = sizeof (gdevmode);

	if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		Sys_Error ("Couldn't set fullscreen DIB mode");

	lastmodestate = modestate;
	modestate = MS_FULLDIB;
	vid_fulldib_on_focus_mode = modenum;
	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_POPUP | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	ExWindowStyle = 0;

	AdjustWindowRectEx (&WindowRect, WindowStyle, FALSE, 0);

	SetWindowLong (mainwindow, GWL_STYLE, WindowStyle | WS_VISIBLE);
	SetWindowLong (mainwindow, GWL_EXSTYLE, ExWindowStyle);

	if (!SetWindowPos (mainwindow,
					   NULL,
					   0, 0,
					   WindowRect.right - WindowRect.left,
					   WindowRect.bottom - WindowRect.top,
					   SWP_NOCOPYBITS | SWP_NOZORDER))
	{
		Sys_Error ("Couldn't resize DIB window");
	}

	// position and show the DIB window
	SetWindowPos (mainwindow, HWND_TOPMOST, 0, 0, 0, 0,
				  SWP_NOSIZE | SWP_SHOWWINDOW | SWP_DRAWFRAME);
	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	vid.numpages = 1;
//	vid.maxwarpwidth = WARP_WIDTH; //qbism from Manoel Kasimier - hi-res waterwarp - removed
//	vid.maxwarpheight = WARP_HEIGHT; //qbism from Manoel Kasimier - hi-res waterwarp - removed
	vid.height = vid.conheight = DIBHeight;
	vid.width = vid.conwidth = DIBWidth;
	vid.maxwarpwidth = vid.width; //qbism from  Manoel Kasimier - hi-res waterwarp
	vid.maxwarpheight = vid.height; //qbism from  Manoel Kasimier - hi-res waterwarp
	vid.aspect = ((float) vid.height / (float) vid.width) *
				 (320.0 / 240.0);

	// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	return true;
}


void VID_RestoreOldMode (int original_mode)
{
	static qboolean	inerror = false;

	if (inerror)
		return;

	in_mode_set = false;
	inerror = true;
	// make sure mode set happens (video mode changes)
	vid_modenum = original_mode - 1;

	if (!VID_SetMode (original_mode, vid_curpal))
	{
		vid_modenum = MODE_WINDOWED - 1;

		if (!VID_SetMode (windowed_default, vid_curpal))
			Sys_Error ("Can't set any video mode");
	}

	inerror = false;
}


void VID_SetDefaultMode (void)
{
	if (vid_initialized)
		VID_SetMode (0, vid_curpal);

	IN_DeactivateMouse ();
}


int VID_SetMode (int modenum, unsigned char *palette)
{
	int				original_mode, temp, dummy;
	qboolean		stat;
	MSG				msg;
	HDC				hdc;

	while ((modenum >= nummodes) || (modenum < 0))
	{
		if (vid_modenum == NO_MODE)
		{
			if (modenum == vid_default)
			{
				modenum = windowed_default;
			}
			else
			{
				modenum = vid_default;
			}

			Cvar_SetValue ("vid_mode", (float) modenum);
		}
		else
		{
			Cvar_SetValue ("vid_mode", (float) vid_modenum);
			return 0;
		}
	}

	if (!force_mode_set && (modenum == vid_modenum))
		return true;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	in_mode_set = true;
	CDAudio_Pause ();
	S_ClearBuffer ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (key_dest != key_menu && !cl.paused) // Manoel Kasimier - edited
		{
			stat = VID_SetWindowedMode (modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode (modenum);
		}
	}
	else
	{
		stat = VID_SetFullDIBMode (modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}

	// create the DIB
	VID_CreateDIB (DIBWidth, DIBHeight, palette, (modelist[modenum].type == MS_WINDOWED));

	window_width = vid.width;
	window_height = vid.height;
	VID_UpdateWindowStatus ();
	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
	{
		VID_RestoreOldMode (original_mode);
		return false;
	}

	if (hide_window)
		return true;

	// now we try to make sure we get the focus on the mode switch, because
	// sometimes in some systems we don't.  We grab the foreground, then
	// finish setting up, pump all our messages, and sleep for a little while
	// to let messages finish bouncing around the system, then we put
	// ourselves at the top of the z order, then grab the foreground again,
	// Who knows if it helps, but it probably doesn't hurt
	if (!force_minimized)
		SetForegroundWindow (mainwindow);

	hdc = GetDC (NULL);

	if (GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE)
		vid_palettized = true;
	else
		vid_palettized = false;

	VID_SetPalette (palette);
	ReleaseDC (NULL, hdc);
	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float) vid_modenum);

	if (vid.width<320) min_vid_width=vid.width; //qbism- Dan East
	else min_vid_width=320;

	if (!VID_AllocBuffers (vid.width, vid.height))
	{
		// couldn't get memory for this mode; try to fall back to previous mode
		VID_RestoreOldMode (original_mode);
		return false;
	}

	D_InitCaches (vid_surfcache, vid_surfcachesize);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	if (!force_minimized)
	{
		SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
					  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
					  SWP_NOCOPYBITS);
		SetForegroundWindow (mainwindow);
	}

	// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	if (!msg_suppress_1)
		Con_Printf ("Video mode: %s\n", VID_GetModeDescription (vid_modenum));  //qbism edited

	VID_SetPalette (palette);
	in_mode_set = false;
	vid.recalc_refdef = 1;
	return true;
}


void VID_SetPalette (unsigned char *palette)
{
	INT			i;
	//HDC			hdc;
	RGBQUAD			colors[256];
	unsigned char *pal = palette;
	byte *table = (byte *) d_8to24table;

	if (!Minimized)
	{
		if (hdcDIBSection)
		{
		// incoming palette is 3 component
			for (i = 0; i < 256; i++, pal += 3)
		{
				colors[i].rgbRed   = pal[0];
				colors[i].rgbGreen = pal[1];
				colors[i].rgbBlue  = pal[2];
				colors[i].rgbReserved = 0;
			}

			//qbism - no, thanks!   colors[0].rgbRed = 0;
			//qbism - no, thanks!   colors[0].rgbGreen = 0;
			//qbism - no, thanks!   colors[0].rgbBlue = 0;
			colors[255].rgbRed = 0xff;
			colors[255].rgbGreen = 0xff;
			colors[255].rgbBlue = 0xff;

			if (SetDIBColorTable (hdcDIBSection, 0, 256, colors) == 0)
			{
				Con_Printf ("DIB_SetPalette() - SetDIBColorTable failed\n");
			}
		}
	}

	memcpy (vid_curpal, palette, sizeof (vid_curpal));
}


void	VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette (palette);
}


void	VID_Init (unsigned char *palette)
{
	int		i, bestmatch, bestmatchmetric, t, dr, dg, db;
	int		basenummodes;
	byte	*ptmp;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&vid_nopageflip);
	Cvar_RegisterVariable (&_vid_wait_override);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&_windowed_mouse);
	Cvar_RegisterVariable (&vid_fullscreen_mode);
	Cvar_RegisterVariable (&vid_windowed_mode);
	Cvar_RegisterVariable (&block_switch);
	Cvar_RegisterVariable (&vid_window_x);
	Cvar_RegisterVariable (&vid_window_y);
	Cmd_AddCommand ("vid_testmode", VID_TestMode_f);
	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
	Cmd_AddCommand ("vid_forcemode", VID_ForceMode_f);
	Cmd_AddCommand ("vid_windowed", VID_Windowed_f);
	Cmd_AddCommand ("vid_fullscreen", VID_Fullscreen_f);
	Cmd_AddCommand ("vid_minimize", VID_Minimize_f);
	VID_InitModes (global_hInstance);
	basenummodes = nummodes;
	VID_GetDisplayModes ();
	//vid.maxwarpwidth = WARP_WIDTH;
	//vid.maxwarpheight = WARP_HEIGHT;
    vid.maxwarpwidth = vid.width; //qbism from  Manoel Kasimier - hi-res waterwarp
	vid.maxwarpheight = vid.height; //qbism from  Manoel Kasimier - hi-res waterwarp

	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));
	vid_testingmode = 0;
	// GDI doesn't let us remap palette index 0, so we'll remap color
	// mappings from that black to another one
	bestmatchmetric = 256 * 256 * 3;

	for (i = 1; i < 256; i++)
	{
		dr = palette[0] - palette[i*3];
		dg = palette[1] - palette[i*3+1];
		db = palette[2] - palette[i*3+2];
		t = (dr * dr) + (dg * dg) + (db * db);

		if (t < bestmatchmetric)
		{
			bestmatchmetric = t;
			bestmatch = i;

			if (t == 0)
				break;
		}
	}

	for (i = 0, ptmp = vid.colormap; i < (1 << (VID_CBITS + 8)); i++, ptmp++)
	{
		if (*ptmp == 0)
			*ptmp = bestmatch;
	}

	if (!COM_CheckParm("-nostartwindowed"))
	{
		startwindowed = 1;
		vid_default = windowed_default;
	}

	if (hwnd_dialog)
		DestroyWindow (hwnd_dialog);

	// sound initialization has to go here, preceded by a windowed mode set,
	// so there's a window for DirectSound to work with but we're not yet
	// fullscreen so the "hardware already in use" dialog is visible if it
	// gets displayed
	// keep the window minimized until we're ready for the first real mode set
	hide_window = true;
	VID_SetMode (MODE_WINDOWED, palette);
	hide_window = false;
	S_Init ();
	vid_initialized = true;
	//force_mode_set = true;
	//qbism VID_SetMode (vid_default, palette);
	//force_mode_set = false;
	vid_realmode = vid_modenum;
	VID_SetPalette (palette);
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;
	strcpy (badmode.modedesc, "Bad mode");
}


void VID_Shutdown (void)
{
	HDC				hdc;
	int				dummy;

	if (vid_initialized)
	{
		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, CDS_FULLSCREEN);

		PostMessage (HWND_BROADCAST, WM_PALETTECHANGED, (WPARAM) mainwindow, (LPARAM) 0);
		PostMessage (HWND_BROADCAST, WM_SYSCOLORCHANGE, (WPARAM) 0, (LPARAM) 0);
		AppActivate (false, false);

		VID_DestroyWindow ();

		if (hwnd_dialog) DestroyWindow (hwnd_dialog);
		if (mainwindow) DestroyWindow (mainwindow);

		vid_testingmode = 0;
		vid_initialized = 0;
	}
}


/*
================
FlipScreen
================
*/
void FlipScreen (vrect_t *rects)
{
	if (hdcDIBSection)
	{
		int numrects = 0;

		while (rects)
		{
			BitBlt (maindc,
					rects->x, rects->y,
					rects->x + rects->width,
					rects->y + rects->height,
					hdcDIBSection,
					rects->x, rects->y,
					SRCCOPY);

			numrects++;
			rects = rects->pnext;
		}
	}
}


//qbism removed D_BeginDirectRect and D_EndDirectRect

void VID_Update (vrect_t *rects)
{
	vrect_t	rect;
	RECT	trect;

	if (!vid_palettized && palette_changed)
	{
		palette_changed = false;
		rect.x = 0;
		rect.y = 0;
		rect.width = vid.width;
		rect.height = vid.height;
		rect.pnext = NULL;
		rects = &rect;
	}

	if (firstupdate)
	{
		if (modestate == MS_WINDOWED)
		{
			GetWindowRect (mainwindow, &trect);

			if ((trect.left != (int) vid_window_x.value) ||
					(trect.top  != (int) vid_window_y.value))
			{
				if (COM_CheckParm ("-resetwinpos"))
				{
					Cvar_SetValue ("vid_window_x", 0.0);
					Cvar_SetValue ("vid_window_y", 0.0);
				}

				VID_CheckWindowXY ();
				SetWindowPos (mainwindow, NULL, (int) vid_window_x.value,
							  (int) vid_window_y.value, 0, 0,
							  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
			}
		}

		if ((_vid_default_mode_win.value != vid_default) &&
				(!startwindowed || (_vid_default_mode_win.value < MODE_FULLSCREEN_DEFAULT)))
		{
			firstupdate = 0;

			if (COM_CheckParm ("-resetwinpos"))
			{
				Cvar_SetValue ("vid_window_x", 0.0);
				Cvar_SetValue ("vid_window_y", 0.0);
			}

			if ((_vid_default_mode_win.value < 0) ||
					(_vid_default_mode_win.value >= nummodes))
			{
				Cvar_SetValue ("_vid_default_mode_win", windowed_default);
			}

			Cvar_SetValue ("vid_mode", _vid_default_mode_win.value);
		}
	}

	// We've drawn the frame; copy it to the screen
	FlipScreen (rects);

	if (vid_testingmode)
	{
		if (realtime >= vid_testendtime)
		{
			VID_SetMode (vid_realmode, vid_curpal);
			vid_testingmode = 0;
		}
	}
	else
	{
		if ((int) vid_mode.value != vid_realmode)
		{
			VID_SetMode ((int) vid_mode.value, vid_curpal);
			Cvar_SetValue ("vid_mode", (float) vid_modenum);
			// so if mode set fails, we don't keep on
			//  trying to set that mode
			vid_realmode = vid_modenum;
		}
	}

	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if ((int) _windowed_mouse.value != windowed_mouse)
		{
			if (key_dest != key_menu && !cl.paused) // Manoel Kasimier - edited
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}

//			windowed_mouse = _windowed_mouse; // Manoel Kasimier - edited
		}
	}
}


//==========================================================================

byte        scantokey[128] =
{
	//  0           1       2       3       4       5       6       7
	//  8           9       A       B       C       D       E       F
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    13 ,    K_CTRL, 'a',  's',     // 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'' ,    '`',    K_SHIFT, '\\',  'z',    'x',    'c',    'v',     // 2
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT, '*',
	K_ALT, ' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,  // 3
	K_F6, K_F7, K_F8, K_F9, K_F10,  K_PAUSE,    0  , K_HOME,
	K_UPARROW, K_PGUP, '-', K_LEFTARROW, '5', K_RIGHTARROW, '+', K_END, //4
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0,             0,              K_F11,
	K_F12, 0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,       // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7
};

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	key = (key >> 16) & 255;

	if (key > 127)
		return 0;

	return scantokey[key];
}

void AppActivate (BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	HDC			hdc;
	int			i, t;
	static BOOL	sound_active;
	ActiveApp = fActive;

	// messy, but it seems to work
	if (vid_fulldib_on_focus_mode)
	{
		Minimized = minimize;

		if (Minimized)
			ActiveApp = false;
	}

	if (vid_initialized)
	{
		// yield the palette if we're losing the focus
		hdc = GetDC (NULL);

		if (!Minimized)
			VID_SetPalette (vid_curpal);

		scr_fullupdate = 0;
		ReleaseDC (NULL, hdc);
	}

	// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		S_ClearBuffer ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		S_ClearBuffer ();
		sound_active = true;
	}

	// minimize/restore fulldib windows/mouse-capture normal windows on demand
	if (!in_mode_set)
	{
		if (ActiveApp)
		{
			if (vid_fulldib_on_focus_mode)
			{
				if (vid_initialized)
				{
					msg_suppress_1 = true;	// don't want to see normal mode set message
					VID_SetMode (vid_fulldib_on_focus_mode, vid_curpal);
					msg_suppress_1 = false;
					t = in_mode_set;
					in_mode_set = true;
					AppActivate (true, false);
					in_mode_set = t;
				}

				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
		}

		if (!ActiveApp)
		{
			if (modestate == MS_FULLDIB)
			{
				if (vid_initialized)
				{
					force_minimized = true;
					i = vid_fulldib_on_focus_mode;
					msg_suppress_1 = true;	// don't want to see normal mode set message
					VID_SetMode (windowed_default, vid_curpal);
					msg_suppress_1 = false;
					vid_fulldib_on_focus_mode = i;
					force_minimized = false;
					// we never seem to get WM_ACTIVATE inactive from this mode set, so we'll
					// do it manually
					t = in_mode_set;
					in_mode_set = true;
					AppActivate (false, true);
					in_mode_set = t;
				}

				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
			else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
}


/*
================
VID_HandlePause
================
*/
void VID_HandlePause (qboolean pause)
{
	if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
	{
		if (pause)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
		else
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}
}


/*
===================================================================

MAIN WINDOW

===================================================================
*/

LONG CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* main window procedure */
LONG WINAPI MainWndProc (
	HWND    hWnd,
	UINT    uMsg,
	WPARAM  wParam,
	LPARAM  lParam)
{
	LONG			lRet = 0;
	int				fwKeys, xPos, yPos, fActive, fMinimized, temp;
	HDC				hdc;
	PAINTSTRUCT		ps;
	static int		recursiveflag;

	switch (uMsg)
	{
	case WM_CREATE:
		break;
	case WM_SYSCOMMAND:

		// Check for maximize being hit
		switch (wParam & ~0x0F)
		{
		case SC_MAXIMIZE:

			// if minimized, bring up as a window before going fullscreen,
			// so will have the right state to restore
			if (Minimized)
			{
				force_mode_set = true;
				VID_SetMode (vid_modenum, vid_curpal);
				force_mode_set = false;
			}

			VID_SetMode ((int) vid_fullscreen_mode.value, vid_curpal);
			break;
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:

			if (modestate != MS_WINDOWED)
			{
				// don't call DefWindowProc() because we don't want to start
				// the screen saver fullscreen
				break;
			}

			// fall through windowed and allow the screen saver to start
		default:

			if (!in_mode_set)
			{
				S_BlockSound ();
				S_ClearBuffer ();
			}

			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);

			if (!in_mode_set)
			{
				S_UnblockSound ();
			}
		}

		break;
	case WM_MOVE:
		window_x = (int) LOWORD (lParam);
		window_y = (int) HIWORD (lParam);
		VID_UpdateWindowStatus ();

		if ((modestate == MS_WINDOWED) && !in_mode_set && !Minimized)
			VID_RememberWindowPos ();

		break;
	case WM_SIZE:
		Minimized = false;

		if (! (wParam & SIZE_RESTORED))
		{
			if (wParam & SIZE_MINIMIZED)
				Minimized = true;
		}

		break;
	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;
	case WM_ACTIVATE:
		fActive = LOWORD (wParam);
		fMinimized = (BOOL) HIWORD (wParam);
		AppActivate (! (fActive == WA_INACTIVE), fMinimized);
		// fix the leftover Alt from any Alt-Tab or the like that switched us away
		ClearAllStates ();

		if (!in_mode_set)
		{
			VID_SetPalette (vid_curpal);
		}

		break;
	case WM_PAINT:
		hdc = BeginPaint (hWnd, &ps);

		if (!in_mode_set && host_initialized)
			SCR_UpdateWholeScreen ();

		EndPaint (hWnd, &ps);
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:

		if (!in_mode_set)
			Key_Event (MapKey (lParam), true);

		break;
	case WM_KEYUP:
	case WM_SYSKEYUP:

		if (!in_mode_set)
			Key_Event (MapKey (lParam), false);

		break;
		// this is complicated because Win32 seems to pack multiple mouse events into
		// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:

		if (!in_mode_set)
		{
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			IN_MouseEvent (temp);
		}

		break;
		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
	case WM_MOUSEWHEEL:

		if ((short) HIWORD (wParam) > 0)
		{
			Key_Event (K_MWHEELUP, true);
			Key_Event (K_MWHEELUP, false);
		}
		else
		{
			Key_Event (K_MWHEELDOWN, true);
			Key_Event (K_MWHEELDOWN, false);
		}

		break;
	case WM_DISPLAYCHANGE:

		if (!in_mode_set && (modestate == MS_WINDOWED) && !vid_fulldib_on_focus_mode)
		{
			force_mode_set = true;
			VID_SetMode (vid_modenum, vid_curpal);
			force_mode_set = false;
		}

		break;
	case WM_CLOSE:

		// this causes Close in the right-click task bar menu not to work, but right
		// now bad things happen if Close is handled in that case (garbage and a
		// crash on Win95)
		if (!in_mode_set)
		{
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
							MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Sys_Quit ();
			}
		}

		break;
	case MM_MCINOTIFY:
		lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
		break;
	default:
		/* pass all unhandled messages to DefWindowProc */
		lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
		break;
	}

	/* return 0 if handled message, 1 if not */
	return lRet;
}

extern void M_Video_f (void); //qbism from Manoel Kasimier - edited
//qbism extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
//qbism- not used - extern void M_DrawPic (int x, int y, qpic_t *pic);

static int	vid_line, vid_wmodes;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
	int		width;
} modedesc_t;

#define MAX_COLUMN_SIZE		5
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 6)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	qpic_t		*p;
	char		*ptr;
	int			lnummodes, i, j, k, column, row, dup, dupmode;
	char		temp[100];
	vmode_t		*pv;
	modedesc_t	tmodedesc;
	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawTransPic ( (/*320*/ min_vid_width-p->width)/2, 4, p); //qbism from Manoel Kasimier + Dan East

	for (i = 0; i < VID_WINDOWED_MODES; i++)
	{
		ptr = VID_GetModeDescriptionMemCheck (i);
		modedescs[i].modenum = modelist[i].modenum;
		modedescs[i].desc = ptr;
		modedescs[i].iscur = 0;

		if (vid_modenum == i)
			modedescs[i].iscur = 1;
	}

	vid_wmodes = VID_WINDOWED_MODES;
	lnummodes = VID_NumModes ();

	for (i = VID_WINDOWED_MODES; i < lnummodes; i++)
	{
		ptr = VID_GetModeDescriptionMemCheck (i);
		pv = VID_GetModePtr (i);

		// we have limited room for fullscreen modes, so don't allow
		// 360-wide modes, because if there are 5 320-wide modes and
		// 5 360-wide modes, we'll run out of space
		if (ptr) //qbism...but now don't care... && ((pv->width != 360) || 1)) //qbism debug was COM_CheckParm("-allow360")))
		{
			dup = 0;

			for (j = VID_WINDOWED_MODES; j < vid_wmodes; j++)
			{
				if (!strcmp (modedescs[j].desc, ptr))
				{
					dup = 1;
					dupmode = j;
					break;
				}
			}

			if (dup || (vid_wmodes < MAX_MODEDESCS))
			{
				if (!dup || COM_CheckParm ("-noforcevga"))
				{
					if (dup)
					{
						k = dupmode;
					}
					else
					{
						k = vid_wmodes;
					}

					modedescs[k].modenum = i;
					modedescs[k].desc = ptr;
					modedescs[k].iscur = 0;
					modedescs[k].width = pv->width;

					if (i == vid_modenum)
						modedescs[k].iscur = 1;

					if (!dup)
						vid_wmodes++;
				}
			}
		}
	}

	// sort the modes on width (to handle picking up oddball dibonly modes
	// after all the others)
	for (i = VID_WINDOWED_MODES; i < (vid_wmodes - 1); i++)
	{
		for (j = (i + 1); j < vid_wmodes; j++)
		{
			if (modedescs[i].width > modedescs[j].width)
			{
				tmodedesc = modedescs[i];
				modedescs[i] = modedescs[j];
				modedescs[j] = tmodedesc;
			}
		}
	}

	M_Print (13 * 8, 36, "Windowed Modes");
	column = 16;
	row = 36 + 2 * 8;

	for (i = 0; i < VID_WINDOWED_MODES; i++)
	{
		if (modedescs[i].iscur)
			M_PrintWhite (column, row, modedescs[i].desc);
		else
			M_Print (column, row, modedescs[i].desc);

		column += 13 * 8;
	}

	if (vid_wmodes > VID_WINDOWED_MODES)
	{
		M_Print (12 * 8, 36 + 4 * 8, "Fullscreen Modes");
		column = 16;
		row = 36 + 6 * 8;

		for (i = VID_WINDOWED_MODES; i < vid_wmodes; i++)
		{
			if (modedescs[i].iscur)
				M_PrintWhite (column, row, modedescs[i].desc);
			else
				M_Print (column, row, modedescs[i].desc);

			column += 13 * 8;

			if (((i - 3) % VID_ROW_SIZE) == (VID_ROW_SIZE - 1))
			{
				column = 16;
				row += 8;
			}
		}
	}

	// line cursor
	if (vid_testingmode)
	{
		sprintf (temp, "TESTING %s",
				 modedescs[vid_line].desc);
		M_Print (13 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 4, temp);
		M_Print (9 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 6,
				 "Please wait 5 seconds...");
	}
	else
	{
		M_Print (9 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8,
				 "Press Enter to set mode");
		M_Print (6 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 3,
				 "T to test mode for 5 seconds");
		ptr = VID_GetModeDescription2 (vid_modenum);

		if (ptr)
		{
			sprintf (temp, "D to set default: %s", ptr);
			M_Print (2 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 5, temp);
		}

		ptr = VID_GetModeDescription2 ((int) _vid_default_mode_win.value);

		if (ptr)
		{
			sprintf (temp, "Current default: %s", ptr);
			M_Print (3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 6, temp);
		}

		M_Print (15 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 8,
				 "Esc to exit");
		row = 36 + 2 * 8 + (vid_line / VID_ROW_SIZE) * 8;
		column = 8 + (vid_line % VID_ROW_SIZE) * 13 * 8;

		if (vid_line >= VID_ROW_SIZE)  //qbism - use VID_ROW_SIZE
			row += 3 * 8;

		M_DrawCharacter (column, row, 12 + ((int) (realtime * 4) & 1));
	}
}


/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	if (vid_testingmode)
		return;

	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Video_f ();  //qbism from Manoel Kasiemer - edited
//		M_Menu_Options_f ();
		break;
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line = ((vid_line / VID_ROW_SIZE) * VID_ROW_SIZE) +
				   ((vid_line + 2) % VID_ROW_SIZE);

		if (vid_line >= vid_wmodes)
			vid_line = vid_wmodes - 1;

		break;
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line = ((vid_line / VID_ROW_SIZE) * VID_ROW_SIZE) +
				   ((vid_line + 4) % VID_ROW_SIZE);

		if (vid_line >= vid_wmodes)
			vid_line = (vid_line / VID_ROW_SIZE) * VID_ROW_SIZE;

		break;
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line -= VID_ROW_SIZE;

		if (vid_line < 0)
		{
			vid_line += ((vid_wmodes + (VID_ROW_SIZE - 1)) /
						  VID_ROW_SIZE) * VID_ROW_SIZE;

			while (vid_line >= vid_wmodes)
				vid_line -= VID_ROW_SIZE;
		}

		break;
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line += VID_ROW_SIZE;

		if (vid_line >= vid_wmodes)
		{
			vid_line -= ((vid_wmodes + (VID_ROW_SIZE - 1)) /
						  VID_ROW_SIZE) * VID_ROW_SIZE;

			while (vid_line < 0)
				vid_line += VID_ROW_SIZE;
		}

		break;
	case K_ENTER:
		S_LocalSound ("misc/menu1.wav");
		VID_SetMode (modedescs[vid_line].modenum, vid_curpal);
		break;
	case 'T':
	case 't':
		S_LocalSound ("misc/menu1.wav");
		// have to set this before setting the mode because WM_PAINT
		// happens during the mode set and does a VID_Update, which
		// checks vid_testingmode
		vid_testingmode = 1;
		vid_testendtime = realtime + 5.0;

		if (!VID_SetMode (modedescs[vid_line].modenum, vid_curpal))
		{
			vid_testingmode = 0;
		}

		break;
	case 'D':
	case 'd':
		S_LocalSound ("misc/menu1.wav");
		firstupdate = 0;
		Cvar_SetValue ("_vid_default_mode_win", vid_modenum);
		break;
	default:
		break;
	}
}


/*
=================
VID_NumModes
=================
*/
int VID_NumModes (void)
{
	return nummodes;
}


/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{
	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_CheckModedescFixup
=================
*/
void VID_CheckModedescFixup (int mode)
{
	int		x, y;

	if (mode == MODE_SETTABLE_WINDOW)
	{
		x = (int) vid_config_x.value;
		y = (int) vid_config_y.value;
		sprintf (modelist[mode].modedesc, "%dx%d", x, y);
		modelist[mode].width = x;
		modelist[mode].height = y;
	}
}


/*
=================
VID_GetModeDescriptionMemCheck
=================
*/
char *VID_GetModeDescriptionMemCheck (int mode)
{
	char		*pinfo;
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);
	pinfo = pv->modedesc;

	if (VID_CheckAdequateMem (pv->width, pv->height))
	{
		return pinfo;
	}
	else
	{
		return NULL;
	}
}


/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);
	pinfo = pv->modedesc;
	return pinfo;
}


/*
=================
VID_GetModeDescription2

Tacks on "windowed" or "fullscreen"
=================
*/
char *VID_GetModeDescription2 (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);

	if (modelist[mode].type == MS_FULLSCREEN)
	{
		sprintf (pinfo, "%s fullscreen", pv->modedesc);
	}
	else if (modelist[mode].type == MS_FULLDIB)
	{
		sprintf (pinfo, "%s fullscreen", pv->modedesc);
	}
	else
	{
		sprintf (pinfo, "%s windowed", pv->modedesc);
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);

	if (modelist[mode].type == MS_FULLDIB)
	{
		sprintf (pinfo, "%s fullscreen", pv->modedesc);
	}
	else
	{
		sprintf (pinfo, "%s windowed", pv->modedesc);
	}

	return pinfo;
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{
	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}


/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int		modenum;
	modenum = Q_atoi (Cmd_Argv (1));
	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));
}


/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, lnummodes;
	char		*pinfo;
	qboolean	na;
	vmode_t		*pv;
	na = false;
	lnummodes = VID_NumModes ();

	for (i = 0; i < lnummodes; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);

		if (VID_CheckAdequateMem (pv->width, pv->height))
		{
			Con_Printf ("%2d: %s\n", i, pinfo);
		}
		else
		{
			Con_Printf ("**: %s\n", pinfo);
			na = true;
		}
	}

	if (na)
	{
		Con_Printf ("\n[**: not enough system RAM for mode]\n");
	}
}


/*
=================
VID_TestMode_f
=================
*/
void VID_TestMode_f (void)
{
	int		modenum;
	double	testduration;

	if (!vid_testingmode)
	{
		modenum = Q_atoi (Cmd_Argv (1));

		if (VID_SetMode (modenum, vid_curpal))
		{
			vid_testingmode = 1;
			testduration = Q_atof (Cmd_Argv (2));

			if (testduration == 0)
				testduration = 5.0;

			vid_testendtime = realtime + testduration;
		}
	}
}


/*
=================
VID_Windowed_f
=================
*/
void VID_Windowed_f (void)
{
	VID_SetMode ((int) vid_windowed_mode.value, vid_curpal);
}


/*
=================
VID_Fullscreen_f
=================
*/
void VID_Fullscreen_f (void)
{
	VID_SetMode ((int) vid_fullscreen_mode.value, vid_curpal);
}


/*
=================
VID_Minimize_f
=================
*/
void VID_Minimize_f (void)
{
	// we only support minimizing windows; if you're fullscreen,
	// switch to windowed first
	if (modestate == MS_WINDOWED)
		ShowWindow (mainwindow, SW_MINIMIZE);
}



/*
=================
VID_ForceMode_f
=================
*/
void VID_ForceMode_f (void)
{
	int		modenum;
	double	testduration;

	if (!vid_testingmode)
	{
		modenum = Q_atoi (Cmd_Argv (1));
		force_mode_set = 1;
		VID_SetMode (modenum, vid_curpal);
		force_mode_set = 0;
	}
}


