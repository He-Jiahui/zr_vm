#define _POSIX_C_SOURCE 200809L
#include "platform.h"
#include "backend.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool window;

bool zrg_platform_open(bool graphics, bool headless, bool interactive) {
    if (!graphics) return true;
#ifdef ZRG_HAS_GL33
    window = zrg_glfw_open(headless, interactive);
    return window;
#else
    (void)headless; (void)interactive;
    return false;
#endif
}

bool zrg_platform_poll(void) {
#ifdef ZRG_HAS_GL33
    if (window) return zrg_glfw_poll();
#endif
    return true;
}

void zrg_platform_input(int64_t axes[2]) {
    axes[0] = axes[1] = 0;
#ifdef ZRG_HAS_GL33
    if (window) zrg_glfw_input(axes);
#endif
}

void zrg_platform_present(void) {
#ifdef ZRG_HAS_GL33
    if (window) { zrg_glfw_present(); return; }
#endif
    zrg_backend_present();
}

void zrg_platform_close(void) {
#ifdef ZRG_HAS_GL33
    if (window) zrg_glfw_close();
#endif
    window = false;
}

static bool make_one(const char *path) {
    struct stat info;
    if (mkdir(path, 0777) == 0) return true;
    return errno == EEXIST && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool zrg_platform_make_directory(const char *directory) {
    char *path = strdup(directory);
    if (!path) return false;
    bool ok = true;
    for (char *p = path + 1; *p && ok; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        ok = make_one(path);
        *p = '/';
    }
    if (ok) ok = make_one(path);
    free(path);
    return ok;
}
