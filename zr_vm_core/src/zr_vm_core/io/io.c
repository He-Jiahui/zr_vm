//
// Created by HeJiahui on 2025/7/6.
//

#include "zr_vm_core/io.h"
#include "zr_vm_core/log.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_common/zr_version_info.h"

/* Bound both encoded input and native expansion before allocating. Each array
 * element consumes at least one encoded byte; multiplication happens only after
 * checking the remaining allocation budget. Zeroed arrays are safe to free even
 * when a later field is truncated. */
#define ZR_IO_MAX_BINARY_BYTES ((TZrSize)256u * 1024u * 1024u)
#define ZR_IO_MAX_DECODE_BYTES ((TZrSize)512u * 1024u * 1024u)
#define ZR_IO_MAX_DECODE_DEPTH 128u
#define ZR_IO_MAX_SERIALIZED_SIZE ((TZrSize)1000000u)

#include "io_bytecode_verify.h"

static TZrPtr io_read_array(SZrIo *io, TZrSize count, TZrSize elementSize) {
    TZrPtr result;
    TZrSize bytes;
    if (count == 0 || io->hasReadError) return ZR_NULL;
    if (!io->boundedBinaryInput || elementSize == 0 ||
        count > ZR_IO_MAX_SERIALIZED_SIZE || count > io->remained ||
        count > io->allocationBudgetRemaining / elementSize) {
        io->hasReadError = ZR_TRUE;
        return ZR_NULL;
    }
    bytes = count * elementSize;
    result = ZrCore_Memory_RawMallocWithType(io->state->global, bytes, ZR_MEMORY_NATIVE_TYPE_IO);
    if (result == ZR_NULL) {
        io->hasReadError = ZR_TRUE;
        return ZR_NULL;
    }
    io->allocationBudgetRemaining -= bytes;
    ZrCore_Memory_RawSet(result, 0, bytes);
    return result;
}

#define ZR_IO_FREE_NATIVE_DATA(GLOBAL, DATA, SIZE) \
    ZrCore_Memory_RawFreeWithType(GLOBAL, (DATA), (SIZE), ZR_MEMORY_NATIVE_TYPE_IO)

/* 宿主读取回调可阻塞或进入外部代码；线程锁宏在启用锁的配置下临时让出锁，返回缓冲区仅借用。 */
/* TODO: 若 io->read 在启用线程锁时 Throw，后续重新加锁被跳过；需核查宿主回调的异常约束和外层恢复路径。 */
static TZrBool io_refill(SZrIo *io) {
    SZrState *state = io->state;
    TZrSize readSize = 0;
    ZR_THREAD_UNLOCK(state);
    TZrBytePtr buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }
    io->remained = readSize;
    io->pointer = buffer;
    return ZR_TRUE;
}

static void io_validate_serialized_size(SZrIo *io, TZrSize value);

static TZrInt32 io_read_char(SZrIo *io) {
    TZrByte value = 0;
    return ZrCore_Io_Read(io, &value, 1u) == 1u ? value : ZR_IO_EOF;
}

/* 二进制协议当前直接采用宿主原生宽度；上层必须检查短读及文件头兼容性。 */
static ZR_FORCE_INLINE TZrSize io_read_size(SZrIo *io) {
    TZrSize size = 0;
    ZrCore_Io_Read(io, (TZrBytePtr) &size, sizeof(size));
    io_validate_serialized_size(io, size);
    return size;
}

static ZR_FORCE_INLINE TZrFloat64 io_read_float(SZrIo *io) {
    TZrFloat64 value = 0;
    ZrCore_Io_Read(io, (TZrBytePtr) &value, sizeof(value));
    return value;
}

static ZR_FORCE_INLINE TZrInt64 io_read_int(SZrIo *io) {
    TZrInt64 value = 0;
    ZrCore_Io_Read(io, (TZrBytePtr) &value, sizeof(value));
    return value;
}

static ZR_FORCE_INLINE TZrUInt64 io_read_u_int(SZrIo *io) {
    TZrUInt64 value = 0;
    ZrCore_Io_Read(io, (TZrBytePtr) &value, sizeof(value));
    return value;
}

/* 两类标量读取宏均依赖 SZrIo 的粘滞错误位，由最外层统一判定截断。 */
#define ZR_IO_READ_RAW(IO, VALUE, SIZE) ZrCore_Io_Read(IO, &(VALUE), SIZE);

#define ZR_IO_TYPE_IS_SIZE_EZrIoMemberDeclareType 0
#define ZR_IO_TYPE_IS_SIZE_EZrValueType 0
#define ZR_IO_TYPE_IS_SIZE_TZrBool 0
#define ZR_IO_TYPE_IS_SIZE_TZrInt32 0
#define ZR_IO_TYPE_IS_SIZE_TZrMetadataToken 0
#define ZR_IO_TYPE_IS_SIZE_TZrSize 1
#define ZR_IO_TYPE_IS_SIZE_TZrUInt16 0
#define ZR_IO_TYPE_IS_SIZE_TZrUInt32 0
#define ZR_IO_TYPE_IS_SIZE_TZrUInt64 0
#define ZR_IO_TYPE_IS_SIZE_TZrUInt8 0
static void io_validate_serialized_size(SZrIo *io, TZrSize value) {
    if (io != ZR_NULL && value > ZR_IO_MAX_SERIALIZED_SIZE) {
        io->hasReadError = ZR_TRUE;
    }
}

static TZrBool io_validate_array_count(SZrIo *io, TZrSize count, TZrSize elementSize) {
    if (io == ZR_NULL || elementSize == 0u || count > (TZrSize)-1 / elementSize ||
        count > ZR_IO_MAX_SERIALIZED_SIZE) {
        if (io != ZR_NULL) {
            io->hasReadError = ZR_TRUE;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
#define ZR_IO_READ_NATIVE_TYPE(IO, DATA, TYPE)                                                                        \
    do {                                                                                                             \
        ZrCore_Io_Read((IO), (TZrBytePtr) &(DATA), sizeof(TYPE));                                                    \
        if (ZR_IO_TYPE_IS_SIZE_##TYPE) {                                                                             \
            io_validate_serialized_size((IO), (TZrSize)(DATA));                                                      \
        }                                                                                                            \
        if ((IO)->hasReadError) return;                                                                             \
    } while (0)


/* The binary cursor lends bounded string bytes directly to GC string creation. */
static SZrString *io_read_string_with_length(SZrIo *io) {
    TZrSize length = io_read_size(io);
    SZrString *string;
    if (io->hasReadError || length == 0) return ZR_NULL;
    if (!io->boundedBinaryInput || length > io->remained ||
        length > io->allocationBudgetRemaining) {
        io->hasReadError = ZR_TRUE;
        return ZR_NULL;
    }
    io->allocationBudgetRemaining -= length;
    string = ZrCore_String_Create(io->state, (TZrNativeString)io->pointer, length);
    if (string == ZR_NULL) {
        io->hasReadError = ZR_TRUE;
        return ZR_NULL;
    }
    io->pointer += length;
    io->remained -= length;
    return string;
}

/* 依赖项名字和摘要均转成 GC 字符串，数组所有权保留在模块源树。 */
static void io_read_imports(SZrIo *io, SZrIoImport *imports, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoImport *import = &imports[i];
        import->name = io_read_string_with_length(io);
        import->md5 = io_read_string_with_length(io);
    }
}

static void io_read_typed_value(SZrIo *io, SZrTypeValue *value);

/* 反序列化的纯值进入运行时值容器时恢复 GC 可追踪的对象引用。 */
static void io_init_value_from_pure(struct SZrState *state,
                                    EZrValueType type,
                                    const TZrPureValue *pure,
                                    SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(value);
    if (pure == ZR_NULL) {
        return;
    }

    switch (type) {
        case ZR_VALUE_TYPE_NULL:
            return;

        case ZR_VALUE_TYPE_BOOL:
            ZrCore_Value_InitAsBool(state, value, pure->nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
            return;

        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            ZrCore_Value_InitAsInt(state, value, pure->nativeObject.nativeInt64);
            value->type = type;
            return;

        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            ZrCore_Value_InitAsUInt(state, value, pure->nativeObject.nativeUInt64);
            value->type = type;
            return;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            ZrCore_Value_InitAsFloat(state, value, pure->nativeObject.nativeDouble);
            value->type = type;
            return;

        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
            ZrCore_Value_InitAsRawObject(state, value, pure->object);
            value->type = type;
            return;

        case ZR_VALUE_TYPE_NATIVE_POINTER:
            ZrCore_Value_InitAsNativePointer(state, value, pure->nativeObject.nativePointer);
            return;

        default:
            value->type = type;
            value->value = *pure;
            value->isGarbageCollectable =
                    (TZrBool)(type == ZR_VALUE_TYPE_STRING || type == ZR_VALUE_TYPE_OBJECT || type == ZR_VALUE_TYPE_ARRAY);
            value->isNative = (TZrBool)!value->isGarbageCollectable;
            return;
    }
}

/* 值的解码由序列化类型判别，调用方须已从对应字段读得类型。 */
static void io_read_value(SZrIo *io, EZrValueType type, TZrPureValue *value) {
    SZrState *state;

    if (io == ZR_NULL || value == ZR_NULL) {
        return;
    }

    if (io->hasReadError || ++io->decodeDepth > ZR_IO_MAX_DECODE_DEPTH) {
        io->hasReadError = ZR_TRUE;
        return;
    }
    state = io->state;
    ZrCore_Memory_RawSet(value, 0, sizeof(*value));
    switch (type) {
        case ZR_VALUE_TYPE_NULL: {
            value->nativeObject.nativeBool = ZR_FALSE;
        } break;
        case ZR_VALUE_TYPE_BOOL: {
            TZrInt32 boolean = io_read_char(io);
            if (boolean < 0 || boolean > 1) io->hasReadError = ZR_TRUE;
            else value->nativeObject.nativeBool = (TZrBool)boolean;
        } break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE: {
            value->nativeObject.nativeDouble = io_read_float(io);
        } break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64: {
            value->nativeObject.nativeInt64 = io_read_int(io);
        } break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64: {
            value->nativeObject.nativeUInt64 = io_read_u_int(io);
        } break;
        case ZR_VALUE_TYPE_STRING: {
            SZrString *stringValue = io_read_string_with_length(io);
            if (stringValue == ZR_NULL) {
                stringValue = ZrCore_String_Create(state, "", 0);
            }
            value->object = ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue);
        } break;
        case ZR_VALUE_TYPE_OBJECT: {
            TZrSize entryCount = 0;
            SZrObject *object;

            ZR_IO_READ_NATIVE_TYPE(io, entryCount, TZrSize);
            if (entryCount > io->remained / sizeof(TZrUInt32) ||
                entryCount > io->allocationBudgetRemaining / (2u * sizeof(SZrTypeValue))) {
                io->hasReadError = ZR_TRUE;
                return;
            }
            io->allocationBudgetRemaining -= entryCount * (2u * sizeof(SZrTypeValue));
            object = ZrCore_Object_New(state, ZR_NULL);
            if (object == ZR_NULL) {
                value->object = ZR_NULL;
                break;
            }
            ZrCore_Object_Init(state, object);

            for (TZrSize index = 0; !io->hasReadError && (index < entryCount); index++) {
                SZrTypeValue keyValue;
                SZrTypeValue entryValue;

                io_read_typed_value(io, &keyValue);
                io_read_typed_value(io, &entryValue);
                ZrCore_Object_SetValue(state, object, &keyValue, &entryValue);
            }

            value->object = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        } break;
        case ZR_VALUE_TYPE_ARRAY: {
            TZrSize elementCount = 0;
            SZrObject *arrayObject;

            ZR_IO_READ_NATIVE_TYPE(io, elementCount, TZrSize);
            if (elementCount > io->remained / sizeof(TZrUInt32) ||
                elementCount > io->allocationBudgetRemaining / (2u * sizeof(SZrTypeValue))) {
                io->hasReadError = ZR_TRUE;
                return;
            }
            io->allocationBudgetRemaining -= elementCount * (2u * sizeof(SZrTypeValue));
            arrayObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
            if (arrayObject == ZR_NULL) {
                value->object = ZR_NULL;
                break;
            }
            ZrCore_Object_Init(state, arrayObject);

            for (TZrSize index = 0; !io->hasReadError && (index < elementCount); index++) {
                SZrTypeValue indexValue;
                SZrTypeValue elementValue;

                io_read_typed_value(io, &elementValue);
                ZrCore_Value_InitAsInt(state, &indexValue, (TZrInt64)index);
                ZrCore_Object_SetValue(state, arrayObject, &indexValue, &elementValue);
            }

            value->object = ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject);
        } break;
        default: {
            io->hasReadError = ZR_TRUE;
        } break;
    }
    --io->decodeDepth;
}

static void io_read_typed_value(SZrIo *io, SZrTypeValue *value) {
    TZrUInt32 rawType = ZR_VALUE_TYPE_NULL;
    TZrPureValue pureValue;

    if (value == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(value);
    if (io == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&pureValue, 0, sizeof(pureValue));
    ZR_IO_READ_NATIVE_TYPE(io, rawType, TZrUInt32);
    if (rawType > (TZrUInt32)ZR_VALUE_TYPE_UNKNOWN) {
        io->hasReadError = ZR_TRUE;
    }
    if (!io->hasReadError) {
        io_read_value(io, (EZrValueType)rawType, &pureValue);
    }
    io_init_value_from_pure(io->state, (EZrValueType)rawType, &pureValue, value);
}

static void io_read_field(SZrIo *io, SZrIoField *field) { field->name = io_read_string_with_length(io); }

static void io_read_references(SZrIo *io, SZrIoReference *references, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoReference *reference = &references[i];
        reference->referenceModuleName = io_read_string_with_length(io);
        reference->referenceModuleMd5 = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, reference->referenceIndex, TZrSize);
    }
}

