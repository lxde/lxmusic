/*
 *      lxmusic.c
 *      
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *      Copyright 2009 Jürgen Hötzel <juergen@archlinux.org>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

/* for status icon */
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_LIBNOTIFY
#include "lxmusic-notify.h"
#endif

#include "lxmusic-plugin-config.h"
#include "utils.h"

enum {
    COL_ID = 0,
    COL_ARTIST,
    COL_ALBUM,
    COL_TITLE,
    COL_LEN,
    COL_WEIGHT, /* font weight, used to show bold font for current track. */
    N_COLS
};

enum {
    REPEAT_NONE,
    REPEAT_CURRENT,
    REPEAT_ALL
};

enum {
    FILTER_ALL,
    FILTER_ARTIST,
    FILTER_ALBUM,
    FILTER_TITLE,
};

typedef struct _UpdateTrack{
    uint32_t id;
    GtkTreeIter it;
}UpdateTrack;

typedef struct _TrackProperties{
    const char *artist;
    const char *album;
    const char *title;
    const char *url;
    const char *mime;
    const char *comment;
    int32_t duration;
    int32_t isvbr;
    int32_t bitrate;
    int32_t size;
}TrackProperties;

static void 	send_notifcation			( const gchar *artist, const gchar* title );

#ifdef HAVE_LIBNOTIFY
static LXMusic_Notification *lxmusic_notification = NULL;
#endif 

static xmmsc_connection_t *con = NULL;
static GtkWidget *main_win = NULL;
static GtkWidget *tray_icon = NULL;
static GtkWidget *play_btn = NULL;
static GtkWidget *time_label = NULL;
static GtkWidget *progress_bar = NULL;
static GtkWidget *status_bar = NULL;
static GtkWidget *notebook = NULL;
static GtkWidget *volume_btn = NULL;
static GtkWidget *inner_vbox = NULL;
static GtkWidget *playlist_view = NULL;
static GtkWidget *repeat_mode_cb = NULL;

static GtkWidget *switch_pl_menu = NULL;
static GSList* switch_pl_menu_group = NULL;

static GtkWidget *add_to_pl_menu = NULL;
static GtkWidget *rm_from_pl_menu = NULL;

static char* cur_playlist = NULL;
static GSList* all_playlists = NULL;

/* to be updated by update_track */
static GHashTable *update_tracks;

static GtkListStore* list_store = NULL;

static int32_t playback_status = 0;
static uint32_t play_time = 0;
static int32_t cur_track_duration = 0;
static int32_t cur_track_id = 0;

static int repeat_mode = REPEAT_NONE;

static char* filter_keyword = NULL;
static uint32_t filter_timeout = 0;

/* config values */
static gboolean show_tray_icon = TRUE;
static gboolean show_playlist = TRUE;
static gboolean close_to_tray = TRUE;
static gboolean play_after_exit = FALSE;

static int filter_field = FILTER_ALL;

/* window size */
static int win_width = 480;
static int win_height = 320;

/* window position */
static int win_xpos = 0;
static int win_ypos = 0;

void 			on_locate_cur_track	(GtkAction* act, gpointer user_data);
void 			on_play_btn_clicked	(GtkButton* btn, gpointer user_data);

static GtkTreeIter	get_current_track_iter	();
static gboolean		get_track_properties 	(xmmsv_t *value, TrackProperties *properties);

static void load_config()
{
    char* path = g_build_filename(g_get_user_config_dir(), "lxmusic", "config", NULL );
    GKeyFile* kf = g_key_file_new();
    if( g_key_file_load_from_file(kf, path, 0, NULL) )
    {
        int v;
        const char grp[] = "Main";
        v = g_key_file_get_integer(kf, grp, "width", NULL);
        if( v > 0 )
            win_width = v;
        v = g_key_file_get_integer(kf, grp, "height", NULL);
        if( v > 0 )
            win_height = v;
		win_xpos = g_key_file_get_integer(kf, grp, "xpos", NULL);
		win_ypos = g_key_file_get_integer(kf, grp, "ypos", NULL);
        kf_get_bool(kf, grp, "show_tray_icon", &show_tray_icon);
        kf_get_bool(kf, grp, "show_playlist", &show_playlist);
        kf_get_bool(kf, grp, "close_to_tray", &close_to_tray);
        kf_get_bool(kf, grp, "play_after_exit", &play_after_exit);
        kf_get_int(kf, grp, "filter", &filter_field);
    }
    g_free(path);
    g_key_file_free(kf);
}

static void save_config()
{
    FILE* f;
    char* dir = g_build_filename(g_get_user_config_dir(), "lxmusic", NULL);
    char* path = g_build_filename(dir, "config", NULL );

    g_mkdir_with_parents(dir, 0700);
    g_free( dir );

    f = fopen(path, "w");
    g_free(path);
    if( f )
    {
        fprintf(f, "[Main]\n");
        fprintf( f, "width=%d\n", win_width );
        fprintf( f, "height=%d\n", win_height );
        fprintf( f, "xpos=%d\n", win_xpos );
        fprintf( f, "ypos=%d\n", win_ypos );
        fprintf( f, "show_tray_icon=%d\n", show_tray_icon );
        fprintf( f, "close_to_tray=%d\n", close_to_tray );
        fprintf( f, "play_after_exit=%d\n", play_after_exit );
        fprintf( f, "show_playlist=%d\n", show_playlist );
        fprintf( f, "filter=%d\n", filter_field );
        fclose(f);
    }
}

static void cancel_pending_update_tracks()
{
    g_hash_table_remove_all( update_tracks );
}

static int on_xmms_quit(xmmsv_t *value, void *user_data)
{
    gtk_widget_destroy(main_win);
    gtk_main_quit();
    return TRUE;
}

void on_quit(GtkAction* act, gpointer user_data)
{
    cancel_pending_update_tracks();

    if( show_playlist )
        gtk_window_get_size(GTK_WINDOW(main_win), &win_width, &win_height);

    if(tray_icon)
        g_object_unref(tray_icon);

#ifdef HAVE_LIBNOTIFY
    lxmusic_notification_free( lxmusic_notification );
#endif 

    if( ! play_after_exit )
    {
        /* quit the server */
        xmmsc_result_t* res = xmmsc_quit(con);
        xmmsc_result_notifier_set_and_unref(res, on_xmms_quit, NULL);
    }
    else
    {
        gtk_widget_destroy(main_win);
        gtk_main_quit();
    }
}

gboolean on_main_win_delete_event(GtkWidget* win, GdkEvent* evt, gpointer user_data)
{
    if( close_to_tray )
    {
        gtk_widget_hide( win );
        return TRUE;
    }
    else
        on_quit(NULL, NULL);
    return FALSE;
}

void on_main_win_destroy(GtkWidget* win)
{
    if( filter_timeout )
    {
        g_source_remove(filter_timeout);
        filter_timeout = 0;
    }
}

static void open_url(GtkAboutDialog* dlg, const char* url, gpointer user_data)
{
    const char* argv[] = {"xdg-open", NULL, NULL};
    argv[1] = url;
    g_spawn_async("/", (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

void on_about(GtkWidget* mi, gpointer data)
{
    const char* authors[] = { "洪任諭 (Hong Jen Yee) <pcman.tw@gmail.com>", 
			      "Jürgen Hötzel <juergen@archlinux.org>", NULL };
    const char* artists[] = { N_("Official icon of xmms2 by Arnaud DIDRY"), NULL };
    GtkWidget* about;

    gtk_about_dialog_set_url_hook(open_url, NULL, NULL);

    about = gtk_about_dialog_new();
    gtk_about_dialog_set_name( (GtkAboutDialog*)about, "LXMusic" );
    gtk_about_dialog_set_logo_icon_name((GtkAboutDialog*)about, "lxmusic");
    gtk_about_dialog_set_version( (GtkAboutDialog*)about, VERSION );
    gtk_about_dialog_set_authors( (GtkAboutDialog*)about, authors );
    gtk_about_dialog_set_artists( (GtkAboutDialog*)about, artists );
    gtk_about_dialog_set_comments( (GtkAboutDialog*)about, _("Music Player for LXDE\nSimple GUI XMMS2 client") );
    gtk_about_dialog_set_license( (GtkAboutDialog*)about, "GNU General Public License" );
    gtk_about_dialog_set_website( (GtkAboutDialog*)about, "http://lxde.org/" );
    gtk_window_set_transient_for( (GtkWindow*)about, (GtkWindow*)main_win );
    gtk_dialog_run( (GtkDialog*)about );
    gtk_widget_destroy( about );
}

void on_pref_output_plugin_changed(GtkTable* table, GtkComboBox* output)
{
    int active = gtk_combo_box_get_active(output);
    Plugin *plugin;
    int row, n_configs;
    /* plugin name translated -> only display translated plugin config
     * names  */
    gboolean display_only_known_config = false;

    if (active == - 1) 
	return;
    
    plugin = plugin_nth( active );
    
    if ( plugin_config_gettext( plugin->name ) != NULL )
	display_only_known_config = true;
    
    n_configs = g_list_length( plugin->config );

    /* destroy previous configuration widgets */
    gtk_container_foreach( GTK_CONTAINER( table ), (GtkCallback) gtk_widget_destroy, NULL );

    if ( n_configs == 0 )
	return;

    gtk_table_resize( table, 1, 2 );
    for ( row = 0; row < n_configs; row++ ) 
    {
	xmmsc_result_t *res;
	PluginConfig *config = plugin_config_nth ( plugin , row );
	const gchar *label_text = plugin_config_gettext( config->name );
	GtkWidget *label;
	
	if (label_text == NULL && display_only_known_config) 
	    continue;
	
	if (label_text == NULL)
	    label_text = config->name;

	label = gtk_label_new( label_text );

	config->entry = gtk_entry_new();
	gtk_misc_set_alignment( GTK_MISC(label), 0, 0.5 );
	gtk_table_attach( table, label, 0, 1, row, row + 1, 0, 0, 0, 0 );
	gtk_table_attach_defaults( table, config->entry, 1, 2, row, row + 1 );

	/* update all posible configuration values */
	res = xmmsc_configval_get(con, config->name);
	xmmsc_result_notifier_set_and_unref(res, plugin_config_widget, config );
    }
    
    gtk_widget_show_all( GTK_WIDGET( table ) );
}


static int on_pref_dlg_init_widget(xmmsv_t* value, void* user_data)
{
    GtkWidget* w = (GtkWidget*)user_data;
    char* val;
    if( xmmsv_get_string(value, (const char**)&val) )
    {
        /* g_debug("val = %s, w is a %s", val, G_OBJECT_TYPE_NAME(w)); */
        if( GTK_IS_SPIN_BUTTON(w) )
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), atoi(val));
        else if( GTK_IS_ENTRY(w) )
            gtk_entry_set_text(GTK_ENTRY(w), val);
        else if( GTK_IS_COMBO_BOX_ENTRY(w) )
            gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w))), val);
        else
            g_debug("%s is not supported", G_OBJECT_TYPE_NAME(w));
    }
    return FALSE;
}

