#include "backend.h"

#include <stdio.h>

bool zrg_write_ppm(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height) {
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool ok = fprintf(file, "P6\n%u %u\n255\n", width, height) > 0;
    for (size_t i = 0; ok && i < (size_t)width * height; ++i)
        ok = fwrite(rgba + i * 4, 1, 3, file) == 3;
    if (fclose(file) != 0) ok = false;
    if (!ok) (void)remove(path);
    return ok;
}
