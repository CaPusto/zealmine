/* Host proof of LZ77 upload order and native 4bpp and 8bpp VRAM uploads. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tileset_lz77.h"

static uint8_t vram[65536];
static unsigned uploads;
static gfx_error upload_error;

gfx_error gfx_tileset_load_none(gfx_context* ctx, uint8_t* tiles, uint16_t size,
                              uint16_t from, uint8_t offset, uint8_t opacity)
{
    unsigned tile_size = ctx->bpp == 4 ? 128 : 256;
    assert(size == tile_size && from == uploads * tile_size);
    assert(offset == 0 && opacity == 0);
    if (upload_error) return upload_error;
    memcpy(vram + from, tiles, size);
    uploads++;
    return GFX_SUCCESS;
}

static size_t read_file(const char* path, uint8_t* bytes, size_t capacity)
{
    FILE* file = fopen(path, "rb");
    assert(file);
    size_t size = fread(bytes, 1, capacity, file);
    assert(!ferror(file) && fgetc(file) == EOF);
    fclose(file);
    return size;
}

int main(int argc, char** argv)
{
    assert(argc == 4);
    static uint8_t packed[65536], expected[65536];
    size_t size = read_file(argv[2], packed, sizeof(packed));
    size_t pixels = read_file(argv[3], expected, sizeof(expected));
    gfx_context ctx = {(uint8_t)atoi(argv[1])};
    unsigned tile_size = ctx.bpp == 4 ? 128 : 256;
    assert(size > 0 && size < 65536 && pixels % tile_size == 0);
    gfx_error (*load)(gfx_context*, const uint8_t*, uint16_t, uint16_t, uint8_t, uint8_t)
        = strcmp(argv[1], "4") == 0 ? tileset_load_lz77_4bit : tileset_load_lz77;
    assert(load(&ctx, packed, (uint16_t)size, 0, 0, 0) == GFX_SUCCESS);
    assert(uploads == pixels / tile_size);
    assert(memcmp(vram, expected, pixels) == 0);
    uploads = 0;
    upload_error = GFX_INVALID_ARG;
    assert(load(&ctx, packed, (uint16_t)size, 0, 0, 0) == GFX_INVALID_ARG);
    assert(uploads == 0);
    return 0;
}
