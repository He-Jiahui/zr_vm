#include "backend_aot_c_function_body.h"

#include "backend_aot_c_emitter.h"

void backend_aot_write_c_function_body(FILE *file,
                                       SZrState *state,
                                       const SZrAotFunctionTable *functionTable,
                                       const SZrAotFunctionEntry *entry) {
    TZrUInt32 instructionIndex;
    TZrBool publishExports;
    TZrUInt32 *callableSlotFunctionIndices;

    if (file == ZR_NULL || entry == ZR_NULL || entry->function == ZR_NULL) {
        return;
    }

    publishExports = entry->flatIndex == ZR_AOT_FUNCTION_TREE_ROOT_INDEX &&
                     entry->function->exportedVariableLength > 0;

    fprintf(file, "static TZrInt64 zr_aot_fn_%u(struct SZrState *state) {\n", (unsigned)entry->flatIndex);
    fprintf(file, "    ZrAotGeneratedFrame frame;\n");
    fprintf(file, "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_BeginGeneratedFunction(state, %u, &frame));\n",
            (unsigned)entry->flatIndex);
    backend_aot_write_c_dispatch_loop(file, entry->flatIndex, entry->function->instructionsLength);
    callableSlotFunctionIndices = backend_aot_allocate_callable_slot_function_indices(state, entry->function);

    for (instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &entry->function->instructionsList[instructionIndex];
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;
        TZrUInt32 operandA1 = instruction->instruction.operand.operand1[0];
        TZrUInt32 operandB1 = instruction->instruction.operand.operand1[1];
        TZrInt32 operandA2 = instruction->instruction.operand.operand2[0];

        fprintf(file, "zr_aot_fn_%u_ins_%u:\n", (unsigned)entry->flatIndex, (unsigned)instructionIndex);
        fprintf(file, "    /* opcode=%u extra=%u op1a=%u op1b=%u op2=%d */\n",
                (unsigned)instruction->instruction.operationCode,
                (unsigned)destinationSlot,
                (unsigned)operandA1,
                (unsigned)operandB1,
                (int)operandA2);
        backend_aot_write_c_begin_instruction(file,
                                              instructionIndex,
                                              backend_aot_c_step_flags_for_instruction(instruction->instruction.operationCode));

        switch (instruction->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            {
                TZrUInt32 callableFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;

                if (backend_aot_resolve_callable_constant_function_index(functionTable,
                                                                        state,
                                                                        entry->function,
                                                                        operandA2,
                                                                        &callableFunctionIndex)) {
                    backend_aot_write_c_direct_callable_constant(file,
                                                                 destinationSlot,
                                                                 (TZrUInt32)operandA2,
                                                                 callableFunctionIndex);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 callableFunctionIndex);
                } else if (backend_aot_c_constant_requires_materialization(state, entry->function, operandA2)) {
                    fprintf(file,
                            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CopyConstant(state, &frame, %u, %u));\n",
                            (unsigned)destinationSlot,
                            (unsigned)operandA2);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                } else if (backend_aot_c_constant_can_emit_immediate(entry->function, operandA2)) {
                    backend_aot_write_c_immediate_constant_copy(file,
                                                                destinationSlot,
                                                                backend_aot_c_get_constant_value(entry->function, operandA2));
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                } else {
                    backend_aot_write_c_direct_constant_copy(file, destinationSlot, (TZrUInt32)operandA2);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            {
                TZrUInt32 callableFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
                TZrUInt32 closureCaptureCount = instruction->instruction.operand.operand1[1];

                if (backend_aot_resolve_callable_constant_function_index(functionTable,
                                                                        state,
                                                                        entry->function,
                                                                        (TZrInt32)operandA1,
                                                                        &callableFunctionIndex)) {
                    if (closureCaptureCount == 0) {
                        backend_aot_write_c_direct_callable_constant(file,
                                                                     destinationSlot,
                                                                     operandA1,
                                                                     callableFunctionIndex);
                    } else {
                        fprintf(file,
                                "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateClosure(state, &frame, %u, %u));\n",
                                (unsigned)destinationSlot,
                                (unsigned)operandA1);
                    }
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 callableFunctionIndex);
                } else {
                    fprintf(file,
                            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateClosure(state, &frame, %u, %u));\n",
                            (unsigned)destinationSlot,
                            (unsigned)operandA1);
                    backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                                 entry->function,
                                                                 destinationSlot,
                                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
                backend_aot_write_c_get_closure_value(file, destinationSlot, (TZrUInt32)operandA2);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
                backend_aot_write_c_direct_get_global(file, destinationSlot);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
                backend_aot_write_c_direct_create_object(file, destinationSlot);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
                backend_aot_write_c_direct_create_array(file, destinationSlot);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
                backend_aot_write_c_set_closure_value(file, destinationSlot, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
                backend_aot_write_c_get_closure_value(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
                backend_aot_write_c_set_closure_value(file, destinationSlot, operandA1);
                break;
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK):
                backend_aot_write_c_direct_stack_copy(file, destinationSlot, (TZrUInt32)operandA2);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             backend_aot_get_callable_slot_function_index(
                                                                     callableSlotFunctionIndices,
                                                                     entry->function,
                                                                     (TZrUInt32)operandA2));
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT):
                backend_aot_write_c_direct_add_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT):
                backend_aot_write_c_direct_sub_int(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ADD):
                backend_aot_write_c_direct_add(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                backend_aot_write_c_direct_mul_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                backend_aot_write_c_direct_div_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(NEG):
                backend_aot_write_c_direct_neg(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TYPEOF):
                backend_aot_write_c_direct_typeof(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_STRUCT):
                backend_aot_write_c_direct_to_struct(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TO_OBJECT):
                backend_aot_write_c_direct_to_object(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
                backend_aot_write_c_direct_logical_equal(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
                backend_aot_write_c_direct_logical_less_signed(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
                backend_aot_write_c_direct_meta_call(file, destinationSlot, operandA1, 0);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
                backend_aot_write_c_direct_meta_call(file,
                                                     destinationSlot,
                                                     operandA1,
                                                     backend_aot_get_callsite_cache_argument_count(
                                                             entry->function,
                                                             operandB1,
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL));
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
                backend_aot_write_c_direct_meta_call(file, destinationSlot, operandA1, 0);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
                backend_aot_write_c_direct_meta_call(file,
                                                     destinationSlot,
                                                     operandA1,
                                                     backend_aot_get_callsite_cache_argument_count(
                                                             entry->function,
                                                             operandB1,
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL));
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                break;
            case ZR_INSTRUCTION_ENUM(META_CALL):
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                backend_aot_write_c_direct_meta_call(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(META_TAIL_CALL)) {
                    backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                }
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
                backend_aot_write_c_dynamic_function_call(file, destinationSlot, operandA1, 0);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
                backend_aot_write_c_dynamic_function_call(file,
                                                          destinationSlot,
                                                          operandA1,
                                                          backend_aot_get_callsite_cache_argument_count(
                                                                  entry->function,
                                                                  operandB1,
                                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL));
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
                backend_aot_write_c_dynamic_function_call(file, destinationSlot, operandA1, 0);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
                backend_aot_write_c_dynamic_function_call(file,
                                                          destinationSlot,
                                                          operandA1,
                                                          backend_aot_get_callsite_cache_argument_count(
                                                                  entry->function,
                                                                  operandB1,
                                                                  ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL));
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(META_GET):
                backend_aot_write_c_direct_meta_get(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(META_SET):
                backend_aot_write_c_direct_meta_set(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
                backend_aot_write_c_direct_meta_get_cached(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
                backend_aot_write_c_direct_meta_set_cached(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
                backend_aot_write_c_direct_meta_get_static_cached(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
                backend_aot_write_c_direct_meta_set_static_cached(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                backend_aot_write_c_direct_logical_not_equal(file, destinationSlot, operandA1, operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_USING):
                backend_aot_write_c_direct_own_using(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_SHARE):
                backend_aot_write_c_direct_own_share(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_WEAK):
                backend_aot_write_c_direct_own_weak(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
                backend_aot_write_c_direct_own_upgrade(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
                backend_aot_write_c_direct_own_release(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(TRY):
                backend_aot_write_c_try(file, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(END_TRY):
                backend_aot_write_c_end_try(file, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(THROW):
                backend_aot_write_c_throw(file, entry->flatIndex, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(CATCH):
                backend_aot_write_c_catch(file, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(END_FINALLY):
                backend_aot_write_c_end_finally(file, entry->flatIndex, destinationSlot);
                break;
            case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
                backend_aot_write_c_set_pending_return(file, entry->flatIndex, destinationSlot, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
                backend_aot_write_c_set_pending_break(file, entry->flatIndex, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
                backend_aot_write_c_set_pending_continue(file, entry->flatIndex, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetMember(state, &frame, %u, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1,
                        (unsigned)operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetByIndex(state, &frame, %u, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1,
                        (unsigned)operandB1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetMember(state, &frame, %u, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1,
                        (unsigned)operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetByIndex(state, &frame, %u, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1,
                        (unsigned)operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
            case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterInit(state, &frame, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
            case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterMoveNext(state, &frame, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterCurrent(state, &frame, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                if ((TZrInt64)instructionIndex + (TZrInt64)operandB1 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandB1 + 1) >=
                            entry->function->instructionsLength) {
                    fprintf(file,
                            "    return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, %u, %u, %u);\n",
                            (unsigned)entry->flatIndex,
                            (unsigned)instructionIndex,
                            (unsigned)instruction->instruction.operationCode);
                } else {
                    fprintf(file,
                            "    {\n"
                            "        TZrBool zr_aot_condition = ZR_FALSE;\n"
                            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterMoveNext(state, &frame, %u, %u));\n"
                            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IsTruthy(state, &frame, %u, &zr_aot_condition));\n"
                            "        if (!zr_aot_condition) {\n"
                            "            goto zr_aot_fn_%u_ins_%u;\n"
                            "        }\n"
                            "    }\n",
                            (unsigned)destinationSlot,
                            (unsigned)operandA1,
                            (unsigned)destinationSlot,
                            (unsigned)entry->flatIndex,
                            (unsigned)((TZrInt64)instructionIndex + (TZrInt64)operandB1 + 1));
                }
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(JUMP):
                if ((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1) >=
                            entry->function->instructionsLength) {
                    fprintf(file,
                            "    return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, %u, %u, %u);\n",
                            (unsigned)entry->flatIndex,
                            (unsigned)instructionIndex,
                            (unsigned)instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump(file,
                                                    entry->flatIndex,
                                                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
                if ((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1 < 0 ||
                    (TZrUInt32)((TZrInt64)instructionIndex + (TZrInt64)operandA2 + 1) >=
                            entry->function->instructionsLength) {
                    fprintf(file,
                            "    return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, %u, %u, %u);\n",
                            (unsigned)entry->flatIndex,
                            (unsigned)instructionIndex,
                            (unsigned)instruction->instruction.operationCode);
                } else {
                    backend_aot_write_c_direct_jump_if(file,
                                                       entry->flatIndex,
                                                       destinationSlot,
                                                       (TZrUInt32)((TZrInt64)instructionIndex +
                                                                   (TZrInt64)operandA2 + 1));
                }
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
            {
                TZrUInt32 calleeFunctionIndex = backend_aot_get_callable_slot_function_index(callableSlotFunctionIndices,
                                                                                              entry->function,
                                                                                              operandA1);
                if (calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
                    calleeFunctionIndex =
                            backend_aot_resolve_callable_slot_function_index_before_instruction(functionTable,
                                                                                                state,
                                                                                                entry->function,
                                                                                                instructionIndex,
                                                                                                operandA1,
                                                                                                0);
                }
                if (calleeFunctionIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
                    backend_aot_write_c_static_direct_function_call(file,
                                                                    destinationSlot,
                                                                    operandA1,
                                                                    operandB1,
                                                                    calleeFunctionIndex);
                } else {
                    backend_aot_write_c_direct_function_call(file, destinationSlot, operandA1, operandB1);
                }
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) ||
                    instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)) {
                    backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
            {
                TZrUInt32 calleeFunctionIndex = backend_aot_get_callable_slot_function_index(callableSlotFunctionIndices,
                                                                                              entry->function,
                                                                                              operandA1);
                if (calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
                    calleeFunctionIndex =
                            backend_aot_resolve_callable_slot_function_index_before_instruction(functionTable,
                                                                                                state,
                                                                                                entry->function,
                                                                                                instructionIndex,
                                                                                                operandA1,
                                                                                                0);
                }
                if (calleeFunctionIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
                    backend_aot_write_c_static_direct_function_call(file,
                                                                    destinationSlot,
                                                                    operandA1,
                                                                    0,
                                                                    calleeFunctionIndex);
                } else {
                    backend_aot_write_c_direct_function_call(file, destinationSlot, operandA1, 0);
                }
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS)) {
                    backend_aot_write_c_tail_return(file, destinationSlot, publishExports);
                }
                break;
            }
            case ZR_INSTRUCTION_ENUM(TO_INT):
                backend_aot_write_c_direct_to_int(file, destinationSlot, operandA1);
                backend_aot_set_callable_slot_function_index(callableSlotFunctionIndices,
                                                             entry->function,
                                                             destinationSlot,
                                                             ZR_AOT_INVALID_FUNCTION_INDEX);
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                if (publishExports || entry->function->exceptionHandlerCount > 0) {
                    fprintf(file,
                            "    return ZrLibrary_AotRuntime_Return(state, &frame, %u, %s);\n",
                            (unsigned)operandA1,
                            publishExports ? "ZR_TRUE" : "ZR_FALSE");
                } else {
                    backend_aot_write_c_direct_return(file, operandA1);
                }
                break;
            default:
                fprintf(file,
                        "    return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, %u, %u, %u);\n",
                        (unsigned)entry->flatIndex,
                        (unsigned)instructionIndex,
                        (unsigned)instruction->instruction.operationCode);
                break;
        }
    }

    fprintf(file,
            "    return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, %u, %u, 0);\n",
            (unsigned)entry->flatIndex,
            (unsigned)entry->function->instructionsLength);
    fprintf(file, "}\n");
    backend_aot_release_callable_slot_function_indices(state, entry->function, callableSlotFunctionIndices);
}
