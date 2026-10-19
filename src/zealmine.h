#ifndef ZEALMINE_H
#define ZEALMINE_H

#include <stdint.h>

#include "field.h"

#define SCREEN_TILES_X 20
#define SCREEN_TILES_Y 15
#define HUD_ROWS 1

/* The board sits one blank row below the HUD. */
#define FIELD_GAP_ROWS 1
#define FIELD_TOP_ROW (HUD_ROWS + FIELD_GAP_ROWS)
#define FIELD_LEFT_COL 0

/* One fixed board size: 14 cells across, 12 cells down. It occupies columns
   0..13 and rows 2..13, so it fits on the 20x15 screen with room to spare and
   neither layer ever needs scrolling. */
#define FIELD_COLS 14
#define FIELD_ROWS 12
#define FIELD_MINE_COUNT 30

/* The picture of the controls fills the margin to the right of the board: its
   left edge is the first column past the board, the 15th counting from the
   left, and its top edge is the board's own first row, the 3rd from the top.
   Six columns wide, it runs exactly to the right edge of the screen. */
#define KEYS_HINT_COL (FIELD_LEFT_COL + FIELD_COLS)
#define KEYS_HINT_ROW FIELD_TOP_ROW
#define KEYS_HINT_COLS 6
#define KEYS_HINT_ROWS 4

#define TILEMAP_COLS 80
#define TILEMAP_ROWS 40

/* Board atlas has 16 columns: colored digits 1..8 at 0..7, red HUD digits
   0..9 at 16..25. Other game tiles occupy 8..9 and 48..56 around the controls
   graphic. Constants must follow the authored layout. */
#define TILE_NUM1 0
#define TILE_NUM2 1
#define TILE_NUM3 2
#define TILE_NUM4 3
#define TILE_NUM5 4
#define TILE_NUM6 5
#define TILE_NUM7 6
#define TILE_NUM8 7
#define TILE_EMPTY 8
#define TILE_PANEL 9
#define TILE_COLON 48
#define TILE_COVERED 49
#define TILE_FLAG 50
#define TILE_MINE 51
#define TILE_MINE_BLOWN 52
#define TILE_MINE_WRONG 53
#define TILE_DIGIT0 16
#define TILE_DIGIT1 17
#define TILE_DIGIT2 18
#define TILE_DIGIT3 19
#define TILE_DIGIT4 20
#define TILE_DIGIT5 21
#define TILE_DIGIT6 22
#define TILE_DIGIT7 23
#define TILE_DIGIT8 24
#define TILE_DIGIT9 25
#define TILE_CURSOR 54
/* Reserve the final tile slot for an SDK-generated transparent blank in both
   title and gameplay modes. Artwork must stay below TILE_BLANK. */
#define TILE_BLANK 255
#define TILE_MINUS 56

#define TILE_COUNT 57

/* The HUD holds the mine count on the left three tiles and the clock on the
   right five, as "MM:SS": two digits, the colon, two more. Everything between
   them is the sapper's ten-tile banner, surrounded by two clear tiles, so the
   row never draws a panel where the old fixed-width middle used to be. */
#define HUD_MINES_X 0
#define HUD_TIMER_X 15
#define CLOCK_DIGITS 2
#define HUD_BANNER_X 4
#define HUD_BANNER_Y 0
#define HUD_BANNER_COLS 10
#define HUD_BANNER_ROWS 1

/* Background pictures on the board screens: the retro divider along the
   bottom row and the soldier standing in the margin right of the board, its
   top-left tile on the 7th row (that is, six rows down from the top) in the
   15th cell across, above the bottom row that the divider owns. */
#define DIVIDER_X 0
#define DIVIDER_Y (SCREEN_TILES_Y - 1)
#define DIVIDER_COLS SCREEN_TILES_X
#define DIVIDER_ROWS 1
#define SOLDIER_X (FIELD_LEFT_COL + FIELD_COLS)
#define SOLDIER_Y 6
#define SOLDIER_COLS 6
#define SOLDIER_ROWS 8

/* How the end of a game announces itself: the message picture on the second
   row, centred across fourteen tiles, and the soldier right of the board
   swapping its neutral pose for one that matches the result, in place. */
#define ENDGAME_X ((SCREEN_TILES_X - ENDGAME_COLS) / 2)
#define ENDGAME_Y HUD_ROWS
#define ENDGAME_COLS 14
#define ENDGAME_ROWS 1

/* The clock counts in seconds and the HUD shows two of each, so it tops out
   where the display does rather than rolling over into a wrong minute. */
#define CLOCK_MAX_SECONDS 5999

#define GAME_PLAYING 0
#define GAME_WON 1
#define GAME_LOST 2

#define FRAME_MS 16
#define FRAMES_PER_SECOND (1000 / FRAME_MS)

/* Gamepad buttons the board itself leaves unused, reused for the two keyboard
   shortcuts: L is Q, R is R. See src/keys.c. */
#define KEY_QUIT 0x0400
#define KEY_RESTART 0x0800

/* Set whenever any key at all is down, for screens that are dismissed by any
   key rather than by one of the mapped ones. */
#define KEY_ANY 0x1000

#define REPEAT_DELAY 14
#define REPEAT_RATE 3

typedef struct {
    Field field;
    uint8_t state;
    uint8_t cursor_x;
    uint8_t cursor_y;
    uint8_t started;
    uint8_t dirty_hud;
    uint16_t timer;
    uint16_t frames;
} Game;

extern Game game;

void game_new(void);
void game_tick(void);
void game_input(uint16_t buttons, uint16_t pressed);

#endif
