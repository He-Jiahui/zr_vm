//
// Mutable native buffer used to assemble one immutable runtime string.
//
#include "zr_vm_core/string_builder.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

#define ZR_STRING_BUILDER_DEFAULT_CAPACITY ((TZrSize)32u)

static TZrBool string_builder_grow(SZrStringBuilder *builder, TZrSize requiredLength) {
    TZrSize nextCapacity;
    TZrNativeString nextData;

    if (builder == ZR_NULL || builder->state == ZR_NULL || builder->state->global == ZR_NULL ||
        requiredLength < builder->length || requiredLength > ZR_MAX_SIZE - 1u) {
        return ZR_FALSE;
    }
    if (requiredLength <= builder->capacity) {
        return ZR_TRUE;
    }

    nextCapacity = builder->capacity > 0u ? builder->capacity : ZR_STRING_BUILDER_DEFAULT_CAPACITY;
    while (nextCapacity < requiredLength) {
        if (nextCapacity > (ZR_MAX_SIZE - 1u) / 2u) {
            nextCapacity = requiredLength;
            break;
        }
        nextCapacity *= 2u;
    }
    if (nextCapacity > ZR_MAX_SIZE - 1u) {
        return ZR_FALSE;
    }

    nextData = (TZrNativeString)ZrCore_Memory_Allocate(builder->state->global,
                                                       builder->data,
                                                       builder->capacity + (builder->capacity > 0u ? 1u : 0u),
                                                       nextCapacity + 1u,
                                                       ZR_MEMORY_NATIVE_TYPE_STRING);
    if (nextData == ZR_NULL) {
        return ZR_FALSE;
    }

    builder->data = nextData;
    builder->capacity = nextCapacity;
    builder->data[builder->length] = '\0';
    return ZR_TRUE;
}

TZrBool ZrCore_StringBuilder_Init(SZrState *state,
                                  SZrStringBuilder *builder,
                                  TZrSize initialCapacity) {
    if (builder == ZR_NULL || state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    builder->state = state;
    builder->data = ZR_NULL;
    builder->length = 0u;
    builder->capacity = 0u;
    if (initialCapacity > 0u && !string_builder_grow(builder, initialCapacity)) {
        builder->state = ZR_NULL;
        return ZR_FALSE;
    }
    if (builder->data != ZR_NULL) {
        builder->data[0] = '\0';
    }
    return ZR_TRUE;
}

void ZrCore_StringBuilder_Dispose(SZrStringBuilder *builder) {
    if (builder == ZR_NULL) {
        return;
    }
    if (builder->data != ZR_NULL && builder->state != ZR_NULL && builder->state->global != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(builder->state->global,
                                      builder->data,
                                      builder->capacity + 1u,
                                      ZR_MEMORY_NATIVE_TYPE_STRING);
    }
    builder->state = ZR_NULL;
    builder->data = ZR_NULL;
    builder->length = 0u;
    builder->capacity = 0u;
}

TZrBool ZrCore_StringBuilder_AppendNative(SZrStringBuilder *builder,
                                          const TZrChar *string,
                                          TZrSize length) {
    TZrSize requiredLength;

    if (builder == ZR_NULL || builder->state == ZR_NULL || (string == ZR_NULL && length > 0u) ||
        length > ZR_MAX_SIZE - builder->length) {
        return ZR_FALSE;
    }
    requiredLength = builder->length + length;
    if (!string_builder_grow(builder, requiredLength)) {
        return ZR_FALSE;
    }
    if (length > 0u) {
        memcpy(builder->data + builder->length, string, length);
        builder->length = requiredLength;
        builder->data[builder->length] = '\0';
    }
    return ZR_TRUE;
}

TZrBool ZrCore_StringBuilder_AppendString(SZrStringBuilder *builder,
                                          const SZrString *string) {
    if (string == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_StringBuilder_AppendNative(builder,
                                             ZrCore_String_GetNativeString(string),
                                             ZrCore_String_GetByteLength(string));
}

SZrString *ZrCore_StringBuilder_Freeze(SZrStringBuilder *builder) {
    SZrString *result;

    if (builder == ZR_NULL || builder->state == ZR_NULL) {
        return ZR_NULL;
    }
    if (builder->length == 0u) {
        result = ZrCore_String_Create(builder->state, "", 0u);
    } else {
        result = ZrCore_String_Create(builder->state, builder->data, builder->length);
    }
    if (result == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_StringBuilder_Dispose(builder);
    return result;
}
