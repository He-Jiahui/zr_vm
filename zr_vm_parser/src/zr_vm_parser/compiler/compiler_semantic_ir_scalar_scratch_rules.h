#ifndef ZR_VM_PARSER_COMPILER_SEMANTIC_IR_SCALAR_SCRATCH_RULES_H
#define ZR_VM_PARSER_COMPILER_SEMANTIC_IR_SCALAR_SCRATCH_RULES_H

#include "zr_vm_common/zr_type_conf.h"

/* Pure literal classification shared by the compiler proof producer and its
 * focused negative tests. `poolIndexInRange` must come from the live compiler
 * constant array before its entry is read. */
TZrBool compiler_semantic_ir_scalar_scratch_literal_types_match(
        EZrValueType canonicalType,
        EZrValueType literalType,
        TZrBool poolIndexInRange,
        EZrValueType constantPoolType);

#endif /* ZR_VM_PARSER_COMPILER_SEMANTIC_IR_SCALAR_SCRATCH_RULES_H */