int on_pref_dlg_init_output_plugin(xmmsv_t* value, void* user_data)
{
    GtkComboBox *output_plugin_cb = GTK_COMBO_BOX( user_data );
    char* selected_plugin_name;
    int i;
    
    /* setup output plugin combobox */
    GtkCellRenderer *cell;
    GtkListStore *store;
    
    store = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_combo_box_set_model(output_plugin_cb, GTK_TREE_MODEL (store) );
    g_object_unref (store);
    
    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (output_plugin_cb), cell, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (output_plugin_cb), cell, "text", 0, NULL);

    for ( i = 0; i < g_list_length( plugin_list() ); i++ ) 
    {
	const gchar *xmms2_name =  plugin_nth( i )->name;
	const gchar *label = plugin_config_gettext( xmms2_name );
	/* fallback to xmms2 plugin names if no translation available*/
	if ( label == NULL )
	    label = xmms2_name;
	gtk_combo_box_append_text( output_plugin_cb, label );
    }
    

    if( xmmsv_get_string(value, (const char**)&selected_plugin_name) )
	for ( i = 0; i < g_list_length( plugin_list()  ); i++ ) 
	{
	    Plugin *plugin = plugin_nth( i );
	    if ( g_strcmp0( plugin->name, selected_plugin_name ) == 0 )
		gtk_combo_box_set_active( output_plugin_cb, i );
    }
    return FALSE;
}

static void on_tray_icon_activate(GtkStatusIcon* icon, gpointer user_data)
{
    /* FIXME: should we unload the playlist to free resources here? */
    if( GTK_WIDGET_VISIBLE(main_win) )
    {
		/* save window position before we hide the window */
		gtk_window_get_position((GtkWindow*)main_win, &win_xpos, &win_ypos);
		save_config();
		gtk_widget_hide(main_win);
	}
    else
	{
        /* restore the window position */
		gtk_window_move((GtkWindow*)main_win, win_xpos, win_ypos);
		gtk_widget_show(main_win);
	}
}

void on_show_main_win(GtkAction* act, gpointer user_data)
{
	gtk_window_present(GTK_WINDOW(main_win));
}

static void on_tray_icon_popup_menu(GtkStatusIcon* icon, guint btn, guint time, gpointer user_data)
{
	/* init tray icon widgets */
	GtkBuilder *builder = gtk_builder_new ();
	if(gtk_builder_add_from_file(builder, PACKAGE_DATA_DIR "/lxmusic/popup.ui.glade", NULL))
	{
        	GtkWidget *tray_play_btn = (GtkWidget*)gtk_builder_get_object(builder, "play");
		GtkWidget *tray_popup = (GtkWidget*)gtk_builder_get_object(builder, "popup");
        	gtk_builder_connect_signals(builder, NULL);
		switch (playback_status)
		{
			case XMMS_PLAYBACK_STATUS_PLAY:
				g_object_set ( (GObject*)tray_play_btn, "stock-id", "gtk-media-pause", NULL);
				break;
			case XMMS_PLAYBACK_STATUS_PAUSE:
			case XMMS_PLAYBACK_STATUS_STOP:
				g_object_set ( (GObject*)tray_play_btn, "stock-id", "gtk-media-play", NULL);
				break;
		}
		gtk_menu_popup((GtkMenu*)tray_popup, NULL, NULL, NULL, NULL, btn, time);
	}
	g_object_unref(builder);

	return;
}

static void create_tray_icon()
{
    tray_icon = (GtkWidget*)gtk_status_icon_new_from_icon_name("lxmusic");
    gtk_status_icon_set_tooltip(GTK_STATUS_ICON(tray_icon), _("LXMusic"));
    g_signal_connect(tray_icon, "activate", G_CALLBACK(on_tray_icon_activate), NULL );
    g_signal_connect(tray_icon, "popup-menu", G_CALLBACK(on_tray_icon_popup_menu), NULL );
}

void on_preference(GtkAction* act, gpointer data)
{
    GtkBuilder* builder = gtk_builder_new();
    
    if( gtk_builder_add_from_file(builder, PACKAGE_DATA_DIR "/lxmusic/pref-dlg.ui.glade", NULL ) )
    {
        xmmsc_result_t* res;
        GtkWidget* show_tray_icon_btn = (GtkWidget*)gtk_builder_get_object(builder, "show_tray_icon");
        GtkWidget* close_to_tray_btn = (GtkWidget*)gtk_builder_get_object(builder, "close_to_tray");
        GtkWidget* play_after_exit_btn = (GtkWidget*)gtk_builder_get_object(builder, "play_after_exit");
        GtkWidget* output_plugin_cb = (GtkWidget*)gtk_builder_get_object(builder, "output_plugin_cb");
        GtkWidget* output_bufsize = (GtkWidget*)gtk_builder_get_object(builder, "output_bufsize");
        GtkWidget* cdrom = (GtkWidget*)gtk_builder_get_object(builder, "cdrom");
        GtkWidget* id3v1_encoding = (GtkWidget*)gtk_builder_get_object(builder, "id3v1_encoding");
        GtkWidget* dlg = (GtkWidget*)gtk_builder_get_object(builder, "pref_dlg");

        gtk_builder_connect_signals(builder, NULL);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_tray_icon_btn), show_tray_icon);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(close_to_tray_btn), close_to_tray);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_after_exit_btn), play_after_exit);

        res = xmmsc_configval_get(con, "output.plugin");
        xmmsc_result_notifier_set_full(res, on_pref_dlg_init_output_plugin, g_object_ref(output_plugin_cb), g_object_unref );
        xmmsc_result_unref(res);

        res = xmmsc_configval_get(con, "output.buffersize");
        xmmsc_result_notifier_set_full(res, on_pref_dlg_init_widget, g_object_ref(output_bufsize), g_object_unref );
        xmmsc_result_unref(res);

        res = xmmsc_configval_get(con, "cdda.device");
        xmmsc_result_notifier_set_full(res, on_pref_dlg_init_widget, g_object_ref(cdrom), g_object_unref );
        xmmsc_result_unref(res);

        res = xmmsc_configval_get(con, "mad.id3v1_encoding");
        xmmsc_result_notifier_set_full(res, on_pref_dlg_init_widget, g_object_ref(id3v1_encoding), g_object_unref );
        xmmsc_result_unref(res);

        if( gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK )
        {
            int i;
            char str[32];
	    Plugin *plugin;

            i = gtk_combo_box_get_active(GTK_COMBO_BOX(output_plugin_cb));
	    plugin = plugin_nth( i );
	    
            res = xmmsc_configval_set( con, "output.plugin", plugin->name );
            xmmsc_result_unref(res);
	    
            g_snprintf(str, 32, "%u",(uint32_t)gtk_spin_button_get_value(GTK_SPIN_BUTTON(output_bufsize)));
            res = xmmsc_configval_set( con, "output.buffersize", str );
            xmmsc_result_unref(res);
	    
	    /* update plugin configuration */
	    for ( i = 0; i < g_list_length( plugin->config ); i++ ) 
	    {
		PluginConfig *config = plugin_config_nth( plugin, i );
		if ( config->value != NULL && g_strcmp0( config->value, gtk_entry_get_text( GTK_ENTRY(config->entry) ) ) != 0 )
		{
		    const gchar *newval = gtk_entry_get_text( GTK_ENTRY(config->entry ) );
		    res = xmmsc_configval_set( con, config->name, newval );
		    xmmsc_result_unref(res);		
		}
	    }

            res = xmmsc_configval_set( con, "cdda.device", gtk_entry_get_text(GTK_ENTRY(cdrom)) );
            xmmsc_result_unref(res);

            res = xmmsc_configval_set( con, "mad.id3v1_encoding", gtk_entry_get_text(GTK_ENTRY(id3v1_encoding)) );
            xmmsc_result_unref(res);

            res = xmmsc_configval_set( con, "mpg123.id3v1_encoding", gtk_entry_get_text(GTK_ENTRY(id3v1_encoding)) );
            xmmsc_result_unref(res);

            show_tray_icon = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_tray_icon_btn));
            close_to_tray = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(close_to_tray_btn));
            play_after_exit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_after_exit_btn));

            if( show_tray_icon )
            {
                if( ! tray_icon )
                    create_tray_icon();
            }
            else
            {
                if( tray_icon )
                {
                    g_object_unref(tray_icon);
                    tray_icon = NULL;
                }
            }
        }
        gtk_widget_destroy(dlg);
    }
    g_object_unref(builder);
}

static int on_track_info_received(xmmsv_t* value, void* user_data)
{
    GtkBuilder* builder = (GtkBuilder*)user_data;
    TrackProperties tp;
    GtkWidget* w;

    get_track_properties( value, &tp );
    if (tp.url && (!g_str_equal(tp.url, "" )))
    {
        w = (GtkWidget*)gtk_builder_get_object(builder, "url");
        if( g_str_has_prefix(tp.url, "file://") )
        {
            char* disp;
	    gchar *decoded_val = g_uri_unescape_string( tp.url, NULL );
	    /* skip file:// */
	    disp = g_filename_display_name( decoded_val + 7 ) ;
	    g_free( decoded_val );
            gtk_entry_set_text(GTK_ENTRY(w), disp);
            g_free(disp);
        }
        else
            gtk_entry_set_text(GTK_ENTRY(w), tp.url);
    }
    if (tp.title || tp.album || tp.artist || tp.comment || tp.mime) 
    {
	const char **key;
	const char *val;
	const char *key_values[] = {
	    "title", tp.title, 
	    "album", tp.album,
	    "artist", tp.artist,
	    "comment", tp.comment,
	    "mime", tp.mime, 
	    NULL
	};

	for ( key=key_values; *key; key += 2 ) 
	{
	    /* check if string_property is non-empty */
	    if (!(val = *(key + 1)))
		continue;
	    w = (GtkWidget*)gtk_builder_get_object(builder, *key);
	    if( GTK_IS_ENTRY(w) )
		gtk_entry_set_text((GtkEntry*)w, val);
	    else if( GTK_IS_LABEL(w) )
		gtk_label_set_text((GtkLabel*)w, val);
	}
    }
    
    /* size & bitrate */
    if (tp.size){
	char buf[100];
	char *size_str;
	size_str = g_format_size_for_display( tp.size );
	strcpy( buf, size_str );
	g_free( size_str );

	if (tp.bitrate) 
	{
	    int len = strlen(buf);
	    g_snprintf(buf + len, 100 - len, " (%s%d Kbps%s)", _("Bitrate: "), tp.bitrate/1000, tp.isvbr ? ", vbr" : "" );
	}
	w = (GtkWidget*)gtk_builder_get_object(builder, "size");
	gtk_label_set_text((GtkLabel*)w, buf);
    }
    xmmsv_unref( value );
    return TRUE;
}

