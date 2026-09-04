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
int megetabrush = 0, onion = 0, oniondark = 0;
int wlayout = 0;

static int meoldbx, meoldby, mebrushx, mebrushy, mebrushx2, mebrushy2;

extern HINSTANCE hInstance;
extern short int blockwidth, blockheight, blockdepth, gridon, zoomon, curbrsh;
extern int blockgapx, blockgapy, blockstaggerx, blockstaggery, clickmask;
extern unsigned char trans16hi, trans16low, transred, transgreen, transblue, trans8bit;
extern short int mapwidth, mapheight, xmapoffset, ymapoffset;
extern int mapaltered;
extern char * blockgfxpt, * blockstrpt;
extern short int * mappt;
extern long int curplacetile;


void MEResizeWind (int wide, int high)
{
	if (mhwnd != NULL && wlayout == 0) MoveWindow (mhwnd, 0, 0, wide-(wide/5), high, TRUE);
	if (mhwnd != NULL && wlayout == 1) MoveWindow (mhwnd, wide/5, 0, wide-(wide/5), high, TRUE);
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

int MEGetX (int xpix, int ypix)
{
int xb;

	if (blockstaggerx || blockstaggery) {
		xpix += (blockstaggerx*zoomon);
		ypix += (blockstaggery*zoomon);
	}
	xpix /= zoomon;
	ypix /= zoomon;
	xb = xpix/blockgapx;

	if ((blockstaggerx || blockstaggery) && clickmask) xb += MEClickmask (xpix, ypix, 0);

	return xb;
}

int MEGetY (int xpix, int ypix)
{
int yb;

	if (blockstaggerx || blockstaggery) {
		xpix += (blockstaggerx*zoomon);
		ypix += (blockstaggery*zoomon);
	}
	xpix /= zoomon;
	ypix /= zoomon;
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
char temptext[128];
char scbits[16];
BLKSTR * myblkstrpt;
//ANISTR * myanpt;
short int * mymappt;

	if (mappt == NULL) return;
	if (blockstrpt == NULL) return;
	if (x>=mapwidth) x = mapwidth-1;
	if (y>=mapheight) y = mapheight-1;
	if (x<0) x = 0;
	if (y<0) y = 0;

	mymappt = mappt;

	mymappt += (x+(y*mapwidth));

	if (*mymappt<0) {
		sprintf (temptext, "X %d/%d, Y %d/%d. Anim %d",
			x, mapwidth-1, y, mapheight-1, (-(*mymappt))/sizeof(ANISTR)-1);
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
		sprintf (temptext, "X %d/%d, Y %d/%d. Block %d (BG=%d, FG=%d, Bits=%s, T=%s)",
			x, mapwidth-1, y, mapheight-1, (*mymappt),
			(int) myblkstrpt->bgoff/(blockwidth*blockheight*((blockdepth+1)/8)),
			(int) myblkstrpt->fgoff/(blockwidth*blockheight*((blockdepth+1)/8)),
			scbits, scbits+9);
	}

	SetWindowText (mhwnd, temptext);
	meoldbx = x; meoldby = y;
}

void MEBrushRect (HWND hwnd, int xoff, int yoff, int xoff2, int yoff2)
{
RECT dragboxrect;
HDC myhdc;
HBRUSH oldbrush;
int oldrop;

	xoff *= (blockgapx*zoomon); xoff2 *= (blockgapx*zoomon);
	yoff *= (blockgapy*zoomon); yoff2 *= (blockgapy*zoomon);
   if (blockstaggerx || blockstaggery) { yoff /= 2; yoff2 /= 2; }
	if (xoff<xoff2) { dragboxrect.left = xoff; dragboxrect.right = xoff2+(blockwidth*zoomon)-1; }
	else { dragboxrect.left = xoff2; dragboxrect.right = xoff+(blockwidth*zoomon)-1; }
	if (yoff<yoff2) { dragboxrect.top = yoff; dragboxrect.bottom = yoff2+(blockheight*zoomon)-1; }
	else { dragboxrect.top = yoff2; dragboxrect.bottom = yoff+(blockheight*zoomon)-1; }
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
		if (zoomon==1) {
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
			StretchBlt (myhdc, (xoff-xmapoffset)*blockwidth*zoomon, (yoff-ymapoffset)*blockheight*zoomon,
				blockwidth*zoomon, blockheight*zoomon, fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
		}

		SelectObject (fromhdc, hbmpold);
		DeleteObject (temphbm);

		DeleteDC (fromhdc);
		ReleaseDC (hwnd, myhdc);
		free (destmempt);
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
		if (zoomon!=1) { k /= zoomon; k+=(zoomon-1); }
		l = (((rc.bottom)-(rc.top+20))/blockgapy)+2;
		if (zoomon!=1) { l /= zoomon; l+=(zoomon-1); }

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
					if (onion) Onionto24 (xmapoffset+i, ymapoffset+(j*2), destmempt, i*blockgapx, j*blockgapy, bwpitch, onion-1);
					else Overlayto24 (xmapoffset+i, ymapoffset+(j*2), destmempt, i*blockgapx, j*blockgapy, bwpitch, -1);
				} else {
					if (onion) Onionto24 (xmapoffset+i, ymapoffset+j, destmempt, i*blockgapx, j*blockgapy, bwpitch, onion-1);
					else Blockto24 (xmapoffset+i, ymapoffset+j, destmempt, i*blockgapx, j*blockgapy, bwpitch);
				}
			}
			for (i=0;i<k;i++) {
				if (blockstaggerx || blockstaggery) {
					if (onion) Onionto24 (xmapoffset+i, ymapoffset+(j*2+1), destmempt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, onion-1);
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
		if (zoomon==1)
			BitBlt (myhdc, 0, 0, k*blockgapx, l*blockgapy, fromhdc, blockstaggerx, blockstaggery, SRCCOPY);
		else
			StretchBlt (myhdc, 0, 0, ((k*blockgapx)-blockstaggerx)*zoomon, ((l*blockgapy)-blockstaggery)*zoomon, fromhdc, blockstaggerx, blockstaggery, k*blockgapx-blockstaggerx, l*blockgapy-blockstaggery, SRCCOPY);
		SelectObject (fromhdc, hbmpold);
		DeleteObject (temphbm);

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

//		MessageBox (hwnd, "Killed3", "Focus", MB_OK);
//		MessageBox (hwnd, "Killed4", "Focus", MB_OK);
		if (blockstaggerx || blockstaggery) {
			MEDrawMap (hwnd, myhdc);
		} else {
			if (onion) Onionto24 (meoldbx, meoldby, destmempt, 0, 0, bwpitch, onion-1);
			else Blockto24 (meoldbx, meoldby, destmempt, 0, 0, bwpitch);
			if (oniondark) {
			i = bwpitch*blockheight; darkenpt = destmempt; while (i) {
				*darkenpt = ((*darkenpt)>>1)&0x7F; darkenpt++; i--;
			} }
			if (onion) Overlayto24 (meoldbx, meoldby, destmempt, 0, 0, bwpitch, -1);

			temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
				destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
			x = (meoldbx-xmapoffset)*blockwidth;
			y = (meoldby-ymapoffset)*blockheight;
//			dfpt = fopen ("deb.txt", "a");
//			fprintf (dfpt, "x=%d, y=%d, thbm=%p, fromhdc=%p\n", x, y, temphbm, fromhdc);
//			fclose (dfpt);
			hbmpold = SelectObject(fromhdc, temphbm);
			if (zoomon==1)
				BitBlt (myhdc, x, y,	blockwidth, blockheight, fromhdc, 0, 0, SRCCOPY);
			else
				StretchBlt (myhdc, x*zoomon, y*zoomon,
					blockwidth*zoomon, blockheight*zoomon, fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
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
			x *= blockgapx;
			y /= 2; y *= blockgapy;
			if (!i) { x -= blockstaggerx; y -= blockstaggery; }
			blttype = SRCPAINT;
		} else {
			x = (xoff-xmapoffset)*blockwidth;
			y = (yoff-ymapoffset)*blockheight;
			blttype = SRCCOPY;
		}

		if (zoomon==1)
			BitBlt (myhdc, x, y,	blockwidth, blockheight, fromhdc, 0, 0, blttype);
		else
			StretchBlt (myhdc, x*zoomon, y*zoomon,
				blockwidth*zoomon, blockheight*zoomon, fromhdc, 0, 0, blockwidth, blockheight, blttype);

		SelectObject (fromhdc, hbmpold);
		DeleteObject (temphbm);

		DeleteDC (fromhdc);
		ReleaseDC (hwnd, myhdc);
		free (destmempt);
}

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
			MEDrawMap (hwnd, myhdc);
		}
		EndPaint (hwnd, &ps);
//		ReleaseDC (hwnd, myhdc);
		break;
	case WM_LBUTTONDOWN:
		if (mappt==NULL) return 0;
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
			}
		SetScrollPos (mhwnd, SB_HORZ, xmapoffset, TRUE);
		InvalidateRgn (mhwnd, NULL, FALSE);
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
		wndclass.hbrBackground = NULL; GetStockObject (GRAY_BRUSH);
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

	ShowWindow(mhwnd, SW_SHOW);
	UpdateWindow(mhwnd);
	return mhwnd;
}