static void io_read_function_local_variables(SZrIo *io, SZrIoFunctionLocalVariable *variables, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionLocalVariable *variable = &variables[i];
        variable->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, variable->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, variable->instructionStartIndex, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->instructionEndIndex, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->startLine, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->endLine, TZrUInt64);
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA) {
            ZR_IO_READ_NATIVE_TYPE(io, variable->scopeDepth, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, variable->escapeFlags, TZrUInt32);
        } else {
            variable->scopeDepth = 0;
            variable->escapeFlags = 0;
        }
    }
}

static void io_read_function_closure_variables(SZrIo *io, SZrIoFunctionClosureVariable *variables, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionClosureVariable *variable = &variables[i];
        variable->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, variable->inStack, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->index, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, variable->valueType, TZrUInt32);
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA) {
            ZR_IO_READ_NATIVE_TYPE(io, variable->scopeDepth, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, variable->escapeFlags, TZrUInt32);
        } else {
            variable->scopeDepth = 0;
            variable->escapeFlags = 0;
        }
    }
}

static void io_read_function_catch_clauses(SZrIo *io, SZrIoFunctionCatchClause *clauses, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        clauses[i].typeName = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, clauses[i].targetInstructionOffset, TZrUInt64);
    }
}

static void io_read_function_exception_handlers(SZrIo *io,
                                                SZrIoFunctionExceptionHandler *handlers,
                                                TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].protectedStartInstructionOffset, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].finallyTargetInstructionOffset, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].afterFinallyInstructionOffset, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].catchClauseStartIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].catchClauseCount, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].hasFinally, TZrUInt8);
    }
}

static void io_read_functions(SZrIo *io, SZrIoFunction *functions, TZrSize count);
static void io_read_function_constant_variables(SZrIo *io, SZrIoFunctionConstantVariable *variables, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionConstantVariable *variable = &variables[i];
        ZR_IO_READ_NATIVE_TYPE(io, variable->type, TZrUInt32);
        variable->hasFunctionValue = ZR_FALSE;
        variable->functionValue = ZR_NULL;
        if (variable->type == ZR_VALUE_TYPE_FUNCTION || variable->type == ZR_VALUE_TYPE_CLOSURE) {
            ZR_IO_READ_NATIVE_TYPE(io, variable->hasFunctionValue, TZrBool);
            if (variable->hasFunctionValue > ZR_TRUE) { io->hasReadError = ZR_TRUE; return; }
            if (variable->hasFunctionValue) {
                variable->functionValue = io_read_array(io, 1u, sizeof(SZrIoFunction));
                if (io->hasReadError) return;
                                io_read_functions(io, variable->functionValue, 1);
            }
        } else if (variable->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
            io_read_value(io, variable->type, &variable->value);
        }
        ZR_IO_READ_NATIVE_TYPE(io, variable->startLine, TZrUInt64);
        if (variable->type == ZR_VALUE_TYPE_NATIVE_POINTER &&
            ZrCore_Io_GetSerializableNativeHelperFunction(variable->startLine) == ZR_NULL) {
            io->hasReadError = ZR_TRUE;
            return;
        }
        ZR_IO_READ_NATIVE_TYPE(io, variable->endLine, TZrUInt64);
    }
}

static void io_read_function_exported_variables(SZrIo *io, SZrIoFunctionExportedVariable *variables, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionExportedVariable *variable = &variables[i];
        ZrCore_Memory_RawSet(variable, 0, sizeof(*variable));
        variable->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
        variable->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, variable->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, variable->accessModifier, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->exportKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->readiness, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->reserved0, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->callableChildIndex, TZrUInt32);
    }
}

static void io_read_function_typed_type_ref(SZrIo *io, SZrIoFunctionTypedTypeRef *typeRef) {
    TZrUInt32 baseType = ZR_VALUE_TYPE_OBJECT;
    TZrUInt8 isNullable = ZR_FALSE;
    TZrUInt32 ownershipQualifier = 0;
    TZrUInt8 isArray = ZR_FALSE;
    TZrUInt32 elementBaseType = ZR_VALUE_TYPE_OBJECT;
    TZrUInt32 staticCType = ZR_STATIC_C_TYPE_DYNAMIC;
    TZrUInt32 staticCTypeId = (TZrUInt32)0xFFFFFFFFu;

    if (io == ZR_NULL || typeRef == ZR_NULL) {
        return;
    }

    ZR_IO_READ_NATIVE_TYPE(io, baseType, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, isNullable, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, ownershipQualifier, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, isArray, TZrUInt8);
    typeRef->typeName = io_read_string_with_length(io);
    ZR_IO_READ_NATIVE_TYPE(io, elementBaseType, TZrUInt32);
    typeRef->elementTypeName = io_read_string_with_length(io);
    if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_SEMIR_STATIC_C_TYPES) {
        ZR_IO_READ_NATIVE_TYPE(io, staticCType, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, staticCTypeId, TZrUInt32);
    }

    typeRef->baseType = (EZrValueType)baseType;
    typeRef->isNullable = isNullable ? ZR_TRUE : ZR_FALSE;
    typeRef->ownershipQualifier = ownershipQualifier;
    typeRef->isArray = isArray ? ZR_TRUE : ZR_FALSE;
    typeRef->elementBaseType = (EZrValueType)elementBaseType;
    typeRef->staticCType = (EZrStaticCType)staticCType;
    typeRef->staticCTypeId =
            typeRef->staticCType == ZR_STATIC_C_TYPE_STRUCT ? staticCTypeId : (TZrUInt32)0xFFFFFFFFu;
}

static void io_read_function_typed_local_bindings(SZrIo *io,
                                                  SZrIoFunctionTypedLocalBinding *bindings,
                                                  TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionTypedLocalBinding *binding = &bindings[i];
        ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
        binding->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, binding->stackSlot, TZrUInt32);
        io_read_function_typed_type_ref(io, &binding->type);
        if (io->sourceVersionPatch >=
            ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS) {
            ZR_IO_READ_NATIVE_TYPE(io, binding->symbolId, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, binding->typeId, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, binding->placeId, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, binding->declarationStartLine, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, binding->declarationStartColumn, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, binding->declarationEndLine, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, binding->declarationEndColumn, TZrUInt32);
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_ROLE_FLAGS) {
            ZR_IO_READ_NATIVE_TYPE(io, binding->roleFlags, TZrUInt32);
        }
    }
}

static void io_read_function_typed_closure_bindings(SZrIo *io,
                                                     SZrIoFunctionTypedClosureBinding *bindings,
                                                     TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionTypedClosureBinding *binding = &bindings[i];

        ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
        ZR_IO_READ_NATIVE_TYPE(io, binding->captureIndex, TZrUInt32);
        io_read_function_typed_type_ref(io, &binding->type);
        ZR_IO_READ_NATIVE_TYPE(io, binding->symbolId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->typeId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->declarationStartLine, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->declarationStartColumn, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->declarationEndLine, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->declarationEndColumn, TZrUInt32);
    }
}

static void io_read_function_typed_export_generic_parameters(
        SZrIo *io,
        SZrIoFunctionTypedExportSymbol *symbol) {
    TZrUInt32 serializedCount = 0;

    if (io == ZR_NULL || io->state == ZR_NULL || io->state->global == ZR_NULL || symbol == ZR_NULL) {
        return;
    }

    ZR_IO_READ_NATIVE_TYPE(io, serializedCount, TZrUInt32);
    if (serializedCount > ZR_IO_MAX_SERIALIZED_SIZE) {
        io->hasReadError = ZR_TRUE;
    }
    if (serializedCount == 0) {
        return;
    }

    symbol->genericParameters = io_read_array(io, serializedCount, sizeof(SZrIoFunctionTypedGenericParameter));
    if (io->hasReadError) return;
    if (symbol->genericParameters != ZR_NULL) {
        symbol->genericParameterCount = serializedCount;
    }

    for (TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++) {
        SZrIoFunctionTypedGenericParameter discardedParameter;
        SZrIoFunctionTypedGenericParameter *parameter =
                symbol->genericParameters != ZR_NULL ? &symbol->genericParameters[index]
                                                     : &discardedParameter;
        TZrUInt32 serializedConstraintCount = 0;

        ZrCore_Memory_RawSet(parameter, 0, sizeof(*parameter));
        parameter->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, parameter->genericKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, parameter->variance, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, parameter->requiresClass, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, parameter->requiresStruct, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, parameter->requiresNew, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, parameter->requiresOwner, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, parameter->reserved0, TZrUInt16);
        ZR_IO_READ_NATIVE_TYPE(io, parameter->requiredOwnershipQualifier, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, serializedConstraintCount, TZrUInt32);
        if (serializedConstraintCount > ZR_IO_MAX_SERIALIZED_SIZE) {
            io->hasReadError = ZR_TRUE;
        }

        if (symbol->genericParameters != ZR_NULL && serializedConstraintCount > 0) {
            parameter->constraintTypeNames = io_read_array(io, serializedConstraintCount, sizeof(SZrString *));
            if (io->hasReadError) return;
            if (parameter->constraintTypeNames != ZR_NULL) {
                parameter->constraintTypeCount = serializedConstraintCount;
            }
        }
        for (TZrUInt32 constraintIndex = 0; !io->hasReadError && (constraintIndex < serializedConstraintCount);
             constraintIndex++) {
            SZrString *constraintName = io_read_string_with_length(io);
            if (parameter->constraintTypeNames != ZR_NULL) {
                parameter->constraintTypeNames[constraintIndex] = constraintName;
            }
        }
    }
}

/* 类型化导出携带签名 token、约束与参数侧表，保持与写入器的字段顺序一致。 */
static void io_read_function_typed_export_symbols(SZrIo *io,
                                                  SZrIoFunctionTypedExportSymbol *symbols,
                                                  TZrSize count) {

    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionTypedExportSymbol *symbol = &symbols[i];
        ZrCore_Memory_RawSet(symbol, 0, sizeof(*symbol));
        symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;

        symbol->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->accessModifier, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->symbolKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->exportKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->readiness, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->reserved0, TZrUInt16);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->callableChildIndex, TZrUInt32);
        io_read_function_typed_type_ref(io, &symbol->valueType);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->parameterCount, TZrSize);
        if (symbol->parameterCount > 0) {
            symbol->parameterTypes = io_read_array(io, symbol->parameterCount, sizeof(SZrIoFunctionTypedTypeRef));
            if (io->hasReadError) return;
            if (symbol->parameterTypes != ZR_NULL) {
                for (TZrSize paramIndex = 0; !io->hasReadError && (paramIndex < symbol->parameterCount); paramIndex++) {
                    io_read_function_typed_type_ref(io, &symbol->parameterTypes[paramIndex]);
                }
            }
        } else {
            symbol->parameterTypes = ZR_NULL;
        }

        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_GENERIC_PARAMETERS) {
            io_read_function_typed_export_generic_parameters(io, symbol);
        }

        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_DECLARATION_SPANS) {
            ZR_IO_READ_NATIVE_TYPE(io, symbol->lineInSourceStart, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->columnInSourceStart, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->lineInSourceEnd, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->columnInSourceEnd, TZrUInt32);
        } else {
            symbol->lineInSourceStart = 0;
            symbol->columnInSourceStart = 0;
            symbol->lineInSourceEnd = 0;
            symbol->columnInSourceEnd = 0;
        }

        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL) {
            ZR_IO_READ_NATIVE_TYPE(io, symbol->metadataToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->signatureToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->signatureBlobOffset, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->signatureBlobLength, TZrUInt32);
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_SIGNATURE_HASH) {
                ZR_IO_READ_NATIVE_TYPE(io, symbol->signatureHash, TZrUInt64);
            } else {
                symbol->signatureHash = 0;
            }
        } else {
            symbol->metadataToken = 0;
            symbol->signatureToken = 0;
            symbol->signatureBlobOffset = 0;
            symbol->signatureBlobLength = 0;
            symbol->signatureHash = 0;
        }
    }
}

