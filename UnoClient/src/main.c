/**
 * @file:       client.c
 * @author:     Nathan Berger
 * @date:       2026-09-20
 * @version     2.1
 * @brief       Client responsible for rendering, network communication and
 *              input handling for the terminal UNO game. Windows only.
 *
 * MIT LICENSE
 *
 * Copyright (c) 2026 Fábio Júnior Nielsson Bastian.
 * Unauthorized copying or use of this file is prohibited.
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <conio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "../include/protocol.h"

#pragma comment(lib, "ws2_32.lib")


/**
 * ============================================================================
 * Constants
 * ============================================================================
 */
#define MIN_SCREEN_HEIGHT 50
#define VERTICAL_MARGIN   10
#define MIN_SCREEN_WIDTH  150
#define SIDE_WIDTH        15
#define TOP_HEIGHT        10
#define BOTTOM_HEIGHT     10

#define COLOR_DEFAULT     0
#define COLOR_BLACK       30
#define COLOR_RED         31
#define COLOR_GREEN       32
#define COLOR_YELLOW      33
#define COLOR_BLUE        34
#define COLOR_MAGENTA     35
#define COLOR_CYAN        36
#define COLOR_WHITE       37
#define CARD_WIDTH  5
#define CARD_HEIGHT 5

#define DEFAULT_SERVER_IP   "127.0.0.1"
#define DEFAULT_SERVER_PORT 8080

#define STATUS_MSG_DURATION_MS 2500

/* Tamanho inicial do buffer de saida usado por renderScreen() (cresce
 * automaticamente via realloc se precisar). */
#define RENDER_BUF_INITIAL_CAPACITY 65536

/* Buffer de stdio maior, para reduzir o numero de escritas reais no console. */
#define STDOUT_BUFFER_SIZE 65536

/* Log de diagnostico: grava em client_debug.log toda mensagem recebida
 * do servidor, para confirmar se o problema de "interface nao atualiza"
 * e o client deixando de renderizar algo que chegou, ou o servidor
 * simplesmente nao mandando a mensagem. Nao afeta a UI. */
#define DEBUG_LOG_FILE "client_debug.log"


/**
 * ============================================================================
 * Structs
 * ============================================================================
 */
typedef struct {
    char character[5];
    int foreground;
    int background;
} Cell;


typedef struct {
    int width;
    int height;
    Cell *buffer;
} Screen;


typedef struct {
    int x;
    int y;
    int width;
    int height;
} UIRectangle;


typedef enum {
    ZONE_TOP,
    ZONE_RIGHT,
    ZONE_LEFT,
    ZONE_BOTTOM,
    ZONE_CENTER
} ZoneType;


typedef struct {
    UIRectangle bounds;
    ZoneType zone;
} Zone;


typedef struct {
    Zone top;
    Zone right;
    Zone bottom;
    Zone left;
    Zone center;
} BoardLayout;


/* Estado apenas de UI da mao do jogador local (nao faz parte do protocolo) */
typedef struct {
    Zone zone;
    int selectedCard;
} HandUI;


typedef enum {
    INPUT_NONE,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_ENTER,
    INPUT_ESCAPE
} Input;


/* Buffer de saida que cresce dinamicamente. Usado para montar o frame
 * inteiro na memoria antes de escrever no console de uma unica vez -
 * ver comentario em renderScreen(). */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;


/**
 * ============================================================================
 * Estado compartilhado entre a thread de rede e a thread principal (UI)
 * ============================================================================
 */
typedef struct {
    CRITICAL_SECTION lock;

    EstadoJogo estado;
    bool hasEstado;

    bool connectionLost;
    bool partidaFinalizada;
    bool adversarioSaiu;

    char statusMsg[128];
    DWORD statusMsgExpiry;
} SharedState;

static SharedState g_shared;
static CRITICAL_SECTION g_logLock;


/**
 * ============================================================================
 * Log de diagnostico (client_debug.log)
 * ============================================================================
 */
void debugLog(const char *fmt, ...) {
    EnterCriticalSection(&g_logLock);

    FILE *f = fopen(DEBUG_LOG_FILE, "a");
    if (f != NULL) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);

        fprintf(f, "\n");
        fclose(f);
    }

    LeaveCriticalSection(&g_logLock);
}


const char *tipoMensagemParaTexto(TipoMensagem tipo) {
    switch (tipo) {
        case MSG_PARTIDA_INICIADA:   return "MSG_PARTIDA_INICIADA";
        case MSG_ESTADO_JOGO:        return "MSG_ESTADO_JOGO";
        case MSG_JOGADA_INVALIDA:    return "MSG_JOGADA_INVALIDA";
        case MSG_PARTIDA_FINALIZADA: return "MSG_PARTIDA_FINALIZADA";
        case MSG_JOGADOR_SAIU:       return "MSG_JOGADOR_SAIU";
        default:                     return "DESCONHECIDO";
    }
}


