#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/writer.h"

void setUp(void) {}

void tearDown(void) {}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL(strstr(text, needle));
}

static void assert_text_does_not_contain(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NULL(strstr(text, needle));
}

static TZrInstruction test_create_instruction_2(EZrInstructionCode opcode,
                                                TZrUInt16 operandExtra,
                                                TZrUInt16 operandA,
                                                TZrUInt16 operandB) {
    TZrInstruction instruction;

    instruction.value = 0u;
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operandA;
    instruction.instruction.operand.operand1[1] = operandB;
    return instruction;
}

static SZrObject *get_or_create_function_metadata_object(SZrState *state, SZrFunction *function) {
    SZrObject *metadataObject;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    if (function->hasDecoratorMetadata &&
        function->decoratorMetadataValue.type == ZR_VALUE_TYPE_OBJECT &&
        function->decoratorMetadataValue.value.object != ZR_NULL) {
        metadataObject = ZR_CAST_OBJECT(state, function->decoratorMetadataValue.value.object);
        if (metadataObject != ZR_NULL) {
            return metadataObject;
        }
    }

    metadataObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(metadataObject);
    ZrCore_Value_InitAsRawObject(state,
                                 &function->decoratorMetadataValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    function->hasDecoratorMetadata = ZR_TRUE;
    return metadataObject;
}

static void mark_function_metadata_bool(SZrState *state,
                                        SZrFunction *function,
                                        const TZrChar *fieldName,
                                        TZrBool fieldValue) {
    SZrObject *metadataObject;
    SZrString *fieldString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(fieldName);

    metadataObject = get_or_create_function_metadata_object(state, function);
    TEST_ASSERT_NOT_NULL(metadataObject);
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(state,
                                 &key,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    ZrCore_Value_InitAsBool(state, &value, fieldValue);
    ZrCore_Object_SetValue(state, metadataObject, &key, &value);
}

static void mark_function_metadata_string(SZrState *state,
                                          SZrFunction *function,
                                          const TZrChar *fieldName,
                                          const TZrChar *fieldValue) {
    SZrObject *metadataObject;
    SZrString *fieldString;
    SZrString *valueString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(fieldName);
    TEST_ASSERT_NOT_NULL(fieldValue);

    metadataObject = get_or_create_function_metadata_object(state, function);
    TEST_ASSERT_NOT_NULL(metadataObject);
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    valueString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldValue);
    TEST_ASSERT_NOT_NULL(fieldString);
    TEST_ASSERT_NOT_NULL(valueString);
    ZrCore_Value_InitAsRawObject(state,
                                 &key,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    ZrCore_Value_InitAsRawObject(state,
                                 &value,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(valueString));
    ZrCore_Object_SetValue(state, metadataObject, &key, &value);
}

static void mark_function_metadata_uint(SZrState *state,
                                        SZrFunction *function,
                                        const TZrChar *fieldName,
                                        TZrUInt64 fieldValue) {
    SZrObject *metadataObject;
    SZrString *fieldString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(fieldName);

    metadataObject = get_or_create_function_metadata_object(state, function);
    TEST_ASSERT_NOT_NULL(metadataObject);
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(state,
                                 &key,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    ZrCore_Value_InitAsUInt(state, &value, fieldValue);
    ZrCore_Object_SetValue(state, metadataObject, &key, &value);
}

static void mark_function_reflectable(SZrState *state, SZrFunction *function) {
    mark_function_metadata_bool(state, function, "reflectable", ZR_TRUE);
}

static void mark_function_dynamic_dependency_function_index(SZrState *state,
                                                           SZrFunction *function,
                                                           TZrUInt32 targetFunctionIndex) {
    mark_function_metadata_uint(state,
                                function,
                                "dynamicDependencyFunctionIndex",
                                (TZrUInt64)targetFunctionIndex);
}

static void mark_function_dynamic_dependency_method_token(SZrState *state,
                                                         SZrFunction *function,
                                                         TZrMetadataToken methodToken) {
    mark_function_metadata_uint(state,
                                function,
                                "dynamicDependencyMethodToken",
                                (TZrUInt64)methodToken);
}

static void mark_function_dynamic_dependency_method_name(SZrState *state,
                                                        SZrFunction *function,
                                                        const TZrChar *methodName) {
    mark_function_metadata_string(state,
                                  function,
                                  "dynamicDependencyMethodName",
                                  methodName);
}

static void mark_function_dynamic_dependency_method_signature_hash(SZrState *state,
                                                                  SZrFunction *function,
                                                                  TZrUInt64 signatureHash) {
    mark_function_metadata_uint(state,
                                function,
                                "dynamicDependencyMethodSignatureHash",
                                signatureHash);
}

static void mark_function_requires_unreferenced_code(SZrState *state, SZrFunction *function) {
    mark_function_metadata_bool(state, function, "requiresUnreferencedCode", ZR_TRUE);
}

static void mark_function_suppresses_requires_unreferenced_code_warning(SZrState *state, SZrFunction *function) {
    mark_function_metadata_bool(state, function, "suppressRequiresUnreferencedCodeWarning", ZR_TRUE);
}

static void mark_function_requires_unreferenced_code_with_reason(SZrState *state,
                                                                 SZrFunction *function,
                                                                 const TZrChar *reason) {
    mark_function_requires_unreferenced_code(state, function);
    mark_function_metadata_string(state, function, "requiresUnreferencedCodeReason", reason);
}

static TZrInstruction create_function_call_instruction(TZrUInt16 destinationSlot,
                                                       TZrUInt16 functionSlot,
                                                       TZrUInt16 argumentCount) {
    return test_create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                     destinationSlot,
                                     functionSlot,
                                     argumentCount);
}

static SZrFunction *create_reflection_annotation_trim_fixture(SZrState *state) {
    SZrFunction *root;

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);

    root->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->instructionsList);
    root->instructionsList[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 0u, 0u, 0u);
    root->instructionsLength = 1u;
    root->stackSize = 1u;
    root->parameterCount = 0u;
    root->lineInSourceStart = 1u;
    root->lineInSourceEnd = 1u;

    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction) * 2u);
    root->childFunctionLength = 2u;

    root->childFunctionList[0].parameterCount = 0u;
    root->childFunctionList[0].stackSize = 1u;
    root->childFunctionList[0].ownerFunction = root;
    root->childFunctionList[0].lineInSourceStart = 10u;
    root->childFunctionList[0].lineInSourceEnd = 10u;

    root->childFunctionList[1].parameterCount = 0u;
    root->childFunctionList[1].stackSize = 1u;
    root->childFunctionList[1].ownerFunction = root;
    root->childFunctionList[1].lineInSourceStart = 20u;
    root->childFunctionList[1].lineInSourceEnd = 20u;
    mark_function_reflectable(state, &root->childFunctionList[1]);
    return root;
}

