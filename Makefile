VERSION = 0.1.1
CC = gcc
INSTALL=install
STRIP=strip
CFLAGS = -DVERSION=\"$(VERSION)\" `pkg-config --cflags gtk+-2.0` `pkg-config --cflags xmms2-client`
LDFLAGS = `pkg-config --libs gtk+-2.0` `pkg-config --libs xmms2-client` `pkg-config --libs xmms2-client-glib`
OBJS = lxmusic.o
BIN = lxmusic

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $(BIN) $(OBJS)

lxmusic.o: lxmusic.c
	$(CC) -c $(CFLAGS) -o lxmusic.o lxmusic.c

install: $(BIN)
	$(INSTALL) -t $(DESTDIR)/usr/bin $(BIN)
	$(STRIP) $(DESTDIR)/$(BIN)

clean:
	rm -f $(BIN) $(OBJS) *~

dist:
	mkdir lxmusic-$(VERSION)
	cp *.c Makefile README INSTALL COPYING AUTHORS lxmusic-$(VERSION)
	tar --bzip2 -cf lxmusic-$(VERSION).tar.bz2 lxmusic-$(VERSION)
	rm -rf lxmusic-$(VERSION)
