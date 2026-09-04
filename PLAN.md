# Mappy 1.3.11 -> 1.4.23 Upgrade Plan

Source baseline: `src/` (1.3.11 + Tier-1/2 patches).
Reference: `mapwin1423/` (`mapwin.exe` 1.4.23, `MAPWIN.INI`, `DOCS/vhistory.html`, `DOCS/MAPWIN.HTM`, `DOCS/mapwin32pro.html`, `luascr/`, IDA `mapwin.map` / `mapwin.lst`).
Chunk preserve: `src/fmp14.c`, `src/fmp14.h` (`EPHD/TSTR/LSTR/OBDT/OBFN`), wired in `src/mapfunc.c`.

Policy: core/editor parity first, Pro editors next, LUA last.

Verified against the 1.4.23 binary (UTF-16 menus, exported symbols, docs). Several items previously marked CANCELLED **are in the EXE**.

## Stage 1 — Core parity DONE

Batch 1: 100 layers in memory, zoom 0.25/0.5, onion All + lower layers, `tbarinfo` 0–6, block labels, `wnesmode`, rotated-tile blit, wheel-to-window, PNG/MAPEXT import, text export, useful functions (halve / 16→8 / big picture), brush handles, key/mouse maps, drop routing, resize tiles, remove duplicate gfx/blocks.

**Stage 1b CANCELLED was wrong — these exist in 1.4.23 and are still missing here:**

| Original (menu / symbol) | Notes |
|---|---|
| **Save FMP without graphics (.FMA)** | MapTools → Useful functions. Menu string in EXE. |
| **Remove Marker graphics…** | Layer menu. `_RemoveMarkerGraphics` in `mapwin.map`. |
| **Layer Bank** (0–9 … 90–99) | Layer menu banks. We only list 0–7 + Go-to-layer. |
| **Flipped tiles mode** menu toggle | INI-only here; original has MapTools item. |
| **Background Layers darkened** | Layer menu item (1.4.7); we only have the onion checkbox. |
| **Auto set BG transparency** | Useful functions item; we only have the Range Edit button. |
| **`luafilemenu=1`** | Replaces File items with `luascr/luafile*.lua`. INI key unused. |

`Adjust Values` **is** implemented (`DIALayerAdj`). Multi-layer `.MAP` import still replaces the current layer only (`MapCustomLoad`).

Other Stage 1 gaps vs binary: NLYR vs our `LY10`–`LY99` (not 1.4.23-compatible); EPHD not applied on load (showuser/onion/curlayer/dividers); tilemodes not stored in FMP; 32-bit blit is colourkey not alphablend (1.4.19); Resize Map missing New Left/New Top; Help → MapWin32 Pro / Edit MAPWIN.INI; Grab New Brush + Handle submenu; Custom menu (we inject Scripts).

## Stage 2 — Pro editors PARTIAL

Object/area/label data path DONE: `objedit.c` dialogs, `OE_DrawObjects`, place/show, OBJSTR export, `OBDT`/`OBFN`.

### Graph editor (Graph tab in the map window) — IN PROGRESS

This is the real Pro graph UI (`graphed.c` / `_GEDrawGraphEd` in the binary), not the extra `D_GRAPHEDIT` dialog.

**Layout pass (matches 1.4.23 screenshot):**
- Tile canvas on the left; FLH/FLV/ROT/CLR/MSK/BLK/REV + Draw size + status along the **bottom**.
- Palette 16×16, RGB bars, live map preview **right-aligned** in the map window (`GEComputeLayout` in `src/maped.c`).
- Palette cells scaled (~20px, shrinks on narrow windows). Title is always `Palette (col N/255)`.
- RGB bars are click/drag and write `cmappt` (8-bit maps recolour by index; truecolour uses the palette as a swatch store). Decimal + `$hex` under each bar.
- Corner preview renders the current viewport with `Blockto24` (was an occupancy grid that looked like noise).
- Palette tools CPY / EXG / SPR / TRN / `?` (pick from tile) wired.

**Still missing vs `mapwin32pro.html` / original Graph tab:**
- REV as last-draw undo (`ctrl+z`); ours is 180° rotate.
- Graph-mode mousebuttons (block0=colour0, pick, fill, line).
- Fill (`f`) / line (`l`) / pick (`p`) keys; ctrl+click find colour in palette.
- `panelpro` (0=no panel / 1=cluster / 2=bar); `prographbg`.
- `sortblocks` / `sortobjects` Lua filter hooks.
- Object properties Text String field; next/prev on brushes and objects.

## Stage 3 — LUA IN PROGRESS

L0 harness DONE (Lua 5.0.3, Script menu, drop, cmdline, mouse 5/10/400+).
68 `mappy.*` names are **registered**; many are stubs or wrong vs `DOCS/luascript.html`. Subplan: `PLAN-LUA.md`.

Must-fix before bundled `luascr/` can match 1.4.23: `getPixel` A,R,G,B; `createBlock(index)`; `sendMessage(CM_,0)`; `shellExecute` 5 args; `fileRequester` / dialogues; `newMap` / chunks / text strings; BlockProp/ObjectProp per-draw hooks; Placer/Area from INI names not hardcoded paths; constants (`NUMLAYERS`, `AN_*`, `MAPONION`, …).

## Stage 4 — Polish / release

- `MAPWIN.INI` comments/defaults vs `mapwin1423/MAPWIN.INI` (`panelpro`, `prographbg`, `hexordec`, `thumbtrack=1`, `tbarinfo=6`, `mwheelstyle` semantics).
- Docs, About, Tutorials-menu already absent (good).
- Resource-leak pass (MemProof: `ReleaseDC`, Anim Preview free, 1280×1024 0.25 crash, 8-bit export colours).
- Full regression: FMP round-trip including NLYR-compatible layers 8+, EPHD apply, all exports, Graph tab, Lua scripts.
