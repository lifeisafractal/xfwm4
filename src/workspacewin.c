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

#ifndef WIN_BORDER_WIDTH
#define WIN_BORDER_WIDTH 1
#endif

#ifndef WIN_ALPHA
#define WIN_ALPHA 0.85
#endif

#ifndef WIN_BORDER_ALPHA
#define WIN_BORDER_ALPHA 0.5
#endif

#ifndef WIN_BORDER_RADIUS
#define WIN_BORDER_RADIUS 10
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "focus.h"
#include "workspacewin.h"

static void wswin_widget_class_init (WswinWidgetClass *klass);

static GType
wswin_widget_get_type (void)
{
    static GType type = G_TYPE_INVALID;

    if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
        static const GTypeInfo info =
        {
            sizeof (WswinWidgetClass),
            NULL,
            NULL,
            (GClassInitFunc) wswin_widget_class_init,
            NULL,
            NULL,
            sizeof (WswinWidget),
            0,
            NULL,
            NULL,
        };

        type = g_type_register_static (GTK_TYPE_WINDOW, "Xfwm4WswinWidget", &info, 0);
    }

    return type;
}

static gboolean
wswinConfigure (WswinWidget *wsw, GdkEventConfigure *event)
{
    GtkWindow *window;
    GdkRectangle monitor;
    GdkScreen *screen;
    gint x, y;

    g_return_val_if_fail(wsw != NULL, FALSE);
    TRACE ("entering wswinConfigure");

    if ((wsw->width == event->width) && (wsw->height == event->height))
    {
        return FALSE;
    }

    window = GTK_WINDOW(wsw);
    screen = gtk_window_get_screen(window);
    gdk_screen_get_monitor_geometry (screen, wsw->monitor_num, &monitor);
    x = monitor.x + (monitor.width - event->width) / 2;
    y = monitor.y + (monitor.height - event->height) / 2;
    gtk_window_move (window, x, y);

    wsw->width = event->width;
    wsw->height = event->height;

    return FALSE;
}

static void
wswin_widget_class_init (WswinWidgetClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
}

static GdkColor *
get_color (GtkWidget *win, GtkStateType state_type)
{
    GtkStyle *style;

    g_return_val_if_fail (win != NULL, NULL);
    g_return_val_if_fail (GTK_IS_WIDGET (win), NULL);
    g_return_val_if_fail (GTK_WIDGET_REALIZED (win), NULL);

    style = gtk_rc_get_style (win);
    if (!style)
    {
        style = gtk_widget_get_style (win);
    }
    if (!style)
    {
        style = gtk_widget_get_default_style ();
    }

    return (&style->bg[state_type]);
}

static void
wswinDraw (WswinWidget *wsw)
{
    GdkScreen *screen;
    cairo_t *cr;
    Wswin *wswin = wsw->wswin;
    GdkColor *bg_normal = get_color(GTK_WIDGET (wsw), GTK_STATE_NORMAL);
    GdkColor *bg_selected = get_color(GTK_WIDGET (wsw), GTK_STATE_SELECTED);
    gdouble width = GTK_WIDGET(wsw)->allocation.width;
    gdouble height = GTK_WIDGET(wsw)->allocation.height;
    gdouble x_size = width / wswin->cols;
    gdouble y_size = height / wswin->rows;
    gint border_radius = WIN_BORDER_RADIUS;
    gdouble border_alpha = WIN_BORDER_ALPHA;
    gdouble alpha = WIN_ALPHA;
    gdouble degrees = 3.14 / 180.0;
    gboolean composited;
    gint row, col, current;

    cr = gdk_cairo_create (GTK_WIDGET(wsw)->window);
    if (G_UNLIKELY (cr == NULL))
      return;

    screen = gtk_widget_get_screen(GTK_WIDGET(wsw));
    composited = gdk_screen_is_composited(screen);

    cairo_set_line_width (cr, 1);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    /* TODO: use workspaces.c style workspaceGetPosition so that we respect freedestop.org's layout stuff */
    /* TODO: creawte a workspace widget for each individual workspace so that they can be redrawn individually (more efficent and modular) */
    for(row = 0; row < wswin->rows; row++)
    {
        for(col = 0; col < wswin->cols; col++)
        {
            current = (row * wswin->cols) + col;
            if(current >= wswin->count)
                continue;

            /* only draw if we are the current or previous work space */
            /* draw no matter what if we are not yet drawn */
            //if((wsw->undrawn == FALSE) && (current != wswin->selected) && (current != wswin->previous))
            //    continue;

            /* Draw a filled rounded rectangle with an outline */
            gint top, bot, left, right;
            top = row*y_size+5;
            bot = (row+1)*y_size-5;
            left = col*x_size+5;
            right = (col+1)*x_size-5;

            cairo_arc (cr, right - border_radius - 0.5, top + border_radius + 0.5, border_radius, -90 * degrees, 0 * degrees);
            cairo_arc (cr, right - border_radius - 0.5, bot - border_radius - 0.5, border_radius, 0 * degrees, 90 * degrees);
            cairo_arc (cr, left + border_radius + 0.5, bot - border_radius - 0.5, border_radius, 90 * degrees, 180 * degrees);
            cairo_arc (cr, left + border_radius + 0.5, top + border_radius + 0.5, border_radius, 180 * degrees, 270 * degrees);
            cairo_close_path(cr);

            if(composited)
                cairo_set_source_rgba (cr, bg_selected->red/65535.0, bg_selected->green/65535.0, bg_selected->blue/65535.0, border_alpha);
            else
                gdk_cairo_set_source_color(cr, bg_normal);

            if(current == wswin->selected)
                gdk_cairo_set_source_color(cr, bg_selected);
            cairo_fill_preserve (cr);
            gdk_cairo_set_source_color(cr, bg_selected);
            cairo_stroke (cr);
        }
    }

    cairo_destroy (cr);

    wsw->undrawn = FALSE;
}

