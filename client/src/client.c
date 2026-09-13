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
#include <sys/ioctl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>


/**
 * ============================================================================
 * Constants
 * ============================================================================
 */
#define MIN_SCREEN_HEIGHT 50
#define VERTICAL_MARGIN   10
#define MIN_SCREEN_WIDTH  200
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


/**
 * ============================================================================
 * Structs
 * ============================================================================
 */
typedef struct {
    char character;
    int  foreground;
    int  background;
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
} Rectangle;


typedef enum {
    ZONE_TOP,
    ZONE_RIGHT,
    ZONE_LEFT,
    ZONE_BOTTOM,
    ZONE_CENTER
} ZoneType;


typedef struct {
    Rectangle bounds;
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
    int  color; // NOVO: cor de fundo da carta (COLOR_RED, COLOR_BLUE, etc.)
} Card;


typedef struct {
    int   id;
    Zone  zone;
    Card *cards;
    int  cardCount;
} Player;


// @IMPROVE(nathan): so funciona no linux esse carinha aqui, depois preciso encontrar uma
// forma melhor de fazer ele ser dinamico, usando alguma variavel na inicializacao, etc.
Screen getScreenSize() {
	struct winsize w;
	Screen  s;

	// sucesso
	if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &w ) == 0 ) {
		s.width  = w.ws_col;
		s.height = w.ws_row - VERTICAL_MARGIN;
		s.buffer = malloc( w.ws_row * w.ws_col * sizeof(Cell) );
		return s;
	}

	s.width  = 0;
	s.height = 0;
	s.buffer = NULL;
	return s;
}


int index( Screen *screen, int x, int y ) {
    return y * screen->width + x;
}


void setPixel( Screen *screen, int x, int y, char character ) {
    if ( x < 0 || x >= screen->width ||  y < 0 || y >= screen->height ) {
        printf( "OUT OF BOUNDS: x=%d y=%d screen=%dx%d\n", x, y, screen->width, screen->height );
        return;
    }
    screen->buffer[ y * screen->width + x ].character = character;
}


void setForeground( Screen *screen, int x, int y, int color ) {
    if ( x < 0 || x >= screen->width || y < 0 || y >= screen->height ) {
        return;
    }
    screen->buffer[ y * screen->width + x ].foreground = color;
}


void setBackground( Screen *screen, int x, int y, int color ) {
    if ( x < 0 || x >= screen->width || y < 0 || y >= screen->height ) {
        return;
    }
    screen->buffer[ y * screen->width + x ].background = color;
}


void clearScreen( Screen *screen ) {
    for ( int y = 0; y < screen->height; y++ ) {
        for ( int x = 0; x < screen->width; x++ ) {
            int idx = y * screen->width + x;
            screen->buffer[idx].character  = ' ';
            screen->buffer[idx].foreground = COLOR_DEFAULT;
            screen->buffer[idx].background = COLOR_DEFAULT;
        }
    }
}


void drawBoardBorder( Screen *screen ) {
    int width  = screen->width;
    int height = screen->height;

    // Top
    for ( int x = 0; x < width; x++ ) {
        setPixel( screen, x, 0, '-' );
    }

    // Bottom
    for ( int x = 0; x < width; x++ ) {
        setPixel( screen, x, height - 1, '-' );
    }

    // Left
    for ( int y = 0; y < height; y++ ) {
        setPixel( screen, 0, y, '|' );
    }

    // Right
    for ( int y = 0; y < height; y++ ) {
        setPixel( screen, width - 1, y, '|' );
    }
}


void renderScreen( Screen *screen ) {
    int lastFg = -1;
    int lastBg = -1;

    for ( int y = 0; y < screen->height; y++ ) {
        for ( int x = 0; x < screen->width; x++ ) {
            int idx  = y * screen->width + x;
            Cell cell = screen->buffer[idx];

            if ( cell.foreground != lastFg || cell.background != lastBg ) {
                if ( cell.foreground == COLOR_DEFAULT && cell.background == COLOR_DEFAULT ) {
                    printf( "\033[0m" );
                } else {
                    int fg = ( cell.foreground == COLOR_DEFAULT ) ? COLOR_WHITE : cell.foreground;
                    int bg = ( cell.background == COLOR_DEFAULT ) ? 49 : cell.background + 10;
                    printf( "\033[%d;%dm", fg, bg );
                }
                lastFg = cell.foreground;
                lastBg = cell.background;
            }

            putchar( cell.character );
        }
        printf( "\033[0m\n" );
        lastFg = -1;
        lastBg = -1;
    }
}


void drawZoneBorder( Screen *screen, Zone layout ) {
	int x = layout.bounds.x;
	int y = layout.bounds.y;
	int width = layout.bounds.width;
	int height = layout.bounds.height;

	switch( layout.zone ) {
		case ZONE_TOP:
            for ( int px = x; px < x + width - 1; px++ ) {
                setPixel( screen, px, height, '-' );
            }
			break;
        case ZONE_BOTTOM:
            for ( int px = x; px < x + width; px++ ) {
                setPixel( screen, px, y, '-' );
            }
		    break;
	}
}


