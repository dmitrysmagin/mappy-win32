/* A tiley map editor for general use */
/* Started 28/9/97 by Robin Burrows (C)1997 */
/* This version 1.23 released ???? (C)2001 */
/* Feel free to alter or use pieces of this for anything you like, but if you use a */
/* substantial portion, you MUST give me a credit in whatever you used it for. */

/* See readsrctxt for more info. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "fmp.h"
#include "fmp14.h"
#include "tileset.h"
#include "compat.h"
#include "mapdefs.h"

int IMLoadImage24(char * imname, unsigned char ** out, int * iw, int * ih);

#define MAPVERH 1
#define MAPVERL 0
#define BKDTMEM (32768*32)
#define ANDTMEM (65536*4)
#define ABUFMEM (2048*8)
#define BKGFXMEM (1048576*8)
#define BLOCKSTRSIZE 32
#define MAXBRSH 16
#define MAPFILL_MAX 10000

#define ER_NONE 0
#define ER_MAPLOADERROR 1
#define ER_TOOMANYPAR 2
#define ER_MAPTOONEW 3
#define ER_OUTOFMEM 4
#define ER_NOOPEN 5
#define ER_MAPSAVEERROR 6

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

/* Prototypes (Is that what they're called?) */
void DrawTheMap(void);
void ShowCoords(void);
void RenderBlockIcons(void);
void ShowBlockGfxGraphics(int, int, int);

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

typedef struct {	/* Generic structure for chunk headers */
    char id1, id2, id3, id4;	/* 4 byte header id. */
    long int headsize;		/* size of header chunk. */
} GENHEAD;

typedef struct {	/* Map header structure */
    /* char M,P,H,D;	4 byte chunk identification. */
    /* long int mphdsize;	size of map header. */
    char mapverhigh;	/* map version number to left of . (ie X.0). */
    char mapverlow;		/* map version number to right of . (ie 0.X). */
    char lsb;		/* if 1, data stored LSB first, otherwise MSB first. */
    char maptype;	/* 0 for 32 offset still, -16 offset anim shorts in BODY added FMP0.5*/
    short int mapwidth;	/* width in blocks. */
    short int mapheight;	/* height in blocks. */
    short int reserved1;
    short int reserved2;
    short int blockwidth;	/* width of a block (tile) in pixels. */
    short int blockheight;	/* height of a block (tile) in pixels. */
    short int blockdepth;	/* depth of a block (tile) in planes (ie. 256 colours is 8) */
    short int blockstrsize;	/* size of a block data structure */
    short int numblockstr;	/* Number of block structures in BKDT */
    short int numblockgfx;	/* Number of 'blocks' in graphics (BODY) */
    unsigned char ckey8bit, ckeyred, ckeygreen, ckeyblue; /* colour key values added FMP0.4*/
    /* info for non rectangular block maps added FMP0.5*/
    short int blockgapx, blockgapy, blockstaggerx, blockstaggery;
    short int clickmask, pillars;
} MPHD;

typedef struct {	/* Editor prefs structure */
    /* char E,D,H,D;	4 byte chunk identification. */
    /* long int edhdsize;	size of editor header. */
    short int xmapoffset;	/* editor offset, in blocks, from left. */
    short int ymapoffset;	/* editor offset, in blocks, from right. */
    long int fgcolour;	/* fg colour for text, buttons etc. */
    long int bgcolour;	/* bg colour for text, buttons etc. */
    short int swidth;		/* width of current screen res */
    short int sheight;	/* height of current screen res */
    short int strtstr;	/* first structure in view */
    short int strtblk;	/* first block graphic in view */
    short int curstr;		/* current block structure */
    short int curanim;	/* current anim structure */
    short int animspd;	/* gap in frames between anims */
    short int span;		/* control panel height */
    short int numbrushes;	/* number of brushes to follow. */
    short int pad;
} EDHD;

typedef struct { /* Info for MapFill function */
    short int leftedge, width, yoff;
} MAPFILL;

/* Arch-safety: in-memory structs must match historic 32-bit layout.
 * File I/O below NEVER dumps these structs directly; it uses fmp.h
 * explicit encoders with fixed disk sizes. Asserts catch drift. */
#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(FMP_BLK_MEMUNIT == 32, "BLKSTR must stay 32 bytes (check long/bitfield packing)");
_Static_assert(FMP_ANI_MEMUNIT == 16, "ANISTR must stay 16 bytes");
_Static_assert(sizeof(MPHD) == 40, "MPHD must stay 40 bytes");
_Static_assert(sizeof(EDHD) == 32, "EDHD must stay 32 bytes");
_Static_assert(sizeof(GENHEAD) == 8, "GENHEAD must stay 8 bytes");
#endif

/* Unknown-chunk list: arch-safe (no pointer hidden inside file image).
 * Legacy code stored a native pointer + raw chunk bytes in one malloc block,
 * which breaks on 64-bit (pointer size 8 vs 4) and on BE hosts. */
typedef struct FmpUnknownChunk {
    struct FmpUnknownChunk *next;
    char id[4];
    uint32_t size; /* chunk body size (host order) */
    /* body bytes follow */
} FmpUnknownChunk;

unsigned char trans16hi, trans16low, transred, transgreen, transblue, trans8bit;
short int sheight, swidth, sdepth, merror, mapwidth, mapheight, strtstr, strtblk, strtanim;
short int blockwidth, blockheight, blockdepth, blockstrsize, animspd, gridon, zoomon, maplayer;
short int xmapoffset, ymapoffset, span, numblockstr, numblockgfx, numblockanim, numbrushes;
short int curstr, curanim, oldxoff, oldyoff, oldblockx, oldblocky, curbrsh, revealanims, pastebrushnt;
int brushhandle = 0;	/* paste origin corner: 0=TL 1=TR 2=BL 3=BR */
long int bgcolour, fgcolour, curplacetile;
/* new for V1.23 */
int mapislsb, maptype;
int blockgapx, blockgapy, blockstaggerx, blockstaggery;
int clickmask, picklayer, picklx, pickly;
char * altgfxpt = NULL;
char novctext[70];
char mapfilename[256];
char gfxfilename[256];
FILE *filept;
GENHEAD genheader; /* kept for compat; NOT written directly (see fmp.h) */
FmpUnknownChunk *chunklist = NULL;
int isopillars = 0, isoreveal = 0, flippedtiles = 0;
int wnesmode = 0, rotatedtiles = 0;
int resize_left = 0, resize_top = 0;	/* 1.4.13 New Left / New Top */

int MapGetCurLayer(void);	/* mapwin.c */
void MapSetCurLayer(int ly);
void ApplyEditorPrefs(void);
int SaveProjectEx(int skipgfx);
extern int onion, oniondark, onionlower;
static int map_keepgfx = 0;	/* FMA reload: keep current BGFX */
extern int showuser, divideron, dividergapx, dividergapy;
extern long dividercol;

