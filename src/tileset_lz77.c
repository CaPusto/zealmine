#include <stdint.h>
#include <zvb_gfx.h>
#include "tileset_lz77.h"

/* Back-references retain 256 decoded bytes in either native pixel format. */
static uint8_t win[256];

#define LZ77_LOADER tileset_load_lz77
#define LZ77_TILE_BYTES 256
#include "tileset_lz77_impl.h"

/* Keep output-mode selection outside the decoding loop so SDCC retains
 * decoded-byte state in registers. from remains an output VRAM byte offset. */
static gfx_error upload_packed_tile(gfx_context* ctx, uint8_t* tile,
                                   uint16_t count, uint16_t from,
                                   uint8_t offset, uint8_t opacity)
{
    if (ctx->bpp == 8)
        return gfx_tileset_load_4bit(ctx, tile, 128,
            (uint16_t)(from + count * 256u), offset, opacity);
    return gfx_tileset_load_none(ctx, tile, 128,
        (uint16_t)(from + count * 128u), offset, opacity);
}

#define LZ77_LOADER tileset_load_lz77_4bit
#define LZ77_TILE_BYTES 128
#include "tileset_lz77_impl.h"
