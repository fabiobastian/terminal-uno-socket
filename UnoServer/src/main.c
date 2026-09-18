#include <stdio.h>
#include <stdlib.h>

#include <winsock2.h>
#include <windows.h>

#include "../include/server.h"
#include "../include/acceptor.h"
#include "../include/worker.h"
#include "../include/writer.h"

#pragma comment(lib, "ws2_32.lib")

int inicializarWinsock(void) {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Erro ao inicializar o Winsock.\n");

        return -1;
    }

    return 0;
}


int inicializarServer(Server *server) {
    server->serverSocket = INVALID_SOCKET;

    server->playerSockets[0] = INVALID_SOCKET;

    server->playerSockets[1] = INVALID_SOCKET;

    server->running = 1;


    if (server_init(server) != 0) {
        return -1;
    }

    return 0;
}


int iniciarThreads(Server *server, HANDLE *acceptor, HANDLE *worker, HANDLE *writer) {
    *acceptor = CreateThread(
        NULL,
        0,
        acceptorThread,
        server,
        0,
        NULL
    );

    if (*acceptor == NULL) {
        printf("Erro ao criar thread acceptor.\n");

        return -1;
    }


    *worker = CreateThread(
        NULL,
        0,
        workerThread,
        server,
        0,
        NULL
    );

    if (*worker == NULL) {
        printf("Erro ao criar thread worker.\n");

        server_request_shutdown(server);

        WaitForSingleObject(*acceptor, INFINITE);

        CloseHandle(*acceptor);

        return -1;
    }


    *writer = CreateThread(
        NULL,
        0,
        writerThread,
        server,
        0,
        NULL
    );

    if (*writer == NULL) {
        printf("Erro ao criar thread writer.\n");

        server_request_shutdown(server);

        WaitForSingleObject(*acceptor, INFINITE);

        WaitForSingleObject(*worker,INFINITE);

        CloseHandle(*acceptor);
        CloseHandle(*worker);

        return -1;
    }


    return 0;
}


int main(void) {
    Server server;

    HANDLE acceptor;
    HANDLE worker;
    HANDLE writer;


    /*
     * ========================================================
     * WINSOCK
     * ========================================================
     */

    if (inicializarWinsock() != 0) {
        return EXIT_FAILURE;
    }


    /*
     * ========================================================
     * SERVER
     * ========================================================
     */

    if (inicializarServer(&server) != 0) {
        WSACleanup();

        return EXIT_FAILURE;
    }


    /*
     * ========================================================
     * THREADS
     * ========================================================
     */

    if (iniciarThreads(&server, &acceptor, &worker, &writer) != 0) {
        server_shutdown(&server);

        WSACleanup();

        return EXIT_FAILURE;
    }


    /*
     * ========================================================
     * LOOP PRINCIPAL
     * ========================================================
     *
     * Por enquanto o main apenas aguarda
     * o comando de encerramento.
     *
     * Futuramente esta parte pode ser substituída
     * por outra forma de controle do servidor.
     */

    printf("\n");
    printf("Servidor iniciado.\n");

    printf("Digite 'q' e ENTER para encerrar.\n");


    while (server.running) {
        int caractere = getchar();

        if (caractere == 'q' || caractere == 'Q') {
            server_request_shutdown(
                &server
            );

            break;
        }
    }


    /*
     * ========================================================
     * AGUARDAR THREADS
     * ========================================================
     */

    WaitForSingleObject(acceptor, INFINITE);

    WaitForSingleObject(worker,INFINITE);

    WaitForSingleObject(writer,INFINITE);


    /*
     * ========================================================
     * LIBERAR HANDLES
     * ========================================================
     */

    CloseHandle(acceptor);
    CloseHandle(worker);
    CloseHandle(writer);


    /*
     * ========================================================
     * LIBERAR SERVER
     * ========================================================
     */

    server_shutdown(&server);


    /*
     * ========================================================
     * WINSOCK
     * ========================================================
     */

    WSACleanup();


    return EXIT_SUCCESS;
}