short int * mappt = NULL;
char * blockgfxpt = NULL;
char * blockstrpt = NULL;
char * animstrpt = NULL;
unsigned char * cmappt = NULL;
short int * brshpt[MAXBRSH]= {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
char brshname[MAXBRSH][64]= {
    "Brush", "Brush", "Brush", "Brush",
    "Brush", "Brush", "Brush", "Brush",
    "Brush", "Brush", "Brush", "Brush",
    "Brush", "Brush", "Brush", "Brush" };
short int * mapmappt[MAXMAPLAYERS]={ NULL };

int mapaltered;

char *errorstring[] = {
    "No error",
    "Couldn't load map file (missing, corrupt or not a map file?)",
    "Usage: Mappy or Mappy [mapfile]",
    "Map file too new, get a newer version of this editor",
    "Out of memory!",
    "Can't open file, invalid filename?",
    "Couldn't save map file (out of disk space, or empty map?)"
};

char *eybs = "Please edit your block structure";

char authorname[70];
char authorinfo1[70];
char authorinfo2[70];
char authorinfo3[70];
char bwidths[16];
char bheights[16];
char mwidths[16];
char mheights[16];
char bslong1[10];
char bslong2[10];
char bsword1[6];
char bsword2[6];
char bsbyte1[4];
char bsbyte2[4];
char bsbyte3[4];

char mynull [2] = { 0, 0 };

BLKSTR blkbuffer;
char * animbufferpt;

//FILE * dbgfpt = NULL;
//pragma warn -sig
int logit(char * str, ...) {
    UNUSED(str);
    return 0;
    /*
va_list argptr;
int cnt = 0;

    va_start(argptr, str);
    if (dbgfpt != NULL)
        cnt = vfprintf(dbgfpt, str, argptr);
    va_end(argptr);
    fflush (dbgfpt);

    return(cnt);
*/
}

void InsertBytes(char * inpt, char * endpt, int amount) {
    char * endpt2 = endpt;
    endpt2 -= amount;
    while (endpt2!=inpt) { endpt--; endpt2--; *endpt = *endpt2; }
    while (amount) { *endpt2 = 0; endpt2++; amount--; }
}

void RemoveBytes(char * inpt, char * endpt, int amount) {
    char * inpt2 = inpt;
    inpt2 += amount;
    while (inpt2!=endpt) { *inpt = *inpt2; inpt++; inpt2++; }
}

void InsertBytesR(char * inpt, char * endpt, int amount) {
    char * inpt2 = inpt;
    inpt2 += amount;
    while (inpt2!=endpt) { *inpt = *inpt2; inpt++; inpt2++; }
    while (amount) { *inpt = 0; inpt++; amount--; }
}

void RemoveBytesR(char * inpt, char * endpt, int amount) {
    char * endpt2 = endpt;
    endpt2 -= amount;
    while (endpt2!=inpt) { endpt--; endpt2--; *endpt = *endpt2; }
}
unsigned long int byteswapl(unsigned long int i) {
    unsigned long int j;
    j = i&0xFF; j <<= 8; i >>= 8; j |= i&0xFF; j <<= 8; i >>= 8;
    j |= i&0xFF; j <<= 8; i >>= 8; j |= i&0xFF; return j;
}

unsigned short int byteswaps(unsigned short int i) {
    unsigned short int j;
    j = i&0xFF; j <<= 8; i >>= 8; j |= i&0xFF; return j;
}

void SetTranspColour(void) {
    switch (blockdepth) {
    case 15:
        trans16hi = ((transred>>1)&0x7C)|((transgreen>>6)&0x03);
        trans16low = ((transgreen<<2)&0xE0)|((transblue>>3)&0x1F);
        break;
    case 16:
        trans16hi = (transred&0xF8)|((transgreen>>5)&0x07);
        trans16low = ((transgreen<<3)&0xE0)|((transblue>>3)&0x1F);
        break;
    default:
        trans16hi = 0; trans16low = 0;
        break;
    }
}

char * GetBrushName(int bnum) {
    return brshname[bnum];
}

short int * GetLayerpt(int layernum) {
    if (layernum<0 || layernum>=MAXMAPLAYERS) return NULL;
    return mapmappt[layernum];
}

int PickLayer(void) {
    int i;

    if (picklayer<0 || picklayer>=MAXMAPLAYERS) return -1;
    if (mapmappt[picklayer]==NULL) return -1;
    mappt = mapmappt[picklayer];
    i = maplayer; maplayer = picklayer;	picklayer = i;
    i = picklx; picklx = xmapoffset; xmapoffset = i;
    i = pickly; pickly = ymapoffset; ymapoffset = i;
    return 0;
}

int ChangeLayer(int newlayer) {
    if (newlayer<0 || newlayer>=MAXMAPLAYERS) return -1;
    if (mapmappt[newlayer]==NULL) return -1;
    mappt = mapmappt[newlayer]; maplayer = newlayer;
    return 0;
}

int DeleteMapLayer(void) {
    int i;

    if (maplayer==0 && mapmappt[1]==NULL) return -1;
    if (mapmappt[maplayer]!=NULL) free(mapmappt[maplayer]);
    for (i=maplayer;i<MAXMAPLAYERS-1;i++) mapmappt[i] = mapmappt[i+1];
    mapmappt[MAXMAPLAYERS-1] = NULL;
    if (maplayer>0) maplayer--;
    mappt = mapmappt[maplayer];
    return maplayer;
}

int CreateMapLayer(void) {
    int i;
    short int * np;

    if (mapmappt[MAXMAPLAYERS-1]!=NULL) return -1;
    if (maplayer+1>=MAXMAPLAYERS) return -1;
    np = (short int *) malloc((size_t)mapwidth*(size_t)mapheight*2);
    if (np==NULL) return -1;
    memset(np, 0, (size_t)mapwidth*(size_t)mapheight*2);
    maplayer++;
    for (i=MAXMAPLAYERS-1;i>maplayer;i--) mapmappt[i] = mapmappt[i-1];
    mapmappt[maplayer] = np;
    return maplayer;
}

int DuplicateMapLayer(void) {
    int i;
    short int * np;

    if (mapmappt[MAXMAPLAYERS-1]!=NULL) return -1;
    if (mapmappt[maplayer]==NULL) return -1;
    if (maplayer+1>=MAXMAPLAYERS) return -1;
    np = (short int *) malloc((size_t)mapwidth*(size_t)mapheight*2);
    if (np==NULL) return -1;
    for (i=MAXMAPLAYERS-1;i>maplayer+1;i--) mapmappt[i] = mapmappt[i-1];
    maplayer++;
    mapmappt[maplayer] = np;
    memcpy(np, mapmappt[maplayer-1], (size_t)mapwidth*(size_t)mapheight*2);
    return maplayer;
}

void FreeAll(void) {
    int i;
    FmpUnknownChunk *tmp;

    mapaltered = 0;
    if (blockgfxpt!=NULL) { free(blockgfxpt); blockgfxpt = NULL; }
    if (altgfxpt!=NULL) { free(altgfxpt); altgfxpt = NULL; }
    if (blockstrpt!=NULL) { free(blockstrpt); blockstrpt = NULL; }
    if (animstrpt!=NULL) { free(animstrpt); animstrpt = NULL; }
    if (cmappt!=NULL) { free(cmappt); cmappt = NULL; }
    for (i=0;i<MAXMAPLAYERS;i++) {
        if (mapmappt[i]!=NULL) { free(mapmappt[i]); mapmappt[i] = NULL; }
    }
    mappt = NULL;

    if (animbufferpt!=NULL) { free(animbufferpt); animbufferpt = NULL; }
    for (i=0;i<MAXBRSH;i++) {
        if (brshpt[i]!=NULL) { free(brshpt[i]); brshpt[i] = NULL; }
        strcpy(brshname[i], "Brush");
    }

    while (chunklist!=NULL) {
        tmp = chunklist->next;
        free(chunklist);
        chunklist = tmp;
    }
    F14_FreeAll();  /* 1.4 chunks: EPHD/TSTR/LSTR/OBDT/OBFN */
    TS_FreeAll();   /* TSDF/TSIN tilesets */

    curstr = 0; numblockstr = 0; numblockgfx = 0;
    curbrsh = 0; numbrushes = 0; mapwidth = 0; mapheight = 0; strtanim = 0;
    maptype = 0;
    blockwidth = 0; blockheight = 0; blockdepth = 0; blockstrsize = BLOCKSTRSIZE;
    fgcolour = 255; bgcolour = 0; curanim = -1; animspd = 0; numblockanim = 0;
    maplayer = 0; authorname[0] = 0; xmapoffset = 0; ymapoffset = 0;
    swidth = 640; sheight = 480; span = 32; strtstr = 0; strtblk = 0;
    authorinfo1[0] = 0; authorinfo2[0] = 0; authorinfo3[0] = 0;
    novctext[0] = 0;
    blockgapx = 0; blockgapy = 0; blockstaggerx = 0; blockstaggery = 0;
    clickmask = 0;

    return;
}

int CreateNewMap(int newmw, int newmh, int newmd, int newbw, int newbh, int newbgx, int newbgy, int newbsx, int newbsy, int newmt) {
    FreeAll();

    cmappt = (unsigned char *) malloc(3*256);
    if (cmappt==NULL) { FreeAll(); return -1; }
    *cmappt=0; *(cmappt+1)=0; *(cmappt+2)=0;
    *(cmappt+(256*3)-3)=255; *(cmappt+(256*3)-2)=255; *(cmappt+(256*3)-1)=255;

    blockgfxpt = (void *) malloc(BKGFXMEM);
    if (blockgfxpt==NULL) { FreeAll(); return -1; }
    blockstrpt = (void *) malloc(BKDTMEM);
    if (blockstrpt==NULL) { FreeAll(); return -1; }
    if (newmw < 1 || newmh < 1) { FreeAll(); return -1; }
    mappt = (short int *) malloc((size_t)newmw*(size_t)newmh*2);
    if (mappt==NULL) { FreeAll(); return -1; }
    mapmappt[0] = mappt; maplayer = 0;

    if (newmd==15) memset(blockgfxpt, 0, (newbw*newbh*2));
    else memset(blockgfxpt, 0, (newbw*newbh*(newmd/8)));

    memset(blockstrpt, 0, BLOCKSTRSIZE);
    //	memset (animstrpt, 0, ANDTMEM); *(animstrpt+ANDTMEM-FMP_ANI_MEMUNIT)=-1;
    memset((char *) mappt, 0, (newmw*newmh*2));

    *(blockstrpt+BLOCKSTRSIZE)=-1;
    *(blockstrpt+BLOCKSTRSIZE+1)=-1;
    //	*(animstrpt+ANDTMEM-FMP_ANI_MEMUNIT)=-1;

    numblockgfx=1; numblockstr=1; strtstr=0; strtblk=0;
    curstr=0; curanim=-1; xmapoffset = 0; ymapoffset = 0;
    mapwidth = newmw; mapheight = newmh; blockwidth = newbw; blockheight = newbh;
    swidth = 640; sheight = 480; sdepth = newmd;
    blockdepth = sdepth; fgcolour = 255, bgcolour = 0;
    if (sdepth==15) fgcolour = 0x7fff;
    if (sdepth==16) fgcolour = 0xffff;
    if (sdepth==24) fgcolour = 0xffffff;
    if (sdepth==32) fgcolour = 0xffffff;
    blockgapx = newbgx; blockgapy = newbgy;
    blockstaggerx = newbsx; blockstaggery = newbsy;
    maptype = newmt; mapislsb = 1;

    mapfilename[0] = 0;
    SetTranspColour();
    return 0;
}

void InitAnims(void) {
    ANISTR * myanpt;
    if (animstrpt==NULL) return;
    numblockanim = 0;
    myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
    while (myanpt->antype!=AN_END) {
        numblockanim++;

        if (myanpt->antype==AN_PPFR) myanpt->antype = AN_PPFF;
        if (myanpt->antype==AN_PPRF) myanpt->antype = AN_PPRR;
        if (myanpt->antype==AN_ONCES) myanpt->antype = AN_ONCE;
        if ((myanpt->antype==AN_LOOPR) || (myanpt->antype==AN_PPRR)) {
            myanpt->ancuroff = myanpt->anstartoff;
            if ((myanpt->anstartoff)!=(myanpt->anendoff)) myanpt->ancuroff=(myanpt->anendoff)-4;
        } else {
            myanpt->ancuroff = myanpt->anstartoff;
        }
        myanpt->ancount = myanpt->andelay;
        myanpt--;
    }
}

void UpdateAnims(void) {
    ANISTR * myanpt;

    if (animstrpt==NULL) return;
    myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
    while (myanpt->antype!=-1) {
        if (myanpt->antype!=AN_NONE) { myanpt->ancount--; if (myanpt->ancount<0) {
                myanpt->ancount = myanpt->andelay;
                if (myanpt->antype==AN_LOOPF) {
                    if (myanpt->anstartoff!=myanpt->anendoff) { myanpt->ancuroff+=4;
                        if (myanpt->ancuroff==myanpt->anendoff) myanpt->ancuroff = myanpt->anstartoff;
                } }
                if (myanpt->antype==AN_LOOPR) {
                    if (myanpt->anstartoff!=myanpt->anendoff) { myanpt->ancuroff-=4;
                        if (myanpt->ancuroff==((myanpt->anstartoff)-4))
                            myanpt->ancuroff = (myanpt->anendoff)-4;
                } }
                if (myanpt->antype==AN_ONCE) {
                    if (myanpt->anstartoff!=myanpt->anendoff) { myanpt->ancuroff+=4;
                        if (myanpt->ancuroff==myanpt->anendoff) { myanpt->antype = AN_ONCES;
                            myanpt->ancuroff = myanpt->anstartoff; }
                } }
                if (myanpt->antype==AN_ONCEH) {
                    if (myanpt->anstartoff!=myanpt->anendoff) {
                        if (myanpt->ancuroff!=((myanpt->anendoff)-4)) myanpt->ancuroff+=4;
                } }
                if (myanpt->antype==AN_PPFF) {
                    if (myanpt->anstartoff!=myanpt->anendoff) { myanpt->ancuroff+=4;
                        if (myanpt->ancuroff==myanpt->anendoff) { myanpt->ancuroff -= 8;
                            myanpt->antype = AN_PPFR;
                            if (myanpt->ancuroff<myanpt->anstartoff) myanpt->ancuroff +=4; }
                } } else {
                    if (myanpt->antype==AN_PPFR) {
                        if (myanpt->anstartoff!=myanpt->anendoff) { myanpt->ancuroff-=4;
                            if (myanpt->ancuroff==((myanpt->anstartoff)-4)) { myanpt->ancuroff += 8;
                                myanpt->antype = AN_PPFF;
                                if (myanpt->ancuroff>myanpt->anendoff) myanpt->ancuroff -=4; }
                } } }
                if (myanpt->antype==AN_PPRR) {
                    if (myanpt->anstartoff!=myanpt->anendoff) { myanpt->ancuroff-=4;
                        if (myanpt->ancuroff==((myanpt->anstartoff)-4)) { myanpt->ancuroff += 8;
                            myanpt->antype = AN_PPRF;
                            if (myanpt->ancuroff>myanpt->anendoff) myanpt->ancuroff -=4; }
                } } else {
                    if (myanpt->antype==AN_PPRF) {
                        if (myanpt->anstartoff!=myanpt->anendoff) { myanpt->ancuroff+=4;
                            if (myanpt->ancuroff==myanpt->anendoff) { myanpt->ancuroff -= 8;
                                myanpt->antype = AN_PPRR;
                                if (myanpt->ancuroff<myanpt->anstartoff) myanpt->ancuroff +=4; }
                } } }
        } } myanpt--; }
}

int MapGetchksz(const void * locpt) {
    const unsigned char * p = (const unsigned char *) locpt;
    return ((((int) (p[0]))<<24)|(((int) (p[1]))<<16)|
        (((int) (p[2]))<<8)|((int) (p[3])));
}

static int chunk_is(const unsigned char * p, const char * id, size_t n) {
    return memcmp(p, id, n) == 0;
}

void MapPutshort(unsigned char * locpt, int shrt) {
    if (mapislsb) {
        locpt[0] = (unsigned char) ((shrt)&0xFF);
        locpt[1] = (unsigned char) ((shrt>>8)&0xFF);
    } else {
        locpt[0] = (unsigned char) ((shrt>>8)&0xFF);
        locpt[1] = (unsigned char) ((shrt)&0xFF);
    }
}

void MapPutlong(unsigned char * locpt, int lng) {
    if (mapislsb) {
        locpt[0] = (unsigned char) ((lng)&0xFF);
        locpt[1] = (unsigned char) ((lng>>8)&0xFF);
        locpt[2] = (unsigned char) ((lng>>16)&0xFF);
        locpt[3] = (unsigned char) ((lng>>24)&0xFF);
    } else {
        locpt[0] = (unsigned char) ((lng>>24)&0xFF);
        locpt[1] = (unsigned char) ((lng>>16)&0xFF);
        locpt[2] = (unsigned char) ((lng>>8)&0xFF);
        locpt[3] = (unsigned char) ((lng)&0xFF);
    }
}

int MapGetshort(unsigned char * locpt) {
    int rval;

    if (mapislsb)
        rval = ((((int) (locpt[1]))<<8)|((int) (locpt[0])));
    else
        rval = ((((int) (locpt[0]))<<8)|((int) (locpt[1])));
    if (rval & 0x8000) rval -= 0x10000;
    return rval;
}

int MapGetlong(unsigned char * locpt) {
    if (mapislsb)
        return ((((int) (locpt[3]))<<24)|(((int) (locpt[2]))<<16)|
        (((int) (locpt[1]))<<8)|((int) (locpt[0])));
    else
        return ((((int) (locpt[0]))<<24)|(((int) (locpt[1]))<<16)|
        (((int) (locpt[2]))<<8)|((int) (locpt[3])));
}

int DecodeMPHD(unsigned char * mdatpt) {
    logit("MPHD: %d bytes.\n", MapGetchksz(mdatpt+4));

    mdatpt += 8;
    if (mdatpt[0] > 1) { merror = ER_MAPTOONEW; return -1; }
    if (mdatpt[2] == 1) mapislsb = 1; else mapislsb = 0;

    maptype = (int) mdatpt[3];
    logit("Maptype=%d\n", maptype);
    if (maptype > 3) { merror = ER_MAPTOONEW; return -1; }
    mapwidth = (short) MapGetshort(mdatpt+4);
    mapheight = (short) MapGetshort(mdatpt+6);
    blockwidth = (short) MapGetshort(mdatpt+12);
    blockheight = (short) MapGetshort(mdatpt+14);
    blockdepth = (short) MapGetshort(mdatpt+16);
    blockstrsize = (short) MapGetshort(mdatpt+18);
    numblockstr = (short) MapGetshort(mdatpt+20);
    numblockgfx = (short) MapGetshort(mdatpt+22);
    if (mapwidth < 1 || mapheight < 1 || mapwidth > 10000 || mapheight > 10000) { merror = ER_MAPLOADERROR; return -1; }
    if (blockstrsize < 1) { merror = ER_MAPLOADERROR; return -1; }

    if (MapGetchksz(mdatpt-4) > 24) {
        transred = mdatpt[25];
        transgreen = mdatpt[26];
        transblue = mdatpt[27];
        trans8bit = mdatpt[24];
        if (blockdepth > 8) SetTranspColour();
    }
    if (MapGetchksz(mdatpt-4) > 28) {
        blockgapx = (int) MapGetshort(mdatpt+28);
        blockgapy = (int) MapGetshort(mdatpt+30);
        blockstaggerx = (int) MapGetshort(mdatpt+32);
        blockstaggery = (int) MapGetshort(mdatpt+34);
    } else {
        blockgapx = (int) blockwidth;
        blockgapy = (int) blockheight;
        blockstaggerx = 0;
        blockstaggery = 0;
    }
    if (MapGetchksz(mdatpt-4) > 36) clickmask = (short) MapGetshort(mdatpt+36);
    else clickmask = 0;
    if (MapGetchksz(mdatpt-4) > 38) isopillars = (short) MapGetshort(mdatpt+38);
    else isopillars = 0;
    /* 1.4.3 tilemodes packed in reserved1 (offset +8) */
    if (MapGetchksz(mdatpt-4) > 10) {
        int tm = MapGetshort(mdatpt+8);
        if (tm) {
            flippedtiles = (tm & 1) ? 1 : 0;
            rotatedtiles = (tm & 2) ? 1 : 0;
            wnesmode = (tm & 4) ? 1 : 0;
        }
    }

    logit("mapwidth=%d\nmapheight=%d\nblockwidth=%d\nblockheight=%d\nblockdepth=%d\nblockstrsize=%d\nnumblockstr=%d\nnumblockgfx=%d\n",
        mapwidth, mapheight, blockwidth, blockheight, blockdepth, blockstrsize, numblockstr, numblockgfx);

    return 0;
}

int DecodeEDHD(unsigned char * mdatpt) {
    int i, j, k;
    unsigned char * mybrshpt;
    short int * mybrsh2pt;
    char * mynamept;

    logit("EDHD: %d bytes.\n", MapGetchksz(mdatpt+4));
    mdatpt += 8;
    xmapoffset = (short) MapGetshort(mdatpt);
    ymapoffset = (short) MapGetshort(mdatpt+2);
    fgcolour = (int) MapGetlong(mdatpt+4);
    bgcolour = (int) MapGetlong(mdatpt+8);
    swidth = (short) MapGetshort(mdatpt+12);
    sheight = (short) MapGetshort(mdatpt+14);
    strtstr = (short) MapGetshort(mdatpt+16);
    strtblk = (short) MapGetshort(mdatpt+18);
    curstr = (short) MapGetshort(mdatpt+20);
    curanim = (short) MapGetshort(mdatpt+22); curanim = -1;
    animspd = (short) MapGetshort(mdatpt+24);
    span = (short) MapGetshort(mdatpt+26);
    numbrushes = (short) MapGetshort(mdatpt+28);
    curbrsh = 0;
    if (clickmask == 0) {
        clickmask = (int) MapGetshort(mdatpt+30);
        if (clickmask<0 || clickmask >= numblockgfx) clickmask = 0;
    }

    if (numbrushes>0) {
        mybrshpt = (mdatpt+32);
        for (i=0;i<8;i++) {
            j = MapGetshort(mybrshpt); j *= MapGetshort(mybrshpt+2); j *= 2; j += 4;
            if (j < 4) { merror = ER_MAPLOADERROR; return -1; }
            brshpt[i] = malloc(j);
            if (brshpt[i]==NULL) { merror = ER_OUTOFMEM; return -1; }
            j /= 2; mybrsh2pt = brshpt[i];
            *mybrsh2pt = (short) MapGetshort(mybrshpt);
            mybrsh2pt++;
            *mybrsh2pt = (short) MapGetshort(mybrshpt+2);
            mybrsh2pt++; mybrshpt+=4;
            j -= 2;
            if (maptype == 0) {
                while (j) {
                    j--;
                    *mybrsh2pt = (short) MapGetshort(mybrshpt);
                    if (*mybrsh2pt >= 0) *mybrsh2pt /= BLOCKSTRSIZE;
                    mybrsh2pt++; mybrshpt+=2;
                }
            } else {
                while (j) {
                    j--;
                    *mybrsh2pt = (short) MapGetshort(mybrshpt);
                    if (*mybrsh2pt < 0) *mybrsh2pt *= FMP_ANI_MEMUNIT;
                    mybrsh2pt++; mybrshpt+=2;
                }
            }
            numbrushes--; if (!numbrushes) i=8;
        }
        /* first 8 brush names */
        mynamept = (char *) mybrshpt;
        if ((MapGetchksz(mdatpt-4)) > (mynamept-((char *) mdatpt))) {
            for (i=0;i<8;i++) {
                if (brshpt[i] != NULL) {
                    strcpy(brshname[i], mynamept);
                    mynamept += strlen(mynamept);
                    mynamept++;
                }
            }
        } else return 0;

        /* more than 8 brushes */
        if (mappy_ptr_is_odd(mynamept)) mynamept++;
        mybrshpt = (unsigned char *) mynamept;
        if ((MapGetchksz(mdatpt-4)) > (((char *) mybrshpt)-((char *) mdatpt))) {
            mybrshpt += 6; k = 8+MapGetshort(mybrshpt); mybrshpt += 2;
            if (k > MAXBRSH) k = MAXBRSH;
            for (i=8;i<k;i++) {
                j = MapGetshort(mybrshpt); j *= MapGetshort(mybrshpt+2); j *= 2; j += 4;
                if (j < 4) { merror = ER_MAPLOADERROR; return -1; }
                brshpt[i] = malloc(j);
                if (brshpt[i]==NULL) { merror = ER_OUTOFMEM; return -1; }
                j /= 2; mybrsh2pt = brshpt[i];
                *mybrsh2pt = (short) MapGetshort(mybrshpt);
                mybrsh2pt++;
                *mybrsh2pt = (short) MapGetshort(mybrshpt+2);
                mybrsh2pt++; mybrshpt+=4;
                j -= 2;
                if (maptype == 0) {
                    while (j) {
                        j--;
                        *mybrsh2pt = (short) MapGetshort(mybrshpt);
                        if (*mybrsh2pt >= 0) *mybrsh2pt /= BLOCKSTRSIZE;
                        mybrsh2pt++; mybrshpt+=2;
                    }
                } else {
                    while (j) {
                        j--;
                        *mybrsh2pt = (short) MapGetshort(mybrshpt);
                        if (*mybrsh2pt < 0) *mybrsh2pt *= FMP_ANI_MEMUNIT;
                        mybrsh2pt++; mybrshpt+=2;
                    }
                }
                strcpy(brshname[i], (char *) mybrshpt);
                mybrshpt += strlen((char *) mybrshpt);
                mybrshpt++;
                if (mappy_ptr_is_odd(mybrshpt)) mybrshpt++;
            }
        }
    }

    logit("xo=%d\nyo=%d\nstrtstr=%d\nstrtblk=%d\ncurstr=%d\nnumbrushes=%d\n",
        xmapoffset, ymapoffset, strtstr, strtblk, curstr, numbrushes);

    return 0;
}

static void AthrCopyField(char * dst, int dstsz, unsigned char * src, int * i, int chksz) {
    int j = 0;
    if (dstsz < 1) return;
    dst[0] = 0;
    while (*i < chksz) {
        unsigned char c = src[*i];
        (*i)++;
        if (c == 0) { dst[j] = 0; return; }
        if (j < dstsz - 1) dst[j++] = (char) c;
    }
    dst[j] = 0;
}

int DecodeATHR(unsigned char * mdatpt) {
    int i, chksz;

    logit("ATHR: %d bytes.\n", MapGetchksz(mdatpt+4));
    chksz = MapGetchksz(mdatpt+4);
    if (chksz < 0) { merror = ER_MAPLOADERROR; return -1; }
    mdatpt += 8;
    i = 0;
    AthrCopyField(authorname, (int) sizeof(authorname), mdatpt, &i, chksz);
    AthrCopyField(authorinfo1, (int) sizeof(authorinfo1), mdatpt, &i, chksz);
    AthrCopyField(authorinfo2, (int) sizeof(authorinfo2), mdatpt, &i, chksz);
    AthrCopyField(authorinfo3, (int) sizeof(authorinfo3), mdatpt, &i, chksz);
    return 0;
}

int DecodeCMAP(unsigned char * mdatpt) {
    logit("CMAP: %d bytes.\n", MapGetchksz(mdatpt+4));
    mdatpt += 8;
    cmappt = (unsigned char *) malloc(MapGetchksz(mdatpt-4));
    if (cmappt==NULL) { merror = ER_OUTOFMEM; return -1; }
    memcpy(cmappt, mdatpt, MapGetchksz(mdatpt-4));
    return 0;
}

int DecodeBKDT(unsigned char * mdatpt) {
    int i, j;
    BLKSTR * myblkpt;

    logit("BKDT: %d bytes.\n", MapGetchksz(mdatpt+4));
    mdatpt += 8;
    blockstrpt = malloc(BKDTMEM);
    if (blockstrpt==NULL) { merror = ER_OUTOFMEM; return -1; }

    if (blockstrsize < 1) { merror = ER_MAPLOADERROR; return -1; }
    myblkpt = (BLKSTR *) blockstrpt;
    j = MapGetchksz(mdatpt-4);
    if (j < 0) { merror = ER_MAPLOADERROR; return -1; }
    i = 0; while (i < j) {
        if (i + 32 > j) break;
        if ((unsigned char *)(myblkpt + 1) > (unsigned char *)blockstrpt + BKDTMEM) { merror = ER_MAPLOADERROR; return -1; }
        myblkpt->bgoff = (int) MapGetlong(mdatpt+i);
        myblkpt->fgoff = (int) MapGetlong(mdatpt+i+4);
        myblkpt->fgoff2 = (int) MapGetlong(mdatpt+i+8);
        myblkpt->fgoff3 = (int) MapGetlong(mdatpt+i+12);
        if (maptype > 0) {
            myblkpt->bgoff *= (blockwidth*blockheight*((blockdepth+1)/8));
            myblkpt->fgoff *= (blockwidth*blockheight*((blockdepth+1)/8));
            myblkpt->fgoff2 *= (blockwidth*blockheight*((blockdepth+1)/8));
            myblkpt->fgoff3 *= (blockwidth*blockheight*((blockdepth+1)/8));
        }
        myblkpt->user1 = (int) MapGetlong(mdatpt+i+16);
        myblkpt->user2 = (int) MapGetlong(mdatpt+i+20);
        myblkpt->user3 = (int) MapGetshort(mdatpt+i+24);
        myblkpt->user4 = (int) MapGetshort(mdatpt+i+26);
        myblkpt->user5 = mdatpt[i+28];
        myblkpt->user6 = mdatpt[i+29];
        myblkpt->user7 = mdatpt[i+30];
        if (mdatpt[i+31]&0x80) myblkpt->unused3 = 1; else myblkpt->unused3 = 0;
        if (mdatpt[i+31]&0x40) myblkpt->unused2 = 1; else myblkpt->unused2 = 0;
        if (mdatpt[i+31]&0x20) myblkpt->unused1 = 1; else myblkpt->unused1 = 0;
        if (mdatpt[i+31]&0x10) myblkpt->trigger = 1; else myblkpt->trigger = 0;
        if (mdatpt[i+31]&0x08) myblkpt->br = 1; else myblkpt->br = 0;
        if (mdatpt[i+31]&0x04) myblkpt->bl = 1; else myblkpt->bl = 0;
        if (mdatpt[i+31]&0x02) myblkpt->tr = 1; else myblkpt->tr = 0;
        if (mdatpt[i+31]&0x01) myblkpt->tl = 1; else myblkpt->tl = 0;
        i += blockstrsize;
        myblkpt++;
    }
    return 0;
}

int DecodeANDT(unsigned char * mdatpt) {
    int aniadjust;
    ANISTR * myanistrpt;
    unsigned char * mdatendpt;
    int * myseqpt;

    {
        int andtsz = MapGetchksz(mdatpt+4);
        logit("ANDT: %d bytes.\n", andtsz);
        if (andtsz < 16 || andtsz > ANDTMEM) { merror = ER_MAPLOADERROR; return -1; }
        mdatpt += 8;
        mdatendpt = mdatpt + andtsz;
    }

    animstrpt = malloc(ANDTMEM);
    if (animstrpt == NULL) { merror = ER_OUTOFMEM; return -1; }
    memset(animstrpt, 0, ANDTMEM);

    aniadjust = 0;
    myanistrpt = (ANISTR *) (mdatendpt);
    while ((unsigned char *) myanistrpt > mdatpt) {
        aniadjust ++;
        myanistrpt--;
        if (myanistrpt->antype == AN_END) break;
    }
    if ((unsigned char *) myanistrpt <= mdatpt || myanistrpt->antype != AN_END) { merror = ER_MAPLOADERROR; return -1; }

    logit("Aniadjust = %d\n", aniadjust);

    myanistrpt = (ANISTR *) (animstrpt+ANDTMEM);
    while ((unsigned char *) myanistrpt > (unsigned char *) animstrpt &&
        mdatendpt >= mdatpt + 16) {
        myanistrpt--;
        mdatendpt -= 16;
        myanistrpt->antype = mdatendpt[0];
        myanistrpt->andelay = mdatendpt[1];
        myanistrpt->ancount = mdatendpt[2];
        myanistrpt->anuser = mdatendpt[3];

        if (maptype == 0) {
            myanistrpt->ancuroff = (int) MapGetlong(mdatendpt+4)+(aniadjust*(16-FMP_ANI_MEMUNIT));
            myanistrpt->anstartoff = (int) MapGetlong(mdatendpt+8)+(aniadjust*(16-FMP_ANI_MEMUNIT));
            myanistrpt->anendoff = (int) MapGetlong(mdatendpt+12)+(aniadjust*(16-FMP_ANI_MEMUNIT));
        } else {
            myanistrpt->ancuroff = (MapGetlong(mdatendpt+4))*(FMP_SEQ_MEMUNIT);
            myanistrpt->ancuroff -= MapGetchksz(mdatpt-4);
            myanistrpt->anstartoff = (MapGetlong(mdatendpt+8))*(FMP_SEQ_MEMUNIT);
            myanistrpt->anstartoff -= MapGetchksz(mdatpt-4);
            myanistrpt->anendoff = (MapGetlong(mdatendpt+12))*(FMP_SEQ_MEMUNIT);
            myanistrpt->anendoff -= MapGetchksz(mdatpt-4);
        }

        logit("anim: type=%d delay=%d count=%d cur=%d strt=%d end=%d\n",
            myanistrpt->antype, myanistrpt->andelay, myanistrpt->ancount,
            myanistrpt->ancuroff, myanistrpt->anstartoff, myanistrpt->anendoff);
        if (myanistrpt->antype == AN_END) break;
    }
    if ((unsigned char *) myanistrpt >= (unsigned char *) animstrpt + ANDTMEM ||
        myanistrpt->antype != AN_END) { merror = ER_MAPLOADERROR; return -1; }

    myseqpt = (int *) myanistrpt;
    while (mdatendpt > mdatpt) {
        if ((unsigned char *) myseqpt <= (unsigned char *) animstrpt) { merror = ER_MAPLOADERROR; return -1; }
        myseqpt--;
        mdatendpt -= 4;
        if (maptype == 0)
            *myseqpt = (int) ((MapGetlong(mdatendpt)/blockstrsize)*FMP_BLK_MEMUNIT);
        else
            *myseqpt = (int) ((MapGetlong(mdatendpt))*FMP_BLK_MEMUNIT);
    }
    InitAnims();
    return 0;
}

int DecodeAGFX(unsigned char * mdatpt) {
    logit("AGFX: %d bytes.\n", MapGetchksz(mdatpt+4));
    altgfxpt = malloc(MapGetchksz(mdatpt+4));
    if (altgfxpt==NULL) { merror = ER_OUTOFMEM; return -1; }
    memcpy(altgfxpt, mdatpt+8, MapGetchksz(mdatpt+4));
    return 0;
}

int DecodeBGFX(unsigned char * mdatpt) {
    int chksz = MapGetchksz(mdatpt+4);
    logit("BGFX: %d bytes.\n", chksz);
    if (chksz < 0 || chksz > BKGFXMEM) { merror = ER_MAPLOADERROR; return -1; }
    blockgfxpt = malloc(BKGFXMEM);
    if (blockgfxpt==NULL) { merror = ER_OUTOFMEM; return -1; }
    memcpy(blockgfxpt, mdatpt+8, (size_t) chksz);
    return 0;
}

int DecodeNOVC(unsigned char * mdatpt) {
    logit("NOVC: %d bytes.\n", MapGetchksz(mdatpt+4));
    if (MapGetchksz(mdatpt+4) < 70) strcpy(novctext, (const char *) (mdatpt+8));
    return 0;
}

static int DecodeLayerData(unsigned char * data, int nbytes, int lnum) {
    int i, j, k, l, cells, written;
    unsigned char * end;
    short int * dest, * src;

    if (lnum<0 || lnum>=MAXMAPLAYERS) return -1;
    if (mapwidth < 1 || mapheight < 1 || nbytes < 0) { merror = ER_MAPLOADERROR; return -1; }
    if (maptype == 0 && blockstrsize < 1) { merror = ER_MAPLOADERROR; return -1; }
    logit("Layer %d: %d bytes.\n", lnum, nbytes);
    cells = ((int) mapwidth) * ((int) mapheight);
    if (mapmappt[lnum]!=NULL) free(mapmappt[lnum]);
    mapmappt[lnum] = malloc((size_t)cells * sizeof(short int));
    if (mapmappt[lnum] == NULL) { merror = ER_OUTOFMEM; return -1; }
    memset(mapmappt[lnum], 0, (size_t)cells * sizeof(short int));
    logit("Maptype=%d\n", maptype);

    end = data + nbytes;
    dest = mapmappt[lnum];
    written = 0;

    if (maptype == 0 || maptype == 1) {
        for (j=0;j<mapheight;j++) {
            for (i=0;i<mapwidth;i++) {
                if (data + 2 > end) { merror = ER_MAPLOADERROR; return -1; }
                *dest = (short int) MapGetshort(data);
                if (maptype == 0) {
                    if (*dest >= 0) *dest /= blockstrsize;
                    else { *dest /= 16; *dest *= FMP_ANI_MEMUNIT; }
                } else {
                    if (*dest < 0) *dest *= FMP_ANI_MEMUNIT;
                }
                data += 2; dest++; written++;
            }
        }
    } else if (maptype == 2 || maptype == 3) {
        for (j=0;j<mapheight;j++) {
            for (i=0;i<mapwidth;) {
                if (data + 2 > end) { merror = ER_MAPLOADERROR; return -1; }
                k = (int) MapGetshort(data);
                data += 2;
                if (k > 0) {
                    while (k) {
                        if (i >= mapwidth || written >= cells || data + 2 > end) { merror = ER_MAPLOADERROR; return -1; }
                        *dest = (short int) MapGetshort(data);
                        if (*dest < 0) *dest *= FMP_ANI_MEMUNIT;
                        dest++; data += 2;
                        i++; k--; written++;
                    }
                } else if (k < 0) {
                    if (data + 2 > end) { merror = ER_MAPLOADERROR; return -1; }
                    if (maptype == 2) {
                        l = (int) MapGetshort(data); data += 2;
                        while (k) {
                            if (i >= mapwidth || written >= cells) { merror = ER_MAPLOADERROR; return -1; }
                            *dest = (short int) l;
                            if (*dest < 0) *dest *= FMP_ANI_MEMUNIT;
                            dest++; i++; k++; written++;
                        }
                    } else {
                        l = (int) MapGetshort(data); data += 2;
                        src = dest + l;
                        while (k) {
                            if (i >= mapwidth || written >= cells) { merror = ER_MAPLOADERROR; return -1; }
                            if (src < mapmappt[lnum] || src >= mapmappt[lnum] + cells) { merror = ER_MAPLOADERROR; return -1; }
                            *dest = *src;
                            if (*dest < 0) *dest *= FMP_ANI_MEMUNIT;
                            dest++; src++; i++; k++; written++;
                        }
                    }
                }
            }
        }
    }

    if (lnum == 0) { mappt = mapmappt[lnum]; maplayer = 0; }
    return 0;
}

int DecodeLayer(unsigned char * mdatpt, int lnum) {
    int chksz = MapGetchksz(mdatpt+4);
    if (chksz < 0) { merror = ER_MAPLOADERROR; return -1; }
    return DecodeLayerData(mdatpt+8, chksz, lnum);
}

int DecodeNULL(unsigned char * mdatpt) {
    FmpUnknownChunk *node, *tail;
    uint32_t sz = (uint32_t) MapGetchksz(mdatpt+4);

    logit("UNKNOWN: %d bytes.\n", MapGetchksz(mdatpt+4));
    node = (FmpUnknownChunk *) malloc(sizeof(FmpUnknownChunk) + sz + 8);
    if (node==NULL) { merror = ER_OUTOFMEM; return -1; }
    node->next = NULL;
    memcpy(node->id, mdatpt, 4);
    node->size = sz;
    memcpy((unsigned char *)(node + 1), mdatpt, sz + 8);

    if (chunklist==NULL) {
        chunklist = node;
        return 0;
    }

    tail = chunklist;
    while (tail->next!=NULL) tail = tail->next;
    tail->next = node;

    return 0;
}

int FmpChunkCount(void) {
    int n = 0;
    FmpUnknownChunk *p = chunklist;
    while (p) { n++; p = p->next; }
    return n;
}

static FmpUnknownChunk *FmpChunkAt(int idx) {
    FmpUnknownChunk *p = chunklist;
    while (p && idx > 0) { p = p->next; idx--; }
    return (idx == 0) ? p : NULL;
}

int FmpChunkCreate(const char *id4, int size, int pos) {
    FmpUnknownChunk *node, *p, *prev;
    int i;
    unsigned char *raw;
    if (id4 == NULL || size < 0) return -1;
    node = (FmpUnknownChunk *) malloc(sizeof(FmpUnknownChunk) + (size_t) size + 8);
    if (node == NULL) return -1;
    memset(node, 0, sizeof(FmpUnknownChunk) + (size_t) size + 8);
    node->next = NULL;
    memcpy(node->id, id4, 4);
    node->size = (uint32_t) size;
    raw = (unsigned char *) (node + 1);
    memcpy(raw, id4, 4);
    raw[4] = (unsigned char) ((size >> 24) & 0xFF);
    raw[5] = (unsigned char) ((size >> 16) & 0xFF);
    raw[6] = (unsigned char) ((size >> 8) & 0xFF);
    raw[7] = (unsigned char) (size & 0xFF);
    if (pos <= 0 || chunklist == NULL) {
        node->next = chunklist;
        chunklist = node;
        return 0;
    }
    prev = chunklist; p = chunklist->next; i = 1;
    while (p && i < pos) { prev = p; p = p->next; i++; }
    node->next = p;
    prev->next = node;
    return i;
}

void FmpChunkDelete(int idx) {
    FmpUnknownChunk *p = chunklist, *prev = NULL;
    while (p && idx > 0) { prev = p; p = p->next; idx--; }
    if (p == NULL) return;
    if (prev) prev->next = p->next;
    else chunklist = p->next;
    free(p);
}

int FmpChunkGetVal(int idx, int off) {
    FmpUnknownChunk *p = FmpChunkAt(idx);
    unsigned char *body;
    if (p == NULL) return 0;
    if (off == -1)
        return ((unsigned char) p->id[0]) |
    (((unsigned char) p->id[1]) << 8) |
    (((unsigned char) p->id[2]) << 16) |
    (((unsigned char) p->id[3]) << 24);
    if (off == -2) return (int) p->size;
    if (off < 0 || off >= (int) p->size) return 0;
    body = (unsigned char *) (p + 1) + 8;
    return body[off];
}

void FmpChunkSetVal(int idx, int off, int val) {
    FmpUnknownChunk *p = FmpChunkAt(idx);
    unsigned char *body;
    if (p == NULL) return;
    if (off == -1) {
        p->id[0] = (char) (val & 0xFF);
        p->id[1] = (char) ((val >> 8) & 0xFF);
        p->id[2] = (char) ((val >> 16) & 0xFF);
        p->id[3] = (char) ((val >> 24) & 0xFF);
        memcpy((unsigned char *) (p + 1), p->id, 4);
        return;
    }
    if (off < 0 || off >= (int) p->size) return;
    body = (unsigned char *) (p + 1) + 8;
    body[off] = (unsigned char) val;
}

int MapIsFmaName(const char *name) {
    int n;
    if (name==NULL) return 0;
    n = (int) strlen(name);
    if (n < 4) return 0;
    name += n - 4;
    return (!strcmp(name, ".fma") || !strcmp(name, ".FMA") || !strcmp(name, ".Fma"));
}

int FindFreeBrush(void) {
    int i;
    for (i=0;i<MAXBRSH;i++)
        if (brshpt[i]==NULL) return i+1;
    return 0;
}

int MapDecode(FILE * mfpt, int mpfilesize) {
    int chkdn;
    unsigned char * fmappospt;
    char mphdr[8];
    char * held_gfx = NULL;
    int held_ngfx = 0;

    logit("MapDecode\n");
    if (map_keepgfx && blockgfxpt!=NULL) {
        held_gfx = blockgfxpt;
        held_ngfx = numblockgfx;
        blockgfxpt = NULL;
    }
    FreeAll();
    logit("After FreeAll\n");
    mpfilesize -= 12;
    while (mpfilesize > 0) {

        if (fread(mphdr, 1, 8, mfpt) != 8) {
            merror = ER_OUTOFMEM;
            if (held_gfx) free(held_gfx);
            FreeAll();
            return -1;
        }
        {
            int chksz = MapGetchksz(mphdr+4);
            if (chksz < 0 || chksz > 64*1024*1024) {
                merror = ER_MAPLOADERROR;
                if (held_gfx) free(held_gfx);
                FreeAll();
                return -1;
            }
            fmappospt = malloc((size_t)chksz + 8);
            if (fmappospt==NULL) {
                merror = ER_OUTOFMEM;
                if (held_gfx) free(held_gfx);
                FreeAll();
                return -1;
            }
            memcpy(fmappospt, mphdr, 8);
            if (fread(fmappospt+8, 1, (size_t)chksz, mfpt) != (size_t)chksz) {
                free(fmappospt);
                merror = ER_MAPLOADERROR;
                if (held_gfx) free(held_gfx);
                FreeAll();
                return -1;
            }
        }

        chkdn = 0;
        if (chunk_is(fmappospt, "ATHR", 4)) { chkdn = 1; DecodeATHR(fmappospt); }
        if (chunk_is(fmappospt, "MPHD", 4)) { chkdn = 1; DecodeMPHD(fmappospt); }
        if (chunk_is(fmappospt, "EDHD", 4)) { chkdn = 1; DecodeEDHD(fmappospt); }
        if (chunk_is(fmappospt, "EPHD", 4)) { chkdn = 1; if (F14_DecodeEPHD(fmappospt, MapGetchksz(fmappospt+4), mapislsb)) { merror = ER_MAPLOADERROR; } }
        if (chunk_is(fmappospt, "TSTR", 4)) { chkdn = 1; if (F14_DecodeTSTR(fmappospt, MapGetchksz(fmappospt+4), mapislsb)) { merror = ER_MAPLOADERROR; } }
        if (chunk_is(fmappospt, "LSTR", 4)) { chkdn = 1; if (F14_DecodeLSTR(fmappospt, MapGetchksz(fmappospt+4), mapislsb)) { merror = ER_MAPLOADERROR; } }
        if (chunk_is(fmappospt, "OBDT", 4)) { chkdn = 1; if (F14_DecodeOBDT(fmappospt, MapGetchksz(fmappospt+4), mapislsb)) { merror = ER_MAPLOADERROR; } }
        if (chunk_is(fmappospt, "OBFN", 4)) { chkdn = 1; if (F14_DecodeOBFN(fmappospt, MapGetchksz(fmappospt+4))) { merror = ER_MAPLOADERROR; } }
        if (chunk_is(fmappospt, "TSDF", 4)) { chkdn = 1; if (TS_DecodeTSDF(fmappospt, MapGetchksz(fmappospt+4), mapislsb, maptype, blockstrsize)) { merror = ER_MAPLOADERROR; } }
        if (chunk_is(fmappospt, "TSIN", 4)) { chkdn = 1; if (TS_DecodeTSIN(fmappospt, MapGetchksz(fmappospt+4), mapislsb)) { merror = ER_MAPLOADERROR; } }
        if (chunk_is(fmappospt, "NLYR", 4)) {
            int nlyr, nloff;
            chkdn = 1;
            {
                int body = MapGetchksz(fmappospt+4);
                if (F14_NlyrInfo(fmappospt, body, mapislsb, &nlyr, &nloff) == 0 &&
                    nloff >= 0 && nloff < body)
                DecodeLayerData(fmappospt + 8 + nloff, body - nloff, nlyr);
                else
                    DecodeNULL(fmappospt);
            }
        }
        if (chunk_is(fmappospt, "CMAP", 4)) { chkdn = 1; DecodeCMAP(fmappospt); }
        if (chunk_is(fmappospt, "BKDT", 4)) { chkdn = 1; DecodeBKDT(fmappospt); }
        if (chunk_is(fmappospt, "ANDT", 4)) { chkdn = 1; DecodeANDT(fmappospt); }
        if (chunk_is(fmappospt, "AGFX", 4)) { chkdn = 1; DecodeAGFX(fmappospt); }
        if (chunk_is(fmappospt, "BGFX", 4)) { chkdn = 1; DecodeBGFX(fmappospt); }
        if (chunk_is(fmappospt, "NOVC", 4)) { chkdn = 1; DecodeNOVC(fmappospt); }
        if (chunk_is(fmappospt, "BODY", 4)) { chkdn = 1; DecodeLayer(fmappospt, 0); }
        if (chunk_is(fmappospt, "LYR", 3) && fmappospt[3]>='1' && fmappospt[3]<='9') { chkdn = 1; DecodeLayer(fmappospt, fmappospt[3]-'0'); }
        /* Layers 10..99 written as LY10 by older builds of this editor */
        if (chunk_is(fmappospt, "LY", 2) && fmappospt[2]>='1' && fmappospt[2]<='9' && fmappospt[3]>='0' && fmappospt[3]<='9') {
            int lyn = (fmappospt[2]-'0')*10+(fmappospt[3]-'0');
            if (lyn>=10 && lyn<MAXMAPLAYERS) { chkdn = 1; DecodeLayer(fmappospt, lyn); }
        }
        if (!chkdn) DecodeNULL(fmappospt);

        mpfilesize -= 8;
        mpfilesize -= MapGetchksz(fmappospt+4);
        free(fmappospt);
        logit("Left = %d bytes\n", mpfilesize);

        if (merror != ER_NONE) {
            if (held_gfx) { free(held_gfx); held_gfx = NULL; }
            FreeAll();
            return -1;
        }
    }
    if (held_gfx) {
        if (blockgfxpt==NULL) {
            blockgfxpt = held_gfx;
            if (numblockgfx<=0) numblockgfx = (short int) held_ngfx;
        } else {
            free(held_gfx);
        }
    }
    SetTranspColour();
    ApplyEditorPrefs();
    return 0;
}

int LoadProject(void) {
    int mretval;
    FILE * fpt;
    char idtag[4];
    unsigned char tempc;
    int mapfilesize;

    //	if (dbgfpt == NULL) dbgfpt = fopen ("debug.txt", "w");
    logit("StartDebug\n");
    merror = ER_NONE;
    fpt = fopen(mapfilename, "rb");
    if (fpt == NULL) { merror = ER_NOOPEN; return -1; }

    mapfilesize = 0;
    merror = ER_MAPLOADERROR;
    if (fread(&idtag[0], 1, 1, fpt) == 1) {
        if (fread(&idtag[1], 1, 1, fpt) == 1) {
            if (fread(&idtag[2], 1, 1, fpt) == 1) {
                if (fread(&idtag[3], 1, 1, fpt) == 1) {
                    if (fread(&tempc, 1, 1, fpt) == 1) {
                        mapfilesize = (((int) tempc)<<24);
                        if (fread(&tempc, 1, 1, fpt) == 1) {
                            mapfilesize |= (((int) tempc)<<16);
                            if (fread(&tempc, 1, 1, fpt) == 1) {
                                mapfilesize |= (((int) tempc)<<8);
                                if (fread(&tempc, 1, 1, fpt) == 1) {
                                    mapfilesize |= ((int) tempc);
                                    mapfilesize += 8;
                                    if (!strncmp(idtag, "FORM", 4)) {
                                        if (fread(&idtag[0], 1, 1, fpt) == 1) {
                                            if (fread(&idtag[1], 1, 1, fpt) == 1) {
                                                if (fread(&idtag[2], 1, 1, fpt) == 1) {
                                                    if (fread(&idtag[3], 1, 1, fpt) == 1) {
                                                        if (!strncmp(idtag, "FMAP", 4)) merror = ER_NONE;
                                    } } } } }
    } } } } } } } }

    if (merror != ER_NONE) { fclose(fpt); return -1; }

    map_keepgfx = 0;
    if (MapIsFmaName(mapfilename) && blockgfxpt!=NULL)
        map_keepgfx = 1;
    mretval = MapDecode(fpt, mapfilesize);
    map_keepgfx = 0;
    fclose(fpt);
    //	if (dbgfpt != NULL) { logit ("MapLoad done\n"); fclose (dbgfpt); dbgfpt = NULL; }
    return mretval;
}

int SizeBrushes(void) {
    int i, j, k;
    short int * mybrshpt;
    j = 0; for (i=0;i<MAXBRSH;i++) {
        if (brshpt[i]!=NULL) {
            mybrshpt = brshpt[i];
            k = *mybrshpt; mybrshpt++; k *= *mybrshpt; j += (k*2)+4;
            if (brshname[i][0] == 0) strcpy(brshname[i], "Brush");
            j += strlen(brshname[i]); j ++;
            if (i==8) j += 8;
            if (i>7) { if (!(strlen(brshname[i])&1)) j++; }
        }
    }
    if (j&1) j++;
    return j;
}

/* RleLayer -- arch-safe: header is BE, body uses mapislsb via MapPutshort.
 * Caller must have already written the 4-byte chunk ID via fmp logic;
 * here we write the full 8-byte header (ID passed by caller pattern in
 * SaveProject). To keep the diff small, RleLayer writes its own header
 * from the global genheader IDs but with explicit BE size. */

static int fmp_write_genheader(FILE *f) {
    unsigned char hdr[FMP_GENHEAD_DISK_SIZE];
    hdr[0] = (unsigned char)genheader.id1; hdr[1] = (unsigned char)genheader.id2;
    hdr[2] = (unsigned char)genheader.id3; hdr[3] = (unsigned char)genheader.id4;
    /* genheader.headsize is stored BE (Motorola) on disk */
    fmp_put_be32(hdr + 4, (uint32_t)byteswapl((unsigned long)genheader.headsize));
    /* NOTE: byteswapl round-trips host->BE; equivalent to fmp_put_be32 of host value.
     * We keep byteswapl for minimal behavioural change. */
    return (fwrite(hdr, 1, sizeof(hdr), f) == sizeof(hdr)) ? 0 : -1;
}

/* Encode in-memory MPHD/EDHD/BLKSTR to fixed-size disk buffers.
 * Disk layout is little- or big-endian per mapislsb for multi-byte fields
 * (except chunk headers which are always BE). No struct dump, no padding. */
static void fmp_encode_mphd(unsigned char out[FMP_MPHD_DISK_SIZE], const MPHD *m, int is_lsb) {
    memset(out, 0, FMP_MPHD_DISK_SIZE);
    out[0] = (unsigned char)m->mapverhigh; out[1] = (unsigned char)m->mapverlow;
    out[2] = (unsigned char)m->lsb; out[3] = (unsigned char)m->maptype;
    fmp_put_i16(out + 4, m->mapwidth, is_lsb);
    fmp_put_i16(out + 6, m->mapheight, is_lsb);
    fmp_put_i16(out + 8, m->reserved1, is_lsb);
    fmp_put_i16(out + 10, m->reserved2, is_lsb);
    fmp_put_i16(out + 12, m->blockwidth, is_lsb);
    fmp_put_i16(out + 14, m->blockheight, is_lsb);
    fmp_put_i16(out + 16, m->blockdepth, is_lsb);
    fmp_put_i16(out + 18, m->blockstrsize, is_lsb);
    fmp_put_i16(out + 20, m->numblockstr, is_lsb);
    fmp_put_i16(out + 22, m->numblockgfx, is_lsb);
    out[24] = m->ckey8bit; out[25] = m->ckeyred; out[26] = m->ckeygreen; out[27] = m->ckeyblue;
    fmp_put_i16(out + 28, m->blockgapx, is_lsb);
    fmp_put_i16(out + 30, m->blockgapy, is_lsb);
    fmp_put_i16(out + 32, m->blockstaggerx, is_lsb);
    fmp_put_i16(out + 34, m->blockstaggery, is_lsb);
    fmp_put_i16(out + 36, m->clickmask, is_lsb);
    fmp_put_i16(out + 38, m->pillars, is_lsb);
}

static void fmp_encode_edhd(unsigned char out[FMP_EDHD_DISK_SIZE], const EDHD *e, int is_lsb) {
    memset(out, 0, FMP_EDHD_DISK_SIZE);
    fmp_put_i16(out + 0, e->xmapoffset, is_lsb);
    fmp_put_i16(out + 2, e->ymapoffset, is_lsb);
    fmp_put_i32(out + 4, (int32_t)e->fgcolour, is_lsb);
    fmp_put_i32(out + 8, (int32_t)e->bgcolour, is_lsb);
    fmp_put_i16(out + 12, e->swidth, is_lsb);
    fmp_put_i16(out + 14, e->sheight, is_lsb);
    fmp_put_i16(out + 16, e->strtstr, is_lsb);
    fmp_put_i16(out + 18, e->strtblk, is_lsb);
    fmp_put_i16(out + 20, e->curstr, is_lsb);
    fmp_put_i16(out + 22, e->curanim, is_lsb);
    fmp_put_i16(out + 24, e->animspd, is_lsb);
    fmp_put_i16(out + 26, e->span, is_lsb);
    fmp_put_i16(out + 28, e->numbrushes, is_lsb);
    fmp_put_i16(out + 30, e->pad, is_lsb);
}

static void fmp_encode_blkstr(unsigned char out[FMP_BLKSTR_DISK_SIZE], const BLKSTR *b, int is_lsb) {
    unsigned char flags;
    memset(out, 0, FMP_BLKSTR_DISK_SIZE);
    fmp_put_i32(out + 0, (int32_t)b->bgoff, is_lsb);
    fmp_put_i32(out + 4, (int32_t)b->fgoff, is_lsb);
    fmp_put_i32(out + 8, (int32_t)b->fgoff2, is_lsb);
    fmp_put_i32(out + 12, (int32_t)b->fgoff3, is_lsb);
    fmp_put_i32(out + 16, (int32_t)b->user1, is_lsb);
    fmp_put_i32(out + 20, (int32_t)b->user2, is_lsb);
    fmp_put_i16(out + 24, b->user3, is_lsb);
    fmp_put_i16(out + 26, b->user4, is_lsb);
    out[28] = b->user5; out[29] = b->user6; out[30] = b->user7;
    flags = 0;
    if (b->tl) flags |= 0x01;
    if (b->tr) flags |= 0x02;
    if (b->bl) flags |= 0x04;
    if (b->br) flags |= 0x08;
    if (b->trigger) flags |= 0x10;
    if (b->unused1) flags |= 0x20;
    if (b->unused2) flags |= 0x40;
    if (b->unused3) flags |= 0x80;
    out[31] = flags;
}

int RleLayer(short int * lyrpt, FILE * wpt) {
    int x, y;
    short int * rlemem;
    int rlepos, runlen;

    rlemem = malloc((size_t)((int) mapwidth)*((int) mapheight)*8);
    if (rlemem==NULL) { merror = ER_OUTOFMEM; return -1; }
    rlepos = 0;
    for (y=0;y<mapheight;y++) {
        for (x=0;x<mapwidth;) {
            runlen = 0;
            if (x >= (mapwidth-1) || lyrpt[x] != lyrpt[x+1]) {
                if (x >= (mapwidth-1)) { runlen++; x++; }
                else {
                    if (x >= (mapwidth-2)) { runlen += 2; x += 2; }
                    else {
                        while (lyrpt[x] != lyrpt[x+1] || lyrpt[x] != lyrpt[x+2]) {
                            runlen++; x++;
                            if (x == mapwidth) break;
                        }
                    }
                }
                if (x < mapwidth) { x++; runlen++; }
                MapPutshort((unsigned char *) (rlemem+rlepos), runlen); rlepos++;
                while (runlen) {
                    MapPutshort((unsigned char *) (rlemem+rlepos), lyrpt[x-runlen]); rlepos++;
                    runlen--;
                }
            } else {
                while (lyrpt[x] == lyrpt[x+1]) {
                    runlen++; x++;
                    if (x == mapwidth) break;
                }
                if (x < mapwidth) { x++; runlen++; }
                MapPutshort((unsigned char *) (rlemem+rlepos), -runlen);
                rlepos++;
                MapPutshort((unsigned char *) (rlemem+rlepos), lyrpt[x-runlen]);
                rlepos++;
            }
        }
        lyrpt += mapwidth;
    }

    genheader.headsize = byteswapl((unsigned long)(rlepos * 2));
    if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; free(rlemem); return -1; }
    if (fwrite(rlemem, 1, (size_t)rlepos*2, wpt) != (size_t)rlepos*2) { merror = ER_MAPSAVEERROR; free(rlemem); return -1; }
    free(rlemem);
    return 0;
}

