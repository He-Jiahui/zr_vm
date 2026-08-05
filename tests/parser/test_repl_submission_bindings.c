#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_parser/type_system.h"

void setUp(void) {}

void tearDown(void) {}

static TZrBool submission_function_has_opcode(
        const SZrFunction *function,
        EZrInstructionCode opcode) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool submission_function_has_value_call(const SZrFunction *function) {
    return (TZrBool)(
            submission_function_has_opcode(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)) ||
            submission_function_has_opcode(function, ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)));
}

static SZrClosure *submission_closure_new(SZrState *state, SZrFunction *function) {
    SZrClosure *closure;

    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }

    closure = ZrCore_Closure_New(state, function->closureValueLength);
    if (closure == ZR_NULL) {
        return ZR_NULL;
    }
    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);
    return closure;
}

static TZrBool submission_closure_copy_capture(
        SZrState *state,
        SZrClosure *destination,
        TZrUInt32 destinationIndex,
        const SZrClosure *source,
        TZrUInt32 sourceIndex) {
    SZrClosureValue *destinationCapture;
    SZrClosureValue *sourceCapture;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL ||
        destinationIndex >= destination->closureValueCount ||
        sourceIndex >= source->closureValueCount) {
        return ZR_FALSE;
    }

    destinationCapture = destination->closureValuesExtend[destinationIndex];
    sourceCapture = source->closureValuesExtend[sourceIndex];
    if (destinationCapture == ZR_NULL || sourceCapture == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(
            state,
            ZrCore_ClosureValue_GetValue(destinationCapture),
            ZrCore_ClosureValue_GetValue(sourceCapture));
    return ZR_TRUE;
}

static TZrBool submission_closure_execute(
        SZrState *state,
        SZrClosure *closure,
        SZrTypeValue *result) {
    SZrFunction *function;
    SZrFunctionStackAnchor stackAnchor;
    TZrStackValuePointer base;
    TZrStackValuePointer resultBase;
    SZrTypeValue *closureValue;
    TZrBool ignoredFunction;

    if (state == ZR_NULL || closure == ZR_NULL || closure->function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    function = closure->function;
    ZrCore_Value_ResetAsNull(result);
    ignoredFunction = ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(
            state, function->stackSize + 1u, base, base, &stackAnchor);
    closureValue = ZrCore_Stack_GetValue(base);
    if (closureValue == ZR_NULL) {
        if (ignoredFunction) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
        }
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer = base + 1;
    if (ignoredFunction) {
        ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    }

    resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &stackAnchor, 1u);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || resultBase == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(resultBase));
    return ZR_TRUE;
}

