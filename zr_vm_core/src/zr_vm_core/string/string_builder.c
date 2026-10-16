//
// 用原生可变缓冲区组装一个不可变运行时字符串。
//
#include "zr_vm_core/string_builder.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

/* 首次追加从小块开始，后续按倍数扩容并保留结尾 NUL 空间。 */
#define ZR_STRING_BUILDER_DEFAULT_CAPACITY ((TZrSize)32u)

/* 先计算安全容量并申请新缓冲区，成功后才更新 builder 的指针与容量。 */
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

/* 仅为尚无资源的 builder 初始化；预留失败后可重新初始化。 */
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

/* 原生缓冲区必须由仍存活的 state/global 对应分配器释放。 */
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

/* 长度以字节计，成功追加后仍维护终止 NUL；失败不推进逻辑长度。 */
TZrBool ZrCore_StringBuilder_AppendNative(SZrStringBuilder *builder,
                                          const TZrChar *string,
                                          TZrSize length) {
    TZrSize requiredLength;

    if (builder == ZR_NULL || builder->state == ZR_NULL || (string == ZR_NULL && length > 0u) ||
        length > ZR_MAX_SIZE - builder->length) {
        return ZR_FALSE;
    }
    requiredLength = builder->length + length;
    /* TODO: GetNativeString 暴露本缓冲区；若以该指针作为 string 且本次扩容搬迁，
     * 下方 memcpy 仍读取旧地址。现有调用只传外部字符串，需补搬迁分配器下的自追加测试。 */
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

/* 运行时字符串的字节视图只在本次追加期间借用。 */
TZrBool ZrCore_StringBuilder_AppendString(SZrStringBuilder *builder,
                                          const SZrString *string) {
    if (string == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_StringBuilder_AppendNative(builder,
                                             ZrCore_String_GetNativeString(string),
                                             ZrCore_String_GetByteLength(string));
}

/* 创建不可变字符串成功后才释放 builder，失败时保留内容供调用方处理。 */
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
