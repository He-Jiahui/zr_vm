//
// Created by HeJiahui on 2025/6/20.
//
#include "zr_vm_core/hash.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/global.h"

#include <time.h>
#include "xxHash/xxhash.h"
ZR_FORCE_INLINE TZrUInt64 ZrHashSeedCreateInternal(TZrNativeString string, TZrSize length, TZrUInt64 seed) {
    return XXH3_64bits_withSeed(string, length, seed);
}

TZrUInt64 ZrCore_HashSeed_Create(SZrGlobalState *global, TZrUInt64 uniqueNumber) {
#define ZR_HASH_SEED_BUFFER_SIZE (sizeof(TZrUInt64) << 2)
    TZrChar buffer[ZR_HASH_SEED_BUFFER_SIZE];
    TZrUInt64 *bufferPtr = ZR_CAST_UINT64_PTR(buffer);
    TZrUInt64 timestamp = ZR_CAST_UINT64(time(ZR_NULL));
    TZrUInt64 globalAddress = ZR_CAST_UINT64(global);
    bufferPtr[0] = timestamp;
    bufferPtr[1] = globalAddress;
    bufferPtr[2] = ZR_CAST_UINT64(&ZrCore_HashSeed_Create);
    bufferPtr[3] = uniqueNumber;
    TZrUInt64 generatedSeed = ZrHashSeedCreateInternal(buffer, ZR_HASH_SEED_BUFFER_SIZE, timestamp);
    return generatedSeed;
#undef ZR_HASH_SEED_BUFFER_SIZE
}

TZrUInt64 ZrCore_Hash_Create(SZrGlobalState *global, TZrNativeString string, TZrSize length) {
    return ZrHashSeedCreateInternal(string, length, global->hashSeed);
}
