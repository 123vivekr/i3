/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010-2011 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 */
#ifndef TRAYCLIENT_H_
#define TRAYCLIENT_H_

#include "common.h"

typedef struct trayclient trayclient;

TAILQ_HEAD(tc_head, trayclient);

struct trayclient {
    xcb_window_t       win;         /* The window ID of the tray client */
    bool               mapped;      /* Whether this window is mapped */

    TAILQ_ENTRY(trayclient) tailq;  /* Pointer for the TAILQ-Macro */
};

#endif
