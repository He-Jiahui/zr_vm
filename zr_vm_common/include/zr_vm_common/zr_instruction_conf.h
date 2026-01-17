//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_INSTRUCTION_CONF_H
#define ZR_INSTRUCTION_CONF_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
// #define ZR_INSTRUCTION_USE_DISPATCH_TABLE

#if defined(ZR_COMPILER_GNU) || defined(ZR_COMPILER_CLANG)
#define ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED 1
#elif defined(ZR_COMPILER_MSVC)
#define ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED 0
#endif


#define ZR_INSTRUCTION_DECLARE(Z)                                                                                      \
    Z(GET_STACK)                                                                                                       \
    Z(SET_STACK)                                                                                                       \
    Z(GET_CONSTANT)                                                                                                    \
    Z(SET_CONSTANT)                                                                                                    \
    Z(GET_CLOSURE)                                                                                                     \
    Z(SET_CLOSURE)                                                                                                     \
    Z(GETUPVAL)                                                                                                        \
    Z(SETUPVAL)                                                                                                        \
    Z(GETTABLE)                                                                                                        \
    Z(SETTABLE)                                                                                                        \
    Z(TO_BOOL)                                                                                                         \
    Z(TO_INT)                                                                                                          \
    Z(TO_UINT)                                                                                                         \
    Z(TO_FLOAT)                                                                                                        \
    Z(TO_STRING)                                                                                                       \
    Z(TO_STRUCT)                                                                                                       \
    Z(TO_OBJECT)                                                                                                       \
    Z(ADD)                                                                                                             \
    Z(ADD_INT)                                                                                                         \
    Z(ADD_FLOAT)                                                                                                       \
    Z(ADD_STRING)                                                                                                      \
    Z(SUB)                                                                                                             \
    Z(SUB_INT)                                                                                                         \
    Z(SUB_FLOAT)                                                                                                       \
    Z(MUL)                                                                                                             \
    Z(MUL_SIGNED)                                                                                                      \
    Z(MUL_UNSIGNED)                                                                                                    \
    Z(MUL_FLOAT)                                                                                                       \
    Z(NEG)                                                                                                             \
    Z(DIV)                                                                                                             \
    Z(DIV_SIGNED)                                                                                                      \
    Z(DIV_UNSIGNED)                                                                                                    \
    Z(DIV_FLOAT)                                                                                                       \
    Z(MOD)                                                                                                             \
    Z(MOD_SIGNED)                                                                                                      \
    Z(MOD_UNSIGNED)                                                                                                    \
    Z(MOD_FLOAT)                                                                                                       \
    Z(POW)                                                                                                             \
    Z(POW_SIGNED)                                                                                                      \
    Z(POW_UNSIGNED)                                                                                                    \
    Z(POW_FLOAT)                                                                                                       \
    Z(SHIFT_LEFT)                                                                                                      \
    Z(SHIFT_LEFT_INT)                                                                                                  \
    Z(SHIFT_RIGHT)                                                                                                     \
    Z(SHIFT_RIGHT_INT)                                                                                                 \
    Z(LOGICAL_NOT)                                                                                                     \
    Z(LOGICAL_AND)                                                                                                     \
    Z(LOGICAL_OR)                                                                                                      \
    Z(LOGICAL_GREATER_SIGNED)                                                                                          \
    Z(LOGICAL_GREATER_UNSIGNED)                                                                                        \
    Z(LOGICAL_GREATER_FLOAT)                                                                                           \
    Z(LOGICAL_LESS_SIGNED)                                                                                             \
    Z(LOGICAL_LESS_UNSIGNED)                                                                                           \
    Z(LOGICAL_LESS_FLOAT)                                                                                              \
    Z(LOGICAL_EQUAL)                                                                                                   \
    Z(LOGICAL_NOT_EQUAL)                                                                                               \
    Z(LOGICAL_GREATER_EQUAL_SIGNED)                                                                                    \
    Z(LOGICAL_GREATER_EQUAL_UNSIGNED)                                                                                  \
    Z(LOGICAL_GREATER_EQUAL_FLOAT)                                                                                     \
    Z(LOGICAL_LESS_EQUAL_SIGNED)                                                                                       \
    Z(LOGICAL_LESS_EQUAL_UNSIGNED)                                                                                     \
    Z(LOGICAL_LESS_EQUAL_FLOAT)                                                                                        \
    Z(BITWISE_NOT)                                                                                                     \
    Z(BITWISE_AND)                                                                                                     \
    Z(BITWISE_OR)                                                                                                      \
    Z(BITWISE_XOR)                                                                                                     \
    Z(BITWISE_SHIFT_LEFT)                                                                                              \
    Z(BITWISE_SHIFT_RIGHT)                                                                                             \
    Z(FUNCTION_CALL)                                                                                                   \
    Z(FUNCTION_TAIL_CALL)                                                                                              \
    Z(FUNCTION_RETURN)                                                                                                 \
    Z(GET_GLOBAL)                                                                                                      \
    Z(GET_SUB_FUNCTION)                                                                                                \
    Z(JUMP)                                                                                                            \
    Z(JUMP_IF)                                                                                                         \
    Z(CREATE_CLOSURE)                                                                                                  \
    Z(CREATE_OBJECT)                                                                                                   \
    Z(CREATE_ARRAY)                                                                                                    \
    Z(TRY)                                                                                                             \
    Z(THROW)                                                                                                           \
    Z(CATCH)


#define ZR_INSTRUCTION_OPCODE(INSTRUCTION) (INSTRUCTION.instruction.operationCode)

