/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "all.h"

/*
 * Updates the WM_CLASS (consisting of the class and instance) for the
 * given window.
 *
 */
void window_update_class(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("empty property, not updating\n");
        return;
    }

    /* We cannot use asprintf here since this property contains two
     * null-terminated strings (for compatibility reasons). Instead, we
     * use strdup() on both strings */
    char *new_class = xcb_get_property_value(prop);

    FREE(win->class_instance);
    FREE(win->class_class);

    win->class_instance = strdup(new_class);
    if ((strlen(new_class) + 1) < xcb_get_property_value_length(prop))
        win->class_class = strdup(new_class + strlen(new_class) + 1);
    else win->class_class = NULL;
    LOG("WM_CLASS changed to %s (instance), %s (class)\n",
        win->class_instance, win->class_class);
}

/*
 * Updates the name by using _NET_WM_NAME (encoded in UTF-8) for the given
 * window. Further updates using window_update_name_legacy will be ignored.
 *
 */
void window_update_name(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("_NET_WM_NAME not specified, not changing\n");
        return;
    }

    /* Save the old pointer to make the update atomic */
    char *new_name;
    if (asprintf(&new_name, "%.*s", xcb_get_property_value_length(prop),
                 (char*)xcb_get_property_value(prop)) == -1) {
        perror("asprintf()");
        DLOG("Could not get window name\n");
    }
    /* Convert it to UCS-2 here for not having to convert it later every time we want to pass it to X */
    FREE(win->name_x);
    FREE(win->name_json);
    win->name_json = new_name;
    win->name_x = convert_utf8_to_ucs2(win->name_json, &win->name_len);
    LOG("_NET_WM_NAME changed to \"%s\"\n", win->name_json);

    win->uses_net_wm_name = true;
}

/*
 * Updates the name by using WM_NAME (encoded in COMPOUND_TEXT). We do not
 * touch what the client sends us but pass it to xcb_image_text_8. To get
 * proper unicode rendering, the application has to use _NET_WM_NAME (see
 * window_update_name()).
 *
 */
void window_update_name_legacy(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("prop == NULL\n");
        return;
    }

    /* ignore update when the window is known to already have a UTF-8 name */
    if (win->uses_net_wm_name)
        return;

    char *new_name;
    if (asprintf(&new_name, "%.*s", xcb_get_property_value_length(prop),
                 (char*)xcb_get_property_value(prop)) == -1) {
        perror("asprintf()");
        DLOG("Could not get legacy window name\n");
        return;
    }

    LOG("Using legacy window title. Note that in order to get Unicode window "
        "titles in i3, the application has to set _NET_WM_NAME (UTF-8)\n");

    FREE(win->name_x);
    FREE(win->name_json);
    win->name_x = new_name;
    win->name_json = strdup(new_name);
    win->name_len = strlen(new_name);
}

/**
 * Updates the CLIENT_LEADER (logical parent window).
 *
 */
void window_update_leader(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("prop == NULL\n");
        return;
    }

    xcb_window_t *leader = xcb_get_property_value(prop);
    if (leader == NULL)
        return;

    DLOG("Client leader changed to %08x\n", *leader);

    win->leader = *leader;
}

/**
 * Updates the TRANSIENT_FOR (logical parent window).
 *
 */
void window_update_transient_for(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("prop == NULL\n");
        return;
    }

    xcb_window_t transient_for;
    if (!xcb_get_wm_transient_for_from_reply(&transient_for, prop))
        return;

    DLOG("Transient for changed to %08x\n", transient_for);

    win->transient_for = transient_for;
}
