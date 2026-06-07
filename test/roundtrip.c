#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encoder.h"
#include "decoder.h"
#include "opts.h"

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "Usage: %s {input_video} {output_file} {config_file}\n", argv[0]);
    return 1;
  }

  const char *in_fname  = argv[1];
  const char *out_fname = argv[2];
  const char *cfg_fname = argv[3];

  decopts_t *dopts = malloc(sizeof(decopts_t));
  memset(dopts, 0, sizeof(decopts_t));
  dopts->fmt = strdup("yuv420p");

  decoder_t *in_dec = new_decoder(in_fname, dopts);
  free_decopts(dopts);
  if (!in_dec) {
    fprintf(stderr, "FAIL: could not open decoder for '%s'\n", in_fname);
    return 1;
  }

  uint32_t W = in_dec->width;
  uint32_t H = in_dec->height;

  printf("input: %s (%ux%u, %.2f fps, %lu frames)\n",
         in_fname, W, H, in_dec->fps, (unsigned long)in_dec->frames);

  encopts_t *opts = new_encopts(cfg_fname);
  if (!opts) {
    fprintf(stderr, "FAIL: could not load config '%s'\n", cfg_fname);
    free_decoder(in_dec);
    return 1;
  }

  printf("config: lib=%s, %ux%u, %.1f fps, fmt=%s\n",
         opts->lib, opts->width, opts->height, opts->fps, opts->fmt);

  encoder_t *enc = new_encoder(out_fname, opts);
  if (!enc) {
    fprintf(stderr, "FAIL: could not create encoder\n");
    free_encopts(opts);
    free_decoder(in_dec);
    return 1;
  }

  if (!enc->open(enc)) {
    fprintf(stderr, "FAIL: encoder open failed\n");
    free_encoder(enc);
    free_decoder(in_dec);
    return 1;
  }

  uint64_t total_enc = 0;
  while (1) {
    uint8_t *buf = in_dec->next(in_dec);
    if (!buf) break;
    if (!enc->encode(enc, buf)) {
      fprintf(stderr, "FAIL: encode frame %lu\n", (unsigned long)total_enc);
      free(buf);
      free_encoder(enc);
      free_encopts(opts);
      free_decoder(in_dec);
      return 1;
    }
    free(buf);
    total_enc++;
  }
  free_decoder(in_dec);
  free_encoder(enc);
  free_encopts(opts);

  if (total_enc == 0) {
    fprintf(stderr, "FAIL: no frames decoded\n");
    return 1;
  }

  printf("encoded %lu frames -> %s\n", (unsigned long)total_enc, out_fname);

  decoder_t *out_dec = new_decoder(out_fname, NULL);
  if (!out_dec) {
    fprintf(stderr, "FAIL: could not open output '%s'\n", out_fname);
    return 1;
  }

  if (out_dec->width != W) {
    fprintf(stderr, "FAIL: width %u != %u\n", out_dec->width, W);
    free_decoder(out_dec);
    return 1;
  }

  if (out_dec->height != H) {
    fprintf(stderr, "FAIL: height %u != %u\n", out_dec->height, H);
    free_decoder(out_dec);
    return 1;
  }

  uint64_t total_dec = 0;
  while (1) {
    uint8_t *buf = out_dec->next(out_dec);
    if (!buf) break;
    free(buf);
    total_dec++;
  }
  free_decoder(out_dec);

  if (total_dec != total_enc) {
    fprintf(stderr, "FAIL: frame count %lu != %lu\n",
            (unsigned long)total_dec, (unsigned long)total_enc);
    return 1;
  }

  printf("output: %s (%ux%u, %lu frames) PASS\n",
         out_fname, W, H, (unsigned long)total_dec);
  return 0;
}
