#include "../include/acceptor.h"
#include "../include/server.h"
#include "../include/listener.h"

#include <stdio.h>
#include <stdlib.h>


DWORD WINAPI acceptorThread(LPVOID arg)
{
    Server *server = (Server *)arg;

    if (server == NULL) {
        return 1;
    }


    printf("[ACCEPTOR] Thread iniciada.\n");


    /*
     * ========================================================
     * JOGADOR 1
     * ========================================================
     */

    printf("[ACCEPTOR] Aguardando jogador 1...\n");


    server->playerSockets[0] = accept(
            server->serverSocket,
            NULL,
            NULL
        );


    if (server->playerSockets[0] ==INVALID_SOCKET) {

        if (!server->running) {
            return 0;
        }

        printf("[ACCEPTOR] Erro ao aceitar jogador 1.\n");

        return 1;
    }


    printf("[ACCEPTOR] Jogador 1 conectado.\n");


    if (!server->running) {
        return 0;
    }


    /*
     * ========================================================
     * JOGADOR 2
     * ========================================================
     */

    printf("[ACCEPTOR] Aguardando jogador 2...\n");


    server->playerSockets[1] = accept(
            server->serverSocket,
            NULL,
            NULL
        );


    if (server->playerSockets[1] == INVALID_SOCKET) {

        if (!server->running) {
            return 0;
        }

        printf("[ACCEPTOR] Erro ao aceitar jogador 2.\n");

        return 1;
    }


    printf("[ACCEPTOR] Jogador 2 conectado.\n");


    /*
     * ========================================================
     * LISTENER 1
     * ========================================================
     */

    ListenerArgs *player1Args = malloc(sizeof(ListenerArgs));


    if (player1Args == NULL) {

        printf("[ACCEPTOR] Erro ao alocar memoria para jogador 1.\n");

        return 1;
    }


    player1Args->playerId = 0;

    player1Args->socket = server->playerSockets[0];

    player1Args->server = server;


    HANDLE player1Listener = CreateThread(
            NULL,
            0,
            listenerThread,
            player1Args,
            0,
            NULL
        );


    if (player1Listener == NULL) {

        printf("[ACCEPTOR] Erro ao criar listener do jogador 1.\n");

        free(player1Args);

        return 1;
    }


    /*
     * ========================================================
     * LISTENER 2
     * ========================================================
     */

    ListenerArgs *player2Args = malloc(sizeof(ListenerArgs));


    if (player2Args == NULL) {

        printf("[ACCEPTOR] Erro ao alocar memoria para jogador 2.\n");

        server_request_shutdown(server);

        WaitForSingleObject(player1Listener, INFINITE);

        CloseHandle(player1Listener);

        return 1;
    }


    player2Args->playerId = 1;

    player2Args->socket = server->playerSockets[1];

    player2Args->server = server;

    HANDLE player2Listener = CreateThread(
            NULL,
            0,
            listenerThread,
            player2Args,
            0,
            NULL
        );

    if (player2Listener == NULL) {

        printf("[ACCEPTOR] Erro ao criar listener do jogador 2.\n");

        free(player2Args);

        server_request_shutdown(server);

        WaitForSingleObject(player1Listener,INFINITE);

        CloseHandle(player1Listener);

        return 1;
    }


    printf("[ACCEPTOR] Dois listeners iniciados.\n");


    /*
     * ========================================================
     * AGUARDAR LISTENERS
     * ========================================================
     */

    HANDLE listeners[] = {
        player1Listener,
        player2Listener
    };

    WaitForMultipleObjects(
        2,
        listeners,
        TRUE,
        INFINITE
    );


    CloseHandle(player1Listener);

    CloseHandle(player2Listener);


    printf("[ACCEPTOR] Listeners finalizados.\n");

    return 0;
}