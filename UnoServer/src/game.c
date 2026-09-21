#include "../include/game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


void game_init(Jogo *jogo){
    if (jogo == NULL) {
        return;
    }

    memset(jogo, 0, sizeof(Jogo));

    jogo->topoBaralho = 0;
    jogo->jogadorDaVez = 0;
    jogo->partidaFinalizada = 0;

    game_criarBaralho(jogo);
    game_embaralhar(jogo);
}


void game_adicionarJogador(Jogo *jogo, int jogadorId, const char *nome) {
    if (jogo == NULL) {
        return;
    }

    if (jogadorId < 0 || jogadorId >= MAX_PLAYERS) {
        return;
    }

    Jogador *jogador = &jogo->jogadores[jogadorId];

    memset(jogador, 0, sizeof(Jogador));

    jogador->id = jogadorId;

    if (nome != NULL) {
        strncpy(
            jogador->nome,
            nome,
            MAX_NOME_JOGADOR - 1
        );

        jogador->nome[MAX_NOME_JOGADOR - 1] = '\0';
    }
}


void game_criarBaralho(Jogo *jogo) {
    if (jogo == NULL) {
        return;
    }

    int id = 0;

    Cor cores[] = {
        AMARELO,
        VERMELHO,
        VERDE,
        AZUL
    };

    jogo->topoBaralho = 0;

    for (int cor = 0; cor < 4; cor++) {

        for (int numero = 0; numero <= 9; numero++) {

            Carta *carta = &jogo->baralho[jogo->topoBaralho];

            carta->id = id++;
            carta->cor = cores[cor];

            snprintf(
                carta->simbolo,
                MAX_SIMBOLO_CARTA,
                "%d",
                numero
            );

            jogo->topoBaralho++;
        }
    }
}


void game_embaralhar(Jogo *jogo) {
    if (jogo == NULL) {
        return;
    }

    srand((unsigned int)time(NULL));

    for (int i = MAX_CARTAS_BARALHO - 1; i > 0; i--) {

        int j = rand() % (i + 1);

        Carta temp = jogo->baralho[i];

        jogo->baralho[i] = jogo->baralho[j];

        jogo->baralho[j] = temp;
    }
}


