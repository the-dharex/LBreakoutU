/*
 * sdl.cpp
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

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "tools.h"
#ifdef __WIIU__
#include <vpad/input.h>
#include <padscore/kpad.h>
#endif
#include "sdl.h"

int Geom::sw = 640; /* safe values before MainWindow is called */
int Geom::sh = 480;
bool Image::useColorKeyBlack = false; /* workaround for old color key in lbr2 themes */

SDL_Renderer *mrc = NULL;

Gamepad gamepad; /* main window render context, got only one */
SDL_Texture *globalTarget = NULL;

void MyRenderPresent()
{
#ifdef __WIIU__
    if (globalTarget) {
        SDL_SetRenderTarget(mrc, NULL);
        SDL_SetTextureBlendMode(globalTarget, SDL_BLENDMODE_NONE);
        SDL_RenderCopy(mrc, globalTarget, NULL, NULL);
        (SDL_RenderPresent)(mrc);
        SDL_SetRenderTarget(mrc, globalTarget);
    } else {
        (SDL_RenderPresent)(mrc);
    }
#else
    (SDL_RenderPresent)(mrc);
#endif
}

/** Main application window */

MainWindow::MainWindow(const char *title, int _w, int _h, int _full)
{
	if (_w <= 0 || _h <= 0) { /* no width or height, use desktop setting */
		SDL_DisplayMode mode;
		SDL_GetCurrentDisplayMode(0,&mode);
		_w = mode.w;
		_h = mode.h;
		_full = 1;
	}
	w = _w;
	h = _h;
	/* TEST  w = 1600; h = 1200; flags = 0; */
	Geom::sw = w;
	Geom::sh = h;
	_loginfo("Creating main window with %dx%d, fullscreen=%d\n",w,h,_full);
 	if (_full) {
		mw = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_FULLSCREEN_DESKTOP);
	} else
		mw = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
					SDL_WINDOWPOS_CENTERED, w, h, 0);
	if(mw == NULL)
		_logsdlerr();
	if ((mr = SDL_CreateRenderer(mw, -1, SDL_RENDERER_ACCELERATED)) == NULL)
		_logsdlerr();
	mrc = mr;

#ifdef __WIIU__
	globalTarget = SDL_CreateTexture(mrc, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
	SDL_SetRenderTarget(mrc, globalTarget);
#endif

	/* back to black */
	SDL_SetRenderDrawColor(mrc,0,0,0,SDL_ALPHA_OPAQUE);
	SDL_RenderClear(mrc);
}
MainWindow::~MainWindow()
{
#ifdef __WIIU__
	if (globalTarget) {
		SDL_DestroyTexture(globalTarget);
		globalTarget = NULL;
	}
#endif
	if (mr)
		SDL_DestroyRenderer(mr);
	if (mw)
		SDL_DestroyWindow(mw);
}

void MainWindow::refresh()
{
	SDL_RenderPresent(mr);
}

/** Image */

/** Create new black image. If no size is given use screen size. */
int Image::create(int w, int h)
{
	if (w == 0 || h == 0)
		SDL_GetRendererOutputSize(mrc,&w,&h);

	_logdebug(1,"Creating new texture of size %dx%d\n",w,h);

	if (tex) {
		SDL_DestroyTexture(tex);
		tex = NULL;
	}

	this->w = w;
	this->h = h;
	if ((tex = SDL_CreateTexture(mrc,SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_TARGET,w, h)) == NULL) {
		_logsdlerr();
		return 0;
	}
	fill(0,0,0,0);
	setBlendMode(1);
	return 1;
}

/* Create texture from pixels in current render target. This only works
 * if the render target has not been yet presented with SDL_RenderPresent()
 * as otherwise the double buffer is flipped and we get the old contents.
 * So the way to go is: build the render target, call this function, then
 * render the present target and copy this image before a new
 * SDL_RenderPresent().
 */
