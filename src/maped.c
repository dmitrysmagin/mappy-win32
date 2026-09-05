#define  STRICT
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"
#include "mapdefs.h"
#include "compat.h"
#include "dpi.h"
#include "luabind.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

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
static HWND ghwnd = NULL;
static HWND palhwnd = NULL;
static HWND ge_toolbtn[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static HWND ge_dsbtn[4] = { NULL, NULL, NULL, NULL };
static HWND ge_dslbl = NULL, ge_status = NULL;
static HFONT ge_font = NULL;
static HFONT me_font = NULL;
static int pal_syncing = 0;
static int pal_hex = 0;		/* 0 = decimal RGB fields, 1 = $XX hex */
static char szMapEdname[] = "Map Editor";
static char szGraphEdname[] = "Graphic Editor";
#define IDC_GETOOL0	2200
#define IDC_GETHICK1	2211
#define IDC_GEDRAWLBL	2220
#define IDC_GESTATUS	2221
#define GETOOL_COUNT	7
static int GE_thickness = 1;
static int GE_gfxcol = 0;
static int GE_paint = 0;
static int GE_palop = 0;		/* 1=CPY 2=EXG 3=SPR 4=pick-from-tile */
static int GE_palsrc = 0;
static int GE_FindBlockForGraphic(int gfx);
static void GEGetColRGB(int * r, int * g, int * b);
static void GESetColChan(int chan, int v);
static void GEPaletteOp(int dest);
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


static void MEPlaceLeftPane(HWND whwnd, int wide, int high) {
    if (whwnd == NULL) return;
    if (wlayout == 0) MoveWindow(whwnd, 0, 0, wide-(wide/5), high, TRUE);
    if (wlayout == 1) MoveWindow(whwnd, wide/5, 0, wide-(wide/5), high, TRUE);
    if (wlayout == -1) MoveWindow(whwnd, 0, 0, wide, high, TRUE);
}

void MEResizeWind(int wide, int high) {
    MEPlaceLeftPane(mhwnd, wide, high);
    MEPlaceLeftPane(ghwnd, wide, high);
}

void MESetScrollRange(int xrng, int yrng, int xpos, int ypos) {
    SetScrollRange(mhwnd, SB_HORZ, 0, xrng, FALSE);
    SetScrollRange(mhwnd, SB_VERT, 0, yrng, FALSE);
    SetScrollPos(mhwnd, SB_HORZ, xpos, TRUE);
    SetScrollPos(mhwnd, SB_VERT, ypos, TRUE);
}

void MEDestroyUndoBuffs(void) {
    if (undobuff1!=NULL) { free(undobuff1); undobuff1 = NULL; }
    if (undobuff2!=NULL) { free(undobuff2); undobuff2 = NULL; }
}

void MEUndo(void) {
    if (undobuff1==NULL || undobuff2==NULL) return;
    memcpy(undobuff2, mappt, mapwidth*mapheight*2);
    memcpy(mappt, undobuff1, mapwidth*mapheight*2);
    memcpy(undobuff1, undobuff2, mapwidth*mapheight*2);
}

void MECopyToUndo(void) {
    if (mappt==NULL) return;
    if (undobuff1==NULL) {
        undobuff1 = malloc(mapwidth*mapheight*2);
        if (undobuff1==NULL) return;
        memset(undobuff1, 0, mapwidth*mapheight*2);
    }
    if (undobuff2==NULL) {
        undobuff2 = malloc(mapwidth*mapheight*2);
        if (undobuff2==NULL) return;
        memset(undobuff2, 0, mapwidth*mapheight*2);
    }
    memcpy(undobuff1, mappt, mapwidth*mapheight*2);
}

static void MEEnsureUndoBuf(void) {
    size_t n = (size_t) mapwidth * (size_t) mapheight * 2;
    if (n == 0) return;
    if (undobuff1==NULL) { undobuff1 = malloc(n); if (undobuff1) memset(undobuff1, 0, n); }
    if (undobuff2==NULL) { undobuff2 = malloc(n); if (undobuff2) memset(undobuff2, 0, n); }
}

void MECopyLayerToUndo(short int *src) {
    size_t n = (size_t) mapwidth * (size_t) mapheight * 2;
    MEEnsureUndoBuf();
    if (undobuff1 != NULL && src != NULL) memcpy(undobuff1, src, n);
}

void MECopyUndoToLayer(short int *dst) {
    size_t n = (size_t) mapwidth * (size_t) mapheight * 2;
    if (undobuff1 != NULL && dst != NULL) memcpy(dst, undobuff1, n);
}

HWND MEGetHWND(void) {
    return mhwnd;
}

HWND GEGetHWND(void) {
    return ghwnd;
}

static void PalSyncFromColor(void);
static void PalSyncButtons(void);
static void PalSetFont(HWND dlg);
static void GEPlacePalette(HWND graph);
static int GEPaletteWidth(HWND hwnd);

static void GEInvalidate(void) {
    if (ghwnd != NULL) InvalidateRect(ghwnd, NULL, FALSE);
    GERefreshPalette();
}

static int GEToolbarHeight(HWND hwnd) {
    return MapDip(hwnd, 24) + MapDip(hwnd, 18) + MapDip(hwnd, 12);
}

static void MEEnsureUiFont(HWND hwnd) {
    HFONT created = MapCreateUiFont(hwnd);
    if (created == NULL) return;
    if (me_font != NULL) DeleteObject(me_font);
    me_font = created;
}

static void GEEnsureUiFont(HWND hwnd) {
    HFONT created;
    int i;

    created = MapCreateUiFont(hwnd);
    if (created == NULL) return;
    if (ge_font != NULL) DeleteObject(ge_font);
    ge_font = created;
    if (ge_dslbl) SendMessage(ge_dslbl, WM_SETFONT, (WPARAM) ge_font, TRUE);
    if (ge_status) SendMessage(ge_status, WM_SETFONT, (WPARAM) ge_font, TRUE);
    for (i = 0; i < GETOOL_COUNT; i++)
        if (ge_toolbtn[i]) SendMessage(ge_toolbtn[i], WM_SETFONT, (WPARAM) ge_font, TRUE);
    for (i = 0; i < 4; i++)
        if (ge_dsbtn[i]) SendMessage(ge_dsbtn[i], WM_SETFONT, (WPARAM) ge_font, TRUE);
    if (palhwnd) PalSetFont(palhwnd);
}

static void GEUpdateToolbar(void) {
    int i;
    for (i = 0; i < 4; i++) {
        if (ge_dsbtn[i] != NULL)
            SendMessage(ge_dsbtn[i], BM_SETCHECK,
            (GE_thickness == i + 1) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

static void GEUpdateStatus(void) {
    char txt[96];
    int blk;
    if (ge_status == NULL) return;
    blk = GE_FindBlockForGraphic(curgfxsel);
    if (blk >= 0)
        sprintf(txt, "This graphic is used by block %d", blk);
    else
        sprintf(txt, "This graphic is not used by any block (click BLK to make one)");
    SetWindowText(ge_status, txt);
}

static void GELayoutToolbar(HWND hwnd) {
    RECT rc;
    int dpi, pw, ph, btnw, btnh, dsw, gap, margin, statush, x, y, i, need, dslblw;

    if (hwnd == NULL) return;
    GetClientRect(hwnd, &rc);
    pw = rc.right - rc.left;
    ph = rc.bottom - rc.top;
    dpi = MapGetDpi(hwnd);
    btnw = MapScale(44, dpi);
    btnh = MapScale(24, dpi);
    dsw = MapScale(28, dpi);
    gap = MapScale(4, dpi);
    margin = MapScale(8, dpi);
    statush = MapScale(18, dpi);
    dslblw = MapScale(72, dpi);
    need = GETOOL_COUNT * (btnw + gap) + gap * 2 + dslblw + 4 * (dsw + gap) + margin * 2;
    if (need > pw && pw > margin * 2) {
        int scale = pw - margin * 2;
        btnw = btnw * scale / need;
        dsw = dsw * scale / need;
        dslblw = dslblw * scale / need;
        if (btnw < MapScale(28, dpi)) btnw = MapScale(28, dpi);
        if (dsw < MapScale(20, dpi)) dsw = MapScale(20, dpi);
    }
    y = ph - statush - gap - btnh - MapScale(4, dpi);
    if (y < 0) y = 0;
    x = margin;
    for (i = 0; i < GETOOL_COUNT; i++) {
        if (ge_toolbtn[i])
            MoveWindow(ge_toolbtn[i], x, y, btnw, btnh, TRUE);
        x += btnw + gap;
    }
    x += gap;
    if (ge_dslbl)
        MoveWindow(ge_dslbl, x, y + (btnh - statush) / 2, dslblw, statush, TRUE);
    x += dslblw + gap;
    for (i = 0; i < 4; i++) {
        if (ge_dsbtn[i])
            MoveWindow(ge_dsbtn[i], x, y, dsw, btnh, TRUE);
        x += dsw + gap;
    }
    if (ge_status)
        MoveWindow(ge_status, margin, ph - statush - MapScale(2, dpi), pw - margin * 2, statush, TRUE);
}

static void GECreateToolbar(HWND parent) {
    static const char * tools[] = { "FLH", "FLV", "ROT", "CLR", "MSK", "BLK", "REV" };
    int i;
    char dsch[2];

    for (i = 0; i < GETOOL_COUNT; i++) {
        ge_toolbtn[i] = CreateWindow("button", tools[i],
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 44, 24, parent, (HMENU)(INT_PTR)(IDC_GETOOL0 + i),
            hInstance, NULL);
    }
    ge_dslbl = CreateWindow("static", "Draw size:",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 72, 18, parent, (HMENU)(INT_PTR) IDC_GEDRAWLBL, hInstance, NULL);
    dsch[1] = 0;
    for (i = 0; i < 4; i++) {
        dsch[0] = (char) ('1' + i);
        ge_dsbtn[i] = CreateWindow("button", dsch,
            WS_CHILD | WS_VISIBLE | BS_PUSHLIKE | BS_CHECKBOX,
            0, 0, 28, 24, parent, (HMENU)(INT_PTR)(IDC_GETHICK1 + i),
            hInstance, NULL);
    }
    ge_status = CreateWindow("static", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 200, 18, parent, (HMENU)(INT_PTR) IDC_GESTATUS, hInstance, NULL);
    GEEnsureUiFont(parent);
    GEUpdateToolbar();
    GELayoutToolbar(parent);
    GEUpdateStatus();
}

void GEShowMode(int show) {
    if (show) {
        if (mhwnd) ShowWindow(mhwnd, SW_HIDE);
        if (ghwnd) {
            ShowWindow(ghwnd, SW_SHOW);
            GELayoutToolbar(ghwnd);
            GEUpdateToolbar();
            GEUpdateStatus();
            GEPlacePalette(ghwnd);
            InvalidateRect(ghwnd, NULL, FALSE);
            SetFocus(ghwnd);
        }
    } else {
        if (ghwnd) ShowWindow(ghwnd, SW_HIDE);
        if (mhwnd) ShowWindow(mhwnd, SW_SHOW);
    }
}

static int GEPaletteCols(void) {
    return (panelpro == 2) ? 32 : 16;
}

static int GEPaletteRows(void) {
    return (panelpro == 2) ? 8 : 16;
}

static int GEPaletteWidth(HWND hwnd) {
    int dpi = MapGetDpi(hwnd);
    int cols = GEPaletteCols();
    int cell = MapScale((panelpro == 2) ? 10 : 14, dpi);
    int w, minw;
    if (cell < MapScale(8, dpi)) cell = MapScale(8, dpi);
    w = cols * cell + MapScale(20, dpi);
    minw = MapScale(168, dpi);
    if (w < minw) w = minw;
    return w;
}

static void PalSetFont(HWND dlg) {
    HWND child;
    if (ge_font == NULL || dlg == NULL) return;
    child = GetWindow(dlg, GW_CHILD);
    while (child) {
        SendMessage(child, WM_SETFONT, (WPARAM) ge_font, TRUE);
        child = GetWindow(child, GW_HWNDNEXT);
    }
}

static void PalSyncButtons(void) {
    if (palhwnd == NULL) return;
    pal_syncing = 1;
    CheckDlgButton(palhwnd, IDC_PALCPY, (GE_palop == 1) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(palhwnd, IDC_PALEXG, (GE_palop == 2) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(palhwnd, IDC_PALSPR, (GE_palop == 3) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(palhwnd, IDC_PALPICK, (GE_palop == 4) ? BST_CHECKED : BST_UNCHECKED);
    pal_syncing = 0;
}

static void PalSyncFromColor(void) {
    char txt[64];
    int r, g, b;
    if (palhwnd == NULL || pal_syncing) return;
    pal_syncing = 1;
    GEGetColRGB(&r, &g, &b);
    sprintf(txt, "Palette (col %d/255)", GE_gfxcol);
    SetDlgItemText(palhwnd, IDC_PALTITLE, txt);
    SendDlgItemMessage(palhwnd, IDC_PALTBARR, TBM_SETPOS, TRUE, 255 - r);
    SendDlgItemMessage(palhwnd, IDC_PALTBARG, TBM_SETPOS, TRUE, 255 - g);
    SendDlgItemMessage(palhwnd, IDC_PALTBARB, TBM_SETPOS, TRUE, 255 - b);
    if (pal_hex) {
        sprintf(txt, "$%02X", r);
        SetDlgItemText(palhwnd, IDC_PALDECR, txt);
        sprintf(txt, "$%02X", g);
        SetDlgItemText(palhwnd, IDC_PALDECG, txt);
        sprintf(txt, "$%02X", b);
        SetDlgItemText(palhwnd, IDC_PALDECB, txt);
    } else {
        sprintf(txt, "%d", r);
        SetDlgItemText(palhwnd, IDC_PALDECR, txt);
        sprintf(txt, "%d", g);
        SetDlgItemText(palhwnd, IDC_PALDECG, txt);
        sprintf(txt, "%d", b);
        SetDlgItemText(palhwnd, IDC_PALDECB, txt);
    }
    SendDlgItemMessage(palhwnd, IDC_PALUDR, UDM_SETPOS, 0, MAKELPARAM(r, 0));
    SendDlgItemMessage(palhwnd, IDC_PALUDG, UDM_SETPOS, 0, MAKELPARAM(g, 0));
    SendDlgItemMessage(palhwnd, IDC_PALUDB, UDM_SETPOS, 0, MAKELPARAM(b, 0));
    CheckDlgButton(palhwnd, IDC_PALHEXVIEW, pal_hex ? BST_CHECKED : BST_UNCHECKED);
    InvalidateRect(GetDlgItem(palhwnd, IDC_PALGRID), NULL, FALSE);
    pal_syncing = 0;
}

void GERefreshPalette(void) {
    if (palhwnd != NULL && IsWindowVisible(palhwnd))
        PalSyncFromColor();
}

static void PalColorChanged(void) {
    mapaltered = 1;
    if (ghwnd) InvalidateRect(ghwnd, NULL, FALSE);
    PalSyncFromColor();
    BEInvalidate();
}

static void PalArmOp(int op) {
    if (GE_palop == op) GE_palop = 0;
    else {
        GE_palop = op;
        GE_palsrc = GE_gfxcol;
    }
    PalSyncButtons();
}

static void PalClickSwatch(int ci) {
    if (ci < 0) ci = 0;
    if (ci > 255) ci = 255;
    if (GE_palop >= 1 && GE_palop <= 3) {
        GEPaletteOp(ci);
        GE_gfxcol = ci;
    } else {
        GE_gfxcol = ci;
        if (GE_palop != 4) GE_palop = 0;
    }
    PalSyncButtons();
    PalSyncFromColor();
    if (ghwnd) InvalidateRect(ghwnd, NULL, FALSE);
}

static void PalApplyTrans(void) {
    if (cmappt == NULL || GE_gfxcol < 0 || GE_gfxcol > 255) return;
    if (blockdepth == 8) {
        cmappt[GE_gfxcol * 3] = cmappt[trans8bit * 3];
        cmappt[GE_gfxcol * 3 + 1] = cmappt[trans8bit * 3 + 1];
        cmappt[GE_gfxcol * 3 + 2] = cmappt[trans8bit * 3 + 2];
    } else {
        cmappt[GE_gfxcol * 3] = transred;
        cmappt[GE_gfxcol * 3 + 1] = transgreen;
        cmappt[GE_gfxcol * 3 + 2] = transblue;
    }
    GE_palop = 0;
    PalSyncButtons();
    PalColorChanged();
}

static int PalParseValue(HWND dlg, int id) {
    char txt[16], * p;
    int v, hex;
    GetDlgItemText(dlg, id, txt, (int) sizeof(txt));
    p = txt;
    hex = pal_hex;
    if (p[0] == '$' || p[0] == '#') {
        p++;
        hex = 1;
    }
    v = (int) strtol(p, NULL, hex ? 16 : 10);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return v;
}

static void PalLayoutDialog(HWND dlg) {
    RECT rc;
    int dpi, cw, ch, m, titleh, btnh, barh, edith, checkh, spinw, gap;
    int cols, rows, cell, gridw, gridh, gx, gy, i, x, y, bw;
    static const int btns[] = { IDC_PALCPY, IDC_PALEXG, IDC_PALSPR, IDC_PALTRN, IDC_PALPICK };
    static const int bars[] = { IDC_PALTBARR, IDC_PALTBARG, IDC_PALTBARB };
    static const int lbls[] = { IDC_PALLBLR, IDC_PALLBLG, IDC_PALLBLB };
    static const int vals[] = { IDC_PALDECR, IDC_PALDECG, IDC_PALDECB };
    static const int uds[] = { IDC_PALUDR, IDC_PALUDG, IDC_PALUDB };

    if (dlg == NULL) return;
    GetClientRect(dlg, &rc);
    cw = rc.right - rc.left;
    ch = rc.bottom - rc.top;
    dpi = MapGetDpi(dlg);
    m = MapScale(6, dpi);
    titleh = MapScale(16, dpi);
    btnh = MapScale(22, dpi);
    barh = MapScale(72, dpi);
    edith = MapScale(20, dpi);
    checkh = MapScale(16, dpi);
    spinw = MapScale(16, dpi);
    gap = MapScale(4, dpi);
    cols = GEPaletteCols();
    rows = GEPaletteRows();

    MoveWindow(GetDlgItem(dlg, IDC_PALTITLE), m, gap, cw - m * 2, titleh, TRUE);

    gy = gap + titleh + gap;
    gridw = cw - m * 2;
    cell = gridw / cols;
    if (cell < MapScale(8, dpi)) cell = MapScale(8, dpi);
    if (cell > MapScale(20, dpi)) cell = MapScale(20, dpi);
    gridw = cell * cols;
    gridh = cell * rows;
    {
        int leftover = ch - gy - btnh - gap - MapScale(12, dpi) - barh
        - gap - edith - gap - checkh - m;
        if (leftover < 0) leftover = 0;
        if (gridh > leftover && leftover >= rows * MapScale(8, dpi)) {
            cell = leftover / rows;
            if (cell < MapScale(8, dpi)) cell = MapScale(8, dpi);
            gridw = cell * cols;
            gridh = cell * rows;
        }
    }
    gx = (cw - gridw) / 2;
    if (gx < m) gx = m;
    MoveWindow(GetDlgItem(dlg, IDC_PALGRID), gx, gy, gridw, gridh, TRUE);

    y = gy + gridh + gap;
    bw = (cw - m * 2 - gap * 4) / 5;
    if (bw < MapScale(24, dpi)) bw = MapScale(24, dpi);
    x = m;
    for (i = 0; i < 5; i++) {
        MoveWindow(GetDlgItem(dlg, btns[i]), x, y, bw, btnh, TRUE);
        x += bw + gap;
    }

    y += btnh + gap;
    {
        int colw = (cw - m * 2) / 3;
        int lblh = MapScale(12, dpi);
        for (i = 0; i < 3; i++) {
            int cx = m + i * colw;
            int fieldw = colw - gap - spinw;
            if (fieldw < MapScale(20, dpi)) fieldw = MapScale(20, dpi);
            MoveWindow(GetDlgItem(dlg, lbls[i]), cx, y, colw, lblh, TRUE);
            MoveWindow(GetDlgItem(dlg, bars[i]), cx, y + lblh, colw - gap, barh, TRUE);
            MoveWindow(GetDlgItem(dlg, vals[i]), cx, y + lblh + barh + gap, fieldw, edith, TRUE);
            SetWindowPos(GetDlgItem(dlg, uds[i]), HWND_TOP,
                cx + fieldw, y + lblh + barh + gap, spinw, edith,
                SWP_SHOWWINDOW);
        }
        y += lblh + barh + gap + edith + gap;
    }
    MoveWindow(GetDlgItem(dlg, IDC_PALHEXVIEW), m, y, cw - m * 2, checkh, TRUE);
}

static void GEPlacePalette(HWND graph) {
    RECT rc;
    int palw, toolh, pw, ph;
    if (palhwnd == NULL || graph == NULL) return;
    GetClientRect(graph, &rc);
    pw = rc.right - rc.left;
    ph = rc.bottom - rc.top;
    toolh = GEToolbarHeight(graph);
    if (panelpro == 0 || betype != BETYPE_GRAPH) {
        ShowWindow(palhwnd, SW_HIDE);
        return;
    }
    palw = GEPaletteWidth(graph);
    if (palw > pw / 2) palw = pw / 2;
    if (palw < 120) palw = 120;
    MoveWindow(palhwnd, pw - palw, 0, palw, ph - toolh, TRUE);
    PalLayoutDialog(palhwnd);
    ShowWindow(palhwnd, SW_SHOW);
    PalSyncButtons();
    PalSyncFromColor();
}

static LRESULT CALLBACK PalGridWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            int cols, rows, cw, ch, cell, cx, cy, ci;
            GetClientRect(hwnd, &rc);
            cw = rc.right - rc.left;
            ch = rc.bottom - rc.top;
            cols = GEPaletteCols();
            rows = GEPaletteRows();
            cell = cw / cols;
            if (ch / rows < cell) cell = ch / rows;
            if (cell < 1) cell = 1;
            FillRect(hdc, &rc, (HBRUSH) GetStockObject(BLACK_BRUSH));
            for (cy = 0; cy < rows; cy++) {
                for (cx = 0; cx < cols; cx++) {
                    RECT prc;
                    HBRUSH hbr;
                    ci = cy * cols + cx;
                    if (ci > 255) continue;
                    prc.left = cx * cell;
                    prc.top = cy * cell;
                    prc.right = prc.left + cell;
                    prc.bottom = prc.top + cell;
                    if (cmappt != NULL)
                        hbr = CreateSolidBrush(RGB(cmappt[ci * 3], cmappt[ci * 3 + 1], cmappt[ci * 3 + 2]));
                    else
                        hbr = CreateSolidBrush(RGB(ci, ci, ci));
                    FillRect(hdc, &prc, hbr);
                    DeleteObject(hbr);
                    if (ci == GE_gfxcol) {
                        HPEN hpen, hold;
                        HBRUSH holdbr;
                        hpen = CreatePen(PS_SOLID, MapPenWidth(hwnd) * 2, RGB(255, 255, 255));
                        hold = SelectObject(hdc, hpen);
                        holdbr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                        Rectangle(hdc, prc.left, prc.top, prc.right, prc.bottom);
                        SelectObject(hdc, hold);
                        SelectObject(hdc, holdbr);
                        DeleteObject(hpen);
                    }
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    case WM_LBUTTONDOWN: {
            RECT rc;
            int cols, rows, cell, mx, my, ci;
            GetClientRect(hwnd, &rc);
            cols = GEPaletteCols();
            rows = GEPaletteRows();
            cell = (rc.right - rc.left) / cols;
            if ((rc.bottom - rc.top) / rows < cell) cell = (rc.bottom - rc.top) / rows;
            if (cell < 1) cell = 1;
            mx = (int)(short) LOWORD(lParam);
            my = (int)(short) HIWORD(lParam);
            ci = (mx / cell) + (my / cell) * cols;
            SetFocus(hwnd);
            PalClickSwatch(ci);
            return 0;
        }
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    case WM_KEYDOWN: {
            int cols = GEPaletteCols();
            int ci = GE_gfxcol;
            switch (LOWORD(wParam)) {
            case VK_LEFT: ci--; break;
            case VK_RIGHT: ci++; break;
            case VK_UP: ci -= cols; break;
            case VK_DOWN: ci += cols; break;
            default: return 0;
            }
            PalClickSwatch(ci);
            return 0;
        }
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static void GERegisterPalGrid(void) {
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PalGridWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "GEPalSwatch";
    RegisterClass(&wc);
}

static INT_PTR CALLBACK DIAPaletteProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int id, code;

    switch (msg) {
    case WM_INITDIALOG: {
            INITCOMMONCONTROLSEX icc;
            int i;
            static const int bars[] = { IDC_PALTBARR, IDC_PALTBARG, IDC_PALTBARB };
            static const int decs[] = { IDC_PALDECR, IDC_PALDECG, IDC_PALDECB };
            static const int uds[] = { IDC_PALUDR, IDC_PALUDG, IDC_PALUDB };
            icc.dwSize = sizeof(icc);
            icc.dwICC = ICC_BAR_CLASSES | ICC_UPDOWN_CLASS;
            InitCommonControlsEx(&icc);
            for (i = 0; i < 3; i++) {
                HWND ud;
                SendDlgItemMessage(hwndDlg, bars[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
                /* Positioned next to the edit in PalLayoutDialog — not UDS_ALIGNRIGHT
               (that sticks to the template rect and ends up over the sliders). */
                ud = CreateWindow(UPDOWN_CLASS, "",
                    WS_CHILD | WS_VISIBLE | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                    0, 0, 16, 18, hwndDlg, (HMENU)(INT_PTR) uds[i], hInstance, NULL);
                if (ud) {
                    SendMessage(ud, UDM_SETRANGE, 0, MAKELPARAM(255, 0));
                    SendMessage(ud, UDM_SETBUDDY, (WPARAM) GetDlgItem(hwndDlg, decs[i]), 0);
                }
            }
            PalSetFont(hwndDlg);
            PalSyncButtons();
            PalSyncFromColor();
            return TRUE;
        }
    case WM_SIZE:
        PalLayoutDialog(hwndDlg);
        return TRUE;
    case WM_COMMAND:
        id = (int) LOWORD(wParam);
        code = (int) HIWORD(wParam);
        if (pal_syncing) return TRUE;
        if (id == IDC_PALCPY && code == BN_CLICKED) { PalArmOp(1); return TRUE; }
        if (id == IDC_PALEXG && code == BN_CLICKED) { PalArmOp(2); return TRUE; }
        if (id == IDC_PALSPR && code == BN_CLICKED) { PalArmOp(3); return TRUE; }
        if (id == IDC_PALPICK && code == BN_CLICKED) { PalArmOp(4); return TRUE; }
        if (id == IDC_PALTRN && code == BN_CLICKED) { PalApplyTrans(); return TRUE; }
        if (id == IDC_PALHEXVIEW && code == BN_CLICKED) {
            pal_hex = (IsDlgButtonChecked(hwndDlg, IDC_PALHEXVIEW) == BST_CHECKED);
            PalSyncFromColor();
            return TRUE;
        }
        if (code == EN_CHANGE) {
            if (id == IDC_PALDECR) { GESetColChan(0, PalParseValue(hwndDlg, id)); PalColorChanged(); }
            else if (id == IDC_PALDECG) { GESetColChan(1, PalParseValue(hwndDlg, id)); PalColorChanged(); }
            else if (id == IDC_PALDECB) { GESetColChan(2, PalParseValue(hwndDlg, id)); PalColorChanged(); }
            return TRUE;
        }
        break;
    case WM_VSCROLL:
        if (pal_syncing) return TRUE;
        id = GetDlgCtrlID((HWND) lParam);
        if (id == IDC_PALUDR || id == IDC_PALUDG || id == IDC_PALUDB) {
            int v = (short) LOWORD(SendMessage((HWND) lParam, UDM_GETPOS, 0, 0));
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            if (id == IDC_PALUDR) GESetColChan(0, v);
            else if (id == IDC_PALUDG) GESetColChan(1, v);
            else GESetColChan(2, v);
            PalColorChanged();
            return TRUE;
        }
        {
            int v = 255 - (int) SendMessage((HWND) lParam, TBM_GETPOS, 0, 0);
            if (id == IDC_PALTBARR) GESetColChan(0, v);
            else if (id == IDC_PALTBARG) GESetColChan(1, v);
            else if (id == IDC_PALTBARB) GESetColChan(2, v);
            else break;
            PalColorChanged();
        }
        return TRUE;
    case WM_DPICHANGED:
        PalSetFont(hwndDlg);
        PalLayoutDialog(hwndDlg);
        return TRUE;
    default:
        break;
    }
    UNUSED(lParam);
    return FALSE;
}

int GEPaletteDlgMessage(MSG * msg) {
    if (palhwnd == NULL || !IsWindowVisible(palhwnd)) return 0;
    return IsDialogMessage(palhwnd, msg);
}

static void GECreatePalette(HWND parent) {
    if (palhwnd != NULL) return;
    GERegisterPalGrid();
    palhwnd = CreateDialog(hInstance, "D_PALEDIT", parent, AsDlgProc(DIAPaletteProc));
    if (palhwnd != NULL) {
        PalSetFont(palhwnd);
        ShowWindow(palhwnd, SW_HIDE);
    }
}

int MEClickmask(int x, int y, int xory) {
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
static int ZX(int v) { return (v * zoomon) / zoomdiv; }
static int ZD(int v) { return (v * zoomdiv) / zoomon; }

int MEGetX(int xpix, int ypix) {
    int xb;

    if (blockstaggerx || blockstaggery) {
        xpix += (blockstaggerx*zoomon/zoomdiv);
        ypix += (blockstaggery*zoomon/zoomdiv);
    }
    xpix = ZD(xpix);
    ypix = ZD(ypix);
    xb = xpix/blockgapx;

    if ((blockstaggerx || blockstaggery) && clickmask) xb += MEClickmask(xpix, ypix, 0);

    return xb;
}

int MEGetY(int xpix, int ypix) {
    int yb;

    if (blockstaggerx || blockstaggery) {
        xpix += (blockstaggerx*zoomon/zoomdiv);
        ypix += (blockstaggery*zoomon/zoomdiv);
    }
    xpix = ZD(xpix);
    ypix = ZD(ypix);
    yb = ypix/blockgapy;

    if ((blockstaggerx || blockstaggery) && clickmask) {
        yb *= 2;
        yb += MEClickmask(xpix, ypix, 1);
    }

    return yb;
}

/* Screen cursor to map-window client pixels. 0 if the cursor is outside
 * the client (caption/border) or there is no map window. */
static int MECursorClient(POINT * pt) {
    RECT rc;

    if (mhwnd == NULL || pt == NULL) return 0;
    GetCursorPos(pt);
    ScreenToClient(mhwnd, pt);
    GetClientRect(mhwnd, &rc);
    if (pt->x < rc.left || pt->x >= rc.right ||
        pt->y < rc.top || pt->y >= rc.bottom) return 0;
    return 1;
}

void MEFillMap(int fillstyle) {
    POINT mousecoords;

    if (mappt==NULL) return;
    if (!MECursorClient(&mousecoords)) return;
    MECopyToUndo();
    MapFill(xmapoffset+MEGetX(mousecoords.x,mousecoords.y),
        ymapoffset+MEGetY(mousecoords.x,mousecoords.y),
        fillstyle);
}

void MEPickBlock(void) {
    POINT mousecoords;

    if (mappt==NULL) return;
    if (!MECursorClient(&mousecoords)) return;
    PickBlock(xmapoffset+MEGetX(mousecoords.x,mousecoords.y),
        ymapoffset+MEGetY(mousecoords.x,mousecoords.y));
}

void MEShowCoords(int x, int y) {
    char temptext[640];
    char scbits[16];
    char dettxt[128];
    BLKSTR * myblkstrpt;
    //ANISTR * myanpt;
    short int * mymappt;
    int nlayers, li;
    const char * lname;
    const char * tstr;
    const char * hint;
    size_t nleft;

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
        snprintf(dettxt, sizeof(dettxt), "Anim %d", (int)((-(*mymappt))/(int)sizeof(ANISTR)-1));
    } else if (*mymappt >= numblockstr) {
        snprintf(dettxt, sizeof(dettxt), "Block %d", (*mymappt));
    } else {
        myblkstrpt = (BLKSTR *) (blockstrpt + (((int)(*mymappt))*sizeof(BLKSTR)));
        memset(scbits, 0, 16);
        if (myblkstrpt->tl) scbits[0] = '1'; else scbits[0] = '0';
        if (myblkstrpt->tr) scbits[1] = '1'; else scbits[1] = '0';
        if (myblkstrpt->bl) scbits[2] = '1'; else scbits[2] = '0';
        if (myblkstrpt->br) scbits[3] = '1'; else scbits[3] = '0';
        if (myblkstrpt->trigger) scbits[4] = '1'; else scbits[4] = '0';
        if (myblkstrpt->unused1) scbits[5] = '1'; else scbits[5] = '0';
        if (myblkstrpt->unused2) scbits[6] = '1'; else scbits[6] = '0';
        if (myblkstrpt->unused3) scbits[7] = '1'; else scbits[7] = '0';
        if (myblkstrpt->trigger) scbits[9] = '1'; else scbits[9] = '0';
        snprintf(dettxt, sizeof(dettxt), "Block %d (BG=%d, FG=%d, Bits=%s, T=%s)",
            (*mymappt),
            (int) myblkstrpt->bgoff/(blockwidth*blockheight*((blockdepth+1)/8)),
            (int) myblkstrpt->fgoff/(blockwidth*blockheight*((blockdepth+1)/8)),
            scbits, scbits+9);
    }

    nlayers = 0; for (li=0;li<MAXMAPLAYERS;li++) if (GetLayerpt(li)!=NULL) nlayers++;

    /* tbarinfo titlebar modes (mapwin.ini) */
    switch (tbarinfo) {
    case 0:
        snprintf(temptext, sizeof(temptext), "Map Editor");
        break;
    case 3:
        if (*mymappt<0) snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d, Layer %d/%d, %s",
            x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, dettxt);
        else snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d, Layer %d/%d, Block %d",
            x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, (*mymappt));
        break;
    case 4:
        snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d, Layer %d/%d, %s",
            x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, dettxt);
        break;
    case 5:
        tstr = F14_TstrGet(0);
        if (tstr==NULL) tstr = "";
        if (*mymappt>=0)
            snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d, Layer %d/%d, Block %d [%s]",
            x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, (*mymappt), tstr);
        else
            snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d, Layer %d/%d, %s [%s]",
            x, mapwidth-1, y, mapheight-1, maplayer, nlayers ? nlayers-1 : 0, dettxt, tstr);
        break;
    case 6:
        lname = F14_LayerName(maplayer);
        if (lname==NULL) lname = "";
        tstr = F14_TstrGet(0);
        if (tstr==NULL) tstr = "";
        snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d, [%s] %d/%d, %s [%s]",
            x, mapwidth-1, y, mapheight-1, lname, maplayer, nlayers ? nlayers-1 : 0, dettxt, tstr);
        break;
    case 2:
        snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d. %s",
            x, mapwidth-1, y, mapheight-1, dettxt);
        break;
    default: /* 1 */
        if (*mymappt<0) snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d. %s",
            x, mapwidth-1, y, mapheight-1, dettxt);
        else snprintf(temptext, sizeof(temptext), "X %d/%d, Y %d/%d. Block %d",
            x, mapwidth-1, y, mapheight-1, (*mymappt));
        break;
    }

    /* Tier-2 tool help: show active L/D/P mode in title (keys L/P/D, ESC cancels) */
    hint = NULL;
    if (linemode) hint = " [LINE L: click end, ESC cancels]";
    else if (pathmode) hint = " [PATH P: click next, ESC ends]";
    else if (dragmode) hint = " [DRAG D: move mouse, D/click ends]";
    if (hint) {
        nleft = strlen(temptext);
        if (nleft < sizeof(temptext) - 1)
            strncat(temptext, hint, sizeof(temptext) - 1 - nleft);
    }

    SetWindowText(mhwnd, temptext);
    meoldbx = x; meoldby = y;
}

void MEBrushRect(HWND hwnd, int xoff, int yoff, int xoff2, int yoff2) {
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
    myhdc = GetDC(hwnd);
    oldrop = SetROP2(myhdc, R2_NOT);
    oldbrush = SelectObject(myhdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(myhdc, dragboxrect.left, dragboxrect.top, dragboxrect.right,
        dragboxrect.bottom);
    SelectObject(myhdc, oldbrush);
    SetROP2(myhdc, oldrop);
    ReleaseDC(hwnd, myhdc);
}

void MEDrawUnderTile(HWND hwnd, int xoff, int yoff) {
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
    destmempt = malloc(bwpitch*blockheight);
    if (destmempt==NULL) return;

    myhdc = GetDC(hwnd);
    if (myhdc==NULL) { free(destmempt); return; }
    fromhdc = CreateCompatibleDC(myhdc);
    if (fromhdc==NULL) { ReleaseDC(hwnd, myhdc); free(destmempt); return; }

    memset(&bih, 0, sizeof(BITMAPINFOHEADER));
    memset(&rgbq, 0, sizeof(RGBQUAD));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = blockwidth;
    bih.biHeight = -blockheight;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    Blockto24(xoff, yoff, destmempt, 0, 0, bwpitch);

    temphbm = CreateDIBitmap(myhdc, &bih, CBM_INIT,
        destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
    if (temphbm==NULL) { DeleteDC(fromhdc); ReleaseDC(hwnd, myhdc); free(destmempt); return; }

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
*/				BitBlt(myhdc, (xoff-xmapoffset)*blockwidth, (yoff-ymapoffset)*blockheight,
        blockwidth, blockheight, fromhdc, 0, 0, SRCCOPY);
        //			}
    } else {
        StretchBlt(myhdc, ZX((xoff-xmapoffset)*blockwidth), ZX((yoff-ymapoffset)*blockheight),
            ZX(blockwidth), ZX(blockheight), fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
    }

    SelectObject(fromhdc, hbmpold);
    DeleteObject(temphbm);

    DeleteDC(fromhdc);
    ReleaseDC(hwnd, myhdc);
    free(destmempt);
}

/* onion backdrop: single layer, All layers, or 0..layer (show lower layers) */
static void MEOnionBg(int x, int y, unsigned char * dst, int xo, int yo, int ls) {
    if (onion==ONION_ALL) OnionCompto24(x, y, dst, xo, yo, ls, MAXMAPLAYERS-1);
    else if (onionlower) OnionCompto24(x, y, dst, xo, yo, ls, onion-1);
    else Onionto24(x, y, dst, xo, yo, ls, onion-1);
}

/* software stretch (zmstyle=0): nearest-neighbour, no StretchBlt */
static void MESoftStretch(HDC dst, int dx, int dy, int dw, int dh,
    HDC src, int sx, int sy, int sw, int sh) {
    int x, y;
    COLORREF c;

    if (dw<=0 || dh<=0 || sw<=0 || sh<=0) return;
    for (y=0;y<dh;y++) {
        for (x=0;x<dw;x++) {
            c = GetPixel(src, sx + (x*sw)/dw, sy + (y*sh)/dh);
            SetPixel(dst, dx+x, dy+y, c);
        }
    }
}

void MEDrawMap(HWND phwnd, HDC myhdc) {
    int i, j, k, l, bwpitch;
    HDC fromhdc;
    HBITMAP temphbm, hbmpold;
    unsigned long int * darkenpt;
    unsigned char * destmempt;
    BITMAPINFOHEADER bih;
    RGBQUAD rgbq;
    RECT rc;

    GetClientRect(phwnd, &rc);
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
        destmempt = malloc(nbytes);
    }
    if (destmempt==NULL) return;

    fromhdc = CreateCompatibleDC(myhdc);
    if (fromhdc==NULL) { free(destmempt); return; }

    memset(&bih, 0, sizeof(BITMAPINFOHEADER));
    memset(&rgbq, 0, sizeof(RGBQUAD));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = k*blockgapx;
    bih.biHeight = -(l*blockgapy);
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    //		k--; l--;
    for (j=0;j<l;j++) {
        for (i=0;i<k;i++) {
            if (blockstaggerx || blockstaggery) {
                if (onion) MEOnionBg(xmapoffset+i, ymapoffset+(j*2), destmempt, i*blockgapx, j*blockgapy, bwpitch);
                else Overlayto24(xmapoffset+i, ymapoffset+(j*2), destmempt, i*blockgapx, j*blockgapy, bwpitch, -1);
            } else {
                if (onion) MEOnionBg(xmapoffset+i, ymapoffset+j, destmempt, i*blockgapx, j*blockgapy, bwpitch);
                else Blockto24(xmapoffset+i, ymapoffset+j, destmempt, i*blockgapx, j*blockgapy, bwpitch);
            }
        }
        for (i=0;i<k;i++) {
            if (blockstaggerx || blockstaggery) {
                if (onion) MEOnionBg(xmapoffset+i, ymapoffset+(j*2+1), destmempt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch);
                else Overlayto24(xmapoffset+i, ymapoffset+(j*2+1), destmempt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, -1);
            }
        }
    }

    if (onion) { if (oniondark) {
            i = (l*bwpitch*blockheight)/4; darkenpt = (unsigned long int *) destmempt; while (i) {
                *darkenpt = ((*darkenpt)>>1)&0x7F7F7F7F; darkenpt++; i--;
        } }
        for (j=0;j<l;j++)
        {	for (i=0;i<k;i++) {
                if (blockstaggerx || blockstaggery) {
                    Overlayto24(xmapoffset+i, ymapoffset+(j*2), destmempt, i*blockgapx, j*blockgapy, bwpitch, -1);
                } else {
                    Overlayto24(xmapoffset+i, ymapoffset+j, destmempt, i*blockgapx, j*blockgapy, bwpitch, -1);
                }
            }
            for (i=0;i<k;i++) {
                if (blockstaggerx || blockstaggery) {
                    Overlayto24(xmapoffset+i, ymapoffset+(j*2+1), destmempt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, -1);
                }
            }
    } }

    temphbm = CreateDIBitmap(myhdc, &bih, CBM_INIT,
        destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
    if (temphbm==NULL) { DeleteDC(fromhdc); free(destmempt); return; }

    hbmpold = SelectObject(fromhdc, temphbm);
    if (zoomon==1 && zoomdiv==1)
        BitBlt(myhdc, 0, 0, k*blockgapx, l*blockgapy, fromhdc, blockstaggerx, blockstaggery, SRCCOPY);
    else if (zmstyle==0)
        MESoftStretch(myhdc, 0, 0, ZX((k*blockgapx)-blockstaggerx), ZX((l*blockgapy)-blockstaggery),
        fromhdc, blockstaggerx, blockstaggery, k*blockgapx-blockstaggerx, l*blockgapy-blockstaggery);
    else
        StretchBlt(myhdc, 0, 0, ZX((k*blockgapx)-blockstaggerx), ZX((l*blockgapy)-blockstaggery), fromhdc, blockstaggerx, blockstaggery, k*blockgapx-blockstaggerx, l*blockgapy-blockstaggery, SRCCOPY);
    SelectObject(fromhdc, hbmpold);
    DeleteObject(temphbm);

    if (wnesmode) {
        /* 3D-wall mode: red vertical wall markers at tile left edges */
        RECT wrc; HPEN wpen, wold; int wx, wy0, wy1;
        GetClientRect(phwnd, &wrc);
        wpen = CreatePen(PS_SOLID, MapPenWidth(phwnd), RGB(255, 0, 0));
        wold = SelectObject(myhdc, wpen);
        for (j=0;j<=k;j++) {
            wx = ZX(j*blockgapx);
            wy0 = 0; wy1 = wrc.bottom;
            MoveToEx(myhdc, wx, wy0, NULL);
            LineTo(myhdc, wx, wy1);
        }
        SelectObject(myhdc, wold);
        DeleteObject(wpen);
    }

    if (divideron && dividergapx >= 8 && dividergapy >= 8) {
        RECT crc; HPEN hpen, holdpen;
        int stepx = dividergapx * zoomon / zoomdiv, stepy = dividergapy * zoomon / zoomdiv;
        int ox = (xmapoffset * blockwidth * zoomon / zoomdiv) % stepx;
        int oy = (ymapoffset * blockheight * zoomon / zoomdiv) % stepy;
        int x, y;
        GetClientRect(phwnd, &crc);
        hpen = CreatePen(PS_SOLID, MapPenWidth(phwnd), RGB((dividercol>>16)&0xFF, (dividercol>>8)&0xFF, dividercol&0xFF));
        holdpen = SelectObject(myhdc, hpen);
        for (x = -ox; x <= crc.right; x += stepx) {
            MoveToEx(myhdc, x, 0, NULL);
            LineTo(myhdc, x, crc.bottom);
        }
        for (y = -oy; y <= crc.bottom; y += stepy) {
            MoveToEx(myhdc, 0, y, NULL);
            LineTo(myhdc, crc.right, y);
        }
        SelectObject(myhdc, holdpen);
        DeleteObject(hpen);
    }

    if (showuser>=1 && showuser<=8) {
        /* show user field / flags overlaying tiles (objects win) */
        char utxt[64]; int vx, vy, ti, tj; short int bv; BLKSTR * bp;
        HGDIOBJ holdf = NULL;
        if (me_font == NULL) MEEnsureUiFont(phwnd);
        if (me_font) holdf = SelectObject(myhdc, me_font);
        SetBkMode(myhdc, TRANSPARENT);
        SetTextColor(myhdc, RGB(255, 255, 0));
        for (tj=0;tj<l;tj++) {
            for (ti=0;ti<k;ti++) {
                vx = xmapoffset+ti; vy = ymapoffset+tj;
                if (vx<0 || vy<0 || vx>=mapwidth || vy>=mapheight) continue;
                if (OE_ObjOverlayText(vx, vy, utxt, sizeof(utxt))) {
                    TextOut(myhdc, ZX(ti*blockgapx), ZX(tj*blockgapy), utxt, strlen(utxt));
                    continue;
                }
                bv = mappt[vx+vy*mapwidth];
                if (bv<0 || bv>=numblockstr) continue;
                bp = (BLKSTR *) (blockstrpt + (((int) bv)*sizeof(BLKSTR)));
                switch (showuser) {
                case 1: sprintf(utxt, "%lu", bp->user1); break;
                case 2: sprintf(utxt, "%lu", bp->user2); break;
                case 3: sprintf(utxt, "%u", bp->user3); break;
                case 4: sprintf(utxt, "%u", bp->user4); break;
                case 5: sprintf(utxt, "%u", bp->user5); break;
                case 6: sprintf(utxt, "%u", bp->user6); break;
                case 7: sprintf(utxt, "%u", bp->user7); break;
                default: sprintf(utxt, "%d%d%d%d%d", bp->tl, bp->tr, bp->bl, bp->br, bp->trigger); break;
                }
                if (showuser >= 1 && showuser <= 7 && showuser == LuaGetStrBlkUser()) {
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
                    ts = F14_TstrGet((int) sv);
                    if (ts && ts[0]) {
                        strncpy(utxt, ts, sizeof(utxt) - 1);
                        utxt[sizeof(utxt) - 1] = 0;
                    }
                }
                TextOut(myhdc, ZX(ti*blockgapx), ZX(tj*blockgapy), utxt, strlen(utxt));
            }
        }
        if (holdf) SelectObject(myhdc, holdf);
    }

    {
        RECT oeclient;
        GetClientRect(phwnd, &oeclient);
        OE_DrawObjects(myhdc, &oeclient);
    }

    DeleteDC(fromhdc);
    free(destmempt);
}

void MEDrawGrid(HWND hwnd, int xoff, int yoff) {
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
    destmempt = malloc(bwpitch*blockheight);
    if (destmempt==NULL) return;

    myhdc = GetDC(hwnd);
    if (myhdc==NULL) { free(destmempt); return; }
    fromhdc = CreateCompatibleDC(myhdc);
    if (fromhdc==NULL) { ReleaseDC(hwnd, myhdc); free(destmempt); return; }

    memset(&bih, 0, sizeof(BITMAPINFOHEADER));
    memset(&rgbq, 0, sizeof(RGBQUAD));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = blockwidth;
    bih.biHeight = -blockheight;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    if (blockstaggerx || blockstaggery) {
        MEDrawMap(hwnd, myhdc);
    } else {
        if (onion) MEOnionBg(meoldbx, meoldby, destmempt, 0, 0, bwpitch);
        else Blockto24(meoldbx, meoldby, destmempt, 0, 0, bwpitch);
        if (oniondark) {
            i = bwpitch*blockheight; darkenpt = destmempt; while (i) {
                *darkenpt = ((*darkenpt)>>1)&0x7F; darkenpt++; i--;
        } }
        if (onion) Overlayto24(meoldbx, meoldby, destmempt, 0, 0, bwpitch, -1);

        temphbm = CreateDIBitmap(myhdc, &bih, CBM_INIT,
            destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
        if (temphbm==NULL) { DeleteDC(fromhdc); ReleaseDC(hwnd, myhdc); free(destmempt); return; }
        x = ZX((meoldbx-xmapoffset)*blockwidth);
        y = ZX((meoldby-ymapoffset)*blockheight);
        hbmpold = SelectObject(fromhdc, temphbm);
        if (zoomon==1 && zoomdiv==1)
            BitBlt(myhdc, x, y,	blockwidth, blockheight, fromhdc, 0, 0, SRCCOPY);
        else
            StretchBlt(myhdc, x, y,
            ZX(blockwidth), ZX(blockheight), fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
        SelectObject(fromhdc, hbmpold);
        DeleteObject(temphbm);
    }

    if (xoff<0 || xoff>=mapwidth || yoff<0 || yoff>=mapheight) {
        DeleteDC(fromhdc);
        ReleaseDC(hwnd, myhdc);
        free(destmempt);
        return;
    }

    if (!(blockstaggerx || blockstaggery)) {
        if (gridstyle == 2) {
            /* crosshatch grid: draw transparent-pixel checkered pattern */
            Blockto24(xoff, yoff, destmempt, 0, 0, bwpitch);
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
            Blockto24(xoff, yoff, destmempt, 0, 0, bwpitch);
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
        Blockto24(xoff, yoff, destmempt, 0, 0, bwpitch);
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

    temphbm = CreateDIBitmap(myhdc, &bih, CBM_INIT,
        destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
    if (temphbm==NULL) { DeleteDC(fromhdc); ReleaseDC(hwnd, myhdc); free(destmempt); return; }

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
        BitBlt(myhdc, x, y,	blockwidth, blockheight, fromhdc, 0, 0, blttype);
    else
        StretchBlt(myhdc, x, y,
        ZX(blockwidth), ZX(blockheight), fromhdc, 0, 0, blockwidth, blockheight, blttype);

    SelectObject(fromhdc, hbmpold);
    DeleteObject(temphbm);

    DeleteDC(fromhdc);
    ReleaseDC(hwnd, myhdc);
    free(destmempt);
}

/* Custom mousebuttons (mapwin.ini): 0 None, 1 Current block, 2 Block 0,
 * 3 Drag map 1, 4 Drag map 2, 5 Placer.lua, 6 Pick block, 7 Lay path,
 * 8 Fill, 9 Line, 10 Area.lua, 100+ CM_ command. List per binary table.
 * Returns 1 if handled (caller must swallow the click), 0 to continue. */
static int rmapped = 0;
static int MBDoAction(int act, HWND hwnd, LPARAM lParam) {
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
        if (act==4) InvalidateRgn(mhwnd, NULL, FALSE);
        return 1;
    case 5: /* Placer.lua — run on click and while dragged */
        { int bx = (LOWORD(lParam)) / blockgapx + xmapoffset;
            int by = (HIWORD(lParam)) / blockgapy + ymapoffset;
            lua_mb_act = 5;
            LuaSetMouseBlock(bx, by, bx, by);
            LuaRunMouseButton(act, bx, by); }
        return 1;
    case 10: /* Area.lua — capture drag, run on button up */
        { int bx = (LOWORD(lParam)) / blockgapx + xmapoffset;
            int by = (HIWORD(lParam)) / blockgapy + ymapoffset;
            lua_mb_act = 10;
            lua_area_x = bx; lua_area_y = by;
            LuaSetMouseBlock(bx, by, bx, by); }
        return 1;
    case 6: /* Pick block */
        MEPickBlock();
        return 1;
    case 7: /* Lay path */
        if (pathmode) pathmode = 0; else { pathmode = 1; linemode = 0; }
        return 1;
    case 8: /* Fill */
        MEFillMap(0);
        return 1;
    case 9: /* Line */
        if (linemode) linemode = 0; else { linemode = 1; pathmode = 0; }
        return 1;
    default:
        if (act>=400) {
            int bx = (LOWORD(lParam)) / blockgapx + xmapoffset;
            int by = (HIWORD(lParam)) / blockgapy + ymapoffset;
            LuaSetMouseBlock(bx, by, bx, by);
            LuaRunMouseButton(act, bx, by);
            return 1;
        }
        if (act>=100) {
            SendMessage(GetParent(hwnd), WM_COMMAND, act, 0);
            return 1;
        }
        return 0;
    }
}

/* ---- Tier-2 helpers ---- */

static void MEBresenhamLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2;

    for (;;) {
        curplacetile = -1;
        if (x0>=0 && x0<mapwidth && y0>=0 && y0<mapheight) {
            short int * mpt = mappt + x0 + y0*mapwidth;
            if (curbrsh>0 && curbrsh<=MAXBRSH) {
                PlaceTile(x0, y0);
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

static void MEDragMap(int xpix, int ypix) {
    int dx = ZD(xpix - dragox);
    int dy = ZD(ypix - dragoy);
    int nx = dragoxoff - dx;
    int ny = dragoyoff - dy;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > mapwidth-1) nx = mapwidth-1;
    if (ny > mapheight-1) ny = mapheight-1;
    xmapoffset = nx;
    ymapoffset = ny;
    SetScrollPos(mhwnd, SB_HORZ, xmapoffset, TRUE);
    SetScrollPos(mhwnd, SB_VERT, ymapoffset, TRUE);
    InvalidateRgn(mhwnd, NULL, FALSE);
}

static void MEDragFinish(void) {
    dragmode = 0;
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

/* ---- Graphic editor mode (Graph tab) ---- */
/* Graph tab: tile on the left, native toolbar along the bottom, optional
 * D_PALEDIT child dialog on the right (panelpro). */

typedef struct {
    int pw, ph;
    int tilex, tiley, tilew, tileh;	/* stretched tile preview */
    int showpanel;
} GELayout;

static int GE_lastpx = 0, GE_lastpy = 0, GE_haslast = 0;
static int GE_mx = 0, GE_my = 0;
static int GE_linemode = 0, GE_linex = 0, GE_liney = 0;
static unsigned char * GE_undo = NULL;
static int GE_undogfx = -1, GE_undolen = 0;
static GELayout gelay;

static int GE_FindBlockForGraphic(int gfx) {
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

static void GEComputeLayout(HWND hwnd, GELayout * L) {
    RECT rc;
    int toolstrip, palw, ts;
    GetClientRect(hwnd, &rc);
    L->pw = rc.right - rc.left;
    L->ph = rc.bottom - rc.top;
    if (L->pw < MapDip(hwnd, 80)) L->pw = MapDip(hwnd, 80);
    if (L->ph < MapDip(hwnd, 80)) L->ph = MapDip(hwnd, 80);

    toolstrip = GEToolbarHeight(hwnd);
    L->showpanel = (panelpro != 0);
    palw = 0;
    if (L->showpanel) {
        palw = GEPaletteWidth(hwnd);
        if (palw > L->pw / 2) palw = L->pw / 2;
    }

    L->tilex = 0;
    L->tiley = 0;
    L->tilew = L->pw - palw - MapDip(hwnd, 8);
    L->tileh = L->ph - toolstrip;
    if (L->tilew < MapDip(hwnd, 8)) L->tilew = MapDip(hwnd, 8);
    if (L->tileh < MapDip(hwnd, 8)) L->tileh = MapDip(hwnd, 8);
    ts = L->tilew < L->tileh ? L->tilew : L->tileh;
    L->tilew = ts;
    L->tileh = ts;
}

static void GEGetColRGB(int * r, int * g, int * b) {
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

static void GESetColChan(int chan, int v) {
    int i = GE_gfxcol;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    if (i < 0) i = 0;
    if (i > 255) i = 255;
    if (cmappt == NULL) return;
    cmappt[i * 3 + chan] = (unsigned char) v;
    mapaltered = 1;
}

static int GETileBytes(void) {
    int bpp = (blockdepth + 1) / 8;
    if (bpp < 1) bpp = 1;
    return blockwidth * blockheight * bpp;
}

static unsigned char * GETilePtr(void) {
    if (blockgfxpt == NULL || curgfxsel < 0) return NULL;
    return (unsigned char *) blockgfxpt + curgfxsel * GETileBytes();
}

static void GEPushUndo(void) {
    int tb = GETileBytes();
    unsigned char * src = GETilePtr();
    if (src == NULL || tb <= 0) return;
    if (GE_undo == NULL || GE_undolen != tb) {
        free(GE_undo);
        GE_undo = malloc((size_t) tb);
        GE_undolen = tb;
    }
    if (GE_undo == NULL) return;
    memcpy(GE_undo, src, (size_t) tb);
    GE_undogfx = curgfxsel;
}

void GEUndo(void) {
    unsigned char * src;
    int tb;
    if (betype != BETYPE_GRAPH) return;
    src = GETilePtr();
    tb = GETileBytes();
    if (src == NULL || GE_undo == NULL || GE_undogfx != curgfxsel || GE_undolen != tb)
        return;
    {
        unsigned char * tmp = malloc((size_t) tb);
        if (tmp == NULL) return;
        memcpy(tmp, src, (size_t) tb);
        memcpy(src, GE_undo, (size_t) tb);
        memcpy(GE_undo, tmp, (size_t) tb);
        free(tmp);
    }
    mapaltered = 1;
    GEInvalidate();
    BEInvalidate();
}

static void GEWritePix(unsigned char * tile, int x, int y, int col, int r, int g, int b) {
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

static void GEPlot(int px, int py) {
    int dx, dy, r, g, b, t;
    unsigned char * tile = GETilePtr();
    if (tile == NULL) return;
    GEGetColRGB(&r, &g, &b);
    t = GE_thickness;
    if (t < 1) t = 1;
    for (dy = 0; dy < t; dy++) {
        for (dx = 0; dx < t; dx++)
            GEWritePix(tile, px + dx, py + dy, GE_gfxcol, r, g, b);
    }
    GE_lastpx = px; GE_lastpy = py; GE_haslast = 1;
}

static void GELine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        GEPlot(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void GEFill(int px, int py) {
    int bpp, n, sp, r, g, b;
    unsigned char * tile, * seen, tgt[4], cur[4];
    int * st;
    if (px < 0 || py < 0 || px >= blockwidth || py >= blockheight) return;
    tile = GETilePtr();
    if (tile == NULL) return;
    bpp = (blockdepth + 1) / 8;
    if (bpp < 1) bpp = 1;
    if (bpp > 4) bpp = 4;
    memcpy(tgt, tile + (py * blockwidth + px) * bpp, (size_t) bpp);
    GEGetColRGB(&r, &g, &b);
    {
        unsigned char tmp[16];
        memset(tmp, 0, sizeof(tmp));
        GEWritePix(tmp, 0, 0, GE_gfxcol, r, g, b);
        memcpy(cur, tmp, (size_t) bpp);
    }
    if (memcmp(tgt, cur, (size_t) bpp) == 0) return;
    n = blockwidth * blockheight;
    seen = calloc((size_t) n, 1);
    st = malloc((size_t) n * 2 * sizeof(int));
    if (seen == NULL || st == NULL) { free(seen); free(st); return; }
    sp = 0;
    st[sp++] = px; st[sp++] = py;
    seen[py * blockwidth + px] = 1;
    while (sp > 0) {
        int y = st[--sp];
        int x = st[--sp];
        int k;
        GEWritePix(tile, x, y, GE_gfxcol, r, g, b);
        for (k = 0; k < 4; k++) {
            static const int ox[4] = { 1, -1, 0, 0 };
            static const int oy[4] = { 0, 0, 1, -1 };
            int nx = x + ox[k], ny = y + oy[k];
            if (nx < 0 || ny < 0 || nx >= blockwidth || ny >= blockheight) continue;
            if (seen[ny * blockwidth + nx]) continue;
            if (memcmp(tile + (ny * blockwidth + nx) * bpp, tgt, (size_t) bpp) != 0)
                continue;
            seen[ny * blockwidth + nx] = 1;
            st[sp++] = nx; st[sp++] = ny;
        }
    }
    free(seen);
    free(st);
    GE_lastpx = px; GE_lastpy = py; GE_haslast = 1;
}

static int GEIsTranspPixel(unsigned char * p) {
    if (blockdepth == 8)
        return *p == trans8bit;
    if (blockdepth == 15 || blockdepth == 16)
        return p[0] == trans16hi && p[1] == trans16low;
    if (blockdepth == 32)
        return p[1] == transred && p[2] == transgreen && p[3] == transblue;
    return p[0] == transblue && p[1] == transgreen && p[2] == transred;
}

static void GEMaskWithClickmask(void) {
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
        if (GEIsTranspPixel(msk + i * bpp))
            memset(src + i * bpp, 0, (size_t) bpp);
    }
}

static void GEReadPixRGB(unsigned char * tile, int px, int py, int * r, int * g, int * b) {
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

static int GENearestPal(int r, int g, int b) {
    int i, best = 0, bestd = 0x7fffffff;
    if (cmappt == NULL) return 0;
    for (i = 0; i < 256; i++) {
        int dr = cmappt[i * 3] - r, dg = cmappt[i * 3 + 1] - g, db = cmappt[i * 3 + 2] - b;
        int d = dr * dr + dg * dg + db * db;
        if (d < bestd) { bestd = d; best = i; }
    }
    return best;
}

static void GEPickAt(int px, int py, int findpal) {
    unsigned char * tile;
    int r, g, b;
    if (px < 0 || py < 0 || px >= blockwidth || py >= blockheight) return;
    tile = GETilePtr();
    if (tile == NULL) return;
    if (blockdepth == 8) {
        int bpp = 1;
        GE_gfxcol = tile[(py * blockwidth + px) * bpp];
        return;
    }
    GEReadPixRGB(tile, px, py, &r, &g, &b);
    if (findpal)
        GE_gfxcol = GENearestPal(r, g, b);
    else if (cmappt != NULL && GE_gfxcol >= 0 && GE_gfxcol < 256) {
        cmappt[GE_gfxcol * 3] = (unsigned char) r;
        cmappt[GE_gfxcol * 3 + 1] = (unsigned char) g;
        cmappt[GE_gfxcol * 3 + 2] = (unsigned char) b;
        mapaltered = 1;
    }
}

static void GEPaletteOp(int dest) {
    int s = GE_palsrc, d = dest;
    if (!cmappt || s < 0 || d < 0 || s > 255 || d > 255) { GE_palop = 0; return; }
    if (GE_palop == 1) {
        memcpy(cmappt + d * 3, cmappt + s * 3, 3);
    } else if (GE_palop == 2) {
        unsigned char tmp[3];
        memcpy(tmp, cmappt + d * 3, 3);
        memcpy(cmappt + d * 3, cmappt + s * 3, 3);
        memcpy(cmappt + s * 3, tmp, 3);
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

static void GEDoTool(int t) {
    int bpp, tilebytes, y;
    unsigned char * src;
    if (blockgfxpt == NULL || curgfxsel < 0) return;
    bpp = (blockdepth + 1) / 8;
    if (bpp < 1) bpp = 1;
    tilebytes = blockwidth * blockheight * bpp;
    src = (unsigned char *) blockgfxpt + curgfxsel * tilebytes;
    if (t >= 0 && t <= 4) GEPushUndo();
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
            unsigned char * tmp = malloc((size_t) tilebytes);
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
                memcpy(src, tmp, (size_t) tilebytes);
                free(tmp);
            }
        }
    } else if (t == 3) { /* CLR: fill with current colour */
        int x, r, g, b;
        GEGetColRGB(&r, &g, &b);
        for (y = 0; y < blockheight; y++)
            for (x = 0; x < blockwidth; x++)
                GEWritePix(src, x, y, GE_gfxcol, r, g, b);
    } else if (t == 4) { /* MSK */
        GEMaskWithClickmask();
    } else if (t == 5) { /* BLK */
        int blk = GE_FindBlockForGraphic(curgfxsel);
        if (blk >= 0) curstr = (short int) blk;
    } else if (t == 6) { /* REV: undo last draw */
        GEUndo();
        return;
    }
    mapaltered = 1;
}

static void MEDrawGraphicEditor(HWND phwnd, HDC myhdc) {
    GELayout * L = &gelay;
    HDC fromhdc;
    HBITMAP temphbm, hbmpold;
    unsigned char * tb;
    BITMAPINFOHEADER bih;
    int pitch;
    RECT prc;

    GEComputeLayout(phwnd, L);

    prc.left = 0; prc.top = 0; prc.right = L->pw; prc.bottom = L->ph;
    {
        COLORREF bg = prographbg ? RGB(0x40, 0x40, 0x40) : RGB(0, 0, 0);
        HBRUSH hbr = CreateSolidBrush(bg);
        FillRect(myhdc, &prc, hbr);
        DeleteObject(hbr);
    }

    if (blockgfxpt == NULL || numblockgfx <= 0) {
        HGDIOBJ holdf = NULL;
        if (ge_font) holdf = SelectObject(myhdc, ge_font);
        SetBkColor(myhdc, RGB(0, 0, 0));
        SetTextColor(myhdc, RGB(255, 255, 255));
        TextOut(myhdc, MapDip(phwnd, 8), MapDip(phwnd, 8), "No graphics loaded", 18);
        if (holdf) SelectObject(myhdc, holdf);
        return;
    }
    if (curgfxsel < 0) curgfxsel = 0;
    if (curgfxsel >= numblockgfx) curgfxsel = numblockgfx - 1;

    pitch = ((blockwidth * 3) + 3) & ~3;
    tb = malloc((size_t) pitch * blockheight);
    if (tb == NULL) return;
    memset(tb, 0, (size_t) pitch * blockheight);
    BlockGfxto24(curgfxsel, tb, 0, 0, pitch);

    fromhdc = CreateCompatibleDC(myhdc);
    if (fromhdc==NULL) { free(tb); return; }
    memset(&bih, 0, sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biWidth = blockwidth;
    bih.biHeight = -blockheight;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    temphbm = CreateDIBitmap(myhdc, &bih, CBM_INIT, tb, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
    if (temphbm==NULL) { DeleteDC(fromhdc); free(tb); return; }
    hbmpold = SelectObject(fromhdc, temphbm);
    StretchBlt(myhdc, L->tilex, L->tiley, L->tilew, L->tileh,
        fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
    SelectObject(fromhdc, hbmpold);
    DeleteObject(temphbm);
    DeleteDC(fromhdc);
    free(tb);

    {
        HPEN hpen, hold;
        int gx, gy;
        hpen = CreatePen(PS_SOLID, MapPenWidth(phwnd), RGB(0xC0, 0xC0, 0xC0));
        hold = SelectObject(myhdc, hpen);
        for (gx = 1; gx < blockwidth; gx++) {
            int sx = L->tilex + gx * L->tilew / blockwidth;
            MoveToEx(myhdc, sx, L->tiley, NULL);
            LineTo(myhdc, sx, L->tiley + L->tileh);
        }
        for (gy = 1; gy < blockheight; gy++) {
            int sy = L->tiley + gy * L->tileh / blockheight;
            MoveToEx(myhdc, L->tilex, sy, NULL);
            LineTo(myhdc, L->tilex + L->tilew, sy);
        }
        SelectObject(myhdc, hold);
        DeleteObject(hpen);
    }

    GEUpdateStatus();
}

static int GETileXY(int mx, int my, int * px, int * py) {
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

static int GEDoTileAction(HWND hwnd, int act, int px, int py) {
    if (act == 0) return 1;
    if (act == 2) { GE_gfxcol = 0; GERefreshPalette(); }
    if (act == 6) {
        GEPickAt(px, py, 1);
        InvalidateRect(hwnd, NULL, FALSE);
        GERefreshPalette();
        return 1;
    }
    if (act == 8) {
        GEPushUndo();
        GEFill(px, py);
        mapaltered = 1;
        InvalidateRect(hwnd, NULL, FALSE);
        BEInvalidate();
        return 1;
    }
    if (act == 9) {
        if (!GE_linemode) {
            GE_linemode = 1;
            GE_linex = px; GE_liney = py;
        } else {
            GEPushUndo();
            GELine(GE_linex, GE_liney, px, py);
            GE_linemode = 0;
            mapaltered = 1;
            BEInvalidate();
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 1;
    }
    if (act != 1 && act != 2) return 0;
    GEPushUndo();
    GE_paint = 1;
    SetCapture(hwnd);
    GEPlot(px, py);
    mapaltered = 1;
    InvalidateRect(hwnd, NULL, FALSE);
    BEInvalidate();
    return 1;
}

static int GEOnLButtonDown(HWND hwnd, int mx, int my) {
    int px, py;
    GEComputeLayout(hwnd, &gelay);
    GE_mx = mx; GE_my = my;

    if (GETileXY(mx, my, &px, &py)) {
        if (GE_palop == 4) {
            GEPickAt(px, py, GetKeyState(VK_CONTROL) < 0);
            GE_palop = 0;
            PalSyncButtons();
            InvalidateRect(hwnd, NULL, FALSE);
            GERefreshPalette();
            return 1;
        }
        if (GetKeyState(VK_CONTROL) < 0) {
            GEPickAt(px, py, 1);
            InvalidateRect(hwnd, NULL, FALSE);
            GERefreshPalette();
            return 1;
        }
        GEDoTileAction(hwnd, mb_l, px, py);
        return 1;
    }
    return 1;
}

static int GEOnRButtonDown(HWND hwnd, int mx, int my) {
    int px, py;
    GEComputeLayout(hwnd, &gelay);
    GE_mx = mx; GE_my = my;
    if (GETileXY(mx, my, &px, &py)) {
        GEDoTileAction(hwnd, mb_r, px, py);
        return 1;
    }
    return 1;
}

static int GEOnMouseMove(HWND hwnd, int mx, int my) {
    int px, py;
    GE_mx = mx; GE_my = my;
    if (GE_paint) {
        GEComputeLayout(hwnd, &gelay);
        if (GETileXY(mx, my, &px, &py)) {
            GEPlot(px, py);
            mapaltered = 1;
            InvalidateRect(hwnd, NULL, FALSE);
            BEInvalidate();
        }
        return 1;
    }
    return 0;
}

static void GEOnLButtonUp(HWND hwnd) {
    if (GE_paint) {
        GE_paint = 0;
        ReleaseCapture();
        InvalidateRect(hwnd, NULL, FALSE);
        BEInvalidate();
    }
}

int GESlide(int dx, int dy) {
    int bpp, x, y, tb;
    unsigned char * src, * tmp;
    if (betype != BETYPE_GRAPH) return 0;
    src = GETilePtr();
    if (src == NULL || (dx == 0 && dy == 0)) return 1;
    bpp = (blockdepth + 1) / 8;
    if (bpp < 1) bpp = 1;
    tb = GETileBytes();
    tmp = malloc((size_t) tb);
    if (tmp == NULL) return 1;
    GEPushUndo();
    memcpy(tmp, src, (size_t) tb);
    for (y = 0; y < blockheight; y++) {
        for (x = 0; x < blockwidth; x++) {
            int sx = (x - dx) % blockwidth;
            int sy = (y - dy) % blockheight;
            if (sx < 0) sx += blockwidth;
            if (sy < 0) sy += blockheight;
            memcpy(src + (y * blockwidth + x) * bpp,
                tmp + (sy * blockwidth + sx) * bpp, (size_t) bpp);
        }
    }
    free(tmp);
    mapaltered = 1;
    GEInvalidate();
    BEInvalidate();
    return 1;
}

static int GEMouseTile(int * px, int * py) {
    if (ghwnd == NULL) return 0;
    GEComputeLayout(ghwnd, &gelay);
    return GETileXY(GE_mx, GE_my, px, py);
}

int GEAccelFill(void) {
    int px, py;
    if (betype != BETYPE_GRAPH) return 0;
    if (!GEMouseTile(&px, &py)) return 1;
    GEPushUndo();
    GEFill(px, py);
    mapaltered = 1;
    GEInvalidate();
    BEInvalidate();
    return 1;
}

int GEAccelPick(void) {
    int px, py;
    if (betype != BETYPE_GRAPH) return 0;
    if (!GEMouseTile(&px, &py)) return 1;
    GEPickAt(px, py, 0);
    GEInvalidate();
    return 1;
}

int GEAccelLine(void) {
    int px, py;
    if (betype != BETYPE_GRAPH) return 0;
    if (!GEMouseTile(&px, &py) || !GE_haslast) return 1;
    GEPushUndo();
    GELine(GE_lastpx, GE_lastpy, px, py);
    mapaltered = 1;
    GEInvalidate();
    BEInvalidate();
    return 1;
}

/* ---- End Tier-2 helpers ---- */

LRESULT CALLBACK MapWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    //char dropname[256];
    int i, xblk, yblk;
    HDC myhdc;
    PAINTSTRUCT ps;

    switch (message) {
    case WM_PAINT:
        myhdc = BeginPaint(hwnd, &ps);
        if (blockgfxpt!=NULL)
            MEDrawMap(hwnd, myhdc);
        EndPaint(hwnd, &ps);
        //		ReleaseDC (hwnd, myhdc);
        break;
    case WM_DPICHANGED:
        MEEnsureUiFont(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case WM_LBUTTONDOWN:
        if (mappt==NULL) return 0;
        if (objplace) {
            /* object placement: click places selected, ctrl+click duplicates */
            int mpx = ZD(LOWORD(lParam)) + xmapoffset * blockwidth;
            int mpy = ZD(HIWORD(lParam)) + ymapoffset * blockheight;
            if (GetKeyState(VK_CONTROL) < 0) {
                const F14Obj * src = F14_GetObject(F14_CurObject());
                if (src != NULL) {
                    int ni = F14_NewObject();
                    if (ni >= 0) {
                        F14_SetObject(ni, src);
                        F14_SetCurObject(ni);
                    }
                }
            }
            MECopyToUndo();
            OE_PlaceSelected(mpx, mpy, GetKeyState(VK_SHIFT) < 0);
            mapaltered = 1;
            InvalidateRgn(mhwnd, NULL, FALSE);
            return 0;
        }
        if (MBDoAction(mb_l, hwnd, lParam)) return 0;
        xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
        yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
        if (linemode) {
            /* line tool: first click sets start, second draws */
            if (linemode==1) {
                linecx = xblk; linecy = yblk;
                linemode = 2;
            } else if (linemode==2) {
                MECopyToUndo();
                MEBresenhamLine(linecx, linecy, xblk, yblk);
                mapaltered = 1;
                linemode = 0;
                InvalidateRgn(mhwnd, NULL, FALSE);
            }
            MEShowCoords(xblk, yblk);
            return 0;
        }
        if (pathmode) {
            /* lay-path: click to place tiles along mouse path */
            if (pathmode==1) {
                pathcx = xblk; pathcy = yblk;
                MECopyToUndo();
                curplacetile = -1;
                PlaceTile(xblk, yblk);
                mapaltered = 1;
                if (blockstaggerx || blockstaggery) InvalidateRgn(mhwnd, NULL, FALSE);
                else MEDrawUnderTile(hwnd, xblk, yblk);
                pathmode = 2;
            } else if (pathmode==2) {
                MEBresenhamLine(pathcx, pathcy, xblk, yblk);
                pathcx = xblk; pathcy = yblk;
                mapaltered = 1;
                if (blockstaggerx || blockstaggery) InvalidateRgn(mhwnd, NULL, FALSE);
                else InvalidateRgn(mhwnd, NULL, FALSE);
            }
            MEShowCoords(xblk, yblk);
            return 0;
        }
        MECopyToUndo();
        curplacetile = -1; painton = 1;
        meoldbx = -1; meoldby = -1; mapaltered = 1;
        if (megetabrush) { gettingbrush = megetabrush; megetabrush = 0;
            xblk = MEGetX(LOWORD(lParam), HIWORD(lParam));
            yblk = MEGetY(LOWORD(lParam), HIWORD(lParam));
            if ((blockstaggerx || blockstaggery) && yblk&1) yblk--;
            MEBrushRect(hwnd, xblk, yblk, xblk, yblk);
            mebrushx = xblk; mebrushx2 = mebrushx;
            mebrushy = yblk; mebrushy2 = mebrushy; }
        /* first paint click also updates the hover cell */
        __attribute__((fallthrough));
    case WM_MOUSEMOVE:
        if (mappt==NULL) return 0;
        if (dragmode) {
            MEDragMap(LOWORD(lParam), HIWORD(lParam));
            return 0;
        }
        xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
        yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
        LuaSetMouseBlock(lua_mb_act==10 ? lua_area_x : xblk,
            lua_mb_act==10 ? lua_area_y : yblk,
            lua_mb_act==10 ? xblk : xblk,
            lua_mb_act==10 ? yblk : yblk);
        if (lua_mb_act == 5 && (GetAsyncKeyState(VK_LBUTTON)<0 ||
            GetAsyncKeyState(VK_RBUTTON)<0 || GetAsyncKeyState(VK_MBUTTON)<0)) {
            if (meoldbx != xblk || meoldby != yblk)
                LuaRunMouseButton(5, xblk, yblk);
        }
        if (GetAsyncKeyState(VK_LBUTTON)>=0 && !gettingbrush &&
            GetAsyncKeyState(VK_RBUTTON)>=0) painton = 0;
        if (meoldbx != xblk || meoldby != yblk) {
            if (painton) {
                if (gettingbrush) {
                    MEBrushRect(hwnd, mebrushx, mebrushy, mebrushx2, mebrushy2);
                    mebrushx2 = (xblk-xmapoffset);
                    mebrushy2 = (yblk-ymapoffset);
                    MEBrushRect(hwnd, mebrushx, mebrushy, mebrushx2, mebrushy2);
                } else {
                    PlaceTile(xblk, yblk);
                    if (blockstaggerx || blockstaggery) InvalidateRgn(mhwnd, NULL, FALSE);
                    else MEDrawUnderTile(hwnd, xblk, yblk);
                }
            }
            //			if (!(blockstaggerx || blockstaggery))
            MEDrawGrid(hwnd, xblk, yblk);
            MEShowCoords(xblk, yblk);
        }
        //		if (curbrsh>0 && curbrsh<9 && !gettingbrush) InvalidateRgn (mhwnd, NULL, FALSE);
        break;
    case WM_ACTIVATE:
        //   	MessageBox (hwnd, "Killed!", "Focus", MB_OK);
    case WM_LBUTTONUP:
        if (lua_mb_act == 10) {
            xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
            yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
            LuaSetMouseBlock(lua_area_x, lua_area_y, xblk, yblk);
            LuaRunMouseButton(10, xblk, yblk);
            lua_mb_act = 0;
        } else lua_mb_act = 0;
        painton = 0;
        if (gettingbrush) {
            if (mebrushx2<mebrushx) { i = mebrushx; mebrushx = mebrushx2; mebrushx2 = i; }
            if (mebrushy2<mebrushy) { i = mebrushy; mebrushy = mebrushy2; mebrushy2 = i; }
            if (MakeBrush(xmapoffset+mebrushx, ymapoffset+mebrushy,
                xmapoffset+mebrushx2, ymapoffset+mebrushy2)!=0)
            MessageBox(hwnd, "No brush number selected, or part of the brush is outside the map area, the previous brush has been kept", "Brush error", MB_ICONEXCLAMATION|MB_OK);
            gettingbrush = 0;
            InvalidateRgn(mhwnd, NULL, FALSE); }
        if (curbrsh>0 && curbrsh<=MAXBRSH) InvalidateRgn(mhwnd, NULL, FALSE);
        break;
    case WM_RBUTTONUP:
        if (mappt==NULL) break;
        if (lua_mb_act == 10) {
            xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
            yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
            LuaSetMouseBlock(lua_area_x, lua_area_y, xblk, yblk);
            LuaRunMouseButton(10, xblk, yblk);
            lua_mb_act = 0;
            rmapped = 0;
            break;
        }
        lua_mb_act = 0;
        if (rmapped) { rmapped = 0; break; }
        MECopyToUndo();
        xblk = xmapoffset + MEGetX(LOWORD(lParam), HIWORD(lParam));
        yblk = ymapoffset + MEGetY(LOWORD(lParam), HIWORD(lParam));
        if (xblk>=0 && xblk<mapwidth && yblk>=0 && yblk<mapheight) {
            curplacetile = 0;
            PlaceTile(xblk, yblk);
            mapaltered = 1;
            if (blockstaggerx || blockstaggery) InvalidateRgn(mhwnd, NULL, FALSE);
            else MEDrawUnderTile(hwnd, xblk, yblk);
            MEShowCoords(xblk, yblk);
        }
        break;
    case WM_RBUTTONDOWN:
        if (mappt==NULL) break;
        if (MBDoAction(mb_r, hwnd, lParam)) rmapped = 1;
        break;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        if (mappt==NULL) break;
        MBDoAction(HIWORD(wParam)==2 ? mb_x2 : mb_x1, hwnd, lParam);
        break;
    case WM_MBUTTONDOWN:
        if (mappt==NULL) break;
        if (MBDoAction(mb_m, hwnd, lParam)) break;
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
            SetScrollPos(mhwnd, SB_VERT, ymapoffset, TRUE);
            InvalidateRgn(mhwnd, NULL, FALSE);
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
        SetScrollPos(mhwnd, SB_VERT, ymapoffset, TRUE);
        InvalidateRgn(mhwnd, NULL, FALSE);
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
        SetScrollPos(mhwnd, SB_HORZ, xmapoffset, TRUE);
        InvalidateRgn(mhwnd, NULL, FALSE);
        break;
    case WM_KEYDOWN:
        if (mappt==NULL) break;
        if (LOWORD(wParam)>=VK_F1 && LOWORD(wParam)<=VK_F10) {
            int fn = LOWORD(wParam)-VK_F1;
            if (keyF[fn]) SendMessage(GetParent(hwnd), WM_COMMAND, keyF[fn], 0);
            break;
        }
        switch (LOWORD(wParam)) {
        case '1': if (key1) { SendMessage(GetParent(hwnd), WM_COMMAND, key1, 0); break; } break;
        case '2': if (key2) { SendMessage(GetParent(hwnd), WM_COMMAND, key2, 0); break; } break;
        case '3': if (key3) { SendMessage(GetParent(hwnd), WM_COMMAND, key3, 0); break; } break;
        case 'I': if (keyi) { SendMessage(GetParent(hwnd), WM_COMMAND, keyi, 0); break; } break;
        case 'K': if (keyk) { SendMessage(GetParent(hwnd), WM_COMMAND, keyk, 0); break; } break;
        case 'M': if (keym) { SendMessage(GetParent(hwnd), WM_COMMAND, keym, 0); break; } break;
        case 'L': /* line tool */
            if (linemode) linemode = 0; else { linemode = 1; pathmode = 0; }
            break;
        case 'P': /* lay-path tool (or pick object in place mode) */
            if (objplace) {
                int mpx = meoldbx * blockwidth + blockwidth / 2;
                int mpy = meoldby * blockheight + blockheight / 2;
                int pi = OE_PickObject(mpx, mpy);
                if (pi >= 0) {
                    F14_SetCurObject(pi);
                    InvalidateRgn(mhwnd, NULL, FALSE);
                }
            } else if (pathmode) { pathmode = 0; } else { pathmode = 1; linemode = 0; }
            break;
        case 'D': /* drag-map */
            if (!dragmode) {
                POINT mc;
                if (MECursorClient(&mc)) {
                    dragmode = 1;
                    dragox = mc.x;
                    dragoy = mc.y;
                    dragoxoff = xmapoffset;
                    dragoyoff = ymapoffset;
                    SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                }
            } else {
                MEDragFinish();
            }
            break;
        case 'E': /* edit block/anim properties */
            if (curanim>=0) DIAEditAnimProperties(mhwnd, curanim);
            else DIAEditProperties(mhwnd, curstr);
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

HWND CreateMapEditorWindow(HWND hwnd) {
    int x;
    RECT rc;
    WNDCLASS wndclass;

    if (mhwnd != NULL) return mhwnd;

    memset(&wndclass, 0, sizeof(WNDCLASS));
    wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT | CS_NOCLOSE;
    wndclass.lpfnWndProc = MapWndProc;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(hInstance, "MAPWINIC");
    wndclass.hCursor = LoadCursor(NULL, (LPCSTR) IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH) GetStockObject(GRAY_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szMapEdname;
    RegisterClass(&wndclass);

    GetWindowRect(hwnd, &rc);
    {
        int split = MapDip(hwnd, MAP_DIP_SPLIT);
        int pad = MapDip(hwnd, MAP_DIP_SPLITPAD);
        int fudge = MapCaptionFudge(hwnd);
        if (wlayout == 0) x = 0; else x = split;
        mhwnd = CreateWindow(szMapEdname, szMapEdname,
            WS_MAXIMIZEBOX	| WS_MINIMIZEBOX | WS_CAPTION | WS_CLIPSIBLINGS |
            WS_THICKFRAME | WS_CHILDWINDOW | WS_HSCROLL | WS_VSCROLL,
            x, 0, (rc.right)-(rc.left)-split-pad,
            (rc.bottom)-(rc.top)-fudge,
            hwnd, (HMENU) 1000, hInstance, NULL);
    }
    MEEnsureUiFont(mhwnd);

    MEUpdateScrollVis(); /* no map yet: hide scrollbars until one loads */
    ShowWindow(mhwnd, SW_SHOW);
    UpdateWindow(mhwnd);
    return mhwnd;
}

static LRESULT CALLBACK GraphWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    HDC myhdc;
    PAINTSTRUCT ps;
    int id;

    switch (message) {
    case WM_PAINT:
        myhdc = BeginPaint(hwnd, &ps);
        if (blockgfxpt != NULL)
            MEDrawGraphicEditor(hwnd, myhdc);
        EndPaint(hwnd, &ps);
        break;
    case WM_COMMAND:
        id = (int) LOWORD(wParam);
        if (id >= IDC_GETOOL0 && id < IDC_GETOOL0 + GETOOL_COUNT) {
            GEDoTool(id - IDC_GETOOL0);
            GEInvalidate();
            BEInvalidate();
            GEUpdateStatus();
            SetFocus(hwnd);
            return 0;
        }
        if (id >= IDC_GETHICK1 && id < IDC_GETHICK1 + 4) {
            GE_thickness = id - IDC_GETHICK1 + 1;
            GEUpdateToolbar();
            SetFocus(hwnd);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        GEOnLButtonDown(hwnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        return 0;
    case WM_LBUTTONUP:
        GEOnLButtonUp(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        GEOnMouseMove(hwnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        return 0;
    case WM_RBUTTONDOWN:
        SetFocus(hwnd);
        GEOnRButtonDown(hwnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        return 0;
    case WM_SIZE:
        GELayoutToolbar(hwnd);
        GEPlacePalette(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case WM_DPICHANGED:
        GEEnsureUiFont(hwnd);
        if (palhwnd) PalSetFont(palhwnd);
        GELayoutToolbar(hwnd);
        GEPlacePalette(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case WM_KEYDOWN:
        switch (LOWORD(wParam)) {
        case 'L': GEAccelLine(); break;
        case 'F': GEAccelFill(); break;
        case 'P': GEAccelPick(); break;
        case 'Z':
            if (GetKeyState(VK_CONTROL) < 0) GEUndo();
            break;
        }
        return 0;
    case WM_DESTROY:
        if (palhwnd != NULL) {
            DestroyWindow(palhwnd);
            palhwnd = NULL;
        }
        ghwnd = NULL;
        if (ge_font != NULL) {
            DeleteObject(ge_font);
            ge_font = NULL;
        }
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

HWND CreateGraphEditorWindow(HWND hwnd) {
    int x;
    RECT rc;
    WNDCLASS wndclass;

    if (ghwnd != NULL) return ghwnd;

    memset(&wndclass, 0, sizeof(WNDCLASS));
    wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT | CS_NOCLOSE;
    wndclass.lpfnWndProc = GraphWndProc;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(hInstance, "MAPWINIC");
    wndclass.hCursor = LoadCursor(NULL, (LPCSTR) IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH) GetStockObject(GRAY_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szGraphEdname;
    RegisterClass(&wndclass);

    GetWindowRect(hwnd, &rc);
    {
        int split = MapDip(hwnd, MAP_DIP_SPLIT);
        int pad = MapDip(hwnd, MAP_DIP_SPLITPAD);
        int fudge = MapCaptionFudge(hwnd);
        if (wlayout == 0) x = 0; else x = split;
        ghwnd = CreateWindow(szGraphEdname, szGraphEdname,
            WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CAPTION | WS_CLIPSIBLINGS |
            WS_CLIPCHILDREN | WS_THICKFRAME | WS_CHILDWINDOW,
            x, 0, (rc.right)-(rc.left)-split-pad,
            (rc.bottom)-(rc.top)-fudge,
            hwnd, (HMENU) 1002, hInstance, NULL);
    }

    GECreateToolbar(ghwnd);
    GECreatePalette(ghwnd);
    ShowWindow(ghwnd, SW_HIDE);
    return ghwnd;
}

/* Hide scrollbars when no map is loaded (avoids zero-range scrollbar
 * artifacts, e.g. white strip when maximized with nothing open). */
void MEUpdateScrollVis(void) {
    if (mhwnd==NULL) return;
    ShowScrollBar(mhwnd, SB_BOTH, mappt!=NULL ? TRUE : FALSE);
}
