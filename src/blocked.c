#define  STRICT
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"
#include "dpi.h"
#include "fmp.h"
#include "fmp14.h"
#include "tileset.h"
#include "luabind.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#define BETYPE_STILL   0
#define BETYPE_ANIM    1
#define BETYPE_GRAPH   2
#define BETYPE_BRUSHES 3
#define BETYPE_OBJECTS 4
#define BETYPE_TILESETS 5
#define BETYPE_COUNT   6

#define IDC_BEMODETAB 1000
#define TAB_H 22

static HWND bhwnd = NULL;
static HWND htab = NULL;
static int be_minpane = 0;
static HFONT be_font = NULL;
static int be_tabh = TAB_H;
short int beyoff = 0, betype = BETYPE_STILL, bebpr = 1;
short int curgfxsel = -1;   /* selected graphic index (Graph mode) */
short int curbrushsel = -1; /* selected brush index (Brushes mode) */
short int curobjsel = -1;   /* selected object index (Objects mode) */
short int curtilesetsel = -1; /* selected tileset index */
static char szBlockEdname[] = "Block Editor";
extern HINSTANCE hInstance;
extern short int blockwidth, blockheight, blockdepth, gridon, zoomon;
extern int panelzoom;
static int BEZoom(void) { return panelzoom ? zoomon : 1; }
extern short int mapwidth, mapheight, xmapoffset, ymapoffset, maplayer;
extern int tsplace, objplace, megetabrush;
extern short int numblockstr, curstr, numblockanim, curanim, curbrsh;
extern char * blockgfxpt, * blockstrpt, * animstrpt;
extern short int numblockgfx;
extern short int * brshpt[MAXBRSH];
extern short int numbrushes;
extern int wlayout;
extern int sortblocks, sortobjects;
extern int mwheelblks, thumbtrack;
extern int mapaltered;

static TilesetDef tsclip;
static short *tscliptiles = NULL;
static int tsclipw, tscliph, tscliphas = 0;

static int BETilesetRowH(HWND hwnd)
{
    int dpi = MapGetDpi(hwnd);
    int h = MapScale(36, dpi);
    int t;
    if (blockheight > 0) {
        t = blockheight + MapScale(6, dpi);
        if (t < h) h = t;
    }
    t = MapScale(22, dpi);
    if (h < t) h = t;
    return h;
}

static int BEIsListType(int t)
{
    return t == BETYPE_TILESETS || t == BETYPE_OBJECTS || t == BETYPE_BRUSHES;
}

static int BEBrushCount(void)
{
    int i, n = 0;
    for (i = 0; i < MAXBRSH; i++)
        if (brshpt[i] != NULL) n++;
    return n;
}

static int BEBrushAt(int disp)
{
    int i, n = 0;
    if (disp < 0)
        return -1;
    for (i = 0; i < MAXBRSH; i++) {
        if (brshpt[i] == NULL)
            continue;
        if (n == disp)
            return i;
        n++;
    }
    return -1;
}

static int BEBrushListPos(int slot)
{
    int i, n = 0;
    if (slot < 0)
        return -1;
    for (i = 0; i < MAXBRSH; i++) {
        if (brshpt[i] == NULL)
            continue;
        if (i == slot)
            return n;
        n++;
    }
    return -1;
}

/* Columns for the tile grid. List modes stay at 1. */
static int BEComputeBpr(HWND hwnd)
{
    RECT rc;
    int k, bpr, zm;

    if (BEIsListType(betype))
        return 1;
    if (hwnd == NULL)
        hwnd = bhwnd;
    if (hwnd == NULL || blockwidth < 1)
        return 1;
    GetClientRect(hwnd, &rc);
    k = rc.right - rc.left;
    zm = BEZoom();
    if (zm < 1) zm = 1;
    if (zm != 1) {
        k /= zm;
        k += (zm - 1);
    }
    if (k < blockwidth)
        k = blockwidth;
    k += 3;
    k &= -4;
    bpr = (k - (24 / zm)) / (blockwidth + 2);
    if (bpr < 1)
        bpr = 1;
    return bpr;
}

static void BESyncBpr(HWND hwnd)
{
    bebpr = (short int) BEComputeBpr(hwnd);
    if (bebpr < 1)
        bebpr = 1;
}

static int BETabStripHeight(void);
static void BEEnsureUiFont(HWND hwnd);
static void BELayoutTabs(HWND parent);
static void BECreateTabControl(HWND parent);
static void BESyncTabSel(void);
static void BESetType(int newtype);
static void BERecalcMinPane(HWND hwnd);

int BEPaneWidth(int parent_client_w)
{
    HWND hwnd = bhwnd;
    int split, minw, room;

    if (hwnd == NULL)
        hwnd = htab;
    split = MapBlockPaneWidth(hwnd, parent_client_w);
    minw = be_minpane;
    if (minw < 1)
        minw = MapDip(hwnd, MAP_DIP_SPLIT);
    if (split < minw)
        split = minw;
    room = parent_client_w - MapDip(hwnd, 160);
    if (room > 0 && split > room)
        split = room;
    return split;
}

void BEResizeWind(int wide, int high) {
    int split;
    if (bhwnd == NULL) return;
    split = BEPaneWidth(wide);
    if (wlayout == 0) MoveWindow(bhwnd, wide - split, 0, split, high, TRUE);
    if (wlayout == 1) MoveWindow(bhwnd, 0, 0, split, high, TRUE);
    if (wlayout == -1) MoveWindow(bhwnd, wide - split, 0, split, high, TRUE);
}

void BEInvalidate(void) {
    InvalidateRgn(bhwnd, NULL, FALSE);
}

void BEUpdateScrollVis(void) {
    if (bhwnd==NULL) return;
    ShowScrollBar(bhwnd, SB_VERT, blockgfxpt!=NULL ? TRUE : FALSE);
}

void BEBlockNext(void) {
    if (betype == BETYPE_STILL && curanim==-1 && curbrsh==0 && curstr<(numblockstr-1)) { curstr++; BEInvalidate(); }
    if (betype == BETYPE_ANIM && curanim!=-1 && curbrsh==0 && curanim<(numblockanim-1)) { curanim++; BEInvalidate(); }
    if (betype == BETYPE_GRAPH && curgfxsel < numblockgfx - 1) { curgfxsel++; BEInvalidate(); }
    if (betype == BETYPE_BRUSHES && curbrushsel < MAXBRSH - 1) {
        int next = curbrushsel + 1;
        while (next < MAXBRSH && brshpt[next] == NULL) next++;
        if (next < MAXBRSH) {
            curbrushsel = (short int) next;
            curbrsh = (short int) (next + 1);
            RejigBrshMenu(curbrsh);
            BEInvalidate();
        }
    }
    if (betype == BETYPE_OBJECTS && curobjsel < F14_NumObjects() - 1) {
        curobjsel++;
        F14_SetCurObject(curobjsel);
        BEInvalidate();
    }
    if (betype == BETYPE_TILESETS && curtilesetsel < TS_NumDefs() - 1) { curtilesetsel++; TS_SetCurDef(curtilesetsel); BEInvalidate(); }
}

