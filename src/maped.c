#define  STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"
#include "luabind.h"

typedef struct {	/* Structure for data blocks */
long int bgoff, fgoff;	/* offsets from start of graphic blocks */
long int fgoff2, fgoff3; /* more overlay blocks */
unsigned long int user1, user2;	/* user long data */
unsigned short int user3, user4;	/* user short data */
unsigned char user5, user6, user7;	/* user byte data */
unsigned char tl : 1;	/* bits for collision detection */
unsigned char tr : 1;
unsigned char bl : 1;
unsigned char br : 1;
unsigned char trigger : 1;	/* bit to trigger an event */
unsigned char unused1 : 1;
unsigned char unused2 : 1;
unsigned char unused3 : 1;
} BLKSTR;

typedef struct { /* Animation control structure */
signed char antype;	/* Type of anim, AN_? */
signed char andelay;	/* Frames to go before next frame */
signed char ancount;	/* Counter, decs each frame, till 0, then resets to andelay */
signed char anuser;	/* User info */
long int ancuroff;	/* Points to current offset in list */
long int anstartoff;	/* Points to start of blkstr offsets list, AFTER ref. blkstr offset */
long int anendoff;	/* Points to end of blkstr offsets list */
} ANISTR;

static HWND mhwnd = NULL;
static char szMapEdname[] = "Map Editor";
static int painton = 0, gettingbrush = 0;
static short int * undobuff1, * undobuff2;
int megetabrush = 0, onion = 0, oniondark = 0, onionlower = 0;
int wlayout = 0;

static int meoldbx, meoldby, mebrushx, mebrushy, mebrushx2, mebrushy2;
static int lua_mb_act = 0, lua_area_x = 0, lua_area_y = 0;

/* Graphic editor mode (when Graph tab selected in Block Editor) */
extern short int betype;
extern short int curgfxsel;
extern short int numblockgfx;
extern unsigned char * cmappt;
#define BETYPE_GRAPH 2

/* Tier-2: line/drag/path tools */
static int linemode = 0;		/* 1=line tool active */
static int linecx, linecy;		/* line start (map coords) */
static int dragmode = 0;		/* 1=drag-map active */
static int dragox, dragoy;		/* drag-map origin pixel */
static int dragoxoff, dragoyoff;	/* drag-map origin offset */
static int pathmode = 0;		/* 1=lay-path active */
static int pathcx, pathcy;		/* path cursor (map coords) */

extern HINSTANCE hInstance;
extern short int blockwidth, blockheight, blockdepth, gridon, zoomon, curbrsh;
extern int blockgapx, blockgapy, blockstaggerx, blockstaggery, clickmask;
extern unsigned char trans16hi, trans16low, transred, transgreen, transblue, trans8bit;
extern short int mapwidth, mapheight, xmapoffset, ymapoffset;
extern short int maplayer;
extern int mapaltered;
extern char * blockgfxpt, * blockstrpt;
extern short int * mappt;
extern long int curplacetile;
extern short int curstr, curanim;
extern short int numblockstr;

/* Tier-1 settings (from mapwin.c) */
extern int showuser, gridstyle, mwheelstyle, mwheelblks, thumbtrack;
extern int divideron, dividergapx, dividergapy;
extern long dividercol;
extern int zoomdiv, tbarinfo, panelzoom, zmstyle, wnesmode, rotatedtiles;
extern int mb_l, mb_m, mb_r, mb_x1, mb_x2;
extern int key1, key2, key3, keyi, keyk, keym;
extern int keyF[10];
extern int objplace;
extern int panelpro, prographbg, sortobjects;

#include "fmp.h"
#include "fmp14.h"


void MEResizeWind (int wide, int high)
{
	if (mhwnd == NULL) return;
	if (wlayout == 0) MoveWindow (mhwnd, 0, 0, wide-(wide/5), high, TRUE);
	if (wlayout == 1) MoveWindow (mhwnd, wide/5, 0, wide-(wide/5), high, TRUE);
	if (wlayout == -1) MoveWindow (mhwnd, 0, 0, wide, high, TRUE);
}

void MESetScrollRange (int xrng, int yrng, int xpos, int ypos)
{
	SetScrollRange (mhwnd, SB_HORZ, 0, xrng, FALSE);
	SetScrollRange (mhwnd, SB_VERT, 0, yrng, FALSE);
	SetScrollPos (mhwnd, SB_HORZ, xpos, TRUE);
	SetScrollPos (mhwnd, SB_VERT, ypos, TRUE);
}

void MEDestroyUndoBuffs (void)
{
	if (undobuff1!=NULL) { free (undobuff1); undobuff1 = NULL; }
	if (undobuff2!=NULL) { free (undobuff2); undobuff2 = NULL; }
}

void MEUndo (void)
{
	if (undobuff1==NULL || undobuff2==NULL) return;
	memcpy (undobuff2, mappt, mapwidth*mapheight*2);
	memcpy (mappt, undobuff1, mapwidth*mapheight*2);
	memcpy (undobuff1, undobuff2, mapwidth*mapheight*2);
}

void MECopyToUndo (void)
{
	if (mappt==NULL) return;
	if (undobuff1==NULL) { undobuff1 = malloc (mapwidth*mapheight*2);
		if (undobuff1==NULL) return; memset (undobuff1, 0, mapwidth*mapheight*2); }
	if (undobuff2==NULL) { undobuff2 = malloc (mapwidth*mapheight*2);
		if (undobuff2==NULL) return; memset (undobuff2, 0, mapwidth*mapheight*2); }
	memcpy (undobuff1, mappt, mapwidth*mapheight*2);
}

static void MEEnsureUndoBuf (void)
{
	size_t n = (size_t) mapwidth * (size_t) mapheight * 2;
	if (n == 0) return;
	if (undobuff1==NULL) { undobuff1 = malloc (n); if (undobuff1) memset (undobuff1, 0, n); }
	if (undobuff2==NULL) { undobuff2 = malloc (n); if (undobuff2) memset (undobuff2, 0, n); }
}

void MECopyLayerToUndo (short int *src)
{
	size_t n = (size_t) mapwidth * (size_t) mapheight * 2;
	MEEnsureUndoBuf ();
	if (undobuff1 != NULL && src != NULL) memcpy (undobuff1, src, n);
}

void MECopyUndoToLayer (short int *dst)
{
	size_t n = (size_t) mapwidth * (size_t) mapheight * 2;
	if (undobuff1 != NULL && dst != NULL) memcpy (dst, undobuff1, n);
}

HWND MEGetHWND (void)
{
	return mhwnd;
}

int MEClickmask (int x, int y, int xory)
{
unsigned char * mygfxpt;

	if (blockgfxpt == NULL) return 0;

	x %= blockgapx; y %= blockgapy;

	if (x >= blockwidth && xory == 0) return 0;
	if (x >= blockwidth && xory == 1) {
		if (y < blockstaggery) return -1;
		else return 1;
	}
	if (y >= blockheight && xory == 1) return 1;
	if (y >= blockheight && xory == 0) {
		if (x < blockstaggerx) return -1;
		else return 0;
	}

	mygfxpt = (unsigned char *) blockgfxpt;
	mygfxpt += (blockwidth*blockheight*((blockdepth+1)/8)*clickmask);
	switch (blockdepth) {
		case 8:
			if (mygfxpt[(y*blockwidth)+x] == trans8bit) {
				if (x < (blockwidth/2) && xory == 0) return -1;
				if (x >= (blockwidth/2) && xory == 0) return 0;
				if (y < (blockheight/2) && xory == 1) return -1;
				if (y >= (blockheight/2) && xory == 1) return 1;
			}
			return 0;
		case 15:
		case 16:
			if (mygfxpt[(y*blockwidth*2)+(x*2)] == trans16hi &&
				mygfxpt[(y*blockwidth*2)+(x*2)+1] == trans16low) {
				if (x < (blockwidth/2) && xory == 0) return -1;
				if (x >= (blockwidth/2) && xory == 0) return 0;
				if (y < (blockheight/2) && xory == 1) return -1;
				if (y >= (blockheight/2) && xory == 1) return 1;
			}
			return 0;
		case 24:
			if (mygfxpt[(y*blockwidth*3)+(x*3)] == transred &&
				mygfxpt[(y*blockwidth*3)+(x*3)+1] == transgreen &&
				mygfxpt[(y*blockwidth*3)+(x*3)+2] == transblue) {
				if (x < (blockwidth/2) && xory == 0) return -1;
				if (x >= (blockwidth/2) && xory == 0) return 0;
				if (y < (blockheight/2) && xory == 1) return -1;
				if (y >= (blockheight/2) && xory == 1) return 1;
			}
			return 0;
		case 32:
			if (mygfxpt[(y*blockwidth*4)+(x*3)+1] == transred &&
				mygfxpt[(y*blockwidth*4)+(x*3)+2] == transgreen &&
				mygfxpt[(y*blockwidth*4)+(x*3)+3] == transblue) {
				if (x < (blockwidth/2) && xory == 0) return -1;
				if (x >= (blockwidth/2) && xory == 0) return 0;
				if (y < (blockheight/2) && xory == 1) return -1;
				if (y >= (blockheight/2) && xory == 1) return 1;
			}
			return 0;
		default:
			return 0;
	}

}

/* zoom helpers: zoomon in {1,2,4}, zoomdiv in {1,2,4} (x1,x0.5,x0.25) */
static int ZX (int v) { return (v * zoomon) / zoomdiv; }
static int ZD (int v) { return (v * zoomdiv) / zoomon; }

int MEGetX (int xpix, int ypix)
{
int xb;

	if (blockstaggerx || blockstaggery) {
		xpix += (blockstaggerx*zoomon/zoomdiv);
		ypix += (blockstaggery*zoomon/zoomdiv);
	}
	xpix = ZD (xpix);
	ypix = ZD (ypix);
	xb = xpix/blockgapx;

	if ((blockstaggerx || blockstaggery) && clickmask) xb += MEClickmask (xpix, ypix, 0);

	return xb;
}

int MEGetY (int xpix, int ypix)
{
int yb;

	if (blockstaggerx || blockstaggery) {
		xpix += (blockstaggerx*zoomon/zoomdiv);
		ypix += (blockstaggery*zoomon/zoomdiv);
	}
	xpix = ZD (xpix);
	ypix = ZD (ypix);
	yb = ypix/blockgapy;

	if ((blockstaggerx || blockstaggery) && clickmask) {
		yb *= 2;
		yb += MEClickmask (xpix, ypix, 1);
	}

	return yb;
}

void MEFillMap (int fillstyle)
{
POINT mousecoords;
RECT merect;

	if (mappt==NULL) return;
	GetCursorPos (&mousecoords);
	GetWindowRect (mhwnd, &merect);
	if (mousecoords.x<merect.left ||
		mousecoords.x>merect.right ||
		mousecoords.y<merect.top ||
		mousecoords.y>merect.bottom) return;

	mousecoords.x -= (merect.left+GetSystemMetrics(SM_CXFRAME));
	mousecoords.y -= (merect.top+GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYFRAME));
	MECopyToUndo ();
	MapFill (xmapoffset+MEGetX(mousecoords.x,mousecoords.y),
		ymapoffset+MEGetY(mousecoords.x,mousecoords.y),
		fillstyle);
}

void MEPickBlock (void)
{
POINT mousecoords;
RECT merect;

	if (mappt==NULL) return;
	GetCursorPos (&mousecoords);
	GetWindowRect (mhwnd, &merect);
	if (mousecoords.x<merect.left ||
		mousecoords.x>merect.right ||
		mousecoords.y<merect.top ||
		mousecoords.y>merect.bottom) return;

	mousecoords.x -= (merect.left+GetSystemMetrics(SM_CXFRAME));
	mousecoords.y -= (merect.top+GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYFRAME));
	PickBlock (xmapoffset+MEGetX(mousecoords.x,mousecoords.y),
		ymapoffset+MEGetY(mousecoords.x,mousecoords.y));
}

