//
// Created by HeJiahui on 2025/6/15.
//

#include "zr_vm_core/execution.h"

#include <math.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/object.h"

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
// extra operand
#define E(INSTRUCTION) INSTRUCTION.instruction.operandExtra
// 4 OPERANDS
#define A0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[0]
#define B0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[1]
#define C0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[2]
#define D0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[3]
// 2 OPERANDS
#define A1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[0]
#define B1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[1]
// 1 OPERAND
#define A2(INSTRUCTION) INSTRUCTION.instruction.operand.operand2[0]

#define BASE(OFFSET) (base + (OFFSET))
#define CONST(OFFSET) (constants + (OFFSET))
#define CLOSURE(OFFSET) (closure->closureValuesExtend[OFFSET])

#define ALGORITHM_1(REGION, OP, TYPE) ZR_VALUE_FAST_SET(destination, REGION, OP(opA->value.nativeObject.REGION), TYPE);
#define ALGORITHM_2(REGION, OP, TYPE)                                                                                  \
    ZR_VALUE_FAST_SET(destination, REGION, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CVT_2(CVT, REGION, OP, TYPE)                                                                         \
    ZR_VALUE_FAST_SET(destination, CVT, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CONST_2(REGION, OP, TYPE, RIGHT)                                                                     \
    ZR_VALUE_FAST_SET(destination, REGION, (opA->value.nativeObject.REGION) OP(RIGHT), TYPE);
#define ALGORITHM_FUNC_2(REGION, OP_FUNC, TYPE)                                                                        \
    ZR_VALUE_FAST_SET(destination, REGION, OP_FUNC(opA->value.nativeObject.REGION, opB->value.nativeObject.REGION),    \
                      TYPE);

#define UPDATE_TRAP(CALL_INFO) (trap = (CALL_INFO)->context.context.trap)
#define UPDATE_BASE(CALL_INFO) (base = (CALL_INFO)->functionBase.valuePointer + 1)
#define UPDATE_STACK(CALL_INFO)                                                                                        \
    {                                                                                                                  \
        if (ZR_UNLIKELY(trap)) {                                                                                       \
            UPDATE_BASE(CALL_INFO);                                                                                    \
        }                                                                                                              \
    }
#define SAVE_PC(STATE, CALL_INFO) ((CALL_INFO)->context.context.programCounter = programCounter)
#define SAVE_STATE(STATE, CALL_INFO)                                                                                   \
    (SAVE_PC(STATE, CALL_INFO), ((STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer))
    // MODIFIABLE: ERROR & STACK & HOOK
#define PROTECT_ESH(STATE, CALL_INFO, EXP) (SAVE_STATE(STATE, CALL_INFO), (EXP), UPDATE_TRAP(CALL_INFO))
    // MODIFIABLE: ERROR & HOOK
#define PROTECT_EH(STATE, CALL_INFO, EXP) (SAVE_PC(STATE), (EXP), UPDATE_TRAP(CALL_INFO))
    // MODIFIABLE: ERROR
#define PROTECT_E(STATE, CALL_INFO, EXP) (SAVE_STATE(STATE), (EXP))

#define JUMP(CALL_INFO, INSTRUCTION, OFFSET)                                                                           \
    {                                                                                                                  \
        programCounter += A2(INSTRUCTION) + (OFFSET);                                                                  \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    }

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
        ZR_INSTRUCTION_FETCH(instruction, programCounter, trap = ZrDebugTraceExecution(state, programCounter);
                             UPDATE_STACK(callInfo), 1);

        // debug line
#if ZR_DEBUG
#endif
        ZR_ASSERT(base == callInfo->functionBase.valuePointer + 1);
        ZR_ASSERT(base <= state->stackTop.valuePointer &&
                  state->stackTop.valuePointer <= state->stackTail.valuePointer);

        SZrTypeValue *destination = E(instruction) == ZR_INSTRUCTION_USE_RET_FLAG ? &ret : BASE(E(instruction));

        ZR_INSTRUCTION_DISPATCH(instruction) {
            ZR_INSTRUCTION_LABEL(GET_STACK) { *destination = BASE(A2(instruction))->value; }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_STACK) { BASE(A2(instruction))->value = *destination; }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CONSTANT) {
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // BASE(B1(instruction))->value = *CONST(ret.value.nativeObject.nativeUInt64);
                *destination = *CONST(A2(instruction));
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CONSTANT) {
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                //*CONST(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                *CONST(A2(instruction)) = *destination;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CLOSURE) {
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // closure function to access
                ZrValueCopy(state, destination, ZrClosureValueGetValue(CLOSURE(A2(instruction))));
                // BASE(B1(instruction))->value = CLOSURE(ret.value.nativeObject.nativeUInt64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CLOSURE) {
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                SZrClosureValue *closureValue = CLOSURE(A2(instruction));
                SZrTypeValue *value = ZrClosureValueGetValue(closureValue);
                SZrTypeValue *newValue = destination;
                // closure function to access
                ZrValueCopy(state, value, newValue);
                // CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                ZrValueBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue), newValue);
                // *CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_BOOL) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_INT) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_UINT) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_FLOAT) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRING) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, +, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, +, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_STRING) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_STRING(opA->type) && ZR_VALUE_IS_TYPE_STRING(opB->type));
                // ALGORITHM_2(nativeDouble, +, opA->type);
                // ZR_VALUE_FAST_SET(&ret, nativeString, ZrStringConcat(state, opA->value.nativeObject.nativeString,
                // opB->value.nativeObject.nativeString), ZR_VALUE_TYPE_STRING);
                // todo: concat string
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, -, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, -, opA->type);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, *, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, *, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, *, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(NEG) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: divide by zero
                if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                    // ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                    ZrDebugRunError(state, "divide by zero");
                }
                ALGORITHM_2(nativeInt64, /, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: divide by zero
                if (ZR_UNLIKELY(opB->value.nativeObject.nativeUInt64 == 0)) {
                    // ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                    ZrDebugRunError(state, "divide by zero");
                }
                ALGORITHM_2(nativeUInt64, /, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_2(nativeDouble, /, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: modulo by zero
                if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                    ZrDebugRunError(state, "modulo by zero");
                }
                TInt64 divisor = opB->value.nativeObject.nativeInt64;
                if (ZR_UNLIKELY(divisor < 0)) {
                    divisor = -divisor;
                }
                ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: modulo by zero
                if (opB->value.nativeObject.nativeUInt64 == 0) {
                    ZrDebugRunError(state, "modulo by zero");
                }
                ALGORITHM_2(nativeUInt64, %, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_FUNC_2(nativeDouble, fmod, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: power domain error
                TInt64 valueA = opA->value.nativeObject.nativeInt64;
                TInt64 valueB = opB->value.nativeObject.nativeInt64;
                if (ZR_UNLIKELY(valueA == 0 && valueB <= 0)) {
                    ZrDebugRunError(state, "power domain error");
                }
                if (ZR_UNLIKELY(valueA < 0)) {
                    ZrDebugRunError(state, "power domain error");
                }
                ALGORITHM_FUNC_2(nativeInt64, ZrMathIntPower, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                SAVE_STATE(state, callInfo); // error: power domain error
                TUInt64 valueA = opA->value.nativeObject.nativeUInt64;
                TUInt64 valueB = opB->value.nativeObject.nativeUInt64;
                if (ZR_UNLIKELY(valueA == 0 && valueB == 0)) {
                    ZrDebugRunError(state, "power domain error");
                }
                ALGORITHM_FUNC_2(nativeUInt64, ZrMathUIntPower, ZR_VALUE_TYPE_UINT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_FUNC_2(nativeDouble, pow, ZR_VALUE_TYPE_DOUBLE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT) {
                // TODO: UNKNOWN TYPE AND META
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_VALUE_TYPE_BOOL, ZR_TRUE);
                } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                    ALGORITHM_1(nativeBool, !, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_VALUE_TYPE_BOOL,
                                      opA->value.nativeObject.nativeInt64 == 0);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_VALUE_TYPE_BOOL,
                                      opA->value.nativeObject.nativeDouble == 0);
                } else {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_VALUE_TYPE_BOOL, ZR_FALSE);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, &&, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, ||, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TBool result = ZrValueEqual(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TBool result = !ZrValueEqual(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeDouble, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_NOT) {
                opA = &BASE(A1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type));
                ALGORITHM_1(nativeInt64, ~, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, &, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, |, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_XOR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, ^, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_CALL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_CLOSURE(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type) &&
                          ZR_VALUE_IS_TYPE_INT(destination->type));
                TZrSize parametersCount = opB->value.nativeObject.nativeUInt64;
                TZrSize returnCount = destination->value.nativeObject.nativeUInt64;
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(A1(instruction)) + parametersCount;
                }
                // save its program counter
                callInfo->context.context.programCounter = programCounter;
                SZrCallInfo *nextCallInfo = ZrFunctionPreCall(state, BASE(A1(instruction)), returnCount);
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
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_CLOSURE(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type) &&
                          ZR_VALUE_IS_TYPE_INT(destination->type));
                TZrSize parametersCount = opB->value.nativeObject.nativeUInt64;
                TZrSize returnCount = ret.value.nativeObject.nativeUInt64;
                // TODO:
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_RETURN) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;

                TZrSize returnCount = opB->value.nativeObject.nativeUInt64;
                TZrSize variableArguments = destination->value.nativeObject.nativeUInt64;

                // save its program counter
                callInfo->context.context.programCounter = programCounter;
                // means the flag of closures to be closed
                // if (A1(instruction)) {
                //     callInfo->yieldContext.returnValueCount = returnCount;
                //     if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
                //         state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
                //     }
                //     // todo close closure values:
                //
                //     trap = callInfo->context.context.trap;
                //     if (ZR_UNLIKELY(trap)) {
                //         base = callInfo->functionBase.valuePointer + 1;
                //     }
                // }
                if (variableArguments > 0) {
                    callInfo->functionBase.valuePointer -=
                            callInfo->context.context.variableArgumentCount + variableArguments;
                }
                state->stackTop.valuePointer = BASE(A1(instruction)) + returnCount;
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
            ZR_INSTRUCTION_LABEL(GETUPVAL) {
                opA = &BASE(A1(instruction))->value;  // upvalue index
                opB = &BASE(B1(instruction))->value;  // destination register
                SZrClosure *currentClosure = ZR_CAST_VM_CLOSURE(state, ZrStackGetValue(callInfo->functionBase.valuePointer));
                if (ZR_UNLIKELY(A1(instruction) >= currentClosure->closureValueCount)) {
                    ZrDebugRunError(state, "upvalue index out of range");
                }
                ZrValueCopy(state, destination, ZrClosureValueGetValue(currentClosure->closureValuesExtend[A1(instruction)]));
            }
            DONE(1);
            
            ZR_INSTRUCTION_LABEL(SETUPVAL) {
                opA = &BASE(A1(instruction))->value;  // upvalue index
                opB = &BASE(B1(instruction))->value;  // source register
                SZrClosure *currentClosure = ZR_CAST_VM_CLOSURE(state, ZrStackGetValue(callInfo->functionBase.valuePointer));
                if (ZR_UNLIKELY(A1(instruction) >= currentClosure->closureValueCount)) {
                    ZrDebugRunError(state, "upvalue index out of range");
                }
                SZrTypeValue *target = ZrClosureValueGetValue(currentClosure->closureValuesExtend[A1(instruction)]);
                ZrValueCopy(state, target, destination);
                ZrValueBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(currentClosure->closureValuesExtend[A1(instruction)]), destination);
            }
            DONE(1);
            
            ZR_INSTRUCTION_LABEL(GETTABLE) {
                opA = &BASE(A1(instruction))->value;  // table object
                opB = &BASE(B1(instruction))->value;  // key
                const SZrTypeValue *result = ZrObjectGetValue(state, ZR_CAST_OBJECT(state, opA->value.object), opB);
                if (result != ZR_NULL) {
                    ZrValueCopy(state, destination, result);
                } else {
                    ZrValueResetAsNull(destination);
                }
            }
            DONE(1);
            
            ZR_INSTRUCTION_LABEL(SETTABLE) {
                opA = &BASE(A1(instruction))->value;  // table object
                opB = &BASE(B1(instruction))->value;  // key
                ZrObjectSetValue(state, ZR_CAST_OBJECT(state, opA->value.object), opB, destination);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP) { JUMP(callInfo, instruction, 0); }
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP_IF) {
                if (destination->value.nativeObject.nativeBool) {
                    JUMP(callInfo, instruction, 0);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_CLOSURE) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
            }
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
