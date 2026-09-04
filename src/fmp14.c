/* fmp14.c - Mappy 1.4-era FMP chunk support (EPHD/TSTR/LSTR/OBDT/OBFN).
 *
 * Format notes (all verified against mapwin1423/mapwin.exe + sample maps):
 *
 * EPHD "extra prefs header" (mapwin.lst sub_4254B0 @004254B0):
 *  body-relative offsets, multibyte fields obey mapislsb:
 *   +0,+2,+4,+6,+8,+10,+12 : 7 x short  (always present)
 *   +16                   : 1 x long   (always present)
 *   if (bodysize > 0x1F): +20,+22 short, +24 long, +28 short
 *   if (bodysize > 0x23): +34 short, then +32 short (yes, that order)
 *   if (bodysize > 0x27): +36 short, then +38 short
 *  Observed sizes: 32 (0x20) and 40 (0x28). vhistory.html says EPHD holds
 *  showuser/onion/curlayer/dividers state; the binary gives no field names,
 *  so fields are exposed by index (F14_EPHD_*). Raw bytes are preserved so
 *  unparsed tail/pad bytes round-trip exactly.
 *
 * TSTR (sub_4256B0 @004256B0): 4 NUL-terminated strings back-to-back,
 *  bounded by the chunk size (authorname/authorinfo1-3 style slots).
 *
 * LSTR (sub_425780 @00425780): layer names:
 *   +0 : u32 offset to strings (8 in all samples)
 *   +4 : u32 name count (clamped to 0xFFF by the original)
 *   +8,+12 : u32 spare (overlap string data in small chunks; preserved)
 *   strings start at body+offset, count NUL-terminated names.
 *  Sample: `08 00 00 00 01 00 00 00 "Layer 0" 00` (lala.fmp, smooth.fmp).
 *  TSTR doc (MAPWIN.HTM "TextStrings and Labels"): "the first int is the
 *  offset to the strings" - same convention.
 *
 * OBDT/OBFN (DOCS/readobj.c DecodeOBDTChunk/DecodeOBFNChunk):
 *  OBFN body: NUL-terminated filenames back-to-back.
 *  OBDT body: +0 long = byte offset from OBDT body start to OBJSTR array
 *   (>=16 required), +4 long = numobjstr, +8 long = curobj,
 *   +12 long = obstrsz (sizeof one disk entry, 80), then numobjstr x
 *   20 x long (xpos,ypos,gfxid,tileid,gxoff,gyoff,gwidth,gheight,
 *   ghandlexoff,ghandleyoff,show,user1-7,flags).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "fmp.h"
#include "fmp14.h"

/* ---------------- EPHD ---------------- */

#define EPHD_MAXBODY 64

static struct {
	int present;
	int bodysize;                 /* 32 or 40 as loaded */
	unsigned char raw[EPHD_MAXBODY];
	int s[14];                    /* S0..S6,S7,S8,S9,S10,S11,S12,S13 */
	int32_t l[2];                 /* L0,L1 */
} ephd;

/* body-relative layout table: {offset, is_long, field_index} */
static const struct { int off; int islong; int idx; int thresh; } ephd_layout[] = {
	{ 0,  0, 0,  0x00 }, { 2,  0, 1,  0x00 }, { 4,  0, 2,  0x00 },
	{ 6,  0, 3,  0x00 }, { 8,  0, 4,  0x00 }, { 10, 0, 5,  0x00 },
	{ 12, 0, 6,  0x00 }, { 16, 1, 0,  0x00 },
	{ 20, 0, 7,  0x1F }, { 22, 0, 8,  0x1F }, { 24, 1, 1,  0x1F },
	{ 28, 0, 9,  0x1F },
	{ 34, 0, 10, 0x23 }, { 32, 0, 11, 0x23 },
	{ 36, 0, 12, 0x27 }, { 38, 0, 13, 0x27 }
};

