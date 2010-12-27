/*
 * vim:ts=4:sw=4:expandtab
 */

#include "all.h"

struct Con *croot;
struct Con *focused;

struct all_cons_head all_cons = TAILQ_HEAD_INITIALIZER(all_cons);

/*
 * Loads tree from ~/.i3/_restart.json (used for in-place restarts).
 *
 */
bool tree_restore(const char *path) {
    char *globbed = resolve_tilde(path);

    if (!path_exists(globbed)) {
        LOG("%s does not exist, not restoring tree\n", globbed);
        free(globbed);
        return false;
    }

    /* TODO: refactor the following */
    croot = con_new(NULL);
    focused = croot;

    tree_append_json(globbed);

    printf("appended tree, using new root\n");
    croot = TAILQ_FIRST(&(croot->nodes_head));
    printf("new root = %p\n", croot);
    Con *out = TAILQ_FIRST(&(croot->nodes_head));
    printf("out = %p\n", out);
    Con *ws = TAILQ_FIRST(&(out->nodes_head));
    printf("ws = %p\n", ws);

    return true;
}

/*
 * Initializes the tree by creating the root node, adding all RandR outputs
 * to the tree (that means randr_init() has to be called before) and
 * assigning a workspace to each RandR output.
 *
 */
void tree_init() {
    Output *output;

    croot = con_new(NULL);
    croot->name = "root";
    croot->type = CT_ROOT;

    Con *ws;
    int c = 1;
    /* add the outputs */
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->active)
            continue;

        Con *oc = con_new(croot);
        oc->name = strdup(output->name);
        oc->type = CT_OUTPUT;
        oc->rect = output->rect;
        output->con = oc;

        char *name;
        asprintf(&name, "[i3 con] output %s", oc->name);
        x_set_name(oc, name);
        free(name);

        /* add a workspace to this output */
        ws = con_new(NULL);
        ws->type = CT_WORKSPACE;
        ws->num = c;
        asprintf(&(ws->name), "%d", c);
        c++;
        con_attach(ws, oc, false);

        asprintf(&name, "[i3 con] workspace %s", ws->name);
        x_set_name(ws, name);
        free(name);

        ws->fullscreen_mode = CF_OUTPUT;
        ws->orientation = HORIZ;
    }

    con_focus(ws);
}

/*
 * Opens an empty container in the current container
 *
 */
Con *tree_open_con(Con *con) {
    if (con == NULL) {
        /* every focusable Con has a parent (outputs have parent root) */
        con = focused->parent;
        /* If the parent is an output, we are on a workspace. In this case,
         * the new container needs to be opened as a leaf of the workspace. */
        if (con->type == CT_OUTPUT)
            con = focused;
        /* If the currently focused container is a floating container, we
         * attach the new container to the workspace */
        if (con->type == CT_FLOATING_CON)
            con = con->parent;
    }

    assert(con != NULL);

    /* 3: re-calculate child->percent for each child */
    con_fix_percent(con, WINDOW_ADD);

    /* 4: add a new container leaf to this con */
    Con *new = con_new(con);
    con_focus(new);

    return new;
}

/*
 * vanishing is the container that is about to be closed (so any floating
 * client which has old_parent == vanishing needs to be "re-parented").
 *
 */
static void fix_floating_parent(Con *con, Con *vanishing) {
    Con *child;

    if (con->old_parent == vanishing) {
        LOG("Fixing vanishing old_parent (%p) of container %p to be %p\n",
                vanishing, con, vanishing->parent);
        con->old_parent = vanishing->parent;
    }

    TAILQ_FOREACH(child, &(con->floating_head), floating_windows)
        fix_floating_parent(child, vanishing);

    TAILQ_FOREACH(child, &(con->nodes_head), nodes)
        fix_floating_parent(child, vanishing);
}

/*
 * Closes the given container including all children
 *
 */
void tree_close(Con *con, bool kill_window, bool dont_kill_parent) {
    Con *parent = con->parent;

    /* check floating clients and adjust old_parent if necessary */
    fix_floating_parent(croot, con);

    /* Get the container which is next focused */
    Con *next = con_next_focused(con);

    DLOG("closing %p, kill_window = %d\n", con, kill_window);
    Con *child;
    /* We cannot use TAILQ_FOREACH because the children get deleted
     * in their parent’s nodes_head */
    while (!TAILQ_EMPTY(&(con->nodes_head))) {
        child = TAILQ_FIRST(&(con->nodes_head));
        DLOG("killing child=%p\n", child);
        tree_close(child, kill_window, true);
    }

    if (con->window != NULL) {
        if (kill_window)
            x_window_kill(con->window->id);
        else {
            /* un-parent the window */
            xcb_reparent_window(conn, con->window->id, root, 0, 0);
            /* TODO: client_unmap to set state to withdrawn */

        }
        free(con->window);
    }

    /* kill the X11 part of this container */
    x_con_kill(con);

    con_detach(con);
    con_fix_percent(parent, WINDOW_REMOVE);

    if (con_is_floating(con)) {
        DLOG("Container was floating, killing floating container\n");
        tree_close(parent, false, false);
        next = NULL;
    }

    free(con->name);
    TAILQ_REMOVE(&all_cons, con, all_cons);
    free(con);

    /* in the case of floating windows, we already focused another container
     * when closing the parent, so we can exit now. */
    if (!next)
        return;

    DLOG("focusing %p / %s\n", next, next->name);
    /* TODO: check if the container (or one of its children) was focused */
    con_focus(next);

    /* check if the parent container is empty now and close it */
    if (!dont_kill_parent &&
        parent->type != CT_WORKSPACE &&
        TAILQ_EMPTY(&(parent->nodes_head))) {
        DLOG("Closing empty parent container\n");
        /* TODO: check if this container would swallow any other client and
         * don’t close it automatically. */
        tree_close(parent, false, false);
    }
}

