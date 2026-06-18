#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"

#include <string.h>

#define TEST_STRING_INDEX 0x1234u
#define TEST_CALLER_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u)
#define TEST_CALLER_TYPE_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_PROVIDER_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define TEST_PROVIDER_TYPE_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u)
#define TEST_PROVIDER_MEMBER_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define TEST_PROVIDER_MEMBER_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 3u)
#define TEST_CALLER_TYPE_REF_HASH ((TZrUInt64)0x1020304050607080ULL)
#define TEST_PROVIDER_TYPE_DEF_HASH ((TZrUInt64)0x8877665544332211ULL)
#define TEST_PROVIDER_MEMBER_DEF_HASH ((TZrUInt64)0x0102030405060708ULL)
#define TEST_PROVIDER_MODULE_HASH ((TZrUInt64)0x1122334455667788ULL)
#define TEST_TYPE_LAYOUT_VERSION 7u
#define TEST_TYPE_LAYOUT_HASH ((TZrUInt64)0x5566778899AABBCCULL)

TZrBool compiler_build_function_metadata_tokens(SZrCompilerState *cs, SZrFunction *function);
TZrBool ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(SZrState *state,
                                                               SZrString *moduleName,
                                                               SZrAstNode *ast);
TZrBool ZrParser_ModuleInitAnalysis_FinalizeCurrentSourceModule(SZrCompilerState *cs,
                                                                SZrString *moduleName,
                                                                SZrFunction *function);
void ZrParser_ModuleInitAnalysis_ClearAstIdentity(SZrGlobalState *global, const SZrAstNode *ast);
typedef struct SZrModuleImportSignatureMismatch SZrModuleImportSignatureMismatch;
TZrBool zr_module_import_signature_verify(SZrState *state,
                                           SZrFunction *callerFunction,
                                           SZrString *path,
                                           SZrObjectModule *module,
                                           SZrModuleImportSignatureMismatch *outMismatch);

void setUp(void) {}

void tearDown(void) {}

static void write_u32_le(TZrByte *buffer, TZrSize offset, TZrUInt32 value) {
    buffer[offset] = (TZrByte)(value & 0xFFu);
    buffer[offset + 1u] = (TZrByte)((value >> 8) & 0xFFu);
    buffer[offset + 2u] = (TZrByte)((value >> 16) & 0xFFu);
    buffer[offset + 3u] = (TZrByte)((value >> 24) & 0xFFu);
}

static TZrByte *allocate_signature_blob(SZrState *state, TZrSize length) {
    TZrByte *blob;

    blob = (TZrByte *)ZrCore_Memory_RawMallocWithType(state->global,
                                                     length,
                                                     ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(blob);
    ZrCore_Memory_RawSet(blob, 0, length);
    return blob;
}

static SZrMetadataTokenRecord *allocate_records(SZrState *state, TZrUInt32 count) {
    SZrMetadataTokenRecord *records;

    records = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataTokenRecord) * count,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(records);
    ZrCore_Memory_RawSet(records, 0, sizeof(SZrMetadataTokenRecord) * count);
    return records;
}

static void attach_string_heap(SZrState *state, SZrFunction *function) {
    function->metadataStringHeap = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrMetadataStringHeapEntry),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap);
    function->metadataStringHeapLength = 1u;
    function->metadataStringHeap[0].stringIndex = TEST_STRING_INDEX;
    function->metadataStringHeap[0].value = ZrCore_String_CreateFromNative(state, "Option");
    TEST_ASSERT_NOT_NULL(function->metadataStringHeap[0].value);
}

