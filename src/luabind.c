/* luabind.c -- Lua 5.0.3 <-> MappyWin32 bridge
 *
 * Implements the mappy.* namespace exposed to Lua scripts.
 * 68 C functions registered into a Lua table.
 */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "mapwin.h"
#include "fmp.h"
#include "luabind.h"

/* ---- debug logging (minimal, for test harness) ---- */
static FILE *dbglog = NULL;
static void dbglog_init(void) {
	if (!dbglog) dbglog = fopen("lua_test.log", "a");
}

/* ---- globals from other modules ---- */
extern short int mapwidth, mapheight, blockwidth, blockheight, blockdepth;
extern short int numblockstr, numblockgfx, numblockanim, numbrushes;
extern short int curstr, curanim, curbrsh, maplayer;
extern short int xmapoffset, ymapoffset;
extern char * blockgfxpt;
extern char * blockstrpt;
extern void * animstrpt;
extern short int * mapmappt[MAXMAPLAYERS];
extern short int * mappt;
extern short int * brshpt[MAXBRSH];
extern char brshname[MAXBRSH][64];
extern char mapfilename[];
extern int mapaltered;
extern int objshow;
extern HWND hwnd;
#define hwndMain hwnd
extern HINSTANCE hInstance;

/* FMP14 object API */
extern int F14_NumObjects (void);
extern int F14_CurObject (void);
extern void F14_SetCurObject (int);
extern const void * F14_GetObject (int);
extern int F14_NewObject (void);
extern void F14_DeleteObject (int);

/* FMP14 chunk API (stub) */
static int F14_NumChunks_stub (void) { return 0; }
static const void * F14_GetChunk_stub (int i) { (void)i; return NULL; }
#define F14_NumChunks F14_NumChunks_stub
#define F14_GetChunk F14_GetChunk_stub

/* Text strings */
#define MAXTSTR 64
extern char tstr[MAXTSTR][256];
extern int numtstr;

/* ---- Lua state ---- */
static lua_State * L = NULL;

/* ---- constants table ---- */
typedef struct { const char * name; int val; } LuaConst;

static const LuaConst map_consts[] = {
	{ "MAPWIDTH", 0 }, { "MAPHEIGHT", 1 },
	{ "BLOCKWIDTH", 2 }, { "BLOCKHEIGHT", 3 }, { "BLOCKDEPTH", 4 },
	{ "NUMBLOCKSTR", 5 }, { "NUMGRAPHICS", 6 },
	{ "BLOCKGAPX", 7 }, { "BLOCKGAPY", 8 },
	{ "BLOCKSTAGGERX", 9 }, { "BLOCKSTAGGERY", 10 },
	{ "CURLAYER", 11 }, { "CURBLOCK", 12 }, { "CURANIM", 13 },
	{ "CURBRUSH", 14 }, { "MOUSEBLOCKX", 15 }, { "MOUSEBLOCKY", 16 },
	{ "GETBLOCKMODE", 17 }, { "MAPFILENAME", 18 }, { "MAPWINPATH", 19 },
	{ "MAPWINVERSION", 20 }, { "MAPTYPE", 21 },
	{ "NUMBRUSHES", 22 }, { "BRUSHWIDTH", 23 }, { "BRUSHNAME", 24 },
	{ "CUROBJECT", 25 }, { "NUMOBJECTS", 26 },
	{ "OBJXPOS", 27 }, { "OBJYPOS", 28 }, { "OBJGFXID", 29 },
	{ "OBJTILEID", 30 }, { "OBJGXOFF", 31 }, { "OBJGYOFF", 32 },
	{ "OBJGWIDTH", 33 }, { "OBJGHEIGHT", 34 },
	{ "OBJGHANDLEXOFF", 35 }, { "OBJGHANDLEYOFF", 36 },
	{ "OBJSHOW", 37 }, { "OBJUSER1", 38 }, { "OBJUSER2", 39 },
	{ "OBJUSER3", 40 }, { "OBJUSER4", 41 }, { "OBJUSER5", 42 },
	{ "OBJUSER6", 43 }, { "OBJUSER7", 44 }, { "OBJFLAGS", 45 },
	{ "NUMCHUNKS", 46 }, { "CURGFX", 47 },
	{ "STRBLKUSER", 48 }, { "MAPONION", 49 }, { "MAPDARK", 50 },
	{ "MOUSEBLOCKX2", 51 }, { "MOUSEBLOCKY2", 52 },
	{ "BLOCKLABELS", 53 }, { "OBJECTLABELS", 54 },
	{ "CUREDIT", 55 }, { "BLOCKSPERROW", 56 }, { "BLOCKS", 57 },
	{ "PALETTEARGB", 58 }, { "NUMANIMS", 59 },
	{ "NUMBLOCKANIM", 60 }, { "XMAPOFFSET", 61 }, { "YMAPOFFSET", 62 },
	{ "MAPDIVIDERS", 63 },
	{ NULL, 0 }
};

static const LuaConst block_consts[] = {
	{ "BLKBG", 0 }, { "BLKFG1", 1 }, { "BLKFG2", 2 }, { "BLKFG3", 3 },
	{ "BLKUSER1", 4 }, { "BLKUSER2", 5 }, { "BLKUSER3", 6 },
	{ "BLKUSER4", 7 }, { "BLKUSER5", 8 }, { "BLKUSER6", 9 },
	{ "BLKUSER7", 10 },
	{ "BLKFLAG1", 11 }, { "BLKFLAG2", 12 }, { "BLKFLAG3", 13 },
	{ "BLKFLAG4", 14 }, { "BLKFLAG5", 15 }, { "BLKFLAG6", 16 },
	{ "BLKFLAG7", 17 }, { "BLKFLAG8", 18 },
	{ NULL, 0 }
};

