#include "../include/writer.h"
#include "../include/server.h"

#include <stdio.h>

static int sendAll(SOCKET socket, const char *buffer, int tamanho) {
    int totalEnviado = 0;

    while (totalEnviado < tamanho) {

        int enviado = send(
            socket,
            buffer + totalEnviado,
            tamanho - totalEnviado,
            0
        );

        if (enviado == SOCKET_ERROR) {
            return -1;
        }

        totalEnviado += enviado;
    }

    return totalEnviado;
}


DWORD WINAPI writerThread(LPVOID arg) {
    Server *server = (Server *)arg;

    if (server == NULL) {
        return 1;
    }

    printf("[WRITER] Thread iniciada.\n");


    while (server->running) {
        Mensagem response;


        /*
         * Aguarda uma resposta na fila.
         */
        if (responseQueuePop(&server->responseQueue, &response) != 0) {
            break;
        }


        /*
         * O Worker colocou o ID do jogador
         * que deve receber a mensagem.
         */
        int jogadorId = response.estado.jogador.id;


        if (jogadorId < 0 || jogadorId >= MAX_PLAYERS) {

            printf("[WRITER] Jogador invalido: %d\n", jogadorId);

            continue;
        }


        SOCKET socket = server->playerSockets[jogadorId];


        if (socket == INVALID_SOCKET) {

            printf("[WRITER] Socket do jogador %d invalido.\n", jogadorId + 1);

            continue;
        }

        printf("[WRITER] Enviando resposta para jogador %d.\n", jogadorId + 1);


        /*
         * Envia a Mensagem inteira.
         */
        if (sendAll(
            socket,
            (const char *)&response,
            sizeof(Mensagem)
        ) < 0) {

            printf("[WRITER] Erro ao enviar resposta para jogador %d. WSAError: %d\n",
                jogadorId + 1,
                WSAGetLastError()
            );

            continue;
        }

        printf("[WRITER] Resposta enviada para jogador %d.\n",jogadorId + 1);
    }


    printf("[WRITER] Thread finalizada.\n");

    return 0;
}