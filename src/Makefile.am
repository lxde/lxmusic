## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@GTK_CFLAGS@ \
	@XMMSC_CFLAGS@

bin_PROGRAMS = lxmusic

lxmusic_SOURCES = \
	lxmusic.c

lxmusic_LDADD = @GTK_LIBS@ @XMMSC_LIBS@ $(INTLLIBS)

