#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

enum{
    COL_ID = 0,
    COL_NUM,
    COL_TITLE,
    COL_TIME,
    N_COLS
};

xmmsc_connection_t *con;
int status = XMMS_PLAYBACK_STATUS_STOP;
static guint play_time = 0;
guint current_id = -1;
static guint playlist_pos = -1;
static guint current_time_len = 0;

GtkWidget* main_win = NULL;
GtkWidget* music_title = NULL;
GtkWidget* time_label = NULL;
GtkWidget* progress_bar = NULL;
GtkWidget* status_bar = NULL;
GtkWidget* list_view = NULL;
static GtkWidget* play_btn = NULL;

GtkTooltips* tips = NULL;

#if 0
static void load_playlist()
{
    xmmsc_result_t *res, *res2;
    res = xmmsc_playlist_list (con);
    xmmsc_result_wait (res);
    if( xmmsc_result_iserror (res) ) {
        fprintf (stderr, "error when asking for the playlist, %s\n",
                 xmmsc_result_get_error (res));
        exit (EXIT_FAILURE);
    }
    for (;xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
        unsigned int id;
        const char *url, *file, *name;
        if( !xmmsc_result_get_uint (res, &id) ) {
            fprintf (stderr, "Couldn't get uint from list\n");
            exit (EXIT_FAILURE);
        }
        res2 = xmmsc_medialib_get_info (con, id);
        xmmsc_result_wait (res2);
        xmmsc_result_get_dict_entry_str(res2, "url", &url);
        file = g_filename_from_uri(url, NULL, NULL);
        name = g_path_get_basename(file);
        g_free( file );
        //g_debug("id = %d, file = %s", id, name );
        g_free( name );
        xmmsc_result_unref(res2);
    }
    xmmsc_result_unref(res);
}
#endif

static void play_jump( int relative )
{
    xmmsc_result_t *res;

    res = xmmsc_playlist_set_next_rel( con, relative );
    xmmsc_result_wait(res);
    xmmsc_result_unref(res);

    res = xmmsc_playback_tickle(con);
    xmmsc_result_wait(res);
    xmmsc_result_unref(res);
}

static void on_prev_next( GtkButton* btn, gpointer user_data )
{
    play_jump( GPOINTER_TO_INT(user_data) );
}

static void on_play_pause( GtkButton* btn, gpointer user_data )
{
    xmmsc_result_t *res;

    if( status == XMMS_PLAYBACK_STATUS_PLAY )
        res = xmmsc_playback_pause(con);
    else
        res = xmmsc_playback_start(con);

    xmmsc_result_wait (res);
    if( xmmsc_result_iserror (res) ) {
        fprintf (stderr, "error: %s\n",
                 xmmsc_result_get_error (res));
    }
    xmmsc_result_unref(res);
}

static void on_stop( GtkButton* btn, gpointer user_data )
{
    xmmsc_result_t *res;
    res = xmmsc_playback_stop(con);
    xmmsc_result_wait (res);
    if( xmmsc_result_iserror (res) ) {
        fprintf (stderr, "error when asking for the playlist, %s\n",
                 xmmsc_result_get_error (res));
    }
    xmmsc_result_unref(res);
}

GtkWidget* add_tool_btn( GtkBox* box,
                         const char* stock_id,
                         const char* tooltip,
                         GCallback callback,
                         gpointer user_data )
{
    GtkWidget *img, *btn;
    img = gtk_image_new_from_stock(stock_id,
                                   GTK_ICON_SIZE_MENU);
    btn = gtk_button_new();
    gtk_button_set_relief( btn, GTK_RELIEF_NONE );
    gtk_button_set_focus_on_click( btn, FALSE );
    GTK_WIDGET_UNSET_FLAGS( btn, GTK_CAN_FOCUS );
    gtk_container_add( btn, img );
    gtk_tooltips_set_tip( tips, btn, tooltip, NULL );
    gtk_box_pack_start( box, btn, FALSE, FALSE, 0);
    g_signal_connect( btn, "clicked", callback, user_data );
    return btn;
}

static void update_play_btn()
{
    // FIXME: This should be implemented
}

static char* get_song_name( xmmsc_result_t* res )
{
    char *url, *file, *name;
    xmmsc_result_get_dict_entry_str( res, "url", &url );
    file = g_filename_from_uri(url, NULL, NULL);
    name = g_path_get_basename(file);
    gtk_label_set_text( music_title, name );
    g_free( file );
    return name;
}

