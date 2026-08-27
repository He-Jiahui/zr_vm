#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "harness/aot_c_link_support.h"
#include "harness/path_support.h"
#include "runtime_support.h"
#include "zr_vm_ffi_fixture_path.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_ffi/runtime.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/ffi_contract.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/writer.h"

#if defined(ZR_PLATFORM_UNIX)
#include <dlfcn.h>
#include "zr_vm_common/zr_aot_abi.h"
#endif

#if defined(ZR_PLATFORM_WIN)
#define ZR_NATIVE_EXTERN_SCALAR_CONTRACT_HASH UINT64_C(0x65c022e3b9014c41)
#define ZR_NATIVE_EXTERN_SCALAR_CONTRACT_HASH_TEXT "0x65c022e3b9014c41"
#else
#define ZR_NATIVE_EXTERN_SCALAR_CONTRACT_HASH UINT64_C(0x824b36ce7d149023)
#define ZR_NATIVE_EXTERN_SCALAR_CONTRACT_HASH_TEXT "0x824b36ce7d149023"
#endif

typedef struct SZrNativeExternBinaryReader {
    TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrNativeExternBinaryReader;

static TZrByte *native_extern_read_file(
        const TZrChar *path,
        TZrSize *outLength) {
    FILE *file = fopen(path, "rb");
    long fileSize;
    TZrByte *bytes;

    if (outLength != ZR_NULL) {
        *outLength = 0u;
    }
    if (file == ZR_NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != ZR_NULL) fclose(file);
        return ZR_NULL;
    }
    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }
    bytes = (TZrByte *)malloc((size_t)fileSize);
    if (bytes == ZR_NULL ||
        (fileSize > 0 && fread(bytes, 1u, (size_t)fileSize, file) != (size_t)fileSize)) {
        free(bytes);
        fclose(file);
        return ZR_NULL;
    }
    fclose(file);
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)fileSize;
    }
    return bytes;
}

static TZrChar *native_extern_read_text_file(const TZrChar *path) {
    TZrSize length = 0u;
    TZrByte *bytes = native_extern_read_file(path, &length);
    TZrChar *text;

    if (bytes == ZR_NULL) {
        return ZR_NULL;
    }
    text = (TZrChar *)malloc(length + 1u);
    if (text == ZR_NULL) {
        free(bytes);
        return ZR_NULL;
    }
    memcpy(text, bytes, length);
    text[length] = '\0';
    free(bytes);
    return text;
}

static void native_extern_write_text_file(const TZrChar *path, const TZrChar *text) {
    FILE *file;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));
    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1u, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void native_extern_hash_file(
        const TZrChar *path,
        TZrChar *buffer,
        TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, bufferSize);
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1u, sizeof(chunk), file)) > 0u) {
        for (TZrSize index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(
            buffer,
            bufferSize,
            ZR_STABLE_HASH_HEX_PRINTF_FORMAT,
            (unsigned long long)hash);
}

static TZrBytePtr native_extern_binary_read(
        SZrState *state,
        TZrPtr customData,
        ZR_OUT TZrSize *size) {
    SZrNativeExternBinaryReader *reader = (SZrNativeExternBinaryReader *)customData;

    ZR_UNUSED_PARAMETER(state);
    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed) {
        return ZR_NULL;
    }
    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return reader->bytes;
}

static void native_extern_binary_close(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static TZrPtr native_extern_test_allocator(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0u) {
        free(pointer);
        return ZR_NULL;
    }
    return pointer == ZR_NULL ? malloc(newSize) : realloc(pointer, newSize);
}

static SZrState *native_extern_create_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(
            native_extern_test_allocator,
            ZR_NULL,
            UINT64_C(0x4e41544956454646),
            &callbacks);

    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    ZrParser_ToGlobalState_Register(global->mainThreadState);
    ZrVmLibMath_Register(global);
    ZrVmLibSystem_Register(global);
    ZrVmLibContainer_Register(global);
    ZrVmLibFfi_Register(global);
    return global->mainThreadState;
}

static void native_extern_destroy_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

static void escape_zr_string(
        TZrChar *destination,
        TZrSize destinationSize,
        const TZrChar *source) {
    TZrSize readIndex = 0u;
    TZrSize writeIndex = 0u;

    TEST_ASSERT_NOT_NULL(destination);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, destinationSize);
    destination[0] = '\0';
    if (source == ZR_NULL) {
        return;
    }
    while (source[readIndex] != '\0' && writeIndex + 1u < destinationSize) {
        if ((source[readIndex] == '\\' || source[readIndex] == '"') &&
            writeIndex + 2u < destinationSize) {
            destination[writeIndex++] = '\\';
        }
        destination[writeIndex++] = source[readIndex++];
    }
    destination[writeIndex] = '\0';
}

static SZrAstNode *parse_source(
        SZrState *state,
        const TZrChar *source,
        const TZrChar *sourceNameText) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)sourceNameText);
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static EZrFfiContractStatus native_extern_build_contract(
        SZrState *state,
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrNativeImportContract *outContract,
        SZrFfiContractDiagnostic *diagnostic) {
    SZrSemanticContext *semanticContext =
            ZrParser_SemanticContext_New(state);
    EZrFfiContractStatus status;

    TEST_ASSERT_NOT_NULL(semanticContext);
    status = ZrParser_FfiContract_Build(
            semanticContext,
            externBlock,
            declaration,
            outContract,
            diagnostic);
    ZrParser_SemanticContext_Free(semanticContext);
    return status;
}

static SZrAstNode *first_extern_function(SZrAstNode *script) {
    SZrAstNode *block;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1u, script->data.script.statements->count);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, block->type);
    TEST_ASSERT_NOT_NULL(block->data.externBlock.declarations);
    TEST_ASSERT_EQUAL_UINT64(1u, block->data.externBlock.declarations->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_EXTERN_FUNCTION_DECLARATION,
            block->data.externBlock.declarations->nodes[0]->type);
    return block->data.externBlock.declarations->nodes[0];
}

static TZrUInt64 native_extern_canonical_layout_hash(
        const SZrFfiSignatureContract *signature,
        const SZrFfiTypeContract *type) {
    SZrTypeLayoutField fields[ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS];
    SZrTypeLayout layout;

    TEST_ASSERT_NOT_NULL(signature);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, type->aggregateFieldCount);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
            signature->aggregateFieldCount,
            type->aggregateFieldStart + type->aggregateFieldCount);
    memset(fields, 0, sizeof(fields));
    for (TZrUInt32 index = 0u; index < type->aggregateFieldCount; index++) {
        const SZrFfiAggregateFieldContract *field =
                &signature->aggregateFields[type->aggregateFieldStart + index];

        fields[index].byteOffset = field->offset;
        fields[index].byteSize = field->size;
    }
    if (type->typeKind == ZR_FFI_CONTRACT_TYPE_UNION) {
        ZrCore_TypeLayout_InitUnion(
                &layout,
                type->size,
                type->alignment,
                0u,
                0u,
                ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                fields,
                type->aggregateFieldCount);
    } else {
        ZrCore_TypeLayout_InitStruct(
                &layout,
                type->size,
                type->alignment,
                ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                fields,
                type->aggregateFieldCount);
    }
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    TEST_ASSERT_TRUE(layout.blittable);
    return layout.layoutHash;
}

static SZrAstNode *find_extern_function(
        SZrAstNode *script,
        const TZrChar *name) {
    SZrAstNode *block;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1u, script->data.script.statements->count);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, block->type);
    TEST_ASSERT_NOT_NULL(block->data.externBlock.declarations);
    for (TZrSize index = 0u;
         index < block->data.externBlock.declarations->count;
         index++) {
        SZrAstNode *declaration = block->data.externBlock.declarations->nodes[index];

        if (declaration != ZR_NULL &&
            declaration->type == ZR_AST_EXTERN_FUNCTION_DECLARATION &&
            declaration->data.externFunctionDeclaration.name != ZR_NULL &&
            declaration->data.externFunctionDeclaration.name->name != ZR_NULL &&
            strcmp(
                    ZrCore_String_GetNativeString(
                            declaration->data.externFunctionDeclaration.name->name),
                    name) == 0) {
            return declaration;
        }
    }
    TEST_FAIL_MESSAGE("native extern function not found");
    return ZR_NULL;
}