void game_distribuirCartas(Jogo *jogo) {
    if (jogo == NULL) {
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {

        jogo->jogadores[i].qtdCartas = 0;
    }

    for (int rodada = 0; rodada < CARTAS_INICIAIS; rodada++) {

        for (int jogadorId = 0; jogadorId < MAX_PLAYERS; jogadorId++) {

            if (jogo->topoBaralho <= 0) {
                return;
            }

            Carta carta = jogo->baralho[jogo->topoBaralho - 1];

            jogo->topoBaralho--;

            Jogador *jogador = &jogo->jogadores[jogadorId];

            jogador->cartas[jogador->qtdCartas] = carta;

            jogador->qtdCartas++;
        }
    }
}


int game_comprarCarta(Jogo *jogo, int jogadorId) {
    if (jogo == NULL) {
        printf("[GAME] jogo == NULL\n");
        return -1;
    }

    if (jogadorId < 0 || jogadorId >= MAX_PLAYERS) {
        printf("[GAME] jogadorId invalido: %d\n", jogadorId);
        return -1;
    }

    printf("[GAME] Jogador %d tentando comprar.\n",jogadorId + 1);
    printf("[GAME] topoBaralho antes: %d\n",jogo->topoBaralho);

    if (jogo->topoBaralho <= 0) {
        printf("[GAME] Baralho vazio.\n");
        return -1;
    }

    Jogador *jogador = &jogo->jogadores[jogadorId];

    printf("[GAME] Quantidade de cartas: %d\n", jogador->qtdCartas);

    if (jogador->qtdCartas >= MAX_QTD_CARTAS_JOGADOR) {
        printf("[GAME] Jogador atingiu limite de cartas.\n");
        return -1;
    }

    Carta carta = jogo->baralho[jogo->topoBaralho - 1];

    jogo->topoBaralho--;

    jogador->cartas[jogador->qtdCartas] = carta;

    jogador->qtdCartas++;

    printf("[GAME] Carta comprada: ID=%d, cor=%d, simbolo=%s\n",
        carta.id,
        carta.cor,
        carta.simbolo
    );

    printf("[GAME] Nova quantidade de cartas: %d\n",jogador->qtdCartas);

    game_proximoJogador(jogo);

    printf("[GAME] Proximo jogador: %d\n",jogo->jogadorDaVez + 1);

    return 0;
}


int game_podeJogarCarta(Jogo *jogo, int jogadorId, int cartaId) {
    if (jogo == NULL) {
        return 0;
    }

    if (jogadorId < 0 || jogadorId >= MAX_PLAYERS) {
        return 0;
    }

    if (jogo->jogadorDaVez != jogadorId) {
        return 0;
    }

    Jogador *jogador = &jogo->jogadores[jogadorId];

    /*
     * Procura a carta na mão do jogador.
     */
    for (int i = 0; i < jogador->qtdCartas; i++) {
        Carta carta = jogador->cartas[i];

        if (carta.id != cartaId) {
            continue;
        }

        /*
         * Pode jogar se a cor for igual
         * ou se o número for igual.
         */
        if (carta.cor == jogo->ultimaCarta.cor) {
            return 1;
        }

        if (strcmp(carta.simbolo, jogo->ultimaCarta.simbolo) == 0) {
            return 1;
        }

        return 0;
    }

    /*
     * Carta não encontrada na mão.
     */
    return 0;
}


int game_jogarCarta(Jogo *jogo, int jogadorId, int cartaId) {
    if (!game_podeJogarCarta(jogo, jogadorId, cartaId)) {
        return -1;
    }

    Jogador *jogador = &jogo->jogadores[jogadorId];

    int indiceCarta = -1;

    /*
     * Encontra a posição da carta
     * dentro da mão.
     */
    for (int i = 0; i < jogador->qtdCartas; i++) {
        if (jogador->cartas[i].id == cartaId) {
            indiceCarta = i;
            break;
        }
    }

    if (indiceCarta == -1) {
        return -1;
    }

    /*
     * A carta jogada passa a ser
     * a última carta da partida.
     */
    jogo->ultimaCarta = jogador->cartas[indiceCarta];

    /*
     * Remove a carta da mão.
     *
     * Deslocamos as cartas seguintes
     * uma posição para a esquerda.
     */
    for (int i = indiceCarta; i < jogador->qtdCartas - 1; i++) {
        jogador->cartas[i] = jogador->cartas[i + 1];
    }

    jogador->qtdCartas--;

    /*
     * Verifica se o jogador venceu.
     */
    if (game_jogadorVenceu(jogo, jogadorId)) {
        jogo->partidaFinalizada = 1;

        printf("[GAME] Jogador %d venceu!\n", jogadorId + 1);

        return 0;
    }

    /*
     * Passa o turno.
     */
    game_proximoJogador(jogo);

    return 0;
}


void game_proximoJogador(Jogo *jogo) {
    if (jogo == NULL) {
        return;
    }

    if (jogo->partidaFinalizada) {
        return;
    }

    jogo->jogadorDaVez = (jogo->jogadorDaVez + 1) % MAX_PLAYERS;
}


int game_jogadorVenceu(Jogo *jogo, int jogadorId) {
    if (jogo == NULL) {
        return 0;
    }

    if (jogadorId < 0 || jogadorId >= MAX_PLAYERS) {
        return 0;
    }

    return jogo->jogadores[jogadorId].qtdCartas == 0;
}


int game_obterCartasPossiveis(Jogo *jogo, int jogadorId) {
    if (jogo == NULL) {
        return -1;
    }

    if (jogadorId < 0 || jogadorId >= MAX_PLAYERS) {
        return -1;
    }

    Jogador *jogador = &jogo->jogadores[jogadorId];

    jogador->qtdCartasPossiveis = 0;

    for (int i = 0; i < jogador->qtdCartas; i++) {
        Carta carta = jogador->cartas[i];

        if (carta.cor == jogo->ultimaCarta.cor || strcmp(carta.simbolo, jogo->ultimaCarta.simbolo) == 0) {
            jogador->idsCartasPossiveis[jogador->qtdCartasPossiveis] = carta.id;

            jogador->qtdCartasPossiveis++;
        }
    }

    return 0;
}


void game_obterEstado(Jogo *jogo, int jogadorId, EstadoJogo *estado) {
    if (jogo == NULL || estado == NULL) {
        return;
    }

    if (jogadorId < 0 || jogadorId >= MAX_PLAYERS) {
        return;
    }

    memset(
        estado,
        0,
        sizeof(EstadoJogo)
    );

    Jogador *jogador = &jogo->jogadores[jogadorId];

    /*
     * Informações do próprio jogador.
     */
    game_obterCartasPossiveis(jogo, jogadorId);

    estado->jogador = *jogador;

    /*
     * Informações gerais da partida.
     */
    estado->partida.numeroRodada = 1;

    estado->partida.suaVez = jogo->jogadorDaVez == jogadorId;

    estado->partida.ultimaCarta = jogo->ultimaCarta;


    /*
     * Informações do adversário.
     */
    int adversarioId = (jogadorId + 1) % MAX_PLAYERS;

    estado->partida.numeroCartasAdversario = jogo->jogadores[adversarioId].qtdCartas;

    strncpy(estado->partida.nomeAdversario, jogo->jogadores[adversarioId].nome, MAX_NOME_JOGADOR - 1);

    estado->partida.nomeAdversario[MAX_NOME_JOGADOR - 1] = '\0';
}