/* token 记录表依赖版本化行协议；上层在整体模型完成后才将索引交给运行时。 */
static void io_read_metadata_token_record_list(SZrIo *io,
                                               TZrSize *outRecordLength,
                                               SZrMetadataTokenRecord **outRecords) {

    if (outRecordLength != ZR_NULL) {
        *outRecordLength = 0;
    }
    if (outRecords != ZR_NULL) {
        *outRecords = ZR_NULL;
    }
    if (io == ZR_NULL || outRecordLength == ZR_NULL || outRecords == ZR_NULL) {
        return;
    }

    ZR_IO_READ_NATIVE_TYPE(io, *outRecordLength, TZrSize);
    if (*outRecordLength > 0) {
        *outRecords = io_read_array(io, *outRecordLength, sizeof(SZrMetadataTokenRecord));
        if (io->hasReadError) return;
        for (TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++) {
            SZrMetadataTokenRecord discardRecord;
            SZrMetadataTokenRecord *record = *outRecords != ZR_NULL ? &(*outRecords)[index] : &discardRecord;
            ZR_IO_READ_NATIVE_TYPE(io, record->token, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, record->relatedToken, TZrMetadataToken);
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_OWNER_CHAIN) {
                ZR_IO_READ_NATIVE_TYPE(io, record->ownerToken, TZrMetadataToken);
            } else {
                record->ownerToken = 0;
            }
            ZR_IO_READ_NATIVE_TYPE(io, record->ownerIndex, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, record->signatureBlobOffset, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, record->signatureBlobLength, TZrUInt32);
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_SIGNATURE_HASH) {
                ZR_IO_READ_NATIVE_TYPE(io, record->signatureHash, TZrUInt64);
            } else {
                record->signatureHash = 0;
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_LAYOUT_IDENTITY) {
                ZR_IO_READ_NATIVE_TYPE(io, record->layoutVersion, TZrUInt32);
                ZR_IO_READ_NATIVE_TYPE(io, record->reserved0, TZrUInt32);
                ZR_IO_READ_NATIVE_TYPE(io, record->layoutHash, TZrUInt64);
            } else {
                record->layoutVersion = 0;
                record->reserved0 = 0;
                record->layoutHash = 0;
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_TOKENS) {
                ZR_IO_READ_NATIVE_TYPE(io, record->targetMetadataToken, TZrMetadataToken);
                ZR_IO_READ_NATIVE_TYPE(io, record->targetSignatureToken, TZrMetadataToken);
            } else {
                record->targetMetadataToken = 0;
                record->targetSignatureToken = 0;
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH) {
                ZR_IO_READ_NATIVE_TYPE(io, record->targetSignatureHash, TZrUInt64);
            } else {
                record->targetSignatureHash = 0;
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_TARGET_MODULE_HASH) {
                ZR_IO_READ_NATIVE_TYPE(io, record->targetModuleSignatureHash, TZrUInt64);
            } else {
                record->targetModuleSignatureHash = 0;
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE) {
                record->requestedModuleVersion = io_read_string_with_length(io);
                record->minModuleVersionInclusive = io_read_string_with_length(io);
                record->maxModuleVersionExclusive = io_read_string_with_length(io);
            } else {
                record->requestedModuleVersion = ZR_NULL;
                record->minModuleVersionInclusive = ZR_NULL;
                record->maxModuleVersionExclusive = ZR_NULL;
            }
        }
    }
}

/* 模块绑定行将 token 与声明关系保留到运行时链接阶段。 */
static void io_read_module_metadata_binding_list(SZrIo *io,
                                                 TZrSize *outBindingLength,
                                                 SZrMetadataTokenBinding **outBindings) {

    if (outBindingLength != ZR_NULL) {
        *outBindingLength = 0;
    }
    if (outBindings != ZR_NULL) {
        *outBindings = ZR_NULL;
    }
    if (io == ZR_NULL || outBindingLength == ZR_NULL || outBindings == ZR_NULL) {
        return;
    }

    ZR_IO_READ_NATIVE_TYPE(io, *outBindingLength, TZrSize);
    if (*outBindingLength > 0) {
        *outBindings = io_read_array(io, *outBindingLength, sizeof(SZrMetadataTokenBinding));
        if (io->hasReadError) return;
        for (TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++) {
            SZrMetadataTokenBinding discardBinding;
            SZrMetadataTokenBinding *binding =
                    *outBindings != ZR_NULL ? &(*outBindings)[index] : &discardBinding;

            ZR_IO_READ_NATIVE_TYPE(io, binding->refToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, binding->refSignatureToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, binding->refSignatureHash, TZrUInt64);
            ZR_IO_READ_NATIVE_TYPE(io, binding->expectedMetadataToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, binding->expectedSignatureToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, binding->expectedSignatureHash, TZrUInt64);
            ZR_IO_READ_NATIVE_TYPE(io, binding->expectedModuleSignatureHash, TZrUInt64);
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY) {
                ZR_IO_READ_NATIVE_TYPE(io, binding->expectedLayoutVersion, TZrUInt32);
                ZR_IO_READ_NATIVE_TYPE(io, binding->reserved0, TZrUInt32);
                ZR_IO_READ_NATIVE_TYPE(io, binding->expectedLayoutHash, TZrUInt64);
            } else {
                binding->expectedLayoutVersion = 0;
                binding->reserved0 = 0;
                binding->expectedLayoutHash = 0;
            }
            ZR_IO_READ_NATIVE_TYPE(io, binding->resolvedMetadataToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, binding->resolvedSignatureToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, binding->resolvedSignatureHash, TZrUInt64);
            ZR_IO_READ_NATIVE_TYPE(io, binding->resolvedModuleSignatureHash, TZrUInt64);
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY) {
                ZR_IO_READ_NATIVE_TYPE(io, binding->resolvedLayoutVersion, TZrUInt32);
                ZR_IO_READ_NATIVE_TYPE(io, binding->reserved1, TZrUInt32);
                ZR_IO_READ_NATIVE_TYPE(io, binding->resolvedLayoutHash, TZrUInt64);
            } else {
                binding->resolvedLayoutVersion = 0;
                binding->reserved1 = 0;
                binding->resolvedLayoutHash = 0;
            }
        }
    }
}

/* 签名中的字符串索引依赖此堆，必须与 token 表的版本及条目边界一致。 */
static void io_read_metadata_string_heap(SZrIo *io,
                                         TZrSize *outStringHeapLength,
                                         SZrMetadataStringHeapEntry **outStringHeap) {

    if (io == ZR_NULL || outStringHeapLength == ZR_NULL || outStringHeap == ZR_NULL) {
        return;
    }

    *outStringHeapLength = 0;
    *outStringHeap = ZR_NULL;
    ZR_IO_READ_NATIVE_TYPE(io, *outStringHeapLength, TZrSize);
    if (*outStringHeapLength == 0) {
        return;
    }

    *outStringHeap = io_read_array(io, *outStringHeapLength, sizeof(SZrMetadataStringHeapEntry));
    if (io->hasReadError) return;
    for (TZrSize index = 0; !io->hasReadError && (index < *outStringHeapLength); index++) {
        SZrMetadataStringHeapEntry discardEntry;
        SZrMetadataStringHeapEntry *entry =
                *outStringHeap != ZR_NULL ? &(*outStringHeap)[index] : &discardEntry;

        ZrCore_Memory_RawSet(entry, 0, sizeof(*entry));
        ZR_IO_READ_NATIVE_TYPE(io, entry->stringIndex, TZrUInt32);
        entry->value = io_read_string_with_length(io);
    }
}

/* token 表、签名堆及模块绑定属于同一版本化模型，后续运行时复制时必须成组解释。 */
static void io_read_function_metadata_token_model(SZrIo *io, SZrIoFunction *function) {

    if (io == ZR_NULL || function == ZR_NULL) {
        return;
    }

    function->metadataTokenRecordLength = 0;
    function->metadataTokenRecords = ZR_NULL;
    function->moduleMetadataTokenRecordLength = 0;
    function->moduleMetadataTokenRecords = ZR_NULL;
    function->signatureBlobHeapLength = 0;
    function->signatureBlobHeap = ZR_NULL;
    function->metadataStringHeapLength = 0;
    function->metadataStringHeap = ZR_NULL;
    function->moduleSignatureHash = 0;
    function->moduleVersion = ZR_NULL;
    function->moduleMetadataBindingLength = 0;
    function->moduleMetadataBindings = ZR_NULL;

    if (io->sourceVersionPatch < ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL) {
        return;
    }

    io_read_metadata_token_record_list(io,
                                       &function->metadataTokenRecordLength,
                                       &function->metadataTokenRecords);
    if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE) {
        function->moduleVersion = io_read_string_with_length(io);
    } else {
        function->moduleVersion = ZR_NULL;
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->signatureBlobHeapLength, TZrSize);
    if (function->signatureBlobHeapLength > 0) {
        function->signatureBlobHeap = io_read_array(io, function->signatureBlobHeapLength, 1u);
        if (io->hasReadError) return;
        if (function->signatureBlobHeap != ZR_NULL) {
            ZrCore_Io_Read(io, function->signatureBlobHeap, function->signatureBlobHeapLength);
        } else {
            TZrByte discardByte = 0;
            for (TZrSize index = 0; !io->hasReadError && (index < function->signatureBlobHeapLength); index++) {
                ZrCore_Io_Read(io, &discardByte, sizeof(discardByte));
            }
        }
    }
    if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_STRING_HEAP) {
        io_read_metadata_string_heap(io,
                                     &function->metadataStringHeapLength,
                                     &function->metadataStringHeap);
    }
    if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_SIGNATURE_HASH) {
        ZR_IO_READ_NATIVE_TYPE(io, function->moduleSignatureHash, TZrUInt64);
    }

    if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_METADATA_REF_TABLE) {
        io_read_metadata_token_record_list(io,
                                           &function->moduleMetadataTokenRecordLength,
                                           &function->moduleMetadataTokenRecords);
    }
    if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_METADATA_BINDINGS) {
        io_read_module_metadata_binding_list(io,
                                             &function->moduleMetadataBindingLength,
                                             &function->moduleMetadataBindings);
    }
}

/* 作用约束与模块版本区间构成同一摘要，供能力检查使用。 */
static void io_read_function_module_effects(SZrIo *io,
                                            SZrIoFunctionModuleEffect *effects,
                                            TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        SZrIoFunctionModuleEffect *effect = &effects[index];

        ZrCore_Memory_RawSet(effect, 0, sizeof(*effect));
        ZR_IO_READ_NATIVE_TYPE(io, effect->kind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, effect->exportKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, effect->readiness, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, effect->reserved0, TZrUInt8);
        effect->moduleName = io_read_string_with_length(io);
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_ASSEMBLY_NAME) {
            effect->assemblyName = io_read_string_with_length(io);
        } else {
            effect->assemblyName = ZR_NULL;
        }
        effect->symbolName = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, effect->lineInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effect->columnInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effect->lineInSourceEnd, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effect->columnInSourceEnd, TZrUInt32);
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH) {
            ZR_IO_READ_NATIVE_TYPE(io, effect->targetMetadataToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, effect->targetSignatureToken, TZrMetadataToken);
            ZR_IO_READ_NATIVE_TYPE(io, effect->targetSignatureHash, TZrUInt64);
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_TARGET_MODULE_HASH) {
                ZR_IO_READ_NATIVE_TYPE(io, effect->targetModuleSignatureHash, TZrUInt64);
            } else {
                effect->targetModuleSignatureHash = 0;
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE) {
                effect->requestedModuleVersion = io_read_string_with_length(io);
                effect->minModuleVersionInclusive = io_read_string_with_length(io);
                effect->maxModuleVersionExclusive = io_read_string_with_length(io);
            } else {
                effect->requestedModuleVersion = ZR_NULL;
                effect->minModuleVersionInclusive = ZR_NULL;
                effect->maxModuleVersionExclusive = ZR_NULL;
            }
        } else {
            effect->targetMetadataToken = 0;
            effect->targetSignatureToken = 0;
            effect->targetSignatureHash = 0;
            effect->targetModuleSignatureHash = 0;
            effect->requestedModuleVersion = ZR_NULL;
            effect->minModuleVersionInclusive = ZR_NULL;
            effect->maxModuleVersionExclusive = ZR_NULL;
        }
    }
}

static void io_read_function_callable_summaries(SZrIo *io,
                                                SZrIoFunctionCallableSummary *summaries,
                                                TZrSize count) {

    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        SZrIoFunctionCallableSummary *summary = &summaries[index];

        ZrCore_Memory_RawSet(summary, 0, sizeof(*summary));
        summary->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, summary->callableChildIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, summary->effectCount, TZrSize);
        if (summary->effectCount > 0) {
            summary->effects = io_read_array(io, summary->effectCount, sizeof(SZrIoFunctionModuleEffect));
            if (io->hasReadError) return;
            if (summary->effects != ZR_NULL) {
                io_read_function_module_effects(io, summary->effects, summary->effectCount);
            }
        }
    }
}

static void io_read_function_top_level_callable_bindings(SZrIo *io,
                                                         SZrIoFunctionTopLevelCallableBinding *bindings,
                                                         TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        SZrIoFunctionTopLevelCallableBinding *binding = &bindings[index];

        ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
        binding->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
        binding->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, binding->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->callableChildIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->accessModifier, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->exportKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->readiness, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->reserved0, TZrUInt8);
    }
}

