#include <stdint.h>

#include <zgdk/tilemap.h>
#include <zgdk/tilemap/scroll.h>
#include <zgdk/types.h>

#include <zvb_gfx.h>
#include <zvb_hardware.h>

#include "assets.h"
#include "field.h"
#include "render.h"
#include "zealmine.h"

gfx_context vctx;

static const uint8_t NUMBER_TILES[9] = {
    TILE_EMPTY, TILE_NUM1, TILE_NUM2, TILE_NUM3,
    TILE_NUM4, TILE_NUM5, TILE_NUM6, TILE_NUM7,
    TILE_NUM8
};

/* The HUD digits do not sit in a run: the red "3" is the number tile, so the
   generator packs digits 0..2 and 4..9 tightly and these index the rest. */
static const uint8_t DIGIT_TILES[10] = {
    TILE_DIGIT0, TILE_DIGIT1, TILE_DIGIT2, TILE_DIGIT3, TILE_DIGIT4,
    TILE_DIGIT5, TILE_DIGIT6, TILE_DIGIT7, TILE_DIGIT8, TILE_DIGIT9
};

static const uint8_t CELL_TILES[] = {
    TILE_COVERED, TILE_EMPTY, TILE_FLAG, TILE_MINE,
    TILE_MINE_BLOWN, TILE_MINE_WRONG
};

static uint8_t row_buf[FIELD_MAX_W];
static uint8_t hud_buf[SCREEN_TILES_X];

static uint8_t cell_tile(uint8_t x, uint8_t y)
{
    uint8_t state = field_at(&game.field, x, y);
    if (state == CELL_REVEALED) {
        uint8_t adj = field_adj(&game.field, x, y);
        return NUMBER_TILES[adj > 8 ? 8 : adj];
    }
    return CELL_TILES[state];
}

/* Written most significant digit first, so the group reads left to right the way
   it is numbered. Leading positions keep their zeroes, as a fixed width counter
   is meant to. */
static void put_digits(uint8_t* buf, uint8_t x, int16_t value, uint8_t digits)
{
    uint8_t i;

    if (value < 0) {
        buf[x] = TILE_MINUS;
        x++;
        digits--;
        value = (int16_t)-value;
    }

    for (i = digits; i > 0; i--) {
        buf[x + i - 1] = DIGIT_TILES[(uint8_t)(value % 10)];
        value /= 10;
    }
}

/* Minutes and seconds, each padded to two digits, separated by a colon. */
static void put_clock(uint8_t* buf, uint8_t x, int16_t seconds)
{
    put_digits(buf, x, (int16_t)(seconds / 60), CLOCK_DIGITS);
    buf[x + CLOCK_DIGITS] = TILE_COLON;
    put_digits(buf, x + CLOCK_DIGITS + 1, (int16_t)(seconds % 60), CLOCK_DIGITS);
}

gfx_error render_init(void)
{
    gfx_error err;

    gfx_enable_screen(0);

    err = gfx_initialize(ZVB_CTRL_VID_MODE_GFX_320_8BIT, &vctx);
    if (err != 0)
        return err;

    err = title_assets_load(&vctx);
    if (err != 0)
        return err;

    render_title();
    gfx_enable_screen(1);

    return 0;
}

/* Draws a w x h run of tile indices with its top left corner at (x, y).
   A layer keeps its rows 81 bytes apart and gfx_tilemap_load() copies its
   length straight through the buffer, so a block only survives being handed in
   one row at a time -- a longer copy runs over the padding between rows and
   shears the picture. */
void render_image(uint8_t layer, uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h, const uint8_t* tiles)
{
    uint8_t row;

    for (row = 0; row < h; row++)
        gfx_tilemap_load(
            &vctx, (void*)(tiles + (uint16_t)row * (uint16_t)w), w, layer, x,
            (uint8_t)(y + row));
}

void render_title(void)
{
    tilemap_scroll(LAYER0, 0, 0);
    tilemap_scroll(LAYER1, 0, 0);

    /* The title has no tile of its own for the HUD layer, so it is cleared with
       the blank tile tools/title2tiles.py keeps at index 0 for this purpose. */
    tilemap_fill(&vctx, LAYER1, TILE_TITLE_BLANK, 0, 0, TILEMAP_COLS, TILEMAP_ROWS);

    render_image(LAYER0, 0, 0, SCREEN_TILES_X, SCREEN_TILES_Y,
                 title_assets_tilemap_at(0));
}

void render_start_game(void)
{
    /* The two tilesets disagree about index 0: the title keeps a transparent
       blank there, the board keeps its number "1". Loading the new tiles while
       the title is still drawn therefore covers the whole screen with "1"s
       until the clear below, and the load is slow enough to see. Hide the
       screen for the swap instead. */
    gfx_enable_screen(0);
    assets_load(&vctx);
    render_clear();
    gfx_enable_screen(1);
}

