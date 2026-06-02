#ifndef COMMON_H
#define COMMON_H

#define callp(C, F, ...) (C)->F((C), ##__VA_ARGS__)

#endif
