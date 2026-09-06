# FMP file format

This is the on-disk layout of Mappy `.FMP` (and graphics-less `.FMA`) files as implemented by this editor (`src/mapfunc.c`, `src/fmp14.c`, `src/tileset.c`). Chunk headers are IFF-style; chunk *bodies* follow Mappy’s own rules.

Older editors that do not know a chunk should skip it (4-byte ID + 4-byte size). This editor keeps unknown chunks and writes them back.

## Container

```
offset  size  field
0       4     "FORM"
4       4     payload size, big-endian (file size minus 8)
8       4     "FMAP"
12      …     chunks, packed back-to-back
```

The FORM size is patched after the rest of the file is written.

There is **no** leading pad between chunks. Some writers pad an individual chunk body to an even number of bytes; the size field includes that pad. The pad is based on the payload length, not the file offset: an odd-sized `OBFN` can start the next chunk at an odd offset. Older saves of this editor reserved the pad in the size but omitted the byte (ftell was already even); the loader backs up one byte when the next chunk ID is not printable ASCII.

### Chunk header (always big-endian)

```
offset  size  field
0       4     chunk ID (ASCII, not NUL-terminated)
4       4     body size in bytes, big-endian
8       N     body
```

The size is the body only, not including the 8-byte header.

### Endianness of chunk bodies

`MPHD.lsb` (`mapislsb`) selects the byte order of **all multi-byte fields in chunk bodies**:

| `lsb` | Order |
|---|---|
| 1 | little-endian (Intel) — normal for this editor |
| 0 | big-endian (Motorola) |

Chunk **headers** (ID + size) are always big-endian, regardless of `lsb`.

Never dump a host `struct` to disk. Sizes below are fixed disk sizes.

## Typical write order

This editor writes, when present:

1. `FORM` / `FMAP`
2. `ATHR` (if any author field is non-empty)
3. `MPHD`
4. `EDHD` (brushes appended in the same chunk)
5. `EPHD`
6. `OBFN`, `OBDT` (if objects exist)
7. `TSTR` (if any text string is non-empty)
8. `LSTR`
9. `TSDF`, `TSIN` (if any tileset definition / instance exists)
10. `CMAP`
11. `BKDT`
12. `ANDT` (if any animation)
13. unknown chunks (preserved from load)
14. `AGFX` (optional; omitted from `.FMA`)
15. `NOVC` (if set)
16. `BGFX` (omitted from `.FMA`)
17. `BODY` (layer 0), `LYR1`–`LYR7`, then one `NLYR` per layer 8–99

Load accepts chunks in any order, except that `MPHD` must appear before layer data (and before `TSDF`) so `maptype` / `blockstrsize` are known.

## Cell values (layers, brushes, tilesets)

A map cell is a signed 16-bit integer.

**In memory** (this editor):

- `>= 0` — still-block index (`0` = first `BLKSTR`, also used as “empty” in brushes/tilesets)
- `< 0` — animation: `-(anim_index + 1) * 16` (`FMP_ANI_MEMUNIT`)

**On disk** the same short is scaled by `maptype`:

| `maptype` | Still (`>= 0`) | Anim (`< 0`) |
|---|---|---|
| 0 (FMP 0.5) | `index * blockstrsize` (32) | stored as the memory value (`/16 * 16` on load) |
| 1 (FMP 1.0) | `index` | `memory / 16` |
| 2, 3 | same as 1, plus RLE on the layer stream | same as 1 |

`TSDF` tiles use this encoding. Brushes in `EDHD` use it too.

## `maptype` / version

`MPHD` stores:

- `mapverhigh.mapverlow` — `0.5` when `maptype == 0`, `1.0` when `maptype >= 1`
- `maptype`:
  - `0` — FMP 0.5 (still cells are byte-offset style)
  - `1` — FMP 1.0 uncompressed
  - `2` — FMP 1.0 RLE (repeat-value runs)
  - `3` — FMP 1.0 RLE (copy-from-offset runs on load; this editor saves type 2)

`mapverhigh > 1` or `maptype > 3` is rejected as too new.

---

## Chunk catalogue

### `ATHR` — author strings

Body: four packed NUL-terminated strings (author, info1, info2, info3). Chunk size is rounded up to a multiple of 4. Bytes are not endian-swapped.

### `MPHD` — map header (40 bytes)

All multi-byte fields obey `lsb`.

