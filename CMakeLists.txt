include(/opt/devkitpro/cmake/WiiU.cmake)
cmake_minimum_required(VERSION 3.16)

project(themiify C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(themiify
    src/App.cpp
    src/byte_stream.cpp
    src/Camera.cpp
    src/Config.cpp
    src/ContentPanel.cpp
    src/DownloadManager.cpp
    src/graphql.cpp
    src/humanize.cpp
    src/ImageLoader.cpp
    src/main.cpp
    src/NavBar.cpp
    src/PluginManager.cpp
    src/screens/ConfirmExitPopup.cpp
    src/screens/DeleteThemePopup.cpp
    src/screens/DownloadThemePopup.cpp
    src/screens/HomeScreen.cpp
    src/screens/InstallThemePopup.cpp
    src/screens/ManageThemesScreen.cpp
    src/screens/QRCodePopup.cpp
    src/screens/SettingsPopup.cpp
    src/screens/SettingsScreen.cpp
    src/screens/ThemeDetailsPopup.cpp
    src/screens/ThemePreviewPopup.cpp
    src/screens/ThemezerScreen.cpp
    src/ThemeManager.cpp
    src/ThemezerAPI.cpp
    src/timer.cpp
    src/tracer.cpp
    src/UI.cpp
    src/utils.cpp
    src/Zip.cpp
)
wut_create_rpx(themiify)
wut_create_wuhb(themiify
    CONTENT content
    NAME Themiify
    SHORTNAME Themiify
    AUTHOR Fangal-Airbag, AlphaCraft9658, dkosmari & more!
    ICON meta/icon.png
    TVSPLASH meta/tv-splash.png
    DRCSPLASH meta/drc-splash.png
)

find_package(PkgConfig REQUIRED)

pkg_check_modules(sdl2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_mixer REQUIRED IMPORTED_TARGET SDL2_mixer)
pkg_check_modules(SDL2_image REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(zlib REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(freetype2 REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)
pkg_check_modules(libpng REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(libzip REQUIRED IMPORTED_TARGET libzip)
pkg_check_modules(libcurl REQUIRED IMPORTED_TARGET libcurl)

add_subdirectory(external)

target_link_libraries(themiify PRIVATE
    bz2
    cafe_glyphs
    glaze
    imgui
    mocha
    wiiu-stdout
    quirc
    hips
    PkgConfig::zlib
    PkgConfig::freetype2
    PkgConfig::harfbuzz
    PkgConfig::libcurl
    PkgConfig::libpng
    PkgConfig::libzip
    PkgConfig::sdl2
    PkgConfig::SDL2_image
    PkgConfig::SDL2_mixer
)

target_compile_definitions(themiify PRIVATE THEMIIFY_VERSION="1.2")

# run program using wiiload

find_program(WIILOAD_BIN wiiload ${TOOL_HINT})

set(WIILOAD "tcp:wiiu" CACHE STRING "Environment variable for wiiload.")

add_custom_target(run
  COMMAND ${CMAKE_COMMAND} -E env WIILOAD=${WIILOAD} ${WIILOAD_BIN} themiify.wuhb
  DEPENDS themiify.wuhb
)
