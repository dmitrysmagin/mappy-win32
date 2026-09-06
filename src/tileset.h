/* tileset.h - Tileset definitions (TSDF) and instances (TSIN).
 *
 * Phase 1: in-memory API + FMP load/save only. Same encode rules as fmp14:
 *  - no host struct dumps; multibyte fields go through fmp_put/fmp_get
 *  - chunk headers are always big-endian; bodies obey mapislsb
 *  - cell values use the same memory/disk scaling as BODY
 */
#ifndef TILESET_H
#define TILESET_H

#include <stdio.h>

#define TS_MAXDEFS 256
#define TS_MAXINST 4096
#define TS_MAXDIM 64
#define TS_NAMELEN 64

#define TS_TSDF_VERSION 1
#define TS_TSDF_HDR_SIZE 16
#define TS_TSIN_HDR_SIZE 16
#define TS_INST_DISK_SIZE 24

typedef struct {
	char name[TS_NAMELEN];
	int width, height;
	int handle_x, handle_y;
	short *tiles; /* width*height; same encoding as map cells */
} TilesetDef;

typedef struct {
	int tset;
	int layer;
	int tx, ty;
	int show;
	int flags;
} TilesetInst;

void TS_FreeAll(void);

int TS_DecodeTSDF(const unsigned char *chunk, int bodysize, int is_lsb,
	int maptype, int blockstrsize);
int TS_DecodeTSIN(const unsigned char *chunk, int bodysize, int is_lsb);

int TS_HasTSDF(void);
int TS_HasTSIN(void);

int TS_SaveTSDF(FILE *f, int is_lsb, int maptype, int blockstrsize);
int TS_SaveTSIN(FILE *f, int is_lsb);

int TS_NumDefs(void);
int TS_CurDef(void);
void TS_SetCurDef(int i);
const TilesetDef *TS_GetDef(int i);
int TS_NewDef(int width, int height);
int TS_SetDefMeta(int i, const char *name, int hx, int hy);
int TS_ResizeDef(int i, int width, int height);
short TS_GetCell(int i, int x, int y);
int TS_SetCell(int i, int x, int y, short v);
void TS_DeleteDef(int i);
int TS_DuplicateDef(int i);
int TS_GrabFromLayer(int i, const short *layer, int mapw, int maph,
	int x1, int y1, int x2, int y2);

int TS_NumInst(void);
int TS_CurInst(void);
void TS_SetCurInst(int i);
const TilesetInst *TS_GetInst(int i);
void TS_SetInst(int i, const TilesetInst *inst);
int TS_NewInst(void);
void TS_DeleteInst(int i);
int TS_PlaceNew(int def, int layer, int tx, int ty);
int TS_InstBounds(int i, int *ox, int *oy, int *w, int *h);
int TS_HitInst(int tx, int ty, int layer);
int TS_FindInstOfDef(int def, int layer);

/* Returns 0 if encode/decode of a built-in sample matches. */
int TS_SelfTest(void);

#endif /* TILESET_H */