static const LuaConst msgbox_consts[] = {
	{ "MMB_OK", MB_OK }, { "MMB_OKCANCEL", MB_OKCANCEL },
	{ "MMB_YESNOCANCEL", MB_YESNOCANCEL },
	{ "MMB_ICONNONE", 0 }, { "MMB_ICONINFO", MB_ICONINFORMATION },
	{ "MMB_ICONQUESTION", MB_ICONQUESTION },
	{ "MMB_ICONEXCLAMATION", MB_ICONEXCLAMATION },
	{ "MMB_CANCEL", IDCANCEL }, { "MMB_YES", IDYES }, { "MMB_NO", IDNO },
	{ "MMB_OPEN", 1 }, { "MMB_SAVE", 2 },
	{ NULL, 0 }
};

static const LuaConst special_consts[] = {
	{ "MPY_UNDO", -9999 },
	{ "MPY_MAINWINDOW", 0 }, { "MPY_MAPWINDOW", 1 }, { "MPY_BLOCKWINDOW", 2 },
	{ NULL, 0 }
};

static void register_constants (lua_State * L)
{
	const LuaConst * c;
	lua_newtable (L);  /* mappy table */
	lua_setglobal (L, "mappy");

	lua_pushvalue (L, LUA_GLOBALSINDEX);
	lua_getglobal (L, "mappy");

	for (c = map_consts; c->name; c++) {
		lua_pushstring (L, c->name);
		lua_pushnumber (L, c->val);
		lua_rawset (L, -3);
	}
	for (c = block_consts; c->name; c++) {
		lua_pushstring (L, c->name);
		lua_pushnumber (L, c->val);
		lua_rawset (L, -3);
	}
	for (c = msgbox_consts; c->name; c++) {
		lua_pushstring (L, c->name);
		lua_pushnumber (L, c->val);
		lua_rawset (L, -3);
	}
	for (c = special_consts; c->name; c++) {
		lua_pushstring (L, c->name);
		lua_pushnumber (L, c->val);
		lua_rawset (L, -3);
	}
	lua_pop (L, 1);
}

/* ---- helper: get mappy table field ---- */
static int get_const (lua_State * L, int idx)
{
	int v = -1;
	if (lua_isnumber (L, idx)) return (int) lua_tonumber (L, idx);
	if (lua_isstring (L, idx)) {
		const char * s = lua_tostring (L, idx);
		const LuaConst * c;
		for (c = map_consts; c->name; c++)
			if (!strcmp (s, c->name)) { v = c->val; break; }
		if (v < 0) for (c = block_consts; c->name; c++)
			if (!strcmp (s, c->name)) { v = c->val; break; }
	}
	return v;
}

/* ---- Block structure access helpers ---- */
/* BLKSTR layout (32 bytes):
 * +0  bgoff (int32)    +4  fgoff (int32)
 * +8  fgoff2 (int32)   +12 fgoff3 (int32)
 * +16 user1-user4 (4x int16, packed as 2 int32)
 * +24 user5-user6 (2x int16) + user7 (int16) + flags bitfield (int16)
 * +28 tl,tr,bl,br (4x int8) + trigger (int16) + unused1,2,3 (3x int8)
 */
static int blk_field_offset (int field, int * is_long)
{
	*is_long = 1;
	switch (field) {
		case 0: return 0;   /* bgoff */
		case 1: return 4;   /* fgoff */
		case 2: return 8;   /* fgoff2 */
		case 3: return 12;  /* fgoff3 */
		case 4: *is_long = 0; return 16;  /* user1 (short) */
		case 5: *is_long = 0; return 18;  /* user2 (short) */
		case 6: *is_long = 0; return 20;  /* user3 (short) */
		case 7: *is_long = 0; return 22;  /* user4 (short) */
		case 8: *is_long = 0; return 24;  /* user5 (short) */
		case 9: *is_long = 0; return 26;  /* user6 (short) */
		case 10: *is_long = 0; return 28; /* user7 (short) -- at offset 28 in packed struct? Let's just use byte offset */
		case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18:
			*is_long = 0; return 30; /* flags byte */
		default: return -1;
	}
}

/* ---- mappy.getValue(constant) ---- */
static int m_getValue (lua_State * L)
{
	int c = get_const (L, 1);
	lua_pushnumber (L, 0);
	switch (c) {
		case 0: lua_pushnumber (L, mapwidth); break;
		case 1: lua_pushnumber (L, mapheight); break;
		case 2: lua_pushnumber (L, blockwidth); break;
		case 3: lua_pushnumber (L, blockheight); break;
		case 4: lua_pushnumber (L, blockdepth); break;
		case 5: lua_pushnumber (L, numblockstr); break;
		case 6: lua_pushnumber (L, numblockgfx); break;
		case 11: lua_pushnumber (L, maplayer); break;
		case 12: lua_pushnumber (L, curstr); break;
		case 13: lua_pushnumber (L, curanim); break;
		case 14: lua_pushnumber (L, curbrsh); break;
		case 21: lua_pushnumber (L, 0); break; /* maptype */
		case 22: lua_pushnumber (L, numbrushes); break;
		case 25: lua_pushnumber (L, F14_CurObject ()); break;
		case 26: lua_pushnumber (L, F14_NumObjects ()); break;
		case 37: lua_pushnumber (L, objshow); break;
		case 46: lua_pushnumber (L, F14_NumChunks ()); break;
		case 47: lua_pushnumber (L, curstr); break; /* CURGFX */
		case 59: lua_pushnumber (L, numblockanim); break;
		case 60: lua_pushnumber (L, numblockanim); break;
		case 61: lua_pushnumber (L, xmapoffset); break;
		case 62: lua_pushnumber (L, ymapoffset); break;
		default: lua_pushnumber (L, 0); break;
	}
	return 1;
}

