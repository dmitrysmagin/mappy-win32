#include "mapdefs.h"

/* mapfunc.c prototypes */
void FreeAll (void);
int CreateNewMap (int, int, int, int, int, int, int, int, int, int);
void LoadProject (void);
int SaveProject (void);
short int * GetLayerpt (int);
int PickLayer (void);
int ChangeLayer (int);
int DeleteMapLayer (void);
int CreateMapLayer (void);
int DuplicateMapLayer (void);
void Onionto24 (int, int, unsigned char *, int, int, int, int);
void Overlayto24 (int, int, unsigned char *, int, int, int, int);
void Blockto24 (int, int, unsigned char *, int, int, int);
void BlockSecto24 (int, unsigned char *, int, int, int, int);
void BlockGfxto24 (int ,unsigned char *, int ,int ,int);
void BlockGfxto8 (int ,unsigned char *, int ,int ,int);
void BlockStrto24 (int, unsigned char *, int, int, int);
void BlockAnimto24 (int, int, unsigned char *, int, int, int);
int IsBlockAttached (int);
void PlaceTile (int, int);
char * GetBrushName (int);
int MakeBrush (int, int, int, int);
int MakeBrushBS (int, int, int, int);
void DestroyAllBrushes (void);
void NewBlock (void);
void CutBlock (void);
void CopyBlock (void);
void PasteBlock (void);
void DestroyUnusedBlocks (void);
void CorrectBlockStructures (void);
void InitAnims (void);
void UpdateAnims (void);
void AnimNew (void);
void AnimCut (void);
void AnimCopy (void);
void AnimPaste (void);
void PickBlock (int, int);
void SetRefFrame (int, int);
void DelAnimFrame (int, int);
void DestroyUnusedAnims (void);
void InsertAnimFrame (int, int, int);
void DestroyUnusedGraphics (void);
int ResizeMap (int, int, int);
void MapFill (int, int, int);
void SetTranspColour (void);
/* mapcust prototypes */
void MapCustSetDefBMPName (char *);
void MapCustomSetFormat (char *);
int MapCustomSave (char *);
int MapCustomLoad (char *, HWND);
/* mapdia.c prototypes */
void DIAEditInfo (HWND);
void DIALayerOnion (HWND);
void DIALayerSlide (HWND);
void DIADividers (HWND);
int DIAImportNumber (HWND);
int DIAExport (HWND, char *);
int DIAResizeMap (HWND);
int DIANewProject (HWND);
int DIAEditProperties (HWND, int);
void DIARangeEditBlocks (HWND);
int DIAEditAnimProperties (HWND, int);
void DIAGrabBrushBS (HWND);
void DIAGetBrushName (HWND, char *);
/* mapwin.c prototypes */
void InvalidateAll (void);
void RejigBrshMenu (int);
/* maped.c prototypes */
void MEResizeWind (int, int);
void MESetScrollRange (int, int, int, int);
void MEPickBlock (void);
void MEFillMap (int);
void MEDestroyUndoBuffs (void);
void MEUndo (void);
HWND CreateMapEditorWindow (HWND);
/* blocked.c prototypes */
void BEResizeWind (int, int);
void BENewBlock (void);
void BECutBlock (void);
void BECopyBlock (void);
void BEPasteBlock (void);
void BEBlockNext (void);
void BEBlockPrev (void);
void BEInvalidate (void);
void BESetScrollRange (void);
HWND CreateBlockEditorWindow (HWND);
/* import.c prototypes */
int IMImportBlocks (char *, int);
/* animprev.cpp prototypes */
void APDisplayMode (char *);
void APSetup (HWND, int);
void APCleanup (void);
int APStart (void);
HWND CreateAnimPreviewWindow (HWND);

