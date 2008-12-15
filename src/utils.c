/*
 *      utils.c
 *      
 *      Copyright 2008 PCMan <pcman@thinkpad>
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
    dlg = gtk_message_dialog_new_with_markup(parent, GTK_DIALOG_MODAL,
                                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, msg );
    gtk_window_set_title(parent, title ? title:_("Error"));
    gtk_dialog_run(dlg);
    gtk_widget_destroy(dlg);
}
