#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

TZrBool compiler_build_function_metadata_tokens(SZrCompilerState *cs, SZrFunction *function);

#define TEST_IMPORT_TARGET_METADATA_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u)
#define TEST_IMPORT_TARGET_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 8u)
#define TEST_IMPORT_TARGET_SIGNATURE_HASH ((TZrUInt64)0x1122334455667788ULL)
#define TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH ((TZrUInt64)0x8877665544332211ULL)
#define TEST_BINDING_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 9u)
#define TEST_BINDING_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 10u)
#define TEST_BINDING_REF_SIGNATURE_HASH ((TZrUInt64)0x0102030405060708ULL)
#define TEST_BINDING_EXPECTED_METADATA_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 11u)
#define TEST_BINDING_EXPECTED_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 12u)
#define TEST_BINDING_EXPECTED_SIGNATURE_HASH ((TZrUInt64)0x1112131415161718ULL)
#define TEST_BINDING_EXPECTED_MODULE_SIGNATURE_HASH ((TZrUInt64)0x2122232425262728ULL)
#define TEST_BINDING_EXPECTED_LAYOUT_VERSION 3u
#define TEST_BINDING_EXPECTED_LAYOUT_HASH ((TZrUInt64)0x5152535455565758ULL)
#define TEST_BINDING_RESOLVED_METADATA_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 13u)
#define TEST_BINDING_RESOLVED_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 14u)
#define TEST_BINDING_RESOLVED_SIGNATURE_HASH ((TZrUInt64)0x3132333435363738ULL)
#define TEST_BINDING_RESOLVED_MODULE_SIGNATURE_HASH ((TZrUInt64)0x4142434445464748ULL)
#define TEST_BINDING_RESOLVED_LAYOUT_VERSION 4u
#define TEST_BINDING_RESOLVED_LAYOUT_HASH ((TZrUInt64)0x6162636465666768ULL)

typedef struct SZrBinaryFixtureReader {
    TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrBinaryFixtureReader;

typedef struct SZrReadSourceTryContext {
    SZrIo *io;
    SZrIoSource *source;
} SZrReadSourceTryContext;

static TZrByte *read_binary_file_owned(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }
    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)fileSize;
    }
    return buffer;
}

static TZrBytePtr binary_fixture_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrBinaryFixtureReader *reader = (SZrBinaryFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed || reader->bytes == ZR_NULL) {
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return reader->bytes;
}

static void binary_fixture_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static void init_base_type_ref(SZrFunctionTypedTypeRef *typeRef, EZrValueType baseType) {
    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = baseType;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
}

static void init_int_type_ref(SZrFunctionTypedTypeRef *typeRef) {
    init_base_type_ref(typeRef, ZR_VALUE_TYPE_INT64);
}

static void init_named_object_type_ref(SZrState *state, SZrFunctionTypedTypeRef *typeRef, const char *typeName) {
    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->typeName = ZrCore_String_CreateFromNative(state, (TZrNativeString)typeName);
    TEST_ASSERT_NOT_NULL(typeRef->typeName);
}

static TZrUInt32 read_u32_le(const TZrByte *buffer, TZrSize offset) {
    return ((TZrUInt32)buffer[offset]) |
           ((TZrUInt32)buffer[offset + 1] << 8) |
           ((TZrUInt32)buffer[offset + 2] << 16) |
           ((TZrUInt32)buffer[offset + 3] << 24);
}

static void write_u32_le(TZrByte *buffer, TZrSize offset, TZrUInt32 value) {
    buffer[offset] = (TZrByte)(value & 0xFFu);
    buffer[offset + 1] = (TZrByte)((value >> 8) & 0xFFu);
    buffer[offset + 2] = (TZrByte)((value >> 16) & 0xFFu);
    buffer[offset + 3] = (TZrByte)((value >> 24) & 0xFFu);
}

static const TZrChar *current_exception_message(SZrState *state) {
    SZrObject *errorObject;
    SZrString *messageName;
    SZrTypeValue key;
    const SZrTypeValue *messageValue;

    if (state == ZR_NULL || !state->hasCurrentException ||
        state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        state->currentException.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    errorObject = ZR_CAST_OBJECT(state, state->currentException.value.object);
    messageName = ZrCore_String_CreateFromNative(state, "message");
    if (errorObject == ZR_NULL || messageName == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(messageName));
    key.type = ZR_VALUE_TYPE_STRING;
    messageValue = ZrCore_Object_GetValue(state, errorObject, &key);
    if (messageValue == ZR_NULL ||
        messageValue->type != ZR_VALUE_TYPE_STRING ||
        messageValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, messageValue->value.object));
}

static SZrString *metadata_string_heap_lookup(const SZrFunction *function, TZrUInt32 stringIndex) {
    if (function == ZR_NULL || function->metadataStringHeap == ZR_NULL || stringIndex == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataStringHeapLength; index++) {
        if (function->metadataStringHeap[index].stringIndex == stringIndex) {
            return function->metadataStringHeap[index].value;
        }
    }

    return ZR_NULL;
}

static void assert_metadata_heap_string_ref(const SZrFunction *function,
                                            const TZrByte *blob,
                                            TZrSize *cursor,
                                            const char *expected) {
    TZrUInt32 stringIndex;
    SZrString *value;

    TEST_ASSERT_NOT_NULL(blob);
    TEST_ASSERT_NOT_NULL(cursor);
    stringIndex = read_u32_le(blob, *cursor);
    *cursor += sizeof(TZrUInt32);
    value = metadata_string_heap_lookup(function, stringIndex);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING(expected, ZrCore_String_GetNativeString(value));
}

static void read_source_try_body(SZrState *state, TZrPtr arguments) {
    SZrReadSourceTryContext *context = (SZrReadSourceTryContext *)arguments;

    ZR_UNUSED_PARAMETER(state);

    if (context != ZR_NULL) {
        context->source = ZrCore_Io_ReadSourceNew(context->io);
    }
}

