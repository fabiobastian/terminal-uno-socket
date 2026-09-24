#include "../include/ui.h"

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#define VERTICAL_MARGIN 10
#define MIN_SCREEN_WIDTH 150
#define SIDE_WIDTH 15
#define TOP_HEIGHT 10
#define BOTTOM_HEIGHT 10

#define COLOR_DEFAULT 0
#define COLOR_BLACK 30
#define COLOR_RED 31
#define COLOR_GREEN 32
#define COLOR_YELLOW 33
#define COLOR_BLUE 34
#define COLOR_MAGENTA 35
#define COLOR_CYAN 36
#define COLOR_WHITE 37

#define CARD_WIDTH 5
#define CARD_HEIGHT 5
#define CARD_GAP 2
#define RENDER_BUF_INITIAL_CAPACITY 65536

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static int corParaAnsi(Cor cor);
static int corTextoParaCarta(Cor cor);
static bool cartaEhJogavel(int cartaId, const int *idsPossiveis, int qtdPossiveis);
static void setPixel(Screen *screen, int x, int y, const char *character);
static void setPixelChar(Screen *screen, int x, int y, char c);
static void setForeground(Screen *screen, int x, int y, int color);
static void setBackground(Screen *screen, int x, int y, int color);
static void drawText(Screen *screen, int x, int y, const char *text, int fg, int bg);
static void drawZoneBorder(Screen *screen, Zone zone);
static void drawCard(Screen *screen, Carta card, int x, int y, int selected, int jogavel);
static void sbInit(StrBuf *sb);
static void sbAppend(StrBuf *sb, const char *data, size_t size);
static void sbAppendStr(StrBuf *sb, const char *data);
static void sbFree(StrBuf *sb);

void enableAnsiConsole(void)
{
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;

    if (console == INVALID_HANDLE_VALUE || !GetConsoleMode(console, &mode)) {
        return;
    }

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
    SetConsoleMode(console, mode);
}

Input readInput(void)
{
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
            default: return INPUT_NONE;
        }
    }

    if (c == 13) return INPUT_ENTER;
    if (c == 27) return INPUT_ESCAPE;

    return INPUT_NONE;
}

Screen getScreenSize(void)
{
    Screen screen = {0};
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (console == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(console, &info)) {
        return screen;
    }

    screen.width = info.srWindow.Right - info.srWindow.Left + 1;
    screen.height = info.srWindow.Bottom - info.srWindow.Top + 1 - VERTICAL_MARGIN;

    if (screen.height <= 0) {
        return (Screen){0};
    }

    screen.buffer = malloc(
        (size_t)screen.width * screen.height * sizeof(Cell)
    );

    if (screen.buffer == NULL) {
        return (Screen){0};
    }

    return screen;
}

void freeScreen(Screen *screen)
{
    if (screen == NULL) return;

    free(screen->buffer);
    screen->buffer = NULL;
    screen->width = 0;
    screen->height = 0;
}

static void setPixel(Screen *screen, int x, int y, const char *character)
{
    if (screen == NULL || screen->buffer == NULL ||
        x < 0 || x >= screen->width ||
        y < 0 || y >= screen->height) {
        return;
    }

    int index = y * screen->width + x;

    strncpy(
        screen->buffer[index].character,
        character,
        sizeof(screen->buffer[index].character) - 1
    );

    screen->buffer[index].character[
        sizeof(screen->buffer[index].character) - 1
    ] = '\0';
}

static void setPixelChar(Screen *screen, int x, int y, char c)
{
    char character[2] = {c, '\0'};
    setPixel(screen, x, y, character);
}

static void setForeground(Screen *screen, int x, int y, int color)
{
    if (screen == NULL || screen->buffer == NULL ||
        x < 0 || x >= screen->width ||
        y < 0 || y >= screen->height) {
        return;
    }

    screen->buffer[y * screen->width + x].foreground = color;
}

static void setBackground(Screen *screen, int x, int y, int color)
{
    if (screen == NULL || screen->buffer == NULL ||
        x < 0 || x >= screen->width ||
        y < 0 || y >= screen->height) {
        return;
    }

    screen->buffer[y * screen->width + x].background = color;
}