void on_file_properties(GtkAction* act, gpointer data)
{
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
    GtkTreeIter it;
    GtkTreeModel* model;
    GList* rows;
    if( ( rows = gtk_tree_selection_get_selected_rows(sel, &model) ) != NULL )
    {
        GtkTreePath* tp = (GtkTreePath*)rows->data;
        if( gtk_tree_model_get_iter(model, &it, tp) )
        {
            GtkBuilder* builder = gtk_builder_new();
            uint32_t id;
            gtk_tree_model_get( model, &it, COL_ID, &id, -1 );
            if( gtk_builder_add_from_file(builder, PACKAGE_DATA_DIR "/lxmusic/track-info.ui.glade", NULL ) )
            {
                xmmsc_result_t* res;
                GtkWidget* dlg = (GtkWidget*)gtk_builder_get_object(builder, "track_info_dlg");
                gtk_builder_connect_signals(builder, NULL);

                res = xmmsc_medialib_get_info(con, id);
                xmmsc_result_notifier_set_full(res, on_track_info_received, g_object_ref(builder), g_object_unref);
                xmmsc_result_unref(res);

                gtk_window_present(GTK_WINDOW(dlg));
            }
            g_object_unref(builder);
        }
        g_list_free(rows);
    }
}

void on_playlist_view_row_activated(GtkTreeView* view,
                              GtkTreePath* path,
                              GtkTreeViewColumn* col,
                              gpointer user_data)
{
    xmmsc_result_t* res;
    GtkTreeModelFilter* filter = (GtkTreeModelFilter*)gtk_tree_view_get_model(view);
    /* convert from model filter path to path of underlying liststore */
    path = gtk_tree_model_filter_convert_path_to_child_path(filter, path);
    if( path )
    {
        uint32_t pos = gtk_tree_path_get_indices(path)[0];
        /* FIXME: need to swtich to another playlist sometimes. */
        res = xmmsc_playlist_set_next( con, pos );
        xmmsc_result_unref(res);

        res = xmmsc_playback_tickle(con);
        xmmsc_result_unref(res);

        /* FIXME: just call play is not enough? */
        if( playback_status != XMMS_PLAYBACK_STATUS_PLAY )
            on_play_btn_clicked((GtkButton*)play_btn, NULL);
        gtk_tree_path_free(path);
    }
}

void on_playlist_view_drag_data_received(GtkWidget          *widget,
                                         GdkDragContext     *drag_ctx,
                                         gint                x,
                                         gint                y,
                                         GtkSelectionData   *data,
                                         uint32_t               info,
                                         uint32_t               time)
{
    char** uris;
    GtkTreePath            *dest_path;
    GtkTreeViewDropPosition pos;
    int insert_pos = gtk_tree_model_iter_n_children( gtk_tree_view_get_model ( GTK_TREE_VIEW(playlist_view ) ), NULL );

    g_signal_stop_emission_by_name(playlist_view, "drag_data_received");
    
    if ( gtk_tree_view_get_dest_row_at_pos( GTK_TREE_VIEW(playlist_view), x, y, &dest_path, &pos ) ) 
    {
	insert_pos = gtk_tree_path_get_indices( dest_path )[0] ;
	if ( pos != GTK_TREE_VIEW_DROP_BEFORE )
	    insert_pos++;
    }

    if( (uris = gtk_selection_data_get_uris(data)) != NULL )
    {
        char** uri;
        for( uri = uris; *uri; ++uri )
        {
            xmmsc_result_t* res;
            if( g_str_has_prefix(*uri, "file://") )
            {
                char* fn = g_filename_from_uri(*uri, NULL, NULL);
                if(fn)
                {
                    /* xmms2 doesn't use standard URL, but instead uses
                     * its own non-standard, weird url which only adds a prefix 
                     * 'file://' to the original file path without URL encoding.
                     */
                    char* url = g_strconcat( "file://", fn, NULL);
                    if(g_file_test(fn, G_FILE_TEST_IS_DIR))
                        res = xmmsc_playlist_rinsert(con, cur_playlist, insert_pos, url);
                    else
                        res = xmmsc_playlist_insert_url(con, cur_playlist, insert_pos, url);
                    g_free(fn);
                    g_free(url);
                }
                else
                    res = xmmsc_playlist_rinsert_encoded(con, cur_playlist, insert_pos, *uri);
            }
            else
                res = xmmsc_playlist_rinsert_encoded(con, cur_playlist, insert_pos, *uri);

            xmmsc_result_unref(res);
        }
        g_strfreev(uris);
    }
}

gboolean on_playlist_view_drag_drop(GtkWidget      *widget,
                                    GdkDragContext *drag_ctx,
                                    gint            x,
                                    gint            y,
                                    uint32_t           time,
                                    gpointer        user_data)
{
    GdkAtom target = gdk_atom_intern_static_string("text/uri-list");

    /*  Don't call the default handler  */
//    g_signal_stop_emission_by_name( widget, "drag-drop" );

    if( g_list_find( drag_ctx->targets, target ) )
    {
        gtk_drag_get_data( widget, drag_ctx, target, time );
        return TRUE;
    }
    else
    {
    }
    gtk_drag_finish(drag_ctx, TRUE, FALSE, time);
    return TRUE;
}

static void refilter_and_keep_sel_visible()
{
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist_view));
    GtkTreeModelFilter* mf;
    GList* sels, *l;
    GtkTreePath* tp;

    /* save paths of selected rows */
    sels = gtk_tree_selection_get_selected_rows(sel, (GtkTreeModel**)&mf);

    /* convert to child paths */
    for( l = sels; l; l = l->next )
    {
        tp = gtk_tree_model_filter_convert_path_to_child_path(mf, (GtkTreePath*)l->data);
        gtk_tree_path_free((GtkTreePath*)l->data);
        l->data = tp;
    }

    /* refilter */
    gtk_tree_model_filter_refilter(mf);

    /* scroll to selected rows */
    for( l = sels; l; l = l->next )
    {
        tp = gtk_tree_model_filter_convert_child_path_to_path( mf, (GtkTreePath*)l->data );
        if( tp )
        {
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(playlist_view), tp, NULL, FALSE, 0.0, 0.0 );
            gtk_tree_path_free(tp);
            break;
        }
    }
    g_list_foreach(sels, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(sels);
}

void on_filter_field_changed(GtkComboBox* cb, gpointer user_data)
{
    filter_field = gtk_combo_box_get_active(cb);
    refilter_and_keep_sel_visible();
}

static gboolean playlist_compare_func( GtkTreeModel* model, GtkTreeIter* it,
				       const gchar *needle ) 
{
    gboolean ret = FALSE;
    char *artist=NULL, *album=NULL, *title=NULL;

    if( ! needle || '\0' == *needle )
        return TRUE;    

    if( filter_field == FILTER_ALL || filter_field == FILTER_ARTIST )
        gtk_tree_model_get(model, it,
                           COL_ARTIST, &artist, -1);

    if( filter_field == FILTER_ALL || filter_field == FILTER_ALBUM )
        gtk_tree_model_get(model, it,
                           COL_ALBUM, &album, -1);

    if( filter_field == FILTER_ALL || filter_field == FILTER_TITLE )
        gtk_tree_model_get(model, it,
                           COL_TITLE, &title, -1);


    if( artist && utf8_strcasestr( artist, needle ) )
        ret = TRUE;
    else if( album && utf8_strcasestr( album, needle ) )
        ret = TRUE;
    else if( title && utf8_strcasestr( title, needle ) )
        ret = TRUE;

    g_free(artist);
    g_free(album);
    g_free(title);

    return ret;
}

static gboolean playlist_filter_func(GtkTreeModel* model, GtkTreeIter* it, gpointer user_data)
{
    return playlist_compare_func (model, it, filter_keyword );
    
}

static gboolean playlist_search_func( GtkTreeModel* model, gint column,
				      const gchar *key, GtkTreeIter *it,
				      gpointer user_data)  
{
    /* from GTK+ docs: 
       
       Note the return value is reversed from what you would normally
       expect, though it has some similarity to strcmp() returning 0 for
       equal strings.
    */
    return ! playlist_compare_func( model, it, key );
}

static gboolean on_filter_timeout(GtkEntry* entry)
{
    g_free(filter_keyword);
    filter_keyword = g_strdup(gtk_entry_get_text(entry));
 //   gtk_tree_model_filter_refilter(filter);

    refilter_and_keep_sel_visible();

    filter_timeout = 0;
    return FALSE;
}

void on_filter_entry_changed(GtkEntry* entry, gpointer user_data)
{
    if( filter_timeout )
        g_source_remove(filter_timeout);
    filter_timeout = g_timeout_add( 600, (GSourceFunc)on_filter_timeout, entry );
}

static gboolean file_filter_fnuc(const GtkFileFilterInfo *inf, gpointer user_data)
{
    return g_str_has_prefix(inf->mime_type, "audio/");
}

static gpointer add_file( const char* file )
{
    gboolean is_dir = g_file_test( file, G_FILE_TEST_IS_DIR );
    xmmsc_result_t *res;
    char *url;

    /* Since xmms2 uses its own url format, this is annoying but inevitable. */
    url = g_strconcat( "file://", file, NULL );

    if( is_dir )
        res = xmmsc_playlist_radd( con, cur_playlist, url );
    else
        res = xmmsc_playlist_add_url( con, cur_playlist, url );
    g_free( url );

    if( res )
        xmmsc_result_unref( res );
    return NULL;
}

