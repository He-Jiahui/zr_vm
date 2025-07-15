//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_TYPE_CONF_H
#define ZR_TYPE_CONF_H


#include "zr_vm_common/zr_common_conf.h"

//
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
#define ZR_VALUE_IS_TYPE_CLOSURE_VALUE(valueType) ((valueType) == ZR_VALUE_TYPE_CLOSURE_VALUE)
#define ZR_VALUE_IS_TYPE_OBJECT(valueType) ((valueType) == ZR_VALUE_TYPE_OBJECT)
#define ZR_VALUE_IS_TYPE_THREAD(valueType) ((valueType) == ZR_VALUE_TYPE_THREAD)

#define ZR_VALUE_IS_TYPE_ZR_ITEM(valueType) ((valueType) >= ZR_VALUE_TYPE_BUFFER && (valueType) <= ZR_VALUE_TYPE_THREAD)

// normal types all can be used in zr_vm (also can convert to string), the others can't be used in zr_vm
#define ZR_VALUE_IS_TYPE_NORMAL(valueType)                                                                             \
    ((valueType) >= ZR_VALUE_TYPE_NULL && (valueType) <= ZR_VALUE_TYPE_NATIVE_DATA)

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


#endif // ZR_TYPE_CONF_H
