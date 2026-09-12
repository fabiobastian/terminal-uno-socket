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


/** 
 * ============================================================================
 * Constants
 * ============================================================================
 */
#define MIN_SCREEN_HEIGHT 50
#define MIN_SCREEN_WIDTH  200


/**
 * ============================================================================
 * Structs
 * ============================================================================
 */
struct Screen {
	int width;
	int height;
};


// @IMPROVE(nathan): so funciona no linux esse carinha aqui, depois preciso encontrar uma
// forma melhor de fazer ele ser dinamico, usando alguma variavel na inicializacao, etc.
struct Screen getScreenSize() {
	struct winsize w;
	struct Screen  s;

	// sucesso
	if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &w ) == 0 ) {
		s.width  = w.ws_row;
		s.height = w.ws_col;
		return s;
	}

	s.width  = 0;
	s.height = 0;
	return s;
}


char *renderBoard( struct Screen screen ) {
	return NULL;
}

int main() {
	struct Screen screen = getScreenSize();

	/**
	 * Setup and screen validation
	 */
	{
		printf( "Screen height: %d\n", screen.height );
		printf( "Screen width: %d\n", screen.width );

		if ( screen.height  < MIN_SCREEN_HEIGHT ) {
			printf( "[ERROR]: Screen height should be at least %d but was %d\n.", screen.height,screen.height );
			return 1;
		}

		if ( screen.width  < MIN_SCREEN_WIDTH ) {
			printf( "[ERROR]: Screen width should be at least %d but was %d\n.", screen.width, screen.width );
			return 1;
		}
	}

	char *board = renderBoard( screen );

	return 0;
}
