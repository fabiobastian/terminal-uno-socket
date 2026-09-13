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

/**
 * ============================================================================
 * Structs
 * ============================================================================
 */
typedef struct {
	int width;
	int height;
	char *buffer;
} Screen;

// @IMPROVE(nathan): so funciona no linux esse carinha aqui, depois preciso encontrar uma
// forma melhor de fazer ele ser dinamico, usando alguma variavel na inicializacao, etc.
Screen getScreenSize() {
	struct winsize w;
	Screen  s;

	// sucesso
	if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &w ) == 0 ) {
		s.width  = w.ws_col;
		s.height = w.ws_row - VERTICAL_MARGIN;
		s.buffer = malloc( w.ws_row * w.ws_col );
		return s;
	}

	s.width  = 0;
	s.height = 0;
	s.buffer = NULL;
	return s;
}


/**
 * Layout Section
 */
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


/**
== Board Layout ==
top
┌─────────────────────────────┐
│                             │
└─────────────────────────────┘

left       center       right
┌────┐    ┌───────┐    ┌────┐
│    │    │       │    │    │
│    │    │       │    │    │
│    │    │       │    │    │
└────┘    └───────┘    └────┘

bottom
┌─────────────────────────────┐
│                             │
└─────────────────────────────┘
 */

int index( Screen *screen, int x, int y ) {
    return y * screen->width + x;
}

void setPixel( Screen *screen, int x, int y, char character ) {
    if ( x < 0 || x >= screen->width ||  y < 0 || y >= screen->height ) {
        printf( "OUT OF BOUNDS: x=%d y=%d screen=%dx%d\n", x, y, screen->width, screen->height );
        return;
    }
    screen->buffer[ y * screen->width + x ] = character;
}

void clearScreen( Screen *screen ) {
    for ( int y = 0; y < screen->height; y++ ) {
        for ( int x = 0; x < screen->width; x++ ) {
            setPixel( screen, x, y, ' ' );
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
    for ( int y = 0; y < screen->height; y++ ) {
        for ( int x = 0; x < screen->width; x++ ) {
            putchar( screen->buffer[ y * screen->width + x ] );
        }
        putchar( '\n' );
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
            for (int px = x; px < x + width; px++) {
                setPixel(screen, px, y, '-');
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
            .width = screen.width - 1,
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

    renderScreen(&screen);

	return 0;
}