/**
 * ============================================================================
 * Console: habilitar sequencias ANSI / VT100
 * ============================================================================
 */
void enableAnsiConsole(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        /*
         * ENABLE_VIRTUAL_TERMINAL_PROCESSING: habilita as sequencias
         * ANSI (\033[...) que usamos para cor/cursor.
         *
         * ENABLE_WRAP_AT_EOL_OUTPUT e REMOVIDO de proposito: por padrao
         * o console do Windows, ao escrever um caractere na ULTIMA
         * coluna da tela (ex.: o canto do tabuleiro), faz o cursor dar
         * a volta e ISSO FORCA O BUFFER A ROLAR uma linha. Como
         * "\033[H" (cursor home) e relativo ao buffer inteiro - e nao a
         * janela visivel - depois dessa rolagem o proximo frame passa a
         * ser desenhado uma linha ACIMA do que esta visivel, e a tela
         * parece "congelar"/"sumir" mostrando sempre o ultimo frame que
         * ainda estava na area visivel. Desligar esse modo evita que
         * escrever no canto da tela dispare essa rolagem indesejada.
         */
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
        SetConsoleMode(hOut, mode);
    }
}


/**
 * ============================================================================
 * Input (nao bloqueante)
 * ============================================================================
 */
Input readInput() {
    if (!_kbhit()) {
        return INPUT_NONE;
    }

    int c = _getch();

    if (c == 0 || c == 224) {
        c = _getch();

        switch (c) {
            case 72: return INPUT_UP;
            case 80: return INPUT_DOWN;
            case 75: return INPUT_LEFT;
            case 77: return INPUT_RIGHT;
        }

        return INPUT_NONE;
    }

    if (c == 13)
        return INPUT_ENTER;

    if (c == 27)
        return INPUT_ESCAPE;

    return INPUT_NONE;
}


Screen getScreenSize() {
    Screen s = {0};

    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (console == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(console, &csbi)) {
        return s;
    }

    s.width =
            csbi.srWindow.Right -
            csbi.srWindow.Left + 1;

    s.height =
            csbi.srWindow.Bottom -
            csbi.srWindow.Top + 1 -
            VERTICAL_MARGIN;

    if (s.height <= 0) {
        s.width = 0;
        s.height = 0;
        return s;
    }

    s.buffer = malloc(
        s.width * s.height * sizeof(Cell)
    );

    if (s.buffer == NULL) {
        s.width = 0;
        s.height = 0;
    }

    return s;
}


void setPixel(Screen *screen, int x, int y, const char *character) {
    if (x < 0 || x >= screen->width || y < 0 || y >= screen->height) {
        return;
    }
    int idx = y * screen->width + x;
    strncpy(screen->buffer[idx].character, character, sizeof(screen->buffer[idx].character) - 1);
    screen->buffer[idx].character[sizeof(screen->buffer[idx].character) - 1] = '\0';
}


void setPixelChar(Screen *screen, int x, int y, char c) {
    char buf[2] = {c, '\0'};
    setPixel(screen, x, y, buf);
}


void setForeground(Screen *screen, int x, int y, int color) {
    if (x < 0 || x >= screen->width || y < 0 || y >= screen->height) {
        return;
    }
    screen->buffer[y * screen->width + x].foreground = color;
}


void setBackground(Screen *screen, int x, int y, int color) {
    if (x < 0 || x >= screen->width || y < 0 || y >= screen->height) {
        return;
    }
    screen->buffer[y * screen->width + x].background = color;
}


/* Escreve uma string ASCII horizontal, uma celula por caractere. */
void drawText(Screen *screen, int x, int y, const char *text, int fg, int bg) {
    for (int i = 0; text[i] != '\0'; ++i) {
        setPixelChar(screen, x + i, y, text[i]);
        setForeground(screen, x + i, y, fg);
        setBackground(screen, x + i, y, bg);
    }
}


/*
 * Reseta apenas o BUFFER INTERNO (matriz de Cell) para espacos em branco.
 *
 * IMPORTANTE: esta funcao NAO escreve mais nada no terminal (antes ela
 * mandava "\033[H\033[2J" a cada frame, o que limpava a tela fisica e
 * deixava tudo em branco por um instante ate renderScreen() redesenhar
 * celula por celula - era exatamente o efeito de "desenhando aos poucos"
 * que estava sendo visto). Como renderScreen() sempre reescreve a tela
 * inteira (todas as linhas/colunas), nao ha necessidade de limpar o
 * terminal a cada frame: basta reposicionar o cursor no topo.
 */
