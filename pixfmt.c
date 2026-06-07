#include <stdlib.h>
#include <string.h>

#include "pixfmt.h"

bool pset = false;
umap_t *pixfmt = NULL;

void init_pixfmt(void) {
  if (pset) return;
  pixfmt = new_umap(1024, djb2);
  pixfmt->push(pixfmt, new_kv(strdup("rgb8"),    (void *)(intptr_t)AV_PIX_FMT_RGB24));
  pixfmt->push(pixfmt, new_kv(strdup("yuv420p"), (void *)(intptr_t)AV_PIX_FMT_YUV420P));
  pset = true;
}

void free_pixfmt(void) {
  if (!pset) return;
  free_umap(pixfmt);
  pixfmt = NULL;
  pset = false;
}
