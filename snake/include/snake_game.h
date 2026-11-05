#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include "main.h"

typedef enum {
    STATE_IDLE      = 0,
    STATE_PLAYING   = 1,
    STATE_PAUSED    = 2,
    STATE_GAME_OVER = 3
} GameState_t;

void Snake_Init(void);
void Snake_Tick(void);          /* called from timer ISR */
void Snake_ProcessInput(void);  /* called from main loop */
GameState_t Snake_GetState(void);

#endif /* SNAKE_GAME_H */