#include <stddef.h>

#include <zos_errors.h>

#include <zgdk/assets.h>

#include <zvb_gfx.h>

#include "assets.h"
#include "tileset_loader.h"

gfx_error assets_load(gfx_context* ctx)
{
    gfx_error err;
    /* Put the largest palette at zero: soldier uploads can use SDK memcpy.
       Tile positions stay unchanged: 96 board, 6 divider, 144 soldier tiles. */
    err = gfx_palette_load(ctx, &_soldier_palette_start,
        (uint16_t)(&_soldier_palette_end - &_soldier_palette_start), 0);
    if (err) return err;
    err = tileset_load_lz77(ctx, &_soldier_tiles_start,
        (uint16_t)(&_soldier_tiles_end - &_soldier_tiles_start), 102 * 256u, 0, 1, 0);
    if (err) return err;
    err = gfx_palette_load(ctx, &_board_palette_start,
        (uint16_t)(&_board_palette_end - &_board_palette_start), 41);
    if (err) return err;
    err = tileset_load_lz77(ctx, &_board_tiles_start,
        (uint16_t)(&_board_tiles_end - &_board_tiles_start), 0, 41, 1, 1);
    if (err) return err;
    err = gfx_palette_load(ctx, &_divider_palette_start,
        (uint16_t)(&_divider_palette_end - &_divider_palette_start), 55);
    if (err) return err;
    err = tileset_load_lz77(ctx, &_divider_tiles_start,
        (uint16_t)(&_divider_tiles_end - &_divider_tiles_start), 96 * 256u, 55, 1, 1);
    if (err) return err;
    return GFX_SUCCESS;
}

uint16_t title_assets_palette_size(void)
{
    return (uint16_t)(&_title_palette_end - &_title_palette_start);
}

uint16_t title_assets_tiles_size(void)
{
    return (uint16_t)(&_title_tiles_end - &_title_tiles_start);
}

const uint8_t* title_assets_tilemap_at(uint16_t offset)
{
    return &_title_tilemap_start + offset;
}

const uint8_t* keys_assets_tilemap_at(uint16_t offset)
{
    return &_keys_tilemap_start + offset;
}

const uint8_t* a_sapper_tilemap_at(uint16_t offset)
{
    return &_a_sapper_tilemap_start + offset;
}

const uint8_t* retro_divider_tilemap_at(uint16_t offset)
{
    return &_retro_divider_tilemap_start + offset;
}

const uint8_t* soldier_tilemap_at(uint16_t offset)
{
    return &_soldier_tilemap_start + offset;
}

const uint8_t* congratulations_tilemap_at(uint16_t offset)
{
    return &_congratulations_tilemap_start + offset;
}

const uint8_t* oops_tilemap_at(uint16_t offset)
{
    return &_oops_tilemap_start + offset;
}

const uint8_t* soldier_congratulations_tilemap_at(uint16_t offset)
{
    return &_soldier_congratulations_tilemap_start + offset;
}

const uint8_t* soldier_defeat_tilemap_at(uint16_t offset)
{
    return &_soldier_defeat_tilemap_start + offset;
}

gfx_error title_assets_load(gfx_context* ctx)
{
    gfx_error err;
    const uint16_t palette_size = title_assets_palette_size();
    const uint16_t tiles_size   = title_assets_tiles_size();

    err = gfx_palette_load(ctx, &_title_palette_start, palette_size, 0);
    if (err)
        return err;

    return tileset_load_lz77(ctx, &_title_tiles_start, tiles_size, 0, 0, 0, 1);
}

void __assets__(void) __naked
{
    INCLUDE_ASSET("board_palette", "assets/board_tiles.ztp");
    INCLUDE_ASSET("board_tiles", "assets/board.zts");
    INCLUDE_ASSET("divider_palette", "assets/divider_tiles.ztp");
    INCLUDE_ASSET("divider_tiles", "assets/divider.zts");
    INCLUDE_ASSET("soldier_palette", "assets/soldier_tiles.ztp");
    INCLUDE_ASSET("soldier_tiles", "assets/soldier.zts");
}

void __title_assets__(void) __naked
{
    INCLUDE_ASSET("title_palette", "assets/title.ztp");
    INCLUDE_ASSET("title_tiles", "assets/title.zts");
    INCLUDE_ASSET("title_tilemap", "assets/title.ztm");
}

void __keys_assets__(void) __naked
{
    INCLUDE_ASSET("keys_tilemap", "assets/use_keys_8bit.ztm");
}

void __a_sapper_assets__(void) __naked
{
    INCLUDE_ASSET("a_sapper_tilemap", "assets/A_SAPPER_MAKES_ONLY_ONE_MISTAKE.ztm");
}

void __retro_divider_assets__(void) __naked
{
    INCLUDE_ASSET("retro_divider_tilemap", "assets/retro_divider_320x16.ztm");
}

void __soldier_assets__(void) __naked
{
    INCLUDE_ASSET("soldier_tilemap", "assets/soldier_neutral.ztm");
}

void __congratulations_assets__(void) __naked
{
    INCLUDE_ASSET("congratulations_tilemap", "assets/Congratulations.ztm");
}

void __oops_assets__(void) __naked
{
    INCLUDE_ASSET("oops_tilemap", "assets/Oops.ztm");
}

void __soldier_congratulations_assets__(void) __naked
{
    INCLUDE_ASSET("soldier_congratulations_tilemap", "assets/soldier_congratulations.ztm");
}

void __soldier_defeat_assets__(void) __naked
{
    INCLUDE_ASSET("soldier_defeat_tilemap", "assets/soldier_defeat.ztm");
}
