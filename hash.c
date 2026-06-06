#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"

kv_t *new_kv(char *k, void *v) {
  kv_t *r = (kv_t *) malloc(sizeof(kv_t));
  if (r == NULL) {
    fprintf(stderr, "failed to malloc kv_t\n");
    return NULL;
  }

  r->k = k;
  r->v = v;
  r->next = NULL;

  return r;
}

void free_kv(kv_t *r) {
  if (r->next != NULL) 
    free_kv(r->next);
  free(r->k); 
  free(r->v); 
  free(r);
}

static bool mpush(umap_t *this, kv_t *r) {
  if (r == NULL) {
    fprintf(stderr, "cannot push NULL\n");
    return false;
  }

  uint64_t idx = (this->hash(r->k)) % (this->size);

  r->next = this->table[idx];
  this->table[idx] = r;

  return true;
}

static void *mfind(umap_t *this, const char *k) {
  if (k == NULL) {
    fprintf(stderr, "cannot find NULL\n");
    return NULL;
  }

  uint64_t idx = (this->hash(k)) % (this->size);

  for (kv_t *r = this->table[idx]; r != NULL; r = r->next) {
    if (strcmp(r->k, k) == 0) {
      return r->v;
    }
  }

  return NULL;
}

umap_t *new_umap(
  uint64_t size, 
  uint64_t (*mhash)(const uint8_t *)) {

  umap_t *mp = (umap_t *) malloc(sizeof(umap_t));
  if (mp == NULL) {
    fprintf(stderr, "failed to malloc umap_t\n");
    return NULL;
  }

  mp->size  = size;
  mp->table = (kv_t **) malloc(size * sizeof(kv_t *));
  
  if (mp->table == NULL) {
    fprintf(stderr, "failed to malloc umap_t table\n");
    return NULL;
  }

  for (uint64_t i = 0; i < size; i++)
    mp->table[i] = NULL;

  mp->push = mpush;
  mp->find = mfind;
  mp->hash = mhash;

  return mp;
}

void free_umap(umap_t *mp) {
  if (mp == NULL) return;
  for (uint64_t i = 0; i < mp->size; i++)
    if (mp->table[i] != NULL)
      free_kv(mp->table[i]);
  free(mp->table);
  free(mp);
}

/* hash functions for string
   refer to http://www.cse.yorku.ca/~oz/hash.html */

uint64_t djb2(const uint8_t *str) {
  uint64_t hash = 5381;
  uint8_t c;
  
  while (c = *str++)
    hash = ((hash << 5) + hash) + c; 
    // hash = hash * 33 + c 

  return hash;
}

uint64_t sdbm(const uint8_t *str) {
  uint64_t hash = 0;
  uint8_t c;
  
  while (c = *str++)
    hash = c + (hash << 6) + (hash << 16) - hash;  
    // hash = hash * 65599 + c;

  return hash;
}

uint64_t loselose(const uint8_t *str) {
  uint64_t hash = 0;
  uint8_t c;
  
  while (c = *str++)
    hash += c;   

  return hash;
}
