#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include "opts.h"

/* ---- encoder opts ---- */

void free_encopts(encopts_t *opts) {
  if (opts == NULL) return;
  free(opts->lib);
  free(opts->fmt);
  for (size_t i = 0; i < opts->n; i++) {
    free(opts->K[i]);
    free(opts->V[i]);
  }
  free(opts->K);
  free(opts->V);
  free(opts);
}

static int is_enc_known(const char *k) {
  return strcmp(k, "lib")     == 0 ||
         strcmp(k, "fps")     == 0 ||
         strcmp(k, "fmt")     == 0 ||
         strcmp(k, "height")  == 0 ||
         strcmp(k, "width")   == 0;
}

static int count_unknown(const char *fname, size_t *n) {
  FILE *f = fopen(fname, "r");
  if (f == NULL) return -1;

  yaml_parser_t p;
  if (!yaml_parser_initialize(&p)) { fclose(f); return -1; }
  yaml_parser_set_input_file(&p, f);

  yaml_event_t e;
  int state = 0;
  char *key = NULL;
  *n = 0;

  while (yaml_parser_parse(&p, &e)) {
    switch (e.type) {
      case YAML_STREAM_END_EVENT:
        yaml_event_delete(&e);
        goto count_done;
      case YAML_MAPPING_START_EVENT: state = 1; break;
      case YAML_MAPPING_END_EVENT:   state = 0; break;
      case YAML_SCALAR_EVENT:
        if (state == 1) {
          key = strdup((const char *) e.data.scalar.value);
          state = 2;
        } else if (state == 2) {
          if (key && !is_enc_known(key)) (*n)++;
          free(key);
          key = NULL;
          state = 1;
        }
        break;
      default:
        break;
    }
    yaml_event_delete(&e);
  }

count_done:
  free(key);
  yaml_parser_delete(&p);
  fclose(f);
  return 0;
}

encopts_t *new_encopts(const char *fname) {
  size_t n = 0;
  if (count_unknown(fname, &n) != 0) {
    fprintf(stderr, "could not read encopts file: %s\n", fname);
    return NULL;
  }

  encopts_t *opts = (encopts_t *) malloc(sizeof(encopts_t));
  if (opts == NULL) return NULL;
  memset(opts, 0, sizeof(encopts_t));

  opts->K = (char **) malloc(n * sizeof(char *));
  opts->V = (char **) malloc(n * sizeof(char *));
  if (n > 0 && (opts->K == NULL || opts->V == NULL)) {
    free(opts->K);
    free(opts->V);
    free(opts);
    return NULL;
  }
  if (opts->K) memset(opts->K, 0, n * sizeof(char *));
  if (opts->V) memset(opts->V, 0, n * sizeof(char *));
  opts->n = n;

  FILE *f = fopen(fname, "r");
  if (f == NULL) { free_encopts(opts); return NULL; }

  yaml_parser_t p;
  if (!yaml_parser_initialize(&p)) { fclose(f); free_encopts(opts); return NULL; }
  yaml_parser_set_input_file(&p, f);

  yaml_event_t e;
  int state = 0;
  char *key = NULL;
  size_t idx = 0;

  while (yaml_parser_parse(&p, &e)) {
    switch (e.type) {
      case YAML_STREAM_END_EVENT:
        yaml_event_delete(&e);
        goto fill_done;
      case YAML_MAPPING_START_EVENT: state = 1; break;
      case YAML_MAPPING_END_EVENT:   state = 0; break;
      case YAML_SCALAR_EVENT: {
        const char *val = (const char *) e.data.scalar.value;
        if (state == 1) {
          key = strdup(val);
          if (key == NULL) goto fail;
          state = 2;
        } else if (state == 2) {
          if (strcmp(key, "lib") == 0) {
            opts->lib = strdup(val);
            if (opts->lib == NULL) { free(key); goto fail; }
          } else if (strcmp(key, "fps") == 0) {
            opts->fps = strtod(val, NULL);
          } else if (strcmp(key, "fmt") == 0) {
            opts->fmt = strdup(val);
            if (opts->fmt == NULL) { free(key); goto fail; }
          } else if (strcmp(key, "height") == 0) {
            opts->height = (uint32_t) strtoul(val, NULL, 10);
          } else if (strcmp(key, "width") == 0) {
            opts->width = (uint32_t) strtoul(val, NULL, 10);
          } else {
            opts->K[idx] = strdup(key);
            opts->V[idx] = strdup(val);
            if (opts->K[idx] == NULL || opts->V[idx] == NULL) {
              free(key);
              goto fail;
            }
            idx++;
          }
          free(key);
          key = NULL;
          state = 1;
        }
        break;
      }
      default:
        break;
    }
    yaml_event_delete(&e);
  }

fill_done:
  yaml_parser_delete(&p);
  fclose(f);
  return opts;

fail:
  free(key);
  yaml_parser_delete(&p);
  fclose(f);
  free_encopts(opts);
  return NULL;
}

