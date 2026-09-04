/* mapwin.c */
/* Main Windows startup and message handler section */

#define  STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapwin.h"
#include "compat.h"
#include "fmp14.h"

HINSTANCE hInstance;
HWND hwnd;
char defpname[256];
static HMENU hMenu = NULL;
static char szAppname[] = "Mappy - Win32";
static char argvline[256];
static int curlayer = 0;

extern unsigned char transred, transgreen, transblue, trans8bit;
extern char * blockstrpt, *blockgfxpt;
extern char * altgfxpt;
extern short int * mappt;
extern char mapfilename[256];
extern int mapaltered, megetabrush, onion, oniondark, onionlower;
extern short int mapwidth, mapheight, blockwidth, blockheight, blockdepth, xmapoffset, ymapoffset;
extern int maptype, blockstaggerx, blockstaggery, isopillars, flippedtiles, isoreveal, clickmask, picklayer, picklx, pickly;
extern short int numblockstr, numblockgfx;
extern short int gridon, zoomon;
extern short int maplayer;
extern short int curbrsh, curanim, pastebrushnt;
extern short int *brshpt[MAXBRSH];

/* in mapcust.c */
extern short int mcblkdefwidth, mcblkdefheight;
extern short int mcmapdefwidth, mcmapdefheight;
extern int mcblkstaggerx, mcblkstaggery, mcblkclickmask;
extern char * mcdname;

/* various */
extern int imskip;
extern int csvadjust;
extern int wlayout;
extern int wnesmode, rotatedtiles;
extern int brushhandle;

/* Tier-1 1.4 settings */
int dobackups = 0;		/* save .BAK before overwrite */
int warnctrlx = 0;		/* prompt on Ctrl+X */
int thumbtrack = 0;		/* live scrollbar tracking */
int mwheelstyle = 1;		/* 0=off 1=scroll 2=arrows 3=both */
int mwheelblks = 1;		/* blocks per wheel click */
int graphicsmem = 0;		/* device caps override */
int simpleimport = 0;		/* default to simple import dialog */
int csvusebg = 0;		/* CSV export uses BG layer */
int gridstyle = 1;		/* 0=none 1=cursor 2=crosshatch */
int showuser = 0;		/* show user-set overlay */
char tbarname[64] = "";		/* custom titlebar name */
int divideron = 0;		/* divider grid overlay enable */
int dividergapx = 32;		/* divider pixel gap X */
int dividergapy = 32;		/* divider pixel gap Y */
long dividercol = 0xFFFFFF;	/* divider line colour RRGGBB */
int tbarinfo = 1;		/* map-window titlebar mode 0..6 */
int panelzoom = 1;		/* block window follows map zoom */
int zmstyle = 1;		/* 0=software 1=hardware zoom */
int zoomdiv = 1;		/* 1=x1 2=x0.5 4=x0.25 (zoomon forced 1) */
char folder[256] = ".";		/* file dialog start folder */
/* Custom mousebuttons (binary list: 0 None,1 Current block,2 Block 0,
 * 3 Drag map 1,4 Drag map 2,5 Placer.lua,6 Pick block,7 Lay path,
 * 8 Fill,9 Line,10 Area.lua; 100+ = CM_ command) */