/* ---- mappy.setValue(constant, value) ---- */
static int m_setValue (lua_State * L)
{
	int c = get_const (L, 1);
	int v = (int) lua_tonumber (L, 2);
	switch (c) {
		case 11: if (v >= 0 && v < MAXMAPLAYERS && mapmappt[v]) { maplayer = v; mappt = mapmappt[v]; } break;
		case 12: if (v >= 0 && v < numblockstr) curstr = v; break;
		case 13: if (v >= -1) curanim = v; break;
		case 14: if (v >= 0 && v <= MAXBRSH) curbrsh = v; break;
		case 37: objshow = v; break;
		case 47: if (v >= 0 && v < numblockgfx) curstr = v; break;
		default: break;
	}
	return 0;
}

/* ---- mappy.getBlock(x, y [,layer]) ---- */
static int m_getBlock (lua_State * L)
{
	int x = (int) lua_tonumber (L, 1);
	int y = (int) lua_tonumber (L, 2);
	int layer = lua_gettop (L) >= 3 ? (int) lua_tonumber (L, 3) : -1;
	short int * mp;
	if (x < 0 || x >= mapwidth || y < 0 || y >= mapheight) { lua_pushnumber (L, 0); return 1; }
	if (layer < 0) mp = mappt;
	else if (layer < MAXMAPLAYERS) mp = mapmappt[layer];
	else { lua_pushnumber (L, 0); return 1; }
	if (mp == NULL) { lua_pushnumber (L, 0); return 1; }
	lua_pushnumber (L, mp[x + y * mapwidth]);
	return 1;
}

/* ---- mappy.setBlock(x, y, block [,layer]) ---- */
static int m_setBlock (lua_State * L)
{
	int x = (int) lua_tonumber (L, 1);
	int y = (int) lua_tonumber (L, 2);
	int b = (int) lua_tonumber (L, 3);
	int layer = lua_gettop (L) >= 4 ? (int) lua_tonumber (L, 4) : -1;
	short int * mp;
	if (x < 0 || x >= mapwidth || y < 0 || y >= mapheight) return 0;
	if (layer < 0) mp = mappt;
	else if (layer < MAXMAPLAYERS) mp = mapmappt[layer];
	else return 0;
	if (mp == NULL) return 0;
	mp[x + y * mapwidth] = (short int) b;
	mapaltered = 1;
	return 0;
}

/* ---- mappy.getBlockValue(block, field) ---- */
static int m_getBlockValue (lua_State * L)
{
	int blk = (int) lua_tonumber (L, 1);
	int field = get_const (L, 2);
	int off, is_long;
	if (blk < 0 || blk >= numblockstr || blockstrpt == NULL) { lua_pushnumber (L, 0); return 1; }
	off = blk_field_offset (field, &is_long);
	if (off < 0) { lua_pushnumber (L, 0); return 1; }
	if (is_long) {
		lua_pushnumber (L, *(int *)(blockstrpt + blk * 32 + off));
	} else {
		lua_pushnumber (L, *(short int *)(blockstrpt + blk * 32 + off));
	}
	return 1;
}

/* ---- mappy.setBlockValue(block, field, value) ---- */
static int m_setBlockValue (lua_State * L)
{
	int blk = (int) lua_tonumber (L, 1);
	int field = get_const (L, 2);
	int val = (int) lua_tonumber (L, 3);
	int off, is_long;
	if (blk < 0 || blk >= numblockstr || blockstrpt == NULL) return 0;
	off = blk_field_offset (field, &is_long);
	if (off < 0) return 0;
	if (is_long) *(int *)(blockstrpt + blk * 32 + off) = val;
	else *(short int *)(blockstrpt + blk * 32 + off) = (short int) val;
	mapaltered = 1;
	return 0;
}

/* ---- mappy.createBlock(count) ---- */
static int m_createBlock (lua_State * L)
{
	int count = lua_gettop (L) >= 1 ? (int) lua_tonumber (L, 1) : 1;
	int start = numblockstr;
	if (count < 1) count = 1;
	{
		int i;
		for (i = 0; i < count; i++) {
			extern void NewBlock (void);
			NewBlock ();
		}
	}
	lua_pushnumber (L, start);
	return 1;
}

/* ---- mappy.deleteBlock(id) ---- */
static int m_deleteBlock (lua_State * L)
{
	int id = (int) lua_tonumber (L, 1);
	if (id >= 0 && id < numblockstr) {
		extern void CutBlock (void);
		extern short int curstr;
		int saved = curstr;
		curstr = id;
		CutBlock ();
		curstr = saved;
		mapaltered = 1;
	}
	return 0;
}

/* ---- mappy.createGraphic(count) ---- */
static int m_createGraphic (lua_State * L)
{
	int count = lua_gettop (L) >= 1 ? (int) lua_tonumber (L, 1) : 1;
	int start = numblockgfx;
	if (count < 1) count = 1;
	numblockgfx += count;
	lua_pushnumber (L, start);
	return 1;
}

/* ---- mappy.deleteGraphic(id) ---- */
static int m_deleteGraphic (lua_State * L)
{
	/* not trivially safe; stub */
	return 0;
}