/*
 * Closes the current container using tree_close().
 *
 */
void tree_close_con() {
    assert(focused != NULL);
    if (focused->type == CT_WORKSPACE) {
        LOG("Cannot close workspace\n");
        return;
    }

    /* Kill con */
    tree_close(focused, true, false);
}

/*
 * Splits (horizontally or vertically) the given container by creating a new
 * container which contains the old one and the future ones.
 *
 */
void tree_split(Con *con, orientation_t orientation) {
    /* for a workspace, we just need to change orientation */
    if (con->type == CT_WORKSPACE) {
        DLOG("Workspace, simply changing orientation to %d\n", orientation);
        con->orientation = orientation;
        return;
    }

    Con *parent = con->parent;
    /* if we are in a container whose parent contains only one
     * child (its split functionality is unused so far), we just change the
     * orientation (more intuitive than splitting again) */
    if (con_num_children(parent) == 1) {
        parent->orientation = orientation;
        DLOG("Just changing orientation of existing container\n");
        return;
    }

    DLOG("Splitting in orientation %d\n", orientation);

    /* 2: replace it with a new Con */
    Con *new = con_new(NULL);
    TAILQ_REPLACE(&(parent->nodes_head), con, new, nodes);
    TAILQ_REPLACE(&(parent->focus_head), con, new, focused);
    new->parent = parent;
    new->orientation = orientation;

    /* 3: swap 'percent' (resize factor) */
    new->percent = con->percent;
    con->percent = 0.0;

    /* 4: add it as a child to the new Con */
    con_attach(con, new, false);
}

/*
 * Moves focus one level up.
 *
 */
void level_up() {
    /* We can focus up to the workspace, but not any higher in the tree */
    if (focused->parent->type != CT_CON &&
        focused->parent->type != CT_WORKSPACE) {
        printf("cannot go up\n");
        return;
    }
    con_focus(focused->parent);
}

/*
 * Moves focus one level down.
 *
 */
void level_down() {
    /* Go down the focus stack of the current node */
    Con *next = TAILQ_FIRST(&(focused->focus_head));
    if (next == TAILQ_END(&(focused->focus_head))) {
        printf("cannot go down\n");
        return;
    }
    con_focus(next);
}

static void mark_unmapped(Con *con) {
    Con *current;

    con->mapped = false;
    TAILQ_FOREACH(current, &(con->nodes_head), nodes)
        mark_unmapped(current);
    if (con->type == CT_WORKSPACE) {
        TAILQ_FOREACH(current, &(con->floating_head), floating_windows) {
            current->mapped = false;
            Con *child = TAILQ_FIRST(&(current->nodes_head));
            child->mapped = false;
        }
    }
}

/*
 * Renders the tree, that is rendering all outputs using render_con() and
 * pushing the changes to X11 using x_push_changes().
 *
 */
void tree_render() {
    if (croot == NULL)
        return;

    printf("-- BEGIN RENDERING --\n");
    /* Reset map state for all nodes in tree */
    /* TODO: a nicer method to walk all nodes would be good, maybe? */
    mark_unmapped(croot);
    croot->mapped = true;

    /* We start rendering at an output */
    Con *output;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        printf("output %p / %s\n", output, output->name);
        render_con(output, false);
    }
    x_push_changes(croot);
    printf("-- END RENDERING --\n");
}

/*
 * Changes focus in the given way (next/previous) and given orientation
 * (horizontal/vertical).
 *
 */
void tree_next(char way, orientation_t orientation) {
    /* 1: get the first parent with the same orientation */
    Con *parent = focused->parent;
    while (focused->type != CT_WORKSPACE &&
           con_orientation(parent) != orientation) {
        LOG("need to go one level further up\n");
        /* if the current parent is an output, we are at a workspace
         * and the orientation still does not match */
        if (parent->type == CT_WORKSPACE)
            return;
        parent = parent->parent;
    }
    Con *current = TAILQ_FIRST(&(parent->focus_head));
    assert(current != TAILQ_END(&(parent->focus_head)));

    /* 2: chose next (or previous) */
    Con *next;
    if (way == 'n') {
        next = TAILQ_NEXT(current, nodes);
        /* if we are at the end of the list, we need to wrap */
        if (next == TAILQ_END(&(parent->nodes_head)))
            next = TAILQ_FIRST(&(parent->nodes_head));
    } else {
        next = TAILQ_PREV(current, nodes_head, nodes);
        /* if we are at the end of the list, we need to wrap */
        if (next == TAILQ_END(&(parent->nodes_head)))
            next = TAILQ_LAST(&(parent->nodes_head), nodes_head);
    }

    /* 3: focus choice comes in here. at the moment we will go down
     * until we find a window */
    /* TODO: check for window, atm we only go down as far as possible */
    while (!TAILQ_EMPTY(&(next->focus_head)))
        next = TAILQ_FIRST(&(next->focus_head));

    DLOG("focusing %p\n", next);
    con_focus(next);
}

