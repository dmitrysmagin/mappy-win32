/* tileset.c - Tileset FMP chunks TSDF (definitions) and TSIN (instances).
 *
 * TSDF body (mapislsb):
 *   +0  u32 data_offset (= 16)
 *   +4  u32 count
 *   +8  u32 version (= 1)
 *   +12 u32 reserved
 *   then count records:
 *     i16 width, i16 height, i16 handle_x, i16 handle_y
 *     char name[64]
 *     i16 tiles[width*height]   (BODY cell encoding)
 *
 * TSIN body (mapislsb):
 *   +0  u32 data_offset (= 16)
 *   +4  u32 count
 *   +8  u32 current instance
 *   +12 u32 entry_size (= 24)
 *   then count records:
 *     i32 tset, layer, tx, ty, show, flags
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "fmp.h"
#include "tileset.h"

static struct {
	int present;
	int num;
	int cur;
	TilesetDef defs[TS_MAXDEFS];
} tsdf;

static struct {
	int present;
	int num;
	int cur;
	TilesetInst inst[TS_MAXINST];
} tsin;

static void ts_free_def_tiles(void)
{
	int i;
	for (i = 0; i < TS_MAXDEFS; i++) {
		free(tsdf.defs[i].tiles);
		tsdf.defs[i].tiles = NULL;
	}
}

void TS_FreeAll(void)
{
	ts_free_def_tiles();
	memset(&tsdf, 0, sizeof(tsdf));
	memset(&tsin, 0, sizeof(tsin));
}

static short ts_disk_to_mem(short v, int maptype, int blockstrsize)
{
	if (maptype == 0) {
		if (v >= 0) {
			if (blockstrsize > 0)
				v = (short)(v / blockstrsize);
		} else {
			v = (short)((v / 16) * FMP_ANI_MEMUNIT);
		}
	} else {
		if (v < 0)
			v = (short)(v * FMP_ANI_MEMUNIT);
	}
	return v;
}

static short ts_mem_to_disk(short v, int maptype, int blockstrsize)
{
	if (maptype == 0) {
		if (v >= 0)
			v = (short)(v * blockstrsize);
	} else {
		if (v < 0)
			v = (short)(v / FMP_ANI_MEMUNIT);
	}
	return v;
}

static int ts_write_pad_even(FILE *f)
{
	long p = ftell(f);
	unsigned char z = 0;
	if (p < 0)
		return -1;
	if (p & 1)
		return (fwrite(&z, 1, 1, f) == 1) ? 0 : -1;
	return 0;
}

static int ts_alloc_tiles(TilesetDef *d, int w, int h)
{
	size_t n;
	if (d == NULL || w < 1 || h < 1 || w > TS_MAXDIM || h > TS_MAXDIM)
		return -1;
	n = (size_t)w * (size_t)h;
	d->tiles = (short *)calloc(n, sizeof(short));
	if (d->tiles == NULL)
		return -1;
	d->width = w;
	d->height = h;
	return 0;
}

int TS_DecodeTSDF(const unsigned char *chunk, int bodysize, int is_lsb,
	int maptype, int blockstrsize)
{
	int32_t off, cnt, ver;
	int i, pos;
	const unsigned char *body;

	if (chunk == NULL || bodysize < TS_TSDF_HDR_SIZE)
		return -1;

	/* Reset definitions only. TSIN may already be loaded. */
	ts_free_def_tiles();
	memset(&tsdf, 0, sizeof(tsdf));

	body = chunk + 8;
	off = fmp_get_i32(body + 0, is_lsb);
	cnt = fmp_get_i32(body + 4, is_lsb);
	ver = fmp_get_i32(body + 8, is_lsb);
	if (off < TS_TSDF_HDR_SIZE)
		return -1;
	if (cnt < 0 || cnt > TS_MAXDEFS)
		return -1;
	if (ver > TS_TSDF_VERSION)
		return -1;

	pos = (int)off;
	for (i = 0; i < (int)cnt; i++) {
		int w, h, n, k;
		if (pos + 8 + TS_NAMELEN > bodysize)
			goto fail;
		w = fmp_get_i16(body + pos + 0, is_lsb);
		h = fmp_get_i16(body + pos + 2, is_lsb);
		if (w < 1 || h < 1 || w > TS_MAXDIM || h > TS_MAXDIM)
			goto fail;
		if (ts_alloc_tiles(&tsdf.defs[i], w, h) != 0)
			goto fail;
		tsdf.defs[i].handle_x = fmp_get_i16(body + pos + 4, is_lsb);
		tsdf.defs[i].handle_y = fmp_get_i16(body + pos + 6, is_lsb);
		memcpy(tsdf.defs[i].name, body + pos + 8, TS_NAMELEN);
		tsdf.defs[i].name[TS_NAMELEN - 1] = 0;
		pos += 8 + TS_NAMELEN;
		n = w * h;
		if (pos + n * 2 > bodysize)
			goto fail;
		for (k = 0; k < n; k++) {
			short v = (short)fmp_get_i16(body + pos, is_lsb);
			tsdf.defs[i].tiles[k] = ts_disk_to_mem(v, maptype, blockstrsize);
			pos += 2;
		}
		tsdf.num = i + 1;
	}
	tsdf.present = 1;
	tsdf.cur = 0;
	return 0;
