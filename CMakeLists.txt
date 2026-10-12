cmake_minimum_required(VERSION 3.16)

set(APP_NAME "zealmine")
set(target_name "zealmine")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

# -------------------------------
# Toolchain
# -------------------------------
set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

# -------------------------------
# Project
# -------------------------------
project(${target_name} C ASM)

# -------------------------------
# Executable
# -------------------------------
add_executable(${target_name}
    src/main.c
    src/assets.c
    src/keys.c
    src/render.c
    src/field.c
    src/tileset_lz77.c
)

target_include_directories(${target_name} PRIVATE src)

# -------------------------------
# SDKs
# -------------------------------
zos_use(zvb)
find_package(ZVB_SDK REQUIRED)
target_link_libraries(${target_name} PUBLIC zvb)

zos_use(coreutils)
find_package(COREUTILS REQUIRED)
target_link_libraries(${target_name} PUBLIC core)

zos_use(zgdk)
find_package(ZGDK REQUIRED)
target_link_libraries(${target_name} PUBLIC zgdk)

target_compile_options(${target_name} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:--nostdlib>
)

# -------------------------------
# Assets
# -------------------------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(ASSETS_DIR ${CMAKE_BINARY_DIR}/assets)
set(TILESET_CODEC ${CMAKE_CURRENT_LIST_DIR}/tools/tileset_lz77.py)
set(BOARD_ATLAS ${CMAKE_CURRENT_LIST_DIR}/assets/board_tiles.gif)
set(BOARD_TILESET ${CMAKE_CURRENT_LIST_DIR}/assets/board_tiles.tsx)

# The authored atlas owns tile indices shared by gameplay and picture maps.
gif2zeal(${target_name} BIT 8 COLORS 256 FILES ${BOARD_ATLAS})
set(BOARD_RAW ${ASSETS_DIR}/board_tiles.zts)
set(BOARD_PALETTE ${ASSETS_DIR}/board_tiles.ztp)

set(BOARD_IMAGES use_keys_8bit A_SAPPER_MAKES_ONLY_ONE_MISTAKE retro_divider_320x16 soldier_neutral Congratulations Oops soldier_congratulations soldier_defeat)
set(BOARD_IMAGE_ZTMS)
foreach(image IN LISTS BOARD_IMAGES)
    set(tmx ${CMAKE_CURRENT_LIST_DIR}/assets/${image}.tmx)
    set(ztm ${ASSETS_DIR}/${image}.ztm)
    tiled2zeal(${target_name} LAYER 1 FILES ${tmx})
    # SDK conversion depends on the TMX; also track its authored tileset.
    add_custom_command(OUTPUT ${ztm} APPEND
        DEPENDS ${BOARD_TILESET} ${BOARD_ATLAS})
    list(APPEND BOARD_IMAGE_ZTMS ${ztm})
endforeach()

add_custom_command(
    OUTPUT ${ASSETS_DIR}/tiles.zts ${ASSETS_DIR}/tiles.ztp
    COMMAND ${Python3_EXECUTABLE} ${TILESET_CODEC}
            -i ${BOARD_RAW} -o ${ASSETS_DIR}/tiles.zts
    COMMAND ${CMAKE_COMMAND} -E copy ${BOARD_PALETTE} ${ASSETS_DIR}/tiles.ztp
    DEPENDS ${BOARD_RAW} ${BOARD_PALETTE} ${TILESET_CODEC}
    COMMENT "Packing SDK board tiles"
    VERBATIM
)

# The source title already fits VRAM and reserves palette index 0. SDK
# conversion is lossless; only blank-tile indexing and LZ77 packing remain.
set(TITLE_GIF ${CMAKE_CURRENT_LIST_DIR}/assets/zeal_mine_title.gif)
gif2zeal(${target_name} BIT 8 COLORS 256 UNIQUE TILEMAP FILES ${TITLE_GIF})
set(TITLE_RAW ${ASSETS_DIR}/zeal_mine_title)
set(TITLE_PACKER ${CMAKE_CURRENT_LIST_DIR}/tools/pack_title.py)

add_custom_command(
    OUTPUT ${ASSETS_DIR}/title.ztp ${ASSETS_DIR}/title.zts ${ASSETS_DIR}/title.ztm
    COMMAND ${Python3_EXECUTABLE} ${TITLE_PACKER}
            -i ${TITLE_RAW}
            -o ${ASSETS_DIR}/title
    DEPENDS ${TITLE_RAW}.zts ${TITLE_RAW}.ztp ${TITLE_RAW}.ztm
            ${TITLE_PACKER} ${TILESET_CODEC}
    COMMENT "Packing SDK title assets"
    VERBATIM
)

add_custom_target(zealmine_tiles_asset ALL
    DEPENDS ${ASSETS_DIR}/tiles.zts ${ASSETS_DIR}/tiles.ztp ${BOARD_IMAGE_ZTMS}
            ${ASSETS_DIR}/title.ztp ${ASSETS_DIR}/title.zts ${ASSETS_DIR}/title.ztm)
add_dependencies(${target_name} zealmine_tiles_asset)

# assets.c pulls the assets in with .incbin, which CMake cannot see, so the
# translation unit is forced to rebuild whenever the generated files change.
set_source_files_properties(src/assets.c PROPERTIES
    OBJECT_DEPENDS "${ASSETS_DIR}/tiles.zts;${ASSETS_DIR}/tiles.ztp;${BOARD_IMAGE_ZTMS};${ASSETS_DIR}/title.ztp;${ASSETS_DIR}/title.zts;${ASSETS_DIR}/title.ztm")

# -------------------------------
# Convert to binary
# -------------------------------
zgdk_add_outputs(${target_name} KEEP_BIN)

# The number tiles must stay paired with the TILE_NUM* constants in zealmine.h;
# make a scrambled pair (as once happened for TILE_NUM2/TILE_NUM3) break the
# build instead of showing wrong digits on the board.
set(DIGITS_CHECK ${CMAKE_CURRENT_LIST_DIR}/tools/check_digits.py)
set(DIGITS_CHECKED ${CMAKE_BINARY_DIR}/.digits_checked)
add_custom_command(
    OUTPUT ${DIGITS_CHECKED}
    COMMAND ${Python3_EXECUTABLE} ${DIGITS_CHECK}
            --zts ${ASSETS_DIR}/tiles.zts
            --ztp ${ASSETS_DIR}/tiles.ztp
            --header ${CMAKE_CURRENT_LIST_DIR}/src/zealmine.h
    COMMAND ${CMAKE_COMMAND} -E touch ${DIGITS_CHECKED}
    DEPENDS ${ASSETS_DIR}/tiles.zts ${ASSETS_DIR}/tiles.ztp
            ${CMAKE_CURRENT_LIST_DIR}/src/zealmine.h ${DIGITS_CHECK} ${TILESET_CODEC}
    COMMENT "Checking number tiles against TILE_NUM* constants"
    VERBATIM
)
add_custom_target(check_digits ALL DEPENDS ${DIGITS_CHECKED})
