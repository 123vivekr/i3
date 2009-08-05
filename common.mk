UNAME=$(shell uname)
DEBUG=1
INSTALL=install
GIT_VERSION=$(shell git describe --tags --always)
VERSION=$(shell git describe --tags --abbrev=0)

CFLAGS += -std=c99
CFLAGS += -pipe
CFLAGS += -Wall
CFLAGS += -Wunused
CFLAGS += -Iinclude
CFLAGS += -I/usr/local/include
CFLAGS += -DI3_VERSION=\"${GIT_VERSION}\"

# Check if pkg-config is installed, because without pkg-config, the following
# check for the version of libxcb cannot be done.
ifeq ($(shell which pkg-config 2>/dev/null 1>/dev/null || echo 1),1)
$(error "pkg-config was not found")
endif

ifeq ($(shell pkg-config --exists xcb-keysyms || echo 1),1)
$(error "pkg-config could not find xcb-keysyms.pc")
endif

ifeq ($(shell pkg-config --exact-version=0.3.3 xcb-keysyms && echo 1),1)
# xcb-keysyms fixed API from 0.3.3 to 0.3.4, so for some months, we will
# have this here. Distributions should upgrade their libxcb in the meantime.
CFLAGS += -DOLD_XCB_KEYSYMS_API
endif

LDFLAGS += -lm
LDFLAGS += -lxcb-event
LDFLAGS += -lxcb-property
LDFLAGS += -lxcb-keysyms
LDFLAGS += -lxcb-atom
LDFLAGS += -lxcb-aux
LDFLAGS += -lxcb-icccm
LDFLAGS += -lxcb-xinerama
LDFLAGS += -lX11
LDFLAGS += -lev
LDFLAGS += -L/usr/local/lib -L/usr/pkg/lib

ifeq ($(UNAME),NetBSD)
# We need -idirafter instead of -I to prefer the system’s iconv over GNU libiconv
CFLAGS += -idirafter /usr/pkg/include
LDFLAGS += -Wl,-rpath,/usr/local/lib -Wl,-rpath,/usr/pkg/lib
endif

ifeq ($(UNAME),FreeBSD)
LDFLAGS += -liconv
endif

ifeq ($(UNAME),Linux)
CFLAGS += -D_GNU_SOURCE
endif

ifeq ($(DEBUG),1)
# Extended debugging flags, macros shall be available in gcc
CFLAGS += -gdwarf-2
CFLAGS += -g3
else
CFLAGS += -O2
endif

# Don’t print command lines which are run
.SILENT:

# Always remake the following targets
.PHONY: install clean dist distclean