static SZrFunctionTypedExportSymbol *allocate_typed_exported_symbols(
        SZrState *state,
        SZrFunction *root,
        TZrUInt32 symbolCount) {
    SZrFunctionTypedExportSymbol *symbols;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, symbolCount);
    symbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol) * symbolCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(symbols);
    memset(symbols, 0, sizeof(SZrFunctionTypedExportSymbol) * symbolCount);
    root->typedExportedSymbols = symbols;
    root->typedExportedSymbolLength = symbolCount;
    return symbols;
}

static void attach_typed_method_token(SZrFunction *root,
                                      SZrFunctionTypedExportSymbol *symbol,
                                      TZrUInt32 callableChildIndex,
                                      TZrMetadataToken methodToken,
                                      TZrUInt8 exportKind) {
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(symbol);

    memset(symbol, 0, sizeof(*symbol));
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = exportKind;
    symbol->callableChildIndex = callableChildIndex;
    symbol->metadataToken = methodToken;
    root->typedExportedSymbols = symbol;
    root->typedExportedSymbolLength = 1u;
}

static void attach_typed_exported_method_token(SZrFunction *root,
                                               SZrFunctionTypedExportSymbol *symbol,
                                               TZrUInt32 callableChildIndex,
                                               TZrMetadataToken methodToken) {
    attach_typed_method_token(root,
                              symbol,
                              callableChildIndex,
                              methodToken,
                              ZR_MODULE_EXPORT_KIND_FUNCTION);
}