static void update_song( xmmsc_result_t* res, GtkTreeIter* it )
{
    char* name = get_song_name( res );
    gtk_list_store_set( gtk_tree_view_get_model(list_view), it,
                        COL_TITLE, name, -1 );
    g_free( name );
    gtk_tree_iter_free( it );
}

static void on_playlist_received( xmmsc_result_t* res, void* user_data )
{
    GtkListStore* list;
    GtkTreeIter it;
    guint pos = 0;

    list = gtk_list_store_new( N_COLS,
                               G_TYPE_UINT,
                               G_TYPE_UINT,
                               G_TYPE_STRING,
                               G_TYPE_STRING );
    for (; xmmsc_result_list_valid(res); xmmsc_result_list_next(res)) {
        guint id;
        xmmsc_result_t* res2;
        char* name;

        gtk_list_store_append( list, &it );
        xmmsc_result_get_uint( res, &id );
        res2 = xmmsc_medialib_get_info( con, id );
/*
        gtk_list_store_set( list, &it,
                            COL_ID, id,
                            COL_NUM, pos++, -1 );
        xmmsc_result_notifier_set(res2, update_song,
                                  gtk_tree_iter_copy(&it));

*/
        xmmsc_result_wait( res2 );
        name = get_song_name( res2 );
        gtk_list_store_set( list, &it,
                            COL_ID, id,
                            COL_NUM, pos++,
                            COL_TITLE, name, -1 );
        g_free( name );
        xmmsc_result_unref(res2);
    }
    gtk_tree_view_set_model( list_view, GTK_TREE_MODEL(list) );
    g_object_unref( list );

    if( GTK_WIDGET_REALIZED( list_view ) )
        gdk_window_set_cursor( list_view->window, NULL );
}

static void update_play_list()
{
    xmmsc_result_t *res;

    if( GTK_WIDGET_REALIZED( list_view ) ) {
        GdkCursor* cur;
        cur = gdk_cursor_new( GDK_WATCH );
        gdk_window_set_cursor( list_view->window, cur );
        gdk_cursor_unref( cur );
    }
    res = xmmsc_playlist_list( con );
    xmmsc_result_notifier_set( res, on_playlist_received, NULL);
    xmmsc_result_unref(res);
}

static void on_row_activated( GtkTreeView* view,
                              GtkTreePath* path,
                              GtkTreeViewColumn* col,
                              gpointer user_data )
{
    play_jump( gtk_tree_path_get_indices(path)[0] - playlist_pos );
}

static void init_list_view()
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;

    g_signal_connect( list_view, "row-activated",
                      G_CALLBACK( on_row_activated ), NULL );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( "#", render,
                                                   "text", COL_NUM, NULL );
    gtk_tree_view_append_column( list_view, col );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( _("Title"), render,
                                                   "text", COL_TITLE, NULL );
    gtk_tree_view_append_column( list_view, col );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( _("Length"), render,
                                                   "text", COL_TIME, NULL );
    gtk_tree_view_append_column( list_view, col );

    update_play_list();
}

