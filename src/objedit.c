/* objedit.c - Object/Area/Label editors.
 *
 * Dialogs D_OBJECT, D_OBJFNAME; export symbol `objstr` ("OBJSTR %sobjstr[%d]");
 * value names OBJXPOS..OBJFLAGS; "Object %d", "Objects (%d/%d)",
 * Graphic kinds: Area / Tile / Image region. Col: 000000 = no border, 000001 = black.
 *
 * Areas = objects with tileid<0 ("area only"); labels = areas with 0x0 + text.
 * External graphics come from the OBFN file list (any format via IMLoadImage24).
 * Border colour lives in flags bits 8..31 (low 8 = F1..F8); disk format untouched.
 */
#define STRICT
#include <windows.h>
#include <commctrl.h>
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

void DIAObjFname(HWND hwnd); /* defined below */

/* border colour = flags high 24 bits (RRGGBB); F1..F8 = low 8 bits */
static int OE_BorderCol(int i) {
    const F14Obj * o = F14_GetObject(i);
    if (o == NULL) return 0;
    return (o->flags >> 8) & 0xFFFFFF;
}

static void OE_SetBorderCol(F14Obj * o, int col) {
    o->flags = (o->flags & 0xFF) | ((col & 0xFFFFFF) << 8);
}

/* ---------------- object geometry ---------------- */

static int OE_IsArea(const F14Obj * o) {
    return (o->gfxid == 0 && o->tileid < 0 && (o->gwidth > 0 || o->gheight > 0));
}

static void OE_Rect(const F14Obj * o, int * x, int * y, int * w, int * h) {
    if (o->gfxid > 0) { *w = blockwidth; *h = blockheight; }
    else { *w = o->gwidth; *h = o->gheight; }
    *x = o->xpos - o->ghandlexoff;
    *y = o->ypos - o->ghandleyoff;
}

static const char * OE_Kind(const F14Obj * o) {
    if (o->gfxid == 0 && o->tileid < 0) {
        if (o->gwidth == 0 && o->gheight == 0) return " (label)";
        return " (area)";
    }
    if (o->gfxid == 0) return " (ext)";
    return " (tile)";
}

void OE_Describe(int i, char * buf, int n) {
    const F14Obj * o = F14_GetObject(i);
    if (buf == NULL || n < 1) return;
    if (o == NULL) snprintf(buf, (size_t) n, "Object %d", i);
    else snprintf(buf, (size_t) n, "Object %d%s", i, OE_Kind(o));
}

/* Clone current (or a default area) at map pixels. Shift snaps to tiles. */
int OE_CloneAt(int mx, int my, int snap) {
    const F14Obj * src = F14_GetObject(F14_CurObject());
    F14Obj o;
    int ni = F14_NewObject();
    if (ni < 0) return -1;
    if (src != NULL) o = *src;
    else {
        memset(&o, 0, sizeof(o));
        o.show = 1;
        o.tileid = -1;
        o.gwidth = blockwidth > 0 ? blockwidth : 16;
        o.gheight = blockheight > 0 ? blockheight : 16;
    }
    if (snap && blockwidth > 0 && blockheight > 0) {
        mx = (mx / blockwidth) * blockwidth;
        my = (my / blockheight) * blockheight;
    }
    o.xpos = mx;
    o.ypos = my;
    o.show = 1;
    F14_SetObject(ni, &o);
    F14_SetCurObject(ni);
    mapaltered = 1;
    return ni;
}

int OE_NewDefault(void) {
    F14Obj o;
    int ni = F14_NewObject();
    if (ni < 0) return -1;
    memset(&o, 0, sizeof(o));
    o.show = 1;
    o.tileid = -1;
    o.gwidth = blockwidth > 0 ? blockwidth : 16;
    o.gheight = blockheight > 0 ? blockheight : 16;
    o.xpos = (xmapoffset + 2) * (blockwidth > 0 ? blockwidth : 16);
    o.ypos = (ymapoffset + 2) * (blockheight > 0 ? blockheight : 16);
    F14_SetObject(ni, &o);
    F14_SetCurObject(ni);
    mapaltered = 1;
    return ni;
}

/* ---------------- external image cache (top-down 24-bit) ---------------- */

#define OE_IMGCACHE 8
static struct { char name[260]; unsigned char * px; int w, h; unsigned long use; } oecache[OE_IMGCACHE];
static unsigned long oeusec = 0;

static unsigned char * OE_Image(int fi, int * w, int * h) {
    const char * nm;
    unsigned char * raw;
    int rw, rh, y, i, k, oldest;

    if (fi < 0 || fi >= F14_NumImages()) return NULL;
    nm = F14_ImageName(fi);
    for (i = 0; i < OE_IMGCACHE; i++) {
        if (oecache[i].px != NULL && !strcmp(oecache[i].name, nm)) {
            oecache[i].use = ++oeusec;
            *w = oecache[i].w; *h = oecache[i].h;
            return oecache[i].px;
        }
    }
    if (IMLoadImage24((char *) nm, &raw, &rw, &rh)) return NULL;
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
    if (oecache[i].px != NULL) free(oecache[i].px);
    strncpy(oecache[i].name, nm, 259); oecache[i].name[259] = 0;
    oecache[i].px = raw; oecache[i].w = rw; oecache[i].h = rh;
    oecache[i].use = ++oeusec;
    *w = rw; *h = rh;
    return raw;
}

/* ---------------- rendering ---------------- */

static void OE_Blit24(HDC hdc, int dx, int dy, int dw, int dh,
    unsigned char * topdown, int sw, int sh) {
    HDC mdc;
    HBITMAP hbm, hold;
    BITMAPINFOHEADER bih;
    RGBQUAD rgbq;

    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    {
        int pitch = (sw * 3 + 3) & ~3;
        unsigned char * src = topdown;
        int own = 0, y;
        if (pitch != sw * 3) {
            src = malloc((size_t) pitch * sh);
            if (src == NULL) return;
            own = 1;
            for (y = 0; y < sh; y++)
                memcpy(src + y * pitch, topdown + y * sw * 3, (size_t) sw * 3);
        }
    memset(&bih, 0, sizeof(bih)); memset(&rgbq, 0, sizeof(rgbq));
    bih.biSize = sizeof(bih);
    bih.biWidth = sw; bih.biHeight = -sh;
    bih.biPlanes = 1; bih.biBitCount = 24; bih.biCompression = BI_RGB;
    mdc = CreateCompatibleDC(hdc);
    if (mdc == NULL) { if (own) free(src); return; }
    hbm = CreateDIBitmap(hdc, &bih, CBM_INIT, src, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
    if (hbm == NULL) { DeleteDC(mdc); if (own) free(src); return; }
    hold = SelectObject(mdc, hbm);
    StretchBlt(hdc, dx, dy, dw, dh, mdc, 0, 0, sw, sh, SRCCOPY);
    SelectObject(mdc, hold);
    DeleteObject(hbm);
    DeleteDC(mdc);
    if (own) free(src);
    }
}

/* Fit a source size into dw x dh (aspect preserved). Returns 1 if ok. */
static int OE_FitBox(int sw, int sh, int dw, int dh, int * ox, int * oy, int * tw, int * th)
{
    if (sw < 1 || sh < 1 || dw < 1 || dh < 1)
        return 0;
    if (sw * dh > sh * dw) {
        *tw = dw;
        *th = (sh * dw) / sw;
    } else {
        *th = dh;
        *tw = (sw * dh) / sh;
    }
    if (*tw < 1) *tw = 1;
    if (*th < 1) *th = 1;
    *ox = (dw - *tw) / 2;
    *oy = (dh - *th) / 2;
    return 1;
}

/* Panel thumb: tile graphic, or shrunk file region. 0 = area/label (keep rect). */
int OE_DrawThumb(HDC hdc, int dx, int dy, int dw, int dh, int i)
{
    const F14Obj * o = F14_GetObject(i);
    int ox, oy, tw, th;

    if (hdc == NULL || o == NULL || dw < 1 || dh < 1)
        return 0;
    if (o->gfxid > 0 && o->gfxid < numblockgfx && blockwidth > 0 && blockheight > 0) {
        unsigned char * tb = malloc((size_t) blockwidth * (size_t) blockheight * 3);
        if (tb == NULL)
            return 0;
        BlockGfxto24(o->gfxid, tb, 0, 0, blockwidth * 3);
        OE_Blit24(hdc, dx, dy, dw, dh, tb, blockwidth, blockheight);
        free(tb);
        return 1;
    }
    if (o->gfxid == 0 && o->tileid >= 0) {
        int iw, ih, sx, sy, sw, sh, r;
        unsigned char * ip = OE_Image(o->tileid, &iw, &ih);
        unsigned char * sub;
        if (ip == NULL)
            return 0;
        sx = o->gxoff;
        sy = o->gyoff;
        sw = o->gwidth;
        sh = o->gheight;
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sx >= iw || sy >= ih)
            return 0;
        if (sw <= 0) sw = iw - sx;
        if (sh <= 0) sh = ih - sy;
        if (sw > iw - sx) sw = iw - sx;
        if (sh > ih - sy) sh = ih - sy;
        if (sw < 1 || sh < 1)
            return 0;
        if (!OE_FitBox(sw, sh, dw, dh, &ox, &oy, &tw, &th))
            return 0;
        sub = malloc((size_t) sw * (size_t) sh * 3);
        if (sub == NULL)
            return 0;
        for (r = 0; r < sh; r++)
            memcpy(sub + r * sw * 3, ip + ((sy + r) * iw + sx) * 3, (size_t) sw * 3);
        OE_Blit24(hdc, dx + ox, dy + oy, tw, th, sub, sw, sh);
        free(sub);
        return 1;
    }
    return 0;
}

