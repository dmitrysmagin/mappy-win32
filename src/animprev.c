#define DIRECTDRAW_VERSION 0x0300
#ifndef _WIN32
#define _WIN32
#endif
#define  NO_STRICT
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ddraw.h>
#include "mapwin.h"
#include "compat.h"

/* MSVC just had to be different... */
#ifndef __GNUC__
#ifndef inline
#define inline
#endif
#endif

/*
#define IDC_APRVMODE	101
#define IDC_APRVDRBG	102
#define IDC_APRVDRFG1	103
#define IDC_APRVDRFG2	104
#define IDC_APRVDRFG3	105
#define IDC_APRVTRAN	106
#define IDC_APRVBSTR	107
#define IDC_APRVTIDX	108
#define IDC_APRVTTRU	109
#define IDC_APRVLFPS 110
*/

#define MMOX 0
#define MMOY 0
#define MSCRW apwidth
#define MSCRH apheight
#define ANDTMEM (65536*4)

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

void InitAnims (void);
void UpdateAnims (void);

extern HINSTANCE hInstance;
extern unsigned char transred, transgreen, transblue, trans8bit;
extern short int mapwidth, mapheight, blockwidth, blockheight, blockdepth;
extern short int xmapoffset, ymapoffset, numblockstr, numblockgfx;
extern short int * mappt;
extern int blockgapx, blockgapy, blockstaggerx, blockstaggery;
extern int isopillars;
extern char * blockgfxpt;
extern char * blockstrpt;
extern char * animstrpt;
extern unsigned char * cmappt;
extern char * altgfxpt;

static char szBigWindowname[] = "Mappy - DirectX preview Window";
static char apcurrentmode[80];
static HWND aphwnd;
static int gameactive, gamerunning, apendpreview;
static int framecount, tickdec, tickfps;
static int scrlxoff, scrlyoff, keyleft, keyright, keyup, keydown;
static int apwidth, apheight, apdepth, aprate, apfps, aptran8, aptranhi, apparatype;
static int showstrlayers, parastr, onetilememory;
static DWORD newticks, oldticks, apfpsoldticks;
static LPDIRECTDRAWSURFACE * maplpDDSTiles = NULL;	// Tiles Direct Draw Surfaces
static LPDIRECTDRAWSURFACE paratile;
static char * mapanimstrendpt;

static LPDIRECTDRAW			lpDD1;				// DirectDraw object
static LPDIRECTDRAW2			lpDD;				// DirectDraw object (the one used)
static LPDIRECTDRAWSURFACE	lpDDSPrimary;	// DirectDraw primary surface
static LPDIRECTDRAWSURFACE	lpDDSBack;		// DirectDraw back surface
static LPDIRECTDRAWPALETTE	lpddpal = NULL;	// a pointer to the created dd palette
static DDSURFACEDESC			ddsd;				// a direct draw surface description struct
static DDSCAPS					ddscaps;			// a direct draw surface capabilities struct
static HRESULT					ddrval;			// result back from dd calls

/* Function 'lifted' and modified from the MappyDX library */

void MapCMAPtoPE (unsigned char * mycmappt, PALETTEENTRY * mypept)
{
int i;
	for (i=0;i<256;i++)
	{
		mypept[i].peRed = (BYTE) *mycmappt; mycmappt++;
		mypept[i].peGreen = (BYTE) *mycmappt; mycmappt++;
		mypept[i].peBlue = (BYTE) *mycmappt; mycmappt++;
		mypept[i].peFlags = PC_NOCOLLAPSE | PC_RESERVED;;
	}
}

void FreeTileSurfaces (void)
{
int i;

	if (maplpDDSTiles != NULL) {
	i = 0; while (maplpDDSTiles[i] != NULL) {
		IDirectDrawSurface2_Release (maplpDDSTiles[i]);
//		maplpDDSTiles[i] = NULL;
		i++;
	}
	free (maplpDDSTiles); maplpDDSTiles = NULL;
	}
	if (paratile!=NULL) IDirectDrawSurface2_Release (paratile); paratile = NULL;
}

int MapHighTo8 (void)
{
int i, j, k, bwpitch;
char * tempblockgfxpt;
unsigned char * temp24bitbuffer, * blkgfxpt;
HDC temphdc, myhdc;
HBITMAP htempbm, holdbm;
BITMAPINFOHEADER bih;
RGBQUAD rgbq;

/* This isn't needed, but left for reference
	if (mapdepth == 8)
	{
		memset (&tempbm, 0, sizeof(BITMAP));
		tempbm.bmWidth = mapblockwidth;
		tempbm.bmHeight = mapblockheight;
		tempbm.bmWidthBytes = mapblockwidth;
		tempbm.bmPlanes = 1;
		tempbm.bmBitsPixel = mapdepth;
	} else {
*/
	memset (&bih, 0, sizeof(BITMAPINFOHEADER));
	memset (&rgbq, 0, sizeof(RGBQUAD));
	bih.biSize = sizeof (BITMAPINFOHEADER);
	bih.biWidth = blockwidth;
	bih.biHeight = -blockheight;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;

	bwpitch = ((blockwidth*3)+3)&0x7FFC;
	temp24bitbuffer = (unsigned char *) malloc (bwpitch*blockheight);
	if (temp24bitbuffer == NULL) return -1;
/* This isn't needed, but left for reference
	}
*/
	tempblockgfxpt = blockgfxpt;
	temphdc = CreateCompatibleDC (NULL);
	for (i=0;i<numblockgfx;i++)
	{
		if ((IDirectDrawSurface2_GetDC (maplpDDSTiles[i], &myhdc)) != DD_OK) return -1;
		switch (blockdepth)
		{
/* This isn't needed, but left for reference
		case 8:
			tempbm.bmBits = tempblockgfxpt;
			htempbm = CreateBitmapIndirect (&tempbm);
			if (htempbm == NULL) return -1;
			break;
*/
		case 15:
			blkgfxpt = (unsigned char *) tempblockgfxpt;
			for (k=0;k<blockheight;k++) {
				for (j=0;j<(blockwidth*3);j+=3)
				{
					temp24bitbuffer[0+j+(k*bwpitch)] = (unsigned char) ((*(blkgfxpt+1)&0x1F)<<3);
					temp24bitbuffer[1+j+(k*bwpitch)] = (unsigned char) (((((*blkgfxpt)&0x3)<<3)|((*(blkgfxpt+1))>>5))<<3);
					temp24bitbuffer[2+j+(k*bwpitch)] = (unsigned char) (((*blkgfxpt)&0x7C)<<1);
					temp24bitbuffer[0+j+(k*bwpitch)] |= ((temp24bitbuffer[0+j+(k*bwpitch)]>>5)&0x07);
					temp24bitbuffer[1+j+(k*bwpitch)] |= ((temp24bitbuffer[1+j+(k*bwpitch)]>>5)&0x07);
					temp24bitbuffer[2+j+(k*bwpitch)] |= ((temp24bitbuffer[2+j+(k*bwpitch)]>>5)&0x07);
					blkgfxpt += 2;
				}
			}
			htempbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
				 temp24bitbuffer, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
			if (htempbm == NULL) return -1;
			break;
		case 16:
			blkgfxpt = (unsigned char *) tempblockgfxpt;
			for (k=0;k<blockheight;k++) {
				for (j=0;j<(blockwidth*3);j+=3)
				{
					temp24bitbuffer[0+j+(k*bwpitch)] = (unsigned char) ((*(blkgfxpt+1)&0x1F)<<3);
					temp24bitbuffer[1+j+(k*bwpitch)] = (unsigned char) (((((*blkgfxpt)&0x7)<<3)|((*(blkgfxpt+1))>>5))<<2);
					temp24bitbuffer[2+j+(k*bwpitch)] = (unsigned char) ((*blkgfxpt)&0xF8);
					temp24bitbuffer[0+j+(k*bwpitch)] |= ((temp24bitbuffer[0+j+(k*bwpitch)]>>5)&0x07);
					temp24bitbuffer[1+j+(k*bwpitch)] |= ((temp24bitbuffer[1+j+(k*bwpitch)]>>6)&0x03);
					temp24bitbuffer[2+j+(k*bwpitch)] |= ((temp24bitbuffer[2+j+(k*bwpitch)]>>5)&0x07);
					blkgfxpt += 2;
				}
			}
			htempbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
				 temp24bitbuffer, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
			if (htempbm == NULL) return -1;
			break;
		case 24:
			blkgfxpt = (unsigned char *) tempblockgfxpt;
			for (k=0;k<blockheight;k++) {
				for (j=0;j<(blockwidth*3);j+=3)
				{
					temp24bitbuffer[2+j+(k*bwpitch)] = *blkgfxpt; blkgfxpt++;
					temp24bitbuffer[1+j+(k*bwpitch)] = *blkgfxpt; blkgfxpt++;
					temp24bitbuffer[0+j+(k*bwpitch)] = *blkgfxpt; blkgfxpt++;
				}
			}
			htempbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
				 temp24bitbuffer, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
			if (htempbm == NULL) return -1;
			break;
		case 32:
		default:
			blkgfxpt = (unsigned char *) tempblockgfxpt;
			for (k=0;k<blockheight;k++) {
				for (j=0;j<(blockwidth*3);j+=3)
				{
					blkgfxpt ++;
					temp24bitbuffer[2+j+(k*bwpitch)] = *blkgfxpt; blkgfxpt++;
					temp24bitbuffer[1+j+(k*bwpitch)] = *blkgfxpt; blkgfxpt++;
					temp24bitbuffer[0+j+(k*bwpitch)] = *blkgfxpt; blkgfxpt++;
				}
			}
			htempbm = CreateDIBitmap (myhdc, &bih, CBM_INIT,
				 temp24bitbuffer, (BITMAPINFO *) &bih, DIB_RGB_COLORS);
			if (htempbm == NULL) return -1;
			break;
		}
		holdbm = (HBITMAP) SelectObject (temphdc, htempbm);
		BitBlt (myhdc, 0, 0, blockwidth, blockheight, temphdc, 0, 0, SRCCOPY);
		SelectObject (temphdc, holdbm);
		DeleteObject (htempbm);
		IDirectDrawSurface2_ReleaseDC (maplpDDSTiles[i], myhdc);
		tempblockgfxpt += blockwidth*blockheight*((blockdepth+1)/8);
	}
	DeleteDC (temphdc);
/* This line was replaced by the one below
	if (mapdepth != 8) free (temp24bitbuffer);
*/
	free (temp24bitbuffer);
	return 0;
}

