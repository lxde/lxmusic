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

#ifndef LXMUSIC_NOTIFY_H
#define LXMUSIC_NOTIFY_H

#include <gtk/gtk.h>
#include <libnotify/notify.h>

typedef struct _LXMusic_Notification LXMusic_Notification;

struct _LXMusic_Notification
{
    NotifyNotification*	notification;
    GtkStatusIcon *status_icon;
};
    
void lxmusic_notification_free( LXMusic_Notification *n );
LXMusic_Notification* lxmusic_notification_new( GtkStatusIcon *status_icon );
void lxmusic_do_notify ( LXMusic_Notification *n,
			const char *summary,
			const char *message );

#endif
