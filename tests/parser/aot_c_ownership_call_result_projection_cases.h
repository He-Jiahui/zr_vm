#ifndef ZR_TEST_AOT_C_OWNERSHIP_CALL_RESULT_PROJECTION_CASES_H
#define ZR_TEST_AOT_C_OWNERSHIP_CALL_RESULT_PROJECTION_CASES_H

#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.h"

static void assert_call_result_copy_is_not_scalar(SZrState *state,
                                                  SZrFunction *function,
                                                  const char *callerName) {
    SZrAotExecIrModule module;
    const SZrAotExecIrFunction *caller = ZR_NULL;
    TZrUInt32 returnedSlot = UINT32_MAX;
    TZrUInt32 checkedCopies = 0u;

    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, function, &module));
    for (TZrUInt32 index = 0u; index < module.functionCount; index++) {
        const SZrFunction *candidate = module.functions[index].function;
        if (candidate->functionName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(candidate->functionName), callerName) == 0) {
            caller = &module.functions[index];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(caller);
    for (TZrUInt32 index = 0u; index < caller->function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &caller->function->typedLocalBindings[index];
        if (binding->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(binding->name), "returned") == 0) {
            returnedSlot = binding->stackSlot;
            break;
        }
    }
    TEST_ASSERT_NOT_EQUAL_UINT32(UINT32_MAX, returnedSlot);
    for (TZrUInt32 index = 0u; index < caller->function->instructionsLength; index++) {
        const TZrInstruction *instruction = &caller->function->instructionsList[index];
        if ((instruction->instruction.operationCode == ZR_INSTRUCTION_OP_GET_STACK ||
             instruction->instruction.operationCode == ZR_INSTRUCTION_OP_SET_STACK) &&
            instruction->instruction.operandExtra == returnedSlot) {
            const TZrUInt32 sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
            TEST_ASSERT_FALSE_MESSAGE(backend_aot_c_scalar_locals_i64_written_before(caller, sourceSlot, index),
                                      "owned call result must not inherit a reused slot's i64 kind");
            TEST_ASSERT_FALSE(backend_aot_c_scalar_locals_u64_written_before(caller, sourceSlot, index));
            TEST_ASSERT_FALSE(backend_aot_c_scalar_locals_f64_written_before(caller, sourceSlot, index));
            TEST_ASSERT_FALSE(backend_aot_c_scalar_locals_bool_value_written_before(caller, sourceSlot, index));
            checkedCopies++;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, checkedCopies);
    backend_aot_exec_ir_release_module(state, &module);
}

static void test_aot_c_never_scalarizes_owned_call_result_copy(void) {
    static const char *source =
            "resource class Leaf {\n"
            " pub var value: int;\n"
            " pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "fn sharedReturn(owner: Shared<Leaf>): Shared<Leaf> { return owner; }\n"
            "fn weakReturn(owner: Shared<Leaf>): Weak<Leaf> { return degrade(owner); }\n"
            "fn sharedCaller(): int {\n"
            " var seed = own Leaf(11); var shared = share(seed);\n"
            " var returned = sharedReturn(shared); drop(shared);\n"
            " var value = returned.value; drop(returned); return value;\n"
            "}\n"
            "fn weakCaller(): int {\n"
            " var seed = own Leaf(22); var shared = share(seed);\n"
            " var returned = weakReturn(shared); drop(shared);\n"
            " var expired = wake(returned);\n"
            " if (expired != null) { return -1; }\n"
            " drop(returned); return 0;\n"
            "}\n"
            "return sharedCaller() + weakCaller();\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "aot_ownership_call_result.zr");
    TEST_ASSERT_NOT_NULL(function);
    assert_call_result_copy_is_not_scalar(state, function, "sharedCaller");
    assert_call_result_copy_is_not_scalar(state, function, "weakCaller");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

#endif