/*
 * Moves the current container in the given way (next/previous) and given
 * orientation (horizontal/vertical).
 *
 */
void tree_move(char way, orientation_t orientation) {
    /* 1: get the first parent with the same orientation */
    Con *parent = focused->parent;
    Con *old_parent = parent;
    if (focused->type == CT_WORKSPACE)
        return;
    bool level_changed = false;
    while (con_orientation(parent) != orientation) {
        DLOG("need to go one level further up\n");
        /* If the current parent is an output, we are at a workspace
         * and the orientation still does not match. In this case, we split the
         * workspace to have the same look & feel as in older i3 releases. */
        if (parent->type == CT_WORKSPACE) {
            DLOG("Arrived at workspace, splitting...\n");
            /* 1: create a new split container */
            Con *new = con_new(NULL);
            new->parent = parent;

            /* 2: copy layout and orientation from workspace */
            new->layout = parent->layout;
            new->orientation = parent->orientation;

            Con *old_focused = TAILQ_FIRST(&(parent->focus_head));
            if (old_focused == TAILQ_END(&(parent->focus_head)))
                old_focused = NULL;

            /* 3: move the existing cons of this workspace below the new con */
            DLOG("Moving cons\n");
            Con *child;
            while (!TAILQ_EMPTY(&(parent->nodes_head))) {
                child = TAILQ_FIRST(&(parent->nodes_head));
                con_detach(child);
                con_attach(child, new, true);
            }

            /* 4: switch workspace orientation */
            parent->orientation = orientation;

            /* 4: attach the new split container to the workspace */
            DLOG("Attaching new split to ws\n");
            con_attach(new, parent, false);

            if (old_focused)
                con_focus(old_focused);

            level_changed = true;

            break;
        }
        parent = parent->parent;
        level_changed = true;
    }
    Con *current = TAILQ_FIRST(&(parent->focus_head));
    assert(current != TAILQ_END(&(parent->focus_head)));

    /* 2: chose next (or previous) */
    Con *next = current;
    if (way == 'n') {
        LOG("i would insert it after %p / %s\n", next, next->name);

        /* Have a look at the next container: If there is no next container or
         * if it is a leaf node, we move the focused one left to it. However,
         * for split containers, we descend into it. */
        next = TAILQ_NEXT(next, nodes);
        if (next == TAILQ_END(&(next->parent->nodes_head))) {
            if (focused == current)
                return;
            next = current;
        } else {
            if (level_changed && con_is_leaf(next)) {
                next = current;
            } else {
                /* if this is a split container, we need to go down */
                while (!TAILQ_EMPTY(&(next->focus_head)))
                    next = TAILQ_FIRST(&(next->focus_head));
            }
        }

        con_detach(focused);
        focused->parent = next->parent;

        TAILQ_INSERT_AFTER(&(next->parent->nodes_head), next, focused, nodes);
        TAILQ_INSERT_HEAD(&(next->parent->focus_head), focused, focused);
        /* TODO: don’t influence focus handling? */
    } else {
        LOG("i would insert it before %p / %s\n", current, current->name);
        bool gone_down = false;
        next = TAILQ_PREV(next, nodes_head, nodes);
        if (next == TAILQ_END(&(next->parent->nodes_head))) {
            if (focused == current)
                return;
            next = current;
        } else {
            if (level_changed && con_is_leaf(next)) {
                next = current;
            } else {
                /* if this is a split container, we need to go down */
                while (!TAILQ_EMPTY(&(next->focus_head))) {
                    gone_down = true;
                    next = TAILQ_FIRST(&(next->focus_head));
                }
            }
        }

        con_detach(focused);
        focused->parent = next->parent;

        /* After going down in the tree, we insert the container *after*
         * the currently focused one even though the command used "before".
         * This is to keep the user experience clear, since the before/after
         * only signifies the direction of the movement on top-level */
        if (gone_down)
            TAILQ_INSERT_AFTER(&(next->parent->nodes_head), next, focused, nodes);
        else TAILQ_INSERT_BEFORE(next, focused, nodes);
        TAILQ_INSERT_HEAD(&(next->parent->focus_head), focused, focused);
        /* TODO: don’t influence focus handling? */
    }

    /* We need to call con_focus() to fix the focus stack "above" the container
     * we just inserted the focused container into (otherwise, the parent
     * container(s) would still point to the old container(s)). */
    con_focus(focused);

    if (con_num_children(old_parent) == 0) {
        DLOG("Old container empty after moving. Let's close it\n");
        tree_close(old_parent, false, false);
    }
}
