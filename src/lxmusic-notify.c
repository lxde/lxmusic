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
#include "lxmusic-notify.h"


LXMusic_Notification* lxmusic_notification_new( GtkStatusIcon *status_icon ) 
{
    LXMusic_Notification *n = g_slice_new( LXMusic_Notification );
    n->notification = NULL;
    n->status_icon = status_icon;
    return n;
}

void lxmusic_notification_free(  LXMusic_Notification *n )
{
    g_slice_free (LXMusic_Notification, n);
}

static void
lxmusic_clear_notify (LXMusic_Notification *n)
{
    if ( n->notification != NULL ) 
	notify_notification_close (n->notification, NULL);
    n->notification = NULL;
}

void
lxmusic_do_notify (LXMusic_Notification *n,
		   const char *summary,
		   const char *message )
{
	NotifyNotification *notify;
	GError *error = NULL;

	g_return_if_fail (n != NULL);
	g_return_if_fail (summary != NULL);
	g_return_if_fail (message != NULL);

	lxmusic_clear_notify (n);

	notify = notify_notification_new (summary, message,
	                                  "lxmusic", NULL);
	n->notification = notify;

	notify_notification_attach_to_status_icon (notify, n->status_icon);
	notify_notification_set_urgency (notify, NOTIFY_URGENCY_NORMAL);
	notify_notification_set_timeout (notify, NOTIFY_EXPIRES_DEFAULT);

	if (!notify_notification_show (notify, &error)) {
	    g_warning ("Failed to show notification: %s", error->message);
	    g_error_free (error);
	}
}

