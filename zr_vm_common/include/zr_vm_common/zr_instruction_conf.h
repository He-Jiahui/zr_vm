//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_INSTRUCTION_CONF_H
#define ZR_INSTRUCTION_CONF_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
/* 分发偏好还须与下方编译器/目标支持条件同时成立；WASM 和 MSVC 走同一编号的 switch。
 * 启用此宏本身不验证输入 opcode，也不声明所有指令处理器已经实现。 */
#define ZR_INSTRUCTION_USE_DISPATCH_TABLE

#if defined(__EMSCRIPTEN__) || defined(ZR_WASM_BUILD)
/* Computed-goto dispatch creates irreducible control flow for the WASM backend.
 * Reuse the portable switch dispatcher instead of expanding those regions. */
#define ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED 0
#elif defined(ZR_COMPILER_GNU) || defined(ZR_COMPILER_CLANG)
#define ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED 1
#elif defined(ZR_COMPILER_MSVC)
#define ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED 0
#endif


/* 单一 opcode 列表同时生成 enum、解释器标签及调试名称；新增项须同步 writer/reader 与执行处理。 */
#define ZR_INSTRUCTION_DECLARE(Z)                                                                                      \
    Z(GET_STACK)                                                                                                       \
    Z(SET_STACK)                                                                                                       \
    Z(GET_CONSTANT)                                                                                                    \
    Z(SET_CONSTANT)                                                                                                    \
    Z(GET_CLOSURE)                                                                                                     \
    Z(SET_CLOSURE)                                                                                                     \
    Z(GETUPVAL)                                                                                                        \
    Z(SETUPVAL)                                                                                                        \
    Z(GET_MEMBER)                                                                                                      \
    Z(SET_MEMBER)                                                                                                      \
    Z(GET_BY_INDEX)                                                                                                    \
    Z(SET_BY_INDEX)                                                                                                    \
    Z(SUPER_ARRAY_BIND_ITEMS)                                                                                          \
    Z(SUPER_ARRAY_GET_INT)                                                                                             \
    Z(SUPER_ARRAY_GET_INT_ITEMS)                                                                                       \
    Z(SUPER_ARRAY_GET_INT_PLAIN_DEST)                                                                                  \
    Z(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST)                                                                            \
    Z(SUPER_ARRAY_SET_INT)                                                                                             \
    Z(SUPER_ARRAY_SET_INT_ITEMS)                                                                                       \
    Z(SUPER_ARRAY_ADD_INT)                                                                                             \
    Z(SUPER_ARRAY_ADD_INT4)                                                                                            \
    Z(SUPER_ARRAY_ADD_INT4_CONST)                                                                                      \
    Z(SUPER_ARRAY_FILL_INT4_CONST)                                                                                     \
    Z(ITER_INIT)                                                                                                       \
    Z(ITER_MOVE_NEXT)                                                                                                  \
    Z(ITER_CURRENT)                                                                                                    \
    Z(TO_BOOL)                                                                                                         \
    Z(TO_INT)                                                                                                          \
    Z(TO_UINT)                                                                                                         \
    Z(TO_FLOAT)                                                                                                        \
    Z(TO_FLOAT_SIGNED)                                                                                                 \
    Z(TO_FLOAT_UNSIGNED)                                                                                               \
    Z(TO_INT_FLOAT)                                                                                                    \
    Z(TO_INT_UNSIGNED)                                                                                                 \
    Z(TO_UINT_FLOAT)                                                                                                   \
    Z(TO_UINT_SIGNED)                                                                                                  \
    Z(TO_STRING)                                                                                                       \
    Z(TO_STRUCT)                                                                                                       \
    Z(TO_OBJECT)                                                                                                       \
    Z(ADD)                                                                                                             \
    Z(ADD_INT)                                                                                                         \
    Z(ADD_INT_PLAIN_DEST)                                                                                              \
    Z(ADD_INT_CONST)                                                                                                   \
    Z(ADD_INT_CONST_PLAIN_DEST)                                                                                        \
    Z(ADD_FLOAT)                                                                                                       \
    Z(ADD_STRING)                                                                                                      \
    Z(SUB)                                                                                                             \
    Z(SUB_INT)                                                                                                         \
    Z(SUB_INT_PLAIN_DEST)                                                                                              \
    Z(SUB_INT_CONST)                                                                                                   \
    Z(SUB_INT_CONST_PLAIN_DEST)                                                                                        \
    Z(SUB_FLOAT)                                                                                                       \
    Z(MUL)                                                                                                             \
    Z(MUL_SIGNED)                                                                                                      \
    Z(MUL_SIGNED_PLAIN_DEST)                                                                                           \
    Z(MUL_SIGNED_CONST)                                                                                                \
    Z(MUL_SIGNED_CONST_PLAIN_DEST)                                                                                     \
    Z(MUL_SIGNED_LOAD_CONST)                                                                                           \
    Z(MUL_SIGNED_LOAD_STACK_CONST)                                                                                     \
    Z(MUL_UNSIGNED)                                                                                                    \
    Z(MUL_FLOAT)                                                                                                       \
    Z(NEG)                                                                                                             \
    Z(DIV)                                                                                                             \
    Z(DIV_SIGNED)                                                                                                      \
    Z(DIV_SIGNED_CONST)                                                                                                \
    Z(DIV_SIGNED_CONST_PLAIN_DEST)                                                                                     \
    Z(DIV_SIGNED_LOAD_CONST)                                                                                           \
    Z(DIV_SIGNED_LOAD_STACK_CONST)                                                                                     \
    Z(DIV_UNSIGNED)                                                                                                    \
    Z(DIV_FLOAT)                                                                                                       \
    Z(MOD)                                                                                                             \
    Z(MOD_SIGNED)                                                                                                      \
    Z(MOD_SIGNED_CONST)                                                                                                \
    Z(MOD_SIGNED_CONST_PLAIN_DEST)                                                                                     \
    Z(MOD_SIGNED_LOAD_CONST)                                                                                           \
    Z(MOD_SIGNED_LOAD_STACK_CONST)                                                                                     \
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
    Z(JUMP_IF_BOOL_FALSE)                                                                                              \
    Z(JUMP_IF_GREATER_SIGNED)                                                                                          \
    Z(JUMP_IF_LESS_EQUAL_SIGNED)                                                                                       \
    Z(JUMP_IF_NOT_EQUAL_SIGNED)                                                                                        \
    Z(JUMP_IF_NOT_EQUAL_SIGNED_CONST)                                                                                  \
    Z(JUMP_IF_NULL)                                                                                                    \
    Z(CREATE_CLOSURE)                                                                                                  \
    Z(CREATE_OBJECT)                                                                                                   \
    Z(CREATE_ARRAY)                                                                                                    \
    Z(OWN_UNIQUE)                                                                                                      \
    Z(OWN_BORROW)                                                                                                      \
    Z(OWN_LOAN)                                                                                                        \
    Z(OWN_SHARE)                                                                                                       \
    Z(OWN_DEGRADE)                                                                                                     \
    Z(MARK_TO_BE_CLOSED)                                                                                               \
    Z(CLOSE_SCOPE)                                                                                                     \
    Z(TRY)                                                                                                             \
    Z(END_TRY)                                                                                                         \
    Z(THROW)                                                                                                           \
    Z(CATCH)                                                                                                           \
    Z(END_FINALLY)                                                                                                     \
    Z(SET_PENDING_RETURN)                                                                                              \
    Z(SET_PENDING_BREAK)                                                                                               \
    Z(SET_PENDING_CONTINUE)                                                                                            \
    Z(TYPEOF)                                                                                                          \
    Z(DYN_CALL)                                                                                                        \
    Z(DYN_TAIL_CALL)                                                                                                   \
    Z(META_CALL)                                                                                                       \
    Z(META_TAIL_CALL)                                                                                                  \
    Z(DYN_ITER_INIT)                                                                                                   \
    Z(DYN_ITER_MOVE_NEXT)                                                                                              \
    Z(SUPER_FUNCTION_CALL_NO_ARGS)                                                                                     \
    Z(SUPER_DYN_CALL_NO_ARGS)                                                                                          \
    Z(SUPER_META_CALL_NO_ARGS)                                                                                         \
    Z(SUPER_DYN_CALL_CACHED)                                                                                           \
    Z(SUPER_META_CALL_CACHED)                                                                                          \
    Z(SUPER_FUNCTION_TAIL_CALL_NO_ARGS)                                                                                \
    Z(SUPER_DYN_TAIL_CALL_NO_ARGS)                                                                                     \
    Z(SUPER_META_TAIL_CALL_NO_ARGS)                                                                                    \
    Z(SUPER_DYN_TAIL_CALL_CACHED)                                                                                      \
    Z(SUPER_META_TAIL_CALL_CACHED)                                                                                     \
    Z(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE)                                                                              \
    Z(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)                                                                          \
    Z(SUPER_META_GET_CACHED)                                                                                           \
    Z(SUPER_META_SET_CACHED)                                                                                           \
    Z(OWN_DETACH)                                                                                                      \
    Z(OWN_WAKE)                                                                                                        \
    Z(OWN_DROP)                                                                                                        \
    Z(META_GET)                                                                                                        \
    Z(META_SET)                                                                                                        \
    Z(SUPER_META_GET_STATIC_CACHED)                                                                                    \
    Z(SUPER_META_SET_STATIC_CACHED)                                                                                    \
    Z(NOP)                                                                                                             \
    Z(ADD_SIGNED)                                                                                                      \
    Z(ADD_SIGNED_PLAIN_DEST)                                                                                           \
    Z(ADD_SIGNED_CONST)                                                                                                \
    Z(ADD_SIGNED_CONST_PLAIN_DEST)                                                                                     \
    Z(ADD_SIGNED_LOAD_CONST)                                                                                           \
    Z(ADD_SIGNED_LOAD_STACK_CONST)                                                                                     \
    Z(ADD_SIGNED_LOAD_STACK)                                                                                           \
    Z(ADD_SIGNED_LOAD_STACK_LOAD_CONST)                                                                                \
    Z(ADD_UNSIGNED)                                                                                                    \
    Z(ADD_UNSIGNED_PLAIN_DEST)                                                                                         \
    Z(ADD_UNSIGNED_CONST)                                                                                              \
    Z(ADD_UNSIGNED_CONST_PLAIN_DEST)                                                                                   \
    Z(SUB_SIGNED)                                                                                                      \
    Z(SUB_SIGNED_PLAIN_DEST)                                                                                           \
    Z(SUB_SIGNED_CONST)                                                                                                \
    Z(SUB_SIGNED_CONST_PLAIN_DEST)                                                                                     \
    Z(SUB_SIGNED_LOAD_CONST)                                                                                           \
    Z(SUB_SIGNED_LOAD_STACK_CONST)                                                                                     \
    Z(SUB_UNSIGNED)                                                                                                    \
    Z(SUB_UNSIGNED_PLAIN_DEST)                                                                                         \
    Z(SUB_UNSIGNED_CONST)                                                                                              \
    Z(SUB_UNSIGNED_CONST_PLAIN_DEST)                                                                                   \
    Z(MUL_UNSIGNED_PLAIN_DEST)                                                                                         \
    Z(MUL_UNSIGNED_CONST)                                                                                              \
    Z(MUL_UNSIGNED_CONST_PLAIN_DEST)                                                                                   \
    Z(NEG_SIGNED)                                                                                                      \
    Z(NEG_FLOAT)                                                                                                       \
    Z(DIV_UNSIGNED_CONST)                                                                                              \
    Z(DIV_UNSIGNED_CONST_PLAIN_DEST)                                                                                   \
    Z(MOD_UNSIGNED_CONST)                                                                                              \
    Z(MOD_UNSIGNED_CONST_PLAIN_DEST)                                                                                   \
    Z(LOGICAL_EQUAL_BOOL)                                                                                              \
    Z(LOGICAL_NOT_EQUAL_BOOL)                                                                                          \
    Z(LOGICAL_NOT_BOOL)                                                                                                \
    Z(LOGICAL_EQUAL_SIGNED)                                                                                            \
    Z(LOGICAL_EQUAL_SIGNED_CONST)                                                                                      \
    Z(LOGICAL_NOT_EQUAL_SIGNED)                                                                                        \
    Z(LOGICAL_EQUAL_UNSIGNED)                                                                                          \
    Z(LOGICAL_NOT_EQUAL_UNSIGNED)                                                                                      \
    Z(LOGICAL_EQUAL_FLOAT)                                                                                             \
    Z(LOGICAL_NOT_EQUAL_FLOAT)                                                                                         \
    Z(LOGICAL_EQUAL_STRING)                                                                                            \
    Z(LOGICAL_NOT_EQUAL_STRING)                                                                                        \
    Z(KNOWN_VM_CALL)                                                                                                   \
    Z(KNOWN_VM_MEMBER_CALL)                                                                                            \
    Z(KNOWN_VM_TAIL_CALL)                                                                                              \
    Z(KNOWN_NATIVE_CALL)                                                                                               \
    Z(KNOWN_NATIVE_MEMBER_CALL)                                                                                        \
    Z(KNOWN_NATIVE_TAIL_CALL)                                                                                          \
    Z(SUPER_KNOWN_VM_CALL_NO_ARGS)                                                                                     \
    Z(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS)                                                                                \
    Z(SUPER_KNOWN_NATIVE_CALL_NO_ARGS)                                                                                 \
    Z(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS)                                                                            \
    Z(GET_MEMBER_SLOT)                                                                                                 \
    Z(SET_MEMBER_SLOT)                                                                                                 \
    Z(SET_MEMBER_SLOT_NULL)                                                                                            \
    Z(KNOWN_VM_MEMBER_CALL_LOAD1_U8)                                                                                   \
    Z(KNOWN_NATIVE_MEMBER_CALL_RECV_U8)                                                                                \
    Z(RESET_STACK_NULL)                                                                                                \
    Z(RESET_STACK_NULL2)                                                                                               \
    Z(MUL_SIGNED_LOAD_STACK)                                                                                           \
    Z(ADD_SIGNED_MOD_CONST)                                                                                            \
    Z(OWN_RETURN_LOAN)                                                                                                 \
    Z(CREATE_INLINE_ARRAY)                                                                                             \
    Z(BIND_INLINE_ARRAY_ELEMENT_PLACE)                                                                                 \
    Z(OWN_VIEW_SHARED)                                                                                                 \
    Z(OWN_VIEW_MUT)                                                                                                    \
    Z(OWN_INTO_GC_BOX)                                                                                                 \
    Z(OWN_RETURN_TO_GC)                                                                                                \
    Z(PROPERTY_REF_LOAD)                                                                                               \
    Z(PROPERTY_REF_STORE)                                                                                              \
    Z(PROPERTY_REF_CREATE_MEMBER)                                                                                      \
    Z(PROPERTY_REF_CREATE_INDEX)                                                                                       \
    Z(FUNCTION_CALL_SPREAD)                                                                                            \
    Z(PROPERTY_REF_CREATE_LOCAL)                                                                                       \
    Z(REQUIRE_NON_NULL)                                                                                                \
    Z(MARK_CLOSE_PROXY)


