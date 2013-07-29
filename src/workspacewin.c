/*      $Id$

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
        MA 02110-1301, USA.


        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "focus.h"
#include "workspacewin.h"

static WswinWidget *
wswinCreateWidget ()
{
    WswinWidget *wsw;

    return wsw;
}

Wswin *
wswinCreate (ScreenInfo *screen_info)
{
    Wswin *wswin;
    int num_monitors, i;

    wswin = g_new0(Wswin, 1);

    num_monitors = myScreenGetNumMonitors (screen_info);
    for (i = 0; i < num_monitors; i++)
    {
        gint monitor_index;

        monitor_index = myScreenGetMonitorIndex(screen_info, i);
        wswin->wswin_list = g_list_append(wswin->wswin_list, wswinCreateWidget());
    }

    return wswin;
}

void
wswinDestroy (Wswin *w)
{
    GList *wswin_list;
    WswinWidget *wsw;

    g_return_if_fail (w != NULL);
    TRACE ("entering wswinDestroy");

    for (wswin_list = w->wswin_list; wswin_list; wswin_list = g_list_next (wswin_list))
    {
        wsw = (WswinWidget *) wswin_list->data;
        g_list_free (wsw->widgets);
        gtk_widget_destroy (GTK_WIDGET (wsw));
    }
    g_list_free (w->wswin_list);
}