/* ---- mappy.getPixel(x, y, graphic) ---- */
static int m_getPixel (lua_State * L)
{
	int x = (int) lua_tonumber (L, 1);
	int y = (int) lua_tonumber (L, 2);
	int g = (int) lua_tonumber (L, 3);
	int bpp, off;
	unsigned char * p;
	if (blockgfxpt == NULL || x < 0 || y < 0 || x >= blockwidth || y >= blockheight) {
		lua_pushnumber (L, 0); return 1;
	}
	bpp = (blockdepth + 1) / 8;
	off = (g * blockheight * blockwidth + y * blockwidth + x) * bpp;
	p = (unsigned char *) blockgfxpt + off;
	if (blockdepth == 8) lua_pushnumber (L, p[0]);
	else if (blockdepth == 15) lua_pushnumber (L, (p[1] << 8) | p[0]);
	else if (blockdepth == 16) lua_pushnumber (L, (p[1] << 8) | p[0]);
	else if (blockdepth == 32) lua_pushnumber (L, (p[3] << 16) | (p[2] << 8) | p[1]);
	else lua_pushnumber (L, (p[2] << 16) | (p[1] << 8) | p[0]);
	return 1;
}

/* ---- mappy.setPixel(x, y, graphic, ...) ---- */
static int m_setPixel (lua_State * L)
{
	int x = (int) lua_tonumber (L, 1);
	int y = (int) lua_tonumber (L, 2);
	int g = (int) lua_tonumber (L, 3);
	int bpp, off;
	unsigned char * p;
	if (blockgfxpt == NULL || x < 0 || y < 0 || x >= blockwidth || y >= blockheight) return 0;
	bpp = (blockdepth + 1) / 8;
	off = (g * blockheight * blockwidth + y * blockwidth + x) * bpp;
	p = (unsigned char *) blockgfxpt + off;
	if (blockdepth == 8) {
		p[0] = (unsigned char) lua_tonumber (L, 4);
	} else if (blockdepth == 15 || blockdepth == 16) {
		int v = (int) lua_tonumber (L, 4);
		p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
	} else if (blockdepth == 32) {
		int v = (int) lua_tonumber (L, 4);
		p[0] = 0; p[1] = v & 0xFF; p[2] = (v >> 8) & 0xFF; p[3] = (v >> 16) & 0xFF;
	} else {
		int r = (int) lua_tonumber (L, 4);
		int g2 = lua_gettop (L) >= 5 ? (int) lua_tonumber (L, 5) : 0;
		int b = lua_gettop (L) >= 6 ? (int) lua_tonumber (L, 6) : 0;
		p[0] = b; p[1] = g2; p[2] = r;
	}
	mapaltered = 1;
	return 0;
}

/* ---- mappy.getPixel(x, y, graphic) for 8-bit palette ---- */
static int m_getPixelRGB (lua_State * L)
{
	int x = (int) lua_tonumber (L, 1);
	int y = (int) lua_tonumber (L, 2);
	int g = (int) lua_tonumber (L, 3);
	int bpp, off, c;
	unsigned char * p;
	extern unsigned char * cmappt;
	if (blockgfxpt == NULL || x < 0 || y < 0 || x >= blockwidth || y >= blockheight) {
		lua_pushnumber (L, 0); lua_pushnumber (L, 0); lua_pushnumber (L, 0); return 3;
	}
	bpp = (blockdepth + 1) / 8;
	off = (g * blockheight * blockwidth + y * blockwidth + x) * bpp;
	p = (unsigned char *) blockgfxpt + off;
	if (blockdepth == 8 && cmappt) {
		c = p[0];
		lua_pushnumber (L, cmappt[c * 3]);
		lua_pushnumber (L, cmappt[c * 3 + 1]);
		lua_pushnumber (L, cmappt[c * 3 + 2]);
	} else {
		lua_pushnumber (L, 0); lua_pushnumber (L, 0); lua_pushnumber (L, 0);
	}
	return 3;
}

/* ---- animation functions ---- */
/* ANISTR layout (from mapfunc.c):
 * +0  antype (char)
 * +1  andelay (char)
 * +2  ancount (char)
 * +3  anuser (char)
 * +4  anstartoff (long)
 * +8  ancuroff (long)
 * +12 anendoff (long)
 * Total 16 bytes = FMP_ANI_MEMUNIT */
#define ANISTR_SIZE 16
#define ANDTMEM 65536

static int m_createAnim (lua_State * L)
{
	int count = lua_gettop (L) >= 1 ? (int) lua_tonumber (L, 1) : 1;
	extern void AnimNew (void);
	int i, start = numblockanim;
	for (i = 0; i < count; i++) AnimNew ();
	lua_pushnumber (L, start);
	return 1;
}

static int m_deleteAnim (lua_State * L)
{
	int id = (int) lua_tonumber (L, 1);
	extern void AnimCut (void);
	extern short int curanim;
	int saved = curanim;
	if (id >= 0 && id < numblockanim) {
		curanim = id;
		AnimCut ();
		curanim = saved;
		mapaltered = 1;
	}
	return 0;
}

static int m_getAnimFrame (lua_State * L)
{
	int anim = (int) lua_tonumber (L, 1);
	int frame = (int) lua_tonumber (L, 2);
	long int * frames;
	long startoff;
	if (animstrpt == NULL || anim < 0 || frame < 0) { lua_pushnumber (L, 0); return 1; }
	/* Get anstartoff from ANISTR at animstrpt + ANDTMEM - (anim+1)*ANISTR_SIZE */
	{
		char * ani = (char *) animstrpt + ANDTMEM - (anim + 1) * ANISTR_SIZE;
		startoff = *(long int *)(ani + 4);
	}
	frames = (long int *) ((char *) animstrpt + ANDTMEM + startoff);
	lua_pushnumber (L, (int) (frames[frame] / FMP_BLK_MEMUNIT));
	return 1;
}

