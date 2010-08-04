#ifndef XCB_H_
#define XCB_H_

#include <xcb/xcb.h>

#define NUM_ATOMS 3

enum {
	#define ATOM_DO(name) name,
	#include "xcb_atoms.def"
};

xcb_atom_t atoms[NUM_ATOMS];

xcb_connection_t *xcb_connection;
xcb_screen_t     *xcb_screens;
xcb_window_t     xcb_root;
xcb_font_t       xcb_font;
int              font_height;

void init_xcb();
void clean_xcb();
void get_atoms();
void destroy_windows();
void create_windows();
void draw_bars();
int get_string_width(char *string);
void handle_xcb_event(xcb_generic_event_t *event);

#endif
