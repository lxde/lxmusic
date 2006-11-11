#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#define HAVE_XMMSC_PLAYLIST_RADD  1

enum{
    COL_ID = 0,
    COL_TITLE,
    COL_LEN,
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
GtkWidget* bitrate_label = NULL;
GtkWidget* progress_bar = NULL;
GtkWidget* status_bar = NULL;
GtkWidget* list_view = NULL;
static GtkWidget* play_btn = NULL;

GtkTooltips* tips = NULL;

static const char* timeval_to_str( guint timeval, char* buf, guint buf_len )
{
    guint hr, min, sec;

    hr = timeval / 3600;
    min = timeval % 3600;
    sec = min % 60;
    min /= 60;
    g_snprintf( buf, buf_len, "%.2u:%.2u:%.2u", hr, min, sec );

    return buf;
}

static void show_error( const char* error )
{
    GtkWidget* dlg = gtk_message_dialog_new( (GtkWindow*)main_win,
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             error );
    gtk_dialog_run( (GtkDialog*)dlg );
    gtk_widget_destroy( dlg );
}

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
    gtk_button_set_relief( (GtkButton*)btn, GTK_RELIEF_NONE );
    gtk_button_set_focus_on_click( (GtkButton*)btn, FALSE );
    GTK_WIDGET_UNSET_FLAGS( btn, GTK_CAN_FOCUS );
    gtk_container_add( (GtkContainer*)btn, img );
    gtk_tooltips_set_tip( tips, btn, tooltip, NULL );
    gtk_box_pack_start( (GtkBox*)box, btn, FALSE, FALSE, 0);
    g_signal_connect( btn, "clicked", callback, user_data );
    return btn;
}

static void update_play_btn()
{
    // FIXME: This should be implemented
}

static char* get_song_name( xmmsc_result_t* res )
{
    char *url;
    const char *decode;

    xmmsc_result_get_dict_entry_str( res, "url", &url );
    if( !url )
        return NULL;
    decode = xmmsc_result_decode_url( res, url ); //g_filename_from_uri(url, NULL, NULL);
    /* local file */
    if( decode ) {
        if( g_str_has_prefix( decode, "file:" ) ) {
            char *name;
            const char* file = decode + 5;
            while( *file == '/' )
                ++file;
            /* FIXME: Should convert file name with g_filename_display_name */
            name = g_path_get_basename(file);
            return name;
        }
        else {
            return g_strdup( decode );
        }
    }
    return g_strdup( url );
}

static void update_song( xmmsc_result_t *res, void *user_data )
{
    char *name;
    guint time_len = 0;
    char time_buf[32];
    GtkTreeView *view = (GtkTreeView*)list_view;
    GtkListStore *list;
    GtkTreeIter* it = (GtkTreeIter*)user_data;

    if( xmmsc_result_iserror( res ) ) {
        xmmsc_result_unref( res );
        gtk_tree_iter_free( it );
        return;
    }

    name = get_song_name( res );
    xmmsc_result_get_dict_entry_int32( res, "duration",
                                       &time_len);
    timeval_to_str( time_len/1000, time_buf, G_N_ELEMENTS(time_buf) );

    GDK_THREADS_ENTER();
    list = (GtkListStore*)gtk_tree_view_get_model( view );

    gtk_list_store_set( list, it,
                        COL_TITLE, name,
                        COL_LEN, time_buf, -1 );

    g_free( name );
    /* This is a bad idea :-( */
    gtk_tree_iter_free( it );
    GDK_THREADS_LEAVE();

    xmmsc_result_unref( res );
}