static void ephd_parse(int is_lsb)
{
	size_t k;
	for (k = 0; k < sizeof(ephd_layout)/sizeof(ephd_layout[0]); k++) {
		int off = ephd_layout[k].off;
		if (ephd.bodysize <= ephd_layout[k].thresh) continue;
		if (off + (ephd_layout[k].islong ? 4 : 2) > ephd.bodysize) continue;
		if (ephd_layout[k].islong)
			ephd.l[ephd_layout[k].idx] = fmp_get_i32(ephd.raw + off, is_lsb);
		else
			ephd.s[ephd_layout[k].idx] = fmp_get_i16(ephd.raw + off, is_lsb);
	}
}

int F14_DecodeEPHD(const unsigned char *chunk, int bodysize, int is_lsb)
{
	if (bodysize < 0 || bodysize > EPHD_MAXBODY) return -1;
	memset(&ephd, 0, sizeof(ephd));
	ephd.present = 1;
	ephd.bodysize = bodysize;
	memcpy(ephd.raw, chunk + 8, (size_t)bodysize);
	ephd_parse(is_lsb);
	return 0;
}

int F14_HasEPHD(void) { return ephd.present; }
int F14_EphdSize(void) { return ephd.present ? ephd.bodysize : 0; }

int F14_EphdGet(int field)
{
	if (!ephd.present || field < 0 || field >= F14_EPHD_NFIELDS) return 0;
	if (field == F14_EPHD_L0) return (int)ephd.l[0];
	if (field == F14_EPHD_L1) return (int)ephd.l[1];
	/* S-fields map: 0..6 -> s[0..6], 8..9 -> s[7..8], 11 -> s[9],
	 * 12 -> s[10], 13 -> s[11], 14 -> s[12], 15 -> s[13] */
	{
		static const int smap[16] = { 0,1,2,3,4,5,6,-1, 7,8,-1, 9, 10,11,12,13 };
		int si = smap[field];
		if (si < 0) return 0;
		return ephd.s[si];
	}
}

void F14_EphdSet(int field, int v)
{
	if (!ephd.present || field < 0 || field >= F14_EPHD_NFIELDS) return;
	if (field == F14_EPHD_L0) { ephd.l[0] = (int32_t)v; return; }
	if (field == F14_EPHD_L1) { ephd.l[1] = (int32_t)v; return; }
	{
		static const int smap[16] = { 0,1,2,3,4,5,6,-1, 7,8,-1, 9, 10,11,12,13 };
		int si = smap[field];
		if (si < 0) return;
		ephd.s[si] = v;
	}
}

int F14_SaveEPHD(FILE *f, int is_lsb)
{
	unsigned char buf[EPHD_MAXBODY];
	size_t k;
	if (!ephd.present) return 0;
	if (fmp_write_chunk_header(f, 'E', 'P', 'H', 'D', (uint32_t)ephd.bodysize) != 0)
		return -1;
	memcpy(buf, ephd.raw, (size_t)ephd.bodysize);
	/* patch parsed slots back over the raw image */
	for (k = 0; k < sizeof(ephd_layout)/sizeof(ephd_layout[0]); k++) {
		int off = ephd_layout[k].off;
		if (ephd.bodysize <= ephd_layout[k].thresh) continue;
		if (off + (ephd_layout[k].islong ? 4 : 2) > ephd.bodysize) continue;
		if (ephd_layout[k].islong)
			fmp_put_i32(buf + off, ephd.l[ephd_layout[k].idx], is_lsb);
		else
			fmp_put_i16(buf + off, ephd.s[ephd_layout[k].idx], is_lsb);
	}
	if (fwrite(buf, 1, (size_t)ephd.bodysize, f) != (size_t)ephd.bodysize)
		return -1;
	return 0;
}

/* ---------------- TSTR (4 fixed strings) ---------------- */

static struct {
	int present;
	char s[4][F14_MAXTEXTSTR];
} tstr;

int F14_DecodeTSTR(const unsigned char *chunk, int bodysize)
{
	int i, pos = 8; /* chunk+8 = body start */
	if (bodysize < 4) return -1;
	memset(&tstr, 0, sizeof(tstr));
	tstr.present = 1;
	for (i = 0; i < 4; i++) {
		int n = 0;
		while (pos < 8 + bodysize && chunk[pos] != 0 && n < F14_MAXTEXTSTR - 1)
			tstr.s[i][n++] = (char)chunk[pos++];
		tstr.s[i][n] = 0;
		if (pos < 8 + bodysize) pos++; /* skip NUL */
		else break;
	}
	return 0;
}

