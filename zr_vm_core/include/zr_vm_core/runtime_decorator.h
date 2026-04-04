//
// Runtime decorator definition-phase helpers.
//

#ifndef ZR_VM_CORE_RUNTIME_DECORATOR_H
#define ZR_VM_CORE_RUNTIME_DECORATOR_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrObject;
struct SZrObjectPrototype;
struct SZrFunction;
struct SZrString;

typedef enum EZrRuntimeDecoratorTargetKind {
    ZR_RUNTIME_DECORATOR_TARGET_KIND_INVALID = 0,
    ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD = 1,
    ZR_RUNTIME_DECORATOR_TARGET_KIND_METHOD = 2,
    ZR_RUNTIME_DECORATOR_TARGET_KIND_PROPERTY = 3
} EZrRuntimeDecoratorTargetKind;

ZR_CORE_API TZrInt64 ZrCore_RuntimeDecorator_ApplyNativeEntry(struct SZrState *state);

ZR_CORE_API TZrInt64 ZrCore_RuntimeDecorator_ApplyMemberNativeEntry(struct SZrState *state);

ZR_CORE_API void ZrCore_RuntimeDecorator_OverlayTypeReflection(struct SZrState *state,
                                                               struct SZrObject *typeReflection,
                                                               struct SZrObjectPrototype *prototype);

ZR_CORE_API void ZrCore_RuntimeDecorator_OverlayFunctionReflection(struct SZrState *state,
                                                                   struct SZrObject *reflectionObject,
                                                                   struct SZrFunction *function);

ZR_CORE_API void ZrCore_RuntimeDecorator_OverlayMemberReflection(struct SZrState *state,
                                                                 struct SZrObject *reflectionObject,
                                                                 struct SZrObjectPrototype *prototype,
                                                                 struct SZrString *memberName,
                                                                 EZrRuntimeDecoratorTargetKind targetKind);

#endif // ZR_VM_CORE_RUNTIME_DECORATOR_H