static void on_playlist_received( xmmsc_result_t* res, void* user_data )
{
    GtkListStore* list;
    GtkTreeIter it;

    list = gtk_list_store_new( N_COLS,
                               G_TYPE_UINT,
                               G_TYPE_STRING,
                               G_TYPE_STRING );
    for (; xmmsc_result_list_valid(res); xmmsc_result_list_next(res)) {
        guint id;
        xmmsc_result_t* res2;
        char* name;

        gtk_list_store_append( list, &it );
        gtk_list_store_set( list, &it,
                            COL_ID, id, -1 );

        xmmsc_result_get_uint( res, &id );
        res2 = xmmsc_medialib_get_info( con, id );
        /* FIXME: is there any better way? */
        xmmsc_result_notifier_set( res2, update_song,
                                   gtk_tree_iter_copy( &it ) );
        xmmsc_result_unref( res2 );
    }
    gtk_tree_view_set_model( (GtkTreeView*)list_view,
                              GTK_TREE_MODEL(list) );
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
    if( status != XMMS_PLAYBACK_STATUS_PLAY )
        on_play_pause( (GtkButton*)play_btn, NULL );
}

static void render_num( GtkTreeViewColumn* col, GtkCellRenderer* render,
                        GtkTreeModel* model, GtkTreeIter* it, gpointer data )
{
    GtkTreePath* path = gtk_tree_model_get_path( model, it );
    char buf[16];
    if( G_UNLIKELY( ! path ) )
        return;
    g_sprintf( buf, "%d", gtk_tree_path_get_indices( path )[0] );
    gtk_tree_path_free( path );
    g_object_set( render, "text", buf, NULL );
}

static void init_list_view()
{
    GtkTreeSelection* tree_sel;
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;

    g_signal_connect( list_view, "row-activated",
                      G_CALLBACK( on_row_activated ), NULL );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( "#", render, NULL );
    gtk_tree_view_column_set_cell_data_func( col, render, render_num, NULL, NULL );
    gtk_tree_view_append_column( (GtkTreeView*)list_view, col );

    render = gtk_cell_renderer_text_new();
    g_object_set( render, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
    col = gtk_tree_view_column_new_with_attributes( _("Title"), render,
                                                   "text", COL_TITLE, NULL );
    gtk_tree_view_column_set_expand( col, TRUE );
    /* gtk_tree_view_column_set_resizable( col, TRUE ); */
    gtk_tree_view_append_column( (GtkTreeView*)list_view, col );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( _("Length"), render,
                                                   "text", COL_LEN, NULL );
    gtk_tree_view_append_column( (GtkTreeView*)list_view, col );

    gtk_tree_view_set_search_column( (GtkTreeView*)list_view, COL_TITLE );
    tree_sel = gtk_tree_view_get_selection( (GtkTreeView*)list_view );
    gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_MULTIPLE );

    update_play_list();
}

static void on_pref( GtkMenuItem* item, gpointer user_data )
{

}

static gpointer add_file( const char* file )
{
    gboolean is_dir = g_file_test( file, G_FILE_TEST_IS_DIR );

/* older xmms2 client lib doesn't have this API, so we have to
   scan the dir ourselves. :-(
*/
#ifndef HAVE_XMMSC_PLAYLIST_RADD
    if( is_dir ) {
        const char *name;
        GDir *dir = g_dir_open( file, 0, NULL );
        if( !dir )
            return;
        while( name = g_dir_read_name( dir ) ) {
            char *path = g_build_filename( file, name, NULL );
            add_file( path );
            g_free( path );
        }
        g_dir_close( dir );
    }
    else
#endif
    {
        xmmsc_result_t *res;
        char *url;
        /* Since xmms2 uses its own url format, this is annoying but inevitable. */
        url = g_strconcat( "file://", file, NULL );

/* If we have xmmsc_playlist_radd */
#ifdef HAVE_XMMSC_PLAYLIST_RADD
        if( is_dir ) {
            res = xmmsc_playlist_radd( con, url );
        }
        else
#endif
        {
            res = xmmsc_playlist_add( con, url );
        }
        g_free( url );

        if( !res )
            return;

        xmmsc_result_wait( res );
        if( xmmsc_result_iserror( res ) ) {
            show_error( xmmsc_result_get_error(res) );
        }
        xmmsc_result_unref( res );
    }
    return NULL;
}

