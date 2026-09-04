#define  STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"
#include "fmp14.h"

#define BETYPE_STILL   0
#define BETYPE_ANIM    1
#define BETYPE_GRAPH   2
#define BETYPE_BRUSHES 3
#define BETYPE_OBJECTS 4
#define BETYPE_COUNT   5

#define BTN_STILL   1000
#define BTN_ANIM    1001
#define BTN_GRAPH   1002
#define BTN_BRUSHES 1003
#define BTN_OBJECTS 1004
#define BTN_FIRST   BTN_STILL
#define BTN_LAST    BTN_OBJECTS

#define TAB_H 20

static HWND bhwnd = NULL;
static HWND hbtn[BETYPE_COUNT] = { NULL };
short int beyoff = 0, betype = BETYPE_STILL, bebpr = 1;
short int curgfxsel = -1;   /* selected graphic index (Graph mode) */
short int curbrushsel = -1; /* selected brush index (Brushes mode) */
short int curobjsel = -1;   /* selected object index (Objects mode) */
static char szBlockEdname[] = "Block Editor";
extern HINSTANCE hInstance;
extern short int blockwidth, blockheight, blockdepth, gridon, zoomon;
extern int panelzoom;
static int BEZoom (void) { return panelzoom ? zoomon : 1; }
extern short int mapwidth, mapheight, xmapoffset, ymapoffset;
extern short int numblockstr, curstr, numblockanim, curanim, curbrsh;
extern char * blockgfxpt, * blockstrpt, * animstrpt;
extern short int numblockgfx;
extern short int * brshpt[MAXBRSH];
extern short int numbrushes;
extern int wlayout;

static void BECreateTabButtons (HWND parent);
static void BEUpdateTabButtons (void);

void BEResizeWind (int wide, int high)
{
	if (bhwnd == NULL) return;
	if (wlayout == 0) MoveWindow (bhwnd, wide-(wide/5), 0, wide/5, high, TRUE);
	if (wlayout == 1) MoveWindow (bhwnd, 0, 0, wide/5, high, TRUE);
	if (wlayout == -1) MoveWindow (bhwnd, wide-(wide/5), 0, wide/5, high, TRUE);
}

void BEInvalidate (void)
{
	InvalidateRgn (bhwnd, NULL, FALSE);
}

void BEUpdateScrollVis (void)
{
	if (bhwnd==NULL) return;
	ShowScrollBar (bhwnd, SB_VERT, blockgfxpt!=NULL ? TRUE : FALSE);
}

void BEBlockNext (void)
{
	if (betype == BETYPE_STILL && curanim==-1 && curbrsh==0 && curstr<(numblockstr-1))
		{ curstr++; BEInvalidate (); }
	if (betype == BETYPE_ANIM && curanim!=-1 && curbrsh==0 && curanim<(numblockanim-1))
		{ curanim++; BEInvalidate (); }
	if (betype == BETYPE_GRAPH && curgfxsel < numblockgfx - 1)
		{ curgfxsel++; BEInvalidate (); }
	if (betype == BETYPE_BRUSHES && curbrushsel < MAXBRSH - 1) {
		int next = curbrushsel + 1;
		while (next < MAXBRSH && brshpt[next] == NULL) next++;
		if (next < MAXBRSH) { curbrushsel = next; BEInvalidate (); }
	}
	if (betype == BETYPE_OBJECTS && curobjsel < F14_NumObjects() - 1)
		{ curobjsel++; BEInvalidate (); }
}

void BEBlockPrev (void)
{
	if (betype == BETYPE_STILL && curanim==-1 && curbrsh==0 && curstr>0)
		{ curstr--; BEInvalidate (); }
	if (betype == BETYPE_ANIM && curanim!=-1 && curbrsh==0 && curanim>0)
		{ curanim--; BEInvalidate (); }
	if (betype == BETYPE_GRAPH && curgfxsel > 0)
		{ curgfxsel--; BEInvalidate (); }
	if (betype == BETYPE_BRUSHES && curbrushsel > 0) {
		int prev = curbrushsel - 1;
		while (prev >= 0 && brshpt[prev] == NULL) prev--;
		if (prev >= 0) { curbrushsel = prev; BEInvalidate (); }
	}
	if (betype == BETYPE_OBJECTS && curobjsel > 0)
		{ curobjsel--; BEInvalidate (); }
}