int CopyBlocksToSurfaces (void)
{
unsigned int i, j, k, rshift, gshift, bshift, rbits, gbits, bbits, rval, gval, bval;
unsigned char * tempblockgfxpt, * tempsurfacept;
DDSURFACEDESC mddsd;
DDPIXELFORMAT mddpf;
RECT cliprect;

	rshift = gshift = bshift = 0;
	rbits = gbits = bbits = 0;
	memset (&mddpf, 0, sizeof(DDPIXELFORMAT));
	mddpf.dwSize = sizeof(DDPIXELFORMAT);
	if (IDirectDrawSurface2_GetPixelFormat (maplpDDSTiles[0], &mddpf)!=DD_OK) return -1;
	if (altgfxpt != NULL && mddpf.dwRGBBitCount == 8) tempblockgfxpt = (unsigned char *) altgfxpt;
	else tempblockgfxpt = (unsigned char *) blockgfxpt;
	if (mddpf.dwRGBBitCount!=8) {
		j = (int) mddpf.dwRBitMask; rshift = 0;
		i = 1; while (!(i&j)) { rshift++; i<<=1; }
		rbits = 0; while (i&j) { rbits++; i<<=1; }
		j = (int) mddpf.dwGBitMask; gshift = 0;
		i = 1; while (!(i&j)) { gshift++; i<<=1; }
		gbits = 0; while (i&j) { gbits++; i<<=1; }
		j = (int) mddpf.dwBBitMask; bshift = 0;
		i = 1; while (!(i&j)) { bshift++; i<<=1; }
		bbits = 0; while (i&j) { bbits++; i<<=1; }
	}

	if (blockdepth!=8 && mddpf.dwRGBBitCount==8 && altgfxpt==NULL) MapHighTo8 ();
	if ((blockdepth==8 || altgfxpt!=NULL) && mddpf.dwRGBBitCount==8) {
		for (i=0;i<((unsigned int)numblockgfx);i++)
		{
			memset (&mddsd, 0, sizeof(DDSURFACEDESC));
			mddsd.dwSize = sizeof(DDSURFACEDESC);
			if (IDirectDrawSurface2_Lock (maplpDDSTiles[i], NULL, &mddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_SURFACEMEMORYPTR, NULL)!=DD_OK) return -1;
			tempsurfacept = (unsigned char *) mddsd.lpSurface;
			for (j=blockheight;j>0;j--)
			{
				memcpy (tempsurfacept, tempblockgfxpt, blockwidth);
				tempsurfacept += mddsd.lPitch;
				tempblockgfxpt += blockwidth;
			}
			IDirectDrawSurface2_Unlock (maplpDDSTiles[i], NULL);
		}
	} else {

	for (i=0;i<((unsigned int)numblockgfx);i++)
	{
		memset (&mddsd, 0, sizeof(DDSURFACEDESC));
		mddsd.dwSize = sizeof(DDSURFACEDESC);
		if (IDirectDrawSurface2_Lock (maplpDDSTiles[i], NULL, &mddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_SURFACEMEMORYPTR, NULL)!=DD_OK) return -1;
		for (j=0;j<((unsigned int)blockheight);j++) {
		tempsurfacept = (unsigned char *) mddsd.lpSurface;
		tempsurfacept += j*mddsd.lPitch;
		for (k=blockwidth;k>0;k--) {
		switch (blockdepth) {
			case 8:
				rval = cmappt[tempblockgfxpt[0]*3];
				rval >>= (8-rbits);
				gval = cmappt[tempblockgfxpt[0]*3+1];
				gval >>= (8-gbits);
				bval = cmappt[tempblockgfxpt[0]*3+2];
				bval >>= (8-bbits);
				tempblockgfxpt++;
				break;
			case 15:
				rval = ((((unsigned long int)tempblockgfxpt[0])<<8)|((unsigned long int)tempblockgfxpt[1]))&0x7E00;
				rval >>= (7+(8-rbits));
				gval = ((((unsigned long int)tempblockgfxpt[0])<<8)|((unsigned long int)tempblockgfxpt[1]))&0x03E0;
				gval >>= (2+(8-gbits));
				bval = ((((unsigned long int)tempblockgfxpt[0])<<8)|((unsigned long int)tempblockgfxpt[1]))&0x001F;
				bval <<= 3;
				bval >>= (8-bbits);
				tempblockgfxpt += 2;
				break;
			case 16:
				rval = ((((unsigned long int)tempblockgfxpt[0])<<8)|((unsigned long int)tempblockgfxpt[1]))&0xF800;
				rval >>= (8+(8-rbits));
				gval = ((((unsigned long int)tempblockgfxpt[0])<<8)|((unsigned long int)tempblockgfxpt[1]))&0x07E0;
				gval >>= (3+(8-gbits));
				bval = ((((unsigned long int)tempblockgfxpt[0])<<8)|((unsigned long int)tempblockgfxpt[1]))&0x001F;
				bval <<= 3;
				bval >>= (8-bbits);
				tempblockgfxpt += 2;
				break;
			case 24:
				rval = (unsigned long int) tempblockgfxpt[0];
				rval >>= (8-rbits);
				gval = (unsigned long int) tempblockgfxpt[1];
				gval >>= (8-gbits);
				bval = (unsigned long int) tempblockgfxpt[2];
				bval >>= (8-bbits);
				tempblockgfxpt += 3;
				break;
			case 32:
				rval = (unsigned long int) tempblockgfxpt[1];
				rval >>= (8-rbits);
				gval = (unsigned long int) tempblockgfxpt[2];
				gval >>= (8-gbits);
				bval = (unsigned long int) tempblockgfxpt[3];
				bval >>= (8-bbits);
				tempblockgfxpt += 4;
				break;
			default:
				return -1;
		}
		if (rbits < 8) rval |= (rval>>rbits);
		if (gbits < 8) gval |= (gval>>gbits);
		if (bbits < 8) bval |= (bval>>bbits);
		rval <<= rshift; rval &= mddpf.dwRBitMask;
		gval <<= gshift; gval &= mddpf.dwGBitMask;
		bval <<= bshift; bval &= mddpf.dwBBitMask;
		switch (mddpf.dwRGBBitCount) {
/* Apparently 16bits are reported even in 555 (15bit) mode */
			case 15:
			case 16:
				*((unsigned short int *)tempsurfacept) = (unsigned short int) (rval|gval|bval);
				tempsurfacept += 2;
				break;
			case 24:
				tempsurfacept[2] = (unsigned char) (((rval|gval|bval)>>16)&0xFF);
				tempsurfacept[1] = (unsigned char) (((rval|gval|bval)>>8)&0xFF);
				tempsurfacept[0] = (unsigned char) ((rval|gval|bval)&0xFF);
				tempsurfacept += 3;
				break;
			case 32:
				tempsurfacept[3] = (unsigned char) (((rval|gval|bval)>>24)&0xFF);
				tempsurfacept[2] = (unsigned char) (((rval|gval|bval)>>16)&0xFF);
				tempsurfacept[1] = (unsigned char) (((rval|gval|bval)>>8)&0xFF);
				tempsurfacept[0] = (unsigned char) ((rval|gval|bval)&0xFF);
				tempsurfacept += 4;
				break;
		}
	} }
		IDirectDrawSurface2_Unlock (maplpDDSTiles[i], NULL);
	}
	}

	if (paratile!=NULL) {
		cliprect.top = 0; cliprect.left = 0; cliprect.right = blockwidth; cliprect.bottom = blockheight;
		IDirectDrawSurface2_BltFast (paratile, 0, 0, maplpDDSTiles[parastr], &cliprect, DDBLTFAST_WAIT );
		IDirectDrawSurface2_BltFast (paratile, blockwidth, 0, maplpDDSTiles[parastr], &cliprect, DDBLTFAST_WAIT );
		IDirectDrawSurface2_BltFast (paratile,0, blockheight, maplpDDSTiles[parastr], &cliprect, DDBLTFAST_WAIT );
		IDirectDrawSurface2_BltFast (paratile, blockwidth, blockheight, maplpDDSTiles[parastr], &cliprect, DDBLTFAST_WAIT );
	}

	return 0;
}