/* 参数默认值和装饰器值可能嵌套函数源树，必须与释放器的递归所有权一致。 */
static void io_read_function_metadata_parameters(SZrIo *io,
                                                 SZrIoFunctionMetadataParameter **outParameters,
                                                 TZrSize *outCount) {
    TZrSize count;

    if (io == ZR_NULL || outParameters == ZR_NULL || outCount == ZR_NULL) {
        return;
    }

    ZR_IO_READ_NATIVE_TYPE(io, count, TZrSize);
    *outCount = count;
    *outParameters = ZR_NULL;
    if (count == 0) {
        return;
    }

    *outParameters = io_read_array(io, count, sizeof(SZrIoFunctionMetadataParameter));
    if (io->hasReadError) return;
    if (*outParameters == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        (*outParameters)[index].name = io_read_string_with_length(io);
        io_read_function_typed_type_ref(io, &(*outParameters)[index].type);
        (*outParameters)[index].hasDefaultValue = ZR_FALSE;
        ZR_IO_READ_NATIVE_TYPE(io, (*outParameters)[index].hasDefaultValue, TZrUInt8);
        if ((*outParameters)[index].hasDefaultValue) {
            io_read_function_constant_variables(io, &(*outParameters)[index].defaultValue, 1);
        } else {
            ZrCore_Memory_RawSet(&(*outParameters)[index].defaultValue, 0, sizeof((*outParameters)[index].defaultValue));
        }
        (*outParameters)[index].hasDecoratorMetadata = ZR_FALSE;
        ZrCore_Memory_RawSet(&(*outParameters)[index].decoratorMetadataValue,
                             0,
                             sizeof((*outParameters)[index].decoratorMetadataValue));
        (*outParameters)[index].decoratorNamesLength = 0;
        (*outParameters)[index].decoratorNames = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA) {
            ZR_IO_READ_NATIVE_TYPE(io, (*outParameters)[index].hasDecoratorMetadata, TZrUInt8);
            if ((*outParameters)[index].hasDecoratorMetadata) {
                io_read_function_constant_variables(io, &(*outParameters)[index].decoratorMetadataValue, 1);
            }

            ZR_IO_READ_NATIVE_TYPE(io, (*outParameters)[index].decoratorNamesLength, TZrSize);
            if ((*outParameters)[index].decoratorNamesLength > 0) {
                (*outParameters)[index].decoratorNames =
                        io_read_array(io, (*outParameters)[index].decoratorNamesLength, sizeof(SZrString *));
                if (io->hasReadError) return;
                if ((*outParameters)[index].decoratorNames != ZR_NULL) {
                    for (TZrSize decoratorIndex = 0; !io->hasReadError && (decoratorIndex < (*outParameters)[index].decoratorNamesLength);
                         decoratorIndex++) {
                        (*outParameters)[index].decoratorNames[decoratorIndex] = io_read_string_with_length(io);
                    }
                }
            }
        }
    }
}

/* 编译期变量路径绑定是分析元数据，加载后仍需复制以保留跨模块诊断身份。 */
static void io_read_function_compile_time_variable_infos(SZrIo *io,
                                                         SZrIoFunctionCompileTimeVariableInfo *infos,
                                                         TZrSize count) {
    SZrGlobalState *global = io != ZR_NULL && io->state != ZR_NULL ? io->state->global : ZR_NULL;

    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        infos[index].name = io_read_string_with_length(io);
        io_read_function_typed_type_ref(io, &infos[index].type);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceEnd, TZrUInt32);
        infos[index].pathBindingsLength = 0;
        infos[index].pathBindings = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_PATH_BINDINGS) {
            ZR_IO_READ_NATIVE_TYPE(io, infos[index].pathBindingsLength, TZrSize);
            if (infos[index].pathBindingsLength > 0) {
                if (global != ZR_NULL) {
                    infos[index].pathBindings = io_read_array(io, infos[index].pathBindingsLength, sizeof(SZrIoFunctionCompileTimePathBinding));
                    if (io->hasReadError) return;
                }
                for (TZrSize bindingIndex = 0; !io->hasReadError && (bindingIndex < infos[index].pathBindingsLength); bindingIndex++) {
                    SZrString *path = io_read_string_with_length(io);
                    TZrUInt8 targetKind = 0;
                    SZrString *targetName;

                    ZR_IO_READ_NATIVE_TYPE(io, targetKind, TZrUInt8);
                    targetName = io_read_string_with_length(io);
                    if (infos[index].pathBindings != ZR_NULL) {
                        infos[index].pathBindings[bindingIndex].path = path;
                        infos[index].pathBindings[bindingIndex].targetKind = targetKind;
                        infos[index].pathBindings[bindingIndex].targetName = targetName;
                    }
                }
            }
        }
    }
}

static void io_read_function_compile_time_function_infos(SZrIo *io,
                                                         SZrIoFunctionCompileTimeFunctionInfo *infos,
                                                         TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        infos[index].name = io_read_string_with_length(io);
        io_read_function_typed_type_ref(io, &infos[index].returnType);
        io_read_function_metadata_parameters(io, &infos[index].parameters, &infos[index].parameterCount);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceEnd, TZrUInt32);
    }
}

static void io_read_function_escape_bindings(SZrIo *io,
                                             SZrIoFunctionEscapeBinding *bindings,
                                             TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        SZrIoFunctionEscapeBinding *binding = &bindings[index];

        ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
        binding->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, binding->slotOrIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->scopeDepth, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->escapeFlags, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->bindingKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->reserved0, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->reserved1, TZrUInt16);
    }
}

/* 函数装饰器值可能嵌套函数常量，源树释放时需按同一标记递归清理。 */
static void io_read_function_decorator_metadata(SZrIo *io, SZrIoFunction *function) {

    if (io == ZR_NULL || function == ZR_NULL || io->state == ZR_NULL || io->state->global == ZR_NULL) {
        return;
    }

    function->hasDecoratorMetadata = ZR_FALSE;
    function->decoratorMetadataValue.type = ZR_VALUE_TYPE_NULL;
    ZrCore_Memory_RawSet(&function->decoratorMetadataValue.value, 0, sizeof(function->decoratorMetadataValue.value));
    function->decoratorMetadataValue.hasFunctionValue = ZR_FALSE;
    function->decoratorMetadataValue.functionValue = ZR_NULL;
    function->decoratorMetadataValue.startLine = 0;
    function->decoratorMetadataValue.endLine = 0;
    function->decoratorNamesLength = 0;
    function->decoratorNames = ZR_NULL;

    ZR_IO_READ_NATIVE_TYPE(io, function->hasDecoratorMetadata, TZrUInt8);
    if (function->hasDecoratorMetadata) {
        io_read_function_constant_variables(io, &function->decoratorMetadataValue, 1);
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->decoratorNamesLength, TZrSize);
    if (function->decoratorNamesLength > 0) {
        function->decoratorNames =
                io_read_array(io, function->decoratorNamesLength, sizeof(SZrString *));
        if (io->hasReadError) return;
        if (function->decoratorNames != ZR_NULL) {
            for (TZrSize index = 0; !io->hasReadError && (index < function->decoratorNamesLength); index++) {
                function->decoratorNames[index] = io_read_string_with_length(io);
            }
        }
    }
}

static void io_read_function_member_entries(SZrIo *io,
                                            SZrIoFunctionMemberEntry *entries,
                                            TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        entries[index].symbol = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].entryKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].reserved0, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].reserved1, TZrUInt16);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].prototypeIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].descriptorIndex, TZrUInt32);
    }
}

static void io_read_function_semir_type_table(SZrIo *io,
                                              SZrIoFunctionTypedTypeRef *typeTable,
                                              TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        io_read_function_typed_type_ref(io, &typeTable[index]);
    }
}

static void io_read_function_semir_ownership_table(SZrIo *io,
                                                   SZrIoSemIrOwnershipEntry *ownershipTable,
                                                   TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        ZR_IO_READ_NATIVE_TYPE(io, ownershipTable[index].state, TZrUInt32);
    }
}

static void io_read_function_semir_effect_table(SZrIo *io,
                                                SZrIoSemIrEffectEntry *effectTable,
                                                TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        ZR_IO_READ_NATIVE_TYPE(io, effectTable[index].kind, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effectTable[index].instructionIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effectTable[index].ownershipInputIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effectTable[index].ownershipOutputIndex, TZrUInt32);
    }
}

static void io_read_function_semir_block_table(SZrIo *io,
                                               SZrIoSemIrBlockEntry *blockTable,
                                               TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        ZR_IO_READ_NATIVE_TYPE(io, blockTable[index].blockId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, blockTable[index].firstInstructionIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, blockTable[index].instructionCount, TZrUInt32);
    }
}

static void io_read_function_semir_instruction_table(SZrIo *io,
                                                     SZrIoSemIrInstruction *instructionTable,
                                                     TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].opcode, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].execInstructionIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].typeTableIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].effectTableIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].destinationSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].operand0, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].operand1, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].deoptId, TZrUInt32);
    }
}

static void io_read_function_semir_deopt_table(SZrIo *io,
                                               SZrIoSemIrDeoptEntry *deoptTable,
                                               TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        ZR_IO_READ_NATIVE_TYPE(io, deoptTable[index].deoptId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, deoptTable[index].execInstructionIndex, TZrUInt32);
    }
}

static void io_read_function_callsite_cache_table(SZrIo *io,
                                                  SZrIoFunctionCallSiteCacheEntry *cacheTable,
                                                  TZrSize count) {
    for (TZrSize index = 0; !io->hasReadError && (index < count); index++) {
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].kind, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].instructionIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].memberEntryIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].deoptId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].argumentCount, TZrUInt32);
    }
}

