#ifndef TILESET_LOADER_H
#define TILESET_LOADER_H

#include <stdint.h>
#include <zvb_gfx.h>

/* Load byte-compressed tiles; packed selects 4bpp source data. Context selects
 * native 4bpp storage or SDK expansion into 8bpp VRAM. from is a VRAM offset. */
gfx_error tileset_load_lz77(gfx_context* ctx, const uint8_t* data, uint16_t size,
                          uint16_t from, uint8_t offset, uint8_t opacity,
                          uint8_t packed);

#endif
