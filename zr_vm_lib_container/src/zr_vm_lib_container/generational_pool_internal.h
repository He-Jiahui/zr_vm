#ifndef ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_INTERNAL_H
#define ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_INTERNAL_H

#include "zr_vm_lib_container/generational_pool.h"

typedef struct SZrPoolSlot {
    uint64_t generation;
    TZrSize nextFree;
    TZrSize readerCount;
    EZrPoolSlotState state;
    TZrBool writerActive;
    TZrBool initialized;
    TZrBool reused;
    TZrBool dirty;
} SZrPoolSlot;

typedef struct SZrPoolSlab {
    void *allocation;
    unsigned char *storage;
    SZrPoolSlot *slots;
    TZrSize baseIndex;
} SZrPoolSlab;

struct SZrPool {
    SZrPoolTypeLayout layout;
    SZrTypeLayout canonicalLayout;
    SZrTypeLayoutRegistryView canonicalRegistry;
    struct SZrState *canonicalState;
    FZrTypeLayoutGcValueVisitor canonicalGcVisitor;
    TZrPtr canonicalGcVisitorUserData;
    TZrBool hasCanonicalLayout;
    SZrPoolConfig config;
    SZrPoolSlab **slabs;
    TZrSize slabCount;
    TZrSize slabCapacity;
    TZrSize elementStride;
    TZrSize freeHead;
    uint64_t id;
    volatile long lockWord;
    TZrBool destroying;
    SZrPoolStats stats;
};

#endif // ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_INTERNAL_H