/* SemIR 表按文件版本整体读取；表间索引在此阶段仍是序列化索引，运行时侧另行校验。 */
static void io_read_function_semir_metadata(SZrIo *io, SZrIoFunction *function) {

    if (io == ZR_NULL || function == ZR_NULL || io->state == ZR_NULL || io->state->global == ZR_NULL) {
        return;
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrTypeTableLength, TZrSize);
    if (function->semIrTypeTableLength > 0) {
        function->semIrTypeTable = io_read_array(io, function->semIrTypeTableLength, sizeof(SZrIoFunctionTypedTypeRef));
        if (io->hasReadError) return;
        if (function->semIrTypeTable != ZR_NULL) {
            io_read_function_semir_type_table(io, function->semIrTypeTable, function->semIrTypeTableLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrOwnershipTableLength, TZrSize);
    if (function->semIrOwnershipTableLength > 0) {
        function->semIrOwnershipTable = io_read_array(io, function->semIrOwnershipTableLength, sizeof(SZrIoSemIrOwnershipEntry));
        if (io->hasReadError) return;
        if (function->semIrOwnershipTable != ZR_NULL) {
            io_read_function_semir_ownership_table(io,
                                                   function->semIrOwnershipTable,
                                                   function->semIrOwnershipTableLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrEffectTableLength, TZrSize);
    if (function->semIrEffectTableLength > 0) {
        function->semIrEffectTable = io_read_array(io, function->semIrEffectTableLength, sizeof(SZrIoSemIrEffectEntry));
        if (io->hasReadError) return;
        if (function->semIrEffectTable != ZR_NULL) {
            io_read_function_semir_effect_table(io, function->semIrEffectTable, function->semIrEffectTableLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrBlockTableLength, TZrSize);
    if (function->semIrBlockTableLength > 0) {
        function->semIrBlockTable = io_read_array(io, function->semIrBlockTableLength, sizeof(SZrIoSemIrBlockEntry));
        if (io->hasReadError) return;
        if (function->semIrBlockTable != ZR_NULL) {
            io_read_function_semir_block_table(io, function->semIrBlockTable, function->semIrBlockTableLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrInstructionLength, TZrSize);
    if (function->semIrInstructionLength > 0) {
        function->semIrInstructions = io_read_array(io, function->semIrInstructionLength, sizeof(SZrIoSemIrInstruction));
        if (io->hasReadError) return;
        if (function->semIrInstructions != ZR_NULL) {
            io_read_function_semir_instruction_table(io,
                                                     function->semIrInstructions,
                                                     function->semIrInstructionLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrDeoptTableLength, TZrSize);
    if (function->semIrDeoptTableLength > 0) {
        function->semIrDeoptTable = io_read_array(io, function->semIrDeoptTableLength, sizeof(SZrIoSemIrDeoptEntry));
        if (io->hasReadError) return;
        if (function->semIrDeoptTable != ZR_NULL) {
            io_read_function_semir_deopt_table(io, function->semIrDeoptTable, function->semIrDeoptTableLength);
        }
    }
}

/* 调用点缓存表在绑定契约表之前读取；后者按缓存索引附加可重定位事实。 */
static void io_read_function_callsite_cache_metadata(SZrIo *io, SZrIoFunction *function) {

    if (io == ZR_NULL || function == ZR_NULL || io->state == ZR_NULL || io->state->global == ZR_NULL) {
        return;
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->callSiteCacheLength, TZrSize);
    if (function->callSiteCacheLength > 0) {
        function->callSiteCaches = io_read_array(io, function->callSiteCacheLength, sizeof(SZrIoFunctionCallSiteCacheEntry));
        if (io->hasReadError) return;
        if (function->callSiteCaches != ZR_NULL) {
            ZrCore_Memory_RawSet(function->callSiteCaches, 0,
                    sizeof(*function->callSiteCaches) * function->callSiteCacheLength);
            io_read_function_callsite_cache_table(io, function->callSiteCaches, function->callSiteCacheLength);
        }
    }
}

static void io_read_ffi_type_contract(
        SZrIo *io,
        SZrFfiTypeContract *type) {
    TZrUInt32 typeKind = 0u;

    ZR_IO_READ_NATIVE_TYPE(io, typeKind, TZrUInt32);
    type->typeKind = (EZrFfiTypeKind)typeKind;
    ZR_IO_READ_NATIVE_TYPE(io, type->size, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, type->alignment, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, type->canonicalTypeHash, TZrUInt64);
    ZR_IO_READ_NATIVE_TYPE(io, type->layoutHash, TZrUInt64);
    ZR_IO_READ_NATIVE_TYPE(io, type->flags, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, type->aggregateFieldStart, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, type->aggregateFieldCount, TZrUInt32);
}

/* FFI 调用签名依赖值类型与所有权约束，运行时投影前还会执行公共验证。 */
static void io_read_ffi_callable_contract(
        SZrIo *io,
        SZrFfiCallableContract *callable) {
    TZrUInt8 isVariadic = 0u;
    TZrUInt32 receiverEffect = 0u;

    ZR_IO_READ_NATIVE_TYPE(io, callable->parameterCount, TZrUInt32);
    if (callable->parameterCount > ZR_FFI_CONTRACT_MAX_PARAMETERS) {
        io->hasReadError = ZR_TRUE;
        return;
    }
    for (TZrUInt32 index = 0u; !io->hasReadError && (index < callable->parameterCount); index++) {
        SZrFfiCallableParameterContract *parameter =
                &callable->parameters[index];
        TZrUInt32 passingForm = 0u;
        TZrUInt32 escapeUpperBound = 0u;
        TZrUInt32 entryInitialization = 0u;
        TZrUInt32 exitInitialization = 0u;
        TZrUInt8 acceptsTemporary = 0u;
        TZrUInt32 callSiteMarker = 0u;

        ZR_IO_READ_NATIVE_TYPE(io, parameter->canonicalTypeHash, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, passingForm, TZrUInt32);
        parameter->passingForm = (EZrFfiCallablePassingForm)passingForm;
        ZR_IO_READ_NATIVE_TYPE(io, escapeUpperBound, TZrUInt32);
        parameter->escapeUpperBound =
                (EZrFfiCallableEscapeUpperBound)escapeUpperBound;
        ZR_IO_READ_NATIVE_TYPE(io, entryInitialization, TZrUInt32);
        parameter->entryInitialization =
                (EZrFfiCallableEntryInitialization)entryInitialization;
        ZR_IO_READ_NATIVE_TYPE(io, exitInitialization, TZrUInt32);
        parameter->exitInitialization =
                (EZrFfiCallableExitInitialization)exitInitialization;
        ZR_IO_READ_NATIVE_TYPE(io, acceptsTemporary, TZrUInt8);
        parameter->acceptsTemporary = acceptsTemporary ? ZR_TRUE : ZR_FALSE;
        ZR_IO_READ_NATIVE_TYPE(io, callSiteMarker, TZrUInt32);
        parameter->callSiteMarker =
                (EZrFfiCallableCallSiteMarker)callSiteMarker;
    }
    ZR_IO_READ_NATIVE_TYPE(io, callable->returnTypeHash, TZrUInt64);
    ZR_IO_READ_NATIVE_TYPE(io, receiverEffect, TZrUInt32);
    callable->receiverEffect = (EZrFfiCallableReceiverEffect)receiverEffect;
    ZR_IO_READ_NATIVE_TYPE(io, callable->effectFlags, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, isVariadic, TZrUInt8);
    callable->isVariadic = isVariadic ? ZR_TRUE : ZR_FALSE;
    ZR_IO_READ_NATIVE_TYPE(io, callable->contractHash, TZrUInt64);
}

/* FFI 导入契约随函数进入源树，运行时复制时再执行公共契约验证。 */
static void io_read_function_native_import_contracts(
        SZrIo *io,
        SZrIoFunction *function) {
    TZrUInt64 serializedContractCount = 0u;

    ZR_IO_READ_NATIVE_TYPE(io, serializedContractCount, TZrUInt64);
    if (serializedContractCount > (TZrUInt64)ZR_FFI_CONTRACT_MAX_IMPORTS_PER_FUNCTION ||
        serializedContractCount > (TZrUInt64)SIZE_MAX) {
        io->hasReadError = ZR_TRUE;
        function->nativeImportContractLength = 0u;
        return;
    }
    function->nativeImportContractLength = (TZrSize)serializedContractCount;
    if (function->nativeImportContractLength == 0u) {
        return;
    }
    function->nativeImportContracts =
            io_read_array(io, function->nativeImportContractLength, sizeof(SZrNativeImportContract));
    if (io->hasReadError) return;
    if (function->nativeImportContracts == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawSet(
            function->nativeImportContracts,
            0,
            sizeof(SZrNativeImportContract) * function->nativeImportContractLength);
    for (TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength);
         contractIndex++) {
        SZrNativeImportContract *contract =
                &function->nativeImportContracts[contractIndex];
        TZrUInt32 abi = 0u;
        TZrUInt32 targetEndianness = 0u;
        TZrUInt32 charset = 0u;
        TZrUInt32 errorPolicy = 0u;
        TZrUInt32 cleanupPolicy = 0u;
        TZrUInt32 callbackLifetime = 0u;
        TZrUInt32 callbackThreadPolicy = 0u;
        TZrUInt32 callbackExceptionPolicy = 0u;
        TZrUInt8 isVariadic = 0u;

        ZR_IO_READ_NATIVE_TYPE(io, contract->schemaVersion, TZrUInt32);
        ZrCore_Io_Read(io, (TZrBytePtr)contract->libraryLocator, sizeof(contract->libraryLocator));
        ZrCore_Io_Read(io, (TZrBytePtr)contract->entryPoint, sizeof(contract->entryPoint));
        ZR_IO_READ_NATIVE_TYPE(io, contract->symbolId, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, contract->declaringModuleId, TZrUInt64);
        io_read_ffi_callable_contract(io, &contract->callable);
        if (io->hasReadError) {
            return;
        }
        ZR_IO_READ_NATIVE_TYPE(io, contract->availability, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, contract->requiredCapabilities, TZrUInt64);
        ZrCore_Io_Read(
                io,
                (TZrBytePtr)contract->sourceMapping.document,
                sizeof(contract->sourceMapping.document));
        ZR_IO_READ_NATIVE_TYPE(
                io, contract->sourceMapping.startOffset, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(
                io, contract->sourceMapping.endOffset, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(
                io, contract->sourceMapping.startLine, TZrInt32);
        ZR_IO_READ_NATIVE_TYPE(
                io, contract->sourceMapping.startColumn, TZrInt32);
        ZR_IO_READ_NATIVE_TYPE(
                io, contract->sourceMapping.endLine, TZrInt32);
        ZR_IO_READ_NATIVE_TYPE(
                io, contract->sourceMapping.endColumn, TZrInt32);
        ZR_IO_READ_NATIVE_TYPE(io, abi, TZrUInt32);
        contract->signature.abi = (EZrFfiAbi)abi;
        ZR_IO_READ_NATIVE_TYPE(io, contract->signature.targetPointerSize, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, targetEndianness, TZrUInt32);
        contract->signature.targetEndianness =
                (EZrFfiTargetEndianness)targetEndianness;
        ZrCore_Io_Read(
                io,
                (TZrBytePtr)contract->signature.targetTriple,
                sizeof(contract->signature.targetTriple));
        ZR_IO_READ_NATIVE_TYPE(io, contract->signature.targetAbiHash, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, charset, TZrUInt32);
        contract->signature.charset = (EZrFfiCharset)charset;
        ZR_IO_READ_NATIVE_TYPE(io, errorPolicy, TZrUInt32);
        contract->signature.errorPolicy = (EZrFfiErrorPolicy)errorPolicy;
        ZR_IO_READ_NATIVE_TYPE(io, cleanupPolicy, TZrUInt32);
        contract->signature.cleanupPolicy = (EZrFfiCleanupPolicy)cleanupPolicy;
        ZR_IO_READ_NATIVE_TYPE(io, callbackLifetime, TZrUInt32);
        contract->signature.callbackLifetime =
                (EZrFfiCallbackLifetime)callbackLifetime;
        ZR_IO_READ_NATIVE_TYPE(io, callbackThreadPolicy, TZrUInt32);
        contract->signature.callbackThreadPolicy =
                (EZrFfiCallbackThreadPolicy)callbackThreadPolicy;
        ZR_IO_READ_NATIVE_TYPE(io, callbackExceptionPolicy, TZrUInt32);
        contract->signature.callbackExceptionPolicy =
                (EZrFfiCallbackExceptionPolicy)callbackExceptionPolicy;
        ZR_IO_READ_NATIVE_TYPE(io, isVariadic, TZrUInt8);
        contract->signature.isVariadic = isVariadic ? ZR_TRUE : ZR_FALSE;
        ZR_IO_READ_NATIVE_TYPE(io, contract->signature.parameterCount, TZrUInt32);
        if (contract->signature.parameterCount >
            ZR_FFI_CONTRACT_MAX_PARAMETERS) {
            io->hasReadError = ZR_TRUE;
            return;
        }
        io_read_ffi_type_contract(io, &contract->signature.returnType);
        for (TZrUInt32 parameterIndex = 0u; !io->hasReadError && (parameterIndex < contract->signature.parameterCount);
             parameterIndex++) {
            SZrFfiParameterContract ignoredParameter = {0};
            SZrFfiParameterContract *parameter =
                    parameterIndex < ZR_FFI_CONTRACT_MAX_PARAMETERS
                            ? &contract->signature.parameters[parameterIndex]
                            : &ignoredParameter;
            TZrUInt32 direction = 0u;
            TZrUInt32 marshalling = 0u;
            TZrUInt32 ownership = 0u;
            TZrUInt8 isNullable = 0u;

            io_read_ffi_type_contract(io, &parameter->type);
            ZR_IO_READ_NATIVE_TYPE(io, direction, TZrUInt32);
            parameter->direction = (EZrFfiDirection)direction;
            ZR_IO_READ_NATIVE_TYPE(io, marshalling, TZrUInt32);
            parameter->marshalling = (EZrFfiMarshallingKind)marshalling;
            ZR_IO_READ_NATIVE_TYPE(io, ownership, TZrUInt32);
            parameter->ownership = (EZrFfiParameterOwnership)ownership;
            ZR_IO_READ_NATIVE_TYPE(io, isNullable, TZrUInt8);
            parameter->isNullable = isNullable ? ZR_TRUE : ZR_FALSE;
            ZR_IO_READ_NATIVE_TYPE(io, parameter->flags, TZrUInt32);
        }
        ZR_IO_READ_NATIVE_TYPE(
                io, contract->signature.aggregateFieldCount, TZrUInt32);
        if (contract->signature.aggregateFieldCount >
            ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS) {
            io->hasReadError = ZR_TRUE;
            return;
        }
        for (TZrUInt32 fieldIndex = 0u; !io->hasReadError && (fieldIndex < contract->signature.aggregateFieldCount);
             fieldIndex++) {
            SZrFfiAggregateFieldContract *field =
                    &contract->signature.aggregateFields[fieldIndex];
            TZrUInt32 fieldTypeKind = 0u;

            ZrCore_Io_Read(
                    io, (TZrBytePtr)field->name, sizeof(field->name));
            ZR_IO_READ_NATIVE_TYPE(io, fieldTypeKind, TZrUInt32);
            field->typeKind = (EZrFfiTypeKind)fieldTypeKind;
            ZR_IO_READ_NATIVE_TYPE(io, field->size, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, field->alignment, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, field->offset, TZrUInt32);
        }
        ZR_IO_READ_NATIVE_TYPE(io, contract->signature.signatureHash, TZrUInt64);
    }
}

static void io_read_classes(SZrIo *io, SZrIoClass *classes, TZrSize count);
static void io_read_structs(SZrIo *io, SZrIoStruct *structs, TZrSize count);

/* 子函数树嵌入所属函数的闭包列表，释放时沿 subFunction 递归。 */
static void io_read_function_closures(SZrIo *io, SZrIoFunctionClosure *closures, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionClosure *closure = &closures[i];
        closure->subFunction = io_read_array(io, 1u, sizeof(SZrIoFunction));
        if (io->hasReadError) return;
                io_read_functions(io, closure->subFunction, 1);
        /* 子函数原生存储由所属函数源树递归回收。 */
    }
}

/* 调试源身份与逐指令位置按版本读取，供异常栈和调试器在加载后使用。 */
static void io_read_function_debug_infos(SZrIo *io, SZrIoFunctionDebugInfo *debugInfos, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunctionDebugInfo *debugInfo = &debugInfos[i];
        debugInfo->sourceFile = ZR_NULL;
        debugInfo->sourceHash = ZR_NULL;
        debugInfo->instructionRanges = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_IDENTITY) {
            debugInfo->sourceFile = io_read_string_with_length(io);
            debugInfo->sourceHash = io_read_string_with_length(io);
        }
        ZR_IO_READ_NATIVE_TYPE(io, debugInfo->instructionsLength, TZrSize);
        debugInfo->instructionsLine = ZR_NULL;
        if (debugInfo->instructionsLength > 0) {
            debugInfo->instructionsLine =
                    io_read_array(io, debugInfo->instructionsLength, sizeof(TZrUInt64));
            if (io->hasReadError) return;
            if (debugInfo->instructionsLine != ZR_NULL) {
                ZrCore_Io_Read(io,
                               (TZrBytePtr) debugInfo->instructionsLine,
                               sizeof(TZrUInt64) * debugInfo->instructionsLength);
            } else {
                TZrUInt64 ignoredLine = 0;
                for (TZrSize index = 0; !io->hasReadError && (index < debugInfo->instructionsLength); index++) {
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredLine, TZrUInt64);
                }
            }
        }

        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES &&
            debugInfo->instructionsLength > 0) {
            debugInfo->instructionRanges = io_read_array(io, debugInfo->instructionsLength, sizeof(SZrIoInstructionSourceRange));
            if (io->hasReadError) return;
            if (debugInfo->instructionRanges != ZR_NULL) {
                ZrCore_Io_Read(io,
                               (TZrBytePtr) debugInfo->instructionRanges,
                               sizeof(SZrIoInstructionSourceRange) * debugInfo->instructionsLength);
            } else {
                SZrIoInstructionSourceRange ignoredRange;
                for (TZrSize index = 0; !io->hasReadError && (index < debugInfo->instructionsLength); index++) {
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredRange.startLine, TZrUInt32);
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredRange.startColumn, TZrUInt32);
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredRange.endLine, TZrUInt32);
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredRange.endColumn, TZrUInt32);
                }
            }
        }
    }
}

/* Decode on indices before multiplying by the current physical slot size. */
static void io_read_frame_coordinate(SZrIo *io, TZrUInt32 *result) {
    *result = 0u;
    TZrUInt32 slot = 0u, intraSlot = 0u;
    const TZrUInt32 stride = (TZrUInt32)sizeof(SZrTypeValueOnStack);
    ZR_IO_READ_NATIVE_TYPE(io, slot, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, intraSlot, TZrUInt32);
    if (io->hasReadError || intraSlot >= stride || slot > (UINT32_MAX - intraSlot) / stride) {
        io->hasReadError = ZR_TRUE;
        return;
    }
    *result = slot * stride + intraSlot;
}

/* 帧布局侧表按版本读取；此处保留原始槽位，运行时投影前另执行严格验证。 */
static void io_read_function_frame_layout(SZrIo *io, SZrIoFunction *function) {

    if (function == ZR_NULL) {
        return;
    }

    io_read_frame_coordinate(io, &function->frameByteSize);
    ZR_IO_READ_NATIVE_TYPE(io, function->frameByteAlign, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, function->frameSlotLayoutsLength, TZrSize);
    if (function->frameSlotLayoutsLength == 0) {
        function->frameSlotLayouts = ZR_NULL;
        return;
    }

    function->frameSlotLayouts =
            io_read_array(io, function->frameSlotLayoutsLength, sizeof(SZrIoFunctionFrameSlotLayout));
    if (io->hasReadError) return;
    if (function->frameSlotLayouts == ZR_NULL) {
        SZrIoFunctionFrameSlotLayout ignoredLayout;
        for (TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++) {
            ZR_IO_READ_NATIVE_TYPE(io, ignoredLayout.stackSlot, TZrUInt32);
            io_read_frame_coordinate(io, &ignoredLayout.byteOffset);
            ZR_IO_READ_NATIVE_TYPE(io, ignoredLayout.byteSize, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, ignoredLayout.byteAlign, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, ignoredLayout.typeLayoutId, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, ignoredLayout.slotKind, TZrUInt8);
            ZR_IO_READ_NATIVE_TYPE(io, ignoredLayout.isParameter, TZrUInt8);
            ZR_IO_READ_NATIVE_TYPE(io, ignoredLayout.reserved0, TZrUInt16);
        }
        function->frameSlotLayoutsLength = 0;
        return;
    }

    for (TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++) {
        SZrIoFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        ZR_IO_READ_NATIVE_TYPE(io, layout->stackSlot, TZrUInt32);
        io_read_frame_coordinate(io, &layout->byteOffset);
        ZR_IO_READ_NATIVE_TYPE(io, layout->byteSize, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, layout->byteAlign, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, layout->typeLayoutId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, layout->slotKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, layout->isParameter, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, layout->reserved0, TZrUInt16);
    }
}


/* 递归构建函数及子函数原生树；长度字段与数组共同交由 ReadSourceFree 管理。 */
static void io_read_functions(SZrIo *io, SZrIoFunction *functions, TZrSize count) {
    if (io->hasReadError || ++io->decodeDepth > ZR_IO_MAX_DECODE_DEPTH) {
        io->hasReadError = ZR_TRUE;
        return;
    }
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoFunction *function = &functions[i];
        function->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, function->startLine, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->endLine, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->parametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, function->hasVarArgs, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->stackSize, TZrUInt32);
        function->vmEntryClearStackSizePlusOne = 0u;
        if (io->sourceVersionPatch >=
            ZR_IO_SOURCE_PATCH_HAS_VM_ENTRY_CLEAR_STACK_SIZE) {
            ZR_IO_READ_NATIVE_TYPE(
                    io, function->vmEntryClearStackSizePlusOne, TZrUInt32);
        }
        function->sourceVersionPatch = io->sourceVersionPatch;
        function->frameByteSize = 0;
        function->frameByteAlign = 0;
        function->frameSlotLayoutsLength = 0;
        function->frameSlotLayouts = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT) {
            io_read_function_frame_layout(io, function);
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->instructionsLength, TZrSize);
        // read instructions ...
        if (function->instructionsLength > 0 &&
            !io_validate_array_count(io, function->instructionsLength, sizeof(TZrInstruction))) {
            return;
        }
        if (function->instructionsLength > 0) {
            function->instructions =
                    io_read_array(io, function->instructionsLength, sizeof(TZrInstruction));
            if (io->hasReadError) return;
            ZrCore_Io_Read(io, (TZrBytePtr) function->instructions, sizeof(TZrInstruction) * function->instructionsLength);
        } else {
            function->instructions = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->localVariablesLength, TZrSize);
        if (function->localVariablesLength > 0) {
            function->localVariables = io_read_array(io, function->localVariablesLength, sizeof(SZrIoFunctionLocalVariable));
            if (io->hasReadError) return;
            io_read_function_local_variables(io, function->localVariables, function->localVariablesLength);
        } else {
            function->localVariables = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->closureVariablesLength, TZrSize);
        if (function->closureVariablesLength > 0) {
            function->closureVariables = io_read_array(io, function->closureVariablesLength, sizeof(SZrIoFunctionClosureVariable));
            if (io->hasReadError) return;
            if (function->closureVariables != ZR_NULL) {
                io_read_function_closure_variables(io, function->closureVariables, function->closureVariablesLength);
            }
        } else {
            function->closureVariables = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->catchClauseCount, TZrSize);
        if (function->catchClauseCount > 0) {
            function->catchClauses = io_read_array(io, function->catchClauseCount, sizeof(SZrIoFunctionCatchClause));
            if (io->hasReadError) return;
            if (function->catchClauses != ZR_NULL) {
                io_read_function_catch_clauses(io, function->catchClauses, function->catchClauseCount);
            }
        } else {
            function->catchClauses = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->exceptionHandlerCount, TZrSize);
        if (function->exceptionHandlerCount > 0) {
            function->exceptionHandlers = io_read_array(io, function->exceptionHandlerCount, sizeof(SZrIoFunctionExceptionHandler));
            if (io->hasReadError) return;
            if (function->exceptionHandlers != ZR_NULL) {
                io_read_function_exception_handlers(io,
                                                    function->exceptionHandlers,
                                                    function->exceptionHandlerCount);
            }
        } else {
            function->exceptionHandlers = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->constantVariablesLength, TZrSize);
        if (function->constantVariablesLength > 0) {
            function->constantVariables = io_read_array(io, function->constantVariablesLength, sizeof(SZrIoFunctionConstantVariable));
            if (io->hasReadError) return;
            io_read_function_constant_variables(io, function->constantVariables, function->constantVariablesLength);
        } else {
            function->constantVariables = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->exportedVariablesLength, TZrSize);
        if (function->exportedVariablesLength > 0) {
            function->exportedVariables = io_read_array(io, function->exportedVariablesLength, sizeof(SZrIoFunctionExportedVariable));
            if (io->hasReadError) return;
            io_read_function_exported_variables(io, function->exportedVariables, function->exportedVariablesLength);
        } else {
            function->exportedVariables = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->typedLocalBindingsLength, TZrSize);
        if (function->typedLocalBindingsLength > 0) {
            function->typedLocalBindings = io_read_array(io, function->typedLocalBindingsLength, sizeof(SZrIoFunctionTypedLocalBinding));
            if (io->hasReadError) return;
            io_read_function_typed_local_bindings(io,
                                                  function->typedLocalBindings,
                                                  function->typedLocalBindingsLength);
        } else {
            function->typedLocalBindings = ZR_NULL;
        }
        if (io->sourceVersionPatch >=
            ZR_IO_SOURCE_PATCH_HAS_TYPED_CLOSURE_CAPTURE_BINDINGS) {
            ZR_IO_READ_NATIVE_TYPE(io, function->typedClosureBindingsLength, TZrSize);
            if (function->typedClosureBindingsLength > 0) {
                function->typedClosureBindings = io_read_array(io, function->typedClosureBindingsLength, sizeof(SZrIoFunctionTypedClosureBinding));
                if (io->hasReadError) return;
                io_read_function_typed_closure_bindings(io,
                                                        function->typedClosureBindings,
                                                        function->typedClosureBindingsLength);
            } else {
                function->typedClosureBindings = ZR_NULL;
            }
        } else {
            function->typedClosureBindingsLength = 0;
            function->typedClosureBindings = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->typedExportedSymbolsLength, TZrSize);
        if (function->typedExportedSymbolsLength > 0) {
            function->typedExportedSymbols = io_read_array(io, function->typedExportedSymbolsLength, sizeof(SZrIoFunctionTypedExportSymbol));
            if (io->hasReadError) return;
            io_read_function_typed_export_symbols(io,
                                                  function->typedExportedSymbols,
                                                  function->typedExportedSymbolsLength);
        } else {
            function->typedExportedSymbols = ZR_NULL;
        }
        io_read_function_metadata_token_model(io, function);
        ZR_IO_READ_NATIVE_TYPE(io, function->staticImportsLength, TZrSize);
        if (function->staticImportsLength > 0) {
            function->staticImports = io_read_array(io, function->staticImportsLength, sizeof(SZrString *));
            if (io->hasReadError) return;
            if (function->staticImports != ZR_NULL) {
                for (TZrSize index = 0; !io->hasReadError && (index < function->staticImportsLength); index++) {
                    function->staticImports[index] = io_read_string_with_length(io);
                }
            }
        } else {
            function->staticImports = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->moduleEntryEffectsLength, TZrSize);
        if (function->moduleEntryEffectsLength > 0) {
            function->moduleEntryEffects = io_read_array(io, function->moduleEntryEffectsLength, sizeof(SZrIoFunctionModuleEffect));
            if (io->hasReadError) return;
            if (function->moduleEntryEffects != ZR_NULL) {
                io_read_function_module_effects(io,
                                                function->moduleEntryEffects,
                                                function->moduleEntryEffectsLength);
            }
        } else {
            function->moduleEntryEffects = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->exportedCallableSummariesLength, TZrSize);
        if (function->exportedCallableSummariesLength > 0) {
            function->exportedCallableSummaries = io_read_array(io, function->exportedCallableSummariesLength, sizeof(SZrIoFunctionCallableSummary));
            if (io->hasReadError) return;
            if (function->exportedCallableSummaries != ZR_NULL) {
                io_read_function_callable_summaries(io,
                                                    function->exportedCallableSummaries,
                                                    function->exportedCallableSummariesLength);
            }
        } else {
            function->exportedCallableSummaries = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->topLevelCallableBindingsLength, TZrSize);
        if (function->topLevelCallableBindingsLength > 0) {
            function->topLevelCallableBindings = io_read_array(io, function->topLevelCallableBindingsLength, sizeof(SZrIoFunctionTopLevelCallableBinding));
            if (io->hasReadError) return;
            if (function->topLevelCallableBindings != ZR_NULL) {
                io_read_function_top_level_callable_bindings(io,
                                                             function->topLevelCallableBindings,
                                                             function->topLevelCallableBindingsLength);
            }
        } else {
            function->topLevelCallableBindings = ZR_NULL;
        }
        function->parameterMetadataLength = 0;
        function->parameterMetadata = ZR_NULL;
        function->hasCallableReturnType = ZR_FALSE;
        ZrCore_Memory_RawSet(&function->callableReturnType, 0, sizeof(function->callableReturnType));
        function->callableReturnType.baseType = ZR_VALUE_TYPE_OBJECT;
        function->callableReturnType.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        function->compileTimeVariableInfosLength = 0;
        function->compileTimeVariableInfos = ZR_NULL;
        function->compileTimeFunctionInfosLength = 0;
        function->compileTimeFunctionInfos = ZR_NULL;
        function->escapeBindingLength = 0;
        function->escapeBindings = ZR_NULL;
        function->returnEscapeSlotCount = 0;
        function->returnEscapeSlots = ZR_NULL;
        function->hasDecoratorMetadata = ZR_FALSE;
        function->decoratorMetadataValue.type = ZR_VALUE_TYPE_NULL;
        ZrCore_Memory_RawSet(&function->decoratorMetadataValue.value, 0, sizeof(function->decoratorMetadataValue.value));
        function->decoratorMetadataValue.hasFunctionValue = ZR_FALSE;
        function->decoratorMetadataValue.functionValue = ZR_NULL;
        function->decoratorMetadataValue.startLine = 0;
        function->decoratorMetadataValue.endLine = 0;
        function->decoratorNamesLength = 0;
        function->decoratorNames = ZR_NULL;
        function->memberEntriesLength = 0;
        function->memberEntries = ZR_NULL;
        function->semIrTypeTableLength = 0;
        function->semIrTypeTable = ZR_NULL;
        function->semIrOwnershipTableLength = 0;
        function->semIrOwnershipTable = ZR_NULL;
        function->semIrEffectTableLength = 0;
        function->semIrEffectTable = ZR_NULL;
        function->semIrBlockTableLength = 0;
        function->semIrBlockTable = ZR_NULL;
        function->semIrInstructionLength = 0;
        function->semIrInstructions = ZR_NULL;
        function->semIrDeoptTableLength = 0;
        function->semIrDeoptTable = ZR_NULL;
        function->callSiteCacheLength = 0;
        function->callSiteCaches = ZR_NULL;
        function->nativeImportContractLength = 0;
        function->nativeImportContracts = ZR_NULL;
        function->testManifestDataLength = 0;
        function->testManifestData = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA) {
            io_read_function_metadata_parameters(io, &function->parameterMetadata, &function->parameterMetadataLength);
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_CALLABLE_RETURN_TYPE) {
            ZR_IO_READ_NATIVE_TYPE(io, function->hasCallableReturnType, TZrUInt8);
            if (function->hasCallableReturnType) {
                io_read_function_typed_type_ref(io, &function->callableReturnType);
            }
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA) {
            ZR_IO_READ_NATIVE_TYPE(io, function->compileTimeVariableInfosLength, TZrSize);
            if (function->compileTimeVariableInfosLength > 0) {
                function->compileTimeVariableInfos =
                        io_read_array(io, function->compileTimeVariableInfosLength, sizeof(SZrIoFunctionCompileTimeVariableInfo));
                if (io->hasReadError) return;
                if (function->compileTimeVariableInfos != ZR_NULL) {
                    io_read_function_compile_time_variable_infos(io,
                                                                 function->compileTimeVariableInfos,
                                                                 function->compileTimeVariableInfosLength);
                }
            }
            ZR_IO_READ_NATIVE_TYPE(io, function->compileTimeFunctionInfosLength, TZrSize);
            if (function->compileTimeFunctionInfosLength > 0) {
                function->compileTimeFunctionInfos =
                        io_read_array(io, function->compileTimeFunctionInfosLength, sizeof(SZrIoFunctionCompileTimeFunctionInfo));
                if (io->hasReadError) return;
                if (function->compileTimeFunctionInfos != ZR_NULL) {
                    io_read_function_compile_time_function_infos(io,
                                                                 function->compileTimeFunctionInfos,
                                                                 function->compileTimeFunctionInfosLength);
                }
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA) {
                ZR_IO_READ_NATIVE_TYPE(io, function->escapeBindingLength, TZrSize);
                if (function->escapeBindingLength > 0) {
                    function->escapeBindings =
                            io_read_array(io, function->escapeBindingLength, sizeof(SZrIoFunctionEscapeBinding));
                    if (io->hasReadError) return;
                    if (function->escapeBindings != ZR_NULL) {
                        io_read_function_escape_bindings(io,
                                                         function->escapeBindings,
                                                         function->escapeBindingLength);
                    }
                }
                ZR_IO_READ_NATIVE_TYPE(io, function->returnEscapeSlotCount, TZrSize);
                if (function->returnEscapeSlotCount > 0) {
                    function->returnEscapeSlots =
                            io_read_array(io, function->returnEscapeSlotCount, sizeof(TZrUInt32));
                    if (io->hasReadError) return;
                    if (function->returnEscapeSlots != ZR_NULL) {
                        ZrCore_Io_Read(io,
                                       (TZrBytePtr)function->returnEscapeSlots,
                                       sizeof(TZrUInt32) * function->returnEscapeSlotCount);
                    }
                }
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TEST_MANIFEST) {
                TZrUInt64 serializedLength = 0U;

                ZR_IO_READ_NATIVE_TYPE(io, serializedLength, TZrUInt64);
                if (serializedLength > (TZrUInt64)UINT32_MAX ||
                    serializedLength > (TZrUInt64)SIZE_MAX) {
                    io->hasReadError = ZR_TRUE;
                } else if (serializedLength > 0U) {
                    function->testManifestDataLength = (TZrSize)serializedLength;
                    function->testManifestData = io_read_array(io, function->testManifestDataLength, 1u);
                    if (io->hasReadError) return;
                    if (function->testManifestData == ZR_NULL) {
                        io->hasReadError = ZR_TRUE;
                        function->testManifestDataLength = 0U;
                    } else {
                        ZrCore_Io_Read(io,
                                       function->testManifestData,
                                       function->testManifestDataLength);
                    }
                }
            } else {
                TZrUInt64 removedLegacyTestInfoCount = 0U;

                ZR_IO_READ_NATIVE_TYPE(io, removedLegacyTestInfoCount, TZrUInt64);
                if (removedLegacyTestInfoCount > 0U) {
                    io->hasReadError = ZR_TRUE;
                    return;
                }
            }
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_DECORATOR_METADATA) {
            io_read_function_decorator_metadata(io, function);
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MEMBER_ENTRIES) {
            ZR_IO_READ_NATIVE_TYPE(io, function->memberEntriesLength, TZrSize);
            if (function->memberEntriesLength > 0) {
                function->memberEntries =
                        io_read_array(io, function->memberEntriesLength, sizeof(SZrIoFunctionMemberEntry));
                if (io->hasReadError) return;
                if (function->memberEntries != ZR_NULL) {
                    io_read_function_member_entries(io, function->memberEntries, function->memberEntriesLength);
                }
            }
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_SEMIR_METADATA) {
            io_read_function_semir_metadata(io, function);
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_CALLSITE_CACHE) {
            io_read_function_callsite_cache_metadata(io, function);
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_CALL_BINDING &&
            !ZrCore_Io_ReadCallBindings(io, function)) {
            io->hasReadError = ZR_TRUE;
            return;
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_NATIVE_IMPORT_CONTRACTS) {
            io_read_function_native_import_contracts(io, function);
        }
        // 读取PROTOTYPES_LENGTH和PROTOTYPES（结构化格式）
        ZR_IO_READ_NATIVE_TYPE(io, function->prototypesLength, TZrSize);
        function->classes = ZR_NULL;
        function->structs = ZR_NULL;
        function->prototypeDataLength = 0;
        function->prototypeData = ZR_NULL;

        // writer 总是会在 PROTOTYPES_LENGTH 后继续写入 CLASS/STRUCT count，
        // 即便 prototypesLength == 0 也会写两个 0，用于固定 .zro 布局。
        TZrSize classCount = 0;
        TZrSize structCount = 0;
        ZR_IO_READ_NATIVE_TYPE(io, classCount, TZrSize);
        function->classesLength = classCount;

        if (classCount > 0) {
            function->classes = io_read_array(io, classCount, sizeof(SZrIoClass));
            if (io->hasReadError) return;
            if (function->classes != ZR_NULL) {
                io_read_classes(io, function->classes, classCount);
            }
        }

        ZR_IO_READ_NATIVE_TYPE(io, structCount, TZrSize);
        function->structsLength = structCount;
        if (structCount > 0) {
            function->structs = io_read_array(io, structCount, sizeof(SZrIoStruct));
            if (io->hasReadError) return;
            if (function->structs != ZR_NULL) {
                io_read_structs(io, function->structs, structCount);
            }
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_PROTOTYPE_BLOB) {
            ZR_IO_READ_NATIVE_TYPE(io, function->prototypeDataLength, TZrSize);
            if (function->prototypeDataLength > 0) {
                function->prototypeData = io_read_array(io, function->prototypeDataLength, 1u);
                if (io->hasReadError) return;
                if (function->prototypeData != ZR_NULL) {
                    ZrCore_Io_Read(io, function->prototypeData, function->prototypeDataLength);
                }
            }
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->closuresLength, TZrSize);
        if (function->closuresLength > 0) {
            function->closures = io_read_array(io, function->closuresLength, sizeof(SZrIoFunctionClosure));
            if (io->hasReadError) return;
            io_read_function_closures(io, function->closures, function->closuresLength);
        } else {
            function->closures = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->debugInfosLength, TZrSize);
        if (function->debugInfosLength > 0) {
            function->debugInfos =
                    io_read_array(io, function->debugInfosLength, sizeof(SZrIoFunctionDebugInfo));
            if (io->hasReadError) return;
            io_read_function_debug_infos(io, function->debugInfos, function->debugInfosLength);
        } else {
            function->debugInfos = ZR_NULL;
        }
        if (!io->hasReadError && !io_verify_function(function)) {
            io->hasReadError = ZR_TRUE;
        }
    }
    --io->decodeDepth;
}

