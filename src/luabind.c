/* luabind.c -- Lua 5.0.3 <-> MappyWin32 bridge (1.4.23 mappy.* API) */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "mapwin.h"
#include "compat.h"
#include "mapdefs.h"
#include "fmp.h"
#include "fmp14.h"
#include "luabind.h"

#define ANDTMEM (65536*4)
#define BLOCKSTRSIZE 32
#define BKGFXMEM (1048576*8)
#define BKDTMEM (32768*32)

#define LV_MAPWIDTH 0
#define LV_MAPHEIGHT 1
#define LV_BLOCKWIDTH 2
#define LV_BLOCKHEIGHT 3
#define LV_BLOCKDEPTH 4
#define LV_NUMBLOCKSTR 5
#define LV_NUMBLOCKGFX 6
#define LV_BLOCKGAPX 7
#define LV_BLOCKGAPY 8
#define LV_BLOCKSTAGGERX 9
#define LV_BLOCKSTAGGERY 10
#define LV_CURLAYER 11
#define LV_CURBLOCK 12
#define LV_CURANIM 13
#define LV_CURBRUSH 14
#define LV_MOUSEBLOCKX 15
#define LV_MOUSEBLOCKY 16
#define LV_GETBLOCKMODE 17
#define LV_MAPFILENAME 18
#define LV_MAPWINPATH 19
#define LV_MAPWINVERSION 20
#define LV_MAPTYPE 21
#define LV_NUMBRUSHES 22
#define LV_BRUSHWIDTH 23
#define LV_BRUSHNAME 24
#define LV_CUROBJECT 25
#define LV_NUMOBJECTS 26
#define LV_OBJXPOS 27
#define LV_OBJYPOS 28
#define LV_OBJGFXID 29
#define LV_OBJTILEID 30
#define LV_OBJGXOFF 31
#define LV_OBJGYOFF 32
#define LV_OBJGWIDTH 33
#define LV_OBJGHEIGHT 34
#define LV_OBJGHANDLEXOFF 35
#define LV_OBJGHANDLEYOFF 36
#define LV_OBJSHOW 37
#define LV_OBJUSER1 38
#define LV_OBJUSER2 39
#define LV_OBJUSER3 40
#define LV_OBJUSER4 41
#define LV_OBJUSER5 42
#define LV_OBJUSER6 43
#define LV_OBJUSER7 44
#define LV_OBJFLAGS 45
#define LV_NUMCHUNKS 46
#define LV_CURGFX 47
#define LV_STRBLKUSER 48
#define LV_MAPONION 49
#define LV_MAPDARK 50
#define LV_MOUSEBLOCKX2 51
#define LV_MOUSEBLOCKY2 52
#define LV_BLOCKLABELS 53
#define LV_OBJECTLABELS 54
#define LV_CUREDIT 55
#define LV_BLOCKSPERROW 56
#define LV_BLOCKS 57
#define LV_NUMANIMS 59
#define LV_NUMBLOCKANIM 60
#define LV_XMAPOFFSET 61
#define LV_YMAPOFFSET 62
#define LV_MAPDIVIDERS 63
#define LV_CLICKMASK 64
#define LV_TRANS8 65
#define LV_TRANSRED 66
#define LV_TRANSGREEN 67
#define LV_TRANSBLUE 68
#define LV_NUMLAYERS 69
#define LV_CMDLINE 70
#define LV_NUMTEXTSTR 71
#define LV_NUMFILESTR 72
#define LV_BEWINDOWTYPE 73
#define LV_STROBJUSER 74
#define LV_BRUSHHEIGHT 75
#define LV_PALETTEARGB 1000

#define ANM_TYPE 0
#define ANM_DELAY 1
#define ANM_COUNT 2
#define ANM_USER 3
#define ANM_CURFRAME 4
#define ANM_REFFRAME -1

#define CHUNK_ID -1
#define CHUNK_SIZE -2

typedef struct {
    long int bgoff, fgoff, fgoff2, fgoff3;
    unsigned long int user1, user2;
    unsigned short int user3, user4;
    unsigned char user5, user6, user7;
    unsigned char tl : 1, tr : 1, bl : 1, br : 1;
    unsigned char trigger : 1, unused1 : 1, unused2 : 1, unused3 : 1;
} BLKSTR;

typedef struct {
    signed char antype, andelay, ancount, anuser;
    long int ancuroff, anstartoff, anendoff;
} ANISTR;

extern short int mapwidth, mapheight, blockwidth, blockheight, blockdepth;
extern short int numblockstr, numblockgfx, numblockanim, numbrushes;
extern short int curstr, curanim, curbrsh, maplayer;
extern short int xmapoffset, ymapoffset;
extern short int betype, bebpr, curgfxsel;
extern int blockgapx, blockgapy, blockstaggerx, blockstaggery;
extern int clickmask, maptype, mapaltered, objshow;
extern int onion, oniondark, divideron, dividergapx, dividergapy;
extern long dividercol;
extern char *blockgfxpt, *blockstrpt, *animstrpt;
extern unsigned char *cmappt;
extern short int *mapmappt[MAXMAPLAYERS];
extern short int *mappt;
extern short int *brshpt[MAXBRSH];
extern char brshname[MAXBRSH][64];
extern char mapfilename[];
extern unsigned char trans8bit, transred, transgreen, transblue;
extern char blocklabels[16][64];
extern char objlabels[16][64];
extern HWND hwnd;
extern HINSTANCE hInstance;

static void dbglog_line(const char * filename) {
    FILE * f = fopen("lua_test.log", "a");
    if (f == NULL) return;
    fprintf(f, "LuaRunScript: %s\n", filename ? filename : "");
    fclose(f);
}

static lua_State *L = NULL;
static char mapwinpath[MAX_PATH];
static int mouse_x, mouse_y, mouse_x2, mouse_y2;
static int curedit;
static int strblkuser = 1, strobjuser = 1;
static int getblockmode;
static int blocksort[MAXBLKSTR];
static int objectsort[F14_MAXOBJ];
static int nblocksort, nobjectsort;

static HWND dlg_hwnd;
static UINT dlg_msg;
static WPARAM dlg_wp;
static LPARAM dlg_lp;
static char dlgproc_path[MAX_PATH];
static char req_title[80], req_msg[1024], req_edit[2048];

#define MAXDLGITEMS 32
static struct {
    char title[128];
    int style, x, y, w, h, nitems, nadded;
    struct {
        char title[128];
        int style, x, y, w, h, idc, type;
    } items[MAXDLGITEMS];
} luadlg;

char luascriptmenu[MAXLUAMENU][260];
int numluascript = 0;

typedef struct { const char *name; int val; } LuaConst;

static const LuaConst all_consts[] = {
    { "MAPWIDTH", LV_MAPWIDTH }, { "MAPHEIGHT", LV_MAPHEIGHT },
    { "BLOCKWIDTH", LV_BLOCKWIDTH }, { "BLOCKHEIGHT", LV_BLOCKHEIGHT },
    { "BLOCKDEPTH", LV_BLOCKDEPTH }, { "NUMBLOCKSTR", LV_NUMBLOCKSTR },
    { "NUMGRAPHICS", LV_NUMBLOCKGFX }, { "NUMBLOCKGFX", LV_NUMBLOCKGFX },
    { "BLOCKGAPX", LV_BLOCKGAPX }, { "BLOCKGAPY", LV_BLOCKGAPY },
    { "BLOCKSTAGGERX", LV_BLOCKSTAGGERX }, { "BLOCKSTAGGERY", LV_BLOCKSTAGGERY },
    { "CURLAYER", LV_CURLAYER }, { "CURBLOCK", LV_CURBLOCK },
    { "CURANIM", LV_CURANIM }, { "CURBRUSH", LV_CURBRUSH },
    { "MOUSEBLOCKX", LV_MOUSEBLOCKX }, { "MOUSEBLOCKY", LV_MOUSEBLOCKY },
    { "GETBLOCKMODE", LV_GETBLOCKMODE }, { "MAPFILENAME", LV_MAPFILENAME },
    { "MAPWINPATH", LV_MAPWINPATH }, { "MAPWINVERSION", LV_MAPWINVERSION },
    { "MAPTYPE", LV_MAPTYPE }, { "NUMBRUSHES", LV_NUMBRUSHES },
    { "BRUSHWIDTH", LV_BRUSHWIDTH }, { "BRUSHHEIGHT", LV_BRUSHHEIGHT },
    { "BRUSHNAME", LV_BRUSHNAME }, { "CUROBJECT", LV_CUROBJECT },
    { "NUMOBJECTS", LV_NUMOBJECTS },
    { "OBJXPOS", LV_OBJXPOS }, { "OBJYPOS", LV_OBJYPOS },
    { "OBJGFXID", LV_OBJGFXID }, { "OBJTILEID", LV_OBJTILEID },
    { "OBJGXOFF", LV_OBJGXOFF }, { "OBJGYOFF", LV_OBJGYOFF },
    { "OBJGWIDTH", LV_OBJGWIDTH }, { "OBJGHEIGHT", LV_OBJGHEIGHT },
    { "OBJGHANDLEXOFF", LV_OBJGHANDLEXOFF }, { "OBJGHANDLEYOFF", LV_OBJGHANDLEYOFF },
    { "OBJSHOW", LV_OBJSHOW },
    { "OBJUSER1", LV_OBJUSER1 }, { "OBJUSER2", LV_OBJUSER2 },
    { "OBJUSER3", LV_OBJUSER3 }, { "OBJUSER4", LV_OBJUSER4 },
    { "OBJUSER5", LV_OBJUSER5 }, { "OBJUSER6", LV_OBJUSER6 },
    { "OBJUSER7", LV_OBJUSER7 }, { "OBJFLAGS", LV_OBJFLAGS },
    { "NUMCHUNKS", LV_NUMCHUNKS }, { "CURGFX", LV_CURGFX },
    { "STRBLKUSER", LV_STRBLKUSER }, { "STROBJUSER", LV_STROBJUSER },
    { "MAPONION", LV_MAPONION }, { "MAPDARK", LV_MAPDARK },
    { "MOUSEBLOCKX2", LV_MOUSEBLOCKX2 }, { "MOUSEBLOCKY2", LV_MOUSEBLOCKY2 },
    { "BLOCKLABELS", LV_BLOCKLABELS }, { "OBJECTLABELS", LV_OBJECTLABELS },
    { "CUREDIT", LV_CUREDIT }, { "BLOCKSPERROW", LV_BLOCKSPERROW },
    { "BLOCKS", LV_BLOCKS }, { "PALETTEARGB", LV_PALETTEARGB },
    { "NUMANIMS", LV_NUMANIMS }, { "NUMBLOCKANIM", LV_NUMBLOCKANIM },
    { "XMAPOFFSET", LV_XMAPOFFSET }, { "YMAPOFFSET", LV_YMAPOFFSET },
    { "MAPDIVIDERS", LV_MAPDIVIDERS }, { "CLICKMASK", LV_CLICKMASK },
    { "TRANS8", LV_TRANS8 }, { "TRANSRED", LV_TRANSRED },
    { "TRANSGREEN", LV_TRANSGREEN }, { "TRANSBLUE", LV_TRANSBLUE },
    { "NUMLAYERS", LV_NUMLAYERS }, { "CMDLINE", LV_CMDLINE },
    { "NUMTEXTSTR", LV_NUMTEXTSTR }, { "NUMFILESTR", LV_NUMFILESTR },
    { "BEWINDOWTYPE", LV_BEWINDOWTYPE },
    { "BLKBG", 0 }, { "BLKFG1", 1 }, { "BLKFG2", 2 }, { "BLKFG3", 3 },
    { "BLKUSER1", 4 }, { "BLKUSER2", 5 }, { "BLKUSER3", 6 }, { "BLKUSER4", 7 },
    { "BLKUSER5", 8 }, { "BLKUSER6", 9 }, { "BLKUSER7", 10 },
    { "BLKFLAG1", 11 }, { "BLKFLAG2", 12 }, { "BLKFLAG3", 13 }, { "BLKFLAG4", 14 },
    { "BLKFLAG5", 15 }, { "BLKFLAG6", 16 }, { "BLKFLAG7", 17 }, { "BLKFLAG8", 18 },
    { "MMB_OK", 0 }, { "MMB_OKCANCEL", MB_OKCANCEL },
    { "MMB_YESNOCANCEL", MB_YESNOCANCEL },
    { "MMB_ICONNONE", 0 }, { "MMB_ICONINFO", MB_ICONINFORMATION },
    { "MMB_ICONQUESTION", MB_ICONQUESTION },
    { "MMB_ICONEXCLAMATION", MB_ICONEXCLAMATION },
    { "MMB_CANCEL", IDCANCEL }, { "MMB_YES", IDYES }, { "MMB_NO", IDNO },
    { "MMB_OPEN", 1 }, { "MMB_SAVE", 2 },
    { "MMB_DIALOGUE1", 1 }, { "MMB_DIALOGUE2", 2 },
    { "MPY_UNDO", -9999 },
    { "MPY_MAINWINDOW", 0 }, { "MPY_MAPWINDOW", 1 }, { "MPY_BLOCKWINDOW", 2 },
    { "ANMTYPE", ANM_TYPE }, { "ANMDELAY", ANM_DELAY }, { "ANMCOUNT", ANM_COUNT },
    { "ANMUSER", ANM_USER }, { "ANMCURFRAME", ANM_CURFRAME },
    { "ANMREFFRAME", ANM_REFFRAME },
    { "AN_END", -1 }, { "AN_NONE", 0 }, { "AN_LOOPF", 1 }, { "AN_LOOPR", 2 },
    { "AN_ONCE", 3 }, { "AN_ONCEH", 4 }, { "AN_PPFF", 5 }, { "AN_PPRR", 6 },
    { "AN_PPRF", 7 }, { "AN_PPFR", 8 }, { "AN_ONCES", 9 },
    { "CHUNKID", CHUNK_ID }, { "CHUNKSIZE", CHUNK_SIZE },
    { NULL, 0 }
};

static void register_constants(lua_State *LS) {
    const LuaConst *c;
    lua_newtable(LS);
    lua_setglobal(LS, "mappy");
    lua_getglobal(LS, "mappy");
    for (c = all_consts; c->name; c++) {
        lua_pushstring(LS, c->name);
        lua_pushnumber(LS, c->val);
        lua_rawset(LS, -3);
    }
    lua_pop(LS, 1);
}

static int get_const(lua_State *LS, int idx) {
    const LuaConst *c;
    if (lua_isnumber(LS, idx)) return (int) lua_tonumber(LS, idx);
    if (lua_isstring(LS, idx)) {
        const char *s = lua_tostring(LS, idx);
        for (c = all_consts; c->name; c++)
            if (!strcmp(s, c->name)) return c->val;
    }
    return 0;
}

static int gfx_bytes(void) {
    int bpp = (blockdepth + 1) / 8;
    if (bpp < 1) bpp = 1;
    return (int) blockwidth * (int) blockheight * bpp;
}

static int off_to_gfx(long off) {
    int b = gfx_bytes();
    if (b <= 0) return 0;
    return (int) (off / b);
}

static int count_layers(void) {
    int i, n = 0;
    for (i = 0; i < MAXMAPLAYERS; i++)
        if (mapmappt[i]) n++;
    return n;
}

static int count_brushes(void) {
    int i, n = 0;
    for (i = 0; i < MAXBRSH; i++)
        if (brshpt[i]) n++;
    return n;
}

static ANISTR *ani_ptr(int anim) {
    if (animstrpt == NULL || anim < 0) return NULL;
    return ((ANISTR *) ((char *) animstrpt + ANDTMEM)) - (anim + 1);
}

static BLKSTR *blk_ptr(int blk) {
    if (blockstrpt == NULL || blk < 0 || blk >= numblockstr) return NULL;
    return ((BLKSTR *) blockstrpt) + blk;
}

static void parse_labels(char dest[16][64], const char *s) {
    int bl = 0, bc = 0;
    if (s == NULL) return;
    while (*s && bl < 16) {
        if (*s == ';') { dest[bl][bc] = 0; bl++; bc = 0; }
        else if (bc < 63) dest[bl][bc++] = *s;
        s++;
    }
    if (bl < 16) dest[bl][bc] = 0;
}

static int win_to_mmb(int r) {
    if (r == IDOK) return 0;
    return r;
}

static int is_abs_path(const char *p) {
    if (p == NULL || p[0] == 0) return 0;
    if (p[0] == '\\' || p[0] == '/') return 1;
    if (p[1] == ':') return 1;
    return 0;
}

const char *LuaGetMapwinPath(void) {
    return mapwinpath;
}

void LuaSetMouseBlock(int x, int y, int x2, int y2) {
    mouse_x = x; mouse_y = y; mouse_x2 = x2; mouse_y2 = y2;
}

static int m_getValue(lua_State *LS) {
    int c = get_const(LS, 1);
    if (c >= LV_PALETTEARGB && c < LV_PALETTEARGB + 256) {
        int idx = c - LV_PALETTEARGB;
        if (cmappt && idx >= 0 && idx < 256) {
            lua_pushnumber(LS, 255);
            lua_pushnumber(LS, cmappt[idx * 3]);
            lua_pushnumber(LS, cmappt[idx * 3 + 1]);
            lua_pushnumber(LS, cmappt[idx * 3 + 2]);
            return 4;
        }
        lua_pushnumber(LS, 255); lua_pushnumber(LS, 0);
        lua_pushnumber(LS, 0); lua_pushnumber(LS, 0);
        return 4;
    }
    switch (c) {
    case LV_MAPWIDTH: lua_pushnumber(LS, mapwidth); break;
    case LV_MAPHEIGHT: lua_pushnumber(LS, mapheight); break;
    case LV_BLOCKWIDTH: lua_pushnumber(LS, blockwidth); break;
    case LV_BLOCKHEIGHT: lua_pushnumber(LS, blockheight); break;
    case LV_BLOCKDEPTH: lua_pushnumber(LS, blockdepth); break;
    case LV_NUMBLOCKSTR: lua_pushnumber(LS, numblockstr); break;
    case LV_NUMBLOCKGFX: lua_pushnumber(LS, numblockgfx); break;
    case LV_BLOCKGAPX: lua_pushnumber(LS, blockgapx); break;
    case LV_BLOCKGAPY: lua_pushnumber(LS, blockgapy); break;
    case LV_BLOCKSTAGGERX: lua_pushnumber(LS, blockstaggerx); break;
    case LV_BLOCKSTAGGERY: lua_pushnumber(LS, blockstaggery); break;
    case LV_CURLAYER: lua_pushnumber(LS, maplayer); break;
    case LV_CURBLOCK: lua_pushnumber(LS, curstr); break;
    case LV_CURANIM: lua_pushnumber(LS, curanim); break;
    case LV_CURBRUSH: lua_pushnumber(LS, curbrsh); break;
    case LV_MOUSEBLOCKX: lua_pushnumber(LS, mouse_x); break;
    case LV_MOUSEBLOCKY: lua_pushnumber(LS, mouse_y); break;
    case LV_GETBLOCKMODE: lua_pushnumber(LS, getblockmode); break;
    case LV_MAPFILENAME: lua_pushstring(LS, mapfilename); return 1;
    case LV_MAPWINPATH: lua_pushstring(LS, mapwinpath); return 1;
    case LV_MAPWINVERSION: lua_pushstring(LS, "1.4.23"); return 1;
    case LV_MAPTYPE: lua_pushnumber(LS, maptype); break;
    case LV_NUMBRUSHES: lua_pushnumber(LS, count_brushes()); break;
    case LV_CUROBJECT: lua_pushnumber(LS, F14_CurObject()); break;
    case LV_NUMOBJECTS: lua_pushnumber(LS, F14_NumObjects()); break;
    case LV_OBJSHOW: lua_pushnumber(LS, objshow); break;
    case LV_NUMCHUNKS: lua_pushnumber(LS, FmpChunkCount()); break;
    case LV_CURGFX: lua_pushnumber(LS, curgfxsel); break;
    case LV_STRBLKUSER: lua_pushnumber(LS, strblkuser); break;
    case LV_STROBJUSER: lua_pushnumber(LS, strobjuser); break;
    case LV_MAPONION: lua_pushnumber(LS, onion); break;
    case LV_MAPDARK: lua_pushnumber(LS, oniondark); break;
    case LV_MOUSEBLOCKX2: lua_pushnumber(LS, mouse_x2); break;
    case LV_MOUSEBLOCKY2: lua_pushnumber(LS, mouse_y2); break;
    case LV_CUREDIT: lua_pushnumber(LS, curedit); break;
    case LV_BLOCKSPERROW: lua_pushnumber(LS, bebpr); break;
    case LV_BLOCKS: lua_pushnumber(LS, numblockstr); break;
    case LV_NUMANIMS:
    case LV_NUMBLOCKANIM: lua_pushnumber(LS, numblockanim); break;
    case LV_XMAPOFFSET: lua_pushnumber(LS, xmapoffset); break;
    case LV_YMAPOFFSET: lua_pushnumber(LS, ymapoffset); break;
    case LV_MAPDIVIDERS:
        lua_pushnumber(LS, divideron);
        lua_pushnumber(LS, dividergapx);
        lua_pushnumber(LS, dividergapy);
        lua_pushnumber(LS, (lua_Number) dividercol);
        return 4;
    case LV_CLICKMASK: lua_pushnumber(LS, clickmask); break;
    case LV_TRANS8: lua_pushnumber(LS, trans8bit); break;
    case LV_TRANSRED: lua_pushnumber(LS, transred); break;
    case LV_TRANSGREEN: lua_pushnumber(LS, transgreen); break;
    case LV_TRANSBLUE: lua_pushnumber(LS, transblue); break;
    case LV_NUMLAYERS: lua_pushnumber(LS, count_layers()); break;
    case LV_CMDLINE: {
            const char *cmd = MapGetCmdLine();
            lua_pushstring(LS, cmd ? cmd : "");
            return 1;
        }
    case LV_NUMTEXTSTR: lua_pushnumber(LS, F14_TstrCount()); break;
    case LV_NUMFILESTR: lua_pushnumber(LS, F14_NumImages()); break;
    case LV_BEWINDOWTYPE: lua_pushnumber(LS, betype); break;
    case LV_BLOCKLABELS: {
            char buf[1024]; int i; buf[0] = 0;
            for (i = 0; i < 16; i++) {
                strcat(buf, blocklabels[i]);
                strcat(buf, ";");
            }
            lua_pushstring(LS, buf);
            return 1;
        }
    case LV_OBJECTLABELS: {
            char buf[1024]; int i; buf[0] = 0;
            for (i = 0; i < 16; i++) {
                strcat(buf, objlabels[i]);
                strcat(buf, ";");
            }
            lua_pushstring(LS, buf);
            return 1;
        }
    default: lua_pushnumber(LS, 0); break;
    }
    return 1;
}

