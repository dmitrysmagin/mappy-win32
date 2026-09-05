/* objedit.c - Object/Area/Label editors and extra graphic dialog.
 *
 * Dialogs D_OBJECT, D_OBJFNAME; export symbol `objstr` ("OBJSTR %sobjstr[%d]");
 * value names OBJXPOS..OBJFLAGS; "Object %d", "Objects (%d/%d)",
 * "Area only\t(no graphic)"; Col: 000000 = no border, 000001 = black.
 *
 * Areas = objects with tileid<0 ("area only"); labels = areas with 0x0 + text.
 * External graphics come from the OBFN file list (any format via IMLoadImage24).
 * Border colour lives in flags bits 8..31 (low 8 = F1..F8); disk format untouched.
 */
#define STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"
#include "compat.h"
#include "dpi.h"
#include "fmp14.h"
#include "luabind.h"

extern HINSTANCE hInstance;
extern short int mapwidth, mapheight, blockwidth, blockheight, blockdepth;
extern short int numblockstr, numblockgfx, curstr;
extern short int xmapoffset, ymapoffset, zoomon, maplayer;
extern short int * mappt;
extern char * blockgfxpt, * blockstrpt;
extern unsigned char * cmappt;
extern int mapaltered;
extern int blockgapx, blockgapy;
extern unsigned char transred, transgreen, transblue;
/* mapwin.c settings */
extern int objplace, objshow, sortobjects, objlayeruser, zoomdiv;
extern int showuser;
extern char objlabels[16][64];

static F14Obj objclip;
static int objcliphas = 0;
static HFONT oe_font = NULL;

void DIAObjFname (HWND hwnd); /* defined below */

/* border colour = flags high 24 bits (RRGGBB); F1..F8 = low 8 bits */
static int OE_BorderCol (int i)
{
const F14Obj * o = F14_GetObject (i);
if (o == NULL) return 0;
return (o->flags >> 8) & 0xFFFFFF;
}

static void OE_SetBorderCol (F14Obj * o, int col)
{
o->flags = (o->flags & 0xFF) | ((col & 0xFFFFFF) << 8);
}

/* ---------------- object geometry ---------------- */

static int OE_IsArea (const F14Obj * o)
{
	return (o->gfxid == 0 && o->tileid < 0 && (o->gwidth > 0 || o->gheight > 0));
}

static void OE_Rect (const F14Obj * o, int * x, int * y, int * w, int * h)
{
	if (o->gfxid > 0) { *w = blockwidth; *h = blockheight; }
	else { *w = o->gwidth; *h = o->gheight; }
	*x = o->xpos - o->ghandlexoff;
	*y = o->ypos - o->ghandleyoff;
}

static const char * OE_Kind (const F14Obj * o)
{
	if (o->gfxid == 0 && o->tileid < 0) {
		if (o->gwidth == 0 && o->gheight == 0) return " (label)";
		return " (area)";
	}
	if (o->gfxid == 0) return " (ext)";
	return "";
}

/* ---------------- external image cache (top-down 24-bit) ---------------- */

#define OE_IMGCACHE 8
static struct { char name[260]; unsigned char * px; int w, h; unsigned long use; } oecache[OE_IMGCACHE];
static unsigned long oeusec = 0;

static unsigned char * OE_Image (int fi, int * w, int * h)
{
const char * nm;
unsigned char * raw;
int rw, rh, y, i, k, oldest;

	if (fi < 0 || fi >= F14_NumImages ()) return NULL;
	nm = F14_ImageName (fi);
	for (i = 0; i < OE_IMGCACHE; i++) {
		if (oecache[i].px != NULL && !strcmp (oecache[i].name, nm)) {
			oecache[i].use = ++oeusec;
			*w = oecache[i].w; *h = oecache[i].h;
			return oecache[i].px;
		}
	}
	if (IMLoadImage24 ((char *) nm, &raw, &rw, &rh)) return NULL;
	/* flip bottom-up to top-down once */
	for (y = 0; y < rh / 2; y++) {
		int ro = rw * 3;
		unsigned char * a = raw + y * ro;
		unsigned char * b = raw + (rh - 1 - y) * ro;
		int x;
		for (x = 0; x < ro; x++) {
			unsigned char t = a[x];
			a[x] = b[x]; b[x] = t;
		}
	}
	oldest = 0;
	for (k = 0; k < OE_IMGCACHE; k++) {
		if (oecache[k].px == NULL) { oldest = k; break; }
		if (oecache[k].use < oecache[oldest].use) oldest = k;
	}
	i = oldest;
	if (oecache[i].px != NULL) free (oecache[i].px);
	strncpy (oecache[i].name, nm, 259); oecache[i].name[259] = 0;
	oecache[i].px = raw; oecache[i].w = rw; oecache[i].h = rh;
	oecache[i].use = ++oeusec;
	*w = rw; *h = rh;
	return raw;
}

/* ---------------- rendering ---------------- */

