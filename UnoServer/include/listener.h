#ifndef LISTENER_H
#define LISTENER_H

#include <winsock2.h>
#include <windows.h>

#include "server.h"

typedef struct {
    int playerId;
    SOCKET socket;
    Server *server;
} ListenerArgs;

DWORD WINAPI listenerThread(LPVOID arg);

#endif