#ifndef TILESET_LZ77_H
#define TILESET_LZ77_H

#include <stdint.h>

#include <zvb_gfx.h>

/* Decompress the repo-specific LZ77 .zts stream emitted by the asset builders
 * using tools/tileset_lz77.py, rather than SDK RLE or LZ. */
gfx_error tileset_load_lz77(gfx_context* ctx, const uint8_t* data, uint16_t size,
                            uint16_t from, uint8_t pal_offset, uint8_t opacity);

/* Decode packed 4bpp tiles: native 4bpp upload or SDK expansion into 8bpp. */
gfx_error tileset_load_lz77_4bit(gfx_context* ctx, const uint8_t* data, uint16_t size,
                              uint16_t from, uint8_t pal_offset, uint8_t opacity);

#endif
