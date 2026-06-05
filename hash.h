#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct kv   kv_t;
typedef struct umap umap_t;

struct kv {
  char *k;
  char *v;
  kv_t *next;
};

kv_t *new_kv(
  char * /* k */, 
  char * /* v */);

/* if called on a head,
   frees the entire chain */
void free_kv(kv_t * /* r */);

struct umap {
  uint64_t size; 
  kv_t     **table; 

  /* non static methods */
  bool     (*push)(umap_t *, kv_t *);
  char    *(*find)(umap_t *, const char *);

  /* static methods */
  uint64_t (*hash)(const uint8_t *);
};

umap_t *new_umap(
  uint64_t /* size */, 
  uint64_t (* /* hash */ )(const uint8_t *));

void free_umap(umap_t * /* mp */);


/* hash functions for string
   refer to http://www.cse.yorku.ca/~oz/hash.html */
uint64_t djb2(const uint8_t * /* str */); 
uint64_t sdbm(const uint8_t * /* str */); 
uint64_t loselose(const uint8_t * /* str */); 

#endif