int F14_HasTSTR(void)
{
	int i;
	if (!tstr.present) return 0;
	for (i = 0; i < 4; i++)
		if (tstr.s[i][0] != 0) return 1;
	return 0;
}

int F14_SaveTSTR(FILE *f)
{
	int i;
	size_t total = 0;
	if (!F14_HasTSTR()) return 0;
	for (i = 0; i < 4; i++) total += strlen(tstr.s[i]) + 1;
	if (fmp_write_chunk_header(f, 'T', 'S', 'T', 'R', (uint32_t)total) != 0)
		return -1;
	for (i = 0; i < 4; i++)
		if (fwrite(tstr.s[i], 1, strlen(tstr.s[i]) + 1, f) != strlen(tstr.s[i]) + 1)
			return -1;
	return 0;
}

int F14_TstrCount(void) { return tstr.present ? 4 : 0; }
const char *F14_TstrGet(int i)
{
	if (!tstr.present || i < 0 || i > 3) return NULL;
	return tstr.s[i];
}
void F14_TstrSet(int i, const char *s)
{
	if (i < 0 || i > 3 || s == NULL) return;
	tstr.present = 1;
	strncpy(tstr.s[i], s, F14_MAXTEXTSTR - 1);
	tstr.s[i][F14_MAXTEXTSTR - 1] = 0;
}

/* ---------------- LSTR (layer names) ---------------- */

static struct {
	int present;
	int spare1, spare2;   /* body longs +8/+12 (overlap string data); preserved */
	char names[F14_MAXLAYERNAMES][F14_MAXNAMELEN];
	int count;
} lstr;

int F14_DecodeLSTR(const unsigned char *chunk, int bodysize, int is_lsb)
{
	int32_t off, cnt;
	int i, pos;
	if (bodysize < 8) return -1;
	memset(&lstr, 0, sizeof(lstr));
	lstr.present = 1;
	off = fmp_get_i32(chunk + 8, is_lsb);
	cnt = fmp_get_i32(chunk + 12, is_lsb);
	if (cnt < 0) cnt = 0;
	if (cnt > F14_MAXLAYERNAMES) cnt = F14_MAXLAYERNAMES;
	lstr.count = (int)cnt;
	if (bodysize >= 16) {
		lstr.spare1 = (int)fmp_get_i32(chunk + 16, is_lsb);
		lstr.spare2 = (int)fmp_get_i32(chunk + 20, is_lsb);
	}
	pos = 8 + (int)off;
	for (i = 0; i < lstr.count; i++) {
		int n = 0;
		while (pos < 8 + bodysize && chunk[pos] != 0 && n < F14_MAXNAMELEN - 1)
			lstr.names[i][n++] = (char)chunk[pos++];
		lstr.names[i][n] = 0;
		if (pos < 8 + bodysize) pos++;
		else break;
	}
	return 0;
}

int F14_HasLSTR(void) { return lstr.present && lstr.count > 0; }

int F14_SaveLSTR(FILE *f, int is_lsb)
{
	unsigned char hdr[8];
	int i;
	size_t total = 0;
	if (!F14_HasLSTR()) return 0;
	for (i = 0; i < lstr.count; i++) total += strlen(lstr.names[i]) + 1;
	if (fmp_write_chunk_header(f, 'L', 'S', 'T', 'R', (uint32_t)(8 + total)) != 0)
		return -1;
	fmp_put_i32(hdr + 0, 8, is_lsb);
	fmp_put_i32(hdr + 4, (int32_t)lstr.count, is_lsb);
	if (fwrite(hdr, 1, 8, f) != 8) return -1;
	for (i = 0; i < lstr.count; i++)
		if (fwrite(lstr.names[i], 1, strlen(lstr.names[i]) + 1, f) != strlen(lstr.names[i]) + 1)
			return -1;
	return 0;
}

