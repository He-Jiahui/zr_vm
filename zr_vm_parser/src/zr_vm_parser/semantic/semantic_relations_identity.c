#include "semantic_relations_identity.h"

#include "zr_vm_core/string.h"

static TZrBool semantic_relations_optional_strings_equal(
        const SZrString *left,
        const SZrString *right) {
    return (TZrBool)(left == right ||
                      (left != ZR_NULL && right != ZR_NULL &&
                       ZrCore_String_Equal((SZrString *)left,
                                           (SZrString *)right)));
}

TZrBool ZrParser_SemanticRelations_RangesEqual(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->start.offset != right->start.offset ||
        left->end.offset != right->end.offset ||
        left->start.line != right->start.line ||
        left->start.column != right->start.column ||
        left->end.line != right->end.line ||
        left->end.column != right->end.column) {
        return ZR_FALSE;
    }
    return (TZrBool)(left->source == right->source ||
                      (left->source != ZR_NULL && right->source != ZR_NULL &&
                       ZrCore_String_Equal(left->source, right->source)));
}

TZrBool ZrParser_SemanticRelations_FactsEqual(
        const SZrSemanticRelationFact *left,
        const SZrSemanticRelationFact *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->kind != right->kind ||
        left->sourceSymbolId != right->sourceSymbolId ||
        left->targetSymbolId != right->targetSymbolId ||
        left->sourceTypeId != right->sourceTypeId ||
        left->targetTypeId != right->targetTypeId ||
        left->hasSourceRange != right->hasSourceRange ||
        left->hasTargetRange != right->hasTargetRange ||
        left->isExternal != right->isExternal ||
        (left->hasSourceRange &&
         !ZrParser_SemanticRelations_RangesEqual(
                 &left->sourceRange, &right->sourceRange)) ||
        (left->hasTargetRange &&
         !ZrParser_SemanticRelations_RangesEqual(
                 &left->targetRange, &right->targetRange))) {
        return ZR_FALSE;
    }
    return (TZrBool)(semantic_relations_optional_strings_equal(
                              left->externalOriginUri,
                              right->externalOriginUri) &&
                      semantic_relations_optional_strings_equal(
                              left->virtualDeclarationUri,
                              right->virtualDeclarationUri));
}
