# LUA Subplan (Stage 3 of PLAN.md) — DONE

Reference: `mapwin1423/luascr/`, `mapwin1423/DOCS/luascript.html`, `mapwin1423/DOCS/vhistory.html`, `src/MAPWIN.INI`.

## L0 — Engine harness [DONE]
- Lua 5.0.3 vendored; Script menu (`&Custom`, IDs 7000+), drop, cmdline, `LuaInit` always.
- Mouse 5 = `luascr\Placer.lua`, 10 = Area (drag then run on button up), 400+ = `lua01`…

## L1 — Core get/setValue + window/menu [DONE]
- Strings: `MAPFILENAME`, `MAPWINPATH` (trailing `\`), `MAPWINVERSION`, `CMDLINE`.
- Palette: `PALETTEARGB+index` returns/sets A,R,G,B (base 1000).
- `NUMBLOCKGFX`, `NUMLAYERS`, `NUMTEXTSTR`, `NUMFILESTR`, `TRANS*`, `CLICKMASK`, `MAPONION`/`MAPDARK`, `MAPDIVIDERS` tuple, `BEWINDOWTYPE`, `BLOCKSPERROW`.
- `moveWindow`, `renameMenuItem`, `getLayerName`/`renameLayer` via LSTR.
- `msgBox` maps IDOK → `MMB_OK` (0) so `== mappy.MMB_OK` matches OK/Cancel.
- BlockProp.lua / ObjectProp.lua on properties `WM_INITDIALOG`; `CUREDIT`, `BLOCKLABELS`/`OBJECTLABELS`.

## L2 — Chunks/maps/files [DONE]
- `newMap`/`closeMap`/`openFMP`/`saveFMP`/`importGraphics`/`resizeMap`.
- Custom chunks via unknown-chunk list (`CHUNKID`/`CHUNKSIZE`).
- `copyLayer` with `MPY_UNDO`; `getBlock` errors on missing layer (Layer count.lua).
- `fileRequester` returns `R, filename`; `doDialogue` type 1/2 and custom `DLGTEMPLATE` + proc.

## L3 — Anims/brushes/objects [DONE]
- Anim insert-at-index, `ANMREFFRAME`, `ANMDELAY`/`ANMTYPE`/…, `AN_*`.
- Brushes 0-based (NUMBRUSHES = next free index); `BRUSHWIDTH`/`HEIGHT`/`NAME`.
- Objects: F14Obj fields, filenames, sort arrays; `shellExecute` 5 args; `sendMessage(code,0)` → `WM_COMMAND`.

## L4 — Script parity [DONE]
- API contracts for bundled `src/luascr/` (Export tiles/palette/colourkey/text strings/anim test/dialogues/Placer/Area).
- `luafilemenu=1` runs `luascr\luafile*.lua` in place of File New/Open/Save/….
- Graphic index (not raw byte offset) for `BLKBG`/`BLKFG*`.
- `getPixel`: 8-bit index, else A,R,G,B; `setPixel` INDEX or A,R,G,B.

## Follow-ups (not blocking Stage 3)
- `sortblocks=1` / `sortobjects=1` run `BlockSort.lua` / `ObjectSort.lua` on panel refresh (Stage 2).
- In-GUI click-through of every Custom-menu script.