static int BEItemCount (void)
{
	switch (betype) {
	case BETYPE_STILL:   return numblockstr;
	case BETYPE_ANIM:    return numblockanim;
	case BETYPE_GRAPH:   return numblockgfx;
	case BETYPE_BRUSHES: return numbrushes;
	case BETYPE_OBJECTS: return F14_NumObjects();
	}
	return 0;
}

void BESetScrollRange (void)
{
	int total = BEItemCount ();
	const char * title = "Block Editor";
	switch (betype) {
	case BETYPE_STILL:   title = "Still Blocks"; break;
	case BETYPE_ANIM:    title = "Anim Blocks"; break;
	case BETYPE_GRAPH:   title = "Graphic Blocks"; break;
	case BETYPE_BRUSHES: title = "Brushes"; break;
	case BETYPE_OBJECTS: title = "Objects"; break;
	}
	if (total > 0 && beyoff >= 0) {
		char buf[64];
		int sel = -1;
		switch (betype) {
		case BETYPE_STILL:   sel = (curanim == -1 && curbrsh == 0) ? curstr : -1; break;
		case BETYPE_ANIM:    sel = (curanim >= 0) ? curanim : -1; break;
		case BETYPE_GRAPH:   sel = curgfxsel; break;
		case BETYPE_BRUSHES: sel = curbrushsel; break;
		case BETYPE_OBJECTS: sel = curobjsel; break;
		}
		if (sel >= 0)
			sprintf (buf, "%s (%d/%d)", title, sel + 1, total);
		else
			sprintf (buf, "%s", title);
		SetWindowText (bhwnd, buf);
	} else {
		SetWindowText (bhwnd, title);
	}
	SetScrollRange (bhwnd, SB_VERT, 0, (total > 0) ? ((total - 1) / bebpr) : 0, FALSE);
	if (beyoff > ((total - 1) / bebpr)) beyoff = (total - 1) / bebpr;
	if (beyoff < 0) beyoff = 0;
	SetScrollPos (bhwnd, SB_VERT, beyoff, TRUE);
}

void BENewBlock (void)
{
	if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0 && numblockstr < MAXBLKSTR) NewBlock ();
	if (betype == BETYPE_ANIM && curanim != -1) AnimNew ();
	else if (betype == BETYPE_ANIM && animstrpt == NULL) AnimNew ();
	BESetScrollRange ();
	BEInvalidate ();
}

void BECutBlock (void)
{
	if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0) CutBlock ();
	if (betype == BETYPE_ANIM && curanim != -1) AnimCut ();
	BESetScrollRange ();
}

void BECopyBlock (void)
{
	if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0) CopyBlock ();
	if (betype == BETYPE_ANIM && curanim != -1) AnimCopy ();
}

void BEPasteBlock (void)
{
	if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0 && numblockstr < MAXBLKSTR) PasteBlock ();
	if (betype == BETYPE_ANIM && curanim != -1) AnimPaste ();
	BESetScrollRange ();
}

static int BEIsSelected (int idx)
{
	switch (betype) {
	case BETYPE_STILL:   return (curanim == -1 && curbrsh == 0 && idx == curstr);
	case BETYPE_ANIM:    return (curanim >= 0 && idx == curanim);
	case BETYPE_GRAPH:   return (idx == curgfxsel);
	case BETYPE_BRUSHES: return (idx == curbrushsel);
	case BETYPE_OBJECTS: return (idx == curobjsel);
	}
	return 0;
}

void BEDrawBlockGrid (unsigned char * dest, int xoff, int yoff, int linewidth, int isatt)
{
int i;

	if (gridon == 1 && isatt == 0) return;
	dest += (xoff*3);
	dest += (yoff*linewidth);
	for (i=0;i<blockwidth;i++)
	{
		if (isatt) {
			dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 255;
		} else {
			dest[i*3] = 255; dest[i*3+1] = 255; dest[i*3+2] = 255;
		}
	}

	for (i=0;i<(blockheight-1);i++)
	{
		if (isatt) {
			dest[0] = 0; dest[1] = 0; dest[2] = 255;
			dest[(blockwidth-1)*3] = 0; dest[(blockwidth-1)*3+1] = 0; dest[(blockwidth-1)*3+2] = 255;
		} else {
			dest[0] = 255; dest[1] = 255; dest[2] = 255;
			dest[(blockwidth-1)*3] = 0; dest[(blockwidth-1)*3+1] = 0; dest[(blockwidth-1)*3+2] = 0;
		}
		dest += linewidth;
	}

	for (i=0;i<blockwidth;i++)
	{
		if (isatt) {
			dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 255;
		} else {
			dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 0;
		}
	}
}