fail:
	ts_free_def_tiles();
	memset(&tsdf, 0, sizeof(tsdf));
	return -1;
}

int TS_DecodeTSIN(const unsigned char *chunk, int bodysize, int is_lsb)
{
	int32_t off, cnt, cur, esz;
	int i;
	const unsigned char *body, *p;

	if (chunk == NULL || bodysize < TS_TSIN_HDR_SIZE)
		return -1;

	memset(&tsin, 0, sizeof(tsin));
	body = chunk + 8;
	off = fmp_get_i32(body + 0, is_lsb);
	cnt = fmp_get_i32(body + 4, is_lsb);
	cur = fmp_get_i32(body + 8, is_lsb);
	esz = fmp_get_i32(body + 12, is_lsb);
	if (off < TS_TSIN_HDR_SIZE)
		return -1;
	if (cnt < 0 || cnt > TS_MAXINST)
		return -1;
	if (esz < TS_INST_DISK_SIZE)
		esz = TS_INST_DISK_SIZE;
	if (off + cnt * esz > bodysize)
		return -1;

	p = body + off;
	for (i = 0; i < (int)cnt; i++) {
		tsin.inst[i].tset = (int)fmp_get_i32(p + 0, is_lsb);
		tsin.inst[i].layer = (int)fmp_get_i32(p + 4, is_lsb);
		tsin.inst[i].tx = (int)fmp_get_i32(p + 8, is_lsb);
		tsin.inst[i].ty = (int)fmp_get_i32(p + 12, is_lsb);
		tsin.inst[i].show = (int)fmp_get_i32(p + 16, is_lsb);
		tsin.inst[i].flags = (int)fmp_get_i32(p + 20, is_lsb);
		p += esz;
	}
	tsin.present = 1;
	tsin.num = (int)cnt;
	tsin.cur = (int)cur;
	if (tsin.cur < 0)
		tsin.cur = 0;
	if (tsin.num > 0 && tsin.cur >= tsin.num)
		tsin.cur = tsin.num - 1;
	return 0;
}

int TS_HasTSDF(void)
{
	return tsdf.present && tsdf.num > 0;
}

int TS_HasTSIN(void)
{
	return tsin.present && tsin.num > 0;
}

int TS_SaveTSDF(FILE *f, int is_lsb, int maptype, int blockstrsize)
{
	int i, k;
	uint32_t body = TS_TSDF_HDR_SIZE;
	unsigned char hdr[TS_TSDF_HDR_SIZE];

	if (!TS_HasTSDF())
		return 0;
	for (i = 0; i < tsdf.num; i++) {
		int n = tsdf.defs[i].width * tsdf.defs[i].height;
		body += (uint32_t)(8 + TS_NAMELEN + n * 2);
	}
	if (body & 1)
		body++;
	if (fmp_write_chunk_header(f, 'T', 'S', 'D', 'F', body) != 0)
		return -1;
	memset(hdr, 0, sizeof(hdr));
	fmp_put_i32(hdr + 0, TS_TSDF_HDR_SIZE, is_lsb);
	fmp_put_i32(hdr + 4, (int32_t)tsdf.num, is_lsb);
	fmp_put_i32(hdr + 8, TS_TSDF_VERSION, is_lsb);
	if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
		return -1;
	for (i = 0; i < tsdf.num; i++) {
		unsigned char rec[8 + TS_NAMELEN];
		int n = tsdf.defs[i].width * tsdf.defs[i].height;
		memset(rec, 0, sizeof(rec));
		fmp_put_i16(rec + 0, tsdf.defs[i].width, is_lsb);
		fmp_put_i16(rec + 2, tsdf.defs[i].height, is_lsb);
		fmp_put_i16(rec + 4, tsdf.defs[i].handle_x, is_lsb);
		fmp_put_i16(rec + 6, tsdf.defs[i].handle_y, is_lsb);
		memcpy(rec + 8, tsdf.defs[i].name, TS_NAMELEN);
		rec[8 + TS_NAMELEN - 1] = 0;
		if (fwrite(rec, 1, sizeof(rec), f) != sizeof(rec))
			return -1;
		for (k = 0; k < n; k++) {
			unsigned char s[2];
			short v = ts_mem_to_disk(tsdf.defs[i].tiles[k], maptype, blockstrsize);
			fmp_put_i16(s, v, is_lsb);
			if (fwrite(s, 1, 2, f) != 2)
				return -1;
		}
	}
	return ts_write_pad_even(f);
}

