#include "field.h"

#define MINE_SENTINEL 0xff

static uint16_t flood_stack[FIELD_MAX_CELLS];

static uint32_t rng_next(uint32_t* state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x ? x : 0x1234567u;
    return *state;
}

static uint16_t index_of(const Field* f, int16_t x, int16_t y)
{
    return (uint16_t)(y * (int16_t)f->w + x);
}

static uint8_t inside(const Field* f, int16_t x, int16_t y)
{
    return x >= 0 && y >= 0 && x < f->w && y < f->h;
}

static uint8_t is_mine(const Field* f, uint16_t i)
{
    return f->adj[i] == MINE_SENTINEL;
}

uint8_t field_at(const Field* f, uint8_t x, uint8_t y)
{
    if (!inside(f, x, y))
        return CELL_MINE_WRONG;
    return f->state[index_of(f, x, y)];
}

uint8_t field_adj(const Field* f, uint8_t x, uint8_t y)
{
    if (!inside(f, x, y))
        return 0;
    return f->adj[index_of(f, x, y)];
}

uint8_t field_is_mine(const Field* f, uint8_t x, uint8_t y)
{
    if (!inside(f, x, y))
        return 0;
    return is_mine(f, index_of(f, x, y));
}

void field_init(Field* f, uint8_t w, uint8_t h, uint8_t mines, uint32_t seed)
{
    uint16_t i;
    for (i = 0; i < FIELD_MAX_CELLS; i++) {
        f->state[i] = CELL_HIDDEN;
        f->adj[i]   = 0;
    }
    f->w          = w;
    f->h          = h;
    f->mines      = mines;
    f->mines_left = mines;
    f->placed     = 0;
    f->opened     = 0;
    f->last_x     = 0;
    f->last_y     = 0;
    f->rng        = seed ? seed : 0x1234567u;
}

void field_generate(Field* f, uint8_t safe_x, uint8_t safe_y)
{
    uint8_t x, y, dx, dy;
    uint16_t i, total, placed = 0, attempts = 0, max_attempts, flags = 0;

    if (!inside(f, safe_x, safe_y))
        safe_x = safe_y = 0;

    total = (uint16_t)f->w * f->h;

    for (i = 0; i < total; i++)
        if (f->state[i] == CELL_FLAGGED)
            flags++;

    max_attempts = total * 8;
    while (placed < f->mines && attempts < max_attempts) {
        uint16_t cell = (uint16_t)(rng_next(&f->rng) % total);
        attempts++;
        if (is_mine(f, cell))
            continue;
        x = (uint8_t)(cell % f->w);
        y = (uint8_t)(cell / f->w);
        if ((int16_t)x >= (int16_t)safe_x - 1 && (int16_t)x <= (int16_t)safe_x + 1
            && (int16_t)y >= (int16_t)safe_y - 1 && (int16_t)y <= (int16_t)safe_y + 1)
            continue;
        f->adj[cell] = MINE_SENTINEL;
        placed++;
    }
    f->placed     = 1;
    f->mines      = (uint8_t)placed;
    f->mines_left = (int16_t)placed - (int16_t)flags;

    for (i = 0; i < total; i++) {
        uint8_t count = 0;
        if (is_mine(f, i))
            continue;
        x = (uint8_t)(i % f->w);
        y = (uint8_t)(i / f->w);
        for (dy = 0; dy < 3; dy++) {
            for (dx = 0; dx < 3; dx++) {
                int16_t nx = (int16_t)x + dx - 1;
                int16_t ny = (int16_t)y + dy - 1;
                if (dx == 1 && dy == 1)
                    continue;
                if (inside(f, nx, ny) && is_mine(f, index_of(f, nx, ny)))
                    count++;
            }
        }
        f->adj[i] = count;
    }
}

void field_resolve(Field* f, uint8_t won)
{
    uint16_t i, total = (uint16_t)f->w * f->h;
    for (i = 0; i < total; i++) {
        if (is_mine(f, i)) {
            if (won) {
                if (f->state[i] == CELL_HIDDEN)
                    f->state[i] = CELL_MINE;
            } else if (f->state[i] != CELL_MINE_EXPLODED) {
                f->state[i] = CELL_MINE_EXPLODED;
            }
        } else if (f->state[i] == CELL_FLAGGED) {
            f->state[i] = CELL_MINE_WRONG;
        }
    }
}

