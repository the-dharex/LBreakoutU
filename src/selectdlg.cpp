/*
 * selectdlg.cpp
 * (C) 2018 by Michael Speck
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "sdl.h"
#include "tools.h"
#include "hiscores.h"
#include "clientgame.h"
#include "mixer.h"
#include "theme.h"
#include "selectdlg.h"

extern SDL_Renderer *mrc;
extern Brick_Conv brick_conv_table[BRICK_COUNT];

SetInfo::SetInfo(const string &n, Theme &theme)
{
	name = n;
	levels = 0;
	version = "1.00"; /* default if not found */
	author = "?";

	validBricks = false;
	memset(bricks, 0, sizeof(bricks));

	labelsInitialized = false;

	if (n[0] == '!') { /* special levels */
		version = "1.00";
		author = "LGames";
		levels = 1;
		return;
	}

	string fpath = getFullLevelsetPath(n);
	string lines[5+EDIT_HEIGHT];
	uint offset = 0;
	
	string content;
#ifdef __WIIU__
	if (fpath.find("/vol/save") == 0) {
		string rel = fpath.substr(9);
		while (rel.length() > 0 && rel[0] == '/') rel = rel.substr(1);
		wiiu_load_file(rel, content);
	} else
#endif
	{
		FILE* f = fopen(fpath.c_str(), "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			long fsize = ftell(f);
			fseek(f, 0, SEEK_SET);
			content.resize(fsize);
			fread(&content[0], 1, fsize, f);
			fclose(f);
		}
	}

	if (content.empty()) {
		_logerr("Levelset %s not found, no preview created\n",n.c_str());
		return;
	}
	
	stringstream ifs(content);
	for (uint i = 0; i < 5+EDIT_HEIGHT; i++)
		readLine(ifs,lines[i]);
	if (lines[0].find("Version") != string::npos) {
		version = trimString(lines[0].substr(lines[0].find(':')+1));
		offset = 1;
	}
	author = lines[1 + offset];

	/* count levels */
	levels = 1;
	while (readLine(ifs, lines[0]))
		if (lines[0].find("Level:") != string::npos)
			levels++;

	/* add bricks of first level */
	for (uint j = 0; j < EDITHEIGHT; j++) {
		for (uint i = 0; i < EDITWIDTH; i++) {
			bricks[j][i] = lines[4+offset+j][i];
		}
	}
	validBricks = true;
}

void SetInfo::initLabels(Theme &theme) {
	if (labelsInitialized) return;
	
	theme.fMenuNormal.setColor(theme.menuFontColorNormal);
	lblNameNormal.setBgColor({0,0,0,0});
	lblNameNormal.setBorder(0);
	lblNameNormal.setText(theme.fMenuNormal, name);

	theme.fMenuNormal.setColor(theme.menuFontColorFocus);
	lblNameFocus.setBgColor({0,0,0,0});
	lblNameFocus.setBorder(0);
	lblNameFocus.setText(theme.fMenuNormal, name);
	
	string str = name + " v" + version + _(" by ") + author;
	theme.fMenuNormal.setColor(theme.menuFontColorNormal);
	lblDetails.setBgColor({0,0,0,0});
	lblDetails.setBorder(0);
	lblDetails.setText(theme.fMenuNormal, str);
	
	str = "(" + to_string(levels) + _(" levels)");
	theme.fMenuNormal.setColor(theme.menuFontColorNormal);
	lblLevels.setBgColor({0,0,0,0});
	lblLevels.setBorder(0);
	lblLevels.setText(theme.fMenuNormal, str);
	
	labelsInitialized = true;
}


