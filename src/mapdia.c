#define  STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"
#include "compat.h"
#include "fmp.h"
#include "fmp14.h"
#include "tileset.h"
#include "luabind.h"

#define ANDTMEM (65536*4)
#define BLOCKSTRSIZE 32

#define AN_END -1
#define AN_NONE 0
#define AN_LOOPF 1
#define AN_LOOPR 2
#define AN_ONCE 3
#define AN_ONCEH 4
#define AN_PPFF 5
#define AN_PPRR 6
#define AN_PPRF 7
#define AN_PPFR 8
#define AN_ONCES 9

#define EXPRT_CMA 0x01
#define EXPRT_ABD 0x02
#define EXPRT_MAR 0x04
#define EXPRT_GFX 0x08
#define EXPRT_BMP 0x10
#define EXPRT_SKP 0x20
#define EXPRT_CSV 0x40
#define EXPRT_SCR 0x80

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

extern HINSTANCE hInstance;
extern short int mapwidth, mapheight, blockwidth, blockheight, blockdepth, numblockstr, numblockgfx, curbrsh;
extern short int numblockanim;
extern int megetatileset;
extern int blockgapx, blockgapy, blockstaggerx, blockstaggery, blockstagger;
extern int clickmask;
extern int maptype;
extern int csvadjust, csvusebg;
extern int divideron, dividergapx, dividergapy;
extern long dividercol;
extern short int maplayer;
extern char blocklabels[16][64];
extern int rotatedtiles, flippedtiles, isopillars;
extern char textexportopts[64];
extern char folder[256];
extern int mapaltered, onion, oniondark, onionlower;
extern char * blockstrpt, * blockgfxpt, * animstrpt, * cmappt;
extern short int * mappt;
extern char * altgfxpt;
extern short int * mapmappt[MAXMAPLAYERS];
extern short int * brshpt[MAXBRSH];
extern char mapfilename[256];
extern char authorname[70];
extern char authorinfo1[70];
extern char authorinfo2[70];
extern char authorinfo3[70];
extern char novctext[70];
extern unsigned char trans8bit, transred, transgreen, transblue;
extern unsigned char trans16hi, trans16low;
extern int resize_left, resize_top;

static int editnum, pgfxnum, editpoint, pickwhat, totalmapmem;
int hexordec = 0;
static int propedit_haspos = 0; /* remember Block Properties pos across reopen */
static RECT propedit_rc;
static short int newmapwidth, newmapheight, newmapdepth, newblockwidth, newblockheight;
static int newblockgapx, newblockgapy, newblockstaggerx, newblockstaggery, newmaptype;
static short int newframes[33];
static char * depthstexts[] = {
    "8bit  (256 colours)",
    "15bit (32,768 colours)",
    "16bit (65,536 colours)",
    "24bit (16,777,216 colours)",
    "32bit (16,777,216 colours)",
    NULL };
static char * versionstexts[] = {
    "FMP 0.5",
    "FMP 1.0",
    "FMP 1.0 RLE",
    NULL };

static char *andescs[] = {
    "NONE", "LOOPF", "LOOPR", "ONCE", "ONCEH", "PPFWD", "PPREV",
    NULL };

static char *lyrdirtexts[] = {
    "Up", "Down", "Left", "Right",
    NULL };

static char * gbname;
static char expname[512];


INT_PTR CALLBACK DIANewMapProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i;
    char textbuf[80];

    switch (msg) {
    case WM_INITDIALOG:
        sprintf(textbuf, "%d", blockwidth);
        if (blockwidth==0) strcpy(textbuf, "32");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKWIDTH), textbuf);
        sprintf(textbuf, "%d", blockheight);
        if (blockheight==0) strcpy(textbuf, "32");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKHEIGHT), textbuf);
        sprintf(textbuf, "%d", mapwidth);
        if (mapwidth==0) strcpy(textbuf, "100");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPWIDTH), textbuf);
        sprintf(textbuf, "%d", mapheight);
        if (mapheight==0) strcpy(textbuf, "100");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPHEIGHT), textbuf);
        sprintf(textbuf, "%d", blockgapx);
        if (blockgapx==0) strcpy(textbuf, "32");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKGAPX), textbuf);
        sprintf(textbuf, "%d", blockgapy);
        if (blockgapy==0) strcpy(textbuf, "32");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKGAPY), textbuf);
        sprintf(textbuf, "%d", blockstaggerx);
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKSTAGGERX), textbuf);
        sprintf(textbuf, "%d", blockstaggery);
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKSTAGGERY), textbuf);
        SendMessage(hwndDlg, WM_COMMAND, 99, 0L);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_NEWMAPBLOCKGAPX:
        case IDC_NEWMAPBLOCKGAPY:
        case IDC_NEWMAPBLOCKSTAGGERX:
        case IDC_NEWMAPBLOCKSTAGGERY:
            //         	sprintf (textbuf, "%d", HIWORD(wParam));
            //				MessageBox (hwndDlg, textbuf, "Debug", MB_OK);
            if (HIWORD(wParam)==512) CheckDlgButton(hwndDlg, IDC_NEWMAPNOTRECT, 1);
            break;
        case 99:
            SendDlgItemMessage(hwndDlg, IDC_NEWMAPDEPTH, CB_RESETCONTENT, 0, 0);
            i = 0; while (depthstexts[i]!=NULL) {
                SendDlgItemMessage(hwndDlg, IDC_NEWMAPDEPTH, CB_ADDSTRING, 0,
                    (LPARAM) ((LPSTR) depthstexts[i])); i++; }
            SendDlgItemMessage(hwndDlg, IDC_NEWMAPDEPTH, CB_SETITEMHEIGHT,
                0, 14);
            SendDlgItemMessage(hwndDlg, IDC_NEWMAPDEPTH, CB_SELECTSTRING, -1,
                (LPARAM) ((LPSTR) depthstexts[0]));

            SendDlgItemMessage(hwndDlg, IDC_NEWMAPVERSION, CB_RESETCONTENT, 0, 0);
            i = 0; while (versionstexts[i]!=NULL) {
                SendDlgItemMessage(hwndDlg, IDC_NEWMAPVERSION, CB_ADDSTRING, 0,
                    (LPARAM) ((LPSTR) versionstexts[i])); i++; }
            SendDlgItemMessage(hwndDlg, IDC_NEWMAPVERSION, CB_SETITEMHEIGHT,
                0, 14);
            SendDlgItemMessage(hwndDlg, IDC_NEWMAPVERSION, CB_SELECTSTRING, -1,
                (LPARAM) ((LPSTR) versionstexts[0]));
            break;
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKWIDTH), textbuf, 80);
            newblockwidth = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKHEIGHT), textbuf, 80);
            newblockheight = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPWIDTH), textbuf, 80);
            newmapwidth = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPHEIGHT), textbuf, 80);
            newmapheight = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPDEPTH), textbuf, 80);
            newmapdepth = 8;
            if (!strcmp(textbuf, depthstexts[1])) newmapdepth = 15;
            if (!strcmp(textbuf, depthstexts[2])) newmapdepth = 16;
            if (!strcmp(textbuf, depthstexts[3])) newmapdepth = 24;
            if (!strcmp(textbuf, depthstexts[4])) newmapdepth = 32;
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPVERSION), textbuf, 80);
            newmaptype = 0;
            if (!strcmp(textbuf, versionstexts[1])) newmaptype = 1;
            if (!strcmp(textbuf, versionstexts[2])) newmaptype = 2;

            if (SendDlgItemMessage(hwndDlg, IDC_NEWMAPNOTRECT, BM_GETCHECK, 0, 0)) {
                GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKGAPX), textbuf, 80);
                newblockgapx = (int) atoi(textbuf);
                GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKGAPY), textbuf, 80);
                newblockgapy = (int) atoi(textbuf);
                GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKSTAGGERX), textbuf, 80);
                newblockstaggerx = (int) atoi(textbuf);
                GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKSTAGGERY), textbuf, 80);
                newblockstaggery = (int) atoi(textbuf);
            } else {
                newblockgapx = newblockwidth;
                newblockgapy = newblockheight;
                newblockstaggerx = 0;
                newblockstaggery = 0;
            }

            if (newmaptype) {
                MessageBox(hwndDlg, "You have selected version 1.0 of FMP, be sure your playback library supports this. "
                    "If not, change the version in MapTools->Map Properties", "New Map", MB_ICONINFORMATION|MB_OK);
            }
            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
            break;
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

INT_PTR CALLBACK DIANewMapSimpleProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    char textbuf[80];

    switch (msg) {
    case WM_INITDIALOG:
        sprintf(textbuf, "%d", blockwidth);
        if (blockwidth==0) strcpy(textbuf, "32");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKWIDTH), textbuf);
        sprintf(textbuf, "%d", blockheight);
        if (blockheight==0) strcpy(textbuf, "32");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKHEIGHT), textbuf);
        sprintf(textbuf, "%d", mapwidth);
        if (mapwidth==0) strcpy(textbuf, "100");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPWIDTH), textbuf);
        sprintf(textbuf, "%d", mapheight);
        if (mapheight==0) strcpy(textbuf, "100");
        SetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPHEIGHT), textbuf);
        CheckDlgButton(hwndDlg, IDC_NEWMAPTRUECOLOUR, 1);
        CheckDlgButton(hwndDlg, IDC_NEWMAPPALETTED, 0);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_NEWMAPADVANCED:
            SendMessage(hwndDlg, WM_CLOSE, 0, 2L);
            break;
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKWIDTH), textbuf, 80);
            newblockwidth = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPBLOCKHEIGHT), textbuf, 80);
            newblockheight = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPWIDTH), textbuf, 80);
            newmapwidth = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_NEWMAPHEIGHT), textbuf, 80);
            newmapheight = (short int) atoi(textbuf);
            if (IsDlgButtonChecked(hwndDlg, IDC_NEWMAPTRUECOLOUR)) newmapdepth = 24;
            else newmapdepth = 8;
            newmaptype = 0;

            newblockgapx = newblockwidth;
            newblockgapy = newblockheight;
            newblockstaggerx = 0;
            newblockstaggery = 0;

            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
            break;
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

void DIAHexDecUsers(HWND hwndDlg) {
    char mytext[80];
    unsigned int li;
    int sli;

    if (hexordec) {
        GetDlgItemText(hwndDlg, IDC_PROPUSER1, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%d", &sli);
            sprintf(mytext, "%08X", (unsigned int) sli);
            SetDlgItemText(hwndDlg, IDC_PROPUSER1, mytext);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER2, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%d", &sli);
            sprintf(mytext, "%08X", (unsigned int) sli);
            SetDlgItemText(hwndDlg, IDC_PROPUSER2, mytext);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER3, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%d", &sli);
            sprintf(mytext, "%04X", (unsigned int) (sli&0xFFFF));
            SetDlgItemText(hwndDlg, IDC_PROPUSER3, mytext);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER4, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%d", &sli);
            sprintf(mytext, "%04X", (unsigned int) (sli&0xFFFF));
            SetDlgItemText(hwndDlg, IDC_PROPUSER4, mytext);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER5, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%d", &sli);
            sprintf(mytext, "%02X", (unsigned int) (sli&0xFF));
            SetDlgItemText(hwndDlg, IDC_PROPUSER5, mytext);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER6, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%d", &sli);
            sprintf(mytext, "%02X", (unsigned int) (sli&0xFF));
            SetDlgItemText(hwndDlg, IDC_PROPUSER6, mytext);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER7, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%d", &sli);
            sprintf(mytext, "%02X", (unsigned int) (sli&0xFF));
            SetDlgItemText(hwndDlg, IDC_PROPUSER7, mytext);
        }
    } else {
        GetDlgItemText(hwndDlg, IDC_PROPUSER1, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%X", &li);
            sprintf(mytext, "%d", (int) li);
            SetDlgItemInt(hwndDlg, IDC_PROPUSER1, (int) li, TRUE);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER2, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%X", &li);
            sprintf(mytext, "%d", (int) li);
            SetDlgItemInt(hwndDlg, IDC_PROPUSER2, (int) li, TRUE);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER3, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%X", &li);
            sprintf(mytext, "%d", (int) li);
            if (li&0x8000) li |= 0xFFFF0000;
            SetDlgItemInt(hwndDlg, IDC_PROPUSER3, (int) li, TRUE);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER4, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%X", &li);
            sprintf(mytext, "%d", (int) li);
            if (li&0x8000) li |= 0xFFFF0000;
            SetDlgItemInt(hwndDlg, IDC_PROPUSER4, (int) li, TRUE);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER5, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%X", &li);
            sprintf(mytext, "%d", (int) li);
            if (li&0x80) li |= 0xFFFFFF00;
            SetDlgItemInt(hwndDlg, IDC_PROPUSER5, (int) li, TRUE);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER6, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%X", &li);
            sprintf(mytext, "%d", (int) li);
            if (li&0x80) li |= 0xFFFFFF00;
            SetDlgItemInt(hwndDlg, IDC_PROPUSER6, (int) li, TRUE);
        }
        GetDlgItemText(hwndDlg, IDC_PROPUSER7, mytext, 80);
        if (mytext[0] != 0) {
            sscanf(mytext, "%X", &li);
            sprintf(mytext, "%d", (int) li);
            if (li&0x80) li |= 0xFFFFFF00;
            SetDlgItemInt(hwndDlg, IDC_PROPUSER7, (int) li, TRUE);
        }
    }
}

void DIAFillUsers(HWND hwndDlg) {
    BLKSTR * myblkstrpt;
    char mytext[80];
    unsigned int li;

    if (hexordec) {
        DIAHexDecUsers(hwndDlg);
    }

    myblkstrpt = (BLKSTR *) blockstrpt;
    myblkstrpt += editnum;
    GetDlgItemText(hwndDlg, IDC_PROPUSER1, mytext, 80);
    sscanf(mytext, "%X", &li);
    myblkstrpt->user1 = li;
    GetDlgItemText(hwndDlg, IDC_PROPUSER2, mytext, 80);
    sscanf(mytext, "%X", &li);
    myblkstrpt->user2 = li;
    GetDlgItemText(hwndDlg, IDC_PROPUSER3, mytext, 80);
    sscanf(mytext, "%X", &li);
    myblkstrpt->user3 = (short int) li;
    GetDlgItemText(hwndDlg, IDC_PROPUSER4, mytext, 80);
    sscanf(mytext, "%X", &li);
    myblkstrpt->user4 = (short int) li;
    GetDlgItemText(hwndDlg, IDC_PROPUSER5, mytext, 80);
    sscanf(mytext, "%X", &li);
    myblkstrpt->user5 = (char) li;
    GetDlgItemText(hwndDlg, IDC_PROPUSER6, mytext, 80);
    sscanf(mytext, "%X", &li);
    myblkstrpt->user6 = (char) li;
    GetDlgItemText(hwndDlg, IDC_PROPUSER7, mytext, 80);
    sscanf(mytext, "%X", &li);
    myblkstrpt->user7 = (char) li;

    if (SendDlgItemMessage(hwndDlg, IDC_PROPTL, BM_GETCHECK, 0, 0))
        myblkstrpt->tl = 1; else myblkstrpt->tl = 0;
    if (SendDlgItemMessage(hwndDlg, IDC_PROPTR, BM_GETCHECK, 0, 0))
        myblkstrpt->tr = 1; else myblkstrpt->tr = 0;
    if (SendDlgItemMessage(hwndDlg, IDC_PROPBL, BM_GETCHECK, 0, 0))
        myblkstrpt->bl = 1; else myblkstrpt->bl = 0;
    if (SendDlgItemMessage(hwndDlg, IDC_PROPBR, BM_GETCHECK, 0, 0))
        myblkstrpt->br = 1; else myblkstrpt->br = 0;
    if (SendDlgItemMessage(hwndDlg, IDC_PROPTRIGGER, BM_GETCHECK, 0, 0))
        myblkstrpt->trigger = 1; else myblkstrpt->trigger = 0;
    if (SendDlgItemMessage(hwndDlg, IDC_PROPUNUSED1, BM_GETCHECK, 0, 0))
        myblkstrpt->unused1 = 1; else myblkstrpt->unused1 = 0;
    if (SendDlgItemMessage(hwndDlg, IDC_PROPUNUSED2, BM_GETCHECK, 0, 0))
        myblkstrpt->unused2 = 1; else myblkstrpt->unused2 = 0;
    if (SendDlgItemMessage(hwndDlg, IDC_PROPUNUSED3, BM_GETCHECK, 0, 0))
        myblkstrpt->unused3 = 1; else myblkstrpt->unused3 = 0;

    {
        int uf = LuaGetStrBlkUser();
        int sel = (int) SendDlgItemMessage(hwndDlg, IDC_PROPSTR, CB_GETCURSEL, 0, 0);
        char tb[256];
        GetWindowText(GetDlgItem(hwndDlg, IDC_PROPSTR), tb, 255);
        if (sel >= 0) {
            const char * old = F14_TstrGet(sel);
            if (tb[0] && (old == NULL || strcmp(old, tb))) F14_TstrSet(sel, tb);
        } else if (tb[0]) {
            sel = F14_TstrCount();
            F14_TstrSet(sel, tb);
        }
        if (sel >= 0 && uf >= 1 && uf <= 7) {
            unsigned long v = (unsigned long) sel;
            if (uf == 1) myblkstrpt->user1 = v;
            else if (uf == 2) myblkstrpt->user2 = v;
            else if (uf == 3) myblkstrpt->user3 = (unsigned short) v;
            else if (uf == 4) myblkstrpt->user4 = (unsigned short) v;
            else if (uf == 5) myblkstrpt->user5 = (unsigned char) v;
            else if (uf == 6) myblkstrpt->user6 = (unsigned char) v;
            else myblkstrpt->user7 = (unsigned char) v;
        }
    }
}

static void DIAFillStrCombo(HWND hwndDlg) {
    BLKSTR * myblkstrpt;
    int uf, i, n, cur = 0;
    uf = LuaGetStrBlkUser();
    myblkstrpt = (BLKSTR *) blockstrpt;
    if (myblkstrpt == NULL) return;
    myblkstrpt += editnum;
    if (uf == 1) cur = (int) myblkstrpt->user1;
    else if (uf == 2) cur = (int) myblkstrpt->user2;
    else if (uf == 3) cur = myblkstrpt->user3;
    else if (uf == 4) cur = myblkstrpt->user4;
    else if (uf == 5) cur = myblkstrpt->user5;
    else if (uf == 6) cur = myblkstrpt->user6;
    else if (uf == 7) cur = myblkstrpt->user7;
    n = F14_TstrCount();
    SendDlgItemMessage(hwndDlg, IDC_PROPSTR, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < n; i++) {
        const char * s = F14_TstrGet(i);
        SendDlgItemMessage(hwndDlg, IDC_PROPSTR, CB_ADDSTRING, 0,
            (LPARAM) (s ? s : ""));
    }
    if (cur >= 0 && cur < n)
        SendDlgItemMessage(hwndDlg, IDC_PROPSTR, CB_SETCURSEL, cur, 0);
}

