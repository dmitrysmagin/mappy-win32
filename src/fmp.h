/* fmp.h - endian-safe, arch-safe FMP serialization helpers for Mappy Win32 modernization.
 *
 * Goals:
 *  - Never fwrite/fread host structs (GENHEAD/MPHD/EDHD/BLKSTR/ANISTR) directly.
 *    Their in-memory layout depends on sizeof(long), padding and bitfield order.
 *  - Chunk headers (4 byte ID + 4 byte BE size) are always big-endian (Motorola).
 *  - Chunk bodies use mapislsb (1 = LE/Intel, 0 = BE/Motorola) via MapPut/MapGet style.
 *  - Disk sizes are fixed constants, independent of host sizeof().
 */
#ifndef FMP_H
#define FMP_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Fixed on-disk sizes (must never use sizeof() for these) */
#define FMP_GENHEAD_DISK_SIZE 8
#define FMP_MPHD_DISK_SIZE 40
#define FMP_EDHD_DISK_SIZE 32
#define FMP_BLKSTR_DISK_SIZE 32
#define FMP_ANISTR_DISK_SIZE 16

/* In-memory scaling units used by mappt/brush/anim tables.
 * Memory stores byte offsets; file stores small indices.
 * Keep as constants so files stay identical even if struct packing changes. */
#define FMP_BLK_MEMUNIT 32
#define FMP_ANI_MEMUNIT 16
#define FMP_SEQ_MEMUNIT 4

/* ---- byte order primitives (shift-based, no aliasing, no host dependency) ---- */
static inline uint32_t fmp_get_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void fmp_put_be32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)((v >> 24) & 0xFF);
    p[1] = (unsigned char)((v >> 16) & 0xFF);
    p[2] = (unsigned char)((v >> 8) & 0xFF);
    p[3] = (unsigned char)(v & 0xFF);
}

/* mapislsb-aware put/get (matches legacy MapPutshort/MapPutlong semantics) */
static inline void fmp_put_i16(unsigned char *p, int v, int is_lsb) {
    uint16_t u = (uint16_t)(int16_t)v;
    if (is_lsb) { p[0] = (unsigned char)(u & 0xFF); p[1] = (unsigned char)((u >> 8) & 0xFF); }
    else { p[0] = (unsigned char)((u >> 8) & 0xFF); p[1] = (unsigned char)(u & 0xFF); }
}

static inline void fmp_put_i32(unsigned char *p, int32_t v, int is_lsb) {
    uint32_t u = (uint32_t)v;
    if (is_lsb) {
        p[0] = (unsigned char)(u & 0xFF); p[1] = (unsigned char)((u >> 8) & 0xFF);
        p[2] = (unsigned char)((u >> 16) & 0xFF); p[3] = (unsigned char)((u >> 24) & 0xFF);
    } else {
        p[0] = (unsigned char)((u >> 24) & 0xFF); p[1] = (unsigned char)((u >> 16) & 0xFF);
        p[2] = (unsigned char)((u >> 8) & 0xFF); p[3] = (unsigned char)(u & 0xFF);
    }
}

static inline int fmp_get_i16(const unsigned char *p, int is_lsb) {
    int v;
    if (is_lsb) v = (int)p[0] | ((int)p[1] << 8);
    else v = ((int)p[0] << 8) | (int)p[1];
    if (v & 0x8000) v -= 0x10000;
    return v;
}

static inline int32_t fmp_get_i32(const unsigned char *p, int is_lsb) {
    uint32_t u;
    if (is_lsb) u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    else u = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
             ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    return (int32_t)u;
}

/* ---- FILE-level chunk header I/O (always BE size) ---- */
static inline int fmp_write_chunk_header(FILE *f, char a, char b, char c, char d, uint32_t size) {
    unsigned char hdr[FMP_GENHEAD_DISK_SIZE];
    hdr[0] = (unsigned char)a; hdr[1] = (unsigned char)b;
    hdr[2] = (unsigned char)c; hdr[3] = (unsigned char)d;
    fmp_put_be32(hdr + 4, size);
    return (fwrite(hdr, 1, sizeof(hdr), f) == sizeof(hdr)) ? 0 : -1;
}

static inline int fmp_write_be32(FILE *f, uint32_t v) {
    unsigned char buf[4];
    fmp_put_be32(buf, v);
    return (fwrite(buf, 1, 4, f) == 4) ? 0 : -1;
}

/* Pad a chunk body to even length. Use payload byte count, not ftell:
 * an earlier odd-sized chunk can leave the file offset odd, in which case
 * ftell-based padding skips the byte the size field already reserved. */
static inline int fmp_write_odd_pad(FILE *f, uint32_t payload) {
    unsigned char z = 0;
    if (payload & 1)
        return (fwrite(&z, 1, 1, f) == 1) ? 0 : -1;
    return 0;
}

#endif /* FMP_H */
