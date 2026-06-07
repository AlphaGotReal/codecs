#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "decoder.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <video_file>\n", argv[0]);
    return 1;
  }

  const char *fname = argv[1];

  decoder_t *dec = new_decoder(fname, "rgb8");
  if (!dec) {
    fprintf(stderr, "could not open decoder for '%s'\n", fname);
    return 1;
  }

  int W    = (int)dec->width;
  int H    = (int)dec->height;
  double fps = dec->fps;
  int delay = fps > 0.0 ? (int)(1000.0 / fps) : 33;
  int pitch = W * 3;

  int win_w = W, win_h = H;
  int max_w = 1280, max_h = 720;
  if (win_w > max_w || win_h > max_h) {
    double scale = (double)max_w / win_w;
    if ((double)max_h / win_h < scale) scale = (double)max_h / win_h;
    win_w = (int)(win_w * scale);
    win_h = (int)(win_h * scale);
  }

  SDL_Init(SDL_INIT_VIDEO);

  SDL_Window *win = SDL_CreateWindow("player",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      win_w, win_h, SDL_WINDOW_RESIZABLE);

  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

  SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24,
      SDL_TEXTUREACCESS_STREAMING, W, H);

  int running = 1;

  while (running) {
    uint32_t tick = SDL_GetTicks();

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = 0;
      } else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)
          running = 0;
      }
    }

    uint8_t *buf = dec->next(dec);
    if (!buf) {
      free_decoder(dec);
      dec = new_decoder(fname, "rgb8");
      if (!dec) {
        fprintf(stderr, "could not reopen decoder\n");
        break;
      }
      continue;
    }

    SDL_UpdateTexture(tex, NULL, buf, pitch);
    free(buf);

    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);

    uint32_t elapsed = SDL_GetTicks() - tick;
    if (elapsed < (uint32_t)delay)
      SDL_Delay(delay - elapsed);
  }

  free_decoder(dec);
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();

  return 0;
}
