UNAME=$(shell uname)
DEBUG=1

CFLAGS += -Wall
CFLAGS += -Iinclude
CFLAGS += -I/usr/local/include

LDFLAGS += -lxcb-wm
LDFLAGS += -lxcb-keysyms
LDFLAGS += -lxcb-xinerama
LDFLAGS += -lX11
LDFLAGS += -L/usr/local/lib -L/usr/pkg/lib
ifeq ($(UNAME),NetBSD)
CFLAGS += -I/usr/pkg/include
LDFLAGS += -Wl,-rpath,/usr/local/lib -Wl,-rpath,/usr/pkg/lib
endif

ifeq ($(DEBUG),1)
# Extended debugging flags, macros shall be available in gcc
CFLAGS += -gdwarf-2
CFLAGS += -g3
else
CFLAGS += -O2
endif

FILES=$(patsubst %.c,%.o,$(wildcard src/*.c))
HEADERS=$(wildcard include/*.h)

src/%.o: src/%.c ${HEADERS}
	$(CC) $(CFLAGS) -c -o $@ $<

all: ${FILES}
	$(CC) -o i3 ${FILES} $(LDFLAGS)

clean:
	rm -f src/*.o

distclean: clean
	rm -f i3