void on_add_files( GtkMenuItem* item, gpointer user_data )
{
    enum { RESPONSE_ADD = 1 };
    GtkWidget *dlg = gtk_file_chooser_dialog_new( NULL, (GtkWindow*)main_win,
                                                  GTK_FILE_CHOOSER_ACTION_OPEN,
                                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                  GTK_STOCK_ADD, RESPONSE_ADD, NULL );
    GtkFileFilter* filter;

    gtk_file_chooser_set_select_multiple( (GtkFileChooser*)dlg, TRUE );

    /* add a custom filter which filters autio files */
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Audio Files"));
    gtk_file_filter_add_custom( filter, GTK_FILE_FILTER_MIME_TYPE, file_filter_fnuc, NULL, NULL );
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("All Files"));
    gtk_file_filter_add_custom(filter, 0, (GtkFileFilterFunc)gtk_true, NULL, NULL );
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), filter);

    if( gtk_dialog_run( (GtkDialog*)dlg ) == RESPONSE_ADD )
    {
        GSList* uris = gtk_file_chooser_get_uris( (GtkFileChooser*)dlg );
        GSList* uri;

        if( ! uris )
            return;

        for( uri = uris; uri; uri = uri->next )
        {
            gchar* file = g_filename_from_uri( uri->data, NULL, NULL );
            add_file( file );
            g_free( file );
            g_free( uri->data );
        }
        g_slist_free( uris );
    }
    gtk_widget_destroy( dlg );
}

void on_add_url( GtkMenuItem* item, gpointer user_data )
{
    GtkWidget *dlg = gtk_dialog_new_with_buttons(
            _("Input a URL"), (GtkWindow*)main_win, GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );
    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start( (GtkBox*)((GtkDialog*)dlg)->vbox, entry, FALSE, FALSE, 4 );
    gtk_dialog_set_default_response( (GtkDialog*)dlg, GTK_RESPONSE_OK );
    gtk_entry_set_activates_default( (GtkEntry*)entry, TRUE );
    gtk_widget_show_all( dlg );
    if( gtk_dialog_run( (GtkDialog*)dlg ) == GTK_RESPONSE_OK )
    {
        xmmsc_result_t *res;
        const char* url = gtk_entry_get_text( (GtkEntry*)entry );
        res = xmmsc_playlist_add_encoded( con, cur_playlist, url );
        xmmsc_result_unref( res );
    }
    gtk_widget_destroy( dlg );
}
/* FIXME: This might be implemented in the future */
/* static void on_add_from_mlib( GtkMenuItem* item, gpointer user_data ) */
/* { */

/* } */

void on_add_btn_clicked(GtkButton* btn, gpointer user_data)
{
    gtk_menu_popup(GTK_MENU(add_to_pl_menu), NULL, NULL, NULL, NULL, 1, GDK_CURRENT_TIME );
}

static int intcmp( gconstpointer a, gconstpointer b )
{
    return (int)b - (int)a;
}

void on_remove_all(GtkAction* act, gpointer user_data)
{
    xmmsc_result_t* res;
    res = xmmsc_playlist_clear(con, cur_playlist);
    xmmsc_result_unref(res);
}

void on_remove_selected(GtkAction* act, gpointer user_data)
{
    if( cur_playlist )
    {
        GtkTreeSelection* tree_sel;
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( playlist_view ) );
        GList *sels = gtk_tree_selection_get_selected_rows( tree_sel, NULL );
        GList *sel;
        if( ! sels )
            return;

        for( sel = sels; sel; sel = sel->next )
        {
            GtkTreePath* path = (GtkTreePath*)sel->data;
            sel->data = (gpointer)gtk_tree_path_get_indices( path )[0];
            gtk_tree_path_free( path );
        }

        /*
            sort the list, and put rows with bigger indicies before those with smaller indicies.
            In this way, all indicies won't be changed during removing items.
        */
        sels = g_list_sort( sels, intcmp );

        for( sel = sels; sel; sel = sel->next )
        {
            xmmsc_result_t* res;
            int pos = (int)sel->data;
            res = xmmsc_playlist_remove_entry( con, cur_playlist, pos );
            xmmsc_result_unref( res );
        }
        g_list_free( sels );
    }
}

void on_remove_btn_clicked(GtkButton* btn, gpointer user_data)
{
    gtk_menu_popup(GTK_MENU(rm_from_pl_menu), NULL, NULL, NULL, NULL, 1, GDK_CURRENT_TIME );
}

gboolean  on_playlist_view_key_press_event (GtkWidget *widget,
					 GdkEventKey *event )
{
    switch ( event->keyval) 
    {
    case GDK_Delete:
	/* dummy values: needs cleanup  */
	on_remove_selected (NULL, NULL); 
	break;
    case GDK_Insert:
	/* dummy values: needs cleanup  */
	on_add_files( NULL, NULL );
	break;
    default:
	return FALSE;	    
    }
    return TRUE;
}

void on_repeat_mode_changed(GtkComboBox* cb, gpointer user_data)
{
    xmmsc_result_t* res;
    const char* repeat_one = "0";
    const char* repeat_all = "0";
    switch(gtk_combo_box_get_active(cb))
    {
    case REPEAT_CURRENT:
        repeat_one = "1";
        break;
    case REPEAT_ALL:
        repeat_all = "1";
        break;
    }
    res = xmmsc_configval_set(con, "playlist.repeat_all", repeat_all);
    xmmsc_result_unref(res);
    res = xmmsc_configval_set(con, "playlist.repeat_one", repeat_one);
    xmmsc_result_unref(res);
}

void on_progress_bar_changed(GtkScale* bar, gpointer user_data)
{
    xmmsc_result_t* res;
    gdouble p = gtk_range_get_value(GTK_RANGE(bar));
    uint32_t new_play_time = p * cur_track_duration / 100;
    res = xmmsc_playback_seek_ms( con, new_play_time );
    xmmsc_result_unref(res);
}

void on_prev_btn_clicked(GtkButton* btn, gpointer user_data)
{
    xmmsc_result_t* res = xmmsc_playlist_set_next_rel(con, -1);
    xmmsc_result_unref(res);
    res = xmmsc_playback_tickle(con);
    xmmsc_result_unref(res);
}

void on_next_btn_clicked(GtkButton* btn, gpointer user_data)
{
    xmmsc_result_t* res = xmmsc_playlist_set_next_rel(con, 1);
    xmmsc_result_unref(res);
    res = xmmsc_playback_tickle(con);
    xmmsc_result_unref(res);
}

static int on_playback_started( xmmsv_t* value, void* user_data )
{
#if 0
    xmmsc_result_unref(res);
    /* FIXME: this can cause some problems sometimes... */
    res = xmmsc_playlist_current_pos(con, cur_playlist);
    xmmsc_result_notifier_set_and_unref(res, on_playlist_pos_changed, NULL);
#endif
    return TRUE;
}

void on_play_btn_clicked(GtkButton* btn, gpointer user_data)
{
    xmmsc_result_t *res;
    if( playback_status == XMMS_PLAYBACK_STATUS_PLAY )
    {
        res = xmmsc_playback_pause(con);
        xmmsc_result_notifier_set_and_unref(res, on_playback_started, NULL);
    }
    else
    {
        res = xmmsc_playback_start(con);
        xmmsc_result_unref(res);
    }
}

void on_stop_btn_clicked(GtkButton* btn, gpointer user_data)
{
    xmmsc_result_t* res = xmmsc_playback_stop(con);
    xmmsc_result_unref(res);
}


static void render_num( GtkTreeViewColumn* col, GtkCellRenderer* render,
                        GtkTreeModel* model, GtkTreeIter* it, gpointer data )
{
    GtkTreePath* path = gtk_tree_model_get_path( model, it );
    char buf[16];
    if( G_UNLIKELY( ! path ) )
        return;
    g_sprintf( buf, "%d", gtk_tree_path_get_indices( path )[0] + 1 );
    gtk_tree_path_free( path );
    g_object_set( render, "text", buf, NULL );
}


static int update_track( xmmsv_t *value, UpdateTrack* ut )
{
    TrackProperties track_properties;
    gboolean current_track_updated;
    char time_buf[32];
    gchar *guessed_title = NULL;
    if( xmmsv_is_error ( value ) ) {
        return FALSE;
    }

    /* check if this update is valid: Maybe it was meanwhile canceled
     * (for example by switching a long loading playlist */
    if (!g_hash_table_lookup( update_tracks, ut )) 
	return TRUE;
    
    /* valid update */
    g_hash_table_remove( update_tracks, ut );
    
    if (!get_track_properties( value, &track_properties)) 
	track_properties.title = guessed_title = guess_title_from_url( track_properties.url );
    timeval_to_str( track_properties.duration/1000, time_buf, G_N_ELEMENTS(time_buf) );

    gtk_list_store_set( list_store, &ut->it,
                        COL_ARTIST, track_properties.artist,
                        COL_ALBUM, track_properties.album,
                        COL_TITLE, track_properties.title,
                        COL_LEN, time_buf, -1 );

    current_track_updated = ut->id == cur_track_id;
    if ( current_track_updated ) 
    {
	/* send desktop notification if current track was updated */  
	send_notifcation( track_properties.artist, track_properties.title );
	if( tray_icon ) 
	{
	    GString* tray_tooltip = create_window_title(track_properties.artist, track_properties.title, playback_status == XMMS_PLAYBACK_STATUS_PLAY);
	    gtk_status_icon_set_tooltip( GTK_STATUS_ICON(tray_icon), tray_tooltip->str );
	    g_string_free( tray_tooltip, TRUE );
	}
	
    }
    g_slice_free(UpdateTrack, ut);
    g_free( guessed_title );
    return FALSE;
}

