#define  STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"

static HWND bhwnd = NULL;
static short int beyoff, betype, bebpr = 1;
static char szBlockEdname[] = "Block Editor";
extern HINSTANCE hInstance;
extern short int blockwidth, blockheight, blockdepth, gridon, zoomon;
extern short int mapwidth, mapheight, xmapoffset, ymapoffset;
extern short int numblockstr, curstr, numblockanim, curanim, curbrsh;
extern char * blockgfxpt, * blockstrpt, * animstrpt;
extern int wlayout;

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

void BEBlockNext (void)
{
	if (!(curbrsh>0 && curbrsh<9) && curanim==-1 && curstr<(numblockstr-1) && !betype)
		{ curstr++; BEInvalidate (); }
	if (!(curbrsh>0 && curbrsh<9) && curanim!=-1 && curanim<(numblockanim-1) && betype)
		{ curanim++; BEInvalidate (); }
}

void BEBlockPrev (void)
{
	if (!(curbrsh>0 && curbrsh<9) && curanim==-1 && curstr>0 && !betype)
		{ curstr--; BEInvalidate (); }
	if (!(curbrsh>0 && curbrsh<9) && curanim!=-1 && curanim>0 && betype)
		{ curanim--; BEInvalidate (); }
}

void BESetScrollRange (void)
{
	if (betype) {
		SetWindowText (bhwnd, "Animated Blocks");
		SetScrollRange (bhwnd, SB_VERT, 0, (numblockanim-1)/bebpr, FALSE);
		if (beyoff>=((numblockanim-1)/bebpr)) beyoff = (numblockanim-1)/bebpr;
	} else {
		SetWindowText (bhwnd, "Still Blocks");
		SetScrollRange (bhwnd, SB_VERT, 0, (numblockstr-1)/bebpr, FALSE);
		if (beyoff>=((numblockstr-1)/bebpr)) beyoff = (numblockstr-1)/bebpr;
	}
	if (beyoff<0) beyoff = 0;
	SetScrollPos (bhwnd, SB_VERT, beyoff, TRUE);
}

void BENewBlock (void)
{
	if (!(curbrsh>0 && curbrsh<9) && curanim == -1 && !betype && (numblockstr < MAXBLKSTR)) NewBlock ();
	if (!(curbrsh>0 && curbrsh<9) && curanim != -1 && betype) AnimNew ();
	else if (!(curbrsh>0 && curbrsh<9) && animstrpt==NULL && betype) AnimNew ();
	BESetScrollRange ();
	BEInvalidate ();
}

void BECutBlock (void)
{
	if (!(curbrsh>0 && curbrsh<9) && curanim == -1 && !betype) CutBlock ();
	if (!(curbrsh>0 && curbrsh<9) && curanim != -1 && betype) AnimCut ();
	BESetScrollRange ();
}

void BECopyBlock (void)
{
	if (!(curbrsh>0 && curbrsh<9) && curanim == -1 && !betype) CopyBlock ();
	if (!(curbrsh>0 && curbrsh<9) && curanim != -1 && betype) AnimCopy ();
}

