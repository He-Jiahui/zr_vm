#ifndef ZRG_HOST_H
#define ZRG_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

typedef enum ZrgGcPolicy { ZRG_GC_AUTO, ZRG_GC_SLACK, ZRG_GC_MANUAL } ZrgGcPolicy;

typedef struct ZrgOptions {
    const char *project;
    const char *out;
    const char *scenario;
    uint32_t frames, seed, gc_budget_us, minor_interval;
    int64_t expect_hash;
    ZrgGcPolicy gc_policy;
    bool help;
} ZrgOptions;

typedef struct ZrgHostVm {
    SZrGlobalState *global;
    SZrState *state;
    bool booted;
} ZrgHostVm;

bool zrg_options_parse(int argc, char **argv, ZrgOptions *options);
void zrg_options_usage(void);
const char *zrg_gc_policy_name(ZrgGcPolicy policy);
bool zrg_host_vm_open(ZrgHostVm *vm, const ZrgOptions *options);
bool zrg_host_vm_call_int(ZrgHostVm *vm, const char *name,
                          const int64_t *arguments, size_t count, int64_t *result);
bool zrg_host_vm_gc_step(ZrgHostVm *vm);
bool zrg_host_vm_shutdown(ZrgHostVm *vm, char *summary, size_t capacity);
void zrg_host_vm_close(ZrgHostVm *vm);
bool zrg_platform_poll(void);
void zrg_platform_present(void);
bool zrg_platform_make_directory(const char *directory);
int zrg_host_loop(ZrgHostVm *vm, const ZrgOptions *options);

#endif