static void init_typed_exported_method_name(SZrState *state,
                                            SZrFunctionTypedExportSymbol *symbol,
                                            TZrUInt32 callableChildIndex,
                                            const TZrChar *methodName,
                                            TZrUInt64 signatureHash) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_NOT_NULL(methodName);

    memset(symbol, 0, sizeof(*symbol));
    symbol->name = ZrCore_String_CreateFromNative(state, (TZrNativeString)methodName);
    TEST_ASSERT_NOT_NULL(symbol->name);
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->callableChildIndex = callableChildIndex;
    symbol->signatureHash = signatureHash;
}

static void attach_typed_exported_method_name(SZrState *state,
                                              SZrFunction *root,
                                              SZrFunctionTypedExportSymbol *symbol,
                                              TZrUInt32 callableChildIndex,
                                              const TZrChar *methodName) {
    TEST_ASSERT_NOT_NULL(root);

    init_typed_exported_method_name(state, symbol, callableChildIndex, methodName, 0u);
    root->typedExportedSymbols = symbol;
    root->typedExportedSymbolLength = 1u;
}

static SZrFunction *create_requires_unreferenced_code_call_fixture(SZrState *state, TZrBool annotateCallee) {
    SZrFunction *root;
    SZrFunction *childFunction;

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);

    root->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->instructionsList);
    root->instructionsList[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 0u, 0u, 0u);
    root->instructionsList[1] = create_function_call_instruction(1u, 0u, 0u);
    root->instructionsList[2] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 0u, 0u, 0u);
    root->instructionsLength = 3u;
    root->stackSize = 2u;
    root->parameterCount = 0u;
    root->lineInSourceStart = 31u;
    root->lineInSourceEnd = 35u;
    root->sourceCodeList = ZrCore_String_CreateFromNative(state, "requires_unreferenced_warning.zr");
    TEST_ASSERT_NOT_NULL(root->sourceCodeList);
    root->executionLocationInfoList = (SZrFunctionExecutionLocationInfo *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionExecutionLocationInfo),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->executionLocationInfoList);
    memset(root->executionLocationInfoList, 0, sizeof(SZrFunctionExecutionLocationInfo));
    root->executionLocationInfoLength = 1u;
    root->executionLocationInfoList[0].currentInstructionOffset = 1u;
    root->executionLocationInfoList[0].lineInSource = 33u;
    root->executionLocationInfoList[0].columnInSourceStart = 9u;
    root->executionLocationInfoList[0].lineInSourceEnd = 33u;
    root->executionLocationInfoList[0].columnInSourceEnd = 23u;

    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction));
    root->childFunctionLength = 1u;

    childFunction = &root->childFunctionList[0];
    childFunction->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(childFunction->instructionsList);
    childFunction->instructionsList[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 0u, 0u, 0u);
    childFunction->instructionsLength = 1u;
    childFunction->stackSize = 0u;
    childFunction->parameterCount = 0u;
    childFunction->ownerFunction = root;
    childFunction->lineInSourceStart = 41u;
    childFunction->lineInSourceEnd = 41u;
    if (annotateCallee) {
        mark_function_requires_unreferenced_code(state, childFunction);
    }
    return root;
}

static SZrFunction *create_requires_unreferenced_code_call_fixture_with_reason(SZrState *state,
                                                                               const TZrChar *reason) {
    SZrFunction *root = create_requires_unreferenced_code_call_fixture(state, ZR_FALSE);

    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    mark_function_requires_unreferenced_code_with_reason(state, &root->childFunctionList[0], reason);
    return root;
}

