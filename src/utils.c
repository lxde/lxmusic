/*
 *      utils.c
 *      
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *      Copyright 2009 Jürgen Hötzel <juergenhoetzel@users.sourceforge.net>
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

int utf8_strncasecmp( const char* s1, const char* s2, int len )
{
    const char *p1 = s1, *p2 = s2;
    int i;
    for( i = 0; i < len; ++i )
    {
        gunichar c1 = g_utf8_get_char(p1);
        gunichar c2 = g_utf8_get_char(p2);

        if( isalpha(c1) )
            c1 = tolower(c1);
        if( isalpha(c2) )
            c2 = tolower(c2);

        if( c1 != c2 )
            return c1 - c2;

        p1 = g_utf8_next_char(p1);
        p2 = g_utf8_next_char(p2);
    }
    return 0;
}

char* utf8_strcasestr( const char* s1, const char* s2 )
{
    const char *p1, *p2;
    int len = g_utf8_strlen(s2, -1);
    for( ; *s1; s1 = g_utf8_next_char(s1) )
    {
        if( 0 == utf8_strncasecmp( s1, s2, len ) )
            return s1;
    }
    return NULL;
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


const char* file_size_to_str( char* buf, guint64 size )
{
    char * unit;
    /* guint point; */
    gfloat val;

    /*
       FIXME: Is floating point calculation slower than integer division?
              Some profiling is needed here.
    */
    if ( size > ( ( guint64 ) 1 ) << 30 )
    {
        if ( size > ( ( guint64 ) 1 ) << 40 )
        {
            /*
            size /= ( ( ( guint64 ) 1 << 40 ) / 10 );
            point = ( guint ) ( size % 10 );
            size /= 10;
            */
            val = ((gfloat)size) / ( ( guint64 ) 1 << 40 );
            unit = "TB";
        }
        else
        {
            /*
            size /= ( ( 1 << 30 ) / 10 );
            point = ( guint ) ( size % 10 );
            size /= 10;
            */
            val = ((gfloat)size) / ( ( guint64 ) 1 << 30 );
            unit = "GB";
        }
    }
    else if ( size > ( 1 << 20 ) )
    {
        /*
        size /= ( ( 1 << 20 ) / 10 );
        point = ( guint ) ( size % 10 );
        size /= 10;
        */
        val = ((gfloat)size) / ( ( guint64 ) 1 << 20 );
        unit = "MB";
    }
    else if ( size > ( 1 << 10 ) )
    {
        /*
        size /= ( ( 1 << 10 ) / 10 );
        point = size % 10;
        size /= 10;
        */
        val = ((gfloat)size) / ( ( guint64 ) 1 << 10 );
        unit = "KB";
    }
    else
    {
        unit = size > 1 ? "Bytes" : "Byte";
        sprintf( buf, "%u %s", ( guint ) size, unit );
        return ;
    }
    /* sprintf( buf, "%llu.%u %s", size, point, unit ); */
    sprintf( buf, "%.1f %s", val, unit );
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
