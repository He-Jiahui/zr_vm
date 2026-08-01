#ifndef ZR_VM_CORE_PROPERTY_REFERENCE_H
#define ZR_VM_CORE_PROPERTY_REFERENCE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrFunction;
struct SZrTypeValueOnStack;

ZR_CORE_API TZrBool ZrCore_PropertyReference_CreateMember(
        struct SZrState *state,
        struct SZrFunction *function,
        struct SZrTypeValueOnStack *frameBase,
        TZrUInt32 receiverSlot,
        const SZrTypeValue *base,
        TZrUInt32 memberEntryIndex,
        SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_PropertyReference_CreateMemberByName(
        struct SZrState *state,
        const SZrTypeValue *base,
        const TZrChar *name,
        SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_PropertyReference_CreateIndex(
        struct SZrState *state,
        const SZrTypeValue *base,
        const SZrTypeValue *key,
        SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_PropertyReference_CreateFrameSlot(
        struct SZrState *state,
        struct SZrFunction *function,
        struct SZrTypeValueOnStack *frameBase,
        TZrUInt32 sourceSlot,
        SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_PropertyReference_IsValid(
        struct SZrState *state,
        SZrTypeValue *reference);

ZR_CORE_API TZrBool ZrCore_PropertyReference_Load(
        struct SZrState *state,
        SZrTypeValue *reference,
        SZrTypeValue *result);

ZR_CORE_API TZrBool ZrCore_PropertyReference_Store(
        struct SZrState *state,
        SZrTypeValue *reference,
        const SZrTypeValue *value);

#endif /* ZR_VM_CORE_PROPERTY_REFERENCE_H */
