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


        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "display.h"
#include "screen.h"
#include "misc.h"
#include "transients.h"
#include "workspaces.h"
#include "workspacewin.h"
#include "settings.h"
#include "client.h"
#include "focus.h"
#include "stacking.h"
#include "hints.h"

typedef struct _WorkspaceSwitchData WorkspaceSwitchData;
struct _WorkspaceSwitchData
{
    Wswin *wswin;
    ScreenInfo *screen_info;
    Client *c;
};

static void
workspaceGetPosition (ScreenInfo *screen_info, int n, int * row, int * col)
{
    NetWmDesktopLayout l;
    int major_length, minor_length, tmp;

    l = screen_info->desktop_layout;
    if (l.orientation == NET_WM_ORIENTATION_HORZ)
    {
        major_length = l.cols;
        minor_length = l.rows;
    }
    else
    {
        major_length = l.rows;
        minor_length = l.cols;
    }

    *row = n / major_length;
    *col = n % major_length;

    switch (l.start)
    {
        case NET_WM_TOPRIGHT:
            *col = major_length - *col - 1;
            break;
        case NET_WM_BOTTOMLEFT:
            *row = minor_length - *row - 1;
            break;
        case NET_WM_BOTTOMRIGHT:
            *col = major_length - *col - 1;
            *row = minor_length - *row - 1;
            break;
        default:
            break;
    }

    if (l.orientation == NET_WM_ORIENTATION_VERT)
    {
        tmp = *row;
        *row = *col;
        *col = tmp;
        if ((l.start == NET_WM_TOPRIGHT) || (l.start == NET_WM_BOTTOMLEFT))
        {
            *row = l.rows - *row - 1;
            *col = l.cols - *col - 1;
        }
    }
}

static gint
workspaceGetNumber (ScreenInfo *screen_info, gint row, gint col)
{
    NetWmDesktopLayout l;
    gulong major_length, minor_length;
    guint n, tmp;

    l = screen_info->desktop_layout;
    if (l.orientation == NET_WM_ORIENTATION_HORZ)
    {
        major_length = l.cols;
        minor_length = l.rows;
    }
    else
    {
        major_length = l.rows;
        minor_length = l.cols;
    }

    if (l.orientation == NET_WM_ORIENTATION_VERT)
    {
        tmp = row;
        row = col;
        col = tmp;
        if ((l.start == NET_WM_TOPRIGHT) || (l.start == NET_WM_BOTTOMLEFT))
        {
            row = minor_length - row - 1;
            col = major_length - col - 1;
        }
    }

    switch (l.start)
    {
        case NET_WM_TOPRIGHT:
            col = major_length - col - 1;
            break;
        case NET_WM_BOTTOMLEFT:
            row = minor_length - row - 1;
            break;
        case NET_WM_BOTTOMRIGHT:
            col = major_length - col - 1;
            row = minor_length - row - 1;
            break;
        default:
            break;
    }

    n = (row * major_length) + col;
    return n;
}

static int
modify_with_wrap (int value, int by, int limit, gboolean wrap)
{
    if (by >= limit) by = limit - 1;
    value += by;
    if (value >= limit)
    {
        if (!wrap)
        {
            value = limit - 1;
        }
        else
        {
            value = value % limit;
        }
    }
    else if (value < 0)
    {
        if (!wrap)
        {
            value = 0;
        }
        else
        {
            value = (value + limit) % limit;
        }
    }
    return value;
}

static void handle_workspace_event(ScreenInfo * screen_info, Client * c, XKeyEvent * ev)
{
    int key;

    key = myScreenGetKeyPressed(screen_info, ev);

    if(c)
    {
        switch(key)
        {
            case KEY_MOVE_UP_WORKSPACE:
                workspaceMove(screen_info, -1, 0, c, ev->time);
                break;
            case KEY_MOVE_DOWN_WORKSPACE:
                workspaceMove(screen_info, 1, 0, c, ev->time);
                break;
            case KEY_MOVE_LEFT_WORKSPACE:
                workspaceMove(screen_info, 0, -1, c, ev->time);
                break;
            case KEY_MOVE_RIGHT_WORKSPACE:
                workspaceMove(screen_info, 0, 1, c, ev->time);
                break;
        }
    }

    switch(key)
    {
        case KEY_UP_WORKSPACE:
            workspaceMove(screen_info, -1, 0, NULL, ev->time);
            break;
        case KEY_DOWN_WORKSPACE:
            workspaceMove(screen_info, 1, 0, NULL, ev->time);
            break;
        case KEY_LEFT_WORKSPACE:
            workspaceMove(screen_info, 0, -1, NULL, ev->time);
            break;
        case KEY_RIGHT_WORKSPACE:
            workspaceMove(screen_info, 0, 1, NULL, ev->time);
            break;
    }
}

