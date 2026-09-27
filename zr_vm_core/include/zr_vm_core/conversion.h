//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_CONVERSION_H
#define ZR_VM_CORE_CONVERSION_H

#include "zr_vm_core/conf.h"

/** @brief 显式执行 C 转型；不检验数值范围、对象动态类型或指针布局。 */
#define ZR_CAST(TARGET_TYPE, EXPRESSION) ((TARGET_TYPE) (EXPRESSION))


/** @brief 调试构建断言原生标志与对象种类后转型。
 * @pre EXPRESSION 非空且无副作用；调试构建会多次求值，发布构建不执行类型检查。 */
#define ZR_CAST_CHECKED_NATIVE(STATE, TARGET_TYPE, EXPRESSION, TYPE_ENUM, IS_NATIVE)                                   \
    (ZR_CHECK(STATE, ((EXPRESSION)->type == (TYPE_ENUM) && (EXPRESSION)->isNative == (IS_NATIVE)), "type mismatch."),  \
     ZR_CAST(TARGET_TYPE, (EXPRESSION)))

/** @brief 调试构建断言原始对象种类后转型。
 * @pre EXPRESSION 非空且无副作用；发布构建仅保留 C 转型，调用方须先保证实际类型。 */
#define ZR_CAST_CHECKED(STATE, TARGET_TYPE, EXPRESSION, TYPE_ENUM)                                                     \
    (ZR_CHECK(STATE, ((EXPRESSION)->type == (TYPE_ENUM)), "type mismatch."), ZR_CAST(TARGET_TYPE, (EXPRESSION)))

// 下列标量与地址别名只固定目标 C 类型；窄化、符号转换和指针对齐仍由调用方保证。
#define ZR_CAST_VOID(EXP) ZR_CAST(void, (EXP))
#define ZR_CAST_BOOL(EXP) ZR_CAST(TZrBool, (EXP))
#define ZR_CAST_CHAR(EXP) ZR_CAST(TZrChar, (EXP))
#define ZR_CAST_INT8(EXP) ZR_CAST(TZrInt8, (EXP))
#define ZR_CAST_UINT8(EXP) ZR_CAST(TZrUInt8, (EXP))
#define ZR_CAST_UINT8_PTR(EXP) ZR_CAST(TZrUInt8 *, (EXP))
#define ZR_CAST_INT32(EXP) ZR_CAST(TZrInt32, (EXP))
#define ZR_CAST_INT(EXP) ZR_CAST(TZrInt32, (EXP))
#define ZR_CAST_UINT(EXP) ZR_CAST(TZrUInt32, (EXP))
#define ZR_CAST_INT64(EXP) ZR_CAST(TZrInt64, (EXP))
#define ZR_CAST_UINT64(EXP) ZR_CAST(TZrUInt64, (EXP))
#define ZR_CAST_UINT64_PTR(EXP) ZR_CAST(TZrUInt64 *, (EXP))
#define ZR_CAST_FLOAT(EXP) ZR_CAST(TZrFloat, (EXP))
#define ZR_CAST_NATIVE_STRING(EXP) ZR_CAST(TZrNativeString, (EXP))
#define ZR_CAST_MEMORY_OFFSET(EXP) ZR_CAST(TZrMemoryOffset, (EXP))
#define ZR_CAST_PTR(EXP) ZR_CAST(TZrPtr, (EXP))
#define ZR_CAST_SIZE(EXP) ZR_CAST(TZrSize, (EXP))


// 原始对象与哈希表、栈槽的指针别名不改变所有权或 GC 根关系。
#define ZR_CAST_RAW_OBJECT(EXP) ZR_CAST(SZrRawObject *, (EXP))
#define ZR_CAST_RAW_OBJECT_PTR(EXP) ZR_CAST(SZrRawObject **, (EXP))
/** @brief 取得对象内嵌 super 成员的地址，交给 GC、值槽及对象接口。
 * @pre EXP 非空且其静态类型确有 SZrRawObject super 成员；结果借用原对象寿命。 */
#define ZR_CAST_RAW_OBJECT_AS_SUPER(EXP) ZR_CAST(SZrRawObject *, &((EXP)->super))
#define ZR_CAST_HASH_KEY_VALUE_PAIR(EXP) ZR_CAST(SZrHashKeyValuePair *, (EXP))
#define ZR_CAST_HASH_KEY_VALUE_PAIR_PTR(EXP) ZR_CAST(SZrHashKeyValuePair **, (EXP))