void BEBlockPrev(void) {
    if (betype == BETYPE_STILL && curanim==-1 && curbrsh==0 && curstr>0) { curstr--; BEInvalidate(); }
    if (betype == BETYPE_ANIM && curanim!=-1 && curbrsh==0 && curanim>0) { curanim--; BEInvalidate(); }
    if (betype == BETYPE_GRAPH && curgfxsel > 0) { curgfxsel--; BEInvalidate(); }
    if (betype == BETYPE_BRUSHES && curbrushsel > 0) {
        int prev = curbrushsel - 1;
        while (prev >= 0 && brshpt[prev] == NULL) prev--;
        if (prev >= 0) {
            curbrushsel = (short int) prev;
            curbrsh = (short int) (prev + 1);
            RejigBrshMenu(curbrsh);
            BEInvalidate();
        }
    }
    if (betype == BETYPE_OBJECTS && curobjsel > 0) {
        curobjsel--;
        F14_SetCurObject(curobjsel);
        BEInvalidate();
    }
    if (betype == BETYPE_TILESETS && curtilesetsel > 0) { curtilesetsel--; TS_SetCurDef(curtilesetsel); BEInvalidate(); }
}

static int BEItemCount(void) {
    switch (betype) {
    case BETYPE_STILL:   return numblockstr;
    case BETYPE_ANIM:    return numblockanim;
    case BETYPE_GRAPH:   return numblockgfx;
    case BETYPE_BRUSHES: return BEBrushCount();
    case BETYPE_OBJECTS: return F14_NumObjects();
    case BETYPE_TILESETS: return TS_NumDefs();
    }
    return 0;
}

static void BERefreshSort(void) {
    if (betype == BETYPE_STILL && sortblocks) LuaRefreshBlockSort();
    if (betype == BETYPE_OBJECTS && sortobjects) LuaRefreshObjectSort();
}

static int BEVisibleCount(void) {
    if (betype == BETYPE_STILL && sortblocks && LuaBlockSortCount() > 0)
        return LuaBlockSortCount();
    if (betype == BETYPE_OBJECTS && sortobjects && LuaObjectSortCount() > 0)
        return LuaObjectSortCount();
    return BEItemCount();
}

static int BEMapDisp(int disp) {
    int v;
    if (betype == BETYPE_STILL && sortblocks) {
        v = LuaBlockSortAt(disp);
        return v;
    }
    if (betype == BETYPE_OBJECTS && sortobjects) {
        v = LuaObjectSortAt(disp);
        return v;
    }
    return disp;
}

static int BEScrollItems(void) {
    int n = BEVisibleCount();
    if (BEIsListType(betype))
        n += 1; /* "+ Add new" */
    return n;
}

static int BEMaxScrollRow(void) {
    int n = BEScrollItems();
    if (bebpr < 1) bebpr = 1;
    if (n < 1) return 0;
    return (n - 1) / bebpr;
}

void BESetScrollRange(void) {
    int total;
    BERefreshSort();
    total = BEVisibleCount();
    const char * title = "Block Editor";
    switch (betype) {
    case BETYPE_STILL:   title = "Still Blocks"; break;
    case BETYPE_ANIM:    title = "Anim Blocks"; break;
    case BETYPE_GRAPH:   title = "Graphic Blocks"; break;
    case BETYPE_BRUSHES: title = "Brushes"; break;
    case BETYPE_OBJECTS: title = "Objects"; break;
    case BETYPE_TILESETS: title = "Tilesets"; break;
    }
    if (total > 0 && beyoff >= 0) {
        char buf[64];
        int sel = -1;
        switch (betype) {
        case BETYPE_STILL:   sel = (curanim == -1 && curbrsh == 0) ? curstr : -1; break;
        case BETYPE_ANIM:    sel = (curanim >= 0) ? curanim : -1; break;
        case BETYPE_GRAPH:   sel = curgfxsel; break;
        case BETYPE_BRUSHES: sel = BEBrushListPos(curbrushsel); break;
        case BETYPE_OBJECTS: sel = curobjsel; break;
        case BETYPE_TILESETS: sel = curtilesetsel; break;
        }
        if (sel >= 0)
            sprintf(buf, "%s (%d/%d)", title, sel + 1, total);
        else
            sprintf(buf, "%s", title);
        SetWindowText(bhwnd, buf);
    } else {
        SetWindowText(bhwnd, title);
    }
    BESyncBpr(bhwnd);
    {
        int rows = BEScrollItems();
        SetScrollRange(bhwnd, SB_VERT, 0, (rows > 0) ? ((rows - 1) / bebpr) : 0, FALSE);
        if (beyoff > ((rows - 1) / bebpr)) beyoff = (rows - 1) / bebpr;
    }
    if (beyoff < 0) beyoff = 0;
    SetScrollPos(bhwnd, SB_VERT, beyoff, TRUE);
}

static void BETilesetCopySel(void)
{
    const TilesetDef *d = TS_GetDef(curtilesetsel);
    int n;

    free(tscliptiles);
    tscliptiles = NULL;
    tscliphas = 0;
    if (d == NULL || d->tiles == NULL)
        return;
    tsclip = *d;
    tsclip.tiles = NULL;
    tsclipw = d->width;
    tscliph = d->height;
    n = d->width * d->height;
    tscliptiles = (short *)malloc((size_t)n * sizeof(short));
    if (tscliptiles == NULL)
        return;
    memcpy(tscliptiles, d->tiles, (size_t)n * sizeof(short));
    tscliphas = 1;
}

static void BETilesetPasteClip(void)
{
    int ni, x, y;
    if (!tscliphas || tscliptiles == NULL)
        return;
    ni = TS_NewDef(tsclipw, tscliph);
    if (ni < 0)
        return;
    TS_SetDefMeta(ni, tsclip.name, tsclip.handle_x, tsclip.handle_y);
    for (y = 0; y < tscliph; y++) {
        for (x = 0; x < tsclipw; x++)
            TS_SetCell(ni, x, y, tscliptiles[y * tsclipw + x]);
    }
    curtilesetsel = (short int)ni;
    TS_SetCurDef(ni);
    mapaltered = 1;
}

static int BEAddTileset(HWND hwnd)
{
    int ni = TS_NewDef(4, 4);
    if (ni < 0)
        return -1;
    curtilesetsel = (short int) ni;
    TS_SetCurDef(ni);
    mapaltered = 1;
    if (!tsplace)
        DIATilesetEdit(hwnd, ni);
    return ni;
}

static int BEAddObject(HWND hwnd)
{
    int ni = OE_NewDefault();
    if (ni < 0)
        return -1;
    curobjsel = (short int) ni;
    if (!objplace)
        DIAObjectProps(hwnd, ni);
    return ni;
}

static int BEAddBrush(HWND hwnd)
{
    int i = FindFreeBrush();
    if (i == 0) {
        MessageBox(hwnd, "All 16 brush slots are used. Destroy a brush first.",
            "Add brush", MB_OK | MB_ICONINFORMATION);
        return -1;
    }
    curbrsh = (short int) i;
    curbrushsel = (short int) (i - 1);
    curanim = -1;
    megetabrush = 1;
    RejigBrshMenu(curbrsh);
    return i;
}

void BENewBlock(void) {
    if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0 && numblockstr < MAXBLKSTR) NewBlock();
    if (betype == BETYPE_ANIM && curanim != -1) AnimNew();
    else if (betype == BETYPE_ANIM && animstrpt == NULL) AnimNew();
    if (betype == BETYPE_TILESETS)
        BEAddTileset(bhwnd);
    if (betype == BETYPE_OBJECTS)
        BEAddObject(bhwnd);
    if (betype == BETYPE_BRUSHES)
        BEAddBrush(bhwnd);
    BESetScrollRange();
    BEInvalidate();
}

void BECutBlock(void) {
    if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0) CutBlock();
    if (betype == BETYPE_ANIM && curanim != -1) AnimCut();
    if (betype == BETYPE_TILESETS && TS_GetDef(curtilesetsel) != NULL) {
        BETilesetCopySel();
        TS_DeleteDef(curtilesetsel);
        curtilesetsel = (short int)TS_CurDef();
        mapaltered = 1;
    }
    BESetScrollRange();
}