static eventFilterStatus
workspaceSwitchFilter (XEvent * xevent, gpointer data)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    WorkspaceSwitchData *passdata;
    eventFilterStatus status;
    int key, modifiers;
    gboolean switching;
    Client *c;

    TRACE ("entering workspaceSwitchFilter");

    passdata = (WorkspaceSwitchData *) data;

    screen_info = passdata->screen_info;
    display_info = screen_info->display_info;
    modifiers = (screen_info->params->keys[KEY_MOVE_UP_WORKSPACE].modifier |
                 screen_info->params->keys[KEY_MOVE_DOWN_WORKSPACE].modifier |
                 screen_info->params->keys[KEY_MOVE_LEFT_WORKSPACE].modifier |
                 screen_info->params->keys[KEY_MOVE_RIGHT_WORKSPACE].modifier |
                 screen_info->params->keys[KEY_LEFT_WORKSPACE].modifier |
                 screen_info->params->keys[KEY_RIGHT_WORKSPACE].modifier |
                 screen_info->params->keys[KEY_UP_WORKSPACE].modifier |
                 screen_info->params->keys[KEY_DOWN_WORKSPACE].modifier);
    status = EVENT_FILTER_STOP;

    switching = TRUE;

    /* Update the display time */
    myDisplayUpdateCurrentTime (display_info, xevent);

    switch (xevent->type)
    {
        case KeyPress:
            key = myScreenGetKeyPressed (screen_info, (XKeyEvent *) xevent);
            c = clientGetFocus ();
            if (c)
            {
                if (key == KEY_MOVE_UP_WORKSPACE |
                    key == KEY_MOVE_DOWN_WORKSPACE |
                    key == KEY_MOVE_LEFT_WORKSPACE |
                    key == KEY_MOVE_RIGHT_WORKSPACE)
                {
                    handle_workspace_event (screen_info, c, (XKeyEvent *) xevent);
                    wswinSetSelected (passdata->wswin, screen_info->current_ws);
                }
            }

            if (key == KEY_UP_WORKSPACE |
                key == KEY_DOWN_WORKSPACE |
                key == KEY_LEFT_WORKSPACE |
                key == KEY_RIGHT_WORKSPACE)
            {
                handle_workspace_event(screen_info, NULL, (XKeyEvent *) xevent);
                wswinSetSelected(passdata->wswin, screen_info->current_ws);
            }
            /* If last key press event didn't have our modifiers pressed, finish workspace switching */
            if (!(xevent->xkey.state & modifiers))
            {
                switching = FALSE;
            }
            break;
        case KeyRelease:
            {
                int keysym = XLookupKeysym (&xevent->xkey, 0);

                if (IsModifierKey(keysym))
                {
                    if (!(myScreenGetModifierPressed (screen_info) & modifiers))
                    {
                        switching = FALSE;
                    }
                }
            }
            break;
        case ButtonPress:
        case ButtonRelease:
        case EnterNotify:
        case MotionNotify:
            break;
        default:
            status = EVENT_FILTER_CONTINUE;
            break;
    }

    if (!switching)
    {
        //TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
workspaceSwitchInteractive (ScreenInfo * screen_info, Client * c, XKeyEvent * ev)
{
    /* TODO: add trace statements (to all code) */
    /* TODO: add support for move with current window */
    DisplayInfo *display_info;
    gboolean g1, g2;
    WorkspaceSwitchData passdata;
    int key, modifier;

    display_info = screen_info->display_info;

    modifier = 0;
    key = myScreenGetKeyPressed (screen_info, ev);
    switch(key)
    {
        case KEY_MOVE_UP_WORKSPACE:
        case KEY_MOVE_DOWN_WORKSPACE:
        case KEY_MOVE_LEFT_WORKSPACE:
        case KEY_MOVE_RIGHT_WORKSPACE:
        case KEY_UP_WORKSPACE:
        case KEY_DOWN_WORKSPACE:
        case KEY_LEFT_WORKSPACE:
        case KEY_RIGHT_WORKSPACE:
            modifier = screen_info->params->keys[key].modifier;
    }

    if (!modifier)
    {
        /*
         * The shortcut has no modifier so there's no point in entering
         * the cycle loop, and we already servied the event above
         * that's it...
         */
        handle_workspace_event(screen_info, c, ev);
        return;
    }

    g1 = myScreenGrabKeyboard (screen_info, ev->time);
    g2 = myScreenGrabPointer (screen_info, LeaveWindowMask,  None, ev->time);

    if (!g1 || !g2)
    {
        TRACE ("grab failed in workspaceSwitchInteractive");

        gdk_beep ();
        myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
        myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));

        return;
    }

    handle_workspace_event(screen_info, c, ev);

    passdata.c = NULL;
    passdata.screen_info = screen_info;
    passdata.wswin = wswinCreate(screen_info);

    TRACE ("entering worspace loop");

    eventFilterPush (display_info->xfilter, workspaceSwitchFilter, &passdata);
    gtk_main ();
    eventFilterPop (display_info->xfilter);

    TRACE("leaving workspace loop");

    updateXserverTime(display_info);

    wswinDestroy(passdata.wswin);
    g_free(passdata.wswin);

    myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));
}

