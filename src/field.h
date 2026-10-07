#ifndef FIELD_H
#define FIELD_H

#include <stdint.h>

#define FIELD_MAX_W 30
#define FIELD_MAX_H 16
#define FIELD_MAX_CELLS (FIELD_MAX_W * FIELD_MAX_H)

#define CELL_HIDDEN 0
#define CELL_REVEALED 1
#define CELL_FLAGGED 2
#define CELL_MINE 3
#define CELL_MINE_EXPLODED 4
#define CELL_MINE_WRONG 5

#define FIELD_REVEAL_NONE 0
#define FIELD_REVEAL_OK 1
#define FIELD_REVEAL_EXPLODE 2
#define FIELD_REVEAL_NOOP 3
#define FIELD_REVEAL_ALREADY 4

typedef struct {
    uint8_t w;
    uint8_t h;
    uint8_t mines;
    uint8_t placed;
    uint8_t state[FIELD_MAX_CELLS];
    uint8_t adj[FIELD_MAX_CELLS];
    int16_t mines_left;
    uint16_t opened;
    uint16_t last_x;
    uint16_t last_y;
    uint32_t rng;
} Field;

void field_init(Field* f, uint8_t w, uint8_t h, uint8_t mines, uint32_t seed);
void field_generate(Field* f, uint8_t safe_x, uint8_t safe_y);
uint8_t field_at(const Field* f, uint8_t x, uint8_t y);
uint8_t field_adj(const Field* f, uint8_t x, uint8_t y);
uint8_t field_is_mine(const Field* f, uint8_t x, uint8_t y);
uint8_t field_reveal(Field* f, uint8_t x, uint8_t y);
uint8_t field_chord(Field* f, uint8_t x, uint8_t y);
uint8_t field_flag(Field* f, uint8_t x, uint8_t y);
uint8_t field_revealed(const Field* f);
uint8_t field_lost(const Field* f);
void field_resolve(Field* f, uint8_t won);

#endif