static void io_read_method(SZrIo *io, SZrIoMethod *method) {
    method->name = io_read_string_with_length(io);
    ZR_IO_READ_NATIVE_TYPE(io, method->functionsLength, TZrSize);
    method->functions = io_read_array(io, method->functionsLength, sizeof(SZrIoFunction));
    if (io->hasReadError) return;
    io_read_functions(io, method->functions, method->functionsLength);
}


static void io_read_property(SZrIo *io, SZrIoProperty *property) {
    property->name = io_read_string_with_length(io);
    /* TODO: 文件固定读取 32 位 propertyType；需核对写入端是否承诺同一宽度与枚举域。 */
    ZR_IO_READ_NATIVE_TYPE(io, property->propertyType, TZrUInt32);
    property->getter = io_read_array(io, 1u, sizeof(SZrIoFunction));
    if (io->hasReadError) return;
    io_read_functions(io, property->getter, 1);
    property->setter = io_read_array(io, 1u, sizeof(SZrIoFunction));
    if (io->hasReadError) return;
    io_read_functions(io, property->setter, 1);
}

static void io_read_meta(SZrIo *io, SZrIoMeta *meta) {
    /* TODO: metaType 按 32 位字段读取；需核对跨编译器枚举宽度的格式承诺。 */
    ZR_IO_READ_NATIVE_TYPE(io, meta->metaType, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, meta->functionsLength, TZrSize);
    meta->functions = io_read_array(io, meta->functionsLength, sizeof(SZrIoFunction));
    if (io->hasReadError) return;
    io_read_functions(io, meta->functions, meta->functionsLength);
}

