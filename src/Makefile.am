## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@LXMUSIC_CFLAGS@ 

bin_PROGRAMS = lxmusic

lxmusic_SOURCES = \
	lxmusic.c \
	utils.c utils.h

lxmusic_LDADD = @LXMUSIC_LIBS@ $(INTLLIBS)

