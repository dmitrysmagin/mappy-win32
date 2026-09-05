#define  STRICT
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"
#include "dpi.h"
#include "fmp14.h"
#include "luabind.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#define BETYPE_STILL   0
#define BETYPE_ANIM    1
#define BETYPE_GRAPH   2
#define BETYPE_BRUSHES 3
#define BETYPE_OBJECTS 4
#define BETYPE_COUNT   5

#define IDC_BEMODETAB 1000
#define TAB_H 22

static HWND bhwnd = NULL;
static HWND htab = NULL;
static HFONT be_font = NULL;
static int be_tabh = TAB_H;
short int beyoff = 0, betype = BETYPE_STILL, bebpr = 1;
short int curgfxsel = -1;   /* selected graphic index (Graph mode) */
short int curbrushsel = -1; /* selected brush index (Brushes mode) */
short int curobjsel = -1;   /* selected object index (Objects mode) */
static char szBlockEdname[] = "Block Editor";
extern HINSTANCE hInstance;
extern short int blockwidth, blockheight, blockdepth, gridon, zoomon;
extern int panelzoom;
static int BEZoom(void) { return panelzoom ? zoomon : 1; }
extern short int mapwidth, mapheight, xmapoffset, ymapoffset;
extern short int numblockstr, curstr, numblockanim, curanim, curbrsh;
extern char * blockgfxpt, * blockstrpt, * animstrpt;
extern short int numblockgfx;
extern short int * brshpt[MAXBRSH];
extern short int numbrushes;
extern int wlayout;
extern int sortblocks, sortobjects;
extern int mwheelblks, thumbtrack;

static int BETabStripHeight(void);
static void BEEnsureUiFont(HWND hwnd);
static void BELayoutTabs(HWND parent);
static void BECreateTabControl(HWND parent);
static void BESyncTabSel(void);
static void BESetType(int newtype);

void BEResizeWind(int wide, int high) {
    if (bhwnd == NULL) return;
    if (wlayout == 0) MoveWindow(bhwnd, wide-(wide/5), 0, wide/5, high, TRUE);
    if (wlayout == 1) MoveWindow(bhwnd, 0, 0, wide/5, high, TRUE);
    if (wlayout == -1) MoveWindow(bhwnd, wide-(wide/5), 0, wide/5, high, TRUE);
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
        if (next < MAXBRSH) { curbrushsel = next; BEInvalidate(); }
    }
    if (betype == BETYPE_OBJECTS && curobjsel < F14_NumObjects() - 1) { curobjsel++; BEInvalidate(); }
}

void BEBlockPrev(void) {
    if (betype == BETYPE_STILL && curanim==-1 && curbrsh==0 && curstr>0) { curstr--; BEInvalidate(); }
    if (betype == BETYPE_ANIM && curanim!=-1 && curbrsh==0 && curanim>0) { curanim--; BEInvalidate(); }
    if (betype == BETYPE_GRAPH && curgfxsel > 0) { curgfxsel--; BEInvalidate(); }
    if (betype == BETYPE_BRUSHES && curbrushsel > 0) {
        int prev = curbrushsel - 1;
        while (prev >= 0 && brshpt[prev] == NULL) prev--;
        if (prev >= 0) { curbrushsel = prev; BEInvalidate(); }
    }
    if (betype == BETYPE_OBJECTS && curobjsel > 0) { curobjsel--; BEInvalidate(); }
}

static int BEItemCount(void) {
    switch (betype) {
    case BETYPE_STILL:   return numblockstr;
    case BETYPE_ANIM:    return numblockanim;
    case BETYPE_GRAPH:   return numblockgfx;
    case BETYPE_BRUSHES: return numbrushes;
    case BETYPE_OBJECTS: return F14_NumObjects();
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

static int BEMaxScrollRow(void) {
    int n = BEVisibleCount();
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
            sprintf(buf, "%s (%d/%d)", title, sel + 1, total);
        else
            sprintf(buf, "%s", title);
        SetWindowText(bhwnd, buf);
    } else {
        SetWindowText(bhwnd, title);
    }
    if (bebpr < 1) bebpr = 1;
    SetScrollRange(bhwnd, SB_VERT, 0, (total > 0) ? ((total - 1) / bebpr) : 0, FALSE);
    if (beyoff > ((total - 1) / bebpr)) beyoff = (total - 1) / bebpr;
    if (beyoff < 0) beyoff = 0;
    SetScrollPos(bhwnd, SB_VERT, beyoff, TRUE);
}

void BENewBlock(void) {
    if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0 && numblockstr < MAXBLKSTR) NewBlock();
    if (betype == BETYPE_ANIM && curanim != -1) AnimNew();
    else if (betype == BETYPE_ANIM && animstrpt == NULL) AnimNew();
    BESetScrollRange();
    BEInvalidate();
}