#define ZR_INSTRUCTION_FETCH(INSTRUCTION, PC, EXCEPTION, N)                                                            \
    {                                                                                                                  \
        if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE)) {                                                               \
            EXCEPTION                                                                                                  \
        }                                                                                                              \
        INSTRUCTION = *(PC += N);                                                                                      \
    }


#define ZR_INSTRUCTION_ENUM(INSTRUCTION) ZR_INSTRUCTION_OP_##INSTRUCTION

#define ZR_INSTRUCTION_ENUM_WRAP(...)                                                                                  \
    ZR_MACRO_REGISTER_WRAP(enum EZrInstructionCode{, ZR_INSTRUCTION_ENUM(ENUM_MAX)}, __VA_ARGS__)

#define ZR_INSTRUCTION_ENUM_DECLARE(INSTRUCTION) ZR_INSTRUCTION_ENUM(INSTRUCTION),


#define ZR_INSTRUCTION_DISPATCH_TABLE_WRAP(...)                                                                        \
    ZR_MACRO_REGISTER_WRAP(static const void *const CZrInstructionDispatchTable[ZR_INSTRUCTION_ENUM(ENUM_MAX)] =       \
                                   {                                                                                   \
                                           ,                                                                           \
                                   },                                                                                  \
                           __VA_ARGS__)

#define ZR_INSTRUCTION_DISPATCH_TABLE_DECLARE(INSTRUCTION) &&LZrInstruction_##INSTRUCTION,

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
#define ZR_INSTRUCTION_DISPATCH_TABLE                                                                                  \
    ZR_INSTRUCTION_DISPATCH_TABLE_WRAP(ZR_INSTRUCTION_DECLARE(ZR_INSTRUCTION_DISPATCH_TABLE_DECLARE));
#define ZR_INSTRUCTION_DISPATCH(INSTRUCTION) goto *CZrInstructionDispatchTable[ZR_INSTRUCTION_OPCODE(INSTRUCTION)];
#define ZR_INSTRUCTION_LABEL(INSTRUCTION) LZrInstruction_##INSTRUCTION:
#define ZR_INSTRUCTION_DONE(INSTRUCTION, PC, N)                                                                        \
    ZR_INSTRUCTION_FETCH(INSTRUCTION, PC, N) ZR_INSTRUCTION_DISPATCH(INSTRUCTION)
#define ZR_INSTRUCTION_DEFAULT()                                                                                       \
    LZrInstructionInvalid_:
#else
#define ZR_INSTRUCTION_DISPATCH_TABLE ((void) 0);
#define ZR_INSTRUCTION_DISPATCH(INSTRUCTION) switch (ZR_INSTRUCTION_OPCODE(INSTRUCTION))
#define ZR_INSTRUCTION_LABEL(INSTRUCTION) case ZR_INSTRUCTION_ENUM(INSTRUCTION):
#define ZR_INSTRUCTION_DONE(INSTRUCTION, PC, N) break;
#define ZR_INSTRUCTION_DEFAULT() default:
#endif

// enum EZrOperationCode {
//     ZR_OPCODE_MOVE,
//     ZR_OPCODE_LOAD_CONSTANT,
//     // 注意：这个枚举已被EZrInstructionCode替代，这里保留作为参考
//     // 实际的指令代码定义在EZrInstructionCode枚举中
//
//     // MATH OPERATION
//     ZR_OPCODE_ADD,
//     ZR_OPCODE_SUB,
//     ZR_OPCODE_MUL,
//     ZR_OPCODE_DIV,
//     ZR_OPCODE_MOD,
//     ZR_OPCODE_SHIFT_LEFT,
//     ZR_OPCODE_SHIFT_RIGHT,
//
//     ZR_OPCODE_LOGICAL_NOT,
//     ZR_OPCODE_LOGICAL_AND,
//     ZR_OPCODE_LOGICAL_OR,
//     ZR_OPCODE_LOGICAL_GREATER,
//     ZR_OPCODE_LOGICAL_LESS,
//     ZR_OPCODE_LOGICAL_EQUAL,
//     ZR_OPCODE_LOGICAL_NOT_EQUAL,
//     ZR_OPCODE_LOGICAL_GREATER_EQUAL,
//     ZR_OPCODE_LOGICAL_LESS_EQUAL,
//
//
//     ZR_OPCODE_BINARY_NOT,
//     ZR_OPCODE_BINARY_AND,
//     ZR_OPCODE_BINARY_OR,
//     ZR_OPCODE_BINARY_XOR,
//
//
//     ZR_OPCODE_ENUM_MAX
// };

// typedef enum EZrOperationCode EZrOperationCode;

ZR_INSTRUCTION_ENUM_WRAP(ZR_INSTRUCTION_DECLARE(ZR_INSTRUCTION_ENUM_DECLARE));

typedef enum EZrInstructionCode EZrInstructionCode;

union TZrInstructionType {
    TUInt8 operand0[4];
    TUInt16 operand1[2];
    TInt32 operand2[1];
};

typedef union TZrInstructionType TZrInstructionType;
#define ZR_INSTRUCTION_USE_RET_FLAG ((TUInt16) (-1))
struct SZrInstruction {
    TUInt16 operationCode;
    TUInt16 operandExtra;
    TZrInstructionType operand;
};

typedef struct SZrInstruction SZrInstruction;

union TZrInstruction {
    SZrInstruction instruction;
    TUInt64 value;
};

typedef union TZrInstruction TZrInstruction;
#endif // ZR_INSTRUCTION_CONF_H