/* transparent blit of top-down 24-bit BGR src (colourkey RGB) via readback */
static void OE_TransBlit(HDC hdc, int dx, int dy, int dw, int dh,
    unsigned char * topdown, int sw, int sh, int kr, int kg, int kb) {
    HDC mdc;
    HBITMAP hbm, hold;
    BITMAPINFOHEADER bih;
    BITMAPINFO bi;
    unsigned char * dst;
    int x, y;

    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    dst = malloc(dw * dh * 3);
    if (dst == NULL) { OE_Blit24(hdc, dx, dy, dw, dh, topdown, sw, sh); return; }
    memset(&bih, 0, sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biWidth = dw; bih.biHeight = -dh;
    bih.biPlanes = 1; bih.biBitCount = 24; bih.biCompression = BI_RGB;
    mdc = CreateCompatibleDC(hdc);
    if (mdc == NULL) { free(dst); return; }
    hbm = CreateCompatibleBitmap(hdc, dw, dh);
    if (hbm == NULL) { DeleteDC(mdc); free(dst); return; }
    hold = SelectObject(mdc, hbm);
    BitBlt(mdc, 0, 0, dw, dh, hdc, dx, dy, SRCCOPY);
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader = bih;
    GetDIBits(mdc, hbm, 0, dh, dst, &bi, DIB_RGB_COLORS);
    for (y = 0; y < dh; y++) {
        for (x = 0; x < dw; x++) {
            unsigned char * sp = topdown + ((y * sh / dh) * sw + (x * sw / dw)) * 3;
            if (sp[2] == kr && sp[1] == kg && sp[0] == kb) continue;
            dst[(y * dw + x) * 3] = sp[0];
            dst[(y * dw + x) * 3 + 1] = sp[1];
            dst[(y * dw + x) * 3 + 2] = sp[2];
        }
    }
    SetDIBits(mdc, hbm, 0, dh, dst, &bi, DIB_RGB_COLORS);
    BitBlt(hdc, dx, dy, dw, dh, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, hold);
    DeleteObject(hbm);
    DeleteDC(mdc);
    free(dst);
}

static int OE_SortIdx[4096];

void OE_DrawObjects(HDC hdc, RECT * client) {
    int n, i, order, ox, oy;
    int zx = zoomon, zd = zoomdiv;

    if (mappt == NULL) return;
    if (!objshow && !objplace) return;
    n = F14_NumObjects();
    if (n <= 0) return;
    if (n > 4096) n = 4096;
    for (i = 0; i < n; i++) OE_SortIdx[i] = i;
    if (sortobjects) {
        int k, ns;
        LuaRefreshObjectSort();
        ns = LuaObjectSortCount();
        if (ns > 0) {
            int shown = 0;
            for (i = 0; i < ns && shown < 4096; i++) {
                int v = LuaObjectSortAt(i);
                if (v >= 0 && v < n) OE_SortIdx[shown++] = v;
            }
            n = shown;
        } else {
            for (i = 0; i < n; i++) {
                for (k = i + 1; k < n; k++) {
                    const F14Obj * a = F14_GetObject(OE_SortIdx[i]);
                    const F14Obj * b = F14_GetObject(OE_SortIdx[k]);
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
        HWND mapw = MEGetHWND();
        int off = MapDip(mapw, 2);
        int pw = MapPenWidth(mapw);
        if (oe_font == NULL) oe_font = MapCreateUiFont(mapw);
        if (oe_font) holdf = SelectObject(hdc, oe_font);
        for (order = 0; order < n; order++) {
            const F14Obj * o;
            int mx, my, mw, mh, cx, cy, cw, ch, bc;
            HPEN hpen, hold;
            HBRUSH hbrold;
            char txt[32];
            i = OE_SortIdx[order];
            o = F14_GetObject(i);
            if (o == NULL || !o->show) continue;
            OE_Rect(o, &mx, &my, &mw, &mh);
            if (mw <= 0) mw = 8;
            if (mh <= 0) mh = 8; /* labels (0x0) stay pickable */
            cx = ((mx - ox) * zx) / zd; cy = ((my - oy) * zx) / zd;
            cw = (mw * zx) / zd; ch = (mh * zx) / zd;
            if (cx + cw < 0 || cy + ch < 0 || cx > client->right || cy > client->bottom)
                continue;
            if (!OE_IsArea(o)) {
                if (o->gfxid > 0 && o->gfxid < numblockgfx) {
                    unsigned char * tb = malloc(blockwidth * blockheight * 3);
                    if (tb != NULL) {
                        BlockGfxto24(o->gfxid, tb, 0, 0, blockwidth * 3);
                        OE_Blit24(hdc, cx, cy, cw, ch, tb, blockwidth, blockheight);
                        free(tb);
                    }
                } else if (o->tileid >= 0) {
                    int iw2, ih2;
                    unsigned char * ip = OE_Image(o->tileid, &iw2, &ih2);
                    if (ip != NULL) {
                        int sx = o->gxoff, sy = o->gyoff, sw = mw, sh = mh, r;
                        unsigned char * sub;
                        if (sx < 0) sx = 0;
                        if (sy < 0) sy = 0;
                        if (sw > iw2 - sx) sw = iw2 - sx;
                        if (sh > ih2 - sy) sh = ih2 - sy;
                        if (sw > 0 && sh > 0) {
                            sub = malloc(sw * sh * 3);
                            if (sub != NULL) {
                                for (r = 0; r < sh; r++)
                                    memcpy(sub + r * sw * 3, ip + ((sy + r) * iw2 + sx) * 3, sw * 3);
                                OE_TransBlit(hdc, cx, cy, cw, ch, sub, sw, sh,
                                    transred, transgreen, transblue);
                                free(sub);
                            }
                        }
                    }
                }
            }
            if (i == F14_CurObject()) {
                hpen = CreatePen(PS_SOLID, pw, objplace ? RGB(255, 255, 0) : RGB(255, 255, 255));
            } else if (objplace) {
                hpen = CreatePen(PS_DOT, pw, RGB(180, 180, 80));
            } else {
                bc = OE_BorderCol(i);
                if (bc == 0) continue; /* 000000 = no border */
                hpen = CreatePen(PS_SOLID, pw, RGB((bc >> 16) & 0xFF, (bc >> 8) & 0xFF, bc & 0xFF));
            }
            hold = SelectObject(hdc, hpen);
            hbrold = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, cx, cy, cx + (cw ? cw : 1) + 1, cy + (ch ? ch : 1) + 1);
            SelectObject(hdc, hold);
            SelectObject(hdc, hbrold);
            DeleteObject(hpen);
            {
                int uf = LuaGetStrObjUser();
                const char * lab = NULL;
                if (uf >= 1 && uf <= 7) {
                    const int * u = &o->user1;
                    lab = F14_TstrGet(u[uf - 1]);
                }
                if (lab && lab[0] && o->gwidth == 0 && o->gheight == 0) {
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(255, 255, 255));
                    TextOut(hdc, cx + off, cy + off, lab, (int) strlen(lab));
                } else if (i != F14_CurObject()) {
                    bc = OE_BorderCol(i);
                    if (bc > 1) {
                        SetBkMode(hdc, TRANSPARENT);
                        SetTextColor(hdc, RGB((bc >> 16) & 0xFF, (bc >> 8) & 0xFF, bc & 0xFF));
                        sprintf(txt, "Object %d", i);
                        TextOut(hdc, cx + off, cy + off, txt, (int) strlen(txt));
                    }
                }
            }
        }
        if (holdf) SelectObject(hdc, holdf);
    }
}

/* topmost object containing map pixel (mx,my); -1 if none */
int OE_PickObject(int mx, int my) {
    int n, i, order;

    n = F14_NumObjects();
    if (n <= 0) return -1;
    if (n > 4096) n = 4096;
    for (order = n - 1; order >= 0; order--) {
        const F14Obj * o = F14_GetObject(order);
        int ox, oy, ow, oh;
        if (o == NULL || !o->show) continue;
        OE_Rect(o, &ox, &oy, &ow, &oh);
        if (ow <= 0) ow = 8;
        if (oh <= 0) oh = 8;
        if (mx >= ox && my >= oy && mx < ox + ow && my < oy + oh) {
            for (i = 0; i < n; i++) {
                const F14Obj * q = F14_GetObject(i);
                int qx, qy, qw, qh;
                if (q == NULL) continue;
                OE_Rect(q, &qx, &qy, &qw, &qh);
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
void OE_PlaceSelected(int mx, int my, int snap) {
    F14Obj o;
    int idx = F14_CurObject();

    if (F14_GetObject(idx) == NULL) return;
    o = *F14_GetObject(idx);
    if (snap) {
        mx = (mx / blockwidth) * blockwidth;
        my = (my / blockheight) * blockheight;
    }
    o.xpos = mx; o.ypos = my;
    F14_SetObject(idx, &o);
    mapaltered = 1;
}

/* overlay text for show-user-info: 1 if an object covers tile (tx,ty) */
int OE_ObjOverlayText(int tx, int ty, char * buf, int nbuf) {
    int n, i, px, py;

    if (F14_NumObjects() <= 0) return 0;
    px = tx * blockwidth + blockwidth / 2;
    py = ty * blockheight + blockheight / 2;
    n = F14_NumObjects();
    if (n > 4096) n = 4096;
    for (i = n - 1; i >= 0; i--) {
        const F14Obj * o = F14_GetObject(i);
        int ox, oy, ow, oh, uf, val;
        if (o == NULL || !o->show) continue;
        OE_Rect(o, &ox, &oy, &ow, &oh);
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
        if (uf >= 1 && uf <= 7 && uf == LuaGetStrObjUser()) {
            const char * ts = F14_TstrGet(val);
            if (ts && ts[0]) {
                snprintf(buf, nbuf, "%s", ts);
                return 1;
            }
        }
        snprintf(buf, nbuf, "O%d:%d", i, val);
        return 1;
    }
    return 0;
}

/* OBJSTR text export, original format ("OBJSTR %sobjstr[%d] = {\n",
 * then "{ %d" + 19x ", %d", struct order). */
void OE_ExportObjStr(FILE * fpt, const char * pre, int useconst) {
    int n, i, f;
    const int * v;

    n = F14_NumObjects();
    if (n <= 0) return;
    fprintf(fpt, "OBJSTR %s%sobjstr[%d] = {\n", useconst ? "const " : "", pre, n);
    for (i = 0; i < n; i++) {
        const F14Obj * o = F14_GetObject(i);
        if (o == NULL) continue;
        v = &o->xpos;
        fprintf(fpt, "{ %d", v[0]);
        for (f = 1; f < 20; f++) fprintf(fpt, ", %d", v[f]);
        if (i < n - 1) fprintf(fpt, " },\n");
        else fprintf(fpt, " }\n");
    }
    fprintf(fpt, "};\n\n");
}

/* ---------------- D_OBJECT properties ---------------- */

static void OE_FillStrCombo(HWND hwndDlg, const F14Obj * o) {
    int uf, cur = 0, i, n;
    uf = LuaGetStrObjUser();
    if (o != NULL && uf >= 1 && uf <= 7) {
        const int * u = &o->user1;
        cur = u[uf - 1];
    }
    n = F14_TstrCount();
    SendDlgItemMessage(hwndDlg, IDC_OBJSTR, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < n; i++) {
        const char * s = F14_TstrGet(i);
        SendDlgItemMessage(hwndDlg, IDC_OBJSTR, CB_ADDSTRING, 0,
            (LPARAM) (s ? s : ""));
    }
    if (cur >= 0 && cur < n)
        SendDlgItemMessage(hwndDlg, IDC_OBJSTR, CB_SETCURSEL, cur, 0);
}

static void OE_ApplyStrCombo(HWND hwndDlg, F14Obj * o) {
    int uf, sel;
    char tb[256];
    if (o == NULL) return;
    uf = LuaGetStrObjUser();
    sel = (int) SendDlgItemMessage(hwndDlg, IDC_OBJSTR, CB_GETCURSEL, 0, 0);
    GetWindowText(GetDlgItem(hwndDlg, IDC_OBJSTR), tb, 255);
    if (sel >= 0) {
        const char * old = F14_TstrGet(sel);
        if (tb[0] && (old == NULL || strcmp(old, tb))) F14_TstrSet(sel, tb);
    } else if (tb[0]) {
        sel = F14_TstrCount();
        F14_TstrSet(sel, tb);
    }
    if (sel >= 0 && uf >= 1 && uf <= 7) {
        int * u = &o->user1;
        u[uf - 1] = sel;
    }
}

enum { OE_MODE_AREA = 0, OE_MODE_TILE = 1, OE_MODE_FILE = 2 };

static int oe_edgfx;
static int oe_filling;

static void OE_FillFileCombo(HWND hwndDlg, int sel) {
    int i, n;

    SendDlgItemMessage(hwndDlg, IDC_OBJFILE, CB_RESETCONTENT, 0, 0);
    n = F14_NumImages();
    if (n <= 0) {
        SendDlgItemMessage(hwndDlg, IDC_OBJFILE, CB_ADDSTRING, 0,
            (LPARAM) "(no image files — click Add)");
        SendDlgItemMessage(hwndDlg, IDC_OBJFILE, CB_SETCURSEL, 0, 0);
        return;
    }
    for (i = 0; i < n; i++)
        SendDlgItemMessage(hwndDlg, IDC_OBJFILE, CB_ADDSTRING, 0,
            (LPARAM) F14_ImageName(i));
    if (sel < 0) sel = 0;
    if (sel >= n) sel = n - 1;
    SendDlgItemMessage(hwndDlg, IDC_OBJFILE, CB_SETCURSEL, sel, 0);
}

static int OE_PropsGetMode(HWND dlg) {
    if (IsDlgButtonChecked(dlg, IDC_OBJKIND_TILE)) return OE_MODE_TILE;
    if (IsDlgButtonChecked(dlg, IDC_OBJKIND_FILE)) return OE_MODE_FILE;
    return OE_MODE_AREA;
}

static void OE_Show(HWND dlg, int id, int on) {
    HWND w = GetDlgItem(dlg, id);
    if (w) ShowWindow(w, on ? SW_SHOW : SW_HIDE);
}

static void OE_PropsTileLab(HWND dlg) {
    char t[64];
    if (numblockgfx <= 1)
        strcpy(t, "No usable tiles");
    else
        sprintf(t, "Tile %d of %d", oe_edgfx, numblockgfx - 1);
    SetDlgItemText(dlg, IDC_OBJTILELAB, t);
}

static void OE_MoveDlg(HWND dlg, int id, int x, int y, int w, int h) {
    RECT r;
    HWND ctl = GetDlgItem(dlg, id);
    if (ctl == NULL) return;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    MapDialogRect(dlg, &r);
    SetWindowPos(ctl, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

static void OE_RebuddySpin(HWND dlg, int editId, int spinId) {
    HWND ed = GetDlgItem(dlg, editId);
    HWND ud = GetDlgItem(dlg, spinId);
    if (ed && ud) SendMessage(ud, UDM_SETBUDDY, (WPARAM) ed, 0);
}

static void OE_ShowPair(HWND dlg, int editId, int spinId, int on) {
    OE_Show(dlg, editId, on);
    OE_Show(dlg, spinId, on);
}

static void OE_MakeSpin(HWND dlg, int editId, int spinId, int lo, int hi) {
    HWND ed, ud;
    RECT rc;
    int sw;

    if (GetDlgItem(dlg, spinId) != NULL) return;
    ed = GetDlgItem(dlg, editId);
    if (ed == NULL) return;
    GetWindowRect(ed, &rc);
    MapWindowPoints(NULL, dlg, (POINT *) &rc, 2);
    sw = GetSystemMetrics(SM_CXVSCROLL);
    if (sw < 16) sw = 16;
    ud = CreateWindowEx(WS_EX_CLIENTEDGE, UPDOWN_CLASS, "",
        WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT |
        UDS_ARROWKEYS | UDS_NOTHOUSANDS,
        rc.right - sw, rc.top, sw, rc.bottom - rc.top,
        dlg, (HMENU) (INT_PTR) spinId, hInstance, NULL);
    if (ud == NULL) return;
    SendMessage(ud, UDM_SETRANGE32, (WPARAM) lo, (LPARAM) hi);
    SendMessage(ud, UDM_SETBUDDY, (WPARAM) ed, 0);
}

static void OE_InitPropSpins(HWND dlg) {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&icc);
    OE_MakeSpin(dlg, IDC_OBJX, IDC_OBJXSPIN, -99999, 999999);
    OE_MakeSpin(dlg, IDC_OBJY, IDC_OBJYSPIN, -99999, 999999);
    OE_MakeSpin(dlg, IDC_OBJHX, IDC_OBJHXSPIN, -99999, 999999);
    OE_MakeSpin(dlg, IDC_OBJHY, IDC_OBJHYSPIN, -99999, 999999);
    OE_MakeSpin(dlg, IDC_OBJGXOFF, IDC_OBJGXSPIN, 0, 99999);
    OE_MakeSpin(dlg, IDC_OBJGYOFF, IDC_OBJGYSPIN, 0, 99999);
    OE_MakeSpin(dlg, IDC_OBJGW, IDC_OBJGWSPIN, 0, 99999);
    OE_MakeSpin(dlg, IDC_OBJGH, IDC_OBJGHSPIN, 0, 99999);
}

static void OE_InitRegSpins(HWND dlg) {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&icc);
    OE_MakeSpin(dlg, IDC_OBJREGX, IDC_OBJREGXSPIN, 0, 99999);
    OE_MakeSpin(dlg, IDC_OBJREGY, IDC_OBJREGYSPIN, 0, 99999);
    OE_MakeSpin(dlg, IDC_OBJREGW, IDC_OBJREGWSPIN, 1, 99999);
    OE_MakeSpin(dlg, IDC_OBJREGH, IDC_OBJREGHSPIN, 1, 99999);
}

static void OE_SetDlgInt(HWND dlg, int id, int v) {
    char t[32];
    sprintf(t, "%d", v);
    SetDlgItemText(dlg, id, t);
}

static int OE_GetDlgInt(HWND dlg, int id) {
    char t[32];
    GetDlgItemText(dlg, id, t, 32);
    return atoi(t);
}

static void OE_InvalidatePrev(HWND dlg) {
    HWND w = GetDlgItem(dlg, IDC_OBJGFXPREV);
    if (w) InvalidateRect(w, NULL, TRUE);
}

static void OE_PropsShowMode(HWND dlg, int mode) {
    int area = (mode == OE_MODE_AREA);
    int tile = (mode == OE_MODE_TILE);
    int file = (mode == OE_MODE_FILE);

    OE_Show(dlg, IDC_OBJHINT, area);
    OE_Show(dlg, IDC_OBJCOORLBL, area);
    OE_Show(dlg, IDC_OBJSIZELBL, area);
    OE_Show(dlg, IDC_OBJFILELBL, file);
    OE_Show(dlg, IDC_OBJFILE, file);
    OE_Show(dlg, IDC_OBJADDFILE, file);
    OE_Show(dlg, IDC_OBJGFXPREV, tile || file);
    OE_Show(dlg, IDC_OBJTILELAB, tile);
    OE_Show(dlg, IDC_OBJPICKTILE, tile);
    OE_Show(dlg, IDC_OBJPICKREG, file);
    OE_Show(dlg, IDC_OBJXLBL, file);
    OE_Show(dlg, IDC_OBJYLBL, file);
    OE_Show(dlg, IDC_OBJWLBL, file);
    OE_Show(dlg, IDC_OBJHLBL, file);
    OE_ShowPair(dlg, IDC_OBJGXOFF, IDC_OBJGXSPIN, file);
    OE_ShowPair(dlg, IDC_OBJGYOFF, IDC_OBJGYSPIN, file);

    if (area) {
        OE_MoveDlg(dlg, IDC_OBJGW, 50, 66, 40, 14);
        OE_MoveDlg(dlg, IDC_OBJGH, 138, 66, 40, 14);
        OE_RebuddySpin(dlg, IDC_OBJGW, IDC_OBJGWSPIN);
        OE_RebuddySpin(dlg, IDC_OBJGH, IDC_OBJGHSPIN);
    } else if (file) {
        OE_MoveDlg(dlg, IDC_OBJGW, 196, 62, 36, 14);
        OE_MoveDlg(dlg, IDC_OBJGH, 250, 62, 36, 14);
        OE_RebuddySpin(dlg, IDC_OBJGW, IDC_OBJGWSPIN);
        OE_RebuddySpin(dlg, IDC_OBJGH, IDC_OBJGHSPIN);
    }
    OE_ShowPair(dlg, IDC_OBJGW, IDC_OBJGWSPIN, area || file);
    OE_ShowPair(dlg, IDC_OBJGH, IDC_OBJGHSPIN, area || file);
    if (tile) OE_PropsTileLab(dlg);

    EnableWindow(GetDlgItem(dlg, IDC_OBJPICKTILE), numblockgfx > 1);
    EnableWindow(GetDlgItem(dlg, IDC_OBJPICKREG), F14_NumImages() > 0);
    InvalidateRect(dlg, NULL, TRUE);
    OE_InvalidatePrev(dlg);
}

static void OE_DrawGfxPrev(HWND dlg, const DRAWITEMSTRUCT * di) {
    int mode, pitch;
    RECT rc = di->rcItem;
    HDC hdc = di->hDC;
    unsigned char * tb;

    FillRect(hdc, &rc, GetSysColorBrush(COLOR_3DDKSHADOW));
    mode = OE_PropsGetMode(dlg);
    if (mode == OE_MODE_TILE) {
        if (oe_edgfx <= 0 || blockgfxpt == NULL || blockwidth <= 0 || blockheight <= 0)
            return;
        pitch = (blockwidth * 3 + 3) & ~3;
        tb = malloc((size_t) pitch * blockheight);
        if (tb == NULL) return;
        memset(tb, 0x44, (size_t) pitch * blockheight);
        BlockGfxto24(oe_edgfx, tb, 0, 0, pitch);
        OE_Blit24(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            tb, blockwidth, blockheight);
        free(tb);
        return;
    }
    if (mode == OE_MODE_FILE) {
        int fi, iw, ih, sx, sy, sw, sh, x, y;
        unsigned char * ip, * sub;
        fi = (int) SendDlgItemMessage(dlg, IDC_OBJFILE, CB_GETCURSEL, 0, 0);
        if (fi < 0 || F14_NumImages() <= 0) return;
        ip = OE_Image(fi, &iw, &ih);
        if (ip == NULL) return;
        sx = OE_GetDlgInt(dlg, IDC_OBJGXOFF);
        sy = OE_GetDlgInt(dlg, IDC_OBJGYOFF);
        sw = OE_GetDlgInt(dlg, IDC_OBJGW);
        sh = OE_GetDlgInt(dlg, IDC_OBJGH);
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sx >= iw) sx = iw - 1;
        if (sy >= ih) sy = ih - 1;
        if (sw < 1) sw = 1;
        if (sh < 1) sh = 1;
        if (sw > iw - sx) sw = iw - sx;
        if (sh > ih - sy) sh = ih - sy;
        sub = malloc((size_t) sw * sh * 3);
        if (sub == NULL) return;
        for (y = 0; y < sh; y++)
            for (x = 0; x < sw; x++) {
                unsigned char * s = ip + ((sy + y) * iw + (sx + x)) * 3;
                unsigned char * d = sub + (y * sw + x) * 3;
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
            }
        OE_Blit24(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            sub, sw, sh);
        free(sub);
    }
}

/* ----- image-region picker ----- */

static struct {
    int imgidx, x, y, w, h;
    int drag, x0, y0;
    int syncing;
    HWND dlg;
} oereg;
static WNDPROC oereg_old;

static int OE_RegFit(HWND hwnd, int iw, int ih, int * ox, int * oy, int * dw, int * dh) {
    RECT rc;
    int cw, ch;
    GetClientRect(hwnd, &rc);
    cw = rc.right; ch = rc.bottom;
    if (iw < 1 || ih < 1 || cw < 2 || ch < 2) return 0;
    if ((long) cw * ih < (long) ch * iw) {
        *dw = cw;
        *dh = (int) (((long) cw * ih) / iw);
    } else {
        *dh = ch;
        *dw = (int) (((long) ch * iw) / ih);
    }
    if (*dw < 1) *dw = 1;
    if (*dh < 1) *dh = 1;
    *ox = (cw - *dw) / 2;
    *oy = (ch - *dh) / 2;
    return 1;
}

static void OE_RegViewToImg(HWND hwnd, int mx, int my, int iw, int ih, int * ix, int * iy) {
    int ox, oy, dw, dh;
    if (!OE_RegFit(hwnd, iw, ih, &ox, &oy, &dw, &dh)) { *ix = 0; *iy = 0; return; }
    *ix = (int) (((long) (mx - ox) * iw) / dw);
    *iy = (int) (((long) (my - oy) * ih) / dh);
    if (*ix < 0) *ix = 0;
    if (*iy < 0) *iy = 0;
    if (*ix >= iw) *ix = iw - 1;
    if (*iy >= ih) *iy = ih - 1;
}

static void OE_RegNorm(int x0, int y0, int x1, int y1, int * x, int * y, int * w, int * h) {
    int l = x0 < x1 ? x0 : x1, t = y0 < y1 ? y0 : y1;
    int r = x0 < x1 ? x1 : x0, b = y0 < y1 ? y1 : y0;
    *x = l; *y = t;
    *w = r - l + 1;
    *h = b - t + 1;
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
}

static void OE_RegWriteEdits(HWND dlg) {
    oereg.syncing = 1;
    OE_SetDlgInt(dlg, IDC_OBJREGX, oereg.x);
    OE_SetDlgInt(dlg, IDC_OBJREGY, oereg.y);
    OE_SetDlgInt(dlg, IDC_OBJREGW, oereg.w);
    OE_SetDlgInt(dlg, IDC_OBJREGH, oereg.h);
    oereg.syncing = 0;
}

static void OE_RegReadEdits(HWND dlg) {
    oereg.x = OE_GetDlgInt(dlg, IDC_OBJREGX);
    oereg.y = OE_GetDlgInt(dlg, IDC_OBJREGY);
    oereg.w = OE_GetDlgInt(dlg, IDC_OBJREGW);
    oereg.h = OE_GetDlgInt(dlg, IDC_OBJREGH);
    if (oereg.w < 1) oereg.w = 1;
    if (oereg.h < 1) oereg.h = 1;
}

static LRESULT CALLBACK OE_RegViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int iw, ih, ox, oy, dw, dh, mx, my, ix, iy;
    unsigned char * ip;

    switch (msg) {
    case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_3DDKSHADOW));
            ip = OE_Image(oereg.imgidx, &iw, &ih);
            if (ip != NULL && OE_RegFit(hwnd, iw, ih, &ox, &oy, &dw, &dh)) {
                int vx, vy, vw, vh;
                HPEN pen, hold;
                HBRUSH hbrold;
                OE_Blit24(hdc, ox, oy, dw, dh, ip, iw, ih);
                vx = ox + (int) (((long) oereg.x * dw) / iw);
                vy = oy + (int) (((long) oereg.y * dh) / ih);
                vw = (int) (((long) oereg.w * dw) / iw);
                vh = (int) (((long) oereg.h * dh) / ih);
                if (vw < 2) vw = 2;
                if (vh < 2) vh = 2;
                hbrold = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 0));
                hold = SelectObject(hdc, pen);
                Rectangle(hdc, vx, vy, vx + vw, vy + vh);
                SelectObject(hdc, GetStockObject(BLACK_PEN));
                Rectangle(hdc, vx + 2, vy + 2, vx + vw - 2, vy + vh - 2);
                SelectObject(hdc, hold);
                SelectObject(hdc, hbrold);
                DeleteObject(pen);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return 1;
    case WM_LBUTTONDOWN:
        ip = OE_Image(oereg.imgidx, &iw, &ih);
        if (ip == NULL) break;
        mx = (short) LOWORD(lParam); my = (short) HIWORD(lParam);
        OE_RegViewToImg(hwnd, mx, my, iw, ih, &ix, &iy);
        oereg.drag = 1; oereg.x0 = ix; oereg.y0 = iy;
        oereg.x = ix; oereg.y = iy; oereg.w = 1; oereg.h = 1;
        SetCapture(hwnd);
        OE_RegWriteEdits(oereg.dlg);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_MOUSEMOVE:
        if (!oereg.drag) break;
        ip = OE_Image(oereg.imgidx, &iw, &ih);
        if (ip == NULL) break;
        mx = (short) LOWORD(lParam); my = (short) HIWORD(lParam);
        OE_RegViewToImg(hwnd, mx, my, iw, ih, &ix, &iy);
        OE_RegNorm(oereg.x0, oereg.y0, ix, iy, &oereg.x, &oereg.y, &oereg.w, &oereg.h);
        OE_RegWriteEdits(oereg.dlg);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP:
        if (!oereg.drag) break;
        oereg.drag = 0;
        ReleaseCapture();
        return 0;
    }
    return CallWindowProc(oereg_old, hwnd, msg, wParam, lParam);
}

INT_PTR CALLBACK DIAObjRegionProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    HWND view;
    UNUSED(lParam);
    switch (msg) {
    case WM_INITDIALOG: {
            char info[80];
            int iw = 0, ih = 0;
            view = GetDlgItem(hwndDlg, IDC_OBJREGVIEW);
            oereg.dlg = hwndDlg;
            oereg.drag = 0;
            oereg_old = (WNDPROC) SetWindowLongPtr(view, GWLP_WNDPROC, (LONG_PTR) OE_RegViewProc);
            OE_Image(oereg.imgidx, &iw, &ih);
            sprintf(info, "Image %d x %d", iw, ih);
            SetDlgItemText(hwndDlg, IDC_OBJREGINFO, info);
            OE_RegWriteEdits(hwndDlg);
            OE_InitRegSpins(hwndDlg);
            break;
        }
    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE && !oereg.syncing) {
            int id = LOWORD(wParam);
            if (id == IDC_OBJREGX || id == IDC_OBJREGY || id == IDC_OBJREGW || id == IDC_OBJREGH) {
                OE_RegReadEdits(hwndDlg);
                view = GetDlgItem(hwndDlg, IDC_OBJREGVIEW);
                if (view) InvalidateRect(view, NULL, FALSE);
            }
            break;
        }
        switch (LOWORD(wParam)) {
        case IDOK: {
                int iw = 0, ih = 0;
                OE_RegReadEdits(hwndDlg);
                OE_Image(oereg.imgidx, &iw, &ih);
                if (iw > 0 && ih > 0) {
                    if (oereg.x < 0) oereg.x = 0;
                    if (oereg.y < 0) oereg.y = 0;
                    if (oereg.x >= iw) oereg.x = iw - 1;
                    if (oereg.y >= ih) oereg.y = ih - 1;
                    if (oereg.w > iw - oereg.x) oereg.w = iw - oereg.x;
                    if (oereg.h > ih - oereg.y) oereg.h = ih - oereg.y;
                    if (oereg.w < 1) oereg.w = 1;
                    if (oereg.h < 1) oereg.h = 1;
                }
                EndDialog(hwndDlg, 1);
                break;
            }
        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            break;
        }
        break;
    case WM_DESTROY:
        view = GetDlgItem(hwndDlg, IDC_OBJREGVIEW);
        if (view && oereg_old)
            SetWindowLongPtr(view, GWLP_WNDPROC, (LONG_PTR) oereg_old);
        oereg_old = NULL;
        break;
    default:
        return 0;
    }
    return -1;
}

