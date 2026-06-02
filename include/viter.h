#ifndef VITER_H
#define VITER_H

#include <stdbool.h>
#include <stdint.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>

#include "common.h"

typedef struct viter viter_t;

struct viter {
  char *in_filename;

  // ffmpeg context
  AVFormatContext *format_ctx;

  bool (*has_next)(viter_t *this);
  uint8_t *(*next)(viter_t *this);
};

viter_t *new_viter(
  const char *in_filename);

#endif