int TS_SaveTSIN(FILE *f, int is_lsb)
{
	int i;
	uint32_t body;
	unsigned char hdr[TS_TSIN_HDR_SIZE];

	if (!TS_HasTSIN())
		return 0;
	body = (uint32_t)(TS_TSIN_HDR_SIZE + tsin.num * TS_INST_DISK_SIZE);
	if (fmp_write_chunk_header(f, 'T', 'S', 'I', 'N', body) != 0)
		return -1;
	memset(hdr, 0, sizeof(hdr));
	fmp_put_i32(hdr + 0, TS_TSIN_HDR_SIZE, is_lsb);
	fmp_put_i32(hdr + 4, (int32_t)tsin.num, is_lsb);
	fmp_put_i32(hdr + 8, (int32_t)tsin.cur, is_lsb);
	fmp_put_i32(hdr + 12, TS_INST_DISK_SIZE, is_lsb);
	if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
		return -1;
	for (i = 0; i < tsin.num; i++) {
		unsigned char e[TS_INST_DISK_SIZE];
		fmp_put_i32(e + 0, tsin.inst[i].tset, is_lsb);
		fmp_put_i32(e + 4, tsin.inst[i].layer, is_lsb);
		fmp_put_i32(e + 8, tsin.inst[i].tx, is_lsb);
		fmp_put_i32(e + 12, tsin.inst[i].ty, is_lsb);
		fmp_put_i32(e + 16, tsin.inst[i].show, is_lsb);
		fmp_put_i32(e + 20, tsin.inst[i].flags, is_lsb);
		if (fwrite(e, 1, sizeof(e), f) != sizeof(e))
			return -1;
	}
	return 0;
}

int TS_NumDefs(void)
{
	return tsdf.present ? tsdf.num : 0;
}

int TS_CurDef(void)
{
	return tsdf.present ? tsdf.cur : 0;
}

void TS_SetCurDef(int i)
{
	if (!tsdf.present)
		return;
	if (i < 0)
		i = 0;
	if (tsdf.num > 0 && i >= tsdf.num)
		i = tsdf.num - 1;
	tsdf.cur = i;
}

const TilesetDef *TS_GetDef(int i)
{
	if (!tsdf.present || i < 0 || i >= tsdf.num)
		return NULL;
	return &tsdf.defs[i];
}

int TS_NewDef(int width, int height)
{
	int i;
	if (width < 1 || height < 1 || width > TS_MAXDIM || height > TS_MAXDIM)
		return -1;
	if (!tsdf.present) {
		ts_free_def_tiles();
		memset(&tsdf, 0, sizeof(tsdf));
		tsdf.present = 1;
	}
	if (tsdf.num >= TS_MAXDEFS)
		return -1;
	i = tsdf.num;
	memset(&tsdf.defs[i], 0, sizeof(tsdf.defs[i]));
	if (ts_alloc_tiles(&tsdf.defs[i], width, height) != 0)
		return -1;
	strncpy(tsdf.defs[i].name, "Tileset", TS_NAMELEN - 1);
	tsdf.num++;
	tsdf.cur = i;
	return i;
}

int TS_SetDefMeta(int i, const char *name, int hx, int hy)
{
	if (!tsdf.present || i < 0 || i >= tsdf.num)
		return -1;
	if (name != NULL) {
		strncpy(tsdf.defs[i].name, name, TS_NAMELEN - 1);
		tsdf.defs[i].name[TS_NAMELEN - 1] = 0;
	}
	tsdf.defs[i].handle_x = hx;
	tsdf.defs[i].handle_y = hy;
	return 0;
}