static int m_setAnimFrame (lua_State * L)
{
	int anim = (int) lua_tonumber (L, 1);
	int frame = (int) lua_tonumber (L, 2);
	int block = (int) lua_tonumber (L, 3);
	long int * frames;
	long startoff;
	if (animstrpt == NULL || anim < 0 || frame < 0) return 0;
	{
		char * ani = (char *) animstrpt + ANDTMEM - (anim + 1) * ANISTR_SIZE;
		startoff = *(long int *)(ani + 4);
	}
	frames = (long int *) ((char *) animstrpt + ANDTMEM + startoff);
	frames[frame] = block * FMP_BLK_MEMUNIT;
	mapaltered = 1;
	return 0;
}

static int m_insertAnimFrame (lua_State * L)
{
	int anim = (int) lua_tonumber (L, 1);
	int frame = (int) lua_tonumber (L, 2);
	int block = (int) lua_tonumber (L, 3);
	extern void InsertAnimFrame (int, int, int);
	InsertAnimFrame (anim, frame, block);
	mapaltered = 1;
	return 0;
}

static int m_cutAnimFrame (lua_State * L)
{
	int anim = (int) lua_tonumber (L, 1);
	int frame = (int) lua_tonumber (L, 2);
	extern void DelAnimFrame (int, int);
	DelAnimFrame (anim, frame);
	mapaltered = 1;
	return 0;
}

static int m_numAnimFrames (lua_State * L)
{
	int anim = (int) lua_tonumber (L, 1);
	long startoff, endoff;
	if (animstrpt == NULL || anim < 0) { lua_pushnumber (L, 0); return 1; }
	{
		char * ani = (char *) animstrpt + ANDTMEM - (anim + 1) * ANISTR_SIZE;
		startoff = *(long int *)(ani + 4);
		endoff = *(long int *)(ani + 12);
	}
	lua_pushnumber (L, (int) ((endoff - startoff) / 4));
	return 1;
}

static int m_getAnimValue (lua_State * L)
{
	int anim = (int) lua_tonumber (L, 1);
	int field = get_const (L, 2);
	char * ani;
	if (animstrpt == NULL || anim < 0) { lua_pushnumber (L, 0); return 1; }
	ani = (char *) animstrpt + ANDTMEM - (anim + 1) * ANISTR_SIZE;
	if (field == 0) lua_pushnumber (L, ani[0]); /* ANMTYPE */
	else if (field == 1) lua_pushnumber (L, ani[1]); /* ANMDELAY */
	else lua_pushnumber (L, 0);
	return 1;
}

static int m_setAnimValue (lua_State * L)
{
	int anim = (int) lua_tonumber (L, 1);
	int field = get_const (L, 2);
	int val = (int) lua_tonumber (L, 3);
	char * ani;
	if (animstrpt == NULL || anim < 0) return 0;
	ani = (char *) animstrpt + ANDTMEM - (anim + 1) * ANISTR_SIZE;
	if (field == 0) ani[0] = (char) val;
	else if (field == 1) ani[1] = (char) val;
	mapaltered = 1;
	return 0;
}

/* ---- brush functions ---- */
static int m_createBrush (lua_State * L)
{
	int num = (int) lua_tonumber (L, 1);
	int x = (int) lua_tonumber (L, 2);
	int y = (int) lua_tonumber (L, 3);
	int w = (int) lua_tonumber (L, 4);
	int h = (int) lua_tonumber (L, 5);
	extern int MakeBrush (int, int, int, int);
	extern short int curbrsh;
	int saved = curbrsh, r;
	curbrsh = (short int) num;
	r = MakeBrush (x, y, x + w - 1, y + h - 1);
	curbrsh = saved;
	lua_pushnumber (L, r == 0 ? 1 : 0);
	return 1;
}

static int m_deleteBrush (lua_State * L)
{
	int num = (int) lua_tonumber (L, 1);
	if (num >= 1 && num <= MAXBRSH && brshpt[num - 1]) {
		free (brshpt[num - 1]); brshpt[num - 1] = NULL;
	}
	return 0;
}

static int m_renameBrush (lua_State * L)
{
	int num = (int) lua_tonumber (L, 1);
	const char * name = lua_tostring (L, 2);
	if (num >= 1 && num <= MAXBRSH && name) {
		strncpy (brshname[num - 1], name, 63);
		brshname[num - 1][63] = 0;
	}
	return 0;
}

static int m_getBrushBlock (lua_State * L)
{
	int num = (int) lua_tonumber (L, 1);
	int x = (int) lua_tonumber (L, 2);
	int y = (int) lua_tonumber (L, 3);
	short int * bp;
	if (num < 1 || num > MAXBRSH || brshpt[num - 1] == NULL) { lua_pushnumber (L, 0); return 1; }
	bp = brshpt[num - 1];
	{
		int bw = bp[0], bh = bp[1];
		if (x < 0 || x >= bw || y < 0 || y >= bh) { lua_pushnumber (L, 0); return 1; }
		lua_pushnumber (L, bp[2 + y * bw + x]);
	}
	return 1;
}

static int m_setBrushBlock (lua_State * L)
{
	int num = (int) lua_tonumber (L, 1);
	int x = (int) lua_tonumber (L, 2);
	int y = (int) lua_tonumber (L, 3);
	int blk = (int) lua_tonumber (L, 4);
	short int * bp;
	if (num < 1 || num > MAXBRSH || brshpt[num - 1] == NULL) return 0;
	bp = brshpt[num - 1];
	{
		int bw = bp[0], bh = bp[1];
		if (x < 0 || x >= bw || y < 0 || y >= bh) return 0;
		bp[2 + y * bw + x] = (short int) blk;
	}
	return 0;
}