/* returns TRUE if the workspace was changed, FALSE otherwise */
gboolean
workspaceMove (ScreenInfo *screen_info, gint rowmod, gint colmod, Client * c, guint32 timestamp)
{
    gint row, col, newrow, newcol, n;
    guint previous_ws;

    g_return_val_if_fail (screen_info != NULL, FALSE);

    TRACE ("entering workspaceMove");

    workspaceGetPosition (screen_info, screen_info->current_ws, &row, &col);
    newrow = modify_with_wrap (row, rowmod, screen_info->desktop_layout.rows, screen_info->params->wrap_layout);
    newcol = modify_with_wrap (col, colmod, screen_info->desktop_layout.cols, screen_info->params->wrap_layout);
    n = workspaceGetNumber (screen_info, newrow, newcol);

    if (n == (gint) screen_info->current_ws)
    {
        return FALSE;
    }

    previous_ws = screen_info->current_ws;
    if ((n >= 0) && (n < (gint) screen_info->workspace_count))
    {
        workspaceSwitch (screen_info, n, c, TRUE, timestamp);
    }
    else if (screen_info->params->wrap_layout)
    {
        if (colmod < 0)
        {
           n = screen_info->workspace_count - 1;
        }
        else
        {
            if (colmod > 0)
            {
                newcol = 0;
            }
            else if (rowmod > 0)
            {
                newrow = 0;
            }
            else if (rowmod < 0)
            {
                newrow--;
            }
            else
            {
                return FALSE;
            }

            n = workspaceGetNumber (screen_info, newrow, newcol);
        }
        workspaceSwitch (screen_info, n, c, TRUE, timestamp);
    }

    return (screen_info->current_ws != previous_ws);
}