```
off  type   field
0    u8     mapverhigh
1    u8     mapverlow
2    u8     lsb (1 = Intel)
3    u8     maptype
4    i16    mapwidth (tiles)
6    i16    mapheight (tiles)
8    i16    reserved1 — tile-mode flags (1.4.3+):
              bit 0 flipped tiles
              bit 1 rotated tiles
              bit 2 wnes / 3D-wall mode
10   i16    reserved2
12   i16    blockwidth (pixels)
14   i16    blockheight (pixels)
16   i16    blockdepth (8, 15, 16, 24, 32)
18   i16    blockstrsize (always 32 in this editor)
20   i16    numblockstr (count of BKDT records)
22   i16    numblockgfx (count of tiles in BGFX)
24   u8     ckey8bit
25   u8     ckeyred
26   u8     ckeygreen
27   u8     ckeyblue
28   i16    blockgapx
30   i16    blockgapy
32   i16    blockstaggerx
34   i16    blockstaggery
36   i16    clickmask (graphic index, or 0)
38   i16    pillars (iso pillar mode)
```

Older files may be shorter than 40 bytes; missing tail fields default (gap = block size, clickmask/pillars = 0).

### `EDHD` — editor header + brushes

Fixed 32-byte header, then optional brush data.

```
off  type   field
0    i16    xmapoffset
2    i16    ymapoffset
4    i32    fgcolour
8    i32    bgcolour
12   i16    swidth
14   i16    sheight
16   i16    strtstr
18   i16    strtblk
20   i16    curstr
22   i16    curanim
24   i16    animspd
26   i16    span
28   i16    numbrushes (count of the first 8 brushes that follow)
30   i16    pad / legacy clickmask if MPHD had none
```

If `numbrushes > 0`, the body continues with up to 8 brushes:

```
i16 width
i16 height
i16 cells[width*height]     (BODY cell encoding)
```

Then, if space remains, eight NUL-terminated brush names. An odd offset is padded with one zero.

If more than 8 brushes exist, an 8-byte extra header follows (`i16 unused[3]`, `i16 extra_count`) and then each extra brush as `width, height, cells[], name\0` with a pad byte when the name length is even.

Maximum 16 brushes. Editor-only; playback libraries ignore `EDHD`.

### `EPHD` — extra editor prefs (1.4)

Body 32 or 40 bytes (`lsb`). This editor always writes 40.

```
off  type   field
0    i16    curlayer
2    i16    onion
4    i16    oniondark
6    i16    onionlower
8    i16    divideron
10   i16    dividergapx
12   i16    dividergapy
16   i32    dividercol
20   i16    showuser          (if size > 0x1F)
22   i16    (spare)
24   i32    (spare, default 0x00FFFFFF)
28   i16    (spare, default 0x20)
32   i16    (spare, default 0x14)   [note: parsed after +34]
34   i16    (spare, default 1)
36   i16    (if size > 0x27)
38   i16    (if size > 0x27)
```

Unparsed tail bytes are preserved on round-trip.

### `TSTR` — text strings (1.4)

```
off  type   field
0    u32    string_offset (16)
4    u32    count
8    u32    spare
12   u32    spare
16   …      count packed NUL-terminated strings
```

Odd body size is padded with one zero. Used for block/object “Text String” combos.

### `LSTR` — layer names (1.4)

```
off  type   field
0    u32    string_offset (8)
4    u32    count
8    …      count packed NUL-terminated names (max 64 bytes each, 100 layers)
```

Always written on save (empty names for unnamed layers). Odd body padded.

### `OBFN` — object graphic filenames (1.4)

Body: packed NUL-terminated paths, no endian fields.

### `OBDT` — objects (1.4)

```
off  type   field
0    i32    array_offset (>= 16)
4    i32    numobjstr
8    i32    curobj
12   i32    obstrsz (80)
16   …      numobjstr records of 20 × i32 (80 bytes):
              xpos, ypos, gfxid, tileid,
              gxoff, gyoff, gwidth, gheight,
              ghandlexoff, ghandleyoff,
              show, user1..user7, flags
```

Positions are pixels. `gfxid > 0` is a BGFX tile; `gfxid == 0` and `tileid >= 0` is an `OBFN` image; `tileid < 0` is an area/label. Objects are overlays — they are not written into `BODY`.

### `TSDF` — tileset definitions (this editor)

Library of reusable `width × height` tile arrays. Not part of Mappy 1.4.23. Older tools treat the chunk as unknown and preserve it.

```
off  type   field
0    u32    data_offset (16)
4    u32    count                         (max 256)
8    u32    version (1)
12   u32    reserved
16   …      count variable records:
              i16 width                   (1..64)
              i16 height                  (1..64)
              i16 handle_x
              i16 handle_y
              char name[64]               (NUL-padded)
              i16 tiles[width*height]     (BODY cell encoding; 0 = empty)
```

Row-major, `tiles[y * width + x]`. Odd body padded with one zero.

A definition is not a placement. Instances live in `TSIN`.

### `TSIN` — tileset instances (this editor)

Placements of a `TSDF` definition on a layer, in **tile** coordinates. Overlay: not baked into `BODY` unless the editor explodes them (Tilesets → Explode selected).