static void OE_Blit24 (HDC hdc, int dx, int dy, int dw, int dh,
	unsigned char * topdown, int sw, int sh)
{
HDC mdc;
HBITMAP hbm, hold;
BITMAPINFOHEADER bih;
RGBQUAD rgbq;

	if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
	memset (&bih, 0, sizeof (bih)); memset (&rgbq, 0, sizeof (rgbq));
	bih.biSize = sizeof (bih);
	bih.biWidth = sw; bih.biHeight = -sh;
	bih.biPlanes = 1; bih.biBitCount = 24; bih.biCompression = BI_RGB;
	mdc = CreateCompatibleDC (hdc);
	if (mdc == NULL) return;
	hbm = CreateDIBitmap (hdc, &bih, CBM_INIT, topdown, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
	if (hbm == NULL) { DeleteDC (mdc); return; }
	hold = SelectObject (mdc, hbm);
	StretchBlt (hdc, dx, dy, dw, dh, mdc, 0, 0, sw, sh, SRCCOPY);
	SelectObject (mdc, hold);
	DeleteObject (hbm);
	DeleteDC (mdc);
}

/* transparent blit of top-down 24-bit BGR src (colourkey RGB) via readback */
static void OE_TransBlit (HDC hdc, int dx, int dy, int dw, int dh,
	unsigned char * topdown, int sw, int sh, int kr, int kg, int kb)
{
HDC mdc;
HBITMAP hbm, hold;
BITMAPINFOHEADER bih;
BITMAPINFO bi;
unsigned char * dst;
int x, y;

	if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
	dst = malloc (dw * dh * 3);
	if (dst == NULL) { OE_Blit24 (hdc, dx, dy, dw, dh, topdown, sw, sh); return; }
	memset (&bih, 0, sizeof (bih));
	bih.biSize = sizeof (bih);
	bih.biWidth = dw; bih.biHeight = -dh;
	bih.biPlanes = 1; bih.biBitCount = 24; bih.biCompression = BI_RGB;
	mdc = CreateCompatibleDC (hdc);
	if (mdc == NULL) { free (dst); return; }
	hbm = CreateCompatibleBitmap (hdc, dw, dh);
	if (hbm == NULL) { DeleteDC (mdc); free (dst); return; }
	hold = SelectObject (mdc, hbm);
	BitBlt (mdc, 0, 0, dw, dh, hdc, dx, dy, SRCCOPY);
	memset (&bi, 0, sizeof (bi));
	bi.bmiHeader = bih;
	GetDIBits (mdc, hbm, 0, dh, dst, &bi, DIB_RGB_COLORS);
	for (y = 0; y < dh; y++) {
		for (x = 0; x < dw; x++) {
			unsigned char * sp = topdown + ((y * sh / dh) * sw + (x * sw / dw)) * 3;
			if (sp[2] == kr && sp[1] == kg && sp[0] == kb) continue;
			dst[(y * dw + x) * 3] = sp[0];
			dst[(y * dw + x) * 3 + 1] = sp[1];
			dst[(y * dw + x) * 3 + 2] = sp[2];
		}
	}
	SetDIBits (mdc, hbm, 0, dh, dst, &bi, DIB_RGB_COLORS);
	BitBlt (hdc, dx, dy, dw, dh, mdc, 0, 0, SRCCOPY);
	SelectObject (mdc, hold);
	DeleteObject (hbm);
	DeleteDC (mdc);
	free (dst);
}

static int OE_SortIdx[4096];

void OE_DrawObjects (HDC hdc, RECT * client)
{
int n, i, order, ox, oy;
int zx = zoomon, zd = zoomdiv;

	if (!objshow || mappt == NULL) return;
	n = F14_NumObjects ();
	if (n <= 0) return;
	if (n > 4096) n = 4096;
	for (i = 0; i < n; i++) OE_SortIdx[i] = i;
	if (sortobjects) {
		int k, ns;
		LuaRefreshObjectSort ();
		ns = LuaObjectSortCount ();
		if (ns > 0) {
			int shown = 0;
			for (i = 0; i < ns && shown < 4096; i++) {
				int v = LuaObjectSortAt (i);
				if (v >= 0 && v < n) OE_SortIdx[shown++] = v;
			}
			n = shown;
		} else {
			for (i = 0; i < n; i++) {
				for (k = i + 1; k < n; k++) {
					const F14Obj * a = F14_GetObject (OE_SortIdx[i]);
					const F14Obj * b = F14_GetObject (OE_SortIdx[k]);
					if (a != NULL && b != NULL && a->ypos > b->ypos) {
						int t = OE_SortIdx[i];
						OE_SortIdx[i] = OE_SortIdx[k]; OE_SortIdx[k] = t;
					}
				}
			}
		}
	}
	ox = xmapoffset * blockwidth; oy = ymapoffset * blockheight;
	{
		HGDIOBJ holdf = NULL;
		HWND mapw = MEGetHWND ();
		int off = MapDip (mapw, 2);
		int pw = MapPenWidth (mapw);
		if (oe_font == NULL) oe_font = MapCreateUiFont (mapw);
		if (oe_font) holdf = SelectObject (hdc, oe_font);
	for (order = 0; order < n; order++) {
		const F14Obj * o;
		int mx, my, mw, mh, cx, cy, cw, ch, bc;
		HPEN hpen, hold;
		HBRUSH hbrold;
		char txt[32];
		i = OE_SortIdx[order];
		o = F14_GetObject (i);
		if (o == NULL || !o->show) continue;
		OE_Rect (o, &mx, &my, &mw, &mh);
		if (mw <= 0) mw = 8;
		if (mh <= 0) mh = 8; /* labels (0x0) stay pickable */
		cx = ((mx - ox) * zx) / zd; cy = ((my - oy) * zx) / zd;
		cw = (mw * zx) / zd; ch = (mh * zx) / zd;
		if (cx + cw < 0 || cy + ch < 0 || cx > client->right || cy > client->bottom)
			continue;
		if (!OE_IsArea (o)) {
			if (o->gfxid > 0 && o->gfxid < numblockgfx) {
				unsigned char * tb = malloc (blockwidth * blockheight * 3);
				if (tb != NULL) {
					BlockGfxto24 (o->gfxid, tb, 0, 0, blockwidth * 3);
					OE_Blit24 (hdc, cx, cy, cw, ch, tb, blockwidth, blockheight);
					free (tb);
				}
			} else if (o->tileid >= 0) {
				int iw2, ih2;
				unsigned char * ip = OE_Image (o->tileid, &iw2, &ih2);
				if (ip != NULL) {
					int sx = o->gxoff, sy = o->gyoff, sw = mw, sh = mh, r;
					unsigned char * sub;
					if (sx < 0) sx = 0;
					if (sy < 0) sy = 0;
					if (sw > iw2 - sx) sw = iw2 - sx;
					if (sh > ih2 - sy) sh = ih2 - sy;
					if (sw > 0 && sh > 0) {
						sub = malloc (sw * sh * 3);
						if (sub != NULL) {
							for (r = 0; r < sh; r++)
								memcpy (sub + r * sw * 3, ip + ((sy + r) * iw2 + sx) * 3, sw * 3);
							OE_TransBlit (hdc, cx, cy, cw, ch, sub, sw, sh,
								transred, transgreen, transblue);
							free (sub);
						}
					}
				}
			}
		}
		if (i == F14_CurObject ()) {
			hpen = CreatePen (PS_SOLID, pw, RGB (255, 255, 255));
		} else {
			bc = OE_BorderCol (i);
			if (bc == 0) continue; /* 000000 = no border */
			hpen = CreatePen (PS_SOLID, pw, RGB ((bc >> 16) & 0xFF, (bc >> 8) & 0xFF, bc & 0xFF));
		}
		hold = SelectObject (hdc, hpen);
		hbrold = SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));
		Rectangle (hdc, cx, cy, cx + (cw ? cw : 1) + 1, cy + (ch ? ch : 1) + 1);
		SelectObject (hdc, hold);
		SelectObject (hdc, hbrold);
		DeleteObject (hpen);
		{
			int uf = LuaGetStrObjUser ();
			const char * lab = NULL;
			if (uf >= 1 && uf <= 7) {
				const int * u = &o->user1;
				lab = F14_TstrGet (u[uf - 1]);
			}
			if (lab && lab[0] && o->gwidth == 0 && o->gheight == 0) {
				SetBkMode (hdc, TRANSPARENT);
				SetTextColor (hdc, RGB (255, 255, 255));
				TextOut (hdc, cx + off, cy + off, lab, (int) strlen (lab));
			} else if (i != F14_CurObject ()) {
				bc = OE_BorderCol (i);
				if (bc > 1) {
					SetBkMode (hdc, TRANSPARENT);
					SetTextColor (hdc, RGB ((bc >> 16) & 0xFF, (bc >> 8) & 0xFF, bc & 0xFF));
					sprintf (txt, "Object %d", i);
					TextOut (hdc, cx + off, cy + off, txt, (int) strlen (txt));
				}
			}
		}
	}
		if (holdf) SelectObject (hdc, holdf);
	}
}

