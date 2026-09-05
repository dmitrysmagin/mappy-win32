/* import.c */
#define  STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "mapwin.h"

extern short int blockwidth, blockheight, blockdepth, numblockgfx, numblockstr;
extern long int fgcolour;
extern char * blockgfxpt, * blockstrpt;
extern char * altgfxpt;
extern unsigned char * cmappt;

int imskip = 1;

void decode_pcx(unsigned char * mempt, int memend, unsigned char * tomempt, int width, int height, int bpp, int pitch);

/* ---- runtime libpng bridge (mapwin.lst: LIBPNG13.DLL -> LIBPNG12.DLL,
 * png_* resolved with GetProcAddress, "Loadlib failed" if neither loads) ---- */
typedef void * png_structp_;
typedef void * png_infop_;
typedef unsigned int png_uint_32_;
typedef struct { unsigned char red, green, blue; } png_color_;
typedef png_color_ * png_colorp_;
typedef unsigned char * png_bytep_;
typedef unsigned char ** png_bytepp_;
typedef void(*png_error_ptr_)(png_structp_, const char *);

typedef int(*png_sig_cmp_fn)(unsigned char *, unsigned long, unsigned long);
typedef void * (*png_create_read_struct_fn)(const char *, void *, void *, void *);
typedef void * (*png_create_info_struct_fn)(void *);
typedef void(*png_init_io_fn)(void *, void *);
typedef void(*png_set_sig_bytes_fn)(void *, int);
typedef void(*png_read_png_fn)(void *, void *, unsigned int, void *);
typedef void(*png_get_IHDR_fn)(void *, void *, png_uint_32_ *, png_uint_32_ *,
    int *, int *, int *, int *, int *);
typedef void(*png_get_PLTE_fn)(void *, void *, png_colorp_ *, int *);
typedef png_bytepp_(*png_get_rows_fn)(void *, void *);
typedef void(*png_destroy_read_struct_fn)(void **, void **, void **);

/* FARPROC vs typed fn: go through void * to silence -Wcast-function-type */
#define IMP_GPA(type, hmod, name) ((type)(void *)GetProcAddress((hmod), (name)))

/* libpng transform flags (png.h) */
#define PNG_TR_STRIP_16 0x0001
#define PNG_TR_PACKING 0x0004
#define PNG_TR_EXPAND 0x0010
#define PNG_TR_BGR 0x0080
#define PNG_TR_STRIP_ALPHA 0x0002

static jmp_buf png_jmp;
static void png_err_fn(void * p, const char * m) { (void) p; (void) m; longjmp(png_jmp, 1); }
static void png_warn_fn(void * p, const char * m) { (void) p; (void) m; }

/* Decode PNG into a BMP-layout buffer (14 + 40 + palette + bottom-up pixels).
 * Supports 4/8/24/32-bit PNGs. Returns 0 ok, -1 on error. */
