#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include "opts.h"

void free_opts(opts_t *opts) {
  if (opts == NULL) return;
  free(opts->lib);
  free(opts->in_fmt);
  free(opts->out_fmt);
  for (size_t i = 0; i < opts->n; i++) {
    free(opts->K[i]);
    free(opts->V[i]);
  }
  free(opts->K);
  free(opts->V);
  free(opts);
}

static int is_known(const char *k) {
  return strcmp(k, "lib")    == 0 ||
         strcmp(k, "fps")    == 0 ||
         strcmp(k, "in_fmt") == 0 ||
         strcmp(k, "out_fmt") == 0;
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
          key = strdup((const char *)e.data.scalar.value);
          state = 2;
        } else if (state == 2) {
          if (key && !is_known(key)) (*n)++;
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

opts_t *new_opts(const char *fname) {
  size_t n = 0;
  if (count_unknown(fname, &n) != 0) {
    fprintf(stderr, "could not read opts file: %s\n", fname);
    return NULL;
  }

  opts_t *opts = (opts_t *) malloc(sizeof(opts_t));
  if (opts == NULL) return NULL;
  memset(opts, 0, sizeof(opts_t));

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
  if (f == NULL) { free_opts(opts); return NULL; }

  yaml_parser_t p;
  if (!yaml_parser_initialize(&p)) { fclose(f); free_opts(opts); return NULL; }
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
        const char *val = (const char *)e.data.scalar.value;
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
          } else if (strcmp(key, "in_fmt") == 0) {
            opts->in_fmt = strdup(val);
            if (opts->in_fmt == NULL) { free(key); goto fail; }
          } else if (strcmp(key, "out_fmt") == 0) {
            opts->out_fmt = strdup(val);
            if (opts->out_fmt == NULL) { free(key); goto fail; }
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
  free_opts(opts);
  return NULL;
}
