#include "Game.h"

Game game_init(void (*video)(ST7735_t *dev),
               void (*input1)(void),
               void (*input2)(void),
               void (*input3)(void)) {

    Game game = (Game)malloc(sizeof(Gam));
    if (game == NULL) {
        return NULL;
    }

	for(int i=0; i<50;i++) game->states[i] = 0;
    
    game->video = video;
    game->input1 = input1;
    game->input2 = input2;
    game->input3 = input3;
    
    return game;
}

bool game_end(Game game) {

	free(game);

	return true;
}

void HUDinput1() {

}

void HUDinput2() {

}

void HUDinput3() {

}

Game create_HUD(int numGames, void (*videooutp)(ST7735_t *dev)) {
	Game hud = game_init(videooutp, HUDinput1, HUDinput2, HUDinput3);
	hud->states[0] = numGames;
}
