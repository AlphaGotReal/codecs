#ifndef PIXFMT_H
#define PIXFMT_H

#include <stdbool.h>
#include <stdint.h>
#include <libavutil/pixfmt.h>

#include "hash.h"

extern bool pset;
extern umap_t *pixfmt;

void init_pixfmt(void);
void free_pixfmt(void);

#endif
