/**
 * @file:       client.c
 * @author:     Nathan Berger
 * @date:       2026-09-12
 * @version     1.0
 * @brief       Client responsible for rendering and orquestrate game logic.
 * 
 * MIT LICENSE
 *
 * Copyright (c) 2026 Fábio Júnior Nielsson Bastian.
 * Unauthorized copying or use of this file is prohibited.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>


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


typedef struct {
    char value;
    char symbol;
    int color; // NOVO: cor de fundo da carta (COLOR_RED, COLOR_BLUE, etc.)
} Card;


typedef struct {
    int id;
    Zone zone;
    Card *cards;
    int cardCount;
    int selectedCard;
} Player;


typedef enum {
    INPUT_NONE,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_ENTER,
    INPUT_ESCAPE
} Input;


Input readInput() {
    int c = _getch();

    if (c == 0 || c == 224) {
        c = _getch();

        switch (c) {
            case 72: return INPUT_UP;
            case 80: return INPUT_DOWN;
            case 75: return INPUT_LEFT;
            case 77: return INPUT_RIGHT;
        }
    }

    if (c == 13)
        return INPUT_ENTER;

    if (c == 27)
        return INPUT_ESCAPE;

    return INPUT_NONE;
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
        printf("OUT OF BOUNDS: x=%d y=%d screen=%dx%d\n", x, y, screen->width, screen->height);
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


void clearScreen(Screen *screen) {
    for (int y = 0; y < screen->height; y++) {
        for (int x = 0; x < screen->width; x++) {
            int idx = y * screen->width + x;
            //screen->buffer[idx].character  = " ";
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

    // Top
    for (int x = 0; x < width; x++) {
        setPixel(screen, x, 0, "═");
    }

    // Bottom
    for (int x = 0; x < width; x++) {
        setPixel(screen, x, height - 1, "═");
    }

    // Left
    for (int y = 0; y < height; y++) {
        if (y == 0) { setPixel(screen, 0, y, "╔"); } else if (y == height - 1) { setPixel(screen, 0, y, "╚"); } else {
            setPixel(screen, 0, y, "║");
        }
    }

    // Right
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
        case ZONE_CENTER:
            const int paddingY = 5;
            const int paddingX = 20;

            int left = x + paddingX;
            int right = x + width - paddingX;
            int top = y + paddingY;
            int bottom = y + height - paddingY;

            // top
            for (int px = left; px <= right; ++px) {
                setPixel(screen, px, top, "═");
            }

            // bottom
            for (int px = left; px <= right; ++px) {
                setPixel(screen, px, bottom, "═");
            }

            // left
            for (int py = top; py <= bottom; ++py) {
                if (py == top) {
                    setPixel(screen, left, py, "╔");
                } else if (py == bottom) {
                    setPixel(screen, left, py, "╚");
                } else {
                    setPixel(screen, left, py, "║");
                }
            }

            // right
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


void drawCard(Screen *screen, Card card, int x, int y, int isSelected) {
    int bottom = y + CARD_HEIGHT - 1;
    char *verticalBorder = isSelected == 0 ? "│" : "║";
    char *horizontalBorder = isSelected == 0 ? "─" : "═";
    char *leftBorder = isSelected == 0 ? "╭" : "╔";
    char *rightBorder = isSelected == 0 ? "╯" : "╝";

    // symbol
    setPixelChar(screen, x + (CARD_WIDTH / 2), y + 2, card.symbol);

    // horizontal (bordas)
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
    }

    // vertical (bordas)
    for (int py = y + 1; py < bottom; ++py) {
        setPixel(screen, x, py, verticalBorder);
        setPixel(screen, x + CARD_WIDTH - 1, py, verticalBorder);
    }

    // preenchimento do miolo
    for (int py = y + 1; py < bottom; ++py) {
        for (int px = x + 1; px < x + CARD_WIDTH - 1; ++px) {
            setBackground(screen, px, py, card.color);
        }
    }
}


void drawPlayerHand(Screen *screen, Player *p) {
    Zone zone = p->zone;
    int paddingY = zone.bounds.y + zone.bounds.height * 0.2;
    int paddingX = zone.bounds.x + zone.bounds.width * 0.1;

    const int CARD_GAP = 2;

    for (int i = 0; i < p->cardCount; ++i) {
        int cardX = paddingX + i * (CARD_WIDTH + CARD_GAP);
        int isSelected = (i == p->selectedCard);
        drawCard(screen, p->cards[i], cardX, paddingY, isSelected);
    }
}


void drawDiscardPile(Screen *screen, Zone zone, Card card) {
    int x = zone.bounds.x + (zone.bounds.width - CARD_WIDTH) / 2;
    int y = zone.bounds.y + (zone.bounds.height - CARD_HEIGHT) / 2;

    drawCard(screen, card, x, y, 0);
}


Card lastCard = {
    .value = '1',
    .symbol = '1',
    .color = COLOR_RED
};

Card cards[] = {
    {
        .value = '1',
        .symbol = '1',
        .color = COLOR_RED
    },
    {
        .value = '1',
        .symbol = '2',
        .color = COLOR_GREEN
    },
    {
        .value = '1',
        .symbol = '7',
        .color = COLOR_BLUE
    },
};


int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    Screen screen = getScreenSize();

    /**
     * Setup and screen validation
     */
    {
        if (screen.height < (MIN_SCREEN_HEIGHT - VERTICAL_MARGIN)) {
            printf("[ERROR]: Screen height should be at least %d but was %d.\n", MIN_SCREEN_HEIGHT, screen.height);
            return 1;
        }

        if (screen.width < MIN_SCREEN_WIDTH) {
            printf("[ERROR]: Screen width should be at least %d but was %d.\n", MIN_SCREEN_WIDTH, screen.width);
            return 1;
        }
    }

    BoardLayout layout = createBoardLayout(screen);
    Player p = {
        .id = 1,
        .zone = layout.top,
        .cards = cards,
        .cardCount = 3,
        .selectedCard = 0
    };

    int running = 1;
    while (running) {
        clearScreen(&screen);

        drawBoardBorder(&screen);

        drawBoard(&screen, layout);

        drawDiscardPile(&screen, layout.center, lastCard);

        drawPlayerHand(&screen, &p);

        renderScreen(&screen);

        Input input = readInput();

        if (input == INPUT_RIGHT && p.selectedCard < p.cardCount - 1) {
            p.selectedCard++;
        }

        if (input == INPUT_LEFT && p.selectedCard > 0) {
            p.selectedCard--;
        }
    }

    return 0;
}