static gboolean
wswin_expose (GtkWidget *wsw, GdkEventExpose *event, gpointer data)
{
    WswinWidget *wswin;
    GdkScreen *screen;
    cairo_t *cr;
    GdkColor *bg_normal = get_color(wsw, GTK_STATE_NORMAL);
    GdkColor *bg_selected = get_color(wsw, GTK_STATE_SELECTED);
    gdouble width = wsw->allocation.width;
    gdouble height = wsw->allocation.height;
    gdouble border_alpha = WIN_BORDER_ALPHA;
    gdouble alpha = WIN_ALPHA;
    gint border_radius = WIN_BORDER_RADIUS;
    gdouble degrees = 3.14 / 180.0;

    screen = gtk_widget_get_screen(GTK_WIDGET(wsw));
    cr = gdk_cairo_create (wsw->window);
    if (G_UNLIKELY (cr == NULL))
      return FALSE;

    wswin = (WswinWidget *) data;

    cairo_set_line_width (cr, 1);

    if (gdk_screen_is_composited(screen)) {
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        gdk_cairo_region(cr, event->region);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.0);
        cairo_fill_preserve(cr);
        cairo_clip(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

        /* Draw a filled rounded rectangle with an outline */
        cairo_arc (cr, width - border_radius - 0.5, border_radius + 0.5, border_radius, -90 * degrees, 0 * degrees);
        cairo_arc (cr, width - border_radius - 0.5, height - border_radius - 0.5, border_radius, 0 * degrees, 90 * degrees);
        cairo_arc (cr, border_radius + 0.5, height - border_radius - 0.5, border_radius, 90 * degrees, 180 * degrees);
        cairo_arc (cr, border_radius + 0.5, border_radius + 0.5, border_radius, 180 * degrees, 270 * degrees);
        cairo_close_path(cr);
        gdk_cairo_set_source_color(cr, bg_normal);
        //cairo_set_source_rgba (cr, bg_normal->red/65535.0, bg_normal->green/65535.0, bg_normal->blue/65535.0, alpha);
        cairo_fill_preserve (cr);
        gdk_cairo_set_source_color(cr, bg_selected);
        //cairo_set_source_rgba (cr, bg_selected->red/65535.0, bg_selected->green/65535.0, bg_selected->blue/65535.0, border_alpha);
        cairo_stroke (cr);
    }
    else{
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_rectangle(cr, 0, 0, width, height);
        gdk_cairo_set_source_color(cr, bg_normal);
        cairo_fill_preserve (cr);
        gdk_cairo_set_source_color(cr, bg_selected);
        cairo_stroke (cr);
    }

    cairo_destroy (cr);

    wswinDraw(wswin);

    return FALSE;
}

