//
// Created by HeJiahui on 2025/6/15.
//

#include "zr_vm_core/execution.h"

#include <math.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/convertion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

void ZrExecute(SZrState *state, SZrCallInfo *callInfo) {
    SZrClosure *closure;
    SZrTypeValue *constants;
    TZrStackValuePointer base;
    SZrTypeValue ret;
    ZrValueResetAsNull(&ret);
    const TZrInstruction *programCounter;
    TZrDebugSignal trap;
    SZrTypeValue *opA;
    SZrTypeValue *opB;
    /*
     * registers macros
     */

    /*
     *
     */
    ZR_INSTRUCTION_DISPATCH_TABLE
#define DONE(N) ZR_INSTRUCTION_DONE(instruction, programCounter, N)
#define A0(INSTRUCTION) INSTRUCTION.instruction.operand.operandA[0]
#define A1(INSTRUCTION) INSTRUCTION.instruction.operand.operandA[1]
#define A2(INSTRUCTION) INSTRUCTION.instruction.operand.operandA[2]
#define A3(INSTRUCTION) INSTRUCTION.instruction.operand.operandA[3
#define B0(INSTRUCTION) INSTRUCTION.instruction.operand.operandB[0]
#define B1(INSTRUCTION) INSTRUCTION.instruction.operand.operandB[1]
#define C0(INSTRUCTION) INSTRUCTION.instruction.operand.operandC[0]
#define BASE(OFFSET) (base + (OFFSET))
#define CONST(OFFSET) (constants + (OFFSET))
#define CLOSURE(OFFSET) (closure->closureValuesExtend + (OFFSET))

#define ALGORITHM_1(REGION, OP, TYPE) ZR_VALUE_FAST_SET(&ret, REGION, OP(opA->value.nativeObject.REGION), TYPE);
#define ALGORITHM_2(REGION, OP, TYPE)                                                                                  \
    ZR_VALUE_FAST_SET(&ret, REGION, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CVT_2(CVT, REGION, OP, TYPE)                                                                         \
    ZR_VALUE_FAST_SET(&ret, CVT, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CONST_2(REGION, OP, TYPE, RIGHT)                                                                     \
    ZR_VALUE_FAST_SET(&ret, REGION, (opA->value.nativeObject.REGION) OP(RIGHT), TYPE);
#define ALGORITHM_FUNC_2(REGION, OP_FUNC, TYPE)                                                                        \
    ZR_VALUE_FAST_SET(&ret, REGION, OP_FUNC(opA->value.nativeObject.REGION, opB->value.nativeObject.REGION), TYPE);
LZrStart:
    trap = state->debugHookSignal;
LZrReturning:
    closure = ZR_CAST_VM_CLOSURE(state, ZrStackGetValue(callInfo->functionBase.valuePointer));
    constants = closure->function->constantValueList;
    programCounter = callInfo->context.context.programCounter;
    if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE)) {
        // todo
    }
    base = callInfo->functionBase.valuePointer + 1;
    for (;;) {
        TZrInstruction instruction;
        /*
         * fetch instruction
         */
        ZR_INSTRUCTION_FETCH(instruction, programCounter, 1);

        // debug line
#if ZR_DEBUG
#endif
        ZR_ASSERT(base == callInfo->functionBase.valuePointer + 1);
        ZR_ASSERT(base <= state->stackTop.valuePointer &&
                  state->stackTop.valuePointer <= state->stackTail.valuePointer);
        ZR_INSTRUCTION_DISPATCH(instruction) {
            ZR_INSTRUCTION_LABEL(GET_STACK) { ret = BASE(B0(instruction))->value; }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_STACK) { BASE(B0(instruction))->value = ret; }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CONSTANT) {
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(ret.type));
                BASE(B0(instruction))->value = *CONST(ret.value.nativeObject.nativeUInt64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CONSTANT) {
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(ret.type));
                *CONST(ret.value.nativeObject.nativeUInt64) = BASE(B0(instruction))->value;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CLOSURE) {
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(ret.type));
                // closure function to access
                ZrValueCopy(state, &(BASE(B0(instruction))->value),
                            ZrClosureValueGetValue(CLOSURE(ret.value.nativeObject.nativeUInt64)));
                // BASE(B0(instruction))->value = CLOSURE(ret.value.nativeObject.nativeUInt64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CLOSURE) {
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(ret.type));
                // closure function to access
                ZrValueCopy(state, ZrClosureValueGetValue(CLOSURE(ret.value.nativeObject.nativeUInt64)),
                            &(BASE(B0(instruction))->value));
                // *CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B0(instruction))->value;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_INT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, +, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, +, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_STRING) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_STRING(opA->type) && ZR_VALUE_IS_TYPE_STRING(opB->type));
                // ALGORITHM_2(nativeDouble, +, opA->type);
                // ZR_VALUE_FAST_SET(&ret, nativeString, ZrStringConcat(state, opA->value.nativeObject.nativeString,
                // opB->value.nativeObject.nativeString), ZR_VALUE_TYPE_STRING); todo: concat string
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_INT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, -, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, -, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_SIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, *, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, *, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, *, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_SIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                if (opB->value.nativeObject.nativeInt64 == 0) {
                    ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                }
                ALGORITHM_2(nativeInt64, /, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_UNSIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                if (opB->value.nativeObject.nativeUInt64 == 0) {
                    ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                }
                ALGORITHM_2(nativeUInt64, /, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, /, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_SIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                if (opB->value.nativeObject.nativeInt64 == 0) {
                    ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                }
                TInt64 divisor = opB->value.nativeObject.nativeInt64;
                if (divisor < 0) {
                    divisor = -divisor;
                }
                ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_UNSIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                if (opB->value.nativeObject.nativeUInt64 == 0) {
                    ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                }
                ALGORITHM_2(nativeUInt64, %, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_FUNC_2(nativeDouble, fmod, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_SIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                if (opA->value.nativeObject.nativeInt64 == 0) {
                    ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                }
                ALGORITHM_FUNC_2(nativeInt64, ZrMathIntPower, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_UNSIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                if (opB->value.nativeObject.nativeUInt64 == 0) {
                    ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                }
                ALGORITHM_FUNC_2(nativeUInt64, ZrMathUIntPower, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_FUNC_2(nativeDouble, fmod, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT) {
                opA = &BASE(A0(instruction))->value;
                if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                    ZR_VALUE_FAST_SET(&ret, nativeBool, ZR_VALUE_TYPE_BOOL, ZR_TRUE);
                } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                    ALGORITHM_1(nativeBool, !, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(&ret, nativeBool, ZR_VALUE_TYPE_BOOL, opA->value.nativeObject.nativeInt64 == 0);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(&ret, nativeBool, ZR_VALUE_TYPE_BOOL, opA->value.nativeObject.nativeDouble == 0);
                } else {
                    ZR_VALUE_FAST_SET(&ret, nativeBool, ZR_VALUE_TYPE_BOOL, ZR_FALSE);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_AND) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, &&, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_OR) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, ||, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_SIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_UNSIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_SIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_UNSIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                TBool result = ZrValueEqual(state, opA, opB);
                ZR_VALUE_FAST_SET(&ret, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                TBool result = !ZrValueEqual(state, opA, opB);
                ZR_VALUE_FAST_SET(&ret, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_SIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_UNSIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_SIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_UNSIGNED) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_FLOAT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_NOT) {
                opA = &BASE(A0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type));
                ALGORITHM_1(nativeInt64, ~, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_AND) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, &, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_OR) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, |, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_XOR) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, ^, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_LEFT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_RIGHT) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_CALL) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_CLOSURE(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type) &&
                          ZR_VALUE_IS_TYPE_INT(ret.type));
                TZrSize parametersCount = opB->value.nativeObject.nativeUInt64;
                TZrSize returnCount = ret.value.nativeObject.nativeUInt64;
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(A0(instruction)) + parametersCount;
                }
                // save its program counter
                callInfo->context.context.programCounter = programCounter;
                SZrCallInfo *nextCallInfo = ZrFunctionPreCall(state, BASE(A0(instruction)), returnCount);
                if (nextCallInfo == ZR_NULL) {
                    // NULL means native call
                    trap = callInfo->context.context.trap;
                } else {
                    // a vm call
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_TAIL_CALL) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_CLOSURE(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type) &&
                          ZR_VALUE_IS_TYPE_INT(ret.type));
                TZrSize parametersCount = opB->value.nativeObject.nativeUInt64;
                TZrSize returnCount = ret.value.nativeObject.nativeUInt64;
                // TODO:
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_RETURN) {
                opA = &BASE(A0(instruction))->value;
                opB = &BASE(B0(instruction))->value;

                TZrSize returnCount = opB->value.nativeObject.nativeUInt64;
                TZrSize variableArguments = ret.value.nativeObject.nativeUInt64;

                // save its program counter
                callInfo->context.context.programCounter = programCounter;
                // A0 means the flag of closures to be closed
                if (A0(instruction)) {
                    callInfo->yieldContext.returnValueCount = returnCount;
                    if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
                        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
                    }
                    // todo close:

                    trap = callInfo->context.context.trap;
                    if (ZR_UNLIKELY(trap)) {
                        base = callInfo->functionBase.valuePointer + 1;
                    }
                }
                if (variableArguments > 0) {
                    callInfo->functionBase.valuePointer -=
                            callInfo->context.context.variableArgumentCount + variableArguments;
                }
                state->stackTop.valuePointer = BASE(A0(instruction)) + returnCount;
                ZrFunctionPostCall(state, callInfo, returnCount);
                trap = callInfo->context.context.trap;
                goto LZrReturn;
            }

        LZrReturn: {
            // return from vm
            if (callInfo->callStatus & ZR_CALL_STATUS_CREATE_FRAME) {
                return;
            } else {
                callInfo = callInfo->previous;
                goto LZrReturning;
            }
        }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_VALUE) {}
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_VALUE) {}
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP) {}
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP_IF) {}
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_CLOSURE) {}
            DONE(1);
            ZR_INSTRUCTION_LABEL(TRY) {}
            DONE(1);
            ZR_INSTRUCTION_LABEL(THROW) {}
            DONE(1);
            ZR_INSTRUCTION_LABEL(CATCH) {}
            DONE(1);
            ZR_INSTRUCTION_DEFAULT() {
                // todo: error unreachable
                ZR_ABORT();
            }
            DONE(1);
        }
    }

#undef DONE
}