int mb_l = 1, mb_m = 4, mb_r = 2, mb_x1 = 0, mb_x2 = 0, mb_x3 = 0, mb_x4 = 0;
/* Function/numeric/letter key mapping to CM_ codes (0 = unmapped) */
int keyF[10] = { 119, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int key1 = 123, key2 = 124, key3 = 125, keyi = 0, keyk = 0, keym = 0;
char textexportopts[64] = "110101010000100111000000"; /* text-export defaults */
char blocklabels[16][64] = {
	"User data:", "U1", "U2", "U3", "U4", "U5", "U6", "U7",
	"tl", "tr", "bl", "br", "bg transp", "flag 1", "flag 2", "flag 3"
};	/* block properties dialogue labels */
char objlabels[16][64] = {
	"Object data:", "U1", "U2", "U3", "U4", "U5", "U6", "U7",
	"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8"
};	/* object properties dialogue labels */
int objplace = 0;		/* object placement mode (map clicks place) */
int objshow = 1;		/* draw objects on map */
int sortobjects = 0;		/* draw objects sorted by Y */
int objlayeruser = 0;		/* user field holding object layer (0 = off) */

void InvalidateAll (void)
{
	InvalidateRgn (hwnd, NULL, FALSE);
}

static void DoBackup (void)
{
	char bakname[260];
	FILE *src, *dst;
	int n;
	char buf[4096];

	if (!dobackups || !mapfilename[0]) return;
	n = strlen(mapfilename);
	if (n+4 >= sizeof(bakname)) return;
	strcpy(bakname, mapfilename);
	strcat(bakname, ".BAK");
	src = fopen(mapfilename, "rb");
	if (!src) return;
	dst = fopen(bakname, "wb");
	if (!dst) { fclose(src); return; }
	while ((n = (int)fread(buf, 1, 4096, src)) > 0) fwrite(buf, 1, n, dst);
	fclose(dst);
	fclose(src);
}

void RejigBrshMenu (int brshnum)
{
int i;

	for (i=0;i<MAXBRSH;i++) {
		ModifyMenu (hMenu, CM_BRUSH1+i, MF_BYCOMMAND | MF_STRING, CM_BRUSH1+i, GetBrushName(i));
		CheckMenuItem (hMenu, CM_BRUSH1+i, MF_UNCHECKED);
	}

	if (brshnum>0 && brshnum<=MAXBRSH) {
		CheckMenuItem (hMenu, CM_BRUSH1+(brshnum-1), MF_CHECKED);
		BEInvalidate ();
	}
}

void RejigLayerMenu (int lyernum)
{
int li;
char mtxt[80];
const char * lname;

	if (mappt==NULL) return;
	if (ChangeLayer (lyernum)<0) {
		MessageBox (hwnd, "Can't change to this layer, doesn't exist", "Map Editor: Layers", MB_OK|MB_ICONEXCLAMATION);
		curlayer = 0;
		InvalidateAll();
		return;
	}
	CheckMenuItem (hMenu, CM_LAYER0, MF_UNCHECKED);
	CheckMenuItem (hMenu, CM_LAYER1, MF_UNCHECKED);
	CheckMenuItem (hMenu, CM_LAYER2, MF_UNCHECKED);
	CheckMenuItem (hMenu, CM_LAYER3, MF_UNCHECKED);
	CheckMenuItem (hMenu, CM_LAYER4, MF_UNCHECKED);
	CheckMenuItem (hMenu, CM_LAYER5, MF_UNCHECKED);
	CheckMenuItem (hMenu, CM_LAYER6, MF_UNCHECKED);
	CheckMenuItem (hMenu, CM_LAYER7, MF_UNCHECKED);

	/* grey layers that don't exist; annotate 0..7 with LSTR names */
	for (li=0;li<8;li++) {
		if (GetLayerpt (li)==NULL)
			EnableMenuItem (hMenu, CM_LAYER0+li, MF_BYCOMMAND|MF_GRAYED);
		else
			EnableMenuItem (hMenu, CM_LAYER0+li, MF_BYCOMMAND|MF_ENABLED);
		lname = F14_LayerName (li);
		if (lname!=NULL && lname[0]) {
			sprintf (mtxt, "Layer %d (%s)", li, lname);
			ModifyMenu (hMenu, CM_LAYER0+li, MF_BYCOMMAND|MF_STRING, CM_LAYER0+li, mtxt);
		}
	}

	switch (lyernum)
	{
		case 0:
			CheckMenuItem (hMenu, CM_LAYER0, MF_CHECKED);
			break;
		case 1:
			CheckMenuItem (hMenu, CM_LAYER1, MF_CHECKED);
			break;
		case 2:
			CheckMenuItem (hMenu, CM_LAYER2, MF_CHECKED);
			break;
		case 3:
			CheckMenuItem (hMenu, CM_LAYER3, MF_CHECKED);
			break;
		case 4:
			CheckMenuItem (hMenu, CM_LAYER4, MF_CHECKED);
			break;
		case 5:
			CheckMenuItem (hMenu, CM_LAYER5, MF_CHECKED);
			break;
		case 6:
			CheckMenuItem (hMenu, CM_LAYER6, MF_CHECKED);
			break;
		case 7:
			CheckMenuItem (hMenu, CM_LAYER7, MF_CHECKED);
			break;
	}
	MEDestroyUndoBuffs();
	InvalidateAll ();
}

void Rejig (void)
{
	if (mappt!=NULL) {
		MESetScrollRange (mapwidth-1, mapheight-1, xmapoffset, ymapoffset);
		BESetScrollRange ();
		RejigBrshMenu (curbrsh);
		RejigLayerMenu (curlayer);
	}
	MEUpdateScrollVis ();
	BEUpdateScrollVis ();
	if (tbarname[0]) {
		SetWindowText(hwnd, tbarname);
	} else {
		SetWindowText(hwnd, "Mappy - Win32");
	}
	InvalidateAll ();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
//char dropname[256];
//HDC myhdc;
//PAINTSTRUCT ps;
int i, oldnumblockgfx;
short int j;
HCURSOR hCurSave;
OPENFILENAME ofn;
char szFilter[128];
char szFile[256];
char szFileTitle[256];
char szInitDir[256];
FILE * marfpt;

	switch (message) {
	case WM_CREATE:
		if (hMenu == NULL) {
			hMenu = LoadMenu(hInstance, "MAPMENU");
			SetMenu(hwnd, hMenu);
			if (gridon) CheckMenuItem (hMenu, CM_TOOLGRID, MF_CHECKED);
			else CheckMenuItem (hMenu, CM_TOOLGRID, MF_UNCHECKED);
			CheckMenuItem (hMenu, CM_TOOLZOOM1, MF_UNCHECKED);
			CheckMenuItem (hMenu, CM_TOOLZOOM2, MF_UNCHECKED);
			CheckMenuItem (hMenu, CM_TOOLZOOM4, MF_UNCHECKED);
			if (zoomon==1) CheckMenuItem (hMenu, CM_TOOLZOOM1, MF_CHECKED);
			if (zoomon==2) CheckMenuItem (hMenu, CM_TOOLZOOM2, MF_CHECKED);
			if (zoomon==4) CheckMenuItem (hMenu, CM_TOOLZOOM4, MF_CHECKED);
			CheckMenuItem (hMenu, CM_BRUSHGRABNT, MF_UNCHECKED);
			if (objshow) CheckMenuItem (hMenu, CM_OBJSHOW, MF_CHECKED);
			else CheckMenuItem (hMenu, CM_OBJSHOW, MF_UNCHECKED);
			CheckMenuItem (hMenu, CM_OBJPLACE, MF_UNCHECKED);
		}
		SendMessage (hwnd, WM_COMMAND, 99, 0L);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case 99:
				if (argvline[0]!=0) {
					strcpy (mapfilename, argvline);
					LoadProject (); curlayer = 0; Rejig ();
					argvline[0]=0;
				}
				break;
			case CM_ARROWUP:
				if (mappt==NULL) break;
				if (ymapoffset>0) ymapoffset--;
				if ((blockstaggerx || blockstaggery) && ymapoffset&1) ymapoffset--;
				MESetScrollRange (mapwidth-1, mapheight-1, xmapoffset, ymapoffset);
				InvalidateAll ();
				break;
			case CM_ARROWDOWN:
				if (mappt==NULL) break;
				if (ymapoffset<(mapheight-2)) ymapoffset++;
				if ((blockstaggerx || blockstaggery) && ymapoffset&1) ymapoffset++;
				MESetScrollRange (mapwidth-1, mapheight-1, xmapoffset, ymapoffset);
				InvalidateAll ();
				break;
			case CM_ARROWLEFT:
				if (mappt==NULL) break;
				if (xmapoffset>0) xmapoffset--;
				MESetScrollRange (mapwidth-1, mapheight-1, xmapoffset, ymapoffset);
				InvalidateAll ();
				break;
			case CM_ARROWRIGHT:
				if (mappt==NULL) break;
				if (xmapoffset<(mapwidth-1)) xmapoffset++;
				MESetScrollRange (mapwidth-1, mapheight-1, xmapoffset, ymapoffset);
				InvalidateAll ();
				break;
			case CM_FILENEW:
				if (mapaltered) { if (MessageBox (hwnd, "Warning:\nCurrent map has been altered but not saved, make new map anyway?",
					"Map Editor", MB_OKCANCEL | MB_ICONQUESTION)==IDOK) { if (DIANewProject (hwnd)!=-1) { MEDestroyUndoBuffs(); curlayer = 0; onion = 0; oniondark = 0; mapaltered = 0; Rejig (); } } }
				else { if (DIANewProject (hwnd)!=-1) { MEDestroyUndoBuffs(); curlayer = 0; onion = 0; oniondark = 0; Rejig (); } }
				break;
			case CM_FILEOPEN:
				szFile[0] = 0; szFileTitle[0] = 0; szInitDir[0] = 0;
				memset (szFilter, 0, sizeof (szFilter));
				strcpy (szFilter, "Map files (*.FMP, *.MAP)|*.fmp;*.map");
				szFilter[24] = 0;
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
					if (mapaltered) { if (MessageBox (hwnd, "Warning:\nCurrent map has been altered but not saved, load anyway?",
						"Map Editor", MB_OKCANCEL | MB_ICONQUESTION)==IDCANCEL) break;
					}
					strcpy (mapfilename, szFile); MEDestroyUndoBuffs();
					if (!strcmp ((mapfilename+strlen(mapfilename))-4, ".map") ||
						!strcmp ((mapfilename+strlen(mapfilename))-4, ".MAP") ||
						!strcmp ((mapfilename+strlen(mapfilename))-4, ".Map")) {
						MapCustomLoad (mapfilename, hwnd);
					}
					else LoadProject ();
					curlayer = 0; onion = 0; oniondark = 0; Rejig ();
				}
				break;
			case CM_FILESAVE:
				if (mapfilename[0] && mappt!=NULL) {
				if (!strcmp ((mapfilename+strlen(mapfilename))-4, ".map") ||
					!strcmp ((mapfilename+strlen(mapfilename))-4, ".MAP") ||
					!strcmp ((mapfilename+strlen(mapfilename))-4, ".Map")) {
					DoBackup();
					if (MapCustomSave (mapfilename))
					MessageBox (hwnd, "Eeek, error saving map!", "Map Editor", MB_OK | MB_ICONEXCLAMATION);
					else mapaltered = 0;
				} else {
					if (numblockstr > 1024 && maptype < 1) {
						MessageBox (hwnd, "FMP0.5 only supports upto 1024 blocks, either reduce the number of blocks, or select a higher version in MapTools>MapProperties", "Map Editor", MB_OK | MB_ICONINFORMATION);
                  break;
					}
					DoBackup();
					if (SaveProject ()) {
						MessageBox (hwnd, "Eeek, error saving map, you should copy your .BAK file and rename it as a new map (like oldmap.fmp) NOW", "Map Editor", MB_OK | MB_ICONEXCLAMATION);
					}
					else mapaltered = 0;
				}
				Rejig(); break; }
/* Do not split SAVE/SAVEAS cases! Save runs into SaveAs if no filename */
			case CM_FILESAVEAS:
				if (mappt==NULL) { MessageBox (hwnd, "No, map to save!", "Map Editor", MB_OK | MB_ICONEXCLAMATION); break; }
				szFile[0] = 0; szFileTitle[0] = 0; szInitDir[0] = 0;
				memset (szFilter, 0, sizeof (szFilter));
				strcpy (szFilter, "Map files (*.FMP, *.MAP)|*.fmp;*.map");
				szFilter[24] = 0;
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
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

				if (GetSaveFileName(&ofn)) {
					strcpy (mapfilename, szFile);
					i = strlen (mapfilename); while (i) { if (mapfilename[i]=='.') break; i--; }
					if (!strcmp (mapfilename+i, ".MAP") || !strcmp (mapfilename+i, ".map") ||
						!strcmp (mapfilename+i, ".Map")) {
						if ((MessageBox (hwnd, ".MAP files are simple maps, only BG layer is preserved with no properties\n"
							"See 'Using Mappy as a simple .map editor' in the help file' for more information\n"
							"Go ahead and save as a .MAP file?\n", ".MAP format information",
							MB_ICONINFORMATION | MB_OKCANCEL)) == IDCANCEL) break;
						DoBackup();
						if (MapCustomSave (mapfilename))
							MessageBox (hwnd, "Eeek, error saving map!", "Map Editor", MB_OK | MB_ICONEXCLAMATION);
						else mapaltered = 0;
						break;
					}
					if (strcmp (mapfilename+i, ".FMP") && strcmp (mapfilename+i, ".fmp")) strcat (mapfilename, ".FMP");
					if (numblockstr > 1024 && maptype < 1) {
						MessageBox (hwnd, "FMP0.5 only supports upto 1024 blocks, either reduce the number of blocks, or select a higher version in MapTools>MapProperties", "Map Editor", MB_OK | MB_ICONINFORMATION);
						break;
					}
					DoBackup();
					if (SaveProject ()) MessageBox (hwnd, "Eeek, error saving map, you should copy your .BAK file and rename it as a new map (like oldmap.fmp) NOW", "Map Editor", MB_OK | MB_ICONEXCLAMATION);
					else mapaltered = 0;
				}
            Rejig();
				break;
			case CM_FILEIMPORT:
				if (mappt==NULL) break;
				szFile[0] = 0; szFileTitle[0] = 0; szInitDir[0] = 0;
				memset (szFilter, 0, sizeof (szFilter));
				strcpy (szFilter, "Image/map array files (*.BMP;*.PCX;*.PNG;*.MAR)|*.bmp;*.pcx;*.png;*.mar");
				szFilter[47] = 0;
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
					MEDestroyUndoBuffs();
					if (!strcmp ((szFile+strlen(szFile))-4, ".MAR") || !strcmp ((szFile+strlen(szFile))-4, ".mar"))
					{
						marfpt = fopen (szFile, "rb");
						if (marfpt==NULL) { MessageBox (hwnd, "Error importing map array, aborted", "Importer", MB_OK|MB_ICONEXCLAMATION); break; }
						fseek (marfpt, 0, SEEK_END);
						i = (int) ftell (marfpt);
						if (i!=(mapwidth*mapheight*2)) { fclose (marfpt); MessageBox (hwnd, "Error importing map array (wrong size!), aborted", "Importer", MB_OK|MB_ICONEXCLAMATION); break; }
						fseek (marfpt, 0, SEEK_SET);
						if (fread (mappt, 1, i, marfpt)!=i) { fclose (marfpt); MessageBox (hwnd, "Error while importing map array", "Importer", MB_OK|MB_ICONEXCLAMATION); break; }
						fclose (marfpt);
						j = 0; for (i=0;i<(mapwidth*mapheight);i++) { if (mappt[i]&0xF) j = 1; }
						for (i=0;i<(mapwidth*mapheight);i++) { if (j) { if (mappt[i] < 0) mappt[i] *= 16; } else { if (mappt[i] > 0) mappt[i] /= 32; } }
					} else {
					if (altgfxpt != NULL) { free (altgfxpt); altgfxpt = NULL; }
					if (MessageBox (hwnd, "Make all imported graphics into NEW block structures?", "Importer", MB_YESNO|MB_ICONQUESTION)==IDYES) i=-1; else i=0;
					oldnumblockgfx = numblockgfx;
					if (i==0) if (MessageBox (hwnd, "Replace (update) existing graphics?", "Importer", MB_YESNO|MB_ICONQUESTION)==IDYES) numblockgfx = 1;
					hCurSave = SetCursor(LoadCursor(NULL, IDC_WAIT));
					if (IMImportBlocks (szFile, i)) {
						MessageBox (hwnd, "There was an error importing blocks, see the helpfile section on importing\n"
							"Likely causes:\nBMP file is compressed (needs MAPEXT.DLL)\nBMP file is not the correct colour depth\nPNG file and LIBPNG12/13.DLL missing", "Import", MB_OK|MB_ICONEXCLAMATION);
						numblockgfx = oldnumblockgfx;
					}
					if (numblockgfx < oldnumblockgfx && i == 0) {
						MessageBox (hwnd, "Warning: Number of block graphics has decreased, some block structures may have lost their graphics", "Import", MB_OK|MB_ICONEXCLAMATION);
                  CorrectBlockStructures ();
					}
					if (numblockgfx > oldnumblockgfx && i == 0) {
						if (MessageBox (hwnd, "Number of block graphics has increased, make new block structures for new graphics?",
							"Import", MB_ICONQUESTION|MB_YESNO)==IDYES) {
							while (oldnumblockgfx < numblockgfx) {
								memset (blockstrpt+(numblockstr*SOBSTR), 0, SOBSTR);
								*((long int *) (blockstrpt+(numblockstr*SOBSTR))) = oldnumblockgfx*blockwidth*blockheight*((blockdepth+1)/8);
								numblockstr++;
								oldnumblockgfx++;
							}
						}
					}
					if ((blockstaggerx || blockstaggery) && clickmask == 0 && numblockstr > 0) {
						MessageBox (hwnd, "Your clickmask has been set to '1' this can be changed in MapTools>Map Properties", "Import", MB_OK|MB_ICONINFORMATION);
						clickmask = 1;
					}
					SetCursor (hCurSave); Rejig ();
					}
				}
				mapaltered = 1;
				InvalidateAll ();
				break;
			case CM_FILEIMPORTAT:
				if (mappt==NULL) break;
				if (numblockgfx==1) { MessageBox (hwnd, "This is for importing a range of block graphics over existing graphics. Please use 'File: Import' menu option instead", "Import", MB_OK|MB_ICONINFORMATION); break; }
				i = DIAImportNumber (hwnd);
				if ((i>>16)<0 || (i>>16)>(numblockgfx)) { MessageBox (hwnd, "Invalid Range", "Import", MB_OK|MB_ICONEXCLAMATION); break; }
				szFile[0] = 0; szFileTitle[0] = 0; szInitDir[0] = 0;
				memset (szFilter, 0, sizeof (szFilter));
				strcpy (szFilter, "Image files (*.BMP;*.PCX)|*.bmp;*.pcx");
				szFilter[25] = 0;
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
					MEDestroyUndoBuffs();
					if (altgfxpt != NULL) { free (altgfxpt); altgfxpt = NULL; }
					hCurSave = SetCursor(LoadCursor(NULL, IDC_WAIT));
					j = numblockgfx;
					numblockgfx = (short int) (i>>16);
					if (IMImportBlocks (szFile, (i&0xFFFF))) MessageBox (hwnd, "There was an error importing blocks, see the helpfile section on importing\n"
						"Likely causes:\nBMP file is compressed\nBMP file is not the correct colour depth", "Import", MB_OK|MB_ICONEXCLAMATION);
					if (numblockgfx<j) numblockgfx = j;
					SetCursor (hCurSave); Rejig ();
				}
				mapaltered = 1;
				InvalidateAll ();
				break;
			case CM_FILEEXPORT:
				if (mappt==NULL || mapfilename[0]==0) {
					MessageBox (hwnd, "No map to export from, or map has not been named ('Save As')", "Exporter", MB_OK|MB_ICONEXCLAMATION);
					break;
				}
				if (!strcmp (mapfilename+strlen(mapfilename)-4, ".FMP") ||
					!strcmp (mapfilename+strlen(mapfilename)-4, ".fmp")) {
					if (DIAExport (hwnd, mapfilename))
						MessageBox (hwnd, "Some elements were not exported correctly!", "Exporter", MB_OK|MB_ICONEXCLAMATION);
					} else MessageBox (hwnd, "Can't export, map's name failed sanity check (doesn't end in .FMP, see above)", mapfilename, MB_OK|MB_ICONEXCLAMATION);
				break;
			case CM_OBJMGR:
				DIAObjectMgr (hwnd);
				InvalidateAll ();
				break;
			case CM_OBJPLACE:
				objplace = 1-objplace;
				if (objplace) CheckMenuItem (hMenu, CM_OBJPLACE, MF_CHECKED);
				else CheckMenuItem (hMenu, CM_OBJPLACE, MF_UNCHECKED);
				break;
			case CM_OBJSHOW:
				objshow = 1-objshow;
				if (objshow) CheckMenuItem (hMenu, CM_OBJSHOW, MF_CHECKED);
				else CheckMenuItem (hMenu, CM_OBJSHOW, MF_UNCHECKED);
				InvalidateAll ();
				break;
			case CM_GRAPHEDIT:
				DIAGraphEdit (hwnd);
				InvalidateAll ();
				break;
			case CM_FILEEXTTXT:
				if (mappt==NULL || mapfilename[0]==0) {
					MessageBox (hwnd, "No map to export from, or map has not been named ('Save As')", "Exporter", MB_OK|MB_ICONEXCLAMATION);
					break;
				}
				if (DIAExportText (hwnd, mapfilename))
					MessageBox (hwnd, "Text export failed!", "Exporter", MB_OK|MB_ICONEXCLAMATION);
				break;
			case CM_FILEEXIT:
				if (mapaltered)
				{ if (MessageBox (NULL, "Warning:\nCurrent map has been altered but not saved, quit anyway?",
					"Map Editor", MB_OKCANCEL | MB_ICONQUESTION)==IDOK) { MEDestroyUndoBuffs(); PostQuitMessage(0); }
				} else {
					MEDestroyUndoBuffs();
					PostQuitMessage(0);
				}
				break;
			case CM_EDITNEW:
				if (mappt==NULL) break;
				BENewBlock ();
				MEDestroyUndoBuffs();
				mapaltered = 1;
				break;
			case CM_EDITCUT:
				if (mappt==NULL) break;
				if (warnctrlx) {
					if (MessageBox(hwnd, "Really cut this block?", "Cut Block", MB_OKCANCEL|MB_ICONQUESTION)==IDCANCEL) break;
				}
				BECutBlock ();
				MEDestroyUndoBuffs();
				mapaltered = 1;
				InvalidateAll ();
				break;
			case CM_EDITCOPY:
				if (mappt==NULL) break;
				BECopyBlock ();
				break;
			case CM_EDITPASTE:
				if (mappt==NULL) break;
				BEPasteBlock ();
				MEDestroyUndoBuffs();
				mapaltered = 1;
				InvalidateAll ();
				break;
			case CM_EDITRESIZE:
				if (mappt==NULL) break;
				DIAResizeMap (hwnd);
				MEDestroyUndoBuffs();
				mapaltered = 1;
				Rejig ();
				break;
			case CM_EDITUNDO:
				if (mappt==NULL) break;
				MEUndo ();
				InvalidateAll ();
				break;
			case CM_TOOLFILL:
				if (mappt==NULL) break;
				MEFillMap (0);
				mapaltered = 1;
				InvalidateAll ();
				break;
			case CM_TOOLFILLRANDOM:
				if (mappt==NULL) break;
				MEFillMap (1);
				mapaltered = 1;
				InvalidateAll ();
				break;
			case CM_TOOLRANGE:
				if (blockstrpt==NULL) break;
				DIARangeEditBlocks (hwnd);
				mapaltered = 1;
				InvalidateAll ();
				break;
			case CM_TOOLREMOVEANIMS:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Really delete unused animations?\n(you will lose any brushes that exist)", "Delete Anims", MB_ICONQUESTION|MB_YESNO)==IDYES) {
					DestroyAllBrushes ();
					DestroyUnusedAnims ();
				}
				mapaltered = 1;
				Rejig ();
				break;
			case CM_TOOLREMOVEBLOCKS:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Really delete unused blocks?\n(you will lose any brushes that exist)", "Delete Blocks", MB_ICONQUESTION|MB_YESNO)==IDYES) {
					DestroyAllBrushes ();
					DestroyUnusedBlocks ();
				}
				mapaltered = 1;
				Rejig ();
				break;
			case CM_TOOLREMOVEGRAPHICS:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Really delete unused graphics?\n(you will lose any brushes that exist)", "Delete Anims", MB_ICONQUESTION|MB_YESNO)==IDYES) {
					DestroyAllBrushes ();
					DestroyUnusedGraphics ();
				}
				mapaltered = 1;
				Rejig ();
				break;
			case CM_TOOLRESIZEBLK:
				DIAResizeBlk (hwnd);
				Rejig ();
				InvalidateAll ();
				break;
			case CM_TOOLDUPGFX:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Remove duplicate graphics?\nThis may take a long time.", "Remove Duplicates", MB_ICONQUESTION|MB_YESNO)==IDYES) {
					RemoveDuplicateGraphics ();
					Rejig ();
				}
				InvalidateAll ();
				break;
			case CM_TOOLDUPBLK:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Remove duplicate blocks?\nThis may take a long time.", "Remove Duplicates", MB_ICONQUESTION|MB_YESNO)==IDYES) {
					RemoveDuplicateBlocks ();
					Rejig ();
				}
				InvalidateAll ();
				break;
			case CM_TOOLINFO:
				if (mappt!=NULL) DIAEditInfo (hwnd);
				InvalidateAll ();
				break;
			case CM_TOOLANIMSETUP:
