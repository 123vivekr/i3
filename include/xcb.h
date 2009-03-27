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
#ifndef _XCB_H
#define _XCB_H

#include "data.h"

#define _NET_WM_STATE_REMOVE    0
#define _NET_WM_STATE_ADD       1
#define _NET_WM_STATE_TOGGLE    2

/* This is the equivalent of XC_left_ptr. I’m not sure why xcb doesn’t have a constant for that. */
#define XCB_CURSOR_LEFT_PTR     68
#define XCB_CURSOR_SB_H_DOUBLE_ARROW 108
#define XCB_CURSOR_SB_V_DOUBLE_ARROW 116

/* from X11/keysymdef.h */
#define XCB_NUM_LOCK                    0xff7f

/* The event masks are defined here because we don’t only set them once but we need to set slight
   variations of them (without XCB_EVENT_MASK_ENTER_WINDOW while rendering the layout) */
/* The XCB_CW_EVENT_MASK for the child (= real window) */
#define CHILD_EVENT_MASK (XCB_EVENT_MASK_PROPERTY_CHANGE | \
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY | \
                          XCB_EVENT_MASK_ENTER_WINDOW)

/* The XCB_CW_EVENT_MASK for its frame */
#define FRAME_EVENT_MASK (XCB_EVENT_MASK_BUTTON_PRESS |          /* …mouse is pressed/released */ \
                          XCB_EVENT_MASK_BUTTON_RELEASE | \
                          XCB_EVENT_MASK_EXPOSURE |              /* …our window needs to be redrawn */ \
                          XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | /* …user moves cursor inside our window */ \
                          XCB_EVENT_MASK_ENTER_WINDOW)           /* …the application tries to resize itself */


enum { _NET_SUPPORTED = 0,
        _NET_SUPPORTING_WM_CHECK,
        _NET_WM_NAME,
        _NET_WM_STATE_FULLSCREEN,
        _NET_WM_STATE,
        _NET_WM_WINDOW_TYPE,
        _NET_WM_WINDOW_TYPE_DOCK,
        _NET_WM_DESKTOP,
        _NET_WM_STRUT_PARTIAL,
        WM_PROTOCOLS,
        WM_DELETE_WINDOW,
        UTF8_STRING
};

extern unsigned int xcb_numlock_mask;

i3Font *load_font(xcb_connection_t *conn, const char *pattern);
uint32_t get_colorpixel(xcb_connection_t *conn, char *hex);
xcb_window_t create_window(xcb_connection_t *conn, Rect r, uint16_t window_class, int cursor,
                           uint32_t mask, uint32_t *values);
void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc, uint32_t mask, uint32_t value);
void xcb_draw_line(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t to_x, uint32_t to_y);
void xcb_draw_rect(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void fake_configure_notify(xcb_connection_t *conn, Rect r, xcb_window_t window);
void xcb_get_numlock_mask(xcb_connection_t *conn);

#endif
