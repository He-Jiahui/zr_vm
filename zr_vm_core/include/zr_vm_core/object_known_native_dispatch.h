#ifndef ZR_VM_CORE_OBJECT_KNOWN_NATIVE_DISPATCH_H
#define ZR_VM_CORE_OBJECT_KNOWN_NATIVE_DISPATCH_H

#include "zr_vm_core/conf.h"

struct SZrObjectPrototype;
struct SZrState;
struct SZrTypeValue;
typedef struct ZrLibCallContext ZrLibCallContext;
typedef TZrBool (*FZrObjectKnownNativeDirectCallback)(ZrLibCallContext *context, struct SZrTypeValue *result);
typedef TZrBool (*FZrObjectKnownNativeReadonlyInlineGetFastCallback)(struct SZrState *state,
                                                                     const struct SZrTypeValue *selfValue,
                                                                     const struct SZrTypeValue *argument0,
                                                                     struct SZrTypeValue *result);
typedef TZrBool (*FZrObjectKnownNativeReadonlyInlineSetNoResultFastCallback)(struct SZrState *state,
                                                                             const struct SZrTypeValue *selfValue,
                                                                             const struct SZrTypeValue *argument0,
                                                                             const struct SZrTypeValue *argument1);

typedef struct SZrObjectKnownNativeDirectDispatch {
    FZrObjectKnownNativeDirectCallback callback;
    const void *moduleDescriptor;
    const void *typeDescriptor;
    const void *functionDescriptor;
    const void *methodDescriptor;
    const void *metaMethodDescriptor;
    FZrObjectKnownNativeReadonlyInlineGetFastCallback readonlyInlineGetFastCallback;
    FZrObjectKnownNativeReadonlyInlineSetNoResultFastCallback readonlyInlineSetNoResultFastCallback;
    struct SZrObjectPrototype *ownerPrototype;
    TZrUInt32 rawArgumentCount;
    TZrBool usesReceiver;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
} SZrObjectKnownNativeDirectDispatch;

typedef enum EZrObjectKnownNativeDirectDispatchFlag {
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NONE = 0,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND = 1u << 0,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT = 1u << 1,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN = 1u << 2,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT = 1u << 3,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL = 1u << 4
} EZrObjectKnownNativeDirectDispatchFlag;

typedef enum EZrObjectKnownNativeDirectDispatchHotFlag {
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_NONE = 0,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_GET_FAST_READY = 1u << 0,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_SET_NO_RESULT_FAST_READY = 1u << 1
} EZrObjectKnownNativeDirectDispatchHotFlag;

#endif
