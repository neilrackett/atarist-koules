# Copyright (C) 2026 Neil Rackett
# SPDX-License-Identifier: GPL-2.0-or-later

# Front end for the per-platform makefiles.  Koules has no configure
# step, so each target platform keeps its own Makefile.<platform> and
# this one just picks between them.
#
#   make                    the default platform (atari)
#   make atari              Atari ST/STE via STDL  -> Makefile.atari
#   make sdl                desktop SDL           -> Makefile.sdl
#   make sim                host physics harness  -> Makefile.sim
#   make clean              clean every platform
#   make help               this list
#
# Command-line variables are inherited by the platform makefile:
#
#   make atari EXTRA_CFLAGS=-DFN_HEAP_DEBUG
#
# To reach a platform's own targets, pass TARGET (or name the makefile
# directly, which is equivalent):
#
#   make atari TARGET=floatcheck
#   make -f Makefile.atari floatcheck
#
# The Atari build needs the STDL submodule:
#
#   git clone --recurse-submodules <url>     # fresh clone
#   git submodule update --init              # existing clone
#
# Note the SDL build is upstream's desktop one and is not maintained by
# the Atari port: the ST work removed the forked sound-server driver it
# expects, and the shared physics is now 16.16 fixed point rather than
# float.  Pristine upstream is on the lr-sdl branch.

PLATFORMS = atari sdl sim
PLATFORM ?= atari

# passed through to the chosen platform makefile; empty means its
# default goal
TARGET ?=

.PHONY: all help clean $(PLATFORMS)

all: $(PLATFORM)

$(PLATFORMS):
	$(MAKE) -f Makefile.$@ $(TARGET)

# Best-effort: one platform failing must not stop the others, so
# failures are collected rather than aborting the loop.  Note the SDL
# clean sweeps object files tree-wide, which reaches extern/stdl and
# removes the built library along with them.
clean:
	@rc=0; for p in $(PLATFORMS); do \
	  echo "==> clean $$p"; \
	  $(MAKE) -f Makefile.$$p clean \
	    || { echo "    Makefile.$$p clean failed"; rc=1; }; \
	done; exit $$rc

help:
	@echo 'make [platform] - build Koules for one platform'
	@echo
	@echo '  atari   Atari ST/STE via STDL (default)'
	@echo '  sdl     desktop SDL (upstream; unmaintained here)'
	@echo '  sim     host physics harness'
	@echo
	@echo '  clean   clean every platform'
	@echo
	@echo 'Examples:'
	@echo '  make'
	@echo '  make atari TARGET=floatcheck'
	@echo '  make atari EXTRA_CFLAGS=-DFN_HEAP_DEBUG'