static int m_setValue(lua_State *LS) {
    int c = get_const(LS, 1);
    int v = (int) lua_tonumber(LS, 2);
    if (c >= LV_PALETTEARGB && c < LV_PALETTEARGB + 256) {
        int idx = c - LV_PALETTEARGB;
        if (cmappt && idx >= 0 && idx < 256) {
            int r = lua_gettop(LS) >= 3 ? (int) lua_tonumber(LS, 3) : v;
            int g = lua_gettop(LS) >= 4 ? (int) lua_tonumber(LS, 4) : 0;
            int b = lua_gettop(LS) >= 5 ? (int) lua_tonumber(LS, 5) : 0;
            if (lua_gettop(LS) >= 5) { /* a,r,g,b */
                r = (int) lua_tonumber(LS, 3);
                g = (int) lua_tonumber(LS, 4);
                b = (int) lua_tonumber(LS, 5);
            }
            cmappt[idx * 3] = (unsigned char) r;
            cmappt[idx * 3 + 1] = (unsigned char) g;
            cmappt[idx * 3 + 2] = (unsigned char) b;
        }
        return 0;
    }
    switch (c) {
    case LV_CURLAYER:
        if (v >= 0 && v < MAXMAPLAYERS && mapmappt[v]) {
            maplayer = (short) v; mappt = mapmappt[v];
        }
        break;
    case LV_CURBLOCK:
        if (v >= 0 && v < numblockstr) curstr = (short) v;
        break;
    case LV_CURANIM: curanim = (short) v; break;
    case LV_CURBRUSH:
        if (v >= 0 && v <= MAXBRSH) curbrsh = (short) v;
        break;
    case LV_GETBLOCKMODE: getblockmode = v; break;
    case LV_MAPFILENAME:
        if (lua_isstring(LS, 2)) {
            strncpy(mapfilename, lua_tostring(LS, 2), 255);
            mapfilename[255] = 0;
            Rejig();
        }
        break;
    case LV_MAPTYPE: maptype = v; break;
    case LV_CUROBJECT: F14_SetCurObject(v); break;
    case LV_OBJSHOW: objshow = v; break;
    case LV_CURGFX:
        if (v >= 0 && v < numblockgfx) curgfxsel = (short) v;
        break;
    case LV_STRBLKUSER: strblkuser = v; break;
    case LV_STROBJUSER: strobjuser = v; break;
    case LV_MAPONION: onion = v; break;
    case LV_MAPDARK: oniondark = v; break;
    case LV_XMAPOFFSET: xmapoffset = (short) v; break;
    case LV_YMAPOFFSET: ymapoffset = (short) v; break;
    case LV_MAPDIVIDERS:
        divideron = v;
        if (lua_gettop(LS) >= 3) dividergapx = (int) lua_tonumber(LS, 3);
        if (lua_gettop(LS) >= 4) dividergapy = (int) lua_tonumber(LS, 4);
        if (lua_gettop(LS) >= 5) dividercol = (long) lua_tonumber(LS, 5);
        break;
    case LV_CLICKMASK: clickmask = v; break;
    case LV_TRANS8: trans8bit = (unsigned char) v; break;
    case LV_TRANSRED: transred = (unsigned char) v; break;
    case LV_TRANSGREEN: transgreen = (unsigned char) v; break;
    case LV_TRANSBLUE: transblue = (unsigned char) v; break;
    case LV_BEWINDOWTYPE: BESetWindowType(v); break;
    case LV_BLOCKLABELS:
        if (lua_isstring(LS, 2)) parse_labels(blocklabels, lua_tostring(LS, 2));
        break;
    case LV_OBJECTLABELS:
        if (lua_isstring(LS, 2)) parse_labels(objlabels, lua_tostring(LS, 2));
        break;
    case LV_CUREDIT: curedit = v; break;
    default: break;
    }
    return 0;
}

static int m_getBlock(lua_State *LS) {
    int x = (int) lua_tonumber(LS, 1);
    int y = (int) lua_tonumber(LS, 2);
    short int *mp;
    if (lua_gettop(LS) >= 3) {
        int layer = (int) lua_tonumber(LS, 3);
        if (layer < 0 || layer >= MAXMAPLAYERS || mapmappt[layer] == NULL)
            return luaL_error(LS, "layer does not exist");
        mp = mapmappt[layer];
    } else mp = mappt;
    if (mp == NULL || x < 0 || x >= mapwidth || y < 0 || y >= mapheight) {
        lua_pushnumber(LS, 0); return 1;
    }
    lua_pushnumber(LS, mp[x + y * mapwidth]);
    return 1;
}

static int m_setBlock(lua_State *LS) {
    int x = (int) lua_tonumber(LS, 1);
    int y = (int) lua_tonumber(LS, 2);
    int b = (int) lua_tonumber(LS, 3);
    short int *mp;
    if (lua_gettop(LS) >= 4) {
        int layer = (int) lua_tonumber(LS, 4);
        if (layer < 0 || layer >= MAXMAPLAYERS || mapmappt[layer] == NULL) return 0;
        mp = mapmappt[layer];
    } else mp = mappt;
    if (mp == NULL || x < 0 || x >= mapwidth || y < 0 || y >= mapheight) return 0;
    mp[x + y * mapwidth] = (short int) b;
    mapaltered = 1;
    return 0;
}

static int m_getBlockValue(lua_State *LS) {
    BLKSTR *bp = blk_ptr((int) lua_tonumber(LS, 1));
    int field = get_const(LS, 2);
    if (bp == NULL) { lua_pushnumber(LS, 0); return 1; }
    switch (field) {
    case 0: lua_pushnumber(LS, off_to_gfx(bp->bgoff)); break;
    case 1: lua_pushnumber(LS, off_to_gfx(bp->fgoff)); break;
    case 2: lua_pushnumber(LS, off_to_gfx(bp->fgoff2)); break;
    case 3: lua_pushnumber(LS, off_to_gfx(bp->fgoff3)); break;
    case 4: lua_pushnumber(LS, (lua_Number) bp->user1); break;
    case 5: lua_pushnumber(LS, (lua_Number) bp->user2); break;
    case 6: lua_pushnumber(LS, bp->user3); break;
    case 7: lua_pushnumber(LS, bp->user4); break;
    case 8: lua_pushnumber(LS, bp->user5); break;
    case 9: lua_pushnumber(LS, bp->user6); break;
    case 10: lua_pushnumber(LS, bp->user7); break;
    case 11: lua_pushnumber(LS, bp->tl); break;
    case 12: lua_pushnumber(LS, bp->tr); break;
    case 13: lua_pushnumber(LS, bp->bl); break;
    case 14: lua_pushnumber(LS, bp->br); break;
    case 15: lua_pushnumber(LS, bp->trigger); break;
    case 16: lua_pushnumber(LS, bp->unused1); break;
    case 17: lua_pushnumber(LS, bp->unused2); break;
    case 18: lua_pushnumber(LS, bp->unused3); break;
    default: lua_pushnumber(LS, 0); break;
    }
    return 1;
}

static int m_setBlockValue(lua_State *LS) {
    BLKSTR *bp = blk_ptr((int) lua_tonumber(LS, 1));
    int field = get_const(LS, 2);
    int val = (int) lua_tonumber(LS, 3);
    long off;
    if (bp == NULL) return 0;
    off = (long) val * gfx_bytes();
    switch (field) {
    case 0: bp->bgoff = off; break;
    case 1: bp->fgoff = off; break;
    case 2: bp->fgoff2 = off; break;
    case 3: bp->fgoff3 = off; break;
    case 4: bp->user1 = (unsigned long) val; break;
    case 5: bp->user2 = (unsigned long) val; break;
    case 6: bp->user3 = (unsigned short) val; break;
    case 7: bp->user4 = (unsigned short) val; break;
    case 8: bp->user5 = (unsigned char) val; break;
    case 9: bp->user6 = (unsigned char) val; break;
    case 10: bp->user7 = (unsigned char) val; break;
    case 11: bp->tl = val ? 1 : 0; break;
    case 12: bp->tr = val ? 1 : 0; break;
    case 13: bp->bl = val ? 1 : 0; break;
    case 14: bp->br = val ? 1 : 0; break;
    case 15: bp->trigger = val ? 1 : 0; break;
    case 16: bp->unused1 = val ? 1 : 0; break;
    case 17: bp->unused2 = val ? 1 : 0; break;
    case 18: bp->unused3 = val ? 1 : 0; break;
    default: break;
    }
    mapaltered = 1;
    return 0;
}

