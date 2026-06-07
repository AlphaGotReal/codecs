#ifndef OPTS_H
#define OPTS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct encopts encopts_t;

struct encopts {
  char *lib;
  double fps;
  char *fmt;

  uint32_t height;
  uint32_t width;

  size_t n;
  char **K;
  char **V;
};

encopts_t *new_encopts(const char *fname);
void free_encopts(encopts_t *opts);

typedef struct decopts decopts_t;

struct decopts {
  char  *fmt;
  int    threads;
  char  *thread_type;
  char  *skip_loop_filter;
  char  *err_detect;
  char  *error_concealment;
  bool   nobuffer;
};

decopts_t *new_decopts(const char *fname);
void free_decopts(decopts_t *d);

#endif
