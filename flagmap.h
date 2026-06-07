#ifndef FLAGMAP_H
#define FLAGMAP_H

#include <stdbool.h>
#include <stdint.h>

#include "hash.h"

/* thread_type: "frame" / "slice" / "auto" -> FF_THREAD_* */
extern bool    thrd_pset;
extern umap_t *thrd_map;
void init_thrd(void);
int  parse_thrd(const char *str);
void free_thrd(void);

/* skip_loop_filter: "none" / "default" / "nonref" / "bidir" / "nonkey" / "all" -> AVDiscard */
extern bool    skip_pset;
extern umap_t *skip_map;
void init_skip(void);
int  parse_skip(const char *str);
void free_skip(void);

/* err_detect: "crccheck" / "bitstream" / "buffer" / "explode" / "ignore_err" / "careful" / "compliant" / "aggressive" -> AV_EF_* */
extern bool    err_pset;
extern umap_t *err_map;
void init_err(void);
int  parse_err(const char *str);    /* supports "crccheck+explode" form */
void free_err(void);

/* error_concealment: "guess_mvs" / "deblock" -> FF_EC_* */
extern bool    ec_pset;
extern umap_t *ec_map;
void init_ec(void);
int  parse_ec(const char *str);     /* supports "guess_mvs+deblock" form */
void free_ec(void);

/* fflags: "nobuffer" -> AVFMT_FLAG_* */
extern bool    fflag_pset;
extern umap_t *fflag_map;
void init_fflag(void);
int  parse_fflag(const char *str);  /* supports combined form */
void free_fflag(void);

void free_all_flagmaps(void);

#endif