void BEDrawBlockSelect (unsigned char * dest, int xoff, int yoff, int linewidth)
{
int i;

	dest += (xoff*3);
	dest += (yoff*linewidth);
	for (i=0;i<blockwidth;i++)
	{
		if ((i%4)<2) { dest[i*3] = 255; dest[i*3+1] = 255; dest[i*3+2] = 255; }
		else { dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 0; }
	}

	for (i=0;i<(blockheight-1);i++)
	{
		if ((i%4)<2) {
			dest[0] = 255; dest[1] = 255; dest[2] = 255;
			dest[(blockwidth-1)*3] = 255; dest[(blockwidth-1)*3+1] = 255; dest[(blockwidth-1)*3+2] = 255;
		} else {
			dest[0] = 0; dest[1] = 0; dest[2] = 0;
			dest[(blockwidth-1)*3] = 0; dest[(blockwidth-1)*3+1] = 0; dest[(blockwidth-1)*3+2] = 0;
		}
		dest += linewidth;
	}

	for (i=0;i<blockwidth;i++)
	{
		if ((i%4)<2) { dest[i*3] = 255; dest[i*3+1] = 255; dest[i*3+2] = 255; }
		else { dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 0; }
	}
}

static void BEDrawBrushColor (unsigned char * dest, int xoff, int yoff, int linewidth, int idx)
{
	int r, c;
	unsigned char colors[16][3] = {
		{200,200,200},{200,100,100},{100,200,100},{100,100,200},
		{200,200,100},{200,100,200},{100,200,200},{150,150,150},
		{180,80,80},{80,180,80},{80,80,180},{180,180,80},
		{180,80,180},{80,180,180},{120,120,120},{100,100,100}
	};
	unsigned char * col = colors[idx % 16];
	for (r = 0; r < blockheight; r++) {
		for (c = 0; c < blockwidth; c++) {
			unsigned char * px = dest + (xoff + c) * 3 + (yoff + r) * linewidth;
			px[0] = col[0]; px[1] = col[1]; px[2] = col[2];
		}
	}
}

static void BEDrawObjectThumb (unsigned char * dest, int xoff, int yoff, int linewidth, int objidx)
{
	const F14Obj * o = F14_GetObject (objidx);
	int r, c;
	if (o == NULL) return;
	for (r = 0; r < blockheight; r++) {
		for (c = 0; c < blockwidth; c++) {
			unsigned char * px = dest + (xoff + c) * 3 + (yoff + r) * linewidth;
			px[0] = 0x88; px[1] = 0x88; px[2] = 0xCC;
		}
	}
	{
		char txt[16];
		int slen;
		sprintf (txt, "%d", objidx);
		slen = strlen (txt);
		for (c = 0; c < slen && c < 4; c++) {
			int px_x = xoff + 2 + c * 6;
			int px_y = yoff + blockheight / 2 - 3;
			int dy, dx;
			for (dy = 0; dy < 6; dy++) {
				for (dx = 0; dx < 5; dx++) {
					unsigned char * px = dest + (px_x + dx) * 3 + (px_y + dy) * linewidth;
					px[0] = 255; px[1] = 255; px[2] = 255;
				}
			}
		}
	}
}