static WswinWidget *
wswinCreateWidget (Wswin *wswin, ScreenInfo *screen_info, gint monitor_num)
{
    WswinWidget *wsw;
    GdkScreen *screen;
    GdkRectangle monitor;

    wsw = g_object_new(wswin_widget_get_type(), "type", GTK_WINDOW_POPUP, NULL);

    wsw->width = -1;
    wsw->height = -1;
    wsw->monitor_num = monitor_num;
    wsw->wswin = wswin;
    wsw->undrawn = TRUE;

    gtk_window_set_screen (GTK_WINDOW (wsw), screen_info->gscr);
    /* TODO: give this its own name like "xfwm4-wswin" */
    /* for now we leave it the same for compatibility with themes */
    gtk_widget_set_name (GTK_WIDGET (wsw), "xfwm4-tabwin");

    /* Check for compositing and set colormap for it */
    screen = gtk_widget_get_screen(GTK_WIDGET(wsw));
    if(gdk_screen_is_composited(screen)) {
        GdkColormap *cmap = gdk_screen_get_rgba_colormap(screen);
        if(cmap)
            gtk_widget_set_colormap(GTK_WIDGET(wsw), cmap);
    }

    gtk_widget_realize (GTK_WIDGET (wsw));
    gtk_container_set_border_width (GTK_CONTAINER (wsw), 4);
    gtk_widget_set_app_paintable(GTK_WIDGET(wsw), TRUE);
    gtk_window_set_position (GTK_WINDOW (wsw), GTK_WIN_POS_NONE);
    gdk_screen_get_monitor_geometry (screen_info->gscr, wsw->monitor_num, &monitor);
    gtk_window_move (GTK_WINDOW(wsw), monitor.x + monitor.width / 2,
                                      monitor.y + monitor.height / 2);
    /* TODO: set the size to be a param */
    gtk_window_set_default_size(GTK_WINDOW(wsw), 64*3*(wswin->cols), 40*3*(wswin->rows));

    g_signal_connect_swapped (wsw, "configure-event",
                                  GTK_SIGNAL_FUNC (wswinConfigure), (gpointer) wsw);
    g_signal_connect (wsw, "expose-event", GTK_SIGNAL_FUNC (wswin_expose), (gpointer) wsw);

    gtk_widget_show_all(GTK_WIDGET(wsw));

    return wsw;
}

void
wswinSetSelected (Wswin *wswin, gint new_ws)
{
    GList *wswin_list;
    WswinWidget *wsw;

    g_return_if_fail ( wswin != NULL );

    /* Param checking */
    if ( (wswin->selected != new_ws) && (new_ws < (wswin->count)) )
    {
        wswin->previous = wswin->selected;
        wswin->selected = new_ws;
    }
    else
        return;

    for(wswin_list = wswin->wswin_list; wswin_list; wswin_list = g_list_next (wswin_list))
    {
        wsw = (WswinWidget *) wswin_list->data;
        if(new_ws < (wswin->count))
        {
                //wswinDraw(wsw);
                gtk_widget_queue_draw (GTK_WIDGET(wsw));
        }
    }

}

Wswin *
wswinCreate (ScreenInfo *screen_info)
{
    Wswin *wswin;
    int num_monitors, i;

    g_return_val_if_fail (screen_info, NULL);

    TRACE ("entering wswinCreate");
    wswin = g_new0(Wswin, 1);
    wswin->wswin_list = NULL;
    wswin->rows = screen_info->desktop_layout.rows;
    wswin->cols = screen_info->desktop_layout.cols;
    wswin->count = screen_info->workspace_count;
    wswin->selected = screen_info->current_ws;
    wswin->previous = wswin->selected;
    num_monitors = myScreenGetNumMonitors (screen_info);
    for (i = 0; i < num_monitors; i++)
    {
        gint monitor_index;

        monitor_index = myScreenGetMonitorIndex(screen_info, i);
        wswin->wswin_list = g_list_append(wswin->wswin_list, wswinCreateWidget(wswin, screen_info, monitor_index));
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

    g_return_if_fail (w != NULL);
    for (wswin_list = w->wswin_list; wswin_list; wswin_list = g_list_next (wswin_list))
    {
        wsw = (WswinWidget *) wswin_list->data;
        g_list_free (wsw->widgets);
        gtk_widget_destroy (GTK_WIDGET (wsw));
    }
    g_list_free (w->wswin_list);
}