void MEShowCoords (int x, int y)
{
char temptext[256];
char scbits[16];
char dettxt[128];
BLKSTR * myblkstrpt;
//ANISTR * myanpt;
short int * mymappt;
int nlayers, li;
const char * lname;
const char * tstr;

	if (mappt == NULL) return;
	if (blockstrpt == NULL) return;
	if (x>=mapwidth) x = mapwidth-1;
	if (y>=mapheight) y = mapheight-1;
	if (x<0) x = 0;
	if (y<0) y = 0;

	mymappt = mappt;

	mymappt += (x+(y*mapwidth));

	dettxt[0] = 0;
	if (*mymappt<0) {
		sprintf (dettxt, "Anim %d", (-(*mymappt))/sizeof(ANISTR)-1);
	} else {
		myblkstrpt = (BLKSTR *) (blockstrpt + (((int)(*mymappt))*sizeof(BLKSTR)));
		memset (scbits, 0, 16);
		if (myblkstrpt->tl) scbits[0] = '1'; else scbits[0] = '0';
		if (myblkstrpt->tr) scbits[1] = '1'; else scbits[1] = '0';
		if (myblkstrpt->bl) scbits[2] = '1'; else scbits[2] = '0';
		if (myblkstrpt->br) scbits[3] = '1'; else scbits[3] = '0';
		if (myblkstrpt->trigger) scbits[4] = '1'; else scbits[4] = '0';
		if (myblkstrpt->unused1) scbits[5] = '1'; else scbits[5] = '0';
		if (myblkstrpt->unused2) scbits[6] = '1'; else scbits[6] = '0';
		if (myblkstrpt->unused3) scbits[7] = '1'; else scbits[7] = '0';
		if (myblkstrpt->trigger) scbits[9] = '1'; else scbits[9] = '0';
		sprintf (dettxt, "Block %d (BG=%d, FG=%d, Bits=%s, T=%s)",
			(*mymappt),
			(int) myblkstrpt->bgoff/(blockwidth*blockheight*((blockdepth+1)/8)),
			(int) myblkstrpt->fgoff/(blockwidth*blockheight*((blockdepth+1)/8)),
			scbits, scbits+9);
	}

	nlayers = 0; for (li=0;li<MAXMAPLAYERS;li++) if (GetLayerpt (li)!=NULL) nlayers++;

	/* tbarinfo titlebar modes (mapwin.ini) */
	switch (tbarinfo) {
		case 0:
			strcpy (temptext, "Map Editor");
			break;
		case 3:
			if (*mymappt<0) sprintf (temptext, "X %d/%d, Y %d/%d, Layer %d/%d, %s",
				x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, dettxt);
			else sprintf (temptext, "X %d/%d, Y %d/%d, Layer %d/%d, Block %d",
				x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, (*mymappt));
			break;
		case 4:
			sprintf (temptext, "X %d/%d, Y %d/%d, Layer %d/%d, %s",
				x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, dettxt);
			break;
		case 5:
			tstr = F14_TstrGet (0);
			if (tstr==NULL) tstr = "";
			sprintf (temptext, "X %d/%d, Y %d/%d, Layer %d/%d, %s [%s]",
				x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0,
				(*mymappt<0) ? dettxt : dettxt, tstr);
			if (*mymappt>=0) sprintf (temptext, "X %d/%d, Y %d/%d, Layer %d/%d, Block %d [%s]",
				x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, (*mymappt), tstr);
			break;
		case 6:
			lname = F14_LayerName (maplayer);
			if (lname==NULL) lname = "";
			tstr = F14_TstrGet (0);
			if (tstr==NULL) tstr = "";
			sprintf (temptext, "X %d/%d, Y %d/%d, [%s] %d/%d, %s [%s]",
				x, mapwidth-1, y, mapheight-1, lname, maplayer, nlayers ? nlayers-1 : 0, dettxt, tstr);
			break;
		case 2:
			sprintf (temptext, "X %d/%d, Y %d/%d. %s",
				x, mapwidth-1, y, mapheight-1, dettxt);
			break;
		default: /* 1 */
			if (*mymappt<0) sprintf (temptext, "X %d/%d, Y %d/%d. %s",
				x, mapwidth-1, y, mapheight-1, dettxt);
			else sprintf (temptext, "X %d/%d, Y %d/%d. Block %d",
				x, mapwidth-1, y, mapheight-1, (*mymappt));
			break;
	}

	/* Tier-2 tool help: show active L/D/P mode in title (keys L/P/D, ESC cancels) */
	if (linemode) strcat (temptext, " [LINE L: click end, ESC cancels]");
	else if (pathmode) strcat (temptext, " [PATH P: click next, ESC ends]");
	else if (dragmode) strcat (temptext, " [DRAG D: move mouse, D/click ends]");

	SetWindowText (mhwnd, temptext);
	meoldbx = x; meoldby = y;
}

void MEBrushRect (HWND hwnd, int xoff, int yoff, int xoff2, int yoff2)
{
RECT dragboxrect;
HDC myhdc;
HBRUSH oldbrush;
int oldrop;

	xoff = (xoff*blockgapx*zoomon)/zoomdiv; xoff2 = (xoff2*blockgapx*zoomon)/zoomdiv;
	yoff = (yoff*blockgapy*zoomon)/zoomdiv; yoff2 = (yoff2*blockgapy*zoomon)/zoomdiv;
   if (blockstaggerx || blockstaggery) { yoff /= 2; yoff2 /= 2; }
	if (xoff<xoff2) { dragboxrect.left = xoff; dragboxrect.right = xoff2+(blockwidth*zoomon/zoomdiv)-1; }
	else { dragboxrect.left = xoff2; dragboxrect.right = xoff+(blockwidth*zoomon/zoomdiv)-1; }
	if (yoff<yoff2) { dragboxrect.top = yoff; dragboxrect.bottom = yoff2+(blockheight*zoomon/zoomdiv)-1; }
	else { dragboxrect.top = yoff2; dragboxrect.bottom = yoff+(blockheight*zoomon/zoomdiv)-1; }
	myhdc = GetDC (hwnd);
	oldrop = SetROP2 (myhdc, R2_NOT);
	oldbrush = SelectObject (myhdc, GetStockObject (HOLLOW_BRUSH));
	Rectangle (myhdc, dragboxrect.left, dragboxrect.top, dragboxrect.right,
		dragboxrect.bottom);
	SelectObject (myhdc, oldbrush);
	SetROP2 (myhdc, oldrop);
	ReleaseDC (hwnd, myhdc);
}

void MEDrawUnderTile (HWND hwnd, int xoff, int yoff)
{
int bwpitch;
HDC myhdc, fromhdc;
HBITMAP temphbm, hbmpold;
unsigned char * destmempt;
BITMAPINFOHEADER bih;
RGBQUAD rgbq;

		if (mappt==NULL) return;
		if (xoff<0 || xoff>=mapwidth) return;
		if (yoff<0 || yoff>=mapheight) return;

		bwpitch = ((blockwidth*3)+3)&0x7FFC;
		destmempt = malloc (bwpitch*blockheight);
		if (destmempt==NULL) return;

		myhdc = GetDC (hwnd);
		if (myhdc==NULL) { free (destmempt); return; }
		fromhdc = CreateCompatibleDC (myhdc);

		memset (&bih, 0, sizeof(BITMAPINFOHEADER));
		memset (&rgbq, 0, sizeof(RGBQUAD));
		bih.biSize = sizeof (BITMAPINFOHEADER);
		bih.biWidth = blockwidth;
		bih.biHeight = -blockheight;
		bih.biPlanes = 1;
		bih.biBitCount = 24;
		bih.biCompression = BI_RGB;

		Blockto24 (xoff, yoff, destmempt, 0, 0, bwpitch);

		temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
			destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);

		hbmpold = SelectObject(fromhdc, temphbm);
		if (zoomon==1 && zoomdiv==1) {
/*			if (blockstaggerx || blockstaggery) {
				if (yoff&1)
					BitBlt (myhdc, ((xoff-xmapoffset)*blockgapx)+blockstaggerx,
						(((yoff/2)-ymapoffset)*blockgapy)+blockstaggery,
						blockwidth, blockheight, fromhdc, 0, 0, SRCCOPY);
				else
					BitBlt (myhdc, (xoff-xmapoffset)*blockgapx, ((yoff/2)-ymapoffset)*blockgapy,
						blockwidth, blockheight, fromhdc, 0, 0, SRCCOPY);
			} else {
*/				BitBlt (myhdc, (xoff-xmapoffset)*blockwidth, (yoff-ymapoffset)*blockheight,
					blockwidth, blockheight, fromhdc, 0, 0, SRCCOPY);
//			}
		} else {
			StretchBlt (myhdc, ZX((xoff-xmapoffset)*blockwidth), ZX((yoff-ymapoffset)*blockheight),
				ZX(blockwidth), ZX(blockheight), fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
		}

		SelectObject (fromhdc, hbmpold);
		DeleteObject (temphbm);

		DeleteDC (fromhdc);
		ReleaseDC (hwnd, myhdc);
		free (destmempt);
}

/* onion backdrop: single layer, All layers, or 0..layer (show lower layers) */
static void MEOnionBg (int x, int y, unsigned char * dst, int xo, int yo, int ls)
{
	if (onion==ONION_ALL) OnionCompto24 (x, y, dst, xo, yo, ls, MAXMAPLAYERS-1);
	else if (onionlower) OnionCompto24 (x, y, dst, xo, yo, ls, onion-1);
	else Onionto24 (x, y, dst, xo, yo, ls, onion-1);
}

/* software stretch (zmstyle=0): nearest-neighbour, no StretchBlt */
static void MESoftStretch (HDC dst, int dx, int dy, int dw, int dh,
	HDC src, int sx, int sy, int sw, int sh)
{
int x, y;
COLORREF c;

	if (dw<=0 || dh<=0 || sw<=0 || sh<=0) return;
	for (y=0;y<dh;y++) {
		for (x=0;x<dw;x++) {
			c = GetPixel (src, sx + (x*sw)/dw, sy + (y*sh)/dh);
			SetPixel (dst, dx+x, dy+y, c);
		}
	}
}

