#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>

#include "queue.h"

#define SERVER_PORT 8080
#define MAX_PLAYERS 2

typedef struct {
    SOCKET serverSocket;
    SOCKET playerSockets[MAX_PLAYERS];

    int running;

    RequestQueue requestQueue;
    ResponseQueue responseQueue;

    HANDLE gameStartedEvent;

} Server;


int server_init(Server *server);

void server_request_shutdown(Server *server);

void server_shutdown(Server *server);

#endif