/* ---- decoder opts ---- */

void free_decopts(decopts_t *d) {
  if (d == NULL) return;
  free(d->fmt);
  free(d->thread_type);
  free(d->skip_loop_filter);
  free(d->err_detect);
  free(d->error_concealment);
  free(d);
}

static int is_dec_known(const char *k) {
  return strcmp(k, "fmt")                == 0 ||
         strcmp(k, "threads")            == 0 ||
         strcmp(k, "thread_type")        == 0 ||
         strcmp(k, "skip_loop_filter")   == 0 ||
         strcmp(k, "err_detect")         == 0 ||
         strcmp(k, "error_concealment")  == 0 ||
         strcmp(k, "nobuffer")           == 0;
}

decopts_t *new_decopts(const char *fname) {
  decopts_t *d = (decopts_t *) malloc(sizeof(decopts_t));
  if (d == NULL) return NULL;
  memset(d, 0, sizeof(decopts_t));

  if (fname == NULL) return d;

  FILE *f = fopen(fname, "r");
  if (f == NULL) {
    fprintf(stderr, "could not open decopts file: %s\n", fname);
    free_decopts(d);
    return NULL;
  }

  yaml_parser_t p;
  if (!yaml_parser_initialize(&p)) { fclose(f); free_decopts(d); return NULL; }
  yaml_parser_set_input_file(&p, f);

  yaml_event_t e;
  int state = 0;
  char *key = NULL;

  while (yaml_parser_parse(&p, &e)) {
    switch (e.type) {
      case YAML_STREAM_END_EVENT:
        yaml_event_delete(&e);
        goto dec_done;
      case YAML_MAPPING_START_EVENT: state = 1; break;
      case YAML_MAPPING_END_EVENT:   state = 0; break;
      case YAML_SCALAR_EVENT: {
        const char *val = (const char *) e.data.scalar.value;
        if (state == 1) {
          key = strdup(val);
          if (key == NULL) goto dec_fail;
          state = 2;
        } else if (state == 2) {
          if (strcmp(key, "fmt") == 0) {
            if (val[0] == '\0') {
              d->fmt = NULL;
            } else {
              d->fmt = strdup(val);
              if (d->fmt == NULL) { free(key); goto dec_fail; }
            }
          } else if (strcmp(key, "threads") == 0) {
            d->threads = (int) strtol(val, NULL, 10);
          } else if (strcmp(key, "thread_type") == 0) {
            d->thread_type = strdup(val);
            if (d->thread_type == NULL) { free(key); goto dec_fail; }
          } else if (strcmp(key, "skip_loop_filter") == 0) {
            d->skip_loop_filter = strdup(val);
            if (d->skip_loop_filter == NULL) { free(key); goto dec_fail; }
          } else if (strcmp(key, "err_detect") == 0) {
            d->err_detect = strdup(val);
            if (d->err_detect == NULL) { free(key); goto dec_fail; }
          } else if (strcmp(key, "error_concealment") == 0) {
            d->error_concealment = strdup(val);
            if (d->error_concealment == NULL) { free(key); goto dec_fail; }
          } else if (strcmp(key, "nobuffer") == 0) {
            d->nobuffer = strcmp(val, "true") == 0 || strcmp(val, "yes") == 0 || strcmp(val, "1") == 0;
          }
          free(key);
          key = NULL;
          state = 1;
        }
        break;
      }
      default:
        break;
    }
    yaml_event_delete(&e);
  }

dec_done:
  yaml_parser_delete(&p);
  fclose(f);
  return d;

dec_fail:
  free(key);
  yaml_parser_delete(&p);
  fclose(f);
  free_decopts(d);
  return NULL;
}
