#ifndef ZEALMINE_ASSETS_H
#define ZEALMINE_ASSETS_H

#include <stdint.h>

#include <zvb_gfx.h>

void __assets__(void);
void __title_assets__(void);
void __keys_assets__(void);
void __a_sapper_assets__(void);
void __congratulations_assets__(void);
void __oops_assets__(void);
void __soldier_congratulations_assets__(void);
void __soldier_defeat_assets__(void);

extern uint8_t _palette_end;
extern uint8_t _palette_start;
extern uint8_t _tiles_end;
extern uint8_t _tiles_start;

extern uint8_t _title_palette_end;
extern uint8_t _title_palette_start;
extern uint8_t _title_tilemap_end;
extern uint8_t _title_tilemap_start;
extern uint8_t _title_tiles_end;
extern uint8_t _title_tiles_start;

extern uint8_t _keys_tilemap_end;
extern uint8_t _keys_tilemap_start;

extern uint8_t _a_sapper_tilemap_end;
extern uint8_t _a_sapper_tilemap_start;

extern uint8_t _retro_divider_tilemap_end;
extern uint8_t _retro_divider_tilemap_start;
extern uint8_t _soldier_tilemap_end;
extern uint8_t _soldier_tilemap_start;

extern uint8_t _congratulations_tilemap_end;
extern uint8_t _congratulations_tilemap_start;
extern uint8_t _oops_tilemap_end;
extern uint8_t _oops_tilemap_start;
extern uint8_t _soldier_congratulations_tilemap_end;
extern uint8_t _soldier_congratulations_tilemap_start;
extern uint8_t _soldier_defeat_tilemap_end;
extern uint8_t _soldier_defeat_tilemap_start;

gfx_error assets_load(gfx_context* ctx);
uint16_t assets_palette_size(void);
uint16_t assets_tiles_size(void);

gfx_error title_assets_load(gfx_context* ctx);
uint16_t title_assets_palette_size(void);
uint16_t title_assets_tiles_size(void);
const uint8_t* title_assets_tilemap_at(uint16_t offset);

/* The keys hint has no palette or tileset of its own: tools/gif2image.py
   merged both into the board's, and this is only the map of where its tiles
   ended up. */
const uint8_t* keys_assets_tilemap_at(uint16_t offset);

/* Same arrangement for the sapper's banner on the HUD row. */
const uint8_t* a_sapper_tilemap_at(uint16_t offset);

/* And for the two board-screen background pictures: the retro divider along
   the bottom row and the soldier right of the board. */
const uint8_t* retro_divider_tilemap_at(uint16_t offset);
const uint8_t* soldier_tilemap_at(uint16_t offset);

const uint8_t* congratulations_tilemap_at(uint16_t offset);
const uint8_t* oops_tilemap_at(uint16_t offset);
const uint8_t* soldier_congratulations_tilemap_at(uint16_t offset);
const uint8_t* soldier_defeat_tilemap_at(uint16_t offset);

#endif