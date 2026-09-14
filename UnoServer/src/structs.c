#define MAX_QTD_CARTAS_JOGADOR 16
#define MAX_NOME_JOGADOR 24
#define MAX_SIMBOLO_CARTA 4

typedef enum {
    AMARELO,
    VERMELHO,
    VERDE,
    AZUL,
    PRETO

} Cor;

typedef struct {
    int id;
    char simbolo[MAX_SIMBOLO_CARTA]; // apenas numeros de 0 - 9
    Cor  cor;
} Carta;

typedef struct {
    int id;
    char nome[MAX_NOME_JOGADOR];
    Carta cartas[MAX_QTD_CARTAS_JOGADOR];
    int idsCartasPossiveis[MAX_QTD_CARTAS_JOGADOR]; // se vier vazio pode comprar
} Jogador;

typedef struct {
    int numeroRodada;
    char suaVez;
    int tempoMs;
    Carta ultimaCarta;
    int numeroCartasAdversario;
    char nomeAdversario[MAX_NOME_JOGADOR];
} Partida;