/** Create levelset list and previews + layout. */
void SelectDialog::init(int sd_type)
{
	uint sw = theme.menuBackground.getWidth();
	uint sh = theme.menuBackground.getHeight();
	vector<string> list, list2;
	string path;

	/* mini-games and installed sets */
	if (sd_type == SDT_ALL) {
		list.push_back(_(TOURNAMENT));
		list.push_back(_(RANDOM20));
		list.push_back(_("!BARRIER!"));
		list.push_back(_("!HUNTER!"));
		list.push_back(_("!INVADERS!"));
		list.push_back(_("!JUMPING_JACK!"));
		list.push_back(_("!OUTBREAK!"));
		list.push_back(_("!SITTING_DUCKS!"));
		readDir(string(DATADIR)+"/levels", RD_FILES, list2);
		for (auto& s : list2)
			list.push_back(s);

		/* if game is installed, get customs from home as well */
		if (string(CONFIGDIR) != ".") {
			readDir(getCustomLevelsetDir(),RD_FILES, list2);
			for (auto& s : list2)
				list.push_back(string("~")+s);
		}
	} else { /* custom sets only */
		/* we need to add LBreakoutHD as it is ignored further down */
		list.push_back("LBreakoutHD");

		/* if not installed use regular levels */
		readDir(getCustomLevelsetDir(), RD_FILES, list2);
		for (auto& s : list2)
			list.push_back(s);
	}

	vlen = (0.7 * sh) / theme.fMenuNormal.getSize(); /* vlen = displayed entries */
	sel = SEL_NONE;
	pos = max = 0;
	if (list.size()-1 > vlen) /* we will skip LBreakoutHD so -1 */
		max = list.size()-1 - vlen;
	cw = 0.2*sw;
	ch = 1.1 * theme.fMenuNormal.getSize();
	lx = 0.1*sw;
	ly = (sh - vlen*ch)/2;
	tx = sw/2;
	ty = ly/2;
	px = 0.4*sw;
	pw = 0.5*sw;
	ph = MAPWIDTH * pw / MAPHEIGHT;
	py = (sh - ph - 3*theme.fMenuNormal.getSize())/2;

	background.createFromScreen();
	previewTex.create(MAPWIDTH*theme.bricks.getGridWidth(), MAPHEIGHT*theme.bricks.getGridHeight());
	lastSel = -2;

	theme.fMenuFocus.setColor(theme.menuFontColorNormal);
	lblTitle.setBgColor({0,0,0,0});
	lblTitle.setBorder(0);
	lblTitle.setText(theme.fMenuFocus, _("Select Levelset (press ESC to exit)"));

	theme.fMenuNormal.setColor(theme.menuFontColorNormal);
	lblPrevNormal.setBgColor({0,0,0,0});
	lblPrevNormal.setBorder(0);
	lblPrevNormal.setText(theme.fMenuNormal, _("<Previous Page>"));
	
	lblNextNormal.setBgColor({0,0,0,0});
	lblNextNormal.setBorder(0);
	lblNextNormal.setText(theme.fMenuNormal, _("<Next Page>"));

	theme.fMenuNormal.setColor(theme.menuFontColorFocus);
	lblPrevFocus.setBgColor({0,0,0,0});
	lblPrevFocus.setBorder(0);
	lblPrevFocus.setText(theme.fMenuNormal, _("<Previous Page>"));
	
	lblNextFocus.setBgColor({0,0,0,0});
	lblNextFocus.setBorder(0);
	lblNextFocus.setText(theme.fMenuNormal, _("<Next Page>"));

	entries.clear();
	for (auto& e : list) {
		if (e == "LBreakoutHD")
			continue;
		SetInfo *si = new SetInfo(e, theme);
		entries.push_back(unique_ptr<SetInfo>(si));
	}
	
	/* Restore theme colors correctly since we changed them for labels */
	theme.fMenuNormal.setColor(theme.menuFontColorNormal);
	theme.fMenuFocus.setColor(theme.menuFontColorFocus);
	
	/* select first entry if any */
	if (entries.size() > 0)
		sel = 0;
}

