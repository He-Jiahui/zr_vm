#ifndef ZR_VM_PARSER_COMPILER_SEMANTIC_SCALAR_RESULT_H
#define ZR_VM_PARSER_COMPILER_SEMANTIC_SCALAR_RESULT_H

#include "compiler_internal.h"

typedef enum EZrCompilerSemanticScalarResult {
    ZR_COMPILER_SEMANTIC_SCALAR_RESULT_NOT_APPLICABLE = 0,
    ZR_COMPILER_SEMANTIC_SCALAR_RESULT_BOUND,
    ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED
} EZrCompilerSemanticScalarResult;

/* Bind an INT64 literal only when the emitted CONSTANT and actual producer pool
 * entry agree on its type, result and index. Other literal types retain their
 * ordinary representation; inconsistent INT64 evidence is rejected. */
EZrCompilerSemanticScalarResult compiler_semantic_scalar_result_bind_literal(
        SZrCompilerState *cs,
        TZrUInt32 resultStackSlot,
        TZrTypeId typeId,
        TZrValueId resultValueId,
        TZrUInt32 constantPoolIndex,
        EZrValueType literalType,
        SZrFileRange sourceRange);

/* Bind only a proven canonical INT64 LOAD result. The caller saves these IDs
 * before binding can grow arrays. Other types and borrowed sources retain the
 * ordinary load path. A temporary gets a new place without INITIALIZE or any
 * literal scratch proof; a self-read local retains its place and metadata.
 * A different local destination is rejected because binding alone is no STORE. */
EZrCompilerSemanticScalarResult compiler_semantic_scalar_result_bind_load(
        SZrCompilerState *cs,
        TZrUInt32 sourceStackSlot,
        TZrUInt32 resultStackSlot,
        TZrTypeId typeId,
        TZrPlaceId sourcePlaceId,
        TZrValueId resultValueId,
        SZrFileRange sourceRange);

/* Transfer an already defined, unborrowed canonical INT64 expression value to
 * a temporary destination. Keep the value's definition and allocate only a new
 * PLACE_BASE binding, without INITIALIZE or literal scratch proof. Other types,
 * loans and values without a definition retain ordinary transfer semantics. */
EZrCompilerSemanticScalarResult compiler_semantic_scalar_result_transfer(
        SZrCompilerState *cs,
        TZrUInt32 sourceStackSlot,
        TZrUInt32 resultStackSlot,
        SZrFileRange sourceRange);

/* Before registration grows arrays, prove that this temporary already holds a
 * defined, unborrowed canonical INT64 value of the local's exact type. Return
 * its stable ID for INITIALIZE; this does not read the temporary's place. */
TZrValueId compiler_semantic_scalar_result_initializer_value(
        const SZrCompilerState *cs,
        const SZrCompilerSemanticIrSlot *source,
        TZrTypeId localTypeId);

#endif