static void io_read_enum_fields(SZrIo *io, SZrIoEnumField *fields, TZrSize count, EZrValueType valueType) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoEnumField *field = &fields[i];
        field->name = io_read_string_with_length(io);
        io_read_value(io, valueType, &field->value);
    }
}

/* 成员类型决定联合体有效分支，释放器据同一判别式选择对应子树。 */
static void io_read_member_declares(SZrIo *io, SZrIoMemberDeclare *declares, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoMemberDeclare *declare = &declares[i];
        ZR_IO_READ_NATIVE_TYPE(io, declare->type, EZrIoMemberDeclareType);
        switch (declare->type) {
            case ZR_IO_MEMBER_DECLARE_TYPE_METHOD: {
                // io_read_functions(io, declare->function, 1);
                declare->method = io_read_array(io, 1u, sizeof(SZrIoMethod));
                if (io->hasReadError) return;
                io_read_method(io, declare->method);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_META: {
                declare->meta = io_read_array(io, 1u, sizeof(SZrIoMeta));
                if (io->hasReadError) return;
                io_read_meta(io, declare->meta);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY: {
                declare->property = io_read_array(io, 1u, sizeof(SZrIoProperty));
                if (io->hasReadError) return;
                io_read_property(io, declare->property);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_ENUM: {
                declare->enumField = io_read_array(io, 1u, sizeof(SZrIoEnumField));
                if (io->hasReadError) return;
                io_read_enum_fields(io, declare->enumField, 1, ZR_VALUE_TYPE_UINT64);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_FIELD: {
                declare->field = io_read_array(io, 1u, sizeof(SZrIoField));
                if (io->hasReadError) return;
                io_read_field(io, declare->field);
            } break;
            default:
                io->hasReadError = ZR_TRUE;
                return;
        }
        // check type and read
    }
}

static void io_read_classes(SZrIo *io, SZrIoClass *classes, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoClass *class = &classes[i];
        class->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, class->superClassLength, TZrSize);
        class->superClasses = io_read_array(io, class->superClassLength, sizeof(SZrIoReference));
        if (io->hasReadError) return;
        io_read_references(io, class->superClasses, class->superClassLength);
        ZR_IO_READ_NATIVE_TYPE(io, class->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, class->declaresLength, TZrSize);
        class->declares = io_read_array(io, class->declaresLength, sizeof(SZrIoMemberDeclare));
        if (io->hasReadError) return;
        io_read_member_declares(io, class->declares, class->declaresLength);
    }
}

static void io_read_structs(SZrIo *io, SZrIoStruct *structs, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoStruct *struct_ = &structs[i];
        struct_->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->superStructLength, TZrSize);
        struct_->superStructs = io_read_array(io, struct_->superStructLength, sizeof(SZrIoReference));
        if (io->hasReadError) return;
        io_read_references(io, struct_->superStructs, struct_->superStructLength);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->declaresLength, TZrSize);
        struct_->declares = io_read_array(io, struct_->declaresLength, sizeof(SZrIoMemberDeclare));
        if (io->hasReadError) return;
        io_read_member_declares(io, struct_->declares, struct_->declaresLength);
    }
}