int Image::createFromScreen()
{
#ifdef __WIIU__
	if (!globalTarget) return 0;
	int w, h;
	SDL_GetRendererOutputSize(mrc, &w, &h);
	create(w, h);
	SDL_Texture *old = SDL_GetRenderTarget(mrc);
	SDL_SetRenderTarget(mrc, tex);
	
	SDL_BlendMode oldBlend;
	SDL_GetTextureBlendMode(globalTarget, &oldBlend);
	SDL_SetTextureBlendMode(globalTarget, SDL_BLENDMODE_NONE);
	
	SDL_RenderCopy(mrc, globalTarget, NULL, NULL);
	
	SDL_SetTextureBlendMode(globalTarget, oldBlend);
	SDL_SetRenderTarget(mrc, old);
	return 1;
#else
	int w, h;

	SDL_GetRendererOutputSize(mrc,&w,&h);
	SDL_Surface *sshot = SDL_CreateRGBSurface(0, w, h, 32,
			0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	SDL_RenderReadPixels(mrc, NULL, SDL_PIXELFORMAT_ARGB8888,
					sshot->pixels, sshot->pitch);
	int ret = load(sshot);
	SDL_FreeSurface(sshot);
	return ret;
#endif
}

/** OLD STUFF FOR SOFTWARE SCALE AS FALLBACK */

/** Set pixel in surface. */
Uint32 set_pixel( SDL_Surface *surf, int x, int y, Uint32 pixel )
{
    int pos = 0;

    if (x < 0 || y < 0 || x >= surf->w || y >= surf->h)
	    return pixel;

    pos = y * surf->pitch + x * surf->format->BytesPerPixel;
    memcpy( (char*)(surf->pixels) + pos, &pixel, surf->format->BytesPerPixel );
    return pixel;
}

/** Get pixel from surface. */
Uint32 get_pixel( SDL_Surface *surf, int x, int y )
{
    int pos = 0;
    Uint32 pixel = 0;

    pos = y * surf->pitch + x * surf->format->BytesPerPixel;
    memcpy( &pixel, (char*)(surf->pixels) + pos, surf->format->BytesPerPixel );
    return pixel;
}

/** Scale surface to half the size */
SDL_Surface *create_small_surface(SDL_Surface *src) {
	SDL_Surface *dst = 0;
	SDL_PixelFormat *spf = src->format;

	if ((dst = SDL_CreateRGBSurface(SDL_SWSURFACE,
			src->w/2, src->h/2,
			spf->BitsPerPixel,
			spf->Rmask, spf->Gmask, spf->Bmask,
			spf->Amask)) == 0) {
		_logsdlerr();
		return 0;
	}

        for ( int j = 0; j < dst->h; j++ ) {
            for ( int i = 0; i < dst->w; i++ )
                set_pixel(dst, i, j,
                           get_pixel( src, i*2, j*2 ) );
        }
	return dst;
}

/** OLD STUFF END */

/** Load image from file. Return
 *   1 on success without any problems
 *   2 on successfully loading surface but failing to create hardware texture
 *     then surface is scaled to half size and smaller texture is created
 *   0 on complete failure
 */
int Image::load(const string& fname)
{
	_logdebug(1,"Loading texture %s\n",fname.c_str());

	/* delete old texture */
	if (tex) {
		SDL_DestroyTexture(tex);
		tex = NULL;
	}
	w = 0;
	h = 0;

	/* load image as software surface */
	SDL_Surface *surf = IMG_Load(fname.c_str());
	if (surf == NULL) {
		_logsdlerr();
		return 0;
	}

	/* set black as color key if requested for old images */
	if (Image::useColorKeyBlack)
		SDL_SetColorKey(surf, SDL_TRUE, 0x0);

	/* create hardware texture from software surface */
	if ((tex = SDL_CreateTextureFromSurface(mrc, surf))) {
		w = surf->w;
		h = surf->h;
		SDL_FreeSurface(surf);
		return 1;
	}
	_logsdlerr();

	/* if we get here, surface was loaded thus file found
	 * but we couldn't create the texture strongly suggesting
	 * it is too large for video memory. so we scale down and
	 * retry once before we give up. */

	/* get new surface half the size */
	SDL_Surface *newsurf = create_small_surface(surf);
	if (newsurf == 0) {
		_logsdlerr();
		SDL_FreeSurface(surf);
		return 0;
	}

	/* create texture with new surface */
	int ret = 0;
	if ((tex = SDL_CreateTextureFromSurface(mrc, newsurf))) {
		w = newsurf->w;
		h = newsurf->h;
		ret = 2;
	}
	SDL_FreeSurface(surf);
	SDL_FreeSurface(newsurf);
	return ret;
}

int Image::load(SDL_Surface *s)
{
	_logdebug(1,"Loading texture from surface %dx%d\n",s->w,s->h);

	if (tex) {
		SDL_DestroyTexture(tex);
		tex = NULL;
	}

	if ((tex = SDL_CreateTextureFromSurface(mrc,s)) == NULL) {
		_logsdlerr();
		return 0;
	}
	w = s->w;
	h = s->h;
	return 1;
}
int Image::load(Image *s, int x, int y, int w, int h)
{
	_logdebug(1,"Loading texture from surface %dx%d\n",s->w,s->h);

	if (tex) {
		SDL_DestroyTexture(tex);
		tex = NULL;
	}

	SDL_Rect srect = {x, y, w, h};
	SDL_Rect drect = {0, 0, w, h};

	this->w = w;
	this->h = h;
	if ((tex = SDL_CreateTexture(mrc,SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_TARGET,w, h)) == NULL) {
		_logsdlerr();
		return 0;
	}
	SDL_SetRenderTarget(mrc, tex);
	SDL_RenderCopy(mrc, s->getTex(), &srect, &drect);
	SDL_SetRenderTarget(mrc, NULL);
	return 1;
}

SDL_Texture *Image::getTex()
{
	return tex;
}

void Image::copy() /* full scale */
{
	if (tex == NULL)
		return;

	SDL_RenderCopy(mrc, tex, NULL, NULL);
}
void Image::copy(int dx, int dy)
{
	if (tex == NULL)
		return;

	SDL_Rect drect = {dx , dy , w, h};
	SDL_RenderCopy(mrc, tex, NULL, &drect);
}
void Image::copy(int dx, int dy, int dw, int dh)
{
	if (tex == NULL)
		return;

	SDL_Rect drect = {dx , dy , dw, dh};
	SDL_RenderCopy(mrc, tex, NULL, &drect);
}
void Image::copy(int sx, int sy, int sw, int sh, int dx, int dy) {
	if (tex == NULL)
		return;

	SDL_Rect srect = {sx, sy, sw, sh};
	SDL_Rect drect = {dx , dy , sw, sh};
	SDL_RenderCopy(mrc, tex, &srect, &drect);
}

void Image::fill(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	SDL_Texture *old = SDL_GetRenderTarget(mrc);
	SDL_SetRenderTarget(mrc,tex);
	SDL_SetRenderDrawBlendMode(mrc, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(mrc,r,g,b,a);
	SDL_RenderClear(mrc);
	SDL_SetRenderDrawBlendMode(mrc, SDL_BLENDMODE_BLEND);
	SDL_SetRenderTarget(mrc,old);
}

void Image::fill(int x, int y, int w, int h, const SDL_Color &c) {
	SDL_Rect r = {x,y,w,h};
	SDL_Texture *old = SDL_GetRenderTarget(mrc);
	SDL_SetRenderTarget(mrc,tex);
	SDL_SetRenderDrawBlendMode(mrc, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(mrc,c.r,c.g,c.b,c.a);
	SDL_RenderFillRect(mrc, &r);
	SDL_SetRenderDrawBlendMode(mrc, SDL_BLENDMODE_BLEND);
	SDL_SetRenderTarget(mrc,old);
}

void Image::scale(int nw, int nh)
{
	_logdebug(1,"Scaling texture of size %dx%d to %dx%d\n",w,h,nw,nh);

	if (tex == NULL)
		return;
	if (nw == w && nh == h)
		return; /* already ok */

	SDL_Texture *newtex = 0;
	if ((newtex = SDL_CreateTexture(mrc,SDL_PIXELFORMAT_RGBA8888,
					SDL_TEXTUREACCESS_TARGET,nw,nh)) == NULL) {
		_logsdlerr();
		return;
	}
	SDL_Texture *oldTarget = SDL_GetRenderTarget(mrc);
	SDL_SetRenderTarget(mrc, newtex);
	SDL_SetTextureBlendMode(newtex, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(mrc,0,0,0,0);
	SDL_RenderClear(mrc);
	SDL_RenderCopy(mrc, tex, NULL, NULL);
	SDL_SetRenderTarget(mrc, oldTarget);

	SDL_DestroyTexture(tex);
	tex = newtex;
	w = nw;
	h = nh;
}

int Image::createShadow(Image &img)
{
	_logdebug(1,"Creating shadow of size %dx%d\n",
				img.getWidth(),img.getHeight());

	/* duplicate first */
	create(img.getWidth(),img.getHeight());
	SDL_Texture *oldTarget = SDL_GetRenderTarget(mrc);
	SDL_SetRenderTarget(mrc, tex);
	
	SDL_SetRenderDrawColor(mrc, 0, 0, 0, 0);
	SDL_RenderClear(mrc);

	SDL_Texture *srcTex = img.getTex();
	if (srcTex) {
		SDL_BlendMode oldBlend;
		Uint8 oldR, oldG, oldB, oldA;
		SDL_GetTextureBlendMode(srcTex, &oldBlend);
		SDL_GetTextureColorMod(srcTex, &oldR, &oldG, &oldB);
		SDL_GetTextureAlphaMod(srcTex, &oldA);
		
		SDL_SetTextureBlendMode(srcTex, SDL_BLENDMODE_NONE);
		SDL_SetTextureColorMod(srcTex, 0, 0, 0);
		SDL_SetTextureAlphaMod(srcTex, 127);
		
		img.copy(0,0);
		
		SDL_SetTextureBlendMode(srcTex, oldBlend);
		SDL_SetTextureColorMod(srcTex, oldR, oldG, oldB);
		SDL_SetTextureAlphaMod(srcTex, oldA);
	}

	SDL_SetRenderTarget(mrc, oldTarget);
	return 1;
}

/** Grid image: large bitmap with same sized icons */

int GridImage::load(const string& fname, int _gw, int _gh)
{
	/* set grid size and load basic image */
	gw = _gw;
	gh = _gh;
	int ret = Image::load(fname);
	/* if 2 is returned the image was scaled down to
	 * half the size so adjust grid size accordingly. */
	if (ret == 2) {
		gw /= 2;
		gh /= 2;
		_logerr("Grid image %s too large for hardware texture: scaled to half size %dx%d\n",
				fname.c_str(),gw,gh);
	}
	return ret;
}

int GridImage::load(SDL_Surface *s, int _gw, int _gh)
{
	gw = _gw;
	gh = _gh;
	return Image::load(s);
}

void GridImage::copy(int gx, int gy, int dx, int dy)
{
	if (tex == NULL )
		return;

	SDL_Rect srect = {gx * gw, gy * gh, gw, gh};
	SDL_Rect drect = {dx , dy , gw, gh};
	SDL_RenderCopy(mrc, tex, &srect, &drect);
}
void GridImage::copy(int gx, int gy, int dx, int dy, int dw, int dh)
{
	if (tex == NULL )
		return;

	SDL_Rect srect = {gx * gw, gy * gh, gw, gh};
	SDL_Rect drect = {dx , dy , dw, dh};
	SDL_RenderCopy(mrc, tex, &srect, &drect);
}
void GridImage::copy(int gx, int gy, int sx, int sy, int sw, int sh, int dx, int dy)
{
	if (tex == NULL )
		return;

	SDL_Rect srect = {gx * gw + sx, gy * gh + sy, sw, sh};
	SDL_Rect drect = {dx , dy , sw, sh};
	SDL_RenderCopy(mrc, tex, &srect, &drect);
}


/** Scale cell by cell to prevent artifacts. */
void GridImage::scale(int ncw, int nch)
{
	if (tex == NULL)
		return;

	int nw = ncw * getGridSizeX();
	int nh = nch * getGridSizeY();
	SDL_Texture *newtex = 0;
	SDL_Rect srect = {0, 0, gw, gh};
	SDL_Rect drect = {0, 0, ncw, nch};

	if (nw == w && nh == h)
		return; /* already ok */

	if ((newtex = SDL_CreateTexture(mrc,SDL_PIXELFORMAT_RGBA8888,
					SDL_TEXTUREACCESS_TARGET,nw,nh)) == NULL) {
		_logsdlerr();
		return;
	}
	SDL_SetRenderTarget(mrc, newtex);
	SDL_SetTextureBlendMode(newtex, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(mrc,0,0,0,0);
	SDL_RenderClear(mrc);
	for (uint j = 0; j < getGridSizeY(); j++)
		for (uint i = 0; i < getGridSizeX(); i++) {
			srect.x = i * gw;
			srect.y = j * gh;
			drect.x = i * ncw;
			drect.y = j * nch;
			SDL_RenderCopy(mrc, tex, &srect, &drect);
		}
	SDL_SetRenderTarget(mrc, NULL);

	SDL_DestroyTexture(tex);
	tex = newtex;
	w = nw;
	h = nh;
	gw = ncw;
	gh = nch;
}

int GridImage::createShadow(GridImage &img)
{
	gw = img.getGridWidth();
	gh = img.getGridHeight();
	return Image::createShadow(img);
}


/** Font */

Font::Font() : font(0), size(0)
{
}
Font::~Font() {
	if (font)
		TTF_CloseFont(font);
}

void Font::load(const string& fname, int sz) {
	if (font) {
		TTF_CloseFont(font);
		font = 0;
		size = 0;
	}

	if ((font = TTF_OpenFont(fname.c_str(), sz)) == NULL) {
		_logsdlerr();
		return;
	}
	size = sz;
	setColor(255,255,255,255);
	setAlign(ALIGN_X_LEFT | ALIGN_Y_TOP);
}

void Font::setColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	SDL_Color c = {r, g, b, a};
	clr = c;
}
void Font::setColor(SDL_Color c) {
	clr = c;
}
void Font::setAlign(int a)
{
	align = a;
}

static SDL_Texture* create_padded_texture(SDL_Renderer *mrc, SDL_Surface *surf) {
	SDL_Surface *padded = SDL_CreateRGBSurfaceWithFormat(0, surf->w + 2, surf->h + 2, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!padded) {
		padded = SDL_CreateRGBSurface(0, surf->w + 2, surf->h + 2, 32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#else
				0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#endif
		);
	}
	if (!padded) return SDL_CreateTextureFromSurface(mrc, surf);
	
	SDL_FillRect(padded, NULL, SDL_MapRGBA(padded->format, 0, 0, 0, 0));
	SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);
	SDL_Rect dst = {1, 1, surf->w, surf->h};
	SDL_BlitSurface(surf, NULL, padded, &dst);
	SDL_Texture *tex = SDL_CreateTextureFromSurface(mrc, padded);
	SDL_FreeSurface(padded);
	return tex;
}

SDL_Texture* text_pool_wrapped[2048] = {NULL};
int pool_idx_wrapped = 0;

void Font::write(int x, int y, const string& _str, int alpha) {
	if (font == 0)
		return;

	/* XXX doesn't look good, why no rendering
	 * into texture directly available? */
	SDL_Surface *surf;
	SDL_Texture *tex;
	SDL_Rect drect;
	const char *str = _str.c_str();

	if (strlen(str) == 0)
		return;

	static SDL_Texture* text_pool[2048] = {NULL};
	static int pool_idx = 0;

	if ((surf = TTF_RenderUTF8_Blended(font, str, clr)) == NULL)
		_logsdlerr();
	
	if (text_pool[pool_idx]) {
		SDL_DestroyTexture(text_pool[pool_idx]);
	}
	text_pool[pool_idx] = create_padded_texture(mrc, surf);
	if (text_pool[pool_idx] == NULL)
		_logsdlerr();
	tex = text_pool[pool_idx];
	pool_idx = (pool_idx + 1) % 2048;

	if (align & ALIGN_X_LEFT)
		drect.x = x;
	else if (align & ALIGN_X_RIGHT)
		drect.x = x - surf->w;
	else
		drect.x = x - surf->w/2; /* center */
	if (align & ALIGN_Y_TOP)
		drect.y = y;
	else if (align & ALIGN_Y_BOTTOM)
		drect.y = y - surf->h;
	else
		drect.y = y - surf->h/2;
	drect.w = surf->w;
	drect.h = surf->h;
	
	drect.x -= 1;
	drect.y -= 1;
	drect.w += 2;
	drect.h += 2;
	if (alpha < 255)
		SDL_SetTextureAlphaMod(tex, alpha);

	/* do a shadow first */
	if (1) {
		SDL_Rect drect2 = drect;
		SDL_Surface *surf2;
		SDL_Texture *tex2;
		SDL_Color clr2 = { 0,0,0,255 };
		if ((surf2 = TTF_RenderUTF8_Blended(font, str, clr2)) == NULL)
			_logsdlerr();
		
		if (text_pool[pool_idx]) {
			SDL_DestroyTexture(text_pool[pool_idx]);
		}
		text_pool[pool_idx] = create_padded_texture(mrc, surf2);
		if (text_pool[pool_idx] == NULL)
			_logsdlerr();
		tex2 = text_pool[pool_idx];
		pool_idx = (pool_idx + 1) % 2048;

		drect2.x += size/10 - 1;
		drect2.y += size/10 - 1;
		drect2.w += 2;
		drect2.h += 2;
		SDL_SetTextureAlphaMod(tex2, alpha/2);
		SDL_RenderCopy(mrc, tex2, NULL, &drect2);
		SDL_FreeSurface(surf2);
		// Texture is in the pool, do not destroy
	}

	SDL_RenderCopy(mrc, tex, NULL, &drect);
	SDL_FreeSurface(surf);
	// Texture is in the pool, do not destroy
}
void Font::writeText(int x, int y, const string& _text, int wrapwidth, int alpha)
{
	if (font == 0)
		return;

	/* XXX doesn't look good, why no rendering
	 * into texture directly available? */
	SDL_Surface *surf;
	SDL_Texture *tex;
	SDL_Rect drect;
	const char *text = _text.c_str();

	if (strlen(text) == 0)
		return;

	if ((surf = TTF_RenderUTF8_Blended_Wrapped(font, text, clr, wrapwidth)) == NULL)
		_logsdlerr();
	
	extern SDL_Texture* text_pool_wrapped[2048];
	extern int pool_idx_wrapped;
	
	if (text_pool_wrapped[pool_idx_wrapped]) {
		SDL_DestroyTexture(text_pool_wrapped[pool_idx_wrapped]);
	}
	text_pool_wrapped[pool_idx_wrapped] = create_padded_texture(mrc, surf);
	if (text_pool_wrapped[pool_idx_wrapped] == NULL)
		_logsdlerr();
	tex = text_pool_wrapped[pool_idx_wrapped];
	pool_idx_wrapped = (pool_idx_wrapped + 1) % 2048;

	drect.x = x;
	drect.y = y;
	drect.w = surf->w;
	drect.h = surf->h;
	
	drect.x -= 1;
	drect.y -= 1;
	drect.w += 2;
	drect.h += 2;
	if (alpha < 255)
		SDL_SetTextureAlphaMod(tex, alpha);
	SDL_RenderCopy(mrc, tex, NULL, &drect);
	SDL_FreeSurface(surf);
	// Texture is in the pool, do not destroy
}

void Label::setText(Font &font, const string &str, uint maxw)
{
	if (str == "") {
		empty = true;
		return;
	}

	int b = border;
	if (b == -1)
		b = 20 * font.getSize() / 100;
	int w = 0, h = 0;

	/* get size */
	if (maxw == 0) /* single centered line */
		font.getTextSize(str, &w, &h);
	else
		font.getWrappedTextSize(str, maxw, &w, &h);

	if (w == 0 || h == 0)
		return;

	SDL_Surface *dst_surf = SDL_CreateRGBSurfaceWithFormat(0, w + 4*b, h + 2*b, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!dst_surf) {
		_logsdlerr();
		return;
	}

	SDL_FillRect(dst_surf, NULL, SDL_MapRGBA(dst_surf->format, bgColor.r, bgColor.g, bgColor.b, bgColor.a));

	SDL_Surface *text_surf = NULL;
	if (maxw == 0) text_surf = TTF_RenderUTF8_Blended(font.getFont(), str.c_str(), font.getColor());
	else text_surf = TTF_RenderUTF8_Blended_Wrapped(font.getFont(), str.c_str(), font.getColor(), maxw);

	if (text_surf) {
		SDL_Color shadowColor = {0, 0, 0, 255};
		SDL_Surface *shadow_surf = NULL;
		if (maxw == 0) shadow_surf = TTF_RenderUTF8_Blended(font.getFont(), str.c_str(), shadowColor);
		else shadow_surf = TTF_RenderUTF8_Blended_Wrapped(font.getFont(), str.c_str(), shadowColor, maxw);

		int draw_x = 2*b;
		int draw_y = b;
		if (maxw == 0) {
			draw_x = (dst_surf->w - text_surf->w) / 2;
			draw_y = (dst_surf->h - text_surf->h) / 2;
		}

		if (shadow_surf) {
			SDL_SetSurfaceAlphaMod(shadow_surf, font.getColor().a / 2);
			SDL_SetSurfaceBlendMode(shadow_surf, SDL_BLENDMODE_BLEND);
			SDL_Rect shadow_rect = { draw_x + font.getSize()/10 - 1, draw_y + font.getSize()/10 - 1, shadow_surf->w, shadow_surf->h };
			SDL_BlitSurface(shadow_surf, NULL, dst_surf, &shadow_rect);
			SDL_FreeSurface(shadow_surf);
		}

		SDL_SetSurfaceAlphaMod(text_surf, font.getColor().a);
		SDL_SetSurfaceBlendMode(text_surf, SDL_BLENDMODE_BLEND);
		SDL_Rect text_rect = { draw_x, draw_y, text_surf->w, text_surf->h };
		SDL_BlitSurface(text_surf, NULL, dst_surf, &text_rect);
		SDL_FreeSurface(text_surf);
	}

	img.load(dst_surf);
	SDL_FreeSurface(dst_surf);
	empty = false;
}

void Gamepad::open()
{
#ifdef __WIIU__
    KPADInit();
    numbuttons = 20;
    _loginfo("Opened Wii U GamePad and WPAD/KPAD controllers\n");
#else
	if (js)
		close(); /* make sure none is opened yet */

	if (SDL_NumJoysticks() == 0) {
		_loginfo("No game controller found...\n");
		return;
	}

	if ((js = SDL_JoystickOpen(0)) == NULL) {
		_logerr("Couldn't open game controller: %s\n",SDL_GetError());
		return;
	}

	_loginfo("Opened game controller 0\n");
	_logdebug(1,"  num axes: %d, num buttons: %d, num balls: %d\n",
			SDL_JoystickNumAxes(js),SDL_JoystickNumButtons(js),
			SDL_JoystickNumBalls(js));

	numbuttons = SDL_JoystickNumButtons(js);
	if (numbuttons > 10)
		numbuttons = 10;
#endif
}

void Gamepad::close()
{
#ifdef __WIIU__
    KPADShutdown();
    _loginfo("Closed KPAD controllers\n");
#else
	if (js) {
		SDL_JoystickClose(js);
		_loginfo("Closed game controller 0\n");
		js = NULL;
	}
#endif
}

/** Update joystick state and return state array (0=off,1=on) */
const Uint8 *Gamepad::update() {
#ifdef __WIIU__
    memset(state,0,sizeof(state));
    
    // VPAD (GamePad)
    static VPADStatus vpad_last;
    static bool vpad_valid = false;
    VPADStatus vpad_data;
    VPADReadError vpad_error;
    int vpad_samples = VPADRead(VPAD_CHAN_0, &vpad_data, 1, &vpad_error);
    if (vpad_samples > 0 && vpad_error == VPAD_READ_SUCCESS) {
        vpad_last = vpad_data;
        vpad_valid = true;
    }
    
    if (vpad_valid) {
        if (vpad_last.hold & VPAD_BUTTON_LEFT || vpad_last.leftStick.x < -0.2f) state[GPAD_LEFT] = 1;
        if (vpad_last.hold & VPAD_BUTTON_RIGHT || vpad_last.leftStick.x > 0.2f) state[GPAD_RIGHT] = 1;
        if (vpad_last.hold & VPAD_BUTTON_UP || vpad_last.leftStick.y > 0.2f) state[GPAD_UP] = 1;
        if (vpad_last.hold & VPAD_BUTTON_DOWN || vpad_last.leftStick.y < -0.2f) state[GPAD_DOWN] = 1;
        
        if (vpad_last.hold & VPAD_BUTTON_A) state[GPAD_BUTTON0 + 0] = 1;
        if (vpad_last.hold & VPAD_BUTTON_B) state[GPAD_BUTTON0 + 1] = 1;
        if (vpad_last.hold & VPAD_BUTTON_X) state[GPAD_BUTTON0 + 2] = 1;
        if (vpad_last.hold & VPAD_BUTTON_Y) state[GPAD_BUTTON0 + 3] = 1;
        if (vpad_last.hold & VPAD_BUTTON_L) state[GPAD_BUTTON0 + 4] = 1;
        if (vpad_last.hold & VPAD_BUTTON_R) state[GPAD_BUTTON0 + 5] = 1;
        if (vpad_last.hold & VPAD_BUTTON_ZL) state[GPAD_BUTTON0 + 6] = 1;
        if (vpad_last.hold & VPAD_BUTTON_ZR) state[GPAD_BUTTON0 + 7] = 1;
        if (vpad_last.hold & VPAD_BUTTON_PLUS) state[GPAD_BUTTON0 + 8] = 1;
        if (vpad_last.hold & VPAD_BUTTON_MINUS) state[GPAD_BUTTON0 + 9] = 1;

        // Touch screen mapping
        static uint16_t last_touched = 0;
        static int last_mx = -1, last_my = -1;
        static Uint8 last_mouse_button = SDL_BUTTON_LEFT;
        VPADTouchData tpCalibrated;
        VPADGetTPCalibratedPointEx(VPAD_CHAN_0, VPAD_TP_854X480, &tpCalibrated, &vpad_last.tpFiltered1);
        
        bool is_touched = (tpCalibrated.validity == VPAD_VALID) && vpad_last.tpFiltered1.touched;
        
        if (is_touched) {
            int mx = (tpCalibrated.x * Geom::sw) / 854;
            int my = (tpCalibrated.y * Geom::sh) / 480;
            
            bool is_rmb = (vpad_last.hold & VPAD_BUTTON_ZL) != 0;
            
            if (last_mx != mx || last_my != my) {
                SDL_Event ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = SDL_MOUSEMOTION;
                ev.motion.state = is_rmb ? SDL_BUTTON_RMASK : SDL_BUTTON_LMASK;
                ev.motion.x = mx;
                ev.motion.y = my;
                SDL_PushEvent(&ev);
            }
            
            if (!last_touched) {
                last_mouse_button = is_rmb ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
                SDL_Event ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = SDL_MOUSEBUTTONDOWN;
                ev.button.button = last_mouse_button;
                ev.button.state = SDL_PRESSED;
                ev.button.x = mx;
                ev.button.y = my;
                SDL_PushEvent(&ev);
            }
            
            last_mx = mx;
            last_my = my;
        } else if (last_touched) {
            SDL_Event ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = SDL_MOUSEBUTTONUP;
            ev.button.button = last_mouse_button;
            ev.button.state = SDL_RELEASED;
            ev.button.x = last_mx != -1 ? last_mx : 0;
            ev.button.y = last_my != -1 ? last_my : 0;
            SDL_PushEvent(&ev);
            
            last_mx = -1;
            last_my = -1;
        }
        last_touched = is_touched;
    }

    // KPAD (Wiimote / Pro Controller)
    static KPADStatus kpad_last[4];
    static bool kpad_valid[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) {
        KPADStatus kpad_data;
        if (KPADRead((KPADChan)i, &kpad_data, 1) > 0) {
            if (kpad_data.error == KPAD_ERROR_OK) {
                kpad_last[i] = kpad_data;
                kpad_valid[i] = true;
            }
        }
        
        if (kpad_valid[i]) {
            // Determine buttons based on extension type
            uint32_t hold = kpad_last[i].hold;
            if (kpad_last[i].extensionType == WPAD_EXT_PRO_CONTROLLER) {
                if (hold & WPAD_PRO_BUTTON_LEFT || kpad_last[i].pro.leftStick.x < -0.2f) state[GPAD_LEFT] = 1;
                if (hold & WPAD_PRO_BUTTON_RIGHT || kpad_last[i].pro.leftStick.x > 0.2f) state[GPAD_RIGHT] = 1;
                if (hold & WPAD_PRO_BUTTON_UP || kpad_last[i].pro.leftStick.y > 0.2f) state[GPAD_UP] = 1;
                if (hold & WPAD_PRO_BUTTON_DOWN || kpad_last[i].pro.leftStick.y < -0.2f) state[GPAD_DOWN] = 1;
                
                if (hold & WPAD_PRO_BUTTON_A) state[GPAD_BUTTON0 + 0] = 1;
                if (hold & WPAD_PRO_BUTTON_B) state[GPAD_BUTTON0 + 1] = 1;
                if (hold & WPAD_PRO_BUTTON_X) state[GPAD_BUTTON0 + 2] = 1;
                if (hold & WPAD_PRO_BUTTON_Y) state[GPAD_BUTTON0 + 3] = 1;
                if (hold & WPAD_PRO_TRIGGER_L) state[GPAD_BUTTON0 + 4] = 1;
                if (hold & WPAD_PRO_TRIGGER_R) state[GPAD_BUTTON0 + 5] = 1;
                if (hold & WPAD_PRO_TRIGGER_ZL) state[GPAD_BUTTON0 + 6] = 1;
                if (hold & WPAD_PRO_TRIGGER_ZR) state[GPAD_BUTTON0 + 7] = 1;
                if (hold & WPAD_PRO_BUTTON_PLUS) state[GPAD_BUTTON0 + 8] = 1;
                if (hold & WPAD_PRO_BUTTON_MINUS) state[GPAD_BUTTON0 + 9] = 1;
            } else if (kpad_last[i].extensionType == WPAD_EXT_CLASSIC) {
                if (hold & WPAD_CLASSIC_BUTTON_LEFT || kpad_last[i].classic.leftStick.x < -0.2f) state[GPAD_LEFT] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_RIGHT || kpad_last[i].classic.leftStick.x > 0.2f) state[GPAD_RIGHT] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_UP || kpad_last[i].classic.leftStick.y > 0.2f) state[GPAD_UP] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_DOWN || kpad_last[i].classic.leftStick.y < -0.2f) state[GPAD_DOWN] = 1;
                
                if (hold & WPAD_CLASSIC_BUTTON_A) state[GPAD_BUTTON0 + 0] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_B) state[GPAD_BUTTON0 + 1] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_X) state[GPAD_BUTTON0 + 2] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_Y) state[GPAD_BUTTON0 + 3] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_L) state[GPAD_BUTTON0 + 4] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_R) state[GPAD_BUTTON0 + 5] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_ZL) state[GPAD_BUTTON0 + 6] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_ZR) state[GPAD_BUTTON0 + 7] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_PLUS) state[GPAD_BUTTON0 + 8] = 1;
                if (hold & WPAD_CLASSIC_BUTTON_MINUS) state[GPAD_BUTTON0 + 9] = 1;
            } else if (kpad_last[i].extensionType == WPAD_EXT_NUNCHUK) {
                // Wiimote held vertically with Nunchuk
                if (hold & WPAD_BUTTON_LEFT || kpad_last[i].nunchuk.stick.x < -0.2f) state[GPAD_LEFT] = 1;
                if (hold & WPAD_BUTTON_RIGHT || kpad_last[i].nunchuk.stick.x > 0.2f) state[GPAD_RIGHT] = 1;
                if (hold & WPAD_BUTTON_UP || kpad_last[i].nunchuk.stick.y > 0.2f) state[GPAD_UP] = 1;
                if (hold & WPAD_BUTTON_DOWN || kpad_last[i].nunchuk.stick.y < -0.2f) state[GPAD_DOWN] = 1;
                if (hold & WPAD_BUTTON_A) state[GPAD_BUTTON0 + 0] = 1;
                if (hold & WPAD_BUTTON_B) state[GPAD_BUTTON0 + 1] = 1;
                if (hold & WPAD_BUTTON_PLUS) state[GPAD_BUTTON0 + 8] = 1;
                if (hold & WPAD_BUTTON_MINUS) state[GPAD_BUTTON0 + 9] = 1;
            } else {
                // Standard Wiimote held horizontally
                if (hold & WPAD_BUTTON_UP) state[GPAD_LEFT] = 1;
                if (hold & WPAD_BUTTON_DOWN) state[GPAD_RIGHT] = 1;
                if (hold & WPAD_BUTTON_LEFT) state[GPAD_DOWN] = 1;
                if (hold & WPAD_BUTTON_RIGHT) state[GPAD_UP] = 1;
                if (hold & WPAD_BUTTON_2) state[GPAD_BUTTON0 + 0] = 1; // 2 -> A
                if (hold & WPAD_BUTTON_1) state[GPAD_BUTTON0 + 1] = 1; // 1 -> B
                if (hold & WPAD_BUTTON_PLUS) state[GPAD_BUTTON0 + 8] = 1;
                if (hold & WPAD_BUTTON_MINUS) state[GPAD_BUTTON0 + 9] = 1;
            }
        }
    }
