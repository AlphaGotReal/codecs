#ifndef COMMON_H
#define COMMON_H
#define call(this, F, ...) (this)->F((this), ##__VA_ARGS__)
#endif
