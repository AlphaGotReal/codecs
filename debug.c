#include <stdio.h>
#include <stdlib.h>

void inc(int *t) {
  (*t)++;
}

int main() {

  int t = 2;
  printf("%d\n", t); 
  inc(&t);
  printf("%d\n", t); 

  return 0;
}