static void CaptureEditorPrefs(void) {
    F14_EnsureEPHD();
    F14_EphdSet(F14_EPHD_SHOWUSER, showuser);
    F14_EphdSet(F14_EPHD_ONION, onion);
    F14_EphdSet(F14_EPHD_ONIONDARK, oniondark);
    F14_EphdSet(F14_EPHD_ONIONLOW, onionlower);
    F14_EphdSet(F14_EPHD_CURLAYER, MapGetCurLayer());
    F14_EphdSet(F14_EPHD_DIVON, divideron);
    F14_EphdSet(F14_EPHD_DIVGAPX, dividergapx);
    F14_EphdSet(F14_EPHD_DIVGAPY, dividergapy);
    F14_EphdSet(F14_EPHD_DIVCOL, (int) dividercol);
}

void ApplyEditorPrefs(void) {
    int v;
    if (!F14_HasEPHD()) return;
    v = F14_EphdGet(F14_EPHD_SHOWUSER);
    if (v >= 0 && v <= 16) showuser = v;
    onion = F14_EphdGet(F14_EPHD_ONION);
    if (onion < 0) onion = 0;
    if (onion > ONION_ALL) onion = ONION_ALL;
    oniondark = F14_EphdGet(F14_EPHD_ONIONDARK) ? 1 : 0;
    onionlower = F14_EphdGet(F14_EPHD_ONIONLOW) ? 1 : 0;
    v = F14_EphdGet(F14_EPHD_CURLAYER);
    if (v >= 0 && v < MAXMAPLAYERS && GetLayerpt(v) != NULL)
        MapSetCurLayer(v);
    divideron = F14_EphdGet(F14_EPHD_DIVON) ? 1 : 0;
    v = F14_EphdGet(F14_EPHD_DIVGAPX);
    if (v >= 8 && v <= 4096) dividergapx = v;
    v = F14_EphdGet(F14_EPHD_DIVGAPY);
    if (v >= 8 && v <= 4096) dividergapy = v;
    dividercol = (long) F14_EphdGet(F14_EPHD_DIVCOL);
}