void drawBoard( Screen *screen, BoardLayout layout ) {
	const float verticalPadding   = 0.80;
	const float horizontalPadding = 0.90;

    int width  = screen->width;
    int height = screen->height;

	drawZoneBorder( screen, layout.top );
	drawZoneBorder( screen, layout.bottom );
}


BoardLayout createBoardLayout( Screen screen ) {
    BoardLayout layout;

    int centerWidth = screen.width - ( 2 * SIDE_WIDTH );
    int centerHeight = screen.height - TOP_HEIGHT - BOTTOM_HEIGHT;

    layout.top = (Zone) {
        .zone = ZONE_TOP,
        .bounds = {
            .x = 1,
            .y = 1,
            .width = screen.width - 1,
            .height = TOP_HEIGHT
        }
    };

    layout.bottom = (Zone) {
        .zone = ZONE_BOTTOM,
        .bounds = {
            .x = 1,
            .y = screen.height - BOTTOM_HEIGHT,
            .width = screen.width - 2,
            .height = BOTTOM_HEIGHT
        }
    };

    layout.left = (Zone) {
        .zone = ZONE_LEFT,
        .bounds = {
            .x = 0,
            .y = TOP_HEIGHT,
            .width = SIDE_WIDTH,
            .height = centerHeight
        }
    };

    layout.right = (Zone) {
        .zone = ZONE_RIGHT,
        .bounds = {
            .x = screen.width - SIDE_WIDTH,
            .y = TOP_HEIGHT,
            .width = SIDE_WIDTH,
            .height = centerHeight
        }
    };

    layout.center = (Zone) {
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


void drawCard(
    Screen *screen,
    Zone zone,
    Card card,
    int paddingX,
    int paddingY
) {
    const int MAX_CARD_LENGTH = 5;

    int maxHeight = zone.bounds.height - (paddingY * 2);

    // symbol
    setPixel( screen, ( paddingX + ( MAX_CARD_LENGTH / 2 ) ), ( paddingY + 2 ), card.symbol );

    // horizontal (bordas)
    for (int x = 0; x < MAX_CARD_LENGTH; ++x) {
        int posX = paddingX + x;

        if (x == 0 || x + 1 == MAX_CARD_LENGTH) {
            setPixel(screen, posX, paddingY, '+');
            setPixel(screen, posX, maxHeight, '+');
        } else {
            setPixel(screen, posX, paddingY, '-');
            setPixel(screen, posX, maxHeight, '-');
        }
    }

    // vertical (bordas)
    for (int y = paddingY + 1; y < maxHeight; ++y) {
        setPixel(screen, paddingX, y, '|');
        setPixel(screen, paddingX + MAX_CARD_LENGTH - 1, y, '|');
    }

    // preenchimento do miolo com a cor da carta
    for (int y = paddingY + 1; y < maxHeight; ++y) {
        for (int x = paddingX + 1; x < paddingX + MAX_CARD_LENGTH - 1; ++x) {
            setBackground( screen, x, y, card.color );
        }
    }
}


void drawPlayerHand( Screen *screen, Player *p ) {
    int paddingY = p->zone.bounds.height * 0.2;
    int paddingX = p->zone.bounds.width  * 0.1;

    const int CARD_WIDTH = 5;
    const int CARD_GAP   = 2;

    for ( int i = 0; i < p->cardCount; ++i ) {
        int cardX = paddingX + i * ( CARD_WIDTH + CARD_GAP );

        drawCard( screen, p->zone, p->cards[i], cardX, paddingY );
    }
}


int main() {
	Screen screen = getScreenSize();

	/**
	 * Setup and screen validation
	 */
	{
		if ( screen.height  < ( MIN_SCREEN_HEIGHT - VERTICAL_MARGIN ) ) {
			printf( "[ERROR]: Screen height should be at least %d but was %d.\n", MIN_SCREEN_HEIGHT, screen.height );
			return 1;
		}

		if ( screen.width  < MIN_SCREEN_WIDTH ) {
			printf( "[ERROR]: Screen width should be at least %d but was %d.\n", MIN_SCREEN_WIDTH, screen.width );
			return 1;
		}
	}

	BoardLayout layout = createBoardLayout(screen);

    clearScreen(&screen);

    drawBoardBorder(&screen);

	drawBoard(&screen, layout);

    Card cards[] = {
        {
            .value  = '1',
            .symbol = '1',
            .color  = COLOR_RED
        },
        {
            .value  = '1',
            .symbol = '2',
            .color  = COLOR_GREEN
        },
        {
            .value  = '1',
            .symbol = '7',
            .color  = COLOR_BLUE
        },
    };

    Player p = {
        .id = 1,
        .zone = layout.top,
        .cards = cards,
        .cardCount = 3
    };

    drawPlayerHand( &screen, &p );

    renderScreen(&screen);

    return 0;
}