#define ZR_INSTRUCTION_OPCODE(INSTRUCTION) (INSTRUCTION.instruction.operationCode)

/* 取下一条指令前检查调试 trap；PC 的更新与 interpreter 分发使用同一宏约定。 */
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

/* GNU/Clang 原生构建使用 computed goto；MSVC/WASM 使用 switch，两路径共享 opcode 编号与解释语义。
 * TODO: computed goto 和 fastDispatchTable 均按 operationCode 直接索引；IO 直接装入指令字节，
 *       需继续确认 module 装载或执行入口对损坏/不可信 opcode 是否作范围校验。 */
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
#define ZR_INSTRUCTION_DISPATCH_TABLE                                                                                  \
    ZR_INSTRUCTION_DISPATCH_TABLE_WRAP(ZR_INSTRUCTION_DECLARE(ZR_INSTRUCTION_DISPATCH_TABLE_DECLARE));
#define ZR_INSTRUCTION_DISPATCH(INSTRUCTION) goto *CZrInstructionDispatchTable[ZR_INSTRUCTION_OPCODE(INSTRUCTION)];
#define ZR_INSTRUCTION_LABEL(INSTRUCTION) LZrInstruction_##INSTRUCTION:
/* TODO: 此分支展开 ZR_INSTRUCTION_FETCH 时少传 EXCEPTION 实参；目前仅见 EXEC_DONE 定义，
 *       未见其调用，若重新启用必须补齐 trap 处理参数并验证 computed-goto 编译。 */
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

/** @brief 指令四字节操作数在不同宽度下的视图；解释器须按 opcode 解释对应槽位。 */
union TZrInstructionType {
    TZrUInt8 operand0[4];
    TZrUInt16 operand1[2];
    TZrInt32 operand2[1];
};

typedef union TZrInstructionType TZrInstructionType;
/** @brief operandExtra 中选择解释器局部 ret 的哨兵值。
 * @note 结果写入和 META_SET 等接收者读取均可使用该暂存值；字段用途由 opcode 决定。
 */
#define ZR_INSTRUCTION_USE_RET_FLAG ((TZrUInt16) (-1))
/** @brief 固定八字节指令实体，parser 写出后由 core reader 和执行器按相同布局读取。 */
struct SZrInstruction {
    TZrUInt16 operationCode;
    TZrUInt16 operandExtra;
    TZrInstructionType operand;
};

typedef struct SZrInstruction SZrInstruction;

/** @brief 将一条指令与 64 位原始传输单元叠合，供二进制 I/O 保持一致宽度。 */
union TZrInstruction {
    SZrInstruction instruction;
    TZrUInt64 value;
};

typedef union TZrInstruction TZrInstruction;
#endif // ZR_INSTRUCTION_CONF_H