void MEDrawMap (HWND phwnd, HDC myhdc)
{
int i, j, k, l, bwpitch;
HDC fromhdc;
HBITMAP temphbm, hbmpold;
unsigned long int * darkenpt;
unsigned char * destmempt;
BITMAPINFOHEADER bih;
RGBQUAD rgbq;
RECT rc;

		GetClientRect (phwnd, &rc);
		if (blockgapx < 1 || blockgapy < 1) return;
		k = (rc.right/blockgapx)+2;
		if (zoomon!=1||zoomdiv!=1) { k = (k*zoomdiv)/zoomon+(zoomon-1); }
		l = (rc.bottom/blockgapy)+2;
		if (zoomon!=1||zoomdiv!=1) { l = (l*zoomdiv)/zoomon+(zoomon-1); }
		if (k < 1) k = 1;
		if (l < 1) l = 1;
		if (k > 512) k = 512;
		if (l > 512) l = 512;

		bwpitch = ((k*blockgapx*3)+3)&~3;
		{
			size_t nbytes = (size_t)(l+2) * (size_t)bwpitch * (size_t)blockgapy;
			if (bwpitch < 4 || nbytes == 0 || nbytes > (96u*1024u*1024u)) return;
			destmempt = malloc (nbytes);
		}
		if (destmempt==NULL) return;

		fromhdc = CreateCompatibleDC (myhdc);

		memset (&bih, 0, sizeof(BITMAPINFOHEADER));
		memset (&rgbq, 0, sizeof(RGBQUAD));
		bih.biSize = sizeof (BITMAPINFOHEADER);
		bih.biWidth = k*blockgapx;
		bih.biHeight = -(l*blockgapy);
		bih.biPlanes = 1;
		bih.biBitCount = 24;
		bih.biCompression = BI_RGB;

//		k--; l--;
		for (j=0;j<l;j++) {
			for (i=0;i<k;i++) {
				if (blockstaggerx || blockstaggery) {
					if (onion) MEOnionBg (xmapoffset+i, ymapoffset+(j*2), destmempt, i*blockgapx, j*blockgapy, bwpitch);
					else Overlayto24 (xmapoffset+i, ymapoffset+(j*2), destmempt, i*blockgapx, j*blockgapy, bwpitch, -1);
				} else {
					if (onion) MEOnionBg (xmapoffset+i, ymapoffset+j, destmempt, i*blockgapx, j*blockgapy, bwpitch);
					else Blockto24 (xmapoffset+i, ymapoffset+j, destmempt, i*blockgapx, j*blockgapy, bwpitch);
				}
			}
			for (i=0;i<k;i++) {
				if (blockstaggerx || blockstaggery) {
					if (onion) MEOnionBg (xmapoffset+i, ymapoffset+(j*2+1), destmempt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch);
					else Overlayto24 (xmapoffset+i, ymapoffset+(j*2+1), destmempt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, -1);
				}
			}
		}

		if (onion) { if (oniondark) {
		i = (l*bwpitch*blockheight)/4; darkenpt = (unsigned long int *) destmempt; while (i) {
			*darkenpt = ((*darkenpt)>>1)&0x7F7F7F7F; darkenpt++; i--;
		} }
		for (j=0;j<l;j++)
		{	for (i=0;i<k;i++)
			{
				if (blockstaggerx || blockstaggery) {
					Overlayto24 (xmapoffset+i, ymapoffset+(j*2), destmempt, i*blockgapx, j*blockgapy, bwpitch, -1);
				} else {
					Overlayto24 (xmapoffset+i, ymapoffset+j, destmempt, i*blockgapx, j*blockgapy, bwpitch, -1);
				}
			}
			for (i=0;i<k;i++) {
				if (blockstaggerx || blockstaggery) {
					Overlayto24 (xmapoffset+i, ymapoffset+(j*2+1), destmempt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, -1);
				}
			}
		} }

		temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
			destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);

		hbmpold = SelectObject(fromhdc, temphbm);
		if (zoomon==1 && zoomdiv==1)
			BitBlt (myhdc, 0, 0, k*blockgapx, l*blockgapy, fromhdc, blockstaggerx, blockstaggery, SRCCOPY);
		else if (zmstyle==0)
			MESoftStretch (myhdc, 0, 0, ZX((k*blockgapx)-blockstaggerx), ZX((l*blockgapy)-blockstaggery),
				fromhdc, blockstaggerx, blockstaggery, k*blockgapx-blockstaggerx, l*blockgapy-blockstaggery);
		else
			StretchBlt (myhdc, 0, 0, ZX((k*blockgapx)-blockstaggerx), ZX((l*blockgapy)-blockstaggery), fromhdc, blockstaggerx, blockstaggery, k*blockgapx-blockstaggerx, l*blockgapy-blockstaggery, SRCCOPY);
		SelectObject (fromhdc, hbmpold);
		DeleteObject (temphbm);

		if (wnesmode) {
			/* 3D-wall mode: red vertical wall markers at tile left edges */
			RECT wrc; HPEN wpen, wold; int wx, wy0, wy1;
			GetClientRect (phwnd, &wrc);
			wpen = CreatePen (PS_SOLID, 1, RGB (255, 0, 0));
			wold = SelectObject (myhdc, wpen);
			for (j=0;j<=k;j++) {
				wx = ZX(j*blockgapx);
				wy0 = 0; wy1 = wrc.bottom;
				MoveToEx (myhdc, wx, wy0, NULL);
				LineTo (myhdc, wx, wy1);
			}
			SelectObject (myhdc, wold);
			DeleteObject (wpen);
		}

		if (divideron && dividergapx >= 8 && dividergapy >= 8) {
			RECT crc; HPEN hpen, holdpen;
			int stepx = dividergapx * zoomon / zoomdiv, stepy = dividergapy * zoomon / zoomdiv;
			int ox = (xmapoffset * blockwidth * zoomon / zoomdiv) % stepx;
			int oy = (ymapoffset * blockheight * zoomon / zoomdiv) % stepy;
			int x, y;
			GetClientRect (phwnd, &crc);
			hpen = CreatePen (PS_SOLID, 1, RGB ((dividercol>>16)&0xFF, (dividercol>>8)&0xFF, dividercol&0xFF));
			holdpen = SelectObject (myhdc, hpen);
			for (x = -ox; x <= crc.right; x += stepx) {
				MoveToEx (myhdc, x, 0, NULL);
				LineTo (myhdc, x, crc.bottom);
			}
			for (y = -oy; y <= crc.bottom; y += stepy) {
				MoveToEx (myhdc, 0, y, NULL);
				LineTo (myhdc, crc.right, y);
			}
			SelectObject (myhdc, holdpen);
			DeleteObject (hpen);
		}

		if (showuser>=1 && showuser<=8) {
			/* show user field / flags overlaying tiles (objects win) */
			char utxt[64]; int vx, vy, ti, tj; short int bv; BLKSTR * bp;
			SetBkMode (myhdc, TRANSPARENT);
			SetTextColor (myhdc, RGB (255, 255, 0));
			for (tj=0;tj<l;tj++) {
				for (ti=0;ti<k;ti++) {
					vx = xmapoffset+ti; vy = ymapoffset+tj;
					if (vx<0 || vy<0 || vx>=mapwidth || vy>=mapheight) continue;
					if (OE_ObjOverlayText (vx, vy, utxt, sizeof (utxt))) {
						TextOut (myhdc, ZX(ti*blockgapx), ZX(tj*blockgapy), utxt, strlen (utxt));
						continue;
					}
					bv = mappt[vx+vy*mapwidth];
					if (bv<0 || bv>=numblockstr) continue;
					bp = (BLKSTR *) (blockstrpt + (((int) bv)*sizeof(BLKSTR)));
					switch (showuser) {
						case 1: sprintf (utxt, "%lu", bp->user1); break;
						case 2: sprintf (utxt, "%lu", bp->user2); break;
						case 3: sprintf (utxt, "%u", bp->user3); break;
						case 4: sprintf (utxt, "%u", bp->user4); break;
						case 5: sprintf (utxt, "%u", bp->user5); break;
						case 6: sprintf (utxt, "%u", bp->user6); break;
						case 7: sprintf (utxt, "%u", bp->user7); break;
						default: sprintf (utxt, "%d%d%d%d%d", bp->tl, bp->tr, bp->bl, bp->br, bp->trigger); break;
					}
					if (showuser >= 1 && showuser <= 7 && showuser == LuaGetStrBlkUser ()) {
						unsigned long sv;
						const char * ts;
						switch (showuser) {
							case 1: sv = bp->user1; break;
							case 2: sv = bp->user2; break;
							case 3: sv = bp->user3; break;
							case 4: sv = bp->user4; break;
							case 5: sv = bp->user5; break;
							case 6: sv = bp->user6; break;
							default: sv = bp->user7; break;
						}
						ts = F14_TstrGet ((int) sv);
						if (ts && ts[0]) {
							strncpy (utxt, ts, sizeof (utxt) - 1);
							utxt[sizeof (utxt) - 1] = 0;
						}
					}
					TextOut (myhdc, ZX(ti*blockgapx), ZX(tj*blockgapy), utxt, strlen (utxt));
				}
			}
		}

		{
			RECT oeclient;
			GetClientRect (phwnd, &oeclient);
			OE_DrawObjects (myhdc, &oeclient);
		}

		DeleteDC (fromhdc);
		free (destmempt);
}

void MEDrawGrid (HWND hwnd, int xoff, int yoff)
{
int i, j, bwpitch, x, y;
HDC myhdc, fromhdc;
HBITMAP temphbm, hbmpold;
unsigned char * destmempt, * darkenpt;
BITMAPINFOHEADER bih;
RGBQUAD rgbq;
DWORD blttype;
//FILE * dfpt;

		if (gettingbrush) return;
		if (mappt==NULL || !gridon) return;
		if (zoomdiv>1) return; /* grid not drawn in software zoomout */
		if (xoff==meoldbx && yoff==meoldby) return;

		bwpitch = ((blockwidth*3)+3)&0x7FFC;
		destmempt = malloc (bwpitch*blockheight);
		if (destmempt==NULL) return;

		myhdc = GetDC (hwnd);
		if (myhdc==NULL) { free (destmempt); return; }
		fromhdc = CreateCompatibleDC (myhdc);

		memset (&bih, 0, sizeof(BITMAPINFOHEADER));
		memset (&rgbq, 0, sizeof(RGBQUAD));
		bih.biSize = sizeof (BITMAPINFOHEADER);
		bih.biWidth = blockwidth;
		bih.biHeight = -blockheight;
		bih.biPlanes = 1;
		bih.biBitCount = 24;
		bih.biCompression = BI_RGB;

		if (blockstaggerx || blockstaggery) {
			MEDrawMap (hwnd, myhdc);
		} else {
			if (onion) MEOnionBg (meoldbx, meoldby, destmempt, 0, 0, bwpitch);
			else Blockto24 (meoldbx, meoldby, destmempt, 0, 0, bwpitch);
			if (oniondark) {
			i = bwpitch*blockheight; darkenpt = destmempt; while (i) {
				*darkenpt = ((*darkenpt)>>1)&0x7F; darkenpt++; i--;
			} }
			if (onion) Overlayto24 (meoldbx, meoldby, destmempt, 0, 0, bwpitch, -1);

			temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
				destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
			x = ZX((meoldbx-xmapoffset)*blockwidth);
			y = ZX((meoldby-ymapoffset)*blockheight);
			hbmpold = SelectObject(fromhdc, temphbm);
			if (zoomon==1 && zoomdiv==1)
				BitBlt (myhdc, x, y,	blockwidth, blockheight, fromhdc, 0, 0, SRCCOPY);
			else
				StretchBlt (myhdc, x, y,
					ZX(blockwidth), ZX(blockheight), fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
			SelectObject (fromhdc, hbmpold);
			DeleteObject (temphbm);
		}

		if (xoff<0 || xoff>=mapwidth || yoff<0 || yoff>=mapheight) {
			DeleteDC (fromhdc);
			ReleaseDC (hwnd, myhdc);
			free (destmempt);
			return;
		}

		if (!(blockstaggerx || blockstaggery)) {
		if (gridstyle == 2) {
			/* crosshatch grid: draw transparent-pixel checkered pattern */
			Blockto24 (xoff, yoff, destmempt, 0, 0, bwpitch);
			for (i=0;i<blockheight;i++) {
				for (j=0;j<blockwidth;j++) {
					/* checkerboard: if (i+j)&1 then darken */
					if ((i+j)&1) {
						destmempt[i*bwpitch+(j*3)] = (destmempt[i*bwpitch+(j*3)]*2)/3;
						destmempt[i*bwpitch+(j*3)+1] = (destmempt[i*bwpitch+(j*3)+1]*2)/3;
						destmempt[i*bwpitch+(j*3)+2] = (destmempt[i*bwpitch+(j*3)+2]*2)/3;
					}
				}
			}
		} else {
		/* cursor-border grid */
		Blockto24 (xoff, yoff, destmempt, 0, 0, bwpitch);
		for (i=0;i<blockwidth;i++) {
			destmempt[i*3] = 0; destmempt[i*3+1] = 0; destmempt[i*3+2] = 0;
		}
		for (i=1;i<blockheight;i++) {
			destmempt[i*bwpitch] = 0;
			destmempt[i*bwpitch+1] = 0;
			destmempt[i*bwpitch+2] = 0;
			destmempt[i*bwpitch+((blockwidth-1)*3)] = 255;
			destmempt[i*bwpitch+((blockwidth-1)*3)+1] = 255;
			destmempt[i*bwpitch+((blockwidth-1)*3)+2] = 255;
		}
		for (i=0;i<blockwidth;i++) {
			destmempt[i*3+((blockheight-1)*bwpitch)] = 255;
			destmempt[i*3+((blockheight-1)*bwpitch)+1] = 255;
			destmempt[i*3+((blockheight-1)*bwpitch)+2] = 255;
		}
		}
		} else {
			Blockto24 (xoff, yoff, destmempt, 0, 0, bwpitch);
			for (i=0;i<blockheight;i++) {
				for (j=0;j<blockwidth;j++) {
					if (destmempt[i*bwpitch+(j*3)] ||
						destmempt[i*bwpitch+1+(j*3)] ||
						destmempt[i*bwpitch+2+(j*3)]) {
							destmempt[i*bwpitch+(j*3)] = -1;
							destmempt[i*bwpitch+1+(j*3)] = -1;
							destmempt[i*bwpitch+2+(j*3)] = -1;
					}
				}
			}
		}

		temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
			destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);

		hbmpold = SelectObject(fromhdc, temphbm);

		if (blockstaggerx || blockstaggery) {
			x = (xoff-xmapoffset);
			y = (yoff-ymapoffset);
			if (y&1) i = 1; else i = 0;
			x = ZX(x*blockgapx);
			y /= 2; y = ZX(y*blockgapy);
			if (!i) { x -= ZX(blockstaggerx); y -= ZX(blockstaggery); }
			blttype = SRCPAINT;
		} else {
			x = ZX((xoff-xmapoffset)*blockwidth);
			y = ZX((yoff-ymapoffset)*blockheight);
			blttype = SRCCOPY;
		}

		if (zoomon==1 && zoomdiv==1)
			BitBlt (myhdc, x, y,	blockwidth, blockheight, fromhdc, 0, 0, blttype);
		else
			StretchBlt (myhdc, x, y,
				ZX(blockwidth), ZX(blockheight), fromhdc, 0, 0, blockwidth, blockheight, blttype);

		SelectObject (fromhdc, hbmpold);
		DeleteObject (temphbm);

		DeleteDC (fromhdc);
		ReleaseDC (hwnd, myhdc);
		free (destmempt);
}

/* Custom mousebuttons (mapwin.ini): 0 None, 1 Current block, 2 Block 0,
 * 3 Drag map 1, 4 Drag map 2, 5 Placer.lua, 6 Pick block, 7 Lay path,
 * 8 Fill, 9 Line, 10 Area.lua, 100+ CM_ command. List per binary table.
 * Returns 1 if handled (caller must swallow the click), 0 to continue. */