/* topmost object containing map pixel (mx,my); -1 if none */
int OE_PickObject (int mx, int my)
{
int n, i, order;

	n = F14_NumObjects ();
	if (n <= 0) return -1;
	if (n > 4096) n = 4096;
	for (order = n - 1; order >= 0; order--) {
		const F14Obj * o = F14_GetObject (order);
		int ox, oy, ow, oh;
		if (o == NULL || !o->show) continue;
		OE_Rect (o, &ox, &oy, &ow, &oh);
		if (ow <= 0) ow = 8;
		if (oh <= 0) oh = 8;
		if (mx >= ox && my >= oy && mx < ox + ow && my < oy + oh) {
			for (i = 0; i < n; i++) {
				const F14Obj * q = F14_GetObject (i);
				int qx, qy, qw, qh;
				if (q == NULL) continue;
				OE_Rect (q, &qx, &qy, &qw, &qh);
				if (qw <= 0) qw = 8;
				if (qh <= 0) qh = 8;
				if (mx < qx || my < qy || mx >= qx + qw || my >= qy + qh) continue;
				if (sortobjects && q->ypos > o->ypos) { o = q; order = i; }
			}
			return order;
		}
	}
	return -1;
}

/* place selected object's handle at map pixel (shift = snap to tiles) */
void OE_PlaceSelected (int mx, int my, int snap)
{
F14Obj o;
int idx = F14_CurObject ();

	if (F14_GetObject (idx) == NULL) return;
	o = *F14_GetObject (idx);
	if (snap) {
		mx = (mx / blockwidth) * blockwidth;
		my = (my / blockheight) * blockheight;
	}
	o.xpos = mx; o.ypos = my;
	F14_SetObject (idx, &o);
	mapaltered = 1;
}

/* overlay text for show-user-info: 1 if an object covers tile (tx,ty) */
int OE_ObjOverlayText (int tx, int ty, char * buf, int nbuf)
{
int n, i, px, py;

	if (F14_NumObjects () <= 0) return 0;
	px = tx * blockwidth + blockwidth / 2;
	py = ty * blockheight + blockheight / 2;
	n = F14_NumObjects ();
	if (n > 4096) n = 4096;
	for (i = n - 1; i >= 0; i--) {
		const F14Obj * o = F14_GetObject (i);
		int ox, oy, ow, oh, uf, val;
		if (o == NULL || !o->show) continue;
		OE_Rect (o, &ox, &oy, &ow, &oh);
		if (ow <= 0) ow = 8;
		if (oh <= 0) oh = 8;
		if (px < ox || py < oy || px >= ox + ow || py >= oy + oh) continue;
		/* object layer filter: layer lives in user field objlayeruser */
		if (objlayeruser >= 1 && objlayeruser <= 7) {
			const int * u = &o->user1;
			if (u[objlayeruser - 1] != maplayer) continue;
		}
		uf = showuser;
		if (uf < 1 || uf > 8) return 0;
		if (uf <= 7) {
			const int * u = &o->user1;
			val = u[uf - 1];
		} else {
			val = (o->show ? 1 : 0) | ((o->flags & 0xFF) << 1);
		}
		if (uf >= 1 && uf <= 7 && uf == LuaGetStrObjUser ()) {
			const char * ts = F14_TstrGet (val);
			if (ts && ts[0]) {
				snprintf (buf, nbuf, "%s", ts);
				return 1;
			}
		}
		snprintf (buf, nbuf, "O%d:%d", i, val);
		return 1;
	}
	return 0;
}

/* OBJSTR text export, original format ("OBJSTR %sobjstr[%d] = {\n",
 * then "{ %d" + 19x ", %d", struct order). */
void OE_ExportObjStr (FILE * fpt, const char * pre, int useconst)
{
int n, i, f;
const int * v;

	n = F14_NumObjects ();
	if (n <= 0) return;
	fprintf (fpt, "OBJSTR %s%sobjstr[%d] = {\n", useconst ? "const " : "", pre, n);
	for (i = 0; i < n; i++) {
		const F14Obj * o = F14_GetObject (i);
		if (o == NULL) continue;
		v = &o->xpos;
		fprintf (fpt, "{ %d", v[0]);
		for (f = 1; f < 20; f++) fprintf (fpt, ", %d", v[f]);
		if (i < n - 1) fprintf (fpt, " },\n");
		else fprintf (fpt, " }\n");
	}
	fprintf (fpt, "};\n\n");
}

/* ---------------- D_OBJECT properties ---------------- */

static void OE_FillStrCombo (HWND hwndDlg, const F14Obj * o)
{
	int uf, cur = 0, i, n;
	uf = LuaGetStrObjUser ();
	if (o != NULL && uf >= 1 && uf <= 7) {
		const int * u = &o->user1;
		cur = u[uf - 1];
	}
	n = F14_TstrCount ();
	SendDlgItemMessage (hwndDlg, IDC_OBJSTR, CB_RESETCONTENT, 0, 0);
	for (i = 0; i < n; i++) {
		const char * s = F14_TstrGet (i);
		SendDlgItemMessage (hwndDlg, IDC_OBJSTR, CB_ADDSTRING, 0,
			(LPARAM) (s ? s : ""));
	}
	if (cur >= 0 && cur < n)
		SendDlgItemMessage (hwndDlg, IDC_OBJSTR, CB_SETCURSEL, cur, 0);
}

static void OE_ApplyStrCombo (HWND hwndDlg, F14Obj * o)
{
	int uf, sel;
	char tb[256];
	if (o == NULL) return;
	uf = LuaGetStrObjUser ();
	sel = (int) SendDlgItemMessage (hwndDlg, IDC_OBJSTR, CB_GETCURSEL, 0, 0);
	GetWindowText (GetDlgItem (hwndDlg, IDC_OBJSTR), tb, 255);
	if (sel >= 0) {
		const char * old = F14_TstrGet (sel);
		if (tb[0] && (old == NULL || strcmp (old, tb))) F14_TstrSet (sel, tb);
	} else if (tb[0]) {
		sel = F14_TstrCount ();
		F14_TstrSet (sel, tb);
	}
	if (sel >= 0 && uf >= 1 && uf <= 7) {
		int * u = &o->user1;
		u[uf - 1] = sel;
	}
}

static void OE_FillFileCombo (HWND hwndDlg)
{
int i, n;
char txt[300];

	SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_ADDSTRING, 0,
		(LPARAM) "Area only\t(no graphic)");
	SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_ADDSTRING, 0,
		(LPARAM) "Tile graphic (see number)");
	n = F14_NumImages ();
	for (i = 0; i < n; i++) {
		sprintf (txt, "External: %s", F14_ImageName (i));
		SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_ADDSTRING, 0, (LPARAM) txt);
	}
}

