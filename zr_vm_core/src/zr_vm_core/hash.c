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

TZrUInt64 ZrCore_Hash_CreateStable64(const TZrByte *data, TZrSize length) {
    return XXH3_64bits(data, length);
}

TZrUInt64 ZrCore_Hash_CreateStable64WithPrefix(const TZrByte *prefix,
                                               TZrSize prefixLength,
                                               const TZrByte *data,
                                               TZrSize length) {
    XXH3_state_t *state;
    TZrUInt64 result;

    state = XXH3_createState();
    if (state == ZR_NULL) {
        return 0;
    }
    if (XXH3_64bits_reset(state) == XXH_ERROR ||
        (prefixLength > 0 && XXH3_64bits_update(state, prefix, prefixLength) == XXH_ERROR) ||
        (length > 0 && XXH3_64bits_update(state, data, length) == XXH_ERROR)) {
        XXH3_freeState(state);
        return 0;
    }

    result = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    return result;
}
