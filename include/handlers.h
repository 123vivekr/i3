/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * (c) 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _HANDLERS_H
#define _HANDLERS_H

int handle_key_release(void *ignored, xcb_connection_t *conn, xcb_key_release_event_t *event);
int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event);
int handle_enter_notify(void *ignored, xcb_connection_t *conn, xcb_enter_notify_event_t *event);
int handle_button_press(void *ignored, xcb_connection_t *conn, xcb_button_press_event_t *event);
int handle_map_request(void *prophs, xcb_connection_t *conn, xcb_map_request_event_t *event);
int handle_configure_event(void *prophs, xcb_connection_t *conn, xcb_configure_notify_event_t *event);
int handle_configure_request(void *prophs, xcb_connection_t *conn, xcb_configure_request_event_t *event);
int handle_unmap_notify_event(void *data, xcb_connection_t *conn, xcb_unmap_notify_event_t *event);
int handle_windowname_change(void *data, xcb_connection_t *conn, uint8_t state,
                             xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop);
int handle_windowname_change_legacy(void *data, xcb_connection_t *conn, uint8_t state,
                                xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop);
int handle_expose_event(void *data, xcb_connection_t *conn, xcb_expose_event_t *event);
int handle_client_message(void *data, xcb_connection_t *conn, xcb_client_message_event_t *event);
int handle_window_type(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                       xcb_atom_t atom, xcb_get_property_reply_t *property);
int handle_normal_hints(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t name, xcb_get_property_reply_t *reply);

#endif