static void drawText(Screen *screen, int x, int y, const char *text, int fg, int bg)
{
    for (int i = 0; text[i] != '\0'; ++i) {
        setPixelChar(screen, x + i, y, text[i]);
        setForeground(screen, x + i, y, fg);
        setBackground(screen, x + i, y, bg);
    }
}

void clearScreen(Screen *screen)
{
    if (screen == NULL || screen->buffer == NULL) return;

    for (int y = 0; y < screen->height; ++y) {
        for (int x = 0; x < screen->width; ++x) {
            int index = y * screen->width + x;
            strcpy(screen->buffer[index].character, " ");
            screen->buffer[index].foreground = COLOR_DEFAULT;
            screen->buffer[index].background = COLOR_DEFAULT;
        }
    }
}

void drawBoardBorder(Screen *screen)
{
    int width = screen->width;
    int height = screen->height;

    for (int x = 0; x < width; ++x) {
        setPixel(screen, x, 0, "═");
        setPixel(screen, x, height - 1, "═");
    }

    for (int y = 0; y < height; ++y) {
        setPixel(screen, 0, y, y == 0 ? "╔" : y == height - 1 ? "╚" : "║");
        setPixel(screen, width - 1, y, y == 0 ? "╗" : y == height - 1 ? "╝" : "║");
    }
}

static void drawZoneBorder(Screen *screen, Zone zone)
{
    int x = zone.bounds.x;
    int y = zone.bounds.y;
    int width = zone.bounds.width;
    int height = zone.bounds.height;

    switch (zone.zone) {
        case ZONE_TOP:
            for (int px = x; px < x + width - 1; ++px) {
                setPixel(screen, px, height, "╼");
            }
            break;

        case ZONE_BOTTOM:
            for (int px = x; px < x + width; ++px) {
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
                setPixel(screen, px, bottom, "═");
            }

            for (int py = top; py <= bottom; ++py) {
                setPixel(screen, left, py, py == top ? "╔" : py == bottom ? "╚" : "║");
                setPixel(screen, right, py, py == top ? "╗" : py == bottom ? "╝" : "║");
            }
            break;
        }

        default:
            break;
    }
}

BoardLayout createBoardLayout(Screen screen)
{
    int centerWidth = screen.width - 2 * SIDE_WIDTH;
    int centerHeight = screen.height - TOP_HEIGHT - BOTTOM_HEIGHT;

    BoardLayout layout = {
        .top = {
            .zone = ZONE_TOP,
            .bounds = {1, 1, screen.width - 1, TOP_HEIGHT}
        },
        .bottom = {
            .zone = ZONE_BOTTOM,
            .bounds = {1, screen.height - BOTTOM_HEIGHT, screen.width - 2, BOTTOM_HEIGHT}
        },
        .left = {
            .zone = ZONE_LEFT,
            .bounds = {0, TOP_HEIGHT, SIDE_WIDTH, centerHeight}
        },
        .right = {
            .zone = ZONE_RIGHT,
            .bounds = {screen.width - SIDE_WIDTH, TOP_HEIGHT, SIDE_WIDTH, centerHeight}
        },
        .center = {
            .zone = ZONE_CENTER,
            .bounds = {SIDE_WIDTH, TOP_HEIGHT, centerWidth, centerHeight}
        }
    };

    return layout;
}

void drawBoard(Screen *screen, BoardLayout layout)
{
    drawZoneBorder(screen, layout.top);
    drawZoneBorder(screen, layout.bottom);
    drawZoneBorder(screen, layout.center);
}

void drawWaitingMessage(Screen *screen, Zone zone)
{
    drawText(
        screen,
        zone.bounds.x + 8,
        zone.bounds.y + zone.bounds.height / 2,
        "Aguardando servidor...",
        COLOR_WHITE,
        COLOR_DEFAULT
    );
}