int APMapRestore (void)
{
int i;

	i = 0;
	while (maplpDDSTiles[i] != NULL)
	{
		if (IDirectDrawSurface2_Restore (maplpDDSTiles[i]) != DD_OK) return -1;
		i++;
	}
	if (paratile!=NULL) IDirectDrawSurface2_Restore (paratile);
	return CopyBlocksToSurfaces ();
}

DWORD MapTranspToPixel (LPDDPIXELFORMAT lpDDpf)
{
DWORD transpresult;
unsigned int i, j, rshift, gshift, bshift, rbits, gbits, bbits;

		transpresult = 0;
		if (lpDDpf->dwRGBBitCount>8) {
			j = (int) lpDDpf->dwRBitMask; rshift = 0;
			i = 1; while (!(i&j)) { rshift++; i<<=1; }
			rbits = 0; while (i&j) { rbits++; i<<=1; }
			j = (int) lpDDpf->dwGBitMask; gshift = 0;
			i = 1; while (!(i&j)) { gshift++; i<<=1; }
			gbits = 0; while (i&j) { gbits++; i<<=1; }
			j = (int) lpDDpf->dwBBitMask; bshift = 0;
			i = 1; while (!(i&j)) { bshift++; i<<=1; }
			bbits = 0; while (i&j) { bbits++; i<<=1; }

			transpresult = (((((aptranhi>>16)&0xFF)<<rshift)>>(8-rbits))&lpDDpf->dwRBitMask) |
								(((((aptranhi>>8)&0xFF)<<gshift)>>(8-gbits))&lpDDpf->dwGBitMask) |
								((((aptranhi&0xFF)<<bshift)>>(8-bbits))&lpDDpf->dwBBitMask);
		}
		return transpresult;
}

int MapCreateTileSurfaces (LPDIRECTDRAW2 mylpDD)
{
int i;
DDSURFACEDESC mddsd;
DDPIXELFORMAT mddpf;
DDCOLORKEY ddck;
DWORD transpcol;

	aptran8 = trans8bit;
	if (blockdepth > 8) {
	aptranhi = (((unsigned long int)transred)<<16)|(((unsigned long int) transgreen)<<8)|(transblue);
	} else {
	aptranhi = (((unsigned long int)(cmappt[aptran8*3]))<<16)|(((unsigned long int) (cmappt[aptran8*3+1]))<<8)|((cmappt[aptran8*3+2]));
	}
	transpcol = 0;
	memset (&mddsd, 0, sizeof (mddsd));
	memset (&mddpf, 0, sizeof (mddpf));
	memset (&ddck, 0, sizeof (ddck));

	mddpf.dwSize = sizeof (mddpf);

	for (i=0;i<numblockgfx;i++)
	{
		mddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		mddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		mddsd.dwHeight = blockheight;
		mddsd.dwWidth = blockwidth;
		mddsd.dwSize = sizeof (mddsd);
		if (IDirectDraw2_CreateSurface (mylpDD, &mddsd, &maplpDDSTiles[i], NULL)!= DD_OK) { FreeTileSurfaces (); return -1; }

		if (!i) {
			if (IDirectDrawSurface2_GetPixelFormat (maplpDDSTiles[i], &mddpf)!=DD_OK) { FreeTileSurfaces(); return -1; }
			if (mddpf.dwRGBBitCount!=8) { transpcol = MapTranspToPixel (&mddpf); }
			else transpcol = aptran8;
		}
		ddck.dwColorSpaceLowValue = transpcol;
		ddck.dwColorSpaceHighValue = transpcol;
		IDirectDrawSurface2_SetColorKey (maplpDDSTiles[i], DDCKEY_SRCBLT, &ddck );
	}
	maplpDDSTiles[i] = NULL;
	mddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	mddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	mddsd.dwHeight = blockheight*2;
	mddsd.dwWidth = blockwidth*2;
	mddsd.dwSize = sizeof (mddsd);

	if (IDirectDraw2_CreateSurface (mylpDD, &mddsd, &paratile, NULL)!= DD_OK) { FreeTileSurfaces (); return -1; }
	if (CopyBlocksToSurfaces ()) { FreeTileSurfaces (); return -1; }
	return 0;
}

inline LPDIRECTDRAWSURFACE APOfftoSurf (long int gfxoff)
{
	return maplpDDSTiles[gfxoff/onetilememory];
}

inline BLKSTR * APAnimToBStr (ANISTR * theanpt)
{
	return (BLKSTR *) (blockstrpt+*((long int *)(mapanimstrendpt+theanpt->ancuroff)));
}

