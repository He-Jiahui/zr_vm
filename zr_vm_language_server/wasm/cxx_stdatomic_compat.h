/* Host C++ compiles of the WASM export glue include C headers that use
 * <stdatomic.h>. GCC's stdatomic.h is not valid C++. This prelude is forced
 * in before those sources so the C11 atomic spellings stay available. */
#ifndef ZR_VM_WASM_CXX_STDATOMIC_COMPAT_H
#define ZR_VM_WASM_CXX_STDATOMIC_COMPAT_H

#ifdef __cplusplus
#include <atomic>
#include <cstdint>

#ifndef _STDATOMIC_H
#define _STDATOMIC_H
typedef std::atomic<std::uint_fast32_t> atomic_uint_fast32_t;
typedef std::atomic<std::uint_fast64_t> atomic_uint_fast64_t;
typedef std::atomic_flag atomic_flag;
#define _Atomic(T) std::atomic<T>
#endif
#endif

#endif