void render_clear(void)
{
    tilemap_scroll(LAYER0, 0, 0);
    tilemap_scroll(LAYER1, 0, 0);
    tilemap_fill(&vctx, LAYER0, TILE_BLANK, 0, 0, TILEMAP_COLS, TILEMAP_ROWS);
    tilemap_fill(&vctx, LAYER1, TILE_BLANK, 0, 0, TILEMAP_COLS, TILEMAP_ROWS);
}

void render_new_game(void)
{
    tilemap_scroll(LAYER0, 0, 0);
    tilemap_scroll(LAYER1, 0, 0);
    tilemap_fill(&vctx, LAYER0, TILE_BLANK, 0, 0, TILEMAP_COLS, TILEMAP_ROWS);
    tilemap_fill(&vctx, LAYER1, TILE_BLANK, 0, 0, TILEMAP_COLS, TILEMAP_ROWS);
    render_field();
    render_hud();
    /* Background pictures, below the board's layer so nothing can cover them:
       the retro divider owns the bottom row, the soldier stands in the margin
       right of the board under the keys hint. */
    render_image(LAYER0, DIVIDER_X, DIVIDER_Y, DIVIDER_COLS, DIVIDER_ROWS,
                 retro_divider_tilemap_at(0));
    render_image(LAYER0, SOLDIER_X, SOLDIER_Y, SOLDIER_COLS, SOLDIER_ROWS,
                 soldier_tilemap_at(0));
    /* On the overlay layer, so the board and the cursor below can never be
       clipped away by it, and so the picture can be cleared by re-running the
       fills above. */
    render_image(LAYER1, KEYS_HINT_COL, KEYS_HINT_ROW,
                 KEYS_HINT_COLS, KEYS_HINT_ROWS, keys_assets_tilemap_at(0));
    render_cursor();
}

/* Called once, when the game ends. The message picture fills the second row
   and the soldier reacts in place, so the next render_new_game() simply
   repaints the neutral pose and the empty row over them. */
void render_game_end(uint8_t won)
{
    if (won) {
        render_image(LAYER1, ENDGAME_X, ENDGAME_Y, ENDGAME_COLS, ENDGAME_ROWS,
                     congratulations_tilemap_at(0));
        render_image(LAYER0, SOLDIER_X, SOLDIER_Y, SOLDIER_COLS, SOLDIER_ROWS,
                     soldier_congratulations_tilemap_at(0));
    } else {
        render_image(LAYER1, ENDGAME_X, ENDGAME_Y, ENDGAME_COLS, ENDGAME_ROWS,
                     oops_tilemap_at(0));
        render_image(LAYER0, SOLDIER_X, SOLDIER_Y, SOLDIER_COLS, SOLDIER_ROWS,
                     soldier_defeat_tilemap_at(0));
    }
}

void render_cell(uint8_t x, uint8_t y)
{
    if (x >= game.field.w || y >= game.field.h)
        return;
    gfx_tilemap_place(
        &vctx, cell_tile(x, y), LAYER0, x + FIELD_LEFT_COL, y + FIELD_TOP_ROW);
}

void render_field(void)
{
    uint8_t x, y;
    for (y = 0; y < game.field.h; y++) {
        for (x = 0; x < game.field.w; x++)
            row_buf[x] = cell_tile(x, y);
        gfx_tilemap_load(
            &vctx, row_buf, game.field.w, LAYER0, FIELD_LEFT_COL, y + FIELD_TOP_ROW);
    }
}

void render_hud(void)
{
    uint8_t i;
    for (i = 0; i < SCREEN_TILES_X; i++) {
        /* The mine count and the clock keep their panel; the banner and the
           two clear tiles around it stay blank, so the row shows only the
           three count tiles and the five clock tiles. */
        hud_buf[i] = (i >= HUD_BANNER_X - 1 && i <= HUD_BANNER_X + HUD_BANNER_COLS)
                       ? TILE_BLANK
                       : TILE_PANEL;
    }
    put_digits(hud_buf, HUD_MINES_X, game.field.mines_left, 3);
    put_clock(hud_buf, HUD_TIMER_X, game.timer);
    gfx_tilemap_load(&vctx, hud_buf, SCREEN_TILES_X, LAYER1, 0, 0);

    /* The whole row was just repainted, so the banner is re-laid over it: the
       mine count moves and the clock ticks, and either redraw must not erase
       it. */
    render_image(LAYER1, HUD_BANNER_X, HUD_BANNER_Y, HUD_BANNER_COLS, HUD_BANNER_ROWS,
                 a_sapper_tilemap_at(0));
}

/* move_cursor() keeps the cursor inside the board and the board fits on screen,
   so the on-screen position needs no clipping check. */
void render_cursor(void)
{
    gfx_tilemap_place(
        &vctx, TILE_CURSOR, LAYER1, game.cursor_x,
        (uint8_t)(game.cursor_y + FIELD_TOP_ROW));
}

void render_cursor_clear(void)
{
    gfx_tilemap_place(
        &vctx, TILE_BLANK, LAYER1, game.cursor_x,
        (uint8_t)(game.cursor_y + FIELD_TOP_ROW));
}
