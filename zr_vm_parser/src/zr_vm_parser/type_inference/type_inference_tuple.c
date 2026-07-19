#include "type_inference_internal.h"

#include "zr_vm_core/array.h"

TZrBool ZrParser_TypeInference_ConvertTupleType(
        SZrCompilerState *cs,
        const SZrType *astType,
        SZrInferredType *result) {
    const SZrTupleType *tupleType;
    TZrSize index;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        astType == ZR_NULL ||
        astType->name == ZR_NULL ||
        astType->name->type != ZR_AST_TUPLE_TYPE ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    tupleType = &astType->name->data.tupleType;
    if (tupleType->elements == ZR_NULL || tupleType->elements->count == 0) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    result->ownershipQualifier = astType->ownershipQualifier;
    ZrCore_Array_Init(
            cs->state,
            &result->elementTypes,
            sizeof(SZrInferredType),
            tupleType->elements->count);

    for (index = 0; index < tupleType->elements->count; index++) {
        const SZrAstNode *elementNode = tupleType->elements->nodes[index];
        SZrInferredType elementType;

        if (elementNode == ZR_NULL || elementNode->type != ZR_AST_TYPE) {
            ZrParser_InferredType_Free(cs->state, result);
            return ZR_FALSE;
        }
        ZrParser_InferredType_Init(cs->state, &elementType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, &elementNode->data.type, &elementType)) {
            ZrParser_InferredType_Free(cs->state, &elementType);
            ZrParser_InferredType_Free(cs->state, result);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, &result->elementTypes, &elementType);
    }

    if (cs->semanticContext != ZR_NULL) {
        ZrParser_Semantic_RegisterInferredType(
                cs->semanticContext,
                result,
                ZR_SEMANTIC_TYPE_KIND_VALUE,
                ZR_NULL,
                astType->name);
    }
    return ZR_TRUE;
}