//				MessageBox (hwnd, "Unable to start preview (not in Borland version)", "Anim Preview", MB_OK); break;
				i = blockdepth;
				if (i<8) i = 8;
				if (i==15) i = 16;
				APSetup (hwnd, i);
				break;
			case CM_TOOLANIMPREVIEW:
//				MessageBox (hwnd, "Unable to start preview (not in Borland version)", "Anim Preview", MB_OK); break;
//				if (blockstaggerx || blockstaggery) { MessageBox (hwnd, "Unable to start preview (not in BETA version for non-rect blocks)", "Anim Preview", MB_OK); break; }
				if (mappt!=NULL) { if (APStart ()) MessageBox (hwnd, "Unable to start preview (see helpfile)", "Anim Preview", MB_OK); }
				break;
			case CM_BRUSHGRAB:
				if (mappt==NULL) break;
				if (curbrsh < 1 || curbrsh > MAXBRSH) {
					MessageBox (hwnd, "Please select a brush first...", "Brush Grab", MB_ICONINFORMATION|MB_OK);
					break;
				}
				megetabrush = 1;
				break;
			case CM_BRUSHGRABNT:
				if (mappt==NULL) break;
				if (pastebrushnt) {
					CheckMenuItem (hMenu, CM_BRUSHGRABNT, MF_UNCHECKED);
					pastebrushnt = 0;
				} else {
					CheckMenuItem (hMenu, CM_BRUSHGRABNT, MF_CHECKED);
					pastebrushnt = 1;
				}
				break;
			case CM_BRUSHGRABBS:
				if (mappt==NULL) break;
				DIAGrabBrushBS (hwnd);
				break;
			case CM_BRUSHDESTROY:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Really delete all brushes?", "Delete Brushes", MB_ICONQUESTION|MB_YESNO)==IDYES) {
					DestroyAllBrushes ();
				}
				break;
			case CM_BRUSHRENAME:
				if (curbrsh >=1 && curbrsh <= MAXBRSH && brshpt[curbrsh-1] != NULL) {
					DIAGetBrushName (hwnd, GetBrushName(curbrsh-1));
					Rejig ();
				} else {
					MessageBox (hwnd, "Please select a grabbed brush first...", "Brush Rename", MB_ICONINFORMATION|MB_OK);
				}
				break;
			case CM_BRUSH1: curbrsh = 1; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH2: curbrsh = 2; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH3: curbrsh = 3; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH4: curbrsh = 4; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH5: curbrsh = 5; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH6: curbrsh = 6; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH7: curbrsh = 7; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH8: curbrsh = 8; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH9: curbrsh = 9; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH10: curbrsh = 10; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH11: curbrsh = 11; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH12: curbrsh = 12; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH13: curbrsh = 13; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH14: curbrsh = 14; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH15: curbrsh = 15; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSH16: curbrsh = 16; RejigBrshMenu (curbrsh);	break;
			case CM_BRUSHHANDLE:
				brushhandle = (brushhandle+1)&3;
				MessageBox (hwnd, brushhandle==0 ? "Brush handle: top-left" :
					brushhandle==1 ? "Brush handle: top-right" :
					brushhandle==2 ? "Brush handle: bottom-left" : "Brush handle: bottom-right",
					"Brush Handle", MB_OK|MB_ICONINFORMATION);
				break;
			case CM_LAYER0:
				curlayer = 0; RejigLayerMenu (curlayer);
				break;
			case CM_LAYER1:
				curlayer = 1; RejigLayerMenu (curlayer);
				break;
			case CM_LAYER2:
				curlayer = 2; RejigLayerMenu (curlayer);
				break;
			case CM_LAYER3:
				curlayer = 3; RejigLayerMenu (curlayer);
				break;
			case CM_LAYER4:
				curlayer = 4; RejigLayerMenu (curlayer);
				break;
			case CM_LAYER5:
				curlayer = 5; RejigLayerMenu (curlayer);
				break;
			case CM_LAYER6:
				curlayer = 6; RejigLayerMenu (curlayer);
				break;
			case CM_LAYER7:
				curlayer = 7; RejigLayerMenu (curlayer);
				break;
			case CM_LAYERADD:
				if (mappt==NULL) break;
				i = CreateMapLayer(); if (i==-1) {
					MessageBox (hwnd, "Can't create new layer (max 100 layers?)", "Map Editor: Layers", MB_OK|MB_ICONINFORMATION);
				} else {
					curlayer = i; RejigLayerMenu (curlayer); InvalidateAll();
				}
				break;
			case CM_LAYERDUP:
				if (mappt==NULL) break;
				i = DuplicateMapLayer(); if (i==-1) {
					MessageBox (hwnd, "Can't duplicate layer (max 100 layers or no source layer?)", "Map Editor: Layers", MB_OK|MB_ICONINFORMATION);
				} else {
					curlayer = i; RejigLayerMenu (curlayer); InvalidateAll();
				}
				break;
			case CM_LAYERGOTO:
				if (mappt==NULL) break;
				i = DIALayerGoto (hwnd);
				if (i>=0) { curlayer = i; RejigLayerMenu (curlayer); }
				break;
			case CM_LAYERRENAME:
				if (mappt==NULL) break;
				DIALayerRename (hwnd);
				RejigLayerMenu (curlayer); InvalidateAll ();
				break;
			case CM_LAYERDELETE:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Are you sure you want to delete current layer?",
					"Map Editor: Layers", MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) break;
				i = DeleteMapLayer(); if (i==-1) {
					MessageBox (hwnd, "Can't delete this layer (last layer?)", "Map Editor: Layers", MB_OK|MB_ICONINFORMATION);
				} else {
					curlayer = i; RejigLayerMenu (curlayer); InvalidateAll();
				}
				break;
			case CM_LAYERONION:
				if (mappt==NULL) break;
