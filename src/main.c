#include <stdint.h>

#include <zos_errors.h>
#include <zos_keyboard.h>
#include <zos_sys.h>
#include <zos_time.h>
#include <zos_video.h>

#include <zgdk/input.h>
#include <zgdk.h>
#include <zgdk/tilemap.h>
#include <zgdk/tilemap/scroll.h>

#include <zvb_gfx.h>
#include <zvb_hardware.h>

#include "field.h"
#include "assets.h"
#include "keys.h"
#include "render.h"
#include "zealmine.h"

Game game;

static uint32_t seed = 0x1234567u;
static uint16_t prev_buttons = 0;
static uint8_t repeat_dir = 0;
static uint8_t repeat_timer = 0;

#define DIR_NONE 0
#define DIR_LEFT 1
#define DIR_RIGHT 2
#define DIR_UP 3
#define DIR_DOWN 4

/* Fold non-deterministic hardware state into a seed. The system's millisecond
   clock advances on its own while the title is shown, and where the video
   beam sits when a read lands depends on exactly which cycle it lands on, so
   both are in-practice unpredictable at the moment the player acts. That gives
   every game -- and every restart -- a genuinely different starting point
   instead of a fixed walk of a sequence. The xorshift steps keep each fresh
   sample from collapsing the whole state. */
static uint32_t entropy_mix(uint32_t v)
{
    uint8_t i;
    zos_time_t now;
    zos_date_t date;

    for (i = 0; i < 6; i++) {
        v ^= v << 13;
        v ^= v >> 17;
        v ^= v << 5;

        if (gettime(0, &now) == ERR_SUCCESS)
            v ^= now.t_millis;

        /* The raster registers only re-latch their high byte when the low one
           was read, so the low byte always comes first. */
        v ^= (uint32_t)zvb_ctrl_vpos_low;
        v ^= (uint32_t)zvb_ctrl_vpos_high << 8;

        /* When a real-time clock exists, its date also separates sessions. */
        if (getdate(&date) == ERR_SUCCESS)
            v ^= (uint32_t)date.d_date
               ^ (uint32_t)date.d_hours << 8
               ^ (uint32_t)date.d_minutes << 16
               ^ (uint32_t)date.d_seconds << 24;
    }
    return v ? v : 0x1234567u;
}

static uint8_t direction_of(uint16_t buttons)
{
    if (buttons & BUTTON_LEFT)
        return DIR_LEFT;
    if (buttons & BUTTON_RIGHT)
        return DIR_RIGHT;
    if (buttons & BUTTON_UP)
        return DIR_UP;
    if (buttons & BUTTON_DOWN)
        return DIR_DOWN;
    return DIR_NONE;
}

static void move_cursor(int8_t dx, int8_t dy)
{
    int16_t x = (int16_t)game.cursor_x + dx;
    int16_t y = (int16_t)game.cursor_y + dy;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= FIELD_COLS)
        x = FIELD_COLS - 1;
    if (y >= FIELD_ROWS)
        y = FIELD_ROWS - 1;

    if (x == game.cursor_x && y == game.cursor_y)
        return;

    render_cursor_clear();
    game.cursor_x = (uint8_t)x;
    game.cursor_y = (uint8_t)y;
    render_cursor();
}

static void handle_repeat(uint16_t buttons)
{
    uint8_t dir = direction_of(buttons);
    int8_t dx = 0, dy = 0;

    if (dir == DIR_NONE) {
        repeat_dir = DIR_NONE;
        return;
    }

    if (dir != repeat_dir) {
        repeat_dir = dir;
        repeat_timer = REPEAT_DELAY;
    } else if (repeat_timer > 0) {
        repeat_timer--;
        return;
    } else {
        repeat_timer = REPEAT_RATE;
    }

    switch (dir) {
        case DIR_LEFT: dx = -1; break;
        case DIR_RIGHT: dx = 1; break;
        case DIR_UP: dy = -1; break;
        case DIR_DOWN: dy = 1; break;
    }
    move_cursor(dx, dy);
}

