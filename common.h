#ifndef COMMON_H
#define COMMON_H

#include <time.h>

double get_wall_time() {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  return (double) time.tv_sec + (double) time.tv_nsec * 1e-9;
}

#define cnow (clock() * 1e-6)
#define rnow get_wall_time()

#include <unistd.h>

long get_file_size(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    return -1;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fclose(file);

  return size;
}

#define non_static_call(this, F, ...) (this)->F((this), ##__VA_ARGS__)
#define static_call(this, F, ...) (this)->F(##__VA_ARGS__)

#endif