static SZrFunction *create_caller_type_ref_fixture(SZrState *state) {
    SZrFunction *function;
    TZrSize typeRefLength = 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    SZrMetadataTokenRecord *records;

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->signatureBlobHeap = allocate_signature_blob(state, typeRefLength);
    function->signatureBlobHeapLength = (TZrUInt32)typeRefLength;
    function->signatureBlobHeap[0] = ZR_METADATA_SIGNATURE_NODE_TYPE_REF;
    write_u32_le(function->signatureBlobHeap, 1u, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    write_u32_le(function->signatureBlobHeap, 1u + sizeof(TZrUInt32), TEST_STRING_INDEX);
    attach_string_heap(state, function);

    records = allocate_records(state, 2u);
    records[0].token = TEST_CALLER_TYPE_REF_TOKEN;
    records[0].relatedToken = TEST_CALLER_TYPE_REF_SIGNATURE_TOKEN;
    records[0].signatureBlobOffset = 0u;
    records[0].signatureBlobLength = (TZrUInt32)typeRefLength;
    records[0].signatureHash = TEST_CALLER_TYPE_REF_HASH;
    records[0].layoutVersion = TEST_TYPE_LAYOUT_VERSION;
    records[0].layoutHash = TEST_TYPE_LAYOUT_HASH;
    records[0].targetMetadataToken = TEST_PROVIDER_TYPE_DEF_TOKEN;
    records[0].targetSignatureToken = TEST_PROVIDER_TYPE_DEF_SIGNATURE_TOKEN;
    records[0].targetSignatureHash = TEST_PROVIDER_TYPE_DEF_HASH;
    records[0].targetModuleSignatureHash = TEST_PROVIDER_MODULE_HASH;
    records[1] = records[0];
    records[1].token = TEST_CALLER_TYPE_REF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_CALLER_TYPE_REF_TOKEN;
    records[1].ownerToken = TEST_CALLER_TYPE_REF_TOKEN;
    function->metadataTokenRecords = records;
    function->metadataTokenRecordLength = 2u;
    return function;
}

static SZrFunction *create_provider_type_def_fixture(SZrState *state) {
    SZrFunction *function;
    TZrSize typeDefLength = 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    SZrMetadataTokenRecord *records;

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->moduleSignatureHash = TEST_PROVIDER_MODULE_HASH;
    function->signatureBlobHeap = allocate_signature_blob(state, typeDefLength);
    function->signatureBlobHeapLength = (TZrUInt32)typeDefLength;
    function->signatureBlobHeap[0] = ZR_METADATA_SIGNATURE_NODE_TYPE_DEF;
    write_u32_le(function->signatureBlobHeap, 1u, TEST_STRING_INDEX);
    write_u32_le(function->signatureBlobHeap, 1u + sizeof(TZrUInt32), 0u);
    write_u32_le(function->signatureBlobHeap, 1u + sizeof(TZrUInt32) * 2u, 0u);
    attach_string_heap(state, function);

    records = allocate_records(state, 2u);
    records[0].token = TEST_PROVIDER_TYPE_DEF_TOKEN;
    records[0].relatedToken = TEST_PROVIDER_TYPE_DEF_SIGNATURE_TOKEN;
    records[0].signatureBlobOffset = 0u;
    records[0].signatureBlobLength = (TZrUInt32)typeDefLength;
    records[0].signatureHash = TEST_PROVIDER_TYPE_DEF_HASH;
    records[0].layoutVersion = TEST_TYPE_LAYOUT_VERSION;
    records[0].layoutHash = TEST_TYPE_LAYOUT_HASH;
    records[1] = records[0];
    records[1].token = TEST_PROVIDER_TYPE_DEF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_PROVIDER_TYPE_DEF_TOKEN;
    records[1].ownerToken = TEST_PROVIDER_TYPE_DEF_TOKEN;
    function->metadataTokenRecords = records;
    function->metadataTokenRecordLength = 2u;
    return function;
}

static void attach_provider_export_symbol(SZrState *state, SZrFunction *function, SZrString *symbolName) {
    SZrFunctionTypedExportSymbol *symbol;

    function->typedExportedSymbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->typedExportedSymbols);
    ZrCore_Memory_RawSet(function->typedExportedSymbols, 0, sizeof(SZrFunctionTypedExportSymbol));
    function->typedExportedSymbolLength = 1u;

    symbol = &function->typedExportedSymbols[0];
    symbol->name = symbolName;
    symbol->accessModifier = ZR_ACCESS_PUBLIC;
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    symbol->metadataToken = TEST_PROVIDER_MEMBER_DEF_TOKEN;
    symbol->signatureToken = TEST_PROVIDER_MEMBER_DEF_SIGNATURE_TOKEN;
    symbol->signatureHash = TEST_PROVIDER_MEMBER_DEF_HASH;
}