static void on_add_clicked(GtkButton* btn, GtkFileChooser* dlg)
{
    GSList* uris = gtk_file_chooser_get_uris( (GtkFileChooser*)dlg );
    GSList* uri;

    if( ! uris )
        return;

    for( uri = uris; uri; uri = uri->next ) {
        gchar* file = g_filename_from_uri( uri->data, NULL, NULL );
        add_file( file );
        g_free( file );
        g_free( uri->data );
    }
    g_slist_free( uris );

    gtk_dialog_response( (GtkDialog*)dlg, GTK_RESPONSE_CLOSE );
}

static void on_add_files( GtkMenuItem* item, gpointer user_data )
{
    GtkWidget *dlg = gtk_file_chooser_dialog_new( NULL, (GtkWindow*)main_win,
                                                  GTK_FILE_CHOOSER_ACTION_OPEN,
                                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL );
    GtkWidget *add_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
    gtk_dialog_add_action_widget( (GtkDialog*)dlg, add_btn, GTK_RESPONSE_CLOSE );
    gtk_widget_show( add_btn );
    g_signal_connect( add_btn, "clicked", G_CALLBACK(on_add_clicked), dlg );
    gtk_file_chooser_set_select_multiple( (GtkFileChooser*)dlg, TRUE );
    /* FIXME: We should add a custom filter which filters autio files */
    /* gtk_file_chooser_add_filter(); */
    gtk_dialog_run( (GtkDialog*)dlg );
    gtk_widget_destroy( dlg );
}

static void on_add_url( GtkMenuItem* item, gpointer user_data )
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
    if( gtk_dialog_run( (GtkDialog*)dlg ) == GTK_RESPONSE_OK ) {
        xmmsc_result_t *res;
        const char* url = gtk_entry_get_text( (GtkEntry*)entry );
        res = xmmsc_playlist_add( con, url );
        xmmsc_result_wait( res );
        if ( xmmsc_result_iserror(res) ) {
            /* FIXME: display proper error msg */
            show_error( xmmsc_result_get_error(res) );
        }
        xmmsc_result_unref( res );
    }
    gtk_widget_destroy( dlg );
}

static void on_add_from_mlib( GtkMenuItem* item, gpointer user_data )
{
    /* FIXME: This might be implemented in the future */
}

static int intcmp( gconstpointer a, gconstpointer b )
{
    return (int)b - (int)a;
}

static void on_remove_sel( GtkMenuItem* item, gpointer user_data )
{
    GtkTreeSelection* tree_sel;
    tree_sel = gtk_tree_view_get_selection( (GtkTreeView*)list_view );
    GList *sels = gtk_tree_selection_get_selected_rows( tree_sel, NULL );
    GList *sel;

    for( sel = sels; sel; sel = sel->next ) {
        GtkTreePath* path = (GtkTreePath*)sel->data;
        sel->data = (gpointer)gtk_tree_path_get_indices( path )[0];
        gtk_tree_path_free( path );
    }

    /*
        sort the list, and put rows with bigger indicies before those with smaller indicies.
        In this way, all indicies won't be changed during removing items.
    */
    sels = g_list_sort( sels, intcmp );

    for( sel = sels; sel; sel = sel->next ) {
        xmmsc_result_t* res;
        int pos = (int)sel->data;
        res = xmmsc_playlist_remove( con, pos );
        xmmsc_result_wait( res );
        xmmsc_result_unref( res );
    }

    g_list_free( sels );
}

static void on_remove_all( GtkMenuItem* item, gpointer user_data )
{
    xmmsc_result_t *res;
    res = xmmsc_playlist_clear( con );
    xmmsc_result_wait( res );
    if ( xmmsc_result_iserror(res) ) {
        show_error( xmmsc_result_get_error(res) );
    }
    xmmsc_result_unref( res );
}

