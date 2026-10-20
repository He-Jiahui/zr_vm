#ifndef ZR_TEST_AOT_C_SCALAR_CALL_RESULT_PROOF_CASES_H
#define ZR_TEST_AOT_C_SCALAR_CALL_RESULT_PROOF_CASES_H

#include "backend_aot/backend_aot_c_scalar_call_result.h"
#include "backend_aot/backend_aot_c_scalar_locals.h"
#include "backend_aot/backend_aot_c_scalar_stack_copy.h"
#include "backend_aot/backend_aot_callable_provenance.h"
#include "backend_aot/backend_aot_internal.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/gc.h"

typedef struct SZrTestAotScalarCallResultProofFixture {
    SZrState *state;
    SZrFunction *root;
    SZrGcRootLease rootLease;
    SZrGcRootLease sourceNameLease;
    SZrAotExecIrModule module;
    SZrAotFunctionTable functions;
} SZrTestAotScalarCallResultProofFixture;

/* Keep owned resources reachable across Unity's assertion longjmp. */
static SZrTestAotScalarCallResultProofFixture g_aotScalarCallResultProof;

static void aot_scalar_call_result_proof_cleanup(void) {
    SZrTestAotScalarCallResultProofFixture *fixture = &g_aotScalarCallResultProof;

    if (fixture->state != ZR_NULL) {
        backend_aot_release_function_table(fixture->state, &fixture->functions);
        backend_aot_exec_ir_release_module(fixture->state, &fixture->module);
        ZrCore_Gc_RootLeaseRelease(fixture->state->global, &fixture->rootLease);
        if (fixture->root != ZR_NULL) {
            ZrCore_Function_Free(fixture->state, fixture->root);
        }
        ZrCore_Gc_RootLeaseRelease(fixture->state->global, &fixture->sourceNameLease);
        ZrTests_Runtime_State_Destroy(fixture->state);
    }
    memset(fixture, 0, sizeof(*fixture));
}

static void aot_scalar_call_result_proof_dump(
        const SZrFunction *root, TZrUInt32 callIndex, TZrUInt32 copyIndex,
        TZrUInt32 sourceSlot, TZrUInt32 resultSlot, TZrUInt32 calleeIndex,
        TZrBool nonprimitive, TZrBool primitive, TZrBool provenance,
        TZrBool reaching, TZrBool localSource) {
    fprintf(stderr,
            "AOT scalar call result proof call=%u copy=%u source=%u result=%u "
            "callee=%u nonprimitive=%u primitive=%u provenance=%u reaching=%u local=%u\n",
            (unsigned)callIndex, (unsigned)copyIndex, (unsigned)sourceSlot,
            (unsigned)resultSlot, (unsigned)calleeIndex, (unsigned)nonprimitive,
            (unsigned)primitive, (unsigned)provenance, (unsigned)reaching,
            (unsigned)localSource);
    for (TZrUInt32 index = 0u; index < root->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &root->typedLocalBindings[index];

        if (binding->stackSlot == sourceSlot || binding->stackSlot == resultSlot) {
            fprintf(stderr,
                    "AOT scalar call result binding row=%u slot=%u name=%s "
                    "base=%u static=%u owner=%u nullable=%u array=%u\n",
                    (unsigned)index, (unsigned)binding->stackSlot,
                    binding->name != ZR_NULL ? ZrCore_String_GetNativeString(binding->name) : "<none>",
                    (unsigned)binding->type.baseType, (unsigned)binding->type.staticCType,
                    (unsigned)binding->type.ownershipQualifier,
                    (unsigned)binding->type.isNullable, (unsigned)binding->type.isArray);
        }
    }
    for (TZrUInt32 index = 0u; index < root->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &root->semIrInstructions[index];

        if (instruction->execInstructionIndex == callIndex &&
            instruction->destinationSlot == sourceSlot &&
            instruction->typeTableIndex < root->semIrTypeTableLength) {
            const SZrFunctionTypedTypeRef *type = &root->semIrTypeTable[instruction->typeTableIndex];

            fprintf(stderr,
                    "AOT scalar call result SemIR row=%u opcode=%u type=%u "
                    "base=%u static=%u static_id=%u owner=%u nullable=%u array=%u name=%s\n",
                    (unsigned)index, (unsigned)instruction->opcode,
                    (unsigned)instruction->typeTableIndex, (unsigned)type->baseType,
                    (unsigned)type->staticCType, (unsigned)type->staticCTypeId,
                    (unsigned)type->ownershipQualifier, (unsigned)type->isNullable,
                    (unsigned)type->isArray,
                    type->typeName != ZR_NULL ? ZrCore_String_GetNativeString(type->typeName) : "<none>");
        }
    }
}

