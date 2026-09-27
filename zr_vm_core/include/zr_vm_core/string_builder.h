//
// 用原生可变缓冲区组装一个不可变运行时字符串。
//
#ifndef ZR_VM_CORE_STRING_BUILDER_H
#define ZR_VM_CORE_STRING_BUILDER_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrString;

/** @brief 原生缓冲区的独占持有者；data 已分配时始终容纳 length 字节及结尾 NUL。 */
/* state 仅借用；必须在 state/global 销毁前 Dispose 或成功 Freeze。 */
struct ZR_STRUCT_ALIGN SZrStringBuilder {
    struct SZrState *state;
    TZrNativeString data;
    TZrSize length;
    TZrSize capacity;
};

typedef struct SZrStringBuilder SZrStringBuilder;

/** @brief 初始化空构建器，可预留首段原生缓冲区。
 * @pre builder 未持有旧缓冲区，state 及其 global 有效。
 * @return 预留失败时为假，builder 不持有缓冲区。
 */
ZR_CORE_API TZrBool ZrCore_StringBuilder_Init(struct SZrState *state,
                                              SZrStringBuilder *builder,
                                              TZrSize initialCapacity);

/** @brief 释放原生缓冲区并清空构建器；可重复调用。 */
ZR_CORE_API void ZrCore_StringBuilder_Dispose(SZrStringBuilder *builder);

/** @brief 按指定字节数追加，允许嵌入 NUL；长度不是 C 字符串长度。
 * @pre 非零长度时 string 指向有效外部数据，不依赖本构建器可能扩容的缓冲区。
 * @return 参数或扩容失败时为假，已有内容与长度保持不变。
 */
ZR_CORE_API TZrBool ZrCore_StringBuilder_AppendNative(SZrStringBuilder *builder,
                                                      const TZrChar *string,
                                                      TZrSize length);

/** @brief 追加运行时字符串的字节内容；原字符串仍由调用方持有。 */
ZR_CORE_API TZrBool ZrCore_StringBuilder_AppendString(SZrStringBuilder *builder,
                                                      const struct SZrString *string);

/** @brief 复制内容为不可变运行时字符串；成功后自动 Dispose。
 * @return 创建失败时为空指针，原缓冲区仍由 builder 持有，可重试或释放。
 */
ZR_CORE_API struct SZrString *ZrCore_StringBuilder_Freeze(SZrStringBuilder *builder);

/** @brief 读取当前字节长度；空指针构建器返回零。 */
ZR_FORCE_INLINE TZrSize ZrCore_StringBuilder_GetLength(const SZrStringBuilder *builder) {
    return builder != ZR_NULL ? builder->length : 0u;
}

/** @brief 借用当前已分配的 NUL 终止缓冲区；尚未分配时为空，扩容或释放后失效。 */
ZR_FORCE_INLINE TZrNativeString ZrCore_StringBuilder_GetNativeString(const SZrStringBuilder *builder) {
    return builder != ZR_NULL ? builder->data : ZR_NULL;
}

#endif // ZR_VM_CORE_STRING_BUILDER_H