INT_PTR CALLBACK DIAPropEditProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    char mytext[80];
    LPMEASUREITEMSTRUCT itempt;
    LPDRAWITEMSTRUCT drawpt;
    int i, j, k, l, m;
    HDC fromhdc;
    HBITMAP temphbm, hbmpold;
    unsigned char * destmempt;
    BITMAPINFOHEADER bih;
    RGBQUAD rgbq;
    DWORD lDlgBaseUnits;
    BLKSTR * myblkstrpt;

    switch (msg) {
    case WM_INITDIALOG:
        LuaRunBlockProp(editnum);
        if (propedit_haspos)
            SetWindowPos(hwndDlg, NULL, propedit_rc.left, propedit_rc.top,
            0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        sprintf(mytext, "Block Structure %d", editnum);
        SetDlgItemText(hwndDlg, IDC_PROPNUM, mytext);
        /* user-settable dialogue labels (blockdialoguelabels= in mapwin.ini) */
        SetDlgItemText(hwndDlg, IDC_PROPLBLUSER, blocklabels[0]);
        SetDlgItemText(hwndDlg, IDC_PROPTRIGGER, blocklabels[12]);
        if (rotatedtiles) SetDlgItemText(hwndDlg, IDC_PROPLBLUSER, "User data: (U7=rotation 1-3)");
        else if (flippedtiles) SetDlgItemText(hwndDlg, IDC_PROPLBLUSER, "User data: (flipped mode)");
        else if (isopillars) SetDlgItemText(hwndDlg, IDC_PROPLBLUSER, "User data: (pillar mode)");
        myblkstrpt = (BLKSTR *) blockstrpt;
        myblkstrpt += editnum;
        sprintf(mytext, "%08X", (unsigned int) myblkstrpt->user1);
        SetDlgItemText(hwndDlg, IDC_PROPUSER1, mytext);
        sprintf(mytext, "%08X", (unsigned int) myblkstrpt->user2);
        SetDlgItemText(hwndDlg, IDC_PROPUSER2, mytext);
        sprintf(mytext, "%04X", myblkstrpt->user3);
        SetDlgItemText(hwndDlg, IDC_PROPUSER3, mytext);
        sprintf(mytext, "%04X", myblkstrpt->user4);
        SetDlgItemText(hwndDlg, IDC_PROPUSER4, mytext);
        sprintf(mytext, "%02X", myblkstrpt->user5);
        SetDlgItemText(hwndDlg, IDC_PROPUSER5, mytext);
        sprintf(mytext, "%02X", myblkstrpt->user6);
        SetDlgItemText(hwndDlg, IDC_PROPUSER6, mytext);
        sprintf(mytext, "%02X", myblkstrpt->user7);
        SetDlgItemText(hwndDlg, IDC_PROPUSER7, mytext);
        if (myblkstrpt->tl) CheckDlgButton(hwndDlg, IDC_PROPTL, 1); else CheckDlgButton(hwndDlg, IDC_PROPTL, 0);
        if (myblkstrpt->tr) CheckDlgButton(hwndDlg, IDC_PROPTR, 1); else CheckDlgButton(hwndDlg, IDC_PROPTR, 0);
        if (myblkstrpt->bl) CheckDlgButton(hwndDlg, IDC_PROPBL, 1); else CheckDlgButton(hwndDlg, IDC_PROPBL, 0);
        if (myblkstrpt->br) CheckDlgButton(hwndDlg, IDC_PROPBR, 1); else CheckDlgButton(hwndDlg, IDC_PROPBR, 0);
        if (myblkstrpt->trigger) CheckDlgButton(hwndDlg, IDC_PROPTRIGGER, 1); else CheckDlgButton(hwndDlg, IDC_PROPTRIGGER, 0);
        if (myblkstrpt->unused1) CheckDlgButton(hwndDlg, IDC_PROPUNUSED1, 1); else CheckDlgButton(hwndDlg, IDC_PROPUNUSED1, 0);
        if (myblkstrpt->unused2) CheckDlgButton(hwndDlg, IDC_PROPUNUSED2, 1); else CheckDlgButton(hwndDlg, IDC_PROPUNUSED2, 0);
        if (myblkstrpt->unused3) CheckDlgButton(hwndDlg, IDC_PROPUNUSED3, 1); else CheckDlgButton(hwndDlg, IDC_PROPUNUSED3, 0);
        if (hexordec) {
            hexordec = 0;
            DIAHexDecUsers(hwndDlg);
            hexordec = 1;
            CheckDlgButton(hwndDlg, IDC_PROPHEX, 0);
            CheckDlgButton(hwndDlg, IDC_PROPDEC, 1);
        } else {
            CheckDlgButton(hwndDlg, IDC_PROPDEC, 0);
            CheckDlgButton(hwndDlg, IDC_PROPHEX, 1);
        }
        DIAFillStrCombo(hwndDlg);
        break;
    case WM_MEASUREITEM:
        itempt = (LPMEASUREITEMSTRUCT) lParam;
        switch (LOWORD(wParam)) {
        case IDC_PROPVIEW:
            itempt->CtlType = ODT_BUTTON;
            itempt->CtlID = IDC_PROPVIEW;
            itempt->itemID = 0;
            itempt->itemWidth = 100;
            itempt->itemHeight = 100;
            itempt->itemData = 0;
            break;
        }
        break;
    case WM_DRAWITEM:
        switch (LOWORD(wParam)) {
        case IDC_PROPVIEW:
            i = 75; j = 75; m = 4;
            break;
        case IDC_PROPBG:
            i = 18; j = 18; m = 0;
            break;
        case IDC_PROPFG1:
            i = 18; j = 18; m = 1;
            break;
        case IDC_PROPFG2:
            i = 18; j = 18; m = 2;
            break;
        case IDC_PROPFG3:
        default:
            i = 18; j = 18; m = 3;
            break;
        }
        drawpt = (LPDRAWITEMSTRUCT) lParam;
        k = blockwidth; k += 3; k &= -4;
        l = blockheight;

        destmempt = malloc(k*l*3);
        if (destmempt==NULL) break;
        memset(destmempt, 0x44, k*l*3);

        fromhdc = CreateCompatibleDC(drawpt->hDC);

        memset(&bih, 0, sizeof(BITMAPINFOHEADER));
        memset(&rgbq, 0, sizeof(RGBQUAD));
        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = k;
        bih.biHeight = -l;
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;
        if (m>3) BlockStrto24(editnum, destmempt, 0, 0, k*3);
        else BlockSecto24(editnum, destmempt, 0, 0, k*3, m);
        temphbm = CreateDIBitmap(drawpt->hDC, &bih, CBM_INIT,
            destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);

        hbmpold = SelectObject(fromhdc, temphbm);
        lDlgBaseUnits = GetDialogBaseUnits();
        StretchBlt(drawpt->hDC, 0, 0, (i*LOWORD(lDlgBaseUnits))/4, (j*HIWORD(lDlgBaseUnits))/8,
            fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
        SelectObject(fromhdc, hbmpold);
        DeleteObject(temphbm);

        DeleteDC(fromhdc);
        free(destmempt);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_PROPHEX:
        case IDC_PROPDEC:
            if (SendDlgItemMessage(hwndDlg, IDC_PROPHEX, BM_GETCHECK, 0, 0)) {
                DIAHexDecUsers(hwndDlg);
                hexordec = 0;
                CheckDlgButton(hwndDlg, IDC_PROPDEC, 0);
                CheckDlgButton(hwndDlg, IDC_PROPHEX, 1);
            } else {
                DIAHexDecUsers(hwndDlg);
                hexordec = 1;
                CheckDlgButton(hwndDlg, IDC_PROPHEX, 0);
                CheckDlgButton(hwndDlg, IDC_PROPDEC, 1);
            }
            //				DIAFillUsers (hwndDlg);
            break;
        case IDC_PROPBG:
            DIAFillUsers(hwndDlg);
            SendMessage(hwndDlg, WM_CLOSE, 0, 3L);
            break;
        case IDC_PROPFG1:
            DIAFillUsers(hwndDlg);
            SendMessage(hwndDlg, WM_CLOSE, 0, 4L);
            break;
        case IDC_PROPFG2:
            DIAFillUsers(hwndDlg);
            SendMessage(hwndDlg, WM_CLOSE, 0, 5L);
            break;
        case IDC_PROPFG3:
            DIAFillUsers(hwndDlg);
            SendMessage(hwndDlg, WM_CLOSE, 0, 6L);
            break;
        case IDC_PROPPREV:
            DIAFillUsers(hwndDlg);
            SendMessage(hwndDlg, WM_CLOSE, 0, 1L);
            break;
        case IDC_PROPNEXT:
            DIAFillUsers(hwndDlg);
            SendMessage(hwndDlg, WM_CLOSE, 0, 2L);
            break;
        case IDC_PROPUNUSED2:
        case IDC_PROPUNUSED3:
            /* Flip H/V flags: redraw the previews in place instead of
                 * closing + reopening the dialog (which reset its position) */
            DIAFillUsers(hwndDlg);
            InvalidateRect(GetDlgItem(hwndDlg, IDC_PROPVIEW), NULL, TRUE);
            InvalidateRect(GetDlgItem(hwndDlg, IDC_PROPBG), NULL, TRUE);
            InvalidateRect(GetDlgItem(hwndDlg, IDC_PROPFG1), NULL, TRUE);
            InvalidateRect(GetDlgItem(hwndDlg, IDC_PROPFG2), NULL, TRUE);
            InvalidateRect(GetDlgItem(hwndDlg, IDC_PROPFG3), NULL, TRUE);
            break;
        case IDOK:
            DIAFillUsers(hwndDlg);
            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
            break;
        }
        break;
    case WM_CLOSE:
        if (lParam!=0 && IsWindowVisible(hwndDlg)) {
            /* reopen follows (prev/next/pick): remember position */
            GetWindowRect(hwndDlg, &propedit_rc);
            propedit_haspos = 1;
        } else if (lParam==0) {
            propedit_haspos = 0;
        }
        EndDialog(hwndDlg, lParam);
        break;
    default:
        return 0;
    }
    return -1;
}

int DIACountAFrames(int anum) {
    ANISTR * myanimpt;

    myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
    myanimpt -= anum; myanimpt--;
    return (myanimpt->anendoff-myanimpt->anstartoff)/4;
}

INT_PTR CALLBACK DIAAnimEditProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    char mytext[80];
    LPMEASUREITEMSTRUCT itempt;
    LPDRAWITEMSTRUCT drawpt;
    int i, j, k, l, m;
    HDC fromhdc;
    HBITMAP temphbm, hbmpold;
    unsigned char * destmempt;
    BITMAPINFOHEADER bih;
    RGBQUAD rgbq;
    DWORD lDlgBaseUnits;
    ANISTR * myanimpt;

    switch (msg) {
    case WM_INITDIALOG:
        myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
        myanimpt -= editnum; myanimpt--;

        sprintf(mytext, "Anim %d, edit point is %d/%d", editnum, editpoint, DIACountAFrames(editnum));
        SetDlgItemText(hwndDlg, IDC_ANIMPOINT, mytext);
        sprintf(mytext, "%d", myanimpt->andelay);
        SetDlgItemText(hwndDlg, IDC_ANIMDELAY, mytext);
        SendMessage(hwndDlg, WM_COMMAND, 99, 0L);
        break;
    case WM_MEASUREITEM:
        itempt = (LPMEASUREITEMSTRUCT) lParam;
        switch (LOWORD(wParam)) {
        case IDC_ANIMREF:
            itempt->CtlType = ODT_BUTTON;
            itempt->CtlID = IDC_ANIMREF;
            itempt->itemID = 0;
            itempt->itemWidth = 32;
            itempt->itemHeight = 32;
            itempt->itemData = 0;
            break;
        case IDC_ANIMFRAMES:
            itempt->CtlType = ODT_LISTBOX;
            itempt->CtlID = IDC_ANIMFRAMES;
            itempt->itemID = 0;
            itempt->itemWidth = 64;
            itempt->itemHeight = 64;
            itempt->itemData = 0;
            break;
        }
        break;
    case WM_DRAWITEM:
        drawpt = (LPDRAWITEMSTRUCT) lParam;
        k = blockwidth*3; k += 3; k &= -4;
        l = blockheight;
        switch (LOWORD(wParam)) {
        case IDC_ANIMREF:
            i = 24; j = 24; m = -1;
            break;
        case IDC_ANIMFRAMES:
        default:
            i = 32; j = 32;
            m = drawpt->itemID;
            if (m<0) m = -2;
            if (m==DIACountAFrames(editnum)) m = -3;
            break;
        }

        if (m==-2) break;
        destmempt = malloc(k*l);
        if (destmempt==NULL) break;
        memset(destmempt, 0x44, k*l);

        fromhdc = CreateCompatibleDC(drawpt->hDC);

        memset(&bih, 0, sizeof(BITMAPINFOHEADER));
        memset(&rgbq, 0, sizeof(RGBQUAD));
        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = blockwidth;
        bih.biHeight = -l;
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;
        if (m==-3) memset(destmempt, 0xFF, blockheight*k);
        else BlockAnimto24(editnum, m, destmempt, 0, 0, k);
        if ((drawpt->itemState&ODS_SELECTED || drawpt->itemState&ODS_FOCUS) && m!=-1) {
            for (m=0;m<blockwidth;m++) {
                if ((m%4)<2) {
                    destmempt[(m*3)] = 255;
                    destmempt[(m*3)+1] = 255;
                    destmempt[(m*3)+2] = 255;
                } else {
                    destmempt[(m*3)] = 0;
                    destmempt[(m*3)+1] = 0;
                    destmempt[(m*3)+2] = 0;
                }
            }
            for (m=0;m<blockheight;m++) {
                if ((m%4)<2) {
                    destmempt[(m*k)] = 0;
                    destmempt[(m*k)+1] = 0;
                    destmempt[(m*k)+2] = 0;
                    destmempt[(m*k)+3] = 255;
                    destmempt[(m*k)+4] = 255;
                    destmempt[(m*k)+5] = 255;
                } else {
                    destmempt[(m*k)] = 255;
                    destmempt[(m*k)+1] = 255;
                    destmempt[(m*k)+2] = 255;
                    destmempt[(m*k)+3] = 0;
                    destmempt[(m*k)+4] = 0;
                    destmempt[(m*k)+5] = 0;
                }
            }
            for (m=0;m<blockwidth;m++) {
                if ((m%4)<2) {
                    destmempt[((blockheight-1)*k)+(m*3)] = 255;
                    destmempt[((blockheight-1)*k)+(m*3)+1] = 255;
                    destmempt[((blockheight-1)*k)+(m*3)+2] = 255;
                } else {
                    destmempt[((blockheight-1)*k)+(m*3)] = 0;
                    destmempt[((blockheight-1)*k)+(m*3)+1] = 0;
                    destmempt[((blockheight-1)*k)+(m*3)+2] = 0;
                }
            }
        }

        temphbm = CreateDIBitmap(drawpt->hDC, &bih, CBM_INIT,
            destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);

        hbmpold = SelectObject(fromhdc, temphbm);
        lDlgBaseUnits = GetDialogBaseUnits();
        StretchBlt(drawpt->hDC, ((drawpt->rcItem.left/2)*LOWORD(lDlgBaseUnits))/4, 0, (i*LOWORD(lDlgBaseUnits))/4, (j*HIWORD(lDlgBaseUnits))/8,
            fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
        SelectObject(fromhdc, hbmpold);
        DeleteObject(temphbm);

        DeleteDC(fromhdc);
        free(destmempt);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 99:
            SendDlgItemMessage(hwndDlg, IDC_ANIMTYPE, CB_RESETCONTENT, 0, 0);
            i = 0; while (andescs[i]!=NULL) {
                SendDlgItemMessage(hwndDlg, IDC_ANIMTYPE, CB_ADDSTRING, 0,
                    (LPARAM) ((LPSTR) andescs[i])); i++; }
            SendDlgItemMessage(hwndDlg, IDC_ANIMTYPE, CB_SETITEMHEIGHT,
                0, 14);
            myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
            myanimpt -= editnum; myanimpt--;
            SendDlgItemMessage(hwndDlg, IDC_ANIMTYPE, CB_SELECTSTRING, -1,
                (LPARAM) ((LPSTR) andescs[myanimpt->antype]));

            SendDlgItemMessage(hwndDlg, IDC_ANIMFRAMES, LB_RESETCONTENT, 0, 0);
            lDlgBaseUnits = GetDialogBaseUnits();
            i = 0; while (i<=(DIACountAFrames(editnum))) {
                sprintf(mytext, "%d", i);
                SendDlgItemMessage(hwndDlg, IDC_ANIMFRAMES, LB_ADDSTRING, 0,
                    (LPARAM) ((LPSTR) mytext));
                SendDlgItemMessage(hwndDlg, IDC_ANIMFRAMES, LB_SETITEMHEIGHT,
                    i, (32*HIWORD(lDlgBaseUnits))/8); i++; }
            SendDlgItemMessage(hwndDlg, IDC_ANIMFRAMES, LB_SETCOLUMNWIDTH, 64, 0L);
            SendDlgItemMessage(hwndDlg, IDC_ANIMFRAMES, LB_SETCURSEL, editpoint, 0L);
            break;
        case IDC_ANIMFRAMES:
            editpoint = SendDlgItemMessage(hwndDlg, IDC_ANIMFRAMES, LB_GETCARETINDEX, 0, 0L);
            sprintf(mytext, "Anim %d, edit point is %d/%d", editnum, editpoint, DIACountAFrames(editnum));
            SetDlgItemText(hwndDlg, IDC_ANIMPOINT, mytext);
            break;
        case IDC_ANIMREF:
            myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
            myanimpt -= editnum; myanimpt--;
            GetDlgItemText(hwndDlg, IDC_ANIMDELAY, mytext, 80);
            i = atoi(mytext);
            if (i>-1 && i<100) myanimpt->andelay = (signed char) i;
            GetDlgItemText(hwndDlg, IDC_ANIMTYPE, mytext, 80);
            i = 0; while (andescs[i]!=NULL) { if (!strcmp(mytext, andescs[i])) myanimpt->antype = (signed char) i; i++; }
            SendMessage(hwndDlg, WM_CLOSE, 0, 1L);
            break;
        case IDC_ANIMEPINS:
            myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
            myanimpt -= editnum; myanimpt--;
            GetDlgItemText(hwndDlg, IDC_ANIMDELAY, mytext, 80);
            i = atoi(mytext);
            if (i>-1 && i<100) myanimpt->andelay = (signed char) i;
            GetDlgItemText(hwndDlg, IDC_ANIMTYPE, mytext, 80);
            i = 0; while (andescs[i]!=NULL) { if (!strcmp(mytext, andescs[i])) myanimpt->antype = (signed char) i; i++; }
            SendMessage(hwndDlg, WM_CLOSE, 0, 2L);
            break;
        case IDC_ANIMEPCUT:
            if (editpoint==DIACountAFrames(editnum)) break;
            DelAnimFrame(editnum, editpoint);
            SendDlgItemMessage(hwndDlg, IDC_ANIMFRAMES, LB_DELETESTRING, editpoint, 0);
            SendDlgItemMessage(hwndDlg, IDC_ANIMFRAMES, LB_SETCURSEL, editpoint, 0L);

            sprintf(mytext, "Anim %d, edit point is %d/%d", editnum, editpoint, DIACountAFrames(editnum));
            SetDlgItemText(hwndDlg, IDC_ANIMPOINT, mytext);
            break;
        case IDOK:
            myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
            myanimpt -= editnum; myanimpt--;
            GetDlgItemText(hwndDlg, IDC_ANIMDELAY, mytext, 80);
            i = atoi(mytext);
            if (i>-1 && i<100) myanimpt->andelay = (signed char) i;
            GetDlgItemText(hwndDlg, IDC_ANIMTYPE, mytext, 80);
            i = 0; while (andescs[i]!=NULL) { if (!strcmp(mytext, andescs[i])) myanimpt->antype = (signed char) i; i++; }
            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
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

INT_PTR CALLBACK DIAPickGfxProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    long int * templpt;
    int i, j, k, l, m;
    HDC fromhdc;
    HBITMAP temphbm, hbmpold;
    unsigned char * destmempt;
    BITMAPINFOHEADER bih;
    RGBQUAD rgbq;
    DWORD lDlgBaseUnits;
    LPMEASUREITEMSTRUCT itempt;
    LPDRAWITEMSTRUCT drawpt;
    BLKSTR * myblkstr;
    ANISTR * myanimpt;
    char textbuf[256];
    char * partpt = "";

    switch (msg) {
    case WM_INITDIALOG:
        myblkstr = (BLKSTR*) blockstrpt;
        myblkstr += editnum;
        if (lParam<4) { SetWindowText(hwndDlg, "Pick Block Graphic");
            pickwhat = 0;
        } else { SetWindowText(hwndDlg, "Pick Block Structure"); pickwhat = 1; }
        switch (lParam) {
        case 0:
            partpt = "BG";
            pgfxnum = (myblkstr->bgoff)/(blockwidth*blockheight*((blockdepth+1)/8));
            break;
        case 1:
            partpt = "FG1";
            pgfxnum = (myblkstr->fgoff)/(blockwidth*blockheight*((blockdepth+1)/8));
            break;
        case 2:
            partpt = "FG2";
            pgfxnum = (myblkstr->fgoff2)/(blockwidth*blockheight*((blockdepth+1)/8));
            break;
        case 3:
            partpt = "FG3";
            pgfxnum = (myblkstr->fgoff3)/(blockwidth*blockheight*((blockdepth+1)/8));
            break;
        case 4:
            myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
            myanimpt--; myanimpt -= editnum; templpt = (long int *) ((animstrpt+ANDTMEM)+(myanimpt->anstartoff));
            templpt--;
            pgfxnum = ((*templpt)/sizeof(BLKSTR));
            break;
        case 5:
        default:
            myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
            myanimpt--; myanimpt -= editnum; templpt = (long int *) ((animstrpt+ANDTMEM)+(myanimpt->anstartoff));
            templpt--;
            pgfxnum = ((*templpt)/sizeof(BLKSTR));
            pickwhat = 2;
            break;
        }
        if (lParam<4) sprintf(textbuf, "Select a graphic for the %s field of block structure %d", partpt, editnum);
        if (lParam==4) sprintf(textbuf, "Choose the reference structure for anim %d", editnum);
        if (lParam==5) sprintf(textbuf, "Choose up to 32 frames to add to anim %d", editnum);
        SetWindowText(GetDlgItem(hwndDlg, IDC_PICKTEXT), textbuf);
        if (pickwhat==2) sprintf(textbuf, "Add frames by clicking them");
        if (pickwhat==1) sprintf(textbuf, "Current structure is %d", pgfxnum);
        if (pickwhat==0) sprintf(textbuf, "Current graphic is %d", pgfxnum);
        SetWindowText(GetDlgItem(hwndDlg, IDC_PICKINFO), textbuf);
        SendMessage(hwndDlg, WM_COMMAND, 99, 0L);
        break;
    case WM_MEASUREITEM:
        itempt = (LPMEASUREITEMSTRUCT) lParam;
        switch (LOWORD(wParam)) {
        case IDC_PICKVIEW:
            itempt->CtlType = ODT_LISTBOX;
            itempt->CtlID = IDC_PICKVIEW;
            itempt->itemID = 0;
            itempt->itemWidth = 64;
            itempt->itemHeight = 64;
            itempt->itemData = 0;
            break;
        }
        break;
    case WM_DRAWITEM:
        if (LOWORD(wParam)!=IDC_PICKVIEW) break;
        drawpt = (LPDRAWITEMSTRUCT) lParam;
        k = blockwidth*3; k += 3; k &= -4;
        l = blockheight;
        m = drawpt->itemID;
        if (m<0) break;

        destmempt = malloc(k*l);
        if (destmempt==NULL) break;
        memset(destmempt, 0x44, k*l);

        fromhdc = CreateCompatibleDC(drawpt->hDC);

        memset(&bih, 0, sizeof(BITMAPINFOHEADER));
        memset(&rgbq, 0, sizeof(RGBQUAD));
        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = blockwidth;
        bih.biHeight = -l;
        bih.biPlanes = 1;
        bih.biBitCount = 24;
        bih.biCompression = BI_RGB;
        if (pickwhat) BlockStrto24(m, destmempt, 0, 0, k);
        else BlockGfxto24(m, destmempt, 0, 0, k);
        if (drawpt->itemState&ODS_SELECTED || drawpt->itemState&ODS_FOCUS) {
            for (i=0;i<blockwidth;i++) {
                if ((i%4)<2) {
                    destmempt[(i*3)] = 255;
                    destmempt[(i*3)+1] = 255;
                    destmempt[(i*3)+2] = 255;
                } else {
                    destmempt[(i*3)] = 0;
                    destmempt[(i*3)+1] = 0;
                    destmempt[(i*3)+2] = 0;
                }
            }
            for (j=1;j<(blockheight-1);j++) {
                if ((j%4)<2) {
                    destmempt[(j*k)] = 255;
                    destmempt[(j*k)+1] = 255;
                    destmempt[(j*k)+2] = 255;
                    destmempt[(j*k)+((blockwidth-1)*3)] = 255;
                    destmempt[(j*k)+((blockwidth-1)*3)+1] = 255;
                    destmempt[(j*k)+((blockwidth-1)*3)+2] = 255;
                } else {
                    destmempt[(j*k)] = 0;
                    destmempt[(j*k)+1] = 0;
                    destmempt[(j*k)+2] = 0;
                    destmempt[(j*k)+((blockwidth-1)*3)] = 0;
                    destmempt[(j*k)+((blockwidth-1)*3)+1] = 0;
                    destmempt[(j*k)+((blockwidth-1)*3)+2] = 0;
                }
            }
            for (i=0;i<blockwidth;i++) {
                if ((i%4)<2) {
                    destmempt[((blockheight-1)*k)+(i*3)] = 255;
                    destmempt[((blockheight-1)*k)+(i*3)+1] = 255;
                    destmempt[((blockheight-1)*k)+(i*3)+2] = 255;
                } else {
                    destmempt[((blockheight-1)*k)+(i*3)] = 0;
                    destmempt[((blockheight-1)*k)+(i*3)+1] = 0;
                    destmempt[((blockheight-1)*k)+(i*3)+2] = 0;
                }
            }
        }
        temphbm = CreateDIBitmap(drawpt->hDC, &bih, CBM_INIT,
            destmempt, (BITMAPINFO *) &bih, DIB_RGB_COLORS);

        hbmpold = SelectObject(fromhdc, temphbm);
        lDlgBaseUnits = GetDialogBaseUnits();
        StretchBlt(drawpt->hDC, 0, ((drawpt->rcItem.top/2)*HIWORD(lDlgBaseUnits))/8, (32*LOWORD(lDlgBaseUnits))/4, (32*HIWORD(lDlgBaseUnits))/8,
            fromhdc, 0, 0, blockwidth, blockheight, SRCCOPY);
        SelectObject(fromhdc, hbmpold);
        DeleteObject(temphbm);

        DeleteDC(fromhdc);
        free(destmempt);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_PICKVIEW:
            pgfxnum = SendDlgItemMessage(hwndDlg, IDC_PICKVIEW, LB_GETCARETINDEX, 0, 0L);
            if (pickwhat==2) {
                if (HIWORD(wParam)==1) { i = 0; while (i<32) {
                        if (newframes[i]==-1) { newframes[i] = (short int) pgfxnum; newframes[i+1] = -1; break; }
                        i++;
                } }
                sprintf(textbuf, "Current structures are: ");
                i = 0; while (i<32) {
                    if (newframes[i]==-1) break;
                    sprintf(textbuf+strlen(textbuf), "%d, ", newframes[i]);
                    i++;
                }
            }
            if (pickwhat==1) sprintf(textbuf, "Current structure is %d", pgfxnum);
            if (pickwhat==0) sprintf(textbuf, "Current graphic is %d", pgfxnum);
            SetWindowText(GetDlgItem(hwndDlg, IDC_PICKINFO), textbuf);
            break;
        case 99:
            SendDlgItemMessage(hwndDlg, IDC_PICKVIEW, LB_RESETCONTENT, 0, 0);
            lDlgBaseUnits = GetDialogBaseUnits();
            if (pickwhat) j = numblockstr;
            else j = numblockgfx;
            i = 0; while (i<j) {
                sprintf(textbuf, "%d", i);
                SendDlgItemMessage(hwndDlg, IDC_PICKVIEW, LB_ADDSTRING, 0,
                    (LPARAM) ((LPSTR) textbuf));
                SendDlgItemMessage(hwndDlg, IDC_PICKVIEW, LB_SETITEMHEIGHT,
                    i, (32*HIWORD(lDlgBaseUnits))/8); i++; }
            if (pickwhat!=2) SendDlgItemMessage(hwndDlg, IDC_PICKVIEW, LB_SETCURSEL, pgfxnum, 0L);
            break;
        case IDOK:
            pgfxnum = SendDlgItemMessage(hwndDlg, IDC_PICKVIEW, LB_GETCARETINDEX, 0, 0L);
            SendMessage(hwndDlg, WM_CLOSE, 0, pgfxnum);
            break;
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

INT_PTR CALLBACK DIAResizeProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    char textbuf[80];

    switch (msg) {
    case WM_INITDIALOG:
        editnum = 5;
        SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 5");
        sprintf(textbuf, "%d", mapwidth);
        SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZEWIDTH), textbuf);
        sprintf(textbuf, "%d", mapheight);
        SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZEHEIGHT), textbuf);
        SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZELEFT), "0");
        SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZETOP), "0");
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_RESIZE1:
            editnum = 1;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 1");
            break;
        case IDC_RESIZE2:
            editnum = 2;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 2");
            break;
        case IDC_RESIZE3:
            editnum = 3;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 3");
            break;
        case IDC_RESIZE4:
            editnum = 4;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 4");
            break;
        case IDC_RESIZE5:
            editnum = 5;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 5");
            break;
        case IDC_RESIZE6:
            editnum = 6;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 6");
            break;
        case IDC_RESIZE7:
            editnum = 7;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 7");
            break;
        case IDC_RESIZE8:
            editnum = 8;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 8");
            break;
        case IDC_RESIZE9:
            editnum = 9;
            SetWindowText(GetDlgItem(hwndDlg, IDC_RESIZECENTER), "Map Centering = 9");
            break;
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_RESIZEWIDTH), textbuf, 80);
            newmapwidth = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_RESIZEHEIGHT), textbuf, 80);
            newmapheight = (short int) atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_RESIZELEFT), textbuf, 80);
            resize_left = atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_RESIZETOP), textbuf, 80);
            resize_top = atoi(textbuf);
            if (resize_left || resize_top)
                SendMessage(hwndDlg, WM_CLOSE, 0, 0);
            else
                SendMessage(hwndDlg, WM_CLOSE, 0, editnum);
            break;
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