void BECopyBlock(void) {
    if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0) CopyBlock();
    if (betype == BETYPE_ANIM && curanim != -1) AnimCopy();
    if (betype == BETYPE_TILESETS)
        BETilesetCopySel();
}

void BEPasteBlock(void) {
    if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0 && numblockstr < MAXBLKSTR) PasteBlock();
    if (betype == BETYPE_ANIM && curanim != -1) AnimPaste();
    if (betype == BETYPE_TILESETS)
        BETilesetPasteClip();
    BESetScrollRange();
}

static int BEIsSelected(int idx) {
    switch (betype) {
    case BETYPE_STILL:   return (curanim == -1 && curbrsh == 0 && idx == curstr);
    case BETYPE_ANIM:    return (curanim >= 0 && idx == curanim);
    case BETYPE_GRAPH:   return (idx == curgfxsel);
    case BETYPE_BRUSHES: return (idx == curbrushsel);
    case BETYPE_OBJECTS: return (idx == curobjsel);
    case BETYPE_TILESETS: return (idx == curtilesetsel);
    }
    return 0;
}

void BEDrawBlockGrid(unsigned char * dest, int xoff, int yoff, int linewidth, int isatt) {
    int i;

    if (gridon == 1 && isatt == 0) return;
    dest += (xoff*3);
    dest += (yoff*linewidth);
    for (i=0;i<blockwidth;i++) {
        if (isatt) {
            dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 255;
        } else {
            dest[i*3] = 255; dest[i*3+1] = 255; dest[i*3+2] = 255;
        }
    }

    for (i=0;i<(blockheight-1);i++) {
        if (isatt) {
            dest[0] = 0; dest[1] = 0; dest[2] = 255;
            dest[(blockwidth-1)*3] = 0; dest[(blockwidth-1)*3+1] = 0; dest[(blockwidth-1)*3+2] = 255;
        } else {
            dest[0] = 255; dest[1] = 255; dest[2] = 255;
            dest[(blockwidth-1)*3] = 0; dest[(blockwidth-1)*3+1] = 0; dest[(blockwidth-1)*3+2] = 0;
        }
        dest += linewidth;
    }

    for (i=0;i<blockwidth;i++) {
        if (isatt) {
            dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 255;
        } else {
            dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 0;
        }
    }
}

void BEDrawBlockSelect(unsigned char * dest, int xoff, int yoff, int linewidth) {
    int i;

    dest += (xoff*3);
    dest += (yoff*linewidth);
    for (i=0;i<blockwidth;i++) {
        if ((i%4)<2) { dest[i*3] = 255; dest[i*3+1] = 255; dest[i*3+2] = 255; }
        else { dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 0; }
    }

    for (i=0;i<(blockheight-1);i++) {
        if ((i%4)<2) {
            dest[0] = 255; dest[1] = 255; dest[2] = 255;
            dest[(blockwidth-1)*3] = 255; dest[(blockwidth-1)*3+1] = 255; dest[(blockwidth-1)*3+2] = 255;
        } else {
            dest[0] = 0; dest[1] = 0; dest[2] = 0;
            dest[(blockwidth-1)*3] = 0; dest[(blockwidth-1)*3+1] = 0; dest[(blockwidth-1)*3+2] = 0;
        }
        dest += linewidth;
    }

    for (i=0;i<blockwidth;i++) {
        if ((i%4)<2) { dest[i*3] = 255; dest[i*3+1] = 255; dest[i*3+2] = 255; }
        else { dest[i*3] = 0; dest[i*3+1] = 0; dest[i*3+2] = 0; }
    }
}

