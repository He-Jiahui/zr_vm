//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_STRING_H
#define ZR_VM_CORE_STRING_H

#include <stdarg.h>
#include <string.h>

#include "zr_vm_core/conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/utf8.h"
struct SZrGlobalState;
struct SZrState;
struct SZrObject;
struct SZrTypeValue;
/** 仅接受编译期窄字符串字面量；用字节长度避免为常量再次扫描 NUL。 */
#define ZR_STRING_LITERAL(STATE, STR) (ZrCore_String_Create((STATE), "" STR, (sizeof(STR) / sizeof(char) - 1)))

/** 短串以内联字节参与驻留，长串持有独立原生缓冲区；0xff 是布局判别值。
 * 对象地址和借用的字节指针受所属 GC 域有效期约束，不应由调用方释放或修改。 */
struct ZR_STRUCT_ALIGN SZrString {
    SZrRawObject super;

    // 尾部
    union {
        TZrSize longStringLength;
        struct SZrString *nextShortString;
    };

    TZrUInt8 shortStringLength;
    // Short strings use raw bytes; long strings use an aligned native pointer.
    union {
        TZrUInt8 stringDataExtend[1];
        TZrNativeString longString;
    };
};

typedef struct SZrString SZrString;


/** 短串驻留表兼作 GC 根；young bucket 索引必须与哈希表当前容量同步。 */
struct ZR_STRUCT_ALIGN SZrStringTable {
    SZrHashSet stringHashSet;
    TZrBool isValid;
    TZrUInt8 *minorYoungBucketFlags;
    TZrSize *minorYoungBucketIndexes;
    TZrSize minorYoungBucketCapacity;
    TZrSize minorYoungBucketCount;
    SZrString *shortStringListHead;
};

typedef struct SZrStringTable SZrStringTable;

/** 格式化器的暂存窗口上限；超出窗口时把片段转入当前 state 的值栈。 */
#define ZR_STRING_FORMAT_BUFFER_SIZE (ZR_LOG_DEBUG_FUNCTION_STR_SIZE_MAX + ZR_NUMBER_TO_STRING_LENGTH_MAX + 95)

/** 格式化调用的一次性缓冲区；isOnStack 表示已有需继续拼接的 GC 字符串栈值。 */
struct ZR_STRUCT_ALIGN SZrNativeStringFormatBuffer {
    struct SZrState *state;
    TZrBool isOnStack;
    TZrSize length;
    char result[ZR_STRING_FORMAT_BUFFER_SIZE];
};

typedef struct SZrNativeStringFormatBuffer SZrNativeStringFormatBuffer;

/** 仅比较以 NUL 结尾的原生文本；二进制字符串应使用带长度的 VM 比较。 */
ZR_FORCE_INLINE TZrInt32 ZrCore_NativeString_Compare(TZrNativeString string1, TZrNativeString string2) {
    return strcmp(string1, string2);
}

/** GC minor 扫描前确认年轻代桶索引对应当前哈希表容量。 */
ZR_FORCE_INLINE TZrBool ZrCore_StringTable_MinorYoungBucketFlagsReady(const SZrStringTable *stringTable) {
    if (stringTable == ZR_NULL) {
        return ZR_FALSE;
    }

    return stringTable->minorYoungBucketCapacity == stringTable->stringHashSet.capacity &&
           (stringTable->stringHashSet.capacity == 0u ||
            (stringTable->minorYoungBucketFlags != ZR_NULL && stringTable->minorYoungBucketIndexes != ZR_NULL));
}

/** 驻留或迁移短串时登记桶，供 minor GC 限定扫描范围；重复登记不增加计数。 */
ZR_FORCE_INLINE TZrBool ZrCore_StringTable_RecordMinorYoungBucket(SZrStringTable *stringTable, TZrSize bucketIndex) {
    if (stringTable == ZR_NULL ||
        stringTable->minorYoungBucketFlags == ZR_NULL ||
        stringTable->minorYoungBucketIndexes == ZR_NULL ||
        bucketIndex >= stringTable->minorYoungBucketCapacity) {
        return ZR_FALSE;
    }

    if (stringTable->minorYoungBucketFlags[bucketIndex] == 0u) {
        ZR_ASSERT(stringTable->minorYoungBucketCount < stringTable->minorYoungBucketCapacity);
        stringTable->minorYoungBucketFlags[bucketIndex] = 1u;
        stringTable->minorYoungBucketIndexes[stringTable->minorYoungBucketCount++] = bucketIndex;
    }

    return ZR_TRUE;
}

