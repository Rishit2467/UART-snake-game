#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <stdint.h>
#include <stdbool.h>

#define GRID_COLS   16
#define GRID_ROWS    2
#define MAX_SNAKE   32

typedef enum {
    DIR_UP    = 0,
    DIR_DOWN  = 1,
    DIR_LEFT  = 2,
    DIR_RIGHT = 3
} Direction_t;

typedef enum {
    STATE_IDLE      = 0,
    STATE_PLAYING   = 1,
    STATE_PAUSED    = 2,
    STATE_GAME_OVER = 3
} GameState_t;

typedef struct {
    uint8_t col;
    uint8_t row;
} Point_t;

typedef struct {
    Point_t     body[MAX_SNAKE];
    uint8_t     length;
    Direction_t dir;
    Point_t     food;
    uint16_t    score;
    GameState_t state;
    bool        moved_this_tick;
} SnakeGame_t;

void     Snake_Init(void);
void     Snake_Tick(void);
void     Snake_SetDirection(Direction_t dir);
void     Snake_Pause(void);
void     Snake_Resume(void);
void     Snake_Reset(void);
GameState_t Snake_GetState(void);
uint16_t Snake_GetScore(void);
uint8_t  Snake_GetLength(void);
Direction_t Snake_GetDir(void);

#endif