INT_PTR CALLBACK DIAObjectPropsProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
char txt[300];
static int edidx = 0;
int i;

	switch (msg) {
		case WM_INITDIALOG:
			edidx = (int) lParam;
			LuaRunObjectProp (edidx);
			{
				const F14Obj * o = F14_GetObject (edidx);
				if (o == NULL) { EndDialog (hwndDlg, -1L); break; }
				sprintf (txt, "%d", o->xpos);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJX), txt);
				sprintf (txt, "%d", o->ypos);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJY), txt);
				sprintf (txt, "%d", o->gfxid);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJGFX), txt);
				OE_FillFileCombo (hwndDlg);
				if (o->gfxid > 0)
					SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_SELECTSTRING, -1,
						(LPARAM) "Tile graphic (see number)");
				else if (o->tileid < 0)
					SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_SELECTSTRING, -1,
						(LPARAM) "Area only\t(no graphic)");
				else {
					sprintf (txt, "External: %s", F14_ImageName (o->tileid));
					if (SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_SELECTSTRING, -1,
						(LPARAM) txt) == CB_ERR)
						SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_SETCURSEL, 0, 0);
				}
				sprintf (txt, "%d", o->gxoff);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJGXOFF), txt);
				sprintf (txt, "%d", o->gyoff);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJGYOFF), txt);
				sprintf (txt, "%d", o->gwidth);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJGW), txt);
				sprintf (txt, "%d", o->gheight);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJGH), txt);
				sprintf (txt, "%d", o->ghandlexoff);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJHX), txt);
				sprintf (txt, "%d", o->ghandleyoff);
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJHY), txt);
				CheckDlgButton (hwndDlg, IDC_OBJSHOW, o->show ? 1 : 0);
				{
					const int * u = &o->user1;
					int ids[7] = { IDC_OBJUSER1, IDC_OBJUSER2, IDC_OBJUSER3,
						IDC_OBJUSER4, IDC_OBJUSER5, IDC_OBJUSER6, IDC_OBJUSER7 };
					for (i = 0; i < 7; i++) {
						sprintf (txt, "%d", u[i]);
						SetWindowText (GetDlgItem (hwndDlg, ids[i]), txt);
					}
				}
				{
					int ids[8] = { IDC_OBJF1, IDC_OBJF2, IDC_OBJF3, IDC_OBJF4,
						IDC_OBJF5, IDC_OBJF6, IDC_OBJF7, IDC_OBJF8 };
					for (i = 0; i < 8; i++) {
						CheckDlgButton (hwndDlg, ids[i], (o->flags >> i) & 1);
						SetDlgItemText (hwndDlg, ids[i], objlabels[8 + i]);
					}
				}
				SetDlgItemText (hwndDlg, IDC_OBJLBLMAIN, objlabels[0]);
				sprintf (txt, "%06X", OE_BorderCol (edidx));
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJCOL), txt);
				if (objlayeruser >= 1 && objlayeruser <= 7) {
					const int * u = &o->user1;
					sprintf (txt, "%d", u[objlayeruser - 1]);
				} else strcpy (txt, "0");
				SetWindowText (GetDlgItem (hwndDlg, IDC_OBJLAYER), txt);
				sprintf (txt, "Object %d", edidx);
				SetWindowText (hwndDlg, txt);
				OE_FillStrCombo (hwndDlg, o);
			}
			break;
		case WM_COMMAND:
		switch (LOWORD(wParam))
		{
			case IDC_OBJADDFILE:
				DIAObjFname (GetParent (hwndDlg));
				OE_FillFileCombo (hwndDlg);
				break;
			case IDC_OBJPREV:
			case IDC_OBJNEXT:
			case IDOK:
			{
				F14Obj o;
				char tb[300];
				int gsel;
				if (F14_GetObject (edidx) == NULL) {
					SendMessage (hwndDlg, WM_CLOSE, 0, -1L);
					break;
				}
				o = *F14_GetObject (edidx);
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJX), tb, 80);
				o.xpos = atoi (tb);
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJY), tb, 80);
				o.ypos = atoi (tb);
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJGFX), tb, 80);
				o.gfxid = atoi (tb);
				if (o.gfxid < 0) o.gfxid = 0;
				gsel = SendDlgItemMessage (hwndDlg, IDC_OBJFILE, CB_GETCURSEL, 0, 0);
				if (gsel == 0) { o.gfxid = 0; o.tileid = -1; }
				else if (gsel == 1) { /* tile graphic: keep number */ }
				else o.tileid = gsel - 2;
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJGXOFF), tb, 80);
				o.gxoff = atoi (tb);
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJGYOFF), tb, 80);
				o.gyoff = atoi (tb);
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJGW), tb, 80);
				o.gwidth = atoi (tb);
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJGH), tb, 80);
				o.gheight = atoi (tb);
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJHX), tb, 80);
				o.ghandlexoff = atoi (tb);
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJHY), tb, 80);
				o.ghandleyoff = atoi (tb);
				o.show = IsDlgButtonChecked (hwndDlg, IDC_OBJSHOW) ? 1 : 0;
				{
					int * u = &o.user1;
					int ids[7] = { IDC_OBJUSER1, IDC_OBJUSER2, IDC_OBJUSER3,
						IDC_OBJUSER4, IDC_OBJUSER5, IDC_OBJUSER6, IDC_OBJUSER7 };
					for (i = 0; i < 7; i++) {
						GetWindowText (GetDlgItem (hwndDlg, ids[i]), tb, 80);
						u[i] = atoi (tb);
					}
				}
				{
					int ids[8] = { IDC_OBJF1, IDC_OBJF2, IDC_OBJF3, IDC_OBJF4,
						IDC_OBJF5, IDC_OBJF6, IDC_OBJF7, IDC_OBJF8 };
					o.flags &= ~0xFF;
					for (i = 0; i < 8; i++) {
						if (IsDlgButtonChecked (hwndDlg, ids[i])) o.flags |= (1 << i);
					}
				}
				GetWindowText (GetDlgItem (hwndDlg, IDC_OBJCOL), tb, 80);
				OE_SetBorderCol (&o, strtol (tb, NULL, 16));
				if (objlayeruser >= 1 && objlayeruser <= 7) {
					int * u = &o.user1;
					GetWindowText (GetDlgItem (hwndDlg, IDC_OBJLAYER), tb, 80);
					u[objlayeruser - 1] = atoi (tb);
				}
				OE_ApplyStrCombo (hwndDlg, &o);
				F14_SetObject (edidx, &o);
				mapaltered = 1;
				if (LOWORD (wParam) == IDC_OBJPREV)
					SendMessage (hwndDlg, WM_CLOSE, 0, 1L);
				else if (LOWORD (wParam) == IDC_OBJNEXT)
					SendMessage (hwndDlg, WM_CLOSE, 0, 2L);
				else
					SendMessage (hwndDlg, WM_CLOSE, 0, 0L);
				break;
			}
			case IDCANCEL:
				SendMessage (hwndDlg, WM_CLOSE, 0, -1L);
				break;
		}
		break;
		case WM_CLOSE:
			EndDialog (hwndDlg, lParam);
			break;
		default:
			return 0;
	}
	return -1;
}

