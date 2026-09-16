/*
 * main.cpp
 * (C) 2018 by Michael Speck
 */

/*
 * This Wii U port was made by thedharex;
 * everyone knows the console failed solely because it didn't have 
 * LBreakoutHD at launch, so I fixed that!
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

using namespace std;

#include "sdl.h"
#include "tools.h"
#include "hiscores.h"
#include "clientgame.h"
#include "mixer.h"
#include "theme.h"
#include "sprite.h"
#include "menu.h"
#include "selectdlg.h"
#include "editor.h"
#include "view.h"

#ifdef __WIIU__
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>
#include <whb/sdcard.h>
#include <SDL2/SDL.h>

extern "C" {
    SDL_Surface* __real_SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
    SDL_Surface* __wrap_SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
        SDL_Surface* surf = __real_SDL_CreateRGBSurface(flags, width, height, depth, Rmask, Gmask, Bmask, Amask);
        if (surf && surf->pixels) {
            SDL_memset(surf->pixels, 0, surf->pitch * surf->h);
        }
        return surf;
    }

    SDL_Surface* __real_SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, Uint32 format);
    SDL_Surface* __wrap_SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, Uint32 format) {
        SDL_Surface* surf = __real_SDL_CreateRGBSurfaceWithFormat(flags, width, height, depth, format);
        if (surf && surf->pixels) {
            SDL_memset(surf->pixels, 0, surf->pitch * surf->h);
        }
        return surf;
    }
}
#endif

int main(int argc, char **argv)
{
	/* i18n */
#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
#endif

#ifdef __WIIU__
    WHBLogCafeInit();
    WHBLogUdpInit();
    WHBMountSdCard();
#endif

	printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright 2018-2024 Michael Speck\n");
	printf("Published under GNU GPL\n");
	printf("---\n");

#ifdef WITH_BUG_REPORT
	printf("Bug reports enabled\n");
	printf("---\n");
#endif

	srand(time(NULL));

	Config config;
	ClientGame cgame(config);
	View view(config, cgame);
	view.runMenu();
	
#ifdef __WIIU__
    WHBUnmountSdCard();
    WHBLogUdpDeinit();
    WHBLogCafeDeinit();
#endif

	return 0;
}
