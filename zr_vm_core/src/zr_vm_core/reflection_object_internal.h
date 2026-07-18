#ifndef ZR_VM_REFLECTION_OBJECT_INTERNAL_H
#define ZR_VM_REFLECTION_OBJECT_INTERNAL_H

#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

TZrBool ZrCore_Reflection_ObjectPinRaw(
        SZrState *state,
        SZrRawObject *object,
        TZrBool *addedByCaller);
void ZrCore_Reflection_ObjectUnpinRaw(
        SZrGlobalState *global,
        SZrRawObject *object,
        TZrBool addedByCaller);
TZrBool ZrCore_Reflection_ObjectPinValue(
        SZrState *state,
        const SZrTypeValue *value,
        TZrBool *addedByCaller);
void ZrCore_Reflection_ObjectUnpinValue(
        SZrGlobalState *global,
        const SZrTypeValue *value,
        TZrBool addedByCaller);
SZrString *ZrCore_Reflection_ObjectMakeString(
        SZrState *state,
        const TZrChar *text);
TZrBool ZrCore_Reflection_ObjectSetFieldValue(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        const SZrTypeValue *value);
TZrBool ZrCore_Reflection_ObjectSetString(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        const TZrChar *value);
TZrBool ZrCore_Reflection_ObjectSetBool(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrBool value);
TZrBool ZrCore_Reflection_ObjectSetObject(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        SZrObject *value,
        EZrValueType valueType);
const SZrTypeValue *ZrCore_Reflection_ObjectGetFieldValue(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName);

#endif
