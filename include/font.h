#include <xcb/xcb.h>

#include "data.h"

#ifndef _FONT_H
#define _FONT_H

i3Font *load_font(xcb_connection_t *c, const char *pattern);

#endif
