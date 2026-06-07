#ifndef PIXFMT_H
#define PIXFMT_H

#include <stdbool.h>
#include <stdint.h>
#include <libavutil/pixfmt.h>

#include "common.h"
#include "hash.h"

static bool pset = false;
static umap_t *pixfmt = NULL;

static void init_pixfmt() {
  if (pset) return;
  pixfmt = new_umap(1024, djb2);
  non_static_call(pixfmt, push, new_kv("rgb8",    (void *)(intptr_t)AV_PIX_FMT_RGB24));
  non_static_call(pixfmt, push, new_kv("yuv420p", (void *)(intptr_t)AV_PIX_FMT_YUV420P));
  pset = true;
}

#endif