static int rmapped = 0;
static int MBDoAction (int act, HWND hwnd, LPARAM lParam)
{
	switch (act) {
		case 0: /* None */
			return 1;
		case 1: /* Current block: normal flow */
			return 0;
		case 2: /* Block 0: select it, then normal flow */
			curstr = 0; curanim = -1; curbrsh = 0;
			return 0;
		case 3: /* Drag map 1 */
		case 4: /* Drag map 2 (full repaint while dragging) */
			dragmode = 1;
			dragox = LOWORD(lParam);
			dragoy = HIWORD(lParam);
			dragoxoff = xmapoffset;
			dragoyoff = ymapoffset;
			SetCursor(LoadCursor(NULL, IDC_SIZEALL));
			if (act==4) InvalidateRgn (mhwnd, NULL, FALSE);
			return 1;
		case 5: /* Placer.lua — run on click and while dragged */
			{ int bx = (LOWORD(lParam)) / blockgapx + xmapoffset;
			  int by = (HIWORD(lParam)) / blockgapy + ymapoffset;
			  lua_mb_act = 5;
			  LuaSetMouseBlock (bx, by, bx, by);
			  LuaRunMouseButton (act, bx, by); }
			return 1;
		case 10: /* Area.lua — capture drag, run on button up */
			{ int bx = (LOWORD(lParam)) / blockgapx + xmapoffset;
			  int by = (HIWORD(lParam)) / blockgapy + ymapoffset;
			  lua_mb_act = 10;
			  lua_area_x = bx; lua_area_y = by;
			  LuaSetMouseBlock (bx, by, bx, by); }
			return 1;
		case 6: /* Pick block */
			MEPickBlock ();
			return 1;
		case 7: /* Lay path */
			if (pathmode) pathmode = 0; else { pathmode = 1; linemode = 0; }
			return 1;
		case 8: /* Fill */
			MEFillMap (0);
			return 1;
		case 9: /* Line */
			if (linemode) linemode = 0; else { linemode = 1; pathmode = 0; }
			return 1;
		default:
			if (act>=400) {
				int bx = (LOWORD(lParam)) / blockgapx + xmapoffset;
				int by = (HIWORD(lParam)) / blockgapy + ymapoffset;
				LuaSetMouseBlock (bx, by, bx, by);
				LuaRunMouseButton (act, bx, by);
				return 1;
			}
			if (act>=100) {
				SendMessage (GetParent (hwnd), WM_COMMAND, act, 0);
				return 1;
			}
			return 0;
	}
}

/* ---- Tier-2 helpers ---- */

