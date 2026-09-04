# Makefile for
# Feud2000AD
#
# --------------------------------------------------------------------

PROGRAM = mapwin.exe

CFILES  = $(wildcard *.c)
OFILES  = $(CFILES:.c=.o)
DFILES  = $(CFILES:.c=.d)  # dependency files, so we only recompile
									# when needed, even if includes change

CFLAGS  = -O2 -Wall -s
LFLAGS  = -mwindows

LIB     = -lddraw -ldxguid

# --------------------------------------------------------------------

$(PROGRAM): $(OFILES)
	windres mapwin.rc -o mapwinrc.o
	gcc $(LFLAGS) -o $@ $(OFILES) mapwinrc.o $(LIB)

%.o: %.c
	gcc $(CFLAGS) -MMD -o $@ -c  $<

# read dependency information, in case include files changed, so all
# c files including the changed file get recompiled

-include $(DFILES)

.PHONY: clean
clean:
	rm -f *.o *.d *.exe
