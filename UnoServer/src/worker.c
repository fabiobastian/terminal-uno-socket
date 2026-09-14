#include "../include/worker.h"
#include "../include/server.h"

#include <stdio.h>


DWORD WINAPI workerThread(LPVOID arg) {
    Server *server = (Server *) arg;

    if (server == NULL) {
        return 1;
    }


    printf(
        "[WORKER] Thread iniciada.\n"
    );


    while (server->running) {
        Solicitacao request;


        /*
         * Espera uma requisição.
         *
         * Pode ficar bloqueado aqui.
         *
         * requestQueueShutdown() acordará
         * esta thread durante o encerramento.
         */
        if (requestQueuePop(
                &server->requestQueue,
                &request
            ) != 0) {
            break;
        }


        printf(
            "[WORKER] Processando request: "
            "jogador=%d, acao=%d, carta=%d\n",
            request.jogadorId + 1,
            request.acao,
            request.cartaId
        );


        /*
         * ====================================================
         * TEMPORÁRIO
         * ====================================================
         *
         * Aqui futuramente entra:
         *
         *     processarJogada(...)
         *
         * que vai acessar o Jogo.
         *
         * Por enquanto apenas recebemos
         * a requisição.
         */
    }


    printf(
        "[WORKER] Thread finalizada.\n"
    );

    return 0;
}