void clearScreen(Screen *screen) {
    for (int y = 0; y < screen->height; y++) {
        for (int x = 0; x < screen->width; x++) {
            int idx = y * screen->width + x;
            strcpy(screen->buffer[idx].character, " ");
            screen->buffer[idx].foreground = COLOR_DEFAULT;
            screen->buffer[idx].background = COLOR_DEFAULT;
        }
    }
}


void drawBoardBorder(Screen *screen) {
    int width = screen->width;
    int height = screen->height;

    for (int x = 0; x < width; x++) {
        setPixel(screen, x, 0, "═");
    }

    for (int x = 0; x < width; x++) {
        setPixel(screen, x, height - 1, "═");
    }

    for (int y = 0; y < height; y++) {
        if (y == 0) { setPixel(screen, 0, y, "╔"); } else if (y == height - 1) { setPixel(screen, 0, y, "╚"); } else {
            setPixel(screen, 0, y, "║");
        }
    }

    for (int y = 0; y < height; y++) {
        if (y == 0) {
            setPixel(screen, width - 1, y, "╗");
        } else if (y == height - 1) {
            setPixel(screen, width - 1, y, "╝");
        } else {
            setPixel(screen, width - 1, y, "║");
        }
    }
}


/**
 * ============================================================================
 * StrBuf: buffer de saida que cresce dinamicamente
 * ============================================================================
 * Em vez de chamar printf() uma vez por celula (o que gerava milhares de
 * escritas pequenas por frame e fazia o console "desenhar aos poucos"),
 * acumulamos o frame inteiro aqui e escrevemos tudo de uma vez com
 * fwrite() no final de renderScreen(). Isso faz o frame aparecer
 * instantaneamente, como uma unica atualizacao.
 */
void sbInit(StrBuf *sb) {
    sb->cap = RENDER_BUF_INITIAL_CAPACITY;
    sb->data = malloc(sb->cap);
    sb->len = 0;
    if (sb->data != NULL) {
        sb->data[0] = '\0';
    }
}


void sbAppend(StrBuf *sb, const char *s, size_t n) {
    if (sb->data == NULL || n == 0) {
        return;
    }

    if (sb->len + n + 1 > sb->cap) {
        size_t newCap = sb->cap;
        while (sb->len + n + 1 > newCap) {
            newCap *= 2;
        }

        char *tmp = realloc(sb->data, newCap);
        if (tmp == NULL) {
            /* Sem memoria: descarta o restante do frame em vez de
             * arriscar um crash. O proximo frame tenta de novo. */
            return;
        }

        sb->data = tmp;
        sb->cap = newCap;
    }

    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}


void sbAppendStr(StrBuf *sb, const char *s) {
    sbAppend(sb, s, strlen(s));
}


void sbFree(StrBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}


/*
 * Desenha o frame inteiro em um StrBuf e escreve tudo de uma vez no
 * console com um unico fwrite()+fflush(). Antes, cada celula (e cada
 * troca de cor) disparava um printf() separado - centenas/milhares de
 * escritas por frame - e era isso que fazia parecer que a tela estava
 * sendo "digitada" linha a linha em vez de aparecer instantaneamente.
 */
void renderScreen(Screen *screen) {
    StrBuf sb;
    sbInit(&sb);

    /* Move o cursor para o topo (NAO limpa a tela - ver clearScreen). */
    sbAppendStr(&sb, "\033[H");

    int lastFg = -1;
    int lastBg = -1;

    for (int y = 0; y < screen->height; y++) {
        for (int x = 0; x < screen->width; x++) {
            int idx = y * screen->width + x;
            Cell cell = screen->buffer[idx];

            if (cell.foreground != lastFg || cell.background != lastBg) {
                if (cell.foreground == COLOR_DEFAULT && cell.background == COLOR_DEFAULT) {
                    sbAppendStr(&sb, "\033[0m");
                } else {
                    char seq[32];
                    int fg = (cell.foreground == COLOR_DEFAULT) ? COLOR_WHITE : cell.foreground;
                    int bg = (cell.background == COLOR_DEFAULT) ? 49 : cell.background + 10;
                    int n = snprintf(seq, sizeof(seq), "\033[%d;%dm", fg, bg);
                    if (n > 0) {
                        sbAppend(&sb, seq, (size_t) n);
                    }
                }
                lastFg = cell.foreground;
                lastBg = cell.background;
            }

            sbAppendStr(&sb, cell.character);
        }

        sbAppendStr(&sb, "\033[0m");

        /* So pula pra proxima linha se NAO formos a ultima linha do frame.
         * Escrever "\n" depois da ultima linha empurraria o cursor para
         * uma linha que nao existe na tela, o que forca o console a
         * rolar o buffer - exatamente o que causa a tela "sumir"/
         * congelar apos o primeiro frame (ver comentario em
         * enableAnsiConsole()). Como o proximo frame comeca com
         * "\033[H" (volta pro topo), nao precisamos avancar o cursor
         * aqui mesmo. */
        if (y < screen->height - 1) {
            sbAppendStr(&sb, "\n");
        }

        lastFg = -1;
        lastBg = -1;
    }

    /* Limpa do cursor ate o fim da tela, para o caso do terminal ter
     * mostrado algo maior num frame anterior (ex.: redimensionamento). */
    sbAppendStr(&sb, "\033[0J");

    if (sb.data != NULL) {
        fwrite(sb.data, 1, sb.len, stdout);
        fflush(stdout);
    }

    sbFree(&sb);
}