int APDrawParallax (LPDIRECTDRAWSURFACE lpDDSDest, int mapxo, int mapyo)
/* lpDDSDest = lpdirectdrawsurface.
 * mapxo = offset, in pixels, from the left edge of the map.
 * mapyo = offset, in pixels, from the top edge of the map.
 */
{
RECT cliprect;
int i, j, xedge, yedge, mapvclip, maphclip, indentx, indenty;
short int *mymappt;
short int *mymap2pt;
BLKSTR *blkdatapt;
ANISTR *myanpt;

	if (blockstaggerx || blockstaggery) return 0;
	if (blockwidth != blockgapx || blockheight != blockgapy) return 0;
/* first, draw blocks that are clipped horizontally (left) */
	maphclip = mapxo%blockwidth;	/* Number of pixels to clip from left */
	mapvclip = mapyo%blockheight;	/* Number of pixels to clip from top */
	indentx = blockwidth-((mapxo/2)%blockwidth);	/* Number of pixels to indent from left */
	indenty = blockheight-((mapyo/2)%blockheight);	/* Number of pixels to indent from top */

	mymappt = (short int *) mappt;
	mymappt += (mapxo/blockwidth)+((mapyo/blockheight)*mapwidth);

/* Draw blocks */
	i = (MMOY-mapvclip);
	while (i<(MSCRH+MMOY)) {
		mymap2pt = mymappt;
		mymap2pt += mapwidth;
		j = (MMOX-maphclip);
		while (j<(MSCRW+MMOX)) {
			if (*mymappt>=0) blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)(*mymappt))*SOBSTR));
			else { myanpt = (ANISTR *) (mapanimstrendpt + *mymappt);
				blkdatapt = APAnimToBStr (myanpt); }
			if (j < 0) cliprect.left = -j; else cliprect.left = 0;
			if (i < 0) cliprect.top = -i; else cliprect.top = 0;
			if (j > ((MSCRW+MMOX)-blockwidth)) cliprect.right = (MSCRW+MMOX)-j; else cliprect.right = blockwidth;
			if (i > ((MSCRH+MMOY)-blockheight)) cliprect.bottom = (MSCRH+MMOY)-i; else cliprect.bottom = blockheight;
			if (cliprect.left < blockwidth && cliprect.top < blockheight) {
				if (j < 0) xedge = 0; else xedge = j;
				if (i < 0) yedge = 0; else yedge = i;
				cliprect.left += indentx;
				cliprect.top += indenty;
				cliprect.right += indentx;
				cliprect.bottom += indenty;
				if (blkdatapt->trigger) {
					if (IDirectDrawSurface2_BltFast (lpDDSDest, xedge, yedge, paratile, &cliprect, DDBLTFAST_WAIT)!=DD_OK)
						return -1;
				}
			}
			mymappt++; j += blockgapx;
		}
		mymappt = mymap2pt;
		i += blockgapy;
	}
	return 0;
}

static int FGBlit (LPDIRECTDRAWSURFACE lpDDSDest, int x, int y, LPDIRECTDRAWSURFACE mapgfxpt)
{
RECT cliprect;
int xedge, yedge;

	if (x <= (MMOX-blockwidth) || y <= (MMOY-blockheight)) return 0;
	if (x >= (MSCRW+MMOX) || y >= (MSCRH+MMOY)) return 0;
	if (x < 0) cliprect.left = -x; else cliprect.left = 0;
	if (y < 0) cliprect.top = -y; else cliprect.top = 0;
	if (x > ((MSCRW+MMOX)-blockwidth)) cliprect.right = (MSCRW+MMOX)-x; else cliprect.right = blockwidth;
	if (y > ((MSCRH+MMOY)-blockheight)) cliprect.bottom = (MSCRH+MMOY)-y; else cliprect.bottom = blockheight;
	if (x < 0) xedge = 0; else xedge = x;
	if (y < 0) yedge = 0; else yedge = y;
	return IDirectDrawSurface2_BltFast (lpDDSDest, xedge, yedge, mapgfxpt, &cliprect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
}


static int FGBlitleft (LPDIRECTDRAWSURFACE lpDDSDest, int x, int y, LPDIRECTDRAWSURFACE mapgfxpt)
{
RECT cliprect;
int xedge, yedge;

	if (x <= (MMOX-(blockwidth/2)) || y <= (MMOY-blockheight)) return 0;
	if (x >= (MSCRW+MMOX) || y >= (MSCRH+MMOY)) return 0;
	if (x < 0) cliprect.left = -x; else cliprect.left = 0;
	if (y < 0) cliprect.top = -y; else cliprect.top = 0;
	if (x > ((MSCRW+MMOX)-(blockwidth/2))) cliprect.right = (MSCRW+MMOX)-x; else cliprect.right = blockwidth/2;
	if (y > ((MSCRH+MMOY)-blockheight)) cliprect.bottom = (MSCRH+MMOY)-y; else cliprect.bottom = blockheight;
	if (x < 0) xedge = 0; else xedge = x;
	if (y < 0) yedge = 0; else yedge = y;
	return IDirectDrawSurface2_BltFast (lpDDSDest, xedge, yedge, mapgfxpt, &cliprect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
}

static int FGBlitright (LPDIRECTDRAWSURFACE lpDDSDest, int x, int y, LPDIRECTDRAWSURFACE mapgfxpt)
{
RECT cliprect;
int xedge, yedge;

	x += (blockwidth/2);
	if (x <= (MMOX-(blockwidth/2)) || y <= (MMOY-blockheight)) return 0;
	if (x >= (MSCRW+MMOX) || y >= (MSCRH+MMOY)) return 0;
	if (x < 0) cliprect.left = (blockwidth/2)-x; else cliprect.left = (blockwidth/2);
	if (y < 0) cliprect.top = -y; else cliprect.top = 0;
	if (x > ((MSCRW+MMOX)-(blockwidth/2))) cliprect.right = ((MSCRW+MMOX)-x)+(blockwidth/2); else cliprect.right = blockwidth;
	if (y > ((MSCRH+MMOY)-blockheight)) cliprect.bottom = (MSCRH+MMOY)-y; else cliprect.bottom = blockheight;
	if (x < 0) xedge = 0; else xedge = x;
	if (y < 0) yedge = 0; else yedge = y;
	return IDirectDrawSurface2_BltFast (lpDDSDest, xedge, yedge, mapgfxpt, &cliprect, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
}

static int BGBlit (LPDIRECTDRAWSURFACE lpDDSDest, int x, int y, LPDIRECTDRAWSURFACE mapgfxpt)
{
RECT cliprect;
int xedge, yedge;

	if (x <= (MMOX-blockwidth) || y <= (MMOY-blockheight)) return 0;
	if (x >= (MSCRW+MMOX) || y >= (MSCRH+MMOY)) return 0;
	if (x < 0) cliprect.left = -x; else cliprect.left = 0;
	if (y < 0) cliprect.top = -y; else cliprect.top = 0;
	if (x > ((MSCRW+MMOX)-blockwidth)) cliprect.right = (MSCRW+MMOX)-x; else cliprect.right = blockwidth;
	if (y > ((MSCRH+MMOY)-blockheight)) cliprect.bottom = (MSCRH+MMOY)-y; else cliprect.bottom = blockheight;
	if (x < 0) xedge = 0; else xedge = x;
	if (y < 0) yedge = 0; else yedge = y;
	return IDirectDrawSurface2_BltFast (lpDDSDest, xedge, yedge, mapgfxpt, &cliprect, DDBLTFAST_WAIT);
}

int APMapDrawBG (LPDIRECTDRAWSURFACE lpDDSDest, int mapxo, int mapyo)
/* lpDDSDest = lpdirectdrawsurface.
 * mapxo = offset, in pixels, from the left edge of the map.
 * mapyo = offset, in pixels, from the top edge of the map.
 */
{
int i, j, mapvclip, maphclip;
short int *mymappt;
short int *mymap2pt;
BLKSTR *blkdatapt;
ANISTR *myanpt;

	mapxo -= blockstaggerx; mapyo -= blockstaggery;
	maphclip = mapxo%blockgapx;	/* Number of pixels to clip from left */
	mapvclip = mapyo%blockgapy;	/* Number of pixels to clip from top */

	mymappt = (short int *) mappt;
	if (blockstaggerx || blockstaggery) {
		mymappt += (mapxo/blockgapx)+((mapyo/blockgapy)*mapwidth*2);
	} else {
		mymappt += (mapxo/blockgapx)+((mapyo/blockgapy)*mapwidth);
	}

/* Draw blocks */
	i = (MMOY-mapvclip);
	if (blockstaggerx || blockstaggery) i -= blockgapy;
	while (i<(MSCRH+MMOY)) {
		mymap2pt = mymappt;
//		if (blockstaggerx || blockstaggery) mymap2pt += (mapwidth*2); else mymap2pt += mapwidth;
		mymap2pt += mapwidth;
		j = (MMOX-maphclip);
		if (blockstaggerx || blockstaggery) j -= blockgapx;
		while (j<(MSCRW+MMOX)) {
			if (*mymappt>=0) blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)(*mymappt))*SOBSTR));
			else { myanpt = (ANISTR *) (mapanimstrendpt + *mymappt);
				blkdatapt = APAnimToBStr (myanpt); }
			if (blockstaggerx || blockstaggery) {
				if (FGBlit (lpDDSDest, j, i, APOfftoSurf(blkdatapt->bgoff))!=DD_OK)
					return -1;
			} else {
				if (BGBlit (lpDDSDest, j, i, APOfftoSurf(blkdatapt->bgoff))!=DD_OK)
					return -1;
			}
			mymappt++; j += blockgapx;
		}
		mymappt = mymap2pt;
		if ((blockstaggerx || blockstaggery)) {

		i += blockstaggery;
		j = (MMOX-(maphclip-blockstaggerx)-blockgapx);
		while (j<(MSCRW+MMOX)) {
			if (*mymappt>=0) blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)(*mymappt))*SOBSTR));
			else { myanpt = (ANISTR *) (mapanimstrendpt + *mymappt);
				blkdatapt = APAnimToBStr (myanpt); }
			if (FGBlit (lpDDSDest, j, i, APOfftoSurf(blkdatapt->bgoff))!=DD_OK) return -1;
			mymappt++; j += blockgapx;
		}

		i -= blockstaggery;
		mymap2pt += mapwidth;
		mymappt = mymap2pt;
		}
		i += blockgapy;
	}
	return 0;
}