static int corParaAnsi(Cor cor)
{
    switch (cor) {
        case AMARELO:  return COLOR_YELLOW;
        case VERMELHO: return COLOR_RED;
        case VERDE:    return COLOR_GREEN;
        case AZUL:     return COLOR_BLUE;
        case PRETO:    return COLOR_MAGENTA;
        default:       return COLOR_WHITE;
    }
}

static int corTextoParaCarta(Cor cor)
{
    return cor == AMARELO ? COLOR_BLACK : COLOR_WHITE;
}

static bool cartaEhJogavel(int cartaId, const int *idsPossiveis, int qtdPossiveis)
{
    for (int i = 0; i < qtdPossiveis; ++i) {
        if (idsPossiveis[i] == cartaId) return true;
    }

    return false;
}

static void drawCard(
    Screen *screen,
    Carta card,
    int x,
    int y,
    int selected,
    int jogavel
)
{
    int bottom = y + CARD_HEIGHT - 1;
    const char *vertical = selected ? "║" : "│";
    const char *horizontal = selected ? "═" : "─";
    const char *left = selected ? "╔" : "╭";
    const char *right = selected ? "╝" : "╯";
    int borderColor = jogavel ? COLOR_GREEN : COLOR_DEFAULT;
    int cardColor = corParaAnsi(card.cor);

    for (int i = 0; i < CARD_WIDTH; ++i) {
        int posX = x + i;

        if (i == 0) {
            setPixel(screen, posX, y, left);
        } else if (i + 1 == CARD_WIDTH) {
            setPixel(screen, posX, bottom, right);
        } else {
            setPixel(screen, posX, y, horizontal);
            setPixel(screen, posX, bottom, horizontal);
        }

        setForeground(screen, posX, y, borderColor);
        setForeground(screen, posX, bottom, borderColor);
    }

    for (int py = y + 1; py < bottom; ++py) {
        setPixel(screen, x, py, vertical);
        setPixel(screen, x + CARD_WIDTH - 1, py, vertical);
        setForeground(screen, x, py, borderColor);
        setForeground(screen, x + CARD_WIDTH - 1, py, borderColor);
    }

    for (int py = y + 1; py < bottom; ++py) {
        for (int px = x + 1; px < x + CARD_WIDTH - 1; ++px) {
            setBackground(screen, px, py, cardColor);
        }
    }

    char symbol[4];
    strncpy(symbol, card.simbolo, 3);
    symbol[3] = '\0';

    int symbolLength = (int)strlen(symbol);
    int innerWidth = CARD_WIDTH - 2;
    int symbolX = x + 1 + (innerWidth - symbolLength) / 2;

    if (symbolX < x + 1) symbolX = x + 1;

    drawText(
        screen,
        symbolX,
        y + 2,
        symbol,
        corTextoParaCarta(card.cor),
        cardColor
    );
}

void drawPlayerHand(Screen *screen, HandUI *hand, const Jogador *jogador)
{
    int paddingY = hand->zone.bounds.y + (int)(hand->zone.bounds.height * 0.2);
    int paddingX = hand->zone.bounds.x + (int)(hand->zone.bounds.width * 0.1);

    for (int i = 0; i < jogador->qtdCartas; ++i) {
        int cardX = paddingX + i * (CARD_WIDTH + CARD_GAP);
        bool selected = i == hand->selectedCard;
        bool jogavel = cartaEhJogavel(
            jogador->cartas[i].id,
            jogador->idsCartasPossiveis,
            jogador->qtdCartasPossiveis
        );

        drawCard(
            screen,
            jogador->cartas[i],
            cardX,
            paddingY,
            selected,
            jogavel
        );
    }
}

void drawDiscardPile(Screen *screen, Zone zone, Carta card)
{
    int x = zone.bounds.x + (zone.bounds.width - CARD_WIDTH) / 2;
    int y = zone.bounds.y + (zone.bounds.height - CARD_HEIGHT) / 2;
    drawCard(screen, card, x, y, 0, 0);
}

