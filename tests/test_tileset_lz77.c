/* Host proof of LZ77 upload order and native 4bpp and 8bpp VRAM uploads. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tileset_loader.h"

static uint8_t vram[65536];
static unsigned uploads, uploaded;
static gfx_error upload_error;

gfx_error gfx_tileset_load_none(gfx_context* ctx, uint8_t* tiles, uint16_t size,
                              uint16_t from, uint8_t offset, uint8_t opacity)
{
    unsigned tile_size = ctx->bpp == 4 ? 128 : 256;
    assert(size <= 256 && size % tile_size == 0 && from == uploaded);

    if (upload_error) return upload_error;
    for (unsigned i = 0; i < size; i++)
        vram[from + i] = (opacity && tiles[i] == 0) ? 0 : tiles[i] + offset;
    uploads++;
    uploaded += size;
    return GFX_SUCCESS;
}

gfx_error gfx_tileset_load_4bit(gfx_context* ctx, uint8_t* tiles, uint16_t size,
                               uint16_t from, uint8_t offset, uint8_t opacity)
{
    assert(ctx->bpp == 8 && size <= 256 && size % 128 == 0 && from == uploaded);
    if (upload_error) return upload_error;
    for (unsigned i = 0; i < size * 2u; i++) {
        uint8_t index = (i & 1) ? tiles[i / 2] & 15 : tiles[i / 2] >> 4;
        vram[from + i] = (opacity && index == 0) ? 0 : index + offset;
    }
    uploads++;
    uploaded += size * 2u;
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
    assert(argc == 4 || argc == 6);
    uint8_t offset = argc == 6 ? (uint8_t)atoi(argv[4]) : 0;
    uint8_t opacity = argc == 6 ? (uint8_t)atoi(argv[5]) : 0;
    static uint8_t packed[65536], expected[65536];
    size_t size = read_file(argv[2], packed, sizeof(packed));
    size_t pixels = read_file(argv[3], expected, sizeof(expected));
    gfx_context ctx = {(uint8_t)(strcmp(argv[1], "expand") == 0 ? 8 : atoi(argv[1]))};
    unsigned tile_size = ctx.bpp == 4 ? 128 : 256;
    assert(size > 0 && size < 65536 && pixels % tile_size == 0);
    uint8_t is_packed = strcmp(argv[1], "8") != 0;
    assert(tileset_load_lz77(&ctx, packed, (uint16_t)size, 0, offset, opacity, is_packed) == GFX_SUCCESS);
    assert(uploaded == pixels);
    assert(memcmp(vram, expected, pixels) == 0);
    uploads = uploaded = 0;
    upload_error = GFX_INVALID_ARG;
    assert(tileset_load_lz77(&ctx, packed, (uint16_t)size, 0, offset, opacity, is_packed) == GFX_INVALID_ARG);
    assert(uploads == 0);
    return 0;
}
