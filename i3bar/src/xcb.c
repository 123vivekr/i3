#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i3/ipc.h>
#include <ev.h>

#include "common.h"

#define NUM_ATOMS 3

enum {
    #define ATOM_DO(name) name,
    #include "xcb_atoms.def"
};

xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];
xcb_atom_t               atoms[NUM_ATOMS];

xcb_connection_t *xcb_connection;
xcb_screen_t     *xcb_screens;
xcb_window_t     xcb_root;
xcb_font_t       xcb_font;

ev_prepare *xcb_prep;
ev_check   *xcb_chk;
ev_io      *xcb_io;

xcb_event_handlers_t xcb_event_handlers;

uint32_t get_colorpixel(const char *s) {
    char strings[3][3] = { { s[0], s[1], '\0'} ,
                           { s[2], s[3], '\0'} ,
                           { s[4], s[5], '\0'} };
    uint8_t r = strtol(strings[0], NULL, 16);
    uint8_t g = strtol(strings[1], NULL, 16);
    uint8_t b = strtol(strings[2], NULL, 16);
    return (r << 16 | g << 8 | b);
}

void handle_button(xcb_button_press_event_t *event) {
    i3_ws *cur_ws;
    i3_output *walk;
    xcb_window_t bar = event->event;
    SLIST_FOREACH(walk, outputs, slist) {
        if (walk->bar == bar) {
            break;
        }
    }

    if (walk == NULL) {
        printf("Unknown Bar klicked!\n");
        return;
    }

    TAILQ_FOREACH(cur_ws, walk->workspaces, tailq) {
        if (cur_ws->visible) {
            break;
        }
    }

    if (cur_ws == NULL) {
        printf("No Workspace active?\n");
        return;
    }

    int32_t x = event->event_x;

    printf("Got Button %d\n", event->detail);

    switch (event->detail) {
        case 1:
            TAILQ_FOREACH(cur_ws, walk->workspaces, tailq) {
                printf("x = %d\n", x);
                if (x < cur_ws->name_width + 10) {
                    break;
                }
                x -= cur_ws->name_width + 10;
            }
            if (cur_ws == NULL) {
                return;
            }
            break;
        case 4:
            if (cur_ws == TAILQ_LAST(walk->workspaces, ws_head)) {
                cur_ws = TAILQ_FIRST(walk->workspaces);
            } else {
                cur_ws = TAILQ_NEXT(cur_ws, tailq);
            }
            break;
        case 5:
            if (cur_ws == TAILQ_FIRST(walk->workspaces)) {
                cur_ws = TAILQ_LAST(walk->workspaces, ws_head);
            } else {
                cur_ws = TAILQ_PREV(cur_ws, ws_head, tailq);
            }
            break;
    }

    char buffer[50];
    snprintf(buffer, 50, "%d", cur_ws->num);
    i3_send_msg(I3_IPC_MESSAGE_TYPE_COMMAND, buffer);
}

void handle_xcb_event(xcb_generic_event_t *event) {
    switch (event->response_type & ~0x80) {
        case XCB_EXPOSE:
            draw_bars();
            break;
        case XCB_BUTTON_PRESS:
            handle_button((xcb_button_press_event_t*) event);
            break;
    }
}

void xcb_prep_cb(struct ev_loop *loop, ev_prepare *watcher, int revenst) {
    xcb_flush(xcb_connection);
}

void xcb_chk_cb(struct ev_loop *loop, ev_check *watcher, int revents) {
    xcb_generic_event_t *event;
    if ((event = xcb_poll_for_event(xcb_connection)) != NULL) {
        handle_xcb_event(event);
    }
    FREE(event);
}

void xcb_io_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    /* Dummy Callback. We only need this, so that xcb-events trigger
     * Prepare- and Check-Watchers */
}

int get_string_width(xcb_char2b_t *string, int glyph_len) {
    xcb_query_text_extents_cookie_t cookie;
    xcb_query_text_extents_reply_t *reply;
    xcb_generic_error_t *error;
    int width;

    cookie = xcb_query_text_extents(xcb_connection, xcb_font, glyph_len, string);
    if ((reply = xcb_query_text_extents_reply(xcb_connection, cookie, &error)) == NULL) {
        printf("ERROR: Could not get text extents!");
        return 7;
    }

    width = reply->overall_width;
    free(reply);
    return width;
}