```
off  type   field
0    u32    data_offset (16)
4    u32    count                         (max 4096)
8    u32    current instance
12   u32    entry_size (24)
16   …      count records of 6 × i32:
              tset    definition index
              layer   0..99
              tx, ty  tile position
              show    0/1
              flags   reserved
```

Written only when `count > 0`.

### `CMAP` — palette

768 bytes: 256 × RGB (8-bit each), not endian-swapped. Present even for truecolour maps.

### `BKDT` — block structures

`numblockstr` records of 32 bytes (`lsb`):

```
off  type   field
0    i32    bgoff
4    i32    fgoff
8    i32    fgoff2
12   i32    fgoff3
16   i32    user1
20   i32    user2
24   i16    user3
26   i16    user4
28   u8     user5
29   u8     user6
30   u8     user7
31   u8     flags:
              bit 0 tl
              bit 1 tr
              bit 2 bl
              bit 3 br
              bit 4 trigger
              bit 5 unused1 (iso stack)
              bit 6 unused2 (flip / pillar)
              bit 7 unused3 (flip / pillar)
```

`bgoff` / `fgoff*` on disk:

- `maptype == 0` — byte offset into `BGFX`
- `maptype >= 1` — graphic index (byte offset / `blockwidth * blockheight * bytes_per_pixel`)

### `ANDT` — animations

The body is stored **backwards** from the end of the chunk:

1. At the end, one or more 16-byte `ANISTR` records, last one `antype == -1` (`AN_END`).
2. Before that, a list of `i32` sequence entries (block references).

Each `ANISTR` (16 bytes, `lsb` on the three longs):

```
off  type   field
0    i8     antype (see below)
1    i8     andelay
2    i8     ancount
3    i8     anuser
4    i32    ancuroff
8    i32    anstartoff
12   i32    anendoff
```

`antype`: `-1` end, `0` none, `1` loop forward, `2` loop reverse, `3` once, `4` once + hold, `5`–`8` ping-pong variants, `9` once (sync).

Offsets: `maptype == 0` uses byte offsets inside the chunk; `maptype >= 1` uses sequence-index units (`FMP_SEQ_MEMUNIT` = 4 in memory). Sequence entries are block-structure indices (`maptype >= 1`) or `index * blockstrsize` (`maptype == 0`).

### `BGFX` — tile graphics

Raw pixels, `numblockgfx` tiles, each `blockwidth * blockheight` samples:

| `blockdepth` | Bytes per pixel |
|---|---|
| 8 | 1 (index into `CMAP`) |
| 15, 16 | 2 |
| 24 | 3 |
| 32 | 4 |

If the payload length is odd, one pad byte is included in the chunk size. Omitted from `.FMA`.

### `AGFX` — alternate graphics

Same layout idea as `BGFX` (one byte per pixel in the size calculation this editor uses). Optional. Omitted from `.FMA`.

### `NOVC` — “no video card” text

NUL-terminated ASCII, even size. Optional.

### `BODY` — layer 0

Uncompressed: `mapwidth * mapheight` signed 16-bit cells, row-major, `lsb`, BODY cell encoding.

`maptype == 2` (save): RLE per row. Each run is:

- `i16 count > 0` then `count` literal cells
- `i16 count < 0` then one cell repeated `-count` times

`maptype == 3` on load: negative count is followed by an offset; cells are copied from `dest + offset`.

### `LYR1` … `LYR7` — layers 1–7

Same body as `BODY`. ID is `LYR` + ASCII `'1'`…`'7'`.

Older builds of this editor also wrote layers 10–99 as `LY10`…`LY99` (two digits). Those still load.

### `NLYR` — layers 8–99 (1.4)

One chunk per layer.

```
off  type   field
0    u32    data_offset (16)
4    i16    layer index
6    i16    width
8    i16    height
10   6      zeros
16   …      same shorts as BODY/LYRn
```

### Unknown chunks

Any ID not listed above is stored as raw header+body and written back after `ANDT`, before `AGFX`. Lua can create/edit these (`CHUNKID` / `CHUNKSIZE`).

---

## Related files

| Extension | Role |
|---|---|
| `.FMP` | Full project (all chunks) |
| `.FMA` | Same as FMP without `BGFX`/`AGFX`. Open an FMP first so graphics stay in memory. |
| `.MAP` | Raw custom array (`maptype` in `MAPWIN.INI`), not this chunk format |
| `.MAR` | Binary dump of the map array |
| `.BAK` | Previous FMP when `backups=1` |

## Implementation

| Chunks | Source |
|---|---|
| Container, MPHD, EDHD, CMAP, BKDT, ANDT, BGFX, AGFX, NOVC, BODY/LYR, unknown | `src/mapfunc.c`, `src/fmp.h` |
| EPHD, TSTR, LSTR, OBDT, OBFN, NLYR prefix | `src/fmp14.c` |
| TSDF, TSIN | `src/tileset.c` |
