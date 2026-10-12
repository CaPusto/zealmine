/* Intentionally included twice: compile fixed tile sizes so SDCC can keep
 * decoding-loop values in registers. The two entry points share one window.
 * No runtime format branch is needed in the decoder. */
gfx_error LZ77_LOADER(gfx_context* ctx, const uint8_t* data, uint16_t size,
                            uint16_t from, uint8_t pal_offset, uint8_t opacity)
{
    uint16_t i = 0;
    uint8_t j = 0;
    uint16_t tile_count = 0;

    while (i < size) {
        uint8_t cmd = data[i++];
        uint8_t len;

        if (cmd & 0x80) {
            if (cmd & 0x40) {
                /* Back-reference: (cmd & 0x3F) + 3 bytes from offset + 1. */
                uint16_t offset = data[i++] + 1;
                len = (cmd & 0x3F) + 3;
                while (len--) {
                    win[j] = win[(uint8_t)(j - offset)];
                    j++;
                }
            } else {
                /* RLE run: (cmd & 0x3F) + 1 copies of the next byte. */
                uint8_t value = data[i++];
                len = (cmd & 0x3F) + 1;
                while (len--) {
                    win[j++] = value;
                }
            }
        } else {
            /* Literal run: cmd + 1 raw bytes. */
            len = cmd + 1;
            while (len--) {
                win[j++] = data[i++];
            }
        }

#if LZ77_TILE_BYTES == 128
        if ((j & 0x7F) == 0) {
#else
        if (j == 0) {
#endif
            /* Keep 256 bytes of history; select output format only at tile upload. */
#if LZ77_TILE_BYTES == 128
            uint8_t* tile = win + (j == 0 ? 128 : 0);
#else
            uint8_t* tile = win;
#endif
#if LZ77_TILE_BYTES == 128
            gfx_error err = upload_packed_tile(ctx, tile, tile_count, from,
                                               pal_offset, opacity);
#else
            gfx_error err = gfx_tileset_load_none(
                ctx, tile, LZ77_TILE_BYTES,
                (uint16_t)(from + tile_count * LZ77_TILE_BYTES),
                pal_offset, opacity);
#endif
            if (err)
                return err;
            tile_count++;
        }
    }

    return GFX_SUCCESS;
}

#undef LZ77_LOADER
#undef LZ77_TILE_BYTES
