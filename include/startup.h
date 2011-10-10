/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _STARTUP_H
#define _STARTUP_H

#define SN_API_NOT_YET_FROZEN 1
#include <libsn/sn-monitor.h>

/**
 * Starts the given application by passing it through a shell. We use double
 * fork to avoid zombie processes. As the started application’s parent exits
 * (immediately), the application is reparented to init (process-id 1), which
 * correctly handles childs, so we don’t have to do it :-).
 *
 * The shell is determined by looking for the SHELL environment variable. If
 * it does not exist, /bin/sh is used.
 *
 */
void start_application(const char *command);

/**
 * Called by libstartup-notification when something happens
 *
 */
void startup_monitor_event(SnMonitorEvent *event, void *userdata);

/**
 * Checks if the given window belongs to a startup notification by checking if
 * the _NET_STARTUP_ID property is set on the window (or on its leader, if it’s
 * unset).
 *
 * If so, returns the workspace on which the startup was initiated.
 * Returns NULL otherwise.
 *
 */
char *startup_workspace_for_window(i3Window *cwindow, xcb_get_property_reply_t *startup_id_reply);

#endif