static void MEBresenhamLine (int x0, int y0, int x1, int y1)
{
	int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
	int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
	int err = dx+dy, e2;

	for (;;) {
		curplacetile = -1;
		if (x0>=0 && x0<mapwidth && y0>=0 && y0<mapheight) {
			short int * mpt = mappt + x0 + y0*mapwidth;
			if (curbrsh>0 && curbrsh<=MAXBRSH) {
				PlaceTile (x0, y0);
			} else if (curanim>=0) {
				*mpt = -((curanim+1)*FMP_ANI_MEMUNIT);
			} else {
				*mpt = curstr;
			}
		}
		if (x0==x1 && y0==y1) break;
		e2 = 2*err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

static void MEDragMap (int xpix, int ypix)
{
	int dx = ZD (xpix - dragox);
	int dy = ZD (ypix - dragoy);
	int nx = dragoxoff - dx;
	int ny = dragoyoff - dy;
	if (nx < 0) nx = 0;
	if (ny < 0) ny = 0;
	if (nx > mapwidth-1) nx = mapwidth-1;
	if (ny > mapheight-1) ny = mapheight-1;
	xmapoffset = nx;
	ymapoffset = ny;
	SetScrollPos (mhwnd, SB_HORZ, xmapoffset, TRUE);
	SetScrollPos (mhwnd, SB_VERT, ymapoffset, TRUE);
	InvalidateRgn (mhwnd, NULL, FALSE);
}

static void MEDragFinish (void)
{
	dragmode = 0;
	SetCursor(LoadCursor(NULL, IDC_ARROW));
}

/* ---- Graphic editor mode (Graph tab) ---- */
/* Graph tab: tile on the left, tools along the bottom, optional palette
 * cluster or bar on the right (panelpro). */

typedef struct {
	int pw, ph;
	int tilex, tiley, tilew, tileh;	/* stretched tile preview */
	int palx, paly, palcell, palw, palh;
	int palcols, palrows;
	int pbtny, pbtnh;			/* CPY/EXG/SPR/TRN/? row */
	int barx[3], bary, barw, barh;
	int valy, hexy;
	int previewx, previewy, previeww, previewh;
	int toolx, tooly, toolw, toolh, ntools;
	int dsx, dsy;
	int statusy;
	int showpanel;
} GELayout;

static int GE_gfxcol = 0;
static int GE_thickness = 1;
static int GE_paint = 0;		/* dragging on the tile */
static int GE_bar = -1;			/* 0=R 1=G 2=B while dragging a bar */
static int GE_palop = 0;		/* 1=CPY 2=EXG 3=SPR 4=pick-from-tile */
static int GE_palsrc = 0;
static int GE_lastpx = 0, GE_lastpy = 0, GE_haslast = 0;
static int GE_mx = 0, GE_my = 0;
static int GE_linemode = 0, GE_linex = 0, GE_liney = 0;
static unsigned char * GE_undo = NULL;
static int GE_undogfx = -1, GE_undolen = 0;
static GELayout gelay;

static int GE_FindBlockForGraphic (int gfx)
{
	int i;
	BLKSTR * blk;
	int unit = blockwidth * blockheight * ((blockdepth + 1) / 8);
	if (unit <= 0) return -1;
	for (i = 0; i < numblockstr; i++) {
		blk = (BLKSTR *) (blockstrpt + i * SOBSTR);
		if ((int)blk->bgoff == gfx * unit)
			return i;
	}
	return -1;
}

static void GEComputeLayout (HWND hwnd, GELayout * L)
{
	RECT rc;
	int palcell, palw, palh, rightx, remain, barh, toolstrip;
	GetClientRect (hwnd, &rc);
	L->pw = rc.right - rc.left;
	L->ph = rc.bottom - rc.top;
	if (L->pw < 80) L->pw = 80;
	if (L->ph < 80) L->ph = 80;

	toolstrip = 48;
	L->showpanel = (panelpro != 0);
	if (panelpro == 2) { L->palcols = 32; L->palrows = 8; }
	else { L->palcols = 16; L->palrows = 16; }

	L->ntools = 7;
	L->toolw = 36;
	L->toolh = 18;
	L->toolx = 8;
	L->tooly = L->ph - 42;
	L->dsx = L->toolx + L->ntools * (L->toolw + 4) + 12;
	L->dsy = L->tooly;
	L->statusy = L->ph - 16;

	if (!L->showpanel) {
		L->tilex = 0;
		L->tiley = 0;
		L->tilew = L->pw - 8;
		L->tileh = L->ph - toolstrip;
		if (L->tileh < 8) L->tileh = 8;
		if (L->tilew < 8) L->tilew = 8;
		{
			int ts = L->tilew < L->tileh ? L->tilew : L->tileh;
			L->tilew = ts;
			L->tileh = ts;
		}
		L->palcell = 12;
		L->palw = L->palh = 0;
		L->palx = L->pw + 64;
		L->paly = 0;
		L->pbtny = 0;
		L->pbtnh = 16;
		L->barw = L->barh = 0;
		L->barx[0] = L->barx[1] = L->barx[2] = L->pw + 64;
		L->bary = L->valy = L->hexy = 0;
		L->previewx = L->pw + 64;
		L->previewy = 0;
		L->previeww = L->previewh = 0;
		return;
	}

	palcell = (panelpro == 2) ? 14 : 20;
	palw = palcell * L->palcols;
	if (palw + 16 > L->pw / 2) {
		palcell = (L->pw / 2 - 16) / L->palcols;
		if (palcell < 8) palcell = 8;
		if (palcell > 24) palcell = 24;
		palw = palcell * L->palcols;
	}
	palh = palcell * L->palrows;
	L->palcell = palcell;
	L->palw = palw;
	L->palh = palh;
	L->palx = L->pw - 8 - palw;
	L->paly = 20;

	rightx = L->palx - 8;
	if (rightx < blockwidth + 8) rightx = L->pw / 2;

	L->tilex = 0;
	L->tiley = 0;
	L->tilew = rightx;
	L->tileh = L->ph - toolstrip;
	if (L->tileh < blockheight) L->tileh = blockheight;
	{
		int ts = L->tilew < L->tileh ? L->tilew : L->tileh;
		if (ts < 8) ts = 8;
		L->tilew = ts;
		L->tileh = ts;
	}

	L->pbtny = L->paly + palh + 4;
	L->pbtnh = 16;

	remain = L->ph - (L->pbtny + L->pbtnh + 4) - 40 - 8;
	if (remain < 80) remain = 80;
	barh = remain * 2 / 5;
	if (barh < 48) barh = 48;
	if (barh > 140) barh = 140;
	L->barh = barh;
	L->barw = (palw - 16) / 3;
	if (L->barw < 10) L->barw = 10;
	L->bary = L->pbtny + L->pbtnh + 6;
	L->barx[0] = L->palx;
	L->barx[1] = L->palx + L->barw + 8;
	L->barx[2] = L->palx + (L->barw + 8) * 2;

	L->valy = L->bary + barh + 4;
	L->hexy = L->valy + 14;

	L->previewx = L->palx;
	L->previewy = L->hexy + 18;
	L->previeww = palw;
	L->previewh = L->ph - L->previewy - 8;
	if (L->previewh < 32) L->previewh = 32;
}

static void GEGetColRGB (int * r, int * g, int * b)
{
	int i = GE_gfxcol;
	if (i < 0) i = 0;
	if (i > 255) i = 255;
	if (cmappt != NULL) {
		*r = cmappt[i * 3];
		*g = cmappt[i * 3 + 1];
		*b = cmappt[i * 3 + 2];
	} else {
		*r = *g = *b = i;
	}
}

static void GESetColChan (int chan, int v)
{
	int i = GE_gfxcol;
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	if (i < 0) i = 0;
	if (i > 255) i = 255;
	if (cmappt == NULL) return;
	cmappt[i * 3 + chan] = (unsigned char) v;
	mapaltered = 1;
}

static int GETileBytes (void)
{
	int bpp = (blockdepth + 1) / 8;
	if (bpp < 1) bpp = 1;
	return blockwidth * blockheight * bpp;
}

static unsigned char * GETilePtr (void)
{
	if (blockgfxpt == NULL || curgfxsel < 0) return NULL;
	return (unsigned char *) blockgfxpt + curgfxsel * GETileBytes ();
}

static void GEPushUndo (void)
{
	int tb = GETileBytes ();
	unsigned char * src = GETilePtr ();
	if (src == NULL || tb <= 0) return;
	if (GE_undo == NULL || GE_undolen != tb) {
		free (GE_undo);
		GE_undo = malloc ((size_t) tb);
		GE_undolen = tb;
	}
	if (GE_undo == NULL) return;
	memcpy (GE_undo, src, (size_t) tb);
	GE_undogfx = curgfxsel;
}

void GEUndo (void)
{
	unsigned char * src;
	int tb;
	if (betype != BETYPE_GRAPH) return;
	src = GETilePtr ();
	tb = GETileBytes ();
	if (src == NULL || GE_undo == NULL || GE_undogfx != curgfxsel || GE_undolen != tb)
		return;
	{
		unsigned char * tmp = malloc ((size_t) tb);
		if (tmp == NULL) return;
		memcpy (tmp, src, (size_t) tb);
		memcpy (src, GE_undo, (size_t) tb);
		memcpy (GE_undo, tmp, (size_t) tb);
		free (tmp);
	}
	mapaltered = 1;
	if (mhwnd) InvalidateRect (mhwnd, NULL, FALSE);
	BEInvalidate ();
}

static void GEWritePix (unsigned char * tile, int x, int y, int col, int r, int g, int b)
{
	int bpp, off;
	if (x < 0 || y < 0 || x >= blockwidth || y >= blockheight) return;
	bpp = (blockdepth + 1) / 8;
	if (bpp < 1) bpp = 1;
	off = (y * blockwidth + x) * bpp;
	if (blockdepth == 8)
		tile[off] = (unsigned char) col;
	else if (blockdepth == 15) {
		unsigned short p = (unsigned short) (((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
		tile[off] = (unsigned char) (p >> 8);
		tile[off + 1] = (unsigned char) (p & 0xFF);
	} else if (blockdepth == 16) {
		unsigned short p = (unsigned short) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
		tile[off] = (unsigned char) (p >> 8);
		tile[off + 1] = (unsigned char) (p & 0xFF);
	} else if (blockdepth == 32) {
		tile[off] = 0;
		tile[off + 1] = (unsigned char) r;
		tile[off + 2] = (unsigned char) g;
		tile[off + 3] = (unsigned char) b;
	} else {
		tile[off] = (unsigned char) b;
		tile[off + 1] = (unsigned char) g;
		tile[off + 2] = (unsigned char) r;
	}
}

static void GEPlot (int px, int py)
{
	int dx, dy, r, g, b, t;
	unsigned char * tile = GETilePtr ();
	if (tile == NULL) return;
	GEGetColRGB (&r, &g, &b);
	t = GE_thickness;
	if (t < 1) t = 1;
	for (dy = 0; dy < t; dy++) {
		for (dx = 0; dx < t; dx++)
			GEWritePix (tile, px + dx, py + dy, GE_gfxcol, r, g, b);
	}
	GE_lastpx = px; GE_lastpy = py; GE_haslast = 1;
}

static void GELine (int x0, int y0, int x1, int y1)
{
	int dx = abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy, e2;
	for (;;) {
		GEPlot (x0, y0);
		if (x0 == x1 && y0 == y1) break;
		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

static void GEFill (int px, int py)
{
	int bpp, n, sp, r, g, b;
	unsigned char * tile, * seen, tgt[4], cur[4];
	int * st;
	if (px < 0 || py < 0 || px >= blockwidth || py >= blockheight) return;
	tile = GETilePtr ();
	if (tile == NULL) return;
	bpp = (blockdepth + 1) / 8;
	if (bpp < 1) bpp = 1;
	if (bpp > 4) bpp = 4;
	memcpy (tgt, tile + (py * blockwidth + px) * bpp, (size_t) bpp);
	GEGetColRGB (&r, &g, &b);
	{
		unsigned char tmp[16];
		memset (tmp, 0, sizeof (tmp));
		GEWritePix (tmp, 0, 0, GE_gfxcol, r, g, b);
		memcpy (cur, tmp, (size_t) bpp);
	}
	if (memcmp (tgt, cur, (size_t) bpp) == 0) return;
	n = blockwidth * blockheight;
	seen = calloc ((size_t) n, 1);
	st = malloc ((size_t) n * 2 * sizeof (int));
	if (seen == NULL || st == NULL) { free (seen); free (st); return; }
	sp = 0;
	st[sp++] = px; st[sp++] = py;
	seen[py * blockwidth + px] = 1;
	while (sp > 0) {
		int y = st[--sp];
		int x = st[--sp];
		int k;
		GEWritePix (tile, x, y, GE_gfxcol, r, g, b);
		for (k = 0; k < 4; k++) {
			static const int ox[4] = { 1, -1, 0, 0 };
			static const int oy[4] = { 0, 0, 1, -1 };
			int nx = x + ox[k], ny = y + oy[k];
			if (nx < 0 || ny < 0 || nx >= blockwidth || ny >= blockheight) continue;
			if (seen[ny * blockwidth + nx]) continue;
			if (memcmp (tile + (ny * blockwidth + nx) * bpp, tgt, (size_t) bpp) != 0)
				continue;
			seen[ny * blockwidth + nx] = 1;
			st[sp++] = nx; st[sp++] = ny;
		}
	}
	free (seen);
	free (st);
	GE_lastpx = px; GE_lastpy = py; GE_haslast = 1;
}

static void GEDrawMapPreview (HDC hdc, const GELayout * L)
{
	int nx, ny, tw, th, i, j, pitch, bw, bh;
	unsigned char * buf;
	HDC fromhdc;
	HBITMAP temphbm, hbmpold;
	BITMAPINFOHEADER bih;
	RECT prc;

	prc.left = L->previewx; prc.top = L->previewy;
	prc.right = L->previewx + L->previeww; prc.bottom = L->previewy + L->previewh;
	FillRect (hdc, &prc, (HBRUSH) GetStockObject (BLACK_BRUSH));

	if (mappt == NULL || blockgfxpt == NULL || blockwidth < 1 || blockheight < 1)
		return;
	if (L->previeww < 4 || L->previewh < 4) return;

	tw = blockwidth;
	th = blockheight;
	nx = L->previeww / tw;
	ny = L->previewh / th;
	if (nx < 1) nx = 1;
	if (ny < 1) ny = 1;
	if (nx > mapwidth) nx = mapwidth;
	if (ny > mapheight) ny = mapheight;

	bw = nx * tw;
	bh = ny * th;
	pitch = ((bw * 3) + 3) & ~3;
	buf = malloc ((size_t) pitch * bh);
	if (buf == NULL) return;
	memset (buf, 0, (size_t) pitch * bh);

	for (j = 0; j < ny; j++) {
		for (i = 0; i < nx; i++) {
			int mx = xmapoffset + i;
			int my = ymapoffset + j;
			if (mx >= 0 && my >= 0 && mx < mapwidth && my < mapheight)
				Blockto24 (mx, my, buf, i * tw, j * th, pitch);
		}
	}

	memset (&bih, 0, sizeof (bih));
	bih.biSize = sizeof (bih);
	bih.biWidth = bw;
	bih.biHeight = -bh;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;

	fromhdc = CreateCompatibleDC (hdc);
	temphbm = CreateDIBitmap (hdc, &bih, CBM_INIT, buf, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
	hbmpold = SelectObject (fromhdc, temphbm);
	StretchBlt (hdc, L->previewx, L->previewy, L->previeww, L->previewh,
		fromhdc, 0, 0, bw, bh, SRCCOPY);
	SelectObject (fromhdc, hbmpold);
	DeleteObject (temphbm);
	DeleteDC (fromhdc);
	free (buf);
}

static void GEApplyBarAtY (int chan, int my)
{
	int v;
	if (gelay.barh <= 1) return;
	v = 255 - ((my - gelay.bary) * 255) / (gelay.barh - 1);
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	GESetColChan (chan, v);
}

static int GEIsTranspPixel (unsigned char * p)
{
	if (blockdepth == 8)
		return *p == trans8bit;
	if (blockdepth == 15 || blockdepth == 16)
		return p[0] == trans16hi && p[1] == trans16low;
	if (blockdepth == 32)
		return p[1] == transred && p[2] == transgreen && p[3] == transblue;
	return p[0] == transblue && p[1] == transgreen && p[2] == transred;
}

static void GEMaskWithClickmask (void)
{
	int bpp, tilebytes, i;
	unsigned char * src, * msk;
	if (blockgfxpt == NULL || curgfxsel < 0) return;
	bpp = (blockdepth + 1) / 8;
	if (bpp < 1) bpp = 1;
	tilebytes = blockwidth * blockheight * bpp;
	if (clickmask < 0 || clickmask >= numblockgfx) return;
	src = (unsigned char *) blockgfxpt + curgfxsel * tilebytes;
	msk = (unsigned char *) blockgfxpt + clickmask * tilebytes;
	for (i = 0; i < blockwidth * blockheight; i++) {
		if (GEIsTranspPixel (msk + i * bpp))
			memset (src + i * bpp, 0, (size_t) bpp);
	}
}

static void GEReadPixRGB (unsigned char * tile, int px, int py, int * r, int * g, int * b)
{
	int bpp, off;
	bpp = (blockdepth + 1) / 8;
	if (bpp < 1) bpp = 1;
	off = (py * blockwidth + px) * bpp;
	if (blockdepth == 8) {
		int c = tile[off];
		if (cmappt != NULL) { *r = cmappt[c * 3]; *g = cmappt[c * 3 + 1]; *b = cmappt[c * 3 + 2]; }
		else *r = *g = *b = c;
	} else if (blockdepth == 32) { *r = tile[off + 1]; *g = tile[off + 2]; *b = tile[off + 3]; }
	else if (blockdepth == 24) { *b = tile[off]; *g = tile[off + 1]; *r = tile[off + 2]; }
	else if (blockdepth == 16) {
		unsigned short p = (unsigned short) ((tile[off] << 8) | tile[off + 1]);
		*r = ((p >> 11) & 31) << 3; *g = ((p >> 5) & 63) << 2; *b = (p & 31) << 3;
	} else if (blockdepth == 15) {
		unsigned short p = (unsigned short) ((tile[off] << 8) | tile[off + 1]);
		*r = ((p >> 10) & 31) << 3; *g = ((p >> 5) & 31) << 3; *b = (p & 31) << 3;
	} else *r = *g = *b = 0;
}

static int GENearestPal (int r, int g, int b)
{
	int i, best = 0, bestd = 0x7fffffff;
	if (cmappt == NULL) return 0;
	for (i = 0; i < 256; i++) {
		int dr = cmappt[i * 3] - r, dg = cmappt[i * 3 + 1] - g, db = cmappt[i * 3 + 2] - b;
		int d = dr * dr + dg * dg + db * db;
		if (d < bestd) { bestd = d; best = i; }
	}
	return best;
}

static void GEPickAt (int px, int py, int findpal)
{
	unsigned char * tile;
	int r, g, b;
	if (px < 0 || py < 0 || px >= blockwidth || py >= blockheight) return;
	tile = GETilePtr ();
	if (tile == NULL) return;
	if (blockdepth == 8) {
		int bpp = 1;
		GE_gfxcol = tile[(py * blockwidth + px) * bpp];
		return;
	}
	GEReadPixRGB (tile, px, py, &r, &g, &b);
	if (findpal)
		GE_gfxcol = GENearestPal (r, g, b);
	else if (cmappt != NULL && GE_gfxcol >= 0 && GE_gfxcol < 256) {
		cmappt[GE_gfxcol * 3] = (unsigned char) r;
		cmappt[GE_gfxcol * 3 + 1] = (unsigned char) g;
		cmappt[GE_gfxcol * 3 + 2] = (unsigned char) b;
		mapaltered = 1;
	}
}

static void GEPaletteOp (int dest)
{
	int s = GE_palsrc, d = dest;
	if (!cmappt || s < 0 || d < 0 || s > 255 || d > 255) { GE_palop = 0; return; }
	if (GE_palop == 1) {
		memcpy (cmappt + d * 3, cmappt + s * 3, 3);
	} else if (GE_palop == 2) {
		unsigned char tmp[3];
		memcpy (tmp, cmappt + d * 3, 3);
		memcpy (cmappt + d * 3, cmappt + s * 3, 3);
		memcpy (cmappt + s * 3, tmp, 3);
	} else if (GE_palop == 3) {
		int a = s < d ? s : d, b = s < d ? d : s, i;
		int r0 = cmappt[a * 3], g0 = cmappt[a * 3 + 1], b0 = cmappt[a * 3 + 2];
		int r1 = cmappt[b * 3], g1 = cmappt[b * 3 + 1], b1 = cmappt[b * 3 + 2];
		int n = b - a;
		if (n < 1) n = 1;
		for (i = 0; i <= n; i++) {
			cmappt[(a + i) * 3]     = (unsigned char) (r0 + (r1 - r0) * i / n);
			cmappt[(a + i) * 3 + 1] = (unsigned char) (g0 + (g1 - g0) * i / n);
			cmappt[(a + i) * 3 + 2] = (unsigned char) (b0 + (b1 - b0) * i / n);
		}
	}
	GE_palop = 0;
}

static void GEDoTool (int t)
{
	int bpp, tilebytes, y;
	unsigned char * src;
	if (blockgfxpt == NULL || curgfxsel < 0) return;
	bpp = (blockdepth + 1) / 8;
	if (bpp < 1) bpp = 1;
	tilebytes = blockwidth * blockheight * bpp;
	src = (unsigned char *) blockgfxpt + curgfxsel * tilebytes;
	if (t >= 0 && t <= 4) GEPushUndo ();
	if (t == 0) { /* FLH */
		for (y = 0; y < blockheight; y++) {
			int x;
			unsigned char * row = src + y * blockwidth * bpp;
			for (x = 0; x < blockwidth / 2; x++) {
				int lx = x, rx2 = blockwidth - 1 - x, k;
				for (k = 0; k < bpp; k++) {
					unsigned char tmp = row[lx * bpp + k];
					row[lx * bpp + k] = row[rx2 * bpp + k];
					row[rx2 * bpp + k] = tmp;
				}
			}
		}
	} else if (t == 1) { /* FLV */
		for (y = 0; y < blockheight / 2; y++) {
			unsigned char * top = src + y * blockwidth * bpp;
			unsigned char * bot = src + (blockheight - 1 - y) * blockwidth * bpp;
			int k, rowbytes = blockwidth * bpp;
			for (k = 0; k < rowbytes; k++) {
				unsigned char tmp = top[k]; top[k] = bot[k]; bot[k] = tmp;
			}
		}
	} else if (t == 2) { /* ROT 90 clockwise, square tiles only */
		if (blockwidth == blockheight) {
			unsigned char * tmp = malloc ((size_t) tilebytes);
			if (tmp) {
				int x;
				for (y = 0; y < blockheight; y++) {
					for (x = 0; x < blockwidth; x++) {
						int k;
						for (k = 0; k < bpp; k++)
							tmp[(x * blockwidth + (blockwidth - 1 - y)) * bpp + k] =
								src[(y * blockwidth + x) * bpp + k];
					}
				}
				memcpy (src, tmp, (size_t) tilebytes);
				free (tmp);
			}
		}
	} else if (t == 3) { /* CLR: fill with current colour */
		int x, r, g, b;
		GEGetColRGB (&r, &g, &b);
		for (y = 0; y < blockheight; y++)
			for (x = 0; x < blockwidth; x++)
				GEWritePix (src, x, y, GE_gfxcol, r, g, b);
	} else if (t == 4) { /* MSK */
		GEMaskWithClickmask ();
	} else if (t == 5) { /* BLK */
		int blk = GE_FindBlockForGraphic (curgfxsel);
		if (blk >= 0) curstr = (short int) blk;
	} else if (t == 6) { /* REV: undo last draw */
		GEUndo ();
		return;
	}
	mapaltered = 1;
}

static void MEDrawGraphicEditor (HWND phwnd, HDC myhdc)
{
	GELayout * L = &gelay;
	HDC fromhdc;
	HBITMAP temphbm, hbmpold;
	unsigned char * tb;
	BITMAPINFOHEADER bih;
	char txt[80];
	int r, g, b, cx, cy, step, pitch;
	RECT prc;
	const GELayout * cl;

	GEComputeLayout (phwnd, L);
	cl = L;

	prc.left = 0; prc.top = 0; prc.right = L->pw; prc.bottom = L->ph;
	{
		COLORREF bg = prographbg ? RGB (0x40, 0x40, 0x40) : RGB (0, 0, 0);
		HBRUSH hbr = CreateSolidBrush (bg);
		FillRect (myhdc, &prc, hbr);
		DeleteObject (hbr);
	}

	if (blockgfxpt == NULL || numblockgfx <= 0) {
		SetBkColor (myhdc, RGB (0, 0, 0));
		SetTextColor (myhdc, RGB (255, 255, 255));
		TextOut (myhdc, 8, 8, "No graphics loaded", 18);
		return;
	}
	if (curgfxsel < 0) curgfxsel = 0;
	if (curgfxsel >= numblockgfx) curgfxsel = numblockgfx - 1;

	pitch = ((blockwidth * 3) + 3) & ~3;
	tb = malloc ((size_t) pitch * blockheight);
	if (tb == NULL) return;
	memset (tb, 0, (size_t) pitch * blockheight);
	BlockGfxto24 (curgfxsel, tb, 0, 0, pitch);

	fromhdc = CreateCompatibleDC (myhdc);
	memset (&bih, 0, sizeof (bih));
	bih.biSize = sizeof (bih);
	bih.biWidth = blockwidth;
	bih.biHeight = -blockheight;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;
	temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT, tb, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
	hbmpold = SelectObject (fromhdc, temphbm);
	StretchBlt (myhdc, L->tilex, L->tiley, L->tilew, L->tileh,
		fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
	SelectObject (fromhdc, hbmpold);
	DeleteObject (temphbm);
	DeleteDC (fromhdc);
	free (tb);

	{
		HPEN hpen, hold;
		int gx, gy;
		hpen = CreatePen (PS_SOLID, 1, RGB (0xC0, 0xC0, 0xC0));
		hold = SelectObject (myhdc, hpen);
		for (gx = 1; gx < blockwidth; gx++) {
			int sx = L->tilex + gx * L->tilew / blockwidth;
			MoveToEx (myhdc, sx, L->tiley, NULL);
			LineTo (myhdc, sx, L->tiley + L->tileh);
		}
		for (gy = 1; gy < blockheight; gy++) {
			int sy = L->tiley + gy * L->tileh / blockheight;
			MoveToEx (myhdc, L->tilex, sy, NULL);
			LineTo (myhdc, L->tilex + L->tilew, sy);
		}
		SelectObject (myhdc, hold);
		DeleteObject (hpen);
	}

	GEGetColRGB (&r, &g, &b);
	SetBkColor (myhdc, prographbg ? RGB (0x40, 0x40, 0x40) : RGB (0, 0, 0));
	SetTextColor (myhdc, RGB (255, 255, 255));
	if (L->showpanel) {
	sprintf (txt, "Palette (col %d/255)", GE_gfxcol);
	TextOut (myhdc, L->palx, 4, txt, (int) strlen (txt));

	for (cy = 0; cy < L->palrows; cy++) {
		for (cx = 0; cx < L->palcols; cx++) {
			int ci = cy * L->palcols + cx;
			if (ci > 255) continue;
			HBRUSH hbr;
			prc.left = L->palx + cx * L->palcell;
			prc.top = L->paly + cy * L->palcell;
			prc.right = prc.left + L->palcell;
			prc.bottom = prc.top + L->palcell;
			if (cmappt != NULL)
				hbr = CreateSolidBrush (RGB (cmappt[ci * 3], cmappt[ci * 3 + 1], cmappt[ci * 3 + 2]));
			else
				hbr = CreateSolidBrush (RGB (ci, ci, ci));
			FillRect (myhdc, &prc, hbr);
			DeleteObject (hbr);
			if (ci == GE_gfxcol) {
				HPEN hpen2, hold2;
				HBRUSH holdbr;
				hpen2 = CreatePen (PS_SOLID, 2, RGB (255, 255, 255));
				hold2 = SelectObject (myhdc, hpen2);
				holdbr = SelectObject (myhdc, GetStockObject (NULL_BRUSH));
				Rectangle (myhdc, prc.left, prc.top, prc.right, prc.bottom);
				SelectObject (myhdc, hold2);
				SelectObject (myhdc, holdbr);
				DeleteObject (hpen2);
			}
		}
	}

	{
		const char * palbtns[] = { "CPY", "EXG", "SPR", "TRN", " ? " };
		int bi, bw = L->palw / 5;
		if (bw < 28) bw = 28;
		for (bi = 0; bi < 5; bi++) {
			RECT brc;
			int selected = (GE_palop == bi + 1);
			brc.left = L->palx + bi * bw;
			brc.top = L->pbtny;
			brc.right = brc.left + bw - 2;
			brc.bottom = L->pbtny + L->pbtnh;
			FillRect (myhdc, &brc, (HBRUSH) GetStockObject (selected ? LTGRAY_BRUSH : WHITE_BRUSH));
			SetBkColor (myhdc, selected ? RGB (192, 192, 192) : RGB (255, 255, 255));
			SetTextColor (myhdc, RGB (0, 0, 0));
			TextOut (myhdc, brc.left + 4, L->pbtny + 1, palbtns[bi], 3);
		}
	}

	for (step = 0; step < L->barh; step++) {
		int v = 255 - step * 255 / (L->barh - 1);
		int ch;
		int handle[3];
		handle[0] = r; handle[1] = g; handle[2] = b;
		for (ch = 0; ch < 3; ch++) {
			HBRUSH hbr;
			COLORREF col;
			if (v == handle[ch] || (L->barh > 1 && step == (255 - handle[ch]) * (L->barh - 1) / 255))
				col = RGB (0xF0, 0xF0, 0xF0);
			else if (ch == 0) col = RGB (v, 0, 0);
			else if (ch == 1) col = RGB (0, v, 0);
			else col = RGB (0, 0, v);
			hbr = CreateSolidBrush (col);
			prc.left = L->barx[ch];
			prc.top = L->bary + step;
			prc.right = L->barx[ch] + L->barw;
			prc.bottom = prc.top + 1;
			FillRect (myhdc, &prc, hbr);
			DeleteObject (hbr);
		}
	}

	SetBkColor (myhdc, RGB (0, 0, 0));
	SetTextColor (myhdc, RGB (255, 255, 255));
	{
		int ch, vals[3];
		vals[0] = r; vals[1] = g; vals[2] = b;
		for (ch = 0; ch < 3; ch++) {
			sprintf (txt, "%03d", vals[ch]);
			TextOut (myhdc, L->barx[ch], L->valy, txt, 3);
			sprintf (txt, "$%02X", vals[ch]);
			TextOut (myhdc, L->barx[ch], L->hexy, txt, 4);
		}
	}

	GEDrawMapPreview (myhdc, cl);
	}

	{
		const char * tools[] = { "FLH", "FLV", "ROT", "CLR", "MSK", "BLK", "REV" };
		int ti;
		for (ti = 0; ti < L->ntools; ti++) {
			RECT trc;
			trc.left = L->toolx + ti * (L->toolw + 4);
			trc.top = L->tooly;
			trc.right = trc.left + L->toolw;
			trc.bottom = L->tooly + L->toolh;
			DrawEdge (myhdc, &trc, EDGE_RAISED, BF_RECT);
			SetBkColor (myhdc, RGB (0, 0, 0));
			SetTextColor (myhdc, RGB (255, 255, 255));
			TextOut (myhdc, trc.left + 6, L->tooly + 2, tools[ti], 3);
		}
	}

	{
		int ds;
		SetBkColor (myhdc, RGB (0, 0, 0));
		SetTextColor (myhdc, RGB (255, 255, 255));
		TextOut (myhdc, L->dsx, L->tooly - 14, "Draw size:", 10);
		for (ds = 1; ds <= 4; ds++) {
			RECT drc;
			drc.left = L->dsx + (ds - 1) * 24;
			drc.top = L->dsy;
			drc.right = drc.left + 22;
			drc.bottom = L->dsy + L->toolh;
			if (ds == GE_thickness) {
				FillRect (myhdc, &drc, (HBRUSH) GetStockObject (WHITE_BRUSH));
				SetBkColor (myhdc, RGB (255, 255, 255));
				SetTextColor (myhdc, RGB (0, 0, 0));
			} else {
				DrawEdge (myhdc, &drc, EDGE_RAISED, BF_RECT);
				SetBkColor (myhdc, RGB (0, 0, 0));
				SetTextColor (myhdc, RGB (255, 255, 255));
			}
			{
				char dsch[2] = { (char) ('0' + ds), 0 };
				TextOut (myhdc, drc.left + 7, L->dsy + 2, dsch, 1);
			}
		}
	}

	{
		int blk = GE_FindBlockForGraphic (curgfxsel);
		SetBkColor (myhdc, RGB (0, 0, 0));
		SetTextColor (myhdc, RGB (0xC0, 0xC0, 0xC0));
		if (blk >= 0)
			sprintf (txt, "This graphic is used by block %d", blk);
		else
			sprintf (txt, "This graphic is not used by any block (click BLK to make one)");
		TextOut (myhdc, 8, L->statusy, txt, (int) strlen (txt));
	}
}

static int GETileXY (int mx, int my, int * px, int * py)
{
	if (mx < gelay.tilex || my < gelay.tiley ||
		mx >= gelay.tilex + gelay.tilew || my >= gelay.tiley + gelay.tileh)
		return 0;
	if (gelay.tilew < 1 || gelay.tileh < 1) return 0;
	*px = (mx - gelay.tilex) * blockwidth / gelay.tilew;
	*py = (my - gelay.tiley) * blockheight / gelay.tileh;
	if (*px < 0) *px = 0;
	if (*py < 0) *py = 0;
	if (*px >= blockwidth) *px = blockwidth - 1;
	if (*py >= blockheight) *py = blockheight - 1;
	return 1;
}

static int GEDoTileAction (HWND hwnd, int act, int px, int py)
{
	if (act == 0) return 1;
	if (act == 2) GE_gfxcol = 0;
	if (act == 6) {
		GEPickAt (px, py, 1);
		InvalidateRect (hwnd, NULL, FALSE);
		return 1;
	}
	if (act == 8) {
		GEPushUndo ();
		GEFill (px, py);
		mapaltered = 1;
		InvalidateRect (hwnd, NULL, FALSE);
		BEInvalidate ();
		return 1;
	}
	if (act == 9) {
		if (!GE_linemode) {
			GE_linemode = 1;
			GE_linex = px; GE_liney = py;
		} else {
			GEPushUndo ();
			GELine (GE_linex, GE_liney, px, py);
			GE_linemode = 0;
			mapaltered = 1;
			BEInvalidate ();
		}
		InvalidateRect (hwnd, NULL, FALSE);
		return 1;
	}
	if (act != 1 && act != 2) return 0;
	GEPushUndo ();
	GE_paint = 1;
	SetCapture (hwnd);
	GEPlot (px, py);
	mapaltered = 1;
	InvalidateRect (hwnd, NULL, FALSE);
	BEInvalidate ();
	return 1;
}

static int GEOnLButtonDown (HWND hwnd, int mx, int my)
{
	int px, py, ch, t, ds, bw;
	GEComputeLayout (hwnd, &gelay);
	GE_mx = mx; GE_my = my;

	if (gelay.showpanel && mx >= gelay.palx && my >= gelay.paly &&
		mx < gelay.palx + gelay.palw && my < gelay.paly + gelay.palh) {
		int ci = ((mx - gelay.palx) / gelay.palcell) +
			((my - gelay.paly) / gelay.palcell) * gelay.palcols;
		if (ci < 0) ci = 0;
		if (ci > 255) ci = 255;
		if (GE_palop >= 1 && GE_palop <= 3) {
			GEPaletteOp (ci);
			GE_gfxcol = ci;
		} else {
			GE_gfxcol = ci;
			GE_palop = 0;
		}
		InvalidateRect (hwnd, NULL, FALSE);
		return 1;
	}

	bw = gelay.palw / 5;
	if (bw < 28) bw = 28;
	if (gelay.showpanel && my >= gelay.pbtny && my < gelay.pbtny + gelay.pbtnh &&
		mx >= gelay.palx && mx < gelay.palx + gelay.palw) {
		int bi = (mx - gelay.palx) / bw;
		if (bi >= 0 && bi < 5) {
			if (bi == 3) {
				/* TRN: write colourkey into the current palette slot */
				if (cmappt && GE_gfxcol >= 0 && GE_gfxcol < 256) {
					if (blockdepth == 8) {
						cmappt[GE_gfxcol * 3] = cmappt[trans8bit * 3];
						cmappt[GE_gfxcol * 3 + 1] = cmappt[trans8bit * 3 + 1];
						cmappt[GE_gfxcol * 3 + 2] = cmappt[trans8bit * 3 + 2];
					} else {
						cmappt[GE_gfxcol * 3] = transred;
						cmappt[GE_gfxcol * 3 + 1] = transgreen;
						cmappt[GE_gfxcol * 3 + 2] = transblue;
					}
				}
				GE_palop = 0;
			} else {
				GE_palop = bi + 1; /* 1 CPY 2 EXG 3 SPR 4 pick */
				GE_palsrc = GE_gfxcol;
			}
			InvalidateRect (hwnd, NULL, FALSE);
			return 1;
		}
	}

	for (ch = 0; gelay.showpanel && ch < 3; ch++) {
		if (mx >= gelay.barx[ch] && mx < gelay.barx[ch] + gelay.barw &&
			my >= gelay.bary && my < gelay.bary + gelay.barh) {
			GE_bar = ch;
			GEApplyBarAtY (ch, my);
			SetCapture (hwnd);
			InvalidateRect (hwnd, NULL, FALSE);
			return 1;
		}
	}

	if (my >= gelay.tooly && my < gelay.tooly + gelay.toolh) {
		for (t = 0; t < gelay.ntools; t++) {
			int x0 = gelay.toolx + t * (gelay.toolw + 4);
			if (mx >= x0 && mx < x0 + gelay.toolw) {
				GEDoTool (t);
				InvalidateRect (hwnd, NULL, FALSE);
				BEInvalidate ();
				return 1;
			}
		}
	}

	if (my >= gelay.dsy && my < gelay.dsy + gelay.toolh) {
		for (ds = 1; ds <= 4; ds++) {
			int x0 = gelay.dsx + (ds - 1) * 24;
			if (mx >= x0 && mx < x0 + 22) {
				GE_thickness = ds;
				InvalidateRect (hwnd, NULL, FALSE);
				return 1;
			}
		}
	}

	if (GETileXY (mx, my, &px, &py)) {
		if (GE_palop == 4) {
			GEPickAt (px, py, GetKeyState (VK_CONTROL) < 0);
			GE_palop = 0;
			InvalidateRect (hwnd, NULL, FALSE);
			return 1;
		}
		if (GetKeyState (VK_CONTROL) < 0) {
			GEPickAt (px, py, 1);
			InvalidateRect (hwnd, NULL, FALSE);
			return 1;
		}
		GEDoTileAction (hwnd, mb_l, px, py);
		return 1;
	}
	return 1;
}

static int GEOnRButtonDown (HWND hwnd, int mx, int my)
{
	int px, py;
	GEComputeLayout (hwnd, &gelay);
	GE_mx = mx; GE_my = my;
	if (GETileXY (mx, my, &px, &py)) {
		GEDoTileAction (hwnd, mb_r, px, py);
		return 1;
	}
	return 1;
}

static int GEOnMouseMove (HWND hwnd, int mx, int my)
{
	int px, py;
	GE_mx = mx; GE_my = my;
	if (GE_bar >= 0) {
		GEApplyBarAtY (GE_bar, my);
		InvalidateRect (hwnd, NULL, FALSE);
		return 1;
	}
	if (GE_paint) {
		GEComputeLayout (hwnd, &gelay);
		if (GETileXY (mx, my, &px, &py)) {
			GEPlot (px, py);
			mapaltered = 1;
			InvalidateRect (hwnd, NULL, FALSE);
			BEInvalidate ();
		}
		return 1;
	}
	return 0;
}

static void GEOnLButtonUp (HWND hwnd)
{
	if (GE_bar >= 0 || GE_paint) {
		GE_bar = -1;
		GE_paint = 0;
		ReleaseCapture ();
		InvalidateRect (hwnd, NULL, FALSE);
		BEInvalidate ();
	}
}

int GESlide (int dx, int dy)
{
	int bpp, x, y, tb;
	unsigned char * src, * tmp;
	if (betype != BETYPE_GRAPH) return 0;
	src = GETilePtr ();
	if (src == NULL || (dx == 0 && dy == 0)) return 1;
	bpp = (blockdepth + 1) / 8;
	if (bpp < 1) bpp = 1;
	tb = GETileBytes ();
	tmp = malloc ((size_t) tb);
	if (tmp == NULL) return 1;
	GEPushUndo ();
	memcpy (tmp, src, (size_t) tb);
	for (y = 0; y < blockheight; y++) {
		for (x = 0; x < blockwidth; x++) {
			int sx = (x - dx) % blockwidth;
			int sy = (y - dy) % blockheight;
			if (sx < 0) sx += blockwidth;
			if (sy < 0) sy += blockheight;
			memcpy (src + (y * blockwidth + x) * bpp,
				tmp + (sy * blockwidth + sx) * bpp, (size_t) bpp);
		}
	}
	free (tmp);
	mapaltered = 1;
	if (mhwnd) InvalidateRect (mhwnd, NULL, FALSE);
	BEInvalidate ();
	return 1;
}

static int GEMouseTile (int * px, int * py)
{
	if (mhwnd == NULL) return 0;
	GEComputeLayout (mhwnd, &gelay);
	return GETileXY (GE_mx, GE_my, px, py);
}

int GEAccelFill (void)
{
	int px, py;
	if (betype != BETYPE_GRAPH) return 0;
	if (!GEMouseTile (&px, &py)) return 1;
	GEPushUndo ();
	GEFill (px, py);
	mapaltered = 1;
	InvalidateRect (mhwnd, NULL, FALSE);
	BEInvalidate ();
	return 1;
}

int GEAccelPick (void)
{
	int px, py;
	if (betype != BETYPE_GRAPH) return 0;
	if (!GEMouseTile (&px, &py)) return 1;
	GEPickAt (px, py, 0);
	InvalidateRect (mhwnd, NULL, FALSE);
	return 1;
}

int GEAccelLine (void)
{
	int px, py;
	if (betype != BETYPE_GRAPH) return 0;
	if (!GEMouseTile (&px, &py) || !GE_haslast) return 1;
	GEPushUndo ();
	GELine (GE_lastpx, GE_lastpy, px, py);
	mapaltered = 1;
	InvalidateRect (mhwnd, NULL, FALSE);
	BEInvalidate ();
	return 1;
}

/* ---- End Tier-2 helpers ---- */

LRESULT CALLBACK MapWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
//char dropname[256];
int i, xblk, yblk;
HDC myhdc;
PAINTSTRUCT ps;

	switch (message) {
	case WM_PAINT:
		myhdc = BeginPaint (hwnd, &ps);
		if (blockgfxpt!=NULL) {
			if (betype == BETYPE_GRAPH)
				MEDrawGraphicEditor (hwnd, myhdc);
			else
				MEDrawMap (hwnd, myhdc);
		}
		EndPaint (hwnd, &ps);
//		ReleaseDC (hwnd, myhdc);
		break;
	case WM_LBUTTONDOWN:
		if (betype == BETYPE_GRAPH) {
			GEOnLButtonDown (hwnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			return 0;
		}
		if (mappt==NULL) return 0;
		if (objplace) {
			/* object placement: click places selected, ctrl+click duplicates */
			int mpx = ZD (LOWORD(lParam)) + xmapoffset * blockwidth;
			int mpy = ZD (HIWORD(lParam)) + ymapoffset * blockheight;
			if (GetKeyState (VK_CONTROL) < 0) {
				const F14Obj * src = F14_GetObject (F14_CurObject ());
				if (src != NULL) {
					int ni = F14_NewObject ();
					if (ni >= 0) {
						F14_SetObject (ni, src);
						F14_SetCurObject (ni);
					}
				}
			}
			MECopyToUndo ();
			OE_PlaceSelected (mpx, mpy, GetKeyState (VK_SHIFT) < 0);
			mapaltered = 1;
			InvalidateRgn (mhwnd, NULL, FALSE);
			return 0;
		}
		if (MBDoAction (mb_l, hwnd, lParam)) return 0;
		xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
		yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
		if (linemode) {
			/* line tool: first click sets start, second draws */
			if (linemode==1) {
				linecx = xblk; linecy = yblk;
				linemode = 2;
			} else if (linemode==2) {
				MECopyToUndo ();
				MEBresenhamLine (linecx, linecy, xblk, yblk);
				mapaltered = 1;
				linemode = 0;
				InvalidateRgn (mhwnd, NULL, FALSE);
			}
			MEShowCoords (xblk, yblk);
			return 0;
		}
		if (pathmode) {
			/* lay-path: click to place tiles along mouse path */
			if (pathmode==1) {
				pathcx = xblk; pathcy = yblk;
				MECopyToUndo ();
				curplacetile = -1;
				PlaceTile (xblk, yblk);
				mapaltered = 1;
				if (blockstaggerx || blockstaggery) InvalidateRgn (mhwnd, NULL, FALSE);
				else MEDrawUnderTile (hwnd, xblk, yblk);
				pathmode = 2;
			} else if (pathmode==2) {
				MEBresenhamLine (pathcx, pathcy, xblk, yblk);
				pathcx = xblk; pathcy = yblk;
				mapaltered = 1;
				if (blockstaggerx || blockstaggery) InvalidateRgn (mhwnd, NULL, FALSE);
				else InvalidateRgn (mhwnd, NULL, FALSE);
			}
			MEShowCoords (xblk, yblk);
			return 0;
		}
		MECopyToUndo ();
		curplacetile = -1; painton = 1;
		meoldbx = -1; meoldby = -1; mapaltered = 1;
		if (megetabrush) { gettingbrush = megetabrush; megetabrush = 0;
		xblk = MEGetX(LOWORD(lParam), HIWORD(lParam));
		yblk = MEGetY(LOWORD(lParam), HIWORD(lParam));
		if ((blockstaggerx || blockstaggery) && yblk&1) yblk--;
		MEBrushRect (hwnd, xblk, yblk, xblk, yblk);
		mebrushx = xblk; mebrushx2 = mebrushx;
		mebrushy = yblk; mebrushy2 = mebrushy; }
/* deliberately run into mousemove */
	case WM_MOUSEMOVE:
		if (betype == BETYPE_GRAPH) {
			GEOnMouseMove (hwnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			return 0;
		}
		if (mappt==NULL) return 0;
		if (dragmode) {
			MEDragMap (LOWORD(lParam), HIWORD(lParam));
			return 0;
		}
		xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
		yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
		LuaSetMouseBlock (lua_mb_act==10 ? lua_area_x : xblk,
			lua_mb_act==10 ? lua_area_y : yblk,
			lua_mb_act==10 ? xblk : xblk,
			lua_mb_act==10 ? yblk : yblk);
		if (lua_mb_act == 5 && (GetAsyncKeyState(VK_LBUTTON)<0 ||
			GetAsyncKeyState(VK_RBUTTON)<0 || GetAsyncKeyState(VK_MBUTTON)<0)) {
			if (meoldbx != xblk || meoldby != yblk)
				LuaRunMouseButton (5, xblk, yblk);
		}
		if (GetAsyncKeyState(VK_LBUTTON)>=0 && !gettingbrush &&
			GetAsyncKeyState(VK_RBUTTON)>=0) painton = 0;
		if (meoldbx != xblk || meoldby != yblk) {
			if (painton) {
				if (gettingbrush) {
					MEBrushRect (hwnd, mebrushx, mebrushy, mebrushx2, mebrushy2);
					mebrushx2 = (xblk-xmapoffset);
					mebrushy2 = (yblk-ymapoffset);
					MEBrushRect (hwnd, mebrushx, mebrushy, mebrushx2, mebrushy2);
				} else {
					PlaceTile (xblk, yblk);
					if (blockstaggerx || blockstaggery) InvalidateRgn (mhwnd, NULL, FALSE);
					else MEDrawUnderTile (hwnd, xblk, yblk);
				}
			}
//			if (!(blockstaggerx || blockstaggery))
			MEDrawGrid (hwnd, xblk, yblk);
			MEShowCoords (xblk, yblk);
		}
//		if (curbrsh>0 && curbrsh<9 && !gettingbrush) InvalidateRgn (mhwnd, NULL, FALSE);
		break;
	case WM_ACTIVATE:
//   	MessageBox (hwnd, "Killed!", "Focus", MB_OK);
	case WM_LBUTTONUP:
		if (betype == BETYPE_GRAPH) {
			GEOnLButtonUp (hwnd);
			return 0;
		}
		if (lua_mb_act == 10) {
			xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
			yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
			LuaSetMouseBlock (lua_area_x, lua_area_y, xblk, yblk);
			LuaRunMouseButton (10, xblk, yblk);
			lua_mb_act = 0;
		} else lua_mb_act = 0;
		painton = 0;
		if (gettingbrush) {
			if (mebrushx2<mebrushx) { i = mebrushx; mebrushx = mebrushx2; mebrushx2 = i; }
			if (mebrushy2<mebrushy) { i = mebrushy; mebrushy = mebrushy2; mebrushy2 = i; }
			if (MakeBrush (xmapoffset+mebrushx, ymapoffset+mebrushy,
				xmapoffset+mebrushx2, ymapoffset+mebrushy2)!=0)
				MessageBox (hwnd, "No brush number selected, or part of the brush is outside the map area, the previous brush has been kept", "Brush error", MB_ICONEXCLAMATION|MB_OK);
			gettingbrush = 0;
			InvalidateRgn (mhwnd, NULL, FALSE); }
			if (curbrsh>0 && curbrsh<=MAXBRSH) InvalidateRgn (mhwnd, NULL, FALSE);
		break;
	case WM_RBUTTONUP:
		if (betype == BETYPE_GRAPH) return 0;
		if (mappt==NULL) break;
		if (lua_mb_act == 10) {
			xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
			yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
			LuaSetMouseBlock (lua_area_x, lua_area_y, xblk, yblk);
			LuaRunMouseButton (10, xblk, yblk);
			lua_mb_act = 0;
			rmapped = 0;
			break;
		}
		lua_mb_act = 0;
		if (rmapped) { rmapped = 0; break; }
		MECopyToUndo ();
		xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
		yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
		if (xblk>=0 && xblk<mapwidth && yblk>=0 && yblk<mapheight) {
			curplacetile = 0;
			PlaceTile (xblk, yblk);
			mapaltered = 1;
			if (blockstaggerx || blockstaggery) InvalidateRgn (mhwnd, NULL, FALSE);
			else MEDrawUnderTile (hwnd, xblk, yblk);
			MEShowCoords (xblk, yblk);
		}
		break;
	case WM_RBUTTONDOWN:
		if (betype == BETYPE_GRAPH) {
			GEOnRButtonDown (hwnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			return 0;
		}
		if (mappt==NULL) break;
		if (MBDoAction (mb_r, hwnd, lParam)) rmapped = 1;
		break;
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
		if (mappt==NULL) break;
		MBDoAction (HIWORD(wParam)==2 ? mb_x2 : mb_x1, hwnd, lParam);
		break;
	case WM_MBUTTONDOWN:
		if (mappt==NULL) break;
		if (MBDoAction (mb_m, hwnd, lParam)) break;
		dragmode = 1;
		dragox = LOWORD(lParam);
		dragoy = HIWORD(lParam);
		dragoxoff = xmapoffset;
		dragoyoff = ymapoffset;
		SetCursor(LoadCursor(NULL, IDC_SIZEALL));
		break;
	case WM_MBUTTONUP:
		if (dragmode) MEDragFinish();
		break;
	case WM_MOUSEWHEEL:
		if (mappt==NULL) break;
		if (mwheelstyle == 1) break; /* parent routes wheel to the block window */
		{
			int delta = (short)HIWORD(wParam);
			int step = (delta>0) ? -mwheelblks : mwheelblks;
			ymapoffset += step;
			if (ymapoffset<0) ymapoffset = 0;
			if (ymapoffset>(mapheight-2)) ymapoffset = mapheight-2;
			if ((blockstaggerx || blockstaggery) && ymapoffset&1) ymapoffset--;
			SetScrollPos (mhwnd, SB_VERT, ymapoffset, TRUE);
			InvalidateRgn (mhwnd, NULL, FALSE);
		}
		break;
	case WM_VSCROLL:
		switch (LOWORD(wParam)) {
			case SB_TOP:
			ymapoffset = 0;
			break;
			case SB_LINEUP:
			if (ymapoffset>0) ymapoffset--;
			break;
			case SB_LINEDOWN:
			if (ymapoffset<(mapheight-2)) ymapoffset++;
			if ((blockstaggerx || blockstaggery) && ymapoffset&1) ymapoffset++;
			break;
			case SB_PAGEUP:
			ymapoffset-=10;
			if (ymapoffset<0) ymapoffset = 0;
			break;
			case SB_PAGEDOWN:
			if (ymapoffset<(mapheight-11)) ymapoffset+=10;
			break;
			case SB_BOTTOM:
			ymapoffset = mapheight-1;
			if (ymapoffset<0) ymapoffset = 0;
			break;
			case SB_THUMBPOSITION:
			ymapoffset = (HIWORD(wParam));
			break;
		case SB_THUMBTRACK:
			if (thumbtrack) ymapoffset = (HIWORD(wParam));
			break;
			}
		if ((blockstaggerx || blockstaggery) && ymapoffset&1) ymapoffset--;
		SetScrollPos (mhwnd, SB_VERT, ymapoffset, TRUE);
		InvalidateRgn (mhwnd, NULL, FALSE);
		break;
	case WM_HSCROLL:
		switch (LOWORD(wParam)) {
			case SB_TOP:
			xmapoffset = 0;
			break;
			case SB_LINEUP:
			if (xmapoffset>0) xmapoffset--;
			break;
			case SB_LINEDOWN:
			if (xmapoffset<(mapwidth-1)) xmapoffset++;
			break;
			case SB_PAGEUP:
			xmapoffset-=10;
			if (xmapoffset<0) xmapoffset = 0;
			break;
			case SB_PAGEDOWN:
			if (xmapoffset<(mapwidth-11)) xmapoffset+=10;
			break;
			case SB_BOTTOM:
			xmapoffset = mapwidth-1;
			if (xmapoffset<0) xmapoffset = 0;
			break;
			case SB_THUMBPOSITION:
			xmapoffset = (HIWORD(wParam));
			break;
		case SB_THUMBTRACK:
			if (thumbtrack) xmapoffset = (HIWORD(wParam));
			break;
			}
		SetScrollPos (mhwnd, SB_HORZ, xmapoffset, TRUE);
		InvalidateRgn (mhwnd, NULL, FALSE);
		break;
	case WM_KEYDOWN:
		if (betype == BETYPE_GRAPH) {
			switch (LOWORD(wParam)) {
				case 'L': GEAccelLine (); break;
				case 'F': GEAccelFill (); break;
				case 'P': GEAccelPick (); break;
				case 'Z':
					if (GetKeyState (VK_CONTROL) < 0) GEUndo ();
					break;
			}
			return 0;
		}
		if (mappt==NULL) break;
		if (LOWORD(wParam)>=VK_F1 && LOWORD(wParam)<=VK_F10) {
			int fn = LOWORD(wParam)-VK_F1;
			if (keyF[fn]) SendMessage (GetParent (hwnd), WM_COMMAND, keyF[fn], 0);
			break;
		}
		switch (LOWORD(wParam)) {
			case '1': if (key1) { SendMessage (GetParent (hwnd), WM_COMMAND, key1, 0); break; } break;
			case '2': if (key2) { SendMessage (GetParent (hwnd), WM_COMMAND, key2, 0); break; } break;
			case '3': if (key3) { SendMessage (GetParent (hwnd), WM_COMMAND, key3, 0); break; } break;
			case 'I': if (keyi) { SendMessage (GetParent (hwnd), WM_COMMAND, keyi, 0); break; } break;
			case 'K': if (keyk) { SendMessage (GetParent (hwnd), WM_COMMAND, keyk, 0); break; } break;
			case 'M': if (keym) { SendMessage (GetParent (hwnd), WM_COMMAND, keym, 0); break; } break;
			case 'L': /* line tool */
				if (linemode) linemode = 0; else { linemode = 1; pathmode = 0; }
				break;
			case 'P': /* lay-path tool (or pick object in place mode) */
				if (objplace) {
					int mpx = meoldbx * blockwidth + blockwidth / 2;
					int mpy = meoldby * blockheight + blockheight / 2;
					int pi = OE_PickObject (mpx, mpy);
					if (pi >= 0) {
						F14_SetCurObject (pi);
						InvalidateRgn (mhwnd, NULL, FALSE);
					}
				}
				else if (pathmode) { pathmode = 0; } else { pathmode = 1; linemode = 0; }
				break;
			case 'D': /* drag-map */
				if (!dragmode) {
					POINT mc; RECT rc;
					GetCursorPos(&mc);
					GetWindowRect(mhwnd, &rc);
					dragmode = 1;
					dragox = mc.x - rc.left;
					dragoy = mc.y - rc.top;
					dragoxoff = xmapoffset;
					dragoyoff = ymapoffset;
					SetCursor(LoadCursor(NULL, IDC_SIZEALL));
				} else {
					MEDragFinish();
				}
				break;
			case 'E': /* edit block/anim properties */
				if (curanim>=0) DIAEditAnimProperties (mhwnd, curanim);
				else DIAEditProperties (mhwnd, curstr);
				break;
			case VK_ESCAPE:
				linemode = 0; pathmode = 0;
				if (dragmode) MEDragFinish();
				break;
		}
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

HWND CreateMapEditorWindow (HWND hwnd)
{
int x;
RECT rc;
WNDCLASS wndclass;

	if (mhwnd != NULL) return mhwnd;

		memset (&wndclass, 0, sizeof(WNDCLASS));
		wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT | CS_NOCLOSE;
		wndclass.lpfnWndProc = MapWndProc;
		wndclass.hInstance = hInstance;
		wndclass.hIcon = LoadIcon(hInstance, "MAPWINIC");
		wndclass.hCursor = LoadCursor(NULL, (LPCSTR) IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH) GetStockObject (GRAY_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = szMapEdname;
		RegisterClass(&wndclass);

	GetWindowRect (hwnd, &rc);
	if (wlayout == 0) x = 0; else x = 128;

	mhwnd = CreateWindow (szMapEdname, szMapEdname,
								WS_MAXIMIZEBOX	| WS_MINIMIZEBOX | WS_CAPTION | WS_CLIPSIBLINGS |
								WS_THICKFRAME | WS_CHILDWINDOW | WS_HSCROLL | WS_VSCROLL,
								x, 0, (rc.right)-(rc.left)-132,
								(rc.bottom)-(rc.top)-30-GetSystemMetrics(SM_CYCAPTION),
								hwnd, (HMENU) 1000, hInstance, NULL);

	MEUpdateScrollVis (); /* no map yet: hide scrollbars until one loads */
	ShowWindow(mhwnd, SW_SHOW);
	UpdateWindow(mhwnd);
	return mhwnd;
}

/* Hide scrollbars when no map is loaded (avoids zero-range scrollbar
 * artifacts, e.g. white strip when maximized with nothing open). */
void MEUpdateScrollVis (void)
{
	if (mhwnd==NULL) return;
	ShowScrollBar (mhwnd, SB_BOTH, mappt!=NULL ? TRUE : FALSE);
}