static int m_createBlock(lua_State *LS) {
    int b = lua_gettop(LS) >= 1 ? (int) lua_tonumber(LS, 1) : numblockstr;
    int saved = curstr;
    if (b < 0) b = 0;
    if (b > numblockstr) b = numblockstr;
    curstr = (short) (b > 0 ? b - 1 : -1);
    NewBlock();
    lua_pushnumber(LS, curstr);
    if (saved >= b) saved++;
    if (saved >= 0 && saved < numblockstr) curstr = (short) saved;
    mapaltered = 1;
    return 1;
}

static int m_deleteBlock(lua_State *LS) {
    int id = (int) lua_tonumber(LS, 1);
    int saved = curstr;
    if (id > 0 && id < numblockstr) {
        curstr = (short) id;
        CutBlock();
        if (saved >= id) saved--;
        if (saved < 0) saved = 0;
        if (saved >= numblockstr) saved = numblockstr - 1;
        curstr = (short) saved;
        mapaltered = 1;
    }
    return 0;
}

static void bump_gfx_offs(int from_bytes, int delta) {
    int i;
    BLKSTR *bp = (BLKSTR *) blockstrpt;
    if (bp == NULL) return;
    for (i = 0; i < numblockstr; i++, bp++) {
        if (delta > 0) {
            if (bp->bgoff >= from_bytes) bp->bgoff += delta;
            if (bp->fgoff >= from_bytes) bp->fgoff += delta;
            if (bp->fgoff2 >= from_bytes) bp->fgoff2 += delta;
            if (bp->fgoff3 >= from_bytes) bp->fgoff3 += delta;
        } else {
            int cut = from_bytes;
            int sz = -delta;
            if (bp->bgoff == cut) bp->bgoff = 0;
            else if (bp->bgoff > cut) bp->bgoff -= sz;
            if (bp->fgoff == cut) bp->fgoff = 0;
            else if (bp->fgoff > cut) bp->fgoff -= sz;
            if (bp->fgoff2 == cut) bp->fgoff2 = 0;
            else if (bp->fgoff2 > cut) bp->fgoff2 -= sz;
            if (bp->fgoff3 == cut) bp->fgoff3 = 0;
            else if (bp->fgoff3 > cut) bp->fgoff3 -= sz;
        }
    }
}

static int m_createGraphic(lua_State *LS) {
    int g = lua_gettop(LS) >= 1 ? (int) lua_tonumber(LS, 1) : numblockgfx;
    int bytes = gfx_bytes();
    if (blockgfxpt == NULL || bytes <= 0) { lua_pushnumber(LS, -1); return 1; }
    if (g < 0) g = 0;
    if (g > numblockgfx) g = numblockgfx;
    InsertBytes(blockgfxpt + g * bytes, blockgfxpt + BKGFXMEM, bytes);
    numblockgfx++;
    bump_gfx_offs(g * bytes, bytes);
    lua_pushnumber(LS, g);
    mapaltered = 1;
    return 1;
}

static int m_deleteGraphic(lua_State *LS) {
    int g = (int) lua_tonumber(LS, 1);
    int bytes = gfx_bytes();
    if (blockgfxpt == NULL || g < 0 || g >= numblockgfx || bytes <= 0) return 0;
    RemoveBytes(blockgfxpt + g * bytes, blockgfxpt + numblockgfx * bytes, bytes);
    numblockgfx--;
    bump_gfx_offs(g * bytes, -bytes);
    mapaltered = 1;
    return 0;
}

static int m_getPixel(lua_State *LS) {
    int x = (int) lua_tonumber(LS, 1);
    int y = (int) lua_tonumber(LS, 2);
    int g = (int) lua_tonumber(LS, 3);
    int bpp, off;
    unsigned char *p;
    if (blockgfxpt == NULL || x < 0 || y < 0 || x >= blockwidth || y >= blockheight ||
        g < 0 || g >= numblockgfx) {
        lua_pushnumber(LS, 0); return 1;
    }
    bpp = (blockdepth + 1) / 8;
    if (bpp < 1) bpp = 1;
    off = (g * blockheight * blockwidth + y * blockwidth + x) * bpp;
    p = (unsigned char *) blockgfxpt + off;
    if (blockdepth == 8) {
        lua_pushnumber(LS, p[0]);
        return 1;
    }
    if (blockdepth == 15 || blockdepth == 16) {
        int v = p[0] | (p[1] << 8);
        int r, g2, b;
        if (blockdepth == 15) {
            r = (v >> 10) & 31; g2 = (v >> 5) & 31; b = v & 31;
            r = r * 255 / 31; g2 = g2 * 255 / 31; b = b * 255 / 31;
        } else {
            r = (v >> 11) & 31; g2 = (v >> 5) & 63; b = v & 31;
            r = r * 255 / 31; g2 = g2 * 255 / 63; b = b * 255 / 31;
        }
        lua_pushnumber(LS, 255);
        lua_pushnumber(LS, r);
        lua_pushnumber(LS, g2);
        lua_pushnumber(LS, b);
        return 4;
    }
    if (blockdepth == 32) {
        lua_pushnumber(LS, p[0]);
        lua_pushnumber(LS, p[1]);
        lua_pushnumber(LS, p[2]);
        lua_pushnumber(LS, p[3]);
        return 4;
    }
    lua_pushnumber(LS, 255);
    lua_pushnumber(LS, p[2]);
    lua_pushnumber(LS, p[1]);
    lua_pushnumber(LS, p[0]);
    return 4;
}

static int m_setPixel(lua_State *LS) {
    int x = (int) lua_tonumber(LS, 1);
    int y = (int) lua_tonumber(LS, 2);
    int g = (int) lua_tonumber(LS, 3);
    int bpp, off, nargs;
    unsigned char *p;
    if (blockgfxpt == NULL || x < 0 || y < 0 || x >= blockwidth || y >= blockheight ||
        g < 0 || g >= numblockgfx) return 0;
    bpp = (blockdepth + 1) / 8;
    if (bpp < 1) bpp = 1;
    off = (g * blockheight * blockwidth + y * blockwidth + x) * bpp;
    p = (unsigned char *) blockgfxpt + off;
    nargs = lua_gettop(LS);
    if (blockdepth == 8) {
        p[0] = (unsigned char) lua_tonumber(LS, 4);
    } else if (nargs >= 7) {
        int a = (int) lua_tonumber(LS, 4);
        int r = (int) lua_tonumber(LS, 5);
        int g2 = (int) lua_tonumber(LS, 6);
        int b = (int) lua_tonumber(LS, 7);
        if (blockdepth == 32) {
            p[0] = (unsigned char) a; p[1] = (unsigned char) r;
            p[2] = (unsigned char) g2; p[3] = (unsigned char) b;
        } else if (blockdepth == 15) {
            int v = ((r >> 3) << 10) | ((g2 >> 3) << 5) | (b >> 3);
            p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
        } else if (blockdepth == 16) {
            int v = ((r >> 3) << 11) | ((g2 >> 2) << 5) | (b >> 3);
            p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
        } else {
            p[0] = (unsigned char) b; p[1] = (unsigned char) g2; p[2] = (unsigned char) r;
        }
    } else {
        int v = (int) lua_tonumber(LS, 4);
        if (blockdepth == 15 || blockdepth == 16) {
            p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
        } else if (blockdepth == 32) {
            p[0] = 255; p[1] = v & 0xFF; p[2] = (v >> 8) & 0xFF; p[3] = (v >> 16) & 0xFF;
        } else {
            p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF;
        }
    }
    mapaltered = 1;
    return 0;
}

static int m_createAnim(lua_State *LS) {
    int anum = lua_gettop(LS) >= 1 ? (int) lua_tonumber(LS, 1) : numblockanim;
    if (anum < 0) anum = 0;
    if (animstrpt == NULL) curanim = -1;
    else curanim = (short) (anum > 0 ? anum - 1 : -1);
    AnimNew();
    lua_pushnumber(LS, curanim);
    mapaltered = 1;
    return 1;
}

static int m_deleteAnim(lua_State *LS) {
    int id = (int) lua_tonumber(LS, 1);
    int saved = curanim;
    if (id >= 0 && id < numblockanim) {
        curanim = (short) id;
        AnimCut();
        curanim = saved;
        if (curanim >= numblockanim) curanim = (short) (numblockanim - 1);
        mapaltered = 1;
    }
    return 0;
}

static int m_getAnimFrame(lua_State *LS) {
    int anim = (int) lua_tonumber(LS, 1);
    int frame = get_const(LS, 2);
    ANISTR *ani = ani_ptr(anim);
    long *frames, off;
    if (ani == NULL) { lua_pushnumber(LS, 0); return 1; }
    if (frame == ANM_REFFRAME) off = ani->anstartoff - 4;
    else off = ani->anstartoff + frame * 4;
    frames = (long *) ((char *) animstrpt + ANDTMEM + off);
    lua_pushnumber(LS, (int) (*frames / BLOCKSTRSIZE));
    return 1;
}

static int m_setAnimFrame(lua_State *LS) {
    int anim = (int) lua_tonumber(LS, 1);
    int frame = get_const(LS, 2);
    int block = (int) lua_tonumber(LS, 3);
    ANISTR *ani = ani_ptr(anim);
    long *frames, off;
    if (ani == NULL) return 0;
    if (frame == ANM_REFFRAME) { SetRefFrame(anim, block); mapaltered = 1; return 0; }
    off = ani->anstartoff + frame * 4;
    frames = (long *) ((char *) animstrpt + ANDTMEM + off);
    *frames = block * BLOCKSTRSIZE;
    mapaltered = 1;
    return 0;
}

static int m_insertAnimFrame(lua_State *LS) {
    InsertAnimFrame((int) lua_tonumber(LS, 1), (int) lua_tonumber(LS, 2),
        (int) lua_tonumber(LS, 3));
    mapaltered = 1;
    return 0;
}

static int m_cutAnimFrame(lua_State *LS) {
    DelAnimFrame((int) lua_tonumber(LS, 1), (int) lua_tonumber(LS, 2));
    mapaltered = 1;
    return 0;
}

