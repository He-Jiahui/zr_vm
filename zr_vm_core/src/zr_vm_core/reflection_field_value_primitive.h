#ifndef ZR_VM_CORE_REFLECTION_FIELD_VALUE_PRIMITIVE_H
#define ZR_VM_CORE_REFLECTION_FIELD_VALUE_PRIMITIVE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrTypeLayoutField;
struct SZrTypeValue;

TZrBool ZrCore_ReflectionFieldValue_LoadPrimitive(
        struct SZrState *state,
        const struct SZrTypeLayoutField *fieldLayout,
        EZrValueType valueType,
        const TZrByte *fieldAddress,
        struct SZrTypeValue *result);

TZrBool ZrCore_ReflectionFieldValue_StorePrimitive(
        const struct SZrTypeLayoutField *fieldLayout,
        EZrValueType valueType,
        TZrByte *fieldAddress,
        const struct SZrTypeValue *value);

#endif