static gboolean get_track_properties (xmmsv_t *value, TrackProperties *properties)  
{
    /* traverse the dict of dict */
    xmmsv_dict_iter_t *parent_it;

    /* fallback for title: online streams often provide channel */
    const char* channel = NULL;

    /* default values: empty */
    bzero( properties, sizeof(TrackProperties) );
    
    xmmsv_get_dict_iter (value, &parent_it);
    while (xmmsv_dict_iter_valid (parent_it ) ) 
    {
	const char *key;
	const char **val_str = NULL;
	int32_t *val_int = NULL;
	xmmsv_t *child_value;
	xmmsv_dict_iter_t *child_it;
	
	/* get child dict */
	xmmsv_dict_iter_pair (parent_it, &key, &child_value);

	/* check type of property */
	if (strcmp( key, "artist" ) == 0)
	    val_str = &(properties->artist);
	else if (strcmp( key, "album" ) == 0)
	    val_str = &(properties->album);
	else if (strcmp( key, "mime" ) == 0)
	    val_str = &(properties->mime);
	else if (strcmp( key, "comment" ) == 0)
	    val_str = &(properties->comment);
	else if (strcmp( key, "channel" ) == 0)
	    val_str = &channel;	    	    
	else if (strcmp( key, "url" ) == 0)
	    val_str = &(properties->url);	    
	else if (strcmp( key, "title" ) == 0) 
	    val_str = &(properties->title);	    
	else if (strcmp( key, "duration" ) == 0)
	    val_int = &(properties->duration);
	else if (strcmp( key, "isvbr" ) == 0)
	    val_int = &(properties->isvbr);
	else if (strcmp( key, "bitrate" ) == 0)
	    val_int = &(properties->bitrate);
	else if (strcmp( key, "size" ) == 0)
	    val_int = &(properties->size);
	
	if (xmmsv_get_dict_iter (child_value, &child_it) && 
	    xmmsv_dict_iter_valid (child_it) && (val_int || val_str) && 
	    xmmsv_dict_iter_pair (child_it, NULL, &child_value)) {

	    if (val_int != NULL) 
		xmmsv_get_int( child_value, val_int);
	    else 
		xmmsv_get_string( child_value, val_str );    
	}
	xmmsv_dict_iter_next (parent_it);
    }

    if ((properties->title == NULL) || g_str_equal( properties->title, "" )) 
    {
	if (channel == NULL)
	    return FALSE;
	else
	    properties->title = channel;
    }
    return TRUE;
}

static void queue_update_track( uint32_t id, GtkTreeIter* it )
{
    UpdateTrack* ut;
    xmmsc_result_t *res;
    ut = g_slice_new(UpdateTrack);
    ut->id = id;
    ut->it = *it;
    g_hash_table_insert( update_tracks, ut, ut );
    res = xmmsc_medialib_get_info( con, id );
    xmmsc_result_notifier_set_full( res, (xmmsc_result_notifier_t)update_track, ut, NULL );
    xmmsc_result_unref( res );
}

static int on_playlist_content_received( xmmsv_t* value, GtkWidget* list_view )
{
    GtkTreeModel* mf;
    GtkTreeIter it;
    int pl_size = xmmsv_list_get_size( value );
    int i;

    /* free prev. model filter */
    if ((mf = gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_view))))
	g_object_unref(mf);
    
    list_store = gtk_list_store_new(N_COLS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT );
    mf = gtk_tree_model_filter_new(GTK_TREE_MODEL(list_store), NULL);
    gtk_tree_model_filter_set_visible_func( GTK_TREE_MODEL_FILTER( mf ), playlist_filter_func, NULL, NULL );
    gtk_tree_view_set_search_equal_func( GTK_TREE_VIEW(playlist_view), playlist_search_func, NULL, NULL );
    g_object_unref(list_store);

    /* invalidate pending track updates */
    cancel_pending_update_tracks();
    
    for ( i = 0; i < pl_size; i++ ) 
    {
        int32_t id;
	xmmsv_t *current_value;
	xmmsc_result_t *res;
        UpdateTrack* ut = g_slice_new(UpdateTrack);

	xmmsv_list_get( value, i, &current_value );
	xmmsv_get_int( current_value, &id );
        gtk_list_store_insert_with_values ( list_store, &it, i, COL_ID, id, COL_WEIGHT, PANGO_WEIGHT_NORMAL, -1 );
	
        ut->id = id;
        ut->it = it;
	/* just insert dummy values so we can distinguish from NULL */
	g_hash_table_insert( update_tracks, ut, ut );
        res = xmmsc_medialib_get_info( con, ut->id );
        xmmsc_result_notifier_set_full( res, (xmmsc_result_notifier_t)update_track, ut, NULL );
        xmmsc_result_unref( res );
    }

    if( GTK_WIDGET_REALIZED( list_view ) )
        gdk_window_set_cursor( list_view->window, NULL );

    gtk_tree_view_set_model( GTK_TREE_VIEW(list_view), mf );

    /* select the first item */
    if( gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mf), &it) )
    {
        GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list_view));
        gtk_tree_selection_select_iter(sel, &it);
    }

    return TRUE;
}

static int on_playlist_get_active(xmmsv_t* value, void* user_data)
{
    char* name;
    if( xmmsv_get_string(value, (const char**)&name) )
    {
        g_free(cur_playlist);
        cur_playlist = g_strdup(name);
    }
    return TRUE;
}

static void update_play_list( GtkWidget* list_view )
{
    xmmsc_result_t *res;

    if( GTK_WIDGET_REALIZED( list_view ) ) {
        GdkCursor* cur;
        cur = gdk_cursor_new( GDK_WATCH );
        gdk_window_set_cursor( list_view->window, cur );
        gdk_cursor_unref( cur );
    }
    res = xmmsc_playlist_list_entries( con, cur_playlist );
    xmmsc_result_notifier_set_and_unref( res, (xmmsc_result_notifier_t)on_playlist_content_received, list_view );
}

static GtkWidget* init_playlist(GtkWidget* list_view)
{
    GtkTreeSelection* tree_sel;
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( "#", render, "weight", COL_WEIGHT, NULL );
    gtk_tree_view_column_set_cell_data_func( col, render, render_num, NULL, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW( list_view ), col );

    render = gtk_cell_renderer_text_new();
    g_object_set( render, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
    col = gtk_tree_view_column_new_with_attributes( _("Artist"), render, "text", COL_ARTIST, "weight", COL_WEIGHT, NULL );
    gtk_tree_view_column_set_fixed_width( col, 80 );
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
    gtk_tree_view_column_set_resizable( col, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW( list_view ), col );

    render = gtk_cell_renderer_text_new();
    g_object_set( render, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
    col = gtk_tree_view_column_new_with_attributes( _("Album"), render, "text", COL_ALBUM, "weight", COL_WEIGHT, NULL );
    gtk_tree_view_column_set_fixed_width( col, 100 );
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
    gtk_tree_view_column_set_resizable( col, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW( list_view ), col );

    render = gtk_cell_renderer_text_new();
    g_object_set( render, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
    col = gtk_tree_view_column_new_with_attributes( _("Title"), render, "text", COL_TITLE, "weight", COL_WEIGHT, NULL );
    gtk_tree_view_column_set_expand( col, TRUE );
    gtk_tree_view_column_set_resizable( col, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW( list_view ), col );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( _("Length"), render, "text", COL_LEN, "weight", COL_WEIGHT, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW( list_view ), col );

    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( list_view ) );
    gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_MULTIPLE );

    return list_view;
}

static void on_switch_to_playlist(GtkWidget* mi, char* pl_name)
{
    xmmsc_result_t* res;
    if( gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mi)) )
    {
        /* if there are pending requests to current playlist, cancel them */
        cancel_pending_update_tracks();

        res = xmmsc_playlist_load(con, pl_name);
        xmmsc_result_unref(res);
    }
}

static int on_playlist_pos_changed( xmmsv_t* res, void* user_data );

static int on_playlist_loaded(xmmsv_t* value, gpointer user_data)
{
    char* name;
    if( !xmmsv_is_error(value) && xmmsv_get_string(value, (const char**)&name) )
    {
        /* FIXME: is this possible? */
        if( cur_playlist && 0 == strcmp((char*)name, cur_playlist) )
            return TRUE;

        g_free(cur_playlist);
        cur_playlist = g_strdup(name);

	/* invalidate currenyly played track id. */
        cur_track_id = 0;

        /* update the menu */
        if( cur_playlist )
        {
            GSList* l;
            for( l = switch_pl_menu_group; l; l=l->next )
            {
                GtkRadioMenuItem* mi = (GtkRadioMenuItem*)l->data;
                char* pl_name = (char*)g_object_get_data(G_OBJECT(mi), "pl_name");
                if( strcmp(cur_playlist, pl_name) == 0 )
                {
                    g_signal_handlers_block_by_func(mi, on_switch_to_playlist, pl_name);
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), TRUE);
                    g_signal_handlers_unblock_by_func(mi, on_switch_to_playlist, pl_name);
                    break;
                }
            }
        }

        /* if there are pending requests, cancel them */
        cancel_pending_update_tracks();

        update_play_list( playlist_view );
    }
    return TRUE;
}

static void add_playlist_to_menu(const char* pl_name, gboolean need_sort)
{
    GtkWidget* mi = gtk_radio_menu_item_new_with_label(switch_pl_menu_group, pl_name);
    char* name = g_strdup(pl_name);
    switch_pl_menu_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(mi));
    g_object_set_data_full(G_OBJECT(mi), "pl_name", name, g_free);
    g_signal_connect( mi, "toggled", G_CALLBACK(on_switch_to_playlist), name);
    gtk_widget_show(mi);

    if( need_sort )
    {
        GSList* l;
        int i = 0;
        for(l=all_playlists; l; l = l->next, ++i)
        {
            if( g_utf8_collate(pl_name, (char*)l->data) < 0 )
                break;
        }
        gtk_menu_shell_insert(GTK_MENU_SHELL(switch_pl_menu), mi, i);
        all_playlists = g_slist_insert(all_playlists, name, i);
    }
    else
    {
        gtk_menu_shell_append(GTK_MENU_SHELL(switch_pl_menu), mi);
        all_playlists = g_slist_append(all_playlists, name );
    }

    /* toggle the menu item. This can trigger the load of the list into tree view */
    if( cur_playlist && !strcmp(pl_name, cur_playlist) )
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), TRUE);
}

