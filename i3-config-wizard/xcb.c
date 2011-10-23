/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <X11/keysym.h>

#include "xcb.h"

extern xcb_window_t root;
unsigned int xcb_numlock_mask;

/*
 * Opens the window we use for input/output and maps it
 *
 */
xcb_window_t open_input_window(xcb_connection_t *conn, uint32_t width, uint32_t height) {
        xcb_window_t win = xcb_generate_id(conn);
        //xcb_cursor_t cursor_id = xcb_generate_id(conn);

#if 0
        /* Use the default cursor (left pointer) */
        if (cursor > -1) {
                i3Font *cursor_font = load_font(conn, "cursor");
                xcb_create_glyph_cursor(conn, cursor_id, cursor_font->id, cursor_font->id,
                                XCB_CURSOR_LEFT_PTR, XCB_CURSOR_LEFT_PTR + 1,
                                0, 0, 0, 65535, 65535, 65535);
        }
#endif

        uint32_t mask = 0;
        uint32_t values[3];

        mask |= XCB_CW_BACK_PIXEL;
        values[0] = 0;

	mask |= XCB_CW_EVENT_MASK;
	values[1] = XCB_EVENT_MASK_EXPOSURE |
                    XCB_EVENT_MASK_BUTTON_PRESS;

        xcb_create_window(conn,
                          XCB_COPY_FROM_PARENT,
                          win, /* the window id */
                          root, /* parent == root */
                          490, 297, width, height, /* dimensions */
                          0, /* border = 0, we draw our own */
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
                          mask,
                          values);

#if 0
        if (cursor > -1)
                xcb_change_window_attributes(conn, result, XCB_CW_CURSOR, &cursor_id);
#endif

        /* Map the window (= make it visible) */
        xcb_map_window(conn, win);

	return win;
}

/*
 * Returns the ID of the font matching the given pattern and stores the height
 * of the font (in pixels) in *font_height. die()s if no font matches.
 *
 */
int get_font_id(xcb_connection_t *conn, char *pattern, int *font_height) {
        xcb_void_cookie_t font_cookie;
        xcb_list_fonts_with_info_cookie_t info_cookie;

        /* Send all our requests first */
        int result;
        result = xcb_generate_id(conn);
        font_cookie = xcb_open_font_checked(conn, result, strlen(pattern), pattern);
        info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

        xcb_generic_error_t *error = xcb_request_check(conn, font_cookie);
        if (error != NULL) {
                fprintf(stderr, "ERROR: Could not open font: %d\n", error->error_code);
                exit(1);
        }

        /* Get information (height/name) for this font */
        xcb_list_fonts_with_info_reply_t *reply = xcb_list_fonts_with_info_reply(conn, info_cookie, NULL);
        if (reply == NULL)
                errx(1, "Could not load font \"%s\"\n", pattern);

        *font_height = reply->font_ascent + reply->font_descent;

        return result;
}