static void on_shuffle( GtkMenuItem* item, gpointer user_data )
{
    xmmsc_result_t *res;
    res = xmmsc_playlist_shuffle( con );
    xmmsc_result_wait( res );
    if ( xmmsc_result_iserror(res) ) {
        show_error( xmmsc_result_get_error(res) );
    }
    xmmsc_result_unref( res );
}

static gboolean delayed_set_win_resizable( gpointer user_data )
{
    gtk_window_set_resizable((GtkWindow*)main_win, TRUE);
    return FALSE;
}

static void on_show_playlist( GtkMenuItem* item, gpointer user_data )
{
    GtkWidget* scroll = gtk_widget_get_parent(list_view);
    if( GTK_WIDGET_VISIBLE(scroll) ) {
        /* FIXME: Little dirty trick used to shrink the window.
                  Maybe gtk_window_set_geometry_hints is needed here. */
        gtk_window_set_resizable((GtkWindow*)main_win, FALSE);
        gtk_widget_hide( scroll );
        /* Since the list has been hidden, the data is not needed */
        gtk_tree_view_set_model( (GtkTreeView*)list_view, NULL );
        g_idle_add( delayed_set_win_resizable, NULL );
    }
    else {
        update_play_list();
        gtk_widget_show( scroll );
        gtk_window_set_resizable((GtkWindow*)main_win, TRUE);
    }
}

#define VERSION "0.1"

static void on_about( GtkMenuItem* item, gpointer user_data )
{
    const char* authors[] = { "洪任諭 (Hong Jen Yee) <pcman.tw@gmail.com>", NULL };
    GtkWidget* about = gtk_about_dialog_new();
    gtk_about_dialog_set_name( (GtkAboutDialog*)about, "LXMusic" );
    gtk_about_dialog_set_version( (GtkAboutDialog*)about, VERSION );
    gtk_about_dialog_set_authors( (GtkAboutDialog*)about, authors );
    gtk_about_dialog_set_comments( (GtkAboutDialog*)about, _("Music Player for LXDE\nSimple GUI XMMS2 client") );
    gtk_about_dialog_set_license( (GtkAboutDialog*)about, "GNU General Public License" );
    gtk_about_dialog_set_website( (GtkAboutDialog*)about, "http://lxde.sourceforge.net/" );
    gtk_window_set_transient_for( (GtkWindow*)about, (GtkWindow*)main_win );
    gtk_dialog_run( (GtkDialog*)about );
    gtk_widget_destroy( about );
}

static GtkWidget* create_menubar()
{
    GtkWidget *menubar, *menu, *item;

    menubar = gtk_menu_bar_new();

    /* main menu */
    item = gtk_menu_item_new_with_mnemonic( "_LXMusic" );
    gtk_menu_shell_append( (GtkMenuShell*)menubar, item );
    menu = gtk_menu_new();
    gtk_menu_item_set_submenu( (GtkMenuItem*)item, menu );

    item = gtk_image_menu_item_new_from_stock( GTK_STOCK_PREFERENCES, NULL );
    g_signal_connect( item, "activate", G_CALLBACK(on_pref), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );
    gtk_widget_set_sensitive( item, FALSE ); /* FIXME: this line should be removed in the future */

    gtk_menu_shell_append( (GtkMenuShell*)menu, gtk_separator_menu_item_new() );

    item = gtk_image_menu_item_new_from_stock( GTK_STOCK_QUIT, NULL );
    g_signal_connect( item, "activate",
                      G_CALLBACK(gtk_main_quit), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );

    /* playlist menu */
    item = gtk_menu_item_new_with_mnemonic( "_Playlist" );
    gtk_menu_shell_append( (GtkMenuShell*)menubar, item );
    menu = gtk_menu_new();
    gtk_menu_item_set_submenu( (GtkMenuItem*)item, menu );

    item = gtk_image_menu_item_new_with_mnemonic( "Add Files" );
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)item,
                                   gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
    g_signal_connect( item, "activate", G_CALLBACK(on_add_files), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );

    item = gtk_image_menu_item_new_with_mnemonic( "Add URL" );
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)item,
                                   gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
    g_signal_connect( item, "activate", G_CALLBACK(on_add_url), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );

#if 0
    item = gtk_image_menu_item_new_with_mnemonic( "Add from Media Library" );
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)item,
                                   gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
    g_signal_connect( item, "activate", G_CALLBACK(on_add_from_mlib), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );
#endif


    gtk_menu_shell_append( (GtkMenuShell*)menu, gtk_separator_menu_item_new() );

    item = gtk_image_menu_item_new_with_mnemonic( "Remove Selected Items" );
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)item,
                                   gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect( item, "activate", G_CALLBACK(on_remove_sel), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );

    item = gtk_image_menu_item_new_with_mnemonic( "Remove All" );
    gtk_image_menu_item_set_image( (GtkImageMenuItem*)item,
                                   gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
    g_signal_connect( item, "activate", G_CALLBACK(on_remove_all), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );

    gtk_menu_shell_append( (GtkMenuShell*)menu, gtk_separator_menu_item_new() );

    item = gtk_menu_item_new_with_mnemonic( "Shuffle" );
    g_signal_connect( item, "activate", G_CALLBACK(on_shuffle), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );


    /* view menu */
    item = gtk_menu_item_new_with_mnemonic( "_View" );
    gtk_menu_shell_append( (GtkMenuShell*)menubar, item );
    menu = gtk_menu_new();
    gtk_menu_item_set_submenu( (GtkMenuItem*)item, menu );

    item = gtk_check_menu_item_new_with_mnemonic( "Show _PlayList" );
    gtk_check_menu_item_set_active( (GtkCheckMenuItem*)item, TRUE );
    g_signal_connect( item, "activate", G_CALLBACK(on_show_playlist), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );


    /* help menu */
    item = gtk_menu_item_new_with_mnemonic( "_Help" );
    gtk_menu_shell_append( (GtkMenuShell*)menubar, item );
    menu = gtk_menu_new();
    gtk_menu_item_set_submenu( (GtkMenuItem*)item, menu );

    item = gtk_image_menu_item_new_from_stock( GTK_STOCK_ABOUT, NULL );
    g_signal_connect( item, "activate", G_CALLBACK(on_about), NULL );
    gtk_menu_shell_append( (GtkMenuShell*)menu, item );

    return menubar;
}