static int m_getBrushValue (lua_State * L)
{
	int num = (int) lua_tonumber (L, 1);
	int field = get_const (L, 2);
	short int * bp;
	if (num < 1 || num > MAXBRSH || brshpt[num - 1] == NULL) { lua_pushnumber (L, 0); return 1; }
	bp = brshpt[num - 1];
	if (field == 23) lua_pushnumber (L, bp[0]); /* BRUSHWIDTH */
	else if (field == 24) lua_pushstring (L, brshname[num - 1]); /* BRUSHNAME */
	else lua_pushnumber (L, 0);
	return 1;
}

/* ---- object functions ---- */
static int m_createObject (lua_State * L)
{
	int id = F14_NewObject ();
	lua_pushnumber (L, id);
	return 1;
}

static int m_deleteObject (lua_State * L)
{
	int id = (int) lua_tonumber (L, 1);
	F14_DeleteObject (id);
	mapaltered = 1;
	return 0;
}

static int m_getObjectValue (lua_State * L)
{
	int obj = (int) lua_tonumber (L, 1);
	int field = get_const (L, 2);
	const int * o = (const int *) F14_GetObject (obj);
	if (o == NULL) { lua_pushnumber (L, 0); return 1; }
	/* F14Obj layout: xpos,ypos,tileid,gfxid,gxoff,gyoff,gwidth,gheight,
	 * ghandlexoff,ghandleyoff,show,user1..user7,flags = 16 int32 */
	if (field >= 27 && field <= 45) lua_pushnumber (L, o[field - 27]);
	else lua_pushnumber (L, 0);
	return 1;
}

static int m_setObjectValue (lua_State * L)
{
	int obj = (int) lua_tonumber (L, 1);
	int field = get_const (L, 2);
	int val = (int) lua_tonumber (L, 3);
	int * o = (int *) F14_GetObject (obj);
	if (o == NULL) return 0;
	if (field >= 27 && field <= 45) o[field - 27] = val;
	mapaltered = 1;
	return 0;
}

static int m_getObjectFilename (lua_State * L)
{
	lua_pushstring (L, "");
	return 1;
}

static int m_setObjectFilename (lua_State * L)
{
	return 0;
}

static int m_getObjectSort (lua_State * L)
{
	lua_pushnumber (L, 0);
	return 1;
}

static int m_setObjectSort (lua_State * L)
{
	return 0;
}

/* ---- chunk functions ---- */
static int m_createChunk (lua_State * L)
{
	lua_pushnumber (L, -1);
	return 1;
}

static int m_deleteChunk (lua_State * L)
{
	return 0;
}

static int m_getChunkValue (lua_State * L)
{
	lua_pushnumber (L, 0);
	return 1;
}

static int m_setChunkValue (lua_State * L)
{
	return 0;
}

/* ---- layer functions ---- */
extern int CreateMapLayer (void);

static int m_createLayer (lua_State * L)
{
	int id = CreateMapLayer ();
	lua_pushnumber (L, id);
	return 1;
}

static int m_deleteLayer (lua_State * L)
{
	int num = (int) lua_tonumber (L, 1);
	if (num >= 0 && num < MAXMAPLAYERS && mapmappt[num]) {
		free (mapmappt[num]); mapmappt[num] = NULL;
		if (maplayer == num) { maplayer = 0; mappt = mapmappt[0]; }
		mapaltered = 1;
	}
	return 0;
}

static int m_copyLayer (lua_State * L)
{
	int src = (int) lua_tonumber (L, 1);
	int dst = (int) lua_tonumber (L, 2);
	if (src < 0 || src >= MAXMAPLAYERS || dst < 0 || dst >= MAXMAPLAYERS) return 0;
	if (mapmappt[src] == NULL) return 0;
	if (mapmappt[dst] == NULL) {
		mapmappt[dst] = malloc (mapwidth * mapheight * 2);
		if (mapmappt[dst] == NULL) return 0;
	}
	memcpy (mapmappt[dst], mapmappt[src], mapwidth * mapheight * 2);
	mapaltered = 1;
	return 0;
}

static int m_renameLayer (lua_State * L)
{
	/* stub: layer names not yet stored */
	return 0;
}

static int m_getLayerName (lua_State * L)
{
	int num = (int) lua_tonumber (L, 1);
	char buf[32];
	if (num >= 0 && num < 10) sprintf (buf, "Layer %d", num);
	else if (num >= 10 && num < 100) sprintf (buf, "LY%d", num);
	else sprintf (buf, "L%d", num);
	lua_pushstring (L, buf);
	return 1;
}

/* ---- map file operations ---- */
static int m_newMap (lua_State * L)
{
	/* stub */
	return 0;
}

static int m_closeMap (lua_State * L)
{
	/* CloseProject doesn't exist in this build; use newMap(0,0,...) approach */
	return 0;
}

static int m_openFMP (lua_State * L)
{
	const char * fn = lua_tostring (L, 1);
	if (fn) {
		extern void LoadProject (void);
		strcpy (mapfilename, fn);
		LoadProject ();
	}
	return 0;
}

static int m_saveFMP (lua_State * L)
{
	const char * fn = lua_tostring (L, 1);
	if (fn) strcpy (mapfilename, fn);
	{
		extern int SaveProject (void);
		lua_pushnumber (L, SaveProject ());
	}
	return 1;
}

static int m_importGraphics (lua_State * L)
{
	return 0;
}

static int m_resizeMap (lua_State * L)
{
	int w = (int) lua_tonumber (L, 1);
	int h = (int) lua_tonumber (L, 2);
	int c = lua_gettop (L) >= 3 ? (int) lua_tonumber (L, 3) : 1;
	extern int ResizeMap (int, int, int);
	lua_pushnumber (L, ResizeMap (w, h, c));
	return 1;
}