void DIAObjectProps (HWND hwnd, int idx)
{
int i;

	i = 1;
	while (i) {
		if (F14_GetObject (idx) == NULL) break;
		i = DialogBoxParam (hInstance, (LPCSTR) "D_OBJECT", hwnd, AsDlgProc(DIAObjectPropsProc), (LPARAM) idx);
		if (i == 1) { if (idx > 0) idx--; else i = 0; }
		else if (i == 2) { if (idx < F14_NumObjects () - 1) idx++; else i = 0; }
	}
	return;
}

/* ---------------- object manager ("Objects (%d/%d)") ---------------- */

static void OE_MgrRefresh (HWND hwndDlg)
{
int n, i;
char txt[64];

	n = F14_NumObjects ();
	SendDlgItemMessage (hwndDlg, IDC_OBJLIST, LB_RESETCONTENT, 0, 0);
	for (i = 0; i < n; i++) {
		const F14Obj * o = F14_GetObject (i);
		sprintf (txt, "Object %d%s", i, o != NULL ? OE_Kind (o) : "");
		SendDlgItemMessage (hwndDlg, IDC_OBJLIST, LB_ADDSTRING, 0, (LPARAM) txt);
	}
	SendDlgItemMessage (hwndDlg, IDC_OBJLIST, LB_SETCURSEL, F14_CurObject (), 0);
	sprintf (txt, "Objects (%d/%d)", F14_CurObject (), n);
	SetWindowText (hwndDlg, txt);
}

INT_PTR CALLBACK DIAObjectMgrProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
int sel, ni;

	UNUSED(lParam);
	switch (msg) {
		case WM_INITDIALOG:
			OE_MgrRefresh (hwndDlg);
			break;
		case WM_COMMAND:
		switch (LOWORD(wParam))
		{
			case IDC_OBJLIST:
				if (HIWORD(wParam) == LBN_SELCHANGE) {
					sel = SendDlgItemMessage (hwndDlg, IDC_OBJLIST, LB_GETCURSEL, 0, 0);
					if (sel >= 0) F14_SetCurObject (sel);
					OE_MgrRefresh (hwndDlg);
				} else if (HIWORD(wParam) == LBN_DBLCLK) {
					sel = SendDlgItemMessage (hwndDlg, IDC_OBJLIST, LB_GETCURSEL, 0, 0);
					if (sel >= 0) {
						F14_SetCurObject (sel);
						DIAObjectProps (hwndDlg, sel);
						OE_MgrRefresh (hwndDlg);
					}
				}
				break;
			case IDC_OBJNEW:
				ni = F14_NewObject ();
				if (ni < 0) {
					MessageBox (hwndDlg, "Can't create object (max reached).", "Objects", MB_OK|MB_ICONINFORMATION);
				} else {
					F14Obj o;
					memset (&o, 0, sizeof (o));
					o.xpos = (xmapoffset + 2) * blockwidth;
					o.ypos = (ymapoffset + 2) * blockheight;
					o.show = 1;
					o.tileid = -1; /* area only by default */
					o.gwidth = blockwidth; o.gheight = blockheight;
					F14_SetObject (ni, &o);
					F14_SetCurObject (ni);
					mapaltered = 1;
				}
				OE_MgrRefresh (hwndDlg);
				break;
			case IDC_OBJCUT:
			case IDC_OBJCOPY:
				sel = F14_CurObject ();
				if (F14_GetObject (sel) != NULL) {
					objclip = *F14_GetObject (sel);
					objcliphas = 1;
					if (LOWORD(wParam) == IDC_OBJCUT) {
						F14_DeleteObject (sel);
						mapaltered = 1;
					}
				}
				OE_MgrRefresh (hwndDlg);
				break;
			case IDC_OBJPASTE:
				if (!objcliphas) break;
				ni = F14_NewObject ();
				if (ni < 0) {
					MessageBox (hwndDlg, "Can't paste object (max reached).", "Objects", MB_OK|MB_ICONINFORMATION);
				} else {
					objclip.xpos += 16; objclip.ypos += 16;
					F14_SetObject (ni, &objclip);
					F14_SetCurObject (ni);
					mapaltered = 1;
				}
				OE_MgrRefresh (hwndDlg);
				break;
			case IDC_OBJEDIT:
				sel = F14_CurObject ();
				if (F14_GetObject (sel) != NULL) DIAObjectProps (hwndDlg, sel);
				OE_MgrRefresh (hwndDlg);
				break;
			case IDC_OBJDEL:
				sel = F14_CurObject ();
				if (F14_GetObject (sel) != NULL &&
					MessageBox (hwndDlg, "Delete this object?", "Objects",
						MB_OKCANCEL|MB_ICONQUESTION) == IDOK) {
					F14_DeleteObject (sel);
					mapaltered = 1;
				}
				OE_MgrRefresh (hwndDlg);
				break;
			case IDOK:
			case IDCANCEL:
				EndDialog (hwndDlg, 0);
				break;
		}
		break;
		default:
			return 0;
	}
	return -1;
}

void DIAObjectMgr (HWND hwnd)
{

	if (mappt == NULL) {
		MessageBox (hwnd, "Load or create a map first.", "Objects", MB_OK|MB_ICONINFORMATION);
		return;
	}
	DialogBox (hInstance, (LPCSTR) "D_OBJMGR", hwnd, AsDlgProc(DIAObjectMgrProc));
	return;
}

/* ---------------- D_OBJFNAME file list ---------------- */

static void OE_FnameRefresh (HWND hwndDlg)
{
int n, i;

	n = F14_NumImages ();
	SendDlgItemMessage (hwndDlg, IDC_OBJFNAMELIST, LB_RESETCONTENT, 0, 0);
	for (i = 0; i < n; i++) {
		SendDlgItemMessage (hwndDlg, IDC_OBJFNAMELIST, LB_ADDSTRING, 0,
			(LPARAM) F14_ImageName (i));
	}
}