static void init_main_win()
{
    GtkWidget *vbox, *hbox, *hbox2, *scroll, *vbox2, *entry, *img, *btn, *menubar;

    tips = gtk_tooltips_new();
#if GTK_CHECK_VERSION( 2, 10, 0 )
    g_object_ref_sink(tips);
#else
    g_object_ref( tips );
    gtk_object_sink( tips );
#endif

    main_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );

    gtk_window_set_title( (GtkWindow*)main_win, "LXMusic" );
    gtk_window_set_icon_name( (GtkWindow*)main_win, "stock_volume" );
    gtk_window_set_default_size( (GtkWindow*)main_win, 280, 480 );
    g_signal_connect( main_win, "delete-event",
                      G_CALLBACK(gtk_main_quit), NULL );

    vbox = gtk_vbox_new( FALSE, 2 );
    gtk_container_add( (GtkContainer*)main_win, vbox );

    /* menu bar */
    menubar = create_menubar();
    gtk_box_pack_start( (GtkBox*)vbox, menubar, FALSE, FALSE, 0 );

    /* current song info */
    hbox = gtk_hbox_new( FALSE, 2 );
    gtk_box_pack_start( (GtkBox*)vbox, hbox, FALSE, FALSE, 0 );
    music_title = gtk_label_new("");
    gtk_label_set_ellipsize( (GtkLabel*)music_title, PANGO_ELLIPSIZE_END );
    gtk_misc_set_padding( (GtkMisc*)music_title, 2, 2 );
    gtk_box_pack_start( (GtkBox*)hbox, music_title, TRUE, TRUE, 2 );

    gtk_box_pack_start( (GtkBox*)vbox, gtk_hseparator_new(), FALSE, FALSE, 0 );

    /* playback control */
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( (GtkBox*)vbox, hbox, FALSE, FALSE, 0 );

    add_tool_btn( (GtkBox*)hbox,
                  GTK_STOCK_MEDIA_PREVIOUS, _("Previous"),
                  G_CALLBACK(on_prev_next), GINT_TO_POINTER(-1) );
    play_btn = add_tool_btn( (GtkBox*)hbox,
                             GTK_STOCK_MEDIA_PLAY, _("Play"),
                             G_CALLBACK(on_play_pause), NULL );
    update_play_btn();
    add_tool_btn( (GtkBox*)hbox,
                  GTK_STOCK_MEDIA_STOP, _("Stop"),
                  G_CALLBACK(on_stop), NULL );
    add_tool_btn( (GtkBox*)hbox,
                  GTK_STOCK_MEDIA_NEXT, _("Next"),
                  G_CALLBACK(on_prev_next), GINT_TO_POINTER(1) );

    /* time & bitrate */
    vbox2 = gtk_vbox_new( FALSE, 1 );
    hbox2 = gtk_hbox_new( FALSE, 1 );
    time_label = gtk_label_new("--:--:--");
    gtk_misc_set_alignment( (GtkMisc*)time_label, 0.0, 0.5 );
    gtk_box_pack_start( (GtkBox*)hbox2, time_label, FALSE, FALSE, 0 );
    bitrate_label = gtk_label_new("192 kbps");
    gtk_misc_set_alignment( (GtkMisc*)time_label, 1.0, 0.5 );
    gtk_box_pack_end( (GtkBox*)hbox2, bitrate_label, FALSE, FALSE, 4 );

    /* progress bar */
    progress_bar = gtk_hscale_new_with_range( 0.0, 100.0, 1.0 );
    GTK_WIDGET_UNSET_FLAGS( progress_bar, GTK_CAN_FOCUS );
    gtk_scale_set_draw_value( (GtkScale*)progress_bar, FALSE );
    gtk_box_pack_start( (GtkBox*)vbox2, hbox2, FALSE, FALSE, 0 );
    gtk_box_pack_start( (GtkBox*)vbox2, progress_bar, FALSE, FALSE, 0 );
    gtk_box_pack_start( (GtkBox*)hbox, vbox2, TRUE, TRUE, 2 );

    /* play list */
    scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( (GtkScrolledWindow*)scroll,
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_shadow_type( (GtkScrolledWindow*)scroll, GTK_SHADOW_IN );
    list_view = gtk_tree_view_new();
    gtk_container_add( (GtkContainer*)scroll, list_view );

    /* play list bar */
/*
    hbox = gtk_hbox_new( FALSE, 2 );
    gtk_box_pack_start( vbox, hbox, FALSE, FALSE, 2 );
//    entry = gtk_entry_new();
//    gtk_box_pack_start( hbox, entry, TRUE, TRUE, 2 );
    img = gtk_image_new_from_stock( GTK_STOCK_ADD,
                                    GTK_ICON_SIZE_MENU);
    btn = gtk_menu_tool_button_new( img, NULL );
    gtk_box_pack_start( hbox, btn, FALSE, FALSE, 2 );

    img = gtk_image_new_from_stock( GTK_STOCK_REMOVE,
                                    GTK_ICON_SIZE_MENU);
    btn = gtk_menu_tool_button_new( img, NULL );
    gtk_box_pack_start( hbox, btn, FALSE, FALSE, 2 );
*/
    gtk_box_pack_start( (GtkBox*)vbox, scroll, TRUE, TRUE, 2 );

    /* status bar */
    status_bar = gtk_statusbar_new();
    gtk_box_pack_start( (GtkBox*)vbox, status_bar, FALSE, FALSE, 2 );
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
            img = gtk_bin_get_child( (GtkBin*)play_btn );
            gtk_image_set_from_stock( (GtkImage*)img, GTK_STOCK_MEDIA_PAUSE,
                                      GTK_ICON_SIZE_MENU );
            break;
        case XMMS_PLAYBACK_STATUS_PAUSE:
        case XMMS_PLAYBACK_STATUS_STOP:
            gtk_tooltips_set_tip( tips, play_btn, _("Play"), NULL );
            img = gtk_bin_get_child( (GtkBin*)play_btn );
            gtk_image_set_from_stock( (GtkImage*)img, GTK_STOCK_MEDIA_PLAY,
                                      GTK_ICON_SIZE_MENU );
            break;
    }
}

