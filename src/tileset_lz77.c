#include <stdint.h>
#include <zvb_gfx.h>
#include "tileset_lz77.h"

/* Back-references retain 256 decoded bytes in either native pixel format. */
static uint8_t win[256];

#define LZ77_LOADER tileset_load_lz77
#define LZ77_TILE_BYTES 256
#include "tileset_lz77_impl.h"

#define LZ77_LOADER tileset_load_lz77_4bit
#define LZ77_TILE_BYTES 128
#include "tileset_lz77_impl.h"
