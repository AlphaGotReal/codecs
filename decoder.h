#ifndef DECODER_H
#define DECODER_H

#include <stdbool.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

struct SwsContext;

typedef struct decoder decoder_t;

struct decoder {
  char     *fname;
  double   fps;
  int64_t  duration;
  uint64_t frames;

  uint32_t width;
  uint32_t height;

  size_t   _vidx;
  bool     _exhausted;

  enum AVPixelFormat target_fmt;
  struct SwsContext  *sws_ctx;

  AVFormatContext   *fmt_ctx;
  AVStream          *stream;
  AVCodecParameters *cparams;

  AVCodecContext    *dec_ctx;
  AVFrame           *frame;
  AVPacket          *pkt;

  bool     (*end)(decoder_t *);
  bool     (*close)(decoder_t *);
  uint8_t *(*next)(decoder_t *);
};

decoder_t *new_decoder(const char *fname, const char *fmt);

void free_decoder(decoder_t *it);

#endif
