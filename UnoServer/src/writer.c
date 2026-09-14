#include "../include/writer.h"
#include "../include/server.h"

#include <stdio.h>


DWORD WINAPI writerThread(LPVOID arg) {
    Server *server = (Server *) arg;

    if (server == NULL) {
        return 1;
    }


    printf(
        "[WRITER] Thread iniciada.\n"
    );


    while (server->running) {
        Mensagem response;


        if (responseQueuePop(
                &server->responseQueue,
                &response
            ) != 0) {
            break;
        }


        /*
         * ====================================================
         * TEMPORÁRIO
         * ====================================================
         *
         * Futuramente:
         *
         *     int jogadorId =
         *         response.estado.jogador.id;
         *
         *     SOCKET socket =
         *         server->playerSockets[jogadorId];
         *
         *     send(...)
         */


        printf(
            "[WRITER] Resposta recebida da fila. "
            "tipo=%d\n",
            response.tipo
        );
    }


    printf(
        "[WRITER] Thread finalizada.\n"
    );

    return 0;
}
