#include <xcb/xcb.h>

#ifndef _DATA_H
#define _DATA_H
/*
 * This file defines all data structures used by i3
 *
 */
#include "queue.h"

/* Forward definitions */
typedef struct Cell Cell;
typedef struct Font i3Font;
typedef struct Container Container;
typedef struct Client Client;
typedef struct Binding Binding;
typedef struct Workspace Workspace;

/* Helper types */
typedef enum { D_LEFT, D_RIGHT, D_UP, D_DOWN } direction_t;

enum {
	BIND_NONE = 0,
	BIND_MOD_1 = XCB_MOD_MASK_1,
	BIND_MOD_2 = XCB_MOD_MASK_2,
	BIND_MOD_3 = XCB_MOD_MASK_3,
	BIND_MOD_4 = XCB_MOD_MASK_4,
	BIND_MOD_5 = XCB_MOD_MASK_5,
	BIND_SHIFT = XCB_MOD_MASK_SHIFT,
	BIND_CONTROL = XCB_MOD_MASK_CONTROL,
	BIND_MODE_SWITCH = (1 << 8)
};

struct Workspace {
	int x;
	int y;
	int width;
	int height;
	int num;

	/* table dimensions */
	int cols;
	int rows;

	/* These are stored here just while this workspace is _not_ shown (see show_workspace()) */
	int current_row;
	int current_col;

	/* This is a two-dimensional dynamic array of Container-pointers. I’ve always wanted
	 * to be a three-star programmer :) */
	Container ***table;
};

/*
 * Defines a position in the table
 *
 */
struct Cell {
	int row;
	int column;
};

struct Binding {
	/* Keycode to bind */
	uint32_t keycode;
	/* Bitmask consisting of BIND_MOD_1, BIND_MODE_SWITCH, … */
	uint32_t mods;
	/* Command, like in command mode */
	char *command;

	TAILQ_ENTRY(Binding) bindings;
};

/*
 * We need to save the height of a font because it is required for each drawing of
 * text but relatively hard to get. As soon as a new font needs to be loaded, a
 * Font-entry will be filled for later use.
 *
 */
struct Font {
	/* The name of the font, that is what the pattern resolves to */
	char *name;
	/* A copy of the pattern to build a cache */
	char *pattern;
	/* The height of the font, built from font_ascent + font_descent */
	int height;
	/* The xcb-id for the font */
	xcb_font_t id;
};

/*
 * A client is X11-speak for a window.
 *
 */
struct Client {
	/* TODO: this is NOT final */
	Cell old_position; /* if you set a client to floating and set it back to managed,
			      it does remember its old position and *tries* to get back there */

	/* Backpointer. A client is inside a container */
	Container *container;

	int x, y;
	int width, height;

	/* Name */
	char *name;
	int name_len;

	/* XCB contexts */
	xcb_window_t frame; /* Our window: The frame around the client */
	xcb_gcontext_t titlegc; /* The titlebar’s graphic context inside the frame */
	xcb_window_t child; /* The client’s window */

	/* The following entry provides the necessary list pointers to use Client with LIST_* macros */
	CIRCLEQ_ENTRY(Client) clients;
};

/*
 * A container is either in default or stacking mode. It sits inside the table.
 *
 */
struct Container {
	/* Those are speaking for themselves: */
	Client *currently_focused;
	int colspan;
	int rowspan;

	/* Position of the container inside our table */
	int row;
	int col;
	/* Width/Height of the container. Changeable by the user */
	int width;
	int height;

	/* Ensure MODE_DEFAULT maps to 0 because we use calloc for initialization later */
	enum { MODE_DEFAULT = 0, MODE_STACK = 1 } mode;
	CIRCLEQ_HEAD(client_head, Client) clients;
};

#endif
