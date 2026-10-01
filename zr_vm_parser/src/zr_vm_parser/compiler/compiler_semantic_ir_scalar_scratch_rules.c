#include "compiler_semantic_ir_scalar_scratch_rules.h"

static TZrBool compiler_semantic_ir_scalar_scratch_supported_type(
        EZrValueType valueType) {
    return (TZrBool)(valueType == ZR_VALUE_TYPE_BOOL ||
                     valueType == ZR_VALUE_TYPE_INT64);
}

TZrBool compiler_semantic_ir_scalar_scratch_literal_types_match(
        EZrValueType canonicalType,
        EZrValueType literalType,
        TZrBool poolIndexInRange,
        EZrValueType constantPoolType) {
    return (TZrBool)(compiler_semantic_ir_scalar_scratch_supported_type(
                             literalType) &&
                     poolIndexInRange == ZR_TRUE &&
                     canonicalType == literalType &&
                     constantPoolType == literalType);
}
