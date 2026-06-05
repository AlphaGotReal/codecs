#ifndef OPTS_H
#define OPTS_H

#include <stdbool.h>

typedef struct opts opts_t;

struct opts {
  char *lib;
  double fps;
  char *in_fmt;
  char *out_fmt;

  /* variable options */
  size_t n;
  char **K; 
  char **V; 
};

opts_t *new_opts(
  const char * /* fname */);

void free_opts(opts_t * /* opts */);

#endif
