## Process this file with automake to produce Makefile.in

SUBDIRS = src po

desktop_DATA=data/lxmusic.desktop
desktopdir=$(datadir)/applications

icon_DATA=data/lxmusic.png
icondir=$(datadir)/icons/hicolor/128x128/apps

uidir=$(datadir)/lxmusic
# GtkBuilder UI definition files
ui_in_files= \
	data/lxmusic.glade
ui_DATA = $(ui_in_files:.glade=.ui)

EXTRA_DIST = \
	autogen.sh \
	$(desktop_DATA) \
	$(icon_DATA) \
	$(ui_DATA)

CLEANFILES = \
	$(ui_DATA) 

# Generate GtkBuilder UI files from Glade files
%.ui: %.glade
	$(GTK_BUILDER_CONVERT) $< $@
#	$(top_builddir)/src/xml-purge $@

