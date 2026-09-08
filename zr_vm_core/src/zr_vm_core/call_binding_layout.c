#include "zr_vm_core/call_binding.h"

#include <string.h>

#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/hash.h"

struct SZrFunction *ZrCore_CallBinding_PrototypeOwner(struct SZrFunction *function) {
    if (function == ZR_NULL) return ZR_NULL;
    if (function->prototypeData != ZR_NULL) return function;
    if (function->prototypeContextFunction != ZR_NULL &&
        function->prototypeContextFunction->prototypeData != ZR_NULL) return function->prototypeContextFunction;
    while (function->ownerFunction != ZR_NULL) {
        function = function->ownerFunction;
        if (function->prototypeData != ZR_NULL) return function;
    }
    return ZR_NULL;
}

TZrUInt64 ZrCore_CallBinding_PrototypeLayoutHash(const struct SZrFunction *function,
                                               TZrUInt32 prototypeIndex) {
    TZrUInt32 count;
    TZrSize offset = sizeof(count);
    if (function == ZR_NULL || function->prototypeData == ZR_NULL ||
        function->prototypeDataLength < sizeof(count)) return 0u;
    memcpy(&count, function->prototypeData, sizeof(count));
    if (count != function->prototypeCount || prototypeIndex >= count) return 0u;
    for (TZrUInt32 index = 0u; index < count; ++index) {
        SZrCompiledPrototypeInfo info;
        TZrUInt64 length;
        if (function->prototypeDataLength - offset < sizeof(info)) return 0u;
        memcpy(&info, function->prototypeData + offset, sizeof(info));
        length = sizeof(info) + ((TZrUInt64)info.inheritsCount + info.decoratorsCount) * sizeof(TZrUInt32) +
                (TZrUInt64)info.membersCount * sizeof(SZrCompiledMemberInfo);
        if (length > function->prototypeDataLength - offset) return 0u;
        if (index == prototypeIndex) return ZrCore_Hash_CreateStable64(
                function->prototypeData + offset, (TZrSize)length);
        offset += (TZrSize)length;
    }
    return 0u;
}
