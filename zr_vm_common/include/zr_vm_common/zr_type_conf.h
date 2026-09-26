//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_TYPE_CONF_H
#define ZR_TYPE_CONF_H


#include "zr_vm_common/zr_common_conf.h"

//
/** @brief VM 值的运行时标签；连续区段被下方谓词与 switch 宏当作分类契约。
 *  变更顺序时须同时核对 core 存储、parser 类型映射和序列化兼容性。 */
enum EZrValueType {
    // BASIC
    ZR_VALUE_TYPE_NULL,
    // BASIC BOOL
    ZR_VALUE_TYPE_BOOL,
    // BASIC INT NUMBER SIGNED
    ZR_VALUE_TYPE_INT8,
    // BASIC INT NUMBER SIGNED
    ZR_VALUE_TYPE_INT16,
    // BASIC INT NUMBER SIGNED
    ZR_VALUE_TYPE_INT32,
    // BASIC INT NUMBER SIGNED
    ZR_VALUE_TYPE_INT64,
    // BASIC INT NUMBER UNSIGNED
    ZR_VALUE_TYPE_UINT8,
    // BASIC INT NUMBER UNSIGNED
    ZR_VALUE_TYPE_UINT16,
    // BASIC INT NUMBER UNSIGNED
    ZR_VALUE_TYPE_UINT32,
    // BASIC INT NUMBER UNSIGNED
    ZR_VALUE_TYPE_UINT64,
    // BASIC FLOAT NUMBER
    ZR_VALUE_TYPE_FLOAT,
    // BASIC FLOAT NUMBER
    ZR_VALUE_TYPE_DOUBLE,
    // BASIC STRING
    ZR_VALUE_TYPE_STRING,
    // ZR_ITEM
    ZR_VALUE_TYPE_BUFFER,
    // ZR_ITEM
    ZR_VALUE_TYPE_ARRAY,
    // ZR_ITEM
    ZR_VALUE_TYPE_FUNCTION,
    // ZR_ITEM
    ZR_VALUE_TYPE_CLOSURE_VALUE,
    // ZR_ITEM
    ZR_VALUE_TYPE_CLOSURE,
    // ZR_ITEM
    ZR_VALUE_TYPE_OBJECT,
    // ZR_ITEM
    ZR_VALUE_TYPE_THREAD,
    // BASIC NATIVE
    ZR_VALUE_TYPE_NATIVE_POINTER,
    // BASIC NATIVE
    ZR_VALUE_TYPE_NATIVE_DATA,
    // BASIC NATIVE INTERNAL
    ZR_VALUE_TYPE_VM_MEMORY,

    //
    ZR_VALUE_TYPE_UNKNOWN,

    ZR_VALUE_TYPE_ENUM_MAX
};

typedef enum EZrValueType EZrValueType;

/** @brief SemIR/AOT 的静态 C 表示类别；DYNAMIC 保留运行时选择，GC_REF 与原生指针不可互换。 */
typedef enum EZrStaticCType {
    ZR_STATIC_C_TYPE_DYNAMIC = 0,
    ZR_STATIC_C_TYPE_BOOL,
    ZR_STATIC_C_TYPE_I8,
    ZR_STATIC_C_TYPE_I16,
    ZR_STATIC_C_TYPE_I32,
    ZR_STATIC_C_TYPE_I64,
    ZR_STATIC_C_TYPE_U8,
    ZR_STATIC_C_TYPE_U16,
    ZR_STATIC_C_TYPE_U32,
    ZR_STATIC_C_TYPE_U64,
    ZR_STATIC_C_TYPE_F32,
    ZR_STATIC_C_TYPE_F64,
    ZR_STATIC_C_TYPE_GC_REF,
    ZR_STATIC_C_TYPE_STRUCT,
    ZR_STATIC_C_TYPE_NATIVE_POINTER,
    ZR_STATIC_C_TYPE_NATIVE_DATA
} EZrStaticCType;

/* 类型谓词供 core 值操作和编译器分支共享；区间判定依赖 EZrValueType 的连续编号。 */
#define ZR_VALUE_IS_TYPE_NULL(valueType) ((valueType) == ZR_VALUE_TYPE_NULL)
#define ZR_VALUE_IS_TYPE_BOOL(valueType) ((valueType) == ZR_VALUE_TYPE_BOOL)
#define ZR_VALUE_IS_TYPE_SIGNED_INT(valueType) ((valueType) >= ZR_VALUE_TYPE_INT8 && (valueType) <= ZR_VALUE_TYPE_INT64)
#define ZR_VALUE_IS_TYPE_UNSIGNED_INT(valueType)                                                                       \
    ((valueType) >= ZR_VALUE_TYPE_UINT8 && (valueType) <= ZR_VALUE_TYPE_UINT64)
