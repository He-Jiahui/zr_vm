#include "backend.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

#include <limits.h>
#include <stdio.h>

bool zrg_write_png(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height) {
    if (!path || !rgba || !width || !height || width > INT_MAX || height > INT_MAX) return false;
    size_t length = 0;
    void *png = tdefl_write_image_to_png_file_in_memory_ex(rgba, (int)width, (int)height,
                                                         4, &length, 6, MZ_FALSE);
    if (!png) return false;
    FILE *file = fopen(path, "wb");
    bool ok = file != NULL;
    if (file) {
        ok = fwrite(png, 1, length, file) == length;
        if (fclose(file) != 0) ok = false;
        if (!ok) (void)remove(path);
    }
    mz_free(png);
    return ok;
}