static void attach_caller_import_effect(SZrState *state,
                                        SZrFunction *function,
                                        SZrString *moduleName,
                                        SZrString *symbolName) {
    SZrFunctionModuleEffect *effect;

    effect = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                       sizeof(SZrFunctionModuleEffect),
                                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(effect);
    ZrCore_Memory_RawSet(effect, 0, sizeof(*effect));
    effect->kind = ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
    effect->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    effect->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    effect->moduleName = moduleName;
    effect->symbolName = symbolName;
    effect->targetMetadataToken = TEST_PROVIDER_MEMBER_DEF_TOKEN;
    effect->targetSignatureToken = TEST_PROVIDER_MEMBER_DEF_SIGNATURE_TOKEN;
    effect->targetSignatureHash = TEST_PROVIDER_MEMBER_DEF_HASH;
    effect->targetModuleSignatureHash = TEST_PROVIDER_MODULE_HASH;
    function->moduleEntryEffects = effect;
    function->moduleEntryEffectLength = 1u;
}

static void test_targeted_type_ref_binds_to_provider_type_def_identity(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    const SZrMetadataTokenBinding *binding;

    TEST_ASSERT_NOT_NULL(state);
    callerFunction = create_caller_type_ref_fixture(state);
    providerFunction = create_provider_type_def_fixture(state);

    TEST_ASSERT_TRUE(ZrCore_Function_BindMatchingTypeRefMetadata(state, callerFunction, providerFunction));
    binding = ZrCore_Function_FindModuleMetadataBinding(callerFunction, TEST_CALLER_TYPE_REF_TOKEN);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(TEST_CALLER_TYPE_REF_TOKEN, binding->refToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_CALLER_TYPE_REF_SIGNATURE_TOKEN, binding->refSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_CALLER_TYPE_REF_HASH, binding->refSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_PROVIDER_TYPE_DEF_TOKEN, binding->expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_PROVIDER_TYPE_DEF_SIGNATURE_TOKEN, binding->expectedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_PROVIDER_TYPE_DEF_HASH, binding->expectedSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_PROVIDER_MODULE_HASH, binding->expectedModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_LAYOUT_VERSION, binding->expectedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(TEST_TYPE_LAYOUT_HASH, binding->expectedLayoutHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_PROVIDER_TYPE_DEF_TOKEN, binding->resolvedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_PROVIDER_TYPE_DEF_SIGNATURE_TOKEN, binding->resolvedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_PROVIDER_TYPE_DEF_HASH, binding->resolvedSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(TEST_PROVIDER_MODULE_HASH, binding->resolvedModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_LAYOUT_VERSION, binding->resolvedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(TEST_TYPE_LAYOUT_HASH, binding->resolvedLayoutHash);

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_type_ref_binding_reports_layout_mismatch_without_partial_binding(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    SZrMetadataTypeRefBindStatus status;
    SZrMetadataTokenRecord *providerTypeDef;

    TEST_ASSERT_NOT_NULL(state);
    callerFunction = create_caller_type_ref_fixture(state);
    providerFunction = create_provider_type_def_fixture(state);
    providerTypeDef = &providerFunction->metadataTokenRecords[0];
    providerTypeDef->layoutHash ^= (TZrUInt64)0x1u;

    ZrCore_Memory_RawSet(&status, 0, sizeof(status));
    TEST_ASSERT_FALSE(ZrCore_Function_BindMatchingTypeRefMetadataWithStatus(state,
                                                                            callerFunction,
                                                                            providerFunction,
                                                                            &status));
    TEST_ASSERT_EQUAL_UINT32(1u, status.callerTypeRefCount);
    TEST_ASSERT_EQUAL_UINT32(0u, status.matchedTypeRefCount);
    TEST_ASSERT_EQUAL_UINT32(0u, status.unmatchedTypeRefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, status.layoutMismatchCount);
    TEST_ASSERT_EQUAL_UINT32(TEST_CALLER_TYPE_REF_TOKEN, status.firstLayoutMismatchTypeRefToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_LAYOUT_VERSION, status.firstExpectedLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(TEST_TYPE_LAYOUT_HASH, status.firstExpectedLayoutHash);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->layoutVersion, status.firstActualLayoutVersion);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->layoutHash, status.firstActualLayoutHash);
    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataBinding(callerFunction, TEST_CALLER_TYPE_REF_TOKEN));

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_type_ref_binding_mismatch_records_loader_diagnostic(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    SZrObjectModule *providerModule;
    SZrString *moduleName;
    SZrString *symbolName;
    SZrMetadataTokenRecord *providerTypeDef;
    const TZrChar *diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    moduleName = ZrCore_String_CreateFromNative(state, "typeref.provider");
    symbolName = ZrCore_String_CreateFromNative(state, "make");
    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_NOT_NULL(symbolName);

    callerFunction = create_caller_type_ref_fixture(state);
    providerFunction = create_provider_type_def_fixture(state);
    providerTypeDef = &providerFunction->metadataTokenRecords[0];
    providerTypeDef->layoutHash ^= (TZrUInt64)0x1u;
    attach_caller_import_effect(state, callerFunction, moduleName, symbolName);
    attach_provider_export_symbol(state, providerFunction, symbolName);

    providerModule = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(providerModule);
    ZrCore_Module_SetInfo(state,
                          providerModule,
                          moduleName,
                          ZrCore_Module_CalculatePathHash(state, moduleName),
                          moduleName);
    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, providerModule, providerFunction);

    ZrCore_GlobalState_ClearModuleLoadDiagnostic(state->global);
    TEST_ASSERT_TRUE(zr_module_import_signature_verify(state,
                                                       callerFunction,
                                                       moduleName,
                                                       providerModule,
                                                       ZR_NULL));
    diagnostic = ZrCore_GlobalState_GetModuleLoadDiagnostic(state->global);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "type_ref_mismatch"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "typeref.provider"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "make"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "layoutMismatches=1"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "firstLayoutMismatchTypeRefToken="));
    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataBinding(callerFunction, TEST_CALLER_TYPE_REF_TOKEN));

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static const SZrMetadataTokenRecord *find_first_table_record(const SZrFunction *function, TZrUInt32 tableTag) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == tableTag) {
            return record;
        }
    }

    return ZR_NULL;
}