/** GC 完成某个年轻代桶后撤销活动记录；调用者迭代时需接受无序压缩。 */
ZR_FORCE_INLINE void ZrCore_StringTable_ClearMinorYoungBucketAt(SZrStringTable *stringTable, TZrSize activeIndex) {
    TZrSize bucketIndex;

    if (stringTable == ZR_NULL ||
        stringTable->minorYoungBucketFlags == ZR_NULL ||
        stringTable->minorYoungBucketIndexes == ZR_NULL ||
        activeIndex >= stringTable->minorYoungBucketCount) {
        return;
    }

    bucketIndex = stringTable->minorYoungBucketIndexes[activeIndex];
    stringTable->minorYoungBucketFlags[bucketIndex] = 0u;
    stringTable->minorYoungBucketCount--;
    if (activeIndex < stringTable->minorYoungBucketCount) {
        stringTable->minorYoungBucketIndexes[activeIndex] =
                stringTable->minorYoungBucketIndexes[stringTable->minorYoungBucketCount];
    }
}

/** 查询驻留布局而非 UTF-8 字符数；空指针返回 false。 */
ZR_FORCE_INLINE TZrBool ZrCore_String_IsShort(const SZrString *string) {
    return string != ZR_NULL && string->shortStringLength < ZR_VM_LONG_STRING_FLAG;
}


/** 原生 NUL 文本长度；不适用于可能含内嵌零字节的 VM 字符串。 */
ZR_FORCE_INLINE TZrSize ZrCore_NativeString_Length(TZrNativeString string) { return strlen(string); }

/** 格式化器寻找下一占位符；返回值借用输入字符串。 */
ZR_FORCE_INLINE TZrChar *ZrCore_NativeString_CharFind(TZrNativeString string, TZrChar ch) { return strchr(string, ch); }

/** 数字格式化时识别已有小数部分；两个参数都须为 NUL 文本。 */
ZR_FORCE_INLINE TZrSize ZrCore_NativeString_Span(TZrNativeString string, TZrChar *charset) {
    return strspn(string, charset);
}

/** 将两个原生文本写入调用方缓冲区；result 至少容纳两段字节与结尾 NUL，且不得与输入区间重叠。 */
ZR_FORCE_INLINE TZrSize ZrCore_NativeString_Concat(TZrNativeString string1, TZrNativeString string2,
                                                   ZR_OUT TZrNativeString result) {
    TZrSize length1 = ZrCore_NativeString_Length(string1);
    TZrSize length2 = ZrCore_NativeString_Length(string2);
    TZrSize length = length1 + length2;
    strcpy(result, string1);
    strcpy(result + length1, string2);
    result[length] = '\0';
    return length;
}


/** 为单个码点编码；有效字节位于 buffer + ZR_STRING_UTF8_SIZE - 返回长度。 */
ZR_FORCE_INLINE TZrSize ZrCore_NativeString_Utf8CharLength(TZrChar *buffer, TZrUInt64 uChar) {
    TZrChar encoded[ZR_STRING_UTF8_SIZE];
    TZrSize length = 0;

    ZR_ASSERT(uChar <= 0xffffffffu);
    if (buffer == ZR_NULL || !ZrCore_Utf8_EncodeCodePoint((TZrUInt32)uChar, encoded, &length)) {
        return 0;
    }

    memcpy(buffer + ZR_STRING_UTF8_SIZE - length, encoded, length);
    return length;
}


/** @brief 格式化原生文本并把结果保留在 state 的值栈顶。
 * @note 返回值借用栈顶 GC 字符串；调用方不得释放，弹栈或可能移动对象的 GC 后须重新取视图。 */
ZR_CORE_API TZrNativeString ZrCore_NativeString_VFormat(struct SZrState *state, TZrNativeString format, va_list args);

/** @brief VFormat 的可变参数入口；支持的占位符须与实现的 va_arg 类型一致。 */
ZR_CORE_API TZrNativeString ZrCore_NativeString_Format(struct SZrState *state, TZrNativeString format, ...);

/** @brief 全局状态启动时建立短串驻留表；须先于主线程字符串初始化。 */
ZR_CORE_API void ZrCore_StringTable_New(struct SZrGlobalState *global);

/** @brief 全局状态关闭时回收驻留表索引；GC 对象由 GC 生命周期处理。 */
ZR_CORE_API void ZrCore_StringTable_Free(struct SZrGlobalState *global, SZrStringTable *stringTable);

/** @brief 主线程启动时初始化驻留桶和永久内存错误字符串。 */
ZR_CORE_API void ZrCore_StringTable_Init(struct SZrState *state);
/** @brief 哈希表扩容后同步 minor GC 的年轻代桶索引；失败返回 false 供调用方决定是否继续。 */
ZR_CORE_API TZrBool ZrCore_StringTable_SyncMinorYoungBucketFlags(struct SZrGlobalState *global);


/** @brief 按显式字节长度创建 GC 字符串；短串驻留，长串单独保存缓冲区。
 * @pre state 的 stringTable 已建立，短串路径的哈希桶已初始化；string 非空且至少有 length 个可读字节。 */
ZR_CORE_API SZrString *ZrCore_String_Create(struct SZrState *state, TZrNativeString string, TZrSize length);

/** 从 NUL 文本创建字符串；含内嵌零字节的输入需改用显式长度入口。 */
ZR_FORCE_INLINE SZrString *ZrCore_String_CreateFromNative(struct SZrState *state, TZrNativeString string) {
    return ZrCore_String_Create(state, string, ZrCore_NativeString_Length(string));
}