void drawZoneBorder(Screen *screen, Zone layout) {
    int x = layout.bounds.x;
    int y = layout.bounds.y;
    int width = layout.bounds.width;
    int height = layout.bounds.height;

    switch (layout.zone) {
        case ZONE_TOP:
            for (int px = x; px < x + width - 1; px++) {
                setPixel(screen, px, height, "╼");
            }
            break;
        case ZONE_BOTTOM:
            for (int px = x; px < x + width; px++) {
                setPixel(screen, px, y, "╼");
            }
            break;
        case ZONE_CENTER: {
            const int paddingY = 5;
            const int paddingX = 20;

            int left = x + paddingX;
            int right = x + width - paddingX;
            int top = y + paddingY;
            int bottom = y + height - paddingY;

            for (int px = left; px <= right; ++px) {
                setPixel(screen, px, top, "═");
            }

            for (int px = left; px <= right; ++px) {
                setPixel(screen, px, bottom, "═");
            }

            for (int py = top; py <= bottom; ++py) {
                if (py == top) {
                    setPixel(screen, left, py, "╔");
                } else if (py == bottom) {
                    setPixel(screen, left, py, "╚");
                } else {
                    setPixel(screen, left, py, "║");
                }
            }

            for (int py = top; py <= bottom; ++py) {
                if (py == top) {
                    setPixel(screen, right, py, "╗");
                } else if (py == bottom) {
                    setPixel(screen, right, py, "╝");
                } else {
                    setPixel(screen, right, py, "║");
                }
            }
            break;
        }
        default:
            break;
    }
}


void drawBoard(Screen *screen, BoardLayout layout) {
    drawZoneBorder(screen, layout.top);
    drawZoneBorder(screen, layout.bottom);
    drawZoneBorder(screen, layout.center);
}


BoardLayout createBoardLayout(Screen screen) {
    BoardLayout layout;

    int centerWidth = screen.width - (2 * SIDE_WIDTH);
    int centerHeight = screen.height - TOP_HEIGHT - BOTTOM_HEIGHT;

    layout.top = (Zone){
        .zone = ZONE_TOP,
        .bounds = {
            .x = 1,
            .y = 1,
            .width = screen.width - 1,
            .height = TOP_HEIGHT
        }
    };

    layout.bottom = (Zone){
        .zone = ZONE_BOTTOM,
        .bounds = {
            .x = 1,
            .y = screen.height - BOTTOM_HEIGHT,
            .width = screen.width - 2,
            .height = BOTTOM_HEIGHT
        }
    };

    layout.left = (Zone){
        .zone = ZONE_LEFT,
        .bounds = {
            .x = 0,
            .y = TOP_HEIGHT,
            .width = SIDE_WIDTH,
            .height = centerHeight
        }
    };

    layout.right = (Zone){
        .zone = ZONE_RIGHT,
        .bounds = {
            .x = screen.width - SIDE_WIDTH,
            .y = TOP_HEIGHT,
            .width = SIDE_WIDTH,
            .height = centerHeight
        }
    };

    layout.center = (Zone){
        .zone = ZONE_CENTER,
        .bounds = {
            .x = SIDE_WIDTH,
            .y = TOP_HEIGHT,
            .width = centerWidth,
            .height = centerHeight
        }
    };

    return layout;
}


/**
 * ============================================================================
 * Mapeamento de cartas do protocolo (Carta/Cor) para renderizacao
 * ============================================================================
 */
