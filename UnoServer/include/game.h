#ifndef GAME_H
#define GAME_H

#include "protocol.h"

#define MAX_PLAYERS 2
#define MAX_CARTAS_BARALHO 40
#define CARTAS_INICIAIS 7

typedef struct {
    Jogador jogadores[MAX_PLAYERS];

    Carta baralho[MAX_CARTAS_BARALHO];
    int topoBaralho;

    Carta ultimaCarta;

    int jogadorDaVez;
    int numeroRodada;

    int partidaFinalizada;

} Jogo;


/*
 * Inicializa o estado da partida.
 */
void game_init(Jogo *jogo);


/*
 * Adiciona um jogador à partida.
 */
void game_adicionarJogador(
    Jogo *jogo,
    int jogadorId,
    const char *nome
);


/*
 * Cria as 40 cartas do jogo:
 *
 * 4 cores × 10 números
 */
void game_criarBaralho(
    Jogo *jogo
);


/*
 * Embaralha as cartas do baralho.
 */
void game_embaralhar(
    Jogo *jogo
);


/*
 * Distribui 7 cartas para cada jogador.
 */
void game_distribuirCartas(
    Jogo *jogo
);


/*
 * Compra uma carta do baralho para o jogador.
 *
 * Retorna:
 *  0  -> sucesso
 * -1  -> erro
 */
int game_comprarCarta(
    Jogo *jogo,
    int jogadorId
);


/*
 * Verifica se uma carta pode ser jogada.
 *
 * Retorna:
 *  1 -> pode jogar
 *  0 -> não pode jogar
 */
int game_podeJogarCarta(
    Jogo *jogo,
    int jogadorId,
    int cartaId
);


/*
 * Executa a jogada de uma carta.
 *
 * Retorna:
 *  0  -> sucesso
 * -1  -> jogada inválida
 */
int game_jogarCarta(
    Jogo *jogo,
    int jogadorId,
    int cartaId
);


/*
 * Passa o turno para o próximo jogador.
 */
void game_proximoJogador(
    Jogo *jogo
);


/*
 * Verifica se o jogador venceu.
 *
 * Retorna:
 *  1 -> venceu
 *  0 -> não venceu
 */
int game_jogadorVenceu(
    Jogo *jogo,
    int jogadorId
);

/*
 * Monta o array idsCartasPossiveis para enviar ao front
 * quais cartas será possivel jogar.
 */
int game_obterCartasPossiveis(
    Jogo *jogo,
    int jogadorId
);

/*
 * Monta o estado do jogo que será
 * enviado para um determinado jogador.
 */
void game_obterEstado(
    Jogo *jogo,
    int jogadorId,
    EstadoJogo *estado
);

#endif