int F14_LstrCount(void) { return lstr.present ? lstr.count : 0; }
const char *F14_LayerName(int i)
{
	if (!lstr.present || i < 0 || i >= lstr.count) return NULL;
	return lstr.names[i];
}
void F14_LayerNameSet(int i, const char *s)
{
	int k;
	if (i < 0 || i >= F14_MAXLAYERNAMES || s == NULL) return;
	lstr.present = 1;
	if (i >= lstr.count) {
		for (k = lstr.count; k <= i; k++) lstr.names[k][0] = 0;
		lstr.count = i + 1;
	}
	strncpy(lstr.names[i], s, F14_MAXNAMELEN - 1);
	lstr.names[i][F14_MAXNAMELEN - 1] = 0;
}

/* ---------------- OBDT / OBFN (Pro objects, DOCS/readobj.c) ---------------- */

#define OBSTR_DISK_LONGS 20
#define OBSTR_DISK_SIZE (OBSTR_DISK_LONGS * 4)

static struct {
	int present;
	F14Obj objs[F14_MAXOBJ];
	int numobj;
	int curobj;
	int obstrsz;              /* disk entry size, 80 */
	int have_obdt;
	char images[F14_MAXIMAGES][F14_MAXIMGNAMELEN];
	int numimg;
} obs;

int F14_DecodeOBFN(const unsigned char *chunk, int bodysize)
{
	int pos = 8, n = 0;
	memset(obs.images, 0, sizeof(obs.images));
	obs.numimg = 0;
	while (pos < 8 + bodysize - 1 && n < F14_MAXIMAGES) {
		int start = pos, len = 0;
		while (pos < 8 + bodysize && chunk[pos] != 0) { pos++; len++; }
		if (len >= F14_MAXIMGNAMELEN) len = F14_MAXIMGNAMELEN - 1;
		memcpy(obs.images[n], chunk + start, (size_t)len);
		obs.images[n][len] = 0;
		n++;
		if (pos < 8 + bodysize) pos++;
	}
	obs.numimg = n;
	return 0;
}

int F14_DecodeOBDT(const unsigned char *chunk, int bodysize, int is_lsb)
{
	int32_t arr_off, num, cur, esz;
	int i;
	const unsigned char *p;
	if (bodysize < 16) return -1;
	arr_off = fmp_get_i32(chunk + 8, is_lsb);
	num = fmp_get_i32(chunk + 12, is_lsb);
	cur = fmp_get_i32(chunk + 16, is_lsb);
	esz = fmp_get_i32(chunk + 20, is_lsb);
	if (arr_off < 16) return -1;
	if (num < 0) num = 0;
	if (num > F14_MAXOBJ) return -1;
	if (esz < OBSTR_DISK_SIZE) esz = OBSTR_DISK_SIZE;
	memset(obs.objs, 0, sizeof(obs.objs));
	obs.numobj = (int)num;
	obs.curobj = (int)cur;
	obs.obstrsz = (int)esz;
	obs.have_obdt = 1;
	p = chunk + 8 + arr_off;
	for (i = 0; i < obs.numobj; i++) {
		const unsigned char *e;
		int *dst;
		int k;
		if (p + OBSTR_DISK_SIZE > chunk + 8 + bodysize) return -1;
		e = p;
		dst = (int *)&obs.objs[i];
		for (k = 0; k < OBSTR_DISK_LONGS; k++)
			dst[k] = (int)fmp_get_i32(e + k * 4, is_lsb);
		p += esz;
	}
	return 0;
}

int F14_HasOBDT(void) { return obs.have_obdt && obs.numobj > 0; }
int F14_HasOBFN(void) { return obs.numimg > 0; }

int F14_SaveOBFN(FILE *f)
{
	int i;
	size_t total = 0;
	if (!F14_HasOBFN()) return 0;
	for (i = 0; i < obs.numimg; i++) total += strlen(obs.images[i]) + 1;
	if (fmp_write_chunk_header(f, 'O', 'B', 'F', 'N', (uint32_t)total) != 0)
		return -1;
	for (i = 0; i < obs.numimg; i++)
		if (fwrite(obs.images[i], 1, strlen(obs.images[i]) + 1, f) != strlen(obs.images[i]) + 1)
			return -1;
	return 0;
}