static void test_submission_context_projects_prior_binding_as_canonical_capture(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionBinding binding;
    SZrParserSubmissionContext context;
    SZrParserSubmissionResult result;
    SZrFunction *function;
    const SZrFunctionTypedTypeRef *captureType = ZR_NULL;
    SZrFunctionSourceRange captureRange;
    TZrUInt32 captureSymbolId = 0u;
    TZrUInt32 captureTypeId = 0u;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "repl_submission_bindings.zr", strlen("repl_submission_bindings.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);

    memset(&binding, 0, sizeof(binding));
    memset(&context, 0, sizeof(context));
    memset(&result, 0, sizeof(result));
    binding.name = ZrCore_String_Create(state, "seed", strlen("seed"));
    binding.kind = ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE;
    ZrParser_InferredType_Init(state, &binding.inferredType, ZR_VALUE_TYPE_INT64);
    binding.symbolId = 701u;
    binding.typeId = 702u;
    binding.placeId = 703u;
    binding.captureIndex = 0u;
    binding.moduleGeneration = 1u;
    binding.environmentGeneration = 1u;
    binding.cellGeneration = 1u;
    binding.declarationRange = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0u, 1, 1),
            ZrParser_FilePosition_Create(4u, 1, 5),
            sourceName);
    TEST_ASSERT_NOT_NULL(binding.name);

    context.bindings = &binding;
    context.bindingCount = 1u;
    context.moduleGeneration = 1u;
    context.environmentGeneration = 1u;
    context.cellGeneration = 2u;

    function = ZrParser_Source_CompileSubmission(
            state,
            "return seed + 3;",
            strlen("return seed + 3;"),
            sourceName,
            &context,
            &result);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(1u, function->closureValueLength);
    TEST_ASSERT_NOT_NULL(function->closureValueList);
    TEST_ASSERT_EQUAL_UINT32(binding.symbolId, function->closureValueList[0].symbolId);
    TEST_ASSERT_EQUAL_UINT32(binding.typeId, function->closureValueList[0].typeId);
    TEST_ASSERT_NOT_NULL(function->typedClosureBindings);
    TEST_ASSERT_EQUAL_UINT32(1u, function->typedClosureBindingLength);
    TEST_ASSERT_TRUE(ZrCore_Function_GetClosureCaptureIdentity(
            function,
            binding.captureIndex,
            &captureType,
            &captureSymbolId,
            &captureTypeId,
            &captureRange));
    TEST_ASSERT_NOT_NULL(captureType);
    TEST_ASSERT_EQUAL_UINT32(binding.symbolId, captureSymbolId);
    TEST_ASSERT_EQUAL_UINT32(binding.typeId, captureTypeId);
    TEST_ASSERT_EQUAL_UINT32(1u, captureRange.startLine);
    TEST_ASSERT_EQUAL_UINT32(1u, captureRange.startColumn);
    TEST_ASSERT_EQUAL_UINT32(1u, captureRange.endLine);
    TEST_ASSERT_EQUAL_UINT32(5u, captureRange.endColumn);

    ZrParser_SubmissionResult_Free(state, &result);
    ZrParser_InferredType_Free(state, &binding.inferredType);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_declaration_publishes_new_closure_binding(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionContext context;
    SZrParserSubmissionResult result;
    SZrParserSubmissionBinding *binding;
    SZrFunction *function;
    const SZrFunctionTypedTypeRef *captureType = ZR_NULL;
    SZrFunctionSourceRange captureRange;
    TZrUInt32 captureSymbolId = 0u;
    TZrUInt32 captureTypeId = 0u;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "repl_submission_declaration.zr", strlen("repl_submission_declaration.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&context, 0, sizeof(context));
    memset(&result, 0, sizeof(result));
    context.moduleGeneration = 1u;
    context.environmentGeneration = 1u;
    context.cellGeneration = 1u;

    function = ZrParser_Source_CompileSubmission(
            state,
            "var count = 7;",
            strlen("var count = 7;"),
            sourceName,
            &context,
            &result);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)result.bindingCount);
    TEST_ASSERT_NOT_NULL(result.bindings);
    binding = &result.bindings[0];
    TEST_ASSERT_EQUAL_STRING("count", ZrCore_String_GetNativeString(binding->name));
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE, binding->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, binding->inferredType.baseType);
    TEST_ASSERT_NOT_EQUAL(0u, binding->symbolId);
    TEST_ASSERT_NOT_EQUAL(0u, binding->typeId);
    TEST_ASSERT_EQUAL_UINT32(0u, binding->captureIndex);
    TEST_ASSERT_EQUAL_UINT64(context.moduleGeneration, binding->moduleGeneration);
    TEST_ASSERT_EQUAL_UINT64(context.environmentGeneration, binding->environmentGeneration);
    TEST_ASSERT_EQUAL_UINT64(context.cellGeneration, binding->cellGeneration);
    TEST_ASSERT_EQUAL_INT(1, binding->declarationRange.start.line);
    TEST_ASSERT_EQUAL_INT(5, binding->declarationRange.start.column);
    TEST_ASSERT_EQUAL_INT(1, binding->declarationRange.end.line);
    TEST_ASSERT_EQUAL_INT(10, binding->declarationRange.end.column);
    TEST_ASSERT_EQUAL_UINT32(1u, function->closureValueLength);
    TEST_ASSERT_TRUE(ZrCore_Function_GetClosureCaptureIdentity(
            function,
            binding->captureIndex,
            &captureType,
            &captureSymbolId,
            &captureTypeId,
            &captureRange));
    TEST_ASSERT_NOT_NULL(captureType);
    TEST_ASSERT_EQUAL_UINT32(binding->symbolId, captureSymbolId);
    TEST_ASSERT_EQUAL_UINT32(binding->typeId, captureTypeId);
    TEST_ASSERT_EQUAL_UINT32(1u, captureRange.startLine);
    TEST_ASSERT_EQUAL_UINT32(5u, captureRange.startColumn);
    TEST_ASSERT_EQUAL_UINT32(1u, captureRange.endLine);
    TEST_ASSERT_EQUAL_UINT32(10u, captureRange.endColumn);

    ZrParser_SubmissionResult_Free(state, &result);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_context_rejects_stale_and_nonpersistable_bindings(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionBinding binding;
    SZrParserSubmissionBinding duplicateBindings[2];
    SZrParserSubmissionContext context;
    SZrParserSubmissionResult result;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_rejections.zr",
            strlen("repl_submission_rejections.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&binding, 0, sizeof(binding));
    memset(&context, 0, sizeof(context));
    memset(&result, 0, sizeof(result));
    binding.name = ZrCore_String_Create(state, "seed", strlen("seed"));
    binding.kind = ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE;
    ZrParser_InferredType_Init(state, &binding.inferredType, ZR_VALUE_TYPE_INT64);
    binding.symbolId = 801u;
    binding.typeId = 802u;
    binding.placeId = 803u;
    binding.captureIndex = 0u;
    binding.moduleGeneration = 1u;
    binding.environmentGeneration = 1u;
    binding.cellGeneration = 1u;
    binding.declarationRange = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0u, 1, 1),
            ZrParser_FilePosition_Create(4u, 1, 5),
            sourceName);
    context.bindings = &binding;
    context.bindingCount = 1u;
    context.moduleGeneration = 1u;
    context.environmentGeneration = 1u;
    context.cellGeneration = 2u;

    binding.cellGeneration = context.cellGeneration;
    TEST_ASSERT_NULL(ZrParser_Source_CompileSubmission(
            state,
            "seed;",
            strlen("seed;"),
            sourceName,
            &context,
            &result));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.bindingCount);

    binding.cellGeneration = 1u;
    binding.inferredType.referenceAccess = ZR_REFERENCE_ACCESS_READONLY;
    TEST_ASSERT_NULL(ZrParser_Source_CompileSubmission(
            state,
            "return seed;",
            strlen("return seed;"),
            sourceName,
            &context,
            &result));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.bindingCount);

    binding.inferredType.referenceAccess = ZR_REFERENCE_ACCESS_NONE;
    binding.inferredType.protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE);
    TEST_ASSERT_NULL(ZrParser_Source_CompileSubmission(
            state,
            "return seed;",
            strlen("return seed;"),
            sourceName,
            &context,
            &result));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.bindingCount);

    binding.inferredType.protocolMask = 0u;
    duplicateBindings[0] = binding;
    duplicateBindings[1] = binding;
    duplicateBindings[1].name = ZrCore_String_Create(state, "seedCopy", strlen("seedCopy"));
    duplicateBindings[1].captureIndex = 1u;
    TEST_ASSERT_NOT_NULL(duplicateBindings[1].name);
    context.bindings = duplicateBindings;
    context.bindingCount = 2u;
    TEST_ASSERT_NULL(ZrParser_Source_CompileSubmission(
            state,
            "return seed;",
            strlen("return seed;"),
            sourceName,
            &context,
            &result));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.bindingCount);

    context.bindings = &binding;
    context.bindingCount = 1u;
    binding.inferredType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
    TEST_ASSERT_NULL(ZrParser_Source_CompileSubmission(
            state,
            "seed;",
            strlen("seed;"),
            sourceName,
            &context,
            &result));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.bindingCount);

    ZrParser_InferredType_Free(state, &binding.inferredType);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_assignment_uses_existing_closure_capture(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionBinding binding;
    SZrParserSubmissionContext context;
    SZrParserSubmissionResult result;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_assignment.zr",
            strlen("repl_submission_assignment.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&binding, 0, sizeof(binding));
    memset(&context, 0, sizeof(context));
    memset(&result, 0, sizeof(result));
    binding.name = ZrCore_String_Create(state, "count", strlen("count"));
    binding.kind = ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE;
    ZrParser_InferredType_Init(state, &binding.inferredType, ZR_VALUE_TYPE_INT64);
    binding.symbolId = 901u;
    binding.typeId = 902u;
    binding.placeId = 903u;
    binding.captureIndex = 0u;
    binding.moduleGeneration = 1u;
    binding.environmentGeneration = 1u;
    binding.cellGeneration = 1u;
    binding.declarationRange = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0u, 1, 1),
            ZrParser_FilePosition_Create(5u, 1, 6),
            sourceName);
    context.bindings = &binding;
    context.bindingCount = 1u;
    context.moduleGeneration = 1u;
    context.environmentGeneration = 1u;
    context.cellGeneration = 2u;

    function = ZrParser_Source_CompileSubmission(
            state,
            "count = count + 1;",
            strlen("count = count + 1;"),
            sourceName,
            &context,
            &result);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(1u, function->closureValueLength);
    TEST_ASSERT_TRUE(submission_function_has_opcode(
            function, ZR_INSTRUCTION_ENUM(GET_CLOSURE)));
    TEST_ASSERT_TRUE(submission_function_has_opcode(
            function, ZR_INSTRUCTION_ENUM(SET_CLOSURE)));

    ZrParser_SubmissionResult_Free(state, &result);
    ZrParser_InferredType_Free(state, &binding.inferredType);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_function_publishes_canonical_callable_signature(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionContext context;
    SZrParserSubmissionResult result;
    SZrParserSubmissionBinding *binding;
    SZrParserSubmissionCallableSignature *signature;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_function.zr",
            strlen("repl_submission_function.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&context, 0, sizeof(context));
    memset(&result, 0, sizeof(result));
    context.moduleGeneration = 1u;
    context.environmentGeneration = 1u;
    context.cellGeneration = 1u;

    function = ZrParser_Source_CompileSubmission(
            state,
            "fn increment(value: int): int { return value + 1; }",
            strlen("fn increment(value: int): int { return value + 1; }"),
            sourceName,
            &context,
            &result);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)result.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)result.callableSignatureCount);
    TEST_ASSERT_NOT_NULL(result.bindings);
    TEST_ASSERT_NOT_NULL(result.callableSignatures);
    binding = &result.bindings[0];
    signature = &result.callableSignatures[binding->callableSignatureIndex];
    TEST_ASSERT_EQUAL_STRING("increment", ZrCore_String_GetNativeString(binding->name));
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_SUBMISSION_BINDING_KIND_CALLABLE, binding->kind);
    TEST_ASSERT_EQUAL_UINT32(0u, binding->captureIndex);
    TEST_ASSERT_EQUAL_UINT32(0u, binding->callableSignatureIndex);
    TEST_ASSERT_EQUAL_UINT32(binding->symbolId, signature->symbolId);
    TEST_ASSERT_EQUAL_UINT32(binding->typeId, signature->typeId);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, signature->returnType.baseType);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)signature->parameterTypes.length);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)signature->parameterPassingModes.length);
    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_TYPE_INT64,
            ((const SZrInferredType *)signature->parameterTypes.head)[0].baseType);

    ZrParser_SubmissionResult_Free(state, &result);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_context_consumes_canonical_callable_signature(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionContext firstContext;
    SZrParserSubmissionContext secondContext;
    SZrParserSubmissionResult firstResult;
    SZrParserSubmissionResult secondResult;
    SZrFunction *firstFunction;
    SZrFunction *secondFunction;
    const SZrFunctionTypedTypeRef *captureType = ZR_NULL;
    SZrFunctionSourceRange captureRange;
    TZrUInt32 captureSymbolId = 0u;
    TZrUInt32 captureTypeId = 0u;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_callable_consumer.zr",
            strlen("repl_submission_callable_consumer.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&firstContext, 0, sizeof(firstContext));
    memset(&secondContext, 0, sizeof(secondContext));
    memset(&firstResult, 0, sizeof(firstResult));
    memset(&secondResult, 0, sizeof(secondResult));
    firstContext.moduleGeneration = 1u;
    firstContext.environmentGeneration = 1u;
    firstContext.cellGeneration = 1u;

    firstFunction = ZrParser_Source_CompileSubmission(
            state,
            "fn increment(value: int): int { return value + 1; }",
            strlen("fn increment(value: int): int { return value + 1; }"),
            sourceName,
            &firstContext,
            &firstResult);
    TEST_ASSERT_NOT_NULL(firstFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)firstResult.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)firstResult.callableSignatureCount);

    secondContext.bindings = firstResult.bindings;
    secondContext.bindingCount = firstResult.bindingCount;
    secondContext.callableSignatures = firstResult.callableSignatures;
    secondContext.callableSignatureCount = firstResult.callableSignatureCount;
    secondContext.moduleGeneration = 1u;
    secondContext.environmentGeneration = 1u;
    secondContext.cellGeneration = 2u;
    secondFunction = ZrParser_Source_CompileSubmission(
            state,
            "return increment(2);",
            strlen("return increment(2);"),
            sourceName,
            &secondContext,
            &secondResult);
    TEST_ASSERT_NOT_NULL(secondFunction);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)secondResult.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(1u, secondFunction->closureValueLength);
    TEST_ASSERT_TRUE(submission_function_has_opcode(
            secondFunction, ZR_INSTRUCTION_ENUM(GET_CLOSURE)));
    TEST_ASSERT_TRUE(submission_function_has_value_call(secondFunction));
    TEST_ASSERT_TRUE(ZrCore_Function_GetClosureCaptureIdentity(
            secondFunction,
            0u,
            &captureType,
            &captureSymbolId,
            &captureTypeId,
            &captureRange));
    TEST_ASSERT_NOT_NULL(captureType);
    TEST_ASSERT_EQUAL_UINT32(firstResult.bindings[0].symbolId, captureSymbolId);
    TEST_ASSERT_EQUAL_UINT32(firstResult.bindings[0].typeId, captureTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            firstResult.callableSignatures[0].symbolId,
            captureSymbolId);
    TEST_ASSERT_EQUAL_UINT32(
            firstResult.callableSignatures[0].typeId,
            captureTypeId);

    ZrParser_SubmissionResult_Free(state, &secondResult);
    ZrParser_SubmissionResult_Free(state, &firstResult);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_closures_preserve_value_bindings_across_cells(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionContext firstContext;
    SZrParserSubmissionContext secondContext;
    SZrParserSubmissionResult firstResult;
    SZrParserSubmissionResult secondResult;
    SZrFunction *firstFunction;
    SZrFunction *secondFunction;
    SZrClosure *firstClosure;
    SZrClosure *secondClosure;
    SZrTypeValue firstValue;
    SZrTypeValue secondValue;
    SZrTypeValue *secondCaptureValue;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_runtime_cells.zr",
            strlen("repl_submission_runtime_cells.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&firstContext, 0, sizeof(firstContext));
    memset(&secondContext, 0, sizeof(secondContext));
    memset(&firstResult, 0, sizeof(firstResult));
    memset(&secondResult, 0, sizeof(secondResult));
    firstContext.moduleGeneration = 1u;
    firstContext.environmentGeneration = 1u;
    firstContext.cellGeneration = 1u;

    firstFunction = ZrParser_Source_CompileSubmission(
            state,
            "var seed = 2;",
            strlen("var seed = 2;"),
            sourceName,
            &firstContext,
            &firstResult);
    TEST_ASSERT_NOT_NULL(firstFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)firstResult.bindingCount);
    firstClosure = submission_closure_new(state, firstFunction);
    TEST_ASSERT_NOT_NULL(firstClosure);
    TEST_ASSERT_TRUE(submission_closure_execute(state, firstClosure, &firstValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, firstValue.type);

    secondContext.bindings = firstResult.bindings;
    secondContext.bindingCount = firstResult.bindingCount;
    secondContext.moduleGeneration = 1u;
    secondContext.environmentGeneration = 1u;
    secondContext.cellGeneration = 2u;
    secondFunction = ZrParser_Source_CompileSubmission(
            state,
            "seed = seed + 3; return seed;",
            strlen("seed = seed + 3; return seed;"),
            sourceName,
            &secondContext,
            &secondResult);
    TEST_ASSERT_NOT_NULL(secondFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, secondFunction->closureValueLength);
    secondClosure = submission_closure_new(state, secondFunction);
    TEST_ASSERT_NOT_NULL(secondClosure);
    TEST_ASSERT_TRUE(submission_closure_copy_capture(
            state, secondClosure, 0u, firstClosure, firstResult.bindings[0].captureIndex));
    TEST_ASSERT_TRUE(submission_closure_execute(state, secondClosure, &secondValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, secondValue.type);
    TEST_ASSERT_EQUAL_INT64(5, secondValue.value.nativeObject.nativeInt64);
    secondCaptureValue = ZrCore_ClosureValue_GetValue(secondClosure->closureValuesExtend[0]);
    TEST_ASSERT_NOT_NULL(secondCaptureValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, secondCaptureValue->type);
    TEST_ASSERT_EQUAL_INT64(5, secondCaptureValue->value.nativeObject.nativeInt64);

    ZrParser_SubmissionResult_Free(state, &secondResult);
    ZrParser_SubmissionResult_Free(state, &firstResult);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_closures_preserve_callable_bindings_across_cells(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionContext firstContext;
    SZrParserSubmissionContext secondContext;
    SZrParserSubmissionResult firstResult;
    SZrParserSubmissionResult secondResult;
    SZrFunction *firstFunction;
    SZrFunction *secondFunction;
    SZrClosure *firstClosure;
    SZrClosure *secondClosure;
    SZrTypeValue firstValue;
    SZrTypeValue secondValue;
    SZrTypeValue *firstCaptureValue;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_runtime_callable.zr",
            strlen("repl_submission_runtime_callable.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&firstContext, 0, sizeof(firstContext));
    memset(&secondContext, 0, sizeof(secondContext));
    memset(&firstResult, 0, sizeof(firstResult));
    memset(&secondResult, 0, sizeof(secondResult));
    firstContext.moduleGeneration = 1u;
    firstContext.environmentGeneration = 1u;
    firstContext.cellGeneration = 1u;

    firstFunction = ZrParser_Source_CompileSubmission(
            state,
            "fn increment(value: int): int { return value + 1; }",
            strlen("fn increment(value: int): int { return value + 1; }"),
            sourceName,
            &firstContext,
            &firstResult);
    TEST_ASSERT_NOT_NULL(firstFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)firstResult.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)firstResult.callableSignatureCount);
    firstClosure = submission_closure_new(state, firstFunction);
    TEST_ASSERT_NOT_NULL(firstClosure);
    TEST_ASSERT_TRUE(submission_closure_execute(state, firstClosure, &firstValue));
    firstCaptureValue = ZrCore_ClosureValue_GetValue(firstClosure->closureValuesExtend[0]);
    TEST_ASSERT_NOT_NULL(firstCaptureValue);
    TEST_ASSERT_NOT_NULL(ZrCore_Closure_GetMetadataFunctionFromValue(state, firstCaptureValue));

    secondContext.bindings = firstResult.bindings;
    secondContext.bindingCount = firstResult.bindingCount;
    secondContext.callableSignatures = firstResult.callableSignatures;
    secondContext.callableSignatureCount = firstResult.callableSignatureCount;
    secondContext.moduleGeneration = 1u;
    secondContext.environmentGeneration = 1u;
    secondContext.cellGeneration = 2u;
    secondFunction = ZrParser_Source_CompileSubmission(
            state,
            "return increment(2);",
            strlen("return increment(2);"),
            sourceName,
            &secondContext,
            &secondResult);
    TEST_ASSERT_NOT_NULL(secondFunction);
    secondClosure = submission_closure_new(state, secondFunction);
    TEST_ASSERT_NOT_NULL(secondClosure);
    TEST_ASSERT_TRUE(submission_closure_copy_capture(
            state, secondClosure, 0u, firstClosure, firstResult.bindings[0].captureIndex));
    TEST_ASSERT_TRUE(submission_closure_execute(state, secondClosure, &secondValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, secondValue.type);
    TEST_ASSERT_EQUAL_INT64(3, secondValue.value.nativeObject.nativeInt64);

    ZrParser_SubmissionResult_Free(state, &secondResult);
    ZrParser_SubmissionResult_Free(state, &firstResult);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_context_rejects_callable_identity_mismatch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionContext firstContext;
    SZrParserSubmissionContext invalidContext;
    SZrParserSubmissionResult firstResult;
    SZrParserSubmissionResult invalidResult;
    SZrParserSubmissionBinding invalidBinding;
    SZrFunction *firstFunction;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_callable_identity_mismatch.zr",
            strlen("repl_submission_callable_identity_mismatch.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&firstContext, 0, sizeof(firstContext));
    memset(&invalidContext, 0, sizeof(invalidContext));
    memset(&firstResult, 0, sizeof(firstResult));
    memset(&invalidResult, 0, sizeof(invalidResult));
    firstContext.moduleGeneration = 1u;
    firstContext.environmentGeneration = 1u;
    firstContext.cellGeneration = 1u;

    firstFunction = ZrParser_Source_CompileSubmission(
            state,
            "fn increment(value: int): int { return value + 1; }",
            strlen("fn increment(value: int): int { return value + 1; }"),
            sourceName,
            &firstContext,
            &firstResult);
    TEST_ASSERT_NOT_NULL(firstFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)firstResult.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)firstResult.callableSignatureCount);

    invalidBinding = firstResult.bindings[0];
    invalidBinding.symbolId++;
    invalidContext.bindings = &invalidBinding;
    invalidContext.bindingCount = 1u;
    invalidContext.callableSignatures = firstResult.callableSignatures;
    invalidContext.callableSignatureCount = firstResult.callableSignatureCount;
    invalidContext.moduleGeneration = 1u;
    invalidContext.environmentGeneration = 1u;
    invalidContext.cellGeneration = 2u;

    TEST_ASSERT_NULL(ZrParser_Source_CompileSubmission(
            state,
            "return increment(2);",
            strlen("return increment(2);"),
            sourceName,
            &invalidContext,
            &invalidResult));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)invalidResult.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)invalidResult.callableSignatureCount);

    ZrParser_SubmissionResult_Free(state, &invalidResult);
    ZrParser_SubmissionResult_Free(state, &firstResult);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_rejects_new_reference_binding_before_publication(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionContext context;
    SZrParserSubmissionResult result;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_new_reference.zr",
            strlen("repl_submission_new_reference.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&context, 0, sizeof(context));
    memset(&result, 0, sizeof(result));
    context.moduleGeneration = 1u;
    context.environmentGeneration = 1u;
    context.cellGeneration = 1u;

    TEST_ASSERT_NULL(ZrParser_Source_CompileSubmission(
            state,
            "var owner: int = 1; var alias: ref int = ref owner;",
            strlen("var owner: int = 1; var alias: ref int = ref owner;"),
            sourceName,
            &context,
            &result));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.bindingCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)result.callableSignatureCount);

    ZrParser_SubmissionResult_Free(state, &result);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_submission_publishes_unique_owner_binding(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrParserSubmissionContext context;
    SZrParserSubmissionResult result;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "repl_submission_unique_owner.zr",
            strlen("repl_submission_unique_owner.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&context, 0, sizeof(context));
    memset(&result, 0, sizeof(result));
    context.moduleGeneration = 1u;
    context.environmentGeneration = 1u;
    context.cellGeneration = 1u;

    function = ZrParser_Source_CompileSubmission(
            state,
            "resource class Tracker {}\n"
            "var owner: Unique<Tracker> = own Tracker();",
            strlen("resource class Tracker {}\n"
                   "var owner: Unique<Tracker> = own Tracker();"),
            sourceName,
            &context,
            &result);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)result.bindingCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
            result.bindings[0].inferredType.ownershipQualifier);
    TEST_ASSERT_EQUAL_UINT32(0u, result.bindings[0].captureIndex);

    ZrParser_SubmissionResult_Free(state, &result);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_submission_context_projects_prior_binding_as_canonical_capture);
    RUN_TEST(test_submission_declaration_publishes_new_closure_binding);
    RUN_TEST(test_submission_context_rejects_stale_and_nonpersistable_bindings);
    RUN_TEST(test_submission_assignment_uses_existing_closure_capture);
    RUN_TEST(test_submission_function_publishes_canonical_callable_signature);
    RUN_TEST(test_submission_context_consumes_canonical_callable_signature);
    RUN_TEST(test_submission_closures_preserve_value_bindings_across_cells);
    RUN_TEST(test_submission_closures_preserve_callable_bindings_across_cells);
    RUN_TEST(test_submission_context_rejects_callable_identity_mismatch);
    RUN_TEST(test_submission_rejects_new_reference_binding_before_publication);
    RUN_TEST(test_submission_publishes_unique_owner_binding);
    return UNITY_END();
}