INT_PTR CALLBACK DIAExportProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i;
    char mytext[80];

    switch (msg) {
    case WM_INITDIALOG:
        CheckDlgButton(hwndDlg, IDC_EXPORTCMA, 0);
        CheckDlgButton(hwndDlg, IDC_EXPORTABD, 0);
        CheckDlgButton(hwndDlg, IDC_EXPORTMAR, 0);
        CheckDlgButton(hwndDlg, IDC_EXPORTGFX, 0);
        CheckDlgButton(hwndDlg, IDC_EXPORTBMP, 0);
        CheckDlgButton(hwndDlg, IDC_EXPORTSKIP, 0);
        CheckDlgButton(hwndDlg, IDC_EXPORTCSV, 0);
        CheckDlgButton(hwndDlg, IDC_EXPORTCSVBG, csvusebg ? 1 : 0);
        CheckDlgButton(hwndDlg, IDC_EXPORTSCR, 0);
        SetWindowText(GetDlgItem(hwndDlg, IDC_EXPORTROW), "20");
        if (strlen(expname) > 4) {
            i = strlen(expname);
            while (i>0) {
                if (expname[i] == '.') expname[i] = 0;
                if (expname[i] == '/' || expname[i] == '\\' || expname[i] == ':') break;
                i--;
            }
            if (i!=0) i++;
            SetWindowText(GetDlgItem(hwndDlg, IDC_EXPORTNAME), expname+i);
            expname[i] = 0;
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            GetDlgItemText(hwndDlg, IDC_EXPORTNAME, expname+strlen(expname), 80);
            i = 0;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTCMA)) i |= EXPRT_CMA;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTABD)) i |= EXPRT_ABD;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTMAR)) i |= EXPRT_MAR;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTGFX)) i |= EXPRT_GFX;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTBMP)) i |= EXPRT_BMP;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTSKIP)) i |= EXPRT_SKP;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTCSV)) i |= EXPRT_CSV;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTSCR)) i |= EXPRT_SCR;
            if (IsDlgButtonChecked(hwndDlg, IDC_EXPORTCSVBG)) csvusebg = 1; else csvusebg = 0;

            GetDlgItemText(hwndDlg, IDC_EXPORTROW, mytext, 80);
            i<<=8; i |= atoi(mytext);

            SendMessage(hwndDlg, WM_CLOSE, 0, i);
            break;
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

INT_PTR CALLBACK DIAEditInfoProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i, j, k, l;
    ANISTR * myanpt;
    unsigned char * tblkpt;
    unsigned char tcolval;
    OPENFILENAME ofn;
    char szFilter[80];
    char szFile[256];
    char szFileTitle[256];
    char szInitDir[256];
    char mytext[280];

    switch (msg) {
    case WM_INITDIALOG:
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOAUTH), authorname);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOLNE1), authorinfo1);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOLNE2), authorinfo2);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOLNE3), authorinfo3);

        sprintf(mytext, "%d", (int) trans8bit);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOCKEY8), mytext);
        i = (int) transred; i <<= 8;
        i |= (int) transgreen; i <<= 8;
        i |= (int) transblue;
        sprintf(mytext, "%06X", i);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOCKEY24), mytext);

        sprintf(mytext, "%d", (int) blockgapx);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOGAPX), mytext);
        sprintf(mytext, "%d", (int) blockgapy);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOGAPY), mytext);
        sprintf(mytext, "%d", (int) blockstaggerx);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOSTAGX), mytext);
        sprintf(mytext, "%d", (int) blockstaggery);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOSTAGY), mytext);
        strcpy(mytext, novctext);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFONOCACHE), mytext);
        sprintf(mytext, "%d", (int) clickmask);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOCLICKNUM), mytext);

        snprintf(mytext, sizeof(mytext), "Filename: %s", mapfilename);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFONAME), mytext);
        totalmapmem = 0;
        i = 0; for (j=0;j<MAXMAPLAYERS;j++) { if (mapmappt[j]!=NULL) i++; }
        sprintf(mytext, "Map Array: Width=%d, Height=%d, Layers=%d, (%d)", mapwidth, mapheight, i,
            mapwidth*mapheight*2*i);
        totalmapmem += mapwidth*mapheight*2*i;
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOMAR), mytext);
        sprintf(mytext, "Block Str: Amount=%d, Size (each)=%d, (%d)", numblockstr, BLOCKSTRSIZE,
            numblockstr*BLOCKSTRSIZE);
        totalmapmem += numblockstr*BLOCKSTRSIZE;
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOBSTR), mytext);
        if (animstrpt==NULL) SetWindowText(GetDlgItem(hwndDlg, IDC_INFOANIM), "Anim Str: Map does not contain animations");
        else {
            j = 0; myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
            while (myanpt->antype!=-1) { j++; myanpt--; }
            k = 0; l = j; if (j) {
                myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
                while (myanpt->antype!=-1) { if ((myanpt->anstartoff)<k) k = myanpt->anstartoff; myanpt--; }
                k -= 4;
            }
            sprintf(mytext, "Anim Str: Amount=%d, (%d)", l, -k);
            totalmapmem += -k;
            SetWindowText(GetDlgItem(hwndDlg, IDC_INFOANIM), mytext);
        }
        i = 0; k = 0; for (j=0;j<MAXBRSH;j++) { if (brshpt[j]!=NULL) {
                i++; k += ((brshpt[j][0])*(brshpt[j][1])*2)+4; } }
        totalmapmem += k;
        sprintf(mytext, "Brushes: Amount=%d, (%d)", i, k);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOBRSH), mytext);
        sprintf(mytext, "Graphics: Amount=%d, Form=%dx%dx%d, (%d)", numblockgfx,
            blockwidth, blockheight, blockdepth, numblockgfx*blockwidth*blockheight*((blockdepth+1)/8));
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOBGFX), mytext);
        totalmapmem += numblockgfx*blockwidth*blockheight*((blockdepth+1)/8);
        if (altgfxpt == NULL) {
            if (blockdepth == 8)
                SetWindowText(GetDlgItem(hwndDlg, IDC_INFOAGFX), "Alt Graphics: Not needed (0)");
            else
                SetWindowText(GetDlgItem(hwndDlg, IDC_INFOAGFX), "Alt Graphics: Not loaded (0)");
        } else {
            sprintf(mytext, "Alt Graphics: Loaded (%d)", numblockgfx*blockwidth*blockheight);
            SetWindowText(GetDlgItem(hwndDlg, IDC_INFOAGFX), mytext);
            totalmapmem += (numblockgfx*blockwidth*blockheight);
        }
        sprintf(mytext, "Total memory: %d bytes (%dK)", totalmapmem, totalmapmem/1024);
        SetWindowText(GetDlgItem(hwndDlg, IDC_INFOTOTAL), mytext);
        SendMessage(hwndDlg, WM_COMMAND, 99, 0L);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 99:
            SendDlgItemMessage(hwndDlg, IDC_INFOFMPVER, CB_RESETCONTENT, 0, 0);
            i = 0; while (versionstexts[i]!=NULL) {
                SendDlgItemMessage(hwndDlg, IDC_INFOFMPVER, CB_ADDSTRING, 0,
                    (LPARAM) ((LPSTR) versionstexts[i])); i++; }
            SendDlgItemMessage(hwndDlg, IDC_INFOFMPVER, CB_SETITEMHEIGHT,
                0, 14);
            SendDlgItemMessage(hwndDlg, IDC_INFOFMPVER, CB_SELECTSTRING, -1,
                (LPARAM) ((LPSTR) versionstexts[maptype]));
            break;
        case IDC_INFOAGFXFREE:
            if (altgfxpt != NULL) {
                free(altgfxpt);
                altgfxpt = NULL;
                SetWindowText(GetDlgItem(hwndDlg, IDC_INFOAGFX), "Alt Graphics: Not loaded (0)");
                totalmapmem -= (numblockgfx*blockwidth*blockheight);
                sprintf(mytext, "Total memory: %d bytes (%dK)", totalmapmem, totalmapmem/1024);
                SetWindowText(GetDlgItem(hwndDlg, IDC_INFOTOTAL), mytext);
            }
            break;
        case IDC_INFOAGFXLOAD:
            if (mappt==NULL) break;
            if (blockdepth==8) {
                MessageBox(hwndDlg, "This feature is only used with 15/16/24/32bit maps", "Alt Graphics", MB_OK|MB_ICONINFORMATION);
                break;
            }
            szFile[0] = 0; szFileTitle[0] = 0; szInitDir[0] = 0;
            memset(szFilter, 0, sizeof(szFilter));
            strcpy(szFilter, "Image files (*.BMP;*.PCX)|*.bmp;*.pcx");
            szFilter[25] = 0;
            memset(&ofn, 0, sizeof(OPENFILENAME));

            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hwndDlg;
            ofn.lpstrFilter = szFilter;
            ofn.nFilterIndex = 1;
            ofn.lpstrFile= szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFileTitle = szFileTitle;
            ofn.nMaxFileTitle = sizeof(szFileTitle);
            ofn.lpstrInitialDir = szInitDir;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

            if (GetOpenFileName(&ofn)) {
                if (altgfxpt != NULL) {
                    free(altgfxpt);
                    altgfxpt = NULL;
                    SetWindowText(GetDlgItem(hwndDlg, IDC_INFOAGFX), "Alt 8bit Graphics: Not loaded (0)");
                    totalmapmem -= (numblockgfx*blockwidth*blockheight);
                    sprintf(mytext, "Total memory: %d bytes (%dK)", totalmapmem, totalmapmem/1024);
                    SetWindowText(GetDlgItem(hwndDlg, IDC_INFOTOTAL), mytext);
                }
                altgfxpt = malloc((numblockgfx+1)*blockwidth*blockheight);
                if (altgfxpt == NULL) {
                    MessageBox(hwndDlg, "Memory allocation failed", "Alt Graphics", MB_OK|MB_ICONEXCLAMATION);
                    break;
                }
                memset(altgfxpt, 0, blockwidth*blockheight);
                if (IMImportBlocks(szFile, -2)) {
                    MessageBox(hwndDlg, "Failed because of incompatible BMP or wrong number of blocks", "Alt Graphics", MB_OK|MB_ICONEXCLAMATION);
                    free(altgfxpt);
                    altgfxpt = NULL;
                    break;
                }
                sprintf(mytext, "Alt Graphics: Loaded (%d)", numblockgfx*blockwidth*blockheight);
                SetWindowText(GetDlgItem(hwndDlg, IDC_INFOAGFX), mytext);
                totalmapmem += (numblockgfx*blockwidth*blockheight);
                sprintf(mytext, "Total memory: %d bytes (%dK)", totalmapmem, totalmapmem/1024);
                SetWindowText(GetDlgItem(hwndDlg, IDC_INFOTOTAL), mytext);
                for (i=0;i<256;i++) {
                    if (((unsigned char *)cmappt)[i*3]>>3 == transred>>3 &&
                        ((unsigned char *)cmappt)[i*3+1]>>3 == transgreen>>3 &&
                        ((unsigned char *)cmappt)[i*3+2]>>3 == transblue>>3) break;
                }
                if (i<256 && i != trans8bit) {
                    sprintf(mytext, "Imported %d blocks OK. Swap current transparent index (%d) with 8bit transparent index (%d)?",
                        numblockgfx, i, trans8bit);
                    if (MessageBox(hwndDlg, mytext, "Alt Graphics", MB_YESNO|MB_ICONQUESTION)==IDYES) {
                        tblkpt = (unsigned char *) altgfxpt;
                        for (j=0;j<numblockgfx;j++) {
                            for (k=0;k<(blockwidth*blockheight);k++) {
                                if (*tblkpt == ((unsigned char) i)) *tblkpt = trans8bit;
                                else if (*tblkpt == trans8bit) *tblkpt = ((unsigned char) i);
                                tblkpt++;
                            }
                        }
                        tcolval = cmappt[i*3];
                        cmappt[i*3] = cmappt[((int)trans8bit)*3];
                        cmappt[((int)trans8bit)*3] = tcolval;
                        tcolval = cmappt[i*3+1];
                        cmappt[i*3+1] = cmappt[((int)trans8bit)*3+1];
                        cmappt[((int)trans8bit)*3+1] = tcolval;
                        tcolval = cmappt[i*3+2];
                        cmappt[i*3+2] = cmappt[((int)trans8bit)*3+2];
                        cmappt[((int)trans8bit)*3+2] = tcolval;
                    }
                }
                if (MessageBox(hwndDlg, "Make transparent colour index black?", "Alt Graphics", MB_YESNO|MB_ICONQUESTION)==IDYES) {
                    cmappt[((int) trans8bit)*3] = 0; cmappt[((int) trans8bit)*3+1] = 0; cmappt[((int) trans8bit)*3+2] = 0;
                }
            }
            break;
        case IDOK:
            GetDlgItemText(hwndDlg, IDC_INFOAUTH, authorname, 68);
            GetDlgItemText(hwndDlg, IDC_INFOLNE1, authorinfo1, 68);
            GetDlgItemText(hwndDlg, IDC_INFOLNE2, authorinfo2, 68);
            GetDlgItemText(hwndDlg, IDC_INFOLNE3, authorinfo3, 68);

            GetWindowText(GetDlgItem(hwndDlg, IDC_INFOFMPVER), mytext, 80);
            newmaptype = 0;
            if (!strcmp(mytext, versionstexts[1])) newmaptype = 1;
            if (!strcmp(mytext, versionstexts[2])) newmaptype = 2;
            maptype = newmaptype;

            GetWindowText(GetDlgItem(hwndDlg, IDC_INFOCKEY8), mytext, 63);
            i = atoi(mytext);
            if (i<0 || i>255) {
                MessageBox(hwndDlg, "Illegal 8bit Colourkey Value, need 0 to 255", "Colour key", MB_ICONEXCLAMATION|MB_OK);
                break;
            }
            trans8bit = (unsigned char) i;
            GetWindowText(GetDlgItem(hwndDlg, IDC_INFOCKEY24), mytext, 63);
            sscanf(mytext, "%X", &i);
            if (i<0 || i>0xFFFFFF) {
                MessageBox(hwndDlg, "Illegal 24bit Colourkey Value, need 000000 to FFFFFF", "Colour key", MB_ICONEXCLAMATION|MB_OK);
                break;
            }
            transred = (unsigned char) ((i>>16)&0xFF);
            transgreen = (unsigned char) ((i>>8)&0xFF);
            transblue = (unsigned char) (i&0xFF);
            SetTranspColour();

            GetWindowText(GetDlgItem(hwndDlg, IDC_INFOCLICKNUM), mytext, 63);
            i = atoi(mytext);
            if (i<0 || i>=numblockgfx) {
                MessageBox(hwndDlg, "Illegal Clickmask number (import some graphics first?)", "Clickmask", MB_ICONEXCLAMATION|MB_OK);
                break;
            }
            clickmask = i;

            GetWindowText(GetDlgItem(hwndDlg, IDC_INFOGAPX), mytext, 63);
            blockgapx = atoi(mytext);
            GetWindowText(GetDlgItem(hwndDlg, IDC_INFOGAPY), mytext, 63);
            blockgapy = atoi(mytext);
            GetWindowText(GetDlgItem(hwndDlg, IDC_INFOSTAGX), mytext, 63);
            blockstaggerx = atoi(mytext);
            GetWindowText(GetDlgItem(hwndDlg, IDC_INFOSTAGY), mytext, 63);
            blockstaggery = atoi(mytext);

            memset(mytext, 0, 64);
            GetWindowText(GetDlgItem(hwndDlg, IDC_INFONOCACHE), mytext, 63);
            strcpy(novctext, mytext);

            mapaltered = 1;
            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
            break;
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

