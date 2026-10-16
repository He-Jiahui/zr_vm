#ifndef ZR_VM_CORE_SESSION_CHECKPOINT_PRIVATE_H
#define ZR_VM_CORE_SESSION_CHECKPOINT_PRIVATE_H

#include "zr_vm_core/session_checkpoint.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Restore barriers are void APIs; reserve their remembered registry while the
 * fallible preparation phase is still active. */
TZrBool garbage_collector_ensure_remembered_registry_capacity(
        SZrGlobalState *global,
        TZrSize minCapacity);

/* Ordinary HashSet_Add nodes are standalone GC allocations and do not consume
 * pairPoolUsed.  Checkpoint validation must classify those nodes separately
 * from reserved pool slots while rejecting pointers into an unused pool tail.
 * The caller still performs the complete pool-chain shape/count validation;
 * this bounded helper repeats the local bounds needed to classify one bucket
 * node without dereferencing an untrusted next pointer. */
static ZR_FORCE_INLINE TZrBool checkpoint_hash_pair_is_valid_pool_member(
        const SZrHashSet *set,
        const SZrHashKeyValuePair *pair,
        TZrBool *outInPool) {
    const SZrHashPairPoolBlock *block;
    TZrSize steps = 0u;
    TZrUInt64 pairAddress;

    if (set == ZR_NULL || pair == ZR_NULL || outInPool == ZR_NULL) {
        return ZR_FALSE;
    }
    *outInPool = ZR_FALSE;
    pairAddress = (TZrUInt64)(TZrPtr)pair;
    for (block = set->pairPoolHead; block != ZR_NULL; block = block->next) {
        TZrSize pairBytes;
        TZrSize usedBytes;
        TZrUInt64 firstPairAddress;
        TZrUInt64 onePastUsedAddress;
        TZrUInt64 onePastCapacityAddress;
        TZrUInt64 offset;

        if (steps++ >= set->pairPoolCapacity || block->capacity == 0u ||
            block->used > block->capacity ||
            block->capacity > (TZrSize)-1 / sizeof(SZrHashKeyValuePair)) {
            return ZR_FALSE;
        }
        pairBytes = block->capacity * sizeof(SZrHashKeyValuePair);
        usedBytes = block->used * sizeof(SZrHashKeyValuePair);
        firstPairAddress = (TZrUInt64)(TZrPtr)&block->pairs[0];
        if (firstPairAddress > (TZrUInt64)-1 - (TZrUInt64)pairBytes ||
            firstPairAddress > (TZrUInt64)-1 - (TZrUInt64)usedBytes) {
            return ZR_FALSE;
        }
        onePastUsedAddress = firstPairAddress + (TZrUInt64)usedBytes;
        onePastCapacityAddress = firstPairAddress + (TZrUInt64)pairBytes;
        if (pairAddress >= firstPairAddress && pairAddress < onePastCapacityAddress) {
            offset = pairAddress - firstPairAddress;
            if (offset % sizeof(SZrHashKeyValuePair) != 0u) {
                return ZR_FALSE;
            }
            if (pairAddress >= onePastUsedAddress) {
                return ZR_FALSE;
            }
            *outInPool = ZR_TRUE;
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

/* GC 值改存对象图索引，避免快照在移动 GC 后保留失效的对象地址。 */
typedef struct ZrCheckpointValue {
    SZrTypeValue scalar;
    TZrSize objectIndex;
    TZrBool isObject;
} ZrCheckpointValue;

/* 保留哈希表的键值关系，恢复时再按当前对象地址重建桶。 */
typedef struct ZrCheckpointPair {
    ZrCheckpointValue key;
    ZrCheckpointValue value;
} ZrCheckpointPair;

typedef struct ZrCheckpointModuleDescriptor {
    TZrSize nameIndex;
    TZrUInt8 accessModifier;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 isReady;
    TZrBool hasName;
} ZrCheckpointModuleDescriptor;

typedef struct ZrCheckpointMemberDescriptor {
    SZrMemberDescriptor value;
    TZrSize objectIndexes[8];
} ZrCheckpointMemberDescriptor;

typedef struct ZrCheckpointInterfaceDispatchEntry {
    SZrInterfaceDispatchEntry value;
    TZrSize interfacePrototypeIndex;
    TZrSize implementationPrototypeIndex;
} ZrCheckpointInterfaceDispatchEntry;

typedef struct ZrCheckpointManagedField {
    SZrManagedFieldInfo value;
    TZrSize nameIndex;
} ZrCheckpointManagedField;

typedef struct ZrCheckpointPrototype {
    TZrSize nameIndex;
    TZrSize superPrototypeIndex;
    TZrSize metaFunctionIndexes[ZR_META_ENUM_MAX];
    TZrBool hasMeta[ZR_META_ENUM_MAX];
    EZrMetaType metaTypes[ZR_META_ENUM_MAX];
    EZrObjectPrototypeType type;
    TZrUInt64 shapeId;
    TZrUInt64 shapeGeneration;
    TZrUInt64 layoutGeneration;
    ZrCheckpointMemberDescriptor *memberDescriptors;
    TZrSize memberDescriptorCount;
    TZrSize memberDescriptorCapacity;
    ZrCheckpointInterfaceDispatchEntry *interfaceDispatchEntries;
    TZrSize interfaceDispatchCount;
    SZrIndexContract indexContract;
    TZrSize indexFunctionIndexes[4];
    SZrIterableContract iterableContract;
    TZrSize iterableFunctionIndex;
    SZrIteratorContract iteratorContract;
    TZrSize iteratorFunctionIndexes[2];
    TZrSize iteratorMemberNameIndex;
    TZrUInt64 protocolMask;
    TZrBool dynamicMemberCapable;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
    TZrUInt32 modifierFlags;
    TZrUInt32 nextVirtualSlotIndex;
    TZrUInt32 nextPropertyIdentity;
    TZrUInt32 layoutByteSize;
    TZrUInt32 layoutByteAlign;
    ZrCheckpointManagedField *managedFields;
    TZrSize managedFieldCount;
    TZrSize managedFieldCapacity;
    TZrBool keyOffsetMapValid;
    ZrCheckpointPair *keyOffsetPairs;
    TZrSize keyOffsetPairCount;
} ZrCheckpointPrototype;

/* 每个可恢复对象只登记一次；句柄保身份，原生数组保留可变侧数据。 */
typedef struct ZrCheckpointObject {
    SZrGcRootHandle handle;
    EZrRawObjectType type;
    EZrObjectInternalType internalType;
    TZrSize pairCount;
    ZrCheckpointPair *pairs;
    TZrBool nodeMapValid;
    TZrSize proPairCount;
    ZrCheckpointPair *proPairs;
    TZrBool proNodeMapValid;
    TZrSize prototypeIndex;
    TZrBool hasPrototype;
    TZrSize functionIndex;
    TZrBool hasFunction;
    TZrSize *captureIndexes;
    TZrSize captureCount;
    TZrSize closureValueIndex;
    TZrBool hasClosureValue;
    ZrCheckpointValue closureValue;
    TZrUInt32 memberVersion;
    TZrUInt8 moduleInitState;
    TZrUInt8 moduleReserved0;
    TZrUInt16 moduleReserved1;
    TZrUInt32 moduleMetadataGeneration;
    TZrSize moduleNameIndex;
    TZrSize fullPathIndex;
    TZrSize metadataFunctionIndex;
    TZrUInt64 modulePathHash;
    TZrBool hasModuleName;
    TZrBool hasFullPath;
    TZrBool hasMetadataFunction;
    TZrBool moduleHasMetadataRuntime;
    SZrMetadataRuntime metadataRuntime;
    ZrCheckpointModuleDescriptor *descriptors;
    TZrSize descriptorCount;
    TZrBool hasDecoratorMetadata;
    ZrCheckpointValue decoratorMetadata;
    ZrCheckpointValue *functionConstants;
    TZrSize functionConstantCount;
    TZrSize cachedStatelessClosureIndex;
    TZrBool hasCachedStatelessClosure;
    TZrSize *prototypeInstanceIndexes;
    TZrSize prototypeInstanceCount;
    ZrCheckpointPrototype *prototype;
    TZrUInt64 functionCallBindingGeneration;
    const SZrAotCodeRegistration *functionMetadataCodeRegistration;
    TZrUInt32 functionMetadataTypeLayoutCount;
    TZrUInt32 functionMetadataGcDescriptorCount;
    TZrUInt32 *typedExportMetadataTokens;
    TZrSize typedExportMetadataTokenCount;
    TZrBool hasRawIntData;
    TZrInt64 *rawIntData;
    TZrSize rawIntLength;
    TZrSize rawIntCapacity;
    EZrSuperArrayStorageMode rawIntStorageMode;
    TZrUInt64 rawIntStorageGeneration;
    TZrBool rawIntDirty;
} ZrCheckpointObject;

typedef struct ZrCheckpointRestorePlan {
    SZrRawObject *object;
    SZrHashSet nodeMap;
    SZrHashSet proNodeMap;
    SZrObjectPrototype *prototype;
    SZrString *moduleName;
    SZrString *fullPath;
    SZrFunction *metadataFunction;
    SZrFunction *closureFunction;
    SZrRawObject **closureCaptures;
    SZrTypeValue decoratorMetadata;
    SZrTypeValue *functionConstants;
    SZrTypeValue closureValue;
    SZrString *prototypeName;
    SZrObjectPrototype *prototypeSuper;
    SZrIndexContract prototypeIndexContract;
    SZrIterableContract prototypeIterableContract;
    SZrIteratorContract prototypeIteratorContract;
    SZrModuleExportDescriptor *descriptors;
    TZrInt64 *rawIntData;
    SZrObjectPrototype **prototypeInstances;
    SZrClosure *cachedStatelessClosure;
    SZrMemberDescriptor *prototypeMemberDescriptors;
    SZrInterfaceDispatchEntry *prototypeInterfaceDispatchEntries;
    SZrManagedFieldInfo *prototypeManagedFields;
    SZrMeta *prototypeMetas[ZR_META_ENUM_MAX];
    SZrHashSet prototypeKeyOffsetMap;
} ZrCheckpointRestorePlan;

/* 快照借用创建线程，并以该线程 GC 域的根句柄固定对象图生命周期。 */
struct SZrSessionCheckpoint {
    SZrState *state;
    ZrCheckpointObject *objects;
    TZrSize objectCount;
    TZrSize objectCapacity;
    ZrCheckpointValue loadedModulesRegistry;
    ZrCheckpointValue zrObject;
    ZrCheckpointValue nullValue;
    ZrCheckpointValue unhandledExceptionHandler;
    TZrBool hasUnhandledExceptionHandler;
    TZrSize errorPrototypeIndex;
    TZrBool hasErrorPrototype;
    TZrSize stackFramePrototypeIndex;
    TZrBool hasStackFramePrototype;
    TZrSize basicTypePrototypeIndexes[ZR_VALUE_TYPE_ENUM_MAX];
    TZrBool hasBasicTypePrototype[ZR_VALUE_TYPE_ENUM_MAX];
    TZrSize metaFunctionNameIndexes[ZR_META_ENUM_MAX];
    TZrBool hasMetaFunctionName[ZR_META_ENUM_MAX];
};

/* 未登记的可选引用不能与对象图中的零号节点混淆。 */
#define ZR_CHECKPOINT_NO_INDEX ((TZrSize)-1)

typedef struct ZrCheckpointRestoreRoots {
    SZrTypeValue loadedModulesRegistry;
    SZrTypeValue nullValue;
    SZrTypeValue zrObject;
    SZrTypeValue unhandledExceptionHandler;
    SZrObjectPrototype *errorPrototype;
    SZrObjectPrototype *stackFramePrototype;
    SZrObjectPrototype *basicTypePrototypes[ZR_VALUE_TYPE_ENUM_MAX];
    SZrString *metaFunctionNames[ZR_META_ENUM_MAX];
} ZrCheckpointRestoreRoots;

/* Internal phase boundary: capture validates its safe point through preflight. */
TZrBool checkpoint_state_is_clean_create_boundary(const SZrState *state);
TZrBool checkpoint_prepare_restore_plan(
        SZrState *state,
        const SZrSessionCheckpoint *checkpoint,
        ZrCheckpointRestorePlan **outPlan,
        ZrCheckpointRestoreRoots *outRoots,
        TZrBool *outNeedsReset);
void checkpoint_release_restore_plan(
        SZrState *state,
        const SZrSessionCheckpoint *checkpoint,
        ZrCheckpointRestorePlan *plan);
void checkpoint_commit_restore_plan(
        SZrState *state,
        const SZrSessionCheckpoint *checkpoint,
        ZrCheckpointRestorePlan *plan,
        ZrCheckpointRestoreRoots roots);

#endif
