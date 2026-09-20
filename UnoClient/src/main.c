/**
 * @file:       client.c
 * @author:     Nathan Berger
 * @date:       2026-09-20
 * @version     2.0
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


void clearScreen(Screen *screen) {
    for (int y = 0; y < screen->height; y++) {
        for (int x = 0; x < screen->width; x++) {
            int idx = y * screen->width + x;
            strcpy(screen->buffer[idx].character, " ");
            screen->buffer[idx].foreground = COLOR_DEFAULT;
            screen->buffer[idx].background = COLOR_DEFAULT;
        }
    }
    printf("\033[H\033[2J");
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


void renderScreen(Screen *screen) {
    int lastFg = -1;
    int lastBg = -1;

    for (int y = 0; y < screen->height; y++) {
        for (int x = 0; x < screen->width; x++) {
            int idx = y * screen->width + x;
            Cell cell = screen->buffer[idx];

            if (cell.foreground != lastFg || cell.background != lastBg) {
                if (cell.foreground == COLOR_DEFAULT && cell.background == COLOR_DEFAULT) {
                    printf("\033[0m");
                } else {
                    int fg = (cell.foreground == COLOR_DEFAULT) ? COLOR_WHITE : cell.foreground;
                    int bg = (cell.background == COLOR_DEFAULT) ? 49 : cell.background + 10;
                    printf("\033[%d;%dm", fg, bg);
                }
                lastFg = cell.foreground;
                lastBg = cell.background;
            }

            printf("%s", cell.character);
        }
        printf("\033[0m\n");
        lastFg = -1;
        lastBg = -1;
    }
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

    return enviarTudo(sock, (const char *) &req, sizeof(req)) == (int) sizeof(req);
}


/* Thread de recebimento: consome mensagens do servidor continuamente e
 * atualiza o estado compartilhado, protegido por CRITICAL_SECTION. */
DWORD WINAPI recvThreadProc(LPVOID param) {
    SOCKET sock = (SOCKET)(uintptr_t) param;

    while (1) {
        Mensagem msg;
        int resultado = recvAll(sock, (char *) &msg, sizeof(msg));

        if (resultado <= 0) {
            EnterCriticalSection(&g_shared.lock);
            g_shared.connectionLost = true;
            LeaveCriticalSection(&g_shared.lock);
            break;
        }

        EnterCriticalSection(&g_shared.lock);

        switch (msg.tipo) {
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
    const char *serverIp = (argc > 1) ? argv[1] : DEFAULT_SERVER_IP;
    int serverPort = (argc > 2) ? atoi(argv[2]) : DEFAULT_SERVER_PORT;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    InitializeCriticalSection(&g_shared.lock);

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

    if (connect(sock, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Erro ao conectar. WSAError: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("Conectado! Aguardando estado inicial do servidor...\n");

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
            printf("\nConexao com o servidor perdida.\n");
            break;
        }
        if (adversarioSaiu) {
            printf("\nO adversario saiu da partida.\n");
            Sleep(2000);
            break;
        }
        if (partidaFinalizada) {
            printf("\nPartida finalizada!\n");
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
                        enviarSolicitacao(sock, jogadorId, ACAO_COMPRAR_CARTA, -1);
                    }
                    break;

                case INPUT_UP:
                    enviarSolicitacao(sock, jogadorId, ACAO_DIZER_UNO, -1);
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
    DeleteCriticalSection(&g_shared.lock);

    printf("Encerrado. Pressione ENTER para sair...\n");
    getchar();

    return EXIT_SUCCESS;
}