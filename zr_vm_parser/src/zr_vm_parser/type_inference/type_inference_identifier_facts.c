#include "type_inference_semantic_facts.h"

#include <string.h>

/*
 * 把类型环境已经命中的标识符绑定投影为读取或写入事实，不在这里重新解析名字。
 * typeId/symbolId 可以仍为 INVALID：isResolved 只表示类型环境命中了此绑定，不保证 canonical identity 可查询。
 * fact 借用 AST 节点和绑定名；后续查询必须早于对应 AST/字符串与语义快照的释放。
 * TODO: 核对 type_inference.c:1284-1285 的 “Keep its reference fact unresolved”：缺 canonical typeId 与
 * fact.isResolved 是两个维度；通过本函数的有效性与 SOURCE_DECLARATION 身份 gate 后会置 isResolved=TRUE，
 * 而 source binding 缺 declaration range 或 symbolId 时直接不发 fact。下一步审阅 caller 注释、身份 producer
 * 与 query consumer 契约后再消除歧义，不推断每个未注解 local 都会发布 resolved fact。
 */
static void type_inference_record_identifier_fact(SZrCompilerState *cs,
                                                  SZrAstNode *node,
                                                  const SZrTypeBinding *binding,
                                                  EZrSemanticReferenceKind kind) {
    SZrSemanticReferenceFact fact;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_IDENTIFIER_LITERAL ||
        binding == ZR_NULL ||
        binding->name == ZR_NULL) {
        return;
    }

    /* 临时/注入绑定可能没有本地声明范围；保留其来源身份字段，不为其他上下文伪造本地声明。 */
    if (binding->originKind == ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION) {
        if (!binding->hasDeclarationRange || binding->symbolId == ZR_SEMANTIC_ID_INVALID) {
            return;
        }
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    if (binding->hasDeclarationRange) {
        fact.declarationRange = binding->declarationRange;
    }
    fact.kind = kind;
    fact.symbolId = binding->symbolId;
    fact.typeId = binding->typeId;
    fact.placeId = binding->placeId;
    fact.originKind = binding->originKind;
    fact.runtimeRootKind = binding->runtimeRootKind;
    fact.originToken = binding->originToken;
    fact.originIndex = binding->originIndex;
    fact.ownershipQualifier = binding->type.ownershipQualifier;
    fact.name = binding->name;
    fact.isResolved = ZR_TRUE;
    if (kind == ZR_SEMANTIC_REFERENCE_WRITE) {
        /* 写入点先以自身范围作为定义并标为 INIT；线性事实解析据此推导后续 READ 的定义与赋值状态。 */
        fact.definitionRange = node->location;
        fact.hasDefinitionRange = ZR_TRUE;
        fact.definiteAssignmentState = ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT;
        fact.hasDefiniteAssignmentState = ZR_TRUE;
    }
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

void type_inference_record_identifier_reference_fact(SZrCompilerState *cs,
                                                      SZrAstNode *node,
                                                      const SZrTypeBinding *binding) {
    type_inference_record_identifier_fact(cs, node, binding, ZR_SEMANTIC_REFERENCE_READ);
}

void type_inference_record_identifier_write_reference_fact(SZrCompilerState *cs,
                                                           SZrAstNode *node,
                                                           const SZrTypeBinding *binding) {
    type_inference_record_identifier_fact(cs, node, binding, ZR_SEMANTIC_REFERENCE_WRITE);
}
