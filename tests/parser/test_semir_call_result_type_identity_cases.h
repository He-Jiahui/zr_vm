#ifndef ZR_TEST_SEMIR_CALL_RESULT_TYPE_IDENTITY_CASES_H
#define ZR_TEST_SEMIR_CALL_RESULT_TYPE_IDENTITY_CASES_H

#include "backend_aot/backend_aot_c_scalar_call_result.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"

typedef struct SZrSemirCallTypeIdentityFixture {
    SZrState *state;
    SZrFunction *function;
    SZrCompilerState compiler;
    SZrAotExecIrModule module;
    SZrGcRootLease functionLease;
    SZrGcRootLease nameLease;
    TZrBool compilerInitialized;
} SZrSemirCallTypeIdentityFixture;

static SZrSemirCallTypeIdentityFixture g_semirCallTypeIdentity;

static void semir_call_type_identity_cleanup(void) {
    SZrSemirCallTypeIdentityFixture *fixture = &g_semirCallTypeIdentity;

    if (fixture->state != ZR_NULL) {
        backend_aot_exec_ir_release_module(fixture->state, &fixture->module);
        if (fixture->compilerInitialized) {
            ZrParser_CompilerState_Free(&fixture->compiler);
        }
        if (fixture->functionLease.object != ZR_NULL) {
            ZrCore_Gc_RootLeaseRelease(fixture->state->global, &fixture->functionLease);
        }
        if (fixture->function != ZR_NULL) {
            ZrCore_Function_Free(fixture->state, fixture->function);
        }
        if (fixture->nameLease.object != ZR_NULL) {
            ZrCore_Gc_RootLeaseRelease(fixture->state->global, &fixture->nameLease);
        }
        destroy_test_state(fixture->state);
    }
    ZrCore_Memory_RawSet(fixture, 0, sizeof(*fixture));
}

static void semir_call_type_identity_register_object_binding(void) {
    SZrSemirCallTypeIdentityFixture *fixture = &g_semirCallTypeIdentity;
    SZrFunctionLocalVariable local = {0};
    SZrFileRange range = {0};
    SZrInferredType type;
    SZrString *name;
    TZrBool registered;

    ZrParser_CompilerState_Init(&fixture->compiler, fixture->state);
    fixture->compilerInitialized = ZR_TRUE;
    fixture->compiler.currentFunction = fixture->function;
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->compiler.typeEnv,
                                 "Fixture prerequisite: OBJECT type environment allocation failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->compiler.semanticContext,
                                 "Fixture prerequisite: OBJECT semantic context allocation failed");
    name = ZrCore_String_Create(fixture->state, "object_result", sizeof("object_result") - 1u);
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "Fixture prerequisite: OBJECT local name allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
            fixture->state, ZR_CAST_RAW_OBJECT_AS_SUPER(name), &fixture->nameLease),
            "Fixture prerequisite: OBJECT local name rooting failed");
    ZrParser_InferredType_Init(fixture->state, &type, ZR_VALUE_TYPE_OBJECT);
    registered = ZrParser_TypeEnvironment_RegisterVariable(
            fixture->state, fixture->compiler.typeEnv, name, &type);
    ZrParser_InferredType_Free(fixture->state, &type);
    TEST_ASSERT_TRUE_MESSAGE(registered, "Fixture prerequisite: explicit OBJECT binding registration failed");
    TEST_ASSERT_TRUE_MESSAGE(compiler_semantic_ir_register_local(
            &fixture->compiler, name, ZrParser_SyntaxContext_Root(), 6u, range, ZR_FALSE),
            "Fixture prerequisite: explicit OBJECT local identity registration failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->compiler.localVars.head,
                                 "Fixture prerequisite: local variable storage allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(fixture->compiler.localVars.isValid &&
                                    fixture->compiler.localVars.capacity > 0u,
                             "Fixture prerequisite: local variable storage is invalid");
    local.name = name;
    local.stackSlot = 6u;
    ZrCore_Array_Push(fixture->state, &fixture->compiler.localVars, &local);
    TEST_ASSERT_TRUE_MESSAGE(compiler_build_typed_local_bindings(
            &fixture->compiler, &fixture->function->typedLocalBindings,
            &fixture->function->typedLocalBindingLength),
            "Fixture prerequisite: explicit OBJECT typed binding construction failed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, fixture->function->typedLocalBindingLength,
                                     "Fixture prerequisite: explicit OBJECT typed binding is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->function->typedLocalBindings,
                                 "Fixture prerequisite: explicit OBJECT typed binding table is missing");
    {
        const SZrFunctionTypedLocalBinding *binding = &fixture->function->typedLocalBindings[0];

        TEST_ASSERT_EQUAL_UINT32(6u, binding->stackSlot);
        TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, binding->type.baseType);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_NONE, binding->type.ownershipQualifier);
        TEST_ASSERT_NULL(binding->type.typeName);
        TEST_ASSERT_FALSE(binding->type.isArray);
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, binding->typeId);
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, binding->placeId);
    }
}

