# Themiify

<p align="center">
    <a href="https://hb-app.store/wiiu/Themiify">
        <img src="hbas/hbasbadge-wiiu.png" width="335" height="96">
    </a>
</p>

Themiify is a homebrew application that allows for on the fly installation of Wii U Menu themes via the [Themezer](https://themezer.net/) API or via `.utheme` files placed on an SD Card.
Once a theme is installed, [StyleMiiU](https://github.com/ThemeCafe/StyleMiiU-Plugin) will read the theme and show it the next time the Wii U Menu is loaded.

## Requirements

Themiify will only work if you have the StyleMiiU aroma plugin installed and if your Wii U Menu's files on your Wii U's NAND are unmodified!


## Usage

### Home Tab

![Themiify Home Screen](hbas/screenshot-1.png)

This shows the current theme set in StyleMiiU.

### Local Themes Tab

You can install `.utheme` files placed in `sd:/wiiu/themes` from this tab. You can also manage your installed themes here with the ability to either delete them or
set them as your current StyleMiiU theme on the fly.

![Themiify Manage Themes Screen](hbas/screenshot-2.png)

### Network Tab
You can download themes uploaded to Themezer from this tab. To search for more specific themes you can use different filters, the search bar or QR codes.

![Themiify Themezer Screen](hbas/screenshot-3.png)

![Themiify Theme Details Popup](hbas/screenshot-4.png)

### Settings Tab

If you are a theme creator, a quick way to dump your menu files from your NAND would be here. Simply select `Dump Wii U Menu Files` and follow the instructions
to dump!

## Support

For further information on usage and troubleshooting of Themiify, please consult the [Theme Café Docs](https://themecafe.github.io/Docs/install/themiify/).

## Building

### Makefile

To build Themiify please ensure you have the following packages installed via the dkp-pacman package manager: `ppc-bz2`, `ppc-zlib`, `ppc-freetype2`,
`ppc-harfbuzz`, `ppc-libpng`, `ppc-libzip`, `wut`, `wiiu-sdl2`, `wiiu-libcurl`, `wiiu-sdl2_mixer` & `wiiu-sdl2_image`.

Then simply run:

```shell
cmake -S . -B build
cmake --build build
```

### Dockerfile

To build Themiify using docker, please run the following command:
```shell
./docker-build.sh
```

## Credits

- [Fangal-Airbag](https://github.com/Fangal-Airbag), [AlphaCraft9658](https://github.com/AlphaCraft9658) & [Daniel K. O](https://github.com/dkosmari)
for all their help on the development of this project!
- Perrohuevo, dewgong and Daniel K. O. for their help designing the UI for this project!
- [Juanen100](https://github.com/Juanen100) for the [StyleMiiU Aroma Plugin](https://github.com/ThemeCafe/StyleMiiU-Plugin)!
- The Theme Café Discord mods, devs and founders!
- Gatto for the amazing [Theme Café Docs](https://themecafe.github.io/Docs/)!
- All the amazing Wii U theme creators!
- And many more!