static int m_numAnimFrames(lua_State *LS) {
    ANISTR *ani = ani_ptr((int) lua_tonumber(LS, 1));
    if (ani == NULL) { lua_pushnumber(LS, 0); return 1; }
    lua_pushnumber(LS, (int) ((ani->anendoff - ani->anstartoff) / 4));
    return 1;
}

static int m_getAnimValue(lua_State *LS) {
    ANISTR *ani = ani_ptr((int) lua_tonumber(LS, 1));
    int field = get_const(LS, 2);
    if (ani == NULL) { lua_pushnumber(LS, 0); return 1; }
    switch (field) {
    case ANM_TYPE: lua_pushnumber(LS, ani->antype); break;
    case ANM_DELAY: lua_pushnumber(LS, ani->andelay); break;
    case ANM_COUNT: lua_pushnumber(LS, ani->ancount); break;
    case ANM_USER: lua_pushnumber(LS, ani->anuser); break;
    case ANM_CURFRAME:
        lua_pushnumber(LS, (int) ((ani->ancuroff - ani->anstartoff) / 4));
        break;
    default: lua_pushnumber(LS, 0); break;
    }
    return 1;
}

static int m_setAnimValue(lua_State *LS) {
    ANISTR *ani = ani_ptr((int) lua_tonumber(LS, 1));
    int field = get_const(LS, 2);
    int val = (int) lua_tonumber(LS, 3);
    if (ani == NULL) return 0;
    switch (field) {
    case ANM_TYPE: ani->antype = (signed char) val; break;
    case ANM_DELAY: ani->andelay = (signed char) val; break;
    case ANM_COUNT: ani->ancount = (signed char) val; break;
    case ANM_USER: ani->anuser = (signed char) val; break;
    case ANM_CURFRAME:
        ani->ancuroff = ani->anstartoff + val * 4;
        if (ani->ancuroff > ani->anendoff) ani->ancuroff = ani->anendoff;
        break;
    default: break;
    }
    mapaltered = 1;
    return 0;
}

static int m_createBrush(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    int x = (int) lua_tonumber(LS, 2);
    int y = (int) lua_tonumber(LS, 3);
    int w = (int) lua_tonumber(LS, 4);
    int h = (int) lua_tonumber(LS, 5);
    int saved = curbrsh, r;
    if (num < 0) num = 0;
    if (num >= MAXBRSH) num = MAXBRSH - 1;
    curbrsh = (short) (num + 1);
    r = MakeBrush(x, y, x + w - 1, y + h - 1);
    curbrsh = (short) saved;
    if (r == 0) numbrushes = (short) count_brushes();
    lua_pushnumber(LS, r == 0 ? 1 : 0);
    return 1;
}

static int m_deleteBrush(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    if (num >= 0 && num < MAXBRSH && brshpt[num]) {
        free(brshpt[num]); brshpt[num] = NULL;
        numbrushes = (short) count_brushes();
    }
    return 0;
}

static int m_renameBrush(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    const char *name = lua_tostring(LS, 2);
    if (num >= 0 && num < MAXBRSH && name) {
        strncpy(brshname[num], name, 63);
        brshname[num][63] = 0;
    }
    return 0;
}

static int m_getBrushBlock(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    int x = (int) lua_tonumber(LS, 2);
    int y = (int) lua_tonumber(LS, 3);
    short int *bp;
    int bw, bh;
    if (num < 0 || num >= MAXBRSH || brshpt[num] == NULL) { lua_pushnumber(LS, 0); return 1; }
    bp = brshpt[num]; bw = bp[0]; bh = bp[1];
    if (x < 0 || x >= bw || y < 0 || y >= bh) { lua_pushnumber(LS, 0); return 1; }
    lua_pushnumber(LS, bp[2 + y * bw + x]);
    return 1;
}

static int m_setBrushBlock(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    int x = (int) lua_tonumber(LS, 2);
    int y = (int) lua_tonumber(LS, 3);
    int blk = (int) lua_tonumber(LS, 4);
    short int *bp;
    int bw, bh;
    if (num < 0 || num >= MAXBRSH || brshpt[num] == NULL) return 0;
    bp = brshpt[num]; bw = bp[0]; bh = bp[1];
    if (x < 0 || x >= bw || y < 0 || y >= bh) return 0;
    bp[2 + y * bw + x] = (short int) blk;
    return 0;
}

static int m_getBrushValue(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    int field = get_const(LS, 2);
    short int *bp;
    if (num < 0 || num >= MAXBRSH || brshpt[num] == NULL) {
        if (field == LV_BRUSHNAME) lua_pushstring(LS, "");
        else lua_pushnumber(LS, 0);
        return 1;
    }
    bp = brshpt[num];
    if (field == LV_BRUSHWIDTH) lua_pushnumber(LS, bp[0]);
    else if (field == LV_BRUSHHEIGHT) lua_pushnumber(LS, bp[1]);
    else if (field == LV_BRUSHNAME) lua_pushstring(LS, brshname[num]);
    else lua_pushnumber(LS, 0);
    return 1;
}

static int m_createObject(lua_State *LS) {
    int pos = lua_gettop(LS) >= 1 ? (int) lua_tonumber(LS, 1) : -1;
    int id = (pos < 0) ? F14_NewObject() : F14_NewObjectAt(pos);
    lua_pushnumber(LS, id);
    mapaltered = 1;
    return 1;
}

static int m_deleteObject(lua_State *LS) {
    F14_DeleteObject((int) lua_tonumber(LS, 1));
    mapaltered = 1;
    return 0;
}

static int obj_field_get(const F14Obj *o, int field) {
    switch (field) {
    case LV_OBJXPOS: return o->xpos;
    case LV_OBJYPOS: return o->ypos;
    case LV_OBJGFXID: return o->gfxid;
    case LV_OBJTILEID: return o->tileid;
    case LV_OBJGXOFF: return o->gxoff;
    case LV_OBJGYOFF: return o->gyoff;
    case LV_OBJGWIDTH: return o->gwidth;
    case LV_OBJGHEIGHT: return o->gheight;
    case LV_OBJGHANDLEXOFF: return o->ghandlexoff;
    case LV_OBJGHANDLEYOFF: return o->ghandleyoff;
    case LV_OBJSHOW: return o->show;
    case LV_OBJUSER1: return o->user1;
    case LV_OBJUSER2: return o->user2;
    case LV_OBJUSER3: return o->user3;
    case LV_OBJUSER4: return o->user4;
    case LV_OBJUSER5: return o->user5;
    case LV_OBJUSER6: return o->user6;
    case LV_OBJUSER7: return o->user7;
    case LV_OBJFLAGS: return o->flags;
    default: return 0;
    }
}

static void obj_field_set(F14Obj *o, int field, int val) {
    switch (field) {
    case LV_OBJXPOS: o->xpos = val; break;
    case LV_OBJYPOS: o->ypos = val; break;
    case LV_OBJGFXID: o->gfxid = val; break;
    case LV_OBJTILEID: o->tileid = val; break;
    case LV_OBJGXOFF: o->gxoff = val; break;
    case LV_OBJGYOFF: o->gyoff = val; break;
    case LV_OBJGWIDTH: o->gwidth = val; break;
    case LV_OBJGHEIGHT: o->gheight = val; break;
    case LV_OBJGHANDLEXOFF: o->ghandlexoff = val; break;
    case LV_OBJGHANDLEYOFF: o->ghandleyoff = val; break;
    case LV_OBJSHOW: o->show = val; break;
    case LV_OBJUSER1: o->user1 = val; break;
    case LV_OBJUSER2: o->user2 = val; break;
    case LV_OBJUSER3: o->user3 = val; break;
    case LV_OBJUSER4: o->user4 = val; break;
    case LV_OBJUSER5: o->user5 = val; break;
    case LV_OBJUSER6: o->user6 = val; break;
    case LV_OBJUSER7: o->user7 = val; break;
    case LV_OBJFLAGS: o->flags = val; break;
    default: break;
    }
}

static int m_getObjectValue(lua_State *LS) {
    const F14Obj *o = F14_GetObject((int) lua_tonumber(LS, 1));
    if (o == NULL) { lua_pushnumber(LS, 0); return 1; }
    lua_pushnumber(LS, obj_field_get(o, get_const(LS, 2)));
    return 1;
}

static int m_setObjectValue(lua_State *LS) {
    int id = (int) lua_tonumber(LS, 1);
    const F14Obj *src = F14_GetObject(id);
    F14Obj o;
    if (src == NULL) return 0;
    o = *src;
    obj_field_set(&o, get_const(LS, 2), (int) lua_tonumber(LS, 3));
    F14_SetObject(id, &o);
    mapaltered = 1;
    return 0;
}

static int m_getObjectFilename(lua_State *LS) {
    const char *s = F14_ImageName((int) lua_tonumber(LS, 1));
    lua_pushstring(LS, s ? s : "");
    return 1;
}

static int m_setObjectFilename(lua_State *LS) {
    int idx = (int) lua_tonumber(LS, 1);
    const char *s = lua_tostring(LS, 2);
    if (s) F14_SetImageName(idx, s);
    mapaltered = 1;
    return 0;
}

static int m_getObjectSort(lua_State *LS) {
    int i = (int) lua_tonumber(LS, 1);
    if (i >= 0 && i < nobjectsort) lua_pushnumber(LS, objectsort[i]);
    else lua_pushnumber(LS, i);
    return 1;
}

static int m_setObjectSort(lua_State *LS) {
    int i = (int) lua_tonumber(LS, 1);
    int v = (int) lua_tonumber(LS, 2);
    if (i >= 0 && i < F14_MAXOBJ) {
        objectsort[i] = v;
        if (i + 1 > nobjectsort) nobjectsort = i + 1;
    }
    return 0;
}

static int m_getBlockSort(lua_State *LS) {
    int i = (int) lua_tonumber(LS, 1);
    if (i >= 0 && i < nblocksort) lua_pushnumber(LS, blocksort[i]);
    else lua_pushnumber(LS, i);
    return 1;
}

static int m_setBlockSort(lua_State *LS) {
    int i = (int) lua_tonumber(LS, 1);
    int v = (int) lua_tonumber(LS, 2);
    if (i >= 0 && i < MAXBLKSTR) {
        blocksort[i] = v;
        if (i + 1 > nblocksort) nblocksort = i + 1;
    }
    return 0;
}

