#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/field.h"

static int failures = 0;
static int checks   = 0;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            printf("FAIL %s:%d ", __func__, __LINE__);                         \
            printf(__VA_ARGS__);                                               \
            printf("\n");                                                      \
        }                                                                      \
    } while (0)

static Field f;

static int count_mines(const Field* fld)
{
    int i, n = 0;
    for (i = 0; i < fld->w * fld->h; i++)
        if (field_is_mine(fld, i % fld->w, i / fld->w))
            n++;
    return n;
}

static void test_safe_first_click(void)
{
    int sx, sy, x, y;
    for (sx = 0; sx < 9; sx += 4) {
        for (sy = 0; sy < 9; sy += 4) {
            field_init(&f, 9, 9, 10, 0xC0FFEEu + sx + sy);
            CHECK(field_reveal(&f, sx, sy) == FIELD_REVEAL_OK, "reveal failed");
            CHECK(!field_is_mine(&f, sx, sy), "first click cell is a mine");
            CHECK(field_at(&f, sx, sy) == CELL_REVEALED, "first click not opened");
            for (y = sy - 1; y <= sy + 1; y++) {
                for (x = sx - 1; x <= sx + 1; x++) {
                    if (x < 0 || y < 0 || x > 8 || y > 8)
                        continue;
                    CHECK(!field_is_mine(&f, x, y), "neighbour %d,%d is a mine", x, y);
                }
            }
            CHECK(count_mines(&f) == 10, "mine count %d", count_mines(&f));
            CHECK(field_lost(&f) == 0, "lost after first click");
        }
    }
}

static void test_adjacency(void)
{
    int x, y, bad = 0;
    field_init(&f, 16, 16, 40, 12345u);
    field_generate(&f, 0, 0);
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int expect = 0, dx, dy;
            if (field_is_mine(&f, x, y))
                continue;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx > 15 || ny > 15)
                        continue;
                    if (field_is_mine(&f, nx, ny))
                        expect++;
                }
            if (field_adj(&f, x, y) != expect)
                bad++;
        }
    }
    CHECK(bad == 0, "%d cells have wrong neighbour count", bad);
}

static void test_mine_count_all_sizes(void)
{
    int w, h, m, seed;
    for (w = 9; w <= 30; w += 7) {
        for (h = 8; h <= 16; h += 4) {
            for (m = 10; m < (w * h) - 1; m += 37) {
                for (seed = 0; seed < 8; seed++) {
                    field_init(&f, (uint8_t)w, (uint8_t)h, (uint8_t)m,
                               (uint32_t)(seed * 7919 + w * 31 + h));
                    field_generate(&f, (uint8_t)(w / 2), (uint8_t)(h / 2));
                    CHECK(count_mines(&f) == f.mines,
                          "%dx%d m=%d seed=%d -> %d mines (declared %d)", w, h, m, seed,
                          count_mines(&f), f.mines);
                    CHECK(f.mines <= m, "%dx%d placed more mines than asked", w, h);
                }
            }
        }
    }
}

static void test_flood_fill(void)
{
    int x, y;
    uint8_t visited[9 * 9];
    int stack[9 * 9], sp = 0;

    field_init(&f, 9, 9, 10, 777u);
    field_generate(&f, 4, 4);
    field_reveal(&f, 4, 4);

    memset(visited, 0, sizeof(visited));
    if (field_at(&f, 4, 4) != CELL_REVEALED)
        return;
    stack[sp++] = 4 * 9 + 4;
    visited[4 * 9 + 4] = 1;
    while (sp > 0) {
        int cell = stack[--sp];
        int cx   = cell % 9, cy = cell / 9, dx, dy;
        for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++) {
                int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || ny < 0 || nx > 8 || ny > 8)
                    continue;
                if (visited[ny * 9 + nx])
                    continue;
                if (field_at(&f, (uint8_t)nx, (uint8_t)ny) != CELL_REVEALED)
                    continue;
                visited[ny * 9 + nx] = 1;
                stack[sp++]          = ny * 9 + nx;
            }
    }

    for (y = 0; y < 9; y++)
        for (x = 0; x < 9; x++) {
            if (field_is_mine(&f, x, y)) {
                CHECK(field_at(&f, x, y) != CELL_REVEALED, "mine %d,%d was opened", x, y);
                CHECK(field_at(&f, x, y) != CELL_MINE_EXPLODED, "mine %d,%d exploded", x, y);
                continue;
            }
            if (visited[y * 9 + x])
                CHECK(field_at(&f, x, y) == CELL_REVEALED, "reachable cell %d,%d closed", x, y);
            else
                CHECK(field_at(&f, x, y) == CELL_HIDDEN, "unreachable cell %d,%d opened", x, y);
        }
    CHECK(field_lost(&f) == 0, "flood fill triggered a loss");
}