static int IMDecodePNG(const char * fname, unsigned char ** outbuf, long * outsize) {
    FILE * fpt;
    unsigned char sig[8];
    HMODULE hpng;
    png_sig_cmp_fn p_sig_cmp;
    png_create_read_struct_fn p_create_read;
    png_create_info_struct_fn p_create_info;
    png_init_io_fn p_init_io;
    png_set_sig_bytes_fn p_set_sig;
    png_read_png_fn p_read_png;
    png_get_IHDR_fn p_get_IHDR;
    png_get_PLTE_fn p_get_PLTE;
    png_get_rows_fn p_get_rows;
    png_destroy_read_struct_fn p_destroy;
    void * png_ptr, * info_ptr;
    png_uint_32_ w, h;
    int bitdepth, colortype, interlace, compr, filt;
    png_bytepp_ rows;
    int npal;
    png_colorp_ pal;
    int ispal, i, x;
    long rowbytes, padrow, imgsize, palbytes, hdrsize, total;
    unsigned char * buf, * dp;
    BITMAPFILEHEADER * bfh;
    BITMAPINFOHEADER * bih;
    RGBQUAD * rq;

    fpt = fopen(fname, "rb");
    if (fpt==NULL) return -1;
    if (fread(sig, 1, 8, fpt)!=8) { fclose(fpt); return -1; }

    hpng = LoadLibrary("LIBPNG13.DLL");
    if (hpng==NULL) hpng = LoadLibrary("LIBPNG12.DLL");
    if (hpng==NULL) {
        fclose(fpt);
        MessageBox(NULL, "PNG import needs LIBPNG12.DLL (or LIBPNG13.DLL) and it could not be loaded.\nConvert the image to BMP or PCX first.", "Map Editor: Import", MB_OK|MB_ICONINFORMATION);
        return -1;
    }
    p_sig_cmp = IMP_GPA(png_sig_cmp_fn, hpng, "png_sig_cmp");
    p_create_read = IMP_GPA(png_create_read_struct_fn, hpng, "png_create_read_struct");
    p_create_info = IMP_GPA(png_create_info_struct_fn, hpng, "png_create_info_struct");
    p_init_io = IMP_GPA(png_init_io_fn, hpng, "png_init_io");
    p_set_sig = IMP_GPA(png_set_sig_bytes_fn, hpng, "png_set_sig_bytes");
    p_read_png = IMP_GPA(png_read_png_fn, hpng, "png_read_png");
    p_get_IHDR = IMP_GPA(png_get_IHDR_fn, hpng, "png_get_IHDR");
    p_get_PLTE = IMP_GPA(png_get_PLTE_fn, hpng, "png_get_PLTE");
    p_get_rows = IMP_GPA(png_get_rows_fn, hpng, "png_get_rows");
    p_destroy = IMP_GPA(png_destroy_read_struct_fn, hpng, "png_destroy_read_struct");
    if (!p_sig_cmp || !p_create_read || !p_create_info || !p_init_io || !p_set_sig ||
        !p_read_png || !p_get_IHDR || !p_get_PLTE || !p_get_rows || !p_destroy) {
        fclose(fpt); FreeLibrary(hpng); return -1;
    }

    if (p_sig_cmp(sig, 0, 8)) { fclose(fpt); FreeLibrary(hpng); return -1; }

    if (setjmp(png_jmp)) {
        fclose(fpt); FreeLibrary(hpng); return -1;
    }
    png_ptr = p_create_read("1.2.24", NULL, png_err_fn, png_warn_fn);
    if (png_ptr==NULL) { fclose(fpt); FreeLibrary(hpng); return -1; }
    info_ptr = p_create_info(png_ptr);
    if (info_ptr==NULL) { p_destroy(&png_ptr, NULL, NULL); fclose(fpt); FreeLibrary(hpng); return -1; }
    p_init_io(png_ptr, fpt);
    p_set_sig(png_ptr, 8);
    p_read_png(png_ptr, info_ptr, PNG_TR_PACKING|PNG_TR_STRIP_16, NULL);
    p_get_IHDR(png_ptr, info_ptr, &w, &h, &bitdepth, &colortype, &interlace, &compr, &filt);
    rows = p_get_rows(png_ptr, info_ptr);
    if (rows==NULL || w==0 || h==0 || w>8192 || h>8192) {
        p_destroy(&png_ptr, &info_ptr, NULL); fclose(fpt); FreeLibrary(hpng); return -1;
    }

    ispal = (colortype==3);
    if (ispal) {
        palbytes = 256*4; hdrsize = 14+40+palbytes; padrow = (w+3)&(~3L);
    } else {
        palbytes = 0; hdrsize = 14+40; padrow = ((w*3)+3)&(~3L);
    }
    imgsize = padrow*h;
    total = hdrsize+imgsize;
    buf = malloc(total);
    if (buf==NULL) { p_destroy(&png_ptr, &info_ptr, NULL); fclose(fpt); FreeLibrary(hpng); return -1; }
    memset(buf, 0, total);

    bfh = (BITMAPFILEHEADER *) buf;
    bfh->bfType = 19778;
    bfh->bfSize = total;
    bfh->bfOffBits = hdrsize;
    bih = (BITMAPINFOHEADER *) (buf+14);
    bih->biSize = 40;
    bih->biWidth = w; bih->biHeight = h;
    bih->biPlanes = 1; bih->biBitCount = ispal ? 8 : 24;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = imgsize;
    if (ispal) { bih->biClrUsed = 256; }
    rq = (RGBQUAD *) (buf+14+40);
    if (ispal) {
        p_get_PLTE(png_ptr, info_ptr, &pal, &npal);
        for (i=0;i<256;i++) {
            if (pal!=NULL && i<npal) {
                rq[i].rgbRed = pal[i].red; rq[i].rgbGreen = pal[i].green; rq[i].rgbBlue = pal[i].blue;
            } else { rq[i].rgbRed = rq[i].rgbGreen = rq[i].rgbBlue = 0; }
            rq[i].rgbReserved = 0;
        }
        if (cmappt!=NULL) {
            for (i=0;i<256;i++) {
                if (pal!=NULL && i<npal) {
                    cmappt[i*3] = pal[i].red; cmappt[i*3+1] = pal[i].green; cmappt[i*3+2] = pal[i].blue;
                } else { cmappt[i*3] = cmappt[i*3+1] = cmappt[i*3+2] = 0; }
            }
        }
    }
    dp = buf+hdrsize;
    rowbytes = ispal ? (long) w : (long) (w*3);
    /* libpng rows are RGB(A)/gray; BMP wants BGR bottom-up.
     * channels derived from colour type (0=gray, 2=RGB, 3=pal, 4=gray+alpha, 6=RGBA) */
    for (i=0;i<(int)h;i++) {
        unsigned char * src = rows[i];
        unsigned char * dst = dp+(h-1-i)*padrow;
        if (ispal) {
            memcpy(dst, src, (size_t) w);
        } else if (colortype==0) {
            for (x=0;x<(int)w;x++) {
                dst[x*3] = dst[x*3+1] = dst[x*3+2] = src[x];
            }
        } else if (colortype==4) {
            for (x=0;x<(int)w;x++) {
                dst[x*3] = dst[x*3+1] = dst[x*3+2] = src[x*2];
            }
        } else if (colortype==6) {
            for (x=0;x<(int)w;x++) {
                dst[x*3] = src[x*4+2]; dst[x*3+1] = src[x*4+1]; dst[x*3+2] = src[x*4];
            }
        } else {
            for (x=0;x<(int)w;x++) {
                dst[x*3] = src[x*3+2]; dst[x*3+1] = src[x*3+1]; dst[x*3+2] = src[x*3];
            }
        }
        (void) rowbytes;
    }

    p_destroy(&png_ptr, &info_ptr, NULL);
    fclose(fpt);
    FreeLibrary(hpng);
    *outbuf = buf; *outsize = total;
    return 0;
}

