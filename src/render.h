#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#include <zvb_gfx.h>

extern gfx_context vctx;

gfx_error render_init(void);
/* Draws a w x h run of tile indices with its top left corner at (x, y). Every
   picture the game shows goes through this: the title, the keys hint, and
   later the win and lose notices. */
void render_image(uint8_t layer, uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h, const uint8_t* tiles);
void render_title(void);
void render_start_game(void);
void render_clear(void);
void render_new_game(void);
void render_cell(uint8_t x, uint8_t y);
void render_field(void);
void render_hud(void);
void render_cursor(void);
void render_cursor_clear(void);
void render_game_end(uint8_t won);

#endif