static void remove_playlist_from_menu(const char* pl_name)
{
    GSList* l;
    l = g_slist_find_custom(all_playlists, pl_name, (GCompareFunc)strcmp);
    all_playlists = g_slist_delete_link(all_playlists, l);

    for( l = switch_pl_menu_group; l; l = l->next )
    {
        GtkRadioMenuItem* mi = (GtkRadioMenuItem*)l->data;
        char* name = (char*)g_object_get_data(G_OBJECT(mi), "pl_name");
        if( name && strcmp(name, pl_name)==0 )
            break;
    }
    if( l )
    {
        /* switch_pl_menu_group might be out of date here. */
        if( l == switch_pl_menu_group )
            switch_pl_menu_group = l->next;
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
}

static gint on_server_quit (xmmsv_t *val, void* user_data) 
{
    xmmsc_unref (con);
    g_warning( "Server Quit" );
    con = NULL;
    on_xmms_quit( NULL, NULL );
    return TRUE;
}

static void on_server_disconnect (void *user_data)
{
    g_warning( "Server gone" );
    on_xmms_quit( NULL, NULL );
}

static int on_playlist_created( xmmsv_t* value, void* user_data )
{
    char* name = (char*)user_data;
    if( name && name[0] && name[0] != '_' )
        add_playlist_to_menu(name, TRUE);

    if( ! cur_playlist )
    {
        /* load the default list */
	xmmsc_result_t *res = xmmsc_playlist_load(con, name);
        xmmsc_result_unref(res);
    }
    return TRUE;
}

static int on_playlists_listed( xmmsv_t* value, void* user_data )
{
    GSList* lists = NULL, *l;
    int pl_size = xmmsv_list_get_size( value );
    int i;
    
    for ( i = 0; i < pl_size; i++ ) 
    {
        const char* str = NULL;
	xmmsv_t *string_value;
	xmmsv_list_get( value, i, &string_value );
	xmmsv_get_string( string_value, &str );
        if( str && str[0] && str[0] != '_' )
            lists = g_slist_prepend(lists, (gpointer) str);
    }
    lists = g_slist_sort(lists, (GCompareFunc)g_utf8_collate);
    for(l = lists; l; l = l->next)
        add_playlist_to_menu((char*)l->data, FALSE);
    g_slist_free(lists);

    /* If there is no playlist, create one */
    if( lists == NULL )
    {
        char* name = g_strdup(_("Default"));
        xmmsc_result_t *res = xmmsc_playlist_create(con, name);
        xmmsc_result_notifier_set_full(res, on_playlist_created, name, g_free);
        xmmsc_result_unref(res);
    }

    if( cur_playlist )
        update_play_list( playlist_view );
    return TRUE;
}

static int on_playlist_content_changed( xmmsv_t* value, void* user_data )
{
    int32_t id = 0;
    int type = 0, pos = -1;
    const char* name = NULL;
    xmmsv_t *string_value, *int_value;
    
    if( xmmsv_is_error( value ) )
	return TRUE;

    xmmsv_dict_get( value, "type", &int_value );
    if( !xmmsv_get_int( int_value, &type ) )
        return TRUE;
    
    xmmsv_dict_get( value, "name", &string_value );
    if ( !xmmsv_get_string( string_value, &name ) )
	return TRUE;

    if( ! name || !cur_playlist || strcmp(name, cur_playlist) )
	return TRUE;

    /* g_debug("type=%d, name=%s", type, name); */

    if( ! list_store )
	return TRUE;
    
    switch( type )
    {
        case XMMS_PLAYLIST_CHANGED_ADD:
        case XMMS_PLAYLIST_CHANGED_INSERT:
	    if ( ! xmmsv_dict_get( value, "position", &int_value ) ) 
		pos = gtk_tree_model_iter_n_children( (GtkTreeModel*)list_store, NULL );
	    else
		xmmsv_get_int( int_value, &pos );
	    
	    if ( xmmsv_dict_get( value, "id", &int_value ) ) 
	    {
                GtkTreeIter it;
		xmmsv_get_int( int_value, &id );
                gtk_list_store_insert_with_values( list_store, &it, pos, COL_ID, id, -1 );
                /* g_debug("playlist_added: %d", id); */
                queue_update_track( id, &it );
            }
            break;
        case XMMS_PLAYLIST_CHANGED_REMOVE:
            if( xmmsv_dict_get( value, "position", &int_value ) ) {
                GtkTreePath* path;
                GtkTreeIter it;
		xmmsv_get_int( int_value, &pos );
                path = gtk_tree_path_new_from_indices( pos, -1 );
                if( gtk_tree_model_get_iter( (GtkTreeModel*)list_store, &it, path ) ) 
		{
                    gtk_list_store_remove( list_store, &it );
		    /* invalidate currently played track */
		    xmmsv_dict_get( value, "id", &int_value );
		    xmmsv_get_int( int_value, &id );
		    if ( id == cur_track_id ) 
			cur_track_id = 0;
		}
                gtk_tree_path_free( path );
	    }
            break;
        case XMMS_PLAYLIST_CHANGED_CLEAR:
        {
            gtk_list_store_clear( list_store );
	    cur_track_id = 0;
            break;
        }
        case XMMS_PLAYLIST_CHANGED_MOVE:
        {
	    /* hmm this seems not to be fully implemeted ? */
            int newpos;
	    g_assert( xmmsv_dict_get( value, "position", &int_value ) );
	    xmmsv_get_int( int_value, &pos );
	    g_assert( xmmsv_dict_get( value, "newposition", &int_value ) );	    
	    xmmsv_get_int( int_value, &newpos );
	    g_warning( "Move from %d to %d not implemented" , pos, newpos);
            break;
        }
        case XMMS_PLAYLIST_CHANGED_SORT:
        case XMMS_PLAYLIST_CHANGED_SHUFFLE:
        {
            update_play_list(playlist_view);
            break;
        }

	    
        case XMMS_PLAYLIST_CHANGED_UPDATE:
            break;
    }
    return TRUE;
}

static int on_playback_status_changed( xmmsv_t *value, void *user_data )
{
    GtkWidget* img;
    if ( !xmmsv_get_int(value, &playback_status) )
    {
        playback_status = XMMS_PLAYBACK_STATUS_STOP;
        return TRUE;
    }

    switch( playback_status )
    {
        case XMMS_PLAYBACK_STATUS_PLAY:
        {
            xmmsc_result_t* res2;
            gtk_widget_set_tooltip_text( play_btn, _("Pause") );
            img = gtk_bin_get_child( (GtkBin*)play_btn );
            gtk_image_set_from_stock( (GtkImage*)img, GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_BUTTON );
            /* FIXME: this can cause some problems sometimes... */
            res2 = xmmsc_playlist_current_pos(con, cur_playlist);
            /* mark currently played track */
            xmmsc_result_notifier_set_and_unref(res2, on_playlist_pos_changed, NULL);
            break;
        }
        case XMMS_PLAYBACK_STATUS_STOP:
            gtk_label_set_text( GTK_LABEL(time_label), "--:--" );
            gtk_range_set_value( GTK_RANGE(progress_bar), 0.0 );
			gtk_window_set_title ((GtkWindow*)main_win, "LXMusic");
        case XMMS_PLAYBACK_STATUS_PAUSE:
            gtk_widget_set_tooltip_text( play_btn, _("Play") );
            img = gtk_bin_get_child( (GtkBin*)play_btn );
            gtk_image_set_from_stock( (GtkImage*)img, GTK_STOCK_MEDIA_PLAY,
                                      GTK_ICON_SIZE_BUTTON );
            break;
    }

    return TRUE;
}

static int on_playback_playtime_changed( xmmsv_t* value, void* user_data )
{
    int32_t time;
    char buf[32];
    if ( xmmsv_is_error(value)
         || ! xmmsv_get_int(value, &time))
        return TRUE;
    
    time /= 1000;
    if( time == play_time )
        return TRUE;
    play_time = time;

    gtk_label_set_text( (GtkLabel*)time_label, timeval_to_str( time, buf, G_N_ELEMENTS(buf) ) );

    if( cur_track_duration > 0 )
    {
        g_signal_handlers_block_by_func(progress_bar, on_progress_bar_changed, user_data);
        gtk_range_set_value( (GtkRange*)progress_bar,
                              (((gdouble)100000 * time) / cur_track_duration) );
        g_signal_handlers_unblock_by_func(progress_bar, on_progress_bar_changed, user_data);
    }
    return TRUE;
}

static int on_playback_track_loaded( xmmsv_t* value, void* user_data )
{
    TrackProperties track_properties;
    const char* err;
    const char *guessed_title;
    GString* window_title;
    
    if (xmmsv_get_error (value, &err)) {
	g_warning( "Server error: %s", err );
	return TRUE;
    }

    if (!get_track_properties( value, &track_properties)) 
	track_properties.title = guessed_title = guess_title_from_url( track_properties.url );
    cur_track_duration = track_properties.duration;    

    window_title = create_window_title(track_properties.artist, track_properties.title, playback_status == XMMS_PLAYBACK_STATUS_PLAY);

    gtk_window_set_title( GTK_WINDOW(main_win), window_title->str );
    
    if( tray_icon )
        gtk_status_icon_set_tooltip(GTK_STATUS_ICON(tray_icon), window_title->str);

    send_notifcation( track_properties.artist, track_properties.title );

    g_string_free( window_title, TRUE );
    xmmsv_unref( value );
    return TRUE;
}


static void send_notifcation( const gchar *artist, const gchar* title ) 
{
#ifdef HAVE_LIBNOTIFY
    if( ! GTK_WIDGET_VISIBLE(main_win) ) 
    {
	GString* notification_message = g_string_new("");
	
	if ( (artist != NULL) && (title != NULL ) ) {	
	    /* metadata available */
	    g_string_append_printf(notification_message, "<b>%s: </b><i>%s</i>", _("Artist"), artist );
	    g_string_append_printf(notification_message, "\n<b>%s: </b><i>%s</i>", _("Title"), title );
	}
	/* use filename without markup */
	else 			
	    g_string_append( notification_message, title );
	lxmusic_do_notify ( lxmusic_notification, _("Now Playing:"), notification_message->str );
	g_string_free( notification_message, TRUE );
    }
#endif	/* HAVE_LIBNOTIFY */
}




static int on_playback_cur_track_changed( xmmsv_t* value, void* user_data )
{
    if( xmmsv_get_int(value, &cur_track_id) && cur_track_id != 0)
    {
        xmmsc_result_t *res2;
        res2 = xmmsc_medialib_get_info(con, cur_track_id);
        xmmsc_result_notifier_set_and_unref(res2, on_playback_track_loaded, NULL);
    }
    
    return TRUE;
}

static int on_playlist_pos_changed( xmmsv_t* val, void* user_data )
{
    const char* name;
    xmmsv_t *name_val;
    int32_t pos = 0;
    
    if( xmmsv_dict_get( val, "name", &name_val ) )
    {
	xmmsv_get_string( name_val, &name );
        if( name && cur_playlist && !strcmp(cur_playlist, name) )
        {
	    xmmsv_t *pos_val, *int_value;

            if( xmmsv_dict_get( val, "position", &pos_val ) && xmmsv_get_int( pos_val, &pos ) && (pos >= 0 ) ) 
            {
                /* mark currently played song in the playlist with bold font. */
                GtkTreePath* path = gtk_tree_path_new_from_indices( pos, -1 );
                GtkTreeIter it;
                if( gtk_tree_model_get_iter( GTK_TREE_MODEL(list_store), &it, path ) )
                {
		    GtkTreeIter cur_track_iter = get_current_track_iter();
		    /* prev. current track */
                    if( cur_track_iter.stamp )
                        gtk_list_store_set(list_store, &cur_track_iter, COL_WEIGHT, PANGO_WEIGHT_NORMAL, -1);
                    gtk_list_store_set(list_store, &it, COL_WEIGHT, PANGO_WEIGHT_BOLD, -1);

		    /* update id of currently played track */
		    xmmsv_dict_get( val, "id", &int_value );
		    xmmsv_get_int( int_value, &cur_track_id );
                }
		/* scroll to currently played song */
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(playlist_view), path, NULL, FALSE, 0.0, 0.0 );
		gtk_tree_path_free( path );
	    }
	}
    }
    return TRUE;
}