static void init_main_win()
{
    GtkWidget* vbox, *hbox, *toolbar, *scroll, *vbox2, *entry;

    tips = gtk_tooltips_new();
#if GTK_CHECK_VERSION( 2, 10, 0 )
    g_object_ref_sink(tips);
#else
    g_object_ref( tips );
    gtk_object_sink( tips );
#endif

    main_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_title((GtkWindow*)main_win, "LXMusic");
    gtk_window_set_default_size(main_win, 280, 480);
    g_signal_connect( main_win, "delete-event",
                      G_CALLBACK(gtk_main_quit), NULL );

    vbox = gtk_vbox_new( FALSE, 2 );
    gtk_container_add( main_win, vbox );

    hbox = gtk_hbox_new( FALSE, 2 );
    gtk_box_pack_start( vbox, hbox, FALSE, FALSE, 0 );
    music_title = gtk_label_new("");
    gtk_misc_set_padding( music_title, 2, 2 );
    gtk_box_pack_start( hbox, music_title, FALSE, FALSE, 2 );

    gtk_box_pack_start( vbox, gtk_hseparator_new(), FALSE, FALSE, 0 );

    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( vbox, hbox, FALSE, FALSE, 0 );

    add_tool_btn( hbox, GTK_STOCK_MEDIA_PREVIOUS, _("Previous"),
                  G_CALLBACK(on_prev_next), GINT_TO_POINTER(-1) );
    play_btn = add_tool_btn( hbox, GTK_STOCK_MEDIA_PLAY, _("Play"),
                             G_CALLBACK(on_play_pause), NULL );
    update_play_btn();
    add_tool_btn( hbox, GTK_STOCK_MEDIA_STOP, _("Stop"),
                  G_CALLBACK(on_stop), NULL );
    add_tool_btn( hbox, GTK_STOCK_MEDIA_NEXT, _("Next"),
                  G_CALLBACK(on_prev_next), GINT_TO_POINTER(1) );

    vbox2 = gtk_vbox_new( FALSE, 1 );
    time_label = gtk_label_new("--:--/--:--");
    gtk_misc_set_alignment( time_label, 0.0, 0.5 );
    progress_bar = gtk_hscale_new_with_range( 0.0, 100.0, 1.0 );
    GTK_WIDGET_UNSET_FLAGS( progress_bar, GTK_CAN_FOCUS );
    gtk_scale_set_draw_value( progress_bar, FALSE );
    gtk_box_pack_start( vbox2, time_label, FALSE, FALSE, 0 );
    gtk_box_pack_start( vbox2, progress_bar, FALSE, FALSE, 0 );
    gtk_box_pack_start( hbox, vbox2, TRUE, TRUE, 2 );

    /* play list */
    scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( scroll,
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_shadow_type( scroll, GTK_SHADOW_IN );
    list_view = gtk_tree_view_new();
    gtk_container_add( scroll, list_view );
    gtk_box_pack_start( vbox, scroll, TRUE, TRUE, 2 );

    /* play list bar */
    hbox = gtk_hbox_new( FALSE, 2 );
    entry = gtk_entry_new();
    gtk_box_pack_start( hbox, entry, TRUE, TRUE, 2 );
/*
    img = gtk_image_new_from_stock(GTK_STOCK_ADD,
                                   GTK_ICON_SIZE_MENU);
    btn = gtk_menu_tool_button_new( img, NULL );
    gtk_toolbar_insert( toolbar, btn, -1 );

    img = gtk_image_new_from_stock(GTK_STOCK_REMOVE,
                                   GTK_ICON_SIZE_MENU);
    btn = gtk_menu_tool_button_new( img, NULL );
    gtk_toolbar_insert( toolbar, btn, -1 );
*/
    /* status bar */
    status_bar = gtk_statusbar_new();
    gtk_box_pack_start( vbox, status_bar, FALSE, FALSE, 2 );
}

static void
on_status_changed( xmmsc_result_t *res, void *user_data )
{
    GtkWidget* img;
    //  char strstat[256];
    if ( !xmmsc_result_get_uint(res, &status) ) {
        status = XMMS_PLAYBACK_STATUS_STOP;
        // FIXME: is this ok?
        return;
    }

    switch( status )
    {
        case XMMS_PLAYBACK_STATUS_PLAY:
            gtk_tooltips_set_tip( tips, play_btn, _("Pause"), NULL );
            img = gtk_bin_get_child( play_btn );
            gtk_image_set_from_stock( img, GTK_STOCK_MEDIA_PAUSE,
                                      GTK_ICON_SIZE_MENU );
            break;
        case XMMS_PLAYBACK_STATUS_PAUSE:
        case XMMS_PLAYBACK_STATUS_STOP:
            gtk_tooltips_set_tip( tips, play_btn, _("Play"), NULL );
            img = gtk_bin_get_child( play_btn );
            gtk_image_set_from_stock( img, GTK_STOCK_MEDIA_PLAY,
                                      GTK_ICON_SIZE_MENU );
            break;
    }
}

static void on_playtime_changed( xmmsc_result_t* res, void* user_data )
{
    guint time;
    guint hr, min, sec;
    xmmsc_result_t* restart;
    char buf[32];
    if ( xmmsc_result_iserror(res)
         || ! xmmsc_result_get_uint(res, &time))
        return;

    restart = xmmsc_result_restart(res);
    xmmsc_result_unref(res);
    xmmsc_result_unref(restart);

    time /= 1000;
    if( time == play_time )
        return;
    play_time = time;
    hr = time / 3600;
    min = time % 3600;
    sec = min % 60;
    min /= 60;
    g_snprintf( buf, sizeof(buf), "%.2u:%.2u:%.2u", hr, min, sec );
    gtk_label_set_text( time_label, buf );

    if( current_time_len > 0 ) {
        gtk_range_set_value( progress_bar, (((gdouble)100000 * time) /
                                            current_time_len) );
    }
}

/*
static void update_music_title( xmmsc_result_t* res, void* user_data )
{
    char *url, *file, *name;
    xmmsc_result_get_dict_entry_str( res, "url", &url );
    file = g_filename_from_uri(url, NULL, NULL);
    name = g_path_get_basename(file);
    gtk_label_set_text( music_title, name );
    g_free( file );
    g_free( name );
    xmmsc_result_unref( res );
}
*/

static void on_current_changed( xmmsc_result_t* res, void* user_data )
{
    gboolean need_unref = GPOINTER_TO_UINT(user_data);
    if( xmmsc_result_get_uint(res, &current_id) ) {
        xmmsc_result_t *res2;
        char* name;
        res2 = xmmsc_medialib_get_info(con, current_id);
//        xmmsc_result_notifier_set( res2, update_music_title, NULL );
        xmmsc_result_wait ( res2 );
        name = get_song_name( res2 );
        gtk_label_set_text( music_title, name );
        g_free( name );

        if( !xmmsc_result_get_dict_entry_int32( res2, "duration",
                                                &current_time_len) )
            current_time_len = -1;
        xmmsc_result_unref(res2);
    }
    if( need_unref )
        xmmsc_result_unref(res);
}

static void on_playlist_pos_changed( xmmsc_result_t* res, void* user_data )
{
    guint pos;
    GtkTreePath* path;
    GtkTreeSelection* sel;

    xmmsc_result_get_uint( res, &playlist_pos );
    path = gtk_tree_path_new_from_indices( playlist_pos, -1 );
    sel = gtk_tree_view_get_selection( list_view );
    gtk_tree_selection_select_path( sel, path );
    gtk_tree_path_free( path );
}

int main( int argc, char **argv )
{
    xmmsc_result_t* res;

    gtk_init( &argc, &argv );

    if( !(con = xmmsc_init ("lxmusic")) ) {
        return EXIT_FAILURE;
    }
_connect_xmms2:
    if (!xmmsc_connect (con, getenv ("XMMS_PATH"))) {
        /* This is too dirty, but is there any better solution? */
        static int try_xmms2d = 0;
        if( try_xmms2d < 10 ) {
            g_spawn_command_line_async( "xmms2d", NULL );
            g_usleep( 500 );
            ++try_xmms2d;
            goto _connect_xmms2;
        }
        fprintf (stderr, "Connection failed: %s\n",
                    xmmsc_get_last_error (con));
        return EXIT_FAILURE;
    }
    xmmsc_mainloop_gmain_init(con);

    res = xmmsc_playback_status(con);
    xmmsc_result_notifier_set(res, on_status_changed, NULL);
    xmmsc_result_unref(res);

    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playback_status,
                       on_status_changed, NULL );
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playback_current_id,
                       on_current_changed, GUINT_TO_POINTER(FALSE) );
    XMMS_CALLBACK_SET( con , xmmsc_signal_playback_playtime,
                       on_playtime_changed, NULL);
    XMMS_CALLBACK_SET( con, xmmsc_playback_current_id,
                       on_current_changed, GUINT_TO_POINTER(TRUE) );
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playlist_current_pos,
                       on_playlist_pos_changed, NULL);
