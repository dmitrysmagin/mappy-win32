/* fmp14.h - Mappy 1.4-era FMP chunk support (EPHD/TSTR/LSTR/OBDT/OBFN).
 *
 * Reverse-engineered from mapwin1423/mapwin.exe (IDA mapwin.map/mapwin.lst)
 * and DOCS/readobj.c, DOCS/MAPWIN.HTM. See fmp14.c for format notes.
 *
 * Design rules (same as fmp.h):
 *  - No host struct is ever dumped to disk; all multibyte fields go through
 *    fmp_put/fmp_get with an explicit is_lsb flag (chunk bodies obey
 *    mapislsb, chunk headers are always big-endian).
 *  - Unknown tail bytes are preserved so load->save round-trips byte-exact.
 *  - All state lives in fmp14.c; mapfunc.c only calls F14_* hooks.
 */
#ifndef FMP14_H
#define FMP14_H

#include <stdio.h>

#define F14_MAXLAYERNAMES 100
#define F14_MAXNAMELEN 64
#define F14_MAXTEXTSTR 256
#define F14_MAXOBJ 4096
#define F14_MAXIMAGES 4096
#define F14_MAXIMGNAMELEN 260

/* Object structure (DOCS/mapdefs.h, DOCS/readobj.c): 20 x int32 on disk. */
typedef struct {
	int xpos, ypos;
	int gfxid, tileid;
	int gxoff, gyoff;
	int gwidth, gheight;
	int ghandlexoff, ghandleyoff;
	int show;
	int user1, user2, user3, user4, user5, user6, user7;
	int flags;
} F14Obj;

/* Lifecycle / hooks for mapfunc.c */
void F14_FreeAll(void);              /* reset all 1.4 state (call from FreeAll) */

/* Decode: body points at chunk start (ID at [0..3], BE size at [4..7]).
 *bodysize is the BE chunk-body size. Returns 0 on success, -1 on error
 * (sets merror via F14_SetErrorCb or returns -1 for caller to handle). */
int F14_DecodeEPHD(const unsigned char *chunk, int bodysize, int is_lsb);
int F14_DecodeTSTR(const unsigned char *chunk, int bodysize);
int F14_DecodeLSTR(const unsigned char *chunk, int bodysize, int is_lsb);
int F14_DecodeOBDT(const unsigned char *chunk, int bodysize, int is_lsb);
int F14_DecodeOBFN(const unsigned char *chunk, int bodysize);

/* Presence queries for SaveProject */
int F14_HasEPHD(void);
int F14_HasTSTR(void);
int F14_HasLSTR(void);
int F14_HasOBDT(void);
int F14_HasOBFN(void);

/* Encode: write ID + BE size + body. Returns 0 ok, -1 on I/O error. */
int F14_SaveEPHD(FILE *f, int is_lsb);
int F14_SaveTSTR(FILE *f);
int F14_SaveLSTR(FILE *f, int is_lsb);
int F14_SaveOBDT(FILE *f, int is_lsb);
int F14_SaveOBFN(FILE *f);

/* Accessors (for future UI / Lua use; index-checked, -1/NULL on error) */
int F14_EphdGet(int field);          /* 0..15, see F14_EPHD_* below */
void F14_EphdSet(int field, int v);
int F14_EphdSize(void);              /* stored body size (32 or 40) */

/* EPHD field ids (offsets below are body-relative byte offsets) */
#define F14_EPHD_S0  0   /* +0  short */
#define F14_EPHD_S1  1   /* +2  short */
#define F14_EPHD_S2  2   /* +4  short */
#define F14_EPHD_S3  3   /* +6  short */
#define F14_EPHD_S4  4   /* +8  short */
#define F14_EPHD_S5  5   /* +10 short */
#define F14_EPHD_S6  6   /* +12 short */
#define F14_EPHD_L0  7   /* +16 long  */
#define F14_EPHD_S7  8   /* +20 short (present if size > 0x1F) */
#define F14_EPHD_S8  9   /* +22 short (present if size > 0x1F) */
#define F14_EPHD_L1  10  /* +24 long  (present if size > 0x1F) */
#define F14_EPHD_S9  11  /* +28 short (present if size > 0x1F) */
#define F14_EPHD_S10 12  /* +34 short (present if size > 0x23; note order) */
#define F14_EPHD_S11 13  /* +32 short (present if size > 0x23) */
#define F14_EPHD_S12 14  /* +36 short (present if size > 0x27) */
#define F14_EPHD_S13 15  /* +38 short (present if size > 0x27) */
#define F14_EPHD_NFIELDS 16

int F14_TstrCount(void);             /* always 4 when present */
const char *F14_TstrGet(int i);
void F14_TstrSet(int i, const char *s);

int F14_LstrCount(void);
const char *F14_LayerName(int i);
void F14_LayerNameSet(int i, const char *s);  /* grows list with "" fill */

int F14_NumObjects(void);
int F14_CurObject(void);
void F14_SetCurObject(int i);
const F14Obj *F14_GetObject(int i);
void F14_SetObject(int i, const F14Obj *o);
int F14_NewObject(void);             /* append zeroed, returns index or -1 */
void F14_DeleteObject(int i);

int F14_NumImages(void);
const char *F14_ImageName(int i);
int F14_AddImage(const char *fname);
void F14_DeleteImage(int i);

/* NOTE: NLYR (100-layer container) and layers 9..99 are intentionally NOT
 * decoded here: no sample map contains NLYR and its container layout cannot
 * be verified. NLYR keeps flowing through the opaque unknown-chunk path,
 * so such maps still round-trip without data loss. */

#endif /* FMP14_H */