int F14_SaveOBDT(FILE *f, int is_lsb)
{
	unsigned char hdr[16];
	unsigned char ent[OBSTR_DISK_SIZE];
	int i, k;
	int32_t arr_off = 16;
	if (!F14_HasOBDT()) return 0;
	if (fmp_write_chunk_header(f, 'O', 'B', 'D', 'T',
			(uint32_t)(16 + obs.numobj * OBSTR_DISK_SIZE)) != 0)
		return -1;
	fmp_put_i32(hdr + 0, arr_off, is_lsb);
	fmp_put_i32(hdr + 4, (int32_t)obs.numobj, is_lsb);
	fmp_put_i32(hdr + 8, (int32_t)obs.curobj, is_lsb);
	fmp_put_i32(hdr + 12, (int32_t)OBSTR_DISK_SIZE, is_lsb);
	if (fwrite(hdr, 1, 16, f) != 16) return -1;
	for (i = 0; i < obs.numobj; i++) {
		const int *src = (const int *)&obs.objs[i];
		for (k = 0; k < OBSTR_DISK_LONGS; k++)
			fmp_put_i32(ent + k * 4, (int32_t)src[k], is_lsb);
		if (fwrite(ent, 1, sizeof(ent), f) != sizeof(ent)) return -1;
	}
	return 0;
}

int F14_NumObjects(void) { return obs.have_obdt ? obs.numobj : 0; }
int F14_CurObject(void) { return obs.have_obdt ? obs.curobj : 0; }
void F14_SetCurObject(int i)
{
	if (!obs.have_obdt) return;
	if (i<0) i = 0;
	if (i>=obs.numobj) i = obs.numobj-1;
	obs.curobj = i;
}
const F14Obj *F14_GetObject(int i)
{
	if (!obs.have_obdt || i < 0 || i >= obs.numobj) return NULL;
	return &obs.objs[i];
}
void F14_SetObject(int i, const F14Obj *o)
{
	if (!obs.have_obdt || i < 0 || i >= obs.numobj || o == NULL) return;
	obs.objs[i] = *o;
}
int F14_NewObject(void)
{
	if (!obs.have_obdt) {
		memset(obs.objs, 0, sizeof(obs.objs));
		obs.numobj = 0; obs.curobj = 0; obs.obstrsz = OBSTR_DISK_SIZE;
		obs.have_obdt = 1;
	}
	if (obs.numobj >= F14_MAXOBJ) return -1;
	memset(&obs.objs[obs.numobj], 0, sizeof(F14Obj));
	return obs.numobj++;
}
void F14_DeleteObject(int i)
{
	int k;
	if (!obs.have_obdt || i < 0 || i >= obs.numobj) return;
	for (k = i; k + 1 < obs.numobj; k++) obs.objs[k] = obs.objs[k + 1];
	obs.numobj--;
	if (obs.curobj >= obs.numobj) obs.curobj = obs.numobj - 1;
	if (obs.curobj < 0) obs.curobj = 0;
}

int F14_NumImages(void) { return obs.numimg; }
const char *F14_ImageName(int i)
{
	if (i < 0 || i >= obs.numimg) return NULL;
	return obs.images[i];
}
int F14_AddImage(const char *fname)
{
	if (fname == NULL || obs.numimg >= F14_MAXIMAGES) return -1;
	strncpy(obs.images[obs.numimg], fname, F14_MAXIMGNAMELEN - 1);
	obs.images[obs.numimg][F14_MAXIMGNAMELEN - 1] = 0;
	return obs.numimg++;
}
void F14_DeleteImage(int i)
{
	int k;
	if (i < 0 || i >= obs.numimg) return;
	for (k = i; k + 1 < obs.numimg; k++)
		memcpy(obs.images[k], obs.images[k + 1], F14_MAXIMGNAMELEN);
	obs.numimg--;
}

/* ---------------- lifecycle ---------------- */

void F14_FreeAll(void)
{
	memset(&ephd, 0, sizeof(ephd));
	memset(&tstr, 0, sizeof(tstr));
	memset(&lstr, 0, sizeof(lstr));
	memset(&obs, 0, sizeof(obs));
	obs.obstrsz = OBSTR_DISK_SIZE;
}
