#ifndef CONFIG_H_
#define CONFIG_H_

#include "common.h"

typedef enum {
    POS_NONE = 0,
    POS_TOP,
    POS_BOT
} position_t;

typedef struct config_t {
    int          hide_on_modifier;
    position_t   position;
    int          verbose;
    struct xcb_color_strings_t colors;
    int          disable_ws;
    char         *bar_id;
    char         *command;
    char         *fontname;
    char         *tray_output;
} config_t;

config_t config;

/**
 * Start parsing the received bar configuration json-string
 *
 */
void parse_config_json(char *json);

/**
 * free()s the color strings as soon as they are not needed anymore.
 *
 */
void free_colors(struct xcb_color_strings_t *colors);

#endif