int SaveProject(void) {
    return SaveProjectEx(0);
}

int SaveProjectEx(int skipgfx) {
    /* attempts to save project file name and path in mapfilename */
    char backupname[128];
    short exbrshinf[4];
    MPHD mphdinfo;
    EDHD edhdinfo;
    short int * mybrshpt;
    int i, j, k, l, m;
    ANISTR * myanpt;
    BLKSTR * mybkpt;
    int * myanseqpt;

    merror = 0;

    //	if (dbgfpt == NULL) dbgfpt = fopen ("debug.txt", "w");
    logit("StartDebug\n");

    if (cmappt==NULL) merror = ER_MAPSAVEERROR;
    if (mappt==NULL) merror = ER_MAPSAVEERROR;
    if (blockstrpt==NULL) merror = ER_MAPSAVEERROR;
    if (blockgfxpt==NULL) merror = ER_MAPSAVEERROR;
    if (numblockstr==0) merror = ER_MAPSAVEERROR;
    if (numblockgfx==0) merror = ER_MAPSAVEERROR;
    if (mapfilename[0]==0) merror = ER_MAPSAVEERROR;

    if (merror) return -1;

    CaptureEditorPrefs();

    strcpy(backupname, mapfilename);
    j = 0; for (i=0; backupname[i]; i++) { if (backupname[i]=='.') j = i; }
    if (j) {
        backupname[j+1] = 'B'; backupname[j+2] = 'A';
        backupname[j+3] = 'K'; backupname[j+4] = 0;
    } else strcat(backupname, ".BAK");

    rename(mapfilename, backupname);

    genheader.id1='F'; genheader.id2='O'; genheader.id3='R'; genheader.id4='M';
    genheader.headsize=0;

    filept = NULL;
    filept = fopen(mapfilename, "wb");
    if (filept==NULL) { merror = ER_NOOPEN; return -1; }
    if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }

    genheader.id1='F'; genheader.id2='M'; genheader.id3='A'; genheader.id4='P';
    if (fwrite(&genheader.id1, 1, 4, filept)!=4) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }

    if ((authorname[0]!=0) || (authorinfo1[0]!=0) || (authorinfo2[0]!=0) || (authorinfo3[0]!=0)) {
        l = (int)strlen(authorname); l += (int)strlen(authorinfo1);
        l += (int)strlen(authorinfo2); l += (int)strlen(authorinfo3); l += 8;
        m = l%4; l -= m;
        genheader.id1='A'; genheader.id2='T'; genheader.id3='H'; genheader.id4='R';
        genheader.headsize=byteswapl((unsigned long)l);
        if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        if (fwrite(authorname, 1, (strlen(authorname)+1), filept)!=(strlen(authorname)+1)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        if (fwrite(authorinfo1, 1, (strlen(authorinfo1)+1), filept)!=(strlen(authorinfo1)+1)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        if (fwrite(authorinfo2, 1, (strlen(authorinfo2)+1), filept)!=(strlen(authorinfo2)+1)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        if (fwrite(authorinfo3, 1, (strlen(authorinfo3)+1+(4-m)), filept)!=(strlen(authorinfo3)+1+(4-m))) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    }

    genheader.id1='M'; genheader.id2='P'; genheader.id3='H'; genheader.id4='D';
    genheader.headsize=byteswapl((unsigned long)FMP_MPHD_DISK_SIZE);
    if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }

    if (maptype == 0) {
        mphdinfo.mapverhigh=0;
        mphdinfo.mapverlow=5;
    } else {
        mphdinfo.mapverhigh=1;
        mphdinfo.mapverlow=0;
    }
    mphdinfo.lsb=mapislsb; /* stored in INTEL format */
    mphdinfo.maptype=maptype;
    mphdinfo.mapwidth=mapwidth;
    mphdinfo.mapheight=mapheight;
    mphdinfo.reserved1=(short int)((flippedtiles?1:0)|(rotatedtiles?2:0)|(wnesmode?4:0));
    mphdinfo.reserved2=0;
    mphdinfo.blockwidth=blockwidth;
    mphdinfo.blockheight=blockheight;
    mphdinfo.blockdepth=blockdepth;
    mphdinfo.blockstrsize=BLOCKSTRSIZE;
    mphdinfo.numblockstr=numblockstr;
    mphdinfo.numblockgfx=numblockgfx;
    mphdinfo.ckeyred=transred;
    mphdinfo.ckeygreen=transgreen;
    mphdinfo.ckeyblue=transblue;
    mphdinfo.ckey8bit=trans8bit;
    mphdinfo.blockgapx=blockgapx;
    mphdinfo.blockgapy=blockgapy;
    mphdinfo.blockstaggerx=blockstaggerx;
    mphdinfo.blockstaggery=blockstaggery;
    mphdinfo.clickmask=(short int) clickmask;
    mphdinfo.pillars=(short int) isopillars;

    { unsigned char mphdbuf[FMP_MPHD_DISK_SIZE];
        fmp_encode_mphd(mphdbuf, &mphdinfo, mapislsb);
        if (fwrite(mphdbuf, 1, sizeof(mphdbuf), filept)!=sizeof(mphdbuf))
        { merror = ER_MAPSAVEERROR; fclose(filept); return -1; } }

    edhdinfo.xmapoffset=xmapoffset;
    edhdinfo.ymapoffset=ymapoffset;
    edhdinfo.fgcolour=fgcolour;
    edhdinfo.bgcolour=bgcolour;
    edhdinfo.sheight=sheight;
    edhdinfo.swidth=swidth;
    edhdinfo.strtstr=strtstr;
    edhdinfo.strtblk=strtblk;
    edhdinfo.span=32;
    edhdinfo.curstr=curstr;
    edhdinfo.curanim=curanim;
    edhdinfo.animspd=animspd;
    for (i=0;i<MAXBRSH;i++) {
        if (brshpt[i] == NULL) {
            for (k=(i+1);k<MAXBRSH;k++) {
                if (brshpt[k] != NULL) {
                    brshpt[i] = brshpt[k];
                    brshpt[k] = NULL;
                    strcpy(brshname[i], brshname[k]);
                    strcpy(brshname[k], "Brush");
                    if (curbrsh == (k+1)) curbrsh = (i+1);
                    break;
                }
            }
        }
    }
    numbrushes = 0; for (i=0;i<8;i++) {	if (brshpt[i] != NULL) numbrushes++; }
    edhdinfo.numbrushes=numbrushes;
    edhdinfo.pad=0;

    i = 0; if (numbrushes>0) i = SizeBrushes();

    genheader.id1='E'; genheader.id2='D'; genheader.id3='H'; genheader.id4='D';
    genheader.headsize=byteswapl((unsigned long)(i+FMP_EDHD_DISK_SIZE));
    if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }

    { unsigned char edhdbuf[FMP_EDHD_DISK_SIZE];
        fmp_encode_edhd(edhdbuf, &edhdinfo, mapislsb);
        if (fwrite(edhdbuf, 1, sizeof(edhdbuf), filept)!=sizeof(edhdbuf))
        { merror = ER_MAPSAVEERROR; fclose(filept); return -1; } }

    if (numbrushes>0) {
        for (i=0;i<8;i++) {
            if (brshpt[i] != NULL) {
                mybrshpt = brshpt[i];
                k = mybrshpt[0]; k *= mybrshpt[1]; j = (k*2)+4;
                if (maptype>0) {
                    while (k) { k--;
                        if (mybrshpt[2+k] < 0) mybrshpt[2+k] /= FMP_ANI_MEMUNIT;
                    }
                } else {
                    while (k) { k--;
                        if (mybrshpt[2+k] >= 0) mybrshpt[2+k] *= BLOCKSTRSIZE;
                    }
                }
                if (!mapislsb) { int bi2; for (bi2 = 0; bi2 < j/2; bi2++) { unsigned short v = (unsigned short)mybrshpt[bi2]; mybrshpt[bi2] = (short)((v >> 8) | (v << 8)); } }
                if (fwrite(brshpt[i], 1, (size_t)j, filept)!=(size_t)j) { merror = ER_MAPSAVEERROR; fclose(filept); }
                if (!mapislsb) { int bi2; for (bi2 = 0; bi2 < j/2; bi2++) { unsigned short v = (unsigned short)mybrshpt[bi2]; mybrshpt[bi2] = (short)((v >> 8) | (v << 8)); } }
                k = mybrshpt[0]; k *= mybrshpt[1]; j = (k*2)+4;
                if (maptype>0) {
                    while (k) { k--;
                        if (mybrshpt[2+k] < 0) mybrshpt[2+k] *= FMP_ANI_MEMUNIT;
                    }
                } else {
                    while (k) { k--;
                        if (mybrshpt[2+k] >= 0) mybrshpt[2+k] /= BLOCKSTRSIZE;
                    }
                }
        } }
        if (merror) return -1;
        for (i=0;i<8;i++) {
            if (brshpt[i] != NULL) {
                if (fwrite(brshname[i], 1, (strlen(brshname[i]))+1, filept) != ((strlen(brshname[i]))+1)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        } }
        if ((ftell(filept)) & 1) {
            i = 0; if (fwrite(&i, 1, 1, filept)!=1) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        }

        /* more than 8 brushes */
        k = 0; for (i=0;i<MAXBRSH;i++) {	if (brshpt[i] != NULL) k++; }
        if (k > 8) {
            unsigned char exbuf[8];
            exbrshinf[0] = 0;
            exbrshinf[1] = 0;
            exbrshinf[2] = 0;
            exbrshinf[3] = (short)(k-8);
            fmp_put_i16(exbuf + 0, exbrshinf[0], mapislsb);
            fmp_put_i16(exbuf + 2, exbrshinf[1], mapislsb);
            fmp_put_i16(exbuf + 4, exbrshinf[2], mapislsb);
            fmp_put_i16(exbuf + 6, exbrshinf[3], mapislsb);
            if (fwrite(exbuf, 1, 8, filept) != 8) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        }
        for (i=8;i<MAXBRSH;i++) {
            if (brshpt[i] != NULL) {
                mybrshpt = brshpt[i];
                k = mybrshpt[0]; k *= mybrshpt[1]; j = (k*2)+4;
                if (maptype>0) {
                    while (k) { k--;
                        if (mybrshpt[2+k] < 0) mybrshpt[2+k] /= FMP_ANI_MEMUNIT;
                    }
                } else {
                    while (k) { k--;
                        if (mybrshpt[2+k] >= 0) mybrshpt[2+k] *= BLOCKSTRSIZE;
                    }
                }
                if (!mapislsb) { int bi2; for (bi2 = 0; bi2 < j/2; bi2++) { unsigned short v = (unsigned short)mybrshpt[bi2]; mybrshpt[bi2] = (short)((v >> 8) | (v << 8)); } }
                if (fwrite(brshpt[i], 1, (size_t)j, filept)!=(size_t)j) { merror = ER_MAPSAVEERROR; fclose(filept); }
                if (!mapislsb) { int bi2; for (bi2 = 0; bi2 < j/2; bi2++) { unsigned short v = (unsigned short)mybrshpt[bi2]; mybrshpt[bi2] = (short)((v >> 8) | (v << 8)); } }
                k = mybrshpt[0]; k *= mybrshpt[1]; j = (k*2)+4;
                if (maptype>0) {
                    while (k) { k--;
                        if (mybrshpt[2+k] < 0) mybrshpt[2+k] *= FMP_ANI_MEMUNIT;
                    }
                } else {
                    while (k) { k--;
                        if (mybrshpt[2+k] >= 0) mybrshpt[2+k] /= BLOCKSTRSIZE;
                    }
                }
                if (fwrite(brshname[i], 1, (strlen(brshname[i]))+1, filept) != ((strlen(brshname[i]))+1)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
                if (!(strlen(brshname[i])&1)) {
                    if (fwrite("\0", 1, 1, filept)!=1) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
                }
            }
        }
    }

    /* 1.4.23 order after EDHD: EPHD, [OBFN/OBDT,] [TSTR,] LSTR, CMAP. */
    F14_EnsureEPHD();
    if (F14_SaveEPHD(filept, mapislsb)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    if (F14_HasOBFN()) {
        if (F14_SaveOBFN(filept)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    }
    if (F14_HasOBDT()) {
        if (F14_SaveOBDT(filept, mapislsb)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    }
    if (F14_HasTSTR()) {
        if (F14_SaveTSTR(filept, mapislsb)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    }
    {
        int nlay = 0;
        while (nlay < MAXMAPLAYERS && mapmappt[nlay] != NULL) nlay++;
        F14_LstrEnsure(nlay > 0 ? nlay : 1);
    }
    if (F14_SaveLSTR(filept, mapislsb)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    if (TS_HasTSDF()) {
        if (TS_SaveTSDF(filept, mapislsb, maptype, BLOCKSTRSIZE)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    }
    if (TS_HasTSIN()) {
        if (TS_SaveTSIN(filept, mapislsb)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    }

    genheader.id1='C'; genheader.id2='M'; genheader.id3='A'; genheader.id4='P';
    genheader.headsize=byteswapl((unsigned long)(3*256));
    if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    if (fwrite(cmappt, 1, 3*256, filept)!=3*256) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }

    genheader.id1='B'; genheader.id2='K'; genheader.id3='D'; genheader.id4='T';
    genheader.headsize=byteswapl((unsigned long)(numblockstr*BLOCKSTRSIZE));
    if (maptype > 0) {
        mybkpt = (BLKSTR *) blockstrpt;
        i = numblockstr; while (i) {
            mybkpt->bgoff /= (blockwidth*blockheight*((blockdepth+1)/8));
            mybkpt->fgoff /= (blockwidth*blockheight*((blockdepth+1)/8));
            mybkpt->fgoff2 /= (blockwidth*blockheight*((blockdepth+1)/8));
            mybkpt->fgoff3 /= (blockwidth*blockheight*((blockdepth+1)/8));
            mybkpt++;
            i--;
        }
    }
    if (fmp_write_genheader(filept)!=0)
    { merror = ER_MAPSAVEERROR; fclose(filept); } { /* arch-safe BKDT body: encode each BLKSTR explicitly, do NOT dump memory */
        unsigned char blkbuf[FMP_BLKSTR_DISK_SIZE];
        int bi;
        BLKSTR *src = (BLKSTR *) blockstrpt;
        for (bi = 0; bi < numblockstr; bi++) {
            BLKSTR tmp = src[bi];
            if (maptype > 0) {
                /* tmp currently holds memory byte offsets; file wants block indices.
                 * The divide was already done in-place above for maptype>0. */
            }
            /* For maptype>0 the in-place divide above already converted offsets;
             * encode with mapislsb. For maptype==0 memory == file units for bgoff? */
            if (maptype == 0) {
                /* maptype 0 files store byte offsets directly; memory matches */
            }
            fmp_encode_blkstr(blkbuf, &tmp, mapislsb);
            if (fwrite(blkbuf, 1, sizeof(blkbuf), filept)!=sizeof(blkbuf)) { merror = ER_MAPSAVEERROR; fclose(filept); break; }
        }
        if (merror) { /* fall through to restore below */ }
    }
    if (maptype > 0) {
        mybkpt = (BLKSTR *) blockstrpt;
        i = numblockstr; while (i) {
            mybkpt->bgoff *= (blockwidth*blockheight*((blockdepth+1)/8));
            mybkpt->fgoff *= (blockwidth*blockheight*((blockdepth+1)/8));
            mybkpt->fgoff2 *= (blockwidth*blockheight*((blockdepth+1)/8));
            mybkpt->fgoff3 *= (blockwidth*blockheight*((blockdepth+1)/8));
            mybkpt++;
            i--;
        }
    }
    if (merror) return -1;

    if (animstrpt!=NULL) {
        /* NOTE: Important to count the anims. */
        myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--; i = 0;
        j = 0; while (myanpt->antype!=-1) { i++; j++; myanpt--; } numblockanim = i;
        if (numblockanim) {
            InitAnims();
            myanpt = (ANISTR *) (animstrpt+ANDTMEM); i = 0; myanpt--;
            while (myanpt->antype!=-1) { if ((myanpt->anstartoff)<i) i = myanpt->anstartoff;
                myanpt--; }
            i -= 4;
            if (maptype > 0) {
                myanseqpt = (int *) ((animstrpt+ANDTMEM)+i);
                while (myanseqpt != ((int *) myanpt)) {
                    *myanseqpt /= BLOCKSTRSIZE;
                    myanseqpt++;
                }
                while (myanpt != ((ANISTR *) (animstrpt+ANDTMEM))) {
                    logit("anim c %d, s %d, e %d\n", myanpt->ancuroff, myanpt->anstartoff, myanpt->anendoff);
                    myanpt->ancuroff -= i;
                    myanpt->ancuroff /= FMP_SEQ_MEMUNIT;
                    myanpt->anstartoff -= i;
                    myanpt->anstartoff /= FMP_SEQ_MEMUNIT;
                    myanpt->anendoff -= i;
                    myanpt->anendoff /= FMP_SEQ_MEMUNIT;
                    logit("now c %d, s %d, e %d\n\n", myanpt->ancuroff, myanpt->anstartoff, myanpt->anendoff);
                    myanpt++;
                }
            }

            genheader.id1='A'; genheader.id2='N'; genheader.id3='D'; genheader.id4='T';
            genheader.headsize=byteswapl((unsigned long)(-i));
            if (fmp_write_genheader(filept)!=0)
            { merror = ER_MAPSAVEERROR; fclose(filept); return -1; } { /* arch/endian-safe ANDT body.
       * Memory layout tail is: [seq int32s][ANISTR array]. On LE host with
       * LE file (mapislsb==1, the normal case) raw bytes are correct.
       * For BE files, byteswap each int32 (keeping ANISTR type/delay/count/user bytes). */
                unsigned char *body = (unsigned char *)(animstrpt+ANDTMEM+i);
                int bodylen = -i;
                if (!mapislsb) {
                    ANISTR *first = (ANISTR *)(animstrpt+ANDTMEM) - (numblockanim+1);
                    unsigned char *seq_end = (unsigned char *)first;
                    int o;
                    for (o = 0; o + 4 <= bodylen && body + o + 4 <= seq_end; o += 4) {
                        unsigned char t = body[o]; body[o] = body[o+3]; body[o+3] = t;
                        t = body[o+1]; body[o+1] = body[o+2]; body[o+2] = t;
                    }
                    for (; o + 16 <= bodylen; o += 16) {
                        int k;
                        for (k = 0; k < 3; k++) {
                            unsigned char *w = body + o + 4 + k*4;
                            unsigned char t = w[0]; w[0] = w[3]; w[3] = t;
                            t = w[1]; w[1] = w[2]; w[2] = t;
                        }
                    }
                    if (fwrite(body, 1, (size_t) bodylen, filept)!=(size_t) bodylen) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
                    /* swap back to restore in-memory LE layout */
                    for (o = 0; o + 4 <= bodylen && body + o + 4 <= seq_end; o += 4) {
                        unsigned char t = body[o]; body[o] = body[o+3]; body[o+3] = t;
                        t = body[o+1]; body[o+1] = body[o+2]; body[o+2] = t;
                    }
                    for (; o + 16 <= bodylen; o += 16) {
                        int k;
                        for (k = 0; k < 3; k++) {
                            unsigned char *w = body + o + 4 + k*4;
                            unsigned char t = w[0]; w[0] = w[3]; w[3] = t;
                            t = w[1]; w[1] = w[2]; w[2] = t;
                        }
                    }
                } else {
                    if (fwrite(body, 1, (size_t) bodylen, filept)!=(size_t) bodylen) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
                }
            }

            if (maptype > 0) {
                myanseqpt = (int *) ((animstrpt+ANDTMEM)+i);
                do {
                    myanpt--;
                    logit("2anim c %d, s %d, e %d\n", myanpt->ancuroff, myanpt->anstartoff, myanpt->anendoff);
                    myanpt->ancuroff *= FMP_SEQ_MEMUNIT;
                    myanpt->ancuroff += i;
                    myanpt->anstartoff *= FMP_SEQ_MEMUNIT;
                    myanpt->anstartoff += i;
                    myanpt->anendoff *= FMP_SEQ_MEMUNIT;
                    myanpt->anendoff += i;
                    logit("2now c %d, s %d, e %d\n\n", myanpt->ancuroff, myanpt->anstartoff, myanpt->anendoff);
                } while (myanpt->antype!=-1);
                while (myanseqpt != ((int *) myanpt)) {
                    *myanseqpt *= BLOCKSTRSIZE;
                    myanseqpt++;
                }
                if (merror) return -1;
            }
        }
    }

    /* Write any data of unknown type that was loaded in previously */
    {
        FmpUnknownChunk *unk = chunklist;
        while (unk!=NULL) {
            unsigned char hdr[FMP_GENHEAD_DISK_SIZE];
            memcpy(hdr, unk->id, 4);
            fmp_put_be32(hdr + 4, unk->size);
            if (fwrite(hdr, 1, sizeof(hdr), filept)!=sizeof(hdr)) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
            if (fwrite((unsigned char *)(unk + 1) + 8, 1, (size_t) unk->size, filept)!=(size_t) unk->size) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
            unk = unk->next;
        }
    }

    if (altgfxpt != NULL && !skipgfx) {
        genheader.id1='A'; genheader.id2='G'; genheader.id3='F'; genheader.id4='X';
        if ((numblockgfx*blockheight*blockwidth)&1)
            genheader.headsize=byteswapl((unsigned long)((numblockgfx*blockheight*blockwidth)+1));
        else
            genheader.headsize=byteswapl((unsigned long)(numblockgfx*blockheight*blockwidth));
        if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }

        if (fwrite(altgfxpt, 1, (size_t) numblockgfx*blockheight*blockwidth,
            filept)!=(size_t) numblockgfx*blockheight*blockwidth) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        if ((numblockgfx*blockheight*blockwidth)&1) {
            if (fwrite(altgfxpt, 1, 1, filept)!=1) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        }
    }

    if (novctext[0] != 0) {
        genheader.id1='N'; genheader.id2='O'; genheader.id3='V'; genheader.id4='C';
        i = (int)strlen(novctext); i++;
        if (i&1) i++;
        genheader.headsize=byteswapl((unsigned long)i);
        if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        if (fwrite(novctext, 1, (size_t) i, filept)!=(size_t) i) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
    }

    if (!skipgfx) {
        genheader.id1='B'; genheader.id2='G'; genheader.id3='F'; genheader.id4='X';
        if (sdepth==15) genheader.headsize=byteswapl((unsigned long)(numblockgfx*blockheight*blockwidth*2)); else {
            if ((numblockgfx*blockheight*blockwidth*(blockdepth/8))&1)
                genheader.headsize=byteswapl((unsigned long)((numblockgfx*blockheight*blockwidth*(blockdepth/8))+1));
            else
                genheader.headsize=byteswapl((unsigned long)(numblockgfx*blockheight*blockwidth*(blockdepth/8)));
        }
        if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; fclose(filept); return -1; }
        if (sdepth==15) { if (fwrite(blockgfxpt, 1, (size_t) numblockgfx*blockheight*blockwidth*2,
                filept)!=(size_t) numblockgfx*blockheight*blockwidth*2)
            { merror = ER_MAPSAVEERROR; fclose(filept); return -1; } } else
        { if (fwrite(blockgfxpt, 1, (size_t) numblockgfx*blockheight*blockwidth*(blockdepth/8),
                filept)!=(size_t) numblockgfx*blockheight*blockwidth*(blockdepth/8))
            { merror = ER_MAPSAVEERROR; fclose(filept); return -1; } }
        if ((numblockgfx*blockheight*blockwidth*(blockdepth/8))&1)
        { if (fwrite(blockgfxpt, 1, 1, filept)!=1)
            { merror = ER_MAPSAVEERROR; fclose(filept); return -1; } }
    }

    for (i=0;i<MAXMAPLAYERS;i++) { if (mapmappt[i]!=NULL) {
            if (i == 0) {
                genheader.id1='B'; genheader.id2='O'; genheader.id3='D'; genheader.id4='Y';
            } else if (i < 8) {
                genheader.id1='L'; genheader.id2='Y'; genheader.id3='R'; genheader.id4=i+48;
            } else {
                /* Layers 8..99: NLYR + 16-byte prefix (1.4.23 @00429423) */
                genheader.id1='N'; genheader.id2='L'; genheader.id3='Y'; genheader.id4='R';
            }
            if (i >= 8)
                genheader.headsize=byteswapl((unsigned long)(mapwidth*mapheight*2 + 16));
            else
                genheader.headsize=byteswapl((unsigned long)(mapwidth*mapheight*2));

            if (maptype > 0) {
                j = ((int) mapwidth)*((int) mapheight);
                while (j) { j--;
                    if (mapmappt[i][j] < 0) mapmappt[i][j] /= FMP_ANI_MEMUNIT;
                }
            } else {
                j = ((int) mapwidth)*((int) mapheight);
                while (j) { j--;
                    if (mapmappt[i][j] >= 0) mapmappt[i][j] *= BLOCKSTRSIZE;
                }
            }

            if (maptype == 2) {
                if (RleLayer(mapmappt[i], filept)) merror = ER_MAPSAVEERROR;
            } else {
                if (fmp_write_genheader(filept)!=0) { merror = ER_MAPSAVEERROR; }
                if (!merror && i >= 8) {
                    if (F14_WriteNLYRPrefix(filept, i, mapwidth, mapheight, mapislsb)) { merror = ER_MAPSAVEERROR; }
                }
                if (!merror) { /* endian-safe BODY/LYR: file shorts obey mapislsb */
                    int cells = ((int)mapwidth)*((int)mapheight);
                    if (!mapislsb) {
                        int ci;
                        for (ci = 0; ci < cells; ci++) {
                            unsigned short v = (unsigned short)mapmappt[i][ci];
                            mapmappt[i][ci] = (short)((v >> 8) | (v << 8));
                        }
                        if (fwrite((char *)mapmappt[i], 1, (size_t)cells*2, filept)!=(size_t)cells*2) { merror = ER_MAPSAVEERROR; }
                        for (ci = 0; ci < cells; ci++) {
                            unsigned short v = (unsigned short)mapmappt[i][ci];
                            mapmappt[i][ci] = (short)((v >> 8) | (v << 8));
                        }
                    } else {
                        if (fwrite((char *)mapmappt[i], 1, (size_t)cells*2, filept)!=(size_t)cells*2) { merror = ER_MAPSAVEERROR; }
                    }
                }
            }

            if (maptype > 0) {
                j = ((int) mapwidth)*((int) mapheight);
                while (j) { j--;
                    if (mapmappt[i][j] < 0) mapmappt[i][j] *= FMP_ANI_MEMUNIT;
                }
            } else {
                j = ((int) mapwidth)*((int) mapheight);
                while (j) { j--;
                    if (mapmappt[i][j] >= 0) mapmappt[i][j] /= BLOCKSTRSIZE;
                }
            }
            if (merror) { fclose(filept); return -1; }
    } }

    if (merror) { fclose(filept); return -1; }

    fseek(filept, 0, SEEK_END);
    {
        long fsize = ftell(filept);
        unsigned char szbuf[4];
        fsize -= 8;
        fmp_put_be32(szbuf, (uint32_t) fsize);
        fseek(filept, 4, SEEK_SET);
        fwrite(szbuf, 1, 4, filept);
    }
    fclose(filept);
    //	if (dbgfpt != NULL) { logit ("MapSave done\n"); fclose (dbgfpt); dbgfpt = NULL; }
    return 0;
}

/*
void DrawBlock (unsigned char * blkgfxpt, unsigned char * destpt, int xoff, int yoff, int linesize)
{
int i, j;

    destpt += (xoff*3)+(yoff*linesize);
    switch (blockdepth)
    {
    case 8:
        for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
        destpt[j*3] = cmappt[(*blkgfxpt)*3+2];
        destpt[j*3+1] = cmappt[(*blkgfxpt)*3+1];
        destpt[j*3+2] = cmappt[(*blkgfxpt)*3];
        blkgfxpt++; }
        destpt += linesize; }
        break;
    case 15:
        for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
        destpt[j*3] = (*(blkgfxpt+1)&0x1F)<<3;
        destpt[j*3+1] = (((((*blkgfxpt)&0x3)<<3)|((*(blkgfxpt+1))>>5)))<<3;
        destpt[j*3+2] = (*blkgfxpt&0xFC)<<1;
        destpt[j*3] |= (destpt[j*3]>>5);
        destpt[j*3+1] |= (destpt[j*3+1]>>5);
        destpt[j*3+2] |= (destpt[j*3+2]>>5);
        blkgfxpt+=2; }
        destpt += linesize; }
        break;
    case 16:
        for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
        destpt[j*3] = (*(blkgfxpt+1)&0x1F)<<3;
        destpt[j*3+1] = (((((*blkgfxpt)&0x7)<<3)|((*(blkgfxpt+1))>>5)))<<2;
        destpt[j*3+2] = (*blkgfxpt&0xF8);
        destpt[j*3] |= (destpt[j*3]>>5);
        destpt[j*3+1] |= (destpt[j*3+1]>>6);
        destpt[j*3+2] |= (destpt[j*3+2]>>5);
        blkgfxpt+=2; }
        destpt += linesize; }
        break;
    case 24:
        for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
        destpt[j*3] = blkgfxpt[2];
        destpt[j*3+1] = blkgfxpt[1];
        destpt[j*3+2] = blkgfxpt[0];
        blkgfxpt+=3; }
        destpt += linesize; }
        break;
    case 32:
        for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
        destpt[j*3] = blkgfxpt[3];
        destpt[j*3+1] = blkgfxpt[2];
        destpt[j*3+2] = blkgfxpt[1];
        blkgfxpt+=4; }
        destpt += linesize; }
        break;
    }
}
*/

int IsBlockAttached(int blnum) {
    if (blnum == 0 || isopillars == 0) return 0;
    if (blnum >= numblockstr) return 0;

    if (((BLKSTR *) blockstrpt)[blnum-1].unused1) return 1; else return 0;
}

void DrawBlock(unsigned char * blkgfxpt, unsigned char * destpt, int xoff, int yoff, int linesize, int flippart) {
    int i, j, istrt, iend, iadd, jstrt, jend, jadd;

    destpt += (xoff*3)+(yoff*linesize);

    if (!flippedtiles) {
        switch (blockdepth) {
        case 8:
            for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
                    destpt[j*3] = cmappt[(*blkgfxpt)*3+2];
                    destpt[j*3+1] = cmappt[(*blkgfxpt)*3+1];
                    destpt[j*3+2] = cmappt[(*blkgfxpt)*3];
                    blkgfxpt++; }
                destpt += linesize; }
            break;
        case 15:
            for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
                    if (!(*(blkgfxpt)==trans16hi && *(blkgfxpt+1)==trans16low)) {
                        destpt[j*3] = (*(blkgfxpt+1)&0x1F)<<3;
                        destpt[j*3+1] = (((((*blkgfxpt)&0x3)<<3)|((*(blkgfxpt+1))>>5)))<<3;
                        destpt[j*3+2] = (*blkgfxpt&0xFC)<<1;
                    } else {
                        destpt[j*3] = 0;
                        destpt[j*3+1] = 0;
                        destpt[j*3+2] = 0;
                    }
                    blkgfxpt+=2; }
                destpt += linesize; }
            break;
        case 16:
            for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
                    if (!(*(blkgfxpt)==trans16hi && *(blkgfxpt+1)==trans16low)) {
                        destpt[j*3] = (*(blkgfxpt+1)&0x1F)<<3;
                        destpt[j*3+1] = (((((*blkgfxpt)&0x7)<<3)|((*(blkgfxpt+1))>>5)))<<2;
                        destpt[j*3+2] = (*blkgfxpt&0xF8);
                    } else {
                        destpt[j*3] = 0;
                        destpt[j*3+1] = 0;
                        destpt[j*3+2] = 0;
                    }
                    blkgfxpt+=2; }
                destpt += linesize; }
            break;
        case 24:
            for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
                    if (!(*blkgfxpt==transred && *(blkgfxpt+1)==transgreen && *(blkgfxpt+2)==transblue)) {
                        destpt[j*3] = blkgfxpt[2];
                        destpt[j*3+1] = blkgfxpt[1];
                        destpt[j*3+2] = blkgfxpt[0];
                    } else {
                        destpt[j*3] = 0;
                        destpt[j*3+1] = 0;
                        destpt[j*3+2] = 0;
                    }
                    blkgfxpt+=3; }
                destpt += linesize; }
            break;
        case 32:
            for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
                    if (!(*(blkgfxpt+1)==transred && *(blkgfxpt+2)==transgreen && *(blkgfxpt+3)==transblue)) {
                        destpt[j*3] = blkgfxpt[3];
                        destpt[j*3+1] = blkgfxpt[2];
                        destpt[j*3+2] = blkgfxpt[1];
                    } else {
                        destpt[j*3] = 0;
                        destpt[j*3+1] = 0;
                        destpt[j*3+2] = 0;
                    }
                    blkgfxpt+=4; }
                destpt += linesize; }
            break;
        }
        return;
    }

    //flipped tiles
    if (flippart&1) {
        jstrt = (blockwidth-1); jend = -1; jadd = -1;
    } else {
        jstrt = 0; jend = blockwidth; jadd = 1;
    }
    if (flippart&2) {
        istrt = (blockheight-1); iend = -1; iadd = -1;
    } else {
        istrt = 0; iend = blockheight; iadd = 1;
    }

    switch (blockdepth) {
    case 8:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                destpt[(i*linesize)+(j*3)] = cmappt[(*blkgfxpt)*3+2];
                destpt[(i*linesize)+(j*3+1)] = cmappt[(*blkgfxpt)*3+1];
                destpt[(i*linesize)+(j*3+2)] = cmappt[(*blkgfxpt)*3];
                blkgfxpt++; }
        }
        break;
    case 15:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                if (!(*(blkgfxpt)==trans16hi && *(blkgfxpt+1)==trans16low)) {
                    destpt[(i*linesize)+(j*3)] = (*(blkgfxpt+1)&0x1F)<<3;
                    destpt[(i*linesize)+(j*3+1)] = (((((*blkgfxpt)&0x3)<<3)|((*(blkgfxpt+1))>>5)))<<3;
                    destpt[(i*linesize)+(j*3+2)] = (*blkgfxpt&0xFC)<<1;
                } else {
                    destpt[(i*linesize)+(j*3)] = 0;
                    destpt[(i*linesize)+(j*3+1)] = 0;
                    destpt[(i*linesize)+(j*3+2)] = 0;
                }
                blkgfxpt+=2; }
        }
        break;
    case 16:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                if (!(*(blkgfxpt)==trans16hi && *(blkgfxpt+1)==trans16low)) {
                    destpt[(i*linesize)+(j*3)] = (*(blkgfxpt+1)&0x1F)<<3;
                    destpt[(i*linesize)+(j*3+1)] = (((((*blkgfxpt)&0x7)<<3)|((*(blkgfxpt+1))>>5)))<<2;
                    destpt[(i*linesize)+(j*3+2)] = (*blkgfxpt&0xF8);
                } else {
                    destpt[(i*linesize)+(j*3)] = 0;
                    destpt[(i*linesize)+(j*3+1)] = 0;
                    destpt[(i*linesize)+(j*3+2)] = 0;
                }
                blkgfxpt+=2; }
        }
        break;
    case 24:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                if (!(*blkgfxpt==transred && *(blkgfxpt+1)==transgreen && *(blkgfxpt+2)==transblue)) {
                    destpt[(i*linesize)+(j*3)] = blkgfxpt[2];
                    destpt[(i*linesize)+(j*3+1)] = blkgfxpt[1];
                    destpt[(i*linesize)+(j*3+2)] = blkgfxpt[0];
                } else {
                    destpt[(i*linesize)+(j*3)] = 0;
                    destpt[(i*linesize)+(j*3+1)] = 0;
                    destpt[(i*linesize)+(j*3+2)] = 0;
                }
                blkgfxpt+=3; }
        }
        break;
    case 32:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                if (!(*(blkgfxpt+1)==transred && *(blkgfxpt+2)==transgreen && *(blkgfxpt+3)==transblue)) {
                    destpt[(i*linesize)+(j*3)] = blkgfxpt[3];
                    destpt[(i*linesize)+(j*3+1)] = blkgfxpt[2];
                    destpt[(i*linesize)+(j*3+2)] = blkgfxpt[1];
                } else {
                    destpt[(i*linesize)+(j*3)] = 0;
                    destpt[(i*linesize)+(j*3+1)] = 0;
                    destpt[(i*linesize)+(j*3+2)] = 0;
                }
                blkgfxpt+=4; }
        }
        break;
    }
}