static int OE_PickRegion(HWND hwnd, int imgidx, int * x, int * y, int * w, int * h) {
    memset(&oereg, 0, sizeof(oereg));
    oereg.imgidx = imgidx;
    oereg.x = *x; oereg.y = *y; oereg.w = *w; oereg.h = *h;
    if (oereg.w < 1) oereg.w = blockwidth > 0 ? blockwidth : 16;
    if (oereg.h < 1) oereg.h = blockheight > 0 ? blockheight : 16;
    if (DialogBox(hInstance, (LPCSTR) "D_OBJREGION", hwnd, AsDlgProc(DIAObjRegionProc)) != 1)
        return 0;
    *x = oereg.x; *y = oereg.y; *w = oereg.w; *h = oereg.h;
    return 1;
}

INT_PTR CALLBACK DIAObjectPropsProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    char txt[300];
    static int edidx = 0;
    int i, mode;

    switch (msg) {
    case WM_INITDIALOG:
        edidx = (int) lParam;
        oe_filling = 1;
        LuaRunObjectProp(edidx);
        {
            const F14Obj * o = F14_GetObject(edidx);
            int kind;
            if (o == NULL) { EndDialog(hwndDlg, -1L); break; }
            OE_SetDlgInt(hwndDlg, IDC_OBJX, o->xpos);
            OE_SetDlgInt(hwndDlg, IDC_OBJY, o->ypos);
            oe_edgfx = o->gfxid;
            if (oe_edgfx < 1 && numblockgfx > 1) oe_edgfx = 1;
            if (o->gfxid > 0) kind = IDC_OBJKIND_TILE;
            else if (o->tileid < 0) kind = IDC_OBJKIND_AREA;
            else kind = IDC_OBJKIND_FILE;
            CheckRadioButton(hwndDlg, IDC_OBJKIND_AREA, IDC_OBJKIND_FILE, kind);
            OE_FillFileCombo(hwndDlg, o->tileid);
            OE_SetDlgInt(hwndDlg, IDC_OBJGXOFF, o->gxoff);
            OE_SetDlgInt(hwndDlg, IDC_OBJGYOFF, o->gyoff);
            OE_SetDlgInt(hwndDlg, IDC_OBJGW, o->gwidth);
            OE_SetDlgInt(hwndDlg, IDC_OBJGH, o->gheight);
            OE_SetDlgInt(hwndDlg, IDC_OBJHX, o->ghandlexoff);
            OE_SetDlgInt(hwndDlg, IDC_OBJHY, o->ghandleyoff);
            CheckDlgButton(hwndDlg, IDC_OBJSHOW, o->show ? 1 : 0);
            {
                const int * u = &o->user1;
                int ids[7] = { IDC_OBJUSER1, IDC_OBJUSER2, IDC_OBJUSER3,
                    IDC_OBJUSER4, IDC_OBJUSER5, IDC_OBJUSER6, IDC_OBJUSER7 };
                for (i = 0; i < 7; i++) OE_SetDlgInt(hwndDlg, ids[i], u[i]);
            }
            {
                int ids[8] = { IDC_OBJF1, IDC_OBJF2, IDC_OBJF3, IDC_OBJF4,
                    IDC_OBJF5, IDC_OBJF6, IDC_OBJF7, IDC_OBJF8 };
                for (i = 0; i < 8; i++) {
                    CheckDlgButton(hwndDlg, ids[i], (o->flags >> i) & 1);
                    SetDlgItemText(hwndDlg, ids[i], objlabels[8 + i]);
                }
            }
            SetDlgItemText(hwndDlg, IDC_OBJLBLMAIN, objlabels[0]);
            sprintf(txt, "%06X", OE_BorderCol(edidx));
            SetWindowText(GetDlgItem(hwndDlg, IDC_OBJCOL), txt);
            if (objlayeruser >= 1 && objlayeruser <= 7) {
                const int * u = &o->user1;
                OE_SetDlgInt(hwndDlg, IDC_OBJLAYER, u[objlayeruser - 1]);
            } else SetDlgItemText(hwndDlg, IDC_OBJLAYER, "0");
            sprintf(txt, "Object %d", edidx);
            SetWindowText(hwndDlg, txt);
            OE_FillStrCombo(hwndDlg, o);
            OE_InitPropSpins(hwndDlg);
            OE_PropsShowMode(hwndDlg, OE_PropsGetMode(hwndDlg));
        }
        oe_filling = 0;
        break;
    case WM_DRAWITEM:
        if (wParam == IDC_OBJGFXPREV)
            OE_DrawGfxPrev(hwndDlg, (const DRAWITEMSTRUCT *) lParam);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OBJKIND_AREA:
        case IDC_OBJKIND_TILE:
        case IDC_OBJKIND_FILE:
            if (HIWORD(wParam) != BN_CLICKED) break;
            mode = OE_PropsGetMode(hwndDlg);
            if (mode == OE_MODE_TILE && oe_edgfx < 1 && numblockgfx > 1)
                oe_edgfx = 1;
            if ((mode == OE_MODE_AREA || mode == OE_MODE_FILE) &&
                OE_GetDlgInt(hwndDlg, IDC_OBJGW) <= 0)
                OE_SetDlgInt(hwndDlg, IDC_OBJGW, blockwidth > 0 ? blockwidth : 16);
            if ((mode == OE_MODE_AREA || mode == OE_MODE_FILE) &&
                OE_GetDlgInt(hwndDlg, IDC_OBJGH) <= 0)
                OE_SetDlgInt(hwndDlg, IDC_OBJGH, blockheight > 0 ? blockheight : 16);
            OE_PropsShowMode(hwndDlg, mode);
            break;
        case IDC_OBJFILE:
            if (HIWORD(wParam) == CBN_SELCHANGE) OE_InvalidatePrev(hwndDlg);
            break;
        case IDC_OBJGXOFF:
        case IDC_OBJGYOFF:
        case IDC_OBJGW:
        case IDC_OBJGH:
            if (HIWORD(wParam) == EN_CHANGE && !oe_filling) OE_InvalidatePrev(hwndDlg);
            break;
        case IDC_OBJPICKTILE: {
                int g = DIAPickGraphicIndex(hwndDlg, oe_edgfx);
                if (g < 0) break;
                if (g < 1) {
                    MessageBox(hwndDlg,
                        "Graphic 0 is reserved and cannot be used as an object tile.",
                        "Object", MB_OK | MB_ICONINFORMATION);
                    if (numblockgfx > 1) g = 1;
                    else break;
                }
                oe_edgfx = g;
                OE_PropsTileLab(hwndDlg);
                OE_InvalidatePrev(hwndDlg);
                break;
            }
        case IDC_OBJPICKREG: {
                int fi, x, y, w, h;
                fi = (int) SendDlgItemMessage(hwndDlg, IDC_OBJFILE, CB_GETCURSEL, 0, 0);
                if (fi < 0 || F14_NumImages() <= 0) {
                    MessageBox(hwndDlg, "Add an image file first.", "Object",
                        MB_OK | MB_ICONINFORMATION);
                    break;
                }
                x = OE_GetDlgInt(hwndDlg, IDC_OBJGXOFF);
                y = OE_GetDlgInt(hwndDlg, IDC_OBJGYOFF);
                w = OE_GetDlgInt(hwndDlg, IDC_OBJGW);
                h = OE_GetDlgInt(hwndDlg, IDC_OBJGH);
                if (OE_PickRegion(hwndDlg, fi, &x, &y, &w, &h)) {
                    oe_filling = 1;
                    OE_SetDlgInt(hwndDlg, IDC_OBJGXOFF, x);
                    OE_SetDlgInt(hwndDlg, IDC_OBJGYOFF, y);
                    OE_SetDlgInt(hwndDlg, IDC_OBJGW, w);
                    OE_SetDlgInt(hwndDlg, IDC_OBJGH, h);
                    oe_filling = 0;
                    OE_InvalidatePrev(hwndDlg);
                }
                break;
            }
        case IDC_OBJADDFILE: {
                int keep = (int) SendDlgItemMessage(hwndDlg, IDC_OBJFILE, CB_GETCURSEL, 0, 0);
                DIAObjFname(hwndDlg);
                if (F14_NumImages() > 0 && (keep < 0 || keep >= F14_NumImages()))
                    keep = F14_NumImages() - 1;
                OE_FillFileCombo(hwndDlg, keep);
                OE_PropsShowMode(hwndDlg, OE_PropsGetMode(hwndDlg));
                break;
            }
        case IDC_OBJPREV:
        case IDC_OBJNEXT:
        case IDOK: {
                F14Obj o;
                char tb[300];
                int gsel;
                if (F14_GetObject(edidx) == NULL) {
                    SendMessage(hwndDlg, WM_CLOSE, 0, -1L);
                    break;
                }
                o = *F14_GetObject(edidx);
                o.xpos = OE_GetDlgInt(hwndDlg, IDC_OBJX);
                o.ypos = OE_GetDlgInt(hwndDlg, IDC_OBJY);
                o.gxoff = OE_GetDlgInt(hwndDlg, IDC_OBJGXOFF);
                o.gyoff = OE_GetDlgInt(hwndDlg, IDC_OBJGYOFF);
                o.gwidth = OE_GetDlgInt(hwndDlg, IDC_OBJGW);
                o.gheight = OE_GetDlgInt(hwndDlg, IDC_OBJGH);
                o.ghandlexoff = OE_GetDlgInt(hwndDlg, IDC_OBJHX);
                o.ghandleyoff = OE_GetDlgInt(hwndDlg, IDC_OBJHY);
                o.show = IsDlgButtonChecked(hwndDlg, IDC_OBJSHOW) ? 1 : 0;
                mode = OE_PropsGetMode(hwndDlg);
                if (mode == OE_MODE_AREA) {
                    o.gfxid = 0;
                    o.tileid = -1;
                } else if (mode == OE_MODE_TILE) {
                    o.gfxid = oe_edgfx;
                    if (o.gfxid < 1) o.gfxid = (numblockgfx > 1) ? 1 : 0;
                    o.tileid = -1;
                    o.gwidth = blockwidth;
                    o.gheight = blockheight;
                } else {
                    o.gfxid = 0;
                    gsel = (int) SendDlgItemMessage(hwndDlg, IDC_OBJFILE, CB_GETCURSEL, 0, 0);
                    o.tileid = (F14_NumImages() > 0 && gsel >= 0) ? gsel : 0;
                }
                {
                    int * u = &o.user1;
                    int ids[7] = { IDC_OBJUSER1, IDC_OBJUSER2, IDC_OBJUSER3,
                        IDC_OBJUSER4, IDC_OBJUSER5, IDC_OBJUSER6, IDC_OBJUSER7 };
                    for (i = 0; i < 7; i++) u[i] = OE_GetDlgInt(hwndDlg, ids[i]);
                }
                {
                    int ids[8] = { IDC_OBJF1, IDC_OBJF2, IDC_OBJF3, IDC_OBJF4,
                        IDC_OBJF5, IDC_OBJF6, IDC_OBJF7, IDC_OBJF8 };
                    o.flags &= ~0xFF;
                    for (i = 0; i < 8; i++) {
                        if (IsDlgButtonChecked(hwndDlg, ids[i])) o.flags |= (1 << i);
                    }
                }
                GetWindowText(GetDlgItem(hwndDlg, IDC_OBJCOL), tb, 80);
                OE_SetBorderCol(&o, strtol(tb, NULL, 16));
                if (objlayeruser >= 1 && objlayeruser <= 7) {
                    int * u = &o.user1;
                    u[objlayeruser - 1] = OE_GetDlgInt(hwndDlg, IDC_OBJLAYER);
                }
                OE_ApplyStrCombo(hwndDlg, &o);
                F14_SetObject(edidx, &o);
                mapaltered = 1;
                if (LOWORD(wParam) == IDC_OBJPREV)
                    SendMessage(hwndDlg, WM_CLOSE, 0, 1L);
                else if (LOWORD(wParam) == IDC_OBJNEXT)
                    SendMessage(hwndDlg, WM_CLOSE, 0, 2L);
                else
                    SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
                break;
            }
        case IDCANCEL:
            SendMessage(hwndDlg, WM_CLOSE, 0, -1L);
            break;
        }
        break;
    case WM_CLOSE:
        EndDialog(hwndDlg, lParam);
        break;
    default:
        return 0;
    }
    return -1;
}

