#include "tileset_loader.h"
#include "tileset_lz77.h"

gfx_error tileset_load_lz77(gfx_context* ctx, const uint8_t* data, uint16_t size,
                          uint16_t from, uint8_t offset, uint8_t opacity,
                          uint8_t packed)
{
    int16_t count;
    uint8_t expand;
    uint8_t* buffer;
    if (!ctx || !data || !size || (!packed && ctx->bpp != 8))
        return GFX_INVALID_ARG;
    expand = packed && ctx->bpp == 8;
    buffer = lz77_buffer();
    lz77_init(data, size);
    while ((count = lz77_read()) > 0) {
        gfx_error err;
        if ((uint8_t)count && (!packed || (uint8_t)count != 128))
            return GFX_INVALID_ARG;
        if (expand) {
            err = gfx_tileset_load_4bit(ctx, buffer, count, from, offset, opacity);
            from += count * 2u;
        } else {
            err = gfx_tileset_load_none(ctx, buffer, count, from, offset, opacity);
            from += count;
        }
        if (err)
            return err;
    }
    return count < 0 ? GFX_INVALID_ARG : GFX_SUCCESS;
}