void BEPasteBlock (void)
{
	if (!(curbrsh>0 && curbrsh<9) && curanim == -1 && !betype && (numblockstr < MAXBLKSTR)) PasteBlock ();
	if (!(curbrsh>0 && curbrsh<9) && curanim != -1 && betype) AnimPaste ();
	BESetScrollRange ();
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

void BEDrawBlocks (HWND phwnd, HDC myhdc)
{
int i, j, k, l;
HDC fromhdc;
HBITMAP temphbm, hbmpold;
unsigned char * destmempt;
BITMAPINFOHEADER bih;
RGBQUAD rgbq;
RECT rc;

		GetWindowRect (phwnd, &rc);
		k = (rc.right)-(rc.left);
		if (zoomon!=1) { k /= zoomon; k+= (zoomon-1); }
		if (k<blockwidth) k = blockwidth;
		k += 3; k &= -4;
		l = ((rc.bottom)-(rc.top))/(blockheight+2);
		if (zoomon!=1) { l /= zoomon; l+= (zoomon-1); }

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


		bebpr = (short int) (k-(24/zoomon))/(blockwidth+2);
		if (bebpr<1) bebpr = 1;
		if (betype) {
		for (i=0;i<l;i++)
		{
			for (j=0;j<bebpr;j++) {
				BlockAnimto24 (((beyoff+i)*bebpr)+j, -1, destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
				if (gridon && (((beyoff+i)*bebpr)+j)<numblockanim) BEDrawBlockGrid (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
				if ((((beyoff+i)*bebpr)+j) == curanim && !(curbrsh>0 && curbrsh<9))
					BEDrawBlockSelect (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
			}
		} } else {
		for (i=0;i<l;i++)
		{
			for (j=0;j<bebpr;j++) {
				BlockStrto24 (((beyoff+i)*bebpr)+j, destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
				if (gridon && (((beyoff+i)*bebpr)+j)<numblockstr) BEDrawBlockGrid (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, IsBlockAttached (((beyoff+i)*bebpr)+j));
				if ((((beyoff+i)*bebpr)+j) == curstr && curanim == -1 && !(curbrsh>0 && curbrsh<9))
					BEDrawBlockSelect (destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
			}
		} }

		temphbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
			destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);

		hbmpold = SelectObject(fromhdc, temphbm);
		if (zoomon==1)
			BitBlt (myhdc, 0, 0, k, l*(blockheight+2), fromhdc, 0, 0, SRCCOPY);
		else
			StretchBlt (myhdc, 0, 0, k*zoomon, l*(blockheight+2)*zoomon, fromhdc, 0, 0, k, l*(blockheight+2), SRCCOPY);
		SelectObject (fromhdc, hbmpold);
		DeleteObject (temphbm);

		DeleteDC (fromhdc);
		free (destmempt);
}

LRESULT CALLBACK BlockWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
//char dropname[256];
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
//		ReleaseDC (hwnd, myhdc);
		break;
	case WM_VSCROLL:
		if (betype) i = numblockanim; else i = numblockstr;
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
		if (LOWORD(lParam)>((blockwidth+2)*zoomon*bebpr)) break;
		if (betype && animstrpt!=NULL) {
		curbrsh = 0;
		curanim = ((beyoff + ((HIWORD(lParam))/((blockheight+2)*zoomon))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*zoomon));
		if (curanim>=numblockanim) curanim = numblockanim-1;
		if (curanim<0) curanim = 0;
		} else {
		curanim = -1; curbrsh = 0;
		curstr = ((beyoff + ((HIWORD(lParam))/((blockheight+2)*zoomon))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*zoomon));
		if (curstr>=numblockstr) curstr = numblockstr-1;
		if (curstr<0) curstr = 0;
		}
		RejigBrshMenu (curbrsh);
		InvalidateRgn (bhwnd, NULL, FALSE);
		break;
	case WM_RBUTTONUP:
		if (blockgfxpt != NULL) betype = 1-betype;
		BESetScrollRange ();
		InvalidateRgn (bhwnd, NULL, FALSE);
		break;
	case WM_LBUTTONDBLCLK:
		if (blockstrpt == NULL) break;
		if (LOWORD(lParam)>((blockwidth+2)*zoomon*bebpr)) break;
		if (betype) {
		curbrsh = 0;
		curanim = ((beyoff + ((HIWORD(lParam))/((blockheight+2)*zoomon))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*zoomon));
		if (curanim>=numblockanim) { curstr = numblockanim-1; break; }
		if (curanim<0) curanim = 0;
		} else {
		curanim = -1; curbrsh = 0;
		curstr = ((beyoff + ((HIWORD(lParam))/((blockheight+2)*zoomon))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*zoomon));
		if (curstr>=numblockstr) { curstr = numblockstr-1; break; }
		if (curstr<0) curstr = 0;
		}
		RejigBrshMenu (curbrsh);
		if (curanim==-1 && curbrsh==0) DIAEditProperties (hwnd, curstr);
		if (curanim!=-1 && curbrsh==0) DIAEditAnimProperties (hwnd, curanim);
		InvalidateAll ();
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
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
		wndclass.hbrBackground = NULL; //GetStockObject (GRAY_BRUSH);
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

	ShowWindow(bhwnd, SW_SHOW);
	UpdateWindow(bhwnd);
	return bhwnd;
}