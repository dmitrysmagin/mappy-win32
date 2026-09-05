# Mappy 1.3.11 -> 1.4.23 Upgrade Plan

Source baseline: `src/` (1.3.11 + Tier-1/2 patches).
Reference: `mapwin1423/` (`mapwin.exe` 1.4.23, `MAPWIN.INI`, `DOCS/vhistory.html`, `DOCS/MAPWIN.HTM`, `DOCS/mapwin32pro.html`, `luascr/`, IDA `mapwin.map` / `mapwin.lst`).
Chunk preserve: `src/fmp14.c`, `src/fmp14.h` (`EPHD/TSTR/LSTR/OBDT/OBFN`), wired in `src/mapfunc.c`.

Policy: core/editor parity first, object/graph editors next, LUA last.

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

Other Stage 1 gaps closed here: EPHD applied on load (`ApplyEditorPrefs`); tilemodes already in MPHD reserved1; Resize Map New Left/New Top; Help → Edit MAPWIN.INI; Grab New Brush + Handle submenu; 32-bit FG/onion alphablend (1.4.19). Still open: NLYR vs our `LY10`–`LY99` (no verified sample; unknown-chunk round-trip).

## Stage 2 — Editors DONE

Object/area/label data path: `objedit.c` dialogs, `OE_DrawObjects`, place/show, OBJSTR export, `OBDT`/`OBFN`.

### Graph editor (Graph tab in the map window)

Layout: tile on the left; FLH/FLV/ROT/CLR/MSK/BLK/REV + Draw size + status along the bottom. Palette / RGB / preview follow `panelpro` (0 none, 1 cluster, 2 bar). `prographbg` sets the window fill.

- REV / Ctrl+Z undoes the last draw (one level, toggle). CLR fills with the current colour.
- Graph mousebuttons: current colour, colour 0, pick, fill, line.
- Keys: fill `f`, line `l`, pick `p`; Ctrl+click finds a colour in the palette. Arrows slide the tile.
- `?` then click: 8-bit picks the index; truecolour writes the pixel into the current slot (Ctrl = nearest palette).
- `sortblocks=1` / `sortobjects=1` run `luascr/BlockSort.lua` / `ObjectSort.lua` when those panels refresh.
- Block and object properties have a Text String combo (writes `STRBLKUSER` / `STROBJUSER`). Object dialog has next/prev. Double-click opens brush rename / object properties. Comma/period already step brushes and objects.

## Stage 3 — LUA [DONE]

L0–L4 in `PLAN-LUA.md`. Bundled `src/luascr/` scripts use the 1.4.23 `mappy.*` contracts (`getPixel` A,R,G,B / 8-bit index, `createBlock(index)`, `fileRequester`/`doDialogue`, `sendMessage(CM_,0)`, `shellExecute` 5 args, Placer/Area/400+ mouse, BlockProp/ObjectProp, `luafilemenu`). Custom menu is `&Custom`. `sortblocks` / `sortobjects` draw filters are in Stage 2. Remaining polish: in-GUI script pass.

## Stage 4 — Polish / release

- `MAPWIN.INI` comments/defaults vs `mapwin1423/MAPWIN.INI` (`panelpro`, `prographbg`, `hexordec`, `thumbtrack=1`, `tbarinfo=6`, `mwheelstyle` semantics).
- Docs, About, Tutorials-menu already absent (good).
- Resource-leak pass (MemProof: `ReleaseDC`, Anim Preview free, 1280×1024 0.25 crash, 8-bit export colours).
- Full regression: FMP round-trip including NLYR-compatible layers 8+, EPHD apply, all exports, Graph tab, Lua scripts.
