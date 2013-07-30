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

#ifndef INC_WORKSPACEWIN_H
#define INC_WORKSPACEWIN_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "focus.h"

typedef struct _Wswin            Wswin;
typedef struct _WswinWidget      WswinWidget;
typedef struct _WswinWidgetClass WswinWidgetClass;

struct _Wswin
{
    GList *wswin_list;
};

struct _WswinWidget
{
    GtkWindow __parent__;
    /* The below must be freed when destroying */
    GList *widgets;

    /* these don't have to be */
    Wswin *wswin;
    GtkWidget *class;
    GtkWidget *label;
    GtkWidget *container;

    gboolean undrawn;
    gint monitor_num;
    gint width, height;
    gint cols;
    gint rows;
    gint count;
    gint selected;
    gint previous;
};

struct _WswinWidgetClass
{
    GtkWindowClass __parent__;
};

Wswin                  *wswinCreate                             (ScreenInfo *);
void                   wswinDestroy                             (Wswin *);
void                   wswinSetSelected                         (Wswin *, gint);

#endif /* INC_WORKSPACEWIN_H */
