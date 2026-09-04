# Makefile for Mappy Win32 (modernized, MinGW64) - lives in src/
# Usage:
#   mingw32-make            # default: no DirectX preview (uses animstub.c)
#   mingw32-make WITH_DX=1  # DirectX anim preview (uses animprev.c, -lddraw -ldxguid)
#   mingw32-make clean
#
# Cross from Linux: make CC=x86_64-w64-mingw32-gcc WINDRES=x86_64-w64-mingw32-windres
# NOTE: use '=' (not '?=') so the in-tree default wins over make's built-in
# CC=cc; command-line 'make CC=...' still overrides (as usual).
CC      = gcc
WINDRES = windres

PROGRAM = mapwin.exe

# Default: stub preview (portable). WITH_DX=1 selects animprev.c.
ifeq ($(WITH_DX),1)
ANIMSRC = animprev.c
DXLIBS  = -lddraw -ldxguid
CDEFINES = -DMAPPY_WITH_DX=1
else
ANIMSRC = animstub.c
DXLIBS  =
CDEFINES =
endif

# Lua 5.0.3 (statically linked)
LUADIR  = lua-5.0.3/src
LUADIR2 = lua-5.0.3/src/lib
LUA_CFILES = $(filter-out $(LUADIR)/ltests.c, $(wildcard $(LUADIR)/*.c))
LUA_CFILES += $(filter-out $(LUADIR2)/loadlib.c, $(wildcard $(LUADIR2)/*.c))
LUA_OFILES = $(LUA_CFILES:.c=.o)
LUA_CFLAGS = -O2 -Wall -Wextra -std=gnu99 -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -I$(LUADIR)/../include

CFILES  = mapfunc.c fmp14.c mapwin.c maped.c blocked.c mapdia.c mapcust.c import.c objedit.c luabind.c $(ANIMSRC)
OFILES  = $(CFILES:.c=.o)
DFILES  = $(CFILES:.c=.d)

CFLAGS  = -O2 -Wall -Wextra -std=gnu99
CFLAGS  += $(CDEFINES) -D_WIN32_WINNT=0x0601 -DWINVER=0x0601
CFLAGS  += -I$(LUADIR)/../include
LDFLAGS = -mwindows
LIBS    = -lcomdlg32 -lgdi32 -luser32 -lkernel32 -lcomctl32 $(DXLIBS)

# --------------------------------------------------------------------

$(PROGRAM): $(OFILES) $(LUA_OFILES) mapwinrc.o
	$(CC) $(LDFLAGS) -o $@ $(OFILES) $(LUA_OFILES) mapwinrc.o $(LIBS)

mapwinrc.o: mapwin.rc mapdefs.h MAPWIN.ICO
	$(WINDRES) mapwin.rc -o mapwinrc.o

# handle lowercase ico name on case-sensitive filesystems
MAPWIN.ICO:
	@if [ ! -f mapwin.ico ] && [ -f MAPWIN.ICO ]; then cp MAPWIN.ICO mapwin.ico; fi

$(LUADIR)/%.o: $(LUADIR)/%.c
	$(CC) $(LUA_CFLAGS) -MMD -MP -o $@ -c $<

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -o $@ -c $<

-include $(DFILES)

.PHONY: clean
clean:
	rm -f *.o *.d *.exe mapwinrc.o $(LUADIR)/*.o $(LUADIR)/*.d
