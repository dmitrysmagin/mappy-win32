/* import.c */
#define  STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"

extern short int blockwidth, blockheight, blockdepth, numblockgfx, numblockstr;
extern long int fgcolour;
extern char * blockgfxpt, * blockstrpt;
extern char * altgfxpt;
extern unsigned char * cmappt;

int imskip = 1;

void decode_pcx (unsigned char * mempt, int memend, unsigned char * tomempt, int width, int height, int bpp, int pitch)
{
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
	 }
	 else c = 1;

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
			 }
			 else if (x == pitch*2) {
		  xx = 0;
		  po = 0;
			 }
			 else
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

int IMImportBlocks (char *imname, int imstyle)
/* If imstyle==-2, alt 8bit are imported */
/* If imstyle==-1, new block structures are created for each block */
/* If imstyle==0, all block graphics are imported */
/* if imstyle>0, imstyle block graphics are imported */
{
int i, j, k, l, noblock, altnumb, isapcx, lastblockg, lastblocks;
FILE * fpt;
unsigned char * bmppt, * bmdatapt, * destpt, * srcpt, * srcpt2, * imcmappt, * mempt;
unsigned char red, green, blue;
long int filesize;
BITMAPFILEHEADER * bfh;
BITMAPINFOHEADER * bmihpt;
RGBQUAD * myrgbpt, * myrgbpt2;
//RGBQUAD colour;

	if (blockgfxpt==NULL) return -1;

	fpt = fopen (imname, "rb");
	if (fpt == NULL) return -1;
	fseek (fpt, 0, SEEK_END);
	filesize = ftell (fpt);
	fseek (fpt, 0, SEEK_SET);

	bmppt = malloc (filesize);
	if (bmppt == NULL) { fclose (fpt); return -1; }
	if (fread (bmppt, 1, filesize, fpt)!=filesize)
	{ fclose (fpt); free (bmppt); return -1; }
	fclose (fpt);

	isapcx = 0;
	if (!strcmp (imname+strlen(imname)-4, ".PCX") ||
		!strcmp (imname+strlen(imname)-4, ".pcx") ||
		!strcmp (imname+strlen(imname)-4, ".Pcx")) {

		isapcx = 1;
		mempt = bmppt;
		bmppt = malloc (sizeof (BITMAPINFOHEADER));
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
			free (bmppt);
			return -1;
		}

		mempt += 2;
		k = ((unsigned short *) mempt)[0];
		mempt += 62;

		bmdatapt = (unsigned char *) malloc ((bmihpt->biBitCount/8)*(bmihpt->biWidth+4)*bmihpt->biHeight);
		if (bmdatapt == NULL) return -1;

		decode_pcx (mempt, filesize, bmdatapt, bmihpt->biWidth, bmihpt->biHeight, bmihpt->biBitCount, k);
//		memset (bmdatapt, 0xF0, bmihpt->biWidth*bmihpt->biHeight);
		altnumb = 0;

	} else {

	bfh = (BITMAPFILEHEADER *) bmppt;
	if (bfh->bfType!=19778) { free (bmppt); return -1; } /* 'BM' identifier 19778*/

	bmdatapt = bmppt + bfh->bfOffBits;
	bmihpt = (BITMAPINFOHEADER *) (bmppt + sizeof (BITMAPFILEHEADER));
	myrgbpt = (RGBQUAD *) (((char *) bmihpt) + bmihpt->biSize);

	if (bmihpt->biCompression != BI_RGB) { free (bmppt); return -1; }

	altnumb = 0;
	if (imstyle == -2 && bmihpt->biBitCount!=8) { free (bmppt); return -1; }

	if (blockdepth==8 && bmihpt->biBitCount!=8) { free (bmppt); return -1; }

	if (bmihpt->biBitCount!=8 && bmihpt->biBitCount!=24) { free (bmppt); return -1; }
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
							if (imstyle==-1 && (numblockstr < MAXBLKSTR)) { memset (blockstrpt+(numblockstr*SOBSTR), 0, SOBSTR);
								*((long int *) (blockstrpt+(numblockstr*SOBSTR))) = numblockgfx*blockwidth*blockheight*((blockdepth+1)/8);
								numblockstr++; }
							if (imstyle == -2) {
								altnumb++;
								if (altnumb == numblockgfx) { free (bmppt); if (isapcx) free (bmdatapt); return 0; }
							} else {
								numblockgfx++;
							}
							if (noblock) { lastblockg = numblockgfx; lastblocks = numblockstr; }
							if (imstyle == 1) { free (bmppt); if (isapcx) free (bmdatapt); return 0; }
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
				if (blockdepth == 8) { free (bmppt); if (isapcx) free (bmdatapt); return -1; }
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
							if (imstyle==-1 && (numblockstr < MAXBLKSTR)) { memset (blockstrpt+(numblockstr*SOBSTR), 0, SOBSTR);
								*((long int *) (blockstrpt+(numblockstr*SOBSTR))) = numblockgfx*blockwidth*blockheight*((blockdepth+1)/8);
								numblockstr++; }
							numblockgfx++;
							if (noblock) { lastblockg = numblockgfx; lastblocks = numblockstr; }
							if (imstyle==1) { free (bmppt); if (isapcx) free (bmdatapt); return 0; }
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

	if (isapcx) free (bmdatapt);
	free (bmppt);
	if (imstyle == -2) return -1;
	return 0;
}