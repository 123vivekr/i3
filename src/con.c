/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * con.c contains all functions which deal with containers directly (creating
 * containers, searching containers, getting specific properties from
 * containers, …).
 *
 */
#include "all.h"

char *colors[] = {
    "#ff0000",
    "#00FF00",
    "#0000FF",
    "#ff00ff",
    "#00ffff",
    "#ffff00",
    "#aa0000",
    "#00aa00",
    "#0000aa",
    "#aa00aa"
};

static void con_on_remove_child(Con *con);

/*
 * Create a new container (and attach it to the given parent, if not NULL).
 * This function initializes the data structures and creates the appropriate
 * X11 IDs using x_con_init().
 *
 */
Con *con_new(Con *parent) {
    Con *new = scalloc(sizeof(Con));
    new->on_remove_child = con_on_remove_child;
    TAILQ_INSERT_TAIL(&all_cons, new, all_cons);
    new->type = CT_CON;
    new->border_style = config.default_border;
    static int cnt = 0;
    DLOG("opening window %d\n", cnt);

    /* TODO: remove window coloring after test-phase */
    DLOG("color %s\n", colors[cnt]);
    new->name = strdup(colors[cnt]);
    //uint32_t cp = get_colorpixel(colors[cnt]);
    cnt++;
    if ((cnt % (sizeof(colors) / sizeof(char*))) == 0)
        cnt = 0;

    x_con_init(new);

    // TODO: this needs to be integrated into src/x.c and updated on config file reloads
    xcb_change_window_attributes(conn, new->frame, XCB_CW_BACK_PIXEL, &config.client.background);

    TAILQ_INIT(&(new->floating_head));
    TAILQ_INIT(&(new->nodes_head));
    TAILQ_INIT(&(new->focus_head));
    TAILQ_INIT(&(new->swallow_head));

    if (parent != NULL)
        con_attach(new, parent, false);

    return new;
}

/*
 * Attaches the given container to the given parent. This happens when moving
 * a container or when inserting a new container at a specific place in the
 * tree.
 *
 * ignore_focus is to just insert the Con at the end (useful when creating a
 * new split container *around* some containers, that is, detaching and
 * attaching them in order without wanting to mess with the focus in between).
 *
 */
void con_attach(Con *con, Con *parent, bool ignore_focus) {
    con->parent = parent;
    Con *loop;
    Con *current = NULL;
    struct nodes_head *nodes_head = &(parent->nodes_head);

    /* Workspaces are handled differently: they need to be inserted at the
     * right position. */
    if (con->type == CT_WORKSPACE) {
        DLOG("it's a workspace. num = %d\n", con->num);
        if (con->num == -1 || TAILQ_EMPTY(nodes_head)) {
            TAILQ_INSERT_TAIL(nodes_head, con, nodes);
        } else {
            current = TAILQ_FIRST(nodes_head);
            if (con->num < current->num) {
                /* we need to insert the container at the beginning */
                TAILQ_INSERT_HEAD(nodes_head, con, nodes);
            } else {
                while (current->num != -1 && con->num > current->num) {
                    current = TAILQ_NEXT(current, nodes);
                    if (current == TAILQ_END(nodes_head)) {
                        current = NULL;
                        break;
                    }
                }
                /* we need to insert con after current, if current is not NULL */
                if (current)
                    TAILQ_INSERT_BEFORE(current, con, nodes);
                else TAILQ_INSERT_TAIL(nodes_head, con, nodes);
            }
        }
        goto add_to_focus_head;
    }

    if (con->type == CT_FLOATING_CON) {
        DLOG("Inserting into floating containers\n");
        TAILQ_INSERT_TAIL(&(parent->floating_head), con, floating_windows);
    } else {
        if (!ignore_focus) {
            /* Get the first tiling container in focus stack */
            TAILQ_FOREACH(loop, &(parent->focus_head), focused) {
                if (loop->type == CT_FLOATING_CON)
                    continue;
                current = loop;
                break;
            }
        }

        /* Insert the container after the tiling container, if found */
        if (current) {
            DLOG("Inserting con = %p after last focused tiling con %p\n",
                 con, current);
            TAILQ_INSERT_AFTER(nodes_head, current, con, nodes);
        } else TAILQ_INSERT_TAIL(nodes_head, con, nodes);
    }

add_to_focus_head:
    /* We insert to the TAIL because con_focus() will correct this.
     * This way, we have the option to insert Cons without having
     * to focus them. */
    TAILQ_INSERT_TAIL(&(parent->focus_head), con, focused);
}

