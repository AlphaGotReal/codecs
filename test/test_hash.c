#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "hash.h"

static int n_pass = 0;
static int n_fail = 0;

#define TEST(name) do {                                          \
  printf("  %-30s ", #name);                                     \
  if (name() == 0) {                                             \
    printf("PASS\n");                                            \
    n_pass++;                                                    \
  } else {                                                       \
    printf("FAIL\n");                                            \
    n_fail++;                                                    \
  }                                                              \
} while (0)

/* ── hash function tests ─────────────────────────────────── */

static int test_djb2_empty(void) {
  return djb2((const uint8_t *)"") == 5381 ? 0 : 1;
}

static int test_djb2_known(void) {
  return djb2((const uint8_t *)"a") == 177670 ? 0 : 1;
}

static int test_sdbm_empty(void) {
  return sdbm((const uint8_t *)"") == 0 ? 0 : 1;
}

static int test_sdbm_known(void) {
  return sdbm((const uint8_t *)"a") == 97 ? 0 : 1;
}

static int test_loselose_empty(void) {
  return loselose((const uint8_t *)"") == 0 ? 0 : 1;
}

static int test_loselose_known(void) {
  return loselose((const uint8_t *)"hello") == 532 ? 0 : 1;
}

static int test_deterministic(void) {
  uint64_t a1 = djb2((const uint8_t *)"xyz");
  uint64_t a2 = djb2((const uint8_t *)"xyz");
  uint64_t b1 = sdbm((const uint8_t *)"xyz");
  uint64_t b2 = sdbm((const uint8_t *)"xyz");
  uint64_t c1 = loselose((const uint8_t *)"xyz");
  uint64_t c2 = loselose((const uint8_t *)"xyz");
  return (a1 == a2 && b1 == b2 && c1 == c2) ? 0 : 1;
}

/* ── kv_t tests ─────────────────────────────────────────── */

static int test_new_kv(void) {
  char *k = strdup("key1");
  char *v = strdup("val1");
  kv_t *kv = new_kv(k, v);
  if (kv == NULL)             return 1;
  if (kv->k != k)             return 1;
  if (kv->v != v)             return 1;
  if (kv->next != NULL)       return 1;
  free_kv(kv);
  return 0;
}

/* ── umap tests ─────────────────────────────────────────── */

static int test_new_umap(void) {
  umap_t *mp = new_umap(8, djb2);
  if (mp == NULL)             return 1;
  if (mp->size != 8)          return 1;
  if (mp->table == NULL)      return 1;
  if (mp->push == NULL)       return 1;
  if (mp->find == NULL)       return 1;
  if (mp->hash == NULL)       return 1;
  free_umap(mp);
  return 0;
}

static int test_push_find(void) {
  umap_t *mp = new_umap(16, djb2);
  mp->push(mp, new_kv(strdup("name"), strdup("alice")));
  char *v = mp->find(mp, "name");
  int ok = (v != NULL && strcmp(v, "alice") == 0);
  free_umap(mp);
  return ok ? 0 : 1;
}

static int test_find_missing(void) {
  umap_t *mp = new_umap(16, djb2);
  char *v = mp->find(mp, "nonexistent");
  int ok = (v == NULL);
  free_umap(mp);
  return ok ? 0 : 1;
}

static int test_collision(void) {
  umap_t *mp = new_umap(1, loselose);
  mp->push(mp, new_kv(strdup("ab"), strdup("first")));
  mp->push(mp, new_kv(strdup("ba"), strdup("second")));

  char *v1 = mp->find(mp, "ab");
  char *v2 = mp->find(mp, "ba");
  int ok = (v1 && strcmp(v1, "first") == 0 &&
            v2 && strcmp(v2, "second") == 0);
  free_umap(mp);
  return ok ? 0 : 1;
}

static int test_push_null(void) {
  umap_t *mp = new_umap(4, djb2);
  int ret = mp->push(mp, NULL);
  free_umap(mp);
  return (ret == 0) ? 0 : 1;
}

static int test_find_null(void) {
  umap_t *mp = new_umap(4, djb2);
  char *v = mp->find(mp, NULL);
  free_umap(mp);
  return (v == NULL) ? 0 : 1;
}

static int test_multiple_entries(void) {
  umap_t *mp = new_umap(16, djb2);
  const char *keys[] = {"k1","k2","k3","k4","k5"};
  const char *vals[] = {"v1","v2","v3","v4","v5"};
  int n = 5;

  for (int i = 0; i < n; i++)
    mp->push(mp, new_kv(strdup(keys[i]), strdup(vals[i])));

  int ok = 1;
  for (int i = 0; i < n; i++) {
    char *v = mp->find(mp, keys[i]);
    if (v == NULL || strcmp(v, vals[i]) != 0) ok = 0;
  }

  free_umap(mp);
  return ok ? 0 : 1;
}

static int test_free_umap(void) {
  umap_t *mp = new_umap(8, sdbm);
  for (int i = 0; i < 100; i++) {
    char k[16], v[16];
    snprintf(k, sizeof k, "k%d", i);
    snprintf(v, sizeof v, "v%d", i);
    mp->push(mp, new_kv(strdup(k), strdup(v)));
  }
  free_umap(mp);
  return 0;
}

/* ── main ───────────────────────────────────────────────── */

int main(void) {
  printf("hash tests\n");
  printf("----------\n\n");

  printf("hash functions:\n");
  TEST(test_djb2_empty);
  TEST(test_djb2_known);
  TEST(test_sdbm_empty);
  TEST(test_sdbm_known);
  TEST(test_loselose_empty);
  TEST(test_loselose_known);
  TEST(test_deterministic);

  printf("\nkv_t:\n");
  TEST(test_new_kv);

  printf("\numap:\n");
  TEST(test_new_umap);
  TEST(test_push_find);
  TEST(test_find_missing);
  TEST(test_collision);
  TEST(test_push_null);
  TEST(test_find_null);
  TEST(test_multiple_entries);
  TEST(test_free_umap);

  printf("\n----------\n");
  printf("%d/%d passed\n", n_pass, n_pass + n_fail);

  return n_fail > 0 ? 1 : 0;
}