int corParaAnsi(Cor cor) {
    switch (cor) {
        case AMARELO:  return COLOR_YELLOW;
        case VERMELHO: return COLOR_RED;
        case VERDE:    return COLOR_GREEN;
        case AZUL:     return COLOR_BLUE;
        case PRETO:    return COLOR_MAGENTA; /* coringa: preto nao contrasta com o fundo do terminal */
        default:       return COLOR_WHITE;
    }
}


/* Cor do texto do simbolo, para garantir contraste com o fundo da carta */
int corTextoParaCarta(Cor cor) {
    return (cor == AMARELO) ? COLOR_BLACK : COLOR_WHITE;
}


bool cartaEhJogavel(int cartaId, const int *idsPossiveis, int qtdPossiveis) {
    for (int i = 0; i < qtdPossiveis; ++i) {
        if (idsPossiveis[i] == cartaId) {
            return true;
        }
    }
    return false;
}


void drawCard(Screen *screen, Carta card, int x, int y, int isSelected, int isJogavel) {
    int bottom = y + CARD_HEIGHT - 1;
    const char *verticalBorder = isSelected ? "║" : "│";
    const char *horizontalBorder = isSelected ? "═" : "─";
    const char *leftBorder = isSelected ? "╔" : "╭";
    const char *rightBorder = isSelected ? "╝" : "╯";
    int borderColor = isJogavel ? COLOR_GREEN : COLOR_DEFAULT;

    /* bordas horizontais */
    for (int i = 0; i < CARD_WIDTH; ++i) {
        int posX = x + i;
        if (i == 0) {
            setPixel(screen, posX, y, leftBorder);
        } else if (i + 1 == CARD_WIDTH) {
            setPixel(screen, posX, bottom, rightBorder);
        } else {
            setPixel(screen, posX, y, horizontalBorder);
            setPixel(screen, posX, bottom, horizontalBorder);
        }
        setForeground(screen, posX, y, borderColor);
        setForeground(screen, posX, bottom, borderColor);
    }

    /* bordas verticais */
    for (int py = y + 1; py < bottom; ++py) {
        setPixel(screen, x, py, verticalBorder);
        setPixel(screen, x + CARD_WIDTH - 1, py, verticalBorder);
        setForeground(screen, x, py, borderColor);
        setForeground(screen, x + CARD_WIDTH - 1, py, borderColor);
    }

    /* preenchimento do miolo */
    for (int py = y + 1; py < bottom; ++py) {
        for (int px = x + 1; px < x + CARD_WIDTH - 1; ++px) {
            setBackground(screen, px, py, corParaAnsi(card.cor));
        }
    }

    /* simbolo (ate 3 caracteres, centralizado) */
    char sym[4];
    strncpy(sym, card.simbolo, 3);
    sym[3] = '\0';
    int symLen = (int) strlen(sym);
    int innerWidth = CARD_WIDTH - 2;
    int symX = x + 1 + (innerWidth - symLen) / 2;
    if (symX < x + 1) symX = x + 1;

    drawText(screen, symX, y + 2, sym, corTextoParaCarta(card.cor), corParaAnsi(card.cor));
}


void drawPlayerHand(Screen *screen, HandUI *hand, const Jogador *jogador) {
    Zone zone = hand->zone;
    int paddingY = zone.bounds.y + zone.bounds.height * 0.2;
    int paddingX = zone.bounds.x + zone.bounds.width * 0.1;

    const int CARD_GAP = 2;

    for (int i = 0; i < jogador->qtdCartas; ++i) {
        int cardX = paddingX + i * (CARD_WIDTH + CARD_GAP);
        int isSelected = (i == hand->selectedCard);
        int isJogavel = cartaEhJogavel(
            jogador->cartas[i].id,
            jogador->idsCartasPossiveis,
            jogador->qtdCartasPossiveis
        );
        drawCard(screen, jogador->cartas[i], cardX, paddingY, isSelected, isJogavel);
    }
}


void drawDiscardPile(Screen *screen, Zone zone, Carta card) {
    int x = zone.bounds.x + (zone.bounds.width - CARD_WIDTH) / 2;
    int y = zone.bounds.y + (zone.bounds.height - CARD_HEIGHT) / 2;

    drawCard(screen, card, x, y, 0, 0);
}