INT_PTR CALLBACK DIALayerOnionProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i;
    char textbuf[80];

    switch (msg) {
    case WM_INITDIALOG:
        CheckDlgButton(hwndDlg, IDC_SKINONOFF, 0);
        CheckDlgButton(hwndDlg, IDC_SKINDARKEN, 0);
        CheckDlgButton(hwndDlg, IDC_SKINLOWER, 0);
        if (onion) CheckDlgButton(hwndDlg, IDC_SKINONOFF, 1);
        if (oniondark) CheckDlgButton(hwndDlg, IDC_SKINDARKEN, 1);
        if (onionlower) CheckDlgButton(hwndDlg, IDC_SKINLOWER, 1);
        SendMessage(hwndDlg, WM_COMMAND, 99, 0L);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 99:
            SendDlgItemMessage(hwndDlg, IDC_SKINLAYER, CB_RESETCONTENT, 0, 0);
            i = 0; while (i<MAXMAPLAYERS) { sprintf(textbuf, "%d", i);
                SendDlgItemMessage(hwndDlg, IDC_SKINLAYER, CB_ADDSTRING, 0,
                    (LPARAM) ((LPSTR) textbuf)); i++; }
            SendDlgItemMessage(hwndDlg, IDC_SKINLAYER, CB_ADDSTRING, 0,
                (LPARAM) ((LPSTR) "All"));
            SendDlgItemMessage(hwndDlg, IDC_SKINLAYER, CB_SETITEMHEIGHT,
                0, 14);
            if (onion==ONION_ALL) strcpy(textbuf, "All");
            else if (onion>0) sprintf(textbuf, "%d", onion-1); else strcpy(textbuf, "0");
            SendDlgItemMessage(hwndDlg, IDC_SKINLAYER, CB_SELECTSTRING, -1,
                (LPARAM) ((LPSTR) textbuf));
            break;
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_SKINLAYER), textbuf, 80);
            if (!strcmp(textbuf, "All")) onion = ONION_ALL;
            else { onion = atoi(textbuf); onion++; }
            if (onion<1) onion = 1;
            if (onion>MAXMAPLAYERS && onion!=ONION_ALL) onion = MAXMAPLAYERS;
            if (IsDlgButtonChecked(hwndDlg, IDC_SKINDARKEN)) oniondark = 1; else oniondark = 0;
            if (IsDlgButtonChecked(hwndDlg, IDC_SKINLOWER)) onionlower = 1; else onionlower = 0;
            if (!IsDlgButtonChecked(hwndDlg, IDC_SKINONOFF)) { onion = 0; oniondark = 0; }

            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
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

INT_PTR CALLBACK DIALayerSlideProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i, sldir, slnum, x, y, slwrap;
    short int tmps;
    short int * slmappt;
    char textbuf[80];

    switch (msg) {
    case WM_INITDIALOG:
        SetWindowText(GetDlgItem(hwndDlg, IDC_SLIDENUM), "0");
        CheckDlgButton(hwndDlg, IDC_SLIDEWRAP, 0);
        SendMessage(hwndDlg, WM_COMMAND, 99, 0L);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 99:
            SendDlgItemMessage(hwndDlg, IDC_SLIDEDIR, CB_RESETCONTENT, 0, 0);
            i = 0; while (i<4) {
                SendDlgItemMessage(hwndDlg, IDC_SLIDEDIR, CB_ADDSTRING, 0,
                    (LPARAM) ((LPSTR) lyrdirtexts[i])); i++; }
            SendDlgItemMessage(hwndDlg, IDC_SLIDEDIR, CB_SETITEMHEIGHT,
                0, 14);
            SendDlgItemMessage(hwndDlg, IDC_SLIDEDIR, CB_SELECTSTRING, -1,
                (LPARAM) ((LPSTR) lyrdirtexts[0]));
            break;
        case IDOK:
            if (IsDlgButtonChecked(hwndDlg, IDC_SLIDEWRAP)) slwrap = 1; else slwrap = 0;
            GetWindowText(GetDlgItem(hwndDlg, IDC_SLIDEDIR), textbuf, 80);
            sldir = 0;
            if (!strcmp(textbuf, lyrdirtexts[0])) sldir = 1;
            if (!strcmp(textbuf, lyrdirtexts[1])) sldir = 2;
            if (!strcmp(textbuf, lyrdirtexts[2])) sldir = 3;
            if (!strcmp(textbuf, lyrdirtexts[3])) sldir = 4;
            GetWindowText(GetDlgItem(hwndDlg, IDC_SLIDENUM), textbuf, 80);
            slnum = atoi(textbuf);
            if (slnum < 1 || slnum > 10000) {
                MessageBox(hwndDlg, "Slide value too low or high", "Layer Slide", MB_ICONEXCLAMATION|MB_OK);
                SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
                break;
            }
            while (slnum) {
                switch (sldir) {
                case 1:
                    for (x=0;x<mapwidth;x++) {
                        slmappt = mappt;
                        slmappt += x;
                        if (slwrap) tmps = *slmappt; else tmps = 0;
                        for (y=0;y<(mapheight-1);y++) {
                            *slmappt = slmappt[mapwidth];
                            slmappt += mapwidth;
                        }
                        *slmappt = tmps;
                    }
                    break;
                case 2:
                    for (x=0;x<mapwidth;x++) {
                        slmappt = mappt;
                        slmappt += x;
                        slmappt += (mapwidth*(mapheight-1));
                        if (slwrap) tmps = *slmappt; else tmps = 0;
                        for (y=(mapheight-1);y>0;y--) {
                            *slmappt = slmappt[-mapwidth];
                            slmappt -= mapwidth;
                        }
                        *slmappt = tmps;
                    }
                    break;
                case 3:
                    for (y=0;y<mapheight;y++) {
                        slmappt = mappt;
                        slmappt += (y*mapwidth);
                        if (slwrap) tmps = *slmappt; else tmps = 0;
                        for (x=0;x<(mapwidth-1);x++) {
                            *slmappt = slmappt[1];
                            slmappt++;
                        }
                        *slmappt = tmps;
                    }
                    break;
                case 4:
                    for (y=0;y<mapheight;y++) {
                        slmappt = mappt;
                        slmappt += (y*mapwidth);
                        slmappt += (mapwidth-1);
                        if (slwrap) tmps = *slmappt; else tmps = 0;
                        for (x=(mapwidth-1);x>0;x--) {
                            *slmappt = slmappt[-1];
                            slmappt--;
                        }
                        *slmappt = tmps;
                    }
                    break;
                }
                slnum--;
            }
            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
            break;
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

INT_PTR CALLBACK DIAImportNumberProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i;
    char textbuf[80];

    switch (msg) {
    case WM_INITDIALOG:
        SetWindowText(GetDlgItem(hwndDlg, IDC_IMPNUMWHERE), "1");
        SetWindowText(GetDlgItem(hwndDlg, IDC_IMPNUMAMOUNT), "1");
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_IMPNUMWHERE), textbuf, 80);
            i = atoi(textbuf);
            if (i<1 || i>numblockgfx) {
                sprintf(textbuf, "Invalid 'start at' number, must be between 1 and %d", numblockgfx-1);
                MessageBox(hwndDlg, textbuf, "Import At", MB_ICONEXCLAMATION|MB_OK);
                break;
            }
            i<<=16;
            GetWindowText(GetDlgItem(hwndDlg, IDC_IMPNUMAMOUNT), textbuf, 80);
            i |= atoi(textbuf);

            SendMessage(hwndDlg, WM_CLOSE, 0, i);
            break;
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

INT_PTR CALLBACK DIARangeEditBlocksProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    unsigned int li, incr;
    long int bstmp;
    int i, j, hdswap;
    char textbuf[80];
    BLKSTR * myblkstrpt;

    switch (msg) {
    case WM_INITDIALOG:
        memset(textbuf, 0 , 80);
        SetDlgItemText(hwndDlg, IDC_GLOBFROM, textbuf);
        SetDlgItemText(hwndDlg, IDC_GLOBTO, textbuf);
        SetDlgItemText(hwndDlg, IDC_GLOBNEWBG, textbuf);
        SetDlgItemText(hwndDlg, IDC_GLOBNEWFG, textbuf);
        SetDlgItemText(hwndDlg, IDC_GLOBNEWFG2, textbuf);
        SetDlgItemText(hwndDlg, IDC_PROPUSER1, textbuf);
        SetDlgItemText(hwndDlg, IDC_PROPUSER2, textbuf);
        SetDlgItemText(hwndDlg, IDC_PROPUSER3, textbuf);
        SetDlgItemText(hwndDlg, IDC_PROPUSER4, textbuf);
        SetDlgItemText(hwndDlg, IDC_PROPUSER5, textbuf);
        SetDlgItemText(hwndDlg, IDC_PROPUSER6, textbuf);
        SetDlgItemText(hwndDlg, IDC_PROPUSER7, textbuf);
        CheckDlgButton(hwndDlg, IDC_PROPTL, 2);
        CheckDlgButton(hwndDlg, IDC_PROPTR, 2);
        CheckDlgButton(hwndDlg, IDC_PROPBL, 2);
        CheckDlgButton(hwndDlg, IDC_PROPBR, 2);
        CheckDlgButton(hwndDlg, IDC_PROPTRIGGER, 2);
        CheckDlgButton(hwndDlg, IDC_PROPUNUSED1, 2);
        CheckDlgButton(hwndDlg, IDC_PROPUNUSED2, 2);
        CheckDlgButton(hwndDlg, IDC_PROPUNUSED3, 2);
        CheckDlgButton(hwndDlg, IDC_GLOBBGFG, 0);
        CheckDlgButton(hwndDlg, IDC_GLOBBGFG2, 0);
        CheckDlgButton(hwndDlg, IDC_GLOBFGFG2, 0);
        CheckDlgButton(hwndDlg, IDC_GLOBINCR, 0);
        if (hexordec) {
            CheckDlgButton(hwndDlg, IDC_PROPHEX, 0);
            CheckDlgButton(hwndDlg, IDC_PROPDEC, 1);
        } else {
            CheckDlgButton(hwndDlg, IDC_PROPDEC, 0);
            CheckDlgButton(hwndDlg, IDC_PROPHEX, 1);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_GLOBHEX:
        case IDC_GLOBDEC:
            if (SendDlgItemMessage(hwndDlg, IDC_PROPHEX, BM_GETCHECK, 0, 0)) {
                DIAHexDecUsers(hwndDlg);
                hexordec = 0;
                CheckDlgButton(hwndDlg, IDC_PROPDEC, 0);
                CheckDlgButton(hwndDlg, IDC_PROPHEX, 1);
            } else {
                DIAHexDecUsers(hwndDlg);
                hexordec = 1;
                CheckDlgButton(hwndDlg, IDC_PROPHEX, 0);
                CheckDlgButton(hwndDlg, IDC_PROPDEC, 1);
            }
            //				DIAFillUsers (hwndDlg);
            break;
        case IDC_GLOBAUTOBG:
            AutoSetBGTransparency();
            MessageBox(hwndDlg, "Autoset done", "Range Edit Blocks", MB_OK|MB_ICONINFORMATION);
            break;
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_GLOBFROM), textbuf, 80);
            i = atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_GLOBTO), textbuf, 80);
            j = atoi(textbuf);
            if (i<0 || i>(numblockstr-1) || j<i || j>(numblockstr-1)) {
                MessageBox(hwndDlg, "Illegal 'from' or 'to' value!", "Range Edit Blocks", MB_OK|MB_ICONEXCLAMATION);
                break;
            }
            myblkstrpt = (BLKSTR *) blockstrpt;
            myblkstrpt += i; incr = 0;
            while (i<(j+1)) {
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPTL)==1) myblkstrpt->tl = 1;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPTL)==0) myblkstrpt->tl = 0;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPTR)==1) myblkstrpt->tr = 1;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPTR)==0) myblkstrpt->tr = 0;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPBL)==1) myblkstrpt->bl = 1;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPBL)==0) myblkstrpt->bl = 0;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPBR)==1) myblkstrpt->br = 1;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPBR)==0) myblkstrpt->br = 0;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPTRIGGER)==1) myblkstrpt->trigger = 1;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPTRIGGER)==0) myblkstrpt->trigger = 0;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPUNUSED1)==1) myblkstrpt->unused1 = 1;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPUNUSED1)==0) myblkstrpt->unused1 = 0;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPUNUSED2)==1) myblkstrpt->unused2 = 1;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPUNUSED2)==0) myblkstrpt->unused2 = 0;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPUNUSED3)==1) myblkstrpt->unused3 = 1;
                if (IsDlgButtonChecked(hwndDlg, IDC_PROPUNUSED3)==0) myblkstrpt->unused3 = 0;
                hdswap = 0;
                if (hexordec) {
                    DIAHexDecUsers(hwndDlg);
                    hexordec = 0; hdswap = 1;
                }
                GetWindowText(GetDlgItem(hwndDlg, IDC_PROPUSER1), textbuf, 80);
                if (textbuf[0]) { sscanf(textbuf, "%X", &li); myblkstrpt->user1 = (unsigned long)(li+incr); }
                GetWindowText(GetDlgItem(hwndDlg, IDC_PROPUSER2), textbuf, 80);
                if (textbuf[0]) { sscanf(textbuf, "%X", &li); myblkstrpt->user2 = (unsigned long)(li+incr); }
                GetWindowText(GetDlgItem(hwndDlg, IDC_PROPUSER3), textbuf, 80);
                if (textbuf[0]) { sscanf(textbuf, "%X", &li); myblkstrpt->user3 = (short int) (li+incr); }
                GetWindowText(GetDlgItem(hwndDlg, IDC_PROPUSER4), textbuf, 80);
                if (textbuf[0]) { sscanf(textbuf, "%X", &li); myblkstrpt->user4 = (short int) (li+incr); }
                GetWindowText(GetDlgItem(hwndDlg, IDC_PROPUSER5), textbuf, 80);
                if (textbuf[0]) { sscanf(textbuf, "%X", &li); myblkstrpt->user5 = (char) (li+incr); }
                GetWindowText(GetDlgItem(hwndDlg, IDC_PROPUSER6), textbuf, 80);
                if (textbuf[0]) { sscanf(textbuf, "%X", &li); myblkstrpt->user6 = (char) (li+incr); }
                GetWindowText(GetDlgItem(hwndDlg, IDC_PROPUSER7), textbuf, 80);
                if (textbuf[0]) { sscanf(textbuf, "%X", &li); myblkstrpt->user7 = (char) (li+incr); }
                if (hdswap) {
                    DIAHexDecUsers(hwndDlg);
                    hexordec = 1; hdswap = 0;
                }

                if (IsDlgButtonChecked(hwndDlg, IDC_GLOBBGFG)==1) { bstmp = myblkstrpt->bgoff; myblkstrpt->bgoff = myblkstrpt->fgoff; myblkstrpt->fgoff = bstmp; }
                if (IsDlgButtonChecked(hwndDlg, IDC_GLOBBGFG2)==1) { bstmp = myblkstrpt->bgoff; myblkstrpt->bgoff = myblkstrpt->fgoff2; myblkstrpt->fgoff2 = bstmp; }
                if (IsDlgButtonChecked(hwndDlg, IDC_GLOBFGFG2)==1) { bstmp = myblkstrpt->fgoff; myblkstrpt->fgoff = myblkstrpt->fgoff2; myblkstrpt->fgoff2 = bstmp; }

                GetWindowText(GetDlgItem(hwndDlg, IDC_GLOBNEWBG), textbuf, 80);
                if (textbuf[0]) {
                    int gfxid = atoi(textbuf) + (int) incr;
                    if (gfxid >= 0 && gfxid < numblockgfx)
                        myblkstrpt->bgoff = (long int) gfxid*blockwidth*blockheight*((blockdepth+1)/8);
                    else
                        MessageBox(hwndDlg, "Illegal graphic number!", "Range Edit Blocks", MB_OK|MB_ICONEXCLAMATION);

                }
                GetWindowText(GetDlgItem(hwndDlg, IDC_GLOBNEWFG), textbuf, 80);
                if (textbuf[0]) {
                    int gfxid = atoi(textbuf) + (int) incr;
                    if (gfxid >= 0 && gfxid < numblockgfx)
                        myblkstrpt->fgoff = (long int) gfxid*blockwidth*blockheight*((blockdepth+1)/8);
                    else
                        MessageBox(hwndDlg, "Illegal graphic number!", "Range Edit Blocks", MB_OK|MB_ICONEXCLAMATION);
                }
                GetWindowText(GetDlgItem(hwndDlg, IDC_GLOBNEWFG2), textbuf, 80);
                if (textbuf[0]) {
                    int gfxid = atoi(textbuf) + (int) incr;
                    if (gfxid >= 0 && gfxid < numblockgfx)
                        myblkstrpt->fgoff2 = (long int) gfxid*blockwidth*blockheight*((blockdepth+1)/8);
                    else
                        MessageBox(hwndDlg, "Illegal graphic number!", "Range Edit Blocks", MB_OK|MB_ICONEXCLAMATION);
                }

                if (IsDlgButtonChecked(hwndDlg, IDC_GLOBINCR)==1) incr++;
                myblkstrpt++; i++;
            }
            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
            break;
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

INT_PTR CALLBACK DIAGrabBrushBSProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i, j, k, l;
    char textbuf[80];

    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemText(hwndDlg, IDC_GRABBSS, "1");
        SetDlgItemText(hwndDlg, IDC_GRABBSW, "1");
        SetDlgItemText(hwndDlg, IDC_GRABBSH, "1");
        SetDlgItemText(hwndDlg, IDC_GRABBSM, "0");
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_GRABBSS), textbuf, 80);
            i = atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_GRABBSW), textbuf, 80);
            j = atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_GRABBSH), textbuf, 80);
            k = atoi(textbuf);
            GetWindowText(GetDlgItem(hwndDlg, IDC_GRABBSM), textbuf, 80);
            l = atoi(textbuf);
            if (i<0 || i>(numblockstr-1) || (i+(j*k))<0 || (i+(j*k))>(numblockstr)) {
                MessageBox(hwndDlg, "Illegal value!", "Grab brush from sequence", MB_OK|MB_ICONEXCLAMATION);
                break;
            }

            if (MakeBrushBS(i, j, k, l))
                MessageBox(hwndDlg, "Error creating brush!", "Grab brush from sequence", MB_OK|MB_ICONEXCLAMATION);

            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
            break;
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

INT_PTR CALLBACK DIABrushRenameProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowText(GetDlgItem(hwndDlg, IDC_BRSHNAME), gbname);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_BRSHNAME), gbname, 63);
            if (gbname[0] == 0) strcpy(gbname, "Brush");
            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
            break;
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

