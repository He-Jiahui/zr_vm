#include "type_inference_internal.h"

#include "zr_vm_core/array.h"

/*
 * 将非空 tuple 类型 AST 递归转换为带 inline elementTypes 的 object 推断类型。
 * 输入不合格或 tuple 为空时尚未初始化 result；元素形状或递归转换失败时会递归释放部分结果。
 * 成功时保留 tuple 的 ownership/readonly 限定，并在语义上下文存在时登记该类型节点。
 */
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
    result->isReadonlyView = astType->isReadonlyView;
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
        /* Array_Push 只复制 SZrInferredType 的字节；其嵌套数组随值移交给结果槽，不能再释放局部副本。 */
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
