/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 * src/ipc.c: Communicating with i3
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <i3/ipc.h>
#include <ev.h>

#include "common.h"

ev_io *i3_connection;

typedef void(*handler_t)(char*);

/*
 * Get a connect to the IPC-interface of i3 and return a filedescriptor
 *
 */
int get_ipc_fd(const char *socket_path) {
    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("ERROR: Could not create Socket!\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strcpy(addr.sun_path, socket_path);
    if (connect(sockfd, (const struct sockaddr*) &addr, sizeof(struct sockaddr_un)) < 0) {
        printf("ERROR: Could not connct to i3\n");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

/*
 * Called, when we get a reply to a command from i3.
 * Since i3 does not give us much feedback on commands, we do not much
 *
 */
void got_command_reply(char *reply) {
    /* TODO: Error handling for command-replies */
}

/*
 * Called, when we get a reply with workspaces-data
 *
 */
void got_workspace_reply(char *reply) {
    printf("Got Workspace-Data!\n");
    parse_workspaces_json(reply);
    draw_bars();
}

/*
 * Called, when we get a reply for a subscription.
 * Since i3 does not give us much feedback on commands, we do not much
 *
 */
void got_subscribe_reply(char *reply) {
    printf("Got Subscribe Reply: %s\n", reply);
    /* TODO: Error handling for subscribe-commands */
}

/*
 * Called, when we get a reply with outputs-data
 *
 */
void got_output_reply(char *reply) {
    printf("Parsing Outputs-JSON...\n");
    parse_outputs_json(reply);
    printf("Reconfiguring Windows...\n");
    reconfig_windows();
}

/* Data-structure to easily call the reply-handlers later */
handler_t reply_handlers[] = {
    &got_command_reply,
    &got_workspace_reply,
    &got_subscribe_reply,
    &got_output_reply,
};

/*
 * Called, when a workspace-event arrives (i.e. the user changed the workspace)
 *
 */
void got_workspace_event(char *event) {
    printf("Got Workspace Event!\n");
    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL);
}

/*
 * Called, when an output-event arrives (i.e. the screen-configuration changed)
 *
 */
void got_output_event(char *event) {
    printf("Got Output Event!\n");
    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_OUTPUTS, NULL);
    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL);
}

/* Data-structure to easily call the reply-handlers later */
handler_t event_handlers[] = {
    &got_workspace_event,
    &got_output_event
};

/*
 * Called, when we get a message from i3
 *
 */
void got_data(struct ev_loop *loop, ev_io *watcher, int events) {
    printf("Got data!\n");
    int fd = watcher->fd;

    /* First we only read the header, because we know it's length */
    uint32_t header_len = strlen(I3_IPC_MAGIC) + sizeof(uint32_t)*2;
    char *header = malloc(header_len);
    if (header == NULL) {
        printf("ERROR: Could not allocate memory!\n");
        exit(EXIT_FAILURE);
    }

    /* We first parse the fixed-length IPC-header, to know, how much data
     * we have to expect */
    uint32_t rec = 0;
    while (rec < header_len) {
        int n = read(fd, header + rec, header_len - rec);
        if (n == -1) {
            printf("ERROR: read() failed!\n");
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            printf("ERROR: Nothing to read!\n");
            exit(EXIT_FAILURE);
        }
        rec += n;
    }

    if (strncmp(header, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC))) {
        printf("ERROR: Wrong magic code: %.*s\n Expected: %s\n",
               (int) strlen(I3_IPC_MAGIC),
               header,
               I3_IPC_MAGIC);
        exit(EXIT_FAILURE);
    }

    char *walk = header + strlen(I3_IPC_MAGIC);
    uint32_t size = *((uint32_t*) walk);
    walk += sizeof(uint32_t);
    uint32_t type = *((uint32_t*) walk);

    /* Now that we know, what to expect, we can start read()ing the rest
     * of the message */
    char *buffer = malloc(size + 1);
    if (buffer == NULL) {
        printf("ERROR: Could not allocate memory!\n");
        exit(EXIT_FAILURE);
    }
    rec = 0;

    while (rec < size) {
        int n = read(fd, buffer + rec, size - rec);
        if (n == -1) {
            printf("ERROR: read() failed!\n");
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            printf("ERROR: Nothing to read!\n");
            exit(EXIT_FAILURE);
        }
        rec += n;
    }
    buffer[size] = '\0';

    /* And call the callback (indexed by the type) */
    if (type & (1 << 31)) {
        type ^= 1 << 31;
        event_handlers[type](buffer);
    } else {
        reply_handlers[type](buffer);
    }

    FREE(header);
    FREE(buffer);
}

/*
 * Sends a Message to i3.
 * type must be a valid I3_IPC_MESSAGE_TYPE (see i3/ipc.h for further information)
 *
 */
int i3_send_msg(uint32_t type, const char *payload) {
    uint32_t len = 0;
    if (payload != NULL) {
        len = strlen(payload);
    }

    /* We are a wellbehaved client and send a proper header first */
    uint32_t to_write = strlen (I3_IPC_MAGIC) + sizeof(uint32_t)*2 + len;
    /* TODO: I'm not entirely sure if this buffer really has to contain more
     * than the pure header (why not just write() the payload from *payload?),
     * but we leave it for now */
    char *buffer = malloc(to_write);
    if (buffer == NULL) {
        printf("ERROR: Could not allocate memory\n");
        exit(EXIT_FAILURE);
    }

    char *walk = buffer;

    strncpy(buffer, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC));
    walk += strlen(I3_IPC_MAGIC);
    memcpy(walk, &len, sizeof(uint32_t));
    walk += sizeof(uint32_t);
    memcpy(walk, &type, sizeof(uint32_t));
    walk += sizeof(uint32_t);

    strncpy(walk, payload, len);

    uint32_t written = 0;

    while (to_write > 0) {
        int n = write(i3_connection->fd, buffer + written, to_write);
        if (n == -1) {
            printf("ERROR: write() failed!\n");
            exit(EXIT_FAILURE);
        }

        to_write -= n;
        written += n;
    }

    FREE(buffer);

    return 1;
}

/*
 * Initiate a connection to i3.
 * socket-path must be a valid path to the ipc_socket of i3
 *
 */
int init_connection(const char *socket_path) {
    int sockfd = get_ipc_fd(socket_path);

    i3_connection = malloc(sizeof(ev_io));
    ev_io_init(i3_connection, &got_data, sockfd, EV_READ);
    ev_io_start(main_loop, i3_connection);

    return 1;
}

/*
 * Subscribe to all the i3-events, we need
 *
 */
void subscribe_events() {
    i3_send_msg(I3_IPC_MESSAGE_TYPE_SUBSCRIBE, "[ \"workspace\", \"output\" ]");
}
