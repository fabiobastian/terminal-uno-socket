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


void drawZone( Screen *screen) {
	const float verticalPadding   = 0.80;
	const float horizontalPadding = 0.90;

    int width  = screen->width;
    int height = screen->height;

    // Top
    for ( int x = 1; x < width - 1; x++ ) {
        setPixel( screen, x, height - ( height * verticalPadding ), '-' );
    }

    // Bottom
    for ( int x = 1; x < width - 1; x++ ) {
        setPixel( screen, x, height - ( height * 0.20 ), '-' );
    }

    // Right
    for ( int y = 1; y < height-1; y++ ) {
        setPixel( screen, width - ( width * horizontalPadding ), y, '|' );
    }

    // Left
    for ( int y = 1; y < height-1; y++ ) {
        setPixel( screen, width - ( width * 0.10 ), y, '|' );
    }
}


int main() {
	Screen screen = getScreenSize();

	/**
	 * Setup and screen validation
	 */
	{
		//printf( "Screen height: %d\n", screen.height );
		//printf( "Screen width: %d\n", screen.width );

		if ( screen.height  < ( MIN_SCREEN_HEIGHT - VERTICAL_MARGIN ) ) {
			printf( "[ERROR]: Screen height should be at least %d but was %d.\n", MIN_SCREEN_HEIGHT, screen.height );
			return 1;
		}

		if ( screen.width  < MIN_SCREEN_WIDTH ) {
			printf( "[ERROR]: Screen width should be at least %d but was %d.\n", MIN_SCREEN_WIDTH, screen.width );
			return 1;
		}
	}

    clearScreen(&screen);

    drawBoardBorder(&screen);

	drawZone(&screen);

    renderScreen(&screen);

	return 0;
}