void BECutBlock(void) {
    if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0) CutBlock();
    if (betype == BETYPE_ANIM && curanim != -1) AnimCut();
    BESetScrollRange();
}

void BECopyBlock(void) {
    if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0) CopyBlock();
    if (betype == BETYPE_ANIM && curanim != -1) AnimCopy();
}

void BEPasteBlock(void) {
    if (betype == BETYPE_STILL && curanim == -1 && curbrsh == 0 && numblockstr < MAXBLKSTR) PasteBlock();
    if (betype == BETYPE_ANIM && curanim != -1) AnimPaste();
    BESetScrollRange();
}

static int BEIsSelected(int idx) {
    switch (betype) {
    case BETYPE_STILL:   return (curanim == -1 && curbrsh == 0 && idx == curstr);
    case BETYPE_ANIM:    return (curanim >= 0 && idx == curanim);
    case BETYPE_GRAPH:   return (idx == curgfxsel);
    case BETYPE_BRUSHES: return (idx == curbrushsel);
    case BETYPE_OBJECTS: return (idx == curobjsel);
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

    bebpr = (short int) (k-(24/BEZoom()))/(blockwidth+2);
    if (bebpr<1) bebpr = 1;

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
    if (newtype == BETYPE_GRAPH) {
        curgfxsel = (short int) BEGfxFromStill();
        if (bebpr > 0) beyoff = (short int) (curgfxsel / bebpr);
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
        if (LOWORD(lParam)>((blockwidth+2)*BEZoom()*bebpr)) break;
        if (HIWORD(lParam) < BETabStripHeight()) break;
        {
            int clicked = ((beyoff + ((HIWORD(lParam)-BETabStripHeight())/((blockheight+2)*BEZoom()))) * bebpr) + ((LOWORD(lParam))/((blockwidth+2)*BEZoom()));
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
            case BETYPE_BRUSHES:
                curbrushsel = clicked;
                if (curbrushsel>=MAXBRSH) curbrushsel = MAXBRSH-1;
                if (curbrushsel<0) curbrushsel = 0;
                if (curbrushsel >= 0 && brshpt[curbrushsel] != NULL) {
                    curbrsh = (short int) (curbrushsel + 1);
                    curanim = -1;
                    RejigBrshMenu(curbrsh);
                }
                break;
            case BETYPE_OBJECTS:
                curobjsel = clicked;
                if (curobjsel>=F14_NumObjects()) curobjsel = F14_NumObjects()-1;
                if (curobjsel<0) curobjsel = 0;
                if (curobjsel >= 0) {
                    F14_SetCurObject(curobjsel);
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
                }
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
            case BETYPE_BRUSHES:
                curbrushsel = clicked;
                if (curbrushsel>=MAXBRSH) { curbrushsel = MAXBRSH-1; break; }
                if (curbrushsel<0) curbrushsel = 0;
                if (curbrushsel >= 0 && brshpt[curbrushsel] != NULL) {
                    curbrsh = (short int) (curbrushsel + 1);
                    DIAGetBrushName(hwnd, GetBrushName(curbrushsel));
                    RejigBrshMenu(curbrsh);
                }
                break;
            case BETYPE_OBJECTS:
                curobjsel = clicked;
                if (curobjsel>=F14_NumObjects()) { curobjsel = F14_NumObjects()-1; break; }
                if (curobjsel<0) curobjsel = 0;
                if (curobjsel >= 0) {
                    F14_SetCurObject(curobjsel);
                    DIAObjectProps(hwnd, curobjsel);
                }
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
    static const char * labels[] = { "Still", "Anim", "Graph", "Brushes", "Objects" };
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
        int split = MapDip(hwnd, MAP_DIP_SPLIT);
        int pad = MapDip(hwnd, MAP_DIP_SPLITPAD);
        int fudge = MapCaptionFudge(hwnd);
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
