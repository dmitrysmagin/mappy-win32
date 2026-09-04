# LUA Subplan (in progress — Stage 3 of PLAN.md)

Reference: `mapwin1423/luascr/`, `mapwin1423/DOCS/luascript.html`, `mapwin1423/DOCS/vhistory.html`, `mapwin1423/MAPWIN.INI:69-100`.

## L0 — Engine harness [DONE]
- Vendoring/build: Lua 5.0.3 in `src/lua/lua-5.0.3/` (core + lib, `loadlib.c` excluded), compiled via Makefile. Build clean, smoke `exit=124`.
- `luabind.c`: 68 `mappy.*` functions registered. Engine lifecycle (init/shutdown), `LuaRunMouseButton`, `LUA_DLL` command-line execution.
- `mapwin.c`: INI `lua0-16` parsed, Script menu (IDs 7000+), command-line `.lua` dispatch, drag-drop `.lua` support, `LuaShutdown()` before cleanup.
- `maped.c`: Mouse button dispatch 5=Placer.lua, 10=Area.lua, 400+=scriptN.lua.

## L1 — Core get/setValue + window/menu [PENDING]
- Map/meta: `MAPFILENAME` (setter updates titlebar), `MAPWINPATH`, `MAPWINVERSION`, `MAPTYPE`, `NUMBRUSHES`, `NUMBLOCKANIM`, `XMAPOFFSET/YMAPOFFSET`, `MAPONION/MAPDARK`, `MAPDIVIDERS (ONOFF,X,Y,COLOUR)`, `MAPDIVIDERS` get returns tuple, `MAPFILENAME/CMDLINE/NUMCHUNKS/CUROBJECT/NUMOBJECTS/NUMFILESTR`.
- UI: `moveWindow (MAIN/MAP/BLOCK, x,y,w,h)`, `renameMenuItem`, `renameLayer`, `getLayerName`, `tbarinfo` integration.
- Dialogs: Dialogue/Dialogue-Proc examples (`luascr/`), BlockProp/ObjectProp per-draw label hooks, `BLOCKLABELS/OBJECTLABELS`.

## L2 — Chunks/maps/files [PENDING]
- `createChunk/deleteChunk/getChunkValue/setChunkValue`, `newMap ([gap/stagger/type])`, `closeMap/openFMP/saveFMP`, `importGraphics (name,start,n)`, `resizeMap (w,h,centering 1-9)`, `copyLayer/updateScreen`, undo-aware script convention.

## L3 — Anims/brushes/objects [PENDING]
- Anims: `createAnim/deleteAnim`, `get/setAnimFrame (incl. ANMREFFRAME)`, `insert/cut`, `numAnimFrames`, `get/setAnimValue (ANMDELAY/COUNT/TYPE/USER/CURFRAME)`, `AN_*` constants.
- Brushes: `create/delete/renameBrush`, `get/setBrushBlock`, `getBrushValue (BRUSHWIDTH/HEIGHT/NAME)`.
- Objects: `get/setObjectValue (OBJXPOS/YPOS/GFXID/TILEID/GXOFF/GYOFF/GWIDTH/GHEIGHT/GHANDLE.../SHOW/USER1-7/FLAGS)`, `create/deleteObject`, `get/setObjectFilename/Sort`, `OBJGXOFF/OBJGYOFF` (1.4.20), `shellExecute/sendMessage`.

## L4 — Script parity [PENDING]
- Port `luascr/`: Export Flash actionscript, Export binary file, Set/Swap palette, Solid rectangle (last row/col fix), Autofringe, Read textfile, Export GBA 16x16 table, Tile graphic test, Random distribution, Merge layers, Show mouse coords, Find block in map, Set colourkey, Set Text Strings, Export all layers as MAR, Placer (smooth-as-draw), Area (solid rect + drag-run).
- Docs: `textexportopts` comment fix, `setValue` docs correction.

## Exit criteria
- All L0-L4 scripts run headless + in-GUI without regression to Stage 1-2 features; INI sample covers `lua0-16`, keys, mousebuttons, `textexportopts`.