void SelectDialog::render()
{
	Font &font = theme.fMenuNormal;
	int y = ly;

	background.copy();

	lblTitle.copy(tx, ty, ALIGN_X_CENTER | ALIGN_Y_CENTER);

	if (pos > 0) {
		if (sel == SEL_PREV)
			lblPrevFocus.copy(lx, ly-ch, ALIGN_X_LEFT | ALIGN_Y_TOP);
		else
			lblPrevNormal.copy(lx, ly-ch, ALIGN_X_LEFT | ALIGN_Y_TOP);
	}
	for (uint i = 0; i < vlen; i++, y += ch) {
		if (pos + i < entries.size()) {
			SetInfo *si = entries[pos + i].get();
			si->initLabels(theme);
			if (sel == (int)(pos + i))
				si->lblNameFocus.copy(lx, y, ALIGN_X_LEFT | ALIGN_Y_TOP);
			else
				si->lblNameNormal.copy(lx, y, ALIGN_X_LEFT | ALIGN_Y_TOP);
		}
	}
	if (pos < max) {
		if (sel == SEL_NEXT)
			lblNextFocus.copy(lx, y, ALIGN_X_LEFT | ALIGN_Y_TOP);
		else
			lblNextNormal.copy(lx, y, ALIGN_X_LEFT | ALIGN_Y_TOP);
	}

	if (sel >= 0 && (uint)sel < entries.size()) {
		SetInfo *si = entries[sel].get();
		if (sel != lastSel) {
			SDL_SetRenderTarget(mrc, previewTex.getTex());
			
			uint sw = theme.menuBackground.getWidth();
			uint sh = theme.menuBackground.getHeight();
			uint bw = theme.bricks.getGridWidth();
			uint bh = theme.bricks.getGridHeight();
			uint soff = bh/3;
			
			Image& wallpaper = theme.wallpapers[rand()%theme.numWallpapers];
			for (uint wy = 0; wy < sh; wy += wallpaper.getHeight())
				for (uint wx = 0; wx < sw; wx += wallpaper.getWidth())
					wallpaper.copy(wx,wy);
			theme.frameShadow.copy(soff,soff);
			
			if (si->validBricks) {
				for (uint j = 0; j < EDITHEIGHT; j++) {
					for (uint i = 0; i < EDITWIDTH; i++) {
						int k = -1;
						for ( k = 0; k < BRICK_COUNT; k++ )
							if (si->bricks[j][i] == brick_conv_table[k].c)
								break;
						if (k < BRICK_COUNT && k != INVIS_BRICK_ID)
							theme.bricksShadow.copy(brick_conv_table[k].id,0,
									(i+1)*bw+bh/3, (1+j)*bh+bh/3);
					}
				}
				for (uint j = 0; j < EDITHEIGHT; j++) {
					for (uint i = 0; i < EDITWIDTH; i++) {
						int k = -1;
						for ( k = 0; k < BRICK_COUNT; k++ )
							if (si->bricks[j][i] == brick_conv_table[k].c)
								break;
						if (k < BRICK_COUNT && k != INVIS_BRICK_ID)
							theme.bricks.copy(brick_conv_table[k].id,0,
									(i+1)*bw, (1+j)*bh);
					}
				}
			} else if (si->name[0] == '!') {
				theme.fMenuNormal.setAlign(ALIGN_X_CENTER | ALIGN_Y_CENTER);
				if (si->name == TOURNAMENT)
					theme.fMenuNormal.write(previewTex.getWidth()/2,previewTex.getHeight()/2,_("Superset with ALL levels"));
				else if (si->name == RANDOM20)
					theme.fMenuNormal.write(previewTex.getWidth()/2,previewTex.getHeight()/2,_("20 randomly selected levels from all sets"));
				else
					theme.fMenuNormal.write(previewTex.getWidth()/2,previewTex.getHeight()/2,_("Mini Game"));
			}
			
			theme.frame.copy(0,0);
			SDL_SetRenderTarget(mrc, NULL);
			
			lastSel = sel;
		}
		
		si->initLabels(theme);
		previewTex.copy(px,py,pw,ph);
		
		si->lblDetails.copy(px + pw/2, py+ph+theme.fMenuNormal.getSize(), ALIGN_X_CENTER | ALIGN_Y_TOP);
		si->lblLevels.copy(px + pw/2, py+ph+theme.fMenuNormal.getSize()*2, ALIGN_X_CENTER | ALIGN_Y_TOP);
	}
}

