//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_OBJECT_H
#define ZR_VM_CORE_OBJECT_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/object_known_native_dispatch.h"
#include "zr_vm_common/zr_contract_conf.h"
struct SZrState;
struct SZrGlobalState;
struct SZrObjectPrototype;
struct SZrFunction;

typedef enum EZrProtocolId {
    ZR_PROTOCOL_ID_NONE = 0,
    ZR_PROTOCOL_ID_EQUATABLE = 1,
    ZR_PROTOCOL_ID_HASHABLE = 2,
    ZR_PROTOCOL_ID_COMPARABLE = 3,
    ZR_PROTOCOL_ID_ITERABLE = 4,
    ZR_PROTOCOL_ID_ITERATOR = 5,
    ZR_PROTOCOL_ID_ARRAY_LIKE = 6,
    ZR_PROTOCOL_ID_TASK_HANDLE = 7,
    ZR_PROTOCOL_ID_TASK_RUNNER = 8
} EZrProtocolId;

#define ZR_PROTOCOL_BIT(PROTOCOL_ID) (1ull << (TZrUInt64)(PROTOCOL_ID))

typedef enum EZrMemberDescriptorKind {
    ZR_MEMBER_DESCRIPTOR_KIND_FIELD = 0,
    ZR_MEMBER_DESCRIPTOR_KIND_METHOD = 1,
    ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY = 2,
    ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER = 3
} EZrMemberDescriptorKind;

typedef struct SZrMemberDescriptor {
    struct SZrString *name;
    EZrMemberDescriptorKind kind;
    TZrBool isStatic;
    TZrBool isWritable;
    TZrBool isDynamicWrite;
    TZrUInt8 reserved0;
    struct SZrFunction *getterFunction;
    struct SZrFunction *setterFunction;
    TZrUInt32 contractRole;
    TZrUInt32 modifierFlags;
    struct SZrString *ownerTypeName;
    struct SZrString *baseDefinitionOwnerTypeName;
    struct SZrString *baseDefinitionName;
    TZrUInt32 virtualSlotIndex;
    TZrUInt32 interfaceContractSlot;
    TZrUInt32 propertyIdentity;
    TZrUInt32 accessorRole;
} SZrMemberDescriptor;

typedef struct SZrIndexContract {
    struct SZrFunction *getByIndexFunction;
    struct SZrFunction *setByIndexFunction;
    struct SZrFunction *containsKeyFunction;
    struct SZrFunction *getLengthFunction;
    struct SZrRawObject *getByIndexKnownNativeCallable;
    struct SZrRawObject *setByIndexKnownNativeCallable;
    FZrNativeFunction getByIndexKnownNativeFunction;
    FZrNativeFunction setByIndexKnownNativeFunction;
    SZrObjectKnownNativeDirectDispatch getByIndexKnownNativeDirectDispatch;
    SZrObjectKnownNativeDirectDispatch setByIndexKnownNativeDirectDispatch;
} SZrIndexContract;

typedef struct SZrIterableContract {
    struct SZrFunction *iterInitFunction;
} SZrIterableContract;

typedef struct SZrIteratorContract {
    struct SZrFunction *moveNextFunction;
    struct SZrFunction *currentFunction;
    struct SZrString *currentMemberName;
} SZrIteratorContract;

enum EZrObjectInternalType {
    ZR_OBJECT_INTERNAL_TYPE_OBJECT,
    ZR_OBJECT_INTERNAL_TYPE_STRUCT,
    ZR_OBJECT_INTERNAL_TYPE_ARRAY,
    ZR_OBJECT_INTERNAL_TYPE_MODULE,
    ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE,
    ZR_OBJECT_INTERNAL_TYPE_PROTOTYPE_INFO,  // 用于在常量池中存储prototype信息的对象
    ZR_OBJECT_INTERNAL_TYPE_CUSTOM_EXTENSION_START,
    // user can add custom extension object prototype here
};

typedef enum EZrObjectInternalType EZrObjectInternalType;

struct ZR_STRUCT_ALIGN SZrObject {
    SZrRawObject super;

    struct SZrObjectPrototype *prototype;

    SZrHashSet nodeMap;

    EZrObjectInternalType internalType;
    TZrUInt32 memberVersion;
    SZrHashKeyValuePair *cachedHiddenItemsPair;
    struct SZrObject *cachedHiddenItemsObject;
    SZrHashKeyValuePair *cachedLengthPair;
    SZrHashKeyValuePair *cachedCapacityPair;
    SZrHashKeyValuePair *cachedStringLookupPair;
    TZrInt64 *superArrayRawIntData;
    TZrSize superArrayRawIntLength;
    TZrSize superArrayRawIntCapacity;
    TZrBool superArrayRawIntDirty;

    // SZrRawObject *gcList;
};

typedef struct SZrObject SZrObject;