static void BEDrawBrushColor(unsigned char * dest, int xoff, int yoff, int linewidth, int idx) {
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

static void BEDrawObjectThumb(unsigned char * dest, int xoff, int yoff, int linewidth, int objidx) {
    const F14Obj * o = F14_GetObject(objidx);
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
        sprintf(txt, "%d", objidx);
        slen = strlen(txt);
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

static void BEBlitCell(HDC hdc, int dx, int dy, int dw, int dh, short cell)
{
    unsigned char *buf;
    HDC mdc;
    HBITMAP hbm, hold;
    BITMAPINFOHEADER bih;
    int pitch, ani;

    if (dw < 1 || dh < 1 || blockwidth < 1 || blockheight < 1)
        return;
    pitch = (blockwidth * 3 + 3) & ~3;
    buf = (unsigned char *)malloc((size_t)pitch * (size_t)blockheight);
    if (buf == NULL)
        return;
    memset(buf, 0x44, (size_t)pitch * (size_t)blockheight);
    if (cell < 0) {
        ani = (-(int)cell / FMP_ANI_MEMUNIT) - 1;
        if (ani >= 0)
            BlockAnimto24(ani, -1, buf, 0, 0, pitch);
    } else {
        BlockStrto24(cell, buf, 0, 0, pitch);
    }
    memset(&bih, 0, sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biWidth = blockwidth;
    bih.biHeight = -blockheight;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    mdc = CreateCompatibleDC(hdc);
    if (mdc == NULL) { free(buf); return; }
    hbm = CreateDIBitmap(hdc, &bih, CBM_INIT, buf, (BITMAPINFO *)&bih, DIB_RGB_COLORS);
    free(buf);
    if (hbm == NULL) { DeleteDC(mdc); return; }
    hold = SelectObject(mdc, hbm);
    StretchBlt(hdc, dx, dy, dw, dh, mdc, 0, 0, blockwidth, blockheight, SRCCOPY);
    SelectObject(mdc, hold);
    DeleteObject(hbm);
    DeleteDC(mdc);
}

static void BEDrawAddNewRow(HDC hdc, RECT * row, int pad, int thumb)
{
    RECT box, bar;
    HBRUSH line, plusbg;
    HPEN pen, oldp;
    HBRUSH oldb;
    const char * txt = "Add new";
    int cx, cy, a;

    line = CreateSolidBrush(RGB(0x33, 0x33, 0x33));
    bar = *row;
    bar.top = row->bottom - 1;
    FillRect(hdc, &bar, line);
    DeleteObject(line);

    plusbg = CreateSolidBrush(RGB(0x55, 0x55, 0x55));
    box.left = row->left + pad;
    box.top = row->top + pad;
    box.right = box.left + thumb;
    box.bottom = box.top + thumb;
    FillRect(hdc, &box, plusbg);
    DeleteObject(plusbg);

    pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    oldp = SelectObject(hdc, pen);
    oldb = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, box.left, box.top, box.right, box.bottom);
    cx = (box.left + box.right) / 2;
    cy = (box.top + box.bottom) / 2;
    a = thumb / 4;
    if (a < 3) a = 3;
    MoveToEx(hdc, cx - a, cy, NULL);
    LineTo(hdc, cx + a + 1, cy);
    MoveToEx(hdc, cx, cy - a, NULL);
    LineTo(hdc, cx, cy + a + 1);
    SelectObject(hdc, oldp);
    SelectObject(hdc, oldb);
    DeleteObject(pen);

    SetTextColor(hdc, RGB(200, 200, 200));
    TextOut(hdc, row->left + pad + thumb + pad, row->top + pad, txt, (int)strlen(txt));
    SetTextColor(hdc, RGB(255, 255, 255));
}

static void BEDrawTilesetList(HWND phwnd, HDC myhdc)
{
    RECT rc, row;
    int tabht = BETabStripHeight();
    int rowh, i, n, thumb, pad;
    HBRUSH bg, sel, line;
    HFONT oldf = NULL;

    GetClientRect(phwnd, &rc);
    rc.top = tabht;
    bg = CreateSolidBrush(RGB(0x44, 0x44, 0x44));
    FillRect(myhdc, &rc, bg);
    DeleteObject(bg);
    bebpr = 1;
    rowh = BETilesetRowH(phwnd);
    pad = MapDip(phwnd, 4);
    thumb = rowh - pad * 2;
    if (thumb < 8) thumb = 8;
    n = TS_NumDefs();
    sel = CreateSolidBrush(RGB(0x66, 0x66, 0x88));
    line = CreateSolidBrush(RGB(0x33, 0x33, 0x33));
    SetBkMode(myhdc, TRANSPARENT);
    SetTextColor(myhdc, RGB(255, 255, 255));
    if (be_font != NULL)
        oldf = SelectObject(myhdc, be_font);
    for (i = 0; (tabht + (i + 1) * rowh) <= rc.bottom + rowh; i++) {
        const TilesetDef *d;
        int idx = beyoff + i;
        char txt[96];
        RECT bar;
        if (idx > n)
            break;
        row.left = rc.left;
        row.right = rc.right;
        row.top = tabht + i * rowh;
        row.bottom = row.top + rowh;
        if (idx == n) {
            BEDrawAddNewRow(myhdc, &row, pad, thumb);
            break;
        }
        if (idx == curtilesetsel)
            FillRect(myhdc, &row, sel);
        bar = row;
        bar.top = row.bottom - 1;
        FillRect(myhdc, &bar, line);
        d = TS_GetDef(idx);
        if (d != NULL && d->tiles != NULL)
            BEBlitCell(myhdc, row.left + pad, row.top + pad, thumb, thumb, d->tiles[0]);
        if (d != NULL)
            sprintf(txt, "%s  (%dx%d)", d->name[0] ? d->name : "Tileset", d->width, d->height);
        else
            sprintf(txt, "Tileset %d", idx);
        TextOut(myhdc, row.left + pad + thumb + pad, row.top + pad, txt, (int)strlen(txt));
    }
    if (oldf != NULL)
        SelectObject(myhdc, oldf);
    DeleteObject(sel);
    DeleteObject(line);
}

static void BEDrawObjectList(HWND phwnd, HDC myhdc)
{
    RECT rc, row;
    int tabht = BETabStripHeight();
    int rowh, i, thumb, pad, vis;
    HBRUSH bg, sel, line;
    HFONT oldf = NULL;

    GetClientRect(phwnd, &rc);
    rc.top = tabht;
    bg = CreateSolidBrush(RGB(0x44, 0x44, 0x44));
    FillRect(myhdc, &rc, bg);
    DeleteObject(bg);
    bebpr = 1;
    BERefreshSort();
    rowh = BETilesetRowH(phwnd);
    pad = MapDip(phwnd, 4);
    thumb = rowh - pad * 2;
    if (thumb < 8) thumb = 8;
    vis = BEVisibleCount();
    sel = CreateSolidBrush(RGB(0x66, 0x66, 0x88));
    line = CreateSolidBrush(RGB(0x33, 0x33, 0x33));
    SetBkMode(myhdc, TRANSPARENT);
    SetTextColor(myhdc, RGB(255, 255, 255));
    if (be_font != NULL)
        oldf = SelectObject(myhdc, be_font);
    for (i = 0; (tabht + (i + 1) * rowh) <= rc.bottom + rowh; i++) {
        const F14Obj *o;
        int slot = beyoff + i;
        int idx;
        char txt[96];
        RECT bar, box;
        COLORREF fill;
        if (slot > vis)
            break;
        row.left = rc.left;
        row.right = rc.right;
        row.top = tabht + i * rowh;
        row.bottom = row.top + rowh;
        if (slot == vis) {
            BEDrawAddNewRow(myhdc, &row, pad, thumb);
            break;
        }
        idx = BEMapDisp(slot);
        if (idx < 0)
            continue;
        if (idx == curobjsel)
            FillRect(myhdc, &row, sel);
        bar = row;
        bar.top = row.bottom - 1;
        FillRect(myhdc, &bar, line);
        o = F14_GetObject(idx);
        box.left = row.left + pad;
        box.top = row.top + pad;
        box.right = box.left + thumb;
        box.bottom = box.top + thumb;
        if (o != NULL && o->gwidth == 0 && o->gheight == 0 && o->gfxid == 0 && o->tileid < 0)
            fill = RGB(0x88, 0x88, 0x88);
        else
            fill = RGB(0x88, 0x88, 0xCC);
        {
            HBRUSH tb = CreateSolidBrush(fill);
            FillRect(myhdc, &box, tb);
            DeleteObject(tb);
        }
        if (o != NULL)
            OE_DrawThumb(myhdc, box.left, box.top, thumb, thumb, idx);
        OE_Describe(idx, txt, (int) sizeof(txt));
        TextOut(myhdc, row.left + pad + thumb + pad, row.top + pad, txt, (int)strlen(txt));
    }
    if (oldf != NULL)
        SelectObject(myhdc, oldf);
    DeleteObject(sel);
    DeleteObject(line);
}

static void BEBrushLabel(int slot, char * txt, int n)
{
    const char * nm;
    int bw, bh;
    if (txt == NULL || n < 1)
        return;
    if (slot < 0 || slot >= MAXBRSH || brshpt[slot] == NULL) {
        snprintf(txt, (size_t) n, "Brush %d", slot + 1);
        return;
    }
    bw = brshpt[slot][0];
    bh = brshpt[slot][1];
    nm = GetBrushName(slot);
    if (nm != NULL && nm[0] && strcmp(nm, "Brush") != 0)
        snprintf(txt, (size_t) n, "%s  (%dx%d)", nm, bw, bh);
    else
        snprintf(txt, (size_t) n, "Brush %d  (%dx%d)", slot + 1, bw, bh);
}

static void BEBlitBrushThumb(HDC hdc, int dx, int dy, int dw, int dh, int slot)
{
    short * br;
    int bw, bh, x, y, tw, th, pitch, ox, oy, sw, sh;
    unsigned char * buf;
    HDC mdc;
    HBITMAP hbm, hold;
    BITMAPINFOHEADER bih;

    if (hdc == NULL || dw < 1 || dh < 1 || blockwidth < 1 || blockheight < 1)
        return;
    if (slot < 0 || slot >= MAXBRSH || brshpt[slot] == NULL)
        return;
    br = brshpt[slot];
    bw = br[0];
    bh = br[1];
    if (bw < 1 || bh < 1)
        return;
    tw = bw * blockwidth;
    th = bh * blockheight;
    if (tw < 1 || th < 1 || tw > 512 || th > 512 ||
        (size_t) tw * (size_t) th > 262144) {
        BEBlitCell(hdc, dx, dy, dw, dh, br[2]);
        return;
    }
    pitch = (tw * 3 + 3) & ~3;
    buf = (unsigned char *) malloc((size_t) pitch * (size_t) th);
    if (buf == NULL) {
        BEBlitCell(hdc, dx, dy, dw, dh, br[2]);
        return;
    }
    memset(buf, 0x44, (size_t) pitch * (size_t) th);
    for (y = 0; y < bh; y++) {
        for (x = 0; x < bw; x++) {
            short cell = br[2 + y * bw + x];
            if (cell < 0) {
                int ani = (-(int) cell / FMP_ANI_MEMUNIT) - 1;
                if (ani >= 0)
                    BlockAnimto24(ani, -1, buf, x * blockwidth, y * blockheight, pitch);
            } else {
                BlockStrto24(cell, buf, x * blockwidth, y * blockheight, pitch);
            }
        }
    }
    if (tw * dh > th * dw) {
        sw = dw;
        sh = (th * dw) / tw;
    } else {
        sh = dh;
        sw = (tw * dh) / th;
    }
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;
    ox = (dw - sw) / 2;
    oy = (dh - sh) / 2;
    memset(&bih, 0, sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biWidth = tw;
    bih.biHeight = -th;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    mdc = CreateCompatibleDC(hdc);
    if (mdc == NULL) { free(buf); return; }
    hbm = CreateDIBitmap(hdc, &bih, CBM_INIT, buf, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
    free(buf);
    if (hbm == NULL) { DeleteDC(mdc); return; }
    hold = SelectObject(mdc, hbm);
    StretchBlt(hdc, dx + ox, dy + oy, sw, sh, mdc, 0, 0, tw, th, SRCCOPY);
    SelectObject(mdc, hold);
    DeleteObject(hbm);
    DeleteDC(mdc);
}

static void BEDrawBrushList(HWND phwnd, HDC myhdc)
{
    RECT rc, row;
    int tabht = BETabStripHeight();
    int rowh, i, vis, thumb, pad;
    HBRUSH bg, sel, line;
    HFONT oldf = NULL;

    GetClientRect(phwnd, &rc);
    rc.top = tabht;
    bg = CreateSolidBrush(RGB(0x44, 0x44, 0x44));
    FillRect(myhdc, &rc, bg);
    DeleteObject(bg);
    bebpr = 1;
    rowh = BETilesetRowH(phwnd);
    pad = MapDip(phwnd, 4);
    thumb = rowh - pad * 2;
    if (thumb < 8) thumb = 8;
    vis = BEBrushCount();
    sel = CreateSolidBrush(RGB(0x66, 0x66, 0x88));
    line = CreateSolidBrush(RGB(0x33, 0x33, 0x33));
    SetBkMode(myhdc, TRANSPARENT);
    SetTextColor(myhdc, RGB(255, 255, 255));
    if (be_font != NULL)
        oldf = SelectObject(myhdc, be_font);
    for (i = 0; (tabht + (i + 1) * rowh) <= rc.bottom + rowh; i++) {
        int slot = BEBrushAt(beyoff + i);
        int disp = beyoff + i;
        char txt[96];
        RECT bar, box;
        if (disp > vis)
            break;
        row.left = rc.left;
        row.right = rc.right;
        row.top = tabht + i * rowh;
        row.bottom = row.top + rowh;
        if (disp == vis) {
            BEDrawAddNewRow(myhdc, &row, pad, thumb);
            break;
        }
        if (slot < 0)
            continue;
        if (slot == curbrushsel)
            FillRect(myhdc, &row, sel);
        bar = row;
        bar.top = row.bottom - 1;
        FillRect(myhdc, &bar, line);
        box.left = row.left + pad;
        box.top = row.top + pad;
        box.right = box.left + thumb;
        box.bottom = box.top + thumb;
        {
            HBRUSH tb = CreateSolidBrush(RGB(0x55, 0x55, 0x55));
            FillRect(myhdc, &box, tb);
            DeleteObject(tb);
        }
        BEBlitBrushThumb(myhdc, box.left, box.top, thumb, thumb, slot);
        BEBrushLabel(slot, txt, (int) sizeof(txt));
        TextOut(myhdc, row.left + pad + thumb + pad, row.top + pad, txt, (int)strlen(txt));
    }
    if (oldf != NULL)
        SelectObject(myhdc, oldf);
    DeleteObject(sel);
    DeleteObject(line);
}

void BEDrawBlocks(HWND phwnd, HDC myhdc) {
    int i, j, k, l;
    HDC fromhdc;
    HBITMAP temphbm, hbmpold;
    unsigned char * destmempt;
    BITMAPINFOHEADER bih;
    RGBQUAD rgbq;
    RECT rc;
    int tabht = BETabStripHeight();

    GetClientRect(phwnd, &rc);
    k = (rc.right)-(rc.left);
    if (BEZoom()!=1) { k /= BEZoom(); k+= (BEZoom()-1); }
    if (k<blockwidth) k = blockwidth;
    k += 3; k &= -4;
    l = ((rc.bottom)-(rc.top) - tabht)/(blockheight+2);
    if (BEZoom()!=1) { l /= BEZoom(); l+= (BEZoom()-1); }
    if (l < 1) l = 1;

    destmempt = malloc(k*l*(blockheight+2)*3);
    if (destmempt==NULL) return;
    memset(destmempt, 0x44, k*l*(blockheight+2)*3);

    fromhdc = CreateCompatibleDC(myhdc);
    if (fromhdc==NULL) { free(destmempt); return; }

    memset(&bih, 0, sizeof(BITMAPINFOHEADER));
    memset(&rgbq, 0, sizeof(RGBQUAD));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = k;
    bih.biHeight = -(l*(blockheight+2));
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    BESyncBpr(phwnd);
    {
        int maxrow = BEMaxScrollRow();
        if (beyoff > maxrow) beyoff = (short int) maxrow;
        if (beyoff < 0) beyoff = 0;
    }

    switch (betype) {
    case BETYPE_STILL:
        BERefreshSort();
        for (i=0;i<l;i++) {
            for (j=0;j<bebpr;j++) {
                int slot = ((beyoff+i)*bebpr)+j;
                int idx = BEMapDisp(slot);
                if (idx < 0) continue;
                BlockStrto24(idx, destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
                if (gridon && idx<numblockstr) BEDrawBlockGrid(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, IsBlockAttached(idx));
                if (BEIsSelected(idx)) BEDrawBlockSelect(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
            }
        }
        break;
    case BETYPE_ANIM:
        for (i=0;i<l;i++) {
            for (j=0;j<bebpr;j++) {
                int idx = ((beyoff+i)*bebpr)+j;
                BlockAnimto24(idx, -1, destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
                if (gridon && idx<numblockanim) BEDrawBlockGrid(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
                if (BEIsSelected(idx)) BEDrawBlockSelect(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
            }
        }
        break;
    case BETYPE_GRAPH:
        for (i=0;i<l;i++) {
            for (j=0;j<bebpr;j++) {
                int idx = ((beyoff+i)*bebpr)+j;
                if (idx < numblockgfx)
                    BlockGfxto24(idx, destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
                if (gridon && idx<numblockgfx) BEDrawBlockGrid(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
                if (BEIsSelected(idx)) BEDrawBlockSelect(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
            }
        }
        break;
    case BETYPE_BRUSHES:
        for (i=0;i<l;i++) {
            for (j=0;j<bebpr;j++) {
                int idx = ((beyoff+i)*bebpr)+j;
                if (idx < MAXBRSH && brshpt[idx] != NULL) {
                    BEDrawBrushColor(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, idx);
                }
                if (gridon && idx<MAXBRSH && brshpt[idx]!=NULL) BEDrawBlockGrid(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
                if (BEIsSelected(idx)) BEDrawBlockSelect(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
            }
        }
        break;
    case BETYPE_OBJECTS:
        BERefreshSort();
        for (i=0;i<l;i++) {
            for (j=0;j<bebpr;j++) {
                int slot = ((beyoff+i)*bebpr)+j;
                int idx = BEMapDisp(slot);
                if (idx < 0) continue;
                if (idx < F14_NumObjects())
                    BEDrawObjectThumb(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, idx);
                if (gridon && idx<F14_NumObjects()) BEDrawBlockGrid(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3, 0);
                if (BEIsSelected(idx)) BEDrawBlockSelect(destmempt, j*(blockwidth+2), i*(blockheight+2), k*3);
            }
        }
        break;
    }

    temphbm = CreateDIBitmap(myhdc, &bih, CBM_INIT,
        destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
    if (temphbm==NULL) { DeleteDC(fromhdc); free(destmempt); return; }

    hbmpold = SelectObject(fromhdc, temphbm);
    if (BEZoom()==1)
        BitBlt(myhdc, 0, tabht, k, l*(blockheight+2), fromhdc, 0, 0, SRCCOPY);
    else
        StretchBlt(myhdc, 0, tabht, k*BEZoom(), l*(blockheight+2)*BEZoom(), fromhdc, 0, 0, k, l*(blockheight+2), SRCCOPY);
    SelectObject(fromhdc, hbmpold);
    DeleteObject(temphbm);

    DeleteDC(fromhdc);
    free(destmempt);
}

static int BEGfxFromStill(void) {
    int unit, gfx;
    long int bgoff;
    if (blockstrpt == NULL || curstr < 0 || curstr >= numblockstr)
        return 0;
    unit = blockwidth * blockheight * ((blockdepth + 1) / 8);
    if (unit < 1) return 0;
    bgoff = *((long int *) (blockstrpt + ((int) curstr) * 32));
    gfx = (int) (bgoff / unit);
    if (gfx < 0) gfx = 0;
    if (numblockgfx > 0 && gfx >= numblockgfx) gfx = numblockgfx - 1;
    return gfx;
}

static void BESetType(int newtype) {
    if (newtype < 0 || newtype >= BETYPE_COUNT) return;
    if (newtype == betype) return;
    betype = newtype;
    beyoff = 0;
    BESyncBpr(bhwnd);
    if (newtype == BETYPE_GRAPH) {
        curgfxsel = (short int) BEGfxFromStill();
        if (curgfxsel < 0) curgfxsel = 0;
        if (numblockgfx > 0 && curgfxsel >= numblockgfx)
            curgfxsel = numblockgfx - 1;
        beyoff = 0;
    }
    if (newtype == BETYPE_TILESETS) {
        curtilesetsel = (short int)TS_CurDef();
        if (curtilesetsel < 0 && TS_NumDefs() > 0) curtilesetsel = 0;
    }
    if (newtype == BETYPE_OBJECTS) {
        curobjsel = (short int) F14_CurObject();
        if (F14_GetObject(curobjsel) == NULL) curobjsel = -1;
    }
    if (newtype == BETYPE_BRUSHES) {
        if (curbrsh >= 1 && curbrsh <= MAXBRSH && brshpt[curbrsh - 1] != NULL)
            curbrushsel = (short int) (curbrsh - 1);
        else
            curbrushsel = -1;
    }
    BESyncTabSel();
    BESetScrollRange();
    InvalidateRgn(bhwnd, NULL, FALSE);
    GEShowMode(newtype == BETYPE_GRAPH);
    InvalidateAll();
}

void BESetWindowType(int newtype) {
    BESetType(newtype);
}

LRESULT CALLBACK BlockWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    short int i;
    HDC myhdc;
    PAINTSTRUCT ps;

    switch (message) {
    case WM_PAINT:
        myhdc = BeginPaint(hwnd, &ps);
        if (blockgfxpt!=NULL) {
            if (betype == BETYPE_TILESETS)
                BEDrawTilesetList(hwnd, myhdc);
            else if (betype == BETYPE_OBJECTS)
                BEDrawObjectList(hwnd, myhdc);
            else if (betype == BETYPE_BRUSHES)
                BEDrawBrushList(hwnd, myhdc);
            else
                BEDrawBlocks(hwnd, myhdc);
            BESetScrollRange();
        }
        EndPaint(hwnd, &ps);
        break;
    case WM_NOTIFY:
        if (lParam != 0 && ((NMHDR *) lParam)->idFrom == IDC_BEMODETAB &&
            ((NMHDR *) lParam)->code == TCN_SELCHANGE) {
            int sel = (int) SendMessage(htab, TCM_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < BETYPE_COUNT)
                BESetType(sel);
        }
        break;
    case WM_SIZE:
        BELayoutTabs(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case WM_DPICHANGED:
        BEEnsureUiFont(hwnd);
        BERecalcMinPane(hwnd);
        BELayoutTabs(hwnd);
        break;
    case WM_VSCROLL:
        BERefreshSort();
        i = (short int) BEMaxScrollRow();
        switch (LOWORD(wParam)) {
        case SB_TOP:
            beyoff = 0;
            break;
        case SB_LINEUP:
            if (beyoff>0) beyoff--;
            break;
        case SB_LINEDOWN:
            if (beyoff<i) beyoff++;
            break;
        case SB_PAGEUP:
            beyoff-=10;
            if (beyoff<0) beyoff = 0;
            break;
        case SB_PAGEDOWN:
            if (beyoff<(i-1)) beyoff+=10;
            if (beyoff>i) beyoff=i;
            break;
        case SB_BOTTOM:
            beyoff = i;
            if (beyoff<0) beyoff = 0;
            break;
        case SB_THUMBPOSITION:
            beyoff = (HIWORD(wParam));
            break;
        case SB_THUMBTRACK:
            if (thumbtrack) beyoff = (HIWORD(wParam));
            break;
        }
        SetScrollPos(bhwnd, SB_VERT, beyoff, TRUE);
        InvalidateRgn(bhwnd, NULL, FALSE);
        break;
    case WM_LBUTTONUP:
        if (!BEIsListType(betype) &&
            LOWORD(lParam)>((blockwidth+2)*BEZoom()*bebpr)) break;
        if (HIWORD(lParam) < BETabStripHeight()) break;
        {
            int clicked;
            if (betype == BETYPE_TILESETS) {
                int rowh = BETilesetRowH(hwnd);
                int n = TS_NumDefs();
                clicked = beyoff + ((HIWORD(lParam) - BETabStripHeight()) / rowh);
                if (clicked < 0) break;
                if (clicked == n) {
                    BEAddTileset(hwnd);
                    BESetScrollRange();
                    InvalidateAll();
                    break;
                }
                if (clicked >= n) break;
                curtilesetsel = (short int)clicked;
                TS_SetCurDef(clicked);
                if (GetKeyState(VK_SHIFT) < 0) {
                    int ii = TS_FindInstOfDef(clicked, maplayer);
                    if (ii < 0) ii = TS_FindInstOfDef(clicked, -1);
                    if (ii >= 0) {
                        const TilesetInst *in = TS_GetInst(ii);
                        TS_SetCurInst(ii);
                        if (in != NULL && blockwidth > 0 && blockheight > 0) {
                            xmapoffset = (short int)in->tx;
                            ymapoffset = (short int)in->ty;
                            if (xmapoffset < 0) xmapoffset = 0;
                            if (ymapoffset < 0) ymapoffset = 0;
                            if (xmapoffset > mapwidth - 1) xmapoffset = mapwidth - 1;
                            if (ymapoffset > mapheight - 1) ymapoffset = mapheight - 1;
                            MESetScrollRange(mapwidth - 1, mapheight - 1, xmapoffset, ymapoffset);
                        }
                    }
                    BESetScrollRange();
                    InvalidateAll();
                    break;
                }
                if (!tsplace)
                    DIATilesetEdit(hwnd, clicked);
                BESetScrollRange();
                InvalidateAll();
                break;
            }
            if (betype == BETYPE_OBJECTS) {
                int rowh = BETilesetRowH(hwnd);
                int vis;
                BERefreshSort();
                vis = BEVisibleCount();
                clicked = beyoff + ((HIWORD(lParam) - BETabStripHeight()) / rowh);
                if (clicked < 0) break;
                if (clicked == vis) {
                    BEAddObject(hwnd);
                    BESetScrollRange();
                    InvalidateAll();
                    break;
                }
                if (clicked >= vis) break;
                clicked = BEMapDisp(clicked);
                if (clicked < 0) break;
                curobjsel = (short int) clicked;
                F14_SetCurObject(clicked);
                if (GetKeyState(VK_SHIFT) < 0) {
                    const F14Obj * o = F14_GetObject(curobjsel);
                    if (o != NULL && blockwidth > 0 && blockheight > 0) {
                        xmapoffset = (short int) (o->xpos / blockwidth);
                        ymapoffset = (short int) (o->ypos / blockheight);
                        if (xmapoffset < 0) xmapoffset = 0;
                        if (ymapoffset < 0) ymapoffset = 0;
                        if (xmapoffset > mapwidth - 1) xmapoffset = mapwidth - 1;
                        if (ymapoffset > mapheight - 1) ymapoffset = mapheight - 1;
                        MESetScrollRange(mapwidth - 1, mapheight - 1, xmapoffset, ymapoffset);
                    }
                }
                BESetScrollRange();
                InvalidateAll();
                break;
            }
            if (betype == BETYPE_BRUSHES) {
                int rowh = BETilesetRowH(hwnd);
                int vis = BEBrushCount();
                clicked = beyoff + ((HIWORD(lParam) - BETabStripHeight()) / rowh);
                if (clicked < 0) break;
                if (clicked == vis) {
                    BEAddBrush(hwnd);
                    BESetScrollRange();
                    InvalidateAll();
                    break;
                }
                if (clicked >= vis) break;
                clicked = BEBrushAt(clicked);
                if (clicked < 0 || brshpt[clicked] == NULL) break;
                curbrushsel = (short int) clicked;
                curbrsh = (short int) (clicked + 1);
                curanim = -1;
                RejigBrshMenu(curbrsh);
                BESetScrollRange();
                InvalidateAll();
                break;
            }
            clicked = ((beyoff + ((HIWORD(lParam)-BETabStripHeight())/((blockheight+2)*BEZoom()))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*BEZoom()));
            clicked = BEMapDisp(clicked);
            switch (betype) {
            case BETYPE_STILL:
                curanim = -1; curbrsh = 0;
                curstr = clicked;
                if (curstr>=numblockstr) curstr = numblockstr-1;
                if (curstr<0) curstr = 0;
                RejigBrshMenu(curbrsh);
                break;
            case BETYPE_ANIM:
                curbrsh = 0;
                curanim = clicked;
                if (curanim>=numblockanim) curanim = numblockanim-1;
                if (curanim<0) curanim = 0;
                RejigBrshMenu(curbrsh);
                break;
            case BETYPE_GRAPH:
                curgfxsel = clicked;
                if (curgfxsel>=numblockgfx) curgfxsel = numblockgfx-1;
                if (curgfxsel<0) curgfxsel = 0;
                break;
            }
        }
        BESetScrollRange();
        InvalidateAll();
        break;
    case WM_RBUTTONUP:
        BESetType((betype + 1) % BETYPE_COUNT);
        break;
    case WM_LBUTTONDBLCLK:
        if (blockstrpt == NULL) break;
        if (betype == BETYPE_TILESETS) {
            int rowh, clicked, n;
            if (HIWORD(lParam) < BETabStripHeight()) break;
            rowh = BETilesetRowH(hwnd);
            n = TS_NumDefs();
            clicked = beyoff + ((HIWORD(lParam) - BETabStripHeight()) / rowh);
            if (clicked == n) {
                BEAddTileset(hwnd);
                BESetScrollRange();
                InvalidateAll();
                break;
            }
            if (clicked < 0 || clicked >= n) break;
            curtilesetsel = (short int)clicked;
            TS_SetCurDef(clicked);
            DIATilesetEdit(hwnd, clicked);
            BESetScrollRange();
            InvalidateAll();
            break;
        }
        if (betype == BETYPE_OBJECTS) {
            int rowh, clicked, vis;
            if (HIWORD(lParam) < BETabStripHeight()) break;
            BERefreshSort();
            vis = BEVisibleCount();
            rowh = BETilesetRowH(hwnd);
            clicked = beyoff + ((HIWORD(lParam) - BETabStripHeight()) / rowh);
            if (clicked == vis) {
                BEAddObject(hwnd);
                BESetScrollRange();
                InvalidateAll();
                break;
            }
            if (clicked < 0 || clicked >= vis) break;
            clicked = BEMapDisp(clicked);
            if (clicked < 0) break;
            curobjsel = (short int) clicked;
            F14_SetCurObject(clicked);
            DIAObjectProps(hwnd, clicked);
            BESetScrollRange();
            InvalidateAll();
            break;
        }
        if (betype == BETYPE_BRUSHES) {
            int rowh, clicked, vis;
            if (HIWORD(lParam) < BETabStripHeight()) break;
            vis = BEBrushCount();
            rowh = BETilesetRowH(hwnd);
            clicked = beyoff + ((HIWORD(lParam) - BETabStripHeight()) / rowh);
            if (clicked == vis) {
                BEAddBrush(hwnd);
                BESetScrollRange();
                InvalidateAll();
                break;
            }
            if (clicked < 0 || clicked >= vis) break;
            clicked = BEBrushAt(clicked);
            if (clicked < 0 || brshpt[clicked] == NULL) break;
            curbrushsel = (short int) clicked;
            curbrsh = (short int) (clicked + 1);
            DIAGetBrushName(hwnd, GetBrushName(clicked));
            RejigBrshMenu(curbrsh);
            BESetScrollRange();
            InvalidateAll();
            break;
        }
        if (LOWORD(lParam)>((blockwidth+2)*BEZoom()*bebpr)) break;
        if (HIWORD(lParam) < BETabStripHeight()) break;
        {
            int clicked = ((beyoff + ((HIWORD(lParam)-BETabStripHeight())/((blockheight+2)*BEZoom()))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*BEZoom()));
            clicked = BEMapDisp(clicked);
            switch (betype) {
            case BETYPE_STILL:
                curanim = -1; curbrsh = 0;
                curstr = clicked;
                if (curstr>=numblockstr) { curstr = numblockstr-1; break; }
                if (curstr<0) curstr = 0;
                RejigBrshMenu(curbrsh);
                DIAEditProperties(hwnd, curstr);
                break;
            case BETYPE_ANIM:
                curbrsh = 0;
                curanim = clicked;
                if (curanim>=numblockanim) { curanim = numblockanim-1; break; }
                if (curanim<0) curanim = 0;
                RejigBrshMenu(curbrsh);
                DIAEditAnimProperties(hwnd, curanim);
                break;
            case BETYPE_GRAPH:
                curgfxsel = clicked;
                if (curgfxsel>=numblockgfx) { curgfxsel = numblockgfx-1; break; }
                if (curgfxsel<0) curgfxsel = 0;
                break;
            }
        }
        InvalidateAll();
        break;
    case WM_DESTROY:
        htab = NULL;
        if (be_font != NULL) {
            DeleteObject(be_font);
            be_font = NULL;
        }
        break;
    case WM_MOUSEWHEEL:
        if (blockgfxpt==NULL) break;
        {
            int bdelta = (short)HIWORD(wParam);
            int bstep = (bdelta>0) ? -mwheelblks : mwheelblks;
            BERefreshSort();
            i = (short int) BEMaxScrollRow();
            beyoff += (short int) bstep;
            if (beyoff<0) beyoff = 0;
            if (beyoff>i) beyoff = i;
            SetScrollPos(bhwnd, SB_VERT, beyoff, TRUE);
            InvalidateRgn(bhwnd, NULL, FALSE);
        }
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static int BETabStripHeight(void) {
    if (be_tabh < 1) return TAB_H;
    return be_tabh;
}

static void BEEnsureUiFont(HWND hwnd) {
    HFONT created = MapCreateUiFont(hwnd);
    if (created == NULL) return;
    if (be_font != NULL) DeleteObject(be_font);
    be_font = created;
    if (htab != NULL)
        SendMessage(htab, WM_SETFONT, (WPARAM) be_font, TRUE);
}

static void BESyncTabSel(void) {
    if (htab == NULL) return;
    if ((int) SendMessage(htab, TCM_GETCURSEL, 0, 0) != betype)
        SendMessage(htab, TCM_SETCURSEL, (WPARAM) betype, 0);
}

static void BERecalcMinPane(HWND hwnd)
{
    RECT r;
    LONG style;
    int i, n, left, right, chrome, need;

    be_minpane = MapDip(hwnd, MAP_DIP_SPLIT);
    if (htab == NULL)
        return;

    SendMessage(htab, TCM_SETPADDING, 0,
        MAKELPARAM(MapDip(hwnd, 6), MapDip(hwnd, 3)));

    style = GetWindowLong(htab, GWL_STYLE);
    SetWindowLong(htab, GWL_STYLE, style & ~TCS_MULTILINE);
    SetWindowPos(htab, NULL, 0, 0, MapDip(hwnd, 2000), MapDip(hwnd, 48),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    n = (int)SendMessage(htab, TCM_GETITEMCOUNT, 0, 0);
    left = 0x7fffffff;
    right = 0;
    for (i = 0; i < n; i++) {
        if (!SendMessage(htab, TCM_GETITEMRECT, (WPARAM)i, (LPARAM)&r))
            continue;
        if (r.left < left) left = r.left;
        if (r.right > right) right = r.right;
    }

    SetWindowLong(htab, GWL_STYLE, style);
    SetWindowPos(htab, NULL, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    if (right <= left)
        return;

    if (bhwnd != NULL) {
        RECT wr, cr;
        GetWindowRect(bhwnd, &wr);
        GetClientRect(bhwnd, &cr);
        chrome = (wr.right - wr.left) - (cr.right - cr.left);
        if (chrome < 8)
            chrome = GetSystemMetrics(SM_CXVSCROLL) + GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    } else {
        chrome = GetSystemMetrics(SM_CXVSCROLL) + GetSystemMetrics(SM_CXSIZEFRAME) * 2;
    }
    need = (right - left) + chrome + MapDip(hwnd, 24);
    if (need > be_minpane)
        be_minpane = need;
}

static void BELayoutTabs(HWND parent) {
    RECT crc, rc;
    int w, dpi, hguess, pad, minstrip;

    if (htab == NULL || parent == NULL) return;
    GetClientRect(parent, &crc);
    w = crc.right - crc.left;
    if (w < 8) w = 8;
    dpi = MapGetDpi(parent);
    pad = MapScale(6, dpi);
    SendMessage(htab, TCM_SETPADDING, 0, MAKELPARAM(pad, MapScale(3, dpi)));

    /* Give the control enough height to wrap, then shrink to the strip. */
    hguess = MapScale(72, dpi);
    SetWindowPos(htab, HWND_TOP, 0, 0, w, hguess, SWP_NOACTIVATE);

    rc.left = 0;
    rc.top = 0;
    rc.right = w;
    rc.bottom = hguess;
    SendMessage(htab, TCM_ADJUSTRECT, FALSE, (LPARAM) &rc);
    be_tabh = rc.top;
    minstrip = MapScale(TAB_H, dpi);
    if (be_tabh < minstrip) be_tabh = minstrip;

    SetWindowPos(htab, HWND_TOP, 0, 0, w, be_tabh, SWP_NOACTIVATE);

    GetClientRect(htab, &rc);
    SendMessage(htab, TCM_ADJUSTRECT, FALSE, (LPARAM) &rc);
    if (rc.top > minstrip) be_tabh = rc.top;
    SetWindowPos(htab, HWND_TOP, 0, 0, w, be_tabh, SWP_NOACTIVATE);
}

static void BECreateTabControl(HWND parent) {
    static const char * labels[] = { "Still", "Anim", "Graph", "Brushes", "Objects", "Tilesets" };
    INITCOMMONCONTROLSEX icc;
    TCITEM tie;
    int i;

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    htab = CreateWindowEx(0, WC_TABCONTROL, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
        TCS_MULTILINE | TCS_HOTTRACK | TCS_RAGGEDRIGHT | TCS_FOCUSONBUTTONDOWN,
        0, 0, MapDip(parent, 64), MapDip(parent, TAB_H),
        parent, (HMENU)(INT_PTR) IDC_BEMODETAB, hInstance, NULL);
    if (htab == NULL) return;

    BEEnsureUiFont(parent);
    memset(&tie, 0, sizeof(tie));
    tie.mask = TCIF_TEXT;
    for (i = 0; i < BETYPE_COUNT; i++) {
        tie.pszText = (LPSTR) labels[i];
        SendMessage(htab, TCM_INSERTITEM, (WPARAM) i, (LPARAM) &tie);
    }
    SendMessage(htab, TCM_SETCURSEL, (WPARAM) betype, 0);
    BERecalcMinPane(parent);
    BELayoutTabs(parent);
}

HWND CreateBlockEditorWindow(HWND hwnd) {
    int x;
    RECT rc;
    WNDCLASS wndclass;

    if (bhwnd != NULL) return bhwnd;

    memset(&wndclass, 0, sizeof(WNDCLASS));
    wndclass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT | CS_NOCLOSE;
    wndclass.lpfnWndProc = BlockWndProc;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(hInstance, "MAPWINIC");
    wndclass.hCursor = LoadCursor(NULL, (LPCSTR) IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH) GetStockObject(GRAY_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szBlockEdname;
    RegisterClass(&wndclass);

    GetWindowRect(hwnd, &rc);
    {
        RECT crc;
        int split, pad, fudge;
        GetClientRect(hwnd, &crc);
        split = BEPaneWidth(crc.right);
        pad = MapDip(hwnd, MAP_DIP_SPLITPAD);
        fudge = MapCaptionFudge(hwnd);
        if (wlayout == 0 || wlayout == -1) x = (rc.right)-(rc.left)-split-pad;
        else x = 0;
        bhwnd = CreateWindow(szBlockEdname, szBlockEdname,
            WS_MAXIMIZEBOX	| WS_MINIMIZEBOX | WS_CAPTION | WS_CLIPSIBLINGS |
            WS_CLIPCHILDREN | WS_THICKFRAME | WS_CHILDWINDOW | WS_VSCROLL,
            x, 0, split,
            (rc.bottom)-(rc.top)-fudge,
            hwnd, (HMENU) 1001, hInstance, NULL);
    }

    BECreateTabControl(bhwnd);
    BEUpdateScrollVis();
    ShowWindow(bhwnd, SW_SHOW);
    UpdateWindow(bhwnd);
    return bhwnd;
}

HWND BEGetHWND(void) {
    return bhwnd;
}