static void test_native_extern_builds_persistent_scalar_contract(void) {
    static const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")#\n"
            "  #zr.ffi.callingConvention(\"c\")#\n"
            "  pub fn Add(lhs: i32, rhs: i32): i32;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrAstNode *declaration;
    SZrAstNode *block;
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, source, "native_extern_contract.zr");
    TEST_ASSERT_NOT_NULL(script);
    declaration = first_extern_function(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_ACCESS_PUBLIC,
            declaration->data.externFunctionDeclaration.accessModifier);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    declaration,
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_STRING("fixture", contract.libraryLocator);
    TEST_ASSERT_EQUAL_STRING("zr_ffi_add_i32", contract.entryPoint);
    TEST_ASSERT_EQUAL_INT(ZR_FFI_CONTRACT_ABI_C, contract.signature.abi);
    TEST_ASSERT_EQUAL_UINT32(2u, contract.signature.parameterCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_I32, contract.signature.returnType.typeKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_I32, contract.signature.parameters[0].type.typeKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_DIRECTION_IN, contract.signature.parameters[0].direction);
    TEST_ASSERT_EQUAL_HEX64(
            ZR_NATIVE_EXTERN_SCALAR_CONTRACT_HASH,
            contract.signature.signatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(
            contract.signature.signatureHash,
            contract.callable.contractHash);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FFI_CONTRACT_AVAILABILITY_ALL, contract.availability);
    TEST_ASSERT_BITS_HIGH(
            ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME,
            contract.requiredCapabilities);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, contract.declaringModuleId);
    TEST_ASSERT_EQUAL_STRING(
            "native_extern_contract.zr", contract.sourceMapping.document);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(
            contract.sourceMapping.endOffset,
            contract.sourceMapping.startOffset);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            ZrParser_FfiContract_Validate(&contract, &diagnostic));

    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_preserves_ref_and_out_directions(void) {
    static const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "  fn Update(value: ref i32, result: out i32): bool;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrAstNode *block;
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, source, "native_extern_directions.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    first_extern_function(script),
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_DIRECTION_REF, contract.signature.parameters[0].direction);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_DIRECTION_OUT, contract.signature.parameters[1].direction);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_BOOL, contract.signature.returnType.typeKind);
    {
        SZrNativeImportContract corrupt = contract;

        corrupt.callable.parameters[0].passingForm =
                ZR_FFI_CALLABLE_PASSING_VALUE;
        corrupt.callable.parameters[0].escapeUpperBound =
                ZR_FFI_CALLABLE_ESCAPE_FUNCTION;
        corrupt.callable.parameters[0].acceptsTemporary = ZR_TRUE;
        corrupt.callable.parameters[0].callSiteMarker =
                ZR_FFI_CALLABLE_CALL_SITE_NONE;
        corrupt.callable.contractHash =
                ZrCommon_FfiCallableContract_ComputeHash(&corrupt.callable);
        TEST_ASSERT_TRUE(
                ZrCommon_FfiCallableContract_Validate(&corrupt.callable));
        TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&corrupt));
    }

    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_rejects_corrupt_persistent_contract(void) {
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;

    memset(&contract, 0, sizeof(contract));
    contract.schemaVersion = ZR_FFI_CONTRACT_SCHEMA_VERSION;
    contract.signature.parameterCount = ZR_FFI_CONTRACT_MAX_PARAMETERS + 1u;
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_PARAMETER_LIMIT,
            ZrParser_FfiContract_Validate(&contract, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FFI_CONTRACT_MAX_PARAMETERS + 1u,
            diagnostic.parameterIndex);

    memset(&contract, 0, sizeof(contract));
    contract.schemaVersion = ZR_FFI_CONTRACT_SCHEMA_VERSION;
    strcpy(contract.libraryLocator, "fixture");
    strcpy(contract.entryPoint, "invalid_enum");
    strcpy(contract.sourceMapping.document, "invalid_enum.zro");
    contract.symbolId = 1u;
    contract.declaringModuleId = 1u;
    contract.availability = ZR_FFI_CONTRACT_AVAILABILITY_ALL;
    contract.requiredCapabilities = ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME;
    contract.signature.abi = ZR_FFI_CONTRACT_ABI_C;
    contract.signature.targetPointerSize = (TZrUInt32)sizeof(TZrPtr);
    contract.signature.targetEndianness = ZR_FFI_CONTRACT_ENDIAN_LITTLE;
    strcpy(
            contract.signature.targetTriple,
            ZrCommon_FfiContract_GetHostTargetTriple());
    contract.signature.targetAbiHash =
            ZrCommon_FfiContract_ComputeTargetAbiHash(
                    contract.signature.abi,
                    contract.signature.targetPointerSize,
                    contract.signature.targetEndianness,
                    contract.signature.targetTriple);
    contract.signature.charset = ZR_FFI_CONTRACT_CHARSET_NONE;
    contract.signature.errorPolicy = ZR_FFI_CONTRACT_ERROR_NONE;
    contract.signature.cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_NONE;
    contract.signature.callbackLifetime = ZR_FFI_CONTRACT_CALLBACK_LIFETIME_NONE;
    contract.signature.callbackThreadPolicy = ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE;
    contract.signature.callbackExceptionPolicy = ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE;
    contract.signature.returnType.typeKind = ZR_FFI_CONTRACT_TYPE_VOID;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    contract.callable.returnTypeHash = 1u;
    contract.callable.contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(&contract.callable);
    TEST_ASSERT_TRUE(ZrCommon_NativeImportContract_Validate(&contract));

    contract.signature.abi = (EZrFfiAbi)(TZrUInt32)UINT32_MAX;
    TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&contract));
    TEST_ASSERT_NOT_EQUAL(
            ZR_FFI_CONTRACT_STATUS_OK,
            ZrParser_FfiContract_Validate(&contract, &diagnostic));

    contract.signature.abi = ZR_FFI_CONTRACT_ABI_C;
    contract.signature.targetAbiHash =
            ZrCommon_FfiContract_ComputeTargetAbiHash(
                    contract.signature.abi,
                    contract.signature.targetPointerSize,
                    contract.signature.targetEndianness,
                    contract.signature.targetTriple);
    strcpy(contract.signature.targetTriple, "x86_64-unknown-invalid");
    contract.signature.targetAbiHash =
            ZrCommon_FfiContract_ComputeTargetAbiHash(
                    contract.signature.abi,
                    contract.signature.targetPointerSize,
                    contract.signature.targetEndianness,
                    contract.signature.targetTriple);
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&contract));
    strcpy(
            contract.signature.targetTriple,
            ZrCommon_FfiContract_GetHostTargetTriple());
    contract.signature.targetAbiHash =
            ZrCommon_FfiContract_ComputeTargetAbiHash(
                    contract.signature.abi,
                    contract.signature.targetPointerSize,
                    contract.signature.targetEndianness,
                    contract.signature.targetTriple);
    contract.signature.returnType.typeKind =
            (EZrFfiTypeKind)(TZrUInt32)UINT32_MAX;
    TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&contract));
    TEST_ASSERT_NOT_EQUAL(
            ZR_FFI_CONTRACT_STATUS_OK,
            ZrParser_FfiContract_Validate(&contract, &diagnostic));
}

static void test_native_extern_persists_availability_and_capabilities(void) {
    static const TZrChar *validSource =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.platform(\"windows\")#\n"
            "  #zr.ffi.requiredCapabilities(16)#\n"
            "  fn OnlyWindows(value: i32): i32;\n"
            "}\n";
    static const TZrChar *invalidPlatformSource =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.platform(\"plan9\")#\n"
            "  fn Bad(value: i32): i32;\n"
            "}\n";
    static const TZrChar *missingFfiCapabilitySource =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.requiredCapabilities(0)#\n"
            "  fn Bad(value: i32): i32;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;
    const TZrChar *invalidSources[] = {
            invalidPlatformSource,
            missingFfiCapabilitySource,
    };

    TEST_ASSERT_NOT_NULL(state);
    {
        SZrAstNode *script = parse_source(
                state, validSource, "native_extern_availability.zr");
        SZrAstNode *block;

        TEST_ASSERT_NOT_NULL(script);
        block = script->data.script.statements->nodes[0];
        TEST_ASSERT_EQUAL_INT(
                ZR_FFI_CONTRACT_STATUS_OK,
                native_extern_build_contract(state,
                        &block->data.externBlock,
                        first_extern_function(script),
                        &contract,
                        &diagnostic));
        TEST_ASSERT_EQUAL_UINT32(
                ZR_FFI_CONTRACT_AVAILABILITY_WINDOWS,
                contract.availability);
        TEST_ASSERT_EQUAL_UINT64(
                ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME,
                contract.requiredCapabilities);
        TEST_ASSERT_TRUE(ZrCommon_NativeImportContract_Validate(&contract));
        contract.availability = 0u;
        TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&contract));
        ZrParser_Ast_Free(state, script);
    }
    for (TZrSize index = 0u;
         index < sizeof(invalidSources) / sizeof(invalidSources[0]);
         index++) {
        SZrAstNode *script = parse_source(
                state, invalidSources[index], "native_extern_invalid_policy.zr");
        SZrAstNode *block;

        TEST_ASSERT_NOT_NULL(script);
        block = script->data.script.statements->nodes[0];
        TEST_ASSERT_EQUAL_INT(
                ZR_FFI_CONTRACT_STATUS_INVALID_POLICY,
                native_extern_build_contract(state,
                        &block->data.externBlock,
                        first_extern_function(script),
                        &contract,
                        &diagnostic));
        ZrParser_Ast_Free(state, script);
    }
    native_extern_destroy_state(state);
}