/* ---- MAPEXT.DLL hook (mapwin.lst sub_40A6EF: LoadLibrary("MAPEXT.DLL") ->
 * GetProcAddress("MPYDecodeBMP") -> malloc(w*h*4+0x2710) ->
 * MPYDecodeBMP(filename, filedata, outbuf), 0 = decoded 'BM' image) ---- */
typedef int(__stdcall *MPYDecodeBMP_fn)(const char *, unsigned char *, unsigned char *);

static int IMTryMapext(const char * fname, unsigned char * filedata, int iw, int ih,
    unsigned char ** outbuf) {
    HMODULE hme;
    MPYDecodeBMP_fn pdec;
    unsigned char * outb;

    hme = LoadLibrary("MAPEXT.DLL");
    if (hme==NULL) return -1;
    pdec = IMP_GPA(MPYDecodeBMP_fn, hme, "MPYDecodeBMP");
    if (pdec==NULL) { FreeLibrary(hme); return -1; }
    outb = malloc(iw*ih*4+0x2710);
    if (outb==NULL) { FreeLibrary(hme); return -1; }
    if (pdec(fname, filedata, outb)) { free(outb); FreeLibrary(hme); return -1; }
    FreeLibrary(hme);
    if (*(unsigned short *) outb != 19778) { free(outb); return -1; }
    *outbuf = outb;
    return 0;
}

/* Load BMP/PCX/PNG image as bottom-up 24-bit BGR pixels (for big-picture
 * matching). Returns 0 ok. Caller frees *out. */
