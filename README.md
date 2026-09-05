# MapWin32

A Win32 tilemap editor for the Mappy `.FMP` format. Robin Burrows started Mappy in 1997; this tree is a modernized MinGW build of the editor sources, with **Mappy 1.4.23 features re-implemented** on top of the 1.3.11 codebase.

The live program is `src/mapwin.exe`.

## What it does

MapWin32 paints tile maps from a shared tileset, stores them as FMP (and related) files, and can export arrays and images for games. A map is a grid of **block** indices (still tiles or animations). Optional **objects** sit on top of that grid as a separate list (areas, labels, tile graphics, or external images) — they are not extra tile layers.

Notable 1.4.23-era behaviour in this build includes:

- Up to 100 layers, onion-skin (including All / lower layers), layer names
- Zoom 0.25 / 0.5 / 1 / 2 / 4
- Editor prefs, text strings, and objects stored in the FMP
- Graph tab for editing tile pixels
- Objects manager, place mode, and object properties
- Lua 5.0.3 scripting (`Custom` menu, drop a `.lua`, or pass a script on the command line)
- PNG import (via `LIBPNG13.DLL` / `LIBPNG12.DLL` if present), custom `.MAP` import/export, text / MAR / BMP exports
- Mouse-wheel, title-bar info modes, dividers, flipped / rotated tiles, and the rest of the 1.4.23 `MAPWIN.INI` surface

Help → Contents opens `MAPWIN.HTM` next to the exe when you ship that file. Help → Edit MAPWIN.INI opens the ini beside the exe.

## Building

You need a 64-bit MinGW-w64 GCC (`gcc`, `windres`, `ld`, `make`). From the repo root:

```text
make
```

That builds `src/mapwin.exe`. Other targets:

```text
make clean
make WITH_DX=1
```

`WITH_DX=1` links the DirectDraw animation preview (`animprev.c`). The default build uses a stub so you do not need the DirectX SDK.

From `src/`:

```text
mingw32-make
```

Cross-compile example:

```text
make CC=x86_64-w64-mingw32-gcc WINDRES=x86_64-w64-mingw32-windres
```

If the linker reports `Permission denied` on `mapwin.exe`, close the running editor and build again.

## Running

Copy or run `src/mapwin.exe`. On first launch it reads `MAPWIN.INI` from the exe directory (defaults live in `src/MAPWIN.INI`). Useful keys are documented in that file: window layout, zoom, wheel behaviour, mouse/key bindings, custom `.MAP` format, object/block dialog labels, and Lua file-menu hooks. `width` / `height` are 96-DPI units; see [High-DPI](#high-dpi) for how the UI scales.

Command line:

```text
mapwin.exe map.fmp
mapwin.exe map.fmp script.lua
```

You can also drop `.fmp`, `.fma`, `.map`, `.bmp`, `.pcx`, `.png`, `.mar`, or `.lua` onto the main window.

Lua scripts that ship with the editor go in `luascr\` next to the exe (`src/luascr/` in this repo). Examples: export helpers, layer tools, Placer / Area mouse scripts, `BlockProp.lua` / `ObjectProp.lua`.

## High-DPI

The exe embeds a Per-Monitor V2 manifest (plus Common Controls 6.0), so Windows does not bitmap-scale `mapwin.exe`. Chrome is laid out in 96-DPI units (DIPs) and converted with the window’s DPI (`src/dpi.h`).

**Tiles stay 1:1 device pixels.** Zoom 0.25 / 0.5 / 1 / 2 / 4 is the only scale for map cells. A 16×16 tile at Zoom ×1 is 16 screen pixels at any DPI. If tiles look small on a 200% laptop, use Zoom ×2 rather than expecting the OS scale to stretch the map.

What *does* follow Windows DPI:

- Startup size: `width` / `height` in `MAPWIN.INI` are DIPs (`640`×`480` → 960×720 at 150%, 1280×960 at 200%). The window is centered on the monitor under the cursor
- Map / Graph / Block split, tab strip, Graph toolbar and palette
- Overlay fonts (user-data and object labels)
- Wall, divider, graph-grid, and object-border pens

Fill (`f`), pick (`p`), and drag-map use client coordinates (`ScreenToClient`), not guessed frame metrics, so they stay on the tile under the cursor when borders or caption height change.

Dialog templates are still `FONT 8, "MS Sans Serif"` in dialog units; the dialog manager scales those once the process is DPI-aware. The DirectDraw animation preview (`WITH_DX=1`) is not DPI-aware.

Dragging the window between monitors with different DPI works for Graph / Block chrome (`WM_DPICHANGED` recreates fonts and relayouts). A full live re-layout of the main frame (and Lua `moveWindow` DIP conversion) is not done yet.

## File formats

| Extension | Role |
|---|---|
| `.FMP` | Full project (map, blocks, graphics, anims, objects, editor prefs) |
| `.FMA` | Map without graphics — load an FMP first, then the FMA |
| `.MAP` | Raw custom array; layout is `maptype` in `MAPWIN.INI` (default `a10`) |
| `.MAR` | Binary map-array dump |
| `.BAK` | Optional rename of the previous FMP when `backups=1` |

Unknown FMP chunks are kept and written back so a load/save round-trip does not drop data the editor does not interpret.

## Layout of this repo

```text
src/           Editor sources, resources, MAPWIN.INI, luascr/
src/lua-5.0.3/ Vendored Lua 5.0.3 (statically linked)
Makefile       Forwards to src/Makefile
```

## License

Original Mappy / MapWin32 editor code is © Robin Burrows. The source comments allow reuse if you credit him when you use a substantial portion.

Lua 5.0.3 is MIT (see `src/lua-5.0.3/COPYRIGHT`). Bundled scripts under `src/luascr/` include their own notices.
