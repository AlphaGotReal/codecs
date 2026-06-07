#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "flagmap.h"

/* ---- thread_type ---- */
bool    thrd_pset = false;
umap_t *thrd_map  = NULL;

void init_thrd(void) {
  if (thrd_pset) return;
  thrd_map = new_umap(64, djb2);

  int *v;

  v = malloc(sizeof(*v)); *v = FF_THREAD_FRAME;
  thrd_map->push(thrd_map, new_kv(strdup("frame"), v));

  v = malloc(sizeof(*v)); *v = FF_THREAD_SLICE;
  thrd_map->push(thrd_map, new_kv(strdup("slice"), v));

  v = malloc(sizeof(*v)); *v = 0;
  thrd_map->push(thrd_map, new_kv(strdup("auto"), v));

  thrd_pset = true;
}

int parse_thrd(const char *str) {
  if (str == NULL) return 0;
  init_thrd();
  void *f = thrd_map->find(thrd_map, str);
  return f ? *(int *)f : 0;
}

void free_thrd(void) {
  if (!thrd_pset) return;
  free_umap(thrd_map);
  thrd_map = NULL;
  thrd_pset = false;
}

/* ---- skip_loop_filter ---- */
bool    skip_pset = false;
umap_t *skip_map  = NULL;

void init_skip(void) {
  if (skip_pset) return;
  skip_map = new_umap(64, djb2);

  int *v;

  v = malloc(sizeof(*v)); *v = AVDISCARD_NONE;
  skip_map->push(skip_map, new_kv(strdup("none"), v));

  v = malloc(sizeof(*v)); *v = AVDISCARD_DEFAULT;
  skip_map->push(skip_map, new_kv(strdup("default"), v));

  v = malloc(sizeof(*v)); *v = AVDISCARD_NONREF;
  skip_map->push(skip_map, new_kv(strdup("nonref"), v));

  v = malloc(sizeof(*v)); *v = AVDISCARD_BIDIR;
  skip_map->push(skip_map, new_kv(strdup("bidir"), v));

  v = malloc(sizeof(*v)); *v = AVDISCARD_NONKEY;
  skip_map->push(skip_map, new_kv(strdup("nonkey"), v));

  v = malloc(sizeof(*v)); *v = AVDISCARD_ALL;
  skip_map->push(skip_map, new_kv(strdup("all"), v));

  skip_pset = true;
}

int parse_skip(const char *str) {
  if (str == NULL) return AVDISCARD_DEFAULT;
  init_skip();
  void *f = skip_map->find(skip_map, str);
  return f ? *(int *)f : AVDISCARD_DEFAULT;
}

void free_skip(void) {
  if (!skip_pset) return;
  free_umap(skip_map);
  skip_map = NULL;
  skip_pset = false;
}

/* ---- err_detect ---- */
static int or_split(umap_t *mp, const char *str) {
  if (str == NULL || *str == '\0') return 0;

  int acc = 0;
  char *dup = strdup(str);
  if (dup == NULL) return 0;

  char *tok = strtok(dup, "+");
  while (tok != NULL) {
    void *f = mp->find(mp, tok);
    if (f) acc |= *(int *)f;
    tok = strtok(NULL, "+");
  }
  free(dup);
  return acc;
}

bool    err_pset = false;
umap_t *err_map  = NULL;

void init_err(void) {
  if (err_pset) return;
  err_map = new_umap(64, djb2);

  int *v;

  v = malloc(sizeof(*v)); *v = AV_EF_CRCCHECK;
  err_map->push(err_map, new_kv(strdup("crccheck"), v));

  v = malloc(sizeof(*v)); *v = AV_EF_BITSTREAM;
  err_map->push(err_map, new_kv(strdup("bitstream"), v));

  v = malloc(sizeof(*v)); *v = AV_EF_BUFFER;
  err_map->push(err_map, new_kv(strdup("buffer"), v));

  v = malloc(sizeof(*v)); *v = AV_EF_EXPLODE;
  err_map->push(err_map, new_kv(strdup("explode"), v));

  v = malloc(sizeof(*v)); *v = AV_EF_IGNORE_ERR;
  err_map->push(err_map, new_kv(strdup("ignore_err"), v));

  v = malloc(sizeof(*v)); *v = AV_EF_CAREFUL;
  err_map->push(err_map, new_kv(strdup("careful"), v));

  v = malloc(sizeof(*v)); *v = AV_EF_COMPLIANT;
  err_map->push(err_map, new_kv(strdup("compliant"), v));

  v = malloc(sizeof(*v)); *v = AV_EF_AGGRESSIVE;
  err_map->push(err_map, new_kv(strdup("aggressive"), v));

  err_pset = true;
}

int parse_err(const char *str) {
  init_err();
  return or_split(err_map, str);
}

void free_err(void) {
  if (!err_pset) return;
  free_umap(err_map);
  err_map = NULL;
  err_pset = false;
}

/* ---- error_concealment ---- */
bool    ec_pset = false;
umap_t *ec_map  = NULL;

void init_ec(void) {
  if (ec_pset) return;
  ec_map = new_umap(64, djb2);

  int *v;

  v = malloc(sizeof(*v)); *v = FF_EC_GUESS_MVS;
  ec_map->push(ec_map, new_kv(strdup("guess_mvs"), v));

  v = malloc(sizeof(*v)); *v = FF_EC_DEBLOCK;
  ec_map->push(ec_map, new_kv(strdup("deblock"), v));

  ec_pset = true;
}

int parse_ec(const char *str) {
  init_ec();
  return or_split(ec_map, str);
}

void free_ec(void) {
  if (!ec_pset) return;
  free_umap(ec_map);
  ec_map = NULL;
  ec_pset = false;
}

/* ---- fflags ---- */
bool    fflag_pset = false;
umap_t *fflag_map  = NULL;

void init_fflag(void) {
  if (fflag_pset) return;
  fflag_map = new_umap(64, djb2);

  int *v;

  v = malloc(sizeof(*v)); *v = AVFMT_FLAG_NOBUFFER;
  fflag_map->push(fflag_map, new_kv(strdup("nobuffer"), v));

  fflag_pset = true;
}

int parse_fflag(const char *str) {
  init_fflag();
  return or_split(fflag_map, str);
}

void free_fflag(void) {
  if (!fflag_pset) return;
  free_umap(fflag_map);
  fflag_map = NULL;
  fflag_pset = false;
}

void free_all_flagmaps(void) {
  free_thrd();
  free_skip();
  free_err();
  free_ec();
  free_fflag();
}
