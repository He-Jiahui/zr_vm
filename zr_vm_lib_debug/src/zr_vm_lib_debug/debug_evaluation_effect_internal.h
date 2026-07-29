#ifndef ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H
#define ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H

#include "debug_internal.h"
#include "zr_vm_parser/semantic_facts.h"

void zr_debug_evaluation_effect_classify_structure(const SZrAstNode *node, TZrUInt32 *effectFlags);
TZrBool zr_debug_evaluation_effect_has_canonical_facts(const SZrSemanticContext *context,
                                                       const SZrAstNode *expression,
                                                       const SZrSemanticExpressionFact *expressionFact);

#endif // ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H
