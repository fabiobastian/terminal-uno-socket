#include "../include/worker.h"
#include "../include/server.h"

#include <stdio.h>
#include <string.h>


DWORD WINAPI workerThread(LPVOID arg) {
    Server *server = (Server *)arg;

    if (server == NULL) {
        return 1;
    }

    printf("[WORKER] Thread iniciada.\n");

    while (server->running) {

        Solicitacao request;

        /*
         * Aguarda uma solicitacao na fila.
         */
        if (requestQueuePop(&server->requestQueue, &request) != 0) {
            break;
        }

        printf("[WORKER] Processando request: jogador=%d, acao=%d, carta=%d\n",
            request.jogadorId + 1,
            request.acao,
            request.cartaId
        );


        /*
         * ====================================================
         * RESPOSTA DE TESTE
         * ====================================================
         */

        Mensagem response;

        memset(
            &response,
            0,
            sizeof(response)
        );


        /*
         * Define o tipo da mensagem.
         */
        response.tipo = MSG_ESTADO_JOGO;


        /*
         * Define para qual jogador
         * essa resposta deve ser enviada.
         *
         * O Writer utilizará esse ID
         * para escolher o socket.
         */
        response.estado.jogador.id = request.jogadorId;


        /*
         * Informações simples apenas para
         * conseguirmos visualizar o teste.
         */
        response.estado.partida.numeroRodada = 1;

        response.estado.partida.suaVez = true;

        response.estado.partida.numeroCartasAdversario = 7;

        strcpy(response.estado.partida.nomeAdversario, "Jogador Teste");


        /*
         * Coloca a resposta na ResponseQueue.
         */
        if (responseQueuePush(&server->responseQueue,response) != 0) {

            printf("[WORKER] Erro ao adicionar resposta na fila.\n");

            break;
        }


        printf("[WORKER] Resposta adicionada na response queue.\n");
    }

    printf("[WORKER] Thread finalizada.\n");

    return 0;
}