#else
	SDL_JoystickUpdate();

	memset(state,0,sizeof(state));
	if (js == NULL)
		return state;

	if (SDL_JoystickGetAxis(js,0) < -3200)
		state[GPAD_LEFT] = 1;
	if (SDL_JoystickGetAxis(js,0) > 3200)
		state[GPAD_RIGHT] = 1;
	if (SDL_JoystickGetAxis(js,1) < -3200)
		state[GPAD_UP] = 1;
	if (SDL_JoystickGetAxis(js,1) > 3200)
		state[GPAD_DOWN] = 1;
	for (uint i = 0; i < numbuttons; i++)
		state[GPAD_BUTTON0 + i] = SDL_JoystickGetButton(js,i);
#endif
	return state;
}

/** Run a standalone dialog for editing a UTF8 string. ESC cancels editing
 * (string is not changed), Enter confirms changes. Return 1 if string was
 * changed, 0 if not changed, -1 if quit requested. */


#ifdef __WIIU__
int runArcadeEditDialog(Font *font, Font *fontFocus, const string &caption, string &str) {
	const char* chars[4] = {
		"ABCDEFGHIJKLM",
		"NOPQRSTUVWXYZ",
		"0123456789.-_",
		" < * > " // < = DEL, * = OK, > = CANCEL
	};
	int cx = 0, cy = 0;
	bool done = false;
	int ret = 0;
	string backup = str;
	Image img;
	img.createFromScreen();
	img.setAlpha(64);
	
	// flush gamepad first
	while (gamepad.opened()) {
		const Uint8 *state = gamepad.update();
		bool any = false;
		for (int i=0; i<GPAD_LAST1; i++) if (state[i]) any = true;
		if (!any) break;
		SDL_Delay(10);
	}
	
	Uint8 last_state[GPAD_LAST1] = {0};
	int sw = img.getWidth();
	int sh = img.getHeight();
	
	Label lblCharsLower[4][13];
	Label lblCharsUpper[4][13];
	Label lblCharsFocusLower[4][13];
	Label lblCharsFocusUpper[4][13];
	Label lblCaption(true);
	bool is_lower = false;
	
	for (int y=0; y<4; y++) {
		for (int x=0; x < (y==3 ? 3 : 13); x++) {
			lblCharsLower[y][x].setBgColor({0,0,0,0});
			lblCharsLower[y][x].setBorder(0);
			lblCharsUpper[y][x].setBgColor({0,0,0,0});
			lblCharsUpper[y][x].setBorder(0);
			lblCharsFocusLower[y][x].setBgColor({0,0,0,0});
			lblCharsFocusLower[y][x].setBorder(0);
			lblCharsFocusUpper[y][x].setBgColor({0,0,0,0});
			lblCharsFocusUpper[y][x].setBorder(0);
			
			string sU, sL;
			if (y == 3) {
				if (x == 0) sU = sL = "DEL";
				else if (x == 1) sU = sL = "OK";
				else if (x == 2) sU = sL = "CANCEL";
			} else {
				char c = chars[y][x];
				sU = c;
				if (c >= 'A' && c <= 'Z') c += 32;
				sL = c;
			}
			lblCharsUpper[y][x].setText(*font, sU);
			lblCharsLower[y][x].setText(*font, sL);
			lblCharsFocusUpper[y][x].setText(*fontFocus, sU);
			lblCharsFocusLower[y][x].setText(*fontFocus, sL);
		}
	}
	lblCaption.setBgColor({0,0,0,0});
	lblCaption.setBorder(0);
	lblCaption.setText(*font, caption + ": " + str);
	
	while (!done) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) { done = true; ret = -1; }
		}
		
		const Uint8 *state = gamepad.update();
		
		#define PRESSED(btn) (state[btn] && !last_state[btn])
		if (PRESSED(GPAD_UP)) { cy--; if (cy < 0) cy = 3; if (cy == 3 && cx > 2) cx = 2; }
		if (PRESSED(GPAD_DOWN)) { cy++; if (cy > 3) cy = 0; if (cy == 3 && cx > 2) cx = 2; }
		if (PRESSED(GPAD_LEFT)) { cx--; if (cx < 0) cx = (cy == 3 ? 2 : 12); }
		if (PRESSED(GPAD_RIGHT)) { cx++; if (cx > (cy == 3 ? 2 : 12)) cx = 0; }
		
		if (PRESSED(GPAD_BUTTON0 + 2) || PRESSED(GPAD_BUTTON0 + 3)) { // X or Y button
			is_lower = !is_lower;
		}
		
		if (PRESSED(GPAD_BUTTON0 + 0)) { // A button
			if (cy == 3) {
				if (cx == 0) { // DEL
					if (str.length() > 0) {
						str = str.substr(0, str.length()-1);
						lblCaption.setText(*font, caption + ": " + str);
					}
				} else if (cx == 1) { // OK
					ret = 1; done = true;
				} else { // CANCEL
					str = backup; ret = 0; done = true;
				}
			} else {
				char c = chars[cy][cx];
				if (is_lower && c >= 'A' && c <= 'Z') c += 32;
				if (c != ' ' && str.length() < 20) {
					str += c;
					lblCaption.setText(*font, caption + ": " + str);
				}
			}
		}
		if (PRESSED(GPAD_BUTTON0 + 1)) { // B button (Cancel)
			str = backup; ret = 0; done = true;
		}
		if (PRESSED(GPAD_BUTTON0 + 8)) { // PLUS (OK)
			ret = 1; done = true;
		}
		
		memcpy(last_state, state, sizeof(last_state));
		
		SDL_SetRenderDrawColor(mrc,0,0,0,255);
		SDL_RenderClear(mrc);
		img.copy();
		
		lblCaption.copy(sw/2, sh/2 - 100);
		
		for (int y=0; y<4; y++) {
			for (int x=0; x < (y==3 ? 3 : 13); x++) {
				int draw_x = sw/2 - 180 + x*30;
				int draw_y = sh/2 + y*40;
				if (y == 3) {
					if (x == 0) draw_x = sw/2 - 100;
					else if (x == 1) draw_x = sw/2;
					else if (x == 2) draw_x = sw/2 + 100;
				}
				
				if (cx == x && cy == y) {
					if (is_lower) lblCharsFocusLower[y][x].copy(draw_x, draw_y);
					else lblCharsFocusUpper[y][x].copy(draw_x, draw_y);
				} else {
					if (is_lower) lblCharsLower[y][x].copy(draw_x, draw_y);
					else lblCharsUpper[y][x].copy(draw_x, draw_y);
				}
			}
		}
		
		SDL_RenderPresent(mrc);
		SDL_Delay(10);
	}
	return ret;
}
#endif