static void do_reveal(void)
{
    uint8_t result;

    if (field_at(&game.field, game.cursor_x, game.cursor_y) == CELL_REVEALED)
        result = field_chord(&game.field, game.cursor_x, game.cursor_y);
    else
        result = field_reveal(&game.field, game.cursor_x, game.cursor_y);

    if (result == FIELD_REVEAL_OK || result == FIELD_REVEAL_EXPLODE) {
        game.started = 1;
        render_field();
    }
    game.dirty_hud = 1;

    if (result == FIELD_REVEAL_EXPLODE) {
        game.state = GAME_LOST;
        field_resolve(&game.field, 0);
        render_field();
        render_game_end(0);
    } else if (field_revealed(&game.field)) {
        game.state = GAME_WON;
        field_resolve(&game.field, 1);
        render_field();
        render_game_end(1);
    }
}

static void do_flag(void)
{
    field_flag(&game.field, game.cursor_x, game.cursor_y);
    render_cell(game.cursor_x, game.cursor_y);
    game.dirty_hud = 1;
}

void game_new(void)
{
    seed = entropy_mix(seed * 1103515245u + 12345u);
    game.state = GAME_PLAYING;
    game.cursor_x = 0;
    game.cursor_y = 0;
    game.started = 0;
    game.dirty_hud = 0;
    game.timer = 0;
    game.frames = 0;
    repeat_dir = DIR_NONE;
    repeat_timer = 0;

    field_init(&game.field, FIELD_COLS, FIELD_ROWS, FIELD_MINE_COUNT, seed);
    render_new_game();
}

void game_tick(void)
{
    game.frames++;
    if (game.state != GAME_PLAYING || !game.started)
        return;
    if (game.frames % FRAMES_PER_SECOND)
        return;
    if (game.timer < CLOCK_MAX_SECONDS)
        game.timer++;
    game.dirty_hud = 1;
}

/* The cursor stays live after the game ends so the board can be scanned. */
void game_input(uint16_t buttons, uint16_t pressed)
{
    handle_repeat(buttons);
    if ((pressed & BUTTON_B) && game.state == GAME_PLAYING)
        do_reveal();
    if ((pressed & BUTTON_A) && game.state == GAME_PLAYING)
        do_flag();
}

static uint16_t read_input(void)
{
    /* zgdk's input_read() would call its own keyboard_read(), which competes
       with keys_read() for the same raw key stream. */
    return (uint16_t)(controller_read() | keys_read());
}

/* Hand the display back the way it was found. The game took the screen into a
   graphics mode of its own, so without the reset whoever started us would draw
   its text into a mode it never set up and appear to hang. */
static void deinit(void)
{
    keys_deinit();
    tilemap_scroll(LAYER0, 0, 0);
    tilemap_scroll(LAYER1, 0, 0);
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}

/* The title is dismissed by any key, not just the ones the board uses. */
static void wait_any_key(void)
{
    /* Restart leaves the key that asked for it still down, so the state on entry
       counts as already seen: otherwise it would dismiss the title right away. */
    uint16_t prev = read_input();

    for (;;) {
        const uint16_t keys = read_input();

        if ((uint16_t)(keys & (uint16_t)~prev) != 0) {
            /* A held key must not reach the board as a fresh press. */
            prev_buttons = keys;
            return;
        }
        prev = keys;
        msleep(FRAME_MS);
    }
}

int main(void)
{
    uint8_t quitting = 0;

    if (input_init(1) != 0)
        return 1;
    if (keys_init() != 0)
        return 1;

    while (quitting == 0) {
        /* Redrawn every pass, so restarting really does come back to the title. */
        const gfx_error gfx_err = render_init();

        if (gfx_err != 0)
            return 1;

        wait_any_key();

        render_start_game();
        game_new();

        while (quitting == 0) {
            const uint16_t buttons = read_input();
            const uint16_t pressed = (uint16_t)(buttons & (uint16_t)~prev_buttons);

            if (pressed & KEY_QUIT) {
                quitting = 1;
                break;
            }
            if (pressed & KEY_RESTART)
                break;

            prev_buttons = buttons;
            game_input(buttons, pressed);
            game_tick();
            if (game.dirty_hud) {
                render_hud();
                game.dirty_hud = 0;
            }
            msleep(FRAME_MS);
        }
    }

    deinit();

    return 0;
}
