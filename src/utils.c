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
    GError* err = NULL;
    gboolean ret = g_key_file_get_boolean(kf, grp, key, &err);
    if( err )
        g_error_free(err);
    else
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


/* Helper function for decoding urls 
   Returned value must be freed
*/
gchar* xmmsv_url_to_string (xmmsv_t *url_value) 
{
	const gchar *value;
	gchar *url = NULL;
	const unsigned char *burl;
	unsigned int blen;
	xmmsv_t *tmp;

	/* First decode the URL encoding */
	tmp = xmmsv_decode_url (url_value);
	if (tmp && xmmsv_get_bin (tmp, &burl, &blen)) {
	    url = g_malloc (blen + 1);
	    memcpy (url, burl, blen);
	    url[blen] = 0;
	    xmmsv_unref (tmp);
	}
	else
	    return NULL;
	
	/* Let's see if the result is valid utf-8. This must be done
	 * since we don't know the charset of the binary string */
	if (url && g_utf8_validate (url, -1, NULL)) {
	    /* If it's valid utf-8 we don't have any problem just
	     * printing it to the screen
	     */
	    return url;
	} else if (url) {
	    /* Not valid utf-8 :-( We make a valid guess here that
	     * the string when it was encoded with URL it was in the
	     * same charset as we have on the terminal now.
	     *
	     * THIS MIGHT BE WRONG since different clients can have
	     * different charsets and DIFFERENT computers most likely
	     * have it.
	     */
	    gchar *tmp2 = g_locale_to_utf8 (url, -1, NULL, NULL, NULL);
	    g_free( url );
	    g_warning ( "Charset guessed:", tmp2 );
	    return tmp2;
	}
	
	g_warning( "Decoding of URL Failed" );
	return NULL;
}

/* helper function to guess title from media properties */
gchar* xmmsv_media_dict_guess_title (xmmsv_t *value) 
{
    const char *url, *file, *title;
    xmmsv_t *string_value;
    gchar *decoded_val;
    
    /* online streams often provide channel */
    if ( xmmsv_dict_get( value, "channel", &string_value ) ) 
	xmmsv_get_string( string_value, &title );
    else
    {
	/*  fallback: url */
	xmmsv_dict_get( value, "url", &string_value );
	/* try to decode URL */
	decoded_val = xmmsv_url_to_string ( string_value );
	/* undecoded url string */	
	if ( decoded_val == NULL )
	    xmmsv_get_string( string_value, &file );
	else
	    file = decoded_val;
	
	file = g_utf8_strrchr ( file, -1, '/' ) + 1;
	title = file;
	if ( decoded_val )
	    g_free ( decoded_val );
    }
    return title;
}