int APMapDrawBGT (LPDIRECTDRAWSURFACE lpDDSDest, int mapxo, int mapyo)
/* lpDDSDest = lpdirectdrawsurface.
 * mapxo = offset, in pixels, from the left edge of the map.
 * mapyo = offset, in pixels, from the top edge of the map.
 */
{
int i, j, mapvclip, maphclip;
short int *mymappt;
short int *mymap2pt;
BLKSTR *blkdatapt;
ANISTR *myanpt;
/* first, draw blocks that are clipped horizontally (left) */
	if (blockstaggerx || blockstaggery) return (APMapDrawBG (lpDDSDest, mapxo, mapyo));

	maphclip = mapxo%blockgapx;	/* Number of pixels to clip from left */
	mapvclip = mapyo%blockgapy;	/* Number of pixels to clip from top */

	mymappt = (short int *) mappt;
	mymappt += (mapxo/blockgapx)+((mapyo/blockgapy)*mapwidth);

/* Draw blocks */
	i = MMOY-mapvclip; while (i<(MSCRH+MMOY)) {
		mymap2pt = mymappt;
		mymap2pt += mapwidth;
		j = MMOX-maphclip; while (j<(MSCRW+MMOX)) {
			if (*mymappt) {
			if (*mymappt>=0) blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)(*mymappt))*SOBSTR));
			else { myanpt = (ANISTR *) (mapanimstrendpt + *mymappt);
				blkdatapt = APAnimToBStr (myanpt); }
			if (blkdatapt->trigger) {
				if (FGBlit (lpDDSDest, j, i, APOfftoSurf(blkdatapt->bgoff))!=DD_OK) return -1;
			} else {
				if (BGBlit (lpDDSDest, j, i, APOfftoSurf(blkdatapt->bgoff))!=DD_OK) return -1;
			}
			}
			mymappt++; j += blockgapx;
		}
		mymappt = mymap2pt;
		i += blockgapy;
	}
	return 0;
}

int APMapDrawFG (LPDIRECTDRAWSURFACE lpDDSDest, int mapxo, int mapyo, int mapfg)
/* lpDDSDest = lpdirectdrawsurface.
 * mapxo = offset, in pixels, from the left edge of the map.
 * mapyo = offset, in pixels, from the top edge of the map.
 */
{
int i, j, mapvclip, maphclip, cplyr, cpysub, pexrows;
short int *mymappt;
short int *mymap2pt;
BLKSTR *blkdatapt;
ANISTR *myanpt;
LPDIRECTDRAWSURFACE mapgfxpt;

	mapxo -= blockstaggerx; mapyo -= blockstaggery;
	maphclip = mapxo%blockgapx;	/* Number of pixels to clip from left */
	mapvclip = mapyo%blockgapy;	/* Number of pixels to clip from top */

	mymappt = (short int *) mappt;
	if (blockstaggerx || blockstaggery) {
		mymappt += (mapxo/blockgapx)+((mapyo/blockgapy)*mapwidth*2);
	} else {
		mymappt += (mapxo/blockgapx)+((mapyo/blockgapy)*mapwidth);
	}

/* Draw blocks */
	i = (MMOY-mapvclip);
	if (blockstaggerx || blockstaggery) i -= blockgapy;
	if (isopillars) pexrows = (blockheight*10); else pexrows = 0;
	while (i<(MSCRH+MMOY+pexrows)) {
		if (mymappt >= (mappt + (mapwidth*mapheight))) return 0;
		mymap2pt = mymappt;
//		if (blockstaggerx || blockstaggery) mymap2pt += (mapwidth*2); else mymap2pt += mapwidth;
		mymap2pt += mapwidth;
		j = (MMOX-maphclip);
		if (blockstaggerx || blockstaggery) j -= blockgapx;
		while (j<(MSCRW+MMOX)) {
			if (*mymappt) {
			if (isopillars) {
				cplyr = 1; cpysub = 0;
				if (*mymappt>=0) blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)(*mymappt))*SOBSTR));
				else { myanpt = (ANISTR *) (mapanimstrendpt + *mymappt);
					blkdatapt = APAnimToBStr (myanpt); }
				do {
					if (!cplyr) blkdatapt++;
					for (;cplyr<4;cplyr++) {
						switch (cplyr) {
							case 0: mapgfxpt = APOfftoSurf(blkdatapt->bgoff); break;
							case 1: mapgfxpt = APOfftoSurf(blkdatapt->fgoff); break;
							case 2: mapgfxpt = APOfftoSurf(blkdatapt->fgoff2); break;
							default:
							case 3: mapgfxpt = APOfftoSurf(blkdatapt->fgoff3); break;
						}
						if (mapgfxpt != maplpDDSTiles[0]) {
				if (blkdatapt->unused2 && !blkdatapt->unused3) {
					if (FGBlitleft (lpDDSDest, j, i+cpysub, mapgfxpt) != DD_OK) return -1;
				} else {
					if (!blkdatapt->unused2 && blkdatapt->unused3) {
						if (FGBlitright (lpDDSDest, j, i+cpysub, mapgfxpt) != DD_OK) return -1;
					} else {
						if (FGBlit (lpDDSDest, j, i+cpysub, mapgfxpt) != DD_OK) return -1;
					}
				}
						}
						cpysub -= blockheight;
					}
					cplyr = 0;
				} while (blkdatapt->unused1);
			} else {
			if (*mymappt>=0) blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)(*mymappt))*SOBSTR));
			else { myanpt = (ANISTR *) (mapanimstrendpt + *mymappt);
				blkdatapt = APAnimToBStr (myanpt); }
			if (!mapfg) mapgfxpt = APOfftoSurf(blkdatapt->fgoff);
			else if (mapfg == 1) mapgfxpt = APOfftoSurf(blkdatapt->fgoff2);
			else mapgfxpt = APOfftoSurf(blkdatapt->fgoff3);
			if (mapgfxpt != maplpDDSTiles[0]) {
				if (FGBlit (lpDDSDest, j, i, mapgfxpt)!=DD_OK) return -1;
			} } }
			mymappt++; j += blockgapx;
		}
		mymappt = mymap2pt;
		if ((blockstaggerx || blockstaggery)) {
		i += blockstaggery;
		j = (MMOX-(maphclip-blockstaggerx)-blockgapx);
		while (j<(MSCRW+MMOX)) {
			if (*mymappt) {
			if (isopillars) {
				cplyr = 1; cpysub = 0;
				if (*mymappt>=0) blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)(*mymappt))*SOBSTR));
				else { myanpt = (ANISTR *) (mapanimstrendpt + *mymappt);
					blkdatapt = APAnimToBStr (myanpt); }
				do {
					if (!cplyr) blkdatapt++;
					for (;cplyr<4;cplyr++) {
						switch (cplyr) {
							case 0: mapgfxpt = APOfftoSurf(blkdatapt->bgoff); break;
							case 1: mapgfxpt = APOfftoSurf(blkdatapt->fgoff); break;
							case 2: mapgfxpt = APOfftoSurf(blkdatapt->fgoff2); break;
							default:
							case 3: mapgfxpt = APOfftoSurf(blkdatapt->fgoff3); break;
						}
						if (mapgfxpt != maplpDDSTiles[0]) {
				if (blkdatapt->unused2 && !blkdatapt->unused3) {
					if (FGBlitleft (lpDDSDest, j, i+cpysub, mapgfxpt) != DD_OK) return -1;
				} else {
					if (!blkdatapt->unused2 && blkdatapt->unused3) {
						if (FGBlitright (lpDDSDest, j, i+cpysub, mapgfxpt) != DD_OK) return -1;
					} else {
						if (FGBlit (lpDDSDest, j, i+cpysub, mapgfxpt) != DD_OK) return -1;
					}
				}
						}
						cpysub -= blockheight;
					}
					cplyr = 0;
				} while (blkdatapt->unused1);
			} else {
			if (*mymappt>=0) blkdatapt = (BLKSTR*) (((char *)blockstrpt) + (((int)(*mymappt))*SOBSTR));
			else { myanpt = (ANISTR *) (mapanimstrendpt + *mymappt);
				blkdatapt = APAnimToBStr (myanpt); }
			if (!mapfg) mapgfxpt = APOfftoSurf(blkdatapt->fgoff);
			else if (mapfg == 1) mapgfxpt = APOfftoSurf(blkdatapt->fgoff2);
			else mapgfxpt = APOfftoSurf(blkdatapt->fgoff3);
			if (mapgfxpt != maplpDDSTiles[0]) {
				if (FGBlit (lpDDSDest, j, i, mapgfxpt)!=DD_OK) return -1;
			} } }
			mymappt++; j += blockgapx;
		}
		i -= blockstaggery;
		mymap2pt += mapwidth;
		mymappt = mymap2pt;
		}
		i += blockgapy;
	}
	return 0;
}

