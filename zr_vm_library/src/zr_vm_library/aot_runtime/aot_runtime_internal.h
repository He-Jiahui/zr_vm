#ifndef ZR_VM_LIBRARY_AOT_RUNTIME_INTERNAL_H
#define ZR_VM_LIBRARY_AOT_RUNTIME_INTERNAL_H

#include "zr_vm_library/conf.h"

struct SZrGlobalState;
struct SZrState;

typedef struct SZrLibraryAotRuntimeState SZrLibraryAotRuntimeState;

SZrLibraryAotRuntimeState *aot_runtime_get_state_from_global(struct SZrGlobalState *global);

void aot_runtime_fail(struct SZrState *state,
                      SZrLibraryAotRuntimeState *runtimeState,
                      const TZrChar *format,
                      ...);

#endif
