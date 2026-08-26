# imgui
find_package(PkgConfig REQUIRED)
pkg_check_modules(sdl2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(freetype2 REQUIRED IMPORTED_TARGET freetype2)

add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp

    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_sdlrenderer2.cpp

    imgui/misc/carousel/imgui_carousel.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
    imgui/misc/freetype/imgui_freetype.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/carousel
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/freetype
)
target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/custom_imconfig.h")

target_link_libraries(imgui PUBLIC PkgConfig::sdl2 PkgConfig::freetype2)

# glaze
add_library(glaze INTERFACE)

target_include_directories(glaze INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/glaze/include)

# mocha
add_library(mocha STATIC
    libmocha/source/disc_interface.c
    libmocha/source/devoptab/devoptab_fsa.cpp
    libmocha/source/devoptab/devoptab_fsa_link.cpp
    libmocha/source/devoptab/devoptab_fsa_mkdir.cpp
    libmocha/source/devoptab/devoptab_fsa_truncate.cpp
    libmocha/source/devoptab/devoptab_fsa_rename.cpp
    libmocha/source/devoptab/devoptab_fsa_write.cpp
    libmocha/source/devoptab/devoptab_fsa_rmdir.cpp
    libmocha/source/devoptab/devoptab_fsa_dirnext.cpp
    libmocha/source/devoptab/devoptab_fsa_statvfs.cpp
    libmocha/source/devoptab/devoptab_fsa_utimes.cpp
    libmocha/source/devoptab/devoptab_fsa_chdir.cpp
    libmocha/source/devoptab/devoptab_fsa_stat.cpp
    libmocha/source/devoptab/devoptab_fsa_fstat.cpp
    libmocha/source/devoptab/devoptab_fsa_utils.cpp
    libmocha/source/devoptab/devoptab_fsa_seek.cpp
    libmocha/source/devoptab/devoptab_fsa_open.cpp
    libmocha/source/devoptab/devoptab_fsa_chmod.cpp
    libmocha/source/devoptab/devoptab_fsa_dirreset.cpp
    libmocha/source/devoptab/devoptab_fsa_fsync.cpp
    libmocha/source/devoptab/devoptab_fsa_dirclose.cpp
    libmocha/source/devoptab/devoptab_fsa_diropen.cpp
    libmocha/source/devoptab/devoptab_fsa_close.cpp
    libmocha/source/devoptab/devoptab_fsa_read.cpp
    libmocha/source/devoptab/devoptab_fsa_fchmod.cpp
    libmocha/source/devoptab/devoptab_fsa_unlink.cpp
    libmocha/source/fsa.cpp
    libmocha/source/utils.cpp
)

target_include_directories(mocha PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libmocha/include
)

# quirc
add_library(quirc STATIC
    quirc/lib/decode.c
    quirc/lib/identify.c
    quirc/lib/quirc.c
    quirc/lib/version_db.c
)

target_include_directories(quirc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/quirc/lib
)

# hips
add_library(hips INTERFACE)

target_include_directories(hips INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/hips/include)

# cafe_glyphs
add_library(cafe_glyphs INTERFACE)

target_include_directories(cafe_glyphs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cafe_glyphs/include)

# wiiu-stdout
add_library(wiiu-stdout STATIC
    wiiu-stdout/wiiu-stdout.c
)

target_include_directories(wiiu-stdout INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/wiiu-stdout)