void init_xcb(char *fontname) {
    /* FIXME: xcb_connect leaks Memory */
    xcb_connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(xcb_connection)) {
        printf("Cannot open display\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to xcb\n");

    /* We have to request the atoms we need */
    #define ATOM_DO(name) atom_cookies[name] = xcb_intern_atom(xcb_connection, 0, strlen(#name), #name);
        #include "xcb_atoms.def"

    xcb_screens = xcb_setup_roots_iterator(xcb_get_setup(xcb_connection)).data;
    xcb_root = xcb_screens->root;

    xcb_font = xcb_generate_id(xcb_connection);
    xcb_open_font(xcb_connection,
                  xcb_font,
                  strlen(fontname),
                  fontname);

    xcb_list_fonts_with_info_cookie_t cookie;
    cookie = xcb_list_fonts_with_info(xcb_connection,
                                      1,
                                      strlen(fontname),
                                      fontname);
    xcb_list_fonts_with_info_reply_t *reply;
    reply = xcb_list_fonts_with_info_reply(xcb_connection,
                                           cookie,
                                           NULL);
    font_height = reply->font_ascent + reply->font_descent;
    FREE(reply);
    printf("Calculated Font-height: %d\n", font_height);

    xcb_io = malloc(sizeof(ev_io));
    xcb_prep = malloc(sizeof(ev_prepare));
    xcb_chk = malloc(sizeof(ev_check));

    ev_io_init(xcb_io, &xcb_io_cb, xcb_get_file_descriptor(xcb_connection), EV_READ);
    ev_prepare_init(xcb_prep, &xcb_prep_cb);
    ev_check_init(xcb_chk, &xcb_chk_cb);

    ev_io_start(main_loop, xcb_io);
    ev_prepare_start(main_loop, xcb_prep);
    ev_check_start(main_loop, xcb_chk);

    /* FIXME: Maybe we can push that further backwards */
    get_atoms();
}

void clean_xcb() {
    xcb_disconnect(xcb_connection);

    ev_check_stop(main_loop, xcb_chk);
    ev_prepare_stop(main_loop, xcb_prep);
    ev_io_stop(main_loop, xcb_io);

    FREE(xcb_chk);
    FREE(xcb_prep);
    FREE(xcb_io);
}

void get_atoms() {
    xcb_intern_atom_reply_t *reply;
    #define ATOM_DO(name) reply = xcb_intern_atom_reply(xcb_connection, atom_cookies[name], NULL); \
        atoms[name] = reply->atom; \
        free(reply);

    #include "xcb_atoms.def"
    printf("Got Atoms\n");
}

void destroy_window(i3_output *output) {
    if (output == NULL) {
        return;
    }
    if (output->bar == XCB_NONE) {
        return;
    }
    xcb_destroy_window(xcb_connection, output->bar);
    output->bar = XCB_NONE;
}

void reconfig_windows() {
    uint32_t mask;
    uint32_t values[4];

    i3_output *walk;
    SLIST_FOREACH(walk, outputs, slist) {
        if (!walk->active) {
            printf("Destroying window for output %s\n", walk->name);
            destroy_window(walk);
            continue;
        }
        if (walk->bar == XCB_NONE) {
            printf("Creating Window for output %s\n", walk->name);

            walk->bar = xcb_generate_id(xcb_connection);
            mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
            values[0] = xcb_screens->black_pixel;
            values[1] = XCB_EVENT_MASK_EXPOSURE |
                        XCB_EVENT_MASK_BUTTON_PRESS;
            xcb_create_window(xcb_connection,
                              xcb_screens->root_depth,
                              walk->bar,
                              xcb_root,
                              walk->rect.x, walk->rect.y,
                              walk->rect.w, font_height + 6,
                              1,
                              XCB_WINDOW_CLASS_INPUT_OUTPUT,
                              xcb_screens->root_visual,
                              mask,
                              values);

            xcb_change_property(xcb_connection,
                                XCB_PROP_MODE_REPLACE,
                                walk->bar,
                                atoms[_NET_WM_WINDOW_TYPE],
                                atoms[ATOM],
                                32,
                                1,
                                (unsigned char*) &atoms[_NET_WM_WINDOW_TYPE_DOCK]);

            walk->bargc = xcb_generate_id(xcb_connection);
            mask = XCB_GC_FONT;
            values[0] = xcb_font;
            xcb_create_gc(xcb_connection,
                          walk->bargc,
                          walk->bar,
                          mask,
                          values);

            xcb_map_window(xcb_connection, walk->bar);
        } else {
            mask = XCB_CONFIG_WINDOW_X |
                   XCB_CONFIG_WINDOW_Y |
                   XCB_CONFIG_WINDOW_WIDTH |
                   XCB_CONFIG_WINDOW_HEIGHT;
            values[0] = walk->rect.x;
            values[1] = walk->rect.y + walk->rect.h - font_height - 6;
            values[2] = walk->rect.w;
            values[3] = font_height + 6;
            printf("Reconfiguring Window for output %s to %d,%d\n", walk->name, values[0], values[1]);
            xcb_configure_window(xcb_connection,
                                 walk->bar,
                                 mask,
                                 values);
        }
    }
}