void
workspaceSwitch (ScreenInfo *screen_info, gint new_ws, Client * c2, gboolean update_focus, guint32 timestamp)
{
    DisplayInfo *display_info;
    Client *c, *new_focus;
    Client *previous;
    GList *list;
    Window dr, window;
    gint rx, ry, wx, wy;
    unsigned int mask;

    g_return_if_fail (screen_info != NULL);

    TRACE ("entering workspaceSwitch");

    display_info = screen_info->display_info;
    if ((new_ws == (gint) screen_info->current_ws) && (screen_info->params->toggle_workspaces))
    {
        new_ws = (gint) screen_info->previous_ws;
    }

    if (new_ws == (gint) screen_info->current_ws)
    {
        return;
    }

    if (screen_info->params->wrap_cycle)
    {
        if (new_ws > (gint) screen_info->workspace_count - 1)
        {
            new_ws = 0;
        }
        if (new_ws < 0)
        {
            new_ws = (gint) screen_info->workspace_count - 1;
        }
    }
    else if ((new_ws > (gint) screen_info->workspace_count - 1) || (new_ws < 0))
    {
        return;
    }

    screen_info->previous_ws = screen_info->current_ws;
    screen_info->current_ws = new_ws;

    new_focus = NULL;
    previous  = NULL;
    c = clientGetFocus ();

    if (c2)
    {
        clientSetWorkspace (c2, new_ws, FALSE);
    }

    if (c)
    {
        if (c->type & WINDOW_REGULAR_FOCUSABLE)
        {
            previous = c;
        }
        if (c2 == c)
        {
            new_focus = c2;
        }
    }

    /* First pass: Show, from top to bottom */
    for (list = g_list_last(screen_info->windows_stack); list; list = g_list_previous (list))
    {
        c = (Client *) list->data;
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            clientSetWorkspace (c, new_ws, TRUE);
        }
        else if (new_ws == (gint) c->win_workspace)
        {
            if (!FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED) && !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
            {
                if (!clientIsTransientOrModal (c) || !clientTransientOrModalHasAncestor (c, new_ws))
                {
                    clientShow (c, FALSE);
                }
            }
        }
    }

    /* Second pass: Hide from bottom to top */
    for (list = screen_info->windows_stack; list; list = g_list_next (list))
    {
        c = (Client *) list->data;

        if (new_ws != (gint) c->win_workspace)
        {
            if (c == previous)
            {
                FLAG_SET (previous->xfwm_flags, XFWM_FLAG_FOCUS);
                clientSetFocus (screen_info, NULL, timestamp, FOCUS_IGNORE_MODAL);
            }
            if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE) && !FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
            {
                if (!clientIsTransientOrModal (c) || !clientTransientOrModalHasAncestor (c, new_ws))
                {
                    clientWithdraw (c, new_ws, FALSE);
                }
            }
        }
    }

    /* Third pass: Check for focus, from top to bottom */
    for (list = g_list_last(screen_info->windows_stack); list; list = g_list_previous (list))
    {
        c = (Client *) list->data;

        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            if ((!new_focus) && (c == previous) && clientSelectMask (c, NULL, 0, WINDOW_REGULAR_FOCUSABLE))
            {
                new_focus = c;
            }
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_FOCUS);
        }
        else if (new_ws == (gint) c->win_workspace)
        {
            if ((!new_focus) && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_FOCUS))
            {
                new_focus = c;
            }
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_FOCUS);
        }
    }

    setNetCurrentDesktop (display_info, screen_info->xroot, new_ws);
    if (!(screen_info->params->click_to_focus))
    {
        if (!(c2) && (XQueryPointer (myScreenGetXDisplay (screen_info), screen_info->xroot, &dr, &window, &rx, &ry, &wx, &wy, &mask)))
        {
            c = clientAtPosition (screen_info, rx, ry, NULL);
            if (c)
            {
                new_focus = c;
            }
        }
    }

    if (update_focus)
    {
        if (new_focus)
        {
            if ((screen_info->params->click_to_focus) && (screen_info->params->raise_on_click))
            {
                if (!(screen_info->params->raise_on_focus) && !clientIsTopMost (new_focus))
                {
                    clientRaise (new_focus, None);
                }
            }
            clientSetFocus (screen_info, new_focus, timestamp, FOCUS_SORT);
        }
        else
        {
            clientFocusTop (screen_info, WIN_LAYER_FULLSCREEN, timestamp);
        }
    }
}

void
workspaceSetNames (ScreenInfo * screen_info, gchar **names, int items)
{
    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (names != NULL);

    TRACE ("entering workspaceSetNames");

    if (screen_info->workspace_names)
    {
        g_strfreev (screen_info->workspace_names);
    }

    screen_info->workspace_names = names;
    screen_info->workspace_names_items = items;
}