/* End of MappyDX functions */

void GameInit (void)
{
	scrlxoff = xmapoffset*blockgapx; scrlyoff = ymapoffset*blockgapy;
	if (blockstaggerx || blockstaggery) scrlyoff /= 2;
	keyleft = 0; keyright = 0; keyup = 0; keydown = 0;
	framecount = 0; oldticks = GetTickCount(); tickdec = 99; tickfps = 0;
	apfpsoldticks = GetTickCount ();
	mapanimstrendpt = (animstrpt+ANDTMEM);
	onetilememory = blockwidth*blockheight*((blockdepth+1)/8);
	InitAnims ();
}

void GameLoop (void)
{
int numloops;
HDC myhdc;
char frmtxt[100];
short int * tempmappt;

	if (!gameactive) return;

	numloops=10000;
	while (1) {
		if (apfpsoldticks>=GetTickCount()) break;
		if (keyright) scrlxoff +=2;
		if (scrlxoff>(mapwidth*blockgapx-apwidth)) scrlxoff = mapwidth*blockgapx-apwidth;

		if (keyleft) scrlxoff -=2;
		if (scrlxoff<0) scrlxoff = 0;

		if (keydown) scrlyoff += 2;
		if (scrlyoff>(mapheight*blockgapy-apheight)) scrlyoff = mapheight*blockgapy-apheight;

		if (keyup) scrlyoff -=2;
		if (scrlyoff<0) scrlyoff = 0;

		if (blockstaggerx || blockstaggery) {
			if (scrlxoff>((mapwidth-1)*blockgapx-apwidth)) scrlxoff = (mapwidth-1)*blockgapx-apwidth;
			if (scrlyoff>(((mapheight-1)/2)*blockgapy-apheight)) scrlyoff = ((mapheight-1)/2)*blockgapy-apheight;
		}

		framecount++;

		UpdateAnims ();
		if (!apfps) break;
		numloops--; if (!numloops) break;
		apfpsoldticks += (1000/apfps);
	}

	newticks = GetTickCount();
	if (oldticks>newticks) oldticks = 0;

	if (showstrlayers&16) {
		if (showstrlayers&32) {
			APDrawParallax (lpDDSBack, scrlxoff, scrlyoff);
		} else {
			tempmappt = mappt;
			if (parastr < 0 || parastr > 7) return;
			mappt = GetLayerpt (parastr);
			if (mappt == NULL) { mappt = tempmappt; return; }
			if (showstrlayers&1) { if (APMapDrawBG (lpDDSBack, scrlxoff/2, scrlyoff/2))
			{
				IDirectDrawSurface2_Restore (lpDDSPrimary);
				IDirectDrawSurface2_Restore (lpDDSBack);
				APMapRestore ();
				APMapDrawBG (lpDDSBack, scrlxoff/2, scrlyoff/2);
			} }
	if (showstrlayers&2) { if (APMapDrawFG (lpDDSBack, scrlxoff/2, scrlyoff/2, 0))
	{
		IDirectDrawSurface2_Restore (lpDDSPrimary);
		IDirectDrawSurface2_Restore (lpDDSBack);
		APMapRestore ();
		APMapDrawFG (lpDDSBack, scrlxoff/2, scrlyoff/2, 0);
	} }

	if (showstrlayers&4) { if (APMapDrawFG (lpDDSBack, scrlxoff/2, scrlyoff/2, 1))
	{
		IDirectDrawSurface2_Restore (lpDDSPrimary);
		IDirectDrawSurface2_Restore (lpDDSBack);
		APMapRestore ();
		APMapDrawFG (lpDDSBack, scrlxoff/2, scrlyoff/2, 1);
	} }

	if (showstrlayers&8) { if (APMapDrawFG (lpDDSBack, scrlxoff/2, scrlyoff/2, 2))
	{
		IDirectDrawSurface2_Restore (lpDDSPrimary);
		IDirectDrawSurface2_Restore (lpDDSBack);
		APMapRestore ();
		APMapDrawFG (lpDDSBack, scrlxoff/2, scrlyoff/2, 2);
	} }
			mappt = tempmappt;
		}
		if (APMapDrawBGT (lpDDSBack, scrlxoff, scrlyoff))
		{
			IDirectDrawSurface2_Restore (lpDDSPrimary);
			IDirectDrawSurface2_Restore (lpDDSBack);
			APMapRestore ();
			APDrawParallax (lpDDSBack, scrlxoff, scrlyoff);
			APMapDrawBGT (lpDDSBack, scrlxoff, scrlyoff);
		} } else { if (showstrlayers&1) { if (APMapDrawBG (lpDDSBack, scrlxoff, scrlyoff))
		{
			IDirectDrawSurface2_Restore (lpDDSPrimary);
			IDirectDrawSurface2_Restore (lpDDSBack);
			APMapRestore ();
			APMapDrawBG (lpDDSBack, scrlxoff, scrlyoff);
	} } }

	if (showstrlayers&2) { if (APMapDrawFG (lpDDSBack, scrlxoff, scrlyoff, 0))
	{
		IDirectDrawSurface2_Restore (lpDDSPrimary);
		IDirectDrawSurface2_Restore (lpDDSBack);
		APMapRestore ();
		APMapDrawFG (lpDDSBack, scrlxoff, scrlyoff, 0);
	} }

	if (showstrlayers&4) { if (APMapDrawFG (lpDDSBack, scrlxoff, scrlyoff, 1))
	{
		IDirectDrawSurface2_Restore (lpDDSPrimary);
		IDirectDrawSurface2_Restore (lpDDSBack);
		APMapRestore ();
		APMapDrawFG (lpDDSBack, scrlxoff, scrlyoff, 1);
	} }

	if (showstrlayers&8) { if (APMapDrawFG (lpDDSBack, scrlxoff, scrlyoff, 2))
	{
		IDirectDrawSurface2_Restore (lpDDSPrimary);
		IDirectDrawSurface2_Restore (lpDDSBack);
		APMapRestore ();
		APMapDrawFG (lpDDSBack, scrlxoff, scrlyoff, 2);
	} }

	if (!apfps) numloops = tickfps; else numloops = apfps;
	sprintf (frmtxt, "X = %d, Y = %d. Approx FPS = %d, Logic = %d", scrlxoff, scrlyoff, tickfps, numloops);
	if (tickdec<0) { 	if (oldticks!=newticks) tickfps = 100000/(newticks-oldticks);
		oldticks = newticks; tickdec = 99; }
	tickdec--;

	if (IDirectDrawSurface2_GetDC (lpDDSBack, &myhdc) == DD_OK)
	{
		SetBkColor( myhdc, RGB( 50, 10, 50 ) );
		SetTextColor( myhdc, RGB( 255, 255, 0 ) );
		TextOut( myhdc, 0, 0, frmtxt, strlen (frmtxt));
		TextOut( myhdc, 0, apheight-16, "Escape to exit, cursors to scroll", 33);
		IDirectDrawSurface2_ReleaseDC(lpDDSBack, myhdc);
	}
	IDirectDrawSurface2_Flip (lpDDSPrimary, NULL, DDFLIP_WAIT);
}