void draw_bars() {
    printf("Drawing Bars...\n");
    int i = 0;
    i3_output *outputs_walk;
    SLIST_FOREACH(outputs_walk, outputs, slist) {
        if (!outputs_walk->active) {
            printf("Output %s inactive, skipping...\n", outputs_walk->name);
            continue;
        }
        if (outputs_walk->bar == XCB_NONE) {
            reconfig_windows();
        }
        uint32_t color = get_colorpixel("000000");
        xcb_change_gc(xcb_connection,
                      outputs_walk->bargc,
                      XCB_GC_FOREGROUND,
                      &color);
        xcb_rectangle_t rect = { 0, 0, outputs_walk->rect.w, font_height + 6 };
        xcb_poly_fill_rectangle(xcb_connection,
                                outputs_walk->bar,
                                outputs_walk->bargc,
                                1,
                                &rect);
        if (statusline != NULL) {
            printf("Printing statusline!\n");
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_BACKGROUND,
                          &color);
            color = get_colorpixel("FFFFFF");
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_FOREGROUND,
                          &color);

            int glyph_count;
            xcb_char2b_t *text = (xcb_char2b_t*) convert_utf8_to_ucs2(statusline, &glyph_count);

            xcb_void_cookie_t cookie;
            cookie = xcb_image_text_16(xcb_connection,
                                       glyph_count,
                                       outputs_walk->bar,
                                       outputs_walk->bargc,
                                       outputs_walk->rect.w - get_string_width(text, glyph_count) - 4,
                                       font_height + 1,
                                       (xcb_char2b_t*) text);

            xcb_generic_error_t *err = xcb_request_check(xcb_connection, cookie);

            if (err != NULL) {
                printf("XCB-Error: %d\n", err->error_code);
            }
        }
        i3_ws *ws_walk;
        TAILQ_FOREACH(ws_walk, outputs_walk->workspaces, tailq) {
            printf("Drawing Button for WS %s at x = %d\n", ws_walk->name, i);
            uint32_t color = get_colorpixel("240000");
            if (ws_walk->visible) {
                color = get_colorpixel("480000");
            }
            if (ws_walk->urgent) {
                printf("WS %s is urgent!\n", ws_walk->name);
                color = get_colorpixel("002400");
            }
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_FOREGROUND,
                          &color);
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_BACKGROUND,
                          &color);
            xcb_rectangle_t rect = { i + 1, 1, ws_walk->name_width + 8, font_height + 4 };
            xcb_poly_fill_rectangle(xcb_connection,
                                    outputs_walk->bar,
                                    outputs_walk->bargc,
                                    1,
                                    &rect);
            color = get_colorpixel("FFFFFF");
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_FOREGROUND,
                          &color);
            xcb_image_text_16(xcb_connection,
                              ws_walk->name_glyphs,
                              outputs_walk->bar,
                              outputs_walk->bargc,
                              i + 5, font_height + 1,
                              ws_walk->ucs2_name);
            i += 10 + ws_walk->name_width;
        }

        i = 0;
    }
}