#define ZR_VALUE_IS_TYPE_INT(valueType)                                                                                \
    (ZR_VALUE_IS_TYPE_SIGNED_INT(valueType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(valueType))
#define ZR_VALUE_IS_TYPE_FLOAT(valueType) ((valueType) >= ZR_VALUE_TYPE_FLOAT && (valueType) <= ZR_VALUE_TYPE_DOUBLE)
#define ZR_VALUE_IS_TYPE_NUMBER(valueType) (ZR_VALUE_IS_TYPE_INT(valueType) || ZR_VALUE_IS_TYPE_FLOAT(valueType))
#define ZR_VALUE_IS_TYPE_STRING(valueType) ((valueType) == ZR_VALUE_TYPE_STRING)
#define ZR_VALUE_IS_TYPE_NATIVE(valueType)                                                                             \
    ((valueType) == ZR_VALUE_TYPE_NATIVE_POINTER || (valueType) == ZR_VALUE_TYPE_NATIVE_DATA ||                        \
     (valueType) == ZR_VALUE_TYPE_VM_MEMORY)

#define ZR_VALUE_IS_TYPE_BASIC(valueType)                                                                              \
    (((valueType) >= ZR_VALUE_TYPE_NULL && (valueType) <= ZR_VALUE_TYPE_STRING) || ZR_VALUE_IS_TYPE_NATIVE(valueType))

#define ZR_VALUE_IS_TYPE_BUFFER(valueType) ((valueType) == ZR_VALUE_TYPE_BUFFER)
#define ZR_VALUE_IS_TYPE_ARRAY(valueType) ((valueType) == ZR_VALUE_TYPE_ARRAY)
#define ZR_VALUE_IS_TYPE_FUNCTION(valueType) ((valueType) == ZR_VALUE_TYPE_FUNCTION)
#define ZR_VALUE_IS_TYPE_CLOSURE(valueType) ((valueType) == ZR_VALUE_TYPE_CLOSURE)
#define ZR_VALUE_IS_TYPE_CLOSURE_VALUE(valueType) ((valueType) == ZR_VALUE_TYPE_CLOSURE_VALUE)
#define ZR_VALUE_IS_TYPE_OBJECT(valueType) ((valueType) == ZR_VALUE_TYPE_OBJECT)
#define ZR_VALUE_IS_TYPE_THREAD(valueType) ((valueType) == ZR_VALUE_TYPE_THREAD)

#define ZR_VALUE_IS_TYPE_ZR_ITEM(valueType) ((valueType) >= ZR_VALUE_TYPE_BUFFER && (valueType) <= ZR_VALUE_TYPE_THREAD)

// normal types all can be used in zr_vm (also can convert to string), the others can't be used in zr_vm
#define ZR_VALUE_IS_TYPE_NORMAL(valueType)                                                                             \
    ((valueType) >= ZR_VALUE_TYPE_NULL && (valueType) <= ZR_VALUE_TYPE_NATIVE_DATA)

/* switch case 组与上述区间保持一致，用于数值转换和运行时运算分派。 */
#define ZR_VALUE_CASES_SIGNED_INT                                                                                      \
    case ZR_VALUE_TYPE_INT8:                                                                                           \
    case ZR_VALUE_TYPE_INT16:                                                                                          \
    case ZR_VALUE_TYPE_INT32:                                                                                          \
    case ZR_VALUE_TYPE_INT64:

#define ZR_VALUE_CASES_UNSIGNED_INT                                                                                    \
    case ZR_VALUE_TYPE_UINT8:                                                                                          \
    case ZR_VALUE_TYPE_UINT16:                                                                                         \
    case ZR_VALUE_TYPE_UINT32:                                                                                         \
    case ZR_VALUE_TYPE_UINT64:

#define ZR_VALUE_CASES_INT                                                                                             \
    ZR_VALUE_CASES_SIGNED_INT                                                                                          \
    ZR_VALUE_CASES_UNSIGNED_INT

#define ZR_VALUE_CASES_FLOAT                                                                                           \
    case ZR_VALUE_TYPE_FLOAT:                                                                                          \
    case ZR_VALUE_TYPE_DOUBLE:

#define ZR_VALUE_CASES_NUMBER                                                                                          \
    ZR_VALUE_CASES_INT                                                                                                 \
    ZR_VALUE_CASES_FLOAT

#define ZR_VALUE_CASES_NATIVE                                                                                          \
    case ZR_VALUE_TYPE_NATIVE_POINTER:                                                                                 \
    case ZR_VALUE_TYPE_NATIVE_DATA:

/* 编译期整数范围以 TZrInt64 比较；uint64 上界受 AST 字面量存储宽度限制。 */
#define ZR_TYPE_RANGE_INT8_MIN ((TZrInt64)INT8_MIN)
#define ZR_TYPE_RANGE_INT8_MAX ((TZrInt64)INT8_MAX)
#define ZR_TYPE_RANGE_INT16_MIN ((TZrInt64)INT16_MIN)
#define ZR_TYPE_RANGE_INT16_MAX ((TZrInt64)INT16_MAX)
#define ZR_TYPE_RANGE_INT32_MIN ((TZrInt64)INT32_MIN)
#define ZR_TYPE_RANGE_INT32_MAX ((TZrInt64)INT32_MAX)
#define ZR_TYPE_RANGE_INT64_MIN ((TZrInt64)INT64_MIN)
#define ZR_TYPE_RANGE_INT64_MAX ((TZrInt64)INT64_MAX)
#define ZR_TYPE_RANGE_UINT8_MAX ((TZrInt64)UINT8_MAX)
#define ZR_TYPE_RANGE_UINT16_MAX ((TZrInt64)UINT16_MAX)
#define ZR_TYPE_RANGE_UINT32_MAX ((TZrInt64)UINT32_MAX)
/* AST integer literals are stored as TZrInt64, so uint64 literal-range checks cannot exceed INT64_MAX. */
#define ZR_TYPE_RANGE_UINT64_MAX ((TZrInt64)INT64_MAX)


#endif // ZR_TYPE_CONF_H