void StopDD (void)
{
	FreeTileSurfaces ();
	if (lpddpal)
	{
		IDirectDrawPalette_Release (lpddpal);
		lpddpal = NULL;
	}
	if (lpDDSBack)
	{
		IDirectDrawSurface2_Release (lpDDSBack);
		lpDDSBack = NULL;
	}
	if (lpDDSPrimary)
	{
		IDirectDrawSurface2_Release (lpDDSPrimary);
		lpDDSPrimary = NULL;
	}
	if (lpDD)
	{
		IDirectDraw2_Release (lpDD);
		lpDD = NULL;
	}
}

int StartDD (HWND hwnd)
{
	if (DirectDrawCreate (NULL,&lpDD1,NULL) != DD_OK)
	{ StopDD (); return -1; }

	if (IDirectDraw_SetCooperativeLevel (lpDD1, hwnd, DDSCL_ALLOWREBOOT | DDSCL_EXCLUSIVE |
	DDSCL_FULLSCREEN | DDSCL_ALLOWMODEX) != DD_OK)
	{ StopDD (); return -1; }

	if (IDirectDraw_QueryInterface(lpDD1, &IID_IDirectDraw2, (VOID *)&lpDD)!=DD_OK) return -1;

	IDirectDraw_Release (lpDD1);
	lpDD1 = NULL;

	ddsd.dwSize = sizeof (ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 1;

	if (IDirectDraw2_SetDisplayMode(lpDD, apwidth, apheight, apdepth, aprate, 0) != DD_OK)
		{ StopDD (); return -1; }

	ddrval = IDirectDraw2_CreateSurface (lpDD, &ddsd, &lpDDSPrimary, NULL);
	if( ddrval != DD_OK )
	{ StopDD (); return -1; }

	ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
	ddrval = IDirectDrawSurface2_GetAttachedSurface(lpDDSPrimary, &ddscaps, &lpDDSBack);
	if( ddrval != DD_OK )
	{ StopDD (); return -1; }

	return 0;
}

LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
static HCURSOR oldcursor;
PAINTSTRUCT ps;

	switch (message) {
	case WM_CREATE:
		oldcursor = SetCursor (NULL);
		break;
	case WM_ACTIVATEAPP:
		gameactive = wParam;
		break;
	case WM_KEYUP:
		switch (wParam) {
		case VK_UP:
			keyup = 0;
			break;
		case VK_DOWN:
			keydown = 0;
			break;
		case VK_LEFT:
			keyleft = 0;
			break;
		case VK_RIGHT:
			keyright = 0;
			break;
		case VK_ESCAPE:
			gameactive = 0; gamerunning = 0;
         FreeTileSurfaces ();
			PostMessage(aphwnd, WM_CLOSE, 0, 0);
			break;
		}
		break;
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_UP:
			keyup = 1;
			break;
		case VK_DOWN:
			keydown = 1;
			break;
		case VK_LEFT:
			keyleft = 1;
			break;
		case VK_RIGHT:
			keyright = 1;
			break;
		}
		break;

	case WM_SETCURSOR:
		SetCursor(NULL);
		return TRUE;
	case WM_PAINT:
		BeginPaint (hwnd, &ps);
		EndPaint (hwnd, &ps);
		break;
	case WM_DESTROY:
		StopDD ();
		SetCursor (oldcursor);
		apendpreview = 1;
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case 1234:
				break;
		}
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

int APStart (void)
{
int i;
MSG msg;
char mymodedesc[80];
PALETTEENTRY thepal[256];

	if (gamerunning) return 0;
	strcpy (mymodedesc, apcurrentmode); if (mymodedesc[0]==0) return -1;
	i = 0; while (mymodedesc[i]!=0) { if (mymodedesc[i]=='*') { mymodedesc[i]=0; break; } i++; }
	i++; while (mymodedesc[i]!=0) { if (mymodedesc[i]==' ') { mymodedesc[i]=0; break; } i++; }
	i++; while (mymodedesc[i]!=0) { if (mymodedesc[i]=='b') { mymodedesc[i]=0; break; } i++; }
	i++; while (mymodedesc[i]!=0) { if (mymodedesc[i]=='h') { mymodedesc[i]=0; break; } i++; }
	apwidth = atoi (mymodedesc);
	i = strlen (mymodedesc); i++;
	apheight = atoi (mymodedesc+i);
	i += strlen (mymodedesc+i); i++;
	apdepth = atoi (mymodedesc+i);
	i += strlen (mymodedesc+i); i++;
	if (mymodedesc[i]=='?') aprate = 0;
	else aprate = atoi (mymodedesc+i);

	if (mapwidth*blockwidth<apwidth || mapheight*blockheight<apheight) {
		MessageBox (NULL, "Map area not big enough to show in this mode", szBigWindowname, MB_OK);
		return -1; }

	if (apdepth==8 && blockdepth!=8 && altgfxpt == NULL) {
		if (MessageBox (NULL, "Warning: Attempting to preview a high/true colour map on an 8bit screen "
			"can take an outrageous amount of setup time and won't look very good. I recommend changing to "
			"15bpp or higher in the Preview Setup. Click OK to preview in 8bit anyway, or cancel to abort",
			szBigWindowname, MB_ICONINFORMATION|MB_OKCANCEL)!=IDOK) return 0;
	}

	aphwnd = CreateWindowEx(WS_EX_TOPMOST, szBigWindowname, szBigWindowname, WS_POPUP,
		0, 0, apwidth, apheight,
		NULL, NULL, hInstance, NULL);

	if (aphwnd==NULL) return -1;
	ShowWindow (aphwnd, SW_SHOW);
	UpdateWindow (aphwnd);
	if (StartDD (aphwnd))
	{
		MessageBox (aphwnd, "Couldn't start DirectX, ending preview...", szBigWindowname, MB_OK);
		DestroyWindow (aphwnd);
		return -1;
	}
	if ((apdepth==8 && blockdepth==8) || (apdepth==8 && altgfxpt!=NULL)) {
		MapCMAPtoPE (cmappt, thepal);
		IDirectDraw2_CreatePalette(lpDD, DDPCAPS_8BIT | DDPCAPS_ALLOW256, thepal, &lpddpal, NULL);
		IDirectDrawSurface2_SetPalette(lpDDSPrimary, lpddpal);
		IDirectDrawSurface2_SetPalette(lpDDSBack, lpddpal);
	}
	maplpDDSTiles = malloc (sizeof(LPDIRECTDRAWSURFACE)*(numblockgfx+8));
	if (MapCreateTileSurfaces (lpDD)) { StopDD (); DestroyWindow (aphwnd); return -1; }
	GameInit (); gameactive = 1; gamerunning = 1;

	msg.message = 0; apendpreview = 0;
	while(msg.message != WM_QUIT && !apendpreview)
	{
		if (PeekMessage(&msg, NULL,0 ,0 ,PM_REMOVE ))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			GameLoop();
		}
	}

	return 0;
}

HRESULT WINAPI APEnumModesCallback (LPDDSURFACEDESC mylpDDSD, LPVOID lphwndDlg)
{
char mytext[80];

	if (mylpDDSD->dwFlags&DDSD_PIXELFORMAT)
		if (!mylpDDSD->ddpfPixelFormat.dwFlags&DDPF_RGB) return DDENUMRET_OK;
	if (mylpDDSD->dwFlags&DDSD_REFRESHRATE && mylpDDSD->dwRefreshRate)
		sprintf (mytext, "%ld*%ld %ldbpp %ldhz", mylpDDSD->dwWidth, mylpDDSD->dwHeight, mylpDDSD->ddpfPixelFormat.dwRGBBitCount,
			mylpDDSD->dwRefreshRate);
	else
		sprintf (mytext, "%ld*%ld %ldbpp ?hz", mylpDDSD->dwWidth, mylpDDSD->dwHeight, mylpDDSD->ddpfPixelFormat.dwRGBBitCount);
	SendDlgItemMessage ((HWND) lphwndDlg, IDC_APRVMODE, CB_ADDSTRING, 0,
		(LPARAM) ((LPSTR) mytext));
	return DDENUMRET_OK;
}