int IMLoadImage24(char * imname, unsigned char ** out, int * iw, int * ih) {
    FILE * fpt;
    long filesize;
    unsigned char * fbuf, * px;
    BITMAPFILEHEADER * bfh;
    BITMAPINFOHEADER * bih;
    int w, h, topdown = 0, bpp;
    long stride, y;
    unsigned char savedpal[768];

    fpt = fopen(imname, "rb");
    if (fpt==NULL) return -1;
    fseek(fpt, 0, SEEK_END);
    filesize = ftell(fpt);
    fseek(fpt, 0, SEEK_SET);
    fbuf = malloc(filesize);
    if (fbuf==NULL) { fclose(fpt); return -1; }
    if (fread(fbuf, 1, (size_t) filesize, fpt)!=(size_t) filesize) { fclose(fpt); free(fbuf); return -1; }
    fclose(fpt);

    if (!strcmp(imname+strlen(imname)-4, ".png") ||
        !strcmp(imname+strlen(imname)-4, ".PNG") ||
        !strcmp(imname+strlen(imname)-4, ".Png")) {
        unsigned char * pngbuf = NULL; long pngsize = 0;
        if (IMDecodePNG(imname, &pngbuf, &pngsize)) { free(fbuf); return -1; }
        free(fbuf); fbuf = pngbuf; filesize = pngsize;
    }

    bfh = (BITMAPFILEHEADER *) fbuf;
    if (bfh->bfType==19778) {
        bih = (BITMAPINFOHEADER *) (fbuf+sizeof(BITMAPFILEHEADER));
        w = bih->biWidth; h = bih->biHeight;
        if (h<0) { h = -h; topdown = 1; }
        bpp = bih->biBitCount;
        if (bih->biCompression != BI_RGB || (bpp!=8 && bpp!=24) ||
            w<=0 || h<=0 || w>4096 || h>4096) { free(fbuf); return -1; }
        px = malloc(w*h*3);
        if (px==NULL) { free(fbuf); return -1; }
        if (bpp==24) {
            unsigned char * src = fbuf+bfh->bfOffBits;
            stride = ((w*3)+3)&(~3L);
            for (y=0;y<h;y++) {
                unsigned char * srow = topdown ? src+(h-1-y)*stride : src+y*stride;
                memcpy(px+y*w*3, srow, w*3);
            }
        } else {
            RGBQUAD * pal = (RGBQUAD *) (((char *) bih)+bih->biSize);
            unsigned char * src = fbuf+bfh->bfOffBits;
            stride = (w+3)&(~3L);
            for (y=0;y<h;y++) {
                unsigned char * srow = topdown ? src+(h-1-y)*stride : src+y*stride;
                int x;
                for (x=0;x<w;x++) {
                    px[(y*w+x)*3] = pal[srow[x]].rgbBlue;
                    px[(y*w+x)*3+1] = pal[srow[x]].rgbGreen;
                    px[(y*w+x)*3+2] = pal[srow[x]].rgbRed;
                }
            }
        }
        free(fbuf);
        *out = px; *iw = w; *ih = h;
        return 0;
    }

    /* try PCX (8/24-bit): decode_pcx writes cmappt, save/restore it */
    memcpy(savedpal, cmappt, 768);
    {
        unsigned char * mempt = fbuf;
        unsigned char * bmdatapt;
        int pw, ph, pbpp, pitch, x;
        mempt += 4;
        pw = -(((unsigned short *) mempt)[0]);
        ph = -(((unsigned short *) mempt)[1]);
        pw += (((unsigned short *) mempt)[2])+1;
        ph += (((unsigned short *) mempt)[3])+1;
        if (pw<=0 || ph<=0 || pw>4096 || ph>4096) { memcpy(cmappt, savedpal, 768); free(fbuf); return -1; }
        mempt += 12;
        for (x=0;x<16;x++) {
            cmappt[x*3] = ((unsigned char *) mempt)[0];
            cmappt[x*3+1] = ((unsigned char *) mempt)[1];
            cmappt[x*3+2] = ((unsigned char *) mempt)[2];
            mempt += 3;
        }
        pbpp = mempt[1]*8;
        if (pbpp!=8 && pbpp!=24) { memcpy(cmappt, savedpal, 768); free(fbuf); return -1; }
        mempt += 2;
        pitch = ((unsigned short *) mempt)[0];
        mempt += 62;
        bmdatapt = malloc((pbpp/8)*(pw+4)*ph);
        if (bmdatapt==NULL) { memcpy(cmappt, savedpal, 768); free(fbuf); return -1; }
        decode_pcx(mempt, filesize, bmdatapt, pw, ph, pbpp, pitch);
        px = malloc(pw*ph*3);
        if (px==NULL) { free(bmdatapt); memcpy(cmappt, savedpal, 768); free(fbuf); return -1; }
        stride = ((pw*(pbpp/8))+3)&-4;
        for (y=0;y<ph;y++) {
            unsigned char * srow = bmdatapt+y*stride;
            int xx;
            if (pbpp==8) {
                for (xx=0;xx<pw;xx++) {
                    px[(y*pw+xx)*3] = cmappt[srow[xx]*3+2];
                    px[(y*pw+xx)*3+1] = cmappt[srow[xx]*3+1];
                    px[(y*pw+xx)*3+2] = cmappt[srow[xx]*3];
                }
            } else {
                for (xx=0;xx<pw;xx++) {
                    px[(y*pw+xx)*3] = srow[xx*3];
                    px[(y*pw+xx)*3+1] = srow[xx*3+1];
                    px[(y*pw+xx)*3+2] = srow[xx*3+2];
                }
            }
        }
        free(bmdatapt);
        memcpy(cmappt, savedpal, 768);
        free(fbuf);
        *out = px; *iw = pw; *ih = ph;
        return 0;
    }
}