static SZrFunction *create_metadata_token_fixture_with_return_type_and_version(SZrState *state,
                                                                               EZrValueType returnBaseType,
                                                                               const TZrChar *moduleVersion) {
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *symbol;
    SZrCompilerState compilerState;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->functionName = ZrCore_String_Create(state, "__entry", strlen("__entry"));
    if (moduleVersion != ZR_NULL) {
        function->moduleVersion = ZrCore_String_CreateFromNative(state, (TZrNativeString)moduleVersion);
    }
    function->typedExportedSymbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->functionName == ZR_NULL ||
        (moduleVersion != ZR_NULL && function->moduleVersion == ZR_NULL) ||
        function->typedExportedSymbols == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(function->typedExportedSymbols, 0, sizeof(SZrFunctionTypedExportSymbol));
    function->typedExportedSymbolLength = 1;
    symbol = &function->typedExportedSymbols[0];
    symbol->name = ZrCore_String_Create(state, "sum", strlen("sum"));
    symbol->stackSlot = 0;
    symbol->accessModifier = ZR_ACCESS_PUBLIC;
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    symbol->parameterCount = 2;
    symbol->parameterTypes = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedTypeRef) * symbol->parameterCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (symbol->name == ZR_NULL || symbol->parameterTypes == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    init_base_type_ref(&symbol->valueType, returnBaseType);
    init_int_type_ref(&symbol->parameterTypes[0]);
    init_int_type_ref(&symbol->parameterTypes[1]);

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    if (!compiler_build_function_metadata_tokens(&compilerState, function)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    return function;
}

static SZrFunction *create_metadata_token_fixture_with_return_type(SZrState *state, EZrValueType returnBaseType) {
    return create_metadata_token_fixture_with_return_type_and_version(state, returnBaseType, "1.0.0");
}

static SZrFunction *create_metadata_token_fixture(SZrState *state) {
    return create_metadata_token_fixture_with_return_type(state, ZR_VALUE_TYPE_INT64);
}

static SZrFunction *create_metadata_token_fixture_with_version(SZrState *state, const TZrChar *moduleVersion) {
    return create_metadata_token_fixture_with_return_type_and_version(state, ZR_VALUE_TYPE_INT64, moduleVersion);
}

static TZrBool add_test_module_metadata_binding(SZrState *state, SZrFunction *function) {
    SZrMetadataTokenBinding *binding;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = (SZrMetadataTokenBinding *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                        sizeof(SZrMetadataTokenBinding),
                                                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
    binding->refToken = TEST_BINDING_REF_TOKEN;
    binding->refSignatureToken = TEST_BINDING_REF_SIGNATURE_TOKEN;
    binding->refSignatureHash = TEST_BINDING_REF_SIGNATURE_HASH;
    binding->expectedMetadataToken = TEST_BINDING_EXPECTED_METADATA_TOKEN;
    binding->expectedSignatureToken = TEST_BINDING_EXPECTED_SIGNATURE_TOKEN;
    binding->expectedSignatureHash = TEST_BINDING_EXPECTED_SIGNATURE_HASH;
    binding->expectedModuleSignatureHash = TEST_BINDING_EXPECTED_MODULE_SIGNATURE_HASH;
    binding->expectedLayoutVersion = TEST_BINDING_EXPECTED_LAYOUT_VERSION;
    binding->expectedLayoutHash = TEST_BINDING_EXPECTED_LAYOUT_HASH;
    binding->resolvedMetadataToken = TEST_BINDING_RESOLVED_METADATA_TOKEN;
    binding->resolvedSignatureToken = TEST_BINDING_RESOLVED_SIGNATURE_TOKEN;
    binding->resolvedSignatureHash = TEST_BINDING_RESOLVED_SIGNATURE_HASH;
    binding->resolvedModuleSignatureHash = TEST_BINDING_RESOLVED_MODULE_SIGNATURE_HASH;
    binding->resolvedLayoutVersion = TEST_BINDING_RESOLVED_LAYOUT_VERSION;
    binding->resolvedLayoutHash = TEST_BINDING_RESOLVED_LAYOUT_HASH;

    function->moduleMetadataBindings = binding;
    function->moduleMetadataBindingLength = 1u;
    function->moduleMetadataBindingCapacity = 1u;
    return ZR_TRUE;
}

static SZrFunction *create_union_metadata_token_fixture(SZrState *state, SZrAstNode *scriptAst) {
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *symbol;
    SZrCompilerState compilerState;

    if (state == ZR_NULL || state->global == ZR_NULL || scriptAst == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->functionName = ZrCore_String_Create(state, "__entry", strlen("__entry"));
    function->typedExportedSymbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->functionName == ZR_NULL || function->typedExportedSymbols == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(function->typedExportedSymbols, 0, sizeof(SZrFunctionTypedExportSymbol));
    function->typedExportedSymbolLength = 1;
    symbol = &function->typedExportedSymbols[0];
    symbol->name = ZrCore_String_Create(state, "choose", strlen("choose"));
    symbol->stackSlot = 0;
    symbol->accessModifier = ZR_ACCESS_PUBLIC;
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    symbol->parameterCount = 0;
    symbol->parameterTypes = ZR_NULL;
    if (symbol->name == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }
    init_named_object_type_ref(state, &symbol->valueType, "Option<int>");

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    compilerState.scriptAst = scriptAst;
    if (!compiler_build_function_metadata_tokens(&compilerState, function)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    return function;
}

static SZrFunction *create_named_signature_metadata_token_fixture(SZrState *state,
                                                                  const char *returnTypeName,
                                                                  const char *parameterTypeName) {
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *symbol;
    SZrCompilerState compilerState;

    if (state == ZR_NULL || state->global == ZR_NULL || returnTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->functionName = ZrCore_String_Create(state, "__entry", strlen("__entry"));
    function->typedExportedSymbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->functionName == ZR_NULL || function->typedExportedSymbols == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(function->typedExportedSymbols, 0, sizeof(SZrFunctionTypedExportSymbol));
    function->typedExportedSymbolLength = 1;
    symbol = &function->typedExportedSymbols[0];
    symbol->name = ZrCore_String_Create(state, "make", strlen("make"));
    symbol->stackSlot = 0;
    symbol->accessModifier = ZR_ACCESS_PUBLIC;
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    symbol->parameterCount = parameterTypeName != ZR_NULL ? 1u : 0u;
    if (symbol->parameterCount > 0) {
        symbol->parameterTypes = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionTypedTypeRef),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (symbol->parameterTypes == ZR_NULL) {
            ZrCore_Function_Free(state, function);
            return ZR_NULL;
        }
        init_named_object_type_ref(state, &symbol->parameterTypes[0], parameterTypeName);
    }
    if (symbol->name == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }
    init_named_object_type_ref(state, &symbol->valueType, returnTypeName);

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    if (!compiler_build_function_metadata_tokens(&compilerState, function)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    return function;
}

static SZrFunction *create_named_return_metadata_token_fixture(SZrState *state, const char *returnTypeName) {
    return create_named_signature_metadata_token_fixture(state, returnTypeName, ZR_NULL);
}

static SZrFunction *create_import_member_ref_metadata_token_fixture(SZrState *state) {
    SZrFunction *function;
    SZrFunctionModuleEffect *effect;
    SZrCompilerState compilerState;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->functionName = ZrCore_String_Create(state, "__entry", strlen("__entry"));
    function->moduleEntryEffects = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionModuleEffect),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->functionName == ZR_NULL || function->moduleEntryEffects == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(function->moduleEntryEffects, 0, sizeof(SZrFunctionModuleEffect));
    function->moduleEntryEffectLength = 1;
    effect = &function->moduleEntryEffects[0];
    effect->kind = ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
    effect->moduleName = ZrCore_String_CreateFromNative(state, "math");
    effect->symbolName = ZrCore_String_CreateFromNative(state, "add");
    effect->targetMetadataToken = TEST_IMPORT_TARGET_METADATA_TOKEN;
    effect->targetSignatureToken = TEST_IMPORT_TARGET_SIGNATURE_TOKEN;
    effect->targetSignatureHash = TEST_IMPORT_TARGET_SIGNATURE_HASH;
    effect->targetModuleSignatureHash = TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH;
    if (effect->moduleName == ZR_NULL || effect->symbolName == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    if (!compiler_build_function_metadata_tokens(&compilerState, function)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    return function;
}

static void init_import_call_effect(SZrState *state,
                                    SZrFunctionModuleEffect *effect,
                                    const char *moduleName,
                                    const char *symbolName) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(effect);
    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_NOT_NULL(symbolName);

    ZrCore_Memory_RawSet(effect, 0, sizeof(*effect));
    effect->kind = ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
    effect->moduleName = ZrCore_String_CreateFromNative(state, (TZrNativeString)moduleName);
    effect->symbolName = ZrCore_String_CreateFromNative(state, (TZrNativeString)symbolName);
    effect->targetMetadataToken = TEST_IMPORT_TARGET_METADATA_TOKEN;
    effect->targetSignatureToken = TEST_IMPORT_TARGET_SIGNATURE_TOKEN;
    effect->targetSignatureHash = TEST_IMPORT_TARGET_SIGNATURE_HASH;
    effect->targetModuleSignatureHash = TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH;
    TEST_ASSERT_NOT_NULL(effect->moduleName);
    TEST_ASSERT_NOT_NULL(effect->symbolName);
}

static SZrFunction *create_module_ref_aggregation_fixture(SZrState *state) {
    SZrFunction *function;
    SZrFunctionCallableSummary *summary;
    SZrCompilerState compilerState;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->functionName = ZrCore_String_Create(state, "__entry", strlen("__entry"));
    function->moduleEntryEffects = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionModuleEffect),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->exportedCallableSummaries = (SZrFunctionCallableSummary *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionCallableSummary),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->functionName == ZR_NULL ||
        function->moduleEntryEffects == ZR_NULL ||
        function->exportedCallableSummaries == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    function->moduleEntryEffectLength = 1;
    init_import_call_effect(state, &function->moduleEntryEffects[0], "math", "add");

    function->exportedCallableSummaryLength = 1;
    summary = &function->exportedCallableSummaries[0];
    ZrCore_Memory_RawSet(summary, 0, sizeof(*summary));
    summary->name = ZrCore_String_CreateFromNative(state, "run");
    summary->callableChildIndex = 0;
    summary->effectCount = 1;
    summary->effects = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionModuleEffect),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (summary->name == ZR_NULL || summary->effects == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }
    init_import_call_effect(state, &summary->effects[0], "math", "add");

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    if (!compiler_build_function_metadata_tokens(&compilerState, function)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    return function;
}

static TZrBool string_equals(const SZrString *value, const char *expected) {
    const char *actual;

    if (value == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }

    actual = ZrCore_String_GetNativeString((SZrString *)value);
    return actual != ZR_NULL && strcmp(actual, expected) == 0;
}

static const SZrFunctionTypedExportSymbol *find_runtime_export_symbol(const SZrFunction *function, const char *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedExportedSymbolLength; ++index) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        if (string_equals(symbol->name, name)) {
            return symbol;
        }
    }

    return ZR_NULL;
}

static const SZrIoFunctionTypedExportSymbol *find_io_export_symbol(const SZrIoFunction *function, const char *name) {
    TZrSize index;

    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedExportedSymbolsLength; ++index) {
        const SZrIoFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        if (string_equals(symbol->name, name)) {
            return symbol;
        }
    }

    return ZR_NULL;
}

static const SZrMetadataTokenRecord *find_token_record(const SZrMetadataTokenRecord *records,
                                                       TZrSize count,
                                                       TZrMetadataToken token) {
    TZrSize index;

    if (records == ZR_NULL || token == 0) {
        return ZR_NULL;
    }

    for (index = 0; index < count; ++index) {
        if (records[index].token == token) {
            return &records[index];
        }
    }

    return ZR_NULL;
}

static const SZrMetadataTokenRecord *find_first_token_record_by_table(const SZrMetadataTokenRecord *records,
                                                                      TZrSize count,
                                                                      EZrMetadataTableTag table) {
    TZrSize index;

    if (records == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < count; ++index) {
        if (ZR_METADATA_TOKEN_TABLE(records[index].token) == (TZrUInt32)table) {
            return &records[index];
        }
    }

    return ZR_NULL;
}

static TZrUInt32 count_token_records_by_table(const SZrMetadataTokenRecord *records,
                                              TZrSize count,
                                              EZrMetadataTableTag table) {
    TZrUInt32 found = 0;
    TZrSize index;

    if (records == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < count; ++index) {
        if (ZR_METADATA_TOKEN_TABLE(records[index].token) == (TZrUInt32)table) {
            found++;
        }
    }

    return found;
}

static void assert_module_ref_table_aggregates_import_refs_runtime(const SZrFunction *function) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->moduleMetadataTokenRecords);
    TEST_ASSERT_EQUAL_UINT32(6u, function->moduleMetadataTokenRecordLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_token_records_by_table(function->moduleMetadataTokenRecords,
                                                          function->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_ASSEMBLY_REF));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_token_records_by_table(function->moduleMetadataTokenRecords,
                                                          function->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_TYPE_REF));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_token_records_by_table(function->moduleMetadataTokenRecords,
                                                          function->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_MEMBER_REF));
    TEST_ASSERT_EQUAL_UINT32(3u,
                             count_token_records_by_table(function->moduleMetadataTokenRecords,
                                                          function->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_SIGNATURE));
}

