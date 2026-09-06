/* dpi.h - 96-DPI (DIP) chrome helpers for Mappy Win32.
 *
 * Tile pixels stay 1:1 (zoomon/zoomdiv). Use these only for window chrome:
 * tabs, split, fonts, overlay pens, INI width/height.
 *
 * GetDpiForWindow is resolved at runtime so we can keep WINVER=0x0601.
 */
#ifndef MAPPY_DPI_H
#define MAPPY_DPI_H

#include <windows.h>
#include <string.h>

enum {
	MAP_DIP_SPLIT = 420,	/* min block pane width at 96 DPI (six tabs, one row) */
	MAP_DIP_SPLITPAD = 4,	/* gap so map+block = parent-(split+4) */
	MAP_DIP_MENUFUDGE = 30	/* menu/frame fudge besides SM_CYCAPTION */
};

static inline int MapGetDpi (HWND hwnd)
{
	static int inited = 0;
	static UINT (WINAPI *pGetDpiForWindow)(HWND) = NULL;
	UINT dpi = 0;
	HDC hdc;

	if (!inited) {
		HMODULE user32 = GetModuleHandleA ("user32.dll");
		if (user32)
			pGetDpiForWindow = (UINT (WINAPI *)(HWND)) (void *) GetProcAddress (user32, "GetDpiForWindow");
		inited = 1;
	}
	if (pGetDpiForWindow && hwnd)
		dpi = pGetDpiForWindow (hwnd);
	if (dpi == 0 && hwnd) {
		hdc = GetDC (hwnd);
		if (hdc) {
			dpi = (UINT) GetDeviceCaps (hdc, LOGPIXELSY);
			ReleaseDC (hwnd, hdc);
		}
	}
	if (dpi == 0) {
		hdc = GetDC (NULL);
		if (hdc) {
			dpi = (UINT) GetDeviceCaps (hdc, LOGPIXELSY);
			ReleaseDC (NULL, hdc);
		}
	}
	if (dpi < 96) dpi = 96;
	return (int) dpi;
}

static inline int MapScale (int px96, int dpi)
{
	return MulDiv (px96, dpi, 96);
}

static inline int MapDip (HWND hwnd, int px96)
{
	return MapScale (px96, MapGetDpi (hwnd));
}

/* Right pane width: never narrower than MAP_DIP_SPLIT (tabs stay on one
 * row), otherwise 1/4 of the parent client. */
static inline int MapBlockPaneWidth (HWND hwnd, int parent_client_w)
{
	int split = MapDip (hwnd, MAP_DIP_SPLIT);
	int quarter = parent_client_w / 4;
	int room;

	if (quarter > split)
		split = quarter;
	room = parent_client_w - MapDip (hwnd, 160);
	if (room > 0 && split > room)
		split = room;
	return split;
}

/* Cosmetic pen width: at least 1 device pixel, 1 DIP at 96. */
static inline int MapPenWidth (HWND hwnd)
{
	int w = MapDip (hwnd, 1);
	return (w < 1) ? 1 : w;
}

/* Message-font at the window DPI. Caller owns the HFONT (DeleteObject). */
static inline HFONT MapCreateUiFont (HWND hwnd)
{
	NONCLIENTMETRICS ncm;
	int dpi = MapGetDpi (hwnd);
	int sysdpi = MapGetDpi (NULL);

	memset (&ncm, 0, sizeof (ncm));
	ncm.cbSize = sizeof (ncm);
	if (!SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof (ncm), &ncm, 0)) {
		ncm.cbSize = sizeof (ncm) - sizeof (int);
		if (!SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
			return NULL;
	}
	if (sysdpi > 0 && dpi != sysdpi && ncm.lfMessageFont.lfHeight)
		ncm.lfMessageFont.lfHeight = MulDiv (ncm.lfMessageFont.lfHeight, dpi, sysdpi);
	return CreateFontIndirect (&ncm.lfMessageFont);
}

static inline int MapCaptionFudge (HWND hwnd)
{
	return MapDip (hwnd, MAP_DIP_MENUFUDGE) + GetSystemMetrics (SM_CYCAPTION);
}

/* Center a DIP-sized window on the monitor under the cursor. */
static inline void MapCenterOnWorkArea (int dipw, int diph, int *x, int *y, int *pw, int *ph)
{
	POINT pt;
	MONITORINFO mi;
	RECT wa;
	int dpi = MapGetDpi (NULL);

	*pw = MapScale (dipw, dpi);
	*ph = MapScale (diph, dpi);
	GetCursorPos (&pt);
	mi.cbSize = sizeof (mi);
	if (GetMonitorInfo (MonitorFromPoint (pt, MONITOR_DEFAULTTOPRIMARY), &mi))
		wa = mi.rcWork;
	else {
		wa.left = 0;
		wa.top = 0;
		wa.right = GetSystemMetrics (SM_CXSCREEN);
		wa.bottom = GetSystemMetrics (SM_CYSCREEN);
	}
	*x = wa.left + ((wa.right - wa.left) - *pw) / 2;
	*y = wa.top + ((wa.bottom - wa.top) - *ph) / 2;
	if (*x < wa.left) *x = wa.left;
	if (*y < wa.top) *y = wa.top;
}

#endif /* MAPPY_DPI_H */