/*
 * Detaches the given container from its current parent
 *
 */
void con_detach(Con *con) {
    if (con->type == CT_FLOATING_CON) {
        TAILQ_REMOVE(&(con->parent->floating_head), con, floating_windows);
        TAILQ_REMOVE(&(con->parent->focus_head), con, focused);
    } else {
        TAILQ_REMOVE(&(con->parent->nodes_head), con, nodes);
        TAILQ_REMOVE(&(con->parent->focus_head), con, focused);
    }
}

/*
 * Sets input focus to the given container. Will be updated in X11 in the next
 * run of x_push_changes().
 *
 */
void con_focus(Con *con) {
    assert(con != NULL);
    DLOG("con_focus = %p\n", con);

    /* 1: set focused-pointer to the new con */
    /* 2: exchange the position of the container in focus stack of the parent all the way up */
    TAILQ_REMOVE(&(con->parent->focus_head), con, focused);
    TAILQ_INSERT_HEAD(&(con->parent->focus_head), con, focused);
    if (con->parent->parent != NULL)
        con_focus(con->parent);

    focused = con;
    if (con->urgent) {
        con->urgent = false;
        workspace_update_urgent_flag(con_get_workspace(con));
    }
    DLOG("con_focus done = %p\n", con);
}

/*
 * Returns true when this node is a leaf node (has no children)
 *
 */
bool con_is_leaf(Con *con) {
    return TAILQ_EMPTY(&(con->nodes_head));
}

/*
 * Returns true if this node accepts a window (if the node swallows windows,
 * it might already have swallowed enough and cannot hold any more).
 *
 */
bool con_accepts_window(Con *con) {
    /* 1: workspaces never accept direct windows */
    if (con->type == CT_WORKSPACE)
        return false;

    if (con->orientation != NO_ORIENTATION) {
        DLOG("container %p does not accepts windows, orientation != NO_ORIENTATION\n", con);
        return false;
    }

    /* TODO: if this is a swallowing container, we need to check its max_clients */
    return (con->window == NULL);
}

/*
 * Gets the output container (first container with CT_OUTPUT in hierarchy) this
 * node is on.
 *
 */
Con *con_get_output(Con *con) {
    Con *result = con;
    while (result != NULL && result->type != CT_OUTPUT)
        result = result->parent;
    /* We must be able to get an output because focus can never be set higher
     * in the tree (root node cannot be focused). */
    assert(result != NULL);
    return result;
}

/*
 * Gets the workspace container this node is on.
 *
 */
Con *con_get_workspace(Con *con) {
    Con *result = con;
    while (result != NULL && result->type != CT_WORKSPACE)
        result = result->parent;
    assert(result != NULL);
    return result;
}

/*
 * helper data structure for the breadth-first-search in
 * con_get_fullscreen_con()
 *
 */
struct bfs_entry {
    Con *con;

    TAILQ_ENTRY(bfs_entry) entries;
};

/*
 * Returns the first fullscreen node below this node.
 *
 */
Con *con_get_fullscreen_con(Con *con) {
    Con *current, *child;

    /* TODO: is breadth-first-search really appropriate? (check as soon as
     * fullscreen levels and fullscreen for containers is implemented) */
    TAILQ_HEAD(bfs_head, bfs_entry) bfs_head = TAILQ_HEAD_INITIALIZER(bfs_head);
    struct bfs_entry *entry = smalloc(sizeof(struct bfs_entry));
    entry->con = con;
    TAILQ_INSERT_TAIL(&bfs_head, entry, entries);

    while (!TAILQ_EMPTY(&bfs_head)) {
        entry = TAILQ_FIRST(&bfs_head);
        current = entry->con;
        if (current != con && current->fullscreen_mode != CF_NONE) {
            /* empty the queue */
            while (!TAILQ_EMPTY(&bfs_head)) {
                entry = TAILQ_FIRST(&bfs_head);
                TAILQ_REMOVE(&bfs_head, entry, entries);
                free(entry);
            }
            return current;
        }

        TAILQ_REMOVE(&bfs_head, entry, entries);
        free(entry);

        TAILQ_FOREACH(child, &(current->nodes_head), nodes) {
            entry = smalloc(sizeof(struct bfs_entry));
            entry->con = child;
            TAILQ_INSERT_TAIL(&bfs_head, entry, entries);
        }

        TAILQ_FOREACH(child, &(current->floating_head), floating_windows) {
            entry = smalloc(sizeof(struct bfs_entry));
            entry->con = child;
            TAILQ_INSERT_TAIL(&bfs_head, entry, entries);
        }
    }

    return NULL;
}