void DIAObjectProps(HWND hwnd, int idx) {
    int i;

    i = 1;
    while (i) {
        if (F14_GetObject(idx) == NULL) break;
        i = DialogBoxParam(hInstance, (LPCSTR) "D_OBJECT", hwnd, AsDlgProc(DIAObjectPropsProc), (LPARAM) idx);
        if (i == 1) { if (idx > 0) idx--; else i = 0; }
        else if (i == 2) { if (idx < F14_NumObjects() - 1) idx++; else i = 0; }
        else i = 0; /* OK / Cancel / close — do not reopen */
    }
    return;
}

/* ---------------- object manager ("Objects (%d/%d)") ---------------- */

static void OE_MgrRefresh(HWND hwndDlg) {
    int n, i;
    char txt[64];

    n = F14_NumObjects();
    SendDlgItemMessage(hwndDlg, IDC_OBJLIST, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < n; i++) {
        const F14Obj * o = F14_GetObject(i);
        sprintf(txt, "Object %d%s", i, o != NULL ? OE_Kind(o) : "");
        SendDlgItemMessage(hwndDlg, IDC_OBJLIST, LB_ADDSTRING, 0, (LPARAM) txt);
    }
    SendDlgItemMessage(hwndDlg, IDC_OBJLIST, LB_SETCURSEL, F14_CurObject(), 0);
    sprintf(txt, "Objects (%d/%d)", F14_CurObject(), n);
    SetWindowText(hwndDlg, txt);
}