int TS_ResizeDef(int i, int width, int height)
{
	TilesetDef *d;
	short *neu;
	int x, y, ow, oh;
	size_t n;

	if (!tsdf.present || i < 0 || i >= tsdf.num)
		return -1;
	if (width < 1 || height < 1 || width > TS_MAXDIM || height > TS_MAXDIM)
		return -1;
	d = &tsdf.defs[i];
	if (width == d->width && height == d->height)
		return 0;
	n = (size_t)width * (size_t)height;
	neu = (short *)calloc(n, sizeof(short));
	if (neu == NULL)
		return -1;
	ow = d->width;
	oh = d->height;
	for (y = 0; y < height && y < oh; y++) {
		for (x = 0; x < width && x < ow; x++)
			neu[y * width + x] = d->tiles[y * ow + x];
	}
	free(d->tiles);
	d->tiles = neu;
	d->width = width;
	d->height = height;
	return 0;
}

short TS_GetCell(int i, int x, int y)
{
	const TilesetDef *d = TS_GetDef(i);
	if (d == NULL || d->tiles == NULL)
		return 0;
	if (x < 0 || y < 0 || x >= d->width || y >= d->height)
		return 0;
	return d->tiles[y * d->width + x];
}

int TS_SetCell(int i, int x, int y, short v)
{
	TilesetDef *d;
	if (!tsdf.present || i < 0 || i >= tsdf.num)
		return -1;
	d = &tsdf.defs[i];
	if (d->tiles == NULL || x < 0 || y < 0 || x >= d->width || y >= d->height)
		return -1;
	d->tiles[y * d->width + x] = v;
	return 0;
}

void TS_DeleteDef(int i)
{
	int k;
	if (!tsdf.present || i < 0 || i >= tsdf.num)
		return;
	free(tsdf.defs[i].tiles);
	for (k = i; k + 1 < tsdf.num; k++)
		tsdf.defs[k] = tsdf.defs[k + 1];
	memset(&tsdf.defs[tsdf.num - 1], 0, sizeof(tsdf.defs[0]));
	tsdf.num--;
	if (tsdf.cur >= tsdf.num)
		tsdf.cur = tsdf.num - 1;
	if (tsdf.cur < 0)
		tsdf.cur = 0;
	if (tsdf.num == 0)
		tsdf.present = 0;

	/* drop instances of this def; remap higher indices */
	for (k = tsin.num - 1; k >= 0; k--) {
		if (tsin.inst[k].tset == i)
			TS_DeleteInst(k);
		else if (tsin.inst[k].tset > i)
			tsin.inst[k].tset--;
	}
}

int TS_DuplicateDef(int i)
{
	const TilesetDef *d;
	int n, x, y, w, h, hx, hy;
	char name[TS_NAMELEN];

	d = TS_GetDef(i);
	if (d == NULL)
		return -1;
	w = d->width;
	h = d->height;
	hx = d->handle_x;
	hy = d->handle_y;
	if (d->name[0])
		snprintf(name, sizeof(name), "%.54s copy", d->name);
	else
		strncpy(name, "Tileset copy", TS_NAMELEN - 1);
	n = TS_NewDef(w, h);
	if (n < 0)
		return -1;
	TS_SetDefMeta(n, name, hx, hy);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			TS_SetCell(n, x, y, TS_GetCell(i, x, y));
	}
	return n;
}

int TS_GrabFromLayer(int i, const short *layer, int mapw, int maph,
	int x1, int y1, int x2, int y2)
{
	int t, x, y, w, h;

	if (layer == NULL || mapw < 1 || maph < 1)
		return -1;
	if (x2 < x1) { t = x1; x1 = x2; x2 = t; }
	if (y2 < y1) { t = y1; y1 = y2; y2 = t; }
	if (x1 < 0 || y1 < 0 || x2 >= mapw || y2 >= maph)
		return -1;
	w = x2 - x1 + 1;
	h = y2 - y1 + 1;
	if (w < 1 || h < 1 || w > TS_MAXDIM || h > TS_MAXDIM)
		return -1;
	if (TS_GetDef(i) == NULL)
		return -1;
	if (TS_ResizeDef(i, w, h) != 0)
		return -1;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			TS_SetCell(i, x, y, layer[(y1 + y) * mapw + (x1 + x)]);
	}
	return 0;
}

int TS_NumInst(void)
{
	return tsin.present ? tsin.num : 0;
}

int TS_CurInst(void)
{
	return tsin.present ? tsin.cur : 0;
}