static void test_native_extern_classifies_aggregate_union_and_target_contract(void) {
    static const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "  struct Point { var x: i32; var y: i32; }\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "    #zr.ffi.offset(0)# var scalar: f32;\n"
            "  }\n"
            "  fn SumPoint(point: Point): i32;\n"
            "  fn ReadValue(value: Value): i32;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrAstNode *block;
    SZrNativeImportContract structContract;
    SZrNativeImportContract unionContract;
    SZrNativeImportContract misalignedContract;
    TZrChar errorBuffer[256];
    TZrUInt64 legacyTargetHash;
    SZrFfiContractDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, source, "native_extern_aggregate.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "SumPoint"),
                    &structContract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_STRUCT,
            structContract.signature.parameters[0].type.typeKind);
    TEST_ASSERT_EQUAL_UINT32(8u, structContract.signature.parameters[0].type.size);
    TEST_ASSERT_EQUAL_UINT32(4u, structContract.signature.parameters[0].type.alignment);
    TEST_ASSERT_NOT_EQUAL_UINT64(
            0u, structContract.signature.parameters[0].type.layoutHash);
    TEST_ASSERT_EQUAL_UINT64(
            native_extern_canonical_layout_hash(
                    &structContract.signature,
                    &structContract.signature.parameters[0].type),
            structContract.signature.parameters[0].type.layoutHash);
    TEST_ASSERT_EQUAL_UINT32(
            2u,
            structContract.signature.parameters[0].type.aggregateFieldCount);
    TEST_ASSERT_EQUAL_STRING(
            "x",
            structContract.signature.aggregateFields[
                    structContract.signature.parameters[0].type.aggregateFieldStart]
                    .name);
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            structContract.signature.aggregateFields[
                    structContract.signature.parameters[0].type.aggregateFieldStart]
                    .offset);
    TEST_ASSERT_EQUAL_STRING(
            "y",
            structContract.signature.aggregateFields[
                    structContract.signature.parameters[0].type.aggregateFieldStart + 1u]
                    .name);
    TEST_ASSERT_EQUAL_UINT32(
            4u,
            structContract.signature.aggregateFields[
                    structContract.signature.parameters[0].type.aggregateFieldStart + 1u]
                    .offset);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrPtr), structContract.signature.targetPointerSize);
    TEST_ASSERT_EQUAL_STRING(
            ZrCommon_FfiContract_GetHostTargetTriple(),
            structContract.signature.targetTriple);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, structContract.signature.targetAbiHash);
    legacyTargetHash = ZR_FFI_CONTRACT_FNV_OFFSET;
    legacyTargetHash = ZrCommon_FfiContract_HashU32(
            legacyTargetHash, (TZrUInt32)structContract.signature.abi);
    legacyTargetHash = ZrCommon_FfiContract_HashU32(
            legacyTargetHash, structContract.signature.targetPointerSize);
    legacyTargetHash = ZrCommon_FfiContract_HashU32(
            legacyTargetHash,
            (TZrUInt32)structContract.signature.targetEndianness);
    TEST_ASSERT_NOT_EQUAL_UINT64(
            legacyTargetHash, structContract.signature.targetAbiHash);

    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "ReadValue"),
                    &unionContract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_UNION,
            unionContract.signature.parameters[0].type.typeKind);
    TEST_ASSERT_EQUAL_UINT32(4u, unionContract.signature.parameters[0].type.size);
    TEST_ASSERT_EQUAL_UINT32(4u, unionContract.signature.parameters[0].type.alignment);
    TEST_ASSERT_EQUAL_UINT32(
            2u,
            unionContract.signature.parameters[0].type.aggregateFieldCount);
    TEST_ASSERT_EQUAL_UINT64(
            native_extern_canonical_layout_hash(
                    &unionContract.signature,
                    &unionContract.signature.parameters[0].type),
            unionContract.signature.parameters[0].type.layoutHash);
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            unionContract.signature.aggregateFields[
                    unionContract.signature.parameters[0].type.aggregateFieldStart + 1u]
                    .offset);
    TEST_ASSERT_NOT_EQUAL_UINT64(
            structContract.signature.parameters[0].type.layoutHash,
            unionContract.signature.parameters[0].type.layoutHash);

    misalignedContract = structContract;
    misalignedContract.signature.aggregateFields[
            misalignedContract.signature.parameters[0].type.aggregateFieldStart +
            1u]
            .offset = 3u;
    misalignedContract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(
                    &misalignedContract.signature);
    TEST_ASSERT_TRUE(
            ZrCommon_NativeImportContract_Validate(&misalignedContract));
    TEST_ASSERT_FALSE(
            ZrVmLibFfi_ValidateNativeImportContract(
                    &misalignedContract,
                    errorBuffer,
                    sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "layout"));

    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_requires_explicit_callback_policy(void) {
    static const TZrChar *missingPolicySource =
            "native extern(\"fixture\") {\n"
            "  delegate Unary(value: f64): f64;\n"
            "  fn Apply(value: f64, callback: Unary): f64;\n"
            "}\n";
    static const TZrChar *explicitPolicySource =
            "native extern(\"fixture\") {\n"
            "  delegate Unary(value: f64): f64;\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  fn Apply(value: f64, callback: Unary): f64;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrAstNode *block;
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, missingPolicySource, "native_extern_callback_missing_policy.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_CALLBACK_POLICY_REQUIRED,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "Apply"),
                    &contract,
                    &diagnostic));
    ZrParser_Ast_Free(state, script);

    script = parse_source(state, explicitPolicySource, "native_extern_callback_policy.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "Apply"),
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_CALLBACK,
            contract.signature.parameters[1].type.typeKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_CALLBACK_LIFETIME_CALL,
            contract.signature.callbackLifetime);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_CALLBACK_THREAD_CALLER,
            contract.signature.callbackThreadPolicy);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_RETURN_DEFAULT,
            contract.signature.callbackExceptionPolicy);

    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_policy_contract_admission(void) {
    static const TZrChar *callbackSource =
            "native extern(\"fixture\") {\n"
            "  delegate Unary(value: f64): f64;\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  fn Apply(value: f64, callback: Unary): f64;\n"
            "}\n";
    static const TZrChar *scalarSource =
            "native extern(\"fixture\") { fn Run(value: i32): i32; }\n";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrAstNode *block;
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;
    TZrChar errorBuffer[256];

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, callbackSource, "native_extern_callback_policy_runtime.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "Apply"),
                    &contract,
                    &diagnostic));
    contract.signature.callbackThreadPolicy =
            ZR_FFI_CONTRACT_CALLBACK_THREAD_ATTACH;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    TEST_ASSERT_TRUE(ZrCommon_NativeImportContract_Validate(&contract));
    TEST_ASSERT_FALSE(ZrVmLibFfi_ValidateNativeImportContract(
            &contract, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "callback policy"));
    ZrParser_Ast_Free(state, script);

    script = parse_source(state, scalarSource, "native_extern_scalar_policy_runtime.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "Run"),
                    &contract,
                    &diagnostic));
    contract.signature.cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_REGISTERED;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&contract));
    TEST_ASSERT_FALSE(ZrVmLibFfi_ValidateNativeImportContract(
            &contract, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "canonical native import contract"));

    contract.signature.cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_NONE;
    contract.signature.errorPolicy = ZR_FFI_CONTRACT_ERROR_THROWS;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&contract));
    TEST_ASSERT_FALSE(ZrVmLibFfi_ValidateNativeImportContract(
            &contract, errorBuffer, sizeof(errorBuffer)));
    TEST_ASSERT_NOT_NULL(strstr(errorBuffer, "canonical native import contract"));

    contract.signature.errorPolicy = ZR_FFI_CONTRACT_ERROR_RETURN_CODE;
    contract.signature.cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_CALLEE;
    contract.signature.signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
    TEST_ASSERT_TRUE(ZrCommon_NativeImportContract_Validate(&contract));
    TEST_ASSERT_TRUE(ZrVmLibFfi_ValidateNativeImportContract(
            &contract, errorBuffer, sizeof(errorBuffer)));

    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_rejects_managed_types_and_invalid_abi(void) {
    static const TZrChar *spanSource =
            "native extern(\"fixture\") { fn Bad(values: Span<i32>): i32; }\n";
    static const TZrChar *ownerSource =
            "native extern(\"fixture\") { fn Bad(value: Unique<i32>): i32; }\n";
    static const TZrChar *abiSource =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.callingConvention(\"unknown\")#\n"
            "  fn Bad(value: i32): i32;\n"
            "}\n";
    static const TZrChar *voidSource =
            "native extern(\"fixture\") { fn Notify(value: i32); }\n";
    const TZrChar *sources[] = {spanSource, ownerSource};
    const EZrFfiContractStatus expectedStatuses[] = {
            ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE,
            ZR_FFI_CONTRACT_STATUS_FORBIDDEN_MANAGED_TYPE};
    SZrState *state = native_extern_create_state();
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    for (TZrSize index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrAstNode *script = parse_source(state, sources[index], "native_extern_forbidden.zr");
        SZrAstNode *block;

        TEST_ASSERT_NOT_NULL(script);
        block = script->data.script.statements->nodes[0];
        TEST_ASSERT_EQUAL_INT(
                expectedStatuses[index],
                native_extern_build_contract(state,
                        &block->data.externBlock,
                        find_extern_function(script, "Bad"),
                        &contract,
                        &diagnostic));
        ZrParser_Ast_Free(state, script);
    }
    {
        SZrAstNode *script = parse_source(state, abiSource, "native_extern_invalid_abi.zr");
        SZrAstNode *block;

        TEST_ASSERT_NOT_NULL(script);
        block = script->data.script.statements->nodes[0];
        TEST_ASSERT_EQUAL_INT(
                ZR_FFI_CONTRACT_STATUS_INVALID_ABI,
                native_extern_build_contract(state,
                        &block->data.externBlock,
                        find_extern_function(script, "Bad"),
                        &contract,
                        &diagnostic));
        ZrParser_Ast_Free(state, script);
    }
    {
        SZrAstNode *script = parse_source(state, voidSource, "native_extern_void.zr");
        SZrAstNode *block;

        TEST_ASSERT_NOT_NULL(script);
        block = script->data.script.statements->nodes[0];
        TEST_ASSERT_EQUAL_INT(
                ZR_FFI_CONTRACT_STATUS_OK,
                native_extern_build_contract(state,
                        &block->data.externBlock,
                        find_extern_function(script, "Notify"),
                        &contract,
                        &diagnostic));
        TEST_ASSERT_EQUAL_INT(
                ZR_FFI_CONTRACT_TYPE_VOID, contract.signature.returnType.typeKind);
        ZrParser_Ast_Free(state, script);
    }
    native_extern_destroy_state(state);
}

static void test_native_extern_accepts_cdecl_callconv_alias(void) {
    static const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.callconv(\"cdecl\")#\n"
            "  fn Call(value: i32): i32;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;
    SZrAstNode *script;
    SZrAstNode *block;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, source, "native_extern_cdecl_alias.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(
                    state,
                    &block->data.externBlock,
                    find_extern_function(script, "Call"),
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_FFI_CONTRACT_ABI_C, contract.signature.abi);

    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_accepts_blittable_local_type_regardless_of_name(void) {
    static const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "  struct Span { var value: i32; }\n"
            "  fn read(value: Span): i32;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrAstNode *block;
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, source, "native_extern_local_span.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "read"),
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_STRUCT,
            contract.signature.parameters[0].type.typeKind);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE,
            contract.signature.parameters[0].type.flags &
                    ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE);

    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_callable_hash_preserves_passing_semantics(void) {
    static const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "  fn byValue(value: i32): i32;\n"
            "  fn byIn(value: in i32): i32;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrAstNode *block;
    SZrNativeImportContract valueContract;
    SZrNativeImportContract inContract;
    SZrFfiContractDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, source, "native_extern_callable_semantics.zr");
    TEST_ASSERT_NOT_NULL(script);
    block = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "byValue"),
                    &valueContract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_STATUS_OK,
            native_extern_build_contract(state,
                    &block->data.externBlock,
                    find_extern_function(script, "byIn"),
                    &inContract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT64(
            valueContract.signature.signatureHash,
            inContract.signature.signatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(
            valueContract.callable.contractHash,
            inContract.callable.contractHash);

    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_current_syntax_executes_static_symbol(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")#\n"
            "  pub fn Add(lhs: i32, rhs: i32): i32;\n"
            "}\n"
            "return Add(19, 23);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_execute.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, function->nativeImportContractLength);
    TEST_ASSERT_NOT_NULL(function->nativeImportContracts);
    TEST_ASSERT_EQUAL_STRING(
            "zr_ffi_add_i32",
            function->nativeImportContracts[0].entryPoint);
    TEST_ASSERT_NOT_EQUAL_UINT64(
            function->nativeImportContracts[0].signature.signatureHash,
            function->nativeImportContracts[0].callable.contractHash);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(
            ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(
            42,
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                    ? (TZrInt64)result.value.nativeObject.nativeUInt64
                    : result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_current_syntax_executes_callback_contract(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  delegate Unary(value: f64): f64;\n"
            "  #zr.ffi.entry(\"zr_ffi_apply_callback\")#\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  pub fn Apply(value: f64, callback: Unary): f64;\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var callback = ffi.callback(Unary, fn(value) => {\n"
            "  return value * 2.0;\n"
            "});\n"
            "return Apply(5.0, callback);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_callback_execute.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, function->nativeImportContractLength);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_CALLBACK,
            function->nativeImportContracts[0].signature.parameters[1].type.typeKind);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(
            0.0001, 10.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_callback_exception_returns_default(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  delegate Unary(value: f64): f64;\n"
            "  #zr.ffi.entry(\"zr_ffi_apply_callback\")#\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  pub fn Apply(value: f64, callback: Unary): f64;\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var callback = ffi.callback(Unary, fn(value) => {\n"
            "  throw \"callback failed\";\n"
            "});\n"
            "return Apply(5.0, callback);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_callback_exception.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(
            0.0001, 0.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_rejects_mismatched_callback_signature(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  delegate Unary(value: f64): f64;\n"
            "  delegate Wrong(value: i32): i32;\n"
            "  #zr.ffi.entry(\"zr_ffi_apply_callback\")#\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  pub fn Apply(value: f64, callback: Unary): f64;\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var callback = ffi.callback(Wrong, fn(value) => {\n"
            "  return value + 1;\n"
            "});\n"
            "return Apply(5.0, callback);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_callback_mismatch.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_Execute(state, function, &result));

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_call_lifetime_rejects_late_callback(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  delegate Unary(value: f64): f64;\n"
            "  #zr.ffi.entry(\"zr_ffi_store_callback\")#\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  pub fn Store(callback: Unary);\n"
            "  #zr.ffi.entry(\"zr_ffi_invoke_stored_callback\")#\n"
            "  pub fn Invoke(value: f64): f64;\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var calls = 0;\n"
            "var callback = ffi.callback(Unary, fn(value) => {\n"
            "  calls = calls + 1;\n"
            "  return value * 2.0;\n"
            "});\n"
            "Store(callback);\n"
            "var ignored = Invoke(5.0);\n"
            "return calls;\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_callback_lifetime.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(
            ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(
            0,
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                    ? (TZrInt64)result.value.nativeObject.nativeUInt64
                    : result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_call_lifetime_callback_supports_reentrancy(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  delegate Unary(value: f64): f64;\n"
            "  #zr.ffi.entry(\"zr_ffi_apply_callback\")#\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  pub fn Apply(value: f64, callback: Unary): f64;\n"
            "  #zr.ffi.entry(\"zr_ffi_apply_callback_twice\")#\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  pub fn ApplyTwice(value: f64, callback: Unary): f64;\n"
            "}\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var callback: ffi.CallbackHandle = null;\n"
            "callback = ffi.callback(Unary, fn(value) => {\n"
            "  if (value == 1.0) { return Apply(0.0, callback); }\n"
            "  return value * 2.0;\n"
            "});\n"
            "return ApplyTwice(1.0, callback);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_callback_reentrancy.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(
            0.0001, 4.5, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_mixed_union_uses_integer_abi_class(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var scalar: f64;\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "  }\n"
            "  #zr.ffi.entry(\"zr_ffi_read_mixed_union\")#\n"
            "  pub fn Read(value: Value): i32;\n"
            "}\n"
            "var value = new Value();\n"
            "value.scalar = 0.0;\n"
            "value.integer = 42;\n"
            "return Read(value);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_mixed_union.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(
            ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(
            42,
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                    ? (TZrInt64)result.value.nativeObject.nativeUInt64
                    : result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_heterogeneous_float_union_uses_integer_abi_class(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var narrow: f32;\n"
            "    #zr.ffi.offset(0)# var wide: f64;\n"
            "  }\n"
            "  #zr.ffi.entry(\"zr_ffi_read_heterogeneous_float_union\")#\n"
            "  pub fn Read(value: Value): i32;\n"
            "}\n"
            "var value = new Value();\n"
            "value.wide = 42.5;\n"
            "return Read(value);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_heterogeneous_float_union.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(
            ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(
            42,
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                    ? (TZrInt64)result.value.nativeObject.nativeUInt64
                    : result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_union_first_member_is_not_overwritten(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var scalar: f64;\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "  }\n"
            "  #zr.ffi.entry(\"zr_ffi_read_mixed_union_scalar\")#\n"
            "  pub fn Read(value: Value): i32;\n"
            "}\n"
            "var value = new Value();\n"
            "value.scalar = 42.5;\n"
            "return Read(value);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(
            escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0,
            snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(
            state, source, "native_extern_mixed_union_first_member.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(
            ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(
            42,
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                    ? (TZrInt64)result.value.nativeObject.nativeUInt64
                    : result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_union_rejects_ambiguous_active_member(void) {
    static const TZrChar *sourceFormats[] = {
            "native extern(\"%s\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var scalar: f64;\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "  }\n"
            "  #zr.ffi.entry(\"zr_ffi_read_mixed_union_scalar\")#\n"
            "  pub fn Read(value: Value): i32;\n"
            "}\n"
            "var value = new Value();\n"
            "return Read(value);\n",
            "native extern(\"%s\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var scalar: f64;\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "  }\n"
            "  #zr.ffi.entry(\"zr_ffi_read_mixed_union_scalar\")#\n"
            "  pub fn Read(value: Value): i32;\n"
            "}\n"
            "var value = new Value();\n"
            "value.scalar = 42.5;\n"
            "value.integer = 42;\n"
            "return Read(value);\n"};
    static const TZrChar *sourceNames[] = {
            "native_extern_union_without_active_member.zr",
            "native_extern_union_with_ambiguous_active_member.zr"};

    for (TZrSize index = 0;
         index < sizeof(sourceFormats) / sizeof(sourceFormats[0]);
         index++) {
        TZrChar escapedPath[4096];
        TZrChar source[8192];
        SZrState *state = native_extern_create_state();
        SZrAstNode *script;
        SZrFunction *function;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);
        escape_zr_string(
                escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
        TEST_ASSERT_GREATER_THAN_INT(
                0,
                snprintf(
                        source,
                        sizeof(source),
                        sourceFormats[index],
                        escapedPath));
        script = parse_source(state, source, sourceNames[index]);
        TEST_ASSERT_NOT_NULL(script);
        function = ZrParser_Compiler_Compile(state, script);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_FALSE(
                ZrTests_Runtime_Function_Execute(state, function, &result));

        ZrCore_Function_Free(state, function);
        ZrParser_Ast_Free(state, script);
        native_extern_destroy_state(state);
    }
}

static void test_native_extern_rejects_union_return_and_writeback(void) {
    static const TZrChar *sourceFormats[] = {
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var scalar: f64;\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "  }\n"
            "  fn Bad(): Value;\n"
            "}\n",
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var scalar: f64;\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "  }\n"
            "  fn Bad(value: ref Value);\n"
            "}\n",
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var scalar: f64;\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "  }\n"
            "  fn Bad(value: out Value);\n"
            "}\n"};
    static const EZrFfiContractStatus expectedStatuses[] = {
            ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE,
            ZR_FFI_CONTRACT_STATUS_INVALID_DIRECTION,
            ZR_FFI_CONTRACT_STATUS_INVALID_DIRECTION};
    static const TZrChar *inputSource =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var scalar: f64;\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "  }\n"
            "  fn Read(value: Value): i32;\n"
            "}\n";
    SZrState *state = native_extern_create_state();
    SZrNativeImportContract contract;
    SZrFfiContractDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    for (TZrSize index = 0;
         index < sizeof(sourceFormats) / sizeof(sourceFormats[0]);
         index++) {
        SZrAstNode *script = parse_source(
                state, sourceFormats[index], "native_extern_union_direction.zr");
        SZrAstNode *block;

        TEST_ASSERT_NOT_NULL(script);
        block = script->data.script.statements->nodes[0];
        TEST_ASSERT_EQUAL_INT(
                expectedStatuses[index],
                native_extern_build_contract(state,
                        &block->data.externBlock,
                        find_extern_function(script, "Bad"),
                        &contract,
                        &diagnostic));
        ZrParser_Ast_Free(state, script);
    }
    {
        SZrAstNode *script = parse_source(
                state, inputSource, "native_extern_union_input_only.zr");
        SZrAstNode *block;

        TEST_ASSERT_NOT_NULL(script);
        block = script->data.script.statements->nodes[0];
        TEST_ASSERT_EQUAL_INT(
                ZR_FFI_CONTRACT_STATUS_OK,
                native_extern_build_contract(state,
                        &block->data.externBlock,
                        find_extern_function(script, "Read"),
                        &contract,
                        &diagnostic));
        contract.signature.returnType = contract.signature.parameters[0].type;
        contract.signature.signatureHash =
                ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
        TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&contract));

        TEST_ASSERT_EQUAL_INT(
                ZR_FFI_CONTRACT_STATUS_OK,
                native_extern_build_contract(state,
                        &block->data.externBlock,
                        find_extern_function(script, "Read"),
                        &contract,
                        &diagnostic));
        contract.signature.parameters[0].direction =
                ZR_FFI_CONTRACT_DIRECTION_REF;
        contract.signature.signatureHash =
                ZrCommon_FfiSignatureContract_ComputeHash(&contract.signature);
        TEST_ASSERT_FALSE(ZrCommon_NativeImportContract_Validate(&contract));
        ZrParser_Ast_Free(state, script);
    }
    native_extern_destroy_state(state);
}

static void test_native_extern_errno_policy_raises_runtime_error(void) {
#if defined(_WIN32) && !defined(_DLL)
    TEST_IGNORE_MESSAGE(
            "Windows static CRTs keep errno private to each DLL; use lastError for that ABI");
#endif
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_set_errno_i32\")#\n"
            "  #zr.ffi.errorPolicy(\"errno\")#\n"
            "  pub fn Fail(code: i32): i32;\n"
            "}\n"
            "return Fail(5);\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_errno.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_Execute(state, function, &result));

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_return_code_and_callee_cleanup_execute(void) {
    static const TZrChar *failureSourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_return_status_i32\")#\n"
            "  #zr.ffi.errorPolicy(\"returnCode\")#\n"
            "  #zr.ffi.cleanup(\"callee\")#\n"
            "  pub fn Status(code: i32): i32;\n"
            "}\n"
            "return Status(7);\n";
    static const TZrChar *successSourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_return_status_i32\")#\n"
            "  #zr.ffi.errorPolicy(\"returnCode\")#\n"
            "  #zr.ffi.cleanup(\"callee\")#\n"
            "  pub fn Status(code: i32): i32;\n"
            "}\n"
            "return Status(0);\n";
    const TZrChar *formats[] = {failureSourceFormat, successSourceFormat};
    TZrChar escapedPath[4096];
    TZrChar source[8192];

    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    for (TZrSize index = 0u; index < 2u; index++) {
        SZrState *state = native_extern_create_state();
        SZrAstNode *script;
        SZrFunction *function;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_GREATER_THAN_INT(
                0, snprintf(source, sizeof(source), formats[index], escapedPath));
        script = parse_source(state, source, "native_extern_return_code.zr");
        TEST_ASSERT_NOT_NULL(script);
        function = ZrParser_Compiler_Compile(state, script);
        TEST_ASSERT_NOT_NULL(function);
        if (index == 0u) {
            TEST_ASSERT_FALSE(
                    ZrTests_Runtime_Function_Execute(state, function, &result));
        } else {
            TEST_ASSERT_TRUE(
                    ZrTests_Runtime_Function_Execute(state, function, &result));
            TEST_ASSERT_TRUE(
                    ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
                    ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
            TEST_ASSERT_EQUAL_INT64(
                    0,
                    ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                            ? (TZrInt64)result.value.nativeObject.nativeUInt64
                            : result.value.nativeObject.nativeInt64);
        }
        ZrCore_Function_Free(state, function);
        ZrParser_Ast_Free(state, script);
        native_extern_destroy_state(state);
    }
}

static void test_native_extern_current_syntax_executes_ref_contract(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_increment_i32\")#\n"
            "  pub fn Increment(value: ref i32): i32;\n"
            "}\n"
            "var value: i32 = 41;\n"
            "var returned = Increment(ref value);\n"
            "return value + returned;\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_ref_execute.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(
            ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(
            84,
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                    ? (TZrInt64)result.value.nativeObject.nativeUInt64
                    : result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_current_syntax_executes_out_contract(void) {
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_increment_i32\")#\n"
            "  pub fn Initialize(value: out i32): i32;\n"
            "}\n"
            "var value: i32 = 41;\n"
            "var returned = Initialize(out value);\n"
            "return value + returned;\n";
    TZrChar escapedPath[4096];
    TZrChar source[8192];
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0, snprintf(source, sizeof(source), sourceFormat, escapedPath));
    script = parse_source(state, source, "native_extern_out_execute.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(
            ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(
            2,
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                    ? (TZrInt64)result.value.nativeObject.nativeUInt64
                    : result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_native_extern_aot_uses_canonical_signature_vector(void) {
    static const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "  struct Point { var x: i32; var y: i32; }\n"
            "  #zr.ffi.kind(\"union\")#\n"
            "  struct Value {\n"
            "    #zr.ffi.offset(0)# var integer: i32;\n"
            "    #zr.ffi.offset(0)# var scalar: f32;\n"
            "  }\n"
            "  delegate Unary(value: f64): f64;\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")#\n"
            "  #zr.ffi.callingConvention(\"c\")#\n"
            "  pub fn Add(lhs: i32, rhs: i32): i32;\n"
            "  fn SumPoint(point: Point): i32;\n"
            "  fn ReadValue(value: Value): i32;\n"
            "  fn Update(value: ref i32, result: out i32): bool;\n"
            "  #zr.ffi.callbackLifetime(\"call\")#\n"
            "  #zr.ffi.callbackThread(\"caller\")#\n"
            "  #zr.ffi.callbackException(\"returnDefault\")#\n"
            "  fn Apply(value: f64, callback: Unary): f64;\n"
            "  #zr.ffi.errorPolicy(\"errno\")#\n"
            "  #zr.ffi.cleanup(\"caller\")#\n"
            "  fn MayFail(code: i32): i32;\n"
            "}\n"
            "return 0;\n";
    static const TZrChar *aotPath = "native_extern_contract_aot.c";
    static const TZrChar *llvmPath = "native_extern_contract_aot.ll";
#if defined(ZR_PLATFORM_UNIX)
    static const TZrChar *sharedPath = "./native_extern_contract_aot.so";
    typedef const ZrAotCompiledModule *(*FGetAotModule)(void);
    TZrChar command[4096];
    void *library;
    void *symbol;
    FGetAotModule getModule = ZR_NULL;
    const ZrAotCompiledModule *module;
#endif
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    TZrChar *generatedText;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, source, "native_extern_aot.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(6u, function->nativeImportContractLength);
    TEST_ASSERT_EQUAL_HEX64(
            ZR_NATIVE_EXTERN_SCALAR_CONTRACT_HASH,
            function->nativeImportContracts[0].signature.signatureHash);
    for (TZrUInt32 index = 0u;
         index < function->nativeImportContractLength;
         index++) {
        TZrChar errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};

        TEST_ASSERT_TRUE_MESSAGE(
                ZrVmLibFfi_ValidateNativeImportContract(
                        &function->nativeImportContracts[index],
                        errorBuffer,
                        sizeof(errorBuffer)),
                errorBuffer);
    }
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, aotPath));
    generatedText = native_extern_read_text_file(aotPath);
    TEST_ASSERT_NOT_NULL(generatedText);
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            "static const SZrNativeImportContract zr_aot_native_import_contracts[]"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            "static const SZrAotNativeImportRange zr_aot_native_import_ranges[]"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            "{ .contractStart = 0u, .contractCount = 6u },"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            ".signatureHash = UINT64_C("
            ZR_NATIVE_EXTERN_SCALAR_CONTRACT_HASH_TEXT ")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText, ".targetTriple = \""));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            ".nativeImportContracts = zr_aot_native_import_contracts,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText, ".nativeImportContractCount = 6u,"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            ".nativeImportRanges = zr_aot_native_import_ranges,"));
    free(generatedText);
    generatedText = ZR_NULL;

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));
    generatedText = native_extern_read_text_file(llvmPath);
    TEST_ASSERT_NOT_NULL(generatedText);
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            "%ZrAotCompiledModule = type { i32, i32, ptr, i32, ptr, ptr, ptr, i64, ptr, i32, ptr, ptr, i32, ptr, i32, ptr, i32, ptr, i32, ptr, i32, ptr, i32, ptr, i32, ptr, i32, ptr, i32, ptr }"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            "@zr_aot_native_import_contracts = private constant [6 x %SZrNativeImportContract]"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            ZrCommon_FfiContract_GetHostTargetTriple()));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            "@zr_aot_native_import_ranges = private constant [1 x %SZrAotNativeImportRange] [%SZrAotNativeImportRange { i32 0, i32 6 }]"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            "  ptr @zr_aot_native_import_contracts,\n  i32 6,\n  ptr @zr_aot_native_import_ranges,\n  i32 1,\n  ptr @zr_aot_code_registration\n}"));

#if defined(ZR_PLATFORM_UNIX)
    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" -L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             aotPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedPath);
    TEST_ASSERT_EQUAL_INT(0, system(command));
    library = dlopen(sharedPath, RTLD_NOW | RTLD_LOCAL);
    TEST_ASSERT_NOT_NULL_MESSAGE(library, dlerror());
    symbol = dlsym(library, "ZrVm_GetAotCompiledModule");
    TEST_ASSERT_NOT_NULL(symbol);
    memcpy(&getModule, &symbol, sizeof(getModule));
    module = getModule();
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_NOT_NULL(module->codeRegistration);
    TEST_ASSERT_EQUAL_UINT32(
            6u, module->codeRegistration->nativeImportContractCount);
    TEST_ASSERT_NOT_NULL(module->codeRegistration->nativeImportContracts);
    TEST_ASSERT_NOT_NULL(module->nativeImportRanges);
    TEST_ASSERT_EQUAL_PTR(
            module->nativeImportRanges,
            module->codeRegistration->nativeImportRanges);
    TEST_ASSERT_EQUAL_UINT32(
            module->functionThunkCount,
            module->nativeImportRangeCount);
    TEST_ASSERT_EQUAL_UINT32(
            module->nativeImportRangeCount,
            module->codeRegistration->nativeImportRangeCount);
    TEST_ASSERT_NOT_NULL(module->codeRegistration->invokers);
    TEST_ASSERT_EQUAL_UINT32(1u, module->codeRegistration->invokerCount);
    TEST_ASSERT_EQUAL_UINT32(0u, module->nativeImportRanges[0].contractStart);
    TEST_ASSERT_EQUAL_UINT32(6u, module->nativeImportRanges[0].contractCount);
    TEST_ASSERT_EQUAL_PTR(
            &module->nativeImportContracts[5],
            ZrLibrary_AotRuntime_ResolveNativeImportContract(
                    module->codeRegistration, 0u, 5u));
    TEST_ASSERT_NULL(ZrLibrary_AotRuntime_ResolveNativeImportContract(
            module->codeRegistration, 0u, 6u));
    TEST_ASSERT_EQUAL_PTR(
            module->nativeImportContracts,
            module->codeRegistration->nativeImportContracts);
    for (TZrUInt32 index = 0u;
         index < function->nativeImportContractLength;
         index++) {
        TEST_ASSERT_EQUAL_HEX64(
                function->nativeImportContracts[index].signature.signatureHash,
                module->codeRegistration->nativeImportContracts[index]
                        .signature.signatureHash);
        TEST_ASSERT_EQUAL_UINT32(
                function->nativeImportContracts[index].availability,
                module->codeRegistration->nativeImportContracts[index]
                        .availability);
        TEST_ASSERT_EQUAL_UINT64(
                function->nativeImportContracts[index].requiredCapabilities,
                module->codeRegistration->nativeImportContracts[index]
                        .requiredCapabilities);
        TEST_ASSERT_EQUAL_STRING(
                function->nativeImportContracts[index].sourceMapping.document,
                module->codeRegistration->nativeImportContracts[index]
                        .sourceMapping.document);
        TEST_ASSERT_TRUE(ZrCommon_NativeImportContract_Validate(
                &module->codeRegistration->nativeImportContracts[index]));
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_STRUCT,
            module->nativeImportContracts[1].signature.parameters[0].type.typeKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_UNION,
            module->nativeImportContracts[2].signature.parameters[0].type.typeKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_DIRECTION_REF,
            module->nativeImportContracts[3].signature.parameters[0].direction);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_DIRECTION_OUT,
            module->nativeImportContracts[3].signature.parameters[1].direction);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_CALLBACK,
            module->nativeImportContracts[4].signature.parameters[1].type.typeKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_ERROR_ERRNO,
            module->nativeImportContracts[5].signature.errorPolicy);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_CLEANUP_CALLER,
            module->nativeImportContracts[5].signature.cleanupPolicy);
    TEST_ASSERT_EQUAL_INT(0, dlclose(library));
    remove(sharedPath);

    snprintf(command,
             sizeof(command),
             "clang -mllvm -opaque-pointers -fPIC -shared "
             "\"%s\" -L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
             "-o \"%s\"",
             llvmPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedPath);
    TEST_ASSERT_EQUAL_INT(0, system(command));
    library = dlopen(sharedPath, RTLD_NOW | RTLD_LOCAL);
    TEST_ASSERT_NOT_NULL_MESSAGE(library, dlerror());
    symbol = dlsym(library, "ZrVm_GetAotCompiledModule");
    TEST_ASSERT_NOT_NULL(symbol);
    memcpy(&getModule, &symbol, sizeof(getModule));
    module = getModule();
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_BACKEND_KIND_LLVM, module->backendKind);
    TEST_ASSERT_NOT_NULL(module->codeRegistration);
    TEST_ASSERT_EQUAL_PTR(
            module->functionThunks,
            module->codeRegistration->functionPointers);
    TEST_ASSERT_EQUAL_UINT32(
            module->functionThunkCount,
            module->codeRegistration->functionCount);
    TEST_ASSERT_EQUAL_UINT32(6u, module->nativeImportContractCount);
    TEST_ASSERT_NOT_NULL(module->nativeImportContracts);
    TEST_ASSERT_EQUAL_UINT32(1u, module->nativeImportRangeCount);
    TEST_ASSERT_NOT_NULL(module->nativeImportRanges);
    TEST_ASSERT_EQUAL_PTR(
            module->nativeImportContracts,
            module->codeRegistration->nativeImportContracts);
    TEST_ASSERT_EQUAL_UINT32(
            module->nativeImportContractCount,
            module->codeRegistration->nativeImportContractCount);
    TEST_ASSERT_EQUAL_PTR(
            module->nativeImportRanges,
            module->codeRegistration->nativeImportRanges);
    TEST_ASSERT_EQUAL_UINT32(
            module->nativeImportRangeCount,
            module->codeRegistration->nativeImportRangeCount);
    TEST_ASSERT_NOT_NULL(module->codeRegistration->invokers);
    TEST_ASSERT_EQUAL_UINT32(1u, module->codeRegistration->invokerCount);
    TEST_ASSERT_EQUAL_UINT32(0u, module->nativeImportRanges[0].contractStart);
    TEST_ASSERT_EQUAL_UINT32(6u, module->nativeImportRanges[0].contractCount);
    for (TZrUInt32 index = 0u;
         index < function->nativeImportContractLength;
         index++) {
        TEST_ASSERT_EQUAL_HEX64(
                function->nativeImportContracts[index].signature.signatureHash,
                module->nativeImportContracts[index].signature.signatureHash);
        TEST_ASSERT_EQUAL_STRING(
                function->nativeImportContracts[index].sourceMapping.document,
                module->nativeImportContracts[index].sourceMapping.document);
        TEST_ASSERT_TRUE(ZrCommon_NativeImportContract_Validate(
                &module->nativeImportContracts[index]));
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_STRUCT,
            module->nativeImportContracts[1].signature.parameters[0].type.typeKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_TYPE_CALLBACK,
            module->nativeImportContracts[4].signature.parameters[1].type.typeKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_ERROR_ERRNO,
            module->nativeImportContracts[5].signature.errorPolicy);
    TEST_ASSERT_EQUAL_INT(0, dlclose(library));
    remove(sharedPath);
#endif

    remove(aotPath);
    remove(llvmPath);
    free(generatedText);
    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

static void test_aot_project_release_clears_global_loader_userdata(void) {
    static const TZrChar *projectJson =
            "{"
            "\"name\":\"aot-loader-release\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = native_extern_create_state();
    SZrLibrary_Project *project;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(
            state,
            (TZrNativeString)projectJson,
            (TZrNativeString)"aot_loader_release_test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(
            state->global,
            ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
            ZR_TRUE));
    TEST_ASSERT_NOT_NULL(state->global->aotModuleLoader);
    TEST_ASSERT_NOT_NULL(state->global->aotModuleLoaderUserData);

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);

    TEST_ASSERT_NULL(state->global->aotModuleLoader);
    TEST_ASSERT_NULL(state->global->aotModuleLoaderUserData);
    native_extern_destroy_state(state);
}

static void test_native_extern_llvm_aot_runtime_accepts_code_registration(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("LLVM AOT runtime admission currently validates the Unix shared-library path");
#else
    static const TZrChar *sourceFormat =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")#\n"
            "  pub fn Add(lhs: i32, rhs: i32): i32;\n"
            "}\n"
            "return Add(20, 22);\n";
    static const TZrChar *projectJson =
            "{"
            "\"name\":\"native-extern-llvm-runtime\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrByte *embeddedBlob;
    TZrSize embeddedBlobLength;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar llvmPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    TZrChar escapedFixturePath[4096];
    TZrChar source[8192];
    TZrChar command[4096];

    TEST_ASSERT_NOT_NULL(state);
    escape_zr_string(
            escapedFixturePath,
            sizeof(escapedFixturePath),
            ZR_VM_FFI_FIXTURE_PATH);
    TEST_ASSERT_GREATER_THAN_INT(
            0,
            snprintf(
                    source,
                    sizeof(source),
                    sourceFormat,
                    escapedFixturePath));
    script = parse_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, function->nativeImportContractLength);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "native_extern_contract", "llvm_runtime", "project", ".zrp",
            projectPath, sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "native_extern_contract", "llvm_runtime/src", "main", ".zr",
            sourcePath, sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "native_extern_contract", "llvm_runtime/bin", "main", ".zro",
            zroPath, sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "native_extern_contract", "llvm_runtime/bin/aot_llvm/src", "main", ".ll",
            llvmPath, sizeof(llvmPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "native_extern_contract", "llvm_runtime/bin/aot_llvm/lib", "zrvm_aot_main", ".so",
            sharedLibraryPath, sizeof(sharedLibraryPath)));

    native_extern_write_text_file(projectPath, projectJson);
    native_extern_write_text_file(sourcePath, source);
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(zroPath));
    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(
            state, function, zroPath, &binaryOptions));
    native_extern_hash_file(zroPath, zroHash, sizeof(zroHash));
    embeddedBlob = native_extern_read_file(zroPath, &embeddedBlobLength);
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(llvmPath));
    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(
            state, function, llvmPath, &aotOptions));

    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(sharedLibraryPath));
    TEST_ASSERT_GREATER_THAN_INT(
            0,
            snprintf(
                    command,
                    sizeof(command),
                    "clang -mllvm -opaque-pointers -fPIC -shared \"%s\" "
                    "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
                    ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
                    "-o \"%s\"",
                    llvmPath,
                    ZR_VM_TESTS_BUILD_LIB_DIR,
                    ZR_VM_TESTS_BUILD_LIB_DIR,
                    sharedLibraryPath));
    TEST_ASSERT_EQUAL_INT(0, system(command));

    project = ZrLibrary_Project_New(
            state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(
            state->global, ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM, ZR_TRUE));
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_AotRuntime_ExecuteEntry(
                    state, ZR_AOT_BACKEND_KIND_LLVM, &result),
            ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(
            ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(
            42,
            ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)
                    ? (TZrInt64)result.value.nativeObject.nativeUInt64
                    : result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(
            ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM,
            ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
#endif
}

static void test_native_extern_contract_roundtrips_through_zro(void) {
    static const TZrChar *source =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_add_i32\")#\n"
            "  #zr.ffi.errorPolicy(\"returnCode\")#\n"
            "  #zr.ffi.cleanup(\"caller\")#\n"
            "  pub fn Add(lhs: i32, rhs: i32): i32;\n"
            "}\n"
            "return 0;\n";
    static const TZrChar *binaryPath = "native_extern_contract_roundtrip.zro";
    SZrState *state = native_extern_create_state();
    SZrAstNode *script;
    SZrFunction *function;
    SZrFunction *loadedFunction;
    TZrByte *bytes;
    TZrSize byteCount;
    SZrNativeExternBinaryReader reader;
    SZrIo io;
    SZrIoSource *ioSource;

    TEST_ASSERT_NOT_NULL(state);
    script = parse_source(state, source, "native_extern_roundtrip.zr");
    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    bytes = native_extern_read_file(binaryPath, &byteCount);
    TEST_ASSERT_NOT_NULL(bytes);
    memset(&reader, 0, sizeof(reader));
    reader.bytes = bytes;
    reader.length = byteCount;
    ZrCore_Io_Init(
            state,
            &io,
            native_extern_binary_read,
            native_extern_binary_close,
            &reader);
    ioSource = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(ioSource);
    loadedFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
    TEST_ASSERT_NOT_NULL(loadedFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, loadedFunction->nativeImportContractLength);
    TEST_ASSERT_NOT_NULL(loadedFunction->nativeImportContracts);
    TEST_ASSERT_EQUAL_STRING(
            "fixture", loadedFunction->nativeImportContracts[0].libraryLocator);
    TEST_ASSERT_EQUAL_STRING(
            "zr_ffi_add_i32", loadedFunction->nativeImportContracts[0].entryPoint);
    TEST_ASSERT_EQUAL_UINT64(
            function->nativeImportContracts[0].signature.signatureHash,
            loadedFunction->nativeImportContracts[0].signature.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(
            function->nativeImportContracts[0].callable.contractHash,
            loadedFunction->nativeImportContracts[0].callable.contractHash);
    TEST_ASSERT_EQUAL_UINT32(
            function->nativeImportContracts[0].callable.parameterCount,
            loadedFunction->nativeImportContracts[0].callable.parameterCount);
    TEST_ASSERT_EQUAL_MEMORY(
            function->nativeImportContracts[0].callable.parameters,
            loadedFunction->nativeImportContracts[0].callable.parameters,
            sizeof(SZrFfiCallableParameterContract) *
                    function->nativeImportContracts[0].callable.parameterCount);
    TEST_ASSERT_EQUAL_UINT64(
            function->nativeImportContracts[0].callable.returnTypeHash,
            loadedFunction->nativeImportContracts[0].callable.returnTypeHash);
    TEST_ASSERT_EQUAL_STRING(
            function->nativeImportContracts[0].signature.targetTriple,
            loadedFunction->nativeImportContracts[0].signature.targetTriple);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_ERROR_RETURN_CODE,
            loadedFunction->nativeImportContracts[0].signature.errorPolicy);
    TEST_ASSERT_EQUAL_INT(
            ZR_FFI_CONTRACT_CLEANUP_CALLER,
            loadedFunction->nativeImportContracts[0].signature.cleanupPolicy);
    TEST_ASSERT_EQUAL_UINT32(
            function->nativeImportContracts[0].availability,
            loadedFunction->nativeImportContracts[0].availability);
    TEST_ASSERT_EQUAL_UINT64(
            function->nativeImportContracts[0].requiredCapabilities,
            loadedFunction->nativeImportContracts[0].requiredCapabilities);
    TEST_ASSERT_EQUAL_UINT64(
            function->nativeImportContracts[0].declaringModuleId,
            loadedFunction->nativeImportContracts[0].declaringModuleId);
    TEST_ASSERT_EQUAL_STRING(
            "native_extern_roundtrip.zr",
            loadedFunction->nativeImportContracts[0].sourceMapping.document);
    ioSource->modules[0].entryFunction->nativeImportContractLength =
            (TZrSize)ZR_FFI_CONTRACT_MAX_IMPORTS_PER_FUNCTION + 1u;
    TEST_ASSERT_NULL(ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource));
    ioSource->modules[0].entryFunction->nativeImportContractLength = 1u;
    ioSource->modules[0].entryFunction->nativeImportContracts[0]
            .signature.signatureHash ^= UINT64_C(1);
    TEST_ASSERT_NULL(ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource));
    memset(&reader, 0, sizeof(reader));
    reader.bytes = bytes;
    reader.length = byteCount - 1u;
    ZrCore_Io_Init(
            state,
            &io,
            native_extern_binary_read,
            native_extern_binary_close,
            &reader);
    TEST_ASSERT_NULL(ZrCore_Io_ReadSourceNew(&io));

    remove(binaryPath);
    free(bytes);
    ZrCore_Function_Free(state, function);
    ZrCore_Function_Free(state, loadedFunction);
    ZrParser_Ast_Free(state, script);
    native_extern_destroy_state(state);
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_native_extern_builds_persistent_scalar_contract);
    RUN_TEST(test_native_extern_preserves_ref_and_out_directions);
    RUN_TEST(test_native_extern_rejects_corrupt_persistent_contract);
    RUN_TEST(test_native_extern_persists_availability_and_capabilities);
    RUN_TEST(test_native_extern_classifies_aggregate_union_and_target_contract);
    RUN_TEST(test_native_extern_requires_explicit_callback_policy);
    RUN_TEST(test_native_extern_policy_contract_admission);
    RUN_TEST(test_native_extern_rejects_managed_types_and_invalid_abi);
    RUN_TEST(test_native_extern_accepts_cdecl_callconv_alias);
    RUN_TEST(test_native_extern_accepts_blittable_local_type_regardless_of_name);
    RUN_TEST(test_native_extern_callable_hash_preserves_passing_semantics);
    RUN_TEST(test_native_extern_current_syntax_executes_static_symbol);
    RUN_TEST(test_native_extern_current_syntax_executes_callback_contract);
    RUN_TEST(test_native_extern_callback_exception_returns_default);
    RUN_TEST(test_native_extern_rejects_mismatched_callback_signature);
    RUN_TEST(test_native_extern_call_lifetime_rejects_late_callback);
    RUN_TEST(test_native_extern_call_lifetime_callback_supports_reentrancy);
    RUN_TEST(test_native_extern_mixed_union_uses_integer_abi_class);
    RUN_TEST(test_native_extern_heterogeneous_float_union_uses_integer_abi_class);
    RUN_TEST(test_native_extern_union_first_member_is_not_overwritten);
    RUN_TEST(test_native_extern_union_rejects_ambiguous_active_member);
    RUN_TEST(test_native_extern_rejects_union_return_and_writeback);
    RUN_TEST(test_native_extern_errno_policy_raises_runtime_error);
    RUN_TEST(test_native_extern_return_code_and_callee_cleanup_execute);
    RUN_TEST(test_native_extern_current_syntax_executes_ref_contract);
    RUN_TEST(test_native_extern_current_syntax_executes_out_contract);
    RUN_TEST(test_native_extern_aot_uses_canonical_signature_vector);
    RUN_TEST(test_native_extern_llvm_aot_runtime_accepts_code_registration);
    RUN_TEST(test_aot_project_release_clears_global_loader_userdata);
    RUN_TEST(test_native_extern_contract_roundtrips_through_zro);
    return UNITY_END();
}
