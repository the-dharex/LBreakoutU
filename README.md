# LBreakoutHD - Wii U Port

## Description
LBreakoutHD is an HD remake of the classic LBreakout2 breakout-style game. This version is a direct port to the Nintendo Wii U using the WUT (Wii U Toolchain) framework.

It includes native support for the Wii U GamePad, featuring full button controls, analog/D-Pad navigation, and touch screen support—which is especially useful for both gameplay and the integrated Level Editor! Custom levels are fully supported and are saved directly to your SD card.

## Controls

### Menus
* **D-Pad / Left Analog Stick**: Navigate
* **A Button**: Select / Accept
* **B Button**: Back / Cancel

### In-Game
* **Touch Screen / D-Pad / Left Analog Stick**: Move Paddle
* **PLUS (+)**: Pause / Unpause the game
* **MINUS (-)**: Leave the game (requires confirmation)
* **L Button**: Warp to the next level (if enough bricks are destroyed)
* **Touch Screen (Tap) / ZL Button**: Destroy a brick (Workaround for bad level design / Cheat)

### Level Editor
* **Touch Screen**: Place selected brick
* **ZL (Hold) + Touch Screen**: Erase bricks
* **D-Pad / Left Analog Stick**: Navigate the block selector and UI

## Custom Levels
Custom levels created in the level editor (or downloaded from the internet) are saved to and loaded from your SD card. 
The directory used is: `sd:/lbreakouthd/levels/`

## Compilation (For Developers)

To compile this port yourself, you will need the standard Wii U homebrew development environment.

### Requirements:
1. **devkitPro** with the **WUT** (Wii U Toolchain) installed.
2. The following WUT port libraries installed via `(dkp-)pacman`:
   * `wiiu-sdl2`
   * `wiiu-sdl2_image`
   * `wiiu-sdl2_mixer`
   * `wiiu-sdl2_ttf`

### Build Instructions:
Open a terminal in the root directory of the project and run:
```bash
make -f Makefile.wiiu
```
This will generate `lbreakouthd.wuhb` (for use with the Aroma environment) and `lbreakouthd.rpx`.

## Credits
* **Michael Speck**: Original LBreakoutHD / LBreakout2 developer.
* **Wii U Port**: Built with the help of the WUT framework and SDL2 ports by the Wii U homebrew community.