static int m_createChunk(lua_State *LS) {
    const char *id = lua_tostring(LS, 1);
    int size = (int) lua_tonumber(LS, 2);
    int pos = lua_gettop(LS) >= 3 ? (int) lua_tonumber(LS, 3) : 0;
    char id4[4] = { 'C', 'H', 'N', 'K' };
    if (id) {
        int n = (int) strlen(id);
        if (n > 4) n = 4;
        memset(id4, 0, 4);
        memcpy(id4, id, n);
    }
    lua_pushnumber(LS, FmpChunkCreate(id4, size, pos));
    mapaltered = 1;
    return 1;
}

static int m_deleteChunk(lua_State *LS) {
    FmpChunkDelete((int) lua_tonumber(LS, 1));
    mapaltered = 1;
    return 0;
}

static int m_getChunkValue(lua_State *LS) {
    int idx = (int) lua_tonumber(LS, 1);
    int off = get_const(LS, 2);
    lua_pushnumber(LS, FmpChunkGetVal(idx, off));
    return 1;
}

static int m_setChunkValue(lua_State *LS) {
    FmpChunkSetVal((int) lua_tonumber(LS, 1), get_const(LS, 2),
        (int) lua_tonumber(LS, 3));
    mapaltered = 1;
    return 0;
}

static int m_createLayer(lua_State *LS) {
    int layer = lua_gettop(LS) >= 1 ? (int) lua_tonumber(LS, 1) : -1;
    int saved = maplayer, r;
    if (layer < 0) {
        int i;
        for (i = 0; i < MAXMAPLAYERS; i++) if (mapmappt[i] == NULL) { layer = i; break; }
        if (layer < 0) { lua_pushnumber(LS, -1); return 1; }
    }
    maplayer = (short) (layer > 0 ? layer - 1 : -1);
    if (maplayer < -1) maplayer = -1;
    r = CreateMapLayer();
    if (r < 0) maplayer = (short) saved;
    lua_pushnumber(LS, r);
    mapaltered = 1;
    return 1;
}

static int m_deleteLayer(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    int saved = maplayer;
    if (num >= 0 && num < MAXMAPLAYERS && mapmappt[num]) {
        maplayer = (short) num;
        DeleteMapLayer();
        if (saved == num) { /* current already adjusted */ }
        else if (saved > num) ChangeLayer(saved - 1);
        else ChangeLayer(saved);
        mapaltered = 1;
    }
    return 0;
}

static int m_copyLayer(lua_State *LS) {
    int src = (int) lua_tonumber(LS, 1);
    int dst = (int) lua_tonumber(LS, 2);
    short int *sp = NULL, *dp = NULL;
    size_t n = (size_t) mapwidth * (size_t) mapheight * 2;
    if (n == 0) return 0;
    if (src == -9999) {
        if (dst == -9999) return 0;
        if (dst < 0 || dst >= MAXMAPLAYERS) return 0;
        if (mapmappt[dst] == NULL) {
            mapmappt[dst] = malloc(n);
            if (mapmappt[dst] == NULL) return 0;
        }
        MECopyUndoToLayer(mapmappt[dst]);
        if (dst == maplayer) mappt = mapmappt[dst];
        return 0;
    }
    if (src < 0 || src >= MAXMAPLAYERS || mapmappt[src] == NULL) return 0;
    sp = mapmappt[src];
    if (dst == -9999) { MECopyLayerToUndo(sp); return 0; }
    if (dst < 0 || dst >= MAXMAPLAYERS) return 0;
    if (mapmappt[dst] == NULL) {
        mapmappt[dst] = malloc(n);
        if (mapmappt[dst] == NULL) return 0;
    }
    dp = mapmappt[dst];
    memcpy(dp, sp, n);
    mapaltered = 1;
    return 0;
}

static int m_renameLayer(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    const char *s = lua_tostring(LS, 2);
    if (s) F14_LayerNameSet(num, s);
    return 0;
}

static int m_getLayerName(lua_State *LS) {
    int num = (int) lua_tonumber(LS, 1);
    const char *s = F14_LayerName(num);
    char buf[32];
    if (s && s[0]) { lua_pushstring(LS, s); return 1; }
    sprintf(buf, "Layer %d", num);
    lua_pushstring(LS, buf);
    return 1;
}

static int m_newMap(lua_State *LS) {
    int w = (int) lua_tonumber(LS, 1);
    int h = (int) lua_tonumber(LS, 2);
    int d = (int) lua_tonumber(LS, 3);
    int tw = (int) lua_tonumber(LS, 4);
    int th = (int) lua_tonumber(LS, 5);
    int gx = lua_gettop(LS) >= 6 ? (int) lua_tonumber(LS, 6) : tw;
    int gy = lua_gettop(LS) >= 7 ? (int) lua_tonumber(LS, 7) : th;
    int sx = lua_gettop(LS) >= 8 ? (int) lua_tonumber(LS, 8) : 0;
    int sy = lua_gettop(LS) >= 9 ? (int) lua_tonumber(LS, 9) : 0;
    int mt = lua_gettop(LS) >= 10 ? (int) lua_tonumber(LS, 10) : 0;
    int r = CreateNewMap(w, h, d, tw, th, gx, gy, sx, sy, mt);
    MEDestroyUndoBuffs();
    Rejig();
    lua_pushnumber(LS, r);
    return 1;
}

static int m_closeMap(lua_State *LS) {
    (void) LS;
    FreeAll();
    MEDestroyUndoBuffs();
    Rejig();
    return 0;
}

static int m_openFMP(lua_State *LS) {
    const char *fn = lua_tostring(LS, 1);
    int r;
    if (fn && fn[0]) {
        strncpy(mapfilename, fn, 255);
        mapfilename[255] = 0;
    }
    r = 0;
    LoadProject();
    MEDestroyUndoBuffs();
    Rejig();
    lua_pushnumber(LS, r);
    return 1;
}

static int m_saveFMP(lua_State *LS) {
    const char *fn = lua_tostring(LS, 1);
    if (fn && fn[0]) {
        strncpy(mapfilename, fn, 255);
        mapfilename[255] = 0;
    }
    lua_pushnumber(LS, SaveProject());
    return 1;
}

static int m_importGraphics(lua_State *LS) {
    const char *name = lua_tostring(LS, 1);
    int start = lua_gettop(LS) >= 2 ? (int) lua_tonumber(LS, 2) : numblockgfx;
    int amt = lua_gettop(LS) >= 3 ? (int) lua_tonumber(LS, 3) : 0;
    int saved, r;
    if (name == NULL) { lua_pushnumber(LS, 0); return 1; }
    saved = numblockgfx;
    if (start < 0) start = 0;
    numblockgfx = (short) start;
    r = IMImportBlocks((char *) name, amt);
    if (numblockgfx < saved) numblockgfx = (short) saved;
    lua_pushnumber(LS, r == 0 ? 1 : 0);
    mapaltered = 1;
    return 1;
}

static int m_resizeMap(lua_State *LS) {
    int w = (int) lua_tonumber(LS, 1);
    int h = (int) lua_tonumber(LS, 2);
    int c = lua_gettop(LS) >= 3 ? (int) lua_tonumber(LS, 3) : 1;
    lua_pushnumber(LS, ResizeMap(w, h, c));
    Rejig();
    return 1;
}

static int m_msgBox(lua_State *LS) {
    const char *title = lua_tostring(LS, 1);
    const char *text = lua_tostring(LS, 2);
    int buttons = (int) lua_tonumber(LS, 3);
    int icon = (int) lua_tonumber(LS, 4);
    int r;
    if (!title) title = "Mappy";
    if (!text) text = "";
    if (buttons == 0) buttons = MB_OK;
    r = MessageBox(MapGetMainHWND(), text, title, (UINT) (buttons | icon));
    lua_pushnumber(LS, win_to_mmb(r));
    return 1;
}

static int m_fileRequester(lua_State *LS) {
    const char *path = lua_tostring(LS, 1);
    const char *desc = lua_tostring(LS, 2);
    const char *ext = lua_tostring(LS, 3);
    int type = (int) lua_tonumber(LS, 4);
    OPENFILENAME ofn;
    char filter[512], file[MAX_PATH], initdir[MAX_PATH];
    int n = 0;
    if (!desc) desc = "All files (*.*)";
    if (!ext) ext = "*.*";
    memset(filter, 0, sizeof(filter));
    strcpy(filter, desc); n = (int) strlen(filter) + 1;
    strcpy(filter + n, ext); n += (int) strlen(ext) + 1;
    filter[n] = 0;
    file[0] = 0;
    initdir[0] = 0;
    if (path && path[0] && strcmp(path, ".")) strncpy(initdir, path, MAX_PATH - 1);
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = MapGetMainHWND();
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initdir[0] ? initdir : NULL;
    ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    if (type == 2) {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
        if (GetSaveFileName(&ofn)) {
            lua_pushnumber(LS, 0);
            lua_pushstring(LS, file);
            return 2;
        }
    } else {
        ofn.Flags |= OFN_FILEMUSTEXIST;
        if (GetOpenFileName(&ofn)) {
            lua_pushnumber(LS, 0);
            lua_pushstring(LS, file);
            return 2;
        }
    }
    lua_pushnumber(LS, IDCANCEL);
    lua_pushstring(LS, "");
    return 2;
}