static void test_aot_c_plain_bool_call_result_preserves_scalar_proof(void) {
    static const char *source =
            "fn both(left: bool, right: bool): bool {\n"
            "    return left && right;\n"
            "}\n"
            "var left: bool = true;\n"
            "var right: bool = true;\n"
            "var result: bool = both(left, right);\n"
            "if (!result) { return 0; }\n"
            "return 1;\n";
    SZrTestAotScalarCallResultProofFixture *fixture = &g_aotScalarCallResultProof;
    SZrString *sourceName;
    const SZrAotExecIrFunction *rootIr;
    const SZrAotExecIrFunction *calleeIr;
    const SZrFunction *callee = ZR_NULL;
    const SZrFunctionTypedTypeRef *projectedReturn;
    TZrUInt32 calleeIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    TZrUInt32 resultSlot = UINT32_MAX;
    TZrUInt32 callIndex = UINT32_MAX;
    TZrUInt32 copyIndex = UINT32_MAX;
    TZrUInt32 resultMatches = 0u;
    TZrUInt32 calleeMatches = 0u;
    TZrUInt32 callMatches = 0u;
    TZrUInt32 copyMatches = 0u;
    TZrUInt32 notMatches = 0u;
    TZrUInt32 sourceSlot;
    TZrBool nonprimitive;
    TZrBool primitive;
    TZrBool provenance;
    TZrBool reaching;
    TZrBool localSource;

    aot_scalar_call_result_proof_cleanup();
    fixture->state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->state,
                                "fixture prerequisite: scalar call result runtime is unavailable");
    sourceName = ZrCore_String_CreateFromNative(fixture->state, "aot_scalar_call_result_proof.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName,
                                "fixture prerequisite: scalar call result source name is unavailable");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
                                     fixture->state, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceName),
                                     &fixture->sourceNameLease),
                             "fixture prerequisite: scalar call result source name could not be rooted");
    fixture->root = ZrParser_Source_Compile(fixture->state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->root,
                                "fixture prerequisite: plain bool call source did not compile");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
                                     fixture->state, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->root),
                                     &fixture->rootLease),
                             "fixture prerequisite: scalar call result root could not be rooted");
    TEST_ASSERT_TRUE_MESSAGE(backend_aot_exec_ir_build_module(
                                     fixture->state, fixture->root, &fixture->module),
                             "fixture prerequisite: scalar call result ExecIR module is unavailable");
    TEST_ASSERT_TRUE_MESSAGE(backend_aot_build_function_table(
                                     fixture->state, fixture->root, &fixture->functions),
                             "fixture prerequisite: scalar call result function table is unavailable");
    rootIr = backend_aot_exec_ir_find_function(&fixture->module, 0u);
    TEST_ASSERT_NOT_NULL_MESSAGE(rootIr, "fixture prerequisite: scalar call result root IR is missing");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(fixture->root, rootIr->function,
                                  "fixture prerequisite: scalar call result root identity differs");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->root->instructionsList,
                                "fixture prerequisite: scalar call result instructions are missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->root->typedLocalBindings,
                                "fixture prerequisite: scalar call result local bindings are missing");
    TEST_ASSERT_TRUE_MESSAGE(fixture->root->semIrInstructionLength == 0u ||
                                     fixture->root->semIrInstructions != ZR_NULL,
                             "fixture prerequisite: scalar call result SemIR rows are missing");
    TEST_ASSERT_TRUE_MESSAGE(fixture->root->semIrTypeTableLength == 0u ||
                                     fixture->root->semIrTypeTable != ZR_NULL,
                             "fixture prerequisite: scalar call result SemIR types are missing");

    for (TZrUInt32 index = 0u; index < fixture->functions.count; index++) {
        const SZrAotFunctionEntry *entry = &fixture->functions.entries[index];

        if (entry->function != ZR_NULL && entry->function != fixture->root &&
            entry->function->functionName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(entry->function->functionName), "both") == 0) {
            callee = entry->function;
            calleeIndex = entry->flatIndex;
            calleeMatches++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, calleeMatches,
                                     "fixture prerequisite: plain bool callee is not unique");
    TEST_ASSERT_TRUE_MESSAGE(callee->hasCallableReturnType,
                             "fixture prerequisite: plain bool callee has no real return metadata");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, callee->parameterCount,
                                     "fixture prerequisite: plain bool callee parameter count differs");
    TEST_ASSERT_TRUE_MESSAGE(callee->callableReturnType.baseType == ZR_VALUE_TYPE_BOOL &&
                                     callee->callableReturnType.ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
                                     !callee->callableReturnType.isNullable && !callee->callableReturnType.isArray &&
                                     callee->callableReturnType.typeName == ZR_NULL,
                             "fixture prerequisite: callee raw return is not plain BOOL");
    calleeIr = backend_aot_exec_ir_find_function(&fixture->module, calleeIndex);
    TEST_ASSERT_NOT_NULL_MESSAGE(calleeIr, "fixture prerequisite: plain bool callee IR is missing");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(callee, calleeIr->function,
                                  "fixture prerequisite: plain bool callee identity differs");
    projectedReturn = backend_aot_exec_ir_callable_return_type(calleeIr);
    TEST_ASSERT_NOT_NULL_MESSAGE(projectedReturn,
                                "fixture prerequisite: plain bool projected return is missing");
    TEST_ASSERT_TRUE_MESSAGE(projectedReturn->baseType == ZR_VALUE_TYPE_BOOL &&
                                     projectedReturn->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
                                     !projectedReturn->isNullable && !projectedReturn->isArray &&
                                     projectedReturn->typeName == ZR_NULL,
                             "fixture prerequisite: callee projected return is not plain BOOL");
    for (TZrUInt32 index = 0u; index < fixture->root->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &fixture->root->typedLocalBindings[index];

        if (binding->name != ZR_NULL && strcmp(ZrCore_String_GetNativeString(binding->name), "result") == 0) {
            resultSlot = binding->stackSlot;
            resultMatches++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, resultMatches,
                                     "fixture prerequisite: saved bool result binding is not unique");
    for (TZrUInt32 index = 0u; index < fixture->root->instructionsLength; index++) {
        const TZrInstruction *instruction = &fixture->root->instructionsList[index];

        if ((instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) ||
             instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)) &&
            instruction->instruction.operand.operand1[1] == 2u &&
            backend_aot_resolve_callable_slot_function_index_before_instruction(
                    &fixture->functions, fixture->state, fixture->root, index,
                    instruction->instruction.operand.operand1[0], 0u) == calleeIndex) {
            callIndex = index;
            callMatches++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, callMatches,
                                     "fixture prerequisite: real plain bool call is not unique");
    sourceSlot = fixture->root->instructionsList[callIndex].instruction.operandExtra;
    for (TZrUInt32 index = callIndex + 1u; index < fixture->root->instructionsLength; index++) {
        const TZrInstruction *instruction = &fixture->root->instructionsList[index];

        if ((instruction->instruction.operationCode == ZR_INSTRUCTION_OP_GET_STACK ||
             instruction->instruction.operationCode == ZR_INSTRUCTION_OP_SET_STACK) &&
            instruction->instruction.operandExtra == resultSlot &&
            instruction->instruction.operand.operand2[0] == (TZrInt32)sourceSlot) {
            copyIndex = index;
            copyMatches++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, copyMatches,
                                     "fixture prerequisite: actual call result copy is not unique");
    for (TZrUInt32 index = copyIndex + 1u; index < fixture->root->instructionsLength; index++) {
        const TZrInstruction *instruction = &fixture->root->instructionsList[index];

        if ((instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(LOGICAL_NOT) ||
             instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL)) &&
            instruction->instruction.operand.operand1[0] == resultSlot) {
            notMatches++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, notMatches,
                                     "fixture prerequisite: saved bool result has no unique not-consumer");

    nonprimitive = backend_aot_c_scalar_call_result_has_nonprimitive_type(
            fixture->root, callee, callIndex, sourceSlot);
    primitive = backend_aot_c_scalar_locals_instruction_writes_primitive(rootIr, callIndex, sourceSlot);
    provenance = backend_aot_c_scalar_stack_copy_has_scalar_provenance_before(rootIr, sourceSlot, copyIndex);
    reaching = backend_aot_c_scalar_locals_bool_written_before(rootIr, sourceSlot, copyIndex);
    localSource = backend_aot_c_scalar_stack_copy_source_can_use_local(
            rootIr, sourceSlot, copyIndex, ZR_STATIC_C_TYPE_BOOL);
    if (nonprimitive || !primitive || !provenance || !reaching || !localSource) {
        aot_scalar_call_result_proof_dump(fixture->root, callIndex, copyIndex, sourceSlot,
                                         resultSlot, calleeIndex, nonprimitive, primitive,
                                         provenance, reaching, localSource);
    }
    TEST_ASSERT_FALSE_MESSAGE(nonprimitive, "plain BOOL call result must not be classified as nonprimitive");
    TEST_ASSERT_TRUE_MESSAGE(primitive, "plain BOOL call must establish a primitive result write");
    TEST_ASSERT_TRUE_MESSAGE(provenance, "saved BOOL call result must retain primitive source provenance");
    TEST_ASSERT_TRUE_MESSAGE(reaching, "saved BOOL call result must have a reaching bool write");
    TEST_ASSERT_TRUE_MESSAGE(localSource, "saved BOOL call result must be available as a scalar local");
    aot_scalar_call_result_proof_cleanup();
}

#endif
