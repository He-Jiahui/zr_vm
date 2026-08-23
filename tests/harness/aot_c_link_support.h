#ifndef ZR_VM_TESTS_AOT_C_LINK_SUPPORT_H
#define ZR_VM_TESTS_AOT_C_LINK_SUPPORT_H

#include "zr_vm_common/zr_common_conf.h"

#if defined(ZR_PLATFORM_UNIX)
#if defined(__APPLE__)
#define ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS                                                        \
    "-lzr_vm_library -lzr_vm_core -lzr_c_json -lzr_miniz -lzr_tiny_dir "                        \
    "-lzr_xx_hash -lzr_utf8proc -lm -pthread "
#else
#define ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS                                                        \
    "-Wl,--start-group -lzr_vm_library -lzr_vm_core -lzr_c_json -lzr_miniz -lzr_tiny_dir "       \
    "-lzr_xx_hash -lzr_utf8proc -Wl,--end-group -lm -ldl -pthread "
#endif
#else
#define ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS ""
#endif

#endif
