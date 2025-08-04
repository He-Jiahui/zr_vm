//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_CONVERSION_H
#define ZR_VM_CORE_CONVERSION_H

#include "zr_vm_core/conf.h"

#define ZR_CAST(TARGET_TYPE, EXPRESSION) ((TARGET_TYPE) (EXPRESSION))


#define ZR_CAST_CHECKED_NATIVE(STATE, TARGET_TYPE, EXPRESSION, TYPE_ENUM, IS_NATIVE)                                   \
    (ZR_CHECK(STATE, ((EXPRESSION)->type == (TYPE_ENUM) && (EXPRESSION)->isNative == (IS_NATIVE)), "type mismatch."),  \
     ZR_CAST(TARGET_TYPE, (EXPRESSION)))

#define ZR_CAST_CHECKED(STATE, TARGET_TYPE, EXPRESSION, TYPE_ENUM)                                                     \
    (ZR_CHECK(STATE, ((EXPRESSION)->type == (TYPE_ENUM)), "type mismatch."), ZR_CAST(TARGET_TYPE, (EXPRESSION)))

#define ZR_CAST_VOID(EXP) ZR_CAST(void, (EXP))
#define ZR_CAST_BOOL(EXP) ZR_CAST(TBool, (EXP))
#define ZR_CAST_CHAR(EXP) ZR_CAST(TChar, (EXP))
#define ZR_CAST_INT8(EXP) ZR_CAST(TInt8, (EXP))
#define ZR_CAST_UINT8(EXP) ZR_CAST(TUInt8, (EXP))
#define ZR_CAST_UINT8_PTR(EXP) ZR_CAST(TUInt8 *, (EXP))
#define ZR_CAST_INT(EXP) ZR_CAST(TInt32, (EXP))
#define ZR_CAST_UINT(EXP) ZR_CAST(TUInt32, (EXP))
#define ZR_CAST_INT64(EXP) ZR_CAST(TInt64, (EXP))
#define ZR_CAST_UINT64(EXP) ZR_CAST(TUInt64, (EXP))
#define ZR_CAST_UINT64_PTR(EXP) ZR_CAST(TUInt64 *, (EXP))
#define ZR_CAST_FLOAT(EXP) ZR_CAST(TFloat, (EXP))
#define ZR_CAST_NATIVE_STRING(EXP) ZR_CAST(TNativeString, (EXP))
#define ZR_CAST_MEMORY_OFFSET(EXP) ZR_CAST(TZrMemoryOffset, (EXP))
#define ZR_CAST_PTR(EXP) ZR_CAST(TZrPtr, (EXP))
#define ZR_CAST_SIZE(EXP) ZR_CAST(TZrSize, (EXP))


#define ZR_CAST_RAW_OBJECT(EXP) ZR_CAST(SZrRawObject *, (EXP))
#define ZR_CAST_RAW_OBJECT_PTR(EXP) ZR_CAST(SZrRawObject **, (EXP))
#define ZR_CAST_RAW_OBJECT_AS_SUPER(EXP) ZR_CAST(SZrRawObject *, &((EXP)->super))
#define ZR_CAST_HASH_KEY_VALUE_PAIR(EXP) ZR_CAST(SZrHashKeyValuePair *, (EXP))
#define ZR_CAST_HASH_KEY_VALUE_PAIR_PTR(EXP) ZR_CAST(SZrHashKeyValuePair **, (EXP))

#define ZR_CAST_STACK_VALUE(EXP) ZR_CAST(SZrTypeValueOnStack *, (EXP))
#define ZR_CAST_FROM_STACK_VALUE(EXP) ZR_CAST(SZrTypeValue *, (EXP))

#define ZR_CAST_CALL_INFO(EXP) ZR_CAST(SZrCallInfo *, (EXP))

#define ZR_CAST_FUNCTION_POINTER(EXP) ZR_CAST(FZrNativeFunction, (EXP)->value.nativeFunction)

#define ZR_CAST_OBJECT(STATE, EXP) ZR_CAST_CHECKED(STATE, SZrObject *, (EXP), ZR_RAW_OBJECT_TYPE_OBJECT)

#define ZR_CAST_STRING(STATE, EXP) ZR_CAST_CHECKED(STATE, TZrString *, (EXP), ZR_RAW_OBJECT_TYPE_STRING)
#define ZR_CAST_STRING_TO_NATIVE(EXP) ZR_CAST(TNativeString, (EXP)->stringDataExtend)


#define ZR_CAST_NATIVE_CLOSURE(STATE, EXP)                                                                             \
    ZR_CAST_CHECKED_NATIVE(STATE, SZrClosureNative *, (EXP), ZR_RAW_OBJECT_TYPE_CLOSURE, ZR_TRUE)
#define ZR_CAST_VM_CLOSURE(STATE, EXP)                                                                                 \
    ZR_CAST_CHECKED_NATIVE(STATE, SZrClosure *, (EXP), ZR_RAW_OBJECT_TYPE_CLOSURE, ZR_FALSE)
#define ZR_CAST_VM_CLOSURE_VALUE(STATE, EXP)                                                                           \
    ZR_CAST_CHECKED_NATIVE(STATE, SZrClosureValue *, (EXP), ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE, ZR_FALSE)
#define ZR_CAST_CLOSURE(STATE, EXP) ZR_CAST_CHECKED(STATE, TZrClosure *, (EXP), ZR_RAW_OBJECT_TYPE_CLOSURE)
#define ZR_CAST_FUNCTION(STATE, EXP) ZR_CAST_CHECKED(STATE, SZrFunction *, (EXP), ZR_RAW_OBJECT_TYPE_FUNCTION)
#define ZR_CAST_NATIVE_FUNCTION(STATE, EXP)                                                                            \
    ZR_CAST_CHECKED_NATIVE(STATE, FZrNativeFunction, (EXP), ZR_RAW_OBJECT_TYPE_FUNCTION, ZR_TRUE)
#endif // ZR_VM_CORE_CONVERSION_H
