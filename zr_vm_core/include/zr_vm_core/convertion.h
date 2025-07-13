//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_CONVERTION_H
#define ZR_VM_CORE_CONVERTION_H

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
#define ZR_CAST_INT(EXP) ZR_CAST(TInt, (EXP))
#define ZR_CAST_UINT(EXP) ZR_CAST(TUInt, (EXP))
#define ZR_CAST_INT64(EXP) ZR_CAST(TInt64, (EXP))
#define ZR_CAST_UINT64(EXP) ZR_CAST(TUInt64, (EXP))
#define ZR_CAST_UINT64_PTR(EXP) ZR_CAST(TUInt64 *, (EXP))
#define ZR_CAST_FLOAT(EXP) ZR_CAST(TFloat, (EXP))
#define ZR_CAST_NATIVE_STRING(EXP) ZR_CAST(TNativeString, (EXP))
#define ZR_CAST_MEMORY_OFFSET(EXP) ZR_CAST(TZrMemoryOffset, (EXP))
#define ZR_CAST_PTR(EXP) ZR_CAST(TZrPtr, (EXP))
#define ZR_CAST_SIZE(EXP) ZR_CAST(TZrSize, (EXP))

#define ZR_CAST_STRING(EXP) ZR_CAST(TZrString *, (EXP))
#define ZR_CAST_STRING_TO_NATIVE(EXP) ZR_CAST(TNativeString, (EXP)->stringDataExtend)

#define ZR_CAST_RAW_OBJECT(EXP) ZR_CAST(SZrRawObject *, (EXP))
#define ZR_CAST_RAW_OBJECT_PTR(EXP) ZR_CAST(SZrRawObject **, (EXP))
#define ZR_CAST_RAW_OBJECT_AS_SUPER(EXP) ZR_CAST(SZrRawObject *, &((EXP)->super))
#define ZR_CAST_HASH_RAW_OBJECT(EXP) ZR_CAST(SZrHashRawObject *, (EXP))
#define ZR_CAST_HASH_RAW_OBJECT_PTR(EXP) ZR_CAST(SZrHashRawObject **, (EXP))
#define ZR_CAST_HASH_RAW_OBJECT_AS_SUPER(EXP) ZR_CAST(SZrHashRawObject *, &((EXP)->super))

#define ZR_CAST_STACK_OBJECT(EXP) ZR_CAST(SZrTypeValueOnStack *, (EXP))

#define ZR_CAST_OBJECT(EXP) ZR_CAST(SZrObject *, (EXP))

#define ZR_CAST_NATIVE_FUNCTION_POINTER(STATE, EXP)                                                                    \
    ZR_CAST_CHECKED_NATIVE(STATE, FZrNativeFunction *, (EXP), ZR_VALUE_TYPE_FUNCTION, ZR_TRUE)
#define ZR_CAST_NATIVE_CLOSURE(STATE, EXP)                                                                             \
    ZR_CAST_CHECKED_NATIVE(STATE, SZrClosureNative *, (EXP), ZR_VALUE_TYPE_FUNCTION, ZR_TRUE)
#define ZR_CAST_VM_CLOSURE(STATE, EXP)                                                                                 \
    ZR_CAST_CHECKED_NATIVE(STATE, SZrClosure *, (EXP), ZR_VALUE_TYPE_FUNCTION, ZR_FALSE)
#define ZR_CAST_CLOSURE(STATE, EXP) ZR_CAST_CHECKED(STATE, TZrClosure *, (EXP), ZR_VALUE_TYPE_FUNCTION)
#endif // ZR_VM_CORE_CONVERTION_H
