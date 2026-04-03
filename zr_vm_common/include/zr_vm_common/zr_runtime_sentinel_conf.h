//
// Shared runtime sentinels, guards, and hash salts.
//

#ifndef ZR_RUNTIME_SENTINEL_CONF_H
#define ZR_RUNTIME_SENTINEL_CONF_H

#include "zr_vm_common/zr_common_conf.h"

#define ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE ((TZrUInt32)0xFFFFFFFFu)
#define ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND 0x1000u
#define ZR_RUNTIME_REFLECTION_ENTRY_HASH_SALT 0xE177ULL

#endif // ZR_RUNTIME_SENTINEL_CONF_H
