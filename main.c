#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <SDL.h>

#define FONT_HEIGHT 32

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int main(int argc, char **argv) {

    /* getting the font into memory. this uses mmap, but a fread variant would
     * work fine */
    int fontfd = open("DroidSansMono.ttf", O_RDONLY);
    if (fontfd < 0) {
        perror("couldn't open font file");
        exit(1);
    }

    struct stat st;
    if (fstat(fontfd, &st) < 0) {
        perror("couldn't stat font file");
        close(fontfd);
        exit(1);
    }

    void *fontdata = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fontfd, 0);
    if (!fontdata) {
        perror("couldn't map font file");
        close(fontfd);
        exit(1);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "sdl init failed: %s\n", SDL_GetError());
        munmap(fontdata, st.st_size);
        close(fontfd);
        exit(1);
    }

    /* creating an off-screen surface to render the glyphs into. stbtt outputs
     * the glyphs in 8-bit greyscale, so we want a 8-bit surface to match */
    SDL_Surface * glyphdata = SDL_CreateRGBSurface(0, 512, 512, 8, 0, 0, 0, 0);
    if (!glyphdata) {
        fprintf(stderr, "couldn't create sdl buffer: %s\n", SDL_GetError());
        munmap(fontdata, st.st_size);
        close(fontfd);
        SDL_Quit();
        exit(1);
    }

    /* 8-bit sdl surfaces are indexed (palletised), so setup a pallete with
     * 256 shades of grey. this is needed so the sdl blitter has something to
     * convert from when blitting to a direct colour surface */
SDL_Color * colors = glyphdata->format->palette->colors;
    for(int i = 0; i < 256; i++){
        colors[i].r = i;
        colors[i].g = i;
        colors[i].b = i;
    }

    /* "bake" (render) lots of interesting glyphs into the bitmap. the cdata
     * array ends up with metrics for each glyph */
    stbtt_bakedchar cdata[96];
    stbtt_BakeFontBitmap(fontdata, stbtt_GetFontOffsetForIndex(fontdata, 0), FONT_HEIGHT, glyphdata->pixels, 512, 512, 32, 96, cdata);

    /* done with the raw font data now */
    munmap(fontdata, st.st_size);
    close(fontfd);

int app_width = 640;
int app_height = 480;
char * app_title = "test";

SDL_Window * window = SDL_CreateWindow(
		app_title, 
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		app_width,
		app_height,
		0);
SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

SDL_RenderSetLogicalSize(renderer, app_width, app_height);

SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

SDL_RenderPresent(renderer);

Uint32 rmask, gmask, bmask, amask;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

SDL_Surface * s = SDL_CreateRGBSurface(0, app_width, app_height, 32, rmask, gmask, bmask, amask);

SDL_FillRect(s, NULL, 0xff0000ff);

    /* the actual text draw. we loop over the characters, find the
     * corresponding glyph and blit it to the correct place in the on-screen
     * surface */

    /* x and y are the position in the dest surface to blit the next glyph to */
    float x = 0, y = 0;
    for (char *c = "Hello"; *c; c++) {
        /* stbtt_aligned_quad effectively holds a source and destination
         * rectangle for the glyph. we get one for the current char */
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(cdata, 512, 512, *c-32, &x, &y, &q, 1);

        /* now convert from stbtt_aligned_quad to source/dest SDL_Rects */

        /* width and height are simple */
        int w = q.x1 - q.x0;
        int h = q.y1 - q.y0;

        /* t0,s0 and t1,s1 are texture-space coordinates, that is floats from
         * 0.0-1.0. we have to scale them back to the pixel space used in the
         * glyph data bitmap. its a simple as multiplying by the glyph bitmap
         * dimensions */
        SDL_Rect src  = { .x = q.s0*512, .y = q.t0*512, .w = w, .h = h };

        /* in gl/d3d the y value is inverted compared to what sdl expects. y0
         * is negative here. we add (subtract) it to the baseline to get the
         * correct "top" position to blit to */
        SDL_Rect dest = { .x = q.x0, .y = FONT_HEIGHT+q.y0, .w = w, .h = h };

        /* draw it */
        SDL_BlitSurface(glyphdata, &src, s, &dest);
    }

SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, s);

SDL_RenderCopy(renderer, texture, NULL, NULL);
SDL_RenderPresent(renderer);

    /* done with the glyphdata now */
    SDL_FreeSurface(glyphdata);

    /* wait for escape */
    SDL_Event e;
    while(SDL_WaitEvent(&e) && e.type != SDL_KEYDOWN && e.key.keysym.sym != SDLK_ESCAPE);

    SDL_FreeSurface(s);
    SDL_Quit();

    exit(0);
}