int DIANewProject(HWND hwnd) {
    int i;

    if (MessageBox(hwnd, "There are two dialogues for creating a new map\n"
        "'Advanced' is the standard dialogue and allows all values to be chosen\n"
        "'Easy' allows only essential values to be chosen, picking defaults for others\n\n"
        "Click 'YES' to use Advanced, or 'NO' to use Easy", "Map Editor: New map", MB_ICONINFORMATION|MB_YESNO) == IDYES) {
        i = 2;
    } else {
        i = DialogBox(hInstance, (LPCSTR) "D_NEWMAPS", hwnd, AsDlgProc(DIANewMapSimpleProc));
    }

    if (i == 2) {
        i = DialogBox(hInstance, (LPCSTR) "D_NEWMAP", hwnd, AsDlgProc(DIANewMapProc));
    }

    if (i!=-1) {
        if (newmapwidth<3 || newmapheight<3 || newmapwidth>10000 || newmapheight>10000) {
            MessageBox(hwnd, "Sorry, map width or height out of range (2 to 10000)", "Map Editor: New map", MB_OK|MB_ICONEXCLAMATION);
            return -1;
        }
        if (newblockwidth<8 || newblockheight<8 || newblockwidth>128 || newblockheight>128) {
            MessageBox(hwnd, "Sorry, block width or height out of range (8 to 128)", "Map Editor: New map", MB_OK|MB_ICONEXCLAMATION);
            return -1;
        }
        if (!(newmapdepth==8 || newmapdepth==15 || newmapdepth==16 || newmapdepth==24 || newmapdepth==32)) {
            MessageBox(hwnd, "Sorry, depth not supported (8, 15, 16, 24 and 32)", "Map Editor: New map", MB_OK|MB_ICONEXCLAMATION);
            return -1;
        }
        if (CreateNewMap(newmapwidth, newmapheight, newmapdepth, newblockwidth, newblockheight, newblockgapx, newblockgapy, newblockstaggerx, newblockstaggery, newmaptype)!=-1) {
            mapaltered = 1;
        } else {
            MessageBox(hwnd, "There was an error creating the map, please restart and try again", "Map Editor: New map", MB_OK|MB_ICONEXCLAMATION);
        }
    }
    return i;
}

int DIAPickBlockGfx(HWND hwnd, int gpart) {
    int i, j;
    BLKSTR *myblkstr;

    newframes[0] = -1;
    i = DialogBoxParam(hInstance, (LPCSTR) "D_PICKER", hwnd, AsDlgProc(DIAPickGfxProc), gpart);
    if (i!=-1) {
        switch (gpart) {
        case 0:
            myblkstr = (BLKSTR *) blockstrpt; myblkstr += editnum;
            myblkstr->bgoff = i*blockwidth*blockheight*((blockdepth+1)/8);
            break;
        case 1:
            myblkstr = (BLKSTR *) blockstrpt; myblkstr += editnum;
            myblkstr->fgoff = i*blockwidth*blockheight*((blockdepth+1)/8);
            break;
        case 2:
            myblkstr = (BLKSTR *) blockstrpt; myblkstr += editnum;
            myblkstr->fgoff2 = i*blockwidth*blockheight*((blockdepth+1)/8);
            break;
        case 3:
            myblkstr = (BLKSTR *) blockstrpt; myblkstr += editnum;
            myblkstr->fgoff3 = i*blockwidth*blockheight*((blockdepth+1)/8);
            break;
        case 4:
            SetRefFrame(editnum, i);
            break;
        case 5:
            j = 0; while (newframes[j]!=-1) {
                InsertAnimFrame(editnum, editpoint, newframes[j]);
                editpoint++; j++;
            }
            break;
        }
    }
    return 0;
}

int DIAEditProperties(HWND hwnd, int btoedit) {
    int i;

    editnum = btoedit;
    i = 1; while (i) {
        i = DialogBox(hInstance, (LPCSTR) "D_PROPEDIT", hwnd, AsDlgProc(DIAPropEditProc));
        if (i==1) if (editnum>0) editnum--;
        if (i==2) if (editnum<(numblockstr-1)) editnum++;
        if (i==3) DIAPickBlockGfx(hwnd, 0);
        if (i==4) DIAPickBlockGfx(hwnd, 1);
        if (i==5) DIAPickBlockGfx(hwnd, 2);
        if (i==6) DIAPickBlockGfx(hwnd, 3);
    }
    return 0;
}

int DIAEditAnimProperties(HWND hwnd, int btoedit) {
    int i;

    editnum = btoedit; editpoint = 0;
    i = 1; while (i) {
        i = DialogBox(hInstance, (LPCSTR) "D_ANIMEDIT", hwnd, AsDlgProc(DIAAnimEditProc));
        if (i==1) DIAPickBlockGfx(hwnd, 4);
        if (i==2) DIAPickBlockGfx(hwnd, 5);
    }
    return 0;
}

int DIAResizeMap(HWND hwnd) {
    int i;

    if (mappt==NULL) return -1;
    i = DialogBox(hInstance, (LPCSTR) "D_RESIZE", hwnd, AsDlgProc(DIAResizeProc));
    if (i>=0) {
        if (newmapwidth<3 || newmapheight<3 || newmapwidth>10000 || newmapheight>10000)
            MessageBox(hwnd, "The new map size was not valid, the map has not been affected", "Resize:", MB_OK|MB_ICONINFORMATION);
        else { if (ResizeMap(newmapwidth, newmapheight, i))
            MessageBox(hwnd, "There was a memory error, please exit immediately, do not save this map", "Resize:", MB_OK|MB_ICONEXCLAMATION); }
    }
    return 0;
}

int DIAWriteBMP(FILE * fpt, int bprow, int bskip) {
    int i, j, k;
    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;
    RGBQUAD colpal[256];
    unsigned char * mycmappt, * bmdatapt, * bmlinept;
    unsigned char tempred, tempgreen, tempblue;

    if (bskip) bskip = 1; else bskip = 0;
    if (bskip && numblockgfx < 2) return -1;
    if (bprow<1) bprow = 1;
    if (bprow>256) bprow = 256;
    memset(&bfh, 0, sizeof(BITMAPFILEHEADER));
    i = blockwidth*bprow;
    if (blockdepth>8) i *= 3;
    i += 3; i &= -4;
    bfh.bfType = 19778; //'BM';
    bfh.bfSize = ((((numblockgfx-1)-bskip)/bprow)+1)*i*blockheight;

    bfh.bfSize += sizeof(BITMAPFILEHEADER);
    bfh.bfSize += sizeof(BITMAPINFOHEADER);
    if (blockdepth==8) bfh.bfSize += ((sizeof(RGBQUAD))*256);
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER);
    bfh.bfOffBits += sizeof(BITMAPINFOHEADER);
    if (blockdepth==8) bfh.bfOffBits += ((sizeof(RGBQUAD))*256);

    memset(&bih, 0, sizeof(BITMAPINFOHEADER));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = blockwidth*bprow;
    bih.biHeight = ((((numblockgfx-1)-bskip)/bprow)+1)*blockheight;
    bih.biPlanes = 1;
    if (blockdepth==8) bih.biBitCount = 8; else bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = 0;
    bih.biXPelsPerMeter = 1600;
    bih.biYPelsPerMeter = 1600;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

    if (cmappt==NULL) return -1;
    mycmappt = (unsigned char *) cmappt;
    for (j=0;j<256;j++) {
        colpal[j].rgbRed = mycmappt[0];
        colpal[j].rgbGreen = mycmappt[1];
        colpal[j].rgbBlue = mycmappt[2];
        colpal[j].rgbReserved = 0;
        mycmappt += 3;
    }

    bmdatapt = malloc(((((numblockgfx-1)-bskip)/bprow)+1)*blockheight*i);
    if (bmdatapt==NULL) return -1;
    memset(bmdatapt, 0, ((((numblockgfx-1)-bskip)/bprow)+1)*blockheight*i);

    tempred = transred; tempgreen = transgreen; tempblue = transblue;
    transred = 0; transgreen = 0; transblue = 0;
    for (k=0;k<((((numblockgfx-1)-bskip)/bprow)+1);k++) {
        for (j=0;j<bprow;j++) {
            if ((j+(k*bprow))>=(numblockgfx-bskip)) break;
            else { if (blockdepth==8) BlockGfxto8(j+(k*bprow)+bskip, bmdatapt, j*blockwidth, k*blockheight, i);
                else BlockGfxto24(j+(k*bprow)+bskip, bmdatapt, j*blockwidth, k*blockheight, i); }
        }
    }
    transred = tempred; transgreen = tempgreen; transblue = tempblue;

    if (fwrite(&bfh, 1, sizeof(BITMAPFILEHEADER), fpt)!=sizeof(BITMAPFILEHEADER)) { free(bmdatapt); return -1; }
    if (fwrite(&bih, 1, sizeof(BITMAPINFOHEADER), fpt)!=sizeof(BITMAPINFOHEADER)) { free(bmdatapt); return -1; }
    if (blockdepth==8) { for (j=0;j<256;j++) {
            if (fwrite(&colpal[j], 1, sizeof(RGBQUAD), fpt)!=sizeof(RGBQUAD)) { free(bmdatapt); return -1; }
    } }
    bmlinept = bmdatapt + ((((((numblockgfx-1)-bskip)/bprow)+1)*blockheight)*i);
    bmlinept -= i;
    for (j=(((((numblockgfx-1)-bskip)/bprow)+1)*blockheight);j>0;j--) {
        if (fwrite(bmlinept, 1, (size_t) i, fpt)!=(size_t) i) { free(bmdatapt); return -1; }
        bmlinept -= i;
    }
    free(bmdatapt);
    return 0;
}

int DIAWriteLevelAsBMP(FILE * fpt) {
    int i, j, k, l, bwpitch;
    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;
    RGBQUAD colpal[256];
    unsigned char * bmdatapt, * bmlinept, * mycmappt;
    unsigned long int * darkenpt;

    memset(&bfh, 0, sizeof(BITMAPFILEHEADER));
    i = mapwidth*blockgapx;
    //	if (blockdepth>8)
    i *= 3;
    i += 3; i &= -4;
    bfh.bfType = 19778; //'BM';
    bfh.bfSize = i*(blockgapy*mapheight);
    if (blockstaggerx || blockstaggery) bfh.bfSize /= 2;

    bfh.bfSize += sizeof(BITMAPFILEHEADER);
    bfh.bfSize += sizeof(BITMAPINFOHEADER);
    if (blockdepth==8) bfh.bfSize += ((sizeof(RGBQUAD))*256);
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER);
    bfh.bfOffBits += sizeof(BITMAPINFOHEADER);
    if (blockdepth==8) bfh.bfOffBits += ((sizeof(RGBQUAD))*256);

    memset(&bih, 0, sizeof(BITMAPINFOHEADER));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = mapwidth*blockgapx;
    bih.biHeight = mapheight*blockgapy;
    if (blockstaggerx || blockstaggery) { bih.biHeight /= 2; bih.biHeight -= blockstaggery; }
    bih.biPlanes = 1;
    if (blockdepth==8) bih.biBitCount = 8; else
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = 0;
    bih.biXPelsPerMeter = 1600;
    bih.biYPelsPerMeter = 1600;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

    if (cmappt==NULL) return -1;
    mycmappt = (unsigned char *) cmappt;
    for (j=0;j<256;j++) {
        colpal[j].rgbRed = mycmappt[0];
        colpal[j].rgbGreen = mycmappt[1];
        colpal[j].rgbBlue = mycmappt[2];
        colpal[j].rgbReserved = 0;
        mycmappt += 3;
    }

    bmdatapt = malloc(mapheight*blockgapy*i);
    if (bmdatapt==NULL) return -1;
    memset(bmdatapt, 0, (mapheight*blockgapy*i));

    bwpitch = i;
    if (blockstaggerx || blockstaggery) k = mapheight/2; else k = mapheight;

    for (j=0;j<k;j++) {
        for (i=0;i<mapwidth;i++) {
            if (blockstaggerx || blockstaggery) {
                if (onion==ONION_ALL) OnionCompto24(i, (j*2), bmdatapt, i*blockgapx, j*blockgapy, bwpitch, MAXMAPLAYERS-1);
                else if (onion && onionlower) OnionCompto24(i, (j*2), bmdatapt, i*blockgapx, j*blockgapy, bwpitch, onion-1);
                else if (onion) Onionto24(i, (j*2), bmdatapt, i*blockgapx, j*blockgapy, bwpitch, onion-1);
                else Overlayto24(i, (j*2), bmdatapt, i*blockgapx, j*blockgapy, bwpitch, -1);
            } else {
                if (onion==ONION_ALL) OnionCompto24(i, j, bmdatapt, i*blockgapx, j*blockgapy, bwpitch, MAXMAPLAYERS-1);
                else if (onion && onionlower) OnionCompto24(i, j, bmdatapt, i*blockgapx, j*blockgapy, bwpitch, onion-1);
                else if (onion) Onionto24(i, j, bmdatapt, i*blockgapx, j*blockgapy, bwpitch, onion-1);
                else Blockto24(i, j, bmdatapt, i*blockgapx, j*blockgapy, bwpitch);
            }
        }
        for (i=0;i<mapwidth;i++) {
            if (blockstaggerx || blockstaggery) {
                if (onion==ONION_ALL) OnionCompto24(i, (j*2+1), bmdatapt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, MAXMAPLAYERS-1);
                else if (onion && onionlower) OnionCompto24(i, (j*2+1), bmdatapt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, onion-1);
                else if (onion) Onionto24(i, (j*2+1), bmdatapt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, onion-1);
                else Overlayto24(i, (j*2+1), bmdatapt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, -1);
            }
        }
    }

    if (onion && blockdepth > 8) { if (oniondark) {
            i = (mapwidth*bwpitch*k)/4; darkenpt = (unsigned long int *) bmdatapt; while (i) {
                *darkenpt = ((*darkenpt)>>1)&0x7F7F7F7F; darkenpt++; i--;
        } }
        for (j=0;j<k;j++)
        {	for (i=0;i<mapwidth;i++) {
                if (blockstaggerx || blockstaggery) {
                    Overlayto24(i, (j*2), bmdatapt, i*blockgapx, j*blockgapy, bwpitch, -1);
                } else {
                    Overlayto24(i, j, bmdatapt, i*blockgapx, j*blockgapy, bwpitch, -1);
                }
            }
            for (i=0;i<mapwidth;i++) {
                if (blockstaggerx || blockstaggery) {
                    Overlayto24(i, (j*2+1), bmdatapt, i*blockgapx+blockstaggerx, j*blockgapy+blockstaggery, bwpitch, -1);
                }
            }
    } }

    if (fwrite(&bfh, 1, sizeof(BITMAPFILEHEADER), fpt)!=sizeof(BITMAPFILEHEADER)) { free(bmdatapt); return -1; }
    if (fwrite(&bih, 1, sizeof(BITMAPINFOHEADER), fpt)!=sizeof(BITMAPINFOHEADER)) { free(bmdatapt); return -1; }
    if (blockdepth==8) { for (j=0;j<256;j++) {
            if (fwrite(&colpal[j], 1, sizeof(RGBQUAD), fpt)!=sizeof(RGBQUAD)) { free(bmdatapt); return -1; }
    } }

    bmlinept = bmdatapt + (k*blockgapy*bwpitch);

    bmlinept -= bwpitch;
    i = ((bwpitch/3)+3)&~3;
    mycmappt = (unsigned char *) cmappt;

    for (j=(k*blockgapy);j>blockstaggery;j--) {
        if (blockdepth == 8) {
            for (k=0;k<=((bwpitch-3)/3);k++) {
                int best = 0, bestd = 0x7fffffff;
                for (l=0;l<256;l++) {
                    int dr = (int)mycmappt[l*3] - (int)bmlinept[k*3+2];
                    int dg = (int)mycmappt[l*3+1] - (int)bmlinept[k*3+1];
                    int db = (int)mycmappt[l*3+2] - (int)bmlinept[k*3];
                    int d = dr*dr + dg*dg + db*db;
                    if (d < bestd) { bestd = d; best = l; if (d == 0) break; }
                }
                bmlinept[k] = (unsigned char) best;
            }
            if (fwrite(bmlinept+(blockstaggerx), 1, (size_t) i, fpt)!=(size_t) i) { free(bmdatapt); return -1; }
        } else {
            if (fwrite(bmlinept+(blockstaggerx*3), 1, (size_t) bwpitch, fpt)!=(size_t) bwpitch) { free(bmdatapt); return -1; }
        }
        bmlinept -= bwpitch;
    }
    free(bmdatapt);
    return 0;
}

