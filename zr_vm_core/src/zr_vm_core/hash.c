//
// Created by HeJiahui on 2025/6/20.
//
#include "zr_vm_core/hash.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/global.h"

#include <time.h>
#include "xxHash/xxhash.h"
ZR_FORCE_INLINE TUInt64 ZrHashSeedCreateInternal(TNativeString string, TZrSize length, TUInt64 seed) {
    return XXH3_64bits_withSeed(string, length, seed);
}

TUInt64 ZrHashSeedCreate(SZrGlobalState *global, TUInt64 uniqueNumber) {
#define ZR_HASH_SEED_BUFFER_SIZE (sizeof(TUInt64) << 2)
    TChar buffer[ZR_HASH_SEED_BUFFER_SIZE];
    TUInt64 *bufferPtr = ZR_CAST_UINT64_PTR(buffer);
    TUInt64 timestamp = ZR_CAST_UINT64(time(ZR_NULL));
    TUInt64 globalAddress = ZR_CAST_UINT64(global);
    bufferPtr[0] = timestamp;
    bufferPtr[1] = globalAddress;
    bufferPtr[2] = ZR_CAST_UINT64(&ZrHashSeedCreate);
    bufferPtr[3] = uniqueNumber;
    TUInt64 generatedSeed = ZrHashSeedCreateInternal(buffer, ZR_HASH_SEED_BUFFER_SIZE, timestamp);
    return generatedSeed;
#undef ZR_HASH_SEED_BUFFER_SIZE
}

TUInt64 ZrHashCreate(SZrGlobalState *global, TNativeString string, TZrSize length) {
    return ZrHashSeedCreateInternal(string, length, global->hashSeed);
}