static const SZrMetadataTokenRecord *find_type_ref_targeting_type_def(const SZrFunction *function,
                                                                      TZrMetadataToken targetTypeDefToken) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_TYPE_REF &&
            record->targetMetadataToken == targetTypeDefToken) {
            return record;
        }
    }

    return ZR_NULL;
}

static void init_named_type_ref(SZrState *state, SZrFunctionTypedTypeRef *typeRef, const char *typeName) {
    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->typeName = ZrCore_String_CreateFromNative(state, (TZrNativeString)typeName);
    TEST_ASSERT_NOT_NULL(typeRef->typeName);
}

static SZrFunction *create_provider_union_function_fixture(SZrState *state,
                                                           SZrAstNode *providerAst,
                                                           SZrString *providerModuleName,
                                                           const char *symbolName,
                                                           const char *returnTypeName) {
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *symbol;
    SZrCompilerState compilerState;

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->functionName = ZrCore_String_CreateFromNative(state, "__entry");
    TEST_ASSERT_NOT_NULL(function->functionName);
    function->typedExportedSymbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->typedExportedSymbols);
    ZrCore_Memory_RawSet(function->typedExportedSymbols, 0, sizeof(SZrFunctionTypedExportSymbol));
    function->typedExportedSymbolLength = 1u;

    symbol = &function->typedExportedSymbols[0];
    symbol->name = ZrCore_String_CreateFromNative(state, (TZrNativeString)symbolName);
    TEST_ASSERT_NOT_NULL(symbol->name);
    symbol->accessModifier = ZR_ACCESS_PUBLIC;
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    init_named_type_ref(state, &symbol->valueType, returnTypeName);

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    compilerState.currentAst = providerAst;
    compilerState.scriptAst = providerAst;
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_FinalizeCurrentSourceModule(&compilerState,
                                                                             providerModuleName,
                                                                             function));
    return function;
}

