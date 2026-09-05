#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mapwin.h"

extern short int * mappt;
extern short int * mapmappt[MAXMAPLAYERS];
extern short int mapwidth, mapheight;
extern short int blockwidth, blockheight, blockdepth, sdepth;
extern short int numblockstr;
extern int clickmask;
extern char mapfilename[256];

static char mapformat[64] = "a10";
static char mcdefBMPname[512] = { 0 };

short int mcblkdefwidth = 16, mcblkdefheight = 16;
short int mcmapdefwidth = 120, mcmapdefheight = 72;
int mcblkstaggerx, mcblkstaggery, mcblkclickmask;
int csvadjust = 0;

static void mc_replace_ext (char * name, const char * ext)
{
	size_t n = strlen (name);
	if (n >= 4) strcpy (name + n - 4, ext);
	else strcat (name, ext);
}

static int mc_fail_after_new (FILE * fpt)
{
	if (fpt) fclose (fpt);
	FreeAll ();
	return -1;
}

void MapCustSetDefBMPName (char * defname)
{
int i;

	while (*defname == '\"') { if (*defname < ' ') return; defname++; }
	i = 0; while (*defname != '\"') {
		if (*defname < ' ' || i > 510) return;
		mcdefBMPname[i] = *defname; i++;
		defname++;
	}
	mcdefBMPname[i] = 0;
}

void MapCustomSetFormat (char * fstring)
{
int i, j;

	i = 0; while (fstring[i] != '\"') { if (i>62) return; i++; }
	i++;
	j = 0; while (fstring[i] != '\"') {
		mapformat[j] = tolower (fstring[i]);
		i++; j++;
		if (j>62) { mapformat[j] = 0; return; }
	}
	mapformat[j] = 0;
	return;
}

