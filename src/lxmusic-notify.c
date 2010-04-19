/*
 *      lxmusic-notify.c
 *      
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "lxmusic-notify.h"

#define LXMUSIC_NOTIFY_NOTIFICATION_SHOW( notify )  do  { GError *error = NULL; 	\
    if (!notify_notification_show (notify, &error)) { 					\
    	g_warning ("Failed to show notification: %s", error->message); 			\
	g_error_free (error); 								\
    }											\
} while (0);							

static NotifyNotification* lxmusic_do_notify_int(const gchar *artist, const gchar *title, const char *summary);

void lxmusic_do_notify_status_icon( const gchar *artist, const gchar *title, const char *summary, GtkStatusIcon* status_icon) 
{
    NotifyNotification* notify = lxmusic_do_notify_int( artist, title, summary );
    g_return_if_fail (notify != NULL);

    notify_notification_attach_to_status_icon( notify, status_icon );
    LXMUSIC_NOTIFY_NOTIFICATION_SHOW( notify );
}

void lxmusic_do_notify_pixbuf( const gchar *artist, const gchar *title, const char *summary, GdkPixbuf* pixbuf) 
{
    NotifyNotification* notify = lxmusic_do_notify_int(artist, title, summary);
    g_return_if_fail (notify != NULL);
    notify_notification_set_icon_from_pixbuf ( notify, pixbuf );
    LXMUSIC_NOTIFY_NOTIFICATION_SHOW( notify );
}


static NotifyNotification* lxmusic_do_notify_int(const gchar *artist, const gchar *title, const char *summary)
{
    GString* message = g_string_new("");
    if ( (artist != NULL) && (title != NULL ) ) {	
	/* metadata available */
	g_string_append_printf(message, "<b>%s: </b><i>%s</i>", _("Artist"), artist );
	g_string_append_printf(message, "\n<b>%s: </b><i>%s</i>", _("Title"), title );
    }
    /* use filename without markup */
    else 			
	g_string_append( message, title );
    
    NotifyNotification *notify = notify_notification_new (summary, message->str, "lxmusic", NULL);
    notify_notification_set_urgency (notify, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout (notify, NOTIFY_EXPIRES_DEFAULT);
    g_string_free( message, TRUE );
    return notify;
}