typedef struct SZrManagedFieldInfo {
    struct SZrString *name;
    TZrUInt32 fieldOffset;
    TZrUInt32 fieldSize;
    TZrUInt32 ownershipQualifier;
    TZrBool callsClose;
    TZrBool callsDestructor;
    TZrUInt32 declarationOrder;
} SZrManagedFieldInfo;

struct ZR_STRUCT_ALIGN SZrObjectPrototype {
    SZrObject super;
    struct SZrString *name;
    EZrObjectPrototypeType type;
    struct SZrMetaTable metaTable;
    struct SZrObjectPrototype *superPrototype;
    SZrMemberDescriptor *memberDescriptors;
    TZrUInt32 memberDescriptorCount;
    TZrUInt32 memberDescriptorCapacity;
    SZrIndexContract indexContract;
    SZrIterableContract iterableContract;
    SZrIteratorContract iteratorContract;
    TZrUInt64 protocolMask;
    TZrBool dynamicMemberCapable;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
    TZrUInt32 modifierFlags;
    TZrUInt32 nextVirtualSlotIndex;
    TZrUInt32 nextPropertyIdentity;
    SZrManagedFieldInfo *managedFields;
    TZrUInt32 managedFieldCount;
    TZrUInt32 managedFieldCapacity;
};

typedef struct SZrObjectPrototype SZrObjectPrototype;

struct ZR_STRUCT_ALIGN SZrStructPrototype {
    SZrObjectPrototype super;
    SZrHashSet keyOffsetMap;
};
typedef struct SZrStructPrototype SZrStructPrototype;


// pure object should be created by this function
ZR_CORE_API SZrObject *ZrCore_Object_New(struct SZrState *state, SZrObjectPrototype *prototype);

ZR_CORE_API SZrObject *ZrCore_Object_NewCustomized(struct SZrState *state, TZrSize size, EZrObjectInternalType internalType);

ZR_FORCE_INLINE SZrMeta *ZrCore_Prototype_GetMetaRecursively(struct SZrGlobalState *global, SZrObjectPrototype *prototype,
                                                       EZrMetaType metaType) {
    ZR_UNUSED_PARAMETER(global);
    while (prototype != ZR_NULL) {
        SZrMeta *meta = prototype->metaTable.metas[metaType];
        if (meta != ZR_NULL) {
            return meta;
        }
        prototype = prototype->superPrototype;
    }
    return ZR_NULL;
}

ZR_FORCE_INLINE SZrMeta *ZrCore_Object_GetMetaRecursively(struct SZrGlobalState *global, SZrObject *object,
                                                    EZrMetaType metaType) {
    SZrObjectPrototype *prototype = object->prototype;
    return ZrCore_Prototype_GetMetaRecursively(global, prototype, metaType);
}

ZR_CORE_API void ZrCore_Object_Init(struct SZrState *state, SZrObject *object);

ZR_CORE_API SZrObject *ZrCore_Object_CloneStruct(struct SZrState *state, const SZrObject *source);

// this function do not call Meta function to compare, only compare address, we use this for hash set to make key
// different
ZR_CORE_API TZrBool ZrCore_Object_CompareWithAddress(struct SZrState *state, SZrObject *object1, SZrObject *object2);

ZR_CORE_API void ZrCore_Object_SetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key,
                                  const SZrTypeValue *value);

ZR_CORE_API void ZrCore_Object_SetExistingPairValueUnchecked(struct SZrState *state,
                                                             SZrObject *object,
                                                             SZrHashKeyValuePair *pair,
                                                             const SZrTypeValue *value);

ZR_CORE_API const SZrTypeValue *ZrCore_Object_GetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key);

ZR_CORE_API const SZrMemberDescriptor *ZrCore_ObjectPrototype_FindMemberDescriptor(SZrObjectPrototype *prototype,
                                                                                    struct SZrString *memberName,
                                                                                    TZrBool includeInherited);

ZR_CORE_API TZrBool ZrCore_ObjectPrototype_AddMemberDescriptor(struct SZrState *state,
                                                               SZrObjectPrototype *prototype,
                                                               const SZrMemberDescriptor *descriptor);