/*
 * Returns true if the node is floating.
 *
 */
bool con_is_floating(Con *con) {
    assert(con != NULL);
    DLOG("checking if con %p is floating\n", con);
    return (con->floating >= FLOATING_AUTO_ON);
}

/*
 * Checks if the given container is either floating or inside some floating
 * container. It returns the FLOATING_CON container.
 *
 */
Con *con_inside_floating(Con *con) {
    assert(con != NULL);
    if (con->type == CT_FLOATING_CON)
        return con;

    if (con->floating >= FLOATING_AUTO_ON)
        return con->parent;

    if (con->type == CT_WORKSPACE)
        return NULL;

    return con_inside_floating(con->parent);
}

/*
 * Returns the container with the given client window ID or NULL if no such
 * container exists.
 *
 */
Con *con_by_window_id(xcb_window_t window) {
    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons)
        if (con->window != NULL && con->window->id == window)
            return con;
    return NULL;
}

/*
 * Returns the container with the given frame ID or NULL if no such container
 * exists.
 *
 */
Con *con_by_frame_id(xcb_window_t frame) {
    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons)
        if (con->frame == frame)
            return con;
    return NULL;
}

/*
 * Returns the first container which wants to swallow this window
 * TODO: priority
 *
 */
Con *con_for_window(i3Window *window, Match **store_match) {
    Con *con;
    Match *match;
    DLOG("searching con for window %p\n", window);
    DLOG("class == %s\n", window->class_class);

    TAILQ_FOREACH(con, &all_cons, all_cons)
        TAILQ_FOREACH(match, &(con->swallow_head), matches) {
            if (!match_matches_window(match, window))
                continue;
            if (store_match != NULL)
                *store_match = match;
            return con;
        }

    return NULL;
}

/*
 * Returns the number of children of this container.
 *
 */
int con_num_children(Con *con) {
    Con *child;
    int children = 0;

    TAILQ_FOREACH(child, &(con->nodes_head), nodes)
        children++;

    return children;
}

/*
 * Updates the percent attribute of the children of the given container. This
 * function needs to be called when a window is added or removed from a
 * container.
 *
 */
void con_fix_percent(Con *con) {
    Con *child;
    int children = con_num_children(con);

    // calculate how much we have distributed and how many containers
    // with a percentage set we have
    double total = 0.0;
    int children_with_percent = 0;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        if (child->percent > 0.0) {
            total += child->percent;
            ++children_with_percent;
        }
    }

    // if there were children without a percentage set, set to a value that
    // will make those children proportional to all others
    if (children_with_percent != children) {
        TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
            if (child->percent <= 0.0) {
                if (children_with_percent == 0)
                    total += (child->percent = 1.0);
                else total += (child->percent = total / children_with_percent);
            }
        }
    }

    // if we got a zero, just distribute the space equally, otherwise
    // distribute according to the proportions we got
    if (total == 0.0) {
        TAILQ_FOREACH(child, &(con->nodes_head), nodes)
            child->percent = 1.0 / children;
    } else if (total != 1.0) {
        TAILQ_FOREACH(child, &(con->nodes_head), nodes)
            child->percent /= total;
    }
}

/*
 * Toggles fullscreen mode for the given container. Fullscreen mode will not be
 * entered when there already is a fullscreen container on this workspace.
 *
 */
void con_toggle_fullscreen(Con *con) {
    Con *workspace, *fullscreen;

    if (con->type == CT_WORKSPACE) {
        DLOG("You cannot make a workspace fullscreen.\n");
        return;
    }

    DLOG("toggling fullscreen for %p / %s\n", con, con->name);
    if (con->fullscreen_mode == CF_NONE) {
        /* 1: check if there already is a fullscreen con */
        workspace = con_get_workspace(con);
        if ((fullscreen = con_get_fullscreen_con(workspace)) != NULL) {
            LOG("Not entering fullscreen mode, container (%p/%s) "
                "already is in fullscreen mode\n",
                fullscreen, fullscreen->name);
        } else {
            /* 2: enable fullscreen */
            con->fullscreen_mode = CF_OUTPUT;
        }
    } else {
        /* 1: disable fullscreen */
        con->fullscreen_mode = CF_NONE;
    }
    DLOG("mode now: %d\n", con->fullscreen_mode);

    /* update _NET_WM_STATE if this container has a window */
    /* TODO: when a window is assigned to a container which is already
     * fullscreened, this state needs to be pushed to the client, too */
    if (con->window == NULL)
        return;

    uint32_t values[1];
    unsigned int num = 0;

    if (con->fullscreen_mode != CF_NONE)
        values[num++] = atoms[_NET_WM_STATE_FULLSCREEN];

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, con->window->id,
                        atoms[_NET_WM_STATE], ATOM, 32, num, values);
}