INT_PTR CALLBACK DIAObjFnameProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
int sel, i;

	UNUSED(lParam);
	switch (msg) {
		case WM_INITDIALOG:
			OE_FnameRefresh (hwndDlg);
			break;
		case WM_COMMAND:
		switch (LOWORD(wParam))
		{
			case IDC_OBJFNADD:
			{
				OPENFILENAME ofn;
				char szFilter[64], szFile[260], szTitle[260];
				memset (szFilter, 0, sizeof (szFilter));
				strcpy (szFilter, "Images (all)|*.*");
				szFilter[14] = 0;
				szFile[0] = 0; szTitle[0] = 0;
				memset (&ofn, 0, sizeof (ofn));
				ofn.lStructSize = sizeof (ofn);
				ofn.hwndOwner = hwndDlg;
				ofn.lpstrFilter = szFilter;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = sizeof (szFile);
				ofn.lpstrFileTitle = szTitle;
				ofn.nMaxFileTitle = sizeof (szTitle);
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
				if (GetOpenFileName (&ofn)) {
					if (F14_AddImage (szFile) < 0)
						MessageBox (hwndDlg, "Can't add file (list full).", "Object Graphic Files", MB_OK|MB_ICONINFORMATION);
					else mapaltered = 1;
					OE_FnameRefresh (hwndDlg);
				}
				break;
			}
			case IDC_OBJFNDEL:
				sel = SendDlgItemMessage (hwndDlg, IDC_OBJFNAMELIST, LB_GETCURSEL, 0, 0);
				if (sel >= 0) {
					/* re-point objects using files at/after sel */
					for (i = 0; i < F14_NumObjects (); i++) {
						F14Obj o;
						if (F14_GetObject (i) == NULL) continue;
						o = *F14_GetObject (i);
						if (o.gfxid == 0 && o.tileid == sel) o.tileid = -1;
						else if (o.gfxid == 0 && o.tileid > sel) o.tileid--;
						F14_SetObject (i, &o);
					}
					F14_DeleteImage (sel);
					mapaltered = 1;
					OE_FnameRefresh (hwndDlg);
				}
				break;
			case IDOK:
			case IDCANCEL:
				EndDialog (hwndDlg, 0);
				break;
		}
		break;
		default:
			return 0;
	}
	return -1;
}

void DIAObjFname (HWND hwnd)
{

	DialogBox (hInstance, (LPCSTR) "D_OBJFNAME", hwnd, AsDlgProc(DIAObjFnameProc));
	return;
}

/* ---------------- tile Graph editor ('Graph' window ops) ---------------- */

static int curgfx = 0;
static unsigned char * gundo = NULL;
static int gundogfx = -1;

static int OE_TileBytes (void)
{
	return blockwidth * blockheight * ((blockdepth + 1) / 8);
}

static void OE_GfxClamp (void)
{
	if (numblockgfx <= 0) curgfx = 0;
	else if (curgfx >= numblockgfx) curgfx = numblockgfx - 1;
	if (curgfx < 0) curgfx = 0;
}

static void OE_GfxUndoPush (void)
{
int tb = OE_TileBytes ();
unsigned char * nb;

	if (blockgfxpt == NULL) return;
	nb = malloc (tb);
	if (nb == NULL) return;
	memcpy (nb, blockgfxpt + curgfx * tb, tb);
	if (gundo != NULL) free (gundo);
	gundo = nb; gundogfx = curgfx;
}

static void OE_GfxRGB (int col, int * r, int * g, int * b)
{
	if (cmappt != NULL) {
		*r = cmappt[(col & 255) * 3]; *g = cmappt[(col & 255) * 3 + 1]; *b = cmappt[(col & 255) * 3 + 2];
	} else { *r = *g = *b = (col & 255); }
}

static void OE_GfxSetPix (unsigned char * tile, int x, int y, int col)
{
int bpp = (blockdepth + 1) / 8;
unsigned char * p;

	if (x < 0 || y < 0 || x >= blockwidth || y >= blockheight) return;
	p = tile + (y * blockwidth + x) * bpp;
	if (blockdepth == 8) { *p = (unsigned char) col; return; }
	{
		int r, g, b;
		OE_GfxRGB (col, &r, &g, &b);
		if (blockdepth == 15) { p[0] = ((g >> 3) & 0x3) | (b & 0xFC); p[1] = ((r >> 3) & 0x1F) | (((g >> 5) & 0x7) << 5); }
		else if (blockdepth == 16) { p[0] = ((g >> 5) & 0x7) | (b & 0xF8); p[1] = ((r >> 3) & 0x1F) | (((g >> 2) & 0x7) << 5); }
		else if (blockdepth == 32) { p[0] = 0; p[1] = r; p[2] = g; p[3] = b; }
		else { p[0] = b; p[1] = g; p[2] = r; }
	}
}