//				if (blockstaggerx || blockstaggery) { MessageBox (hwnd, "Unable to onionskin (only on non-staggered maps)", "Onionskin", MB_OK); break; }
				DIALayerOnion (hwnd);
				InvalidateAll ();
				break;
			case CM_LAYERCLEAR:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Are you sure you want to completely clear this layer (make all map array fields block 0)?",
					"Map Editor: Layers", MB_OKCANCEL|MB_ICONQUESTION)==IDOK) {
					memset (mappt, 0, mapwidth*mapheight*2);
					InvalidateAll();
				}
				break;
			case CM_LAYERSLIDE:
				if (mappt==NULL) break;
				DIALayerSlide (hwnd);
				InvalidateAll();
				break;
			case CM_TOOLHALVE:
				if (mappt==NULL) break;
				if (MessageBox (hwnd, "Halve block dimensions? (you will lose any brushes that exist)",
					"Useful Functions", MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) break;
				MEDestroyUndoBuffs();
				if (HalveBlockDims ())
					MessageBox (hwnd, "Can't halve (need even block dimensions >= 2 and graphics loaded).", "Useful Functions", MB_OK|MB_ICONINFORMATION);
				else Rejig ();
				break;
			case CM_TOOLCONV16:
				if (mappt==NULL) break;
				MEDestroyUndoBuffs();
				i = Convert16to8 ();
				if (i==-2)
					MessageBox (hwnd, "Can't convert: remove animations first (they reference block offsets).", "Useful Functions", MB_OK|MB_ICONINFORMATION);
				else if (i)
					MessageBox (hwnd, "Can't convert (need 16x16 blocks, fits in 30000 structures).", "Useful Functions", MB_OK|MB_ICONINFORMATION);
				else { curlayer = maplayer; Rejig (); }
				break;
			case CM_TOOLBIGPIC:
				if (mappt==NULL) break;
				szFile[0] = 0; szFileTitle[0] = 0; szInitDir[0] = 0;
				memset (szFilter, 0, sizeof (szFilter));
				strcpy (szFilter, "Images (*.BMP;*.PCX;*.PNG)|*.bmp;*.pcx;*.png");
				szFilter[26] = 0;
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
					MEDestroyUndoBuffs();
					hCurSave = SetCursor(LoadCursor(NULL, IDC_WAIT));
					i = BigPictureToLayer (szFile);
					SetCursor(hCurSave);
					if (i==-2)
						MessageBox (hwnd, "Image pixels must equal map pixels exactly (mapwidth*blockwidth x mapheight*blockheight).", "Useful Functions", MB_OK|MB_ICONINFORMATION);
					else if (i<0)
						MessageBox (hwnd, "Can't make layer (need rectangular map, graphics loaded, max 100 layers).", "Useful Functions", MB_OK|MB_ICONINFORMATION);
					else { curlayer = i; RejigLayerMenu (curlayer); InvalidateAll(); }
				}
				break;
			case CM_HELPCONTENTS:
				sprintf (szFile, "%sMAPWIN.HTM", defpname);
				ShellExecute(NULL, "open", szFile, NULL, NULL, SW_SHOW);
				break;
			case CM_HELPABOUT:
				strcpy (szFile, "Mappy for Win32, V1.3.11\n(C)2002 Robin Burrows <mappy@tilemap.co.uk>\n"
					"http://www.tilemap.co.uk\nor\n"
					"http://www.geocities.com/rburrowsgc/robmpy.htm\n\n"
					"Keys in Map Editor: L=line tool, P=lay-path, D/middle-button=drag map, ESC=cancels, E=edit block.\n"
					"Objects menu: manager (areas/labels), place mode (click places, ctrl+click duplicates, p picks).");
				MessageBox (hwnd, szFile, "MappyWin32: About", MB_OK|MB_ICONINFORMATION);
				break;
			case CM_TOOLGRID:
				gridon--;
				if (gridon < 0) gridon = 2;
				if (gridon) CheckMenuItem (hMenu, CM_TOOLGRID, MF_CHECKED);
				else CheckMenuItem (hMenu, CM_TOOLGRID, MF_UNCHECKED);
				InvalidateAll ();
				break;
			case CM_TOOLDIVIDERS:
				DIADividers (hwnd);
				InvalidateAll ();
				break;
			case CM_TOOLZOOM1:
				zoomon = 1; zoomdiv = 1;
				CheckMenuItem (hMenu, CM_TOOLZOOM1, MF_CHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM2, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM4, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM05, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM025, MF_UNCHECKED);
				Rejig ();
				break;
			case CM_TOOLZOOM2:
				zoomon = 2; zoomdiv = 1;
				CheckMenuItem (hMenu, CM_TOOLZOOM1, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM2, MF_CHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM4, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM05, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM025, MF_UNCHECKED);
				Rejig ();
				break;
			case CM_TOOLZOOM4:
				zoomon = 4; zoomdiv = 1;
				CheckMenuItem (hMenu, CM_TOOLZOOM1, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM2, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM4, MF_CHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM05, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM025, MF_UNCHECKED);
				Rejig ();
				break;
			case CM_TOOLZOOM05:
				zoomon = 1; zoomdiv = 2;
				CheckMenuItem (hMenu, CM_TOOLZOOM1, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM2, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM4, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM05, MF_CHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM025, MF_UNCHECKED);
				Rejig ();
				break;
			case CM_TOOLZOOM025:
				zoomon = 1; zoomdiv = 4;
				CheckMenuItem (hMenu, CM_TOOLZOOM1, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM2, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM4, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM05, MF_UNCHECKED);
				CheckMenuItem (hMenu, CM_TOOLZOOM025, MF_CHECKED);
				Rejig ();
				break;
			case CM_TOOLWNES:
				wnesmode = 1-wnesmode;
				if (wnesmode) CheckMenuItem (hMenu, CM_TOOLWNES, MF_CHECKED);
				else CheckMenuItem (hMenu, CM_TOOLWNES, MF_UNCHECKED);
				InvalidateAll ();
				break;
			case CM_TOOLROTATED:
				rotatedtiles = 1-rotatedtiles;
				if (rotatedtiles) CheckMenuItem (hMenu, CM_TOOLROTATED, MF_CHECKED);
				else CheckMenuItem (hMenu, CM_TOOLROTATED, MF_UNCHECKED);
				InvalidateAll ();
				break;
			case CM_TOOLPILLARS:
				isopillars = 1-isopillars;
				if (isopillars) CheckMenuItem (hMenu, CM_TOOLPILLARS, MF_CHECKED);
				else CheckMenuItem (hMenu, CM_TOOLPILLARS, MF_UNCHECKED);
				InvalidateAll ();
				break;
			case CM_TOOLREVEAL:
				isoreveal = 1-isoreveal;
				if (isoreveal) CheckMenuItem (hMenu, CM_TOOLREVEAL, MF_CHECKED);
				else CheckMenuItem (hMenu, CM_TOOLREVEAL, MF_UNCHECKED);
				InvalidateAll ();
				break;
			case CM_BLOCKNEXT:
				if (mappt==NULL) break;
				BEBlockNext ();
				break;
			case CM_BLOCKPREV:
				if (mappt==NULL) break;
				BEBlockPrev ();
				break;
			case CM_PICKLAYER:
				if (mappt==NULL) break;
				PickLayer ();
				InvalidateAll ();
				break;
			case CM_PICKBLOCK:
				if (mappt==NULL) break;
				MEPickBlock ();
				InvalidateAll ();
				break;
		}
		break;
	case WM_SIZE:
		MEResizeWind (LOWORD(lParam), HIWORD(lParam));
		BEResizeWind (LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_MOUSEWHEEL:
		/* mousewheel scrolls the window the mouse is over */
		{
			POINT wpt; HWND wchild;
			GetCursorPos (&wpt);
			wchild = WindowFromPoint (wpt);
			if (wchild!=NULL && wchild!=hwnd) {
				SendMessage (wchild, message, wParam, lParam);
				break;
			}
		}
		break;
	case WM_KEYDOWN:
		if (LOWORD(wParam)>=VK_F1 && LOWORD(wParam)<=VK_F10) {
			int fn = LOWORD(wParam)-VK_F1;
			if (keyF[fn]) { SendMessage (hwnd, WM_COMMAND, keyF[fn], 0); break; }
		}
		if (mappt==NULL) break;
		if (GetKeyState(VK_SHIFT)<0) {
			/* shift+arrows scroll ×10 */
			switch (LOWORD(wParam)) {
				case VK_UP:    SendMessage(hwnd, WM_COMMAND, CM_ARROWUP, 0);
				               { int k=9; while(k--&&ymapoffset>0){SendMessage(hwnd,WM_COMMAND,CM_ARROWUP,0);} } break;
				case VK_DOWN:  SendMessage(hwnd, WM_COMMAND, CM_ARROWDOWN, 0);
				               { int k=9; while(k--&&ymapoffset<(mapheight-2)){SendMessage(hwnd,WM_COMMAND,CM_ARROWDOWN,0);} } break;
				case VK_LEFT:  SendMessage(hwnd, WM_COMMAND, CM_ARROWLEFT, 0);
				               { int k=9; while(k--&&xmapoffset>0){SendMessage(hwnd,WM_COMMAND,CM_ARROWLEFT,0);} } break;
				case VK_RIGHT: SendMessage(hwnd, WM_COMMAND, CM_ARROWRIGHT, 0);
				               { int k=9; while(k--&&xmapoffset<(mapwidth-1)){SendMessage(hwnd,WM_COMMAND,CM_ARROWRIGHT,0);} } break;
			}
		}
		break;
	case WM_DROPFILES:
		if (mapaltered) {
			if (MessageBox(hwnd, "Current map has been altered but not saved, load anyway?",
				"Map Editor", MB_OKCANCEL|MB_ICONQUESTION)==IDCANCEL) break;
		}
		{
			char dropname[256];
			char * dext;
			char savename[256];
			DragQueryFile((HDROP)wParam, 0, dropname, 255);
			DragFinish((HDROP)wParam);
			if (!dropname[0]) break;
			dext = dropname+strlen(dropname)-4;
			if (strcmp (dext, ".fmp") && strcmp (dext, ".FMP") && strcmp (dext, ".Fmp") &&
				strcmp (dext, ".map") && strcmp (dext, ".MAP") && strcmp (dext, ".Map") &&
				strcmp (dext, ".bmp") && strcmp (dext, ".BMP") && strcmp (dext, ".Bmp") &&
				strcmp (dext, ".pcx") && strcmp (dext, ".PCX") && strcmp (dext, ".Pcx") &&
				strcmp (dext, ".png") && strcmp (dext, ".PNG") && strcmp (dext, ".Png") &&
				strcmp (dext, ".mar") && strcmp (dext, ".MAR") && strcmp (dext, ".Mar") &&
				strcmp (dext, ".lua") && strcmp (dext, ".LUA") && strcmp (dext, ".Lua")) {
				MessageBox (hwnd, "Unrecognised file type (need .FMP, .MAP, .BMP, .PCX, .PNG or .MAR).\nMap name not changed.", "Map Editor", MB_OK|MB_ICONINFORMATION);
				break;
			}
			MEDestroyUndoBuffs();
			strcpy(mapfilename, dropname);
			if (!strcmp((mapfilename+strlen(mapfilename))-4, ".map") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".MAP") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".Map")) {
				MapCustomLoad(mapfilename, hwnd);
			} else if (!strcmp((mapfilename+strlen(mapfilename))-4, ".bmp") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".BMP") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".Bmp") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".pcx") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".PCX") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".Pcx") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".png") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".PNG") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".Png")) {
				/* image drop: import graphics (needs a map loaded) */
				if (mappt==NULL) {
					MessageBox (hwnd, "Load or create a map first, then drop the image again to import it.", "Map Editor", MB_OK|MB_ICONINFORMATION);
					break;
				}
				strcpy (savename, mapfilename);
				if (IMImportBlocks (dropname, 0)) {
					MessageBox (hwnd, "There was an error importing blocks (PNG needs LIBPNG12/13.DLL).", "Import", MB_OK|MB_ICONEXCLAMATION);
				} else mapaltered = 1;
				strcpy (mapfilename, savename);
				InvalidateAll();
				break;
			} else if (!strcmp((mapfilename+strlen(mapfilename))-4, ".mar") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".MAR") ||
				!strcmp((mapfilename+strlen(mapfilename))-4, ".Mar")) {
				/* map-array drop: re-import into current layer */
				if (mappt==NULL) {
					MessageBox (hwnd, "Load or create a map first, then drop the .MAR again.", "Map Editor", MB_OK|MB_ICONINFORMATION);
					break;
				}
				strcpy (savename, mapfilename);
				marfpt = fopen (dropname, "rb");
				if (marfpt!=NULL) {
					fseek (marfpt, 0, SEEK_END);
					i = (int) ftell (marfpt);
					fseek (marfpt, 0, SEEK_SET);
					if (i==(mapwidth*mapheight*2) && fread (mappt, 1, i, marfpt)==i) mapaltered = 1;
					else MessageBox (hwnd, "Error importing map array (wrong size!).", "Importer", MB_OK|MB_ICONEXCLAMATION);
					fclose (marfpt);
				}
				strcpy (mapfilename, savename);
				InvalidateAll();
				break;
			} else {
				LoadProject();
			}
			curlayer = 0; onion = 0; oniondark = 0; Rejig();
			mapaltered = 0;
			InvalidateAll();
		}
		break;
	case WM_CLOSE:
		if (mapaltered)
			{ if (MessageBox (NULL, "Warning:\nCurrent map has been altered but not saved, quit anyway?",
				"Map Editor", MB_OKCANCEL | MB_ICONQUESTION)==IDOK) { MEDestroyUndoBuffs(); PostQuitMessage(0); }
			} else {
				MEDestroyUndoBuffs();
				PostQuitMessage(0);
			}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