/* 1.4.19: 32-bit overlays use source alpha (byte 0), RGB in 1..3. */
static void Blend32Over(unsigned char * dest, const unsigned char * src) {
    unsigned a;
    if (src[1]==transred && src[2]==transgreen && src[3]==transblue) return;
    a = src[0];
    if (a==0) return;
    if (a>=255) {
        dest[0] = src[3]; dest[1] = src[2]; dest[2] = src[1];
        return;
    }
    dest[0] = (unsigned char) ((src[3]*(int)a + dest[0]*(255-(int)a))/255);
    dest[1] = (unsigned char) ((src[2]*(int)a + dest[1]*(255-(int)a))/255);
    dest[2] = (unsigned char) ((src[1]*(int)a + dest[2]*(255-(int)a))/255);
}

void OverBlock(unsigned char * blkgfxpt, unsigned char * destpt, int xoff, int yoff, int linesize, int isopart) {
    int i, j, istrt, iend, iadd, jstrt, jend, jadd;

    destpt += (xoff*3)+(yoff*linesize);
    /*	switch (isopart) {
        case 1:
            stpos = 0; enpos = blockwidth/2;
            break;
        case 2:
            stpos = blockwidth/2; enpos = blockwidth;
            blkgfxpt += (stpos*((blockdepth+1)/8));
            break;
        default:
            stpos = 0; enpos = blockwidth;
            break;
    }
*/
    istrt = 0; iend = blockheight; iadd = 1;
    jstrt = 0; jend = blockwidth; jadd = 1;

    if (flippedtiles) {
        //flipped tiles
        if (isopart&1) {
            jstrt = (blockwidth-1); jend = -1; jadd = -1;
        } else {
            jstrt = 0; jend = blockwidth; jadd = 1;
        }
        if (isopart&2) {
            istrt = (blockheight-1); iend = -1; iadd = -1;
        } else {
            istrt = 0; iend = blockheight; iadd = 1;
        }
    } else {
        //isometric split pillars
        if (isopart == 3) isopart = 0;
        if (isopart&1) { jstrt = 0; jend = blockwidth/2; jadd = 1; }
        if (isopart&2) { jstrt = blockwidth/2; jend = blockwidth; jadd = 1; blkgfxpt += (jstrt*((blockdepth+1)/8)); }
    }

    switch (blockdepth) {
    case 8:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                if (*blkgfxpt!=trans8bit) {
                    destpt[(i*linesize)+(j*3)] = cmappt[(*blkgfxpt)*3+2];
                    destpt[(i*linesize)+(j*3+1)] = cmappt[(*blkgfxpt)*3+1];
                    destpt[(i*linesize)+(j*3+2)] = cmappt[(*blkgfxpt)*3]; }
                blkgfxpt++; }
            if (isopart && !flippedtiles) blkgfxpt += blockwidth/2;
        }
        break;
    case 15:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                if (!(*(blkgfxpt)==trans16hi && *(blkgfxpt+1)==trans16low)) {
                    destpt[(i*linesize)+(j*3)] = (*(blkgfxpt+1)&0x1F)<<3;
                    destpt[(i*linesize)+(j*3+1)] = (((((*blkgfxpt)&0x3)<<3)|((*(blkgfxpt+1))>>5)))<<3;
                    destpt[(i*linesize)+(j*3+2)] = (*blkgfxpt&0xFC)<<1; }
                blkgfxpt+=2; }
            if (isopart && !flippedtiles) blkgfxpt += blockwidth;
        }
        break;
    case 16:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                if (!(*(blkgfxpt)==trans16hi && *(blkgfxpt+1)==trans16low)) {
                    destpt[(i*linesize)+(j*3)] = (*(blkgfxpt+1)&0x1F)<<3;
                    destpt[(i*linesize)+(j*3+1)] = (((((*blkgfxpt)&0x7)<<3)|((*(blkgfxpt+1))>>5)))<<2;
                    destpt[(i*linesize)+(j*3+2)] = (*blkgfxpt&0xF8); }
                blkgfxpt+=2; }
            if (isopart && !flippedtiles) blkgfxpt += blockwidth;
        }
        break;
    case 24:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                if (!(*blkgfxpt==transred && *(blkgfxpt+1)==transgreen && *(blkgfxpt+2)==transblue)) {
                    destpt[(i*linesize)+(j*3)] = blkgfxpt[2];
                    destpt[(i*linesize)+(j*3+1)] = blkgfxpt[1];
                    destpt[(i*linesize)+(j*3+2)] = blkgfxpt[0]; }
                blkgfxpt+=3; }
            if (isopart && !flippedtiles) blkgfxpt += (blockwidth)+(blockwidth/2);
        }
        break;
    case 32:
        for (i=istrt;i!=iend;i+=iadd) {	for (j=jstrt;j!=jend;j+=jadd) {
                Blend32Over(destpt+(i*linesize)+(j*3), blkgfxpt);
                blkgfxpt+=4; }
            if (isopart && !flippedtiles) blkgfxpt += blockwidth*2;
        }
        break;
    }
    /*	switch (blockdepth)
    {
    case 8:
        for (i=0;i<blockheight;i++) {	for (j=stpos;j<enpos;j++) {
        if (*blkgfxpt!=trans8bit) {
        destpt[j*3] = cmappt[(*blkgfxpt)*3+2];
        destpt[j*3+1] = cmappt[(*blkgfxpt)*3+1];
        destpt[j*3+2] = cmappt[(*blkgfxpt)*3]; }
        blkgfxpt++; }
        if (isopart) blkgfxpt += blockwidth/2;
        destpt += linesize; }
        break;
    case 15:
        for (i=0;i<blockheight;i++) {	for (j=stpos;j<enpos;j++) {
        if (!(*(blkgfxpt)==trans16hi && *(blkgfxpt+1)==trans16low)) {
        destpt[j*3] = (*(blkgfxpt+1)&0x1F)<<3;
        destpt[j*3+1] = (((((*blkgfxpt)&0x3)<<3)|((*(blkgfxpt+1))>>5)))<<3;
        destpt[j*3+2] = (*blkgfxpt&0xFC)<<1; }
        blkgfxpt+=2; }
        if (isopart) blkgfxpt += blockwidth;
        destpt += linesize; }
        break;
    case 16:
        for (i=0;i<blockheight;i++) {	for (j=stpos;j<enpos;j++) {
        if (!(*(blkgfxpt)==trans16hi && *(blkgfxpt+1)==trans16low)) {
        destpt[j*3] = (*(blkgfxpt+1)&0x1F)<<3;
        destpt[j*3+1] = (((((*blkgfxpt)&0x7)<<3)|((*(blkgfxpt+1))>>5)))<<2;
        destpt[j*3+2] = (*blkgfxpt&0xF8); }
        blkgfxpt+=2; }
        if (isopart) blkgfxpt += blockwidth;
        destpt += linesize; }
        break;
    case 24:
        for (i=0;i<blockheight;i++) {	for (j=stpos;j<enpos;j++) {
        if (!(*blkgfxpt==transred && *(blkgfxpt+1)==transgreen && *(blkgfxpt+2)==transblue)) {
        destpt[j*3] = blkgfxpt[2];
        destpt[j*3+1] = blkgfxpt[1];
        destpt[j*3+2] = blkgfxpt[0]; }
        blkgfxpt+=3; }
        if (isopart) blkgfxpt += (blockwidth+(blockwidth/2));
        destpt += linesize; }
        break;
    case 32:
        for (i=0;i<blockheight;i++) {	for (j=stpos;j<enpos;j++) {
        if (!(*(blkgfxpt+1)==transred && *(blkgfxpt+2)==transgreen && *(blkgfxpt+3)==transblue)) {
        destpt[j*3] = blkgfxpt[3];
        destpt[j*3+1] = blkgfxpt[2];
        destpt[j*3+2] = blkgfxpt[1]; }
        blkgfxpt+=4; }
        if (isopart) blkgfxpt += blockwidth*2;
        destpt += linesize; }
        break;
    }
*/
}

static void RotRect24(unsigned char * destpt, int xoff, int yoff, int linesize, int rot);

static BLKSTR * MapCellBlkstr(short int cell) {
    ANISTR * myanpt;
    long int * templpt;
    int boff;

    if (blockstrpt==NULL) return NULL;
    if (cell >= 0) {
        if (cell >= numblockstr) return NULL;
        return (BLKSTR *) (((char *)blockstrpt) + (((int)cell)*FMP_BLK_MEMUNIT));
    }
    if (animstrpt==NULL) return NULL;
    myanpt = (ANISTR *) ((animstrpt+ANDTMEM)+cell);
    if ((unsigned char *) myanpt < (unsigned char *) animstrpt ||
        (unsigned char *) (myanpt + 1) > (unsigned char *) animstrpt + ANDTMEM)
    return NULL;
    templpt = (long int *) ((animstrpt+ANDTMEM)+(myanpt->anstartoff));
    templpt--;
    if ((unsigned char *) templpt < (unsigned char *) animstrpt ||
        (unsigned char *) (templpt + 1) > (unsigned char *) animstrpt + ANDTMEM)
    return NULL;
    boff = (int) (*templpt);
    if (boff < 0 || (boff % FMP_BLK_MEMUNIT) || (boff / FMP_BLK_MEMUNIT) >= numblockstr)
        return NULL;
    return (BLKSTR *) (blockstrpt + boff);
}

