//
// Shared runtime sentinels, guards, and hash salts.
//

#ifndef ZR_RUNTIME_SENTINEL_CONF_H
#define ZR_RUNTIME_SENTINEL_CONF_H

#include "zr_vm_common/zr_common_conf.h"

#define ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE ((TZrUInt32)0xFFFFFFFFu)
#define ZR_RUNTIME_DEBUG_HOOK_LINE_NONE ((TZrUInt32)0xFFFFFFFFu)
#define ZR_RUNTIME_SEMIR_DEOPT_ID_NONE ((TZrUInt32)0u)
#define ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND 0x1000u
/*
 * Typical upper bound for canonical user-space pointers on 64-bit platforms (e.g. AMD64 Linux lower half).
 * Used for cheap guards on GC edges that must not dereference obviously non-heap values.
 */
#if UINTPTR_MAX == UINT64_C(0xFFFFFFFFFFFFFFFF)
#define ZR_RUNTIME_LIKELY_USER_POINTER_MAX_INCLUSIVE ((TZrPtr)UINT64_C(0x00007FFFFFFFFFFF))
#endif
#define ZR_RUNTIME_REFLECTION_ENTRY_HASH_SALT 0xE177ULL

#endif // ZR_RUNTIME_SENTINEL_CONF_H