void drawHUD(Screen *screen, Zone topZone, const EstadoJogo *estado, const char *statusMsg) {
    int x = topZone.bounds.x + 2;
    int y = topZone.bounds.y + 1;

    char linha1[160];
    snprintf(linha1, sizeof(linha1), "Rodada %d | Adversario: %s (%d cartas)",
        estado->partida.numeroRodada,
        estado->partida.nomeAdversario,
        estado->partida.numeroCartasAdversario);
    drawText(screen, x, y, linha1, COLOR_WHITE, COLOR_DEFAULT);

    const char *turno = estado->partida.suaVez ? "SUA VEZ" : "AGUARDANDO ADVERSARIO...";
    int turnoColor = estado->partida.suaVez ? COLOR_GREEN : COLOR_YELLOW;
    drawText(screen, x, y + 2, turno, turnoColor, COLOR_DEFAULT);

    char linha2[64];
    snprintf(linha2, sizeof(linha2), "Suas cartas: %d", estado->jogador.qtdCartas);
    drawText(screen, x, y + 4, linha2, COLOR_WHITE, COLOR_DEFAULT);

    drawText(screen, x, y + 6,
        "<- -> selecionar   ENTER jogar   BAIXO comprar   CIMA UNO!   ESC sair",
        COLOR_CYAN, COLOR_DEFAULT);

    if (statusMsg[0] != '\0') {
        drawText(screen, x, y + 8, statusMsg, COLOR_RED, COLOR_DEFAULT);
    }
}


/**
 * ============================================================================
 * Rede
 * ============================================================================
 */
int enviarTudo(SOCKET socket, const char *buffer, int tamanho) {
    int totalEnviado = 0;

    while (totalEnviado < tamanho) {
        int enviado = send(socket, buffer + totalEnviado, tamanho - totalEnviado, 0);

        if (enviado == SOCKET_ERROR) {
            return -1;
        }

        totalEnviado += enviado;
    }

    return totalEnviado;
}


int recvAll(SOCKET socket, char *buffer, int tamanho) {
    int totalRecebido = 0;

    while (totalRecebido < tamanho) {
        int recebido = recv(socket, buffer + totalRecebido, tamanho - totalRecebido, 0);

        if (recebido == 0) {
            return 0;
        }

        if (recebido == SOCKET_ERROR) {
            return -1;
        }

        totalRecebido += recebido;
    }

    return totalRecebido;
}


bool enviarSolicitacao(SOCKET sock, int jogadorId, TipoAcao acao, int cartaId) {
    Solicitacao req = {0};
    req.jogadorId = jogadorId;
    req.acao = acao;
    req.cartaId = cartaId;

    bool ok = enviarTudo(sock, (const char *) &req, sizeof(req)) == (int) sizeof(req);

    debugLog("ENVIADO -> jogadorId=%d acao=%d cartaId=%d ok=%d",
        jogadorId, (int) acao, cartaId, ok ? 1 : 0);

    return ok;
}


/* Thread de recebimento: consome mensagens do servidor continuamente e
 * atualiza o estado compartilhado, protegido por CRITICAL_SECTION.
 *
 * OBS: a primeira mensagem (MSG_PARTIDA_INICIADA) e recebida e validada
 * de forma SINCRONA em main(), ANTES desta thread ser criada (ver secao
 * 10 da spec: a primeira mensagem deve ser obrigatoriamente
 * MSG_PARTIDA_INICIADA e o client deve validar isso). O case abaixo para
 * MSG_PARTIDA_INICIADA existe apenas como rede de seguranca, caso essa
 * mensagem chegue de novo por algum motivo - o fluxo normal nunca deve
 * passar por aqui para ela. */
DWORD WINAPI recvThreadProc(LPVOID param) {
    SOCKET sock = (SOCKET)(uintptr_t) param;

    while (1) {
        Mensagem msg;
        int resultado = recvAll(sock, (char *) &msg, sizeof(msg));

        if (resultado <= 0) {
            debugLog("RECEBIDO -> recvAll retornou %d (0=servidor fechou, <0=erro WSAError=%d)",
                resultado, WSAGetLastError());

            EnterCriticalSection(&g_shared.lock);
            g_shared.connectionLost = true;
            LeaveCriticalSection(&g_shared.lock);
            break;
        }

        debugLog("RECEBIDO <- tipo=%s (%d) suaVez=%d jogadorId=%d qtdCartas=%d",
            tipoMensagemParaTexto(msg.tipo), (int) msg.tipo,
            msg.estado.partida.suaVez, msg.estado.jogador.id,
            msg.estado.jogador.qtdCartas);

        EnterCriticalSection(&g_shared.lock);

        switch (msg.tipo) {
            case MSG_PARTIDA_INICIADA:
                /* Rede de seguranca - ver comentario acima da funcao. */
                g_shared.estado = msg.estado;
                g_shared.hasEstado = true;
                break;

            case MSG_ESTADO_JOGO:
                g_shared.estado = msg.estado;
                g_shared.hasEstado = true;
                break;

            case MSG_JOGADA_INVALIDA:
                snprintf(g_shared.statusMsg, sizeof(g_shared.statusMsg), "Jogada invalida!");
                g_shared.statusMsgExpiry = GetTickCount() + STATUS_MSG_DURATION_MS;
                break;

            case MSG_PARTIDA_FINALIZADA:
                g_shared.estado = msg.estado;
                g_shared.hasEstado = true;
                g_shared.partidaFinalizada = true;
                break;

            case MSG_JOGADOR_SAIU:
                g_shared.adversarioSaiu = true;
                break;
        }

        LeaveCriticalSection(&g_shared.lock);

        if (msg.tipo == MSG_PARTIDA_FINALIZADA || msg.tipo == MSG_JOGADOR_SAIU) {
            break;
        }
    }

    return 0;
}