void BEDrawBlocks (HWND phwnd, HDC myhdc)
{
int i, j, k, l;
HDC fromhdc;
HBITMAP temphbm, hbmpold;
unsigned char * destmempt;
BITMAPINFOHEADER bih;
RGBQUAD rgbq;
RECT rc;
int tabht = TAB_H;

		GetWindowRect (phwnd, &rc);
		k = (rc.right)-(rc.left);
		if (BEZoom()!=1) { k /= BEZoom(); k+= (BEZoom()-1); }
		if (k<blockwidth) k = blockwidth;
		k += 3; k &= -4;
		l = ((rc.bottom)-(rc.top) - tabht)/(blockheight+2);
		if (BEZoom()!=1) { l /= BEZoom(); l+= (BEZoom()-1); }
		if (l < 1) l = 1;

		destmempt = malloc (k*l*(blockheight+2)*3);
		if (destmempt==NULL) return;
		memset (destmempt, 0x44, k*l*(blockheight+2)*3);

		fromhdc = CreateCompatibleDC (myhdc);

		memset (&bih, 0, sizeof(BITMAPINFOHEADER));
		memset (&rgbq, 0, sizeof(RGBQUAD));
		bih.biSize = sizeof (BITMAPINFOHEADER);
		bih.biWidth = k;
		bih.biHeight = -(l*(blockheight+2));
		bih.biPlanes = 1;
		bih.biBitCount = 24;
		bih.biCompression = BI_RGB;

		bebpr = (short int) (k-(24/BEZoom()))/(blockwidth+2);
		if (bebpr<1) bebpr = 1;

		switch (betype) {
		case BETYPE_STILL:
			for (i=0;i<l;i++) {
				for (j=0;j<bebpr;j++) {
					int idx = ((beyoff+i)*bebpr)+j;
					BlockStrto24 (idx, destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
					if (gridon && idx<numblockstr) BEDrawBlockGrid (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, IsBlockAttached (idx));
					if (BEIsSelected(idx)) BEDrawBlockSelect (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
				}
			}
			break;
		case BETYPE_ANIM:
			for (i=0;i<l;i++) {
				for (j=0;j<bebpr;j++) {
					int idx = ((beyoff+i)*bebpr)+j;
					BlockAnimto24 (idx, -1, destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
					if (gridon && idx<numblockanim) BEDrawBlockGrid (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
					if (BEIsSelected(idx)) BEDrawBlockSelect (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
				}
			}
			break;
		case BETYPE_GRAPH:
			for (i=0;i<l;i++) {
				for (j=0;j<bebpr;j++) {
					int idx = ((beyoff+i)*bebpr)+j;
					if (idx < numblockgfx)
						BlockGfxto24 (idx, destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
					if (gridon && idx<numblockgfx) BEDrawBlockGrid (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
					if (BEIsSelected(idx)) BEDrawBlockSelect (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
				}
			}
			break;
		case BETYPE_BRUSHES:
			for (i=0;i<l;i++) {
				for (j=0;j<bebpr;j++) {
					int idx = ((beyoff+i)*bebpr)+j;
					if (idx < MAXBRSH && brshpt[idx] != NULL) {
						BEDrawBrushColor (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, idx);
					}
					if (gridon && idx<MAXBRSH && brshpt[idx]!=NULL) BEDrawBlockGrid (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
					if (BEIsSelected(idx)) BEDrawBlockSelect (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
				}
			}
			break;
		case BETYPE_OBJECTS:
			for (i=0;i<l;i++) {
				for (j=0;j<bebpr;j++) {
					int idx = ((beyoff+i)*bebpr)+j;
					if (idx < F14_NumObjects())
						BEDrawObjectThumb (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, idx);
					if (gridon && idx<F14_NumObjects()) BEDrawBlockGrid (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
					if (BEIsSelected(idx)) BEDrawBlockSelect (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
				}
			}
			break;
		}

		temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
			destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);

		hbmpold = SelectObject(fromhdc, temphbm);
		if (BEZoom()==1)
			BitBlt (myhdc, 0, tabht, k, l*(blockheight+2), fromhdc, 0, 0, SRCCOPY);
		else
			StretchBlt (myhdc, 0, tabht, k*BEZoom(), l*(blockheight+2)*BEZoom(), fromhdc, 0, 0, k, l*(blockheight+2), SRCCOPY);
		SelectObject (fromhdc, hbmpold);
		DeleteObject (temphbm);

		DeleteDC (fromhdc);
		free (destmempt);
}

static void BESetType (int newtype)
{
	if (newtype < 0 || newtype >= BETYPE_COUNT) return;
	if (newtype == betype) return;
	betype = newtype;
	beyoff = 0;
	BEUpdateTabButtons ();
	BESetScrollRange ();
	InvalidateRgn (bhwnd, NULL, FALSE);
}

LRESULT CALLBACK BlockWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
short int i;
HDC myhdc;
PAINTSTRUCT ps;

	switch (message) {
	case WM_PAINT:
		myhdc = BeginPaint (hwnd, &ps);
		if (blockgfxpt!=NULL) {
			BEDrawBlocks (hwnd, myhdc);
			BESetScrollRange();
		}
		EndPaint (hwnd, &ps);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case BTN_STILL:   BESetType (BETYPE_STILL); break;
		case BTN_ANIM:    BESetType (BETYPE_ANIM); break;
		case BTN_GRAPH:   BESetType (BETYPE_GRAPH); break;
		case BTN_BRUSHES: BESetType (BETYPE_BRUSHES); break;
		case BTN_OBJECTS: BESetType (BETYPE_OBJECTS); break;
		}
		break;
	case WM_VSCROLL:
		i = BEItemCount ();
		switch (LOWORD(wParam)) {
			case SB_TOP:
			beyoff = 0;
			break;
			case SB_LINEUP:
			if (beyoff>0) beyoff--;
			break;
			case SB_LINEDOWN:
			if (beyoff<(i-1)) beyoff++;
			break;
			case SB_PAGEUP:
			beyoff-=10;
			if (beyoff<0) beyoff = 0;
			break;
			case SB_PAGEDOWN:
			if (beyoff<(i-2)) beyoff+=10;
			if (beyoff>(i-1)) beyoff=i-1;
			break;
			case SB_BOTTOM:
			beyoff = i-1;
			if (beyoff<0) beyoff = 0;
			break;
			case SB_THUMBPOSITION:
			beyoff = (HIWORD(wParam));
			break;
			}
		SetScrollPos (bhwnd, SB_VERT, beyoff, TRUE);
		InvalidateRgn (bhwnd, NULL, FALSE);
		break;
	case WM_LBUTTONUP:
		if (LOWORD(lParam)>((blockwidth+2)*BEZoom()*bebpr)) break;
		if (HIWORD(lParam) < TAB_H) break;
		{
			int clicked = ((beyoff + ((HIWORD(lParam)-TAB_H)/((blockheight+2)*BEZoom()))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*BEZoom()));
			switch (betype) {
			case BETYPE_STILL:
				curanim = -1; curbrsh = 0;
				curstr = clicked;
				if (curstr>=numblockstr) curstr = numblockstr-1;
				if (curstr<0) curstr = 0;
				RejigBrshMenu (curbrsh);
				break;
			case BETYPE_ANIM:
				curbrsh = 0;
				curanim = clicked;
				if (curanim>=numblockanim) curanim = numblockanim-1;
				if (curanim<0) curanim = 0;
				RejigBrshMenu (curbrsh);
				break;
			case BETYPE_GRAPH:
				curgfxsel = clicked;
				if (curgfxsel>=numblockgfx) curgfxsel = numblockgfx-1;
				if (curgfxsel<0) curgfxsel = 0;
				break;
			case BETYPE_BRUSHES:
				curbrushsel = clicked;
				if (curbrushsel>=MAXBRSH) curbrushsel = MAXBRSH-1;
				if (curbrushsel<0) curbrushsel = 0;
				break;
			case BETYPE_OBJECTS:
				curobjsel = clicked;
				if (curobjsel>=F14_NumObjects()) curobjsel = F14_NumObjects()-1;
				if (curobjsel<0) curobjsel = 0;
				break;
			}
		}
		BESetScrollRange ();
		InvalidateRgn (bhwnd, NULL, FALSE);
		break;
	case WM_RBUTTONUP:
		BESetType ((betype + 1) % BETYPE_COUNT);
		break;
	case WM_LBUTTONDBLCLK:
		if (blockstrpt == NULL) break;
		if (LOWORD(lParam)>((blockwidth+2)*BEZoom()*bebpr)) break;
		if (HIWORD(lParam) < TAB_H) break;
		{
			int clicked = ((beyoff + ((HIWORD(lParam)-TAB_H)/((blockheight+2)*BEZoom()))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*BEZoom()));
			switch (betype) {
			case BETYPE_STILL:
				curanim = -1; curbrsh = 0;
				curstr = clicked;
				if (curstr>=numblockstr) { curstr = numblockstr-1; break; }
				if (curstr<0) curstr = 0;
				RejigBrshMenu (curbrsh);
				DIAEditProperties (hwnd, curstr);
				break;
			case BETYPE_ANIM:
				curbrsh = 0;
				curanim = clicked;
				if (curanim>=numblockanim) { curanim = numblockanim-1; break; }
				if (curanim<0) curanim = 0;
				RejigBrshMenu (curbrsh);
				DIAEditAnimProperties (hwnd, curanim);
				break;
			case BETYPE_GRAPH:
				curgfxsel = clicked;
				if (curgfxsel>=numblockgfx) { curgfxsel = numblockgfx-1; break; }
				if (curgfxsel<0) curgfxsel = 0;
				break;
			case BETYPE_BRUSHES:
				curbrushsel = clicked;
				if (curbrushsel>=MAXBRSH) { curbrushsel = MAXBRSH-1; break; }
				if (curbrushsel<0) curbrushsel = 0;
				break;
			case BETYPE_OBJECTS:
				curobjsel = clicked;
				if (curobjsel>=F14_NumObjects()) { curobjsel = F14_NumObjects()-1; break; }
				if (curobjsel<0) curobjsel = 0;
				break;
			}
		}
		InvalidateAll ();
		break;
	case WM_MOUSEWHEEL:
		if (blockgfxpt==NULL) break;
		{
			int bdelta = (short)HIWORD(wParam);
			i = BEItemCount ();
			if (bdelta>0) { if (beyoff>0) beyoff--; }
			else { if (beyoff<(i-1)) beyoff++; }
			SetScrollPos (bhwnd, SB_VERT, beyoff, TRUE);
			InvalidateRgn (bhwnd, NULL, FALSE);
		}
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

static void BECreateTabButtons (HWND parent)
{
	int bw, bx, i;
	RECT rc;
	const char * labels[] = { "Still", "Anim", "Graph", "Brushes", "Objects" };
	int ids[] = { BTN_STILL, BTN_ANIM, BTN_GRAPH, BTN_BRUSHES, BTN_OBJECTS };

	GetWindowRect (parent, &rc);
	bw = ((rc.right - rc.left) - 4) / BETYPE_COUNT;
	if (bw < 20) bw = 20;

	for (i = 0; i < BETYPE_COUNT; i++) {
		bx = i * bw + 2;
		hbtn[i] = CreateWindow ("button", labels[i],
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			bx, 0, bw, TAB_H,
			parent, (HMENU)(INT_PTR)ids[i], hInstance, NULL);
		SendMessage (hbtn[i], WM_SETFONT, (WPARAM)GetStockObject (DEFAULT_GUI_FONT), TRUE);
	}
	BEUpdateTabButtons ();
}

static void BEUpdateTabButtons (void)
{
	int i;
	for (i = 0; i < BETYPE_COUNT; i++) {
		if (hbtn[i] != NULL) {
			if (i == betype)
				SendMessage (hbtn[i], BM_SETSTATE, TRUE, 0);
			else
				SendMessage (hbtn[i], BM_SETSTATE, FALSE, 0);
		}
	}
}

HWND CreateBlockEditorWindow (HWND hwnd)
{
int x;
RECT rc;
WNDCLASS wndclass;

	if (bhwnd != NULL) return bhwnd;

		memset (&wndclass, 0, sizeof(WNDCLASS));
		wndclass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT | CS_NOCLOSE;
		wndclass.lpfnWndProc = BlockWndProc;
		wndclass.hInstance = hInstance;
		wndclass.hIcon = LoadIcon(hInstance, "MAPWINIC");
		wndclass.hCursor = LoadCursor(NULL, (LPCSTR) IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH) GetStockObject (GRAY_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = szBlockEdname;
		RegisterClass(&wndclass);

	GetWindowRect (hwnd, &rc);
	if (wlayout == 0) x = (rc.right)-(rc.left)-132;
	else if (wlayout == -1) x = (rc.right)-(rc.left)-132;
	else x = 0;
		bhwnd = CreateWindow (szBlockEdname, szBlockEdname,
								WS_MAXIMIZEBOX	| WS_MINIMIZEBOX | WS_CAPTION | WS_CLIPSIBLINGS |
								WS_THICKFRAME | WS_CHILDWINDOW | WS_VSCROLL,
								x, 0, 128,
								(rc.bottom)-(rc.top)-30-GetSystemMetrics(SM_CYCAPTION),
								hwnd, (HMENU) 1001, hInstance, NULL);

	BECreateTabButtons (bhwnd);
	BEUpdateScrollVis ();
	ShowWindow(bhwnd, SW_SHOW);
	UpdateWindow(bhwnd);
	return bhwnd;
}
