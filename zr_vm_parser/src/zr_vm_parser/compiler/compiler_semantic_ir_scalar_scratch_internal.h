#ifndef ZR_VM_PARSER_COMPILER_SEMANTIC_IR_SCALAR_SCRATCH_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_SEMANTIC_IR_SCALAR_SCRATCH_INTERNAL_H

#include "compiler_internal.h"

/* Record proof metadata only for a directly pooled bool/i64 literal whose
 * canonical type and actual constant-pool entry agree. */
TZrBool compiler_semantic_ir_record_literal_scalar_scratch_proof(
        SZrCompilerState *compiler,
        TZrPlaceId placeId,
        TZrValueId valueId,
        EZrValueType literalType);

#endif /* ZR_VM_PARSER_COMPILER_SEMANTIC_IR_SCALAR_SCRATCH_INTERNAL_H */
