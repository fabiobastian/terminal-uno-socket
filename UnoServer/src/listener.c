#include "../include/listener.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * Recebe exatamente a quantidade de bytes solicitada.
 *
 * TCP não garante que uma chamada de recv()
 * irá retornar todos os bytes solicitados.
 *
 * Retorno:
 *
 *     > 0  -> quantidade total recebida
 *     0    -> conexão encerrada
 *    -1    -> erro
 */
static int recvAll(
    SOCKET socket,
    char *buffer,
    int tamanho
)
{
    int totalRecebido = 0;

    while (totalRecebido < tamanho) {

        int recebido = recv(
            socket,
            buffer + totalRecebido,
            tamanho - totalRecebido,
            0
        );

        if (recebido == 0) {
            /*
             * O cliente encerrou a conexão.
             */
            return 0;
        }

        if (recebido == SOCKET_ERROR) {
            return -1;
        }

        totalRecebido += recebido;
    }

    return totalRecebido;
}


DWORD WINAPI listenerThread(LPVOID arg)
{
    ListenerArgs *args = (ListenerArgs *)arg;

    if (args == NULL) {
        return 1;
    }

    printf(
        "[LISTENER] Jogador %d conectado.\n",
        args->playerId + 1
    );


    /*
     * ========================================================
     * LOOP DE RECEBIMENTO
     * ========================================================
     *
     * O Listener não processa as regras do UNO.
     *
     * Ele apenas:
     *
     *     recv()
     *       ↓
     *     Request
     *       ↓
     *     Request Queue
     */

    while (1) {

        Solicitacao request;


        /*
         * O ID do jogador não precisa ser confiado
         * ao cliente.
         *
         * O servidor já sabe qual socket pertence
         * a este jogador.
         */
        request.jogadorId = args->playerId;


        /*
         * Receber o restante da solicitação.
         *
         * Como jogadorId pertence ao controle do servidor,
         * recebemos a struct inteira temporariamente.
         */

        int resultado = recvAll(
            args->socket,
            (char *)&request,
            sizeof(Solicitacao)
        );


        /*
         * ====================================================
         * CLIENTE DESCONECTOU
         * ====================================================
         */

        if (resultado == 0) {

            printf(
                "[LISTENER] Jogador %d desconectou.\n",
                args->playerId + 1
            );

            break;
        }


        /*
         * ====================================================
         * ERRO
         * ====================================================
         */

        if (resultado < 0) {

            printf(
                "[LISTENER] Erro ao receber dados "
                "do jogador %d.\n",
                args->playerId + 1
            );

            break;
        }


        /*
         * ====================================================
         * GARANTIR IDENTIDADE DO JOGADOR
         * ====================================================
         *
         * O cliente não deve decidir qual jogador ele é.
         *
         * Portanto sobrescrevemos o campo com a informação
         * conhecida pelo servidor.
         */

        request.jogadorId = args->playerId;


        /*
         * ====================================================
         * COLOCAR NA REQUEST QUEUE
         * ====================================================
         */

        if (requestQueuePush(
            &args->server->requestQueue,
            request
        ) != 0) {

            printf(
                "[LISTENER] Erro ao adicionar "
                "request na fila.\n"
            );

            break;
        }


        /*
         * Apenas para visualização durante os testes.
         *
         * Posteriormente isso poderá ser removido.
         */

        printf(
            "[LISTENER] Request recebida do jogador %d "
            "(acao=%d, carta=%d).\n",
            request.jogadorId + 1,
            request.acao,
            request.cartaId
        );
    }


    /*
     * ========================================================
     * ENCERRAMENTO
     * ========================================================
     */

    closesocket(args->socket);

    args->server->playerSockets[
        args->playerId
    ] = INVALID_SOCKET;


    free(args);

    return 0;
}