static void reveal_area(Field* f, uint8_t x, uint8_t y)
{
    uint16_t i  = index_of(f, x, y);
    uint16_t sp = 1;

    f->state[i] = CELL_REVEALED;
    f->opened++;
    flood_stack[0] = i;

    while (sp > 0) {
        uint16_t cell = flood_stack[--sp];
        uint8_t cx    = (uint8_t)(cell % f->w);
        uint8_t cy    = (uint8_t)(cell / f->w);
        uint8_t dx, dy;
        for (dy = 0; dy < 3; dy++) {
            for (dx = 0; dx < 3; dx++) {
                int16_t nx, ny;
                uint16_t ni;
                if (dx == 1 && dy == 1)
                    continue;
                nx = (int16_t)cx + dx - 1;
                ny = (int16_t)cy + dy - 1;
                if (!inside(f, nx, ny))
                    continue;
                ni = index_of(f, nx, ny);
                if (f->state[ni] != CELL_HIDDEN || is_mine(f, ni))
                    continue;
                f->state[ni] = CELL_REVEALED;
                f->opened++;
                if (f->adj[ni] == 0)
                    flood_stack[sp++] = ni;
            }
        }
    }
}

uint8_t field_reveal(Field* f, uint8_t x, uint8_t y)
{
    uint16_t i;

    if (!inside(f, x, y))
        return FIELD_REVEAL_NOOP;

    if (!f->placed)
        field_generate(f, x, y);

    i = index_of(f, x, y);
    if (f->state[i] == CELL_REVEALED)
        return FIELD_REVEAL_ALREADY;
    if (f->state[i] == CELL_FLAGGED)
        return FIELD_REVEAL_NOOP;
    if (is_mine(f, i)) {
        f->state[i] = CELL_MINE_EXPLODED;
        f->last_x   = x;
        f->last_y   = y;
        return FIELD_REVEAL_EXPLODE;
    }

    reveal_area(f, x, y);
    return FIELD_REVEAL_OK;
}

uint8_t field_chord(Field* f, uint8_t x, uint8_t y)
{
    uint16_t i;
    uint8_t dx, dy, flags = 0;
    int16_t nx, ny;

    if (!inside(f, x, y))
        return FIELD_REVEAL_NOOP;

    i = index_of(f, x, y);
    if (f->state[i] != CELL_REVEALED || is_mine(f, i))
        return FIELD_REVEAL_NOOP;

    if (f->adj[i] == 0)
        return FIELD_REVEAL_NOOP;

    for (dy = 0; dy < 3; dy++) {
        for (dx = 0; dx < 3; dx++) {
            if (dx == 1 && dy == 1)
                continue;
            nx = (int16_t)x + dx - 1;
            ny = (int16_t)y + dy - 1;
            if (inside(f, nx, ny) && f->state[index_of(f, nx, ny)] == CELL_FLAGGED)
                flags++;
        }
    }
    if (flags != f->adj[i])
        return FIELD_REVEAL_NOOP;

    for (dy = 0; dy < 3; dy++) {
        for (dx = 0; dx < 3; dx++) {
            uint16_t ni;
            if (dx == 1 && dy == 1)
                continue;
            nx = (int16_t)x + dx - 1;
            ny = (int16_t)y + dy - 1;
            if (!inside(f, nx, ny))
                continue;
            ni = index_of(f, nx, ny);
            if (f->state[ni] != CELL_HIDDEN)
                continue;
            if (is_mine(f, ni)) {
                f->state[ni] = CELL_MINE_EXPLODED;
                f->last_x    = (uint16_t)nx;
                f->last_y    = (uint16_t)ny;
                return FIELD_REVEAL_EXPLODE;
            }
            reveal_area(f, (uint8_t)nx, (uint8_t)ny);
        }
    }
    return FIELD_REVEAL_OK;
}

uint8_t field_flag(Field* f, uint8_t x, uint8_t y)
{
    uint16_t i;

    if (!inside(f, x, y))
        return FIELD_REVEAL_NOOP;

    i = index_of(f, x, y);
    if (f->state[i] != CELL_HIDDEN && f->state[i] != CELL_FLAGGED)
        return FIELD_REVEAL_NOOP;

    if (f->state[i] == CELL_FLAGGED) {
        f->state[i]    = CELL_HIDDEN;
        f->mines_left++;
    } else {
        f->state[i] = CELL_FLAGGED;
        f->mines_left--;
    }
    return FIELD_REVEAL_OK;
}

uint8_t field_revealed(const Field* f)
{
    uint16_t i, total = (uint16_t)f->w * f->h;
    if (!f->placed)
        return 0;
    for (i = 0; i < total; i++)
        if (!is_mine(f, i) && f->state[i] != CELL_REVEALED)
            return 0;
    return 1;
}

uint8_t field_lost(const Field* f)
{
    uint16_t i, total = (uint16_t)f->w * f->h;
    for (i = 0; i < total; i++)
        if (f->state[i] == CELL_MINE_EXPLODED)
            return 1;
    return 0;
}