int runEditDialog(Font &font, const string &caption, string &str)
{
#ifdef __WIIU__
	return runArcadeEditDialog(&font, &font, caption, str);
#else
	int ret = 0;
	SDL_Event event;
	string backup = str;
	Image img;
	bool done = false;

	img.createFromScreen();
	img.setAlpha(64);

	font.setAlign(ALIGN_X_CENTER | ALIGN_Y_CENTER);

	SDL_StartTextInput();
	while (!done) {
		if (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				done = true;
				ret = -1;
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_ESCAPE:
					str = backup;
					done = true;
					break;
				case SDL_SCANCODE_RETURN:
					ret = 1;
					done = true;
					break;
				case SDL_SCANCODE_BACKSPACE:
					if (str.length()>0)
						str = str.substr(0, str.length()-1);
					break;
				default: break;
				}
				break;
			case SDL_TEXTINPUT:
				str += event.text.text;
				break;
			}
		}

		/* redraw */
		SDL_SetRenderDrawColor(mrc,0,0,0,255);
		SDL_RenderClear(mrc);
		img.copy();
		string text(caption + ": " + str);
		font.write(img.getWidth()/2,img.getHeight()/2,text);
		SDL_RenderPresent(mrc);
		SDL_Delay(10);
		FlushUselessEvents();
	}
	SDL_StopTextInput();
	return ret;
