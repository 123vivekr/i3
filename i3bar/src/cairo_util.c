/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2015 Ingo Bürk and contributors (see also: LICENSE)
 *
 * cairo_util.c: Utility for operations using cairo.
 *
 */
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <cairo/cairo-xcb.h>

#include "common.h"
#include "libi3.h"

xcb_connection_t *xcb_connection;
xcb_visualtype_t *visual_type;

/*
 * Initialize the cairo surface to represent the given drawable.
 *
 */
void cairo_surface_init(surface_t *surface, xcb_drawable_t drawable, int width, int height) {
    surface->id = drawable;

    surface->gc = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(xcb_connection, surface->gc, surface->id, 0, NULL);
    if (xcb_request_failed(gc_cookie, "Could not create graphical context"))
        exit(EXIT_FAILURE);

    surface->surface = cairo_xcb_surface_create(xcb_connection, surface->id, visual_type, width, height);
    surface->cr = cairo_create(surface->surface);
}

/*
 * Destroys the surface.
 *
 */
void cairo_surface_free(surface_t *surface) {
    xcb_free_gc(xcb_connection, surface->gc);
    cairo_surface_destroy(surface->surface);
    cairo_destroy(surface->cr);
}

/*
 * Parses the given color in hex format to an internal color representation.
 * Note that the input must begin with a hash sign, e.g., "#3fbc59".
 *
 */
color_t cairo_hex_to_color(const char *color) {
    char alpha[2];
    if (strlen(color) == strlen("#rrggbbaa")) {
        alpha[0] = color[7];
        alpha[1] = color[8];
    } else {
        alpha[0] = alpha[1] = 'F';
    }

    char groups[4][3] = {
        {color[1], color[2], '\0'},
        {color[3], color[4], '\0'},
        {color[5], color[6], '\0'},
        {alpha[0], alpha[1], '\0'}};

    return (color_t){
        .red = strtol(groups[0], NULL, 16) / 255.0,
        .green = strtol(groups[1], NULL, 16) / 255.0,
        .blue = strtol(groups[2], NULL, 16) / 255.0,
        .alpha = strtol(groups[3], NULL, 16) / 255.0,
        .colorpixel = get_colorpixel(color)};
}

/*
 * Set the given color as the source color on the surface.
 *
 */
void cairo_set_source_color(surface_t *surface, color_t color) {
    cairo_set_source_rgba(surface->cr, color.red, color.green, color.blue, color.alpha);
}

/**
 * Draw the given text using libi3.
 * This function also marks the surface dirty which is needed if other means of
 * drawing are used. This will be the case when using XCB to draw text.
 *
 */
void cairo_draw_text(i3String *text, surface_t *surface, color_t fg_color, color_t bg_color, int x, int y, int max_width) {
    set_font_colors(surface->gc, fg_color.colorpixel, bg_color.colorpixel);
    draw_text(text, surface->id, surface->gc, visual_type, x, y, max_width);

    cairo_surface_mark_dirty(surface->surface);
}