int MapCustomLoad (char * loadname, HWND hwnd)
{
int i, j, k, l, layeridx;
short int newmw, newmh, newmd, newbw, newbh;
int modemsb;
unsigned char mapoutput[4];
FILE * fpt;
FILE * bmfpt;
short int * mymappt;
char * mcname;
long int arpos, first_arpos;
unsigned char * bmppt;
BITMAPFILEHEADER * bfh;
BITMAPINFOHEADER * bmihpt;
OPENFILENAME ofn;
char szFilter[64];
char szFile[512];
char szFileTitle[256];
char szInitDir[256];

		modemsb = 0;
		first_arpos = -1;
		memset (mapoutput, 0, sizeof (mapoutput));
		mcname = malloc (strlen (loadname)+8);
		if (mcname == NULL) return -1;
		strcpy (mcname, loadname);
		mc_replace_ext (mcname, ".MAP");
		fpt = fopen (mcname, "rb");
		if (fpt==NULL) { free (mcname); return -1; }

		if (mcmapdefwidth < 4 || mcmapdefwidth > 10000) newmw = 100; else newmw = mcmapdefwidth;
		if (mcmapdefheight < 4 || mcmapdefheight > 10000) newmh = 100; else newmh = mcmapdefheight;
		if (mcblkdefwidth < 8 || mcblkdefwidth > 128) newbw = 32; else newbw = mcblkdefwidth;
		if (mcblkdefheight < 8 || mcblkdefheight > 128) newbh = 32; else newbh = mcblkdefheight;

		strcpy (szFile, mcdefBMPname);
		bmfpt = fopen (mcdefBMPname, "rb");
		if (bmfpt == NULL) {
			mc_replace_ext (mcname, ".BMP");
			strcpy (szFile, mcname);
			bmfpt = fopen (mcname, "rb");
			if (bmfpt == NULL) {
			mc_replace_ext (mcname, ".PCX");
			strcpy (szFile, mcname);
			bmfpt = fopen (mcname, "rb");
			if (bmfpt == NULL) {
				szFile[0] = 0; szFileTitle[0] = 0; szInitDir[0] = 0;
				memset (szFilter, 0, sizeof (szFilter));
				strcpy (szFilter, "BMP/PCX files (*.BMP,*.PCX)|*.bmp;*.pcx");
				szFilter[27] = 0;
				memset(&ofn, 0, sizeof(OPENFILENAME));

				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = szFilter;
				ofn.nFilterIndex = 1;
				ofn.lpstrFile= szFile;
				ofn.nMaxFile = sizeof(szFile);
				ofn.lpstrFileTitle = szFileTitle;
				ofn.nMaxFileTitle = sizeof(szFileTitle);
				ofn.lpstrInitialDir = szInitDir;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

				if (GetOpenFileName(&ofn)) {
					bmfpt = fopen (szFile, "rb");
				}
			}
			}
		}
		free (mcname);
		if (bmfpt == NULL) { fclose (fpt); return -1; }

		fseek (bmfpt, 0, SEEK_END);
		arpos = ftell (bmfpt);
		fseek (bmfpt, 0, SEEK_SET);

		bmppt = malloc (arpos);
		if (bmppt == NULL) { fclose (bmfpt); fclose (fpt); return -1; }
		if (fread (bmppt, 1, arpos, bmfpt) != arpos) {
			free (bmppt); fclose (bmfpt); fclose (fpt); return -1;
		}
		fclose (bmfpt);

		if (szFile[strlen (szFile)-1] == 'p' || szFile[strlen (szFile)-1] == 'P') {
			bfh = (BITMAPFILEHEADER *) bmppt;
			if (bfh->bfType!=19778) {
					free (bmppt); fclose (fpt); return -1;
			} // 'BM' identifier 19778

			bmihpt = (BITMAPINFOHEADER *) (bmppt + sizeof (BITMAPFILEHEADER));
			newmd = (short int) bmihpt->biBitCount;
			free (bmppt);
		} else {
			newmd = bmppt[65] * 8;
			if (newmd != 8 && newmd != 24) newmd = 24;
			free (bmppt);
		}

		i = 0; while (mapformat[i] != 0) {
			switch (mapformat[i]) {
				case 'l':
					modemsb = 0;
					i++;
					break;
				case 'm':
					modemsb = 1;
					i++;
					break;
				case 'w':
					i++;
					if (mapformat[i] < '1' || mapformat[i] > '4') { fclose (fpt); return -1; }
					memset (mapoutput, 0, sizeof (mapoutput));
					if (modemsb == 1) {
						if (fread (&mapoutput[4-(mapformat[i]-'0')],
								1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
							fclose (fpt); return -1;
						}
						newmw = (short int) mapoutput[3];
						newmw |= (((short int) mapoutput[2])<<8);
					} else {
						if (fread (mapoutput, 1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
							fclose (fpt); return -1;
						}
						newmw = (short int) mapoutput[0];
						newmw |= (((short int) mapoutput[1])<<8);
					}
					i++;
					break;
				case 'h':
					i++;
					if (mapformat[i] < '1' || mapformat[i] > '4') { fclose (fpt); return -1; }
					memset (mapoutput, 0, sizeof (mapoutput));
					if (modemsb == 1) {
						if (fread (&mapoutput[4-(mapformat[i]-'0')],
								1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
							fclose (fpt); return -1;
						}
						newmh = (short int) mapoutput[3];
						newmh |= (((short int) mapoutput[2])<<8);
					} else {
						if (fread (mapoutput, 1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
							fclose (fpt); return -1;
						}
						newmh = (short int) mapoutput[0];
						newmh |= (((short int) mapoutput[1])<<8);
					}
					i++;
					break;
				case 'a':
					if (first_arpos < 0) first_arpos = ftell (fpt);
					{
						int nbytes = 1;
						if (mapformat[i+1] >= '1' && mapformat[i+1] <= '4')
							nbytes = mapformat[i+1] - '0';
						fseek (fpt, (long)newmh * (long)newmw * nbytes, SEEK_CUR);
						if (mapformat[i+2] == '-') i += 4; else i += 3;
					}
					break;
				default:
					i++;
					break;
			}
		}

		mcname = malloc (strlen (mapfilename)+8);
		if (mcname == NULL) { fclose (fpt); return -1; }
		strcpy (mcname, mapfilename);
		if (CreateNewMap (newmw, newmh, newmd, newbw, newbh, newbw, newbh, mcblkstaggerx, mcblkstaggery, 0)) { fclose (fpt); return -1; }
		clickmask = mcblkclickmask;
		strcpy (mapfilename, mcname);
		free (mcname);

		IMImportBlocks (szFile, -1);

		if (first_arpos >= 0) fseek (fpt, first_arpos, SEEK_SET);
		else fseek (fpt, arpos, SEEK_SET);
		layeridx = 0;
		i = 0; while (mapformat[i] != 0) {
				if (mapformat[i] == 'a') {
					i++;
					if (layeridx >= MAXMAPLAYERS) return mc_fail_after_new (fpt);
					if (layeridx == 0) {
						mymappt = mappt;
					} else {
						if (mapmappt[layeridx] == NULL) {
							mapmappt[layeridx] = (short int *) malloc ((size_t)mapwidth*(size_t)mapheight*2);
							if (mapmappt[layeridx] == NULL) return mc_fail_after_new (fpt);
							memset (mapmappt[layeridx], 0, (size_t)mapwidth*(size_t)mapheight*2);
						}
						mymappt = mapmappt[layeridx];
					}
					j = ((int) mapwidth)*((int) mapheight);
					if (mapformat[i+1] == '-') {
						l = (int) (mapformat[i+2]-'0');
					} else {
						l = (int) (mapformat[i+1]-'0');
						l = -l;
					}
					mapoutput[3] = 0; mapoutput[2] = 0; mapoutput[1] = 0; mapoutput[0] = 0;
					if (modemsb == 1) {
						while (j) {
							if (mapformat[i] < '1' || mapformat[i] > '4')
								return mc_fail_after_new (fpt);
							if (fread (&mapoutput[4-(mapformat[i]-'0')],
								1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
								return mc_fail_after_new (fpt);
							}
							k = ((int) mapoutput[3]);
							k |= ((((int) mapoutput[2]) & 0xFF)<<8);
							k |= ((((int) mapoutput[1]) & 0xFF)<<16);
							k |= ((((int) mapoutput[0]) & 0xFF)<<24);
							k += l;
							if (k < 0) k = 0;
							if (k >= ((int) numblockstr)) k = ((int) numblockstr-1);
							*mymappt = (short int) k;
							mymappt++;
							j--;
						}
					} else {
						while (j) {
							if (mapformat[i] < '1' || mapformat[i] > '4')
								return mc_fail_after_new (fpt);
							if (fread (&mapoutput[0],
								1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
								return mc_fail_after_new (fpt);
							}
							k = ((int) mapoutput[0]);
							k |= ((((int) mapoutput[1]) & 0xFF)<<8);
							k |= ((((int) mapoutput[2]) & 0xFF)<<16);
							k |= ((((int) mapoutput[3]) & 0xFF)<<24);
							k += l;
							if (k < 0) k = 0;
							if (k >= ((int) numblockstr)) k = ((int) numblockstr-1);
							*mymappt = (short int) k;
							mymappt++;
							j--;
						}
					}
					if (mapformat[i+1] == '-') i += 3; else i += 2;
					layeridx++;
					continue;
				}
				i++;
		}
		fclose (fpt);

		return 0;
}

int MapCustomSave (char * savename)
{
int i, j, k, l, layeridx;
int modemsb;
char mapoutput[4];
FILE * fpt;
short int * mymappt;
char * mcname;

		modemsb = 0;
		mcname = malloc (strlen (savename)+8);
		if (mcname == NULL) return -1;
		strcpy (mcname, savename);
		mc_replace_ext (mcname, ".MAP");
		fpt = fopen (mcname, "wb");
		free (mcname);
		if (fpt==NULL) return -1;
		layeridx = 0;
		i = 0; while (mapformat[i] != 0) {
			switch (mapformat[i]) {
				case 'l':
					modemsb = 0;
					i++;
					break;
				case 'm':
					modemsb = 1;
					i++;
					break;
				case 'w':
					if (modemsb == 1) {
						mapoutput[3] = (unsigned char) (mapwidth & 0xFF);
						mapoutput[2] = (unsigned char) ((mapwidth>>8) & 0xFF);
						mapoutput[1] = 0;
						mapoutput[0] = 0;
						i++; if (mapformat[i] >= '1' && mapformat[i] <= '4') {
							if (fwrite (&mapoutput[4-(mapformat[i]-'0')],
									1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
								fclose (fpt); return -1;
							}
						}
					} else {
						mapoutput[0] = (unsigned char) (mapwidth & 0xFF);
						mapoutput[1] = (unsigned char) ((mapwidth>>8) & 0xFF);
						mapoutput[2] = 0;
						mapoutput[3] = 0;
						i++; if (mapformat[i] >= '1' && mapformat[i] <= '4') {
							if (fwrite (mapoutput, 1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
								fclose (fpt); return -1;
							}
						}
					}
					i++;
					break;
				case 'h':
					if (modemsb == 1) {
						mapoutput[3] = (unsigned char) (mapheight & 0xFF);
						mapoutput[2] = (unsigned char) ((mapheight>>8) & 0xFF);
						mapoutput[1] = (unsigned char) ((mapheight>>16) & 0xFF);
						mapoutput[0] = (unsigned char) ((mapheight>>24) & 0xFF);
						i++; if (mapformat[i] >= '1' && mapformat[i] <= '4') {
							if (fwrite (&mapoutput[4-(mapformat[i]-'0')],
									1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
								fclose (fpt); return -1;
							}
						}
					} else {
						mapoutput[0] = (unsigned char) (mapheight & 0xFF);
						mapoutput[1] = (unsigned char) ((mapheight>>8) & 0xFF);
						mapoutput[2] = (unsigned char) ((mapheight>>16) & 0xFF);
						mapoutput[3] = (unsigned char) ((mapheight>>24) & 0xFF);
						i++; if (mapformat[i] >= '1' && mapformat[i] <= '4') {
							if (fwrite (mapoutput, 1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
								fclose (fpt); return -1;
							}
						}
					}
					i++;
					break;
				case 'a':
					i++;
					if (layeridx < MAXMAPLAYERS && mapmappt[layeridx] != NULL)
						mymappt = mapmappt[layeridx];
					else
						mymappt = mappt;
					j = ((int) mapwidth)*((int) mapheight);
					if (mapformat[i+1] == '-') {
						l = (int) (mapformat[i+2]-'0');
						l = -l;
					} else {
						l = (int) (mapformat[i+1]-'0');
					}
					if (modemsb == 1) {
						while (j) {
							if (*mymappt < 0) k = 0;
							else k = (int) *mymappt;
							mymappt++;
//							k /= SOBSTR;
							k += l;
							if (k < 0) k = 0;
							mapoutput[3] = (unsigned char) (k & 0xFF);
							mapoutput[2] = (unsigned char) ((k>>8) & 0xFF);
							mapoutput[1] = (unsigned char) ((k>>16) & 0xFF);
							mapoutput[0] = (unsigned char) ((k>>24) & 0xFF);
							if (mapformat[i] >= '1' && mapformat[i] <= '4') {
								if (fwrite (&mapoutput[4-(mapformat[i]-'0')],
									1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
									fclose (fpt); return -1;
								}
							}
							j--;
						}
					} else {
						while (j) {
							if (*mymappt < 0) k = 0;
							else k = (int) *mymappt;
							mymappt++;
//							k /= SOBSTR;
							k += l;
							if (k < 0) k = 0;
							mapoutput[0] = (unsigned char) (k & 0xFF);
							mapoutput[1] = (unsigned char) ((k>>8) & 0xFF);
							mapoutput[2] = (unsigned char) ((k>>16) & 0xFF);
							mapoutput[3] = (unsigned char) ((k>>24) & 0xFF);
							if (mapformat[i] >= '1' && mapformat[i] <= '4') {
								if (fwrite (&mapoutput[0],
									1, mapformat[i]-'0', fpt) != mapformat[i]-'0') {
									fclose (fpt); return -1;
								}
							}
							j--;
						}
					}
					if (l < 0) i+=3; else i += 2;
					layeridx++;
					break;
				default:
					i++;
					break;
			}
		}
		fclose (fpt);
		return 0;
}
