#ifndef TILESET_LZ77_H
#define TILESET_LZ77_H

#include <stdint.h>

#include <zvb_gfx.h>

/* Decompress a .zts stream into the video board's tileset, the way the game
 * reads the assets tools/gif2tiles.py now emits instead of SDK RLE. */
gfx_error tileset_load_lz77(gfx_context* ctx, const uint8_t* data, uint16_t size,
                            uint16_t from, uint8_t pal_offset, uint8_t opacity);

#endif