//
// Runtime reflection helpers for `%type`.
//

#ifndef ZR_VM_CORE_REFLECTION_H
#define ZR_VM_CORE_REFLECTION_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrObject;
struct SZrObjectPrototype;
struct SZrObjectModule;
struct SZrFunction;
struct SZrString;
struct SZrTypeValue;

ZR_CORE_API TZrInt64 ZrCore_Reflection_TypeOfNativeEntry(struct SZrState *state);

ZR_CORE_API TZrBool ZrCore_Reflection_TypeOfValue(struct SZrState *state,
                                                  const struct SZrTypeValue *targetValue,
                                                  struct SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_Reflection_IsReflectionObject(struct SZrState *state, struct SZrObject *object);

ZR_CORE_API struct SZrString *ZrCore_Reflection_FormatObject(struct SZrState *state, struct SZrObject *object);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildTypeLiteralObject(struct SZrState *state,
                                                                       struct SZrString *typeName);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildCallableTypeLiteralObject(
        struct SZrState *state,
        struct SZrString *callableName,
        struct SZrString *returnTypeName,
        struct SZrString *const *parameterNames,
        struct SZrString *const *parameterTypeNames,
        struct SZrString *const *parameterModeNames,
        TZrUInt32 parameterCount,
        struct SZrString *const *genericParameterNames,
        TZrUInt32 genericParameterCount,
        TZrBool isVariadic);

ZR_CORE_API void ZrCore_Reflection_AttachModuleRuntimeMetadata(struct SZrState *state,
                                                               struct SZrObjectModule *module,
                                                               struct SZrFunction *entryFunction);

ZR_CORE_API void ZrCore_Reflection_AttachPrototypeRuntimeMetadata(struct SZrState *state,
                                                                  struct SZrObjectPrototype *prototype,
                                                                  struct SZrObjectModule *module,
                                                                  struct SZrFunction *entryFunction);

ZR_CORE_API struct SZrObject *ZrCore_Reflection_BuildDecoratorTargetMemberReflection(
        struct SZrState *state,
        struct SZrObjectPrototype *prototype,
        struct SZrString *memberName,
        TZrUInt32 targetKind);

#endif // ZR_VM_CORE_REFLECTION_H
