# Mappy 1.3.11 -> 1.4.23 Upgrade Plan

Source baseline: `src/` (1.3.11 + Tier-1/2 patches).
Reference: `mapwin1423/` (`mapwin.exe`, `MAPWIN.INI`, `DOCS/vhistory.html`, `DOCS/MAPWIN.HTM`, `DOCS/mapwin32pro.html`, `luascr/`).
Chunk preserve only (no UI): `src/fmp14.c`, `src/fmp14.h` (`EPHD/TSTR/LSTR/OBDT/OBFN`), wired in `src/mapfunc.c`.

Policy: push LUA to later stages. Do core/editor parity first, Pro editors next, LUA last.

## Stage 1 — Core parity (no LUA, no Pro editors)
Batch 1 DONE (builds clean, `mapwin.exe` smoke `exit=124`):
- Layers 8 -> 100 in memory (`MAXMAPLAYERS` in `src/mapdefs.h`; `src/mapfunc.c`, `src/mapdia.c:71,1248`); file BODY/LYR1-9 + LY10-99 extension (`src/mapfunc.c` load/save); NLYR stays opaque (layout unverified from `mapwin.lst sub_425FD0`, round-trips). Menu 0-7 + Go-to-layer (8+) + Rename (LSTR) + grey-missing + titlebar names.
- Zoom x0.25/x0.5 (`zoomdiv`, `src/maped.c` ZX/ZD, `CM_TOOLZOOM025/05`), `zmstyle` software stretch (`MESoftStretch`), grid off in zoomout, `panelzoom` block-window gate (`src/blocked.c`).
- Onionskin full: `All` (`ONION_ALL`), Show-lower-layers (`onionlower`, `OnionCompto24`), dialog `src/mapdia.c:1467` + `D_OSKIN`, export paths updated.
- Titlebar `tbarinfo` 0-6 + layer count/names + TSTR (`src/maped.c:MEShowCoords`); `showuser` 1-8 tile overlay.
- Block labels (`blockdialoguelabels` INI -> `D_PROPEDIT`), `e` edits block/anim, mode hint (flipped/pillar/rotated).
- `wnesmode` red walls + menu, `rotatedtiles` INI + menu + label hint (render rotation pending).
- Input: wheel-to-window-under-mouse (`src/mapwin.c`), block-window wheel, drag-drop whitelist + PNG/MAPEXT friendly errors, `folder` INI, `wlayout -1`.
- INI: `tbarinfo/panelzoom/zmstyle/textexportopts` parsed; `MAPWIN.INI` synced.

Stage 1b DONE (all `mapwin.lst`-checked where noted, build clean, smoke `exit=124`):
- PNG import via runtime libpng bridge (`src/import.c:IMDecodePNG`: `LIBPNG13.DLL` -> `LIBPNG12.DLL` fallback, `png_*` via `GetProcAddress`, 4/8/24/32-bit incl. palette/gray/alpha paths; BMP-layout handoff to existing pipeline). Drop + File:Import filters include `.png`.
- MAPEXT hook exact per `.lst sub_40A6EF` (`src/import.c:IMTryMapext`: `LoadLibrary("MAPEXT.DLL")` -> `MPYDecodeBMP(filename,filedata,outbuf)`, `malloc(w*h*4+0x2710)`, non-zero fails; also used for bitcount 1/4 + non-`BI_RGB`, matching `cmp ax,7` branch).
- Rotated-tiles render (`RotRect24` in `src/mapfunc.c`: USER7 1=90/2=180/3=270, square-only for 90/270; hooked in `Blockto24/Overlayto24/Onionto24/BlockSecto24`; skipped for pillars/stagger early-exits). `tilemodes`-in-FMP: `.lst` shows `wnesmode` INI-only (single read site) — INI persist is faithful, no chunk work needed.
- Export textfile (`File:Export textfile...`, `D_TXTEXP`, `DIAExportText` in `src/mapdia.c`): CONST/CMAP(+GBA15)/ANIBLKSTR/MAPARRAY(1D/2D, all/single layers)/BLKGFX(+16COL nibbles)/MERGEFLIP(0x4000/0x8000)/RESOLVEBG/USEPREFIX; defaults + session persist via `textexportopts`.
- Useful functions submenu: Halve block dims (`.lst`-verified "lose brushes" prompt, channel-correct 8/15/16/24/32 averaging), Convert 16x16->8x8 (refuses on anims), Create-map-from-big-picture as new layer (SAD best-match, exact-size check).
- Brush: 4-corner handles (`Brush:handle` cycles, `src/mapfunc.c:brushhandle`), paste-off-edges clipping (`PlaceTile`). Brush-window anims: N/A (Block window `betype` already shows anims).
- Key/mouse mapping (`.lst`-verified tables): `l/m/r/xbutton1-4` + exact 0-10 action list (`None/Current block/Block 0/Drag map 1/Drag map 2/Placer.lua/Pick block/Lay path/Fill/Line/Area.lua`; 100+ = CM_, Lua entries message Stage 3); `keyF1-10/key1-3/keyi/keyk/keym` -> CM_ in main + map windows; F1 accelerator removed so `keyF1` remaps (default 119 = help).
- Drop routing fixed (images -> import, `.MAR` -> layer re-import, no rename on non-map drops).