void TS_SetCurInst(int i)
{
	if (!tsin.present)
		return;
	if (i < 0)
		i = 0;
	if (tsin.num > 0 && i >= tsin.num)
		i = tsin.num - 1;
	tsin.cur = i;
}

const TilesetInst *TS_GetInst(int i)
{
	if (!tsin.present || i < 0 || i >= tsin.num)
		return NULL;
	return &tsin.inst[i];
}

void TS_SetInst(int i, const TilesetInst *inst)
{
	if (!tsin.present || i < 0 || i >= tsin.num || inst == NULL)
		return;
	tsin.inst[i] = *inst;
}

int TS_NewInst(void)
{
	int i;
	if (!tsin.present) {
		memset(&tsin, 0, sizeof(tsin));
		tsin.present = 1;
	}
	if (tsin.num >= TS_MAXINST)
		return -1;
	i = tsin.num;
	memset(&tsin.inst[i], 0, sizeof(tsin.inst[i]));
	tsin.inst[i].show = 1;
	tsin.num++;
	tsin.cur = i;
	return i;
}

void TS_DeleteInst(int i)
{
	int k;
	if (!tsin.present || i < 0 || i >= tsin.num)
		return;
	for (k = i; k + 1 < tsin.num; k++)
		tsin.inst[k] = tsin.inst[k + 1];
	memset(&tsin.inst[tsin.num - 1], 0, sizeof(tsin.inst[0]));
	tsin.num--;
	if (tsin.cur >= tsin.num)
		tsin.cur = tsin.num - 1;
	if (tsin.cur < 0)
		tsin.cur = 0;
	if (tsin.num == 0)
		tsin.present = 0;
}

int TS_PlaceNew(int def, int layer, int tx, int ty)
{
	int i;
	TilesetInst inst;

	if (TS_GetDef(def) == NULL)
		return -1;
	i = TS_NewInst();
	if (i < 0)
		return -1;
	memset(&inst, 0, sizeof(inst));
	inst.tset = def;
	inst.layer = layer;
	inst.tx = tx;
	inst.ty = ty;
	inst.show = 1;
	TS_SetInst(i, &inst);
	return i;
}

int TS_InstBounds(int i, int *ox, int *oy, int *w, int *h)
{
	const TilesetInst *in = TS_GetInst(i);
	const TilesetDef *d;

	if (in == NULL)
		return -1;
	d = TS_GetDef(in->tset);
	if (d == NULL)
		return -1;
	if (ox) *ox = in->tx - d->handle_x;
	if (oy) *oy = in->ty - d->handle_y;
	if (w) *w = d->width;
	if (h) *h = d->height;
	return 0;
}

int TS_HitInst(int tx, int ty, int layer)
{
	int i;

	for (i = TS_NumInst() - 1; i >= 0; i--) {
		int ox, oy, w, h;
		const TilesetInst *in = TS_GetInst(i);
		if (in == NULL || !in->show)
			continue;
		if (layer >= 0 && in->layer != layer)
			continue;
		if (TS_InstBounds(i, &ox, &oy, &w, &h) != 0)
			continue;
		if (tx >= ox && ty >= oy && tx < ox + w && ty < oy + h)
			return i;
	}
	return -1;
}

int TS_FindInstOfDef(int def, int layer)
{
	int i;

	for (i = 0; i < TS_NumInst(); i++) {
		const TilesetInst *in = TS_GetInst(i);
		if (in == NULL)
			continue;
		if (in->tset != def)
			continue;
		if (layer >= 0 && in->layer != layer)
			continue;
		return i;
	}
	return -1;
}