ZR_CORE_API TZrBool ZrCore_Object_GetMember(struct SZrState *state,
                                            SZrTypeValue *receiver,
                                            struct SZrString *memberName,
                                            SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_GetMemberCachedDescriptorUnchecked(struct SZrState *state,
                                                                     SZrTypeValue *receiver,
                                                                     struct SZrObjectPrototype *ownerPrototype,
                                                                     TZrUInt32 descriptorIndex,
                                                                     SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_SetMember(struct SZrState *state,
                                            SZrTypeValue *receiver,
                                            struct SZrString *memberName,
                                            const SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Object_SetMemberCachedDescriptorUnchecked(struct SZrState *state,
                                                                     SZrTypeValue *receiver,
                                                                     struct SZrObjectPrototype *ownerPrototype,
                                                                     TZrUInt32 descriptorIndex,
                                                                     const SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Object_InvokeMember(struct SZrState *state,
                                               SZrTypeValue *receiver,
                                               struct SZrString *memberName,
                                               const SZrTypeValue *arguments,
                                               TZrSize argumentCount,
                                               SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_ResolveMemberCallable(struct SZrState *state,
                                                        SZrTypeValue *receiver,
                                                        struct SZrString *memberName,
                                                        struct SZrObjectPrototype **outOwnerPrototype,
                                                        struct SZrFunction **outFunction,
                                                        TZrBool *outIsStatic);

ZR_CORE_API TZrBool ZrCore_Object_InvokeResolvedFunction(struct SZrState *state,
                                                         struct SZrFunction *function,
                                                         TZrBool isStatic,
                                                         SZrTypeValue *receiver,
                                                         const SZrTypeValue *arguments,
                                                         TZrSize argumentCount,
                                                         SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_GetByIndex(struct SZrState *state,
                                             SZrTypeValue *receiver,
                                             const SZrTypeValue *key,
                                             SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_SetByIndex(struct SZrState *state,
                                             SZrTypeValue *receiver,
                                             const SZrTypeValue *key,
                                             const SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Object_SuperArrayGetInt(struct SZrState *state,
                                                   SZrTypeValue *receiver,
                                                   const SZrTypeValue *key,
                                                   SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_SuperArraySetInt(struct SZrState *state,
                                                   SZrTypeValue *receiver,
                                                   const SZrTypeValue *key,
                                                   const SZrTypeValue *value);

ZR_CORE_API TZrBool ZrCore_Object_SuperArrayAddInt(struct SZrState *state,
                                                   SZrTypeValue *receiver,
                                                   const SZrTypeValue *value,
                                                   SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(struct SZrState *state,
                                                                    TZrStackValuePointer receiverBase,
                                                                    TZrInt64 repeatCount,
                                                                    TZrInt64 value);

ZR_CORE_API TZrBool ZrCore_Object_IterInit(struct SZrState *state,
                                           SZrTypeValue *iterableValue,
                                           SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_IterMoveNext(struct SZrState *state,
                                               SZrTypeValue *iteratorValue,
                                               SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Object_IterCurrent(struct SZrState *state,
                                              SZrTypeValue *iteratorValue,
                                              SZrTypeValue *result);

// Prototype 创建和管理函数
ZR_CORE_API SZrObjectPrototype *ZrCore_ObjectPrototype_New(struct SZrState *state, SZrString *name, EZrObjectPrototypeType type);

ZR_CORE_API SZrStructPrototype *ZrCore_StructPrototype_New(struct SZrState *state, SZrString *name);

ZR_CORE_API void ZrCore_ObjectPrototype_SetSuper(struct SZrState *state, SZrObjectPrototype *prototype, SZrObjectPrototype *superPrototype);

ZR_CORE_API void ZrCore_ObjectPrototype_InitMetaTable(struct SZrState *state, SZrObjectPrototype *prototype);

ZR_CORE_API void ZrCore_StructPrototype_AddField(struct SZrState *state, SZrStructPrototype *prototype, SZrString *fieldName, TZrSize offset);

ZR_CORE_API void ZrCore_ObjectPrototype_AddMeta(struct SZrState *state, SZrObjectPrototype *prototype, EZrMetaType metaType, struct SZrFunction *function);

ZR_CORE_API void ZrCore_ObjectPrototype_AddManagedField(struct SZrState *state,
                                                  SZrObjectPrototype *prototype,
                                                  SZrString *fieldName,
                                                  TZrUInt32 fieldOffset,
                                                  TZrUInt32 fieldSize,
                                                  TZrUInt32 ownershipQualifier,
                                                  TZrBool callsClose,
                                                  TZrBool callsDestructor,
                                                  TZrUInt32 declarationOrder);

ZR_CORE_API void ZrCore_ObjectPrototype_SetIndexContract(SZrObjectPrototype *prototype,
                                                         const SZrIndexContract *contract);

ZR_CORE_API void ZrCore_ObjectPrototype_SetIterableContract(SZrObjectPrototype *prototype,
                                                            const SZrIterableContract *contract);

ZR_CORE_API void ZrCore_ObjectPrototype_SetIteratorContract(SZrObjectPrototype *prototype,
                                                            const SZrIteratorContract *contract);

ZR_CORE_API void ZrCore_ObjectPrototype_AddProtocol(SZrObjectPrototype *prototype, EZrProtocolId protocolId);

ZR_CORE_API TZrBool ZrCore_ObjectPrototype_ImplementsProtocol(SZrObjectPrototype *prototype, EZrProtocolId protocolId);

ZR_CORE_API TZrBool ZrCore_Object_IsInstanceOfPrototype(SZrObject *object, SZrObjectPrototype *prototype);

#endif // ZR_VM_CORE_OBJECT_H
