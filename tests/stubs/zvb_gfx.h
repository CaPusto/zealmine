#ifndef TEST_ZVB_GFX_H
#define TEST_ZVB_GFX_H
#include <stdint.h>
typedef struct { uint8_t bpp; } gfx_context;
typedef int gfx_error;
#define GFX_SUCCESS 0
#define GFX_INVALID_ARG 1
gfx_error gfx_tileset_load_none(gfx_context*, uint8_t*, uint16_t, uint16_t, uint8_t, uint8_t);
gfx_error gfx_tileset_load_4bit(gfx_context*, uint8_t*, uint16_t, uint16_t, uint8_t, uint8_t);
#endif
