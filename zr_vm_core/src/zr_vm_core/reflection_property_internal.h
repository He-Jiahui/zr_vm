#ifndef ZR_VM_CORE_REFLECTION_PROPERTY_INTERNAL_H
#define ZR_VM_CORE_REFLECTION_PROPERTY_INTERNAL_H

#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"

typedef struct SZrReflectionPropertyHost {
    SZrObject *(*buildMemberInfo)(SZrState *state,
                                 const TZrChar *name,
                                 const TZrChar *qualifiedName,
                                 const TZrChar *kind,
                                 TZrUInt64 hash);
    void (*assignOwnerLinks)(SZrState *state,
                             SZrObject *memberReflection,
                             SZrObject *ownerReflection,
                             SZrObject *moduleReflection);
    void (*populateCompiledMetadata)(SZrState *state,
                                     SZrObject *memberReflection,
                                     SZrFunction *entryFunction,
                                     const SZrCompiledMemberInfo *member);
    void (*populateDecoratorMetadata)(SZrState *state,
                                      SZrObject *memberReflection,
                                      SZrFunction *entryFunction,
                                      const SZrCompiledMemberInfo *member);
    SZrFunction *(*extractFunction)(SZrState *state,
                                    SZrFunction *entryFunction,
                                    TZrUInt32 constantIndex);
    void (*populateParameters)(SZrState *state,
                               SZrObject *callableReflection,
                               SZrFunction *function,
                               TZrUInt32 visibleParameterCount);
    void (*populateFunctionMetadata)(SZrState *state,
                                     SZrObject *reflectionObject,
                                     SZrFunction *function);
    const TZrChar *(*stringFromConstant)(SZrState *state,
                                         SZrFunction *entryFunction,
                                         TZrUInt32 constantIndex,
                                         const TZrChar *fallback);
    void (*setFieldBool)(SZrState *state,
                         SZrObject *object,
                         const TZrChar *fieldName,
                         TZrBool value);
    void (*setFieldInt)(SZrState *state,
                        SZrObject *object,
                        const TZrChar *fieldName,
                        TZrInt64 value);
    void (*setFieldString)(SZrState *state,
                           SZrObject *object,
                           const TZrChar *fieldName,
                           const TZrChar *value);
    void (*setFieldObject)(SZrState *state,
                           SZrObject *object,
                           const TZrChar *fieldName,
                           SZrObject *fieldObject,
                           EZrValueType valueType);
    void (*addNamedEntry)(SZrState *state,
                          SZrObject *membersObject,
                          const TZrChar *memberName,
                          SZrObject *entryObject);
} SZrReflectionPropertyHost;

TZrBool ZrCore_ReflectionProperty_IsCanonicalCarrier(
        const SZrCompiledMemberInfo *member);
TZrBool ZrCore_ReflectionProperty_ShouldSkipCanonicalMember(
        const SZrCompiledMemberInfo *members,
        TZrUInt32 memberCount,
        const SZrCompiledMemberInfo *member);
void ZrCore_ReflectionProperty_PopulateCurrent(
        SZrState *state,
        SZrObject *membersObject,
        SZrObject *typeReflection,
        SZrObject *moduleReflection,
        SZrObjectPrototype *prototype,
        const TZrChar *qualifiedTypeName,
        SZrFunction *entryFunction,
        const SZrCompiledMemberInfo *members,
        TZrUInt32 memberCount,
        const SZrReflectionPropertyHost *host);

#endif
