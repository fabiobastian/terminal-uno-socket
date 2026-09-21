#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>

#define MAX_QTD_CARTAS_JOGADOR 16
#define MAX_NOME_JOGADOR 24
#define MAX_SIMBOLO_CARTA 8

typedef enum {
    AMARELO,
    VERMELHO,
    VERDE,
    AZUL,
    PRETO
} Cor;


typedef struct {
    int id;
    char simbolo[MAX_SIMBOLO_CARTA];
    Cor cor;
} Carta;


typedef struct {
    int id;
    char nome[MAX_NOME_JOGADOR];

    int qtdCartas;
    Carta cartas[MAX_QTD_CARTAS_JOGADOR];

    int qtdCartasPossiveis;
    int idsCartasPossiveis[MAX_QTD_CARTAS_JOGADOR];

} Jogador;


typedef struct {
    int numeroRodada;
    bool suaVez;
    int tempoMs;

    Carta ultimaCarta;

    int numeroCartasAdversario;
    char nomeAdversario[MAX_NOME_JOGADOR];

} Partida;


/*
 * Estado enviado pelo servidor para um cliente.
 */
typedef struct {
    Jogador jogador;
    Partida partida;
} EstadoJogo;


/*
 * ============================================================
 * MENSAGENS SERVIDOR -> CLIENTE
 * ============================================================
 */

typedef enum {
    MSG_PARTIDA_INICIADA,
    MSG_ESTADO_JOGO,
    MSG_JOGADA_INVALIDA,
    MSG_PARTIDA_FINALIZADA,
    MSG_JOGADOR_SAIU
} TipoMensagem;


typedef struct {
    TipoMensagem tipo;
    EstadoJogo estado;
} Mensagem;


/*
 * ============================================================
 * AÇÕES CLIENTE -> SERVIDOR
 * ============================================================
 */

typedef enum {
    ACAO_JOGAR_CARTA,
    ACAO_COMPRAR_CARTA,
    ACAO_DIZER_UNO
} TipoAcao;


typedef struct {
    int jogadorId;
    TipoAcao acao;
    int cartaId;
} Solicitacao;

#endif