void on_locate_cur_track(GtkAction* act, gpointer user_data)
{
    GtkTreeIter cur_track_iter = get_current_track_iter();
    if( cur_track_iter.stamp )
    {
        GtkTreeModelFilter* mf;
        GtkTreeIter it;
        mf = (GtkTreeModelFilter*)gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_view));
        if( gtk_tree_model_filter_convert_child_iter_to_iter(mf, &it, &cur_track_iter) )
        {
            GtkTreePath* path;
            /*
            GtkTreeSelection* sel = gtk_tree_view_get_selection(playlist_view);
            gtk_tree_selection_select_iter(sel, &it);
            */
            if( (path = gtk_tree_model_get_path(GTK_TREE_MODEL(mf), &it)) != NULL )
            {
                gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(playlist_view), path, NULL, FALSE, 0.0, 0.0);
                gtk_tree_path_free(path);
            }
        }
    }
}

static GtkTreeIter get_current_track_iter() 
{
    GtkTreeIter it, found_it = {0};
    int count=0;
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(list_store), &it )) 
    {
	do {
	    count++;
	    gint weight;
	    gtk_tree_model_get (GTK_TREE_MODEL(list_store), 
				&it, COL_WEIGHT, &weight, -1);
	    if (weight == PANGO_WEIGHT_BOLD) 
		found_it = it;
	} while (!found_it.stamp && gtk_tree_model_iter_next( GTK_TREE_MODEL(list_store), &it ));
    }
    return found_it;
}

static void get_channel_volumes( const char *key, xmmsv_t *value, void *user_data)
{
    GSList** volumes = (GSList**)user_data;
    int32_t volume;
    xmmsv_get_int( value , &volume );
    *volumes = g_slist_prepend(*volumes, (gpointer)volume);
}

static void get_channel_volume_names(const char* key, xmmsv_t *value, void *user_data)
{
    GSList** volumes = (GSList**)user_data;
    *volumes = g_slist_prepend(*volumes, (gpointer)key);
}

static int on_volume_btn_set_volume(xmmsv_t *value, void* user_data)
{
    GSList* volumes = NULL, *l;
    uint32_t val = GPOINTER_TO_UINT(user_data);

    g_assert( value != NULL );
    xmmsv_dict_foreach( value, get_channel_volume_names, &volumes);

    for( l = volumes; l; l = l->next )
    {
        xmmsc_result_t* res2;
        res2 = xmmsc_playback_volume_set(con, (char*)l->data, val);
        xmmsc_result_unref(res2);
    }
    g_slist_free(volumes);
    return TRUE;
}

static void on_volume_btn_changed(GtkScaleButton* btn, gdouble val, gpointer user_data)
{
    xmmsc_result_t* res;
    res = xmmsc_playback_volume_get(con);
    xmmsc_result_notifier_set_and_unref(res, on_volume_btn_set_volume, GUINT_TO_POINTER((uint32_t)val));
}

static void on_volume_btn_scrolled(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
	guint volume;
	GtkAdjustment *vol_adj;
	xmmsc_result_t *res;
	res = xmmsc_playback_volume_get(con);
	if (event->type != GDK_SCROLL)
		return;
	vol_adj = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON(user_data));
	switch (event->direction)
	{
		case GDK_SCROLL_UP:
			volume = gtk_adjustment_get_value (vol_adj) + 2;
			gtk_adjustment_set_value (GTK_ADJUSTMENT(vol_adj), volume);
			break;
		case GDK_SCROLL_DOWN:
			volume = gtk_adjustment_get_value (vol_adj) - 2;
			gtk_adjustment_set_value (GTK_ADJUSTMENT(vol_adj), volume);
			break;
		default:
			return;
	}
    xmmsc_result_notifier_set_and_unref(res, on_volume_btn_set_volume, GUINT_TO_POINTER((uint32_t)volume));
}

static int on_playback_volume_changed( xmmsv_t* value, void* user_data )
{
    GSList* volumes = NULL, *l;
    uint32_t vol = 0;

    if(xmmsv_is_error ( value )) 
    {
	const gchar *error_msg;
	xmmsv_get_error( value, &error_msg );
	g_warning( "%s: %s", __func__, error_msg );
    }
    else if (xmmsv_is_type( value, XMMSV_TYPE_DICT ))
    {
	xmmsv_dict_foreach( value, get_channel_volumes, &volumes );
        for( l = volumes; l; l = l->next )
        {
            if( vol < GPOINTER_TO_UINT(l->data) )
                vol = GPOINTER_TO_UINT(l->data);
        }
        g_slist_free(volumes);

        g_signal_handlers_block_by_func( volume_btn, on_volume_btn_changed, NULL );
        gtk_scale_button_set_value( GTK_SCALE_BUTTON(volume_btn), vol );
        g_signal_handlers_unblock_by_func( volume_btn, on_volume_btn_changed, NULL );
    }
    
    return TRUE;
}

static int on_collection_changed( xmmsv_t* dict, void* user_data )
{
    /* xmmsc_result_dict_foreach(res, dict_foreach, NULL); */
    
    xmmsv_t *string_value, *int_value;
    const char *name, *ns;
    gint32 type;
    
    
    xmmsv_dict_get (dict, "name", &string_value);
    xmmsv_get_string( string_value, &name);

    xmmsv_dict_get (dict, "namespace", &string_value);
    xmmsv_get_string( string_value, &ns );

    xmmsv_dict_get (dict, "type", &int_value);	
    xmmsv_get_int( int_value, &type );
    
    /* g_debug("name=%s, ns=%s, type=%d", name, ns, type); */

    /* currently we only care about playlists */
    if( ns && strcmp(ns, "Playlists") == 0 )
    {
        switch(type)
        {
        case XMMS_COLLECTION_CHANGED_ADD:
            add_playlist_to_menu( name, TRUE );
            break;
        case XMMS_COLLECTION_CHANGED_UPDATE:
            break;
        case XMMS_COLLECTION_CHANGED_RENAME:
            // rename_playlist( name, newname );
            break;
        case XMMS_COLLECTION_CHANGED_REMOVE:
            remove_playlist_from_menu( name );
            break;
        }
    }
    return TRUE;
}

static int on_media_lib_entry_changed(xmmsv_t* value, void* user_data)
{
    /* g_debug("mlib entry changed"); */
    int32_t id = 0;
    if( xmmsv_get_int (value, &id) )
    {
        GtkTreeModel* model = (GtkTreeModel*)list_store;
        GtkTreeIter it;
        if( !model )
            return TRUE;
        /* FIXME: This is damn inefficient, but I didn't have a
         * better way now.
         * Maybe it can be improved using custom tree model. :-( */
        if( gtk_tree_model_get_iter_first(model, &it) )
        {
            uint32_t _id;
            do{
                gtk_tree_model_get(model, &it, COL_ID, &_id, -1);
                if( _id == id )
                {
                    /* g_debug("found! update: %d", id); */
                    queue_update_track( id, &it );
                    break;
                }
            }while(gtk_tree_model_iter_next(model, &it));
        }
    }
    return TRUE;
}

static void config_changed_foreach(const char *key, xmmsv_t *value, void* user_data)
{
    if( strncmp( key, "playlist.", 9) == 0 )
    {
        const char* val;
	g_assert( xmmsv_get_string( value, &val ) );

        if( strcmp( key + 9, "repeat_one") == 0 )
        {
            if( val[0] == '1' )
                repeat_mode = REPEAT_CURRENT;
            else
            {
                if( repeat_mode == REPEAT_CURRENT )
                    repeat_mode = REPEAT_NONE;
            }
        }
        else if( strcmp( key + 9, "repeat_all") == 0 )
        {
            if( val[0] == '1' )
                repeat_mode = REPEAT_ALL;
            else
            {
                if( repeat_mode == REPEAT_ALL )
                    repeat_mode = REPEAT_NONE;
            }
        }
        g_signal_handlers_block_by_func(repeat_mode_cb, on_repeat_mode_changed, NULL );
        gtk_combo_box_set_active( GTK_COMBO_BOX(repeat_mode_cb), repeat_mode );
        g_signal_handlers_unblock_by_func(repeat_mode_cb, on_repeat_mode_changed, NULL );
    }
}

static int on_configval_changed(xmmsv_t* value, void* user_data)
{
    xmmsv_dict_foreach( value, config_changed_foreach, NULL );
    return TRUE;
}

static void setup_xmms_callbacks()
{
    xmmsc_result_t* res;

    xmmsc_disconnect_callback_set (con, on_server_disconnect, NULL);
    /* play status */
    res = xmmsc_playback_status(con);
    xmmsc_result_notifier_set_and_unref(res, on_playback_status_changed, NULL);
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playback_status, on_playback_status_changed, NULL );

    /* server quit */
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_quit, on_server_quit, NULL );

    /* play time */
    /* this is a signal rather than broadcast, so restart is needed in the callback func. */
    XMMS_CALLBACK_SET( con, xmmsc_signal_playback_playtime, on_playback_playtime_changed, NULL);

    /* playlist changed */
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playlist_changed, on_playlist_content_changed, NULL );

    /* playlist loaded */
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playlist_loaded, on_playlist_loaded, NULL );

    /* current track info */
    res = xmmsc_playback_current_id( con );
    xmmsc_result_notifier_set_and_unref( res, on_playback_cur_track_changed, NULL );
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playback_current_id, on_playback_cur_track_changed, NULL );

    /* current pos in playlist */
//    res = xmmsc_playlist_current_pos( con, "_active" );
//    xmmsc_result_notifier_set( res, on_playlist_pos_changed, NULL );
//    xmmsc_result_unref(res);
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playlist_current_pos, on_playlist_pos_changed, NULL);

    /* volume */
    res = xmmsc_playback_volume_get(con);
    xmmsc_result_notifier_set_and_unref(res, on_playback_volume_changed, NULL );
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playback_volume_changed, on_playback_volume_changed, NULL );

    /* media lib */
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_medialib_entry_changed, on_media_lib_entry_changed, NULL );

    XMMS_CALLBACK_SET( con, xmmsc_broadcast_collection_changed, on_collection_changed, NULL );


    /* config values */
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_configval_changed, on_configval_changed, NULL );
}