int GetEquals (char * strpt)
{
int i;

	i = 0; while (strpt[i] != '=') i++; i++;
	while (strpt[i] == ' ') i++;
	return i;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
int i, defwidth, defheight, flength;
MSG msg;
WNDCLASS wndclass;
HACCEL haccel;
FILE * fpt;
char * iniinfo, * inipos;

//	MessageBox (NULL, "Warning:\nThis is VERY alpha status for testing only, expect a lot of bugs etc, use V1.22 for a stable version. Do not use on important maps",
//		"Map Editor", MB_OK | MB_ICONEXCLAMATION);

	hInstance = hInst;
	GetModuleFileName (hInstance, defpname, 254);
	i=strlen (defpname); while (i) { if (defpname[i]=='\\' || defpname[i]=='/') break; i--; }
	if (i>0) defpname[i+1] = 0;

 	argvline[0] = 0;
	if (lpszCmdParam!=NULL && (strlen(argvline)<256)) {
	if (lpszCmdParam[0] == '"') { strcpy (argvline, lpszCmdParam+1);
		argvline[strlen(argvline)-1] = 0; } else { strcpy (argvline, lpszCmdParam); } }

	defwidth = 640; defheight = 480; gridon = 0; zoomon = 1; picklayer = 1;
	picklx = 0; pickly = 0; csvadjust = -1; flippedtiles = 0; wlayout = 0;
	transred = 255; transgreen = 0; transblue = 255; trans8bit = 0;
	i = strlen (defpname); strcpy (defpname+i, "MAPWIN.INI");
	fpt = fopen (defpname, "rb");
	defpname[i] = 0;
	if (fpt != NULL) {
		iniinfo = malloc (4096);
		if (iniinfo!=NULL) {
		memset (iniinfo, 0, 4096);
		flength = fread (iniinfo, 1, 4096, fpt);

			inipos = iniinfo;
			while (inipos<(iniinfo+flength)) {
			if (inipos[0]!=';') {
				if (!strncmp (inipos, "width", 5)) { i = GetEquals(inipos); defwidth = atoi (inipos+i); }
				if (!strncmp (inipos, "height", 6)) { i = GetEquals(inipos); defheight = atoi (inipos+i); }
				if (!strncmp (inipos, "wlayout", 7)) { i = GetEquals(inipos); if (inipos[i]=='0') wlayout = 0; if (inipos[i]=='1') wlayout = 1; if (inipos[i]=='-') wlayout = -1; }
				if (!strncmp (inipos, "grid", 4)) { i = GetEquals(inipos); if (inipos[i]=='1') gridon = 1; if (inipos[i]=='2') gridon = 2; }
				if (!strncmp (inipos, "zoom", 4)) { i = GetEquals(inipos); if (inipos[i]=='1') { zoomon = 1; zoomdiv = 1; } }
				if (!strncmp (inipos, "zoom", 4)) { i = GetEquals(inipos); if (inipos[i]=='2') { zoomon = 2; zoomdiv = 1; } }
				if (!strncmp (inipos, "zoom", 4)) { i = GetEquals(inipos); if (inipos[i]=='4') { zoomon = 4; zoomdiv = 1; } }
				if (!strncmp (inipos, "zoom", 4)) { i = GetEquals(inipos); if (inipos[i]=='-' && inipos[i+1]=='2') { zoomon = 1; zoomdiv = 2; } }
				if (!strncmp (inipos, "zoom", 4)) { i = GetEquals(inipos); if (inipos[i]=='-' && inipos[i+1]=='4') { zoomon = 1; zoomdiv = 4; } }
				if (!strncmp (inipos, "zmstyle", 7)) { i = GetEquals(inipos); zmstyle = atoi (inipos+i); }
				if (!strncmp (inipos, "panelzoom", 9)) { i = GetEquals(inipos); panelzoom = atoi (inipos+i); }
				if (!strncmp (inipos, "tbarinfo", 8)) { i = GetEquals(inipos); tbarinfo = atoi (inipos+i); if (tbarinfo<0) tbarinfo = 0; if (tbarinfo>6) tbarinfo = 6; }
				if (!strncmp (inipos, "picklayer", 9)) { i = GetEquals(inipos); picklayer = atoi (inipos+i); }
				if (!strncmp (inipos, "flippedtiles", 12)) { i = GetEquals(inipos); flippedtiles = atoi (inipos+i); }
				if (!strncmp (inipos, "transred", 8)) { i = GetEquals(inipos); transred = atoi (inipos+i); }
				if (!strncmp (inipos, "transgreen", 10)) { i = GetEquals(inipos); transgreen = atoi (inipos+i); }
				if (!strncmp (inipos, "transblue", 9)) { i = GetEquals(inipos); transblue = atoi (inipos+i); }
				if (!strncmp (inipos, "trans8bit", 9)) { i = GetEquals(inipos); trans8bit = atoi (inipos+i); }
				if (!strncmp (inipos, "apmode", 6)) { i = GetEquals(inipos); APDisplayMode (inipos+i); }
				if (!strncmp (inipos, "importskip", 10)) { i = GetEquals(inipos); imskip = atoi (inipos+i); }
				if (!strncmp (inipos, "csvadjust", 9)) { i = GetEquals(inipos); csvadjust = atoi (inipos+i); }
				if (!strncmp (inipos, "maptype", 7)) { i = GetEquals(inipos); MapCustomSetFormat (inipos+i); }
				if (!strncmp (inipos, "mapdefw", 7)) { i = GetEquals(inipos); mcmapdefwidth = atoi (inipos+i); }
				if (!strncmp (inipos, "mapdefh", 7)) { i = GetEquals(inipos); mcmapdefheight = atoi (inipos+i); }
				if (!strncmp (inipos, "mapdefbw", 8)) { i = GetEquals(inipos); mcblkdefwidth = atoi (inipos+i); }
				if (!strncmp (inipos, "mapdefbh", 8)) { i = GetEquals(inipos); mcblkdefheight = atoi (inipos+i); }
				if (!strncmp (inipos, "mapdefBMP", 9)) { i = GetEquals(inipos); MapCustSetDefBMPName (inipos+i); }
				if (!strncmp (inipos, "mapstaggerx", 11)) { i = GetEquals(inipos); mcblkstaggerx = atoi (inipos+i); }
				if (!strncmp (inipos, "mapstaggery", 11)) { i = GetEquals(inipos); mcblkstaggery = atoi (inipos+i); }
				if (!strncmp (inipos, "mapclickmask", 12)) { i = GetEquals(inipos); mcblkclickmask = atoi (inipos+i); }
				if (!strncmp (inipos, "backups", 7)) { i = GetEquals(inipos); dobackups = atoi (inipos+i); }
				if (!strncmp (inipos, "warnctrlx", 9)) { i = GetEquals(inipos); warnctrlx = atoi (inipos+i); }
				if (!strncmp (inipos, "thumbtrack", 10)) { i = GetEquals(inipos); thumbtrack = atoi (inipos+i); }
				if (!strncmp (inipos, "mwheelstyle", 11)) { i = GetEquals(inipos); mwheelstyle = atoi (inipos+i); }
				if (!strncmp (inipos, "mwheelblks", 10)) { i = GetEquals(inipos); mwheelblks = atoi (inipos+i); }
				if (!strncmp (inipos, "graphicsmem", 11)) { i = GetEquals(inipos); graphicsmem = atoi (inipos+i); }
				if (!strncmp (inipos, "simpleimport", 12)) { i = GetEquals(inipos); simpleimport = atoi (inipos+i); }
				if (!strncmp (inipos, "csvusebg", 8)) { i = GetEquals(inipos); csvusebg = atoi (inipos+i); }
				if (!strncmp (inipos, "gridstyle", 9)) { i = GetEquals(inipos); gridstyle = atoi (inipos+i); if (gridstyle) gridon = gridstyle; }
				if (!strncmp (inipos, "showuser", 8)) { i = GetEquals(inipos); showuser = atoi (inipos+i); }
				if (!strncmp (inipos, "divideron", 9)) { i = GetEquals(inipos); divideron = atoi (inipos+i); }
				if (!strncmp (inipos, "dividergapx", 11)) { i = GetEquals(inipos); dividergapx = atoi (inipos+i); }
				if (!strncmp (inipos, "dividergapy", 11)) { i = GetEquals(inipos); dividergapy = atoi (inipos+i); }
				if (!strncmp (inipos, "dividercol", 10)) { i = GetEquals(inipos); dividercol = strtol (inipos+i, NULL, 16) & 0xFFFFFF; }
				if (!strncmp (inipos, "wnesmode", 8)) { i = GetEquals(inipos); wnesmode = atoi (inipos+i); }
				if (!strncmp (inipos, "rotatedtiles", 12)) { i = GetEquals(inipos); rotatedtiles = atoi (inipos+i); }
				if (!strncmp (inipos, "folder", 6)) { i = GetEquals(inipos); { int q = 0; while (inipos[i] && inipos[i]!='\r' && inipos[i]!='\n' && q<250) { folder[q++] = inipos[i]; i++; } folder[q]=0; } }
				if (!strncmp (inipos, "textexportopts", 14)) { i = GetEquals(inipos); { int q = 0; while (inipos[i] && inipos[i]!='\r' && inipos[i]!='\n' && q<60) { textexportopts[q++] = inipos[i]; i++; } textexportopts[q]=0; } }
				if (!strncmp (inipos, "blockdialoguelabels", 19)) { i = GetEquals(inipos); { int bl = 0, bc = 0; while (inipos[i] && inipos[i]!='\r' && inipos[i]!='\n' && bl<16) { if (inipos[i]==';') { blocklabels[bl][bc]=0; bl++; bc = 0; } else if (bc<63) { blocklabels[bl][bc++] = inipos[i]; } i++; } if (bl<16) blocklabels[bl][bc]=0; } }
				if (!strncmp (inipos, "lbutton", 7)) { i = GetEquals(inipos); mb_l = atoi (inipos+i); }
				if (!strncmp (inipos, "mbutton", 7)) { i = GetEquals(inipos); mb_m = atoi (inipos+i); }
				if (!strncmp (inipos, "rbutton", 7)) { i = GetEquals(inipos); mb_r = atoi (inipos+i); }
				if (!strncmp (inipos, "xbutton1", 8)) { i = GetEquals(inipos); mb_x1 = atoi (inipos+i); }
				if (!strncmp (inipos, "xbutton2", 8)) { i = GetEquals(inipos); mb_x2 = atoi (inipos+i); }
				if (!strncmp (inipos, "xbutton3", 8)) { i = GetEquals(inipos); mb_x3 = atoi (inipos+i); }
				if (!strncmp (inipos, "xbutton4", 8)) { i = GetEquals(inipos); mb_x4 = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF10", 6)) { i = GetEquals(inipos); keyF[9] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF1=", 6)) { i = GetEquals(inipos); keyF[0] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF2=", 6)) { i = GetEquals(inipos); keyF[1] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF3=", 6)) { i = GetEquals(inipos); keyF[2] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF4=", 6)) { i = GetEquals(inipos); keyF[3] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF5=", 6)) { i = GetEquals(inipos); keyF[4] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF6=", 6)) { i = GetEquals(inipos); keyF[5] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF7=", 6)) { i = GetEquals(inipos); keyF[6] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF8=", 6)) { i = GetEquals(inipos); keyF[7] = atoi (inipos+i); }
				if (!strncmp (inipos, "keyF9=", 6)) { i = GetEquals(inipos); keyF[8] = atoi (inipos+i); }
				if (!strncmp (inipos, "key1", 4)) { i = GetEquals(inipos); key1 = atoi (inipos+i); }
				if (!strncmp (inipos, "key2", 4)) { i = GetEquals(inipos); key2 = atoi (inipos+i); }
				if (!strncmp (inipos, "key3", 4)) { i = GetEquals(inipos); key3 = atoi (inipos+i); }
				if (!strncmp (inipos, "keyi", 4)) { i = GetEquals(inipos); keyi = atoi (inipos+i); }
				if (!strncmp (inipos, "keyk", 4)) { i = GetEquals(inipos); keyk = atoi (inipos+i); }
				if (!strncmp (inipos, "keym", 4)) { i = GetEquals(inipos); keym = atoi (inipos+i); }
				if (!strncmp (inipos, "objectdialoguelabels", 20)) { i = GetEquals(inipos); { int bl = 0, bc = 0; while (inipos[i] && inipos[i]!='\r' && inipos[i]!='\n' && bl<16) { if (inipos[i]==';') { objlabels[bl][bc]=0; bl++; bc = 0; } else if (bc<63) { objlabels[bl][bc++] = inipos[i]; } i++; } if (bl<16) objlabels[bl][bc]=0; } }
				if (!strncmp (inipos, "sortobjects", 11)) { i = GetEquals(inipos); sortobjects = atoi (inipos+i); }
				if (!strncmp (inipos, "objlayeruser", 12)) { i = GetEquals(inipos); objlayeruser = atoi (inipos+i); }
				if (!strncmp (inipos, "tbarname", 8)) { i = GetEquals(inipos); { int q = 0; if (inipos[i]=='"') i++; while (inipos[i] && inipos[i]!='"' && q<62) { tbarname[q++] = inipos[i]; i++; } tbarname[q]=0; } }
			}
			while (inipos<(iniinfo+flength)) { if (*inipos==0x0a) break; inipos++; } inipos++;
		}
		free (iniinfo);
		}
		fclose (fpt);
	}

	FreeAll ();

	if (!hPrevInstance) {
		memset (&wndclass, 0, sizeof(WNDCLASS));
		wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT;
		wndclass.lpfnWndProc = WndProc;
		wndclass.hInstance = hInstance;
		wndclass.hIcon = LoadIcon(hInstance, "MAPWINIC");
		wndclass.hCursor = LoadCursor(NULL, (LPCSTR) IDC_ARROW);
		wndclass.hbrBackground = GetStockObject (GRAY_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = szAppname;
		RegisterClass(&wndclass);
	} else exit (-1);

	haccel = LoadAccelerators (hInstance, "MAPACCEL");

	hwnd = CreateWindow  (szAppname, szAppname,
								WS_OVERLAPPEDWINDOW,
								(GetSystemMetrics (SM_CXSCREEN))/2-(defwidth/2),
								(GetSystemMetrics (SM_CYSCREEN))/2-(defheight/2),
								defwidth, defheight,
								NULL, NULL, hInstance, NULL);

	ShowWindow (hwnd, nCmdShow);
	UpdateWindow (hwnd);

	CreateMapEditorWindow (hwnd);
	CreateBlockEditorWindow (hwnd);
	CreateAnimPreviewWindow (hwnd);
	Rejig ();

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (TranslateAccelerator( hwnd, haccel, (LPMSG)&msg ) == 0) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	APCleanup ();
	FreeAll ();
	return (int)msg.wParam;
}
