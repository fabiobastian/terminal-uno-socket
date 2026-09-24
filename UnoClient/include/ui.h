#ifndef UI_H
#define UI_H

#include "protocol.h"

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

void enableAnsiConsole(void);
Input readInput(void);

Screen getScreenSize(void);
void freeScreen(Screen *screen);

void clearScreen(Screen *screen);
void drawBoardBorder(Screen *screen);
BoardLayout createBoardLayout(Screen screen);
void drawBoard(Screen *screen, BoardLayout layout);
void drawWaitingMessage(Screen *screen, Zone zone);

void drawPlayerHand(Screen *screen, HandUI *hand, const Jogador *jogador);
void drawDiscardPile(Screen *screen, Zone zone, Carta card);
void drawHUD(Screen *screen, Zone topZone, const EstadoJogo *estado, const char *statusMsg);

void renderScreen(Screen *screen);

#endif