static const SZrAotExecIrInstruction *semir_call_type_identity_build(TZrBool withObjectBinding) {
    SZrSemirCallTypeIdentityFixture *fixture = &g_semirCallTypeIdentity;
    const SZrSemIrInstruction *source;
    SZrSemIrInstruction sourceSnapshot;
    const SZrAotExecIrFunction *functionIr;
    const SZrAotExecIrInstruction *projected = ZR_NULL;
    TZrUInt32 projectedCount = 0u;

    semir_call_type_identity_cleanup();
    fixture->state = create_test_state();
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->state, "Fixture prerequisite: call type state allocation failed");
    fixture->function = create_single_call_instruction_function(
            fixture->state, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 6u, 2u, 3u);
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->function, "Fixture prerequisite: call type function allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
            fixture->state, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->function), &fixture->functionLease),
            "Fixture prerequisite: call type function rooting failed");
    if (withObjectBinding) {
        semir_call_type_identity_register_object_binding();
    } else {
        TEST_ASSERT_EQUAL_UINT32(0u, fixture->function->typedLocalBindingLength);
        TEST_ASSERT_NULL(fixture->function->typedLocalBindings);
    }
    TEST_ASSERT_TRUE_MESSAGE(compiler_build_function_semir_metadata(fixture->state, fixture->function),
                             "Fixture prerequisite: real call SemIR metadata construction failed");
    source = find_semir_opcode(fixture->function, ZR_SEMIR_OPCODE_DYN_CALL);
    TEST_ASSERT_NOT_NULL_MESSAGE(source, "Fixture prerequisite: real DYN_CALL row is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->function->semIrTypeTable,
                                 "Fixture prerequisite: real call type table is missing");
    TEST_ASSERT_TRUE_MESSAGE(source->typeTableIndex < fixture->function->semIrTypeTableLength,
                             "Fixture prerequisite: real call type index is invalid");
    TEST_ASSERT_EQUAL_UINT32(0u, source->execInstructionIndex);
    TEST_ASSERT_EQUAL_UINT32(6u, source->destinationSlot);
    TEST_ASSERT_EQUAL_UINT32(2u, source->operand0);
    TEST_ASSERT_EQUAL_UINT32(3u, source->operand1);
    sourceSnapshot = *source;
    TEST_ASSERT_TRUE_MESSAGE(backend_aot_exec_ir_build_module(
            fixture->state, fixture->function, &fixture->module),
            "Fixture prerequisite: real call ExecIR projection failed");
    source = find_semir_opcode(fixture->function, ZR_SEMIR_OPCODE_DYN_CALL);
    TEST_ASSERT_NOT_NULL_MESSAGE(source, "Fixture prerequisite: final DYN_CALL row is missing");
    TEST_ASSERT_EQUAL_UINT32(sourceSnapshot.execInstructionIndex, source->execInstructionIndex);
    TEST_ASSERT_EQUAL_UINT32(sourceSnapshot.destinationSlot, source->destinationSlot);
    TEST_ASSERT_EQUAL_UINT32(sourceSnapshot.operand0, source->operand0);
    TEST_ASSERT_EQUAL_UINT32(sourceSnapshot.operand1, source->operand1);
    TEST_ASSERT_EQUAL_UINT32(sourceSnapshot.typeTableIndex, source->typeTableIndex);
    functionIr = backend_aot_exec_ir_find_function(&fixture->module, 0u);
    TEST_ASSERT_NOT_NULL_MESSAGE(functionIr, "Fixture prerequisite: projected call function is missing");
    TEST_ASSERT_TRUE_MESSAGE(functionIr->function == fixture->function,
                             "Fixture prerequisite: projected call function identity changed");
    for (TZrUInt32 index = 0u; index < fixture->module.instructionCount; index++) {
        const SZrAotExecIrInstruction *candidate = &fixture->module.instructions[index];

        if (candidate->functionIndex == functionIr->flatIndex &&
            candidate->execInstructionIndex == source->execInstructionIndex &&
            candidate->semIrOpcode == ZR_SEMIR_OPCODE_DYN_CALL) {
            projected = candidate;
            projectedCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, projectedCount,
                                     "Fixture prerequisite: projected call is not unique");
    TEST_ASSERT_NOT_NULL(projected);
    TEST_ASSERT_EQUAL_UINT32(source->destinationSlot, projected->destinationSlot);
    TEST_ASSERT_EQUAL_UINT32(source->operand0, projected->operand0);
    TEST_ASSERT_EQUAL_UINT32(source->operand1, projected->operand1);
    TEST_ASSERT_EQUAL_UINT32(source->typeTableIndex, projected->typeTableIndex);
    return projected;
}

