#include "../include/worker.h"
#include "../include/server.h"
#include "../include/game.h"

#include <stdio.h>
#include <string.h>


static void prepararEstadoInicial(Jogo *jogo) {
    game_init(jogo);

    /*
     * Por enquanto os nomes são definidos
     * pelo servidor.
     *
     * Posteriormente podemos receber os nomes
     * através de uma requisição.
     */
    game_adicionarJogador(jogo,0,"Jogador 1");

    game_adicionarJogador(jogo,1,"Jogador 2");

    /*
     * Cada jogador recebe 7 cartas.
     */
    game_distribuirCartas(jogo);

    /*
     * A primeira carta do jogo é retirada
     * do topo do baralho.
     */
    jogo->ultimaCarta = jogo->baralho[jogo->topoBaralho - 1];

    jogo->topoBaralho--;

    /*
     * Jogador 1 começa.
     */
    jogo->jogadorDaVez = 0;

    printf("[GAME] Partida inicializada.\n");

    printf("[GAME] Jogador da vez: %d\n", jogo->jogadorDaVez + 1);

    printf("[GAME] Primeira carta: %s, cor=%d\n", jogo->ultimaCarta.simbolo, jogo->ultimaCarta.cor);
}


static void prepararResposta(Jogo *jogo, int jogadorId, Mensagem *response) {
    memset(
        response,
        0,
        sizeof(Mensagem)
    );

    response->tipo = MSG_ESTADO_JOGO;

    game_obterEstado(jogo,jogadorId,&response->estado);
}


static void prepararJogadaInvalida(Jogo *jogo, int jogadorId, Mensagem *response) {
    memset(
        response,
        0,
        sizeof(Mensagem)
    );

    response->tipo = MSG_JOGADA_INVALIDA;

    game_obterEstado(jogo, jogadorId, &response->estado);
}


static void enviarPartidaIniciada(Server *server, Jogo *jogo) {
    for (int jogadorId = 0; jogadorId < MAX_PLAYERS; jogadorId++) {

        Mensagem response;

        memset(
            &response,
            0,
            sizeof(Mensagem)
        );

        response.tipo = MSG_PARTIDA_INICIADA;

        game_obterEstado(jogo, jogadorId, &response.estado);

        if (responseQueuePush(&server->responseQueue, response) != 0) {
            printf("[WORKER] Erro ao enviar MSG_PARTIDA_INICIADA para jogador %d.\n", jogadorId + 1);

            return;
        }

        printf("[WORKER] MSG_PARTIDA_INICIADA adicionada para jogador %d.\n", jogadorId + 1);
    }
}


static void enviarEstadoParaTodos(Server *server,Jogo *jogo) {
    for (int jogadorId = 0; jogadorId < MAX_PLAYERS; jogadorId++) {

        Mensagem response;

        memset(
            &response,
            0,
            sizeof(Mensagem)
        );

        response.tipo = MSG_ESTADO_JOGO;

        game_obterEstado(jogo, jogadorId, &response.estado);

        if (responseQueuePush(&server->responseQueue, response) != 0) {

            printf("[WORKER] Erro ao enviar estado para jogador %d.\n", jogadorId + 1);

            return;
        }

        printf("[WORKER] Estado do jogo enviado para jogador %d.\n", jogadorId + 1);
    }
}


DWORD WINAPI workerThread(LPVOID arg) {
    Server *server = (Server *) arg;

    if (server == NULL) {
        return 1;
    }

    printf("[WORKER] Thread iniciada.\n");

    printf("[WORKER] Aguardando inicio da partida...\n");

    DWORD resultadoEvento = WaitForSingleObject(
        server->gameStartedEvent,
        INFINITE
    );

    if (resultadoEvento != WAIT_OBJECT_0 || !server->running) {
        printf("[WORKER] Partida nao iniciada.\n");

        return 0;
    }

    /*
     * O jogo será criado quando os dois
      * jogadores estiverem conectados
     */

    Jogo jogo;
    prepararEstadoInicial(&jogo);
    enviarPartidaIniciada(server, &jogo);

    printf("[WORKER] Partida iniciada.\n");

    while (server->running) {
        Solicitacao request;

        /*
         * Espera uma requisição da fila.
         */
        if (requestQueuePop(&server->requestQueue, &request) != 0) {
            break;
        }

        printf(
            "[WORKER] Processando request: jogador=%d, acao=%d, carta=%d\n",
            request.jogadorId + 1,
            request.acao,
            request.cartaId
        );

        Mensagem response;


        /*
         * =================================
         * JOGAR CARTA
         * =================================
         */
        if (request.acao == ACAO_JOGAR_CARTA) {

            printf("[WORKER] Jogador %d tentando jogar carta %d.\n", request.jogadorId + 1, request.cartaId);

            int resultado = game_jogarCarta(&jogo, request.jogadorId, request.cartaId);

            if (resultado != 0) {

                printf("[WORKER] Jogada invalida.\n");

                prepararJogadaInvalida(&jogo, request.jogadorId, &response);

                if (responseQueuePush(&server->responseQueue, response) != 0) {
                    break;
                }

            } else {

                printf("[WORKER] Jogada valida.\n");

                enviarEstadoParaTodos(server,&jogo);
            }
        }


        /*
         * =================================
         * COMPRAR CARTA
         * =================================
         */
        else if (request.acao == ACAO_COMPRAR_CARTA) {

            printf("[WORKER] Jogador %d comprando carta.\n", request.jogadorId + 1);

            int resultado = game_comprarCarta(&jogo, request.jogadorId);

            if (resultado != 0) {

                printf("[WORKER] Nao foi possivel comprar carta.\n");

                prepararJogadaInvalida(&jogo, request.jogadorId, &response);

                if (responseQueuePush(&server->responseQueue, response) != 0) {
                    break;
                }
            }
            else {
                printf("[WORKER] Carta comprada.\n");

                enviarEstadoParaTodos(server,&jogo);
            }
        }


        /*
         * =================================
         * DIZER UNO e OUTRAS
         * =================================
         *
         * Ainda não existe essa regra
         * na nossa implementação.
         */
        else {
            printf("[WORKER] Acao nao suportada: %d\n", request.acao);

            prepararJogadaInvalida(&jogo, request.jogadorId, &response);
        }


        /*
         * Coloca a resposta na fila
         * para o Writer enviar.
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