/* Return 1 if selection made, 0 otherwise */
int SelectDialog::run()
{
	SDL_Event ev;
	bool leave = false;
	int ret = 0;

	render();
	while (!quitReceived && !leave) {
		/* handle events */
		bool newEvent = false;
#ifdef __WIIU__
		static int gpad_delay = 0;
		if (gpad_delay > 0) gpad_delay--;
		
		if (SDL_PollEvent(&ev)) {
			newEvent = true;
		}
		
		static Uint8 prev_gpadstate[GPAD_LAST1] = {0};
		const Uint8 *gpadstate = gamepad.update();
		
		if (!newEvent || ev.type != SDL_QUIT) {
			SDL_Scancode code = SDL_SCANCODE_UNKNOWN;
			
			if (gpadstate[GPAD_BUTTON0 + 0] && !prev_gpadstate[GPAD_BUTTON0 + 0]) code = SDL_SCANCODE_RETURN; // A button
			else if (gpadstate[GPAD_BUTTON0 + 1] && !prev_gpadstate[GPAD_BUTTON0 + 1]) code = SDL_SCANCODE_ESCAPE; // B button
			
			if (gpadstate[GPAD_UP] && !prev_gpadstate[GPAD_UP]) { code = SDL_SCANCODE_UP; gpad_delay = 25; }
			else if (gpadstate[GPAD_DOWN] && !prev_gpadstate[GPAD_DOWN]) { code = SDL_SCANCODE_DOWN; gpad_delay = 25; }
			else if (gpadstate[GPAD_LEFT] && !prev_gpadstate[GPAD_LEFT]) { code = SDL_SCANCODE_PAGEUP; gpad_delay = 25; }
			else if (gpadstate[GPAD_RIGHT] && !prev_gpadstate[GPAD_RIGHT]) { code = SDL_SCANCODE_PAGEDOWN; gpad_delay = 25; }
			else if (code == SDL_SCANCODE_UNKNOWN && gpad_delay == 0) {
				if (gpadstate[GPAD_UP]) { code = SDL_SCANCODE_UP; gpad_delay = 6; }
				else if (gpadstate[GPAD_DOWN]) { code = SDL_SCANCODE_DOWN; gpad_delay = 6; }
				else if (gpadstate[GPAD_LEFT]) { code = SDL_SCANCODE_PAGEUP; gpad_delay = 6; }
				else if (gpadstate[GPAD_RIGHT]) { code = SDL_SCANCODE_PAGEDOWN; gpad_delay = 6; }
			}
			
			if (code != SDL_SCANCODE_UNKNOWN) {
				ev.type = SDL_KEYDOWN;
				ev.key.keysym.scancode = code;
				newEvent = true;
			}
		}
		memcpy(prev_gpadstate, gpadstate, GPAD_LAST1);
		if (newEvent) {
#else
		if (SDL_WaitEvent(&ev)) {
#endif
			if (ev.type == SDL_QUIT)
				quitReceived = true;
			if (ev.type == SDL_KEYDOWN) {
				switch (ev.key.keysym.scancode) {
				case SDL_SCANCODE_ESCAPE:
					leave = true;
					break;
				case SDL_SCANCODE_PAGEUP:
					changeSelection(-vlen);
					break;
				case SDL_SCANCODE_PAGEDOWN:
					changeSelection(vlen);
					break;
				case SDL_SCANCODE_UP:
					changeSelection(-1);
					break;
				case SDL_SCANCODE_DOWN:
					changeSelection(1);
					break;
				default:
					break;
				}
			}
			if (ev.type == SDL_MOUSEMOTION && entries.size() > 0) {
				int oldsel = sel;
				if (ev.motion.x >= lx && ev.motion.y >= ly &&
						ev.motion.x < (int)(lx + cw) &&
						ev.motion.y < int(ly + ch*vlen)) {
					sel = pos + (ev.motion.y - ly)/ch;
					if (sel < 0) sel = 0;
					if ((uint)sel >= entries.size())
						sel = (int)entries.size()-1;
				} else if (ev.motion.y < ly)
					sel = SEL_PREV;
				else if (ev.motion.y > int(ly + ch*vlen))
					sel = SEL_NEXT;
				else
					sel = SEL_NONE;
				if (sel != oldsel)
					mixer.play(theme.sMenuMotion);
			} else if (ev.type == SDL_MOUSEMOTION) {
				sel = SEL_NONE;
			}
			if (ev.type == SDL_MOUSEWHEEL) {
				if (ev.wheel.y < 0) {
					goNextPage();
					sel = SEL_NONE;
				} else if (ev.wheel.y > 0) {
					goPrevPage();
					sel = SEL_NONE;
				}
			}
			if (ev.type == SDL_MOUSEBUTTONDOWN) {
				if (sel == SEL_PREV)
					goPrevPage();
				else if (sel == SEL_NEXT)
					goNextPage();
				else if (sel != SEL_NONE) {
					ret = 1;
					leave = true;
				}
				if (sel != SEL_NONE)
					mixer.play(theme.sMenuClick);
			}
			if (ev.type == SDL_KEYDOWN && sel >= 0 &&
					ev.key.keysym.scancode == SDL_SCANCODE_RETURN) {
				ret = 1;
				leave = true;
				mixer.play(theme.sMenuClick);
			}
		}
		/* render */
		render();
		SDL_RenderPresent(mrc);
		SDL_Delay(10);
		FlushUselessEvents(); /* prevent event loop from dying */
	}

	/* clear events for menu loop */
	FlushInputEvents();

	return ret;
}
