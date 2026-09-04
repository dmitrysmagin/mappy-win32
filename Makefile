# Delegate to src/Makefile so `make` works from the repo root.
# Usage:
#   make
#   make WITH_DX=1
#   make clean

.PHONY: all clean

all:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean
