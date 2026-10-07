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

set(TILES_GIF ${CMAKE_CURRENT_LIST_DIR}/assets/tiles.gif)
set(TILES_GEN ${CMAKE_CURRENT_LIST_DIR}/tools/gif2image.py)
set(ASSETS_DIR ${CMAKE_BINARY_DIR}/assets)

# gif2zeal.py masks the RGB565 green channel to 5 bits instead of 6, so it is
# replaced by tools/gif2image.py, which produces byte identical tilesets.
#
# Pictures drawn over the board cannot have a tileset of their own -- the video
# board keeps one for the whole screen -- so they share the sheet's palette and
# are merged into its tileset, each leaving a .ztm of the tile indices it landed
# on. Adding another one, such as the win and lose pictures, is one word in
# BOARD_IMAGES.
set(BOARD_IMAGES use_keys_8bit A_SAPPER_MAKES_ONLY_ONE_MISTAKE retro_divider_320x16 soldier_neutral Congratulations Oops soldier_congratulations soldier_defeat)

set(BOARD_IMAGE_ARGS)
set(BOARD_IMAGE_ZTMS)
set(BOARD_IMAGE_GIFS)
foreach(image IN LISTS BOARD_IMAGES)
    set(gif ${CMAKE_CURRENT_LIST_DIR}/assets/${image}.gif)
    list(APPEND BOARD_IMAGE_GIFS ${gif})
    list(APPEND BOARD_IMAGE_ARGS
         -o ${gif}
         -m ${ASSETS_DIR}/${image}.ztm)
    list(APPEND BOARD_IMAGE_ZTMS ${ASSETS_DIR}/${image}.ztm)
endforeach()

add_custom_command(
    OUTPUT ${ASSETS_DIR}/tiles.zts ${ASSETS_DIR}/tiles.ztp ${BOARD_IMAGE_ZTMS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSETS_DIR}
    COMMAND ${Python3_EXECUTABLE} ${TILES_GEN}
            -i ${TILES_GIF}
            -t ${ASSETS_DIR}/tiles.zts
            -p ${ASSETS_DIR}/tiles.ztp
            ${BOARD_IMAGE_ARGS}
    DEPENDS ${TILES_GIF} ${TILES_GEN} ${BOARD_IMAGE_GIFS}
    COMMENT "Converting assets/tiles.gif"
    VERBATIM
)

# The title is a whole screen, so it needs a tilemap of its own on top of the
# palette and tileset, and the tileset has to be trimmed to fit in tiles VRAM.
set(TITLE_GIF ${CMAKE_CURRENT_LIST_DIR}/assets/zeal_mine_title.gif)
set(TITLE_GEN ${CMAKE_CURRENT_LIST_DIR}/tools/title2tiles.py)

add_custom_command(
    OUTPUT ${ASSETS_DIR}/title.ztp ${ASSETS_DIR}/title.zts ${ASSETS_DIR}/title.ztm
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSETS_DIR}
    COMMAND ${Python3_EXECUTABLE} ${TITLE_GEN}
            -i ${TITLE_GIF}
            -p ${ASSETS_DIR}/title.ztp
            -t ${ASSETS_DIR}/title.zts
            -m ${ASSETS_DIR}/title.ztm
    DEPENDS ${TITLE_GIF} ${TITLE_GEN}
    COMMENT "Converting assets/zeal_mine_title.gif"
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
            ${CMAKE_CURRENT_LIST_DIR}/src/zealmine.h ${DIGITS_CHECK}
    COMMENT "Checking number tiles against TILE_NUM* constants"
    VERBATIM
)
add_custom_target(check_digits ALL DEPENDS ${DIGITS_CHECKED})
