## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@LIBNOTIFY_LIBS@ \
	@LXMUSIC_CFLAGS@ 

bin_PROGRAMS = lxmusic

libnotify_sources = lxmusic-notify.c lxmusic-notify.h

lxmusic_SOURCES = \
	lxmusic.c \
	lxmusic-plugin-config.c \
	lxmusic-plugin-config.h \
	$(libnotify_sources) \
	utils.c utils.h

lxmusic_LDADD = @LXMUSIC_LIBS@ 	@LIBNOTIFY_LIBS@  $(INTLLIBS)

