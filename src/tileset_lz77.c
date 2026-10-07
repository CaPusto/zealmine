#include <stdint.h>

#include <zvb_gfx.h>

#include "tileset_lz77.h"

#define ZM_TILE_SIZE 256

gfx_error tileset_load_lz77(gfx_context* ctx, const uint8_t* data, uint16_t size,
                            uint16_t from, uint8_t pal_offset, uint8_t opacity)
{
    /* Rolling window holding the last 256 decoded bytes, which is exactly what
     * a back-reference may point into. Every 256-byte tile is handed to the
     * video board as soon as it completes, so the stream costs no more than a
     * single tile of RAM. Matches may reach back into the previous tile, hence
     * the circular indexing through the 256-byte wrap. */
    static uint8_t win[ZM_TILE_SIZE];
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

        if (j == 0) {
            /* A whole tile just wrapped around the window: give it to the board. */
            gfx_error err = gfx_tileset_load_none(
                ctx, win, ZM_TILE_SIZE,
                (uint16_t)(from + (uint16_t)tile_count * ZM_TILE_SIZE),
                pal_offset, opacity);
            if (err)
                return err;
            tile_count++;
        }
    }

    return GFX_SUCCESS;
}