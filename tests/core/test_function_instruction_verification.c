#include <stdint.h>
#include <stdio.h>
#include "zr_vm_core/function.h"
#include "function/function_bytecode_internal.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "verification failed at line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)
static TZrInstruction op(EZrInstructionCode code, TZrUInt16 e, TZrUInt16 a, TZrUInt16 b) {
    TZrInstruction i = {0};
    i.instruction.operationCode = code;
    i.instruction.operandExtra = e;
    i.instruction.operand.operand1[0] = a;
    i.instruction.operand.operand1[1] = b;
    return i;
}
int main(void) {
    SZrFunction f = {0};
    TZrInstruction code[4] = {0};
    CHECK(!ZrCore_Function_VerifyInstructions(NULL));
    CHECK(!ZrCore_Function_VerifyInstructions(&f));
    f.stackSize = 2;
    f.instructionsList = code;
    f.instructionsLength = 1;
    code[0] = op(ZR_INSTRUCTION_ENUM(NOP), 0, 0, 0);
    CHECK(!ZrCore_Function_VerifyInstructions(&f));
    CHECK(function_verify_guarded_instructions(&f)); /* legacy NOP fallthrough stays guarded */
    code[0] = op(ZR_INSTRUCTION_ENUM(GET_STACK), 0, 2, 0);
    CHECK(!function_verify_guarded_instructions(&f));
    code[0] = op(ZR_INSTRUCTION_ENUM(JUMP), 0, 0, 0);
    CHECK(function_verify_guarded_instructions(&f)); /* legacy jump to end */
    code[0].instruction.operand.operand2[0] = 1;
    CHECK(!function_verify_guarded_instructions(&f));
    code[0] = op(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 0, 0);
    CHECK(ZrCore_Function_VerifyInstructions(&f));
    code[0] = op(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 0, 1);
    CHECK(!ZrCore_Function_VerifyInstructions(&f)); /* yielding return needs a resume instruction */
    code[0] = op(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 2, 1, 0);
    CHECK(!ZrCore_Function_VerifyInstructions(&f));
    code[0] = op(ZR_INSTRUCTION_ENUM(JUMP), 0, 0, 0); /* target == length */
    CHECK(!ZrCore_Function_VerifyInstructions(&f));
    code[0].instruction.operand.operand2[0] = -1; /* self loop */
    CHECK(ZrCore_Function_VerifyInstructions(&f));
    f.instructionsLength = 2;
    code[1] = op(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 0, 0, 0);
    code[0] = op((EZrInstructionCode)UINT16_MAX, 0, 0, 0);
    CHECK(!ZrCore_Function_VerifyInstructions(&f));
    code[0] = op(ZR_INSTRUCTION_ENUM(GET_STACK), 0, 2, 0);
    CHECK(!ZrCore_Function_VerifyInstructions(&f));
    code[0] = op(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0, 0);
    CHECK(!ZrCore_Function_VerifyInstructions(&f));
    code[0] = op(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0, 1, 1);
    CHECK(!ZrCore_Function_VerifyInstructions(&f));
    code[0] = op(ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE), 0, 1, 0);
    CHECK(!ZrCore_Function_VerifyInstructions(&f)); /* skip runs off the body */
    f.instructionsLength = 3;
    code[2] = code[1];
    CHECK(ZrCore_Function_VerifyInstructions(&f));
    code[0].instruction.operand.operand1[1] = 1;
    CHECK(!ZrCore_Function_VerifyInstructions(&f)); /* fused branch to length */
    return failures != 0;
}
