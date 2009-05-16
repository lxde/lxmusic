desktop_DATA=lxmusic.desktop
desktopdir=$(datadir)/applications

icon_DATA=lxmusic.png
icondir=$(datadir)/pixmaps

uidir=$(datadir)/lxmusic
# GtkBuilder UI definition files
ui_in_files= \
	lxmusic.glade \
	pref-dlg.glade \
	track-info.glade \
	popup.glade
ui_DATA = $(ui_in_files:.glade=.ui.glade)

EXTRA_DIST = \
	$(desktop_DATA) \
	$(icon_DATA) \
	$(ui_DATA)

CLEANFILES = \
	$(ui_DATA) 

# Generate GtkBuilder UI files from Glade files
%.ui.glade: %.glade
	$(top_srcdir)/gtk-builder-convert $< $@
#	$(top_builddir)/src/xml-purge $@