/*
    XMMS_CALLBACK_SET(con, xmmsc_playback_current_id,
                        sig_handle_current_id, con);
    XMMS_CALLBACK_SET(con, xmmsc_broadcast_playback_current_id,
                        bc_handle_current_id, con);
    XMMS_CALLBACK_SET(con, xmmsc_broadcast_medialib_entry_changed,
                        bc_handle_medialib_entry_changed, con);
    XMMS_CALLBACK_SET(con, xmmsc_signal_playback_playtime,
                        sig_handle_playtime, title_str);
    XMMS_CALLBACK_SET(con, xmmsc_broadcast_playback_status,
                        bc_handle_playback_status_change, NULL);
    XMMS_CALLBACK_SET(con, xmmsc_broadcast_playback_volume_changed,
                        bc_handle_volume_change, NULL);

    xmmsc_disconnect_callback_set(con, connection_lost, NULL);

    res = xmmsc_playback_status(con);
    xmmsc_result_notifier_set(res, n_handle_playback_status_change, NULL);
    xmmsc_result_unref(res);
    res = xmmsc_playback_volume_get(con);
    xmmsc_result_notifier_set(res, n_volume_init, NULL);
    xmmsc_result_unref(res);
*/

    init_main_win();
    gtk_widget_show_all( main_win );

    init_list_view();

    gtk_main();
    g_object_unref( tips );
    xmmsc_unref (con);
    return EXIT_SUCCESS;
}