void
workspaceSetCount (ScreenInfo * screen_info, guint count)
{
    DisplayInfo *display_info;
    Client *c;
    GList *list;

    g_return_if_fail (screen_info != NULL);

    TRACE ("entering workspaceSetCount");

    if (count < 1)
    {
        count = 1;
    }
    if (count == screen_info->workspace_count)
    {
        return;
    }

    display_info = screen_info->display_info;
    setHint (display_info, screen_info->xroot, NET_NUMBER_OF_DESKTOPS, count);
    screen_info->workspace_count = count;

    for (list = screen_info->windows_stack; list; list = g_list_next (list))
    {
        c = (Client *) list->data;
        if (c->win_workspace > count - 1)
        {
            clientSetWorkspace (c, count - 1, TRUE);
        }
    }
    if (screen_info->current_ws > count - 1)
    {
        workspaceSwitch (screen_info, count - 1, NULL, TRUE, myDisplayGetCurrentTime (display_info));
    }
    setNetWorkarea (display_info, screen_info->xroot, screen_info->workspace_count,
                    screen_info->width, screen_info->height, screen_info->margins);
    /* Recompute the layout based on the (changed) number of desktops */
    getDesktopLayout (display_info, screen_info->xroot, screen_info->workspace_count,
                     &screen_info->desktop_layout);
}

void
workspaceInsert (ScreenInfo * screen_info, guint position)
{
    Client *c;
    GList *list;
    guint count;

    g_return_if_fail (screen_info != NULL);

    TRACE ("entering workspaceInsert");

    count = screen_info->workspace_count;
    workspaceSetCount(screen_info, count + 1);

    if (position > count)
    {
        return;
    }

    for (list = screen_info->windows_stack; list; list = g_list_next (list))
    {
        c = (Client *) list->data;
        if (c->win_workspace >= position)
        {
            clientSetWorkspace (c, c->win_workspace + 1, TRUE);
        }
    }
}

void
workspaceDelete (ScreenInfo * screen_info, guint position)
{
    Client *c;
    guint i, count;

    g_return_if_fail (screen_info != NULL);

    TRACE ("entering workspaceDelete");

    count = screen_info->workspace_count;
    if ((count < 1) || (position > count))
    {
        return;
    }

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        if (c->win_workspace > position)
        {
            clientSetWorkspace (c, c->win_workspace - 1, TRUE);
        }
    }

    workspaceSetCount(screen_info, count - 1);
}

void
workspaceUpdateArea (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
    Client *c;
    int prev_top;
    int prev_left;
    int prev_right;
    int prev_bottom;
    guint i;

    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (screen_info->margins != NULL);
    g_return_if_fail (screen_info->gnome_margins != NULL);

    TRACE ("entering workspaceUpdateArea");

    display_info = screen_info->display_info;
    prev_top = screen_info->margins[STRUTS_TOP];
    prev_left = screen_info->margins[STRUTS_LEFT];
    prev_right = screen_info->margins[STRUTS_RIGHT];
    prev_bottom = screen_info->margins[STRUTS_BOTTOM];

    for (i = 0; i < 4; i++)
    {
        screen_info->margins[i] = screen_info->gnome_margins[i];
    }

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT)
            && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
        {
            screen_info->margins[STRUTS_TOP]    = MAX (screen_info->margins[STRUTS_TOP],    c->struts[STRUTS_TOP]);
            screen_info->margins[STRUTS_LEFT]   = MAX (screen_info->margins[STRUTS_LEFT],   c->struts[STRUTS_LEFT]);
            screen_info->margins[STRUTS_RIGHT]  = MAX (screen_info->margins[STRUTS_RIGHT],  c->struts[STRUTS_RIGHT]);
            screen_info->margins[STRUTS_BOTTOM] = MAX (screen_info->margins[STRUTS_BOTTOM], c->struts[STRUTS_BOTTOM]);
        }
    }

    if ((prev_top != screen_info->margins[STRUTS_TOP]) || (prev_left != screen_info->margins[STRUTS_LEFT])
        || (prev_right != screen_info->margins[STRUTS_RIGHT])
        || (prev_bottom != screen_info->margins[STRUTS_BOTTOM]))
    {
        TRACE ("Margins have changed, updating net_workarea");
        setNetWorkarea (display_info, screen_info->xroot, screen_info->workspace_count,
                        screen_info->width, screen_info->height, screen_info->margins);
        /* Also prevent windows from being off screen, just like when screen is resized */
        clientScreenResize(screen_info, FALSE);
    }
}