static void on_playtime_changed( xmmsc_result_t* res, void* user_data )
{
    guint time;
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

    gtk_label_set_text( (GtkLabel*)time_label,
                        timeval_to_str( time, buf, G_N_ELEMENTS(buf) ) );

    if( current_time_len > 0 ) {
        gtk_range_set_value( (GtkRange*)progress_bar,
                              (((gdouble)100000 * time) / current_time_len) );
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

static void on_current_id_changed( xmmsc_result_t* res, void* user_data )
{
    if( xmmsc_result_get_uint(res, &current_id) ) {
        xmmsc_result_t *res2;
        char* name;
        res2 = xmmsc_medialib_get_info(con, current_id);
        xmmsc_result_wait ( res2 );
        name = get_song_name( res2 );
        gtk_label_set_text( (GtkLabel*)music_title, name );
        g_free( name );

        if( !xmmsc_result_get_dict_entry_int32( res2, "duration",
                                                &current_time_len) )
            current_time_len = -1;
        xmmsc_result_unref(res2);
    }
}

static void on_playlist_pos_changed( xmmsc_result_t* res, void* user_data )
{
    guint pos;
    GtkTreePath* path;
    GtkTreeSelection* sel;

    xmmsc_result_get_uint( res, &playlist_pos );
    path = gtk_tree_path_new_from_indices( playlist_pos, -1 );
    sel = gtk_tree_view_get_selection( (GtkTreeView*)list_view );
    gtk_tree_selection_select_path( sel, path );
    gtk_tree_path_free( path );
}

static void on_volume_change( xmmsc_result_t* res, void* user_data )
{

}

static void on_playlist_change( xmmsc_result_t* res, void* user_data )
{
    guint id = 0;
    int type = 0, pos = -1;
    GtkListStore* list;

    if( G_UNLIKELY( xmmsc_result_iserror( res ) ) )
        return;

    if( G_UNLIKELY( ! xmmsc_result_get_dict_entry_int32( res, "type", &type ) ) )
        return;
    GDK_THREADS_ENTER();
    list = (GtkListStore*)gtk_tree_view_get_model( (GtkTreeView*)list_view );

    switch( type ) {
        case XMMS_PLAYLIST_CHANGED_ADD:
        case XMMS_PLAYLIST_CHANGED_INSERT:
            if( G_UNLIKELY( ! xmmsc_result_get_dict_entry_int32( res, "position", &pos ) ) )
                pos = gtk_tree_model_iter_n_children( (GtkTreeModel*)list, NULL );
            if( G_LIKELY( xmmsc_result_get_dict_entry_uint32( res, "id", &id ) ) ) {
                GtkTreeIter it;
                xmmsc_result_t *res2;
                gtk_list_store_insert_with_values( list, &it, pos,
                                                   COL_ID, id, -1 );
                if( res2 = xmmsc_medialib_get_info( con, id ) ) {
                    /* FIXME: is there any better way? */
                    xmmsc_result_notifier_set( res2, update_song,
                                               gtk_tree_iter_copy( &it ) );
                    xmmsc_result_unref( res2 );
                }
            }
            break;
        case XMMS_PLAYLIST_CHANGED_REMOVE:
            if( G_LIKELY( xmmsc_result_get_dict_entry_int32( res, "position", &pos ) ) ) {
                GtkTreePath* path;
                GtkTreeIter it;
                path = gtk_tree_path_new_from_indices( pos, -1 );
                if( gtk_tree_model_get_iter( (GtkTreeModel*)list, &it, path ) ) {
                    gtk_list_store_remove( list, &it );
                }
                gtk_tree_path_free( path );
            }
            break;
        case XMMS_PLAYLIST_CHANGED_CLEAR: {
            gtk_list_store_clear( list );
            break;
        }
        case XMMS_PLAYLIST_CHANGED_MOVE:
        {
            int newpos = 0;
            if( G_UNLIKELY( xmmsc_result_get_dict_entry_int32( res, "position", &pos ) ) )
                break;
            if( G_UNLIKELY( xmmsc_result_get_dict_entry_int32( res, "newposition", &newpos ) ) )
                break;
            break;
        }
        case XMMS_PLAYLIST_CHANGED_SORT:
        case XMMS_PLAYLIST_CHANGED_SHUFFLE:
            /* FIXME: We have to reload the list here */
            update_play_list();
            break;
    }
    GDK_THREADS_LEAVE();
}

int main( int argc, char **argv )
{
    xmmsc_result_t* res;

    gtk_init( &argc, &argv );

    if( !(con = xmmsc_init ("lxmusic")) ) {
        return EXIT_FAILURE;
    }

    if( !xmmsc_connect (con, getenv ("XMMS_PATH")) ) {
        if( ! g_spawn_command_line_sync( "xmms2-launcher", NULL, NULL, NULL, NULL )
            || !xmmsc_connect (con, getenv ("XMMS_PATH")) ) {
            fprintf (stderr, "Connection failed: %s\n",
                     xmmsc_get_last_error (con));
            return EXIT_FAILURE;
        }
    }
    xmmsc_mainloop_gmain_init(con);

    init_main_win();

    /* play status */
    res = xmmsc_playback_status(con);
    xmmsc_result_wait( res );
    on_status_changed( res, NULL );
    xmmsc_result_unref(res);
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playback_status,
                       on_status_changed, NULL );

    /* current song info */
    res = xmmsc_playback_current_id( con );
    xmmsc_result_wait( res );
    on_current_id_changed( res, NULL );
    xmmsc_result_unref(res);
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playback_current_id,
                       on_current_id_changed, NULL );

    /* play time */
    XMMS_CALLBACK_SET( con , xmmsc_signal_playback_playtime,
                       on_playtime_changed, NULL);

    /* current pos in playlist */
    res = xmmsc_playlist_current_pos( con );
    xmmsc_result_wait( res );
    on_playlist_pos_changed( res, NULL );
    xmmsc_result_unref(res);
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playlist_current_pos,
                       on_playlist_pos_changed, NULL);

    /* volume */
    res = xmmsc_playback_volume_get(con);
    xmmsc_result_wait( res );
    on_volume_change( res, NULL );
    xmmsc_result_unref(res);
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playback_volume_changed,
                       on_volume_change, NULL );

    /* playlist changed */
    XMMS_CALLBACK_SET( con, xmmsc_broadcast_playlist_changed,
                       on_playlist_change, NULL );

/*
    XMMS_CALLBACK_SET(con, xmmsc_broadcast_medialib_entry_changed,
                        bc_handle_medialib_entry_changed, con);

    xmmsc_disconnect_callback_set(con, connection_lost, NULL);
*/

    gtk_widget_show_all( main_win );

    init_list_view();

    gtk_main();
    g_object_unref( tips );

    /* If the playback has been stopped, quit xmms2d, too. */
    if( status == XMMS_PLAYBACK_STATUS_STOP ) {
        xmmsc_result_t *res = xmmsc_quit( con );
        xmmsc_result_wait( res );
        xmmsc_result_unref( res );
    }

    xmmsc_unref (con);
    return EXIT_SUCCESS;
}
