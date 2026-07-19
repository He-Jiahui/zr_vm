#include "zr_vm_parser/semantic_ir.h"

#include <stdio.h>

const TZrChar *ZrParser_SemanticIr_OpcodeName(EZrSemanticIrOpcode opcode) {
    static const TZrChar *const names[] = {
        "invalid",
        "constant",
        "convert",
        "place.base",
        "place.project",
        "load",
        "store",
        "initialize",
        "move",
        "copy",
        "drop",
        "borrow.shared",
        "borrow.mut",
        "borrow.mut.reserve",
        "reborrow",
        "loan.activate",
        "loan.end",
        "dereference",
        "call.typed",
        "call.virtual",
        "call.dynamic",
        "call.meta",
        "branch",
        "switch",
        "return",
        "throw",
        "scope.enter",
        "scope.exit",
        "cleanup",
        "value.construct",
        "aggregate.construct",
        "field.initialize",
        "union.construct",
        "gc.new",
        "own.construct",
        "property.get",
        "property.set",
        "property.ref_get",
        "destructure.evaluate",
        "shape.validate",
        "destructure.project",
        "destructure.leaf_assign",
        "destructure.leaf_bind",
        "destructure.rest",
    };

    if (opcode < ZR_SEMANTIC_IR_INVALID || opcode >= ZR_SEMANTIC_IR_ENUM_MAX) {
        return "invalid";
    }
    return names[opcode];
}

TZrBool ZrParser_SemanticIr_FormatGolden(
        const SZrSemanticIrFunction *function,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrSize offset = 0;
    TZrSize index;

    if (buffer == ZR_NULL || bufferSize == 0U) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    if (function == ZR_NULL || !function->instructions.isValid) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        int written;

        if (instruction == ZR_NULL || offset >= bufferSize) {
            buffer[0] = '\0';
            return ZR_FALSE;
        }
        written = snprintf(
                buffer + offset,
                bufferSize - offset,
                "%u %s type=%u place=%u value=%u result=%u\n",
                (unsigned int)instruction->id,
                ZrParser_SemanticIr_OpcodeName(instruction->opcode),
                (unsigned int)instruction->typeId,
                (unsigned int)instruction->placeId,
                (unsigned int)instruction->valueId,
                (unsigned int)instruction->resultValueId);
        if (written < 0 || (TZrSize)written >= bufferSize - offset) {
            buffer[0] = '\0';
            return ZR_FALSE;
        }
        offset += (TZrSize)written;
    }
    return ZR_TRUE;
}