static INT_PTR CALLBACK LuaReqProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    (void) l;
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowText(h, req_title);
        SetDlgItemText(h, IDC_LUAMSG, req_msg);
        SetDlgItemText(h, IDC_LUAEDIT, req_edit);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(w) == IDOK) {
            GetDlgItemText(h, IDC_LUAEDIT, req_edit, (int) sizeof(req_edit));
            EndDialog(h, IDOK);
            return TRUE;
        }
        if (LOWORD(w) == IDCANCEL) {
            EndDialog(h, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static int push_split_strings(lua_State *LS, const char *s) {
    int n = 1;
    const char *p = s;
    char buf[2048];
    int i = 0;
    while (*p) {
        if (*p == ',') {
            buf[i] = 0;
            lua_pushstring(LS, buf);
            n++; i = 0; p++;
        } else {
            if (i < (int) sizeof(buf) - 1) buf[i++] = *p;
            p++;
        }
    }
    buf[i] = 0;
    lua_pushstring(LS, buf);
    return n;
}

static WORD *put_ustr(WORD *p, const char *s) {
    if (!s) s = "";
    while (*s) { *p++ = (WORD) (unsigned char) *s++; }
    *p++ = 0;
    return p;
}

static WORD *align_dw(WORD *p) {
    uintptr_t u = (uintptr_t) p;
    u = (u + 3) & ~(uintptr_t) 3;
    return (WORD *) u;
}

static INT_PTR CALLBACK LuaCustomDlgProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    dlg_hwnd = h; dlg_msg = msg; dlg_wp = w; dlg_lp = l;
    if (msg == WM_INITDIALOG) {
        dlg_msg = WM_COMMAND; dlg_wp = 99; dlg_lp = 0;
        if (dlgproc_path[0]) LuaRunScript(dlgproc_path);
        return TRUE;
    }
    if (msg == WM_COMMAND) {
        if (dlgproc_path[0]) LuaRunScript(dlgproc_path);
        if (LOWORD(w) == IDOK) { EndDialog(h, IDOK); return TRUE; }
        if (LOWORD(w) == IDCANCEL) { EndDialog(h, IDCANCEL); return TRUE; }
    }
    return FALSE;
}

static int run_custom_dialogue(lua_State *LS) {
    unsigned char buf[8192];
    DLGTEMPLATE *dt = (DLGTEMPLATE *) buf;
    WORD *p;
    int i, r;
    memset(buf, 0, sizeof(buf));
    dt->style = (luadlg.style ? (DWORD) luadlg.style : (DS_MODALFRAME | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU));
    dt->style |= DS_SETFONT;
    dt->dwExtendedStyle = 0;
    dt->cdit = (WORD) luadlg.nadded;
    dt->x = (short) luadlg.x; dt->y = (short) luadlg.y;
    dt->cx = (short) luadlg.w; dt->cy = (short) luadlg.h;
    p = (WORD *) (dt + 1);
    *p++ = 0; *p++ = 0;
    p = put_ustr(p, luadlg.title);
    *p++ = 8;
    p = put_ustr(p, "MS Sans Serif");
    for (i = 0; i < luadlg.nadded; i++) {
        DLGITEMTEMPLATE *it;
        p = align_dw(p);
        it = (DLGITEMTEMPLATE *) p;
        it->style = (DWORD) luadlg.items[i].style;
        if (it->style == 0) it->style = WS_CHILD | WS_VISIBLE;
        it->dwExtendedStyle = 0;
        it->x = (short) luadlg.items[i].x;
        it->y = (short) luadlg.items[i].y;
        it->cx = (short) luadlg.items[i].w;
        it->cy = (short) luadlg.items[i].h;
        it->id = (WORD) luadlg.items[i].idc;
        p = (WORD *) (it + 1);
        *p++ = 0xFFFF;
        *p++ = (WORD) luadlg.items[i].type;
        p = put_ustr(p, luadlg.items[i].title);
        *p++ = 0;
    }
    r = (int) DialogBoxIndirect(hInstance, dt, MapGetMainHWND(), AsDlgProc(LuaCustomDlgProc));
    lua_pushnumber(LS, win_to_mmb(r));
    return 1;
}

static int m_doDialogue(lua_State *LS) {
    int narg = lua_gettop(LS);
    int type, r, nret;
    if (narg == 1 && lua_isstring(LS, 1)) {
        const char *p = lua_tostring(LS, 1);
        strncpy(dlgproc_path, p ? p : "", MAX_PATH - 1);
        dlgproc_path[MAX_PATH - 1] = 0;
        return run_custom_dialogue(LS);
    }
    {
        const char *t = lua_tostring(LS, 1);
        const char *m = lua_tostring(LS, 2);
        const char *s = lua_tostring(LS, 3);
        strncpy(req_title, t ? t : "Mappy", sizeof(req_title) - 1);
        strncpy(req_msg, m ? m : "", sizeof(req_msg) - 1);
        strncpy(req_edit, s ? s : "", sizeof(req_edit) - 1);
        req_title[sizeof(req_title) - 1] = 0;
        req_msg[sizeof(req_msg) - 1] = 0;
        req_edit[sizeof(req_edit) - 1] = 0;
    }
    type = narg >= 4 ? (int) lua_tonumber(LS, 4) : 1;
    r = (int) DialogBox(hInstance, "D_LUAREQ", MapGetMainHWND(), AsDlgProc(LuaReqProc));
    lua_pushnumber(LS, win_to_mmb(r));
    if (r != IDOK) {
        lua_pushstring(LS, req_edit);
        return 2;
    }
    if (type == 2) {
        nret = push_split_strings(LS, req_edit);
        return 1 + nret;
    }
    lua_pushstring(LS, req_edit);
    return 2;
}

static int m_createDialogue(lua_State *LS) {
    const char *t = lua_tostring(LS, 1);
    memset(&luadlg, 0, sizeof(luadlg));
    if (t) strncpy(luadlg.title, t, sizeof(luadlg.title) - 1);
    luadlg.style = (int) lua_tonumber(LS, 2);
    luadlg.x = (int) lua_tonumber(LS, 3);
    luadlg.y = (int) lua_tonumber(LS, 4);
    luadlg.w = (int) lua_tonumber(LS, 5);
    luadlg.h = (int) lua_tonumber(LS, 6);
    luadlg.nitems = (int) lua_tonumber(LS, 7);
    luadlg.nadded = 0;
    lua_pushnumber(LS, 1);
    return 1;
}

static int m_addDialogueItem(lua_State *LS) {
    int i;
    const char *t;
    if (luadlg.nadded >= MAXDLGITEMS) return 0;
    i = luadlg.nadded++;
    t = lua_tostring(LS, 1);
    if (t) strncpy(luadlg.items[i].title, t, sizeof(luadlg.items[i].title) - 1);
    luadlg.items[i].style = (int) lua_tonumber(LS, 2);
    luadlg.items[i].x = (int) lua_tonumber(LS, 3);
    luadlg.items[i].y = (int) lua_tonumber(LS, 4);
    luadlg.items[i].w = (int) lua_tonumber(LS, 5);
    luadlg.items[i].h = (int) lua_tonumber(LS, 6);
    luadlg.items[i].idc = (int) lua_tonumber(LS, 7);
    luadlg.items[i].type = (int) lua_tonumber(LS, 8);
    return 0;
}

static int m_getDialogueParam(lua_State *LS) {
    lua_pushnumber(LS, (lua_Number) (uintptr_t) dlg_hwnd);
    lua_pushnumber(LS, dlg_msg);
    lua_pushnumber(LS, (lua_Number) dlg_wp);
    lua_pushnumber(LS, (lua_Number) dlg_lp);
    return 4;
}

static int m_checkDlgButton(lua_State *LS) {
    if (dlg_hwnd)
        CheckDlgButton(dlg_hwnd, (int) lua_tonumber(LS, 1), (UINT) lua_tonumber(LS, 2));
    return 0;
}

static int m_isDlgButtonChecked(lua_State *LS) {
    int r = 0;
    if (dlg_hwnd) r = (int) IsDlgButtonChecked(dlg_hwnd, (int) lua_tonumber(LS, 1));
    lua_pushnumber(LS, r);
    return 1;
}

static int m_getDlgItemText(lua_State *LS) {
    char buf[1024];
    buf[0] = 0;
    if (dlg_hwnd) GetDlgItemText(dlg_hwnd, (int) lua_tonumber(LS, 1), buf, 1024);
    lua_pushstring(LS, buf);
    return 1;
}

static int m_setDlgItemText(lua_State *LS) {
    const char *s = lua_tostring(LS, 2);
    if (dlg_hwnd && s) SetDlgItemText(dlg_hwnd, (int) lua_tonumber(LS, 1), s);
    return 0;
}

static int m_sendDlgItemMessage(lua_State *LS) {
    int idc = (int) lua_tonumber(LS, 1);
    UINT msg = (UINT) lua_tonumber(LS, 2);
    WPARAM wp = (WPARAM) (uintptr_t) lua_tonumber(LS, 3);
    LPARAM lp;
    LRESULT r = 0;
    if (lua_type(LS, 4) == LUA_TSTRING) lp = (LPARAM) lua_tostring(LS, 4);
    else lp = (LPARAM) (intptr_t) lua_tonumber(LS, 4);
    if (dlg_hwnd) r = SendDlgItemMessage(dlg_hwnd, idc, msg, wp, lp);
    lua_pushnumber(LS, (lua_Number) r);
    return 1;
}

static int m_updateScreen(lua_State *LS) {
    (void) LS;
    InvalidateAll();
    return 0;
}

static int m_moveWindow(lua_State *LS) {
    int which = (int) lua_tonumber(LS, 1);
    int x = (int) lua_tonumber(LS, 2);
    int y = (int) lua_tonumber(LS, 3);
    int w = (int) lua_tonumber(LS, 4);
    int h = (int) lua_tonumber(LS, 5);
    HWND hw = NULL;
    if (which == 0) hw = MapGetMainHWND();
    else if (which == 1) hw = (betype == 2 && GEGetHWND() != NULL) ? GEGetHWND() : MEGetHWND();
    else if (which == 2) hw = BEGetHWND();
    if (hw) MoveWindow(hw, x, y, w, h, TRUE);
    return 0;
}

static int m_renameMenuItem(lua_State *LS) {
    int code = (int) lua_tonumber(LS, 1);
    const char *name = lua_tostring(LS, 2);
    if (name) MapRenameMenuItem(code, name);
    return 0;
}

static int m_getTextString(lua_State *LS) {
    const char *s = F14_TstrGet((int) lua_tonumber(LS, 1));
    lua_pushstring(LS, s ? s : "");
    return 1;
}

static int m_setTextString(lua_State *LS) {
    int i = (int) lua_tonumber(LS, 1);
    const char *s = lua_tostring(LS, 2);
    if (s && !strcmp(s, "-del")) {
        if (i == F14_TstrCount() - 1) F14_TstrDelLast();
        return 0;
    }
    if (s) F14_TstrSet(i, s);
    mapaltered = 1;
    return 0;
}

static int m_shiftVal(lua_State *LS) {
    int a = (int) lua_tonumber(LS, 1);
    int b = (int) lua_tonumber(LS, 2);
    lua_pushnumber(LS, b >= 0 ? (a >> b) : (a << (-b)));
    return 1;
}

static int m_andVal(lua_State *LS) {
    lua_pushnumber(LS, (int) lua_tonumber(LS, 1) & (int) lua_tonumber(LS, 2));
    return 1;
}

static int m_orVal(lua_State *LS) {
    lua_pushnumber(LS, (int) lua_tonumber(LS, 1) | (int) lua_tonumber(LS, 2));
    return 1;
}

static int m_shellExecute(lua_State *LS) {
    const char *action = lua_tostring(LS, 1);
    const char *file = lua_tostring(LS, 2);
    const char *params = lua_tostring(LS, 3);
    const char *dir = lua_tostring(LS, 4);
    int style = lua_gettop(LS) >= 5 ? (int) lua_tonumber(LS, 5) : SW_SHOWNORMAL;
    if (style <= 0) style = SW_SHOWNORMAL;
    if (file) ShellExecute(MapGetMainHWND(), action, file, params, dir, style);
    return 0;
}

static int m_sendMessage(lua_State *LS) {
    int code = (int) lua_tonumber(LS, 1);
    SendMessage(MapGetMainHWND(), WM_COMMAND, (WPARAM) code, 0);
    return 0;
}

typedef int(*lua_CFunc_wrap) (lua_State *);
typedef struct { const char *name; lua_CFunc_wrap fn; } LuaReg;

static const LuaReg mappy_funcs[] = {
    { "getValue", m_getValue }, { "setValue", m_setValue },
    { "getBlock", m_getBlock }, { "setBlock", m_setBlock },
    { "getBlockValue", m_getBlockValue }, { "setBlockValue", m_setBlockValue },
    { "getPixel", m_getPixel }, { "setPixel", m_setPixel },
    { "createBlock", m_createBlock }, { "deleteBlock", m_deleteBlock },
    { "createGraphic", m_createGraphic }, { "deleteGraphic", m_deleteGraphic },
    { "getBlockSort", m_getBlockSort }, { "setBlockSort", m_setBlockSort },
    { "createAnim", m_createAnim }, { "deleteAnim", m_deleteAnim },
    { "getAnimFrame", m_getAnimFrame }, { "setAnimFrame", m_setAnimFrame },
    { "insertAnimFrame", m_insertAnimFrame }, { "cutAnimFrame", m_cutAnimFrame },
    { "numAnimFrames", m_numAnimFrames },
    { "getAnimValue", m_getAnimValue }, { "setAnimValue", m_setAnimValue },
    { "createBrush", m_createBrush }, { "deleteBrush", m_deleteBrush },
    { "renameBrush", m_renameBrush },
    { "getBrushBlock", m_getBrushBlock }, { "setBrushBlock", m_setBrushBlock },
    { "getBrushValue", m_getBrushValue },
    { "createObject", m_createObject }, { "deleteObject", m_deleteObject },
    { "getObjectValue", m_getObjectValue }, { "setObjectValue", m_setObjectValue },
    { "getObjectFilename", m_getObjectFilename },
    { "setObjectFilename", m_setObjectFilename },
    { "getObjectSort", m_getObjectSort }, { "setObjectSort", m_setObjectSort },
    { "createChunk", m_createChunk }, { "deleteChunk", m_deleteChunk },
    { "getChunkValue", m_getChunkValue }, { "setChunkValue", m_setChunkValue },
    { "createLayer", m_createLayer }, { "deleteLayer", m_deleteLayer },
    { "copyLayer", m_copyLayer },
    { "renameLayer", m_renameLayer }, { "getLayerName", m_getLayerName },
    { "newMap", m_newMap }, { "closeMap", m_closeMap },
    { "openFMP", m_openFMP }, { "saveFMP", m_saveFMP },
    { "importGraphics", m_importGraphics }, { "resizeMap", m_resizeMap },
    { "msgBox", m_msgBox }, { "fileRequester", m_fileRequester },
    { "doDialogue", m_doDialogue },
    { "createDialogue", m_createDialogue }, { "addDialogueItem", m_addDialogueItem },
    { "getDialogueParam", m_getDialogueParam },
    { "checkDlgButton", m_checkDlgButton },
    { "isDlgButtonChecked", m_isDlgButtonChecked },
    { "getDlgItemText", m_getDlgItemText }, { "setDlgItemText", m_setDlgItemText },
    { "sendDlgItemMessage", m_sendDlgItemMessage },
    { "updateScreen", m_updateScreen },
    { "moveWindow", m_moveWindow }, { "renameMenuItem", m_renameMenuItem },
    { "getTextString", m_getTextString }, { "setTextString", m_setTextString },
    { "shiftVal", m_shiftVal }, { "andVal", m_andVal }, { "orVal", m_orVal },
    { "shellExecute", m_shellExecute }, { "sendMessage", m_sendMessage },
    { NULL, NULL }
};

static void init_mapwinpath(void) {
    char *slash;
    mapwinpath[0] = 0;
    if (!GetModuleFileName(NULL, mapwinpath, MAX_PATH)) return;
    slash = strrchr(mapwinpath, '\\');
    if (!slash) slash = strrchr(mapwinpath, '/');
    if (slash) slash[1] = 0;
    else strcat(mapwinpath, "\\");
}

int LuaInit(void) {
    const LuaReg *r;
    init_mapwinpath();
    L = lua_open();
    if (L == NULL) return -1;
    luaopen_base(L);
    luaopen_table(L);
    luaopen_string(L);
    luaopen_math(L);
    luaopen_debug(L);
    luaopen_io(L);
    register_constants(L);
    lua_getglobal(L, "mappy");
    for (r = mappy_funcs; r->name; r++) {
        lua_pushstring(L, r->name);
        lua_pushcfunction(L, r->fn);
        lua_rawset(L, -3);
    }
    lua_pop(L, 1);
    lua_pushstring(L, mapwinpath);
    lua_setglobal(L, "mapwinpath");
    return 0;
}

void LuaShutdown(void) {
    if (L) { lua_close(L); L = NULL; }
}

static int run_file(const char *filename) {
    FILE *f;
    long len;
    char *buf;
    int r;
    if (L == NULL) return -1;
    f = fopen(filename, "rb");
    if (f == NULL) return -1;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t) len + 1);
    if (buf == NULL) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t) len, f) != (size_t) len) { free(buf); fclose(f); return -1; }
    buf[len] = 0;
    fclose(f);
    r = lua_dostring(L, buf);
    free(buf);
    if (r != 0) {
        const char *err = lua_tostring(L, -1);
        MessageBox(MapGetMainHWND(), err ? err : "Unknown error", "LUA script error", MB_OK | MB_ICONEXCLAMATION);
        lua_pop(L, 1);
        return -1;
    }
    return 0;
}