int DIAExport(HWND hwnd, char * name) {
    int i, j, k, l, m, bprow;
    FILE * fpt;
    ANISTR * myanpt;
    BLKSTR * myblk;
    int * myintpt;
    unsigned char * palpt;

    if (mappt==NULL) return -1;
    strcpy(expname, name);
    //	MessageBox (hwnd, expname, "Debug", MB_OK);
    i = DialogBox(hInstance, (LPCSTR) "D_EXPORT", hwnd, AsDlgProc(DIAExportProc));
    if (i==-1) return 0;
    bprow = i&0xFF;
    i>>=8;
    strcat(expname, ".EXT");
    if (i&EXPRT_CMA) {
        strcpy(expname+strlen(expname)-4, ".CMA");
        //		MessageBox (hwnd, expname, "Debug", MB_OK);
        fpt = fopen(expname, "wb");
        if (fpt==NULL) return -1;
        if (fwrite(cmappt, 1, 768, fpt)!=768) { fclose(fpt); return -1; }
        fclose(fpt);
    }
    if (i&EXPRT_ABD) {
        strcpy(expname+strlen(expname)-4, ".ABD");
        fpt = fopen(expname, "wb");
        if (fpt==NULL) return -1;
        j = 0; if (animstrpt!=NULL) { myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
            while (myanpt->antype!=-1) { j++; myanpt--; } }
        if (j) {
            myanpt = (ANISTR *) (animstrpt+ANDTMEM); k = 0; myanpt--;
            while (myanpt->antype!=-1) { if ((myanpt->anstartoff)<k) k = myanpt->anstartoff; myanpt--; }
            k -= 4; l = -k;
            fwrite(&l, 1, 4, fpt);
            fwrite(animstrpt+ANDTMEM+k, 1, l, fpt);
        }
        fwrite(blockstrpt, 1, numblockstr*BLOCKSTRSIZE, fpt);
        fclose(fpt);
    }
    if (i&EXPRT_MAR) {
        strcpy(expname+strlen(expname)-4, ".MAR");
        fpt = fopen(expname, "wb");
        if (fpt==NULL) return -1;
        if (numblockstr <= 1024) {
            for (j=0;j<(mapwidth*mapheight);j++) if (mappt[j] > 0) mappt[j] *= 32;
            j = mapwidth*mapheight*2;
            if (fwrite(mappt, 1, (size_t) j, fpt)!=(size_t) j) { fclose(fpt);
                for (j=0;j<(mapwidth*mapheight);j++) if (mappt[j] > 0) mappt[j] /= 32;
                return -1; }
            for (j=0;j<(mapwidth*mapheight);j++) if (mappt[j] > 0) mappt[j] /= 32;
        } else {
            for (j=0;j<(mapwidth*mapheight);j++) if (mappt[j] < 0) mappt[j] /= 16;
            j = mapwidth*mapheight*2;
            if (fwrite(mappt, 1, (size_t) j, fpt)!=(size_t) j) { fclose(fpt);
                for (j=0;j<(mapwidth*mapheight);j++) if (mappt[j] < 0) mappt[j] *= 16;
                return -1; }
            for (j=0;j<(mapwidth*mapheight);j++) if (mappt[j] < 0) mappt[j] *= 16;
            MessageBox(hwnd, "Warning: You have more than 1024 block structures, only use this .MAR with FMP1.0 compatible editors/libraries", "Export .MAR", MB_OK|MB_ICONINFORMATION);
        }
        fclose(fpt);
    }
    if (i&EXPRT_CSV) {
        strcpy(expname+strlen(expname)-4, ".CSV");
        fpt = fopen(expname, "w");
        if (fpt==NULL) return -1;
        fprintf(fpt, "//V1.3.11, not properly documented yet, but see MappyAL for\n//header defs and functions etc\n\n");
        fprintf(fpt, "BLKSTR blockstr[%d] = {\n", numblockstr);
        myblk = (BLKSTR *) blockstrpt;
        for (j=0;j<numblockstr;j++) {
            l = 0; if (myblk->tl) l |= 1;
            if (myblk->tr) l |= 2;
            if (myblk->bl) l |= 4;
            if (myblk->br) l |= 8;
            if (myblk->trigger) l |= 16;
            if (myblk->unused1) l |= 32;
            if (myblk->unused2) l |= 64;
            if (myblk->unused3) l |= 128;
            fprintf(fpt, "{ %d, %d, %d, %d, ", (int) myblk->bgoff/(blockwidth*blockheight*((blockdepth+1)/8)), (int) myblk->fgoff/(blockwidth*blockheight*((blockdepth+1)/8)), (int) myblk->fgoff2/(blockwidth*blockheight*((blockdepth+1)/8)), (int) myblk->fgoff3/(blockwidth*blockheight*((blockdepth+1)/8)));
            fprintf(fpt, "%d, %d, %d, %d, ", (int) myblk->user1, (int) myblk->user2, myblk->user3, myblk->user4);
            if (j == (numblockstr-1)) fprintf(fpt, "%d, %d, %d, 0x%02X }\n", myblk->user5, myblk->user6, myblk->user7, l);
            else fprintf(fpt, "%d, %d, %d, 0x%02X },\n", myblk->user5, myblk->user6, myblk->user7, l);
            myblk++;
        }
        fprintf(fpt, "};\n\n");
        if (animstrpt != NULL) {
            InitAnims();
            myanpt = (ANISTR *) (animstrpt+ANDTMEM); l = 0; j = 0; k = 0; myanpt--;
            while (myanpt->antype!=-1) { if (myanpt->anstartoff<j) j = myanpt->anstartoff; myanpt--; l++; k-=16; }
            j -= 4; k -= 16;
            fprintf(fpt, "int animseq[%d] = {\n", (k-j)/4);
            myintpt = (int *) (animstrpt+ANDTMEM+j);
            m = 1; while (1) {
                if (m == ((k-j)/4)) { fprintf(fpt, "%d", (*myintpt)/32); break; }
                if (!(m%16)) fprintf(fpt, "%d,\n", (*myintpt)/32);
                else fprintf(fpt, "%d, ", (*myintpt)/32);
                myintpt++; m++;
            }
            fprintf(fpt, "\n};\n\n");
            fprintf(fpt, "ANISTR animstr[%d] = {\n", l);
            myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
            while (myanpt->antype!=-1) {
                fprintf(fpt, "{ %d, %d, %d, %d, ", (int) myanpt->antype, (int) myanpt->andelay, (int) myanpt->ancount, (int) myanpt->anuser);
                l--; if (l)	fprintf(fpt, "%d, %d, %d },\n", (int) (myanpt->ancuroff-j)/4, (int) (myanpt->anstartoff-j)/4, (int) (myanpt->anendoff-j)/4);
                else fprintf(fpt, "%d, %d, %d }\n", (int) (myanpt->ancuroff-j)/4, (int) (myanpt->anstartoff-j)/4, (int) (myanpt->anendoff-j)/4);
                myanpt--;
            }
            fprintf(fpt, "};\n\n");
        }
        fprintf(fpt, "int maparray[%d][%d] = {\n", mapheight, mapwidth);
        {
            short int * csvpt = csvusebg ? mapmappt[0] : mappt;
            if (csvpt == NULL) csvpt = mappt;
            for (l=0;l<mapheight;l++) {
                j = 0; k = 16;
                fprintf(fpt, "{ ");
                while (j < (mapwidth)) {
                    fprintf(fpt, "%d", (csvpt[j+(l*mapwidth)]+csvadjust)); j++; k--;
                    if (j < (mapwidth)) fprintf(fpt, ",");
                    if (!k) { fprintf(fpt, "\n"); k = 16; }
                }
                if (l < (mapheight-1)) fprintf(fpt, " },\n"); else fprintf(fpt, " }\n");
            }
        }
        fprintf(fpt, "};\n\n");
        if (blockdepth == 8) {
            fprintf(fpt, "unsigned short mapcmap[256] = {\n");
            palpt = (unsigned char *) cmappt;
            for (j=0;j<256;j++) {
                k = (palpt[j*3] >> 3) | ((palpt[j*3+1]>>3) << 5) | ((palpt[j*3+2]>>3) << 10);
                if (j == 255) { fprintf(fpt, "0x%04X\n};\n\n", k); break; }
                if (j%8 == 7) fprintf(fpt,"0x%04X,\n", k);
                else fprintf(fpt,"0x%04X, ", k);
            }
            fprintf(fpt, "unsigned char blockgfx[%d] = {\n", numblockgfx*blockwidth*blockheight*((blockdepth+1)/8));
            palpt = (unsigned char *) blockgfxpt;
            for (j=0;j<numblockgfx;j++) {
                for (k=0;k<blockheight;k+=8) {
                    for (l=0;l<blockwidth;l+=8) {
                        for (m=0;m<8;m++) {
                            fprintf(fpt, "0x%02X, ", palpt[l+(k*blockwidth)+(m*blockwidth)]);
                            fprintf(fpt, "0x%02X, ", palpt[l+(k*blockwidth)+(m*blockwidth)+1]);
                            fprintf(fpt, "0x%02X, ", palpt[l+(k*blockwidth)+(m*blockwidth)+2]);
                            fprintf(fpt, "0x%02X, ", palpt[l+(k*blockwidth)+(m*blockwidth)+3]);
                            fprintf(fpt, "0x%02X, ", palpt[l+(k*blockwidth)+(m*blockwidth)+4]);
                            fprintf(fpt, "0x%02X, ", palpt[l+(k*blockwidth)+(m*blockwidth)+5]);
                            fprintf(fpt, "0x%02X, ", palpt[l+(k*blockwidth)+(m*blockwidth)+6]);
                            if ((k+8)>=blockheight && (l+8)>=blockwidth && (j+1) == numblockgfx && m == 7)
                                fprintf(fpt, "0x%02X\n};\n\n", palpt[l+(k*blockwidth)+(m*blockwidth)+7]);
                            else
                                fprintf(fpt, "0x%02X,\n", palpt[l+(k*blockwidth)+(m*blockwidth)+7]);
                        }
                    }
                }
                palpt += blockwidth*blockheight*((blockdepth+1)/8);
            }
        }
        fclose(fpt);
    }
    if (i&EXPRT_GFX) {
        if (!numblockgfx) return -1;
        strcpy(expname+strlen(expname)-4, ".GFX");
        fpt = fopen(expname, "wb");
        if (fpt==NULL) return -1;
        j = blockwidth*blockheight*((blockdepth+1)/8)*numblockgfx;
        if (fwrite(blockgfxpt, 1, (size_t) j, fpt)!=(size_t) j) { fclose(fpt); return -1; }
        fclose(fpt);
    }
    if (i&EXPRT_SCR) {
        if (!mappt) return -1;
        MessageBox(hwnd, "Warning: This may take a while to complete and use a lot of disk space\n"
            "An 8bit map with duplicate colours may not use the same index as the original", "Export ?scrn.BMP", MB_OK|MB_ICONINFORMATION);
        strcpy(expname+strlen(expname)-4, "scrn.BMP");
        fpt = fopen(expname, "wb");
        strcpy(expname+strlen(expname)-8, ".BMP");
        if (fpt==NULL) return -1;
        j = DIAWriteLevelAsBMP(fpt);
        fclose(fpt);
        if (j) return -1;
    }
    if (i&EXPRT_BMP) {
        if (!numblockgfx) return -1;
        strcpy(expname+strlen(expname)-4, ".BMP");
        fpt = fopen(expname, "wb");
        if (fpt==NULL) return -1;
        j = DIAWriteBMP(fpt, bprow, i&EXPRT_SKP);
        fclose(fpt);
        if (j) return -1;
    }
    return 0;
}

void DIAEditInfo(HWND hwnd) {

    DialogBox(hInstance, (LPCSTR) "D_MAPINFO", hwnd, AsDlgProc(DIAEditInfoProc));
    return;
}

void DIALayerOnion(HWND hwnd) {

    DialogBox(hInstance, (LPCSTR) "D_OSKIN", hwnd, AsDlgProc(DIALayerOnionProc));
    return;
}

void DIALayerSlide(HWND hwnd) {

    DialogBox(hInstance, (LPCSTR) "D_SLIDE", hwnd, AsDlgProc(DIALayerSlideProc));
    return;
}

int DIAImportNumber(HWND hwnd) {
    int i;

    i = DialogBox(hInstance, (LPCSTR) "D_IMPNUM", hwnd, AsDlgProc(DIAImportNumberProc));
    return i;
}

void DIARangeEditBlocks(HWND hwnd) {

    DialogBox(hInstance, (LPCSTR) "D_PROPGLOB", hwnd, AsDlgProc(DIARangeEditBlocksProc));
    return;
}

void DIAGrabBrushBS(HWND hwnd) {

    if (curbrsh<1 || curbrsh>8) {
        MessageBox(hwnd, "Select brush number to grab to first", "Grab Brush from sequence",
            MB_ICONINFORMATION|MB_OK);
        return;
    }
    DialogBox(hInstance, (LPCSTR) "D_GRABBS", hwnd, AsDlgProc(DIAGrabBrushBSProc));
    return;
}

void DIAGetBrushName(HWND hwnd, char * bname) {

    gbname = bname;
    DialogBox(hInstance, (LPCSTR) "D_BRNAME", hwnd, AsDlgProc(DIABrushRenameProc));
    return;
}

INT_PTR CALLBACK DIADividersProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    char textbuf[80];

    switch (msg) {
    case WM_INITDIALOG:
        if (divideron) CheckDlgButton(hwndDlg, IDC_DIVON, 1);
        else CheckDlgButton(hwndDlg, IDC_DIVON, 0);
        sprintf(textbuf, "%d", dividergapx);
        SetWindowText(GetDlgItem(hwndDlg, IDC_DIVGAPX), textbuf);
        sprintf(textbuf, "%d", dividergapy);
        SetWindowText(GetDlgItem(hwndDlg, IDC_DIVGAPY), textbuf);
        sprintf(textbuf, "%06lX", (unsigned long) (dividercol & 0xFFFFFF));
        SetWindowText(GetDlgItem(hwndDlg, IDC_DIVCOL), textbuf);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (IsDlgButtonChecked(hwndDlg, IDC_DIVON)) divideron = 1; else divideron = 0;
            GetWindowText(GetDlgItem(hwndDlg, IDC_DIVGAPX), textbuf, 80);
            dividergapx = atoi(textbuf);
            if (dividergapx < 8) dividergapx = 8;
            if (dividergapx > 4096) dividergapx = 4096;
            GetWindowText(GetDlgItem(hwndDlg, IDC_DIVGAPY), textbuf, 80);
            dividergapy = atoi(textbuf);
            if (dividergapy < 8) dividergapy = 8;
            if (dividergapy > 4096) dividergapy = 4096;
            GetWindowText(GetDlgItem(hwndDlg, IDC_DIVCOL), textbuf, 80);
            dividercol = strtol(textbuf, NULL, 16) & 0xFFFFFF;
            SendMessage(hwndDlg, WM_CLOSE, 0, 0L);
            break;
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

void DIADividers(HWND hwnd) {

    DialogBox(hInstance, (LPCSTR) "D_DIVIDER", hwnd, AsDlgProc(DIADividersProc));
    return;
}

INT_PTR CALLBACK DIALayerGotoProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    char textbuf[80];
    int i;

    switch (msg) {
    case WM_INITDIALOG:
        sprintf(textbuf, "%d", maplayer);
        SetWindowText(GetDlgItem(hwndDlg, IDC_IMPNUMWHERE), textbuf);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_IMPNUMWHERE), textbuf, 80);
            i = atoi(textbuf);
            if (i<0 || i>=MAXMAPLAYERS) {
                sprintf(textbuf, "Layer must be between 0 and %d", MAXMAPLAYERS-1);
                MessageBox(hwndDlg, textbuf, "Go To Layer", MB_ICONEXCLAMATION|MB_OK);
                break;
            }
            SendMessage(hwndDlg, WM_CLOSE, 0, i);
            break;
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

int DIALayerGoto(HWND hwnd) {
    int i;

    i = DialogBox(hInstance, (LPCSTR) "D_LAYERNUM", hwnd, AsDlgProc(DIALayerGotoProc));
    return i;
}

