#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "decoder.h"
#include "opts.h"

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
  }

  decoder_t *dec = new_decoder(fname, dopts);
  if (!dec) {
    fprintf(stderr, "could not open decoder for '%s'\n", fname);
    if (cfg == NULL) free_decopts(dopts);
    return 1;
  }

  double wall_start = rnow;
  double cpu_start  = cnow;

  uint64_t nframes = 0;
  while (1) {
    uint8_t *buf = dec->next(dec);
    if (!buf) break;
    free(buf);
    nframes++;
  }

  double wall_end = rnow;
  double cpu_end  = cnow;

  double wall_elapsed = wall_end - wall_start;
  double cpu_elapsed  = cpu_end - cpu_start;

  printf("file   : %s\n", fname);
  if (dec->dopts && dec->dopts->fmt)
    printf("fmt     : %s\n", dec->dopts->fmt);
  else
    printf("fmt     : native\n");
  printf("frames: %lu\n", (unsigned long) nframes);
  printf("wall  : %.6f s\n", wall_elapsed);
  printf("cpu   : %.6f s\n", cpu_elapsed);
  if (nframes > 0) {
    printf("avg wall/frame: %.3f ms\n", wall_elapsed / nframes * 1000.0);
    printf("avg cpu/frame : %.3f ms\n", cpu_elapsed / nframes * 1000.0);
  }

  free_decoder(dec);
  return 0;
}
