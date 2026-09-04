#define  STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"

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

		GetWindowRect (phwnd, &rc);
		k = (((rc.right)-(rc.left))/blockgapx)+2;
		if (zoomon!=1||zoomdiv!=1) { k = (k*zoomdiv)/zoomon+(zoomon-1); }
		l = (((rc.bottom)-(rc.top+20))/blockgapy)+2;
		if (zoomon!=1||zoomdiv!=1) { l = (l*zoomdiv)/zoomon+(zoomon-1); }

		bwpitch = ((k*blockgapx*3)+3)&0x7FFC;
		destmempt = malloc ((l+2)*bwpitch*blockgapy);
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
			char utxt[24]; int vx, vy, ti, tj; short int bv; BLKSTR * bp;
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
		case 5: /* Placer.lua */
		case 10: /* Area.lua */
			{ extern int LuaRunMouseButton (int, int, int);
			  int bx = (LOWORD(lParam)) / blockgapx + xmapoffset;
			  int by = (HIWORD(lParam)) / blockgapy + ymapoffset;
			  LuaRunMouseButton (act, bx, by); }
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
				extern int LuaRunMouseButton (int, int, int);
				int bx = (LOWORD(lParam)) / blockgapx + xmapoffset;
				int by = (HIWORD(lParam)) / blockgapy + ymapoffset;
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

static int GE_gfxcol = 0;
static int GE_thickness = 1;

static int GE_FindBlockForGraphic (int gfx)
{
	int i;
	BLKSTR * blk;
	for (i = 0; i < numblockstr; i++) {
		blk = (BLKSTR *) (blockstrpt + i * SOBSTR);
		if ((int)blk->bgoff == gfx * blockwidth * blockheight * ((blockdepth+1)/8))
			return i;
	}
	return -1;
}

