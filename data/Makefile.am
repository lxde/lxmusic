desktop_in_files=lxmusic.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
desktopdir=$(datadir)/applications

icon_DATA=lxmusic.png
icondir=$(datadir)/pixmaps

uidir=$(datadir)/lxmusic
# GtkBuilder UI definition files
ui_DATA= \
	lxmusic.ui.glade \
	pref-dlg.ui.glade \
	track-info.ui.glade \
	popup.ui.glade

EXTRA_DIST = \
	$(desktop_DATA) \
	$(icon_DATA) \
	$(ui_DATA)


