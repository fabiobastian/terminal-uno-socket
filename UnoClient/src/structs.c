typedef enum {
    AMARELO,
    VERMELHO,
    VERDE,
    AZUL
} Cor;

// apenas numeros de 0 - 9
typedef struct {
    uint id;
    char *simbolo;
    Cor  cor;
} Carta;

typedef struct {
    int id;
    char *nome;
    Carta[] cartas;
    int[] idsCartasPossiveis; // se vier vazio pode comprar
} Jogador;

typedef struct {
    int numeroRodada;
    char suaVez;
    int tempoMs;
    Carta ultimaCarta;
    int numeroCartasAdversario;
    char *nomeAdversario;
} Partida;
