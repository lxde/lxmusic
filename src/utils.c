/*
 *      utils.c
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
#include <config.h>
#endif

#include <stdlib.h>
#include <glib/gi18n.h>
#include "utils.h"

void kf_get_bool(GKeyFile* kf, const char* grp, const char* key, gboolean* val)
{
    GError* err = NULL;
    gboolean ret = g_key_file_get_boolean(kf, grp, key, &err);
    if( err )
        g_error_free(err);
    else
        *val = ret;
}

void kf_get_int(GKeyFile* kf, const char* grp, const char* key, int* val)
{
    gint ret;
    if (( ret = g_key_file_get_integer(kf, grp, key, NULL ) ) != 0 )
        *val = ret;
}

gchar* utf8_strcasestr( const char* s1, const char* s2 )
{
    gchar *p1 = g_utf8_casefold ( s1, -1 );
    gchar *p2 = g_utf8_casefold ( s2, -1 );

    gchar *found = g_strstr_len( p1, -1, p2 );
    g_free( p1 );
    g_free( p2 );
    
    return found;
}

const char* timeval_to_str( guint timeval, char* buf, guint buf_len )
{
    guint hr, min, sec;

    hr = timeval / 3600;
    min = timeval % 3600;
    sec = min % 60;
    min /= 60;
    if( hr > 0 )
        g_snprintf( buf, buf_len, "%.2u:%.2u:%.2u", hr, min, sec );
    else
        g_snprintf( buf, buf_len, "%.2u:%.2u", min, sec );

    return buf;
}

void show_error(GtkWindow* parent, const char* title, const char* msg)
{
    GtkMessageDialog* dlg;
    dlg = (GtkMessageDialog*)gtk_message_dialog_new_with_markup(parent, GTK_DIALOG_MODAL,
                                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", msg );
    gtk_window_set_title(GTK_WINDOW(dlg), title ? title:_("Error"));
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(GTK_WIDGET(dlg));
}

/* guess title from url value returned string must be freed  */
gchar* guess_title_from_url (const char *url) 
{
    char *decoded_val = g_uri_unescape_string (url, NULL ) ;
    gchar *title = g_strdup( g_utf8_strrchr ( decoded_val, -1, '/' ) + 1 );
    g_free ( decoded_val );
    return title;
}

GString* create_window_title( const gchar* artist, const gchar* title, gboolean is_playing ) 
{
    /* default */
    GString *window_title = g_string_new(_("LXMusic" ));

    /* playing or pause */
    if (is_playing)
    {
	if ( artist != NULL )
	    g_string_append_printf( window_title, " - %s", artist );
	if ( title != NULL )
	    g_string_append_printf( window_title, " - %s", title );
    }
    return window_title;
}

/* helper function: most likely we wan't to unref directly after
 * setting a notifier in async operation */

void xmmsc_result_notifier_set_and_unref (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data)
{
	xmmsc_result_notifier_set_full (res, func, user_data, NULL);
	xmmsc_result_unref(res);
}

	