BOOL CALLBACK DIAAPSetupProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
int i;
char textbuf[80];

	switch (msg) {
		case WM_INITDIALOG:
			if (parastr<0) parastr = 0; if (parastr>=numblockstr) parastr = numblockstr-1;
			if (apfps<0) apfps = 0; if (apfps>500) apfps = 500;
			sprintf (textbuf, "%d", apfps);
			SetWindowText (GetDlgItem (hwndDlg, IDC_APRVLFPS), textbuf);
			sprintf (textbuf, "%d", parastr);
			SetWindowText (GetDlgItem (hwndDlg, IDC_APRVBSTR), textbuf);
			if (showstrlayers&1) CheckDlgButton (hwndDlg, IDC_APRVDRBG, 1);
			else CheckDlgButton (hwndDlg, IDC_APRVDRBG, 0);
			if (showstrlayers&2) CheckDlgButton (hwndDlg, IDC_APRVDRFG1, 1);
			else CheckDlgButton (hwndDlg, IDC_APRVDRFG1, 0);
			if (showstrlayers&4) CheckDlgButton (hwndDlg, IDC_APRVDRFG2, 1);
			else CheckDlgButton (hwndDlg, IDC_APRVDRFG2, 0);
			if (showstrlayers&8) CheckDlgButton (hwndDlg, IDC_APRVDRFG3, 1);
			else CheckDlgButton (hwndDlg, IDC_APRVDRFG3, 0);
			if (showstrlayers&16) CheckDlgButton (hwndDlg, IDC_APRVTRAN, 1);
			else CheckDlgButton (hwndDlg, IDC_APRVTRAN, 0);
			if (apparatype==0) {
				CheckDlgButton (hwndDlg, IDC_APRVGFP, 1);
				CheckDlgButton (hwndDlg, IDC_APRVLFP, 0);
			} else {
				CheckDlgButton (hwndDlg, IDC_APRVGFP, 0);
				CheckDlgButton (hwndDlg, IDC_APRVLFP, 1);
			}
			SendMessage (hwndDlg, WM_COMMAND, 99, 0L);
			break;
		case WM_COMMAND:
		switch (LOWORD(wParam))
		{
			case 99:
				SendDlgItemMessage (hwndDlg, IDC_APRVMODE, CB_RESETCONTENT, 0, 0);

				if (DirectDrawCreate (NULL,&lpDD1,NULL) != DD_OK) break;

				if (IDirectDraw_QueryInterface(lpDD1, &IID_IDirectDraw2, (LPVOID *)&lpDD)!=DD_OK)
					{ IDirectDraw_Release (lpDD1); break; }

				IDirectDraw_Release(lpDD1);
				lpDD1 = NULL;

				IDirectDraw2_EnumDisplayModes (lpDD, DDEDM_REFRESHRATES, NULL, (LPVOID) hwndDlg,
					(LPDDENUMMODESCALLBACK) APEnumModesCallback);

				IDirectDraw2_Release (lpDD);
				lpDD = NULL;

				SendDlgItemMessage(hwndDlg, IDC_APRVMODE, CB_SETITEMHEIGHT,
						 0, 14);
				SendDlgItemMessage (hwndDlg, IDC_APRVMODE, CB_SELECTSTRING, 0xFFFFFFFFL,
					(LPARAM) ((LPSTR) apcurrentmode));
				break;
			case IDOK:
				GetWindowText (GetDlgItem (hwndDlg, IDC_APRVLFPS), textbuf, 80);
				i = atoi (textbuf);
				if (i<0 || i>500) { MessageBox (hwndDlg, "Logic fps invalid, choose 0 for default or 1-500",
					"Anim Preview", MB_OK|MB_ICONEXCLAMATION);
					break;
				}
				apfps = i;

				GetWindowText (GetDlgItem (hwndDlg, IDC_APRVBSTR), textbuf, 80);
				parastr = atoi (textbuf);
				if (parastr<0) parastr = 0; if (parastr>=numblockgfx) parastr = numblockgfx-1;
//				GetWindowText (GetDlgItem (hwndDlg, IDC_APRVTTRU), textbuf, 80);
//				sscanf (textbuf, "%X", &aptranhi);
//				if (aptranhi<0) aptranhi = 0; if (aptranhi>0xFFFFFF) aptranhi = 0xFFFFFF;
//				GetWindowText (GetDlgItem (hwndDlg, IDC_APRVTIDX), textbuf, 80);
//				aptran8 = atoi (textbuf);
//				if (aptran8<0) aptran8 = 0; if (aptran8>255) aptran8 = 255;

				if (IsDlgButtonChecked (hwndDlg, IDC_APRVDRBG)) showstrlayers |= 1; else showstrlayers &= ~1;
				if (IsDlgButtonChecked (hwndDlg, IDC_APRVDRFG1)) showstrlayers |= 2; else showstrlayers &= ~2;
				if (IsDlgButtonChecked (hwndDlg, IDC_APRVDRFG2)) showstrlayers |= 4; else showstrlayers &= ~4;
				if (IsDlgButtonChecked (hwndDlg, IDC_APRVDRFG3)) showstrlayers |= 8; else showstrlayers &= ~8;
				if (IsDlgButtonChecked (hwndDlg, IDC_APRVTRAN)) showstrlayers |= 16; else showstrlayers &= ~16;
				if (IsDlgButtonChecked (hwndDlg, IDC_APRVGFP)) { showstrlayers |= 32; apparatype = 0; }
				else { showstrlayers &= ~32; apparatype = 1; }
				GetWindowText (GetDlgItem (hwndDlg, IDC_APRVMODE), apcurrentmode, 80);
				SendMessage (hwndDlg, WM_CLOSE, 0, 0L);
				break;
			case IDCANCEL:
				SendMessage (hwndDlg, WM_CLOSE, 0, -1L);
				break;
		}
		break;
		case WM_CLOSE:
			EndDialog (hwndDlg, lParam);
			break;
		default:
			return 0;
	}
	return -1;
}

void APSetup (HWND hwnd, int apthedepth)
{
DLGPROC dlgprc;

	dlgprc = (DLGPROC) MakeProcInstance (DIAAPSetupProc, hInstance);
	DialogBox (hInstance, (LPCSTR) "D_APSETUP", hwnd, dlgprc);
	FreeProcInstance((FARPROC) dlgprc);
}

void APCleanup (void)
{
	gamerunning = 0;
	gameactive = 0;
	StopDD ();
	if (aphwnd != NULL)
	{
		DestroyWindow (aphwnd);
		aphwnd = NULL;
	}
}

void APDisplayMode (char * modestr)
{
int i, j;

	i = 0; j = 0;
	if (modestr[i] != '\"') return; i++;
	while (modestr[i] != '\"') {
		apcurrentmode[j] = modestr[i];
		if (i>64) return;
		i++; j++;
	}
	apcurrentmode[j] = 0;
	showstrlayers = 3;
//	apwidth = 640; apheight = 480; apdepth = 8; aprate = 0;
	aptran8 = trans8bit;
	aptranhi = (((unsigned long int)transred)<<16)|(((unsigned long int) transgreen)<<8)|(transblue);
}

HWND CreateAnimPreviewWindow (HWND hwnd)
{
WNDCLASS wndclass;

	if (aphwnd != NULL) return aphwnd;

		  wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT;
		  wndclass.lpfnWndProc = PreviewWndProc;
		  wndclass.cbClsExtra = 0;
		  wndclass.cbWndExtra = 0;
		  wndclass.hInstance = hInstance;
		  wndclass.hIcon = LoadIcon(hInstance, "MAPWINIC");
		  wndclass.hCursor = LoadCursor(NULL, (LPCSTR) IDC_ARROW);
		  wndclass.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
		  wndclass.lpszMenuName = szBigWindowname;
		  wndclass.lpszClassName = szBigWindowname;
		  RegisterClass(&wndclass);

	gameactive = 0; gamerunning = 0; parastr = 0;
	apparatype = 0;

	return NULL;
}

