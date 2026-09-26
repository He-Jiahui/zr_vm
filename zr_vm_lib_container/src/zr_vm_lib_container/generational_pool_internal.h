#ifndef ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_INTERNAL_H
#define ZR_VM_LIB_CONTAINER_GENERATIONAL_POOL_INTERNAL_H

#include "zr_vm_lib_container/generational_pool.h"

/* 槽位状态同时约束弱句柄代数、借用冲突、延迟析构和屏障扫描。 */
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

/* slab 将稳定槽位元数据与按布局对齐的元素存储绑定，扩容不移动已发布元素。 */
typedef struct SZrPoolSlab {
    void *allocation;
    unsigned char *storage;
    SZrPoolSlot *slots;
    TZrSize baseIndex;
} SZrPoolSlab;

/* 池持有 slab/统计，浅复制回调描述符；规范类型布局的外部状态仍按借用契约管理。 */
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