#define ZR_CAST_STACK_VALUE(EXP) ZR_CAST(SZrTypeValueOnStack *, (EXP))
#define ZR_CAST_FROM_STACK_VALUE(EXP) ZR_CAST(SZrTypeValue *, (EXP))

#define ZR_CAST_CALL_INFO(EXP) ZR_CAST(SZrCallInfo *, (EXP))

/** @brief 从已标记为 native pointer 的值槽取本机函数地址。
 * @pre EXP 指向有效值槽，且 value.nativeFunction 对应该槽当前类型；此宏不做运行时检查。 */
#define ZR_CAST_FUNCTION_POINTER(EXP) ZR_CAST(FZrNativeFunction, (EXP)->value.nativeFunction)

/** @brief 将对象或数组原始对象解释为共用的 SZrObject 布局。
 * @pre EXP 非空、无副作用且动态种类为 OBJECT 或 ARRAY；种类仅在调试构建断言。 */
#define ZR_CAST_OBJECT(STATE, EXP)                                                                                      \
    (ZR_CHECK(STATE,                                                                                                     \
              ((EXP)->type == ZR_RAW_OBJECT_TYPE_OBJECT || (EXP)->type == ZR_RAW_OBJECT_TYPE_ARRAY),                   \
              "type mismatch."),                                                                                         \
     ZR_CAST(SZrObject *, (EXP)))

/** @brief 从原始对象取得字符串布局；类型约束由调试断言检查。 */
#define ZR_CAST_STRING(STATE, EXP) ZR_CAST_CHECKED(STATE, SZrString *, (EXP), ZR_RAW_OBJECT_TYPE_STRING)
/** @brief 借用短串内联字节区的地址；不复制字节。
 * @pre EXP 非空且 shortStringLength 表示短串；长串须用 ZrCore_String_GetNativeString。
 * BUG: VFormat 与 CreateTryHitCache 可传入长串，此宏把 longString 指针槽当作字节串。 */
#define ZR_CAST_STRING_TO_NATIVE(EXP) ZR_CAST(TZrNativeString, (EXP)->stringDataExtend)


/** @brief 断言原生闭包的对象种类与 native 标志后取得其布局。 */
#define ZR_CAST_NATIVE_CLOSURE(STATE, EXP)                                                                             \
    ZR_CAST_CHECKED_NATIVE(STATE, SZrClosureNative *, (EXP), ZR_RAW_OBJECT_TYPE_CLOSURE, ZR_TRUE)
/** @brief 断言 VM 闭包种类且 native 为假后取得其布局。 */
#define ZR_CAST_VM_CLOSURE(STATE, EXP)                                                                                 \
    ZR_CAST_CHECKED_NATIVE(STATE, SZrClosure *, (EXP), ZR_RAW_OBJECT_TYPE_CLOSURE, ZR_FALSE)
/** @brief 断言 VM 闭包捕获值种类且 native 为假后取得其布局。 */
#define ZR_CAST_VM_CLOSURE_VALUE(STATE, EXP)                                                                           \
    ZR_CAST_CHECKED_NATIVE(STATE, SZrClosureValue *, (EXP), ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE, ZR_FALSE)
/** @brief 将闭包原始对象转为 TZrClosure 指针；调试构建只断言对象种类，不区分 native 标志。 */
#define ZR_CAST_CLOSURE(STATE, EXP) ZR_CAST_CHECKED(STATE, TZrClosure *, (EXP), ZR_RAW_OBJECT_TYPE_CLOSURE)
/** @brief 从原始对象取得函数布局；类型约束由调试断言检查。 */
#define ZR_CAST_FUNCTION(STATE, EXP) ZR_CAST_CHECKED(STATE, SZrFunction *, (EXP), ZR_RAW_OBJECT_TYPE_FUNCTION)
/** @brief 调试构建断言原生函数对象后转为本机函数类型。
 * TODO: 当前仓库未见直接调用；启用前须核对原始对象指针转函数指针的 ABI 与取值来源。 */
#define ZR_CAST_NATIVE_FUNCTION(STATE, EXP)                                                                            \
    ZR_CAST_CHECKED_NATIVE(STATE, FZrNativeFunction, (EXP), ZR_RAW_OBJECT_TYPE_FUNCTION, ZR_TRUE)
#endif // ZR_VM_CORE_CONVERSION_H
