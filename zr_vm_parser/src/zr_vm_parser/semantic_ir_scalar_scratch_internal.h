#ifndef ZR_VM_PARSER_SEMANTIC_IR_SCALAR_SCRATCH_INTERNAL_H
#define ZR_VM_PARSER_SEMANTIC_IR_SCALAR_SCRATCH_INTERNAL_H

#include "zr_vm_parser/semantic_ir.h"

/* The proof array is optional optimization metadata. A malformed or missing
 * array is treated as having no trusted proofs. */
TZrBool zr_parser_semantic_ir_scalar_scratch_proofs_well_formed(
        const SZrSemanticIrFunction *function);

const SZrSemanticIrScalarScratchProof *
zr_parser_semantic_ir_scalar_scratch_proof_for_place(
        const SZrSemanticIrFunction *function,
        TZrPlaceId placeId);

TZrBool zr_parser_semantic_ir_scalar_scratch_matches_direct_initializer(
        const SZrSemanticIrFunction *function,
        const SZrSemanticIrScalarScratchProof *proof);

TZrBool zr_parser_semantic_ir_scalar_scratch_append_proof(
        SZrSemanticIrFunction *function,
        const SZrSemanticIrScalarScratchProof *proof);

#endif /* ZR_VM_PARSER_SEMANTIC_IR_SCALAR_SCRATCH_INTERNAL_H */
