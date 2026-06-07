#include <stdlib.h>
#include <string.h>

#include "pixfmt.h"

bool pset = false;
umap_t *pixfmt = NULL;

void init_pixfmt(void) {
  if (pset) return;
  pixfmt = new_umap(1024, djb2);

  enum AVPixelFormat *v;

  v = malloc(sizeof(*v)); *v = AV_PIX_FMT_RGB24;
  pixfmt->push(pixfmt, new_kv(strdup("rgb8"), v));

  v = malloc(sizeof(*v)); *v = AV_PIX_FMT_BGR24;
  pixfmt->push(pixfmt, new_kv(strdup("bgr8"), v));

  v = malloc(sizeof(*v)); *v = AV_PIX_FMT_YUV420P;
  pixfmt->push(pixfmt, new_kv(strdup("yuv420p"), v));

  pset = true;
}

void free_pixfmt(void) {
  if (!pset) return;
  free_umap(pixfmt);
  pixfmt = NULL;
  pset = false;
}