void decode_pcx(unsigned char * mempt, int memend, unsigned char * tomempt, int width, int height, int bpp, int pitch) {
    int c;
    int xx, po;
    int x, y;
    char ch;
    unsigned char * membasept, * bmparr;

    membasept = mempt;

    for (y=(height-1); y>=0; y--) {       /* read RLE encoded PCX data */
        if (bpp==8) po = 0; else po = 2;
        x = xx = 0;
        bmparr = tomempt;
        bmparr += (((width*(bpp/8))+3)&-4)*y;

        while (x < (pitch*(bpp/8))) {
            ch = mempt[0]; mempt++;
            if ((ch & 0xC0) == 0xC0) {
                c = (ch & 0x3F);
                ch = mempt[0]; mempt++;
            } else c = 1;

            if (bpp == 8) {
                while (c--) {
                    if (x < width)
                        bmparr[x] = ((BYTE) ch);
                    x++;
                }
            } else {
                while (c--) {
                    if (xx < width)
                        bmparr[xx*3+po] = ch;
                    x++;
                    //          po = 2;
                    if (x == pitch) {
                        xx = 0;
                        po = 1;
                    } else if (x == pitch*2) {
                        xx = 0;
                        po = 0;
                    } else
                    xx++;
                }
            }
        }
    }

    if (bpp == 8) {                  /* look for a 256 color palette */
        while (mempt<(membasept+memend)) {
            if (mempt[0]==12) {
                mempt++;
                for (c=0; c<256; c++) {
                    cmappt[c*3] = ((BYTE *) mempt)[0];
                    cmappt[c*3+1] = ((BYTE *) mempt)[1];
                    cmappt[c*3+2] = ((BYTE *) mempt)[2];
                    mempt += 3;
                }
                break;
            }
            mempt++;
        }
    }
}