Stage 1b REMAINING DONE (build clean, smoke `exit=124`):
- Resize block dimensions: `D_RESIZET` dialog (`DIAResizeBlk` in `src/mapdia.c`): width/height edit (4..252), depth combo (8/15/16/24/32), block struct offsets rescaled proportionally, graphics buffer zeroed with "graphics have been reset" warning. Menu item `CM_TOOLRESIZEBLK` under Useful functions.
- Remove duplicate graphics: `RemoveDuplicateGraphics` in `src/mapdia.c` (brute-force O(n^2) pixel-by-pixel compare via `memcmp`, mark-and-sweep deletion, block reference reindexing, buffer compaction). Menu `CM_TOOLDUPGFX` (Ctrl+Shift+G).
- Remove duplicate blocks: `RemoveDuplicateBlocks` in `src/mapdia.c` (O(n^2) 32-byte record compare with mask `0x0F0FFFFF` on flags, map layer reindexing, `memmove` compaction). Menu `CM_TOOLDUPBLK` (Ctrl+Shift+B).

Stage 1b CANCELLED (no verified spec in binary):
- `Save FMP without graphics`: No conditional skip found in the binary's save path; BGFX always written.
- Import `.MAP` as layers: `sub_439040` always replaces current layer; no "import as additional layer" function exists.
- Marker-graphics / Adjust-Values helpers: Not found as distinct functions in `.lst`.

## Stage 2 — Pro editors (still no LUA) DONE
Batch built clean, smoke `exit=124`:
- Object/Area/Label editors (`src/objedit.c`): `D_OBJECT` dialog (8 user fields, border colour picker, coords, U8 kind field), `DIAObjectProps`; `D_OBJMGR` dialog (list + New/Cut/Copy/Paste/Edit/Delete, double-click properties, `DIAObjectMgr`); `DIAObjFname` (external graphic files add/remove); `objlabels` INI + labels mode (`showuser` 8+ uses object fields when `objlayeruser` nonzero).
- Object rendering: `OE_DrawObjects` hooked into `MEDrawMap` (`src/maped.c`), `sortobjects` + `objlayeruser` INI persist; `OE_ObjOverlayText` feeds `showuser` overlay for object-rich maps.
- Object placement: `CM_OBJPLACE` toggle, click places selected object (shift=with-labels), ctrl+click duplicates, `p` picks object under cursor; `CM_OBJSHOW` toggles rendering.
- OBJSTR text export: `OE_ExportObjStr` (area-only + ext-gfx objects, block-concat labels), wired into `DIAExportText` (`IDC_TXTOBJSTR` checkbox, `textexportopts[18]`).
- Tile Graph editor: `D_GRAPHEDIT` (owner-draw 24bpp preview, nav ◀/▶, New/Cut/Copy/Paste with Windows clipboard via CF_DIB (8/24-bit, nearest-match palette, bidirectional), Fill/Line with thickness, Undo buffer, `<`/`>` nav, BLK quick-assign (no highlight), colour/thickness fields); `DIAObjFname` external file list (add/remove, objects re-pointed on delete).
- Objects menu + menu items wired in `mapwin.c` (`CM_OBJMGR/CM_OBJPLACE/CM_OBJSHOW/CM_GRAPHEDIT`); Objects menu checked at load; `objplace` exported to `maped.c`.
- `F14_SetCurObject`, `F14_DeleteImage` added to `src/fmp14.c` public API.
- `objedit.o` added to Makefile.

## Stage 3 — LUA (in progress)
Lua 5.0.3 source placed in `src/lua/lua-5.0.3/`. Makefile compiles core + lib objects. `luabind.c` registers 68 `mappy.*` functions + constants. `mapwin.c` wires INI `lua0-16`, Script menu (IDs 7000+), command-line `.lua` dispatch, `LuaShutdown()`. `maped.c` dispatches mouse buttons 5/10/400+ to `LuaRunMouseButton`. Build clean, smoke `exit=124`. Runtime testing pending (requires GUI). Subplan in `PLAN-LUA.md`.

## Stage 4 — Polish / release
- `MAPWIN.INI` comments/defaults sync with `mapwin1423/MAPWIN.INI`, docs sync (`MAPWIN.HTM`, Pro docs), About cleanup, Tutorials-menu removal check.
- Resource-leak pass (MemProof items: `ReleaseDC`, Anim Preview free, 1280x1024 0.25 crash, 8-bit export colours).
- Full regression: FMP round-trip (unknown-chunk preserve + EPHD/TSTR/LSTR/OBDT/OBFN), all exports, all dialogs.