INT_PTR CALLBACK DIALayerRenameProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    const char * oldn;

    switch (msg) {
    case WM_INITDIALOG:
        oldn = F14_LayerName(maplayer);
        if (oldn==NULL) oldn = "";
        SetWindowText(GetDlgItem(hwndDlg, IDC_BRSHNAME), oldn);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
                char nb[64];
                GetWindowText(GetDlgItem(hwndDlg, IDC_BRSHNAME), nb, 63);
                nb[63] = 0;
                F14_LayerNameSet(maplayer, nb);
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

void DIALayerRename(HWND hwnd) {

    DialogBox(hInstance, (LPCSTR) "D_LAYERNAME", hwnd, AsDlgProc(DIALayerRenameProc));
    return;
}

/* ---- Export textfile (1.3.26 C text export; GBA/Lua variants are Stage 3).
 * textexportopts chars: 0 CONST,1 CMAP,2 ANIBLKSTR,3 MAPARRAY,4 BLKGFX,
 * 5 MA2D,6 MA1D,7 MAALL,8 MASINGLE,9 MERGEFLIP,10 16COL,12 CMAPGBA,
 * 15 RESOLVEBG,17 USEPREFIX,18 OBJSTR. ---- */
static int txf[24];
static char txprefix[64];

INT_PTR CALLBACK DIAExportTextProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i;
    static const int ids[15] = {
        IDC_TXTCONST, IDC_TXTCMAP, IDC_TXTANIBLK, IDC_TXTMAPARR, IDC_TXTBLKGFX,
        IDC_TXTMA2D, IDC_TXTMA1D, IDC_TXTMAALL, IDC_TXTMASINGLE, IDC_TXTMERGEFLIP,
        IDC_TXT16COL, IDC_TXTCMAPGBA, IDC_TXTRESOLVEBG, IDC_TXTUSEPREFIX, IDC_TXTOBJSTR };
    static const int opti[15] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 17, 18 };

    switch (msg) {
    case WM_INITDIALOG:
        for (i=0;i<15;i++)
            CheckDlgButton(hwndDlg, ids[i], textexportopts[opti[i]]=='1' ? 1 : 0);
        SetWindowText(GetDlgItem(hwndDlg, IDC_TXTPREFIX), "mappy");
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            for (i=0;i<15;i++) {
                txf[opti[i]] = IsDlgButtonChecked(hwndDlg, ids[i]) ? 1 : 0;
                textexportopts[opti[i]] = txf[opti[i]] ? '1' : '0';
            }
            GetWindowText(GetDlgItem(hwndDlg, IDC_TXTPREFIX), txprefix, 63);
            txprefix[63] = 0;
            if (txprefix[0]==0) strcpy(txprefix, "mappy");
            SendMessage(hwndDlg, WM_CLOSE, 0, 1L);
            break;
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

/* resolved cell value for text/CSV export */
static int TXCellVal(short int * layer, int idx, int withstructs) {
    int v = layer[idx];
    BLKSTR * bp;

    if (v<0) {
        if (!withstructs && txf[15]) {
            /* resolve anim reference frame, then use its BG */
            ANISTR * ap = (ANISTR *) (animstrpt+ANDTMEM+v);
            long int * tp = (long int *) (animstrpt+ANDTMEM+ap->anstartoff);
            tp--;
            bp = (BLKSTR *) (blockstrpt+(*tp));
            v = (int) bp->bgoff/(blockwidth*blockheight*((blockdepth+1)/8));
        }
        return v+csvadjust;
    }
    bp = (BLKSTR *) (blockstrpt+(((int) v)*sizeof(BLKSTR)));
    if (!withstructs || txf[15]) {
        v = (int) bp->bgoff/(blockwidth*blockheight*((blockdepth+1)/8));
    } else if (txf[9] && flippedtiles) {
        /* MERGEFLIP: fold H/V flip flags into bits 14/15 */
        if (bp->unused2) v |= 0x4000;
        if (bp->unused3) v |= 0x8000;
    }
    return v+csvadjust;
}

int DIAExportText(HWND hwnd, char * name) {
    OPENFILENAME ofn;
    char szFilter[64], szFile[256], szFileTitle[256];
    char outname[300];
    FILE * fpt;
    int i, j, k, l, m, li;
    ANISTR * myanpt;
    BLKSTR * myblk;
    int * myintpt;
    unsigned char * palpt;
    short int * lay;
    char sym[128], pre[72];
    short int nlayers;

    if (mappt==NULL) return -1;
    memset(txf, 0, sizeof(txf));
    i = DialogBox(hInstance, (LPCSTR) "D_TXTEXP", hwnd, AsDlgProc(DIAExportTextProc));
    if (i!=1) return 0;

    strcpy(outname, name);
    i = strlen(outname);
    while (i>0 && outname[i]!='.' && outname[i]!='/' && outname[i]!='\\' && outname[i]!=':') i--;
    if (outname[i]=='.') outname[i] = 0;
    strcat(outname, ".txt");
    memset(szFilter, 0, sizeof(szFilter));
    strcpy(szFilter, "Text files (*.TXT)|*.txt");
    szFilter[18] = 0;
    strcpy(szFile, outname);
    szFileTitle[0] = 0;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFileTitle = szFileTitle;
    ofn.nMaxFileTitle = sizeof(szFileTitle);
    ofn.lpstrInitialDir = folder;
    ofn.lpstrDefExt = "txt";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    if (!GetSaveFileName(&ofn)) return 0;
    strcpy(outname, szFile);

    fpt = fopen(outname, "w");
    if (fpt==NULL) return -1;
    if (txf[17]) sprintf(pre, "%s_", txprefix); else pre[0] = 0;

    fprintf(fpt, "/* Exported by MappyWin32 (C text export) from %s */\n\n", name);

    if (txf[1] && blockdepth==8) {
        palpt = (unsigned char *) cmappt;
        if (txf[12]) {
            sprintf(sym, "%smapcmap", pre);
            fprintf(fpt, "%sunsigned short %s[256] = {\n", txf[0] ? "const " : "", sym);
            for (j=0;j<256;j++) {
                k = (palpt[j*3] >> 3) | ((palpt[j*3+1]>>3) << 5) | ((palpt[j*3+2]>>3) << 10);
                if (j==255) fprintf(fpt, "0x%04X\n};\n\n", k);
                else if (j%8==7) fprintf(fpt, "0x%04X,\n", k);
                else fprintf(fpt, "0x%04X, ", k);
            }
        } else {
            sprintf(sym, "%smapcmap", pre);
            fprintf(fpt, "%sunsigned int %s[256] = {\n", txf[0] ? "const " : "", sym);
            for (j=0;j<256;j++) {
                k = (palpt[j*3]<<16) | (palpt[j*3+1]<<8) | palpt[j*3+2];
                if (j==255) fprintf(fpt, "0x%06X\n};\n\n", k);
                else if (j%8==7) fprintf(fpt, "0x%06X,\n", k);
                else fprintf(fpt, "0x%06X, ", k);
            }
        }
    }

    if (txf[2]) {
        sprintf(sym, "%sblockstr", pre);
        fprintf(fpt, "%sBLKSTR %s[%d] = {\n", txf[0] ? "const " : "", sym, numblockstr);
        myblk = (BLKSTR *) blockstrpt;
        for (j=0;j<numblockstr;j++) {
            l = 0; if (myblk->tl) l |= 1;
            if (myblk->tr) l |= 2;
            if (myblk->bl) l |= 4;
            if (myblk->br) l |= 8;
            if (myblk->trigger) l |= 16;
            if (myblk->unused1) l |= 32;
            if (myblk->unused2) l |= 64;
            if (myblk->unused3) l |= 128;
            fprintf(fpt, "{ %d, %d, %d, %d, ", (int) myblk->bgoff/(blockwidth*blockheight*((blockdepth+1)/8)), (int) myblk->fgoff/(blockwidth*blockheight*((blockdepth+1)/8)), (int) myblk->fgoff2/(blockwidth*blockheight*((blockdepth+1)/8)), (int) myblk->fgoff3/(blockwidth*blockheight*((blockdepth+1)/8)));
            fprintf(fpt, "%d, %d, %d, %d, ", (int) myblk->user1, (int) myblk->user2, myblk->user3, myblk->user4);
            if (j == (numblockstr-1)) fprintf(fpt, "%d, %d, %d, 0x%02X }\n", myblk->user5, myblk->user6, myblk->user7, l);
            else fprintf(fpt, "%d, %d, %d, 0x%02X },\n", myblk->user5, myblk->user6, myblk->user7, l);
            myblk++;
        }
        fprintf(fpt, "};\n\n");
        if (animstrpt != NULL) {
            InitAnims();
            myanpt = (ANISTR *) (animstrpt+ANDTMEM); l = 0; j = 0; k = 0; myanpt--;
            while (myanpt->antype!=-1) { if (myanpt->anstartoff<j) j = myanpt->anstartoff; myanpt--; l++; k-=16; }
            j -= 4; k -= 16;
            sprintf(sym, "%sanimseq", pre);
            fprintf(fpt, "%sint %s[%d] = {\n", txf[0] ? "const " : "", sym, (k-j)/4);
            myintpt = (int *) (animstrpt+ANDTMEM+j);
            m = 1; while (1) {
                if (m == ((k-j)/4)) { fprintf(fpt, "%d", (*myintpt)/32); break; }
                if (!(m%16)) fprintf(fpt, "%d,\n", (*myintpt)/32);
                else fprintf(fpt, "%d, ", (*myintpt)/32);
                myintpt++; m++;
            }
            fprintf(fpt, "\n};\n\n");
            sprintf(sym, "%sanimstr", pre);
            fprintf(fpt, "%sANISTR %s[%d] = {\n", txf[0] ? "const " : "", sym, l);
            myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
            while (myanpt->antype!=-1) {
                fprintf(fpt, "{ %d, %d, %d, %d, ", (int) myanpt->antype, (int) myanpt->andelay, (int) myanpt->ancount, (int) myanpt->anuser);
                l--; if (l)	fprintf(fpt, "%d, %d, %d },\n", (int) (myanpt->ancuroff-j)/4, (int) (myanpt->anstartoff-j)/4, (int) (myanpt->anendoff-j)/4);
                else fprintf(fpt, "%d, %d, %d }\n", (int) (myanpt->ancuroff-j)/4, (int) (myanpt->anstartoff-j)/4, (int) (myanpt->anendoff-j)/4);
                myanpt--;
            }
            fprintf(fpt, "};\n\n");
        }
    }

    if (txf[3]) {
        nlayers = 0;
        for (li=0;li<MAXMAPLAYERS;li++) if (GetLayerpt(li)!=NULL) nlayers++;
        for (li=0;li<MAXMAPLAYERS;li++) {
            lay = GetLayerpt(li);
            if (lay==NULL) continue;
            if (!txf[7] && lay!=mappt) continue; /* single layer only */
            if (txf[7] && nlayers>1) sprintf(sym, "%smaparray_L%d", pre, li);
            else sprintf(sym, "%smaparray", pre);
            if (txf[5]) {
                fprintf(fpt, "%sint %s[%d][%d] = {\n", txf[0] ? "const " : "", sym, mapheight, mapwidth);
                for (l=0;l<mapheight;l++) {
                    fprintf(fpt, "{ ");
                    for (j=0;j<mapwidth;j++) {
                        fprintf(fpt, "%d", TXCellVal(lay, j+l*mapwidth, txf[2]));
                        if (j<mapwidth-1) fprintf(fpt, ",");
                    }
                    if (l<mapheight-1) fprintf(fpt, " },\n"); else fprintf(fpt, " }\n");
                }
                fprintf(fpt, "};\n\n");
            } else {
                fprintf(fpt, "%sint %s[%d] = {\n", txf[0] ? "const " : "", sym, mapwidth*mapheight);
                for (l=0;l<mapwidth*mapheight;l++) {
                    fprintf(fpt, "%d", TXCellVal(lay, l, txf[2]));
                    if (l<mapwidth*mapheight-1) fprintf(fpt, ",");
                    if ((l+1)%16==0) fprintf(fpt, "\n");
                }
                fprintf(fpt, "};\n\n");
            }
            if (!txf[7]) break;
        }
    }

    if (txf[4] && numblockgfx) {
        palpt = (unsigned char *) blockgfxpt;
        sprintf(sym, "%sblockgfx", pre);
        if (txf[10] && blockdepth==8) {
            int ng = numblockgfx*blockwidth*blockheight/2;
            fprintf(fpt, "%sunsigned char %s[%d] = {\n", txf[0] ? "const " : "", sym, ng);
            for (j=0;j<ng;j++) {
                k = ((palpt[j*2]&0x0F)<<4) | (palpt[j*2+1]&0x0F);
                if (j==ng-1) fprintf(fpt, "0x%02X\n};\n\n", k);
                else if (j%16==15) fprintf(fpt, "0x%02X,\n", k);
                else fprintf(fpt, "0x%02X, ", k);
            }
        } else {
            int ng = numblockgfx*blockwidth*blockheight*((blockdepth+1)/8);
            fprintf(fpt, "%sunsigned char %s[%d] = {\n", txf[0] ? "const " : "", sym, ng);
            for (j=0;j<ng;j++) {
                if (j==ng-1) fprintf(fpt, "0x%02X\n};\n\n", palpt[j]);
                else if (j%16==15) fprintf(fpt, "0x%02X,\n", palpt[j]);
                else fprintf(fpt, "0x%02X, ", palpt[j]);
            }
        }
    }

    if (txf[18]) {
        OE_ExportObjStr(fpt, pre, txf[0]);
    }

    fclose(fpt);
    return 0;
}

/* ---------------- Resize Block Dimensions (D_RESIZET) ---------------- */

extern short int blockwidth, blockheight, blockdepth;
extern char * blockgfxpt;
extern short int numblockgfx, numblockstr;
extern short int mapwidth, mapheight;
extern char * blockstrpt;

INT_PTR CALLBACK DIAResizeBlkProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    char txt[32];
    int neww, newh, newd;

    UNUSED(lParam);
    switch (msg) {
    case WM_INITDIALOG:
        sprintf(txt, "%d", (int) blockwidth);
        SetWindowText(GetDlgItem(hwndDlg, IDC_RESBLKW), txt);
        sprintf(txt, "%d", (int) blockheight);
        SetWindowText(GetDlgItem(hwndDlg, IDC_RESBLKH), txt);
        SendDlgItemMessage(hwndDlg, IDC_RESBLKD, CB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(hwndDlg, IDC_RESBLKD, CB_ADDSTRING, 0, (LPARAM) "8");
        SendDlgItemMessage(hwndDlg, IDC_RESBLKD, CB_ADDSTRING, 0, (LPARAM) "15");
        SendDlgItemMessage(hwndDlg, IDC_RESBLKD, CB_ADDSTRING, 0, (LPARAM) "16");
        SendDlgItemMessage(hwndDlg, IDC_RESBLKD, CB_ADDSTRING, 0, (LPARAM) "24");
        SendDlgItemMessage(hwndDlg, IDC_RESBLKD, CB_ADDSTRING, 0, (LPARAM) "32");
        {
            int sel = 0;
            if (blockdepth == 15) sel = 1;
            else if (blockdepth == 16) sel = 2;
            else if (blockdepth == 24) sel = 3;
            else if (blockdepth == 32) sel = 4;
            SendDlgItemMessage(hwndDlg, IDC_RESBLKD, CB_SETCURSEL, sel, 0);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            GetWindowText(GetDlgItem(hwndDlg, IDC_RESBLKW), txt, 32);
            neww = atoi(txt);
            GetWindowText(GetDlgItem(hwndDlg, IDC_RESBLKH), txt, 32);
            newh = atoi(txt);
            if (neww < 4 || neww > 252 || newh < 4 || newh > 252) {
                MessageBox(hwndDlg, "Width and height must be between 4 and 252.", "Resize Block Dimensions", MB_OK|MB_ICONINFORMATION);
                break;
            }
            {
                int sidx = SendDlgItemMessage(hwndDlg, IDC_RESBLKD, CB_GETCURSEL, 0, 0);
                int depths[] = { 8, 15, 16, 24, 32 };
                newd = depths[sidx];
            }
            if (neww == blockwidth && newh == blockheight && newd == blockdepth) {
                EndDialog(hwndDlg, 0);
                break;
            }
            {
                int tbpp_old = (blockdepth + 1) / 8;
                int tbpp_new = (newd + 1) / 8;
                int oldpixsz = blockwidth * blockheight * tbpp_old;
                int newpixsz = neww * newh * tbpp_new;
                int i;
                int * bs;
                if (oldpixsz == 0 || newpixsz == 0) break;
                bs = (int *) blockstrpt;
                for (i = 0; i < numblockstr; i++) {
                    int * rec = bs + i * 8;
                    int j;
                    for (j = 0; j < 4; j++) {
                        long val = rec[j];
                        rec[j] = (int) ((long) val * newpixsz / oldpixsz);
                    }
                }
                blockwidth = (short int) neww;
                blockheight = (short int) newh;
                blockdepth = (short int) newd;
                if (blockgfxpt != NULL) memset(blockgfxpt, 0, numblockgfx * newpixsz);
                mapaltered = 1;
                MessageBox(hwndDlg, "Block dimensions changed.\nBlock graphics have been reset to zero.", "Resize Block Dimensions", MB_OK|MB_ICONINFORMATION);
            }
            EndDialog(hwndDlg, 1);
            break;
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

void DIAResizeBlk(HWND hwnd) {

    if (mappt == NULL) {
        MessageBox(hwnd, "Load or create a map first.", "Resize Block Dimensions", MB_OK|MB_ICONINFORMATION);
        return;
    }
    DialogBox(hInstance, (LPCSTR) "D_RESIZET", hwnd, AsDlgProc(DIAResizeBlkProc));
    return;
}

/* ---------------- Remove Duplicate Graphics ---------------- */

static int OE_GfxBytes(void) {
    return blockwidth * blockheight * ((blockdepth + 1) / 8);
}

static int OE_GfxEq(int a, int b) {
    char * ga = blockgfxpt + a * OE_GfxBytes();
    char * gb = blockgfxpt + b * OE_GfxBytes();
    return (memcmp(ga, gb, OE_GfxBytes()) == 0);
}

void RemoveDuplicateGraphics(void) {
    int i, j, compacted;
    char * deleted;
    int bytes_per_gfx;

    if (blockgfxpt == NULL || numblockgfx <= 1) return;
    bytes_per_gfx = OE_GfxBytes();
    deleted = (char *) calloc(numblockgfx, 1);
    if (deleted == NULL) return;

    compacted = 0;
    for (i = 1; i < numblockgfx; i++) {
        if (deleted[i]) continue;
        for (j = 0; j < i; j++) {
            if (deleted[j]) continue;
            if (OE_GfxEq(i, j)) {
                deleted[i] = 1;
                /* reindex block references: i -> j */
                /* BLKSTR layout (32 bytes): bgoff(+0), fgoff(+4), fgoff2(+8), fgoff3(+12), ... */
                {
                    int k;
                    for (k = 0; k < numblockstr; k++) {
                        int * rec = (int *) (blockstrpt + k * 32);
                        if (rec[0] == i * bytes_per_gfx) rec[0] = j * bytes_per_gfx;
                        if (rec[1] == i * bytes_per_gfx) rec[1] = j * bytes_per_gfx;
                        if (rec[2] == i * bytes_per_gfx) rec[2] = j * bytes_per_gfx;
                        if (rec[3] == i * bytes_per_gfx) rec[3] = j * bytes_per_gfx;
                    }
                }
                break;
            }
        }
    }

    /* compact: move non-deleted forward */
    compacted = 0;
    for (i = 0; i < numblockgfx; i++) {
        if (deleted[i]) {
            compacted++;
            continue;
        }
        if (compacted > 0) {
            memcpy(blockgfxpt + (i - compacted) * bytes_per_gfx,
                blockgfxpt + i * bytes_per_gfx, bytes_per_gfx);
        }
    }
    numblockgfx -= compacted;
    free(deleted);
    mapaltered = 1;
}

/* ---------------- Remove Duplicate Blocks ---------------- */

static int OE_BlkEq(int a, int b) {
    /* BLKSTR layout: 8 int32 (32 bytes), last int32 masks bits 20-23 */
    unsigned int * ra = (unsigned int *) (blockstrpt + a * 32);
    unsigned int * rb = (unsigned int *) (blockstrpt + b * 32);
    int k;
    for (k = 0; k < 7; k++) {
        if (ra[k] != rb[k]) return 0;
    }
    if ((ra[7] & 0x0F0FFFFF) != (rb[7] & 0x0F0FFFFF)) return 0;
    return 1;
}

void RemoveDuplicateBlocks(void) {
    int i, j;

    if (blockstrpt == NULL || numblockstr <= 1) return;

    for (i = 1; i < numblockstr; i++) {
        for (j = 0; j < i; j++) {
            if (OE_BlkEq(i, j)) {
                /* duplicate: reindex all map layers i -> j */
                int k, mx, my;
                for (k = 0; k < MAXMAPLAYERS; k++) {
                    short int * mp = mapmappt[k];
                    if (mp == NULL) continue;
                    for (my = 0; my < mapheight; my++) {
                        for (mx = 0; mx < mapwidth; mx++) {
                            if (mp[mx + my * mapwidth] == (short int) i)
                                mp[mx + my * mapwidth] = (short int) j;
                        }
                    }
                }
                /* shift higher-indexed blocks down by overwriting */
                memmove(blockstrpt + i * 32, blockstrpt + (i + 1) * 32,
                    (numblockstr - i - 1) * 32);
                numblockstr--;
                i--; /* re-check this index */
                mapaltered = 1;
                break;
            }
        }
    }
}

/* ---- D_SHOWUSER: Show User Data Overlay ---- */

static const char * showuser_labels[] = {
    "Off", "User 1", "User 2", "User 3", "User 4",
    "User 5", "User 6", "User 7", "Flags"
};
#define SHOWUSER_COUNT 9

INT_PTR CALLBACK DIAShowUserProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i;
    extern int showuser;

    UNUSED(lParam);
    switch (msg) {
    case WM_INITDIALOG:
        for (i = 0; i < SHOWUSER_COUNT; i++)
            SendDlgItemMessage(hwndDlg, IDC_SHOWUSERFIELD, CB_ADDSTRING, 0, (LPARAM) showuser_labels[i]);
        SendDlgItemMessage(hwndDlg, IDC_SHOWUSERFIELD, CB_SETCURSEL, showuser, 0);
        {
            char hex[16];
            sprintf(hex, "%06X", showuser);
            SetWindowText(GetDlgItem(hwndDlg, IDC_SHOWUSERHEX), hex);
        }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            i = SendDlgItemMessage(hwndDlg, IDC_SHOWUSERFIELD, CB_GETCURSEL, 0, 0);
            if (i >= 0 && i < SHOWUSER_COUNT) showuser = i;
            EndDialog(hwndDlg, 1);
            return TRUE;
        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void DIAShowUser(HWND hwnd) {
    DialogBox(hInstance, (LPCSTR) "D_SHOWUSER", hwnd, AsDlgProc(DIAShowUserProc));
}

/* ---- D_CUSTOMBTN: Custom Mouse Buttons ---- */

static const char * btn_actions[] = {
    "None", "Current block", "Block 0", "Drag map 1", "Drag map 2",
    "Placer.lua", "Pick block", "Lay path", "Fill", "Line", "Area.lua"
};
#define BTN_ACTION_COUNT 11

extern int mb_l, mb_m, mb_r, mb_x1, mb_x2, mb_x3, mb_x4;

INT_PTR CALLBACK DIACustomBtnProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    int i, sel;
    int ids[7] = { IDC_CUSTBTN0, IDC_CUSTBTN1, IDC_CUSTBTN2, IDC_CUSTBTN3,
        IDC_CUSTBTN4, IDC_CUSTBTN5, IDC_CUSTBTN6 };
    int vals[7];

    UNUSED(lParam);
    switch (msg) {
    case WM_INITDIALOG:
        vals[0] = mb_l; vals[1] = mb_r; vals[2] = mb_m;
        vals[3] = mb_x1; vals[4] = mb_x2; vals[5] = mb_x3; vals[6] = mb_x4;
        for (i = 0; i < 7; i++) {
            int j;
            for (j = 0; j < BTN_ACTION_COUNT; j++)
                SendDlgItemMessage(hwndDlg, ids[i], CB_ADDSTRING, 0, (LPARAM) btn_actions[j]);
            sel = vals[i];
            if (sel < 0 || sel >= BTN_ACTION_COUNT) sel = 0;
            SendDlgItemMessage(hwndDlg, ids[i], CB_SETCURSEL, sel, 0);
        }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            for (i = 0; i < 7; i++) {
                sel = SendDlgItemMessage(hwndDlg, ids[i], CB_GETCURSEL, 0, 0);
                if (sel < 0 || sel >= BTN_ACTION_COUNT) sel = 0;
                vals[i] = sel;
            }
            mb_l = vals[0]; mb_r = vals[1]; mb_m = vals[2];
            mb_x1 = vals[3]; mb_x2 = vals[4]; mb_x3 = vals[5]; mb_x4 = vals[6];
            EndDialog(hwndDlg, 1);
            return TRUE;
        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void DIACustomBtn(HWND hwnd) {
    DialogBox(hInstance, (LPCSTR) "D_CUSTOMBTN", hwnd, AsDlgProc(DIACustomBtnProc));
}

/* ---- D_LYRADJ: Layer Adjust ---- */

INT_PTR CALLBACK DIALayerAdjProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    extern short int * mappt;
    extern short int mapwidth, mapheight;

    UNUSED(lParam);
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowText(GetDlgItem(hwndDlg, IDC_LYRADJVAL), "0");
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
                char txt[32];
                int adj, x, y;
                GetWindowText(GetDlgItem(hwndDlg, IDC_LYRADJVAL), txt, sizeof(txt));
                adj = atoi(txt);
                if (mappt != NULL && adj != 0) {
                    short int maxblk = numblockstr > 0 ? numblockstr - 1 : 0;
                    for (y = 0; y < mapheight; y++) {
                        for (x = 0; x < mapwidth; x++) {
                            int v = mappt[x + y * mapwidth] + adj;
                            if (v < 0) v = 0;
                            if (v > maxblk) v = maxblk;
                            mappt[x + y * mapwidth] = (short int) v;
                        }
                    }
                    mapaltered = 1;
                }
            }
            EndDialog(hwndDlg, 1);
            return TRUE;
        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void DIALayerAdj(HWND hwnd) {
    DialogBox(hInstance, (LPCSTR) "D_LYRADJ", hwnd, AsDlgProc(DIALayerAdjProc));
}

/* ---- D_REMOVE: Remove Unused/Duplicate ---- */

extern void DestroyUnusedAnims(void);
extern void DestroyUnusedBlocks(void);
extern void DestroyUnusedGraphics(void);
extern void RemoveDuplicateGraphics(void);
extern void RemoveDuplicateBlocks(void);

INT_PTR CALLBACK DIRemoveProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    UNUSED(lParam);
    switch (msg) {
    case WM_INITDIALOG:
        CheckDlgButton(hwndDlg, IDC_REMOVEANIM, 1);
        CheckDlgButton(hwndDlg, IDC_REMOVEBLOCK, 1);
        CheckDlgButton(hwndDlg, IDC_REMOVEGFX, 1);
        CheckDlgButton(hwndDlg, IDC_REMOVEDUPBLK, 1);
        CheckDlgButton(hwndDlg, IDC_REMOVEDUPGFX, 1);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (MessageBox(hwndDlg, "Really delete selected unused/duplicate items?",
                "Delete Unused/Duplicate", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                SetCursor(LoadCursor(NULL, IDC_WAIT));
                if (IsDlgButtonChecked(hwndDlg, IDC_REMOVEANIM)) DestroyUnusedAnims();
                if (IsDlgButtonChecked(hwndDlg, IDC_REMOVEBLOCK)) DestroyUnusedBlocks();
                if (IsDlgButtonChecked(hwndDlg, IDC_REMOVEGFX)) DestroyUnusedGraphics();
                if (IsDlgButtonChecked(hwndDlg, IDC_REMOVEDUPBLK)) RemoveDuplicateBlocks();
                if (IsDlgButtonChecked(hwndDlg, IDC_REMOVEDUPGFX)) RemoveDuplicateGraphics();
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            }
            EndDialog(hwndDlg, 1);
            return TRUE;
        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void DIRemove(HWND hwnd) {
    DialogBox(hInstance, (LPCSTR) "D_REMOVE", hwnd, AsDlgProc(DIRemoveProc));
}

/* ---------------- D_TILESET ---------------- */

#define TS_PANE_MAP  0
#define TS_PANE_PICK 1
#define TS_GAP 2

static const char ts_pane_class[] = "MappyTSPane";
static int ts_pane_class_ok = 0;

static int ts_edidx;
static int ts_w, ts_h, ts_hx, ts_hy;
static char ts_name[TS_NAMELEN];
static short *ts_work = NULL;
static HWND ts_map_hwnd = NULL;
static HWND ts_pick_hwnd = NULL;
static HWND ts_dlg = NULL;
static int ts_map_sx, ts_map_sy;
static int ts_pick_yoff;
static int ts_pick_mode; /* 0 still, 1 anim */
static int ts_pick_idx;
static short ts_stamp;
static int ts_paint;

static void TS_WorkFree(void)
{
    free(ts_work);
    ts_work = NULL;
}

static int TS_WorkAlloc(int w, int h)
{
    short *neu;
    int x, y, ow, oh;

    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > TS_MAXDIM) w = TS_MAXDIM;
    if (h > TS_MAXDIM) h = TS_MAXDIM;
    if (ts_work != NULL && w == ts_w && h == ts_h)
        return 0;
    neu = (short *)calloc((size_t)w * (size_t)h, sizeof(short));
    if (neu == NULL)
        return -1;
    ow = ts_w;
    oh = ts_h;
    if (ts_work != NULL) {
        for (y = 0; y < h && y < oh; y++) {
            for (x = 0; x < w && x < ow; x++)
                neu[y * w + x] = ts_work[y * ow + x];
        }
        free(ts_work);
    }
    ts_work = neu;
    ts_w = w;
    ts_h = h;
    return 0;
}

static void TS_WorkLoad(int idx)
{
    const TilesetDef *d = TS_GetDef(idx);
    int n;

    TS_WorkFree();
    ts_edidx = idx;
    ts_w = 0;
    ts_h = 0;
    ts_hx = 0;
    ts_hy = 0;
    ts_name[0] = 0;
    if (d == NULL)
        return;
    memcpy(ts_name, d->name, TS_NAMELEN);
    ts_name[TS_NAMELEN - 1] = 0;
    ts_hx = d->handle_x;
    ts_hy = d->handle_y;
    if (TS_WorkAlloc(d->width, d->height) != 0)
        return;
    n = ts_w * ts_h;
    if (d->tiles != NULL && n > 0)
        memcpy(ts_work, d->tiles, (size_t)n * sizeof(short));
}

static int TS_WorkCommit(void)
{
    int x, y;

    if (TS_GetDef(ts_edidx) == NULL)
        return -1;
    if (TS_ResizeDef(ts_edidx, ts_w, ts_h) != 0)
        return -1;
    TS_SetDefMeta(ts_edidx, ts_name, ts_hx, ts_hy);
    for (y = 0; y < ts_h; y++) {
        for (x = 0; x < ts_w; x++)
            TS_SetCell(ts_edidx, x, y, ts_work[y * ts_w + x]);
    }
    mapaltered = 1;
    return 0;
}

static void TS_BlitCell1(HDC hdc, int dx, int dy, short cell)
{
    unsigned char *buf;
    HDC mdc;
    HBITMAP hbm, hold;
    BITMAPINFOHEADER bih;
    int pitch, ani;

    if (blockwidth < 1 || blockheight < 1)
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
    BitBlt(hdc, dx, dy, blockwidth, blockheight, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, hold);
    DeleteObject(hbm);
    DeleteDC(mdc);
}

static short TS_PickCell(int idx)
{
    if (ts_pick_mode)
        return (short)(-((idx + 1) * FMP_ANI_MEMUNIT));
    return (short)idx;
}

static void TS_SetLocalPick(int idx)
{
    int n = ts_pick_mode ? numblockanim : numblockstr;
    if (idx < 0) idx = 0;
    if (n > 0 && idx >= n) idx = n - 1;
    ts_pick_idx = idx;
    ts_stamp = TS_PickCell(idx);
}

static void TS_StampInfo(HWND hwndDlg)
{
    char txt[80];
    if (ts_pick_mode)
        sprintf(txt, "Anim %d", ts_pick_idx);
    else
        sprintf(txt, "Still %d", ts_pick_idx);
    SetWindowText(GetDlgItem(hwndDlg, IDC_TSINFO), txt);
}

static void TS_DlgFill(HWND hwndDlg)
{
    char txt[128];

    SetWindowText(GetDlgItem(hwndDlg, IDC_TSNAME), ts_name);
    sprintf(txt, "%d", ts_w);
    SetWindowText(GetDlgItem(hwndDlg, IDC_TSWIDTH), txt);
    sprintf(txt, "%d", ts_h);
    SetWindowText(GetDlgItem(hwndDlg, IDC_TSHEIGHT), txt);
    sprintf(txt, "%d", ts_hx);
    SetWindowText(GetDlgItem(hwndDlg, IDC_TSHX), txt);
    sprintf(txt, "%d", ts_hy);
    SetWindowText(GetDlgItem(hwndDlg, IDC_TSHY), txt);
    sprintf(txt, "Tileset %d", ts_edidx);
    SetWindowText(hwndDlg, txt);
    TS_StampInfo(hwndDlg);
    if (ts_map_hwnd) InvalidateRect(ts_map_hwnd, NULL, FALSE);
    if (ts_pick_hwnd) InvalidateRect(ts_pick_hwnd, NULL, FALSE);
}

static int TS_DlgReadMeta(HWND hwndDlg)
{
    char tb[80];
    int w, h;

    GetWindowText(GetDlgItem(hwndDlg, IDC_TSNAME), ts_name, TS_NAMELEN - 1);
    ts_name[TS_NAMELEN - 1] = 0;
    GetWindowText(GetDlgItem(hwndDlg, IDC_TSWIDTH), tb, 80);
    w = atoi(tb);
    GetWindowText(GetDlgItem(hwndDlg, IDC_TSHEIGHT), tb, 80);
    h = atoi(tb);
    GetWindowText(GetDlgItem(hwndDlg, IDC_TSHX), tb, 80);
    ts_hx = atoi(tb);
    GetWindowText(GetDlgItem(hwndDlg, IDC_TSHY), tb, 80);
    ts_hy = atoi(tb);
    if (TS_WorkAlloc(w, h) != 0)
        return -1;
    return 0;
}

static int TS_ScrollPos(HWND hwnd, int bar, int code, int *pos, int maxv, int line)
{
    SCROLLINFO si;
    int n = *pos;

    if (line < 1) line = 1;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_TRACKPOS;
    GetScrollInfo(hwnd, bar, &si);
    switch (LOWORD(code)) {
    case SB_LINEUP: n -= line; break;
    case SB_LINEDOWN: n += line; break;
    case SB_PAGEUP: n -= (int)si.nPage; break;
    case SB_PAGEDOWN: n += (int)si.nPage; break;
    case SB_TOP: n = 0; break;
    case SB_BOTTOM: n = maxv; break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK: n = si.nTrackPos; break;
    default: return 0;
    }
    if (n < 0) n = 0;
    if (n > maxv) n = maxv;
    if (n == *pos) return 0;
    *pos = n;
    SetScrollPos(hwnd, bar, n, TRUE);
    return 1;
}

static void TS_MapUpdateScroll(HWND hwnd)
{
    RECT rc;
    SCROLLINFO si;
    int cw, ch, maxx, maxy;

    GetClientRect(hwnd, &rc);
    cw = ts_w * blockwidth;
    ch = ts_h * blockheight;
    maxx = cw - rc.right;
    maxy = ch - rc.bottom;
    if (maxx < 0) maxx = 0;
    if (maxy < 0) maxy = 0;
    if (ts_map_sx > maxx) ts_map_sx = maxx;
    if (ts_map_sy > maxy) ts_map_sy = maxy;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = cw > 0 ? cw - 1 : 0;
    si.nPage = (UINT)(rc.right > 0 ? rc.right : 1);
    si.nPos = ts_map_sx;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
    si.nMax = ch > 0 ? ch - 1 : 0;
    si.nPage = (UINT)(rc.bottom > 0 ? rc.bottom : 1);
    si.nPos = ts_map_sy;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

static int TS_PickCount(void)
{
    return ts_pick_mode ? numblockanim : numblockstr;
}

static int TS_PickBpr(HWND hwnd)
{
    RECT rc;
    int bpr;
    GetClientRect(hwnd, &rc);
    bpr = (rc.right - TS_GAP) / (blockwidth + TS_GAP);
    if (bpr < 1) bpr = 1;
    return bpr;
}

static void TS_PickUpdateScroll(HWND hwnd)
{
    RECT rc;
    SCROLLINFO si;
    int bpr, rows, vis, maxr;

    GetClientRect(hwnd, &rc);
    bpr = TS_PickBpr(hwnd);
    rows = (TS_PickCount() + bpr - 1) / bpr;
    if (rows < 1) rows = 1;
    vis = rc.bottom / (blockheight + TS_GAP);
    if (vis < 1) vis = 1;
    maxr = rows - vis;
    if (maxr < 0) maxr = 0;
    if (ts_pick_yoff > maxr) ts_pick_yoff = maxr;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = rows > 0 ? rows - 1 : 0;
    si.nPage = (UINT)vis;
    si.nPos = ts_pick_yoff;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

static void TS_PaintMap(HWND hwnd, HDC hdc)
{
    RECT rc;
    HBRUSH br;
    int x, y, dx, dy;

    GetClientRect(hwnd, &rc);
    br = CreateSolidBrush(RGB(0x44, 0x44, 0x44));
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    if (ts_work == NULL || ts_w < 1 || ts_h < 1 || blockwidth < 1 || blockheight < 1)
        return;
    for (y = 0; y < ts_h; y++) {
        dy = y * blockheight - ts_map_sy;
        if (dy + blockheight < 0 || dy > rc.bottom)
            continue;
        for (x = 0; x < ts_w; x++) {
            dx = x * blockwidth - ts_map_sx;
            if (dx + blockwidth < 0 || dx > rc.right)
                continue;
            TS_BlitCell1(hdc, dx, dy, ts_work[y * ts_w + x]);
        }
    }
}

static void TS_PaintPick(HWND hwnd, HDC hdc)
{
    RECT rc;
    HBRUSH br;
    int bpr, i, j, idx, n;

    GetClientRect(hwnd, &rc);
    br = CreateSolidBrush(RGB(0x44, 0x44, 0x44));
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    if (blockwidth < 1 || blockheight < 1)
        return;
    bpr = TS_PickBpr(hwnd);
    n = TS_PickCount();
    for (i = 0; i <= rc.bottom / (blockheight + TS_GAP) + 1; i++) {
        for (j = 0; j < bpr; j++) {
            int dx = j * (blockwidth + TS_GAP) + 1;
            int dy = i * (blockheight + TS_GAP) + 1;
            idx = (ts_pick_yoff + i) * bpr + j;
            if (idx >= n)
                continue;
            TS_BlitCell1(hdc, dx, dy, TS_PickCell(idx));
            if (idx == ts_pick_idx) {
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                HPEN old = SelectObject(hdc, pen);
                HBRUSH oldb = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
                Rectangle(hdc, dx, dy, dx + blockwidth, dy + blockheight);
                SelectObject(hdc, old);
                SelectObject(hdc, oldb);
                DeleteObject(pen);
            }
        }
    }
}

static void TS_MapHit(HWND hwnd, LPARAM lParam, int clear)
{
    int mx, my, cx, cy;

    if (ts_work == NULL || ts_w < 1 || ts_h < 1 || blockwidth < 1 || blockheight < 1)
        return;
    if (ts_dlg)
        TS_DlgReadMeta(ts_dlg);
    mx = LOWORD(lParam) + ts_map_sx;
    my = HIWORD(lParam) + ts_map_sy;
    cx = mx / blockwidth;
    cy = my / blockheight;
    if (cx < 0 || cy < 0 || cx >= ts_w || cy >= ts_h)
        return;
    ts_work[cy * ts_w + cx] = clear ? 0 : ts_stamp;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void TS_PickHit(HWND hwnd, LPARAM lParam)
{
    RECT rc;
    int bpr, col, row, idx, n;

    GetClientRect(hwnd, &rc);
    if (blockwidth < 1 || blockheight < 1)
        return;
    bpr = TS_PickBpr(hwnd);
    col = LOWORD(lParam) / (blockwidth + TS_GAP);
    row = ts_pick_yoff + HIWORD(lParam) / (blockheight + TS_GAP);
    if (col < 0 || col >= bpr)
        return;
    idx = row * bpr + col;
    n = TS_PickCount();
    if (idx < 0 || idx >= n)
        return;
    TS_SetLocalPick(idx);
    if (ts_dlg)
        TS_StampInfo(ts_dlg);
    InvalidateRect(hwnd, NULL, FALSE);
}

static LRESULT CALLBACK TSPaneProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int pane = (int)(INT_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    PAINTSTRUCT ps;
    HDC hdc;

    switch (msg) {
    case WM_NCCREATE: {
            CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        if (pane == TS_PANE_MAP)
            TS_PaintMap(hwnd, hdc);
        else
            TS_PaintPick(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        if (pane == TS_PANE_MAP) {
            ts_paint = 1;
            SetCapture(hwnd);
            TS_MapHit(hwnd, lParam, 0);
        } else {
            TS_PickHit(hwnd, lParam);
        }
        return 0;
    case WM_RBUTTONDOWN:
        if (pane == TS_PANE_MAP) {
            ts_paint = 2;
            SetCapture(hwnd);
            TS_MapHit(hwnd, lParam, 1);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (pane == TS_PANE_MAP && ts_paint)
            TS_MapHit(hwnd, lParam, ts_paint == 2);
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        if (ts_paint) {
            ts_paint = 0;
            ReleaseCapture();
        }
        return 0;
    case WM_VSCROLL:
        if (pane == TS_PANE_MAP) {
            int maxy = ts_h * blockheight;
            RECT rc; GetClientRect(hwnd, &rc);
            maxy -= rc.bottom;
            if (maxy < 0) maxy = 0;
            if (TS_ScrollPos(hwnd, SB_VERT, (int)wParam, &ts_map_sy, maxy, blockheight))
                InvalidateRect(hwnd, NULL, FALSE);
        } else {
            int bpr = TS_PickBpr(hwnd);
            int rows = (TS_PickCount() + bpr - 1) / bpr;
            RECT rc; GetClientRect(hwnd, &rc);
            int vis = rc.bottom / (blockheight + TS_GAP);
            int maxr = rows - (vis < 1 ? 1 : vis);
            if (maxr < 0) maxr = 0;
            if (TS_ScrollPos(hwnd, SB_VERT, (int)wParam, &ts_pick_yoff, maxr, 1))
                InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_HSCROLL:
        if (pane == TS_PANE_MAP) {
            int maxx = ts_w * blockwidth;
            RECT rc; GetClientRect(hwnd, &rc);
            maxx -= rc.right;
            if (maxx < 0) maxx = 0;
            if (TS_ScrollPos(hwnd, SB_HORZ, (int)wParam, &ts_map_sx, maxx, blockwidth))
                InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEWHEEL: {
            int delta = (short)HIWORD(wParam);
            if (pane == TS_PANE_MAP) {
                ts_map_sy += (delta > 0) ? -blockheight : blockheight;
                TS_MapUpdateScroll(hwnd);
            } else {
                ts_pick_yoff += (delta > 0) ? -1 : 1;
                if (ts_pick_yoff < 0) ts_pick_yoff = 0;
                TS_PickUpdateScroll(hwnd);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
    case WM_SIZE:
        if (pane == TS_PANE_MAP)
            TS_MapUpdateScroll(hwnd);
        else
            TS_PickUpdateScroll(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static int TS_RegisterPanes(void)
{
    WNDCLASS wc;
    if (ts_pane_class_ok)
        return 1;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = TSPaneProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
    wc.lpszClassName = ts_pane_class;
    if (!RegisterClass(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;
    ts_pane_class_ok = 1;
    return 1;
}

static HWND TS_CreatePane(HWND parent, int id, int pane)
{
    HWND ph, child;
    RECT rc;

    ph = GetDlgItem(parent, id);
    if (ph == NULL)
        return NULL;
    GetWindowRect(ph, &rc);
    MapWindowPoints(NULL, parent, (POINT *)&rc, 2);
    ShowWindow(ph, SW_HIDE);
    {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS;
        if (pane == TS_PANE_MAP)
            style |= WS_HSCROLL;
        child = CreateWindowEx(WS_EX_CLIENTEDGE, ts_pane_class, "",
            style,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            parent, (HMENU)(INT_PTR)id, hInstance, (LPVOID)(INT_PTR)pane);
    }
    return child;
}

INT_PTR CALLBACK DIATilesetEditProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
        TS_WorkLoad((int)lParam);
        if (ts_work == NULL) {
            EndDialog(hwndDlg, -1L);
            break;
        }
        ts_dlg = hwndDlg;
        ts_map_sx = ts_map_sy = 0;
        ts_pick_yoff = 0;
        ts_pick_mode = 0;
        TS_SetLocalPick(0);
        ts_paint = 0;
        if (!TS_RegisterPanes()) {
            EndDialog(hwndDlg, -1L);
            break;
        }
        ts_map_hwnd = TS_CreatePane(hwndDlg, IDC_TSGRID, TS_PANE_MAP);
        ts_pick_hwnd = TS_CreatePane(hwndDlg, IDC_TSPICK, TS_PANE_PICK);
        if (ts_map_hwnd) TS_MapUpdateScroll(ts_map_hwnd);
        if (ts_pick_hwnd) TS_PickUpdateScroll(ts_pick_hwnd);
        TS_DlgFill(hwndDlg);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_TSWIDTH:
        case IDC_TSHEIGHT:
            if (HIWORD(wParam) == EN_KILLFOCUS) {
                TS_DlgReadMeta(hwndDlg);
                if (ts_map_hwnd) {
                    TS_MapUpdateScroll(ts_map_hwnd);
                    InvalidateRect(ts_map_hwnd, NULL, FALSE);
                }
            }
            break;
        case IDC_TSPICKSTILL:
        case IDC_TSPICKANIM:
            ts_pick_mode = (LOWORD(wParam) == IDC_TSPICKANIM);
            ts_pick_yoff = 0;
            TS_SetLocalPick(0);
            TS_StampInfo(hwndDlg);
            if (ts_pick_hwnd) {
                TS_PickUpdateScroll(ts_pick_hwnd);
                InvalidateRect(ts_pick_hwnd, NULL, FALSE);
            }
            break;
        case IDC_TSNEW: {
                int ni;
                TS_DlgReadMeta(hwndDlg);
                TS_WorkCommit();
                ni = TS_NewDef(4, 4);
                if (ni < 0) {
                    MessageBox(hwndDlg, "Can't create tileset (max reached).",
                        "Tileset", MB_OK | MB_ICONINFORMATION);
                    break;
                }
                ts_map_sx = ts_map_sy = 0;
                TS_WorkLoad(ni);
                if (ts_map_hwnd) TS_MapUpdateScroll(ts_map_hwnd);
                TS_DlgFill(hwndDlg);
                mapaltered = 1;
                break;
            }
        case IDC_TSDUP: {
                int ni;
                TS_DlgReadMeta(hwndDlg);
                TS_WorkCommit();
                ni = TS_DuplicateDef(ts_edidx);
                if (ni < 0) {
                    MessageBox(hwndDlg, "Can't duplicate tileset.",
                        "Tileset", MB_OK | MB_ICONINFORMATION);
                    break;
                }
                ts_map_sx = ts_map_sy = 0;
                TS_WorkLoad(ni);
                if (ts_map_hwnd) TS_MapUpdateScroll(ts_map_hwnd);
                TS_DlgFill(hwndDlg);
                mapaltered = 1;
                break;
            }
        case IDC_TSDEL:
            if (MessageBox(hwndDlg, "Delete this tileset?", "Tileset",
                MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
                TS_DeleteDef(ts_edidx);
                mapaltered = 1;
                ts_map_hwnd = NULL;
                ts_pick_hwnd = NULL;
                ts_dlg = NULL;
                TS_WorkFree();
                EndDialog(hwndDlg, 0);
            }
            break;
        case IDC_TSPREV:
        case IDC_TSNEXT: {
                int ni = ts_edidx;
                TS_DlgReadMeta(hwndDlg);
                TS_WorkCommit();
                if (LOWORD(wParam) == IDC_TSPREV) {
                    if (ni > 0) ni--;
                } else if (ni < TS_NumDefs() - 1) {
                    ni++;
                }
                ts_map_sx = ts_map_sy = 0;
                TS_WorkLoad(ni);
                if (ts_map_hwnd) TS_MapUpdateScroll(ts_map_hwnd);
                TS_DlgFill(hwndDlg);
                break;
            }
        case IDC_TSGRAB:
            TS_DlgReadMeta(hwndDlg);
            if (TS_WorkCommit() != 0)
                break;
            ts_map_hwnd = NULL;
            ts_pick_hwnd = NULL;
            ts_dlg = NULL;
            TS_WorkFree();
            EndDialog(hwndDlg, 3);
            break;
        case IDOK:
            TS_DlgReadMeta(hwndDlg);
            TS_WorkCommit();
            ts_map_hwnd = NULL;
            ts_pick_hwnd = NULL;
            ts_dlg = NULL;
            TS_WorkFree();
            EndDialog(hwndDlg, 0);
            break;
        case IDCANCEL:
            ts_map_hwnd = NULL;
            ts_pick_hwnd = NULL;
            ts_dlg = NULL;
            TS_WorkFree();
            EndDialog(hwndDlg, -1);
            break;
        }
        break;
    case WM_CLOSE:
        ts_map_hwnd = NULL;
        ts_pick_hwnd = NULL;
        ts_dlg = NULL;
        TS_WorkFree();
        EndDialog(hwndDlg, -1);
        break;
    default:
        return 0;
    }
    return -1;
}

void DIATilesetEdit(HWND hwnd, int idx)
{
    int r;

    if (mappt == NULL) {
        MessageBox(hwnd, "Load or create a map first.", "Tileset",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (TS_GetDef(idx) == NULL) {
        idx = TS_NewDef(4, 4);
        if (idx < 0) {
            MessageBox(hwnd, "Can't create tileset (max reached).", "Tileset",
                MB_OK | MB_ICONINFORMATION);
            return;
        }
        mapaltered = 1;
    }
    TS_SetCurDef(idx);
    r = (int)DialogBoxParam(hInstance, (LPCSTR)"D_TILESET", hwnd,
        AsDlgProc(DIATilesetEditProc), (LPARAM)idx);
    TS_WorkFree();
    if (r == 3) {
        megetatileset = 1;
        MessageBox(hwnd, "Drag a rectangle on the map to copy tiles into this tileset.",
            "Grab tileset", MB_OK | MB_ICONINFORMATION);
    }
}


