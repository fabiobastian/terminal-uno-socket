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
#include <string.h>


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


void setPixel( Screen *screen, int x, int y, const char *character ) {
    if ( x < 0 || x >= screen->width || y < 0 || y >= screen->height ) {
        printf( "OUT OF BOUNDS: x=%d y=%d screen=%dx%d\n", x, y, screen->width, screen->height );
        return;
    }
    int idx = y * screen->width + x;
    strncpy( screen->buffer[idx].character, character, sizeof(screen->buffer[idx].character) - 1 );
    screen->buffer[idx].character[ sizeof(screen->buffer[idx].character) - 1 ] = '\0';
}


void setPixelChar( Screen *screen, int x, int y, char c ) {
    char buf[2] = { c, '\0' };
    setPixel( screen, x, y, buf );
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
            //screen->buffer[idx].character  = " ";
            strcpy( screen->buffer[idx].character, " " );
            screen->buffer[idx].foreground = COLOR_DEFAULT;
            screen->buffer[idx].background = COLOR_DEFAULT;
        }
    }
    printf( "\033[H\033[2J" );
}


void drawBoardBorder( Screen *screen ) {
    int width  = screen->width;
    int height = screen->height;

    // Top
    for ( int x = 0; x < width; x++ ) {
        setPixel( screen, x, 0, "═" );
    }

    // Bottom
    for ( int x = 0; x < width; x++ ) {
        setPixel( screen, x, height - 1, "═" );
    }

    // Left
    for ( int y = 0; y < height; y++ ) {
        if     ( y == 0 ){         setPixel( screen, 0, y, "╔" );}
        else if ( y == height -1 ) {setPixel( screen, 0, y, "╚" );}
        else {                     setPixel( screen, 0, y, "║" ); }
    }

    // Right
    for (int y = 0; y < height; y++) {
    if (y == 0) {
        setPixel(screen, width - 1, y, "╗");
    }
    else if (y == height - 1) {
        setPixel(screen, width - 1, y, "╝");
    }
    else {
        setPixel(screen, width - 1, y, "║");
    }
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

            printf( "%s", cell.character );
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
                setPixel( screen, px, height, "╼" );
            }
			break;
        case ZONE_BOTTOM:
            for ( int px = x; px < x + width; px++ ) {
                setPixel( screen, px, y, "╼" );
            }
		    break;
        case ZONE_CENTER:
            const int paddingY = 5;
            const int paddingX = 20;

            int left   = x + paddingX;
            int right  = x + width  - paddingX;
            int top    = y + paddingY;
            int bottom = y + height - paddingY;

            // top
            for ( int px = left; px <= right; ++px ) {
                setPixel( screen, px, top, "═" );
            }

            // bottom
            for ( int px = left; px <= right; ++px ) {
                setPixel( screen, px, bottom, "═" );
            }

            // left
            for ( int py = top; py <= bottom; ++py ) {
                if ( py == top ) {
                    setPixel( screen, left, py, "╔" );
                }
                else if ( py == bottom ) {
                    setPixel( screen, left, py, "╚" );
                }
                else {
                    setPixel( screen, left, py, "║" );
                }
            }

            // right
            for ( int py = top; py <= bottom; ++py ) {
                if ( py == top) {
                    setPixel( screen, right, py, "╗" );
                }
                else if ( py == bottom ) {
                    setPixel( screen, right, py, "╝" );
                } else {
                    setPixel( screen, right, py, "║" );
                }
            }
            break;
	}
}


void drawBoard( Screen *screen, BoardLayout layout ) {
	drawZoneBorder( screen, layout.top );
	drawZoneBorder( screen, layout.bottom );
	drawZoneBorder( screen, layout.center );
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


void drawCard( Screen *screen, Card card, int x, int y ) {
    int bottom = y + CARD_HEIGHT - 1;

    // symbol
    setPixelChar( screen, x + (CARD_WIDTH / 2), y + 2, card.symbol );

    // horizontal (bordas)
    for (int i = 0; i < CARD_WIDTH; ++i) {
        int posX = x + i;
        if ( i == 0 ) {
            setPixel( screen, posX, y, "╭" );
        } else if ( i + 1 == CARD_WIDTH ) {
            setPixel( screen, posX, bottom, "╯" );
        } else {
            setPixel( screen, posX, y, "─" );
            setPixel( screen, posX, bottom, "─" );
        }
    }

    // vertical (bordas)
    for (int py = y + 1; py < bottom; ++py) {
        setPixel(screen, x, py, "│");
        setPixel(screen, x + CARD_WIDTH - 1, py, "│");
    }

    // preenchimento do miolo
    for (int py = y + 1; py < bottom; ++py) {
        for (int px = x + 1; px < x + CARD_WIDTH - 1; ++px) {
            setBackground( screen, px, py, card.color );
        }
    }
}

void drawPlayerHand( Screen *screen, Player *p ) {
    Zone zone = p->zone;
    int paddingY = zone.bounds.y + zone.bounds.height * 0.2;
    int paddingX = zone.bounds.x + zone.bounds.width  * 0.1;

    const int CARD_GAP = 2;

    for ( int i = 0; i < p->cardCount; ++i ) {
        int cardX = paddingX + i * ( CARD_WIDTH + CARD_GAP );
        drawCard( screen, p->cards[i], cardX, paddingY );
    }
}

void drawDiscardPile( Screen *screen, Zone zone, Card card ) {
    int x = zone.bounds.x + (zone.bounds.width  - CARD_WIDTH)  / 2;
    int y = zone.bounds.y + (zone.bounds.height - CARD_HEIGHT) / 2;

    drawCard( screen, card, x, y );
}


void drawCardDeck( Screen screen, Zone zone, Card card ) {
    //TODO implement
}


int main() {
    Card lastCard = {
        .value  = '1',
        .symbol = '1',
        .color  = COLOR_RED
    };

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

	BoardLayout layout = createBoardLayout( screen );

    Player p = {
        .id = 1,
        .zone = layout.top,
        .cards = cards,
        .cardCount = 3
    };

    clearScreen( &screen );

    drawBoardBorder( &screen );

    drawBoard( &screen, layout );

    drawDiscardPile( &screen, layout.center, lastCard );

    drawPlayerHand( &screen, &p );

    renderScreen( &screen );

    return 0;
}
