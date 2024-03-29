#include "utils.h"

int get_size_of_file(FILE *file) {
  if (file == NULL) {
    return -1;
  }

  fseek(file, 0L, SEEK_END);
  int file_size = ftell(file);
  rewind(file);

  return file_size;
}

int get_no_of_bits(int n) { return ceil(log2(n)) + 1; }
