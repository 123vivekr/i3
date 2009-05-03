UNAME=$(shell uname)
DEBUG=1
INSTALL=install
MAKE=make
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

# Depend on the object files of all source-files in src/*.c and on all header files
FILES=$(patsubst %.c,%.o,$(wildcard src/*.c))
HEADERS=$(wildcard include/*.h)

# Depend on the specific file (.c for each .o) and on all headers
src/%.o: src/%.c ${HEADERS}
	echo "CC $<"
	$(CC) $(CFLAGS) -c -o $@ $<

all: ${FILES}
	echo "LINK i3"
	$(CC) -o i3 ${FILES} $(LDFLAGS)

install: all
	echo "INSTALL"
	$(INSTALL) -d -m 0755 $(DESTDIR)/usr/bin
	$(INSTALL) -d -m 0755 $(DESTDIR)/etc/i3
	$(INSTALL) -d -m 0755 $(DESTDIR)/usr/share/xsessions
	$(INSTALL) -m 0755 i3 $(DESTDIR)/usr/bin/
	test -e $(DESTDIR)/etc/i3/config || $(INSTALL) -m 0644 i3.config $(DESTDIR)/etc/i3/config
	$(INSTALL) -m 0644 i3.desktop $(DESTDIR)/usr/share/xsessions/

dist: clean
	[ ! -d i3-${VERSION} ] || rm -rf i3-${VERSION}
	[ ! -e i3-${VERSION}.tar.bz2 ] || rm i3-${VERSION}.tar.bz2
	mkdir i3-${VERSION}
	cp DEPENDS GOALS LICENSE PACKAGE-MAINTAINER TODO RELEASE-* i3.config i3.desktop pseudo-doc.doxygen i3-${VERSION}
	cp -r src include man i3-${VERSION}
	# Only copy toplevel documentation (important stuff)
	mkdir i3-${VERSION}/docs
	find docs -maxdepth 1 -type f ! -name "*.xcf" -exec cp '{}' i3-${VERSION}/docs \;
	sed -e 's/^GIT_VERSION=\(.*\)/GIT_VERSION=${GIT_VERSION}/g;s/^VERSION=\(.*\)/VERSION=${VERSION}/g' Makefile > i3-${VERSION}/Makefile
	tar cf i3-${VERSION}.tar i3-${VERSION}
	bzip2 -9 i3-${VERSION}.tar
	rm -rf i3-${VERSION}

clean:
	rm -f src/*.o
	$(MAKE) -C docs clean
	$(MAKE) -C man clean

distclean: clean
	rm -f i3