static int on_cfg_repeat_all_received(xmmsv_t* value, void* user_data)
{
    char* val;
    GtkComboBox* cb = (GtkComboBox*)user_data;
    if( xmmsv_get_string(value, (const char**)&val) )
    {
        if( val && val[0] == '1' )
        {
            repeat_mode = REPEAT_ALL;
            gtk_combo_box_set_active( cb, repeat_mode );
        }
    }
    return TRUE;
}

static int on_cfg_repeat_one_received(xmmsv_t* value, void* user_data)
{
    GtkComboBox* cb = (GtkComboBox*)user_data;
    char* val;
    if( xmmsv_get_string( value, (const char**)&val) )
    {
        if( val && val[0] == '1' )
        {
            repeat_mode = REPEAT_CURRENT;
            gtk_combo_box_set_active( cb, repeat_mode );
        }
    }
    return TRUE;
}


static void setup_ui()
{
    GtkBuilder *builder;
    GtkUIManager* mgr;
    GtkWidget *hbox, *cb, *switch_pl_mi, *show_pl_mi;
    xmmsc_result_t* res;

    builder = gtk_builder_new();
    if( ! gtk_builder_add_from_file(builder, PACKAGE_DATA_DIR "/lxmusic/lxmusic.ui.glade", NULL) )
        exit(1);

    main_win = (GtkWidget*)gtk_builder_get_object(builder, "main_win");
    play_btn = (GtkWidget*)gtk_builder_get_object(builder, "play_btn");
    time_label = (GtkWidget*)gtk_builder_get_object(builder, "time_label");
    progress_bar = (GtkWidget*)gtk_builder_get_object(builder, "progress_bar");
    notebook = (GtkWidget*)gtk_builder_get_object(builder, "notebook");
    status_bar = (GtkWidget*)gtk_builder_get_object(builder, "status_bar");

    inner_vbox = (GtkWidget*)gtk_builder_get_object(builder, "inner_vbox");
    playlist_view = (GtkWidget*)gtk_builder_get_object(builder, "playlist_view");
    gtk_drag_dest_add_uri_targets(playlist_view);

    repeat_mode_cb = (GtkWidget*)gtk_builder_get_object(builder, "repeat_mode");

    mgr = (GtkUIManager*)gtk_builder_get_object(builder, "uimanager1");
    switch_pl_mi = gtk_ui_manager_get_widget( mgr, "/menubar/playlist_mi/switch_to_pl" );
    switch_pl_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(switch_pl_mi), switch_pl_menu);

    add_to_pl_menu = gtk_menu_item_get_submenu((GtkMenuItem*)gtk_ui_manager_get_widget( mgr, "/menubar/playlist_mi/add_to_pl" ));
    rm_from_pl_menu = gtk_menu_item_get_submenu((GtkMenuItem*)gtk_ui_manager_get_widget( mgr, "/menubar/playlist_mi/remove_from_pl" ));

    show_pl_mi = gtk_ui_manager_get_widget( mgr, "/menubar/view_mi/show_pl" );

    cb = (GtkWidget*)gtk_builder_get_object(builder, "repeat_mode");
    gtk_combo_box_set_active(GTK_COMBO_BOX(cb), REPEAT_NONE);
//    gtk_combo_box_set_active(cb, repeat_mode);
    res = xmmsc_configval_get( con, "playlist.repeat_all" );
    xmmsc_result_notifier_set_and_unref( res, on_cfg_repeat_all_received, cb );
    res = xmmsc_configval_get( con, "playlist.repeat_one" );
    xmmsc_result_notifier_set_and_unref( res, on_cfg_repeat_one_received, cb );

    cb = (GtkWidget*)gtk_builder_get_object(builder, "filter_field");
    gtk_combo_box_set_active(GTK_COMBO_BOX(cb), filter_field);

    /* add volume button */
    hbox = (GtkWidget*)gtk_builder_get_object(builder, "top_hbox");
    volume_btn = gtk_volume_button_new();
    gtk_scale_button_get_adjustment(GTK_SCALE_BUTTON(volume_btn))->upper = 100;
    gtk_widget_show(volume_btn);
    gtk_box_pack_start(GTK_BOX(hbox), volume_btn, FALSE, TRUE, 0);
    g_signal_connect(volume_btn, "value-changed", G_CALLBACK(on_volume_btn_changed), NULL);
    g_signal_connect(volume_btn, "scroll-event", G_CALLBACK(on_volume_btn_scrolled), volume_btn);

    /* init the playlist widget */
    init_playlist(playlist_view);

    /* signal handlers */
    gtk_builder_connect_signals(builder, NULL);

    /* window icon */
    gtk_window_set_icon_from_file(GTK_WINDOW(main_win), PACKAGE_DATA_DIR"/pixmaps/lxmusic.png", NULL );

    gtk_window_set_default_size(GTK_WINDOW(main_win), win_width, win_height);
    /* this can trigger signal handler and show or hide the playlist. */
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_pl_mi), show_playlist);

    g_object_unref(builder);
    gtk_widget_show_all(notebook);
    gtk_window_move(GTK_WINDOW(main_win), win_xpos, win_ypos);
	gtk_widget_show (main_win);

    /* tray icon */
    if( show_tray_icon )
        create_tray_icon();
}

void on_new_playlist(GtkAction* act, gpointer user_data)
{
    GtkWidget *dlg = gtk_dialog_new_with_buttons(
            _("Create new playlist"), (GtkWindow*)main_win, GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );
    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start( (GtkBox*)((GtkDialog*)dlg)->vbox, entry, FALSE, FALSE, 4 );
    gtk_dialog_set_default_response( (GtkDialog*)dlg, GTK_RESPONSE_OK );
    gtk_entry_set_activates_default( (GtkEntry*)entry, TRUE );
    gtk_widget_show_all( dlg );
    if( gtk_dialog_run( (GtkDialog*)dlg ) == GTK_RESPONSE_OK )
    {
        xmmsc_result_t *res;
        const char* name = gtk_entry_get_text( (GtkEntry*)entry );
        res = xmmsc_playlist_create( con, name );
        xmmsc_result_unref( res );

        /* load the newly created list */
        xmmsc_playlist_load(con, name);
        xmmsc_result_unref( res );
    }
    gtk_widget_destroy( dlg );
}

void on_del_playlist(GtkAction* act, gpointer user_data)
{
    GSList *prev=NULL, *l;
    char* switch_to = NULL;

    /* NOTE: we should at least have one playlist */
    if( !switch_pl_menu_group || !switch_pl_menu_group )
        return;
    if( ! cur_playlist )
        return;

    /* switching to another playlist before removing the current one. */
    for( l = all_playlists; l; l = l->next )
    {
        char* name = (char*)l->data;
        if( 0 == strcmp( name, cur_playlist ) )
        {
            if( l->next )
                switch_to = (char*)l->next->data;
            else if(prev)
                switch_to = (char*)prev->data;
            break;
        }
        prev= l;
    }
    if( switch_to )
    {
        xmmsc_result_t* res;
        res = xmmsc_playlist_load(con, switch_to);
        xmmsc_result_unref(res);

        res = xmmsc_playlist_remove(con, cur_playlist);
        xmmsc_result_unref(res);
    }
}

void on_show_playlist(GtkAction* act, gpointer user_data)
{
    show_playlist = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(act));
    if(GTK_WIDGET_VISIBLE(inner_vbox))
    {
        if( ! show_playlist )
        {
            /* save current size to restore it later. */
            gtk_window_get_size(GTK_WINDOW(main_win), &win_width, &win_height );
            gtk_widget_hide(inner_vbox);
            /* Dirty trick used to shrink the window to its minimal size */
            gtk_window_resize(GTK_WINDOW(main_win), 1, 1);
        }
    }
    else
    {
        if( show_playlist )
        {
            gtk_window_resize(GTK_WINDOW(main_win), win_width, win_height );
            gtk_widget_show(inner_vbox);
        }
    }
}

void on_playlist_cut(GtkAction* act, gpointer user_data)
{
    show_error( GTK_WINDOW(main_win), NULL, "Not yet implemented");
}

void on_playlist_copy(GtkAction* act, gpointer user_data)
{
    show_error( GTK_WINDOW(main_win), NULL, "Not yet implemented");
}

void on_playlist_paste(GtkAction* act, gpointer user_data)
{
    show_error( GTK_WINDOW(main_win), NULL, "Not yet implemented");
}


int main (int argc, char *argv[])
{
    xmmsc_result_t* res;

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    if( !(con = xmmsc_init ("lxmusic")) )
        return EXIT_FAILURE;

    /* try to connect to xmms2d, and launch the daemon if needed. */
    if( !xmmsc_connect(con, getenv("XMMS_PATH")) )
    {
        if( ! g_spawn_command_line_sync( "xmms2-launcher --yes-run-as-root", NULL, NULL, NULL, NULL )
            || !xmmsc_connect(con, getenv ("XMMS_PATH")) )
        {
            fprintf(stderr, "Connection failed: %s\n",
                     xmmsc_get_last_error (con));
            return EXIT_FAILURE;
        }
    }

    gtk_init(&argc, &argv);

    plugin_config_setup(con);

    update_tracks =  g_hash_table_new( g_direct_hash, NULL );
       
    xmmsc_mainloop_gmain_init(con);

    load_config();

    /* build the GUI */
    setup_ui();

#ifdef HAVE_LIBNOTIFY
    if (!notify_is_initted ())
	notify_init ("LXMusic");
    lxmusic_notification  = lxmusic_notification_new( GTK_STATUS_ICON( tray_icon ) );
#endif

    /* some dirty hacks to show the window earlier :-D */
    while( gtk_events_pending() )
        gtk_main_iteration();
    gdk_display_sync(gtk_widget_get_display(main_win));

    /* display currently active playlist */
    res = xmmsc_playlist_current_active(con);
    xmmsc_result_notifier_set_and_unref(res, on_playlist_get_active, NULL);

    /* load all existing playlists and add them to the menu */
    res = xmmsc_playlist_list( con );
    xmmsc_result_notifier_set_and_unref(res, on_playlists_listed, NULL);

    /* register callbacks */
    setup_xmms_callbacks();

    gtk_main ();

    cancel_pending_update_tracks();
    g_hash_table_unref ( update_tracks );
    save_config();

    return 0;
}

