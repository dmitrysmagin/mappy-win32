# LUA Subplan (deferred — after PLAN.md Stages 1-2)

Reference: `mapwin1423/luascr/`, `mapwin1423/DOCS/luascript.html`, `mapwin1423/DOCS/vhistory.html`, `mapwin1423/MAPWIN.INI:69-100`.

## L0 — Engine harness
- Vendoring/build: Lua 5.0.3 (1.4.21 reverted from 5.1: `writeIntLSB` rounding bug), link into `src/` (`Makefile`), no `lua*` in `src/*.c` today.
- Lifecycle: init/close, error popup on script error (1.4.21, thanks Merick), `mappy.sendMessage(106,0)` exit path, command-line `mapwin "luascr/X.lua" test.fmp`, drag-drop `.lua` onto editor.
- INI: `lua01-16` (menu) + extended 30-script table (16 menu + rest key-bound), `luafilemenu` File-menu replacement (`luafilenewmap.lua`, `luafileopen.lua`, ...), `keyF1-10`/`key1-3`/`keyi/keyk/keym` CM mapping, `mousebuttons` (`lbutton/mbutton/rbutton/xbutton1`, 0-10 list + 100+ commands + 400+ lua-run).

## L1 — Core get/setValue + window/menu
- Map/meta: `MAPFILENAME` (setter updates titlebar), `MAPWINPATH`, `MAPWINVERSION`, `MAPTYPE`, `NUMBRUSHES`, `NUMBLOCKANIM`, `XMAPOFFSET/YMAPOFFSET`, `MAPONION/MAPDARK`, `MAPDIVIDERS (ONOFF,X,Y,COLOUR)`, `MAPDIVIDERS` get returns tuple, `MAPFILENAME/CMDLINE/NUMCHUNKS/CUROBJECT/NUMOBJECTS/NUMFILESTR`.
- UI: `moveWindow (MAIN/MAP/BLOCK, x,y,w,h)`, `renameMenuItem`, `renameLayer`, `getLayerName`, `tbarinfo` integration.
- Dialogs: Dialogue/Dialogue-Proc examples (`luascr/`), BlockProp/ObjectProp per-draw label hooks, `BLOCKLABELS/OBJECTLABELS`.

## L2 — Chunks/maps/files
- `createChunk/deleteChunk/getChunkValue/setChunkValue`, `newMap ([gap/stagger/type])`, `closeMap/openFMP/saveFMP`, `importGraphics (name,start,n)`, `resizeMap (w,h,centering 1-9)`, `copyLayer/updateScreen`, undo-aware script convention.

## L3 — Anims/brushes/objects
- Anims: `createAnim/deleteAnim`, `get/setAnimFrame (incl. ANMREFFRAME)`, `insert/cut`, `numAnimFrames`, `get/setAnimValue (ANMDELAY/COUNT/TYPE/USER/CURFRAME)`, `AN_*` constants.
- Brushes: `create/delete/renameBrush`, `get/setBrushBlock`, `getBrushValue (BRUSHWIDTH/HEIGHT/NAME)`.
- Objects: `get/setObjectValue (OBJXPOS/YPOS/GFXID/TILEID/GXOFF/GYOFF/GWIDTH/GHEIGHT/GHANDLE.../SHOW/USER1-7/FLAGS)`, `create/deleteObject`, `get/setObjectFilename/Sort`, `OBJGXOFF/OBJGYOFF` (1.4.20), `shellExecute/sendMessage`.

## L4 — Script parity
- Port `luascr/`: Export Flash actionscript, Export binary file, Set/Swap palette, Solid rectangle (last row/col fix), Autofringe, Read textfile, Export GBA 16x16 table, Tile graphic test, Random distribution, Merge layers, Show mouse coords, Find block in map, Set colourkey, Set Text Strings, Export all layers as MAR, Placer (smooth-as-draw), Area (solid rect + drag-run).
- Docs: `textexportopts` comment fix, `setValue` docs correction.

## Exit criteria
- All L0-L4 scripts run headless + in-GUI without regression to Stage 1-2 features; INI sample covers `lua01-16`, keys, mousebuttons, `textexportopts`.
