#include <stddef.h>

#include <zos_errors.h>

#include <zgdk/assets.h>

#include <zvb_gfx.h>

#include "assets.h"
#include "tileset_lz77.h"

uint16_t assets_palette_size(void)
{
    return (uint16_t)(&_palette_end - &_palette_start);
}

uint16_t assets_tiles_size(void)
{
    return (uint16_t)(&_tiles_end - &_tiles_start);
}

gfx_error assets_load(gfx_context* ctx)
{
    gfx_error err;
    const uint16_t palette_size = assets_palette_size();
    const uint16_t tiles_size   = assets_tiles_size();

    err = gfx_palette_load(ctx, &_palette_start, palette_size, 0);
    if (err)
        return err;

    return tileset_load_lz77(ctx, &_tiles_start, tiles_size, 0, 0, 0);
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

    return tileset_load_lz77(ctx, &_title_tiles_start, tiles_size, 0, 0, 0);
}

void __assets__(void) __naked
{
    INCLUDE_ASSET("palette", "assets/tiles.ztp");
    INCLUDE_ASSET("tiles", "assets/tiles.zts");
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