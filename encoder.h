#ifndef ENCODER_H
#define ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "hash.h"
#include "opts.h"

typedef struct encoder encoder_t;

struct encoder {
  const char *fname;
  opts_t *opt;

  umap_t *qfmt; // string -> fmt

  /* private members */
  AVFormatContext *fmt_ctx;
  AVCodecContext  *codec_ctx;
  AVFrame         *frame;
  AVPacket        *pkt;
  AVStream        *stream;
  int64_t          pts;

  /* non static methods */
  bool (*open)(encoder_t *);
  bool (*encode)(encoder_t *, const uint8_t *);
  bool (*flush)(encoder_t *);
  bool (*close)(encoder_t *);
};

encoder_t *new_encoder(
  const char * /* fname */, 
  opts_t * /* opt */);

void free_encoder(
  encoder_t * /* enc */);

#endif