void Onionto24(int x, int y, unsigned char * destpt, int xoff, int yoff, int linesize, int olayer) {
    int i, isop;
    short int * mymappt;
    unsigned char * srcpt;
    BLKSTR * blkdatapt;

    srcpt = (unsigned char *) blockgfxpt;
    if (x<mapwidth && y<mapheight && x>=0 && y>=0) {
        if (olayer >= MAXMAPLAYERS) return;
        if (olayer<0) mymappt = mappt;
        else mymappt = mapmappt[olayer];
        if (mymappt == NULL) return;
        mymappt += x+(y*mapwidth);
        blkdatapt = MapCellBlkstr(*mymappt);
        if (blkdatapt == NULL) return;

        if (blockstaggerx || blockstaggery) {
            if (blkdatapt->bgoff) {
                OverBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, 0);
            }
            isop = 0;
            if (isopillars && blkdatapt->unused2 && !blkdatapt->unused3) isop = 1;
            if (isopillars && !blkdatapt->unused2 && blkdatapt->unused3) isop = 2;
            if (blkdatapt->fgoff) {
                OverBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, isop);
            }
            if (isopillars) yoff -= blockheight;
            if (yoff < 0 || isoreveal) return;
            if (blkdatapt->fgoff2) {
                OverBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, isop);
            }
            if (isopillars) yoff -= blockheight;
            if (yoff < 0) return;
            if (blkdatapt->fgoff3) {
                OverBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, isop);
            }
            while (blkdatapt->unused1) {
                blkdatapt++;
                isop = 0;
                if (isopillars && blkdatapt->unused2 && !blkdatapt->unused3) isop = 1;
                if (isopillars && !blkdatapt->unused2 && blkdatapt->unused3) isop = 2;
                if (isopillars) yoff -= blockheight;
                if (yoff < 0) return;
                if (blkdatapt->bgoff) {
                    OverBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, isop);
                }
                if (isopillars) yoff -= blockheight;
                if (yoff < 0) return;
                if (blkdatapt->fgoff) {
                    OverBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, isop);
                }
                if (isopillars) yoff -= blockheight;
                if (yoff < 0) return;
                if (blkdatapt->fgoff2) {
                    OverBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, isop);
                }
                if (isopillars) yoff -= blockheight;
                if (yoff < 0) return;
                if (blkdatapt->fgoff3) {
                    OverBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, isop);
                }
            }
        } else {
            DrawBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, 0);
            if (blkdatapt->fgoff) {
                OverBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, 0);
            }
            if (blkdatapt->fgoff2) {
                OverBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, 0);
            }
            if (blkdatapt->fgoff3) {
                OverBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, 0);
            }
            if (rotatedtiles && blkdatapt->user7>=1 && blkdatapt->user7<=3)
                RotRect24(destpt, xoff, yoff, linesize, blkdatapt->user7);
        }
    } else {
        destpt += (xoff*3)+(yoff*linesize);
        for (i=0;i<blockheight;i++) {
            memset(destpt, 0x44, blockwidth*3);
            destpt += linesize;
        }
    }
}

/* OnionCompto24: composite layers 0..top (skipping NULL) as onion backdrop.
 * Used for onion==ONION_ALL and onionlower ("show lower layers"). */
void OnionCompto24(int x, int y, unsigned char * destpt, int xoff, int yoff, int linesize, int top) {
    int l;

    if (top<0) return;
    if (top>=MAXMAPLAYERS) top = MAXMAPLAYERS-1;
    for (l=0;l<=top;l++) {
        if (mapmappt[l]==NULL) continue;
        Onionto24(x, y, destpt, xoff, yoff, linesize, l);
    }
}

void Overlayto24(int x, int y, unsigned char * destpt, int xoff, int yoff, int linesize, int olayer) {
    int i, isop, rotu7;
    short int * mymappt;
    unsigned char * srcpt;
    BLKSTR * blkdatapt;

    srcpt = (unsigned char *) blockgfxpt;
    if (x<mapwidth && y<mapheight && x>=0 && y>=0) {
        if (olayer >= MAXMAPLAYERS) return;
        if (olayer<0) mymappt = mappt;
        else mymappt = mapmappt[olayer];
        if (mymappt == NULL) return;
        mymappt += x+(y*mapwidth);
        blkdatapt = MapCellBlkstr(*mymappt);
        if (blkdatapt == NULL) return;
        rotu7 = (rotatedtiles && blkdatapt->user7>=1 && blkdatapt->user7<=3) ? blkdatapt->user7 : 0;

        if (blkdatapt->bgoff) {
            OverBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, 0);
        }
        isop = 0;
        if (isopillars && blkdatapt->unused2 && !blkdatapt->unused3) isop = 1;
        if (isopillars && !blkdatapt->unused2 && blkdatapt->unused3) isop = 2;
        if (blkdatapt->fgoff) {
            OverBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, isop);
        }
        if (isopillars) yoff -= blockheight;
        if (yoff < 0 || isoreveal) return;
        if (blkdatapt->fgoff2) {
            OverBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, isop);
        }
        if (isopillars) yoff -= blockheight;
        if (yoff < 0) return;
        if (blkdatapt->fgoff3) {
            OverBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, isop);
        }
        while (blkdatapt->unused1) {
            blkdatapt++;
            isop = 0;
            if (isopillars && blkdatapt->unused2 && !blkdatapt->unused3) isop = 1;
            if (isopillars && !blkdatapt->unused2 && blkdatapt->unused3) isop = 2;
            if (isopillars) yoff -= blockheight;
            if (yoff < 0) return;
            if (blkdatapt->bgoff) {
                OverBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, isop);
            }
            if (isopillars) yoff -= blockheight;
            if (yoff < 0) return;
            if (blkdatapt->fgoff) {
                OverBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, isop);
            }
            if (isopillars) yoff -= blockheight;
            if (yoff < 0) return;
            if (blkdatapt->fgoff2) {
                OverBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, isop);
            }
            if (isopillars) yoff -= blockheight;
            if (yoff < 0) return;
            if (blkdatapt->fgoff3) {
                OverBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, isop);
            }
        }
        if (rotu7 && !isopillars) RotRect24(destpt, xoff, yoff, linesize, rotu7);
    } else {
        destpt += (xoff*3)+(yoff*linesize);
        for (i=0;i<blockheight;i++) {
            memset(destpt, 0x44, blockwidth*3);
            destpt += linesize;
        }
    }
}

/* RotRect24: USER7 rotation for rotatedtiles mode (1=90, 2=180, 3=270).
 * Operates on the rendered 24-bit tile rect in place. 90/270 need square tiles. */
static void RotRect24(unsigned char * destpt, int xoff, int yoff, int linesize, int rot) {
    int w = blockwidth, h = blockheight, x, y;
    unsigned char * tmp, * sp, * dp;
    unsigned char t0, t1, t2;

    if (rot==2) {
        tmp = malloc(w*3);
        if (tmp==NULL) return;
        for (y=0;y<h/2;y++) {
            unsigned char * ra = destpt+((yoff+y)*linesize)+(xoff*3);
            unsigned char * rb = destpt+((yoff+h-1-y)*linesize)+(xoff*3);
            memcpy(tmp, ra, w*3);
            for (x=0;x<w;x++) {
                ra[x*3] = rb[(w-1-x)*3]; ra[x*3+1] = rb[(w-1-x)*3+1]; ra[x*3+2] = rb[(w-1-x)*3+2];
            }
            for (x=0;x<w;x++) {
                rb[x*3] = tmp[(w-1-x)*3]; rb[x*3+1] = tmp[(w-1-x)*3+1]; rb[x*3+2] = tmp[(w-1-x)*3+2];
            }
        }
        if (h&1) {
            unsigned char * rm = destpt+((yoff+h/2)*linesize)+(xoff*3);
            for (x=0;x<w/2;x++) {
                t0 = rm[x*3]; t1 = rm[x*3+1]; t2 = rm[x*3+2];
                rm[x*3] = rm[(w-1-x)*3]; rm[x*3+1] = rm[(w-1-x)*3+1]; rm[x*3+2] = rm[(w-1-x)*3+2];
                rm[(w-1-x)*3] = t0; rm[(w-1-x)*3+1] = t1; rm[(w-1-x)*3+2] = t2;
            }
        }
        free(tmp);
        return;
    }
    if (w!=h) return;
    tmp = malloc(w*h*3);
    if (tmp==NULL) return;
    sp = destpt+(yoff*linesize)+(xoff*3);
    for (y=0;y<h;y++) {
        for (x=0;x<w;x++) {
            if (rot==1) dp = tmp+((y*w+x)*3), sp = destpt+((yoff+h-1-x)*linesize)+((xoff+y)*3);
            else dp = tmp+((y*w+x)*3), sp = destpt+((yoff+x)*linesize)+((xoff+w-1-y)*3);
            dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
        }
    }
    sp = destpt+(yoff*linesize)+(xoff*3);
    for (y=0;y<h;y++) {
        memcpy(sp+y*linesize, tmp+y*w*3, w*3);
    }
    free(tmp);
}

void Blockto24(int x, int y, unsigned char * destpt, int xoff, int yoff, int linesize) {
    int i, isop;
    short int * mymappt;
    unsigned char * srcpt;
    BLKSTR * blkdatapt;

    srcpt = (unsigned char *) blockgfxpt;
    if (x<mapwidth && y<mapheight && x>=0 && y>=0) {
        mymappt = mappt;
        mymappt += x+(y*mapwidth);
        blkdatapt = MapCellBlkstr(*mymappt);
        if (blkdatapt == NULL) {
            destpt += (xoff*3)+(yoff*linesize);
            for (i=0;i<blockheight;i++) {
                memset(destpt, 0x44, blockwidth*3);
                destpt += linesize;
            }
            return;
        }

        isop = 0;
        if (isopillars && blkdatapt->unused2 && !blkdatapt->unused3) isop = 1;
        if (isopillars && !blkdatapt->unused2 && blkdatapt->unused3) isop = 2;
        if (flippedtiles && blkdatapt->unused2) isop = 1;
        if (flippedtiles && blkdatapt->unused3) isop += 2;
        DrawBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, isop);
        if (blkdatapt->fgoff) {
            OverBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, isop);
        }
        if (isopillars) return;
        if (blkdatapt->fgoff2) {
            OverBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, isop);
        }
        if (blkdatapt->fgoff3) {
            OverBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, isop);
        }
        if (rotatedtiles && blkdatapt->user7>=1 && blkdatapt->user7<=3)
            RotRect24(destpt, xoff, yoff, linesize, blkdatapt->user7);
    } else {
        destpt += (xoff*3)+(yoff*linesize);
        for (i=0;i<blockheight;i++) {
            memset(destpt, 0x44, blockwidth*3);
            destpt += linesize;
        }
    }
}

void BlockSecto24(int strnum, unsigned char * destpt, int xoff, int yoff, int linesize, int bpart) {
    int isop;
    unsigned char * srcpt;
    BLKSTR * blkdatapt;

    srcpt = (unsigned char *) blockgfxpt;
    if (strnum<numblockstr && strnum>-1) {
        blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (strnum*(FMP_BLK_MEMUNIT)));

        isop = 0;
        if (flippedtiles && blkdatapt->unused2) isop = 1;
        if (flippedtiles && blkdatapt->unused3) isop += 2;
        switch (bpart) {
        case 0:
            DrawBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, isop);
            break;
        case 1:
            DrawBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, isop);
            break;
        case 2:
            DrawBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, isop);
            break;
        case 3:
            DrawBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, isop);
            break;
        }
        if (rotatedtiles && blkdatapt->user7>=1 && blkdatapt->user7<=3)
            RotRect24(destpt, xoff, yoff, linesize, blkdatapt->user7);
    }
}

void BlockGfxto24(int gfxnum, unsigned char * destpt, int xoff, int yoff, int linesize) {
    int i;
    unsigned char * srcpt;

    srcpt = (unsigned char *) blockgfxpt;
    if (gfxnum<numblockgfx && gfxnum>-1) {
        DrawBlock(srcpt+(gfxnum*blockwidth*blockheight*((blockdepth+1)/8)), destpt, xoff, yoff, linesize, 0);
    } else {
        destpt += (xoff*3)+(yoff*linesize);
        for (i=0;i<blockheight;i++) {
            memset(destpt, 0x44, blockwidth*3);
            destpt += linesize;
        }
    }
}

void BlockGfxto8(int gfxnum, unsigned char * destpt, int xoff, int yoff, int linesize) {
    int i, j;
    unsigned char * srcpt;

    srcpt = (unsigned char *) blockgfxpt;
    srcpt += gfxnum*blockwidth*blockheight;
    destpt += xoff+(yoff*linesize);
    if (gfxnum<numblockgfx && gfxnum>-1 && blockdepth==8) {
        for (i=0;i<blockheight;i++) {	for (j=0;j<blockwidth;j++) {
                destpt[j] = srcpt[0];
                srcpt++; }
            destpt += linesize; }
    } else {
        for (i=0;i<blockheight;i++) {
            memset(destpt, 0, blockwidth);
            destpt += linesize;
        }
    }
}

void BlockStrto24(int strnum, unsigned char * destpt, int xoff, int yoff, int linesize) {
    int i, isop;
    unsigned char * srcpt;
    BLKSTR * blkdatapt;

    srcpt = (unsigned char *) blockgfxpt;
    if (strnum<numblockstr && strnum>-1) {
        blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)strnum)*(FMP_BLK_MEMUNIT)));

        isop = 0;
        if (flippedtiles && blkdatapt->unused2) isop = 1;
        if (flippedtiles && blkdatapt->unused3) isop += 2;
        DrawBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, isop);
        if (blkdatapt->fgoff) {
            OverBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, isop);
        }
        if (isopillars) return;
        if (blkdatapt->fgoff2) {
            OverBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, isop);
        }
        if (blkdatapt->fgoff3) {
            OverBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, isop);
        }
    } else {
        destpt += (xoff*3)+(yoff*linesize);
        for (i=0;i<blockheight;i++) {
            memset(destpt, 0x44, blockwidth*3);
            destpt += linesize;
        }
    }
}

void BlockAnimto24(int aninum, int frame, unsigned char * destpt, int xoff, int yoff, int linesize) {
    int isop;
    unsigned char * srcpt;
    long int * templpt;
    ANISTR * myanimpt;
    BLKSTR * blkdatapt;

    if (animstrpt==NULL) return;
    srcpt = (unsigned char *) blockgfxpt;
    myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
    myanimpt--;
    if (myanimpt->antype==AN_END) return;
    while (aninum) {
        myanimpt--; aninum--;
        if (myanimpt->antype==AN_END) return;
    }

    templpt = (long int *) ((animstrpt+ANDTMEM)+(myanimpt->anstartoff));
    if (frame==-1) templpt--;
    else templpt += frame;
    blkdatapt = (BLKSTR *) (blockstrpt + (*templpt));

    isop = 0;
    if (flippedtiles && blkdatapt->unused2) isop = 1;
    if (flippedtiles && blkdatapt->unused3) isop += 2;
    DrawBlock(srcpt+(blkdatapt->bgoff), destpt, xoff, yoff, linesize, isop);
    if (blkdatapt->fgoff) {
        OverBlock(srcpt+(blkdatapt->fgoff), destpt, xoff, yoff, linesize, isop);
    }
    if (isopillars) return;
    if (blkdatapt->fgoff2) {
        OverBlock(srcpt+(blkdatapt->fgoff2), destpt, xoff, yoff, linesize, isop);
    }
    if (blkdatapt->fgoff3) {
        OverBlock(srcpt+(blkdatapt->fgoff3), destpt, xoff, yoff, linesize, isop);
    }
}

void PlaceTile(int xoff, int yoff) {
    int k, l, m;
    short int * mymappt = mappt;
    short int * mymap2pt;
    short int * mybrshpt;

    if (mappt==NULL) return;
    if (xoff<0 || xoff>=mapwidth) return;
    if (yoff<0 || yoff>=mapheight) return;

    if (curbrsh>0 && curbrsh<=MAXBRSH) {
        if ((blockstaggerx || blockstaggery) && (yoff&1)) yoff--;
    }
    if ((xoff+(yoff*mapwidth))==curplacetile) return;
    curplacetile = (xoff+(yoff*mapwidth));
    mymappt += (xoff+(yoff*mapwidth));

    if (!(curbrsh>0 && curbrsh<=MAXBRSH)) { if (curanim>=0) {
            *mymappt = -((curanim+1)*FMP_ANI_MEMUNIT);
            return;
    } }

    if (curbrsh>0 && curbrsh<=MAXBRSH) {
        if (brshpt[curbrsh-1]==NULL) return;
        mybrshpt = brshpt[curbrsh-1];
        k = (int) *mybrshpt; mybrshpt++; l = (int) *mybrshpt; mybrshpt++;
        /* brush handle: paste origin corner */
        {
            int ox = xoff, oy = yoff, sx = 0, sy = 0, ex = k, ey = l, bw = k;
            short int * srcpt;
            if (brushhandle==1) ox = xoff-k+1;
            else if (brushhandle==2) oy = yoff-l+1;
            else if (brushhandle==3) { ox = xoff-k+1; oy = yoff-l+1; }
            /* paste off edges: clip to map */
            if (ox<0) { sx = -ox; ox = 0; }
            if (oy<0) { sy = -oy; oy = 0; }
            if (ox+k>mapwidth) ex = mapwidth-ox;
            if (oy+l>mapheight) ey = mapheight-oy;
            if (ex<=sx || ey<=sy) return;
            mymappt = mappt+(ox+(oy*mapwidth));
            srcpt = mybrshpt+(sy*bw)+sx;
            for (;sy<ey;sy++) {
                mymap2pt = mymappt;
                for (m=sx;m<ex;m++) {
                    if (srcpt[m-sx] || pastebrushnt) mymappt[m-sx] = srcpt[m-sx];
                }
                mymappt = mymap2pt; mymappt += mapwidth;
                srcpt += bw;
            }
        }
    } else { if (curanim==-1) { *mymappt = curstr;
    } }
}

int MakeBrush(int x1, int y1, int x2, int y2) {
    int k, l;
    short int * mybrshpt, * mymappt, * mymap2pt;

    if (curbrsh<1 || curbrsh>MAXBRSH) return -1;
    if (x1<0 || x2<0 || y1<0 || y2<0) return -1;
    if (x1>=mapwidth || x2>=mapwidth || y1>=mapheight || y2>=mapheight) return -1;
    if (brshpt[curbrsh-1]!=NULL) free(brshpt[curbrsh-1]);
    brshpt[curbrsh-1] = NULL;

    brshpt[curbrsh-1] = malloc((((x2-x1)+1)*((y2-y1)+1)*2)+4);
    if (brshpt[curbrsh-1]==NULL) return -1;

    mybrshpt = (short int *) brshpt[curbrsh-1];
    *mybrshpt = (x2-x1)+1; mybrshpt++;
    *mybrshpt = (y2-y1)+1; mybrshpt++;
    mymappt = (short int *) mapmappt[maplayer];
    mymappt += (y1*((int)mapwidth));
    mymappt += x1;
    for (k=0;k<(y2-y1+1);k++) {
        mymap2pt = mymappt;
        for (l=0;l<(x2-x1+1);l++) {
            *mybrshpt = *mymappt;
            mybrshpt++; mymappt++;
        }
        mymappt = mymap2pt;
        mymappt += mapwidth;
    }
    return 0;
}

int MakeBrushBS(int bsstart, int bswidth, int bsheight, int bsmodu) {
    int j, k;
    short int * mybrshpt;

    if (curbrsh<1 || curbrsh>MAXBRSH) return -1;
    if (brshpt[curbrsh-1]!=NULL) free(brshpt[curbrsh-1]);
    brshpt[curbrsh-1] = NULL;

    brshpt[curbrsh-1] = malloc((bswidth*bsheight*2)+4);
    if (brshpt[curbrsh-1]==NULL) return -1;

    mybrshpt = (short int *) brshpt[curbrsh-1];
    *mybrshpt = bswidth; mybrshpt++;
    *mybrshpt = bsheight; mybrshpt++;

    for (j=0;j<bsheight;j++) {
        for (k=0;k<bswidth;k++) {
            if (bsstart >= numblockstr || bsstart < 0) {
                free(brshpt[curbrsh-1]); brshpt[curbrsh-1] = NULL;
                return -1;
            }
            *mybrshpt = bsstart;
            mybrshpt++; bsstart++;
        }
        bsstart += bsmodu;
    }
    return 0;
}

void DestroyAllBrushes(void) {
    int i;

    for (i=0;i<MAXBRSH;i++) {
        if (brshpt[i]!=NULL) { free(brshpt[i]); brshpt[i] = NULL; }
        strcpy(brshname[i], "Brush");
    }
}

/* pixel channel helpers for resampling (all depths) */
static void HV_pix2rgb(unsigned char * p, int * r, int * g, int * b) {
    if (blockdepth==8) { *r = cmappt[(*p)*3]; *g = cmappt[(*p)*3+1]; *b = cmappt[(*p)*3+2]; }
    else if (blockdepth==15) { *r = ((p[1]&0x1F)<<3); *g = ((((p[0]&0x3)<<3)|(p[1]>>5)))<<3; *b = (p[0]&0xFC)<<1; }
    else if (blockdepth==16) { *r = ((p[1]&0x1F)<<3); *g = ((((p[0]&0x7)<<3)|(p[1]>>5)))<<2; *b = (p[0]&0xF8); }
    else if (blockdepth==32) { *r = p[1]; *g = p[2]; *b = p[3]; }
    else { *r = p[2]; *g = p[1]; *b = p[0]; }
}

static void HV_rgb2pix(unsigned char * p, int r, int g, int b) {
    if (blockdepth==8) {
        int bi, bd = 1<<30, bv = 0;
        for (bi=0;bi<256;bi++) {
            int dr = cmappt[bi*3]-r, dg = cmappt[bi*3+1]-g, db = cmappt[bi*3+2]-b;
            int dd = dr*dr+dg*dg+db*db;
            if (dd<bd) { bd = dd; bv = bi; }
        }
        *p = (unsigned char) bv;
    } else if (blockdepth==15) { p[0] = ((g>>3)&0x3)|((b)&0xFC); p[1] = ((r>>3)&0x1F)|(((g>>5)&0x7)<<5); }
    else if (blockdepth==16) { p[0] = ((g>>5)&0x7)|((b)&0xF8); p[1] = ((r>>3)&0x1F)|(((g>>2)&0x7)<<5); }
    else if (blockdepth==32) { p[0] = 0; p[1] = r; p[2] = g; p[3] = b; }
    else { p[0] = b; p[1] = g; p[2] = r; }
}