static void io_read_interfaces(SZrIo *io, SZrIoInterface *interfaces, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoInterface *interface = &interfaces[i];
        interface->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, interface->superInterfaceLength, TZrSize);
        interface->superInterfaces =
                io_read_array(io, interface->superInterfaceLength, sizeof(SZrIoReference));
        if (io->hasReadError) return;
        io_read_references(io, interface->superInterfaces, interface->superInterfaceLength);
        ZR_IO_READ_NATIVE_TYPE(io, interface->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, interface->declaresLength, TZrSize);
        interface->declares = io_read_array(io, interface->declaresLength, sizeof(SZrIoMemberDeclare));
        if (io->hasReadError) return;
        io_read_member_declares(io, interface->declares, interface->declaresLength);
    }
}

static void io_read_enums(SZrIo *io, SZrIoEnum *enums, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoEnum *enum_ = &enums[i];
        enum_->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, enum_->valueType, EZrValueType);
        ZR_IO_READ_NATIVE_TYPE(io, enum_->fieldsLength, TZrSize);
        enum_->fields = io_read_array(io, enum_->fieldsLength, sizeof(*enum_->fields));
        if (io->hasReadError) return;
        io_read_enum_fields(io, enum_->fields, enum_->fieldsLength, enum_->valueType);
    }
}
/* 模块级声明使用 type 判别子树，后续释放器按相同分支回收。 */
static void io_read_module_declares(SZrIo *io, SZrIoModuleDeclare *declares, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoModuleDeclare *declare = &declares[i];
        ZR_IO_READ_NATIVE_TYPE(io, declare->type, EZrIoMemberDeclareType);
        // check type and read
        switch (declare->type) {
            case ZR_IO_MODULE_DECLARE_TYPE_CLASS: {
                declare->class_ = io_read_array(io, 1u, sizeof(SZrIoClass));
                if (io->hasReadError) return;
                io_read_classes(io, declare->class_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_FUNCTION: {
                declare->function = io_read_array(io, 1u, sizeof(SZrIoFunction));
                if (io->hasReadError) return;
                io_read_functions(io, declare->function, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_STRUCT: {
                declare->struct_ = io_read_array(io, 1u, sizeof(SZrIoStruct));
                if (io->hasReadError) return;
                io_read_structs(io, declare->struct_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_INTERFACE: {
                declare->interface_ = io_read_array(io, 1u, sizeof(SZrIoInterface));
                if (io->hasReadError) return;
                io_read_interfaces(io, declare->interface_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_ENUM: {
                declare->enum_ = io_read_array(io, 1u, sizeof(SZrIoEnum));
                if (io->hasReadError) return;
                io_read_enums(io, declare->enum_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_FIELD: {
                declare->field = io_read_array(io, 1u, sizeof(SZrIoField));
                if (io->hasReadError) return;
                io_read_field(io, declare->field);
            } break;
            default:
                io->hasReadError = ZR_TRUE;
                return;
        }
    }
}

/* 模块数组接在文件头之后；导入、声明、入口函数成为单一源树的子对象。 */
static void io_read_modules(SZrIo *io, SZrIoModule *modules, TZrSize count) {
    for (TZrSize i = 0; !io->hasReadError && (i < count); i++) {
        SZrIoModule *module = &modules[i];
        module->name = io_read_string_with_length(io);
        module->md5 = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, module->importsLength, TZrSize);
        module->imports = io_read_array(io, module->importsLength, sizeof(SZrIoImport));
        if (io->hasReadError) return;
        io_read_imports(io, module->imports, module->importsLength);
        ZR_IO_READ_NATIVE_TYPE(io, module->declaresLength, TZrSize);
        module->declares = io_read_array(io, module->declaresLength, sizeof(SZrIoModuleDeclare));
        if (io->hasReadError) return;
        io_read_module_declares(io, module->declares, module->declaresLength);
        module->entryFunction = io_read_array(io, 1u, sizeof(SZrIoFunction));
        if (io->hasReadError) return;
        io_read_functions(io, module->entryFunction, 1);
    }
}

SZrIo *ZrCore_Io_New(struct SZrGlobalState *global) {
    SZrIo *io = ZrCore_Memory_RawMallocWithType(global, sizeof(*io), ZR_MEMORY_NATIVE_TYPE_IO);
    if (io != ZR_NULL) {
        ZrCore_Memory_RawSet(io, 0, sizeof(*io));
        io->allocationBudgetRemaining = ZR_IO_MAX_DECODE_BYTES;
    }
    return io;
}

void ZrCore_Io_Free(struct SZrGlobalState *global, SZrIo *io) {
    if (io != ZR_NULL) {
        ZR_IO_FREE_NATIVE_DATA(global, io, sizeof(SZrIo));
    }
}

void ZrCore_Io_Init(SZrState *state, SZrIo *io, FZrIoRead read, FZrIoClose close, TZrPtr customData) {
    io->state = state;
    io->read = read;
    io->close = close;
    io->customData = customData;
    io->pointer = ZR_NULL;
    io->remained = 0;
    io->isBinary = ZR_FALSE;
    io->hasReadError = ZR_FALSE;
    io->sourceVersionPatch = 0;
    io->boundedBinaryInput = ZR_FALSE;
    io->allocationBudgetRemaining = ZR_IO_MAX_DECODE_BYTES;
    io->decodeDepth = 0;
}

/* A sticky error always initializes scalar destinations. A bounded binary
 * cursor never refills past the copied input, even if the callback has more data. */
TZrSize ZrCore_Io_Read(SZrIo *io, TZrBytePtr buffer, TZrSize size) {
    TZrSize requestedSize = size;
    if (size == 0) return 0;
    if (buffer == ZR_NULL) {
        if (io != ZR_NULL) io->hasReadError = ZR_TRUE;
        return 0;
    }
    if (io == ZR_NULL || io->hasReadError) {
        ZrCore_Memory_RawSet(buffer, 0, size);
        return 0;
    }
    while (size > 0) {
        TZrSize read;
        if (io->remained == 0 &&
            (io->boundedBinaryInput || io->read == ZR_NULL || !io_refill(io))) {
            ZrCore_Memory_RawSet(buffer, 0, size);
            io->hasReadError = ZR_TRUE;
            return requestedSize - size;
        }
        read = size <= io->remained ? size : io->remained;
        ZrCore_Memory_RawCopy(buffer, io->pointer, read);
        io->remained -= read;
        io->pointer += read;
        buffer += read;
        size -= read;
    }
    return requestedSize;
}

static void io_read_source(SZrIo *io, SZrIoSource *source) {
    ZrCore_Io_Read(io, (TZrBytePtr)source->signature, sizeof(source->signature));
    ZR_IO_READ_NATIVE_TYPE(io, source->versionMajor, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->versionMinor, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->versionPatch, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->format, TZrUInt64);
    ZR_IO_READ_NATIVE_TYPE(io, source->nativeIntSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeSizeSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeInstructionSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->isBigEndian, TZrBool);
    ZR_IO_READ_NATIVE_TYPE(io, source->isDebug, TZrBool);
    ZrCore_Io_Read(io, (TZrBytePtr)source->optional, sizeof(source->optional));
    io->sourceVersionPatch = source->versionPatch;
    if (!io->hasReadError &&
        memcmp(source->signature, ZR_IO_SOURCE_SIGNATURE, sizeof(source->signature)) == 0 &&
        source->versionMajor == ZR_VM_MAJOR_VERSION && source->versionMinor == ZR_VM_MINOR_VERSION &&
        source->versionPatch < ZR_IO_SOURCE_PATCH_MIN_SUPPORTED) {
        ZrCore_Log_Error(io->state,
                "unsupported .zro format patch %u (minimum %u): recompile from source for the current value/stack ABI\n",
                (unsigned)source->versionPatch, (unsigned)ZR_IO_SOURCE_PATCH_MIN_SUPPORTED);
        io->hasReadError = ZR_TRUE;
        return;
    }
    /* The historical isBigEndian field actually stores the writer's little-
     * endian flag. No native-width or byte-order conversion is implemented. */
    if (io->hasReadError ||
        memcmp(source->signature, ZR_IO_SOURCE_SIGNATURE, sizeof(source->signature)) != 0 ||
        source->versionMajor != ZR_VM_MAJOR_VERSION || source->versionMinor != ZR_VM_MINOR_VERSION ||
        source->versionPatch < ZR_IO_SOURCE_PATCH_MIN_SUPPORTED ||
        source->versionPatch > ZR_IO_SOURCE_PATCH_CURRENT ||
        source->format != (((TZrUInt64)source->versionMajor << ZR_IO_VERSION_FORMAT_SHIFT_BITS) | source->versionMinor) ||
        source->nativeIntSize != sizeof(TZrInt64) || source->typeSizeSize != sizeof(TZrSize) ||
        source->typeInstructionSize != sizeof(TZrInstruction) ||
        source->isBigEndian != (ZR_IO_IS_LITTLE_ENDIAN ? ZR_TRUE : ZR_FALSE) ||
        source->isDebug > ZR_TRUE || source->optional[0] != 0 ||
        source->optional[1] != 0 || source->optional[2] != 0) {
        io->hasReadError = ZR_TRUE;
        return;
    }
    io->sourceVersionPatch = source->versionPatch;
    ZR_IO_READ_NATIVE_TYPE(io, source->modulesLength, TZrSize);
    source->modules = io_read_array(io, source->modulesLength, sizeof(*source->modules));
    if (io->hasReadError) return;
    io_read_modules(io, source->modules, source->modulesLength);
}

/* Copy streaming input once so all subsequent count checks can use exact
 * remaining bytes. An endless input callback is bounded by the same limit. */
SZrIoSource *ZrCore_Io_ReadSourceNew(SZrIo *io) {
    SZrGlobalState *global;
    SZrIo bounded;
    SZrIoSource *source = ZR_NULL;
    TZrBytePtr bytes = ZR_NULL;
    TZrSize length = 0, capacity = 0;
    if (io == ZR_NULL || io->state == ZR_NULL || io->state->global == ZR_NULL ||
        io->read == ZR_NULL || io->hasReadError) return ZR_NULL;
    global = io->state->global;
    while (io->remained > 0 || io_refill(io)) {
        TZrSize required;
        if (io->remained > ZR_IO_MAX_BINARY_BYTES - length) {
            io->hasReadError = ZR_TRUE;
            break;
        }
        required = length + io->remained;
        if (required > capacity) {
            TZrSize newCapacity = capacity == 0 ? 4096u : capacity;
            TZrBytePtr grown;
            while (newCapacity < required) newCapacity *= 2u;
            grown = ZrCore_Memory_Allocate(global, bytes, capacity, newCapacity, ZR_MEMORY_NATIVE_TYPE_IO);
            if (grown == ZR_NULL) {
                io->hasReadError = ZR_TRUE;
                break;
            }
            bytes = grown;
            capacity = newCapacity;
        }
        ZrCore_Memory_RawCopy(bytes + length, io->pointer, io->remained);
        length = required;
        io->pointer += io->remained;
        io->remained = 0;
    }
    if (!io->hasReadError && length > 0) {
        source = ZrCore_Memory_RawMallocWithType(global, sizeof(*source), ZR_MEMORY_NATIVE_TYPE_IO);
        if (source != ZR_NULL) {
            ZrCore_Memory_RawSet(source, 0, sizeof(*source));
            bounded = *io;
            bounded.pointer = bytes;
            bounded.remained = length;
            bounded.boundedBinaryInput = ZR_TRUE;
            bounded.allocationBudgetRemaining = ZR_IO_MAX_DECODE_BYTES;
            bounded.decodeDepth = 0;
            io_read_source(&bounded, source);
            io->sourceVersionPatch = bounded.sourceVersionPatch;
            io->allocationBudgetRemaining = bounded.allocationBudgetRemaining;
            io->hasReadError = (TZrBool)(bounded.hasReadError || bounded.remained != 0);
        } else {
            io->hasReadError = ZR_TRUE;
        }
    } else {
        io->hasReadError = ZR_TRUE;
    }
    if (bytes != ZR_NULL) ZR_IO_FREE_NATIVE_DATA(global, bytes, capacity);
    if (io->hasReadError) {
        ZrCore_Io_ReadSourceFree(global, source);
        return ZR_NULL;
    }
    return source;
}

/* 兼容由全局加载器填充的路径：此函数拥有游标与 close，返回源树由调用方持有。 */
/* TODO: sourceLoader 若通过 Throw 离开，当前调用不会执行 close/Io_Free；需核查宿主回调是否允许跨越此边界。 */
SZrIoSource *ZrCore_Io_LoadSource(struct SZrState *state, TZrNativeString sourceName, TZrNativeString md5) {
    SZrGlobalState *global = state->global;
    SZrIo *io = ZrCore_Io_New(global);
    if (io == ZR_NULL) return ZR_NULL;
    TZrBool success = global->sourceLoader(state, sourceName, md5, io);
    if (!success) {
        ZrCore_Io_Free(global, io);
        return ZR_NULL;
    }
    SZrIoSource *source = ZrCore_Io_ReadSourceNew(io);
    if (io->close != ZR_NULL) {
        io->close(state, io->customData);
    }
    ZrCore_Io_Free(global, io);
    return source;
}