static void test_aot_c_code_stripping_preserves_reflectable_function_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_annotation_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_reflection_annotation_preserve";
    options.sourceHash = "aot-c-reflection-annotation-preserve";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-reflection-annotation-preserve";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "reflectable_function_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoot[0] = 2 */");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_prunes_unannotated_unreachable_function(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_annotation_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->childFunctionList[1].hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&function->childFunctionList[1].decoratorMetadataValue);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_reflection_annotation_prune";
    options.sourceHash = "aot-c-reflection-annotation-prune";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-reflection-annotation-prune";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "unannotated_function_prune",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoots = 0 */");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state)");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_1(struct SZrState *state)");
    assert_text_does_not_contain(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 2 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 1 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_function_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_annotation_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->childFunctionList[1].hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&function->childFunctionList[1].decoratorMetadataValue);
    mark_function_dynamic_dependency_function_index(state, function, 2u);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_dynamic_dependency_preserve";
    options.sourceHash = "aot-c-dynamic-dependency-preserve";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-dynamic-dependency-preserve";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "dynamic_dependency_function_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoot[0] = 2 */");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_method_token_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u);
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *exportedSymbol;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_annotation_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->childFunctionList[1].hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&function->childFunctionList[1].decoratorMetadataValue);
    exportedSymbol = allocate_typed_exported_symbols(state, function, 1u);
    attach_typed_exported_method_token(function, exportedSymbol, 1u, methodToken);
    mark_function_dynamic_dependency_method_token(state, function, methodToken);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_dynamic_dependency_method_token_preserve";
    options.sourceHash = "aot-c-dynamic-dependency-method-token-preserve";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-dynamic-dependency-method-token-preserve";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "dynamic_dependency_method_token_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoot[0] = 2 */");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_non_exported_dynamic_dependency_method_token_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 8u);
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *methodSymbol;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_annotation_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->childFunctionList[1].hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&function->childFunctionList[1].decoratorMetadataValue);
    methodSymbol = allocate_typed_exported_symbols(state, function, 1u);
    attach_typed_method_token(function,
                              methodSymbol,
                              1u,
                              methodToken,
                              ZR_MODULE_EXPORT_KIND_VALUE);
    mark_function_dynamic_dependency_method_token(state, function, methodToken);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_non_exported_dynamic_dependency_method_token_preserve";
    options.sourceHash = "aot-c-non-exported-dynamic-dependency-method-token-preserve";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-non-exported-dynamic-dependency-method-token-preserve";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "non_exported_dynamic_dependency_method_token_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoot[0] = 2 */");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_method_name_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *exportedSymbol;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_annotation_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->childFunctionList[1].hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&function->childFunctionList[1].decoratorMetadataValue);
    exportedSymbol = allocate_typed_exported_symbols(state, function, 1u);
    attach_typed_exported_method_name(state, function, exportedSymbol, 1u, "target");
    mark_function_dynamic_dependency_method_name(state, function, "target");

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_dynamic_dependency_method_name_preserve";
    options.sourceHash = "aot-c-dynamic-dependency-method-name-preserve";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-dynamic-dependency-method-name-preserve";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "dynamic_dependency_method_name_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoot[0] = 2 */");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_preserves_dynamic_dependency_method_name_signature_hash_metadata(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *exportedSymbols;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_reflection_annotation_trim_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    function->childFunctionList[1].hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&function->childFunctionList[1].decoratorMetadataValue);
    exportedSymbols = allocate_typed_exported_symbols(state, function, 2u);
    init_typed_exported_method_name(state, &exportedSymbols[0], 0u, "target", 0x1111u);
    init_typed_exported_method_name(state, &exportedSymbols[1], 1u, "target", 0x2222u);
    mark_function_dynamic_dependency_method_name(state, function, "target");
    mark_function_dynamic_dependency_method_signature_hash(state, function, 0x2222u);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_dynamic_dependency_method_name_signature_hash_preserve";
    options.sourceHash = "aot-c-dynamic-dependency-method-name-signature-hash-preserve";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-dynamic-dependency-method-name-signature-hash-preserve";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "dynamic_dependency_method_name_signature_hash_root",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoots = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.annotationRoot[0] = 2 */");
    assert_text_contains(generatedCText, "static TZrInt64 zr_aot_fn_2(struct SZrState *state)");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_emits_trim_annotation_warning_for_requires_unreferenced_code_callee(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_requires_unreferenced_code_call_fixture(state, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_requires_unreferenced_warning";
    options.sourceHash = "aot-c-requires-unreferenced-warning";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-requires-unreferenced-warning";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "requires_unreferenced_call_warning",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* trim_warnings.annotationCount = 1 */");
    assert_text_contains(generatedCText,
                         "/* trim_warning.annotation[0] function=0 instruction=1 targetFunction=1 sourceFile=\"requires_unreferenced_warning.zr\" sourceLine=33 sourceLineEnd=33 sourceColumn=9 sourceColumnEnd=23 reason=requires-unreferenced-code */");
    assert_text_contains(generatedCText, "/* trim_warnings.runtimeFallbackCount = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_suppresses_trim_annotation_warning_for_requires_unreferenced_code_callee(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_requires_unreferenced_code_call_fixture(state, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_requires_unreferenced_warning_suppressed";
    options.sourceHash = "aot-c-requires-unreferenced-warning-suppressed";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-requires-unreferenced-warning-suppressed";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    options.suppressAnnotationWarnings = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "requires_unreferenced_call_warning_suppressed",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* trim_warnings.annotationCount = 0 */");
    assert_text_contains(generatedCText, "/* trim_warnings.annotationSuppressedCount = 1 */");
    assert_text_does_not_contain(generatedCText, "trim_warning.annotation[0]");
    assert_text_contains(generatedCText, "/* trim_warnings.runtimeFallbackCount = 0 */");
    assert_text_contains(generatedCText, "/* trim_warnings.runtimeFallbackSuppressedCount = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_suppresses_trim_annotation_warning_from_callsite_annotation(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_requires_unreferenced_code_call_fixture(state, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(function);
    mark_function_suppresses_requires_unreferenced_code_warning(state, function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_requires_unreferenced_callsite_suppressed";
    options.sourceHash = "aot-c-requires-unreferenced-callsite-suppressed";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-requires-unreferenced-callsite-suppressed";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "requires_unreferenced_callsite_suppressed",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* trim_warnings.annotationCount = 0 */");
    assert_text_contains(generatedCText, "/* trim_warnings.annotationSuppressedCount = 1 */");
    assert_text_does_not_contain(generatedCText, "trim_warning.annotation[0]");
    assert_text_contains(generatedCText, "/* trim_warnings.runtimeFallbackCount = 0 */");
    assert_text_contains(generatedCText, "/* trim_warnings.runtimeFallbackSuppressedCount = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_skips_trim_annotation_warning_for_unannotated_static_callee(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_requires_unreferenced_code_call_fixture(state, ZR_FALSE);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_no_requires_unreferenced_warning";
    options.sourceHash = "aot-c-no-requires-unreferenced-warning";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-no-requires-unreferenced-warning";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "unannotated_static_call_no_warning",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* trim_warnings.annotationCount = 0 */");
    assert_text_does_not_contain(generatedCText, "trim_warning.annotation[0]");
    assert_text_contains(generatedCText, "/* trim_warnings.runtimeFallbackCount = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_emits_trim_annotation_warning_reason_text_for_requires_unreferenced_code_callee(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_requires_unreferenced_code_call_fixture_with_reason(state, "uses \"name\" lookup");
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_requires_unreferenced_warning_reason";
    options.sourceHash = "aot-c-requires-unreferenced-warning-reason";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-requires-unreferenced-warning-reason";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reflection_annotation_preserve",
                                                       "generated",
                                                       "requires_unreferenced_call_warning_reason",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = ZrTests_ReadTextFile(generatedCPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedCText, "/* trim_warnings.annotationCount = 1 */");
    assert_text_contains(generatedCText,
                         "/* trim_warning.annotation[0] function=0 instruction=1 targetFunction=1 sourceFile=\"requires_unreferenced_warning.zr\" sourceLine=33 sourceLineEnd=33 sourceColumn=9 sourceColumnEnd=23 reason=requires-unreferenced-code message=\"uses \\\"name\\\" lookup\" */");
    assert_text_contains(generatedCText, "/* trim_warnings.runtimeFallbackCount = 0 */");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_code_stripping_preserves_reflectable_function_metadata);
    RUN_TEST(test_aot_c_code_stripping_prunes_unannotated_unreachable_function);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_function_metadata);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_method_token_metadata);
    RUN_TEST(test_aot_c_code_stripping_preserves_non_exported_dynamic_dependency_method_token_metadata);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_method_name_metadata);
    RUN_TEST(test_aot_c_code_stripping_preserves_dynamic_dependency_method_name_signature_hash_metadata);
    RUN_TEST(test_aot_c_emits_trim_annotation_warning_for_requires_unreferenced_code_callee);
    RUN_TEST(test_aot_c_suppresses_trim_annotation_warning_for_requires_unreferenced_code_callee);
    RUN_TEST(test_aot_c_suppresses_trim_annotation_warning_from_callsite_annotation);
    RUN_TEST(test_aot_c_skips_trim_annotation_warning_for_unannotated_static_callee);
    RUN_TEST(test_aot_c_emits_trim_annotation_warning_reason_text_for_requires_unreferenced_code_callee);
    return UNITY_END();
}