INT_PTR CALLBACK DIAObjectMgrProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int sel, ni;

    UNUSED(lParam);
    switch (msg) {
    case WM_INITDIALOG:
        OE_MgrRefresh(hwndDlg);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OBJLIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                sel = SendDlgItemMessage(hwndDlg, IDC_OBJLIST, LB_GETCURSEL, 0, 0);
                if (sel >= 0) F14_SetCurObject(sel);
                OE_MgrRefresh(hwndDlg);
            } else if (HIWORD(wParam) == LBN_DBLCLK) {
                sel = SendDlgItemMessage(hwndDlg, IDC_OBJLIST, LB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    F14_SetCurObject(sel);
                    DIAObjectProps(hwndDlg, sel);
                    OE_MgrRefresh(hwndDlg);
                }
            }
            break;
        case IDC_OBJNEW:
            ni = F14_NewObject();
            if (ni < 0) {
                MessageBox(hwndDlg, "Can't create object (max reached).", "Objects", MB_OK|MB_ICONINFORMATION);
            } else {
                F14Obj o;
                memset(&o, 0, sizeof(o));
                o.xpos = (xmapoffset + 2) * blockwidth;
                o.ypos = (ymapoffset + 2) * blockheight;
                o.show = 1;
                o.tileid = -1; /* area only by default */
                o.gwidth = blockwidth; o.gheight = blockheight;
                F14_SetObject(ni, &o);
                F14_SetCurObject(ni);
                mapaltered = 1;
            }
            OE_MgrRefresh(hwndDlg);
            break;
        case IDC_OBJCUT:
        case IDC_OBJCOPY:
            sel = F14_CurObject();
            if (F14_GetObject(sel) != NULL) {
                objclip = *F14_GetObject(sel);
                objcliphas = 1;
                if (LOWORD(wParam) == IDC_OBJCUT) {
                    F14_DeleteObject(sel);
                    mapaltered = 1;
                }
            }
            OE_MgrRefresh(hwndDlg);
            break;
        case IDC_OBJPASTE:
            if (!objcliphas) break;
            ni = F14_NewObject();
            if (ni < 0) {
                MessageBox(hwndDlg, "Can't paste object (max reached).", "Objects", MB_OK|MB_ICONINFORMATION);
            } else {
                objclip.xpos += 16; objclip.ypos += 16;
                F14_SetObject(ni, &objclip);
                F14_SetCurObject(ni);
                mapaltered = 1;
            }
            OE_MgrRefresh(hwndDlg);
            break;
        case IDC_OBJEDIT:
            sel = F14_CurObject();
            if (F14_GetObject(sel) != NULL) DIAObjectProps(hwndDlg, sel);
            OE_MgrRefresh(hwndDlg);
            break;
        case IDC_OBJDEL:
            sel = F14_CurObject();
            if (F14_GetObject(sel) != NULL &&
                MessageBox(hwndDlg, "Delete this object?", "Objects",
                MB_OKCANCEL|MB_ICONQUESTION) == IDOK) {
                F14_DeleteObject(sel);
                mapaltered = 1;
            }
            OE_MgrRefresh(hwndDlg);
            break;
        case IDOK:
        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            break;
        }
        break;
    default:
        return 0;
    }
    return -1;
}

