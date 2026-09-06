# Makefile for Mappy Win32 (modernized, MinGW64) - lives in src/
# From repo root: make / make clean / make WITH_DX=1
# From this directory:
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
LUA_CFLAGS = -pipe -O2 -Wall -Wextra -std=gnu99 -fno-lto -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -I$(LUADIR)/../include
LUA_CFLAGS += -Wno-implicit-fallthrough -Wno-address -Wno-type-limits -Wno-pointer-to-int-cast -Wno-unused-value

CFILES  = mapfunc.c fmp14.c tileset.c mapwin.c maped.c blocked.c mapdia.c mapcust.c import.c objedit.c luabind.c $(ANIMSRC)
OFILES  = $(CFILES:.c=.o)
DFILES  = $(CFILES:.c=.d)

CFLAGS  = -O2 -Wall -Wextra -std=gnu99 -fno-lto
CFLAGS  += $(CDEFINES) -D_WIN32_WINNT=0x0601 -DWINVER=0x0601
CFLAGS  += -I$(LUADIR)/../include
# Collect2's LTO plugin crashes on GCC 15.2.0; use ld directly
LD      = ld
LDFLAGS = -m i386pep --subsystem windows -Bdynamic
CRTBEGIN = $(shell $(CC) -print-file-name=crtbegin.o)
CRTEND   = $(shell $(CC) -print-file-name=crtend.o)
CRT2     = $(shell $(CC) -print-file-name=crt2.o)
LIBDIRS  = -L$(shell dirname $(CRTBEGIN))
SYSLIBS  = -lmingw32 -lgcc -lgcc_eh -lmingwex -lmsvcrt -lkernel32 -lpthread
LIBS    = -lcomdlg32 -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lole32 $(DXLIBS)

$(PROGRAM): $(OFILES) $(LUA_OFILES) mapwinrc.o
	$(LD) $(LDFLAGS) $(LIBDIRS) -o $@ $(CRT2) $(CRTBEGIN) \
		$(OFILES) $(LUA_OFILES) mapwinrc.o $(LIBS) $(SYSLIBS) $(CRTEND)

mapwinrc.o: mapwin.rc mapdefs.h mapwin.manifest MAPWIN.ICO
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