/*
 * Moves the given container to the currently focused container on the given
 * workspace.
 * TODO: is there a better place for this function?
 *
 */
void con_move_to_workspace(Con *con, Con *workspace) {
    if (con->type == CT_WORKSPACE) {
        DLOG("Moving workspaces is not yet implemented.\n");
        return;
    }

    if (con_is_floating(con)) {
        DLOG("Using FLOATINGCON instead\n");
        con = con->parent;
    }

    /* 1: save the container which is going to be focused after the current
     * container is moved away */
    Con *focus_next = con_next_focused(con);

    /* 2: get the focused container of this workspace */
    Con *next = con_descend_focused(workspace);

    /* 3: we go up one level, but only when next is a normal container */
    if (next->type != CT_WORKSPACE)
        next = next->parent;

    /* 4: if the target container is floating, we get the workspace instead.
     * Only tiling windows need to get inserted next to the current container.
     * */
    Con *floatingcon = con_inside_floating(next);
    if (floatingcon != NULL) {
        DLOG("floatingcon, going up even further\n");
        next = floatingcon->parent;
    }

    DLOG("Re-attaching container to %p / %s\n", next, next->name);
    /* 5: re-attach the con to the parent of this focused container */
    Con *parent = con->parent;
    con_detach(con);
    con_attach(con, next, false);

    /* 6: fix the percentages */
    con_fix_percent(parent);
    con->percent = 0.0;
    con_fix_percent(next);

    /* 7: focus the con on the target workspace (the X focus is only updated by
     * calling tree_render(), so for the "real" focus this is a no-op) */
    con_focus(con);

    /* 8: keep focus on the current workspace */
    con_focus(focus_next);

    CALL(parent, on_remove_child);
}

/*
 * Returns the orientation of the given container (for stacked containers,
 * vertical orientation is used regardless of the actual orientation of the
 * container).
 *
 */
int con_orientation(Con *con) {
    /* stacking containers behave like they are in vertical orientation */
    if (con->layout == L_STACKED)
        return VERT;

    if (con->layout == L_TABBED)
        return HORIZ;

    return con->orientation;
}

/*
 * Returns the container which will be focused next when the given container
 * is not available anymore. Called in tree_close and con_move_to_workspace
 * to properly restore focus.
 *
 */
Con *con_next_focused(Con *con) {
    Con *next;
    /* floating containers are attached to a workspace, so we focus either the
     * next floating container (if any) or the workspace itself. */
    if (con->type == CT_FLOATING_CON) {
        DLOG("selecting next for CT_FLOATING_CON\n");
        next = TAILQ_NEXT(con, floating_windows);
        if (next == TAILQ_END(&(parent->floating_head))) {
            Con *ws = con_get_workspace(con);
            next = ws;
            DLOG("no more floating containers for next = %p, restoring workspace focus\n", next);
            while (next != TAILQ_END(&(ws->focus_head)) && !TAILQ_EMPTY(&(next->focus_head))) {
                next = TAILQ_FIRST(&(next->focus_head));
                if (next == con) {
                    DLOG("skipping container itself, we want the next client\n");
                    next = TAILQ_NEXT(next, focused);
                }
            }
            if (next == TAILQ_END(&(ws->focus_head))) {
                DLOG("Focus list empty, returning ws\n");
                next = ws;
            }
        }
        return next;
    }

    /* try to focus the next container on the same level as this one */
    next = TAILQ_NEXT(con, focused);

    /* if that was not possible, go up to its parent */
    if (next == TAILQ_END(&(parent->nodes_head)))
        next = con->parent;

    /* now go down the focus stack as far as
     * possible, excluding the current container */
    while (!TAILQ_EMPTY(&(next->focus_head)) &&
           TAILQ_FIRST(&(next->focus_head)) != con)
        next = TAILQ_FIRST(&(next->focus_head));

    return next;
}

/*
 * Get the next/previous container in the specified orientation. This may
 * travel up until it finds a container with suitable orientation.
 *
 */