static void assert_module_ref_table_aggregates_import_refs_io(const SZrIoFunction *function) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->moduleMetadataTokenRecords);
    TEST_ASSERT_EQUAL_UINT32(6u, (TZrUInt32)function->moduleMetadataTokenRecordLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_token_records_by_table(function->moduleMetadataTokenRecords,
                                                          function->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_ASSEMBLY_REF));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_token_records_by_table(function->moduleMetadataTokenRecords,
                                                          function->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_TYPE_REF));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_token_records_by_table(function->moduleMetadataTokenRecords,
                                                          function->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_MEMBER_REF));
    TEST_ASSERT_EQUAL_UINT32(3u,
                             count_token_records_by_table(function->moduleMetadataTokenRecords,
                                                          function->moduleMetadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_SIGNATURE));
}

static void assert_runtime_token_model(const SZrFunction *function) {
    const SZrFunctionTypedExportSymbol *symbol;
    const SZrMetadataTokenRecord *moduleRecord;
    const SZrMetadataTokenRecord *moduleSignatureRecord;
    const SZrMetadataTokenRecord *memberRecord;
    const SZrMetadataTokenRecord *signatureRecord;
    const TZrByte *moduleBlob;
    TZrSize moduleCursor = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);
    TEST_ASSERT_TRUE(function->metadataTokenRecordLength >= 2);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);
    TEST_ASSERT_TRUE(function->signatureBlobHeapLength > 0);

    moduleRecord = find_token_record(function->metadataTokenRecords,
                                     function->metadataTokenRecordLength,
                                     ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u));
    TEST_ASSERT_NOT_NULL(moduleRecord);
    TEST_ASSERT_EQUAL_UINT32(0u, moduleRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_SIGNATURE,
                             ZR_METADATA_TOKEN_TABLE(moduleRecord->relatedToken));
    TEST_ASSERT_TRUE(moduleRecord->signatureBlobLength > 0u);
    TEST_ASSERT_TRUE(moduleRecord->signatureBlobOffset + moduleRecord->signatureBlobLength <=
                     function->signatureBlobHeapLength);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, moduleRecord->signatureHash);

    moduleSignatureRecord = find_token_record(function->metadataTokenRecords,
                                              function->metadataTokenRecordLength,
                                              moduleRecord->relatedToken);
    TEST_ASSERT_NOT_NULL(moduleSignatureRecord);
    TEST_ASSERT_EQUAL_UINT32(moduleRecord->token, moduleSignatureRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(moduleRecord->token, moduleSignatureRecord->relatedToken);
    TEST_ASSERT_EQUAL_UINT32(moduleRecord->signatureBlobOffset, moduleSignatureRecord->signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(moduleRecord->signatureBlobLength, moduleSignatureRecord->signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT64(moduleRecord->signatureHash, moduleSignatureRecord->signatureHash);

    moduleBlob = function->signatureBlobHeap + moduleRecord->signatureBlobOffset;
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_MODULE, moduleBlob[moduleCursor]);
    moduleCursor += 1;
    assert_metadata_heap_string_ref(function, moduleBlob, &moduleCursor, "__entry");
    assert_metadata_heap_string_ref(function, moduleBlob, &moduleCursor, "1.0.0");
    TEST_ASSERT_EQUAL_UINT32(moduleRecord->signatureBlobLength, moduleCursor);

    symbol = find_runtime_export_symbol(function, "sum");
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_MEMBER_DEF, ZR_METADATA_TOKEN_TABLE(symbol->metadataToken));
    TEST_ASSERT_EQUAL_UINT32(1u, ZR_METADATA_TOKEN_RID(symbol->metadataToken));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_SIGNATURE, ZR_METADATA_TOKEN_TABLE(symbol->signatureToken));
    TEST_ASSERT_EQUAL_UINT32(1u, ZR_METADATA_TOKEN_RID(symbol->signatureToken));
    TEST_ASSERT_TRUE(symbol->signatureBlobLength > 0);
    TEST_ASSERT_TRUE(symbol->signatureBlobOffset + symbol->signatureBlobLength <= function->signatureBlobHeapLength);
    TEST_ASSERT_TRUE(symbol->signatureHash != 0u);
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_METHOD_SIG,
                            function->signatureBlobHeap[symbol->signatureBlobOffset]);

    memberRecord = find_token_record(function->metadataTokenRecords,
                                     function->metadataTokenRecordLength,
                                     symbol->metadataToken);
    signatureRecord = find_token_record(function->metadataTokenRecords,
                                        function->metadataTokenRecordLength,
                                        symbol->signatureToken);
    TEST_ASSERT_NOT_NULL(memberRecord);
    TEST_ASSERT_NOT_NULL(signatureRecord);
    TEST_ASSERT_EQUAL_UINT32(symbol->signatureToken, memberRecord->relatedToken);
    TEST_ASSERT_EQUAL_UINT32(symbol->signatureBlobOffset, memberRecord->signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(symbol->signatureBlobLength, memberRecord->signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT32(symbol->signatureBlobOffset, signatureRecord->signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(symbol->signatureBlobLength, signatureRecord->signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT64(symbol->signatureHash, memberRecord->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(symbol->signatureHash, signatureRecord->signatureHash);
}

static void assert_union_signature_model(const SZrFunction *function) {
    const SZrFunctionTypedExportSymbol *symbol;
    const TZrByte *blob;
    TZrSize cursor;
    TZrUInt32 argumentCount;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);
    symbol = find_runtime_export_symbol(function, "choose");
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_TRUE(symbol->signatureBlobLength > 0);
    TEST_ASSERT_TRUE(symbol->signatureBlobOffset + symbol->signatureBlobLength <= function->signatureBlobHeapLength);
    TEST_ASSERT_TRUE(symbol->signatureHash != 0u);

    blob = function->signatureBlobHeap + symbol->signatureBlobOffset;
    cursor = 0;
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_METHOD_SIG, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT8(1u, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT8(0u, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(0u, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);

    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_UNION, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    assert_metadata_heap_string_ref(function, blob, &cursor, "Option");

    argumentCount = read_u32_le(blob, cursor);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT32(1u, argumentCount);
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, read_u32_le(blob, cursor));
}

static void assert_union_type_spec_token_model(const SZrFunction *function) {
    const SZrMetadataTokenRecord *typeSpecRecord = ZR_NULL;
    const SZrMetadataTokenRecord *signatureRecord;
    const TZrByte *blob;
    TZrSize cursor;
    TZrUInt32 argumentCount;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_TYPE_SPEC) {
            typeSpecRecord = record;
            break;
        }
    }

    TEST_ASSERT_NOT_NULL(typeSpecRecord);
    TEST_ASSERT_EQUAL_UINT32(0u, typeSpecRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_SIGNATURE,
                             ZR_METADATA_TOKEN_TABLE(typeSpecRecord->relatedToken));
    TEST_ASSERT_TRUE(typeSpecRecord->signatureBlobLength > 0u);
    TEST_ASSERT_TRUE(typeSpecRecord->signatureBlobOffset + typeSpecRecord->signatureBlobLength <=
                     function->signatureBlobHeapLength);
    TEST_ASSERT_TRUE(typeSpecRecord->signatureHash != 0u);

    signatureRecord = find_token_record(function->metadataTokenRecords,
                                        function->metadataTokenRecordLength,
                                        typeSpecRecord->relatedToken);
    TEST_ASSERT_NOT_NULL(signatureRecord);
    TEST_ASSERT_EQUAL_UINT32(typeSpecRecord->token, signatureRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(typeSpecRecord->signatureBlobOffset, signatureRecord->signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(typeSpecRecord->signatureBlobLength, signatureRecord->signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT64(typeSpecRecord->signatureHash, signatureRecord->signatureHash);

    blob = function->signatureBlobHeap + typeSpecRecord->signatureBlobOffset;
    cursor = 0;
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_UNION, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    assert_metadata_heap_string_ref(function, blob, &cursor, "Option");
    argumentCount = read_u32_le(blob, cursor);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT32(1u, argumentCount);
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, read_u32_le(blob, cursor));
}

static void assert_union_type_def_token_model(const SZrFunction *function) {
    const SZrMetadataTokenRecord *typeDefRecord;
    const SZrMetadataTokenRecord *signatureRecord;
    const TZrByte *blob;
    TZrSize cursor;
    TZrUInt32 variantCount;
    TZrUInt32 fieldCount;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);

    typeDefRecord = find_first_token_record_by_table(function->metadataTokenRecords,
                                                     function->metadataTokenRecordLength,
                                                     ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(typeDefRecord);
    TEST_ASSERT_EQUAL_UINT32(1u, ZR_METADATA_TOKEN_RID(typeDefRecord->token));
    TEST_ASSERT_EQUAL_UINT32(0u, typeDefRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_SIGNATURE,
                             ZR_METADATA_TOKEN_TABLE(typeDefRecord->relatedToken));
    TEST_ASSERT_TRUE(typeDefRecord->signatureBlobLength > 0u);
    TEST_ASSERT_TRUE(typeDefRecord->signatureBlobOffset + typeDefRecord->signatureBlobLength <=
                     function->signatureBlobHeapLength);
    TEST_ASSERT_TRUE(typeDefRecord->signatureHash != 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, typeDefRecord->layoutVersion);
    TEST_ASSERT_TRUE(typeDefRecord->layoutHash != 0u);

    signatureRecord = find_token_record(function->metadataTokenRecords,
                                        function->metadataTokenRecordLength,
                                        typeDefRecord->relatedToken);
    TEST_ASSERT_NOT_NULL(signatureRecord);
    TEST_ASSERT_EQUAL_UINT32(typeDefRecord->token, signatureRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(typeDefRecord->token, signatureRecord->relatedToken);
    TEST_ASSERT_EQUAL_UINT32(typeDefRecord->signatureBlobOffset, signatureRecord->signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(typeDefRecord->signatureBlobLength, signatureRecord->signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT64(typeDefRecord->signatureHash, signatureRecord->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(typeDefRecord->layoutVersion, signatureRecord->layoutVersion);
    TEST_ASSERT_EQUAL_UINT64(typeDefRecord->layoutHash, signatureRecord->layoutHash);

    blob = function->signatureBlobHeap + typeDefRecord->signatureBlobOffset;
    cursor = 0;
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_TYPE_DEF, blob[cursor]);
    cursor += 1;
    assert_metadata_heap_string_ref(function, blob, &cursor, "Option");
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);

    TEST_ASSERT_TRUE(cursor + sizeof(TZrUInt32) <= typeDefRecord->signatureBlobLength);
    variantCount = read_u32_le(blob, cursor);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT32(2u, variantCount);

    assert_metadata_heap_string_ref(function, blob, &cursor, "None");
    TEST_ASSERT_EQUAL_UINT32(ZR_UNION_VARIANT_UNIT, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT32(0u, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    fieldCount = read_u32_le(blob, cursor);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT32(0u, fieldCount);

    assert_metadata_heap_string_ref(function, blob, &cursor, "Some");
    TEST_ASSERT_EQUAL_UINT32(ZR_UNION_VARIANT_TUPLE, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT32(0u, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    fieldCount = read_u32_le(blob, cursor);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT32(1u, fieldCount);

    assert_metadata_heap_string_ref(function, blob, &cursor, "value");
    TEST_ASSERT_EQUAL_UINT32(ZR_PARAMETER_PASSING_MODE_VALUE, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_TYPE_REF, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    assert_metadata_heap_string_ref(function, blob, &cursor, "T");
    TEST_ASSERT_EQUAL_UINT32(typeDefRecord->signatureBlobLength, cursor);
}

static void assert_union_type_def_layout_identity_preserved(const SZrMetadataTokenRecord *expectedRecords,
                                                            TZrUInt32 expectedRecordCount,
                                                            const SZrMetadataTokenRecord *actualRecords,
                                                            TZrUInt32 actualRecordCount) {
    const SZrMetadataTokenRecord *expectedTypeDef;
    const SZrMetadataTokenRecord *actualTypeDef;

    expectedTypeDef = find_first_token_record_by_table(expectedRecords,
                                                       expectedRecordCount,
                                                       ZR_METADATA_TABLE_TYPE_DEF);
    actualTypeDef = find_first_token_record_by_table(actualRecords,
                                                     actualRecordCount,
                                                     ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(expectedTypeDef);
    TEST_ASSERT_NOT_NULL(actualTypeDef);
    TEST_ASSERT_EQUAL_UINT32(1u, expectedTypeDef->layoutVersion);
    TEST_ASSERT_TRUE(expectedTypeDef->layoutHash != 0u);
    TEST_ASSERT_EQUAL_UINT32(expectedTypeDef->layoutVersion, actualTypeDef->layoutVersion);
    TEST_ASSERT_EQUAL_UINT64(expectedTypeDef->layoutHash, actualTypeDef->layoutHash);
}

static void assert_generic_type_spec_token_model(const SZrFunction *function) {
    const SZrMetadataTokenRecord *typeSpecRecord = ZR_NULL;
    const TZrByte *blob;
    TZrSize cursor;
    TZrUInt32 argumentCount;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_TYPE_SPEC) {
            typeSpecRecord = record;
            break;
        }
    }

    TEST_ASSERT_NOT_NULL(typeSpecRecord);
    TEST_ASSERT_TRUE(typeSpecRecord->signatureBlobLength > 0u);
    TEST_ASSERT_TRUE(typeSpecRecord->signatureBlobOffset + typeSpecRecord->signatureBlobLength <=
                     function->signatureBlobHeapLength);

    blob = function->signatureBlobHeap + typeSpecRecord->signatureBlobOffset;
    cursor = 0;
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_TYPE_REF, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    assert_metadata_heap_string_ref(function, blob, &cursor, "Box");
    argumentCount = read_u32_le(blob, cursor);
    cursor += sizeof(TZrUInt32);
    TEST_ASSERT_EQUAL_UINT32(1u, argumentCount);
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, read_u32_le(blob, cursor));
}

static void assert_single_type_spec_record_pair(const SZrFunction *function) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_token_records_by_table(function->metadataTokenRecords,
                                                          function->metadataTokenRecordLength,
                                                          ZR_METADATA_TABLE_TYPE_SPEC));
}

static TZrUInt32 count_string_heap_entries_for_text(const SZrMetadataStringHeapEntry *strings,
                                                    TZrUInt32 stringCount,
                                                    const char *text) {
    TZrUInt32 count = 0;

    if (strings == ZR_NULL || text == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < stringCount; index++) {
        const TZrChar *entryText =
                strings[index].value != ZR_NULL ? ZrCore_String_GetNativeString(strings[index].value) : ZR_NULL;

        if (entryText != ZR_NULL && strcmp(entryText, text) == 0) {
            count++;
        }
    }

    return count;
}

static void assert_shared_metadata_string_heap_model(const SZrFunction *function) {
    const SZrMetadataTokenRecord *typeSpecRecord;
    const TZrByte *blob;
    TZrSize cursor;
    TZrUInt32 boxStringIndex;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap);
    TEST_ASSERT_TRUE(function->metadataStringHeapLength > 0u);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_string_heap_entries_for_text(function->metadataStringHeap,
                                                                function->metadataStringHeapLength,
                                                                "Box"));

    typeSpecRecord = find_first_token_record_by_table(function->metadataTokenRecords,
                                                      function->metadataTokenRecordLength,
                                                      ZR_METADATA_TABLE_TYPE_SPEC);
    TEST_ASSERT_NOT_NULL(typeSpecRecord);
    TEST_ASSERT_TRUE(typeSpecRecord->signatureBlobLength > 0u);
    TEST_ASSERT_TRUE(typeSpecRecord->signatureBlobOffset + typeSpecRecord->signatureBlobLength <=
                     function->signatureBlobHeapLength);

    blob = function->signatureBlobHeap + typeSpecRecord->signatureBlobOffset;
    cursor = 0;
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT8(ZR_METADATA_SIGNATURE_NODE_TYPE_REF, blob[cursor]);
    cursor += 1;
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, read_u32_le(blob, cursor));
    cursor += sizeof(TZrUInt32);
    boxStringIndex = read_u32_le(blob, cursor);
    cursor += sizeof(TZrUInt32);
    for (TZrUInt32 index = 0; index < function->metadataStringHeapLength; index++) {
        if (function->metadataStringHeap[index].stringIndex == boxStringIndex) {
            TEST_ASSERT_NOT_NULL(function->metadataStringHeap[index].value);
            TEST_ASSERT_EQUAL_STRING("Box", ZrCore_String_GetNativeString(function->metadataStringHeap[index].value));
            break;
        }
        if (index + 1u == function->metadataStringHeapLength) {
            TEST_FAIL_MESSAGE("Box string heap index was not found");
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, read_u32_le(blob, cursor));
}

static const SZrMetadataTokenRecord *find_first_type_spec_record(const SZrFunction *function) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    return find_first_token_record_by_table(function->metadataTokenRecords,
                                            function->metadataTokenRecordLength,
                                            ZR_METADATA_TABLE_TYPE_SPEC);
}

static void assert_io_token_model(const SZrIoFunction *function, const SZrFunction *expectedFunction) {
    const SZrIoFunctionTypedExportSymbol *symbol;
    const SZrMetadataTokenRecord *memberRecord;
    const SZrMetadataTokenRecord *signatureRecord;
    const SZrFunctionTypedExportSymbol *expectedSymbol;
    const SZrMetadataTokenRecord *expectedMemberRecord;
    const SZrMetadataTokenRecord *expectedSignatureRecord;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(expectedFunction);
    TEST_ASSERT_EQUAL_UINT32(expectedFunction->metadataTokenRecordLength, (TZrUInt32)function->metadataTokenRecordLength);
    TEST_ASSERT_EQUAL_UINT32(expectedFunction->signatureBlobHeapLength, (TZrUInt32)function->signatureBlobHeapLength);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);
    TEST_ASSERT_NOT_NULL(function->signatureBlobHeap);
    TEST_ASSERT_EQUAL_MEMORY(expectedFunction->signatureBlobHeap,
                             function->signatureBlobHeap,
                             expectedFunction->signatureBlobHeapLength);

    symbol = find_io_export_symbol(function, "sum");
    expectedSymbol = find_runtime_export_symbol(expectedFunction, "sum");
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_NOT_NULL(expectedSymbol);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_MEMBER_DEF, ZR_METADATA_TOKEN_TABLE(symbol->metadataToken));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_SIGNATURE, ZR_METADATA_TOKEN_TABLE(symbol->signatureToken));
    TEST_ASSERT_TRUE(symbol->signatureBlobLength > 0);
    TEST_ASSERT_TRUE(symbol->signatureBlobOffset + symbol->signatureBlobLength <= function->signatureBlobHeapLength);
    TEST_ASSERT_EQUAL_UINT64(expectedSymbol->signatureHash, symbol->signatureHash);

    memberRecord = find_token_record(function->metadataTokenRecords,
                                     function->metadataTokenRecordLength,
                                     symbol->metadataToken);
    signatureRecord = find_token_record(function->metadataTokenRecords,
                                        function->metadataTokenRecordLength,
                                        symbol->signatureToken);
    expectedMemberRecord = find_token_record(expectedFunction->metadataTokenRecords,
                                             expectedFunction->metadataTokenRecordLength,
                                             expectedSymbol->metadataToken);
    expectedSignatureRecord = find_token_record(expectedFunction->metadataTokenRecords,
                                                expectedFunction->metadataTokenRecordLength,
                                                expectedSymbol->signatureToken);
    TEST_ASSERT_NOT_NULL(memberRecord);
    TEST_ASSERT_NOT_NULL(signatureRecord);
    TEST_ASSERT_NOT_NULL(expectedMemberRecord);
    TEST_ASSERT_NOT_NULL(expectedSignatureRecord);
    TEST_ASSERT_EQUAL_UINT32(symbol->signatureToken, memberRecord->relatedToken);
    TEST_ASSERT_EQUAL_UINT64(expectedMemberRecord->signatureHash, memberRecord->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(expectedSignatureRecord->signatureHash, signatureRecord->signatureHash);
}

static void assert_metadata_binding_fields(const SZrMetadataTokenBinding *binding) {
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(TEST_BINDING_REF_TOKEN, binding->refToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_BINDING_REF_SIGNATURE_TOKEN, binding->refSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_BINDING_REF_SIGNATURE_HASH, binding->refSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_BINDING_EXPECTED_METADATA_TOKEN, binding->expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_BINDING_EXPECTED_SIGNATURE_TOKEN, binding->expectedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_BINDING_EXPECTED_SIGNATURE_HASH, binding->expectedSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_BINDING_EXPECTED_MODULE_SIGNATURE_HASH, binding->expectedModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_BINDING_EXPECTED_LAYOUT_VERSION, binding->expectedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(TEST_BINDING_EXPECTED_LAYOUT_HASH, binding->expectedLayoutHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_BINDING_RESOLVED_METADATA_TOKEN, binding->resolvedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_BINDING_RESOLVED_SIGNATURE_TOKEN, binding->resolvedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_BINDING_RESOLVED_SIGNATURE_HASH, binding->resolvedSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_BINDING_RESOLVED_MODULE_SIGNATURE_HASH, binding->resolvedModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_BINDING_RESOLVED_LAYOUT_VERSION, binding->resolvedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(TEST_BINDING_RESOLVED_LAYOUT_HASH, binding->resolvedLayoutHash);
}

static void test_signature_hash_is_stable_and_changes_with_normalized_signature(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *firstFunction;
    SZrFunction *secondFunction;
    SZrFunction *changedFunction;
    const SZrFunctionTypedExportSymbol *firstSymbol;
    const SZrFunctionTypedExportSymbol *secondSymbol;
    const SZrFunctionTypedExportSymbol *changedSymbol;

    TEST_ASSERT_NOT_NULL(state);

    firstFunction = create_metadata_token_fixture(state);
    secondFunction = create_metadata_token_fixture(state);
    changedFunction = create_metadata_token_fixture_with_return_type(state, ZR_VALUE_TYPE_DOUBLE);
    TEST_ASSERT_NOT_NULL(firstFunction);
    TEST_ASSERT_NOT_NULL(secondFunction);
    TEST_ASSERT_NOT_NULL(changedFunction);

    firstSymbol = find_runtime_export_symbol(firstFunction, "sum");
    secondSymbol = find_runtime_export_symbol(secondFunction, "sum");
    changedSymbol = find_runtime_export_symbol(changedFunction, "sum");
    TEST_ASSERT_NOT_NULL(firstSymbol);
    TEST_ASSERT_NOT_NULL(secondSymbol);
    TEST_ASSERT_NOT_NULL(changedSymbol);
    TEST_ASSERT_TRUE(firstSymbol->signatureHash != 0u);
    TEST_ASSERT_EQUAL_UINT64(firstSymbol->signatureHash, secondSymbol->signatureHash);
    TEST_ASSERT_TRUE(firstSymbol->signatureHash != changedSymbol->signatureHash);
    TEST_ASSERT_TRUE(firstFunction->moduleSignatureHash != 0u);
    TEST_ASSERT_EQUAL_UINT64(firstFunction->moduleSignatureHash, secondFunction->moduleSignatureHash);
    TEST_ASSERT_TRUE(firstFunction->moduleSignatureHash != changedFunction->moduleSignatureHash);

    ZrCore_Function_Free(state, firstFunction);
    ZrCore_Function_Free(state, secondFunction);
    ZrCore_Function_Free(state, changedFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_metadata_tokens_and_signature_blob_roundtrip_through_binary_and_runtime(void) {
    const char *binaryPath = "metadata_token_model_roundtrip.zro";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    const SZrIoFunction *binaryEntry;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    function = create_metadata_token_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, function->typedExportedSymbolLength);
    assert_runtime_token_model(function);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    io.isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    binaryEntry = sourceObject->modules[0].entryFunction;
    assert_io_token_model(binaryEntry, function);
    TEST_ASSERT_EQUAL_UINT64(function->moduleSignatureHash, binaryEntry->moduleSignatureHash);

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    assert_runtime_token_model(runtimeFunction);
    TEST_ASSERT_EQUAL_UINT64(function->moduleSignatureHash, runtimeFunction->moduleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(function->metadataTokenRecordLength, runtimeFunction->metadataTokenRecordLength);
    TEST_ASSERT_EQUAL_UINT32(function->signatureBlobHeapLength, runtimeFunction->signatureBlobHeapLength);
    TEST_ASSERT_EQUAL_MEMORY(function->signatureBlobHeap,
                             runtimeFunction->signatureBlobHeap,
                             function->signatureBlobHeapLength);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_module_metadata_binding_roundtrips_through_binary_and_runtime(void) {
    const char *binaryPath = "metadata_token_binding_roundtrip.zro";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    const SZrIoFunction *binaryEntry;
    const SZrMetadataTokenBinding *runtimeBinding;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    function = create_metadata_token_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(add_test_module_metadata_binding(state, function));
    assert_metadata_binding_fields(ZrCore_Function_FindModuleMetadataBinding(function, TEST_BINDING_REF_TOKEN));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    io.isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    binaryEntry = sourceObject->modules[0].entryFunction;
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)binaryEntry->moduleMetadataBindingLength);
    assert_metadata_binding_fields(&binaryEntry->moduleMetadataBindings[0]);

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    runtimeBinding = ZrCore_Function_FindModuleMetadataBinding(runtimeFunction, TEST_BINDING_REF_TOKEN);
    assert_metadata_binding_fields(runtimeBinding);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_metadata_strings_are_indexed_in_shared_heap_through_binary_and_runtime(void) {
    const char *binaryPath = "metadata_token_string_heap_roundtrip.zro";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    const SZrIoFunction *binaryEntry;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    function = create_named_signature_metadata_token_fixture(state, "Box<int>", "Box<int>");
    TEST_ASSERT_NOT_NULL(function);
    assert_shared_metadata_string_heap_model(function);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    io.isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    binaryEntry = sourceObject->modules[0].entryFunction;
    TEST_ASSERT_EQUAL_UINT32(function->metadataStringHeapLength,
                             (TZrUInt32)binaryEntry->metadataStringHeapLength);
    TEST_ASSERT_NOT_NULL(binaryEntry->metadataStringHeap);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             count_string_heap_entries_for_text(binaryEntry->metadataStringHeap,
                                                                (TZrUInt32)binaryEntry->metadataStringHeapLength,
                                                                "Box"));

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    assert_shared_metadata_string_heap_model(runtimeFunction);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_reader_rejects_future_metadata_patch_with_version_diagnostic(void) {
    const char *binaryPath = "metadata_token_future_patch.zro";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrReadSourceTryContext readContext;
    EZrThreadStatus status;
    const TZrChar *message;
    char actualPatchText[64];
    char supportedPatchText[64];
    const TZrSize versionPatchOffset = 12u;

    TEST_ASSERT_NOT_NULL(state);

    function = create_metadata_token_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > versionPatchOffset + sizeof(TZrUInt32));
    TEST_ASSERT_EQUAL_UINT32(ZR_IO_SOURCE_PATCH_CURRENT, read_u32_le(buffer, versionPatchOffset));

    write_u32_le(buffer, versionPatchOffset, ZR_IO_SOURCE_PATCH_CURRENT + 1u);
    snprintf(actualPatchText, sizeof(actualPatchText), "actualPatch=%u", (unsigned)(ZR_IO_SOURCE_PATCH_CURRENT + 1u));
    snprintf(supportedPatchText, sizeof(supportedPatchText), "supportedPatch=%u", (unsigned)ZR_IO_SOURCE_PATCH_CURRENT);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    io.isBinary = ZR_TRUE;
    memset(&readContext, 0, sizeof(readContext));
    readContext.io = &io;

    status = ZrCore_Exception_TryRun(state, read_source_try_body, &readContext);
    TEST_ASSERT_TRUE(ZrCore_Exception_IsStausError(status));
    TEST_ASSERT_NULL(readContext.source);
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "io source version patch is newer than this runtime"));
    TEST_ASSERT_NOT_NULL(strstr(message, actualPatchText));
    TEST_ASSERT_NOT_NULL(strstr(message, supportedPatchText));

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_module_signature_hash_changes_with_module_version(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *firstFunction;
    SZrFunction *secondFunction;
    SZrFunction *changedFunction;

    TEST_ASSERT_NOT_NULL(state);

    firstFunction = create_metadata_token_fixture_with_version(state, "1.0.0");
    secondFunction = create_metadata_token_fixture_with_version(state, "1.0.0");
    changedFunction = create_metadata_token_fixture_with_version(state, "2.0.0");
    TEST_ASSERT_NOT_NULL(firstFunction);
    TEST_ASSERT_NOT_NULL(secondFunction);
    TEST_ASSERT_NOT_NULL(changedFunction);

    TEST_ASSERT_TRUE(firstFunction->moduleSignatureHash != 0u);
    TEST_ASSERT_EQUAL_UINT64(firstFunction->moduleSignatureHash, secondFunction->moduleSignatureHash);
    TEST_ASSERT_TRUE(firstFunction->moduleSignatureHash != changedFunction->moduleSignatureHash);

    ZrCore_Function_Free(state, firstFunction);
    ZrCore_Function_Free(state, secondFunction);
    ZrCore_Function_Free(state, changedFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_module_signature_hash_changes_with_union_type_def_contract(void) {
    const char *firstSource =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    const char *changedSource =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "    Other(code: i32);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *firstAst;
    SZrAstNode *changedAst;
    SZrFunction *firstFunction;
    SZrFunction *changedFunction;
    const SZrFunctionTypedExportSymbol *firstSymbol;
    const SZrFunctionTypedExportSymbol *changedSymbol;
    const SZrMetadataTokenRecord *firstTypeDef;
    const SZrMetadataTokenRecord *changedTypeDef;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_union_module_hash.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    firstAst = ZrParser_Parse(state, firstSource, strlen(firstSource), sourceName);
    changedAst = ZrParser_Parse(state, changedSource, strlen(changedSource), sourceName);
    TEST_ASSERT_NOT_NULL(firstAst);
    TEST_ASSERT_NOT_NULL(changedAst);

    firstFunction = create_union_metadata_token_fixture(state, firstAst);
    changedFunction = create_union_metadata_token_fixture(state, changedAst);
    TEST_ASSERT_NOT_NULL(firstFunction);
    TEST_ASSERT_NOT_NULL(changedFunction);

    firstSymbol = find_runtime_export_symbol(firstFunction, "choose");
    changedSymbol = find_runtime_export_symbol(changedFunction, "choose");
    firstTypeDef = find_first_token_record_by_table(firstFunction->metadataTokenRecords,
                                                    firstFunction->metadataTokenRecordLength,
                                                    ZR_METADATA_TABLE_TYPE_DEF);
    changedTypeDef = find_first_token_record_by_table(changedFunction->metadataTokenRecords,
                                                      changedFunction->metadataTokenRecordLength,
                                                      ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(firstSymbol);
    TEST_ASSERT_NOT_NULL(changedSymbol);
    TEST_ASSERT_NOT_NULL(firstTypeDef);
    TEST_ASSERT_NOT_NULL(changedTypeDef);
    TEST_ASSERT_EQUAL_UINT64(firstSymbol->signatureHash, changedSymbol->signatureHash);
    TEST_ASSERT_TRUE(firstTypeDef->signatureHash != changedTypeDef->signatureHash);
    TEST_ASSERT_TRUE(firstFunction->moduleSignatureHash != 0u);
    TEST_ASSERT_TRUE(changedFunction->moduleSignatureHash != 0u);
    TEST_ASSERT_TRUE(firstFunction->moduleSignatureHash != changedFunction->moduleSignatureHash);

    ZrCore_Function_Free(state, firstFunction);
    ZrCore_Function_Free(state, changedFunction);
    ZrParser_Ast_Free(state, firstAst);
    ZrParser_Ast_Free(state, changedAst);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_union_return_type_uses_union_signature_node(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_union_signature.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    function = create_union_metadata_token_fixture(state, ast);
    TEST_ASSERT_NOT_NULL(function);
    assert_union_signature_model(function);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, ast);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_union_return_type_emits_type_spec_token_record(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_union_type_spec.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    function = create_union_metadata_token_fixture(state, ast);
    TEST_ASSERT_NOT_NULL(function);
    assert_union_type_spec_token_model(function);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, ast);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_union_return_type_emits_type_def_token_record(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_union_type_def.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    function = create_union_metadata_token_fixture(state, ast);
    TEST_ASSERT_NOT_NULL(function);
    assert_union_type_def_token_model(function);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, ast);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_union_type_def_layout_identity_roundtrips_through_binary_and_runtime(void) {
    const char *binaryPath = "metadata_union_type_def_layout_roundtrip.zro";
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    const SZrIoFunction *binaryEntry;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_union_type_def_layout.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    function = create_union_metadata_token_fixture(state, ast);
    TEST_ASSERT_NOT_NULL(function);
    assert_union_type_def_token_model(function);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    io.isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    binaryEntry = sourceObject->modules[0].entryFunction;
    assert_union_type_def_layout_identity_preserved(function->metadataTokenRecords,
                                                    function->metadataTokenRecordLength,
                                                    binaryEntry->metadataTokenRecords,
                                                    (TZrUInt32)binaryEntry->metadataTokenRecordLength);

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    assert_union_type_def_layout_identity_preserved(function->metadataTokenRecords,
                                                    function->metadataTokenRecordLength,
                                                    runtimeFunction->metadataTokenRecords,
                                                    runtimeFunction->metadataTokenRecordLength);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrParser_Ast_Free(state, ast);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_generic_return_type_emits_generic_inst_type_spec(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);

    function = create_named_return_metadata_token_fixture(state, "Box<int>");
    TEST_ASSERT_NOT_NULL(function);
    assert_generic_type_spec_token_model(function);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_duplicate_generic_type_spec_is_deduplicated(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);

    function = create_named_signature_metadata_token_fixture(state, "Box<int>", "Box<int>");
    TEST_ASSERT_NOT_NULL(function);
    assert_generic_type_spec_token_model(function);
    assert_single_type_spec_record_pair(function);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_matching_type_spec_records_bind_by_canonical_signature(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    const SZrMetadataTokenRecord *callerTypeSpec;
    const SZrMetadataTokenRecord *providerTypeSpec;
    const SZrMetadataTokenBinding *binding;

    TEST_ASSERT_NOT_NULL(state);

    callerFunction = create_named_return_metadata_token_fixture(state, "Box<int>");
    providerFunction = create_named_return_metadata_token_fixture(state, "Box<int>");
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(providerFunction);

    callerTypeSpec = find_first_type_spec_record(callerFunction);
    providerTypeSpec = find_first_type_spec_record(providerFunction);
    TEST_ASSERT_NOT_NULL(callerTypeSpec);
    TEST_ASSERT_NOT_NULL(providerTypeSpec);
    TEST_ASSERT_EQUAL_UINT64(callerTypeSpec->signatureHash, providerTypeSpec->signatureHash);
    TEST_ASSERT_TRUE(callerTypeSpec->signatureBlobLength > 0u);
    TEST_ASSERT_EQUAL_UINT32(callerTypeSpec->signatureBlobLength, providerTypeSpec->signatureBlobLength);
    TEST_ASSERT_EQUAL_MEMORY(callerFunction->signatureBlobHeap + callerTypeSpec->signatureBlobOffset,
                             providerFunction->signatureBlobHeap + providerTypeSpec->signatureBlobOffset,
                             callerTypeSpec->signatureBlobLength);

    TEST_ASSERT_TRUE(ZrCore_Function_BindMatchingTypeSpecMetadata(state, callerFunction, providerFunction));
    binding = ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeSpec->token);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(callerTypeSpec->token, binding->refToken);
    TEST_ASSERT_EQUAL_UINT32(callerTypeSpec->relatedToken, binding->refSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(callerTypeSpec->signatureHash, binding->refSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(callerTypeSpec->token, binding->expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(callerTypeSpec->relatedToken, binding->expectedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(callerTypeSpec->signatureHash, binding->expectedSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(providerTypeSpec->token, binding->resolvedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(providerTypeSpec->relatedToken, binding->resolvedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeSpec->signatureHash, binding->resolvedSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(providerFunction->moduleSignatureHash, binding->resolvedModuleSignatureHash);

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_union_type_spec_binding_records_type_def_layout_identity(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *callerAst;
    SZrAstNode *providerAst;
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    const SZrMetadataTokenRecord *callerTypeSpec;
    const SZrMetadataTokenRecord *callerTypeDef;
    const SZrMetadataTokenRecord *providerTypeDef;
    const SZrMetadataTokenBinding *typeSpecBinding;
    const SZrMetadataTokenBinding *typeDefBinding;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_union_type_spec_layout_binding.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    callerAst = ZrParser_Parse(state, source, strlen(source), sourceName);
    providerAst = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(callerAst);
    TEST_ASSERT_NOT_NULL(providerAst);

    callerFunction = create_union_metadata_token_fixture(state, callerAst);
    providerFunction = create_union_metadata_token_fixture(state, providerAst);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(providerFunction);

    callerTypeSpec = find_first_token_record_by_table(callerFunction->metadataTokenRecords,
                                                      callerFunction->metadataTokenRecordLength,
                                                      ZR_METADATA_TABLE_TYPE_SPEC);
    callerTypeDef = find_first_token_record_by_table(callerFunction->metadataTokenRecords,
                                                     callerFunction->metadataTokenRecordLength,
                                                     ZR_METADATA_TABLE_TYPE_DEF);
    providerTypeDef = find_first_token_record_by_table(providerFunction->metadataTokenRecords,
                                                       providerFunction->metadataTokenRecordLength,
                                                       ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(callerTypeSpec);
    TEST_ASSERT_NOT_NULL(callerTypeDef);
    TEST_ASSERT_NOT_NULL(providerTypeDef);
    TEST_ASSERT_EQUAL_UINT32(1u, callerTypeDef->layoutVersion);
    TEST_ASSERT_TRUE(callerTypeDef->layoutHash != 0u);
    TEST_ASSERT_EQUAL_UINT32(callerTypeDef->layoutVersion, providerTypeDef->layoutVersion);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->layoutHash, providerTypeDef->layoutHash);

    TEST_ASSERT_TRUE(ZrCore_Function_BindMatchingTypeSpecMetadata(state, callerFunction, providerFunction));
    typeSpecBinding = ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeSpec->token);
    typeDefBinding = ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeDef->token);
    TEST_ASSERT_NOT_NULL(typeSpecBinding);
    TEST_ASSERT_NOT_NULL(typeDefBinding);
    TEST_ASSERT_EQUAL_UINT32(callerTypeDef->token, typeDefBinding->refToken);
    TEST_ASSERT_EQUAL_UINT32(callerTypeDef->relatedToken, typeDefBinding->refSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->signatureHash, typeDefBinding->refSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(callerTypeDef->token, typeDefBinding->expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(callerTypeDef->relatedToken, typeDefBinding->expectedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->signatureHash, typeDefBinding->expectedSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->token, typeDefBinding->resolvedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, typeDefBinding->resolvedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, typeDefBinding->resolvedSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(callerTypeDef->layoutVersion, typeDefBinding->expectedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->layoutHash, typeDefBinding->expectedLayoutHash);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->layoutVersion, typeDefBinding->resolvedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->layoutHash, typeDefBinding->resolvedLayoutHash);

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrParser_Ast_Free(state, callerAst);
    ZrParser_Ast_Free(state, providerAst);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_union_type_spec_binding_reports_layout_mismatch_without_partial_binding(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *callerAst;
    SZrAstNode *providerAst;
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    SZrMetadataTypeSpecBindStatus status;
    const SZrMetadataTokenRecord *callerTypeSpec;
    const SZrMetadataTokenRecord *callerTypeDef;
    SZrMetadataTokenRecord *providerTypeDef;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_union_type_spec_layout_mismatch.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    callerAst = ZrParser_Parse(state, source, strlen(source), sourceName);
    providerAst = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(callerAst);
    TEST_ASSERT_NOT_NULL(providerAst);

    callerFunction = create_union_metadata_token_fixture(state, callerAst);
    providerFunction = create_union_metadata_token_fixture(state, providerAst);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(providerFunction);

    callerTypeSpec = find_first_token_record_by_table(callerFunction->metadataTokenRecords,
                                                      callerFunction->metadataTokenRecordLength,
                                                      ZR_METADATA_TABLE_TYPE_SPEC);
    callerTypeDef = find_first_token_record_by_table(callerFunction->metadataTokenRecords,
                                                     callerFunction->metadataTokenRecordLength,
                                                     ZR_METADATA_TABLE_TYPE_DEF);
    providerTypeDef = (SZrMetadataTokenRecord *)find_first_token_record_by_table(
            providerFunction->metadataTokenRecords,
            providerFunction->metadataTokenRecordLength,
            ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(callerTypeSpec);
    TEST_ASSERT_NOT_NULL(callerTypeDef);
    TEST_ASSERT_NOT_NULL(providerTypeDef);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->signatureHash, providerTypeDef->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->layoutHash, providerTypeDef->layoutHash);

    providerTypeDef->layoutHash ^= (TZrUInt64)0x1u;
    ZrCore_Memory_RawSet(&status, 0, sizeof(status));
    TEST_ASSERT_FALSE(ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(state,
                                                                            callerFunction,
                                                                            providerFunction,
                                                                            &status));
    TEST_ASSERT_EQUAL_UINT32(1u, status.callerTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(0u, status.matchedTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(0u, status.unmatchedTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(1u, status.layoutMismatchCount);
    TEST_ASSERT_EQUAL_UINT32(callerTypeSpec->token, status.firstLayoutMismatchTypeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(callerTypeDef->layoutVersion, status.firstExpectedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->layoutHash, status.firstExpectedLayoutHash);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->layoutVersion, status.firstActualLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->layoutHash, status.firstActualLayoutHash);
    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeSpec->token));
    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeDef->token));

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrParser_Ast_Free(state, callerAst);
    ZrParser_Ast_Free(state, providerAst);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_union_type_spec_binding_reports_type_def_contract_mismatch_without_partial_binding(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *callerAst;
    SZrAstNode *providerAst;
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    SZrMetadataTypeSpecBindStatus status;
    const SZrMetadataTokenRecord *callerTypeSpec;
    const SZrMetadataTokenRecord *callerTypeDef;
    SZrMetadataTokenRecord *providerTypeDef;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_union_type_def_contract_mismatch.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    callerAst = ZrParser_Parse(state, source, strlen(source), sourceName);
    providerAst = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(callerAst);
    TEST_ASSERT_NOT_NULL(providerAst);

    callerFunction = create_union_metadata_token_fixture(state, callerAst);
    providerFunction = create_union_metadata_token_fixture(state, providerAst);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(providerFunction);

    callerTypeSpec = find_first_token_record_by_table(callerFunction->metadataTokenRecords,
                                                      callerFunction->metadataTokenRecordLength,
                                                      ZR_METADATA_TABLE_TYPE_SPEC);
    callerTypeDef = find_first_token_record_by_table(callerFunction->metadataTokenRecords,
                                                     callerFunction->metadataTokenRecordLength,
                                                     ZR_METADATA_TABLE_TYPE_DEF);
    providerTypeDef = (SZrMetadataTokenRecord *)find_first_token_record_by_table(
            providerFunction->metadataTokenRecords,
            providerFunction->metadataTokenRecordLength,
            ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(callerTypeSpec);
    TEST_ASSERT_NOT_NULL(callerTypeDef);
    TEST_ASSERT_NOT_NULL(providerTypeDef);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->signatureHash, providerTypeDef->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->layoutHash, providerTypeDef->layoutHash);

    providerTypeDef->signatureHash ^= (TZrUInt64)0x1u;
    ZrCore_Memory_RawSet(&status, 0, sizeof(status));
    TEST_ASSERT_FALSE(ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(state,
                                                                            callerFunction,
                                                                            providerFunction,
                                                                            &status));
    TEST_ASSERT_EQUAL_UINT32(1u, status.callerTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(0u, status.matchedTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(0u, status.unmatchedTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(1u, status.definitionMismatchCount);
    TEST_ASSERT_EQUAL_UINT32(0u, status.layoutMismatchCount);
    TEST_ASSERT_EQUAL_UINT32(callerTypeSpec->token, status.firstDefinitionMismatchTypeSpecToken);
    TEST_ASSERT_EQUAL_UINT64(callerTypeDef->signatureHash, status.firstExpectedDefinitionSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, status.firstActualDefinitionSignatureHash);
    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeSpec->token));
    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeDef->token));

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrParser_Ast_Free(state, callerAst);
    ZrParser_Ast_Free(state, providerAst);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_type_spec_binding_reports_unmatched_caller_signature(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    SZrMetadataTypeSpecBindStatus status;

    TEST_ASSERT_NOT_NULL(state);

    callerFunction = create_named_return_metadata_token_fixture(state, "Box<int>");
    providerFunction = create_named_return_metadata_token_fixture(state, "Box<float>");
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(providerFunction);

    ZrCore_Memory_RawSet(&status, 0, sizeof(status));
    TEST_ASSERT_FALSE(ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(state,
                                                                            callerFunction,
                                                                            providerFunction,
                                                                            &status));
    TEST_ASSERT_EQUAL_UINT32(1u, status.callerTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(0u, status.matchedTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(1u, status.unmatchedTypeSpecCount);
    TEST_ASSERT_EQUAL_UINT32(find_first_type_spec_record(callerFunction)->token, status.firstUnmatchedTypeSpecToken);
    TEST_ASSERT_EQUAL_UINT64(find_first_type_spec_record(callerFunction)->signatureHash,
                             status.firstUnmatchedSignatureHash);
    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataBinding(
            callerFunction,
            find_first_type_spec_record(callerFunction)->token));

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_import_member_ref_records_preserve_assembly_type_owner_chain(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    const SZrMetadataTokenRecord *assemblyRecord;
    const SZrMetadataTokenRecord *assemblySignatureRecord;
    const SZrMetadataTokenRecord *typeRecord;
    const SZrMetadataTokenRecord *typeSignatureRecord;
    const SZrMetadataTokenRecord *memberRecord;
    const SZrMetadataTokenRecord *memberSignatureRecord;

    TEST_ASSERT_NOT_NULL(state);

    function = create_import_member_ref_metadata_token_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->metadataTokenRecords);
    TEST_ASSERT_TRUE(function->metadataTokenRecordLength >= 6u);

    assemblyRecord = find_first_token_record_by_table(function->metadataTokenRecords,
                                                      function->metadataTokenRecordLength,
                                                      ZR_METADATA_TABLE_ASSEMBLY_REF);
    typeRecord = find_first_token_record_by_table(function->metadataTokenRecords,
                                                  function->metadataTokenRecordLength,
                                                  ZR_METADATA_TABLE_TYPE_REF);
    memberRecord = find_first_token_record_by_table(function->metadataTokenRecords,
                                                    function->metadataTokenRecordLength,
                                                    ZR_METADATA_TABLE_MEMBER_REF);
    TEST_ASSERT_NOT_NULL(assemblyRecord);
    TEST_ASSERT_NOT_NULL(typeRecord);
    TEST_ASSERT_NOT_NULL(memberRecord);
    TEST_ASSERT_EQUAL_UINT32(0u, assemblyRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(assemblyRecord->token, typeRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(typeRecord->token, memberRecord->ownerToken);

    assemblySignatureRecord = find_token_record(function->metadataTokenRecords,
                                                function->metadataTokenRecordLength,
                                                assemblyRecord->relatedToken);
    typeSignatureRecord = find_token_record(function->metadataTokenRecords,
                                            function->metadataTokenRecordLength,
                                            typeRecord->relatedToken);
    memberSignatureRecord = find_token_record(function->metadataTokenRecords,
                                              function->metadataTokenRecordLength,
                                              memberRecord->relatedToken);
    TEST_ASSERT_NOT_NULL(assemblySignatureRecord);
    TEST_ASSERT_NOT_NULL(typeSignatureRecord);
    TEST_ASSERT_NOT_NULL(memberSignatureRecord);
    TEST_ASSERT_EQUAL_UINT32(assemblyRecord->token, assemblySignatureRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(typeRecord->token, typeSignatureRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(memberRecord->token, memberSignatureRecord->ownerToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_METADATA_TOKEN, memberRecord->targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_SIGNATURE_TOKEN, memberRecord->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_METADATA_TOKEN, memberSignatureRecord->targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_SIGNATURE_TOKEN, memberSignatureRecord->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_SIGNATURE_HASH, memberRecord->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_SIGNATURE_HASH, memberSignatureRecord->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH, assemblyRecord->targetModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH,
                             assemblySignatureRecord->targetModuleSignatureHash);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void assert_import_target_identity_runtime(const SZrFunction *function) {
    const SZrMetadataTokenRecord *memberRecord;
    const SZrMetadataTokenRecord *memberSignatureRecord;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, function->moduleEntryEffectLength);
    TEST_ASSERT_NOT_NULL(function->moduleEntryEffects);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_METADATA_TOKEN, function->moduleEntryEffects[0].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_SIGNATURE_TOKEN, function->moduleEntryEffects[0].targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_SIGNATURE_HASH, function->moduleEntryEffects[0].targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH,
                             function->moduleEntryEffects[0].targetModuleSignatureHash);

    memberRecord = find_first_token_record_by_table(function->metadataTokenRecords,
                                                    function->metadataTokenRecordLength,
                                                    ZR_METADATA_TABLE_MEMBER_REF);
    TEST_ASSERT_NOT_NULL(memberRecord);
    memberSignatureRecord = find_token_record(function->metadataTokenRecords,
                                              function->metadataTokenRecordLength,
                                              memberRecord->relatedToken);
    TEST_ASSERT_NOT_NULL(memberSignatureRecord);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_METADATA_TOKEN, memberRecord->targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_SIGNATURE_TOKEN, memberRecord->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_METADATA_TOKEN, memberSignatureRecord->targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_SIGNATURE_TOKEN, memberSignatureRecord->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_SIGNATURE_HASH, memberRecord->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_SIGNATURE_HASH, memberSignatureRecord->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH,
                             function->moduleMetadataTokenRecords[0].targetModuleSignatureHash);
}

static void assert_import_target_identity_io(const SZrIoFunction *function) {
    const SZrMetadataTokenRecord *memberRecord;
    const SZrMetadataTokenRecord *memberSignatureRecord;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)function->moduleEntryEffectsLength);
    TEST_ASSERT_NOT_NULL(function->moduleEntryEffects);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_METADATA_TOKEN, function->moduleEntryEffects[0].targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_SIGNATURE_TOKEN, function->moduleEntryEffects[0].targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_SIGNATURE_HASH, function->moduleEntryEffects[0].targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH,
                             function->moduleEntryEffects[0].targetModuleSignatureHash);

    memberRecord = find_first_token_record_by_table(function->metadataTokenRecords,
                                                    function->metadataTokenRecordLength,
                                                    ZR_METADATA_TABLE_MEMBER_REF);
    TEST_ASSERT_NOT_NULL(memberRecord);
    memberSignatureRecord = find_token_record(function->metadataTokenRecords,
                                              function->metadataTokenRecordLength,
                                              memberRecord->relatedToken);
    TEST_ASSERT_NOT_NULL(memberSignatureRecord);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_METADATA_TOKEN, memberRecord->targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_SIGNATURE_TOKEN, memberRecord->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_METADATA_TOKEN, memberSignatureRecord->targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_IMPORT_TARGET_SIGNATURE_TOKEN, memberSignatureRecord->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_SIGNATURE_HASH, memberRecord->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_SIGNATURE_HASH, memberSignatureRecord->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_IMPORT_TARGET_MODULE_SIGNATURE_HASH,
                             function->moduleMetadataTokenRecords[0].targetModuleSignatureHash);
}

static void test_import_target_signature_identity_roundtrips_through_binary_and_runtime(void) {
    const char *binaryPath = "metadata_token_import_target_roundtrip.zro";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    const SZrIoFunction *binaryEntry;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    function = create_import_member_ref_metadata_token_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    assert_import_target_identity_runtime(function);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    io.isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    binaryEntry = sourceObject->modules[0].entryFunction;
    assert_import_target_identity_io(binaryEntry);

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    assert_import_target_identity_runtime(runtimeFunction);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_module_ref_table_aggregates_duplicate_import_refs_through_binary_and_runtime(void) {
    const char *binaryPath = "metadata_token_module_ref_table_roundtrip.zro";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    const SZrIoFunction *binaryEntry;
    SZrFunction *runtimeFunction = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    function = create_module_ref_aggregation_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(10u, function->metadataTokenRecordLength);
    assert_module_ref_table_aggregates_import_refs_runtime(function);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    io.isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
    binaryEntry = sourceObject->modules[0].entryFunction;
    assert_module_ref_table_aggregates_import_refs_io(binaryEntry);

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    assert_module_ref_table_aggregates_import_refs_runtime(runtimeFunction);

    remove(binaryPath);
    free(buffer);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_metadata_tokens_and_signature_blob_roundtrip_through_binary_and_runtime);
    RUN_TEST(test_module_metadata_binding_roundtrips_through_binary_and_runtime);
    RUN_TEST(test_metadata_strings_are_indexed_in_shared_heap_through_binary_and_runtime);
    RUN_TEST(test_reader_rejects_future_metadata_patch_with_version_diagnostic);
    RUN_TEST(test_signature_hash_is_stable_and_changes_with_normalized_signature);
    RUN_TEST(test_module_signature_hash_changes_with_module_version);
    RUN_TEST(test_module_signature_hash_changes_with_union_type_def_contract);
    RUN_TEST(test_union_return_type_uses_union_signature_node);
    RUN_TEST(test_union_return_type_emits_type_spec_token_record);
    RUN_TEST(test_union_return_type_emits_type_def_token_record);
    RUN_TEST(test_union_type_def_layout_identity_roundtrips_through_binary_and_runtime);
    RUN_TEST(test_generic_return_type_emits_generic_inst_type_spec);
    RUN_TEST(test_duplicate_generic_type_spec_is_deduplicated);
    RUN_TEST(test_matching_type_spec_records_bind_by_canonical_signature);
    RUN_TEST(test_union_type_spec_binding_records_type_def_layout_identity);
    RUN_TEST(test_union_type_spec_binding_reports_layout_mismatch_without_partial_binding);
    RUN_TEST(test_union_type_spec_binding_reports_type_def_contract_mismatch_without_partial_binding);
    RUN_TEST(test_type_spec_binding_reports_unmatched_caller_signature);
    RUN_TEST(test_import_member_ref_records_preserve_assembly_type_owner_chain);
    RUN_TEST(test_import_target_signature_identity_roundtrips_through_binary_and_runtime);
    RUN_TEST(test_module_ref_table_aggregates_duplicate_import_refs_through_binary_and_runtime);
    return UNITY_END();
}