static void test_import_target_signature_emits_stable_provider_type_ref(void) {
    const char *providerSource =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n"
            "pub func choose(): Option<int> {\n"
            "    return Option<int>.None;\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *providerModuleName;
    SZrString *providerSourceName;
    SZrAstNode *providerAst;
    SZrFunction *providerFunction;
    SZrFunction *callerFunction;
    SZrFunctionModuleEffect *effect;
    SZrCompilerState compilerState;
    const SZrMetadataTokenRecord *providerTypeDef;
    const SZrMetadataTokenRecord *callerTypeRef;
    const SZrMetadataTokenBinding *binding;

    TEST_ASSERT_NOT_NULL(state);
    providerModuleName = ZrCore_String_CreateFromNative(state, "provider");
    providerSourceName = ZrCore_String_CreateFromNative(state, "provider.zr");
    TEST_ASSERT_NOT_NULL(providerModuleName);
    TEST_ASSERT_NOT_NULL(providerSourceName);
    providerAst = ZrParser_Parse(state, providerSource, strlen(providerSource), providerSourceName);
    TEST_ASSERT_NOT_NULL(providerAst);
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state,
                                                                            providerModuleName,
                                                                            providerAst));
    providerFunction = create_provider_union_function_fixture(state, providerAst, providerModuleName, "choose", "Option<int>");
    TEST_ASSERT_NOT_NULL(providerFunction);
    providerTypeDef = find_first_table_record(providerFunction, ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(providerTypeDef);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, providerFunction->moduleSignatureHash);

    callerFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    effect = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                       sizeof(SZrFunctionModuleEffect),
                                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(effect);
    ZrCore_Memory_RawSet(effect, 0, sizeof(*effect));
    effect->kind = ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
    effect->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    effect->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    effect->moduleName = providerModuleName;
    effect->symbolName = providerFunction->typedExportedSymbols[0].name;
    effect->targetMetadataToken = providerFunction->typedExportedSymbols[0].metadataToken;
    effect->targetSignatureToken = providerFunction->typedExportedSymbols[0].signatureToken;
    effect->targetSignatureHash = providerFunction->typedExportedSymbols[0].signatureHash;
    effect->targetModuleSignatureHash = providerFunction->moduleSignatureHash;
    callerFunction->moduleEntryEffects = effect;
    callerFunction->moduleEntryEffectLength = 1u;

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = callerFunction;
    TEST_ASSERT_TRUE(compiler_build_function_metadata_tokens(&compilerState, callerFunction));
    callerTypeRef = find_type_ref_targeting_type_def(callerFunction, providerTypeDef->token);
    TEST_ASSERT_NOT_NULL(callerTypeRef);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, callerTypeRef->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, callerTypeRef->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(providerFunction->moduleSignatureHash, callerTypeRef->targetModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->layoutVersion, callerTypeRef->layoutVersion);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->layoutHash, callerTypeRef->layoutHash);

    TEST_ASSERT_TRUE(ZrCore_Function_BindMatchingTypeRefMetadata(state, callerFunction, providerFunction));
    binding = ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeRef->token);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->token, binding->resolvedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, binding->resolvedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, binding->resolvedSignatureHash);

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, providerAst);
    ZrParser_Ast_Free(state, providerAst);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_nested_generic_import_target_signature_emits_provider_type_ref(void) {
    const char *providerSource =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n"
            "pub func choose(): Option<int> {\n"
            "    return Option<int>.None;\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *providerModuleName;
    SZrString *providerSourceName;
    SZrAstNode *providerAst;
    SZrFunction *providerFunction;
    SZrFunction *callerFunction;
    SZrFunctionModuleEffect *effect;
    SZrCompilerState compilerState;
    const SZrMetadataTokenRecord *providerTypeDef;
    const SZrMetadataTokenRecord *callerTypeRef;

    TEST_ASSERT_NOT_NULL(state);
    providerModuleName = ZrCore_String_CreateFromNative(state, "provider");
    providerSourceName = ZrCore_String_CreateFromNative(state, "provider.zr");
    TEST_ASSERT_NOT_NULL(providerModuleName);
    TEST_ASSERT_NOT_NULL(providerSourceName);
    providerAst = ZrParser_Parse(state, providerSource, strlen(providerSource), providerSourceName);
    TEST_ASSERT_NOT_NULL(providerAst);
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state,
                                                                            providerModuleName,
                                                                            providerAst));
    providerFunction = create_provider_union_function_fixture(state,
                                                             providerAst,
                                                             providerModuleName,
                                                             "choose",
                                                             "Box<Option<int>>");
    TEST_ASSERT_NOT_NULL(providerFunction);
    providerTypeDef = find_first_table_record(providerFunction, ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(providerTypeDef);

    callerFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    effect = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                       sizeof(SZrFunctionModuleEffect),
                                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(effect);
    ZrCore_Memory_RawSet(effect, 0, sizeof(*effect));
    effect->kind = ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
    effect->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    effect->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    effect->moduleName = providerModuleName;
    effect->symbolName = providerFunction->typedExportedSymbols[0].name;
    effect->targetMetadataToken = providerFunction->typedExportedSymbols[0].metadataToken;
    effect->targetSignatureToken = providerFunction->typedExportedSymbols[0].signatureToken;
    effect->targetSignatureHash = providerFunction->typedExportedSymbols[0].signatureHash;
    effect->targetModuleSignatureHash = providerFunction->moduleSignatureHash;
    callerFunction->moduleEntryEffects = effect;
    callerFunction->moduleEntryEffectLength = 1u;

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = callerFunction;
    TEST_ASSERT_TRUE(compiler_build_function_metadata_tokens(&compilerState, callerFunction));
    callerTypeRef = find_type_ref_targeting_type_def(callerFunction, providerTypeDef->token);
    TEST_ASSERT_NOT_NULL(callerTypeRef);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, callerTypeRef->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, callerTypeRef->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(providerFunction->moduleSignatureHash, callerTypeRef->targetModuleSignatureHash);

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, providerAst);
    ZrParser_Ast_Free(state, providerAst);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_explicit_import_type_annotation_emits_provider_type_ref(void) {
    const char *providerSource =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *providerModuleName;
    SZrString *providerSourceName;
    SZrAstNode *providerAst;
    SZrFunction *providerFunction;
    SZrFunction *callerFunction;
    SZrFunctionTypedLocalBinding *bindingInfo;
    SZrCompilerState compilerState;
    const SZrMetadataTokenRecord *providerTypeDef;
    const SZrMetadataTokenRecord *callerTypeRef;
    const SZrMetadataTokenBinding *binding;

    TEST_ASSERT_NOT_NULL(state);
    providerModuleName = ZrCore_String_CreateFromNative(state, "provider");
    providerSourceName = ZrCore_String_CreateFromNative(state, "provider.zr");
    TEST_ASSERT_NOT_NULL(providerModuleName);
    TEST_ASSERT_NOT_NULL(providerSourceName);
    providerAst = ZrParser_Parse(state, providerSource, strlen(providerSource), providerSourceName);
    TEST_ASSERT_NOT_NULL(providerAst);
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state,
                                                                            providerModuleName,
                                                                            providerAst));
    providerFunction = create_provider_union_function_fixture(state,
                                                             providerAst,
                                                             providerModuleName,
                                                             "choose",
                                                             "Option<int>");
    TEST_ASSERT_NOT_NULL(providerFunction);
    providerTypeDef = find_first_table_record(providerFunction, ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(providerTypeDef);

    callerFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    bindingInfo = (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedLocalBinding),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(bindingInfo);
    ZrCore_Memory_RawSet(bindingInfo, 0, sizeof(*bindingInfo));
    bindingInfo->name = ZrCore_String_CreateFromNative(state, "maybe");
    TEST_ASSERT_NOT_NULL(bindingInfo->name);
    init_named_type_ref(state, &bindingInfo->type, "provider.Option<int>");
    callerFunction->typedLocalBindings = bindingInfo;
    callerFunction->typedLocalBindingLength = 1u;

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = callerFunction;
    TEST_ASSERT_EQUAL_UINT32(0u, callerFunction->moduleEntryEffectLength);
    TEST_ASSERT_TRUE(compiler_build_function_metadata_tokens(&compilerState, callerFunction));
    callerTypeRef = find_type_ref_targeting_type_def(callerFunction, providerTypeDef->token);
    TEST_ASSERT_NOT_NULL(callerTypeRef);
    TEST_ASSERT_NOT_NULL(find_first_table_record(callerFunction, ZR_METADATA_TABLE_ASSEMBLY_REF));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_ASSEMBLY_REF, ZR_METADATA_TOKEN_TABLE(callerTypeRef->ownerToken));
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, callerTypeRef->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, callerTypeRef->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(providerFunction->moduleSignatureHash, callerTypeRef->targetModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->layoutVersion, callerTypeRef->layoutVersion);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->layoutHash, callerTypeRef->layoutHash);

    TEST_ASSERT_TRUE(ZrCore_Function_BindMatchingTypeRefMetadata(state, callerFunction, providerFunction));
    binding = ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeRef->token);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->token, binding->resolvedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, binding->resolvedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, binding->resolvedSignatureHash);

    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, providerAst);
    ZrParser_Ast_Free(state, providerAst);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_unqualified_import_type_alias_annotation_emits_provider_type_ref(void) {
    const char *providerSource =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *providerModuleName;
    SZrString *providerSourceName;
    SZrAstNode *providerAst;
    SZrFunction *providerFunction;
    SZrFunction *callerFunction;
    SZrFunctionTypedLocalBinding *bindingInfo;
    SZrTypeBinding aliasBinding;
    SZrCompilerState compilerState;
    const SZrMetadataTokenRecord *providerTypeDef;
    const SZrMetadataTokenRecord *callerTypeRef;
    const SZrMetadataTokenBinding *binding;

    TEST_ASSERT_NOT_NULL(state);
    providerModuleName = ZrCore_String_CreateFromNative(state, "provider");
    providerSourceName = ZrCore_String_CreateFromNative(state, "provider.zr");
    TEST_ASSERT_NOT_NULL(providerModuleName);
    TEST_ASSERT_NOT_NULL(providerSourceName);
    providerAst = ZrParser_Parse(state, providerSource, strlen(providerSource), providerSourceName);
    TEST_ASSERT_NOT_NULL(providerAst);
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state,
                                                                            providerModuleName,
                                                                            providerAst));
    providerFunction = create_provider_union_function_fixture(state,
                                                             providerAst,
                                                             providerModuleName,
                                                             "choose",
                                                             "Option<int>");
    TEST_ASSERT_NOT_NULL(providerFunction);
    providerTypeDef = find_first_table_record(providerFunction, ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(providerTypeDef);

    callerFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    bindingInfo = (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedLocalBinding),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(bindingInfo);
    ZrCore_Memory_RawSet(bindingInfo, 0, sizeof(*bindingInfo));
    bindingInfo->name = ZrCore_String_CreateFromNative(state, "maybe");
    TEST_ASSERT_NOT_NULL(bindingInfo->name);
    init_named_type_ref(state, &bindingInfo->type, "Option<int>");
    callerFunction->typedLocalBindings = bindingInfo;
    callerFunction->typedLocalBindingLength = 1u;

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = callerFunction;
    ZrCore_Array_Init(state, &compilerState.typeValueAliases, sizeof(SZrTypeBinding), 1u);
    ZrCore_Memory_RawSet(&aliasBinding, 0, sizeof(aliasBinding));
    aliasBinding.name = ZrCore_String_CreateFromNative(state, "Option");
    TEST_ASSERT_NOT_NULL(aliasBinding.name);
    ZrParser_InferredType_InitFull(state,
                                   &aliasBinding.type,
                                   ZR_VALUE_TYPE_OBJECT,
                                   ZR_FALSE,
                                   ZrCore_String_CreateFromNative(state, "provider.Option"));
    TEST_ASSERT_NOT_NULL(aliasBinding.type.typeName);
    ZrCore_Array_Push(state, &compilerState.typeValueAliases, &aliasBinding);

    TEST_ASSERT_EQUAL_UINT32(0u, callerFunction->moduleEntryEffectLength);
    TEST_ASSERT_TRUE(compiler_build_function_metadata_tokens(&compilerState, callerFunction));
    callerTypeRef = find_type_ref_targeting_type_def(callerFunction, providerTypeDef->token);
    TEST_ASSERT_NOT_NULL(callerTypeRef);
    TEST_ASSERT_NOT_NULL(find_first_table_record(callerFunction, ZR_METADATA_TABLE_ASSEMBLY_REF));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_ASSEMBLY_REF, ZR_METADATA_TOKEN_TABLE(callerTypeRef->ownerToken));
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, callerTypeRef->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, callerTypeRef->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(providerFunction->moduleSignatureHash, callerTypeRef->targetModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->layoutVersion, callerTypeRef->layoutVersion);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->layoutHash, callerTypeRef->layoutHash);

    TEST_ASSERT_TRUE(ZrCore_Function_BindMatchingTypeRefMetadata(state, callerFunction, providerFunction));
    binding = ZrCore_Function_FindModuleMetadataBinding(callerFunction, callerTypeRef->token);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->token, binding->resolvedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, binding->resolvedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, binding->resolvedSignatureHash);

    ZrParser_InferredType_Free(state, &aliasBinding.type);
    ZrCore_Array_Free(state, &compilerState.typeValueAliases);
    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, providerAst);
    ZrParser_Ast_Free(state, providerAst);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_nested_unqualified_import_type_alias_annotation_emits_provider_type_ref(void) {
    const char *providerSource =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *providerModuleName;
    SZrString *providerSourceName;
    SZrAstNode *providerAst;
    SZrFunction *providerFunction;
    SZrFunction *callerFunction;
    SZrFunctionTypedLocalBinding *bindingInfo;
    SZrTypeBinding aliasBinding;
    SZrCompilerState compilerState;
    const SZrMetadataTokenRecord *providerTypeDef;
    const SZrMetadataTokenRecord *callerTypeRef;

    TEST_ASSERT_NOT_NULL(state);
    providerModuleName = ZrCore_String_CreateFromNative(state, "provider");
    providerSourceName = ZrCore_String_CreateFromNative(state, "provider.zr");
    TEST_ASSERT_NOT_NULL(providerModuleName);
    TEST_ASSERT_NOT_NULL(providerSourceName);
    providerAst = ZrParser_Parse(state, providerSource, strlen(providerSource), providerSourceName);
    TEST_ASSERT_NOT_NULL(providerAst);
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state,
                                                                            providerModuleName,
                                                                            providerAst));
    providerFunction = create_provider_union_function_fixture(state,
                                                             providerAst,
                                                             providerModuleName,
                                                             "choose",
                                                             "Option<int>");
    TEST_ASSERT_NOT_NULL(providerFunction);
    providerTypeDef = find_first_table_record(providerFunction, ZR_METADATA_TABLE_TYPE_DEF);
    TEST_ASSERT_NOT_NULL(providerTypeDef);

    callerFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    bindingInfo = (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedLocalBinding),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(bindingInfo);
    ZrCore_Memory_RawSet(bindingInfo, 0, sizeof(*bindingInfo));
    bindingInfo->name = ZrCore_String_CreateFromNative(state, "maybeBox");
    TEST_ASSERT_NOT_NULL(bindingInfo->name);
    init_named_type_ref(state, &bindingInfo->type, "Box<Option<int>>");
    callerFunction->typedLocalBindings = bindingInfo;
    callerFunction->typedLocalBindingLength = 1u;

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = callerFunction;
    ZrCore_Array_Init(state, &compilerState.typeValueAliases, sizeof(SZrTypeBinding), 1u);
    ZrCore_Memory_RawSet(&aliasBinding, 0, sizeof(aliasBinding));
    aliasBinding.name = ZrCore_String_CreateFromNative(state, "Option");
    TEST_ASSERT_NOT_NULL(aliasBinding.name);
    ZrParser_InferredType_InitFull(state,
                                   &aliasBinding.type,
                                   ZR_VALUE_TYPE_OBJECT,
                                   ZR_FALSE,
                                   ZrCore_String_CreateFromNative(state, "provider.Option"));
    TEST_ASSERT_NOT_NULL(aliasBinding.type.typeName);
    ZrCore_Array_Push(state, &compilerState.typeValueAliases, &aliasBinding);

    TEST_ASSERT_TRUE(compiler_build_function_metadata_tokens(&compilerState, callerFunction));
    callerTypeRef = find_type_ref_targeting_type_def(callerFunction, providerTypeDef->token);
    TEST_ASSERT_NOT_NULL(callerTypeRef);
    TEST_ASSERT_NOT_NULL(find_first_table_record(callerFunction, ZR_METADATA_TABLE_ASSEMBLY_REF));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_ASSEMBLY_REF, ZR_METADATA_TOKEN_TABLE(callerTypeRef->ownerToken));
    TEST_ASSERT_EQUAL_UINT32(providerTypeDef->relatedToken, callerTypeRef->targetSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(providerTypeDef->signatureHash, callerTypeRef->targetSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(providerFunction->moduleSignatureHash, callerTypeRef->targetModuleSignatureHash);

    ZrParser_InferredType_Free(state, &aliasBinding.type);
    ZrCore_Array_Free(state, &compilerState.typeValueAliases);
    ZrCore_Function_Free(state, callerFunction);
    ZrCore_Function_Free(state, providerFunction);
    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, providerAst);
    ZrParser_Ast_Free(state, providerAst);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_targeted_type_ref_binds_to_provider_type_def_identity);
    RUN_TEST(test_type_ref_binding_reports_layout_mismatch_without_partial_binding);
    RUN_TEST(test_type_ref_binding_mismatch_records_loader_diagnostic);
    RUN_TEST(test_import_target_signature_emits_stable_provider_type_ref);
    RUN_TEST(test_nested_generic_import_target_signature_emits_provider_type_ref);
    RUN_TEST(test_explicit_import_type_annotation_emits_provider_type_ref);
    RUN_TEST(test_unqualified_import_type_alias_annotation_emits_provider_type_ref);
    RUN_TEST(test_nested_unqualified_import_type_alias_annotation_emits_provider_type_ref);
    return UNITY_END();
}