int IMImportBlocks(char *imname, int imstyle)
/* If imstyle==-2, alt 8bit are imported */
/* If imstyle==-1, new block structures are created for each block */
/* If imstyle==0, all block graphics are imported */
/* if imstyle>0, imstyle block graphics are imported */
{
    int i, j, k, l, noblock, altnumb, isapcx, lastblockg, lastblocks;
    FILE * fpt;
    unsigned char * bmppt, * bmdatapt, * destpt, * srcpt, * srcpt2, * imcmappt, * mempt, * pcxfile;
    unsigned char red, green, blue;
    long int filesize;
    BITMAPFILEHEADER * bfh;
    BITMAPINFOHEADER * bmihpt;
    RGBQUAD * myrgbpt, * myrgbpt2;
    //RGBQUAD colour;

    if (blockgfxpt==NULL) return -1;

    fpt = fopen(imname, "rb");
    if (fpt == NULL) return -1;
    fseek(fpt, 0, SEEK_END);
    filesize = ftell(fpt);
    fseek(fpt, 0, SEEK_SET);

    bmppt = malloc(filesize);
    if (bmppt == NULL) { fclose(fpt); return -1; }
    if (fread(bmppt, 1, (size_t) filesize, fpt)!=(size_t) filesize) { fclose(fpt); free(bmppt); return -1; }
    fclose(fpt);

    isapcx = 0;
    pcxfile = NULL;
    if (!strcmp(imname+strlen(imname)-4, ".png") ||
        !strcmp(imname+strlen(imname)-4, ".PNG") ||
        !strcmp(imname+strlen(imname)-4, ".Png")) {
        /* runtime libpng bridge (LIBPNG13.DLL -> LIBPNG12.DLL) */
        unsigned char * pngbuf = NULL; long pngsize = 0;
        if (IMDecodePNG(imname, &pngbuf, &pngsize)) { free(bmppt); return -1; }
        free(bmppt); bmppt = pngbuf; filesize = pngsize;
    }
    if (!strcmp(imname+strlen(imname)-4, ".PCX") ||
        !strcmp(imname+strlen(imname)-4, ".pcx") ||
        !strcmp(imname+strlen(imname)-4, ".Pcx")) {

        isapcx = 1;
        pcxfile = bmppt;
        mempt = pcxfile;
        bmppt = malloc(sizeof(BITMAPINFOHEADER));
        if (bmppt == NULL) { free(pcxfile); return -1; }
        bmihpt = (BITMAPINFOHEADER *) (bmppt);

        mempt += 4;

        i = -(((unsigned short *) mempt)[0]);        /* xmin */
        j = -(((unsigned short *) mempt)[1]);       /* ymin */
        i += (((unsigned short *) mempt)[2]) + 1;      /* xmax */
        j += (((unsigned short *) mempt)[3]) + 1;     /* ymax */

        bmihpt->biWidth = i;
        bmihpt->biHeight = j;

        mempt += 12;						 /* skip DPI values */

        imcmappt = cmappt;
        for (k=0; k<16; k++) {           /* read the 16 colour palette */
            imcmappt[0] = ((BYTE *) mempt)[0];
            imcmappt[1] = ((BYTE *) mempt)[1];
            imcmappt[2] = ((BYTE *) mempt)[2];
            imcmappt += 3; mempt += 3;
        }

        bmihpt->biBitCount = mempt[1] * 8;          /* how many colour planes? */
        if ((bmihpt->biBitCount != 8) && (bmihpt->biBitCount != 24)) {
            free(bmppt);
            free(pcxfile);
            return -1;
        }

        mempt += 2;
        k = ((unsigned short *) mempt)[0];
        mempt += 62;

        bmdatapt = (unsigned char *) malloc((bmihpt->biBitCount/8)*(bmihpt->biWidth+4)*bmihpt->biHeight);
        if (bmdatapt == NULL) { free(bmppt); free(pcxfile); return -1; }

        decode_pcx(mempt, filesize, bmdatapt, bmihpt->biWidth, bmihpt->biHeight, bmihpt->biBitCount, k);
        //		memset (bmdatapt, 0xF0, bmihpt->biWidth*bmihpt->biHeight);
        altnumb = 0;

    } else {

        bfh = (BITMAPFILEHEADER *) bmppt;
        if (bfh->bfType!=19778) { free(bmppt); return -1; } /* 'BM' identifier 19778*/

        bmdatapt = bmppt + bfh->bfOffBits;
        bmihpt = (BITMAPINFOHEADER *) (bmppt + sizeof(BITMAPFILEHEADER));
        myrgbpt = (RGBQUAD *) (((char *) bmihpt) + bmihpt->biSize);

        if (bmihpt->biCompression != BI_RGB ||
            (bmihpt->biBitCount!=8 && bmihpt->biBitCount!=24)) {
            /* unknown/compressed graphics: MAPEXT.DLL MPYDecodeBMP hook */
            unsigned char * mxbuf = NULL;
            int mxw = bmihpt->biWidth, mxh = bmihpt->biHeight;
            if (mxh<0) mxh = -mxh;
            if (mxw<=0 || mxh<=0 || mxw>8192 || mxh>8192) { free(bmppt); return -1; }
            if (IMTryMapext(imname, bmppt, mxw, mxh, &mxbuf)) { free(bmppt); return -1; }
            free(bmppt); bmppt = mxbuf;
            bfh = (BITMAPFILEHEADER *) bmppt;
            if (bfh->bfType!=19778) { free(bmppt); return -1; }
            bmdatapt = bmppt + bfh->bfOffBits;
            bmihpt = (BITMAPINFOHEADER *) (bmppt + sizeof(BITMAPFILEHEADER));
            myrgbpt = (RGBQUAD *) (((char *) bmihpt) + bmihpt->biSize);
            if (bmihpt->biCompression != BI_RGB) { free(bmppt); return -1; }
        }

        altnumb = 0;
        if (imstyle == -2 && bmihpt->biBitCount!=8) { free(bmppt); return -1; }

        if (blockdepth==8 && bmihpt->biBitCount!=8) { free(bmppt); return -1; }

        if (bmihpt->biBitCount!=8 && bmihpt->biBitCount!=24) { free(bmppt); return -1; }
        j = bmihpt->biClrUsed; if (!j) j = 256;
        if ((cmappt!=NULL && bmihpt->biBitCount==8) || (imstyle == -2 && bmihpt->biBitCount==8)) {
            imcmappt = cmappt; myrgbpt2 = myrgbpt;
            for (i=0;i<j;i++) {
                imcmappt[0] = myrgbpt2->rgbRed;
                imcmappt[1] = myrgbpt2->rgbGreen;
                imcmappt[2] = myrgbpt2->rgbBlue;
                imcmappt += 3; myrgbpt2 ++;
            }
        }

    }

    lastblockg = numblockgfx; lastblocks = numblockstr;

    switch (bmihpt->biBitCount) {
    case 8:
        fgcolour = 0; j = 0;
        for (i=0;i<256;i++) {
            k = (int) (((int) (cmappt[i*3])) + ((int) (cmappt[(i*3)+1])) + ((int) (cmappt[(i*3)+2])));
            if (j<k) { j = k; fgcolour = i; }
        }

        bmihpt->biWidth+=3; bmihpt->biWidth&=-4;
        srcpt = bmdatapt + (bmihpt->biWidth*bmihpt->biHeight);
        srcpt2 = srcpt;
        j = bmihpt->biHeight; j -= blockheight; while (j>=0) {
            srcpt = srcpt2;
            i = bmihpt->biWidth/blockwidth; while (i) {
                if (imstyle == -2)
                    destpt = (unsigned char *) altgfxpt + (altnumb*blockwidth*blockheight);
                else
                    destpt = (unsigned char *) blockgfxpt + (numblockgfx*blockwidth*blockheight*((blockdepth+1)/8));
                noblock = 0;
                k = blockheight; while (k) {
                    srcpt -= bmihpt->biWidth;
                    l = blockwidth; while (l) {
                        if (blockdepth == 8 || imstyle == -2) { destpt[0] = srcpt[0]; if (destpt[0]!=0) noblock = 1; destpt++; } else {
                            red = cmappt[((int)(*srcpt))*3];
                            green = cmappt[((int)(*srcpt))*3+1];
                            blue = cmappt[((int)(*srcpt))*3+2];
                            if (red || green || blue) noblock = 1;
                            switch (blockdepth) {
                            case 15:
                                destpt[0] = (red&0xF8)>>1;
                                destpt[0] |= (green&0xC0)>>6;
                                destpt[1] = (green&0x38)<<2;
                                destpt[1] |= (blue&0xF8)>>3;
                                destpt += 2;
                                break;
                            case 16:
                                destpt[0] = (red&0xF8);
                                destpt[0] |= (green&0xE0)>>5;
                                destpt[1] = (green&0x1C)<<3;
                                destpt[1] |= (blue&0xF8)>>3;
                                destpt += 2;
                                break;
                            case 24:
                                destpt[0] = red;
                                destpt[1] = green;
                                destpt[2] = blue;
                                destpt += 3;
                                break;
                            case 32:
                                destpt[0] = 0;
                                destpt[1] = red;
                                destpt[2] = green;
                                destpt[3] = blue;
                                destpt += 4;
                                break;
                        } }
                        srcpt++; l--;
                    }
                    srcpt -= blockwidth; k--;
                }
                if (noblock || !imskip || (imskip == 2 && numblockgfx > 1)) {
                    if (imstyle==-1 && (numblockstr < MAXBLKSTR)) { memset(blockstrpt+(numblockstr*SOBSTR), 0, SOBSTR);
                        *((long int *) (blockstrpt+(numblockstr*SOBSTR))) = numblockgfx*blockwidth*blockheight*((blockdepth+1)/8);
                        numblockstr++; }
                    if (imstyle == -2) {
                        altnumb++;
                        if (altnumb == numblockgfx) { free(bmppt); if (isapcx) { free(bmdatapt); free(pcxfile); } return 0; }
                    } else {
                        numblockgfx++;
                    }
                    if (noblock) { lastblockg = numblockgfx; lastblocks = numblockstr; }
                    if (imstyle == 1) { free(bmppt); if (isapcx) { free(bmdatapt); free(pcxfile); } return 0; }
                    if (imstyle > 1) imstyle--;
                }
                srcpt += (bmihpt->biWidth*blockheight);
                srcpt += blockwidth;
                i--;
            }
            srcpt2 -= (bmihpt->biWidth*blockheight);
            j -= blockheight;
        }
        if (imskip == 2) { numblockgfx = lastblockg; numblockstr = lastblocks; }
        break;
    case 24:
        if (blockdepth == 8) { free(bmppt); if (isapcx) { free(bmdatapt); free(pcxfile); } return -1; }
        bmihpt->biWidth*=3; bmihpt->biWidth+=3; bmihpt->biWidth&=-4;
        srcpt = bmdatapt + (bmihpt->biWidth*bmihpt->biHeight);
        srcpt2 = srcpt;
        j = bmihpt->biHeight; j -= blockheight; while (j>=0) {
            srcpt = srcpt2;
            i = (bmihpt->biWidth/3)/blockwidth; while (i) {
                destpt = (unsigned char *) blockgfxpt + (numblockgfx*blockwidth*blockheight*((blockdepth+1)/8));
                noblock = 0;
                k = blockheight; while (k) {
                    srcpt -= bmihpt->biWidth;
                    l = blockwidth; while (l) {
                        if (srcpt[0] || srcpt[1] || srcpt[2]) noblock = 1;
                        switch (blockdepth) {
                        case 15:
                            destpt[0] = (srcpt[2]&0xF8)>>1;
                            destpt[0] |= (srcpt[1]&0xC0)>>6;
                            destpt[1] = (srcpt[1]&0x38)<<2;
                            destpt[1] |= (srcpt[0]&0xF8)>>3;
                            destpt += 2;
                            break;
                        case 16:
                            destpt[0] = (srcpt[2]&0xF8);
                            destpt[0] |= (srcpt[1]&0xE0)>>5;
                            destpt[1] = (srcpt[1]&0x1C)<<3;
                            destpt[1] |= (srcpt[0]&0xF8)>>3;
                            destpt += 2;
                            break;
                        case 24:
                            destpt[0] = srcpt[2];
                            destpt[1] = srcpt[1];
                            destpt[2] = srcpt[0];
                            destpt += 3;
                            break;
                        case 32:
                            destpt[0] = 0;
                            destpt[1] = srcpt[2];
                            destpt[2] = srcpt[1];
                            destpt[3] = srcpt[0];
                            destpt += 4;
                            break;
                        }
                        srcpt+=3; l--;
                    }
                    srcpt -= blockwidth*3; k--;
                }
                if (noblock || !imskip || (imskip == 2 && numblockgfx > 1)) {
                    if (imstyle==-1 && (numblockstr < MAXBLKSTR)) { memset(blockstrpt+(numblockstr*SOBSTR), 0, SOBSTR);
                        *((long int *) (blockstrpt+(numblockstr*SOBSTR))) = numblockgfx*blockwidth*blockheight*((blockdepth+1)/8);
                        numblockstr++; }
                    numblockgfx++;
                    if (noblock) { lastblockg = numblockgfx; lastblocks = numblockstr; }
                    if (imstyle==1) { free(bmppt); if (isapcx) { free(bmdatapt); free(pcxfile); } return 0; }
                    if (imstyle>1) imstyle--;
                }
                srcpt += (bmihpt->biWidth*blockheight);
                srcpt += blockwidth*3;
                i--;
            }
            srcpt2 -= (bmihpt->biWidth*blockheight);
            j -= blockheight;
        }
        if (imskip == 2) { numblockgfx = lastblockg; numblockstr = lastblocks; }
        break;
    }

    if (isapcx) { free(bmdatapt); free(pcxfile); }
    free(bmppt);
    if (imstyle == -2) return -1;
    return 0;
}