int LuaRunScript(const char *filename) {
    char full[MAX_PATH * 2];
    dbglog_line(filename);
    if (filename == NULL || filename[0] == 0) return -1;
    if (is_abs_path(filename)) return run_file(filename);
    if (mapwinpath[0]) {
        snprintf(full, sizeof(full), "%s%s", mapwinpath, filename);
        if (run_file(full) == 0) return 0;
    }
    return run_file(filename);
}

int LuaRunString(const char *code) {
    int r;
    if (L == NULL) return -1;
    r = lua_dostring(L, code);
    if (r != 0) {
        MessageBox(MapGetMainHWND(), lua_tostring(L, -1), "LUA script error", MB_OK | MB_ICONEXCLAMATION);
        lua_pop(L, 1);
        return -1;
    }
    return 0;
}

int LuaRunMouseButton(int button, int x, int y) {
    char path[MAX_PATH * 2];
    (void) x; (void) y;
    if (L == NULL) return -1;
    if (button == 5) {
        sprintf(path, "%sluascr\\Placer.lua", mapwinpath);
        return LuaRunScript(path);
    }
    if (button == 10) {
        sprintf(path, "%sluascr\\Area.lua", mapwinpath);
        return LuaRunScript(path);
    }
    if (button >= 400) {
        int idx = button - 399;
        if (idx >= 0 && idx < MAXLUAMENU && luascriptmenu[idx][0])
            return LuaRunScript(luascriptmenu[idx]);
        return -1;
    }
    return -1;
}

void LuaRunBlockProp(int editidx) {
    char path[MAX_PATH * 2];
    curedit = editidx;
    sprintf(path, "%sluascr\\BlockProp.lua", mapwinpath);
    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        LuaRunScript(path);
}

void LuaRunObjectProp(int editidx) {
    char path[MAX_PATH * 2];
    curedit = editidx;
    sprintf(path, "%sluascr\\ObjectProp.lua", mapwinpath);
    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        LuaRunScript(path);
}

int LuaGetStrBlkUser(void) {
    return strblkuser;
}

int LuaGetStrObjUser(void) {
    return strobjuser;
}

void LuaRefreshBlockSort(void) {
    int i;
    char path[MAX_PATH * 2];
    nblocksort = numblockstr;
    if (nblocksort > MAXBLKSTR) nblocksort = MAXBLKSTR;
    for (i = 0; i < nblocksort; i++) blocksort[i] = i;
    sprintf(path, "%sluascr\\BlockSort.lua", mapwinpath);
    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        LuaRunScript(path);
}

void LuaRefreshObjectSort(void) {
    int i, n;
    char path[MAX_PATH * 2];
    n = F14_NumObjects();
    nobjectsort = n;
    if (nobjectsort > F14_MAXOBJ) nobjectsort = F14_MAXOBJ;
    for (i = 0; i < nobjectsort; i++) objectsort[i] = i;
    sprintf(path, "%sluascr\\ObjectSort.lua", mapwinpath);
    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        LuaRunScript(path);
}

int LuaBlockSortCount(void) {
    return nblocksort;
}

int LuaBlockSortAt(int i) {
    if (i < 0 || i >= nblocksort) return -1;
    return blocksort[i];
}

int LuaObjectSortCount(void) {
    return nobjectsort;
}

int LuaObjectSortAt(int i) {
    if (i < 0 || i >= nobjectsort) return -1;
    return objectsort[i];
}

int LuaFileMenuRun(int cmd) {
    const char *fn = NULL;
    char path[MAX_PATH * 2];
    switch (cmd) {
    case CM_FILENEW: fn = "luafilenewmap.lua"; break;
    case CM_FILEOPEN: fn = "luafileopen.lua"; break;
    case CM_FILESAVE: fn = "luafilesave.lua"; break;
    case CM_FILESAVEAS: fn = "luafilesaveas.lua"; break;
    case CM_FILEIMPORT: fn = "luafileimport.lua"; break;
    case CM_FILEIMPORTAT: fn = "luafileimportat.lua"; break;
    case CM_FILEEXPORT: fn = "luafileexport.lua"; break;
    case CM_FILEEXTTXT: fn = "luafileexportastext.lua"; break;
    case CM_FILEEXIT: fn = "luafileexit.lua"; break;
    default: return 0;
    }
    sprintf(path, "%sluascr\\%s", mapwinpath, fn);
    if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) return 0;
    LuaRunScript(path);
    return 1;
}