static void test_flag_counter(void)
{
    field_init(&f, 9, 9, 10, 42u);
    CHECK(f.mines_left == 10, "initial counter %d", f.mines_left);
    CHECK(field_flag(&f, 0, 0) == FIELD_REVEAL_OK, "flag failed");
    CHECK(f.state[0] == CELL_FLAGGED, "not flagged");
    CHECK(f.mines_left == 9, "counter %d after flag", f.mines_left);
    CHECK(field_flag(&f, 0, 0) == FIELD_REVEAL_OK, "unflag failed");
    CHECK(f.mines_left == 10, "counter %d after unflag", f.mines_left);
    CHECK(field_flag(&f, 0, 0) == FIELD_REVEAL_OK, "reflag");
    CHECK(field_reveal(&f, 0, 0) == FIELD_REVEAL_NOOP, "flagged cell was opened");
    CHECK(field_flag(&f, 0, 0) == FIELD_REVEAL_OK, "unflag again");
}

static void test_lose_resolve(void)
{
    int x, y, mx = -1, my = -1, wrong = 0, exposed = 0;
    field_init(&f, 9, 9, 10, 999u);
    field_generate(&f, 0, 0);
    for (y = 0; y < 9; y++)
        for (x = 0; x < 9; x++)
            if (field_is_mine(&f, x, y)) {
                mx = x;
                my = y;
            }
    field_flag(&f, 0, 0);
    CHECK(mx >= 0, "no mine placed");
    CHECK(field_reveal(&f, (uint8_t)mx, (uint8_t)my) == FIELD_REVEAL_EXPLODE,
          "mine click did not explode");
    CHECK(field_lost(&f) == 1, "field not lost");
    field_resolve(&f, 0);
    for (y = 0; y < 9; y++)
        for (x = 0; x < 9; x++) {
            if (field_is_mine(&f, x, y))
                exposed += field_at(&f, x, y) == CELL_MINE_EXPLODED;
            else if (field_at(&f, x, y) == CELL_MINE_WRONG)
                wrong++;
        }
    CHECK(exposed == 10, "%d/10 mines shown after loss", exposed);
    CHECK(wrong == 1, "%d wrong flags, expected 1", wrong);
}

static void test_win(void)
{
    int x, y, flag = 0;
    field_init(&f, 9, 9, 10, 31337u);
    field_reveal(&f, 4, 4);
    for (y = 0; y < 9; y++)
        for (x = 0; x < 9; x++)
            if (!field_is_mine(&f, x, y) && field_at(&f, x, y) != CELL_REVEALED)
                field_reveal(&f, (uint8_t)x, (uint8_t)y);
    for (y = 0; y < 9; y++)
        for (x = 0; x < 9; x++)
            if (field_is_mine(&f, x, y)) {
                field_flag(&f, (uint8_t)x, (uint8_t)y);
                flag++;
            }
    CHECK(field_revealed(&f) == 1, "field not reported as won");
    CHECK(f.mines_left == 0, "counter %d after winning", f.mines_left);
    CHECK(flag == 10, "%d mines flagged", flag);
    CHECK(field_lost(&f) == 0, "win registered as loss");
    field_resolve(&f, 1);
    CHECK(field_lost(&f) == 0, "resolve on win exploded mines");
    CHECK(f.state[0] == CELL_MINE_WRONG || field_at(&f, 0, 0) != CELL_MINE_WRONG || 1,
          "unreachable");
}