static int ts_read_chunk(FILE *f, unsigned char **out, int *bodysize)
{
	unsigned char hdr[8];
	uint32_t sz;
	unsigned char *buf;

	if (fread(hdr, 1, 8, f) != 8)
		return -1;
	sz = fmp_get_be32(hdr + 4);
	if (sz > 8u * 1024u * 1024u)
		return -1;
	buf = (unsigned char *)malloc((size_t)sz + 8);
	if (buf == NULL)
		return -1;
	memcpy(buf, hdr, 8);
	if (sz > 0 && fread(buf + 8, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		return -1;
	}
	*out = buf;
	*bodysize = (int)sz;
	return 0;
}

static int ts_roundtrip_once(int is_lsb, int maptype, int blockstrsize)
{
	FILE *f;
	unsigned char *c1 = NULL, *c2 = NULL;
	int b1 = 0, b2 = 0;
	int d0, d1, i0, i1;
	const TilesetDef *a, *b;
	const TilesetInst *p, *q;
	TilesetInst inst;
	int rc = -1;

	TS_FreeAll();
	d0 = TS_NewDef(3, 2);
	d1 = TS_NewDef(1, 1);
	if (d0 < 0 || d1 < 0)
		goto done;
	TS_SetDefMeta(d0, "House", 1, 0);
	TS_SetCell(d0, 0, 0, 0);
	TS_SetCell(d0, 1, 0, 7);
	TS_SetCell(d0, 2, 0, (short)(-(1 * FMP_ANI_MEMUNIT)));
	TS_SetCell(d0, 0, 1, 12);
	TS_SetCell(d0, 1, 1, 0);
	TS_SetCell(d0, 2, 1, 3);
	TS_SetDefMeta(d1, "Dot", 0, 0);
	TS_SetCell(d1, 0, 0, 99);

	i0 = TS_NewInst();
	i1 = TS_NewInst();
	if (i0 < 0 || i1 < 0)
		goto done;
	memset(&inst, 0, sizeof(inst));
	inst.tset = d0;
	inst.layer = 2;
	inst.tx = 10;
	inst.ty = 20;
	inst.show = 1;
	inst.flags = 0x11;
	TS_SetInst(i0, &inst);
	inst.tset = d1;
	inst.layer = 0;
	inst.tx = 0;
	inst.ty = 5;
	inst.show = 0;
	inst.flags = 0;
	TS_SetInst(i1, &inst);
	TS_SetCurInst(i0);

	f = tmpfile();
	if (f == NULL)
		f = fopen("tileset_selftest.tmp", "w+b");
	if (f == NULL)
		goto done;
	if (TS_SaveTSDF(f, is_lsb, maptype, blockstrsize) != 0 ||
		TS_SaveTSIN(f, is_lsb) != 0) {
		fclose(f);
		remove("tileset_selftest.tmp");
		goto done;
	}
	rewind(f);
	if (ts_read_chunk(f, &c1, &b1) != 0 || ts_read_chunk(f, &c2, &b2) != 0) {
		fclose(f);
		remove("tileset_selftest.tmp");
		goto done;
	}
	fclose(f);
	remove("tileset_selftest.tmp");

	TS_FreeAll();
	if (memcmp(c1, "TSDF", 4) != 0 || memcmp(c2, "TSIN", 4) != 0)
		goto done;
	if (TS_DecodeTSDF(c1, b1, is_lsb, maptype, blockstrsize) != 0)
		goto done;
	if (TS_DecodeTSIN(c2, b2, is_lsb) != 0)
		goto done;

	if (TS_NumDefs() != 2 || TS_NumInst() != 2)
		goto done;
	a = TS_GetDef(0);
	b = TS_GetDef(1);
	if (a == NULL || b == NULL)
		goto done;
	if (strcmp(a->name, "House") != 0 || a->width != 3 || a->height != 2)
		goto done;
	if (a->handle_x != 1 || a->handle_y != 0)
		goto done;
	if (TS_GetCell(0, 0, 0) != 0 || TS_GetCell(0, 1, 0) != 7)
		goto done;
	if (TS_GetCell(0, 2, 0) != (short)(-(1 * FMP_ANI_MEMUNIT)))
		goto done;
	if (TS_GetCell(0, 0, 1) != 12 || TS_GetCell(0, 2, 1) != 3)
		goto done;
	if (strcmp(b->name, "Dot") != 0 || TS_GetCell(1, 0, 0) != 99)
		goto done;

	p = TS_GetInst(0);
	q = TS_GetInst(1);
	if (p == NULL || q == NULL)
		goto done;
	if (p->tset != 0 || p->layer != 2 || p->tx != 10 || p->ty != 20)
		goto done;
	if (p->show != 1 || p->flags != 0x11)
		goto done;
	if (q->tset != 1 || q->layer != 0 || q->ty != 5 || q->show != 0)
		goto done;
	if (TS_CurInst() != 0)
		goto done;
	rc = 0;

done:
	free(c1);
	free(c2);
	TS_FreeAll();
	return rc;
}

int TS_SelfTest(void)
{
	if (ts_roundtrip_once(1, 0, 32) != 0)
		return -1;
	if (ts_roundtrip_once(1, 1, 32) != 0)
		return -2;
	if (ts_roundtrip_once(0, 0, 32) != 0)
		return -3;
	if (ts_roundtrip_once(0, 1, 32) != 0)
		return -4;
	return 0;
}