#endif
}

/** Run standalone confirm dialog. Return 1 if confirmed
 * changed, 0 if not, -1 if quit requested. */
int runConfirmDialog(Font &font, const string &caption)
{
	Image img;

	img.createFromScreen();
	img.setAlpha(32);
	SDL_SetRenderDrawColor(mrc,0,0,0,255);
	SDL_RenderClear(mrc);
	img.copy();
	font.setAlign(ALIGN_X_CENTER | ALIGN_Y_CENTER);
	font.write(img.getWidth()/2, img.getHeight()/2, caption);

	return waitForConfirmation();
}

/** Wait for confirmation key.
 * Returns 1 = confirmed, 0 = cancel, -1 = quit requested.
 * The render target must not have been presented yet otherwise we get
 * the wrong buffer content for createFromScreen(). */
int waitForConfirmation()
{
	int ret = 0;
	bool done = false;
	bool esc_pressed = false;
	SDL_Event event;
	Image sshot;

	sshot.createFromScreen();
	SDL_RenderPresent(mrc);

	while (!done) {
		bool newEvent = false;
		if (SDL_PollEvent(&event)) newEvent = true;
#ifdef __WIIU__
		static Uint8 prev_gpadstate[GPAD_LAST1] = {0};
		const Uint8 *gpadstate = gamepad.update();
		
		if (!newEvent || event.type != SDL_QUIT) {
			SDL_Scancode code = SDL_SCANCODE_UNKNOWN;
			if (gpadstate[GPAD_BUTTON0 + 0] && !prev_gpadstate[GPAD_BUTTON0 + 0]) code = SDL_SCANCODE_ESCAPE; // A button -> NO (cancel)
			else if (gpadstate[GPAD_BUTTON0 + 1] && !prev_gpadstate[GPAD_BUTTON0 + 1]) code = SDL_SCANCODE_RETURN; // B button -> YES (confirm)
			if (code != SDL_SCANCODE_UNKNOWN) {
				event.type = SDL_KEYDOWN;
				event.key.keysym.scancode = code;
				newEvent = true;
			}
		}
		memcpy(prev_gpadstate, gpadstate, GPAD_LAST1);
#endif
		if (newEvent) {
			switch (event.type) {
			case SDL_QUIT:
				done = true;
				ret = -1;
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_ESCAPE: /* ESC = cancel */
					done = true;
					esc_pressed = true;
					break;
				case SDL_SCANCODE_RETURN: /* Return = confirm */
					ret = 1;
					done = true;
					break;
				default: break;
				}
				break;
			case SDL_TEXTINPUT:
				/* check UTF-8 text input against single character strings */
				if (string(event.text.text) == string(_("y"))) {
					done = true;
					ret = 1;
				} else if (string(event.text.text) == string(_("n"))) {
					done = true;
					ret = 0;
				}
				break;
			}
		}

		SDL_Delay(20);
		sshot.copy();
		SDL_RenderPresent(mrc);
		FlushUselessEvents();
	}

	/* prevent ESC loop */
#ifndef __WIIU__
	if (esc_pressed) {
		done = false;
		while (!done) {
			if (SDL_WaitEvent(&event) && event.type == SDL_KEYUP
				&& event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
				done = true;
		}
	}
#endif

	return ret;
}
