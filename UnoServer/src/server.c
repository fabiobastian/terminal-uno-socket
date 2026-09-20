#include "../include/server.h"

#include <stdio.h>

int server_init(Server *server) {
    struct sockaddr_in serverAddress = {0};

    if (requestQueueInit(&server->requestQueue) != 0) {
        printf("Erro ao inicializar request queue.\n");

        return -1;
    }


    if (responseQueueInit(&server->responseQueue) != 0) {
        printf("Erro ao inicializar response queue.\n");

        requestQueueDestroy(&server->requestQueue);

        return -1;
    }

    server->gameStartedEvent = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        NULL
    );

    if (server->gameStartedEvent == NULL) {
        printf("Erro ao criar evento da partida.\n");

        return -1;
    }


    server->serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (server->serverSocket == INVALID_SOCKET) {
        printf("Erro ao criar socket. WSAError: %d\n", WSAGetLastError());

        responseQueueDestroy(&server->responseQueue);

        requestQueueDestroy(&server->requestQueue);

        return -1;
    }


    serverAddress.sin_family = AF_INET;

    serverAddress.sin_addr.s_addr = INADDR_ANY;

    serverAddress.sin_port = htons(SERVER_PORT);


    if (bind(
            server->serverSocket,
            (struct sockaddr *) &serverAddress,
            sizeof(serverAddress)
        ) == SOCKET_ERROR) {
        printf("Erro ao executar bind(). WSAError: %d\n", WSAGetLastError());

        closesocket(server->serverSocket);

        server->serverSocket = INVALID_SOCKET;

        responseQueueDestroy(&server->responseQueue);

        requestQueueDestroy(&server->requestQueue);

        return -1;
    }


    if (listen(server->serverSocket, MAX_PLAYERS) == SOCKET_ERROR) {
        printf("Erro ao executar listen(). WSAError: %d\n",WSAGetLastError());

        closesocket(server->serverSocket);

        server->serverSocket = INVALID_SOCKET;

        responseQueueDestroy(&server->responseQueue);

        requestQueueDestroy(&server->requestQueue);

        return -1;
    }


    printf("Servidor escutando na porta %d...\n", SERVER_PORT);

    return 0;
}


/*
 * Apenas sinaliza que o servidor deve parar.
 *
 * Não destrói as filas.
 */
void server_request_shutdown(Server *server) {
    if (server == NULL) {
        return;
    }

    printf("[SERVER] Solicitando encerramento...\n");

    server->running = 0;

    if (server->gameStartedEvent != NULL) {
        SetEvent(server->gameStartedEvent);
    }

    /*
     * Acorda Worker e Writer caso estejam
     * bloqueados nas filas.
     */
    requestQueueShutdown(&server->requestQueue);

    responseQueueShutdown(&server->responseQueue);


    /*
     * Fecha o socket de escuta.
     *
     * Isso permite que o acceptor termine
     * caso esteja bloqueado em accept().
     */
    if (server->serverSocket != INVALID_SOCKET) {
        closesocket(server->serverSocket);

        server->serverSocket = INVALID_SOCKET;
    }


    /*
     * shutdown() nos sockets dos jogadores
     * faz recv() retornar e permite que
     * os listeners terminem.
     *
     * Quem faz closesocket() depois são
     * os próprios listeners.
     */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (server->playerSockets[i] != INVALID_SOCKET) {
            shutdown(server->playerSockets[i], SD_BOTH);
        }
    }
}


/*
 * Só deve ser chamada depois que todas
 * as threads terminaram.
 */
void server_shutdown(Server *server) {
    if (server == NULL) {
        return;
    }

    if (server->gameStartedEvent != NULL) {

        CloseHandle(server->gameStartedEvent);

        server->gameStartedEvent = NULL;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (server->playerSockets[i] != INVALID_SOCKET) {
            closesocket(server->playerSockets[i]);

            server->playerSockets[i] = INVALID_SOCKET;
        }
    }


    if (server->serverSocket != INVALID_SOCKET) {
        closesocket(server->serverSocket);

        server->serverSocket = INVALID_SOCKET;
    }


    responseQueueDestroy(&server->responseQueue);

    requestQueueDestroy(&server->requestQueue);
}
