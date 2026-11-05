#include "snake_game.h"
#include <string.h>

static SnakeGame_t game;

/* Simple LCG random number generator */
static uint32_t rng_state = 12345;
static uint32_t rng_next(void)
{
    rng_state = rng_state * 1664525UL + 1013904223UL;
    return rng_state;
}

static void spawn_food(void)
{
    bool occupied;
    do {
        occupied = false;
        game.food.col = rng_next() % GRID_COLS;
        game.food.row = rng_next() % GRID_ROWS;
        for (uint8_t i = 0; i < game.length; i++)
        {
            if (game.body[i].col == game.food.col &&
                game.body[i].row == game.food.row)
            {
                occupied = true;
                break;
            }
        }
    } while (occupied);
}

void Snake_Init(void)
{
    memset(&game, 0, sizeof(game));
    game.length     = 3;
    game.dir        = DIR_RIGHT;
    game.score      = 0;
    game.state      = STATE_IDLE;

    /* Start in middle */
    game.body[0].col = 4; game.body[0].row = 0;
    game.body[1].col = 3; game.body[1].row = 0;
    game.body[2].col = 2; game.body[2].row = 0;

    spawn_food();
}

void Snake_Reset(void)
{
    Snake_Init();
    game.state = STATE_PLAYING;
}

void Snake_Tick(void)
{
    if (game.state != STATE_PLAYING) return;

    /* Calculate new head position */
    Point_t new_head = game.body[0];

    switch (game.dir)
    {
        case DIR_RIGHT: new_head.col++; break;
        case DIR_LEFT:  new_head.col--; break;
        case DIR_DOWN:  new_head.row++; break;
        case DIR_UP:    new_head.row--; break;
    }

    /* Wall collision — wrap around on 16x2 grid */
    if (new_head.col >= GRID_COLS) new_head.col = 0;
    if (new_head.col > 127)        new_head.col = GRID_COLS - 1;
    if (new_head.row >= GRID_ROWS) new_head.row = 0;
    if (new_head.row > 127)        new_head.row = GRID_ROWS - 1;

    /* Self collision */
    for (uint8_t i = 0; i < game.length - 1; i++)
    {
        if (game.body[i].col == new_head.col &&
            game.body[i].row == new_head.row)
        {
            game.state = STATE_GAME_OVER;
            return;
        }
    }

    /* Check food */
    bool ate = (new_head.col == game.food.col &&
                new_head.row == game.food.row);

    /* Move body */
    if (!ate)
    {
        /* Shift body */
        for (uint8_t i = game.length - 1; i > 0; i--)
            game.body[i] = game.body[i-1];
    }
    else
    {
        /* Grow */
        if (game.length < MAX_SNAKE)
        {
            for (uint8_t i = game.length; i > 0; i--)
                game.body[i] = game.body[i-1];
            game.length++;
        }
        game.score++;
        rng_state += game.score;
        spawn_food();
    }

    game.body[0] = new_head;
    game.moved_this_tick = true;
}

void Snake_SetDirection(Direction_t dir)
{
    /* Prevent reversing */
    if (game.dir == DIR_RIGHT && dir == DIR_LEFT)  return;
    if (game.dir == DIR_LEFT  && dir == DIR_RIGHT) return;
    if (game.dir == DIR_UP    && dir == DIR_DOWN)  return;
    if (game.dir == DIR_DOWN  && dir == DIR_UP)    return;
    game.dir = dir;
}

void Snake_Pause(void)
{
    if (game.state == STATE_PLAYING)
        game.state = STATE_PAUSED;
}

void Snake_Resume(void)
{
    if (game.state == STATE_PAUSED)
        game.state = STATE_PLAYING;
}

GameState_t Snake_GetState(void)  { return game.state; }
uint16_t    Snake_GetScore(void)  { return game.score; }
uint8_t     Snake_GetLength(void) { return game.length; }
Direction_t Snake_GetDir(void)    { return game.dir; }

/* Expose body for rendering */
Point_t *Snake_GetBody(void)      { return game.body; }
Point_t  Snake_GetFood(void)      { return game.food; }