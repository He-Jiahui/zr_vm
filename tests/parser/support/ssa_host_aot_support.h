#ifndef ZR_TESTS_SSA_HOST_AOT_SUPPORT_H
#define ZR_TESTS_SSA_HOST_AOT_SUPPORT_H

/* Keep host-ABI success fixtures aligned with exec_ir_host_aot_target.c. */
#if defined(_WIN32) && defined(_WIN64) && defined(_MSC_VER) && \
        (defined(_M_X64) || defined(_M_AMD64)) && !defined(_M_ARM64) && !defined(_M_ARM64EC)
#define ZR_TESTS_HOST_SCALAR_AOT_SUPPORTED 1
#else
#define ZR_TESTS_HOST_SCALAR_AOT_SUPPORTED 0
#endif

#endif