/* Halve block dimensions (warn: destroys brushes). Averages 2x2 pixels. */
int HalveBlockDims(void) {
    int g, x, y, bpp, nw, nh;
    unsigned char * ngfx, * sp;
    int r[4], gg[4], b[4];

    if (blockgfxpt==NULL || numblockgfx<=0) return -1;
    if ((blockwidth&1) || (blockheight&1) || blockwidth<2 || blockheight<2) return -1;
    bpp = (blockdepth+1)/8;
    nw = blockwidth/2; nh = blockheight/2;
    ngfx = malloc(numblockgfx*nw*nh*bpp);
    if (ngfx==NULL) return -1;
    for (g=0;g<numblockgfx;g++) {
        for (y=0;y<nh;y++) {
            for (x=0;x<nw;x++) {
                int k;
                for (k=0;k<4;k++) {
                    sp = (unsigned char *) blockgfxpt+(g*blockwidth*blockheight*bpp)+
                    (((y*2)+(k/2))*blockwidth+(x*2)+(k%2))*bpp;
                    HV_pix2rgb(sp, &r[k], &gg[k], &b[k]);
                }
                HV_rgb2pix(ngfx+(g*nw*nh+y*nw+x)*bpp,
                    (r[0]+r[1]+r[2]+r[3]+2)/4, (gg[0]+gg[1]+gg[2]+gg[3]+2)/4, (b[0]+b[1]+b[2]+b[3]+2)/4);
            }
        }
    }
    free(blockgfxpt); blockgfxpt = (char *) ngfx;
    blockwidth = nw; blockheight = nh;
    if (!(blockgapx&1)) blockgapx /= 2;
    if (!(blockgapy&1)) blockgapy /= 2;
    if (!(blockstaggerx&1)) blockstaggerx /= 2;
    if (!(blockstaggery&1)) blockstaggery /= 2;
    DestroyAllBrushes();
    mapaltered = 1;
    return 0;
}

/* Convert 16x16 map to 8x8 (GBA): each block becomes 4, map doubles. */
int Convert16to8(void) {
    int g, q, x, y, bpp;
    int nng, nns;
    unsigned char * ngfx;
    char * nstr;
    short int * nlay[MAXMAPLAYERS];
    BLKSTR * sb, * db;
    int qox[4] = { 0, 8, 0, 8 }, qoy[4] = { 0, 0, 8, 8 };

    if (blockwidth!=16 || blockheight!=16) return -1;
    if (blockgfxpt==NULL || blockstrpt==NULL || mappt==NULL) return -1;
    if (animstrpt!=NULL) return -2; /* anims reference block offsets */
    if (numblockgfx>1024 || numblockstr>7500) return -1;
    bpp = (blockdepth+1)/8;
    nng = numblockgfx*4; nns = numblockstr*4;
    ngfx = malloc(nng*8*8*bpp);
    nstr = malloc(nns*BLOCKSTRSIZE);
    if (ngfx==NULL || nstr==NULL) { if (ngfx) free(ngfx); if (nstr) free(nstr); return -1; }
    for (g=0;g<numblockgfx;g++) {
        for (q=0;q<4;q++) {
            for (y=0;y<8;y++) {
                memcpy(ngfx+((g*4+q)*64+y*8)*bpp,
                    blockgfxpt+(g*256+(qoy[q]+y)*16+qox[q])*bpp, 8*bpp);
            }
        }
    }
    for (g=0;g<numblockstr;g++) {
        sb = (BLKSTR *) (blockstrpt+g*BLOCKSTRSIZE);
        for (q=0;q<4;q++) {
            int gg;
            db = (BLKSTR *) (nstr+(g*4+q)*BLOCKSTRSIZE);
            memcpy(db, sb, BLOCKSTRSIZE);
            gg = (int) sb->bgoff/(16*16*bpp);
            db->bgoff = (gg*4+q)*64*bpp;
            gg = (int) sb->fgoff/(16*16*bpp);
            db->fgoff = (gg*4+q)*64*bpp;
            gg = (int) sb->fgoff2/(16*16*bpp);
            db->fgoff2 = (gg*4+q)*64*bpp;
            gg = (int) sb->fgoff3/(16*16*bpp);
            db->fgoff3 = (gg*4+q)*64*bpp;
        }
    }
    for (g=0;g<MAXMAPLAYERS;g++) {
        short int * ol = mapmappt[g];
        short int * nl;
        if (ol==NULL) { nlay[g] = NULL; continue; }
        nl = malloc(mapwidth*2*mapheight*2*2);
        if (nl==NULL) {
            int h; for (h=0;h<g;h++) if (nlay[h]) free(nlay[h]);
            free(ngfx); free(nstr); return -1;
        }
        for (y=0;y<mapheight;y++) {
            for (x=0;x<mapwidth;x++) {
                int s = ol[x+y*mapwidth];
                if (s<0) {
                    nl[(x*2)+(y*2)*mapwidth*2] = s;
                    nl[(x*2+1)+(y*2)*mapwidth*2] = s;
                    nl[(x*2)+(y*2+1)*mapwidth*2] = s;
                    nl[(x*2+1)+(y*2+1)*mapwidth*2] = s;
                } else {
                    nl[(x*2)+(y*2)*mapwidth*2] = s*4;
                    nl[(x*2+1)+(y*2)*mapwidth*2] = s*4+1;
                    nl[(x*2)+(y*2+1)*mapwidth*2] = s*4+2;
                    nl[(x*2+1)+(y*2+1)*mapwidth*2] = s*4+3;
                }
            }
        }
        nlay[g] = nl;
    }
    for (g=0;g<MAXMAPLAYERS;g++) {
        if (mapmappt[g]) free(mapmappt[g]);
        mapmappt[g] = nlay[g];
    }
    mappt = mapmappt[maplayer];
    free(blockgfxpt); blockgfxpt = (char *) ngfx;
    free(blockstrpt); blockstrpt = nstr;
    numblockgfx = nng; numblockstr = nns;
    mapwidth *= 2; mapheight *= 2;
    blockwidth = 8; blockheight = 8; blockgapx = 8; blockgapy = 8;
    if (curstr>=numblockstr) curstr = numblockstr-1;
    DestroyAllBrushes();
    mapaltered = 1;
    return 0;
}

/* Create map from big picture (append as new layer): best-match each
 * map-sized cell of the image against block BG graphics. Image pixels
 * must equal map pixels exactly. */
int BigPictureToLayer(char * imgname) {
    unsigned char * img = NULL;
    int iw, ih, x, y, g, s;
    unsigned char * cache;
    int * g2s;
    int nl, best, bi;
    long long bestd, dd;
    int mwpx = mapwidth*blockwidth, mhpx = mapheight*blockheight;

    if (mappt==NULL || blockgfxpt==NULL || blockstrpt==NULL) return -1;
    if (blockstaggerx || blockstaggery) return -1;
    if (IMLoadImage24(imgname, &img, &iw, &ih)) return -1;
    if (iw!=mwpx || ih!=mhpx) { free(img); return -2; }
    if (numblockgfx<=0 || numblockgfx>4096) { free(img); return -1; }
    if ((long) numblockgfx*blockwidth*blockheight*3 > 64*1024*1024) { free(img); return -1; }
    cache = malloc(numblockgfx*blockwidth*blockheight*3);
    g2s = malloc(numblockgfx*sizeof(int));
    if (cache==NULL || g2s==NULL) {
        if (cache!=NULL) free(cache);
        if (g2s!=NULL) free(g2s);
        free(img); return -1;
    }
    for (g=0;g<numblockgfx;g++) {
        BlockGfxto24(g, cache+g*blockwidth*blockheight*3, 0, 0, blockwidth*3);
        g2s[g] = -1;
    }
    for (s=0;s<numblockstr;s++) {
        BLKSTR * bp = (BLKSTR *) (blockstrpt+s*BLOCKSTRSIZE);
        g = (int) bp->bgoff/(blockwidth*blockheight*((blockdepth+1)/8));
        if (g>=0 && g<numblockgfx && g2s[g]<0) g2s[g] = s;
    }
    nl = CreateMapLayer();
    if (nl<0) { free(cache); free(g2s); free(img); return -1; }
    for (y=0;y<mapheight;y++) {
        for (x=0;x<mapwidth;x++) {
            bestd = (1LL << 62); bi = 0; best = 0;
            for (g=0;g<numblockgfx;g++) {
                int px, py;
                unsigned char * gp = cache+g*blockwidth*blockheight*3;
                dd = 0;
                for (py=0;py<blockheight;py++) {
                    /* loader is bottom-up: map row y=0 is image top */
                    unsigned char * ip = img+(((mhpx-1-(y*blockheight+py))*mwpx)+(x*blockwidth))*3;
                    for (px=0;px<blockwidth;px++) {
                        int dr = (int) ip[px*3+2]-gp[(py*blockwidth+px)*3+2];
                        int dg = (int) ip[px*3+1]-gp[(py*blockwidth+px)*3+1];
                        int db = (int) ip[px*3]-gp[(py*blockwidth+px)*3];
                        dd += dr*dr+dg*dg+db*db;
                        if (dd>=bestd) break;
                    }
                    if (dd>=bestd) break;
                }
                if (dd<bestd) { bestd = dd; best = g; }
            }
            bi = (best>=0 && g2s[best]>=0) ? g2s[best] : 0;
            mapmappt[nl][x+y*mapwidth] = bi;
        }
    }
    free(cache); free(g2s); free(img);
    mapaltered = 1;
    return nl;
}

void NewBlock(void) {
    int i, j, k;
    short int * mymappt;
    char * strpt = (char *) blockstrpt;
    ANISTR * myanpt;
    long int * templpt;

    curstr++; numblockstr++; strpt+=(((int)curstr)*BLOCKSTRSIZE);
    InsertBytes(strpt, blockstrpt+BKDTMEM, BLOCKSTRSIZE);

    for (k=0;k<MAXMAPLAYERS;k++) { mymappt = mapmappt[k]; if (mymappt != NULL) {
            for (j=0;j<mapheight;j++) {
                for (i=0;i<mapwidth;i++) {
                    if (*mymappt>=(curstr)) (*mymappt)++;
                    mymappt++;
                }
    } } }
    for (k=0;k<MAXBRSH;k++) {
        if (brshpt[k]) { mymappt = brshpt[k]; i = *mymappt; mymappt++; i *= *mymappt; mymappt++;
            while (i) {
                if (*mymappt>=(curstr)) (*mymappt)++;
                mymappt++; i--; } } }

    if (animstrpt != NULL) {
        myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
        while (myanpt->antype!=-1) myanpt--;
        templpt = (long int *) animstrpt;
        while (templpt!=(long int *) myanpt) { if (*templpt>=(((int)curstr)*BLOCKSTRSIZE)) *templpt+=BLOCKSTRSIZE; templpt++; }
    }
}

void CutBlock(void) {
    short int i, j, k;
    short int * mymappt;
    char * blktpt = blockstrpt;
    BLKSTR * blkpt;
    ANISTR * myanpt;
    long int * templpt;

    if (!curstr) return;
    blktpt += ((int)curstr)*BLOCKSTRSIZE;
    blkpt = (BLKSTR *) blktpt;
    blkbuffer.bgoff = blkpt->bgoff;
    blkbuffer.fgoff = blkpt->fgoff;
    blkbuffer.fgoff2 = blkpt->fgoff2;
    blkbuffer.fgoff3 = blkpt->fgoff3;
    blkbuffer.user1 = blkpt->user1;
    blkbuffer.user2 = blkpt->user2;
    blkbuffer.user3 = blkpt->user3;
    blkbuffer.user4 = blkpt->user4;
    blkbuffer.user5 = blkpt->user5;
    blkbuffer.user6 = blkpt->user6;
    blkbuffer.user7 = blkpt->user7;
    blkbuffer.tl = blkpt->tl;
    blkbuffer.tr = blkpt->tr;
    blkbuffer.bl = blkpt->bl;
    blkbuffer.br = blkpt->br;
    blkbuffer.trigger = blkpt->trigger;
    blkbuffer.unused1 = blkpt->unused1;
    blkbuffer.unused2 = blkpt->unused2;
    blkbuffer.unused3 = blkpt->unused3;
    RemoveBytes(blktpt, blockstrpt+BKDTMEM, BLOCKSTRSIZE);
    for (k=0;k<MAXMAPLAYERS;k++) { mymappt = mapmappt[k]; if (mymappt != NULL) {
            for (j=0;j<mapheight;j++) {
                for (i=0;i<mapwidth;i++) {
                    if (*mymappt>curstr) (*mymappt)--;
                    else if (*mymappt==curstr) (*mymappt)=0;
                    mymappt++;
                }
    } } }
    for (k=0;k<MAXBRSH;k++) {
        if (brshpt[k]) { mymappt = brshpt[k]; i = *mymappt; mymappt++; i *= *mymappt; mymappt++;
            while (i) {
                if (*mymappt>curstr) (*mymappt)--;
                else if (*mymappt==curstr) (*mymappt)=0;
                mymappt++; i--; } } }

    if (animstrpt != NULL) {
        myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
        while (myanpt->antype!=-1) myanpt--;
        templpt = (long int *) animstrpt;
        while (templpt!=(long int *) myanpt)
        { if (*templpt>(((int)curstr)*BLOCKSTRSIZE)) *templpt-=BLOCKSTRSIZE;
            else if (*templpt==(((int)curstr)*BLOCKSTRSIZE)) *templpt = 0;
            templpt++; }
    }

    numblockstr--; if (curstr==numblockstr) curstr--;
}

void CopyBlock(void) {
    char * blktpt = blockstrpt;
    BLKSTR * blkpt;

    blktpt += ((int)curstr)*BLOCKSTRSIZE;
    blkpt = (BLKSTR *) blktpt;
    blkbuffer.bgoff = blkpt->bgoff;
    blkbuffer.fgoff = blkpt->fgoff;
    blkbuffer.fgoff2 = blkpt->fgoff2;
    blkbuffer.fgoff3 = blkpt->fgoff3;
    blkbuffer.user1 = blkpt->user1;
    blkbuffer.user2 = blkpt->user2;
    blkbuffer.user3 = blkpt->user3;
    blkbuffer.user4 = blkpt->user4;
    blkbuffer.user5 = blkpt->user5;
    blkbuffer.user6 = blkpt->user6;
    blkbuffer.user7 = blkpt->user7;
    blkbuffer.tl = blkpt->tl;
    blkbuffer.tr = blkpt->tr;
    blkbuffer.bl = blkpt->bl;
    blkbuffer.br = blkpt->br;
    blkbuffer.trigger = blkpt->trigger;
    blkbuffer.unused1 = blkpt->unused1;
    blkbuffer.unused2 = blkpt->unused2;
    blkbuffer.unused3 = blkpt->unused3;
}

void PasteBlock(void) {
    short int i, j, k;
    short int * mymappt;
    char * blktpt = blockstrpt;
    BLKSTR * blkpt;
    ANISTR * myanpt;
    long int * templpt;

    curstr++; numblockstr++;
    blktpt += ((int)curstr)*BLOCKSTRSIZE;
    InsertBytes(blktpt, blockstrpt+BKDTMEM, BLOCKSTRSIZE);
    for (k=0;k<MAXMAPLAYERS;k++) { mymappt = mapmappt[k]; if (mymappt != NULL) {
            for (j=0;j<mapheight;j++) {
                for (i=0;i<mapwidth;i++) {
                    if (*mymappt>=(curstr)) (*mymappt)++;
                    mymappt++;
                }
    } } }
    for (k=0;k<MAXBRSH;k++) {
        if (brshpt[k]) { mymappt = brshpt[k]; i = *mymappt; mymappt++; i *= *mymappt; mymappt++;
            while (i) {
                if (*mymappt>=(curstr)) (*mymappt)++;
                mymappt++; i--; } } }

    if (animstrpt != NULL) {
        myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
        while (myanpt->antype!=-1) myanpt--;
        templpt = (long int *) animstrpt;
        while (templpt!=(long int *) myanpt) { if (*templpt>=(curstr*BLOCKSTRSIZE)) *templpt+=BLOCKSTRSIZE; templpt++; }
    }

    blkpt = (BLKSTR *) blktpt;
    blkpt->bgoff = blkbuffer.bgoff;
    blkpt->fgoff = blkbuffer.fgoff;
    blkpt->fgoff2 = blkbuffer.fgoff2;
    blkpt->fgoff3 = blkbuffer.fgoff3;
    blkpt->user1 = blkbuffer.user1;
    blkpt->user2 = blkbuffer.user2;
    blkpt->user3 = blkbuffer.user3;
    blkpt->user4 = blkbuffer.user4;
    blkpt->user5 = blkbuffer.user5;
    blkpt->user6 = blkbuffer.user6;
    blkpt->user7 = blkbuffer.user7;
    blkpt->tl = blkbuffer.tl;
    blkpt->tr = blkbuffer.tr;
    blkpt->bl = blkbuffer.bl;
    blkpt->br = blkbuffer.br;
    blkpt->trigger = blkbuffer.trigger;
    blkpt->unused1 = blkbuffer.unused1;
    blkpt->unused2 = blkbuffer.unused2;
    blkpt->unused3 = blkbuffer.unused3;
}

void DestroyUnusedBlocks(void) {
    int blkcount, blkfound, i, j, k;
    short int curblocktemp;
    ANISTR *myanpt;
    long int * templpt;
    short int * mymappt;

    if (mappt==NULL) return;
    if (blockstrpt==NULL) return;

    blkcount = (numblockstr-1); while (blkcount) {
        blkfound = 0;
        if (isopillars && blkcount > 0) { if (((BLKSTR *) blockstrpt)[blkcount-1].unused1) blkfound = 1; }
        for (k=0;k<MAXMAPLAYERS;k++) {
            mymappt = mapmappt[k]; if (mymappt!=NULL) {
                for (j=0;j<mapheight;j++) {
                    for (i=0;i<mapwidth;i++) {
                        if (*mymappt == (blkcount)) { blkfound = 1; break; }
                        mymappt++;
                    }
                    if (blkfound) break;
                }
                if (blkfound) break;
        } }
        if (animstrpt != NULL && !blkfound) {
            myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
            while (myanpt->antype!=-1) myanpt--;
            templpt = (long int *) animstrpt;
            while (templpt!=(long int *) myanpt) {
                if (*templpt==(blkcount*FMP_BLK_MEMUNIT)) { blkfound = 1; break; }
                templpt++;
        } }
        blkcount--;
        if (!blkfound) {
            curblocktemp = curstr; curstr = blkcount+1;
            CutBlock(); curstr = curblocktemp; }
    }
    curanim = -1; curstr = 0;
}

void CorrectBlockStructures(void) {
    int i, j;
    BLKSTR * myblk;

    myblk = (BLKSTR *) blockstrpt;
    j = blockwidth*blockheight*((blockdepth+1)/8)*numblockgfx;
    for (i=0;i<numblockstr;i++) {
        if (myblk->bgoff >= j) myblk->bgoff = 0;
        if (myblk->fgoff >= j) myblk->fgoff = 0;
        if (myblk->fgoff2 >= j) myblk->fgoff2 = 0;
        if (myblk->fgoff3 >= j) myblk->fgoff3 = 0;
        myblk++;
    }
}

void AnimNew(void) {
    int i, j, k;
    ANISTR * myanimpt;
    ANISTR * myanim3pt;
    char * myanim2pt;
    long int * templpt;
    short int * mymappt;

    if (animstrpt==NULL) {
        animstrpt = malloc(ANDTMEM);
        if (animstrpt == NULL) return;
        memset(animstrpt, 0, ANDTMEM);
        *(animstrpt+ANDTMEM-FMP_ANI_MEMUNIT)=-1;
        if (curanim==-1) curanim = 0;
        numblockanim = 1;
    } else {
        if (numblockanim) curanim++;
        numblockanim++;
    }
    myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
    myanimpt -= curanim; j = -((curanim+1)*FMP_ANI_MEMUNIT);
    InsertBytesR((char *) animstrpt, (char *) myanimpt, FMP_ANI_MEMUNIT);
    myanimpt--; myanimpt->antype = AN_NONE; myanimpt->andelay = 3;
    myanimpt->ancount = myanimpt->andelay; myanimpt->anuser = 0;
    myanim2pt = (char *) (animstrpt+ANDTMEM);
    myanim2pt -= (FMP_ANI_MEMUNIT*(numblockanim+1));
    InsertBytesR((char *) animstrpt, (char *) myanim2pt, 4);
    myanim3pt = (ANISTR *) (animstrpt+ANDTMEM); myanim3pt--;
    while (myanim3pt->antype!=-1) {
        if ((myanim3pt->anstartoff)<=((myanim2pt-(animstrpt+ANDTMEM))+FMP_ANI_MEMUNIT)) {
            myanim3pt->anstartoff -= (4+FMP_ANI_MEMUNIT);
            myanim3pt->ancuroff -= (4+FMP_ANI_MEMUNIT);
            myanim3pt->anendoff -= (4+FMP_ANI_MEMUNIT);
        }
        myanim3pt--;
    }
    myanimpt->anstartoff = (myanim2pt-(animstrpt+ANDTMEM));
    myanimpt->ancuroff = (myanim2pt-(animstrpt+ANDTMEM));
    myanimpt->anendoff = (myanim2pt-(animstrpt+ANDTMEM));
    myanim2pt -= 4; templpt = (long int *) myanim2pt; *templpt=0;
    /* (int) curstr*FMP_BLK_MEMUNIT; */

    for (k=0;k<MAXMAPLAYERS;k++) { mymappt = mapmappt[k]; if (mymappt != NULL) {
            i = mapwidth*mapheight; while (i) {
                if (*mymappt<=j) *mymappt -= FMP_ANI_MEMUNIT;
                mymappt++; i--; } } }

    for (k=0;k<MAXBRSH;k++) {
        if (brshpt[k]) { mymappt = brshpt[k]; i = *mymappt; mymappt++; i *= *mymappt; mymappt++;
            while (i) {
                if (*mymappt<=j) *mymappt -= FMP_ANI_MEMUNIT;
                mymappt++; i--; } } }
}

void AnimCut(void) {
    int i, k, copycount;
    ANISTR * myanimpt;
    char * myanim2pt;
    char * myabpt;
    char * mycpypt;
    long int * myab2pt;
    short int * mymappt;

    if (!numblockanim) return;
    myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
    myanimpt -= curanim; myanimpt--;
    if (animbufferpt==NULL) animbufferpt = malloc(ABUFMEM);
    myabpt = animbufferpt;
    *myabpt = myanimpt->antype; myabpt++;
    *myabpt = myanimpt->andelay; myabpt++;
    *myabpt = myanimpt->ancount; myabpt++;
    *myabpt = myanimpt->anuser; myabpt++;
    myab2pt = (long int *) myabpt;
    *myab2pt = ((myanimpt->anendoff)-(myanimpt->anstartoff))+4;
    myabpt += 4; copycount = *myab2pt; mycpypt = (char *) (animstrpt+ANDTMEM);
    mycpypt += myanimpt->anstartoff; myanim2pt = (char *) mycpypt; mycpypt -= 4;
    while (copycount) { *myabpt = *mycpypt; myabpt++; mycpypt++; copycount--; }

    myanimpt++; RemoveBytesR( (char *) animstrpt, (char *) myanimpt, FMP_ANI_MEMUNIT);
    RemoveBytesR( (char *) animstrpt, (char *) ((myanim2pt+FMP_ANI_MEMUNIT)+(*myab2pt)-4),
        *myab2pt);
    myanimpt = (ANISTR *) (animstrpt+ANDTMEM); myanimpt--;
    while (myanimpt->antype!=-1) {
        if ((myanimpt->anstartoff)<=((int) (myanim2pt-(animstrpt+ANDTMEM)))) {
            myanimpt->anstartoff += *myab2pt;
            myanimpt->ancuroff += *myab2pt;
            myanimpt->anendoff += *myab2pt;
        }
        myanimpt->anstartoff += (FMP_ANI_MEMUNIT);
        myanimpt->ancuroff += (FMP_ANI_MEMUNIT);
        myanimpt->anendoff += (FMP_ANI_MEMUNIT);
        myanimpt--;
    }
    copycount = -((curanim+1)*FMP_ANI_MEMUNIT);
    numblockanim--; if (numblockanim==curanim) curanim--; if (curanim<0) curanim = 0;

    for (k=0;k<MAXMAPLAYERS;k++) { mymappt = mapmappt[k]; if (mymappt != NULL) {
            i = mapwidth*mapheight; while (i) {
                if (*mymappt<copycount) *mymappt += FMP_ANI_MEMUNIT;
                else if (*mymappt==copycount) *mymappt = 0;
                mymappt++; i--; } } }

    for (k=0;k<MAXBRSH;k++) {
        if (brshpt[k]) { mymappt = brshpt[k]; i = *mymappt; mymappt++; i *= *mymappt; mymappt++;
            while (i) {
                if (*mymappt<copycount) *mymappt += FMP_ANI_MEMUNIT;
                else if (*mymappt==copycount) *mymappt = 0;
                mymappt++; i--; } } }

    if (numblockanim==0) { free(animstrpt); animstrpt = NULL; curanim = -1; }
}