void drawHUD(Screen *screen, Zone topZone, const EstadoJogo *estado, const char *statusMsg)
{
    int x = topZone.bounds.x + 2;
    int y = topZone.bounds.y + 1;
    char line[160];

    snprintf(
        line,
        sizeof(line),
        "Rodada %d | Adversario: %s (%d cartas)",
        estado->partida.numeroRodada,
        estado->partida.nomeAdversario,
        estado->partida.numeroCartasAdversario
    );
    drawText(screen, x, y, line, COLOR_WHITE, COLOR_DEFAULT);

    const char *turn = estado->partida.suaVez
        ? "SUA VEZ"
        : "AGUARDANDO ADVERSARIO...";
    int turnColor = estado->partida.suaVez ? COLOR_GREEN : COLOR_YELLOW;
    drawText(screen, x, y + 2, turn, turnColor, COLOR_DEFAULT);

    snprintf(line, sizeof(line), "Suas cartas: %d", estado->jogador.qtdCartas);
    drawText(screen, x, y + 4, line, COLOR_WHITE, COLOR_DEFAULT);

    drawText(
        screen,
        x,
        y + 6,
        "<- -> selecionar   ENTER jogar   BAIXO comprar   CIMA UNO!   ESC sair",
        COLOR_CYAN,
        COLOR_DEFAULT
    );

    if (statusMsg != NULL && statusMsg[0] != '\0') {
        drawText(screen, x, y + 8, statusMsg, COLOR_RED, COLOR_DEFAULT);
    }
}

static void sbInit(StrBuf *sb)
{
    sb->cap = RENDER_BUF_INITIAL_CAPACITY;
    sb->len = 0;
    sb->data = malloc(sb->cap);

    if (sb->data != NULL) sb->data[0] = '\0';
}

static void sbAppend(StrBuf *sb, const char *data, size_t size)
{
    if (sb->data == NULL || size == 0) return;

    if (sb->len + size + 1 > sb->cap) {
        size_t newCap = sb->cap;

        while (sb->len + size + 1 > newCap) {
            newCap *= 2;
        }

        char *newData = realloc(sb->data, newCap);
        if (newData == NULL) return;

        sb->data = newData;
        sb->cap = newCap;
    }

    memcpy(sb->data + sb->len, data, size);
    sb->len += size;
    sb->data[sb->len] = '\0';
}

static void sbAppendStr(StrBuf *sb, const char *data)
{
    sbAppend(sb, data, strlen(data));
}

static void sbFree(StrBuf *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

void renderScreen(Screen *screen)
{
    StrBuf sb;
    sbInit(&sb);
    sbAppendStr(&sb, "\033[H");

    int lastFg = -1;
    int lastBg = -1;

    for (int y = 0; y < screen->height; ++y) {
        for (int x = 0; x < screen->width; ++x) {
            Cell cell = screen->buffer[y * screen->width + x];

            if (cell.foreground != lastFg || cell.background != lastBg) {
                if (cell.foreground == COLOR_DEFAULT &&
                    cell.background == COLOR_DEFAULT) {
                    sbAppendStr(&sb, "\033[0m");
                } else {
                    char sequence[32];
                    int fg = cell.foreground == COLOR_DEFAULT
                        ? COLOR_WHITE
                        : cell.foreground;
                    int bg = cell.background == COLOR_DEFAULT
                        ? 49
                        : cell.background + 10;
                    int length = snprintf(
                        sequence,
                        sizeof(sequence),
                        "\033[%d;%dm",
                        fg,
                        bg
                    );

                    if (length > 0) {
                        sbAppend(&sb, sequence, (size_t)length);
                    }
                }

                lastFg = cell.foreground;
                lastBg = cell.background;
            }

            sbAppendStr(&sb, cell.character);
        }

        sbAppendStr(&sb, "\033[0m");

        if (y < screen->height - 1) {
            sbAppendStr(&sb, "\n");
        }

        lastFg = -1;
        lastBg = -1;
    }

    sbAppendStr(&sb, "\033[0J");

    if (sb.data != NULL) {
        fwrite(sb.data, 1, sb.len, stdout);
        fflush(stdout);
    }

    sbFree(&sb);
}
