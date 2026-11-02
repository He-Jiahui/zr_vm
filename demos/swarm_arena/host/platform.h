#ifndef ZRG_PLATFORM_H
#define ZRG_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

bool zrg_platform_open(bool graphics, bool headless, bool interactive);
bool zrg_platform_poll(void);
void zrg_platform_input(int64_t axes[2]);
void zrg_platform_present(void);
void zrg_platform_close(void);
bool zrg_platform_make_directory(const char *directory);

#ifdef ZRG_HAS_GL33
bool zrg_glfw_open(bool headless, bool interactive);
bool zrg_glfw_poll(void);
void zrg_glfw_input(int64_t axes[2]);
void zrg_glfw_present(void);
void zrg_glfw_close(void);
#endif

#endif