void AnimCopy(void) {
    int copycount;
    ANISTR * myanimpt;
    char * myabpt;
    char * mycpypt;
    long int * myab2pt;
    if (!numblockanim) return;
    myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
    myanimpt -= curanim; myanimpt--;
    if (animbufferpt==NULL) animbufferpt = malloc(ABUFMEM);
    myabpt = animbufferpt;
    *myabpt = myanimpt->antype; myabpt++;
    *myabpt = myanimpt->andelay; myabpt++;
    *myabpt = myanimpt->ancount; myabpt++;
    *myabpt = myanimpt->anuser; myabpt++;
    myab2pt = (long int *) myabpt;
    *myab2pt = (myanimpt->anendoff-myanimpt->anstartoff)+4;
    myabpt += 4; copycount = *myab2pt; mycpypt = (char *) (animstrpt+ANDTMEM);
    mycpypt += myanimpt->anstartoff; mycpypt -= 4;
    while (copycount) { *myabpt = *mycpypt; myabpt++; mycpypt++; copycount--; }
}

void AnimPaste(void) {
    int i, j, k, copycount;
    ANISTR * myanimpt;
    ANISTR * myanim2pt;
    char * myabpt;
    char * mycpypt;
    long int * myab2pt;
    short int * mymappt;

    if (animbufferpt==NULL) return;
    myanimpt = (ANISTR *) (animstrpt+ANDTMEM); myanimpt--;
    myab2pt = (long int *) animbufferpt; myab2pt++; copycount = *myab2pt;
    while (myanimpt->antype!=-1) { myanimpt->anstartoff -= (FMP_ANI_MEMUNIT+copycount);
        myanimpt->ancuroff -= (FMP_ANI_MEMUNIT+ copycount);
        myanimpt->anendoff -= (FMP_ANI_MEMUNIT+ copycount); myanimpt--; }

    myanimpt = (ANISTR *) (animstrpt+ANDTMEM);
    myanimpt -= curanim; /* if (numblockanim!=0) myanimpt--; */
    j = -((curanim+1)*FMP_ANI_MEMUNIT);
    InsertBytesR((char *) animstrpt, (char *) myanimpt, FMP_ANI_MEMUNIT);
    myabpt = animbufferpt; myanimpt--;
    myanimpt->antype = *myabpt; myabpt++;
    myanimpt->andelay = *myabpt; myabpt++;
    myanimpt->ancount = *myabpt; myabpt++;
    myanimpt->anuser = *myabpt; myabpt++; myabpt += 4;
    myanim2pt = (ANISTR *) myanimpt;
    while (myanim2pt->antype!=-1) myanim2pt--;
    InsertBytesR((char *) animstrpt, (char *) myanim2pt, copycount);
    mycpypt = (char *) myanim2pt; mycpypt -= copycount; copycount -=4;
    *mycpypt = *myabpt; mycpypt++; myabpt++; *mycpypt = *myabpt; mycpypt++; myabpt++;
    *mycpypt = *myabpt; mycpypt++; myabpt++; *mycpypt = *myabpt; mycpypt++; myabpt++;
    myanimpt->anstartoff = (mycpypt-(animstrpt+ANDTMEM));
    myanimpt->ancuroff = (mycpypt-(animstrpt+ANDTMEM));
    while (copycount) { *mycpypt = *myabpt; mycpypt++; myabpt++; copycount--; }
    myanimpt->anendoff = (mycpypt-(animstrpt+ANDTMEM));

    for (k=0;k<MAXMAPLAYERS;k++) { mymappt = mapmappt[k]; if (mymappt != NULL) {
            i = mapwidth*mapheight; while (i) {
                if (*mymappt<=j) *mymappt -= FMP_ANI_MEMUNIT;
                mymappt++; i--; } } }

    for (k=0;k<MAXBRSH;k++) {
        if (brshpt[k]) { mymappt = brshpt[k]; i = *mymappt; mymappt++; i *= *mymappt; mymappt++;
            while (i) {
                if (*mymappt<=j) *mymappt -= FMP_ANI_MEMUNIT;
                mymappt++; i--; } } }

    numblockanim++; curanim++;
}

void PickBlock(int x, int y) {
    short int * mymappt;

    if (x<0 || x>=mapwidth || y<0 || y>=mapheight) return;
    if (mappt==NULL) return;
    curbrsh = 0;
    mymappt = mappt;
    mymappt += x+(y*((int)mapwidth));
    if (mymappt[0]<0) curanim = ((-(*mymappt))/(FMP_ANI_MEMUNIT))-1;
    else { curanim = -1; curstr = (*mymappt); }
}

void DelAnimFrame(int anum, int epoint) {
    long int insertoff, tempoff;
    ANISTR * myanpt;
    ANISTR * myan2pt;
    long int * templpt;

    myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt -= anum; myanpt--;
    insertoff = myanpt->anstartoff; insertoff += epoint*FMP_SEQ_MEMUNIT;
    if (insertoff>=(myanpt->anendoff)) return;
    templpt = (long int *) ((animstrpt+ANDTMEM)+insertoff); templpt++;
    RemoveBytesR((char *) animstrpt, (char *) templpt, 4);

    myan2pt = (ANISTR *) myanpt; tempoff = myanpt->anstartoff;
    myanpt = (ANISTR *) (animstrpt+ANDTMEM);
    while (myanpt->antype!=-1) {
        if ((myanpt->anstartoff) < insertoff) myanpt->anstartoff += 4;
        if ((myanpt->ancuroff) < insertoff) myanpt->ancuroff += 4;
        if ((myanpt->anendoff) < insertoff) myanpt->anendoff += 4;
        myanpt--;
    }
    myan2pt->anstartoff = tempoff+4;
}

void DestroyUnusedAnims(void) {
    int ancount, anfound, i, j, k;
    short int curanimtemp;
    ANISTR * myanpt;
    short int * mymappt;

    if (mappt==NULL) return;
    if (animstrpt==NULL) return;
    myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt--;
    ancount = 0; while (myanpt->antype!=-1) { ancount++; myanpt--; }

    while (ancount) {
        anfound = 0;
        for (k=0;k<MAXMAPLAYERS;k++) {
            mymappt = mapmappt[k]; if (mymappt!=NULL) {
                for (j=0;j<mapheight;j++) {
                    for (i=0;i<mapwidth;i++) {
                        if (*mymappt == -((ancount)*FMP_ANI_MEMUNIT)) { anfound = 1; break; }
                        mymappt++;
                    }
                    if (anfound) break;
                }
                if (anfound) break;
        } }
        ancount--;
        if (!anfound) { curanimtemp = curanim; curanim = ancount;
            AnimCut(); curanim = curanimtemp; }
    }
    curanim = -1; curstr = 0;
}

void InsertAnimFrame(int anum, int epoint, int frame) {
    long int insertoff, tempoff;
    ANISTR * myanpt;
    ANISTR * myan2pt;
    long int * templpt;

    myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt -= anum; myanpt--;
    insertoff = myanpt->anstartoff; insertoff += epoint*FMP_SEQ_MEMUNIT;
    if (insertoff>(myanpt->anendoff)) return;
    templpt = (long int *) ((animstrpt+ANDTMEM)+insertoff);
    InsertBytesR((char *) animstrpt, (char *) templpt, 4);
    templpt--; *templpt = frame*BLOCKSTRSIZE;

    myan2pt = (ANISTR *) myanpt; tempoff = myanpt->anstartoff;
    myanpt = (ANISTR *) (animstrpt+ANDTMEM);
    while (myanpt->antype!=-1) {
        if ((myanpt->anstartoff) < insertoff) myanpt->anstartoff -= 4;
        if ((myanpt->ancuroff) < insertoff) myanpt->ancuroff -= 4;
        if ((myanpt->anendoff) < insertoff) myanpt->anendoff -= 4;
        myanpt--;
    }
    myan2pt->anstartoff = tempoff-4;
}

void SetRefFrame(int refsnum, int frame) {
    long int insertoff;
    ANISTR * myanpt;
    long int * templpt;

    myanpt = (ANISTR *) (animstrpt+ANDTMEM); myanpt -= refsnum; myanpt--;
    insertoff = myanpt->anstartoff; insertoff-=4;
    templpt = (long int *) ((animstrpt+ANDTMEM)+insertoff);
    *templpt = frame*BLOCKSTRSIZE;
}

void DestroyUnusedGraphics(void) {
    int i, j, k;
    BLKSTR * mystrpt;

    if (blockstrpt==NULL) return;
    if (blockgfxpt==NULL) return;
    for (i=(numblockgfx-1);i>0;i--) {
        j = 0;
        mystrpt = (BLKSTR *) blockstrpt;
        mystrpt += numblockstr;
        for (k=(numblockstr-1);k>=0;k--) {
            mystrpt--;
            if (mystrpt->bgoff==(i*blockwidth*blockheight*((blockdepth+1)/8))) j = 1;
            if (mystrpt->fgoff==(i*blockwidth*blockheight*((blockdepth+1)/8))) j = 1;
            if (mystrpt->fgoff2==(i*blockwidth*blockheight*((blockdepth+1)/8))) j = 1;
            if (mystrpt->fgoff3==(i*blockwidth*blockheight*((blockdepth+1)/8))) j = 1;
        }
        if (j==0) {
            RemoveBytes(blockgfxpt+(i*blockwidth*blockheight*((blockdepth+1)/8)),
                blockgfxpt+(numblockgfx*blockwidth*blockheight*((blockdepth+1)/8)),
                blockwidth*blockheight*((blockdepth+1)/8));
            numblockgfx--;
            mystrpt = (BLKSTR *) blockstrpt;
            for (k=0;k<numblockstr;k++) {
                if (mystrpt->bgoff==(i*blockwidth*blockheight*((blockdepth+1)/8)))
                    mystrpt->bgoff = 0;
                if (mystrpt->fgoff==(i*blockwidth*blockheight*((blockdepth+1)/8)))
                    mystrpt->fgoff = 0;
                if (mystrpt->fgoff2==(i*blockwidth*blockheight*((blockdepth+1)/8)))
                    mystrpt->fgoff2 = 0;
                if (mystrpt->fgoff3==(i*blockwidth*blockheight*((blockdepth+1)/8)))
                    mystrpt->fgoff3 = 0;
                if (mystrpt->bgoff>(i*blockwidth*blockheight*((blockdepth+1)/8)))
                    mystrpt->bgoff -= (blockwidth*blockheight*((blockdepth+1)/8));
                if (mystrpt->fgoff>(i*blockwidth*blockheight*((blockdepth+1)/8)))
                    mystrpt->fgoff -= (blockwidth*blockheight*((blockdepth+1)/8));
                if (mystrpt->fgoff2>(i*blockwidth*blockheight*((blockdepth+1)/8)))
                    mystrpt->fgoff2 -= (blockwidth*blockheight*((blockdepth+1)/8));
                if (mystrpt->fgoff3>(i*blockwidth*blockheight*((blockdepth+1)/8)))
                    mystrpt->fgoff3 -= (blockwidth*blockheight*((blockdepth+1)/8));
                mystrpt++;
            }
        }
    }
}

void RemoveMarkerGraphics(void) {
    BLKSTR * mystrpt;
    int i;

    if (blockstrpt==NULL || blockgfxpt==NULL) return;
    mystrpt = (BLKSTR *) blockstrpt;
    for (i=0;i<numblockstr;i++) {
        mystrpt->fgoff3 = 0;
        mystrpt++;
    }
    DestroyUnusedGraphics();
}

void AutoSetBGTransparency(void) {
    BLKSTR * myblkstrpt;
    unsigned char * myautgfxpt;
    int i, j;

    if (blockgfxpt==NULL || blockstrpt==NULL) return;
    myblkstrpt = (BLKSTR *) blockstrpt;
    for (i=0;i<numblockstr;i++) {
        if (i == 0) myblkstrpt->trigger = 1;
        else myblkstrpt->trigger = 0;
        myautgfxpt = (unsigned char *) (blockgfxpt+myblkstrpt->bgoff);
        for (j=0;j<(blockwidth*blockheight);j++) {
            switch (blockdepth) {
            case 8:
                if (*myautgfxpt == trans8bit) myblkstrpt->trigger = 1;
                myautgfxpt++;
                break;
            case 15:
            case 16:
                if ((*(myautgfxpt)==trans16hi && *(myautgfxpt+1)==trans16low)) myblkstrpt->trigger = 1;
                myautgfxpt += 2;
                break;
            case 24:
                if ((*myautgfxpt==transred && *(myautgfxpt+1)==transgreen && *(myautgfxpt+2)==transblue)) myblkstrpt->trigger = 1;
                myautgfxpt += 3;
                break;
            case 32:
                if ((*(myautgfxpt+1)==transred && *(myautgfxpt+2)==transgreen && *(myautgfxpt+3)==transblue)) myblkstrpt->trigger = 1;
                myautgfxpt += 4;
                break;
            }
        }
        myblkstrpt++;
    }
}

int ResizeMap(int neww, int newh, int mapc) {
    int i, j, k;
    short int sxoff, syoff, dxoff, dyoff;
    short int * temppt, * spt, * dpt;
    short int * newlayers[MAXMAPLAYERS];

    if (neww < 1 || newh < 1) return -1;
    for (k=0;k<MAXMAPLAYERS;k++) newlayers[k] = NULL;
    for (k=0;k<MAXMAPLAYERS;k++) {
        if (mapmappt[k] != NULL) {
            newlayers[k] = (short int *) malloc((size_t)neww*(size_t)newh*2);
            if (newlayers[k]==NULL) {
                for (i=0;i<MAXMAPLAYERS;i++) if (newlayers[i]) free(newlayers[i]);
                return -1;
            }
            memset(newlayers[k], 0, (size_t)neww*(size_t)newh*2);
        }
    }

    for (k=0;k<MAXMAPLAYERS;k++) { if (mapmappt[k] != NULL) {
            temppt = newlayers[k];

            sxoff = 0; syoff = 0; dxoff = 0; dyoff = 0;
            if (mapc==0) {
                sxoff = (short int) resize_left;
                syoff = (short int) resize_top;
                if (sxoff<0) { dxoff = (short int) (-sxoff); sxoff = 0; }
                if (syoff<0) { dyoff = (short int) (-syoff); syoff = 0; }
            } else {
                if ((mapc==1) || (mapc==4) || (mapc==7)) {
                    sxoff = 0; dxoff = 0; }
                if ((mapc==3) || (mapc==6) || (mapc==9)) {
                    dxoff = neww-mapwidth; if (dxoff<0) dxoff = 0;
                    sxoff = mapwidth-neww; if (sxoff<0) sxoff = 0; }
                if ((mapc==2) || (mapc==5) || (mapc==8)) {
                    dxoff = (neww-mapwidth)/2; if (dxoff<0) { sxoff = -dxoff; dxoff = 0; } }

                if ((mapc==1) || (mapc==2) || (mapc==3)) {
                    syoff = 0; dyoff = 0; }
                if ((mapc==4) || (mapc==5) || (mapc==6)) {
                    dyoff = (newh-mapheight)/2; if (dyoff<0) { syoff = -dyoff; dyoff = 0; } }
                if ((mapc==7) || (mapc==8) || (mapc==9)) {
                    dyoff = newh-mapheight; if (dyoff<0) dyoff = 0;
                    syoff = mapheight-newh; if (syoff<0) syoff = 0; }
            }

            {
                int copyh, copyw;
                copyh = newh - dyoff;
                if (copyh > (mapheight - syoff)) copyh = mapheight - syoff;
                if (copyh < 0) copyh = 0;
                copyw = neww - dxoff;
                if (copyw > (mapwidth - sxoff)) copyw = mapwidth - sxoff;
                if (copyw < 0) copyw = 0;
                j = copyh; while (j) {
                    spt = mapmappt[k]; dpt = temppt;
                    spt += sxoff; spt += syoff*mapwidth;
                    dpt += dxoff; dpt += dyoff*neww;
                    syoff++; dyoff++;
                    i = copyw;
                    while (i) {
                        *dpt = *spt; spt++; dpt++;
                        i--;
                    }
                    j--;
                }
            }
            free(mapmappt[k]); mapmappt[k] = temppt; } }

    mapwidth = neww; mapheight = newh; xmapoffset = 0; ymapoffset = 0;
    mappt = mapmappt[maplayer];
    return 0;
}

void FillwithPattern(int x, int y, int fstyle) {
    int i, j, mybrushwidth, mybrushheight, mybrshx, mybrshy, myinitx;
    short int * mymappt, * mybrshpt;

    mybrushwidth = 0;
    mybrushheight = 0;
    mybrshx = 0;
    mybrshy = 0;
    myinitx = 0;
    mybrshpt = NULL;

    if (curbrsh>0 && curbrsh<=MAXBRSH) {
        if ((blockstaggerx || blockstaggery) && y&1) y--;
        mybrshpt = brshpt[curbrsh-1];
        if (mybrshpt==NULL || mybrshpt[0] < 1 || mybrshpt[1] < 1) {
            curbrsh = 0; curanim = -1; curstr = 0;
            mybrshpt = NULL;
        } else {
            mybrushwidth = *mybrshpt; mybrshpt++;
            mybrushheight = *mybrshpt; mybrshpt++;
            if (!fstyle) {
                mybrshx = (mybrushwidth-(x%mybrushwidth));
                if (mybrshx==mybrushwidth) mybrshx = 0;
                myinitx = mybrshx;
                mybrshy = (mybrushheight-(y%mybrushheight));
                if (mybrshy==mybrushheight) mybrshy = 0;
                mybrshpt += mybrshx+(mybrshy*mybrushwidth);
        } }
    }

    mymappt = mappt;
    for (j=0;j<mapheight;j++) {
        for (i=0;i<mapwidth;i++) {
            if (*mymappt==0x7fff) {
                if (curbrsh>0 && curbrsh<=MAXBRSH) {
                    if (!fstyle) *mymappt = *mybrshpt;
                    else *mymappt = mybrshpt[(rand()%mybrushwidth)+((rand()%mybrushheight)*mybrushwidth)];
                } else { if (curanim!=-1 && animstrpt!=NULL) {
                        *mymappt = -((curanim+1)*FMP_ANI_MEMUNIT);
                    } else {
                        *mymappt = (curstr);
            } } }
            mymappt++;
            if (curbrsh>0 && curbrsh<=MAXBRSH && !fstyle) { mybrshpt++; mybrshx++;
                if (mybrshx==mybrushwidth) { mybrshx = 0; mybrshpt -= mybrushwidth; }
            }
        }
        if (curbrsh>0 && curbrsh<=MAXBRSH && !fstyle) { mybrshpt -= mybrshx; mybrshx = myinitx; mybrshpt += myinitx;
            mybrshy++; mybrshpt += mybrushwidth;
            if (mybrshy==mybrushheight) { mybrshy = 0; mybrshpt -= (mybrushheight*mybrushwidth); }
        }
    }
}

void FillaLine(MAPFILL * myfillpt) {
    int i, j;
    short int * fillmappt;

    fillmappt = mappt;
    fillmappt += myfillpt->yoff*mapwidth;

    i = (int) myfillpt->leftedge;
    j = (int) myfillpt->width;
    while (j) {
        fillmappt[i] = 0x7fff; /* Fill with 0x7fff so as not to continually overwrite... */
        i++; j--;
    }
}

short int AddaLine(MAPFILL * myfillpt, int fxoff, int fyoff, short int fillval) {
    short int filllength, returnfill;
    short int * mymappt;
    MAPFILL * mapfill_end = myfillpt + MAPFILL_MAX;

    if (fxoff<0 || fxoff>=mapwidth) return 1;
    if (fyoff<0 || fyoff>=mapheight) return 1;
    mymappt = mappt;
    mymappt += fyoff*mapwidth;
    if (fillval==-1) { fillval = mymappt[fxoff]; returnfill = fillval; } else returnfill = -1;
    if (mymappt[fxoff]!=fillval) return 1;

    while (myfillpt->leftedge!=-1) {
        if (myfillpt >= mapfill_end) return 1;
        if (myfillpt->leftedge==-2) {
            if (myfillpt + 1 >= mapfill_end) return 1;
            myfillpt++; myfillpt->leftedge=-2; myfillpt--; break;
        }
        myfillpt++;
    }

    while (fxoff>=0) { if (mymappt[fxoff]!=fillval) break; fxoff--; } fxoff++;
    myfillpt->yoff = (short int) fyoff;
    myfillpt->leftedge = (short int) fxoff;
    filllength = 0; while (fxoff<mapwidth) { if (mymappt[fxoff]!=fillval) break; filllength++; fxoff++; }
    myfillpt->width = filllength;
    if (returnfill==-1) return filllength; else return returnfill;
}

void MapFill(int x, int y, int fillstyle) {
    int fillx, filly;
    short int tofill, linestodo, checkxpos;
    MAPFILL * mapfillpt, * thisfillpt;

    if (mappt==NULL) return;
    if (x<0 || x>=mapwidth) return;
    if (y<0 || y>=mapheight) return;

    mapfillpt = malloc(sizeof(MAPFILL)*MAPFILL_MAX);
    if (mapfillpt==NULL) return;
    mapfillpt->leftedge=-2;

    fillx = x; filly = y;
    tofill = AddaLine(mapfillpt, x, y, -1);

    while (1) {
        linestodo = 0;
        thisfillpt = mapfillpt; while (thisfillpt < mapfillpt + MAPFILL_MAX && thisfillpt->leftedge!=-2) {
            if (thisfillpt->leftedge!=-1) {
                FillaLine(thisfillpt);
                checkxpos = thisfillpt->leftedge;
                while (checkxpos<(thisfillpt->leftedge+thisfillpt->width))
                    checkxpos += AddaLine(mapfillpt, checkxpos, thisfillpt->yoff-1, tofill);
                checkxpos = thisfillpt->leftedge;
                while (checkxpos<(thisfillpt->leftedge+thisfillpt->width))
                    checkxpos += AddaLine(mapfillpt, checkxpos, thisfillpt->yoff+1, tofill);
                thisfillpt->leftedge = -1;
                linestodo = 1;
            }
            thisfillpt++;
        }
        if (!linestodo) break;
    }
    free(mapfillpt);
    FillwithPattern(fillx, filly, fillstyle);
}
