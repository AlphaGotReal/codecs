#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <SDL2/SDL.h>

#include "decoder.h"
#include "opts.h"

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
  (void)sig;
  running = 0;
}

static Uint32 lookup_fmt(const char *fmt, int W, int *pitch) {
  if (strcmp(fmt, "yuv420p") == 0) {
    *pitch = W;
    return SDL_PIXELFORMAT_IYUV;
  }
  if (strcmp(fmt, "bgr8") == 0) {
    *pitch = W * 3;
    return SDL_PIXELFORMAT_BGR24;
  }
  *pitch = W * 3;
  return SDL_PIXELFORMAT_RGB24;
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s <video_file> [dec_config.yaml]\n", argv[0]);
    return 1;
  }

  const char *fname = argv[1];
  const char *cfg   = argc >= 3 ? argv[2] : NULL;

  decopts_t *dopts = cfg ? new_decopts(cfg) : malloc(sizeof(decopts_t));
  if (dopts == NULL) {
    fprintf(stderr, "could not create decoder opts\n");
    return 1;
  }
  if (cfg == NULL) {
    memset(dopts, 0, sizeof(decopts_t));
    dopts->fmt = strdup("rgb8");
  }
  char *saved_fmt = strdup(dopts->fmt ? dopts->fmt : "rgb8");

  decoder_t *dec = new_decoder(fname, dopts);
  if (!dec) {
    fprintf(stderr, "could not open decoder for '%s'\n", fname);
    free(saved_fmt);
    return 1;
  }

  int W    = (int)dec->width;
  int H    = (int)dec->height;
  double fps = dec->fps;
  int delay = fps > 0.0 ? (int) (1000.0 / fps) : 33;
  int pitch;
  Uint32 sdl_fmt = lookup_fmt(saved_fmt, W, &pitch);

  signal(SIGINT, handle_sigint);

  SDL_Init(SDL_INIT_VIDEO);

  SDL_Window *win = SDL_CreateWindow("player",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      W, H, SDL_WINDOW_RESIZABLE);

  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

  SDL_Texture *tex = SDL_CreateTexture(ren, sdl_fmt,
      SDL_TEXTUREACCESS_STREAMING, W, H);
  free(saved_fmt);

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
    if (!running) break;

    uint8_t *buf = dec->next(dec);
    if (!buf) {
      free_decoder(dec);
      dopts = cfg ? new_decopts(cfg) : malloc(sizeof(decopts_t));
      if (dopts == NULL) break;
      if (cfg == NULL) {
        memset(dopts, 0, sizeof(decopts_t));
        dopts->fmt = strdup("rgb8");
      }
      dec = new_decoder(fname, dopts);
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
