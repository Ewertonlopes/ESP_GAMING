#ifndef GAME_H_
#define GAME_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "axp192.h"
#include "st7735s.h"
#include "fontx.h"
#include "bmpfile.h"
#include "decode_jpeg.h"
#include "decode_png.h"
#include "pngle.h"

typedef struct {
	int states[50];
    
	void (*video)(ST7735_t *dev);
	
	void (*input1)(void);
    void (*input2)(void);
    void (*input3)(void);
} Gam, *Game;

Game game_init(void (*video)(ST7735_t *dev),
               void (*input1)(void),
               void (*input2)(void),
               void (*input3)(void));

bool game_end(Game game);

Game create_HUD(int numGames);

#endif