Con *con_get_next(Con *con, char way, orientation_t orientation) {
    DLOG("con_get_next(way=%c, orientation=%d)\n", way, orientation);
    /* 1: get the first parent with the same orientation */
    Con *cur = con;
    while (con_orientation(cur->parent) != orientation) {
        DLOG("need to go one level further up\n");
        if (cur->parent->type == CT_WORKSPACE) {
            LOG("that's a workspace, we can't go further up\n");
            return NULL;
        }
        cur = cur->parent;
    }

    /* 2: chose next (or previous) */
    Con *next;
    if (way == 'n') {
        next = TAILQ_NEXT(cur, nodes);
        /* if we are at the end of the list, we need to wrap */
        if (next == TAILQ_END(&(parent->nodes_head)))
            return NULL;
    } else {
        next = TAILQ_PREV(cur, nodes_head, nodes);
        /* if we are at the end of the list, we need to wrap */
        if (next == TAILQ_END(&(cur->nodes_head)))
            return NULL;
    }
    DLOG("next = %p\n", next);

    return next;
}

/*
 * Returns the focused con inside this client, descending the tree as far as
 * possible. This comes in handy when attaching a con to a workspace at the
 * currently focused position, for example.
 *
 */
Con *con_descend_focused(Con *con) {
    Con *next = con;
    while (!TAILQ_EMPTY(&(next->focus_head)))
        next = TAILQ_FIRST(&(next->focus_head));
    return next;
}

/*
 * Returns a "relative" Rect which contains the amount of pixels that need to
 * be added to the original Rect to get the final position (obviously the
 * amount of pixels for normal, 1pixel and borderless are different).
 *
 */
Rect con_border_style_rect(Con *con) {
    switch (con_border_style(con)) {
    case BS_NORMAL:
        return (Rect){2, 0, -(2 * 2), -2};

    case BS_1PIXEL:
        return (Rect){1, 1, -2, -2};

    case BS_NONE:
        return (Rect){0, 0, 0, 0};

    default:
        assert(false);
    }
}

/*
 * Use this function to get a container’s border style. This is important
 * because when inside a stack, the border style is always BS_NORMAL.
 * For tabbed mode, the same applies, with one exception: when the container is
 * borderless and the only element in the tabbed container, the border is not
 * rendered.
 *
 */
int con_border_style(Con *con) {
    Con *fs = con_get_fullscreen_con(con->parent);
    if (fs == con) {
        DLOG("this one is fullscreen! overriding BS_NONE\n");
        return BS_NONE;
    }

    if (con->parent->layout == L_STACKED)
        return (con_num_children(con->parent) == 1 ? con->border_style : BS_NORMAL);

    if (con->parent->layout == L_TABBED && con->border_style != BS_NORMAL)
        return (con_num_children(con->parent) == 1 ? con->border_style : BS_NORMAL);

    return con->border_style;
}

/*
 * This function changes the layout of a given container. Use it to handle
 * special cases like changing a whole workspace to stacked/tabbed (creates a
 * new split container before).
 *
 */
void con_set_layout(Con *con, int layout) {
    /* When the container type is CT_WORKSPACE, the user wants to change the
     * whole workspace into stacked/tabbed mode. To do this and still allow
     * intuitive operations (like level-up and then opening a new window), we
     * need to create a new split container. */
    if (con->type == CT_WORKSPACE) {
        DLOG("Creating new split container\n");
        /* 1: create a new split container */
        Con *new = con_new(NULL);
        new->parent = con;

        /* 2: set the requested layout on the split con */
        new->layout = layout;

        /* 3: While the layout is irrelevant in stacked/tabbed mode, it needs
         * to be set. Otherwise, this con will not be interpreted as a split
         * container. */
        new->orientation = HORIZ;

        Con *old_focused = TAILQ_FIRST(&(con->focus_head));
        if (old_focused == TAILQ_END(&(con->focus_head)))
            old_focused = NULL;

        /* 4: move the existing cons of this workspace below the new con */
        DLOG("Moving cons\n");
        Con *child;
        while (!TAILQ_EMPTY(&(con->nodes_head))) {
            child = TAILQ_FIRST(&(con->nodes_head));
            con_detach(child);
            con_attach(child, new, true);
        }

        /* 4: attach the new split container to the workspace */
        DLOG("Attaching new split to ws\n");
        con_attach(new, con, false);

        if (old_focused)
            con_focus(old_focused);

        tree_flatten(croot);

        return;
    }

    con->layout = layout;
}

static void con_on_remove_child(Con *con) {
    /* Nothing to do for workspaces */
    if (con->type == CT_WORKSPACE)
        return;

    /* TODO: check if this container would swallow any other client and
     * don’t close it automatically. */
    DLOG("on_remove_child\n");
    if (con_num_children(con) == 0) {
        DLOG("Container empty, closing\n");
        tree_close(con, false, false);
    }
}