/* ---- UI/Dialogue functions ---- */
static int m_msgBox (lua_State * L)
{
	const char * title = lua_tostring (L, 1);
	const char * text = lua_tostring (L, 2);
	int buttons = (int) lua_tonumber (L, 3);
	int icon = (int) lua_tonumber (L, 4);
	int r;
	if (!title) title = "Mappy";
	if (!text) text = "";
	r = MessageBox (hwndMain, text, title, buttons | icon);
	lua_pushnumber (L, r);
	return 1;
}

static int m_fileRequester (lua_State * L)
{
	/* stub */
	lua_pushstring (L, "");
	return 1;
}

static int m_doDialogue (lua_State * L)
{
	/* stub: simple input dialog */
	lua_pushstring (L, "");
	return 1;
}

static int m_createDialogue (lua_State * L)
{
	lua_pushnumber (L, 0);
	return 1;
}

static int m_addDialogueItem (lua_State * L)
{
	return 0;
}

static int m_doDialogueProc (lua_State * L)
{
	lua_pushnumber (L, 0);
	return 1;
}

static int m_getDialogueParam (lua_State * L)
{
	lua_pushnumber (L, 0); lua_pushnumber (L, 0);
	lua_pushnumber (L, 0); lua_pushnumber (L, 0);
	return 4;
}

static int m_checkDlgButton (lua_State * L)
{
	return 0;
}

static int m_isDlgButtonChecked (lua_State * L)
{
	lua_pushnumber (L, 0);
	return 1;
}

static int m_getDlgItemText (lua_State * L)
{
	lua_pushstring (L, "");
	return 1;
}

static int m_setDlgItemText (lua_State * L)
{
	return 0;
}

static int m_sendDlgItemMessage (lua_State * L)
{
	lua_pushnumber (L, 0);
	return 1;
}

static int m_showDlgWindow (lua_State * L)
{
	return 0;
}

static int m_updateScreen (lua_State * L)
{
	extern void InvalidateAll (void);
	InvalidateAll ();
	return 0;
}

static int m_moveWindow (lua_State * L)
{
	return 0;
}

static int m_renameMenuItem (lua_State * L)
{
	return 0;
}

/* Text strings - stub (tstr not exposed in this build) */

static int m_getTextString (lua_State * L)
{
	lua_pushstring (L, "");
	return 1;
}

static int m_setTextString (lua_State * L)
{
	return 0;
}

/* ---- utility ---- */
static int m_shiftVal (lua_State * L)
{
	int a = (int) lua_tonumber (L, 1);
	int b = (int) lua_tonumber (L, 2);
	lua_pushnumber (L, b >= 0 ? (a << b) : (a >> (-b)));
	return 1;
}

static int m_andVal (lua_State * L)
{
	lua_pushnumber (L, (int) lua_tonumber (L, 1) & (int) lua_tonumber (L, 2));
	return 1;
}

static int m_orVal (lua_State * L)
{
	lua_pushnumber (L, (int) lua_tonumber (L, 1) | (int) lua_tonumber (L, 2));
	return 1;
}

static int m_shellExecute (lua_State * L)
{
	const char * cmd = lua_tostring (L, 1);
	if (cmd) ShellExecute (NULL, "open", cmd, NULL, NULL, SW_SHOWNORMAL);
	return 0;
}

static int m_sendMessage (lua_State * L)
{
	HWND hw = (HWND)(long long) lua_tonumber (L, 1);
	int msg = (int) lua_tonumber (L, 2);
	int wp = (int) lua_tonumber (L, 3);
	if (hw) SendMessage (hw, msg, wp, 0);
	return 0;
}

/* ---- palette ---- */
static int m_paletteARGB (lua_State * L)
{
	int idx = (int) lua_tonumber (L, 1);
	extern unsigned char * cmappt;
	if (cmappt && idx >= 0 && idx < 256) {
		lua_pushnumber (L, cmappt[idx * 3 + 2]); /* R */
		lua_pushnumber (L, cmappt[idx * 3 + 1]); /* G */
		lua_pushnumber (L, cmappt[idx * 3]);     /* B */
		lua_pushnumber (L, 255);                 /* A */
		return 4;
	}
	lua_pushnumber (L, 0); lua_pushnumber (L, 0);
	lua_pushnumber (L, 0); lua_pushnumber (L, 0);
	return 4;
}

/* ---- registration table ---- */
typedef int (*lua_CFunc_wrap)(lua_State *);

typedef struct { const char * name; lua_CFunc_wrap fn; } LuaReg;