void DIAObjectMgr(HWND hwnd) {

    if (mappt == NULL) {
        MessageBox(hwnd, "Load or create a map first.", "Objects", MB_OK|MB_ICONINFORMATION);
        return;
    }
    DialogBox(hInstance, (LPCSTR) "D_OBJMGR", hwnd, AsDlgProc(DIAObjectMgrProc));
    return;
}

/* ---------------- D_OBJFNAME file list ---------------- */

static void OE_FnameRefresh(HWND hwndDlg) {
    int n, i;

    n = F14_NumImages();
    SendDlgItemMessage(hwndDlg, IDC_OBJFNAMELIST, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < n; i++) {
        SendDlgItemMessage(hwndDlg, IDC_OBJFNAMELIST, LB_ADDSTRING, 0,
            (LPARAM) F14_ImageName(i));
    }
}

INT_PTR CALLBACK DIAObjFnameProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int sel, i;

    UNUSED(lParam);
    switch (msg) {
    case WM_INITDIALOG:
        OE_FnameRefresh(hwndDlg);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OBJFNADD: {
                OPENFILENAME ofn;
                char szFilter[64], szFile[260], szTitle[260];
                memset(szFilter, 0, sizeof(szFilter));
                strcpy(szFilter, "Images (all)|*.*");
                szFilter[14] = 0;
                szFile[0] = 0; szTitle[0] = 0;
                memset(&ofn, 0, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwndDlg;
                ofn.lpstrFilter = szFilter;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFileTitle = szTitle;
                ofn.nMaxFileTitle = sizeof(szTitle);
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                if (GetOpenFileName(&ofn)) {
                    if (F14_AddImage(szFile) < 0)
                        MessageBox(hwndDlg, "Can't add file (list full).", "Object Graphic Files", MB_OK|MB_ICONINFORMATION);
                    else mapaltered = 1;
                    OE_FnameRefresh(hwndDlg);
                }
                break;
            }
        case IDC_OBJFNDEL:
            sel = SendDlgItemMessage(hwndDlg, IDC_OBJFNAMELIST, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                /* re-point objects using files at/after sel */
                for (i = 0; i < F14_NumObjects(); i++) {
                    F14Obj o;
                    if (F14_GetObject(i) == NULL) continue;
                    o = *F14_GetObject(i);
                    if (o.gfxid == 0 && o.tileid == sel) o.tileid = -1;
                    else if (o.gfxid == 0 && o.tileid > sel) o.tileid--;
                    F14_SetObject(i, &o);
                }
                F14_DeleteImage(sel);
                mapaltered = 1;
                OE_FnameRefresh(hwndDlg);
            }
            break;
        case IDOK:
        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            break;
        }
        break;
    default:
        return 0;
    }
    return -1;
}

void DIAObjFname(HWND hwnd) {

    DialogBox(hInstance, (LPCSTR) "D_OBJFNAME", hwnd, AsDlgProc(DIAObjFnameProc));
    return;
}
