/* Expanded only inside dispatch, so RunError retains its saved PC and
 * catch/finally transfer. Keep ownership stores and float fallback here. */
#define EXECUTION_CHECKED_MUL_RESULT(LEFT, RIGHT) \
    TZrInt64 checkedMultiplyResult__; \
    if (ZR_UNLIKELY(!execution_checked_i64_multiply((LEFT), (RIGHT), &checkedMultiplyResult__))) { \
        ZrCore_Debug_RunError(state, "signed integer multiplication overflow"); \
    }
#define EXECUTION_CHECKED_MUL_STORE(LEFT, RIGHT) \
    do { \
        EXECUTION_CHECKED_MUL_RESULT(LEFT, RIGHT); \
        EXECUTION_STORE_PLAIN_REUSE(nativeInt64, checkedMultiplyResult__, ZR_VALUE_TYPE_INT64); \
    } while (0)
#define EXECUTION_CHECKED_MUL_STORE_DIRECT(DESTINATION, LEFT, RIGHT) \
    do { \
        EXECUTION_CHECKED_MUL_RESULT(LEFT, RIGHT); \
        EXECUTION_STORE_PLAIN_DIRECT_TO(DESTINATION, nativeInt64, checkedMultiplyResult__, ZR_VALUE_TYPE_INT64); \
    } while (0)
#define EXECUTE_MUL_SIGNED_BODY()                                                                                      \
    do {                                                                                                               \
        opA = SIGNED_FRAME_VALUE_SLOT(A1(instruction));                                                                \
        opB = SIGNED_FRAME_VALUE_SLOT(B1(instruction));                                                                \
        if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {                                     \
            EXECUTION_CHECKED_MUL_STORE(opA->value.nativeObject.nativeInt64, opB->value.nativeObject.nativeInt64);                                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_SIGNED");                        \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_BODY_PLAIN_DEST()                                                                           \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = SIGNED_FRAME_VALUE_SLOT(E(instruction));                                    \
        const SZrTypeValue *leftValue__ = SIGNED_FRAME_VALUE_SLOT(A1(instruction));                                    \
        const SZrTypeValue *rightValue__ = SIGNED_FRAME_VALUE_SLOT(B1(instruction));                                   \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type) && ZR_VALUE_IS_TYPE_INT(rightValue__->type)) {                    \
            EXECUTION_CHECKED_MUL_STORE_DIRECT(plainDestination__, leftValue__->value.nativeObject.nativeInt64, rightValue__->value.nativeObject.nativeInt64);                                                     \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, leftValue__, rightValue__, "MUL_SIGNED");      \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_CONST_BODY()                                                                                \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = SIGNED_FRAME_VALUE_SLOT(A1(instruction));                                                                \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            EXECUTION_CHECKED_MUL_STORE(opA->value.nativeObject.nativeInt64, constOpB->value.nativeObject.nativeInt64);         \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MUL_SIGNED_CONST");                                                                               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_CONST_BODY_PLAIN_DEST()                                                                     \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = SIGNED_FRAME_VALUE_SLOT(E(instruction));                                    \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = SIGNED_FRAME_VALUE_SLOT(A1(instruction));                                    \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type)) {                                                                 \
            EXECUTION_CHECKED_MUL_STORE_DIRECT(plainDestination__, leftValue__->value.nativeObject.nativeInt64, constOpB->value.nativeObject.nativeInt64);                                                     \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    leftValue__,                                                                                       \
                    constOpB,                                                                                          \
                    "MUL_SIGNED_CONST");                                                                               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_LOAD_CONST_BODY()                                                                           \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_CONSTANT_SLOT(constantIndex__, instruction.instruction.operand.operand0[1]);               \
        opA = SIGNED_FRAME_VALUE_SLOT(instruction.instruction.operand.operand0[0]);                                     \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            EXECUTION_CHECKED_MUL_STORE(opA->value.nativeObject.nativeInt64, constOpB->value.nativeObject.nativeInt64);         \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MUL_SIGNED_LOAD_CONST");                                                                          \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_LOAD_STACK_CONST_BODY()                                                                     \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_STACK_SLOT(instruction.instruction.operand.operand0[0],                                    \
                                       instruction.instruction.operand.operand0[1]);                                   \
        opA = SIGNED_FRAME_VALUE_SLOT(instruction.instruction.operand.operand0[1]);                                     \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            EXECUTION_CHECKED_MUL_STORE(opA->value.nativeObject.nativeInt64, constOpB->value.nativeObject.nativeInt64);         \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MUL_SIGNED_LOAD_STACK_CONST");                                                                    \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_LOAD_STACK_BODY()                                                                           \
    do {                                                                                                               \
        opA = SIGNED_FRAME_VALUE_SLOT(instruction.instruction.operand.operand0[0]);                                     \
        opB = SIGNED_FRAME_VALUE_SLOT(instruction.instruction.operand.operand0[1]);                                     \
        if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {                                     \
            EXECUTION_CHECKED_MUL_STORE(opA->value.nativeObject.nativeInt64, opB->value.nativeObject.nativeInt64);                                                         \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    opB,                                                                                               \
                    "MUL_SIGNED_LOAD_STACK");                                                                          \
        }                                                                                                              \
    } while (0)