int main(int argc, char *argv[]) {
    /* Buffer de stdio maior: reduz o numero de escritas reais no console
     * (complementa o fwrite unico de renderScreen). */
    static char stdoutBuf[STDOUT_BUFFER_SIZE];
    setvbuf(stdout, stdoutBuf, _IOFBF, sizeof(stdoutBuf));

    const char *serverIp = (argc > 1) ? argv[1] : DEFAULT_SERVER_IP;
    int serverPort = (argc > 2) ? atoi(argv[2]) : DEFAULT_SERVER_PORT;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    enableAnsiConsole();

    InitializeCriticalSection(&g_shared.lock);
    InitializeCriticalSection(&g_logLock);

    /* Comeca o log do zero a cada execucao. */
    remove(DEBUG_LOG_FILE);
    debugLog("=== client iniciado (pid=%lu) ===", (unsigned long) GetCurrentProcessId());

    /*
     * ========================================================
     * WINSOCK / CONEXAO
     * ========================================================
     */
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Erro ao inicializar Winsock.\n");
        return EXIT_FAILURE;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("Erro ao criar socket.\n");
        WSACleanup();
        return EXIT_FAILURE;
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons((u_short) serverPort);
    serverAddress.sin_addr.s_addr = inet_addr(serverIp);

    printf("Conectando a %s:%d...\n", serverIp, serverPort);
    fflush(stdout);

    if (connect(sock, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Erro ao conectar. WSAError: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("Conectado! Aguardando estado inicial do servidor...\n");
    fflush(stdout);

    /*
     * ========================================================
     * PRIMEIRA MENSAGEM: deve ser obrigatoriamente
     * MSG_PARTIDA_INICIADA (secao 10 da spec). Recebida de forma
     * sincrona, ANTES de criar a thread de recebimento, e validada -
     * isso estava faltando na versao anterior deste arquivo.
     * ========================================================
     */
    Mensagem primeiraMensagem;
    int resultadoInicial = recvAll(sock, (char *) &primeiraMensagem, sizeof(primeiraMensagem));

    if (resultadoInicial == 0) {
        printf("\nServidor desconectou antes de iniciar a partida.\n");
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (resultadoInicial < 0) {
        printf("\nErro ao receber mensagem inicial. WSAError: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    debugLog("RECEBIDO (sincrono) <- tipo=%s (%d) suaVez=%d jogadorId=%d",
        tipoMensagemParaTexto(primeiraMensagem.tipo), (int) primeiraMensagem.tipo,
        primeiraMensagem.estado.partida.suaVez, primeiraMensagem.estado.jogador.id);

    if (primeiraMensagem.tipo != MSG_PARTIDA_INICIADA) {
        printf("\nERRO DE PROTOCOLO!\n");
        printf("Esperado: MSG_PARTIDA_INICIADA\n");
        printf("Recebido: %d\n", primeiraMensagem.tipo);
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    /* Ainda nao existe outra thread rodando, entao nao precisa de lock
     * para esta primeira escrita. */
    g_shared.estado = primeiraMensagem.estado;
    g_shared.hasEstado = true;

    HANDLE hRecvThread = CreateThread(NULL, 0, recvThreadProc, (LPVOID)(uintptr_t) sock, 0, NULL);
    if (hRecvThread == NULL) {
        printf("Erro ao criar thread de recebimento.\n");
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    /*
     * ========================================================
     * SETUP DE TELA
     * ========================================================
     */
    Screen screen = getScreenSize();

    if (screen.height < (MIN_SCREEN_HEIGHT - VERTICAL_MARGIN)) {
        printf("[ERROR]: Screen height should be at least %d but was %d.\n", MIN_SCREEN_HEIGHT, screen.height);
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (screen.width < MIN_SCREEN_WIDTH) {
        printf("[ERROR]: Screen width should be at least %d but was %d.\n", MIN_SCREEN_WIDTH, screen.width);
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    BoardLayout layout = createBoardLayout(screen);
    HandUI hand = { .zone = layout.bottom, .selectedCard = 0 };

    int jogadorId = -1;
    int running = 1;

    /* Limpa o terminal e esconde o cursor UMA VEZ, antes do loop.
     * renderScreen() ja reescreve a tela inteira em todo frame, entao
     * nao ha necessidade (nem beneficio) de limpar de novo a cada
     * iteracao - era isso que causava o "flash" + desenho progressivo. */
    printf("\033[2J\033[H\033[?25l");
    fflush(stdout);

    while (running) {
        EstadoJogo estado;
        bool hasEstado, connectionLost, partidaFinalizada, adversarioSaiu;
        char statusMsg[128];
        DWORD now = GetTickCount();

        EnterCriticalSection(&g_shared.lock);
        estado = g_shared.estado;
        hasEstado = g_shared.hasEstado;
        connectionLost = g_shared.connectionLost;
        partidaFinalizada = g_shared.partidaFinalizada;
        adversarioSaiu = g_shared.adversarioSaiu;
        if (g_shared.statusMsg[0] != '\0' && now > g_shared.statusMsgExpiry) {
            g_shared.statusMsg[0] = '\0';
        }
        strncpy(statusMsg, g_shared.statusMsg, sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';
        LeaveCriticalSection(&g_shared.lock);

        if (hasEstado) {
            jogadorId = estado.jogador.id;
        }

        clearScreen(&screen);
        drawBoardBorder(&screen);
        drawBoard(&screen, layout);

        if (!hasEstado) {
            drawText(&screen, layout.center.bounds.x + 8,
                layout.center.bounds.y + layout.center.bounds.height / 2,
                "Aguardando servidor...", COLOR_WHITE, COLOR_DEFAULT);
        } else {
            drawDiscardPile(&screen, layout.center, estado.partida.ultimaCarta);

            if (hand.selectedCard >= estado.jogador.qtdCartas) {
                hand.selectedCard = (estado.jogador.qtdCartas > 0) ? estado.jogador.qtdCartas - 1 : 0;
            }

            drawPlayerHand(&screen, &hand, &estado.jogador);
            drawHUD(&screen, layout.top, &estado, statusMsg);
        }

        renderScreen(&screen);

        if (connectionLost) {
            printf("\033[?25h\nConexao com o servidor perdida.\n");
            break;
        }
        if (adversarioSaiu) {
            printf("\033[?25h\nO adversario saiu da partida.\n");
            fflush(stdout);
            Sleep(2000);
            break;
        }
        if (partidaFinalizada) {
            printf("\033[?25h\nPartida finalizada!\n");
            fflush(stdout);
            Sleep(2000);
            break;
        }

        Input input = readInput();

        if (hasEstado) {
            switch (input) {
                case INPUT_RIGHT:
                    if (hand.selectedCard < estado.jogador.qtdCartas - 1) {
                        hand.selectedCard++;
                    }
                    break;

                case INPUT_LEFT:
                    if (hand.selectedCard > 0) {
                        hand.selectedCard--;
                    }
                    break;

                case INPUT_ENTER:
                    if (estado.partida.suaVez && estado.jogador.qtdCartas > 0) {
                        int cartaId = estado.jogador.cartas[hand.selectedCard].id;
                        enviarSolicitacao(sock, jogadorId, ACAO_JOGAR_CARTA, cartaId);
                    }
                    break;

                case INPUT_DOWN:
                    if (estado.partida.suaVez) {
                        /* Secao 18 da spec: cartaId nao e utilizado nesta
                         * acao e deve ser 0 (o codigo anterior enviava -1). */
                        enviarSolicitacao(sock, jogadorId, ACAO_COMPRAR_CARTA, 0);
                    }
                    break;

                case INPUT_UP:
                    enviarSolicitacao(sock, jogadorId, ACAO_DIZER_UNO, 0);
                    break;

                case INPUT_ESCAPE:
                    running = 0;
                    break;

                default:
                    break;
            }
        } else if (input == INPUT_ESCAPE) {
            running = 0;
        }

        Sleep(30);
    }

    closesocket(sock);
    WSACleanup();
    free(screen.buffer);
    debugLog("=== client encerrado ===");
    DeleteCriticalSection(&g_shared.lock);
    DeleteCriticalSection(&g_logLock);

    printf("\033[?25h"); /* garante que o cursor volta a aparecer */
    printf("Encerrado. Pressione ENTER para sair...\n");
    fflush(stdout);
    getchar();

    return EXIT_SUCCESS;
}