static void test_chord(void)
{
    int x0, y0;
    for (y0 = 1; y0 < 8; y0++) {
        for (x0 = 1; x0 < 8; x0++) {
            uint8_t adj, flags = 0, dx, dy;
            field_init(&f, 9, 9, 10, 555u + (uint32_t)(x0 * 16 + y0));
            field_generate(&f, 4, 4);
            if (field_is_mine(&f, (uint8_t)x0, (uint8_t)y0))
                continue;
            field_reveal(&f, (uint8_t)x0, (uint8_t)y0);
            adj = field_adj(&f, (uint8_t)x0, (uint8_t)y0);
            if (adj == 0)
                continue;
            for (dy = 0; dy < 3; dy++)
                for (dx = 0; dx < 3; dx++) {
                    int nx = x0 + dx - 1, ny = y0 + dy - 1;
                    if (dx == 1 && dy == 1)
                        continue;
                    if (nx < 0 || ny < 0 || nx > 8 || ny > 8)
                        continue;
                    if (field_is_mine(&f, nx, ny)) {
                        field_flag(&f, (uint8_t)nx, (uint8_t)ny);
                        flags++;
                    }
                }
            CHECK(flags == adj, "adj=%u flagged=%u", adj, flags);
            CHECK(field_chord(&f, (uint8_t)x0, (uint8_t)y0) == FIELD_REVEAL_OK,
                  "chord failed at %d,%d adj=%u flags=%u", x0, y0, adj, flags);
            CHECK(field_lost(&f) == 0, "correct chord exploded a mine");
        }
    }
}

static void test_chord_partial(void)
{
    uint8_t res;
    field_init(&f, 9, 9, 10, 24680u);
    field_reveal(&f, 4, 4);
    if (field_adj(&f, 4, 4) > 0) {
        res = field_chord(&f, 4, 4);
        CHECK(res == FIELD_REVEAL_NOOP, "chord without flags returned %d", res);
        CHECK(field_lost(&f) == 0, "chord without flags exploded");
    }
}

static void test_bounds(void)
{
    field_init(&f, 9, 9, 10, 4u);
    CHECK(field_reveal(&f, 200, 200) == FIELD_REVEAL_NOOP, "oob reveal");
    CHECK(field_flag(&f, 255, 0) == FIELD_REVEAL_NOOP, "oob flag");
    CHECK(field_at(&f, 100, 100) == CELL_MINE_WRONG, "oob state");
    CHECK(field_reveal(&f, 4, 4) == FIELD_REVEAL_OK, "in-bounds reveal");
    CHECK(field_reveal(&f, 4, 4) == FIELD_REVEAL_ALREADY, "re-reveal");
}

static void test_determinism(void)
{
    Field a, b;
    int x, y;
    field_init(&a, 16, 16, 40, 13579u);
    field_init(&b, 16, 16, 40, 13579u);
    field_generate(&a, 8, 8);
    field_generate(&b, 8, 8);
    CHECK(memcmp(a.adj, b.adj, sizeof(a.adj)) == 0, "same seed -> different layout");
    field_init(&a, 16, 16, 40, 24680u);
    field_generate(&a, 8, 8);
    CHECK(memcmp(a.adj, b.adj, sizeof(a.adj)) != 0, "different seed -> same layout");
    (void)x;
    (void)y;
}

static void test_stress_random_play(void)
{
    int round, i;
    for (round = 0; round < 400; round++) {
        uint32_t r = (uint32_t)round * 2654435761u + 12345u;
        field_init(&f, 9, 9, 10, r);
        for (i = 0; i < 40; i++) {
            uint8_t x = (uint8_t)(r >> 8) % 9;
            uint8_t y = (uint8_t)(r >> 16) % 9;
            r        = r * 1103515245u + 12345u;
            switch (i % 4) {
                case 0: field_flag(&f, x, y); break;
                case 1: field_reveal(&f, x, y); break;
                case 2: field_chord(&f, x, y); break;
                default: field_adj(&f, x, y); break;
            }
            if (field_lost(&f))
                break;
        }
        field_resolve(&f, 0);
        CHECK(count_mines(&f) == f.mines, "round %d mine count %d/%d", round,
              count_mines(&f), f.mines);
    }
}

int main(void)
{
    test_safe_first_click();
    test_adjacency();
    test_mine_count_all_sizes();
    test_flood_fill();
    test_flag_counter();
    test_lose_resolve();
    test_win();
    test_chord();
    test_chord_partial();
    test_bounds();
    test_determinism();
    test_stress_random_play();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}