/** @brief 项目清单等重复原生文本的创建入口；先查询全局 API 字符串缓存。 */
ZR_CORE_API SZrString *ZrCore_String_CreateTryHitCache(struct SZrState *state, TZrNativeString string);


/** 借用短串内联字节；仅对短串有效，GC 移动或对象释放后视图失效。 */
ZR_FORCE_INLINE TZrNativeString ZrCore_String_GetNativeStringShort(const SZrString *string) {
    ZR_ASSERT(string->shortStringLength < ZR_VM_LONG_STRING_FLAG);
    return (TZrNativeString) string->stringDataExtend;
}

/** 借用长串缓冲区指针槽；不得修改槽或缓冲区，否则哈希/比较契约失效。 */
ZR_FORCE_INLINE TZrNativeString *ZrCore_String_GetNativeStringLong(const SZrString *string) {
    ZR_ASSERT(string->shortStringLength == ZR_VM_LONG_STRING_FLAG);
    return (TZrNativeString *)&string->longString;
}

/** 借用按布局选择的 NUL 字节视图；可含内嵌 NUL，移动 GC 后须重新取得，长度另取 GetByteLength。 */
ZR_FORCE_INLINE TZrNativeString ZrCore_String_GetNativeString(const SZrString *string) {
    if (string->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(string);
    } else {
        return *ZrCore_String_GetNativeStringLong(string);
    }
}

/** 返回存储字节数而非 Unicode 码点数；空指针约定为零。 */
ZR_FORCE_INLINE TZrSize ZrCore_String_GetByteLength(const SZrString *string) {
    if (string == ZR_NULL) {
        return 0;
    }

    return string->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? (TZrSize)string->shortStringLength
                   : string->longStringLength;
}

/** @brief 验证 UTF-8 并输出码点数；失败时调用方不应使用 outLength。 */
ZR_CORE_API TZrBool ZrCore_String_GetCodePointLength(const SZrString *string, TZrSize *outLength);

/** @brief 将码点序号转换为字节偏移，供切片等操作保持 UTF-8 边界。 */
ZR_CORE_API TZrBool ZrCore_String_CodePointCountToByteOffset(const SZrString *string,
                                                             TZrSize codePointCount,
                                                             TZrSize *outOffset);

/** @brief 把有效 UTF-8 的原始字节复制成 VM 数组；失败时 *outArray 置空。 */
ZR_CORE_API TZrBool ZrCore_String_ToByteArray(struct SZrState *state,
                                              const SZrString *string,
                                              struct SZrObject **outArray);

/** @brief 比较字符串字节内容；两个参数均须为非空且仍有效的 GC 字符串。 */
ZR_CORE_API TZrBool ZrCore_String_Equal(SZrString *string1, SZrString *string2);

/** 哈希容器的比较回调，复用字符串内容相等语义而不依赖 state。 */
ZR_FORCE_INLINE TZrBool ZrCore_String_Compare(struct SZrState *state, const SZrString *string1,
                                              const SZrString *string2) {
    ZR_UNUSED_PARAMETER(state);
    return ZrCore_String_Equal((SZrString *) string1, (SZrString *) string2);
}

/** @brief 合并 state 栈顶 count 个字符串为一个值；同一栈窗口不得并发重入。
 * @pre 栈顶至少有 count 个字符串；分配失败时窗口收缩为一个 null。 */
ZR_CORE_API void ZrCore_String_Concat(struct SZrState *state, TZrSize count);

/** @brief 可先把非字符串栈值转成文本的合并入口；转换可触发 GC。 */
ZR_CORE_API void ZrCore_String_ConcatSafe(struct SZrState *state, TZrSize count);

/** @brief 两个 GC 字符串的热路径拼接；短串结果可写入全局 pair cache，由 GC 扫描与重写。 */
ZR_CORE_API SZrString *ZrCore_String_ConcatPair(struct SZrState *state,
                                                const SZrString *left,
                                                const SZrString *right);

/** @brief VM 字符串与显式长度原生片段拼接；原生输入只在调用期间借用。 */
ZR_CORE_API SZrString *ZrCore_String_ConcatStringAndNative(struct SZrState *state,
                                                           const SZrString *stringValue,
                                                           TZrNativeString nativeString,
                                                           TZrSize nativeLength,
                                                           TZrBool stringOnLeft);


/** @brief 数字或原生指针转换成 GC 字符串，供 Value_ConvertToString 和元方法使用。 */
ZR_CORE_API SZrString *ZrCore_String_FromNumber(struct SZrState *state, struct SZrTypeValue *value);
// 对象转文本的入口位于 ZrCore_Value_ConvertToString。

// TODO: 确认是否需要单独的 UTF-8 验证或转码创建入口；当前 Create 仅按字节长度选择布局。

#endif // ZR_VM_CORE_STRING_H
