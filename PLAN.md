# Mappy 1.3.11 -> 1.4.23 Upgrade Plan

Source baseline: `src/` (1.3.11 + Tier-1/2 patches).
Reference: `mapwin1423/` (`mapwin.exe` 1.4.23, `MAPWIN.INI`, `DOCS/vhistory.html`, `DOCS/MAPWIN.HTM`, `DOCS/mapwin32pro.html`, `luascr/`, IDA `mapwin.map` / `mapwin.lst`).
Chunk preserve: `src/fmp14.c`, `src/fmp14.h` (`EPHD/TSTR/LSTR/OBDT/OBFN`), wired in `src/mapfunc.c`.

Policy: core/editor parity first, Pro editors next, LUA last.

Verified against the 1.4.23 binary (UTF-16 menus, exported symbols, docs). Several items previously marked CANCELLED **are in the EXE**.

## Stage 1 — Core parity DONE

Batch 1: 100 layers in memory, zoom 0.25/0.5, onion All + lower layers, `tbarinfo` 0–6, block labels, `wnesmode`, rotated-tile blit, wheel-to-window, PNG/MAPEXT import, text export, useful functions (halve / 16→8 / big picture), brush handles, key/mouse maps, drop routing, resize tiles, remove duplicate gfx/blocks.

**Stage 1b DONE** (menus were stubbed; handlers + behaviour now match 1.4.23):

| Original (menu / symbol) | Notes |
|---|---|
| **Save FMP without graphics (.FMA)** | Useful functions. `SaveProjectEx(1)` omits BGFX/AGFX. Open FMA after an FMP to keep graphics. |
| **Remove Marker graphics…** | Layer menu. Clears FG3 then `DestroyUnusedGraphics`. |
| **Layer Bank** (0–9 … 90–99) | Layer menu shows the current bank of 10 layers. |
| **Flipped tiles mode** | MapTools toggle (also INI / MPHD reserved1). |
| **Background Layers darkened** | Layer menu toggle (`oniondark`). |
| **Auto set BG transparency** | Useful functions + Range Edit button. |
| **`luafilemenu=1`** | Stage 3: File items run `luascr/luafile*.lua` when present. |

`Adjust Values` **is** implemented (`DIALayerAdj`). Multi-layer `.MAP` import still replaces the current layer only (`MapCustomLoad`).

Other Stage 1 gaps closed here: EPHD applied on load (`ApplyEditorPrefs`); tilemodes already in MPHD reserved1; Resize Map New Left/New Top; Help → MapWin32 Pro / Edit MAPWIN.INI; Grab New Brush + Handle submenu; 32-bit FG/onion alphablend (1.4.19). Still open: NLYR vs our `LY10`–`LY99` (no verified sample; unknown-chunk round-trip).

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

## Stage 3 — LUA [DONE]

L0–L4 in `PLAN-LUA.md`. Bundled `src/luascr/` scripts use the 1.4.23 `mappy.*` contracts (`getPixel` A,R,G,B / 8-bit index, `createBlock(index)`, `fileRequester`/`doDialogue`, `sendMessage(CM_,0)`, `shellExecute` 5 args, Placer/Area/400+ mouse, BlockProp/ObjectProp, `luafilemenu`). Custom menu is `&Custom`. Remaining polish: `sortblocks` draw filter, in-GUI script pass.

## Stage 4 — Polish / release

- `MAPWIN.INI` comments/defaults vs `mapwin1423/MAPWIN.INI` (`panelpro`, `prographbg`, `hexordec`, `thumbtrack=1`, `tbarinfo=6`, `mwheelstyle` semantics).
- Docs, About, Tutorials-menu already absent (good).
- Resource-leak pass (MemProof: `ReleaseDC`, Anim Preview free, 1280×1024 0.25 crash, 8-bit export colours).
- Full regression: FMP round-trip including NLYR-compatible layers 8+, EPHD apply, all exports, Graph tab, Lua scripts.