static void MEDrawGraphicEditor (HWND phwnd, HDC myhdc)
{
	RECT rc;
	int pw, ph, tilepx, tilepy, tilesize;
	HDC fromhdc;
	HBITMAP temphbm, hbmpold;
	unsigned char * tb;
	BITMAPINFOHEADER bih;
	char txt[64];

	GetClientRect (phwnd, &rc);
	pw = rc.right - rc.left;
	ph = rc.bottom - rc.top;

	/* Background: dark grey */
	{ RECT brc = rc; FillRect (myhdc, &brc, (HBRUSH)GetStockObject(DKGRAY_BRUSH)); }

	if (blockgfxpt == NULL || numblockgfx <= 0) {
		SetBkColor (myhdc, RGB(0x44,0x44,0x44));
		SetTextColor (myhdc, RGB(255,255,255));
		TextOut (myhdc, 8, 8, "No graphics loaded", 18);
		return;
	}
	if (curgfxsel < 0) curgfxsel = 0;
	if (curgfxsel >= numblockgfx) curgfxsel = numblockgfx - 1;

	/* Tile preview: fill left portion of window */
	tilesize = ph - 80;
	if (tilesize < blockwidth) tilesize = blockwidth;
	if (tilesize > pw - 200) tilesize = pw - 200;
	tilepx = 8;
	tilepy = 8;

	tb = malloc (blockwidth * blockheight * 3);
	if (tb == NULL) return;
	memset (tb, 0, blockwidth * blockheight * 3);
	BlockGfxto24 (curgfxsel, tb, 0, 0, blockwidth * 3);

	fromhdc = CreateCompatibleDC (myhdc);
	memset (&bih, 0, sizeof(bih));
	bih.biSize = sizeof(bih);
	bih.biWidth = blockwidth;
	bih.biHeight = -blockheight;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;

	temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT, tb, (BITMAPINFO*)&bih, DIB_RGB_COLORS);
	hbmpold = SelectObject (fromhdc, temphbm);
	StretchBlt (myhdc, tilepx, tilepy, tilesize, tilesize, fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
	SelectObject (fromhdc, hbmpold);
	DeleteObject (temphbm);

	/* Grid lines on tile preview */
	{
		HPEN hpen, hold;
		int gx, gy;
		hpen = CreatePen (PS_SOLID, 1, RGB(0x66,0x66,0x66));
		hold = SelectObject (myhdc, hpen);
		for (gx = 1; gx < blockwidth; gx++) {
			int sx = tilepx + gx * tilesize / blockwidth;
			MoveToEx (myhdc, sx, tilepy, NULL);
			LineTo (myhdc, sx, tilepy + tilesize);
		}
		for (gy = 1; gy < blockheight; gy++) {
			int sy = tilepy + gy * tilesize / blockheight;
			MoveToEx (myhdc, tilepx, sy, NULL);
			LineTo (myhdc, tilepx + tilesize, sy);
		}
		SelectObject (myhdc, hold);
		DeleteObject (hpen);
	}

	free (tb);

	/* Palette: top-right area */
	{
		int palx = tilepx + tilesize + 16;
		int paly = 8;
		int palsz = pw - palx - 8;
		int cols, rows, cw, ch, cx, cy;
		RECT prc;

		if (palsz > 256) palsz = 256;
		if (palsz > 16 * 12) palsz = 16 * 12;
		cols = 16;
		rows = (palsz / cols > 16) ? 16 : palsz / cols;
		if (rows < 1) rows = 1;
		cw = palsz / cols;
		ch = cw;

		SetBkColor (myhdc, RGB(0x44,0x44,0x44));
		SetTextColor (myhdc, RGB(255,255,255));
		sprintf (txt, "Palette (col %d/%d)", GE_gfxcol, (1 << blockdepth) - 1);
		TextOut (myhdc, palx, paly, txt, strlen(txt));
		paly += 16;

		for (cy = 0; cy < rows; cy++) {
			for (cx = 0; cx < cols; cx++) {
				int ci = cy * cols + cx;
				HBRUSH hbr;
				prc.left = palx + cx * cw;
				prc.top = paly + cy * ch;
				prc.right = prc.left + cw;
				prc.bottom = prc.top + ch;
				if (cmappt != NULL)
					hbr = CreateSolidBrush (RGB(cmappt[ci*3], cmappt[ci*3+1], cmappt[ci*3+2]));
				else
					hbr = CreateSolidBrush (RGB(ci, ci, ci));
				FillRect (myhdc, &prc, hbr);
				DeleteObject (hbr);
				if (ci == GE_gfxcol) {
					HPEN hpen2, hold2;
					hpen2 = CreatePen (PS_SOLID, 2, RGB(255,255,255));
					hold2 = SelectObject (myhdc, hpen2);
					Rectangle (myhdc, prc.left, prc.top, prc.right, prc.bottom);
					SelectObject (myhdc, hold2);
					DeleteObject (hpen2);
				}
			}
		}

		/* RGB value display below palette */
		{
			int rx = palx, ry = paly + rows * ch + 8;
			int r = 0, g = 0, b = 0;
			if (cmappt != NULL) {
				r = cmappt[GE_gfxcol * 3];
				g = cmappt[GE_gfxcol * 3 + 1];
				b = cmappt[GE_gfxcol * 3 + 2];
			} else { r = g = b = GE_gfxcol; }
			sprintf (txt, "RGB: %3d %3d %3d", r, g, b);
			SetTextColor (myhdc, RGB(255,255,255));
			TextOut (myhdc, rx, ry, txt, strlen(txt));
			sprintf (txt, "$%02X $%02X $%02X", r, g, b);
			TextOut (myhdc, rx, ry + 16, txt, strlen(txt));
		}
	}

	/* Corner map preview: bottom-right area */
	{
		int cpx = tilepx + tilesize + 16;
		int cpy = ph - 120;
		int cpw = pw - cpx - 8;
		int cph = 100;
		if (cpw > 0 && cph > 0 && mappt != NULL) {
			RECT cprc;
			int mx, my, bsx, bsy;
			cprc.left = cpx; cprc.top = cpy;
			cprc.right = cpx + cpw; cprc.bottom = cpy + cph;
			FillRect (myhdc, &cprc, (HBRUSH)GetStockObject(BLACK_BRUSH));
			bsx = cpw / mapwidth;
			bsy = cph / mapheight;
			if (bsx < 1) bsx = 1;
			if (bsy < 1) bsy = 1;
			for (my = 0; my < mapheight; my++) {
				for (mx = 0; mx < mapwidth; mx++) {
					int blk = mappt[mx + my * mapwidth];
					HBRUSH hbr;
					COLORREF col;
					if (blk >= 0 && blk < numblockstr) {
						BLKSTR * bp = (BLKSTR *)(blockstrpt + blk * SOBSTR);
						if (bp->bgoff == 0 && bp->fgoff == 0)
							col = RGB(0x44, 0x44, 0x44);
						else
							col = RGB(0x88, 0xAA, 0x88);
					} else {
						col = RGB(0x44, 0x44, 0x66);
					}
					cprc.left = cpx + mx * bsx;
					cprc.top = cpy + my * bsy;
					cprc.right = cprc.left + bsx;
					cprc.bottom = cprc.top + bsy;
					hbr = CreateSolidBrush (col);
					FillRect (myhdc, &cprc, hbr);
					DeleteObject (hbr);
				}
			}
		}
	}

	/* Tool buttons at bottom */
	{
		int bx = 8, by = ph - 30;
		const char * tools[] = { "FLH", "FLV", "ROT", "CLR", "BLK", "REV" };
		int toolw = 40, toolh = 20;
		int t;
		SetBkColor (myhdc, RGB(0xC0,0xC0,0xC0));
		SetTextColor (myhdc, RGB(0,0,0));
		for (t = 0; t < 6; t++) {
			RECT trc;
			trc.left = bx; trc.top = by;
			trc.right = bx + toolw; trc.bottom = by + toolh;
			DrawEdge (myhdc, &trc, EDGE_RAISED, BF_RECT);
			TextOut (myhdc, bx + 4, by + 3, tools[t], strlen(tools[t]));
			bx += toolw + 4;
		}
		/* Draw size selector */
		bx += 16;
		SetTextColor (myhdc, RGB(255,255,255));
		SetBkColor (myhdc, RGB(0x44,0x44,0x44));
		TextOut (myhdc, bx, by + 3, "Draw size:", 10);
		bx += 64;
		{
			int ds;
			for (ds = 1; ds <= 4; ds++) {
				RECT trc;
				trc.left = bx; trc.top = by;
				trc.right = bx + 20; trc.bottom = by + toolh;
				if (ds == GE_thickness) {
					FillRect (myhdc, &trc, (HBRUSH)GetStockObject(WHITE_BRUSH));
					SetTextColor (myhdc, RGB(0,0,0));
				} else {
					DrawEdge (myhdc, &trc, EDGE_RAISED, BF_RECT);
					SetTextColor (myhdc, RGB(255,255,255));
				}
				{
					char dsch[2] = { '0' + ds, 0 };
					TextOut (myhdc, bx + 6, by + 3, dsch, 1);
				}
				bx += 22;
			}
		}
	}

	/* Status text */
	{
		int blk = GE_FindBlockForGraphic (curgfxsel);
		SetBkColor (myhdc, RGB(0x44,0x44,0x44));
		SetTextColor (myhdc, RGB(255,255,255));
		if (blk >= 0)
			sprintf (txt, "This graphic is used by block %d", blk);
		else
			sprintf (txt, "Graphic %d/%d (unused)", curgfxsel + 1, numblockgfx);
		TextOut (myhdc, 8, ph - 12, txt, strlen(txt));
	}
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
			/* Graphic editor mode: handle tile drawing and palette */
			RECT rc;
			int pw, ph, tilepx, tilepy, tilesize, tileend;
			int mx = LOWORD(lParam), my = HIWORD(lParam);
			GetClientRect (hwnd, &rc);
			pw = rc.right - rc.left;
			ph = rc.bottom - rc.top;
			tilesize = ph - 80;
			if (tilesize < blockwidth) tilesize = blockwidth;
			if (tilesize > pw - 200) tilesize = pw - 200;
			tilepx = 8; tilepy = 8;
			tileend = tilepx + tilesize;

			/* Click on tile preview: draw pixel */
			if (mx >= tilepx && mx < tileend && my >= tilepy && my < tilepy + tilesize) {
				int px = (mx - tilepx) * blockwidth / tilesize;
				int py = (my - tilepy) * blockheight / tilesize;
				int bpp = (blockdepth + 1) / 8;
				unsigned char * tile = (unsigned char *)blockgfxpt + curgfxsel * blockwidth * blockheight * bpp;
				if (px >= 0 && px < blockwidth && py >= 0 && py < blockheight) {
					if (blockdepth == 8)
						tile[py * blockwidth + px] = (unsigned char)GE_gfxcol;
					else {
						int r, g, b;
						if (cmappt) { r = cmappt[GE_gfxcol*3]; g = cmappt[GE_gfxcol*3+1]; b = cmappt[GE_gfxcol*3+2]; }
						else { r = g = b = GE_gfxcol; }
						tile[(py * blockwidth + px) * bpp + 0] = b;
						tile[(py * blockwidth + px) * bpp + 1] = g;
						tile[(py * blockwidth + px) * bpp + 2] = r;
					}
					mapaltered = 1;
					InvalidateRect (hwnd, NULL, FALSE);
					BEInvalidate ();
				}
			}
			/* Click on palette: change colour */
			{
				int palx = tilepx + tilesize + 16;
				int paly = 8 + 16;
				int palsz = pw - palx - 8;
				int cols = 16, cw, ch, rows;
				if (palsz > 256) palsz = 256;
				if (palsz > 16 * 12) palsz = 16 * 12;
				cw = palsz / cols;
				ch = cw;
				rows = palsz / cols / cw;
				if (rows > 16) rows = 16;
				if (mx >= palx && my >= paly && mx < palx + cols * cw && my < paly + rows * ch) {
					int ci = ((mx - palx) / cw) + ((my - paly) / ch) * cols;
					if (ci >= 0 && ci < (1 << blockdepth)) {
						GE_gfxcol = ci;
						InvalidateRect (hwnd, NULL, FALSE);
					}
				}
			}
			/* Click on tool buttons */
			{
				int bx = 8, by = ph - 30;
				int toolw = 40, toolh = 20;
				int t;
				if (my >= by && my < by + toolh) {
					for (t = 0; t < 6; t++) {
						if (mx >= bx && mx < bx + toolw) {
							int bpp = (blockdepth + 1) / 8;
							int tilebytes = blockwidth * blockheight * bpp;
							unsigned char * src = (unsigned char *)blockgfxpt + curgfxsel * tilebytes;
							MECopyToUndo ();
							if (t == 0) {
								/* FLH: flip horizontal */
								int y;
								for (y = 0; y < blockheight; y++) {
									int x;
									unsigned char * row = src + y * blockwidth * bpp;
									for (x = 0; x < blockwidth / 2; x++) {
										int lx = x, rx = blockwidth - 1 - x;
										int k;
										for (k = 0; k < bpp; k++) {
											unsigned char tmp = row[lx * bpp + k];
											row[lx * bpp + k] = row[rx * bpp + k];
											row[rx * bpp + k] = tmp;
										}
									}
								}
							} else if (t == 1) {
								/* FLV: flip vertical */
								int y;
								for (y = 0; y < blockheight / 2; y++) {
									int ty = y, by2 = blockheight - 1 - y;
									unsigned char * top = src + ty * blockwidth * bpp;
									unsigned char * bot = src + by2 * blockwidth * bpp;
									int rowbytes = blockwidth * bpp;
									int k;
									for (k = 0; k < rowbytes; k++) {
										unsigned char tmp = top[k];
										top[k] = bot[k];
										bot[k] = tmp;
									}
								}
							} else if (t == 2) {
								/* ROT: rotate 90 degrees clockwise */
								unsigned char * tmp = malloc (tilebytes);
								if (tmp) {
									int x, y;
									for (y = 0; y < blockheight; y++) {
										for (x = 0; x < blockwidth; x++) {
											int k;
											for (k = 0; k < bpp; k++) {
												/* (x,y) -> (blockheight-1-y, x) */
												int sx = (y * blockwidth + x) * bpp + k;
												int dx = ((blockheight - 1 - y) + x * blockheight) * bpp + k;
												tmp[dx] = src[sx];
											}
										}
									}
									memcpy (src, tmp, tilebytes);
									free (tmp);
								}
							} else if (t == 3) {
								/* CLR: clear tile */
								memset (src, 0, tilebytes);
							} else if (t == 4) {
								/* BLK: block lookup */
								{
									int blk = GE_FindBlockForGraphic (curgfxsel);
									if (blk >= 0) {
										curstr = blk;
										/* show message in status */
										MessageBeep (0);
									}
								}
							} else if (t == 5) {
								/* REV: reverse pixel order (180 rotation) */
								int y;
								for (y = 0; y < blockheight / 2; y++) {
									int ty = y, by2 = blockheight - 1 - y;
									int x;
									for (x = 0; x < blockwidth; x++) {
										int k;
										int lx = x, rx = blockwidth - 1 - x;
										for (k = 0; k < bpp; k++) {
											int sx = (ty * blockwidth + lx) * bpp + k;
											int dx = (by2 * blockwidth + rx) * bpp + k;
											unsigned char tmp = src[sx];
											src[sx] = src[dx];
											src[dx] = tmp;
										}
									}
								}
								if (blockheight % 2 == 1) {
									int midy = blockheight / 2;
									int x;
									for (x = 0; x < blockwidth / 2; x++) {
										int lx = x, rx = blockwidth - 1 - x;
										int k;
										for (k = 0; k < bpp; k++) {
											int sx = (midy * blockwidth + lx) * bpp + k;
											int dx = (midy * blockwidth + rx) * bpp + k;
											unsigned char tmp = src[sx];
											src[sx] = src[dx];
											src[dx] = tmp;
										}
									}
								}
							}
							mapaltered = 1;
							InvalidateRect (hwnd, NULL, FALSE);
							break;
						}
						bx += toolw + 4;
					}
				}
				/* Click on draw size selector */
				bx += 16 + 64;
				if (my >= by && my < by + toolh) {
					int ds;
					for (ds = 1; ds <= 4; ds++) {
						if (mx >= bx && mx < bx + 20) {
							GE_thickness = ds;
							InvalidateRect (hwnd, NULL, FALSE);
							break;
						}
						bx += 22;
					}
				}
			}
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
				if (curbrsh>0 && curbrsh<=MAXBRSH) {
					PlaceTile (xblk, yblk);
				} else if (curanim>=0) {
					*(mappt+xblk+yblk*mapwidth) = -((curanim+1)*FMP_ANI_MEMUNIT);
				} else {
					*(mappt+xblk+yblk*mapwidth) = curstr;
				}
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
		if (mappt==NULL) return 0;
		if (betype == BETYPE_GRAPH) return 0;
		if (dragmode) {
			MEDragMap (LOWORD(lParam), HIWORD(lParam));
			return 0;
		}
		xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
		yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
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
		if (mappt==NULL) break;
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
		{
			int delta = (short)HIWORD(wParam);
			int step = (delta>0) ? -mwheelblks : mwheelblks;
			if (mwheelstyle & 1) {
				ymapoffset += step;
				if (ymapoffset<0) ymapoffset = 0;
				if (ymapoffset>(mapheight-2)) ymapoffset = mapheight-2;
				if ((blockstaggerx || blockstaggery) && ymapoffset&1) ymapoffset--;
				SetScrollPos (mhwnd, SB_VERT, ymapoffset, TRUE);
				InvalidateRgn (mhwnd, NULL, FALSE);
			}
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