static void semir_call_type_identity_assert_plain_type(const SZrFunctionTypedTypeRef *type,
                                                      TZrUInt32 expectedStaticCType) {
    TEST_ASSERT_EQUAL_UINT32(expectedStaticCType, type->staticCType);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, type->staticCTypeId);
    TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_NONE, type->ownershipQualifier);
    TEST_ASSERT_FALSE(type->isNullable);
    TEST_ASSERT_FALSE(type->isArray);
    TEST_ASSERT_NULL(type->typeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_OBJECT, type->elementBaseType);
    TEST_ASSERT_NULL(type->elementTypeName);
}

static void test_semir_call_without_result_binding_preserves_unknown_type(void) {
    const SZrAotExecIrInstruction *call = semir_call_type_identity_build(ZR_FALSE);
    const SZrFunction *function = g_semirCallTypeIdentity.function;
    const SZrFunctionTypedTypeRef *type = &function->semIrTypeTable[call->typeTableIndex];
    TZrBool nonprimitive = backend_aot_c_scalar_call_result_has_nonprimitive_type(
            function, ZR_NULL, call->execInstructionIndex, call->destinationSlot);

    semir_call_type_identity_assert_plain_type(type, ZR_STATIC_C_TYPE_DYNAMIC);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ZR_VALUE_TYPE_UNKNOWN, type->baseType,
            "Missing call result bindings must remain UNKNOWN rather than explicit OBJECT evidence");
    TEST_ASSERT_FALSE_MESSAGE(nonprimitive,
            "Missing UNKNOWN call result metadata must not supply nonprimitive evidence");
}

static void test_semir_explicit_object_call_result_remains_nonprimitive(void) {
    const SZrAotExecIrInstruction *call = semir_call_type_identity_build(ZR_TRUE);
    const SZrFunction *function = g_semirCallTypeIdentity.function;
    const SZrFunctionTypedTypeRef *type = &function->semIrTypeTable[call->typeTableIndex];
    const SZrFunctionTypedTypeRef *unknown = ZR_NULL;
    TZrBool nonprimitive = backend_aot_c_scalar_call_result_has_nonprimitive_type(
            function, ZR_NULL, call->execInstructionIndex, call->destinationSlot);

    for (TZrUInt32 index = 0u; index < function->semIrTypeTableLength; index++) {
        if (function->semIrTypeTable[index].baseType == ZR_VALUE_TYPE_UNKNOWN) {
            unknown = &function->semIrTypeTable[index];
            break;
        }
    }
    semir_call_type_identity_assert_plain_type(type, ZR_STATIC_C_TYPE_GC_REF);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ZR_VALUE_TYPE_OBJECT, type->baseType,
            "Explicit OBJECT call result bindings must retain their real semantic type");
    TEST_ASSERT_TRUE_MESSAGE(nonprimitive,
            "Explicit OBJECT call result metadata must remain nonprimitive evidence");
    TEST_ASSERT_NOT_NULL_MESSAGE(unknown,
            "Explicit OBJECT call result types must remain separate from the missing UNKNOWN entry");
    TEST_ASSERT_TRUE_MESSAGE(unknown != type,
            "Explicit OBJECT call result types must not reuse missing UNKNOWN identity");
    semir_call_type_identity_assert_plain_type(unknown, ZR_STATIC_C_TYPE_DYNAMIC);
}

#endif