static void OE_GfxLine (int x0, int y0, int x1, int y1, int thick, int col)
{
unsigned char * tile;
int dx = abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
int dy = -abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
int err = dx + dy, e2, tx, ty;

	tile = (unsigned char *) blockgfxpt + curgfx * OE_TileBytes ();
	for (;;) {
		for (ty = 0; ty < thick; ty++) {
			for (tx = 0; tx < thick; tx++)
				OE_GfxSetPix (tile, x0 + tx, y0 + ty, col);
		}
		if (x0 == x1 && y0 == y1) break;
		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

static void OE_GfxRefresh (HWND hwndDlg)
{
char txt[64];
InvalidateRect (GetDlgItem (hwndDlg, IDC_GFXVIEW), NULL, TRUE);
sprintf (txt, "Graphic %d/%d", curgfx, numblockgfx);
SetWindowText (GetDlgItem (hwndDlg, IDC_GFXNUM), txt);
}

/* copy current tile to Windows clipboard as CF_DIB (24-bit, for paint pkgs) */
static void OE_GfxToClipboard (HWND hwndDlg)
{
unsigned char * tile;
int tb, dibsz;
HGLOBAL hg;
unsigned char * dib;
BITMAPINFOHEADER * bih;
int x, y, bpp;

	(void) hwndDlg;
	tb = OE_TileBytes ();
	tile = (unsigned char *) blockgfxpt + curgfx * tb;
	bpp = (blockdepth + 1) / 8;
	dibsz = sizeof (BITMAPINFOHEADER) + blockwidth * blockheight * 3;
	hg = GlobalAlloc (GMEM_MOVEABLE, dibsz);
	if (hg == NULL) return;
	dib = GlobalLock (hg);
	bih = (BITMAPINFOHEADER *) dib;
	memset (bih, 0, sizeof (*bih));
	bih->biSize = sizeof (*bih);
	bih->biWidth = blockwidth; bih->biHeight = blockheight;
	bih->biPlanes = 1; bih->biBitCount = 24; bih->biCompression = BI_RGB;
	for (y = 0; y < blockheight; y++) {
		for (x = 0; x < blockwidth; x++) {
			unsigned char * d = dib + sizeof (*bih) + ((blockheight - 1 - y) * blockwidth + x) * 3;
			if (blockdepth == 8) {
				int c = tile[y * blockwidth + x];
				if (cmappt != NULL) { d[2] = cmappt[c * 3]; d[1] = cmappt[c * 3 + 1]; d[0] = cmappt[c * 3 + 2]; }
				else d[2] = d[1] = d[0] = c;
			} else {
				unsigned char * s = tile + (y * blockwidth + x) * bpp;
				if (blockdepth == 15) { d[2] = (s[1] & 0x1F) << 3; d[1] = ((((s[0] & 0x3) << 3) | (s[1] >> 5))) << 3; d[0] = (s[0] & 0xFC) << 1; }
				else if (blockdepth == 16) { d[2] = (s[1] & 0x1F) << 3; d[1] = ((((s[0] & 0x7) << 3) | (s[1] >> 5))) << 2; d[0] = s[0] & 0xF8; }
				else if (blockdepth == 32) { d[2] = s[1]; d[1] = s[2]; d[0] = s[3]; }
				else { d[2] = s[2]; d[1] = s[1]; d[0] = s[0]; }
			}
		}
	}
	GlobalUnlock (hg);
	if (OpenClipboard (hwndDlg)) {
		EmptyClipboard ();
		SetClipboardData (CF_DIB, hg);
		CloseClipboard ();
	} else GlobalFree (hg);
}

/* paste CF_DIB (8/24-bit) over current tile, stretched */
static void OE_GfxFromClipboard (HWND hwndDlg)
{
HGLOBAL hg;
BITMAPINFOHEADER * bih;
unsigned char * bits, * pal;
int sw, sh, x, y, topdown = 0;

	(void) hwndDlg;
	if (!IsClipboardFormatAvailable (CF_DIB)) {
		MessageBox (hwndDlg, "No bitmap on the clipboard.", "Graphic Editor", MB_OK|MB_ICONINFORMATION);
		return;
	}
	if (!OpenClipboard (hwndDlg)) return;
	hg = GetClipboardData (CF_DIB);
	if (hg == NULL) { CloseClipboard (); return; }
	bih = (BITMAPINFOHEADER *) GlobalLock (hg);
	if (bih == NULL || bih->biSize < 40) { CloseClipboard (); return; }
	sw = bih->biWidth; sh = bih->biHeight;
	if (sh < 0) { sh = -sh; topdown = 1; }
	if (sw <= 0 || sh <= 0 || sw > 1024 || sh > 1024 ||
		(bih->biBitCount != 8 && bih->biBitCount != 24) ||
		bih->biCompression != BI_RGB) {
		GlobalUnlock (hg); CloseClipboard ();
		MessageBox (hwndDlg, "Clipboard bitmap must be uncompressed 8 or 24-bit.", "Graphic Editor", MB_OK|MB_ICONINFORMATION);
		return;
	}
	bits = (unsigned char *) bih + bih->biSize + (bih->biBitCount == 8 ? 256 * 4 : 0);
	pal = (unsigned char *) bih + bih->biSize;
	OE_GfxUndoPush ();
	{
		unsigned char * tile = (unsigned char *) blockgfxpt + curgfx * OE_TileBytes ();
		int sstride = ((sw * bih->biBitCount / 8) + 3) & (~3);
		for (y = 0; y < blockheight; y++) {
			int sy = y * sh / blockheight;
			unsigned char * srow = bits + (topdown ? sy * sstride : (sh - 1 - sy) * sstride);
			for (x = 0; x < blockwidth; x++) {
				int sx = x * sw / blockwidth;
				if (bih->biBitCount == 24) {
					unsigned char * s = srow + sx * 3;
					int best = 0, bd = 1 << 30, c2;
					if (blockdepth == 8 && cmappt != NULL) {
						for (c2 = 0; c2 < 256; c2++) {
							int dr = cmappt[c2 * 3] - s[2], dg = cmappt[c2 * 3 + 1] - s[1], db = cmappt[c2 * 3 + 2] - s[0];
							int dd = dr * dr + dg * dg + db * db;
							if (dd < bd) { bd = dd; best = c2; }
						}
						OE_GfxSetPix (tile, x, y, best);
					} else if (blockdepth == 8) {
						OE_GfxSetPix (tile, x, y, s[0]);
					} else {
						int r = s[2], g = s[1], b = s[0], k;
						unsigned char tmp[4];
						if (blockdepth == 15) { tmp[0] = ((g >> 3) & 0x3) | (b & 0xFC); tmp[1] = ((r >> 3) & 0x1F) | (((g >> 5) & 0x7) << 5); }
						else if (blockdepth == 16) { tmp[0] = ((g >> 5) & 0x7) | (b & 0xF8); tmp[1] = ((r >> 3) & 0x1F) | (((g >> 2) & 0x7) << 5); }
						else if (blockdepth == 32) { tmp[0] = 0; tmp[1] = r; tmp[2] = g; tmp[3] = b; }
						else { tmp[0] = b; tmp[1] = g; tmp[2] = r; }
						k = (blockdepth + 1) / 8;
						memcpy (tile + (y * blockwidth + x) * k, tmp, k);
					}
				} else {
					int c2 = srow[sx];
					if (blockdepth == 8) OE_GfxSetPix (tile, x, y, c2);
					else OE_GfxSetPix (tile, x, y, (pal[c2 * 4 + 2] + pal[c2 * 4 + 1] + pal[c2 * 4]) / 3);
				}
			}
		}
	}
	GlobalUnlock (hg);
	CloseClipboard ();
	mapaltered = 1;
	OE_GfxRefresh (hwndDlg);
}

INT_PTR CALLBACK DIAGraphEditProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
char txt[80];
LPMEASUREITEMSTRUCT itempt;
LPDRAWITEMSTRUCT drawpt;

	switch (msg) {
		case WM_INITDIALOG:
			OE_GfxClamp ();
			SetWindowText (GetDlgItem (hwndDlg, IDC_GFXCOL), "0");
			SendDlgItemMessage (hwndDlg, IDC_GFXTHICK, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessage (hwndDlg, IDC_GFXTHICK, CB_ADDSTRING, 0, (LPARAM) "1");
			SendDlgItemMessage (hwndDlg, IDC_GFXTHICK, CB_ADDSTRING, 0, (LPARAM) "2");
			SendDlgItemMessage (hwndDlg, IDC_GFXTHICK, CB_ADDSTRING, 0, (LPARAM) "3");
			SendDlgItemMessage (hwndDlg, IDC_GFXTHICK, CB_ADDSTRING, 0, (LPARAM) "4");
			SendDlgItemMessage (hwndDlg, IDC_GFXTHICK, CB_SETCURSEL, 0, 0);
			SetWindowText (GetDlgItem (hwndDlg, IDC_GFXL0), "0");
			SetWindowText (GetDlgItem (hwndDlg, IDC_GFXL1), "0");
			SetWindowText (GetDlgItem (hwndDlg, IDC_GFXL2), "0");
			SetWindowText (GetDlgItem (hwndDlg, IDC_GFXL3), "0");
			OE_GfxRefresh (hwndDlg);
			break;
		case WM_MEASUREITEM:
			itempt = (LPMEASUREITEMSTRUCT) lParam;
			if (LOWORD (wParam) == IDC_GFXVIEW) {
				itempt->CtlType = ODT_BUTTON;
				itempt->CtlID = IDC_GFXVIEW;
				itempt->itemWidth = 96;
				itempt->itemHeight = 96;
			}
			break;
		case WM_DRAWITEM:
			if (LOWORD (wParam) != IDC_GFXVIEW) break;
			drawpt = (LPDRAWITEMSTRUCT) lParam;
			{
				HDC mdc;
				HBITMAP hbm, hold;
				BITMAPINFOHEADER bih;
				RGBQUAD rgbq;
				unsigned char * tb;
				int ww = drawpt->rcItem.right - drawpt->rcItem.left;
				int hh = drawpt->rcItem.bottom - drawpt->rcItem.top;
				OE_GfxClamp ();
				tb = malloc (blockwidth * blockheight * 3);
				if (tb == NULL) break;
				BlockGfxto24 (curgfx, tb, 0, 0, blockwidth * 3);
				memset (&bih, 0, sizeof (bih)); memset (&rgbq, 0, sizeof (rgbq));
				bih.biSize = sizeof (bih);
				bih.biWidth = blockwidth; bih.biHeight = -blockheight;
				bih.biPlanes = 1; bih.biBitCount = 24; bih.biCompression = BI_RGB;
				mdc = CreateCompatibleDC (drawpt->hDC);
				hbm = CreateDIBitmap (drawpt->hDC, &bih, CBM_INIT, tb,
					(BITMAPINFO *) &bih, DIB_RGB_COLORS);
				hold = SelectObject (mdc, hbm);
				StretchBlt (drawpt->hDC, drawpt->rcItem.left, drawpt->rcItem.top,
					ww, hh, mdc, 0, 0, blockwidth, blockheight, SRCCOPY);
				SelectObject (mdc, hold);
				DeleteObject (hbm);
				DeleteDC (mdc);
				free (tb);
			}
			break;
		case WM_COMMAND:
		switch (LOWORD(wParam))
		{
			case IDC_GFXPREV:
				if (curgfx > 0) curgfx--;
				OE_GfxRefresh (hwndDlg);
				break;
			case IDC_GFXNEXT:
				if (curgfx < numblockgfx - 1) curgfx++;
				OE_GfxRefresh (hwndDlg);
				break;
			case IDC_GFXNEW:
				if (numblockgfx >= 4096) {
					MessageBox (hwndDlg, "Graphic limit reached (4096).", "Graphic Editor", MB_OK|MB_ICONINFORMATION);
					break;
				}
				{
					char * ng = realloc (blockgfxpt, (numblockgfx + 1) * OE_TileBytes ());
					if (ng == NULL) {
						MessageBox (hwndDlg, "Out of memory.", "Graphic Editor", MB_OK|MB_ICONEXCLAMATION);
						break;
					}
					blockgfxpt = ng;
					memset (blockgfxpt + numblockgfx * OE_TileBytes (), 0, OE_TileBytes ());
					if (gundo != NULL) { free (gundo); gundo = NULL; gundogfx = -1; }
					curgfx = numblockgfx;
					numblockgfx++;
					mapaltered = 1;
				}
				OE_GfxRefresh (hwndDlg);
				break;
			case IDC_GFXCUT:
				OE_GfxToClipboard (hwndDlg);
				OE_GfxUndoPush ();
				memset (blockgfxpt + curgfx * OE_TileBytes (), 0, OE_TileBytes ());
				mapaltered = 1;
				OE_GfxRefresh (hwndDlg);
				break;
			case IDC_GFXCOPY:
				OE_GfxToClipboard (hwndDlg);
				break;
			case IDC_GFXPASTE:
				OE_GfxFromClipboard (hwndDlg);
				break;
			case IDC_GFXFILL:
				OE_GfxUndoPush ();
				{
					unsigned char * tile = (unsigned char *) blockgfxpt + curgfx * OE_TileBytes ();
					int x, y;
					GetWindowText (GetDlgItem (hwndDlg, IDC_GFXCOL), txt, 80);
					for (y = 0; y < blockheight; y++)
						for (x = 0; x < blockwidth; x++)
							OE_GfxSetPix (tile, x, y, atoi (txt));
					mapaltered = 1;
				}
				OE_GfxRefresh (hwndDlg);
				break;
			case IDC_GFXLINE:
			{
				int x0, y0, x1, y1, th, col;
				OE_GfxUndoPush ();
				GetWindowText (GetDlgItem (hwndDlg, IDC_GFXL0), txt, 80);
				x0 = atoi (txt);
				GetWindowText (GetDlgItem (hwndDlg, IDC_GFXL1), txt, 80);
				y0 = atoi (txt);
				GetWindowText (GetDlgItem (hwndDlg, IDC_GFXL2), txt, 80);
				x1 = atoi (txt);
				GetWindowText (GetDlgItem (hwndDlg, IDC_GFXL3), txt, 80);
				y1 = atoi (txt);
				th = SendDlgItemMessage (hwndDlg, IDC_GFXTHICK, CB_GETCURSEL, 0, 0) + 1;
				if (th < 1 || th > 4) th = 1;
				GetWindowText (GetDlgItem (hwndDlg, IDC_GFXCOL), txt, 80);
				col = atoi (txt);
				OE_GfxLine (x0, y0, x1, y1, th, col);
				mapaltered = 1;
				OE_GfxRefresh (hwndDlg);
				break;
			}
			case IDC_GFXUNDO:
				if (gundo != NULL && gundogfx == curgfx) {
					memcpy (blockgfxpt + curgfx * OE_TileBytes (), gundo, OE_TileBytes ());
					free (gundo); gundo = NULL; gundogfx = -1;
					mapaltered = 1;
					OE_GfxRefresh (hwndDlg);
				} else {
					MessageBox (hwndDlg, "Nothing to undo.", "Graphic Editor", MB_OK|MB_ICONINFORMATION);
				}
				break;
			case IDC_GFXBLK:
				if (numblockstr >= MAXBLKSTR) {
					MessageBox (hwndDlg, "Block structure limit reached.", "Graphic Editor", MB_OK|MB_ICONINFORMATION);
					break;
				}
				{
					long * bgoffpt;
					curstr = numblockstr - 1;
					NewBlock ();
					/* fresh struct is zeroed by InsertBytes gap; point BG at tile */
					bgoffpt = (long *) (blockstrpt + ((long) curstr) * 32);
					*bgoffpt = (long) curgfx * OE_TileBytes ();
					mapaltered = 1;
					MessageBox (hwndDlg, "New block structure created using this graphic.", "Graphic Editor", MB_OK|MB_ICONINFORMATION);
				}
				break;
			case IDOK:
			case IDCANCEL:
				if (gundo != NULL) { free (gundo); gundo = NULL; gundogfx = -1; }
				EndDialog (hwndDlg, 0);
				break;
		}
		break;
		default:
			return 0;
	}
	return -1;
}

void DIAGraphEdit (HWND hwnd)
{

	if (mappt == NULL || blockgfxpt == NULL) {
		MessageBox (hwnd, "Load or create a map first.", "Graphic Editor", MB_OK|MB_ICONINFORMATION);
		return;
	}
	DialogBox (hInstance, (LPCSTR) "D_GRAPHEDIT", hwnd, AsDlgProc(DIAGraphEditProc));
	return;
}