static const LuaReg mappy_funcs[] = {
	{ "getValue", m_getValue },
	{ "setValue", m_setValue },
	{ "getBlock", m_getBlock },
	{ "setBlock", m_setBlock },
	{ "getBlockValue", m_getBlockValue },
	{ "setBlockValue", m_setBlockValue },
	{ "getPixel", m_getPixel },
	{ "setPixel", m_setPixel },
	{ "getPixelRGB", m_getPixelRGB },
	{ "createBlock", m_createBlock },
	{ "deleteBlock", m_deleteBlock },
	{ "createGraphic", m_createGraphic },
	{ "deleteGraphic", m_deleteGraphic },
	{ "createAnim", m_createAnim },
	{ "deleteAnim", m_deleteAnim },
	{ "getAnimFrame", m_getAnimFrame },
	{ "setAnimFrame", m_setAnimFrame },
	{ "insertAnimFrame", m_insertAnimFrame },
	{ "cutAnimFrame", m_cutAnimFrame },
	{ "numAnimFrames", m_numAnimFrames },
	{ "getAnimValue", m_getAnimValue },
	{ "setAnimValue", m_setAnimValue },
	{ "createBrush", m_createBrush },
	{ "deleteBrush", m_deleteBrush },
	{ "renameBrush", m_renameBrush },
	{ "getBrushBlock", m_getBrushBlock },
	{ "setBrushBlock", m_setBrushBlock },
	{ "getBrushValue", m_getBrushValue },
	{ "createObject", m_createObject },
	{ "deleteObject", m_deleteObject },
	{ "getObjectValue", m_getObjectValue },
	{ "setObjectValue", m_setObjectValue },
	{ "getObjectFilename", m_getObjectFilename },
	{ "setObjectFilename", m_setObjectFilename },
	{ "getObjectSort", m_getObjectSort },
	{ "setObjectSort", m_setObjectSort },
	{ "createChunk", m_createChunk },
	{ "deleteChunk", m_deleteChunk },
	{ "getChunkValue", m_getChunkValue },
	{ "setChunkValue", m_setChunkValue },
	{ "createLayer", m_createLayer },
	{ "deleteLayer", m_deleteLayer },
	{ "copyLayer", m_copyLayer },
	{ "renameLayer", m_renameLayer },
	{ "getLayerName", m_getLayerName },
	{ "newMap", m_newMap },
	{ "closeMap", m_closeMap },
	{ "openFMP", m_openFMP },
	{ "saveFMP", m_saveFMP },
	{ "importGraphics", m_importGraphics },
	{ "resizeMap", m_resizeMap },
	{ "msgBox", m_msgBox },
	{ "fileRequester", m_fileRequester },
	{ "doDialogue", m_doDialogue },
	{ "createDialogue", m_createDialogue },
	{ "addDialogueItem", m_addDialogueItem },
	{ "doDialogueProc", m_doDialogueProc },
	{ "getDialogueParam", m_getDialogueParam },
	{ "checkDlgButton", m_checkDlgButton },
	{ "isDlgButtonChecked", m_isDlgButtonChecked },
	{ "getDlgItemText", m_getDlgItemText },
	{ "setDlgItemText", m_setDlgItemText },
	{ "sendDlgItemMessage", m_sendDlgItemMessage },
	{ "showDlgWindow", m_showDlgWindow },
	{ "updateScreen", m_updateScreen },
	{ "moveWindow", m_moveWindow },
	{ "renameMenuItem", m_renameMenuItem },
	{ "getTextString", m_getTextString },
	{ "setTextString", m_setTextString },
	{ "shiftVal", m_shiftVal },
	{ "andVal", m_andVal },
	{ "orVal", m_orVal },
	{ "shellExecute", m_shellExecute },
	{ "sendMessage", m_sendMessage },
	{ "paletteARGB", m_paletteARGB },
	{ NULL, NULL }
};

/* ---- public API ---- */

char luascriptmenu[MAXLUAMENU][260];
int  numluascript = 0;

int LuaInit (void)
{
	L = lua_open ();
	if (L == NULL) return -1;
	luaopen_base (L);
	luaopen_table (L);
	luaopen_string (L);
	luaopen_math (L);
	luaopen_debug (L);
	luaopen_io (L);
	register_constants (L);
	{
		const LuaReg * r;
		lua_getglobal (L, "mappy");
		for (r = mappy_funcs; r->name; r++) {
			lua_pushstring (L, r->name);
			lua_pushcfunction (L, r->fn);
			lua_rawset (L, -3);
		}
		lua_pop (L, 1);
	}
	return 0;
}

void LuaShutdown (void)
{
	if (L) { lua_close (L); L = NULL; }
}

int LuaRunScript (const char * filename)
{
	FILE * f;
	long len;
	char * buf;
	int r;
	dbglog_init();
	if (dbglog) fprintf(dbglog, "LuaRunScript: %s\n", filename);
	if (dbglog) fflush(dbglog);
	if (L == NULL) return -1;
	f = fopen (filename, "rb");
	if (f == NULL) return -1;
	fseek (f, 0, SEEK_END);
	len = ftell (f);
	fseek (f, 0, SEEK_SET);
	buf = malloc (len + 1);
	if (buf == NULL) { fclose (f); return -1; }
	if (fread (buf, 1, len, f) != (size_t) len) { free (buf); fclose (f); return -1; }
	buf[len] = 0;
	fclose (f);
	r = lua_dostring (L, buf);
	free (buf);
	if (r != 0) {
		const char *err = lua_tostring (L, -1);
		MessageBox (hwndMain, err ? err : "Unknown error", "LUA script error", MB_OK | MB_ICONEXCLAMATION);
		lua_pop (L, 1);
		return -1;
	}
	return 0;
}

int LuaRunString (const char * code)
{
	int r;
	if (L == NULL) return -1;
	r = lua_dostring (L, code);
	if (r != 0) {
		MessageBox (hwndMain, lua_tostring (L, -1), "LUA script error", MB_OK | MB_ICONEXCLAMATION);
		lua_pop (L, 1);
		return -1;
	}
	return 0;
}

int LuaRunMouseButton (int button, int x, int y)
{
	char buf[256];
	if (L == NULL) return -1;
	/* mouse buttons 5/10 map to Placer.lua/Area.lua; 400+ are arbitrary */
	if (button == 5) {
		sprintf (buf, "dofile(mapwinpath..'/luascr/Placer.lua')");
	} else if (button == 10) {
		sprintf (buf, "dofile(mapwinpath..'/luascr/Area.lua')");
	} else if (button >= 400) {
		sprintf (buf, "dofile(mapwinpath..'/luascr/script%d.lua')", button - 400);
	} else return -1;
	return LuaRunString (buf);
}
