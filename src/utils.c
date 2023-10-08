#include "utils.h"

int get_size_of_file(FILE* file) {
    if(file == NULL) {
        return -1;
    }

    fseek(file, 0L, SEEK_END);
    int file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    return file_size;
}