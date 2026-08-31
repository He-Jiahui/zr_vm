//
// Mutable native buffer used to assemble one immutable runtime string.
//
#ifndef ZR_VM_CORE_STRING_BUILDER_H
#define ZR_VM_CORE_STRING_BUILDER_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrString;

struct ZR_STRUCT_ALIGN SZrStringBuilder {
    struct SZrState *state;
    TZrNativeString data;
    TZrSize length;
    TZrSize capacity;
};

typedef struct SZrStringBuilder SZrStringBuilder;

ZR_CORE_API TZrBool ZrCore_StringBuilder_Init(struct SZrState *state,
                                              SZrStringBuilder *builder,
                                              TZrSize initialCapacity);

ZR_CORE_API void ZrCore_StringBuilder_Dispose(SZrStringBuilder *builder);

ZR_CORE_API TZrBool ZrCore_StringBuilder_AppendNative(SZrStringBuilder *builder,
                                                      const TZrChar *string,
                                                      TZrSize length);

ZR_CORE_API TZrBool ZrCore_StringBuilder_AppendString(SZrStringBuilder *builder,
                                                      const struct SZrString *string);

ZR_CORE_API struct SZrString *ZrCore_StringBuilder_Freeze(SZrStringBuilder *builder);

ZR_FORCE_INLINE TZrSize ZrCore_StringBuilder_GetLength(const SZrStringBuilder *builder) {
    return builder != ZR_NULL ? builder->length : 0u;
}

ZR_FORCE_INLINE TZrNativeString ZrCore_StringBuilder_GetNativeString(const SZrStringBuilder *builder) {
    return builder != ZR_NULL ? builder->data : ZR_NULL;
}

#endif // ZR_VM_CORE_STRING_BUILDER_H
