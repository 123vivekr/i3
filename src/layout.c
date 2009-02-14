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
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xcb/xcb.h>

#include "font.h"
#include "i3.h"
#include "xcb.h"
#include "table.h"
#include "util.h"

/* All functions handling layout/drawing of window decorations */

/*
 * (Re-)draws window decorations for a given Client
 *
 */
void decorate_window(xcb_connection_t *conn, Client *client) {
        uint32_t mask = 0;
        uint32_t values[3];
        i3Font *font = load_font(conn, pattern);
        uint32_t background_color,
                 text_color,
                 border_color;

        if (client->container->currently_focused == client) {
                background_color = get_colorpixel(conn, client->frame, "#285577");
                text_color = get_colorpixel(conn, client->frame, "#ffffff");
                border_color = get_colorpixel(conn, client->frame, "#4c7899");
        } else {
                background_color = get_colorpixel(conn, client->frame, "#222222");
                text_color = get_colorpixel(conn, client->frame, "#888888");
                border_color = get_colorpixel(conn, client->frame, "#333333");
        }

        /* Our plan is the following:
           - Draw a rect around the whole client in background_color
           - Draw two lines in a lighter color
           - Draw the window’s title

           Note that xcb_image_text apparently adds 1xp border around the font? Can anyone confirm this?
         */

        /* Draw a green rectangle around the window */
        mask = XCB_GC_FOREGROUND;
        values[0] = background_color;
        xcb_change_gc(conn, client->titlegc, mask, values);

        xcb_rectangle_t rect = {0, 0, client->width, client->height};
        xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &rect);

        /* Draw the lines */
        /* TODO: this needs to be more beautiful somewhen. maybe stdarg + change_gc(gc, ...) ? */
#define DRAW_LINE(colorpixel, x, y, to_x, to_y) { \
                uint32_t draw_values[1]; \
                draw_values[0] = colorpixel; \
                xcb_change_gc(conn, client->titlegc, XCB_GC_FOREGROUND, draw_values); \
                xcb_point_t points[] = {{x, y}, {to_x, to_y}}; \
                xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, client->frame, client->titlegc, 2, points); \
        }

        DRAW_LINE(border_color, 2, 0, client->width, 0);
        DRAW_LINE(border_color, 2, font->height + 3, 2 + client->width, font->height + 3);

        /* Draw the font */
        mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;

        values[0] = text_color;
        values[1] = background_color;
        values[2] = font->id;

        xcb_change_gc(conn, client->titlegc, mask, values);

        /* TODO: utf8? */
        char *label;
        asprintf(&label, "(%08x) %.*s", client->frame, client->name_len, client->name);
        xcb_void_cookie_t text_cookie = xcb_image_text_8_checked(conn, strlen(label), client->frame,
                                        client->titlegc, 3 /* X */, font->height /* Y = baseline of font */, label);
        check_error(conn, text_cookie, "Could not draw client's title");
        free(label);
}

static void render_container(xcb_connection_t *connection, Container *container) {
        Client *client;
        i3Font *font = load_font(connection, pattern);

        if (container->mode == MODE_DEFAULT) {
                int num_clients = 0;
                CIRCLEQ_FOREACH(client, &(container->clients), clients)
                        num_clients++;
                printf("got %d clients in this default container.\n", num_clients);

                int current_client = 0;
                CIRCLEQ_FOREACH(client, &(container->clients), clients) {
                        /* TODO: at the moment, every column/row is screen / num_cols. This
                         * needs to be changed to "percentage of the screen" by
                         * default and adjustable by the user if necessary.
                         */

                        /* Check if we changed client->x or client->y by updating it…
                         * Note the bitwise OR instead of logical OR to force evaluation of both statements */
                        if (client->force_reconfigure |
                            (client->x != (client->x = container->x + (container->col * container->width))) |
                            (client->y != (client->y = container->y + (container->row * container->height +
                                          (container->height / num_clients) * current_client)))) {
                                printf("frame needs to be pushed to %dx%d\n", client->x, client->y);
                                /* Note: We can use a pointer to client->x like an array of uint32_ts
                                   because it is followed by client->y by definition */
                                xcb_configure_window(connection, client->frame,
                                                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &(client->x));
                        }

                        /* TODO: vertical default layout */
                        if (client->force_reconfigure |
                            (client->width != (client->width = container->width)) |
                            (client->height != (client->height = container->height / num_clients))) {
                                xcb_configure_window(connection, client->frame,
                                                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                                                &(client->width));

                                /* Adjust the position of the child inside its frame.
                                 * The coordinates of the child are relative to its frame, we
                                 * add a border of 2 pixel to each value */
                                uint32_t mask = XCB_CONFIG_WINDOW_X |
                                                XCB_CONFIG_WINDOW_Y |
                                                XCB_CONFIG_WINDOW_WIDTH |
                                                XCB_CONFIG_WINDOW_HEIGHT;
                                uint32_t values[4] = {2,                                              /* x */
                                                      font->height + 2 + 2,                           /* y */
                                                      client->width - (2 + 2),                        /* width */
                                                      client->height - ((font->height + 2 + 2) + 2)}; /* height */

                                printf("fullscreen frame/child will be at %dx%d with size %dx%d\n",
                                                values[0], values[1], values[2], values[3]);

                                xcb_configure_window(connection, client->child, mask, values);
                        }

                        if (client->force_reconfigure)
                                client->force_reconfigure = false;

                        current_client++;
                }
        } else {
                /* TODO: Implement stacking */
        }
}

void render_layout(xcb_connection_t *conn) {
        int cols, rows;
        int screen;
        for (screen = 0; screen < num_screens; screen++) {
                printf("Rendering screen %d\n", screen);
                if (workspaces[screen].fullscreen_client != NULL)
                        /* This is easy: A client has entered fullscreen mode, so we don’t render at all */
                        continue;
                /* TODO: get the workspace which is active on the screen */
                int width = workspaces[screen].width;
                int height = workspaces[screen].height;

                printf("got %d rows and %d cols\n", c_ws->rows, c_ws->cols);
                printf("each of them therefore is %d px width and %d px height\n",
                                width / c_ws->cols, height / c_ws->rows);

                /* Go through the whole table and render what’s necessary */
                for (cols = 0; cols < c_ws->cols; cols++)
                        for (rows = 0; rows < c_ws->rows; rows++) {
                                Container *con = CUR_TABLE[cols][rows];
                                printf("container has %d colspan, %d rowspan\n",
                                                con->colspan, con->rowspan);
                                /* Update position of the container */
                                con->row = rows;
                                con->col = cols;
                                con->x = workspaces[screen].x;
                                con->y = workspaces[screen].y;
                                con->width = (width / c_ws->cols) * con->colspan;
                                con->height = (height / c_ws->rows) * con->rowspan;

                                /* Render it */
                                render_container(conn, CUR_TABLE[cols][rows]);
                        }
        }

        xcb_flush(conn);
}
