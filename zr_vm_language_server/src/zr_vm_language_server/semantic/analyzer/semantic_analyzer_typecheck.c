//
// Created by Auto on 2025/01/XX.
//

#include "semantic/analyzer/semantic_analyzer_internal.h"
#include "semantic/analyzer/semantic_analyzer_union_patterns.h"
#include "zr_vm_parser/const_assignment.h"
#include "zr_vm_parser/semantic_source_metadata.h"
#include "type_inference/type_inference_semantic_facts.h"
#include "zr_vm_parser/variance.h"

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);
TZrBool bind_foreach_element_type_from_inferred_iterable(SZrCompilerState *cs,
                                                         const SZrInferredType *iterableType,
                                                         SZrInferredType *outType);
static TZrBool semantic_type_from_ast(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer,
                                      const SZrType *typeNode,
                                      SZrInferredType *result);
static TZrBool semantic_infer_node_type(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrAstNode *node,
                                        SZrInferredType *result,
                                        TZrBool *outPublishedDiagnostic);

static void semantic_typecheck_using_statement(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrAstNode *node);
static void semantic_typecheck_switch_expression(SZrState *state,
                                                 SZrSemanticAnalyzer *analyzer,
                                                 SZrAstNode *node);

static TZrBool semantic_text_equals(const TZrChar *value, const TZrChar *expected) {
    return value != ZR_NULL && expected != ZR_NULL && strcmp(value, expected) == 0;
}

static void semantic_record_reachability_fact_at_range(SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node,
                                                       SZrFileRange range,
                                                       EZrSemanticReachabilityCause cause,
                                                       SZrAstNode *causeNode) {
    SZrSemanticReachabilityFact fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = range;
    fact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
    fact.cause = cause;
    fact.causeNode = causeNode;
    ZrParser_SemanticFacts_AppendReachability(analyzer->semanticContext, &fact);
}

static SZrFileRange semantic_reachability_fact_range_for_node(const SZrAstNode *node) {
    SZrFileRange range;

    if (node == ZR_NULL) {
        return ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                         ZrParser_FilePosition_Create(0, 0, 0),
                                         ZR_NULL);
    }

    range = node->location;
    if (node->type == ZR_AST_VARIABLE_DECLARATION) {
        const SZrVariableDeclaration *declaration = &node->data.variableDeclaration;

        if (declaration->pattern != ZR_NULL) {
            range = ZrParser_FileRange_Merge(range, declaration->pattern->location);
        }
        if (declaration->typeInfo != ZR_NULL && declaration->typeInfo->name != ZR_NULL) {
            range = ZrParser_FileRange_Merge(range, declaration->typeInfo->name->location);
        }
        if (declaration->value != ZR_NULL) {
            range = ZrParser_FileRange_Merge(range, declaration->value->location);
        }
    }

    return range;
}

static void semantic_record_reachability_fact(SZrSemanticAnalyzer *analyzer,
                                              SZrAstNode *node,
                                              EZrSemanticReachabilityCause cause,
                                              SZrAstNode *causeNode) {
    if (node == ZR_NULL) {
        return;
    }

    semantic_record_reachability_fact_at_range(analyzer,
                                               node,
                                               semantic_reachability_fact_range_for_node(node),
                                               cause,
                                               causeNode);
}

static void semantic_record_logical_fact(SZrSemanticAnalyzer *analyzer,
                                         SZrAstNode *node,
                                         EZrSemanticLogicalFactKind kind,
                                         TZrBool hasKnownValue,
                                         TZrBool knownValue,
                                         SZrAstNode *relatedNode) {
    SZrSemanticLogicalFact fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = kind;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.hasKnownValue = hasKnownValue;
    fact.knownValue = knownValue;
    fact.relatedNode = relatedNode;
    ZrParser_SemanticFacts_AppendLogical(analyzer->semanticContext, &fact);
}

static TZrBool semantic_has_constant_branch_reachability_fact(SZrSemanticAnalyzer *analyzer,
                                                              SZrAstNode *node) {
    TZrSize index;

    if (analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        !analyzer->semanticContext->reachabilityFacts.isValid) {
        return ZR_FALSE;
    }

    for (index = 0; index < analyzer->semanticContext->reachabilityFacts.length; index++) {
        const SZrSemanticReachabilityFact *fact =
                (const SZrSemanticReachabilityFact *)ZrCore_Array_Get(
                        &analyzer->semanticContext->reachabilityFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->node == node &&
            fact->cause == ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void semantic_record_constant_if_condition_facts(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrAstNode *node) {
    TZrBool conditionValue = ZR_FALSE;
    SZrAstNode *conditionEvidence = ZR_NULL;
    SZrAstNode *condition;
    SZrAstNode *unreachableBranch;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_IF_EXPRESSION) {
        return;
    }

    condition = node->data.ifExpression.condition;
    if (!ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
                analyzer,
                condition,
                &conditionValue,
                &conditionEvidence)) {
        return;
    }

    unreachableBranch = conditionValue
                        ? node->data.ifExpression.elseExpr
                        : node->data.ifExpression.thenExpr;
    if (semantic_has_constant_branch_reachability_fact(analyzer, unreachableBranch)) {
        return;
    }

    semantic_record_logical_fact(analyzer,
                                 condition,
                                 conditionValue
                                ? ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE
                                : ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE,
                                 ZR_TRUE,
                                 conditionValue,
                                 conditionEvidence != ZR_NULL ? conditionEvidence : unreachableBranch);
    if (unreachableBranch != ZR_NULL) {
        semantic_record_reachability_fact(analyzer,
                                          unreachableBranch,
                                          ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH,
                                          condition);
    }
}

/** 类型检查跨入可调用体时保存父级编译器上下文，避免接收者和泛型环境串到兄弟声明。 */
typedef struct SZrSemanticTypecheckContextSnapshot {
    SZrTypePrototypeInfo *typePrototype;
    SZrAstNode *typeNode;
    SZrString *typeName;
    SZrAstNode *functionNode;
} SZrSemanticTypecheckContextSnapshot;

static SZrTypeEnvironment *semantic_typecheck_push_runtime_type_binding_scope(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    SZrTypeEnvironment *savedEnv;
    SZrTypeEnvironment *newEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return ZR_NULL;
    }

    savedEnv = analyzer->compilerState->typeEnv;
    newEnv = ZrParser_TypeEnvironment_New(state);
    if (newEnv == ZR_NULL) {
        return savedEnv;
    }

    newEnv->parent = savedEnv;
    newEnv->semanticContext =
        savedEnv != ZR_NULL ? savedEnv->semanticContext : analyzer->compilerState->semanticContext;
    analyzer->compilerState->typeEnv = newEnv;
    return savedEnv;
}

static void semantic_typecheck_pop_runtime_type_binding_scope(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrTypeEnvironment *savedEnv) {
    SZrTypeEnvironment *currentEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return;
    }

    currentEnv = analyzer->compilerState->typeEnv;
    if (currentEnv == savedEnv) {
        return;
    }

    analyzer->compilerState->typeEnv = savedEnv;
    if (currentEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, currentEnv);
    }
}

/* A syntactic may-write proof, not a purity assumption about unknown syntax.
 * Name/context matching deliberately over-approximates writes through shadows;
 * calls, references and other unmodelled effects may change any mutable value.
 * A NULL binding asks only whether such an unmodelled effect is present. */
static TZrBool semantic_typecheck_node_may_write_range(
        const SZrAstNode *node, const SZrTypeBinding *binding, const SZrTypeEnvironment *typeEnv);

/* Only nominal builtin types prove that arithmetic/indexing cannot call user
 * meta methods. Do not re-infer expressions or consume environment-bound facts. */
static const SZrInferredType *semantic_typecheck_syntax_binding_type(
        const SZrAstNode *node, const SZrTypeEnvironment *typeEnv) {
    if (node == ZR_NULL) return ZR_NULL;
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        const SZrTypeBinding *binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
                typeEnv, node->data.identifier.name, node->data.identifier.syntaxContext);
        return binding != ZR_NULL ? &binding->type : ZR_NULL;
    }
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) return ZR_NULL;
    const SZrInferredType *type = semantic_typecheck_syntax_binding_type(node->data.primaryExpression.property, typeEnv);
    const SZrAstNodeArray *members = node->data.primaryExpression.members;
    for (TZrSize index = 0; members != ZR_NULL && index < members->count; ++index) {
        const SZrAstNode *member = members->nodes[index];
        if (type == ZR_NULL || type->baseType != ZR_VALUE_TYPE_ARRAY || type->elementTypes.length != 1 ||
            member == ZR_NULL || member->type != ZR_AST_MEMBER_EXPRESSION || !member->data.memberExpression.computed) {
            return ZR_NULL;
        }
        type = (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0);
    }
    return type;
}

static TZrBool semantic_typecheck_syntax_is_primitive(
        const SZrAstNode *node, const SZrTypeEnvironment *typeEnv) {
    if (node == ZR_NULL) return ZR_FALSE;
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
            return ZR_TRUE;
        case ZR_AST_BINARY_EXPRESSION:
            return semantic_typecheck_syntax_is_primitive(node->data.binaryExpression.left, typeEnv) &&
                   semantic_typecheck_syntax_is_primitive(node->data.binaryExpression.right, typeEnv);
        case ZR_AST_LOGICAL_EXPRESSION:
            return semantic_typecheck_syntax_is_primitive(node->data.logicalExpression.left, typeEnv) &&
                   semantic_typecheck_syntax_is_primitive(node->data.logicalExpression.right, typeEnv);
        case ZR_AST_UNARY_EXPRESSION:
            return semantic_typecheck_syntax_is_primitive(node->data.unaryExpression.argument, typeEnv);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return semantic_typecheck_syntax_is_primitive(node->data.conditionalExpression.test, typeEnv) &&
                   semantic_typecheck_syntax_is_primitive(node->data.conditionalExpression.consequent, typeEnv) &&
                   semantic_typecheck_syntax_is_primitive(node->data.conditionalExpression.alternate, typeEnv);
        case ZR_AST_PRIMARY_EXPRESSION:
            /* Parentheses do not change nominal builtin-ness. An actual member
             * still needs the restrictive builtin-array proof below. */
            if (node->data.primaryExpression.members == ZR_NULL || node->data.primaryExpression.members->count == 0) {
                return semantic_typecheck_syntax_is_primitive(node->data.primaryExpression.property, typeEnv);
            }
            /* fall through */
        default: {
            const SZrInferredType *type = semantic_typecheck_syntax_binding_type(node, typeEnv);
            return type != ZR_NULL && (ZR_VALUE_IS_TYPE_NUMBER(type->baseType) ||
                   type->baseType == ZR_VALUE_TYPE_BOOL || type->baseType == ZR_VALUE_TYPE_STRING);
        }
    }
}

/* A scalar-looking identifier can still write shared caller storage. Numeric
 * refinements intentionally erase reference access, so consult the canonical
 * source parameter as well as an explicit writable-ref type. Missing identity
 * is not evidence that this is an unaliased value write. */
static TZrBool semantic_typecheck_identifier_write_is_opaque(
        const SZrAstNode *identifier, const SZrTypeEnvironment *typeEnv) {
    const SZrTypeBinding *binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            typeEnv, identifier->data.identifier.name, identifier->data.identifier.syntaxContext);
    const SZrSemanticSymbolRecord *symbol = ZR_NULL;
    const SZrType *annotation = ZR_NULL;
    if (binding == ZR_NULL) return ZR_TRUE;
    for (const SZrTypeEnvironment *env = typeEnv; env != ZR_NULL && symbol == ZR_NULL; env = env->parent) {
        symbol = ZrParser_Semantic_FindSymbolById(env->semanticContext, binding->symbolId);
    }
    if (symbol == ZR_NULL || symbol->astNode == ZR_NULL) return ZR_TRUE;
    if (symbol->astNode->type == ZR_AST_PARAMETER) {
        const SZrParameter *parameter = &symbol->astNode->data.parameter;
        if (parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_REF_READONLY ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_IN ||
            parameter->passingMode == ZR_PARAMETER_PASSING_MODE_IN) return ZR_FALSE;
        if (parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_REF ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_SCOPED_REF ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_OUT ||
            parameter->passingMode == ZR_PARAMETER_PASSING_MODE_REF ||
            parameter->passingMode == ZR_PARAMETER_PASSING_MODE_OUT) return ZR_TRUE;
        annotation = parameter->typeInfo;
    } else if (symbol->astNode->type == ZR_AST_VARIABLE_DECLARATION) {
        annotation = symbol->astNode->data.variableDeclaration.typeInfo;
    }
    return binding->type.referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE ||
           (annotation != ZR_NULL && annotation->referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE);
}

static TZrBool semantic_typecheck_nodes_may_write_range(
        const SZrAstNodeArray *nodes, const SZrTypeBinding *binding, const SZrTypeEnvironment *typeEnv) {
    if (nodes != ZR_NULL && nodes->nodes != ZR_NULL) {
        for (TZrSize index = 0; index < nodes->count; ++index) {
            if (semantic_typecheck_node_may_write_range(nodes->nodes[index], binding, typeEnv)) return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_typecheck_node_may_write_range(
        const SZrAstNode *node, const SZrTypeBinding *binding, const SZrTypeEnvironment *typeEnv) {
    if (node == ZR_NULL) return ZR_FALSE;
    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            return ZR_FALSE;
        case ZR_AST_EXPRESSION_STATEMENT:
            return semantic_typecheck_node_may_write_range(node->data.expressionStatement.expr, binding, typeEnv);
        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_typecheck_node_may_write_range(node->data.variableDeclaration.value, binding, typeEnv);
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            const SZrAstNode *left = node->data.assignmentExpression.left;
            if (left == ZR_NULL || left->type != ZR_AST_IDENTIFIER_LITERAL) return ZR_TRUE;
            /* Binding lookup normalizes hygiene namespaces. A same-spelled
             * write is conservatively a may-write even if raw contexts differ. */
            return semantic_typecheck_identifier_write_is_opaque(left, typeEnv) ||
                   (binding != ZR_NULL && ZrCore_String_Equal(left->data.identifier.name, binding->name)) ||
                   !semantic_typecheck_syntax_is_primitive(left, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.assignmentExpression.right, binding, typeEnv);
        }
        case ZR_AST_BINARY_EXPRESSION:
            return !semantic_typecheck_syntax_is_primitive(node->data.binaryExpression.left, typeEnv) ||
                   !semantic_typecheck_syntax_is_primitive(node->data.binaryExpression.right, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.binaryExpression.left, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.binaryExpression.right, binding, typeEnv);
        case ZR_AST_LOGICAL_EXPRESSION:
            return !semantic_typecheck_syntax_is_primitive(node->data.logicalExpression.left, typeEnv) ||
                   !semantic_typecheck_syntax_is_primitive(node->data.logicalExpression.right, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.logicalExpression.left, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.logicalExpression.right, binding, typeEnv);
        case ZR_AST_UNARY_EXPRESSION: {
            const TZrChar *op = node->data.unaryExpression.op.op;
            return op == ZR_NULL || op[1] != '\0' || strchr("+-!~", *op) == ZR_NULL ||
                   !semantic_typecheck_syntax_is_primitive(node->data.unaryExpression.argument, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.unaryExpression.argument, binding, typeEnv);
        }
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return semantic_typecheck_node_may_write_range(node->data.conditionalExpression.test, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.conditionalExpression.consequent, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.conditionalExpression.alternate, binding, typeEnv);
        case ZR_AST_PRIMARY_EXPRESSION: {
            const SZrAstNode *property = node->data.primaryExpression.property;
            const SZrAstNodeArray *members = node->data.primaryExpression.members;
            const SZrInferredType *type = semantic_typecheck_syntax_binding_type(property, typeEnv);
            if (semantic_typecheck_node_may_write_range(property, binding, typeEnv)) return ZR_TRUE;
            for (TZrSize index = 0; members != ZR_NULL && index < members->count; ++index) {
                const SZrAstNode *member = members->nodes[index];
                /* A getter, user @getItem or explicit call is an unknown effect.
                 * Only indexing an already-known builtin array is a pure read. */
                if (type == ZR_NULL || type->baseType != ZR_VALUE_TYPE_ARRAY || member == ZR_NULL ||
                    member->type != ZR_AST_MEMBER_EXPRESSION || !member->data.memberExpression.computed ||
                    semantic_typecheck_node_may_write_range(member->data.memberExpression.property, binding, typeEnv)) {
                    return ZR_TRUE;
                }
                type = type->elementTypes.length == 1
                        ? (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0) : ZR_NULL;
            }
            return ZR_FALSE;
        }
        case ZR_AST_ARRAY_LITERAL:
            return semantic_typecheck_nodes_may_write_range(node->data.arrayLiteral.elements, binding, typeEnv);
        case ZR_AST_OBJECT_LITERAL:
            return semantic_typecheck_nodes_may_write_range(node->data.objectLiteral.properties, binding, typeEnv);
        case ZR_AST_KEY_VALUE_PAIR:
            return (node->data.keyValuePair.keyIsComputed &&
                    semantic_typecheck_node_may_write_range(node->data.keyValuePair.key, binding, typeEnv)) ||
                   semantic_typecheck_node_may_write_range(node->data.keyValuePair.value, binding, typeEnv);
        case ZR_AST_BLOCK:
            return semantic_typecheck_nodes_may_write_range(node->data.block.body, binding, typeEnv);
        case ZR_AST_RETURN_STATEMENT:
            return node->data.returnStatement.isReferenceReturn ||
                   semantic_typecheck_node_may_write_range(node->data.returnStatement.expr, binding, typeEnv);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return semantic_typecheck_node_may_write_range(node->data.breakContinueStatement.expr, binding, typeEnv);
        case ZR_AST_IF_EXPRESSION:
            return semantic_typecheck_node_may_write_range(node->data.ifExpression.condition, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.ifExpression.thenExpr, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.ifExpression.elseExpr, binding, typeEnv);
        case ZR_AST_WHILE_LOOP:
            return semantic_typecheck_node_may_write_range(node->data.whileLoop.cond, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.whileLoop.block, binding, typeEnv);
        case ZR_AST_FOR_LOOP:
            return semantic_typecheck_node_may_write_range(node->data.forLoop.init, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.forLoop.cond, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.forLoop.block, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.forLoop.step, binding, typeEnv);
        case ZR_AST_FOREACH_LOOP:
            return semantic_typecheck_node_may_write_range(node->data.foreachLoop.expr, binding, typeEnv) ||
                   semantic_typecheck_node_may_write_range(node->data.foreachLoop.block, binding, typeEnv);
        default:
            return ZR_TRUE;
    }
}

/* Initializer/branch refinements are invariant only for bindings not written on
 * a backedge. Widen may-writes to their declared contract, retaining identity.
 * for.init and foreach.expr run once, but the source check does not transfer
 * their writes yet: forget those effects too until a supported exit replay.
 * Merely reading a value in the preheader never widens it. */
static SZrTypeEnvironment *semantic_typecheck_push_loop_range_scope(
        SZrState *state, SZrSemanticAnalyzer *analyzer, const SZrAstNode *loopNode,
        const SZrTypeEnvironment *iterationEnv) {
    SZrTypeEnvironment *savedEnv =
            semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
    SZrTypeEnvironment *loopEnv;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        savedEnv == ZR_NULL || analyzer->compilerState->typeEnv == savedEnv) {
        return savedEnv;
    }
    loopEnv = analyzer->compilerState->typeEnv;
    for (SZrTypeEnvironment *env = savedEnv; env != ZR_NULL; env = env->parent) {
        /* A foreach item is initialized anew on each iteration. */
        if (env == iterationEnv) continue;
        for (TZrSize index = 0; index < env->variableTypes.length; ++index) {
            const SZrTypeBinding *binding =
                    (const SZrTypeBinding *)ZrCore_Array_Get(&env->variableTypes, index);
            SZrSymbol *symbol;
            const SZrType *declaredTypeNode = ZR_NULL;
            SZrTypeBinding widened;

            if (binding == ZR_NULL || binding->name == ZR_NULL ||
                !ZR_VALUE_IS_TYPE_NUMBER(binding->type.baseType) ||
                !binding->type.hasRangeConstraint ||
                !semantic_typecheck_node_may_write_range(loopNode, binding, savedEnv) ||
                ZrParser_TypeEnvironment_FindVariableBindingWithContext(
                        savedEnv, binding->name, binding->syntaxContext) != binding) {
                continue;
            }
            symbol = ZrLanguageServer_SymbolTable_FindBySemanticId(
                    analyzer->symbolTable, binding->symbolId);
            if (symbol == ZR_NULL && binding->hasDeclarationRange) {
                symbol = ZrLanguageServer_SymbolTable_FindDefinition(
                        analyzer->symbolTable, binding->declarationRange);
            }
            if (symbol != ZR_NULL && symbol->isConst) {
                continue;
            }
            if (symbol != ZR_NULL && symbol->astNode != ZR_NULL) {
                if (symbol->astNode->type == ZR_AST_PARAMETER) {
                    const SZrParameter *parameter = &symbol->astNode->data.parameter;
                    if (parameter->passingMode == ZR_PARAMETER_PASSING_MODE_IN) {
                        continue;
                    }
                    declaredTypeNode = parameter->typeInfo;
                } else if (symbol->astNode->type == ZR_AST_VARIABLE_DECLARATION) {
                    declaredTypeNode = symbol->astNode->data.variableDeclaration.typeInfo;
                }
            }

            widened = *binding;
            memset(&widened.type, 0, sizeof(widened.type));
            ZrParser_InferredType_Copy(state, &widened.type, &binding->type);
            widened.type.hasRangeConstraint = ZR_FALSE;
            widened.type.minValue = widened.type.maxValue = 0;
            ZrParser_InferredType_ResetRangeSegments(&widened.type);
            if (declaredTypeNode != ZR_NULL) {
                SZrInferredType declaredType;
                ZrParser_InferredType_Init(state, &declaredType, ZR_VALUE_TYPE_OBJECT);
                if (semantic_type_from_ast(state, analyzer, declaredTypeNode, &declaredType)) {
                    widened.type.hasRangeConstraint = declaredType.hasRangeConstraint;
                    widened.type.minValue = declaredType.minValue;
                    widened.type.maxValue = declaredType.maxValue;
                    (void)ZrParser_InferredType_CopyRangeSegments(state, &widened.type, &declaredType);
                }
                ZrParser_InferredType_Free(state, &declaredType);
            }
            ZrCore_Array_Push(state, &loopEnv->variableTypes, &widened);
        }
    }
    return savedEnv;
}

/* A failed optional join must not resurrect a preheader singleton. Rebuild the
 * same may-write overlay and copy only its declared/unknown types into the
 * current lexical scope; never publish a loop-local binding or source fact. */
static void semantic_typecheck_join_loop_ranges(
        SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    SZrAstNode *condition = node->type == ZR_AST_WHILE_LOOP ? node->data.whileLoop.cond :
                           node->type == ZR_AST_FOR_LOOP ? node->data.forLoop.cond : ZR_NULL;
    TZrBool conditionValue = ZR_FALSE;
    TZrBool neverRuns = condition != ZR_NULL &&
            ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
                    analyzer, condition, &conditionValue, ZR_NULL) && !conditionValue;
    TZrBool joined = ZR_FALSE;
    if (neverRuns || !semantic_typecheck_node_may_write_range(node, ZR_NULL, analyzer->compilerState->typeEnv)) {
        if (node->type == ZR_AST_WHILE_LOOP) {
            joined = ZrParser_TypeInference_TryJoinWhileNumericAssignments(analyzer->compilerState, node);
        } else if (node->type == ZR_AST_FOR_LOOP) {
            joined = ZrParser_TypeInference_TryJoinForNumericAssignments(analyzer->compilerState, node);
        } else {
            joined = ZrParser_TypeInference_TryJoinForeachNumericAssignments(analyzer->compilerState, node);
        }
    }
    if (joined) return;
    /* A false loop cannot write through its body or step; only an unsupported
     * for initializer may still have invalidated an incoming range. */
    const SZrAstNode *writes = neverRuns
            ? (node->type == ZR_AST_FOR_LOOP ? node->data.forLoop.init : ZR_NULL) : node;
    SZrTypeEnvironment *savedEnv = semantic_typecheck_push_loop_range_scope(state, analyzer, writes, ZR_NULL);
    SZrTypeEnvironment *overlay = analyzer->compilerState->typeEnv;
    if (savedEnv != ZR_NULL && overlay != savedEnv) {
        for (TZrSize index = 0; index < overlay->variableTypes.length; ++index) {
            const SZrTypeBinding *source = (const SZrTypeBinding *)ZrCore_Array_Get(&overlay->variableTypes, index);
            SZrTypeBinding *existing = ZR_NULL;
            for (TZrSize target = 0; target < savedEnv->variableTypes.length; ++target) {
                SZrTypeBinding *candidate = (SZrTypeBinding *)ZrCore_Array_Get(&savedEnv->variableTypes, target);
                if (ZrCore_String_Equal(candidate->name, source->name) &&
                    ZrParser_SyntaxContext_Equal(candidate->syntaxContext, source->syntaxContext)) {
                    existing = candidate;
                    break;
                }
            }
            if (existing != ZR_NULL) {
                ZrParser_InferredType_Free(state, &existing->type);
                ZrParser_InferredType_Copy(state, &existing->type, &source->type);
            } else {
                SZrTypeBinding binding = *source;
                memset(&binding.type, 0, sizeof(binding.type));
                ZrParser_InferredType_Copy(state, &binding.type, &source->type);
                ZrCore_Array_Push(state, &savedEnv->variableTypes, &binding);
            }
        }
    }
    semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedEnv);
}

static void semantic_typecheck_register_variable_binding(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrString *name,
                                                         const SZrType *typeNode,
                                                         SZrAstNode *valueNode,
                                                         SZrAstNode *declarationNode) {
    SZrInferredType bindingType;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (typeNode != ZR_NULL) {
        if (!semantic_type_from_ast(state, analyzer, typeNode, &bindingType)) {
            ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(
                    state,
                    analyzer,
                    typeNode->name != ZR_NULL ? typeNode->name->location : ZrParser_FileRange_Create(
                                                                          ZrParser_FilePosition_Create(0, 0, 0),
                                                                          ZrParser_FilePosition_Create(0, 0, 0),
                                                                          ZR_NULL));
            ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
                    state, analyzer, name, ZR_NULL, declarationNode);
            ZrParser_InferredType_Free(state, &bindingType);
            return;
        }
        if (valueNode != ZR_NULL) {
            SZrInferredType initializerType;
            ZrParser_InferredType_Init(state, &initializerType, ZR_VALUE_TYPE_OBJECT);
            if (semantic_infer_node_type(
                        state, analyzer, valueNode, &initializerType, ZR_NULL)) {
                (void)ZrParser_TypeInference_TryApplyInitializerNumericRange(state,
                                                                             &bindingType,
                                                                             &initializerType);
            }
            ZrParser_InferredType_Free(state, &initializerType);
        }
    } else if (valueNode != ZR_NULL) {
        TZrBool publishedDiagnostic = ZR_FALSE;
        if (!semantic_infer_node_type(state,
                                      analyzer,
                                      valueNode,
                                      &bindingType,
                                      &publishedDiagnostic)) {
            if (analyzer->compilerState != ZR_NULL && analyzer->compilerState->hasError) {
                ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state, analyzer, valueNode->location);
            } else if (!publishedDiagnostic) {
                ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(
                        state, analyzer, valueNode->location);
            }
            ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
                    state, analyzer, name, ZR_NULL, declarationNode);
            ZrParser_InferredType_Free(state, &bindingType);
            return;
        }
    }

    ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
        state,
        analyzer,
        name,
        &bindingType,
        declarationNode);
    ZrParser_InferredType_Free(state, &bindingType);
}

static void semantic_typecheck_register_parameter_bindings(SZrState *state,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrAstNodeArray *params) {
    if (state == ZR_NULL || analyzer == ZR_NULL || params == ZR_NULL || params->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *param;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        param = &paramNode->data.parameter;
        semantic_typecheck_register_variable_binding(state,
                                                     analyzer,
                                                     param->name != ZR_NULL ? param->name->name : ZR_NULL,
                                                     param->typeInfo,
                                                     ZR_NULL,
                                                     paramNode);
    }
}

static void semantic_typecheck_register_foreach_binding(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrForeachLoop *foreachLoop,
                                                        SZrFileRange diagnosticLocation) {
    SZrString *name;
    SZrInferredType bindingType;
    TZrBool hasBindingType = ZR_FALSE;

    if (state == ZR_NULL || analyzer == ZR_NULL || foreachLoop == ZR_NULL ||
        analyzer->compilerState == ZR_NULL || analyzer->compilerState->typeEnv == ZR_NULL) {
        return;
    }

    name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, foreachLoop->pattern);
    if (name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (foreachLoop->typeInfo != ZR_NULL) {
        hasBindingType = semantic_type_from_ast(state, analyzer, foreachLoop->typeInfo, &bindingType);
    } else if (foreachLoop->expr != ZR_NULL) {
        SZrInferredType iterableType;

        ZrParser_InferredType_Init(state, &iterableType, ZR_VALUE_TYPE_OBJECT);
        if (semantic_infer_node_type(state,
                                     analyzer,
                                     foreachLoop->expr,
                                     &iterableType,
                                     ZR_NULL)) {
            hasBindingType =
                bind_foreach_element_type_from_inferred_iterable(analyzer->compilerState, &iterableType, &bindingType);
        }
        ZrParser_InferredType_Free(state, &iterableType);
    }

    if (!hasBindingType) {
        ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(state, analyzer, diagnosticLocation);
        ZrParser_InferredType_Free(state, &bindingType);
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
        state,
        analyzer,
        name,
        &bindingType,
        foreachLoop->pattern);
    ZrParser_InferredType_Free(state, &bindingType);
}

static SZrString *semantic_typecheck_extract_owner_type_name(SZrAstNode *ownerTypeNode) {
    if (ownerTypeNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (ownerTypeNode->type) {
        case ZR_AST_CLASS_DECLARATION:
            return ownerTypeNode->data.classDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.classDeclaration.name->name
                       : ZR_NULL;

        case ZR_AST_STRUCT_DECLARATION:
            return ownerTypeNode->data.structDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.structDeclaration.name->name
                       : ZR_NULL;

        case ZR_AST_INTERFACE_DECLARATION:
            return ownerTypeNode->data.interfaceDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.interfaceDeclaration.name->name
                       : ZR_NULL;

        default:
            return ZR_NULL;
    }
}

static void semantic_typecheck_push_compiler_context(SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *ownerTypeNode,
                                                     SZrAstNode *functionNode,
                                                     SZrSemanticTypecheckContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;
    SZrString *typeName;

    if (snapshot != ZR_NULL) {
        memset(snapshot, 0, sizeof(*snapshot));
    }

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    snapshot->typePrototype = compilerState->currentTypePrototypeInfo;
    snapshot->typeNode = compilerState->currentTypeNode;
    snapshot->typeName = compilerState->currentTypeName;
    snapshot->functionNode = compilerState->currentFunctionNode;

    typeName = semantic_typecheck_extract_owner_type_name(ownerTypeNode);
    if (typeName != ZR_NULL) {
        compilerState->currentTypeNode = ownerTypeNode;
        compilerState->currentTypeName = typeName;
        compilerState->currentTypePrototypeInfo =
            find_compiler_type_prototype_inference(compilerState, typeName);
    }

    if (functionNode != ZR_NULL) {
        compilerState->currentFunctionNode = functionNode;
    }
}

static void semantic_typecheck_pop_compiler_context(
        SZrSemanticAnalyzer *analyzer,
        const SZrSemanticTypecheckContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    compilerState->currentTypePrototypeInfo = snapshot->typePrototype;
    compilerState->currentTypeNode = snapshot->typeNode;
    compilerState->currentTypeName = snapshot->typeName;
    compilerState->currentFunctionNode = snapshot->functionNode;
}

static void semantic_typecheck_record_super_constructor_facts(
        SZrCompilerState *compilerState,
        SZrAstNode *functionNode) {
    SZrTypePrototypeInfo *superPrototype;
    SZrTypeMemberInfo temporaryConstructor;
    SZrAstNode superTypeTarget;
    SZrString *superTypeName;
    TZrBool hasConstructor = ZR_FALSE;
    TZrBool appendedTemporary = ZR_FALSE;
    TZrSize index;

    if (compilerState == ZR_NULL || functionNode == ZR_NULL ||
        functionNode->type != ZR_AST_CLASS_META_FUNCTION ||
        !functionNode->data.classMetaFunction.hasSuperCall ||
        compilerState->currentTypePrototypeInfo == ZR_NULL) {
        return;
    }

    superTypeName = compilerState->currentTypePrototypeInfo->extendsTypeName;
    if (superTypeName == ZR_NULL) {
        return;
    }

    superPrototype = find_compiler_type_prototype_inference(compilerState, superTypeName);
    if (superPrototype == ZR_NULL) {
        return;
    }

    for (index = 0U; index < superPrototype->members.length; index++) {
        SZrTypeMemberInfo *member =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(&superPrototype->members, index);
        if (member != ZR_NULL && member->isMetaMethod &&
            member->metaType == ZR_META_CONSTRUCTOR) {
            hasConstructor = ZR_TRUE;
            break;
        }
    }

    memset(&temporaryConstructor, 0, sizeof(temporaryConstructor));
    memset(&superTypeTarget, 0, sizeof(superTypeTarget));
    superTypeTarget.type = ZR_AST_IDENTIFIER_LITERAL;
    superTypeTarget.data.identifier.name = superTypeName;
    if (!hasConstructor &&
        type_inference_source_constructor_member_build(
                compilerState,
                &superTypeTarget,
                superTypeName,
                &temporaryConstructor)) {
        ZrCore_Array_Push(compilerState->state,
                          &superPrototype->members,
                          &temporaryConstructor);
        appendedTemporary = ZR_TRUE;
    }

    type_inference_record_super_constructor_call_facts(
            compilerState,
            functionNode,
            superTypeName,
            functionNode->data.classMetaFunction.superArgs);

    if (appendedTemporary) {
        superPrototype->members.length--;
        type_inference_source_constructor_member_free(
                compilerState,
                &temporaryConstructor);
    }
}

/* 参数、this/super 绑定必须在正文检查之前进入新的词法类型环境，退出时恢复编译器上下文。 */
static void semantic_typecheck_callable_body(SZrState *state,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrAstNode *functionNode,
                                             SZrAstNodeArray *params,
                                             SZrAstNodeArray *preludeExpressions,
                                             SZrAstNode *body) {
    SZrSemanticTypecheckContextSnapshot contextSnapshot;
    SZrTypeEnvironment *savedTypeEnv;

    semantic_typecheck_push_compiler_context(analyzer, ZR_NULL, functionNode, &contextSnapshot);
    savedTypeEnv = semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
    ZrLanguageServer_SemanticAnalyzer_RegisterTypecheckReceiverBindings(
            state, analyzer, functionNode);
    semantic_typecheck_register_parameter_bindings(state, analyzer, params);
    if (functionNode != ZR_NULL &&
        functionNode->type == ZR_AST_CLASS_META_FUNCTION &&
        analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL &&
        analyzer->compilerState->currentTypePrototypeInfo != ZR_NULL) {
        semantic_typecheck_record_super_constructor_facts(
                analyzer->compilerState,
                functionNode);
    }

    if (params != ZR_NULL && params->nodes != ZR_NULL) {
        for (TZrSize i = 0; i < params->count; i++) {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, params->nodes[i]);
        }
    }
    if (preludeExpressions != ZR_NULL && preludeExpressions->nodes != ZR_NULL) {
        for (TZrSize i = 0; i < preludeExpressions->count; i++) {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                  analyzer,
                                                                  preludeExpressions->nodes[i]);
        }
    }
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, body);

    semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
    semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
}

static void semantic_clear_compiler_error(SZrSemanticAnalyzer *analyzer) {
    if (analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL) {
        analyzer->compilerState->hasError = ZR_FALSE;
        ZrParser_Compiler_ClearStructuredError(analyzer->compilerState);
    }
}

static TZrBool semantic_publish_current_compiler_diagnostic(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    TZrBool published;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->compilerState == ZR_NULL) {
        return ZR_FALSE;
    }

    published =
            ZrLanguageServer_SemanticAnalyzer_PublishCurrentCompilerQueryDiagnostic(
                    state,
                    analyzer);
    /* BUG: PublishCurrentDiagnostic 在构造或追加结构化事实失败时返回 false，
     * 这里仍清除编译器错误；可达的诊断因此被静默丢弃。 */
    semantic_clear_compiler_error(analyzer);
    return published;
}

static TZrBool semantic_type_from_ast(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer,
                                      const SZrType *typeNode,
                                      SZrInferredType *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeNode != ZR_NULL &&
        ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(analyzer,
                                                                        ZR_NULL,
                                                                        analyzer->compilerState != ZR_NULL
                                                                            ? analyzer->compilerState->currentFunctionNode
                                                                            : ZR_NULL,
                                                                        typeNode,
                                                                        result)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool semantic_infer_node_type(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrAstNode *node,
                                        SZrInferredType *result,
                                        TZrBool *outPublishedDiagnostic) {
    if (outPublishedDiagnostic != ZR_NULL) {
        *outPublishedDiagnostic = ZR_FALSE;
    }

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state, analyzer, node, result)) {
        return ZR_TRUE;
    }

    if (analyzer->compilerState != ZR_NULL && analyzer->compilerState->hasError) {
        ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state, analyzer, node->location);
        if (outPublishedDiagnostic != ZR_NULL &&
            analyzer->semanticContext != ZR_NULL &&
            !analyzer->compilerState->hasError) {
            *outPublishedDiagnostic = ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* using 守卫的 union payload 仅在命中分支可见，同时记录资源离开作用域时的清理事实。 */
static void semantic_typecheck_using_statement(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrAstNode *node) {
    SZrUsingStatement *usingStmt;
    SZrSemanticUnionPatternResolution resolution;
    TZrBool hasUnionPattern;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_USING_STATEMENT) {
        return;
    }

    usingStmt = &node->data.usingStatement;
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->resource);
    if (usingStmt->guardKind == ZR_USING_GUARD_DROP &&
        analyzer->compilerState != ZR_NULL) {
        SZrInferredType resourceType;
        TZrBool hasResourceType;
        ZrParser_InferredType_Init(state, &resourceType, ZR_VALUE_TYPE_OBJECT);
        hasResourceType = ZrParser_SemanticMetadata_InferUsingResourceType(
                analyzer->compilerState, usingStmt->resource, &resourceType);
        ZrParser_SemanticMetadata_RecordUsingCleanup(
                analyzer->compilerState, node, hasResourceType ? &resourceType : ZR_NULL);
        ZrParser_InferredType_Free(state, &resourceType);
    }

    ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(state, &resolution);
    hasUnionPattern = ZrLanguageServer_SemanticAnalyzer_ResolveUsingUnionPattern(state,
                                                                                 analyzer,
                                                                                 usingStmt,
                                                                                 &resolution);
    if (hasUnionPattern && usingStmt->body != ZR_NULL) {
        SZrTypeEnvironment *savedTypeEnv =
            semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
        ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(state,
                                                                       analyzer,
                                                                       &resolution,
                                                                       ZR_FALSE,
                                                                       ZR_TRUE);
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->body);
        semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
    } else {
        if (hasUnionPattern) {
            /* TODO: body 为空时 payload 被登记在当前类型环境，后续 elseBody 可见；
             * 需与符号收集路径一同核对无正文 using 的 else 语义。 */
            ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(state,
                                                                           analyzer,
                                                                           &resolution,
                                                                           ZR_FALSE,
                                                                           ZR_TRUE);
        }
        if (!hasUnionPattern && usingStmt->body != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->body);
        }
    }

    if (usingStmt->elseBody != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->elseBody);
    }
    ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionFree(state, &resolution);
}

/* 每个 switch case 有独立 payload 类型环境；所有 case 后才判定 union 覆盖和 default 可达性。 */
static void semantic_typecheck_switch_expression(SZrState *state,
                                                 SZrSemanticAnalyzer *analyzer,
                                                 SZrAstNode *node) {
    SZrSwitchExpression *switchExpr;
    SZrInferredType subjectType;
    TZrBool subjectTypeInitialized = ZR_FALSE;
    TZrBool hasSubjectType = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_SWITCH_EXPRESSION) {
        return;
    }

    switchExpr = &node->data.switchExpression;
    switchExpr->isUnionExhaustive = ZR_FALSE;
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, switchExpr->expr);
    if (switchExpr->expr != ZR_NULL) {
        ZrParser_InferredType_Init(state, &subjectType, ZR_VALUE_TYPE_OBJECT);
        subjectTypeInitialized = ZR_TRUE;
        hasSubjectType = semantic_infer_node_type(
                state, analyzer, switchExpr->expr, &subjectType, ZR_NULL);
    }

    if (switchExpr->cases != ZR_NULL && switchExpr->cases->nodes != ZR_NULL) {
        for (index = 0; index < switchExpr->cases->count; index++) {
            SZrAstNode *caseNode = switchExpr->cases->nodes[index];
            SZrSwitchCase *switchCase;
            SZrSemanticUnionPatternResolution resolution;
            TZrBool hasUnionPattern;

            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                continue;
            }

            switchCase = &caseNode->data.switchCase;
            ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(state, &resolution);
            hasUnionPattern = hasSubjectType &&
                              ZrLanguageServer_SemanticAnalyzer_ResolveSwitchUnionPattern(state,
                                                                                          analyzer,
                                                                                          switchCase->value,
                                                                                          &subjectType,
                                                                                          &resolution);
            if (hasUnionPattern && switchCase->block != ZR_NULL) {
                SZrTypeEnvironment *savedTypeEnv =
                    semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
                ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(state,
                                                                               analyzer,
                                                                               &resolution,
                                                                               ZR_FALSE,
                                                                               ZR_TRUE);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, switchCase->block);
                semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            } else {
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, switchCase->value);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, switchCase->block);
            }
            ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionFree(state, &resolution);
        }
    }

    if (hasSubjectType) {
        ZrLanguageServer_SemanticAnalyzer_AnalyzeSwitchUnionExhaustiveness(state,
                                                                           analyzer,
                                                                           node,
                                                                           &subjectType);
    }

    if (switchExpr->defaultCase != ZR_NULL &&
        switchExpr->defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(
                state,
                analyzer,
                switchExpr->defaultCase->data.switchDefault.block);
    }

    if (subjectTypeInitialized) {
        ZrParser_InferredType_Free(state, &subjectType);
    }
}

static void semantic_check_primary_call_with_parser_inference(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *node) {
    SZrInferredType result;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    (void)semantic_infer_node_type(state, analyzer, node, &result, ZR_NULL);
    if (analyzer->compilerState != ZR_NULL &&
        analyzer->compilerState->hasError) {
        (void)semantic_publish_current_compiler_diagnostic(state, analyzer);
    }
    ZrParser_InferredType_Free(state, &result);
}

static SZrSymbol *semantic_find_enclosing_callable(SZrSymbolTable *table, SZrFileRange position) {
    SZrSymbol *bestSymbol = ZR_NULL;

    if (table == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL || (*symbolPtr)->astNode == ZR_NULL) {
                continue;
            }

            if ((*symbolPtr)->type != ZR_SYMBOL_FUNCTION && (*symbolPtr)->type != ZR_SYMBOL_METHOD) {
                continue;
            }

            if ((*symbolPtr)->location.start.offset <= position.start.offset &&
                position.end.offset <= (*symbolPtr)->location.end.offset &&
                (bestSymbol == ZR_NULL ||
                 (*symbolPtr)->location.start.offset >= bestSymbol->location.start.offset)) {
                bestSymbol = *symbolPtr;
            }
        }
    }

    return bestSymbol;
}

static const SZrType *semantic_callable_return_type(SZrSymbol *symbol) {
    if (symbol == ZR_NULL || symbol->astNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (symbol->astNode->type == ZR_AST_FUNCTION_DECLARATION) {
        return symbol->astNode->data.functionDeclaration.returnType;
    }
    if (symbol->astNode->type == ZR_AST_CLASS_METHOD) {
        return symbol->astNode->data.classMethod.returnType;
    }

    return ZR_NULL;
}

/**
 * @brief 在符号收集后按 AST 作用域发布精确类型、所有权、控制流和结构化诊断事实。
 * @note 值表达式继承当前环境；声明、分支和可调用体由此调度器单独管理环境边界。
 */
void ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 根据节点类型进行类型检查
    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL && binExpr->right != ZR_NULL) {
                ZR_UNUSED_PARAMETER(binExpr);
            }
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrInferredType assignmentType;

            /* Complete assignment inference preserves the target's write role. */
            ZrParser_InferredType_Init(state, &assignmentType, ZR_VALUE_TYPE_OBJECT);
            (void)semantic_infer_node_type(
                    state,
                    analyzer,
                    node,
                    &assignmentType,
                    ZR_NULL);
            ZrParser_InferredType_Free(state, &assignmentType);

            (void)ZrParser_ConstAssignment_PublishDiagnostic(
                    analyzer->compilerState, analyzer->ast, node);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            ZR_UNUSED_PARAMETER(&node->data.functionCall);
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->typeInfo != ZR_NULL && varDecl->value != ZR_NULL) {
                SZrInferredType expectedType;
                SZrInferredType valueType;
                SZrFileRange expectedLocation;
                TZrBool hasValueType;
                TZrBool compatible = ZR_FALSE;

                ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(state, &valueType, ZR_VALUE_TYPE_OBJECT);
                if (!semantic_type_from_ast(state, analyzer, varDecl->typeInfo, &expectedType)) {
                    ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(
                            state,
                            analyzer,
                            varDecl->typeInfo->name != ZR_NULL ? varDecl->typeInfo->name->location : node->location);
                    ZrParser_InferredType_Free(state, &valueType);
                    ZrParser_InferredType_Free(state, &expectedType);
                    break;
                }
                TZrBool publishedDiagnostic = ZR_FALSE;
                hasValueType = semantic_infer_node_type(
                        state,
                        analyzer,
                        varDecl->value,
                        &valueType,
                        &publishedDiagnostic);
                if (!hasValueType && !publishedDiagnostic) {
                    ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(state, analyzer, varDecl->value->location);
                } else {
                    expectedLocation =
                            varDecl->typeInfo->name != ZR_NULL
                                    ? varDecl->typeInfo->name->location
                                    : node->location;
                    compatible = analyzer->compilerState != ZR_NULL &&
                                 ZrParser_AssignmentCompatibility_CheckDetailed(
                                         analyzer->compilerState,
                                         &expectedType,
                                         &valueType,
                                         varDecl->value->location,
                                         &expectedLocation);
                }
                if (hasValueType && !compatible) {
                    (void)semantic_publish_current_compiler_diagnostic(
                            state,
                            analyzer);
                }
                ZrParser_InferredType_Free(state, &valueType);
                ZrParser_InferredType_Free(state, &expectedType);
            }
            break;
        }
        
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                SZrSymbol *enclosingCallable =
                        semantic_find_enclosing_callable(analyzer->symbolTable, node->location);
                const SZrType *returnTypeNode = semantic_callable_return_type(enclosingCallable);
                if (returnTypeNode != ZR_NULL) {
                    SZrInferredType expectedType;
                    SZrInferredType actualType;
                    SZrFileRange expectedLocation;
                    TZrBool compatible = ZR_FALSE;
                    TZrBool hasActualType;

                    ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
                    ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
                    if (!semantic_type_from_ast(state, analyzer, returnTypeNode, &expectedType)) {
                        ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(
                                state,
                                analyzer,
                                returnTypeNode->name != ZR_NULL ? returnTypeNode->name->location : node->location);
                        ZrParser_InferredType_Free(state, &actualType);
                        ZrParser_InferredType_Free(state, &expectedType);
                        break;
                    }
                    TZrBool publishedDiagnostic = ZR_FALSE;
                    hasActualType = semantic_infer_node_type(
                            state,
                            analyzer,
                            returnStmt->expr,
                            &actualType,
                            &publishedDiagnostic);
                    expectedLocation =
                            returnTypeNode->name != ZR_NULL
                                    ? returnTypeNode->name->location
                                    : node->location;
                    compatible = hasActualType &&
                                 analyzer->compilerState != ZR_NULL &&
                                 ZrParser_AssignmentCompatibility_CheckDetailed(
                                         analyzer->compilerState,
                                         &expectedType,
                                         &actualType,
                                         returnStmt->expr->location,
                                         &expectedLocation);
                    if (!hasActualType && !publishedDiagnostic) {
                        ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(state,
                                                                        analyzer,
                                                                        returnStmt->expr->location);
                    }
                    if (hasActualType && !compatible) {
                        (void)semantic_publish_current_compiler_diagnostic(
                                state,
                                analyzer);
                    }
                    ZrParser_InferredType_Free(state, &actualType);
                    ZrParser_InferredType_Free(state, &expectedType);
                } else {
                    SZrInferredType actualType;
                    TZrBool hasActualType;

                    ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
                    TZrBool publishedDiagnostic = ZR_FALSE;
                    hasActualType = semantic_infer_node_type(
                            state,
                            analyzer,
                            returnStmt->expr,
                            &actualType,
                            &publishedDiagnostic);
                    ZrParser_InferredType_Free(state, &actualType);
                    if (!hasActualType && !publishedDiagnostic) {
                        ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(state,
                                                                        analyzer,
                                                                        returnStmt->expr->location);
                    }
                }
            }
            break;
        }

        case ZR_AST_EXPRESSION_STATEMENT: {
            SZrAstNode *expr = node->data.expressionStatement.expr;
            SZrInferredType exprType;

            if (expr != ZR_NULL && expr->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
                ZrParser_InferredType_Init(state, &exprType, ZR_VALUE_TYPE_OBJECT);
                (void)semantic_infer_node_type(state, analyzer, expr, &exprType, ZR_NULL);
                ZrParser_InferredType_Free(state, &exprType);
            }
            break;
        }

        case ZR_AST_LOGICAL_EXPRESSION: {
            TZrBool leftValue = ZR_FALSE;
            SZrAstNode *leftEvidence = ZR_NULL;
            SZrAstNode *left = node->data.logicalExpression.left;
            SZrAstNode *right = node->data.logicalExpression.right;
            if (right != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
                        analyzer,
                        left,
                        &leftValue,
                        &leftEvidence) &&
                ((semantic_text_equals(node->data.logicalExpression.op, "||") && leftValue) ||
                 (semantic_text_equals(node->data.logicalExpression.op, "&&") && !leftValue))) {
                semantic_record_logical_fact(analyzer,
                                             node,
                                             ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT,
                                             ZR_TRUE,
                                             leftValue,
                                             right);
                semantic_record_reachability_fact_at_range(analyzer,
                                                           right,
                                                           right->location,
                                                           ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT,
                                                           leftEvidence != ZR_NULL ? leftEvidence : left);
            }
            break;
        }

        case ZR_AST_IF_EXPRESSION: {
            semantic_record_constant_if_condition_facts(state, analyzer, node);
            break;
        }

        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            if (primaryExpr->property != ZR_NULL &&
                primaryExpr->property->type == ZR_AST_IDENTIFIER_LITERAL &&
                primaryExpr->members != ZR_NULL &&
                primaryExpr->members->count > 0 &&
                primaryExpr->members->nodes[0] != ZR_NULL &&
                primaryExpr->members->nodes[0]->type == ZR_AST_FUNCTION_CALL) {
                semantic_check_primary_call_with_parser_inference(
                        state, analyzer, node);
            } else if (primaryExpr->property != ZR_NULL &&
                       primaryExpr->members != ZR_NULL &&
                       primaryExpr->members->count > 1 &&
                       primaryExpr->members->nodes[0] != ZR_NULL &&
                       primaryExpr->members->nodes[0]->type == ZR_AST_MEMBER_EXPRESSION &&
                       primaryExpr->members->nodes[1] != ZR_NULL &&
                       primaryExpr->members->nodes[1]->type == ZR_AST_FUNCTION_CALL) {
                semantic_check_primary_call_with_parser_inference(
                        state, analyzer, node);
            }
            break;
        }

        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            if (!ZrParser_Compiler_ValidateExternCallableDecorators(
                        analyzer->compilerState, node)) {
                ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(
                        state, analyzer, node->location);
            }
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (!ZrParser_Compiler_ValidateFfiWrapperDecorators(
                        analyzer->compilerState, node)) {
                ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(
                        state, analyzer, node->location);
            }
            break;

        case ZR_AST_PARAMETER: {
            SZrAstNode *callable =
                    analyzer->compilerState != ZR_NULL
                            ? analyzer->compilerState->currentFunctionNode
                            : ZR_NULL;
            if (callable != ZR_NULL &&
                (callable->type == ZR_AST_EXTERN_FUNCTION_DECLARATION ||
                 callable->type == ZR_AST_EXTERN_DELEGATE_DECLARATION) &&
                !ZrParser_Compiler_ValidateExternParameterDecorators(
                        analyzer->compilerState, node)) {
                ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(
                        state, analyzer, node->location);
            }
            break;
        }

        case ZR_AST_INTERFACE_DECLARATION:
            (void)ZrParser_Variance_PublishInterfaceDiagnostics(
                    analyzer->compilerState, node);
            break;
        
        default:
            break;
    }
    
    if (ZrLanguageServer_SemanticAnalyzer_TypecheckValueChildren(state, analyzer, node)) {
        return;
    }

    // 递归检查子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_EXTERN_BLOCK: {
            SZrExternBlock *externBlock = &node->data.externBlock;
            if (externBlock->declarations != ZR_NULL && externBlock->declarations->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < externBlock->declarations->count; i++) {
                    if (externBlock->declarations->nodes[i] != ZR_NULL) {
                        SZrAstNode *declaration = externBlock->declarations->nodes[i];
                        if (declaration->type == ZR_AST_STRUCT_DECLARATION &&
                            !ZrParser_Compiler_ValidateExternStructDecorators(
                                    analyzer->compilerState,
                                    declaration)) {
                            ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(
                                    state,
                                    analyzer,
                                    declaration->location);
                        }
                        if (declaration->type == ZR_AST_ENUM_DECLARATION &&
                            !ZrParser_Compiler_ValidateExternEnumDecorators(
                                    analyzer->compilerState,
                                    declaration)) {
                            ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(
                                    state,
                                    analyzer,
                                    declaration->location);
                        }
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                              analyzer,
                                                                              declaration);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            SZrTypeEnvironment *savedTypeEnv =
                semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            TZrBool terminated = ZR_FALSE;
            SZrAstNode *terminatingNode = ZR_NULL;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        if (terminated) {
                            semantic_record_reachability_fact(
                                    analyzer,
                                    block->body->nodes[i],
                                    ZrLanguageServer_SemanticAnalyzer_ReachabilityCauseForExitNode(terminatingNode),
                                    terminatingNode);
                        }
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, block->body->nodes[i]);
                        if (!terminated &&
                            ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                                    analyzer,
                                    block->body->nodes[i])) {
                            terminated = ZR_TRUE;
                            terminatingNode = block->body->nodes[i];
                        }
                    }
                }
            }
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, varDecl->pattern);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, varDecl->value);
            semantic_typecheck_register_variable_binding(
                    state,
                    analyzer,
                    ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, varDecl->pattern),
                    varDecl->typeInfo,
                    varDecl->value,
                    node);
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            semantic_typecheck_using_statement(state, analyzer, node);
            break;
        }

        case ZR_AST_COMPILE_TIME_DECLARATION:
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(
                    state, analyzer, node->data.compileTimeDeclaration.declaration);
            break;

        case ZR_AST_LAMBDA_EXPRESSION:
            semantic_typecheck_callable_body(state, analyzer, node,
                    node->data.lambdaExpression.params, ZR_NULL, node->data.lambdaExpression.block);
            break;

        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;
            SZrTypeEnvironment *savedTypeEnv;

            ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
                    state, analyzer, funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL,
                    ZR_NULL, node);
            semantic_typecheck_push_compiler_context(analyzer, ZR_NULL, node, &contextSnapshot);
            savedTypeEnv = semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            semantic_typecheck_register_parameter_bindings(state, analyzer, funcDecl->params);
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->body);
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }
        /* BUG: extern 函数和下方 delegate 仅遍历 params；parser 将 variadic 参数另存于 args，LSP 因而漏掉其非法装饰器诊断。 */
        case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
            SZrExternFunctionDeclaration *funcDecl = &node->data.externFunctionDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;
            SZrTypeEnvironment *savedTypeEnv;

            ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
                    state, analyzer, funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL,
                    ZR_NULL, node);
            semantic_typecheck_push_compiler_context(analyzer, ZR_NULL, node, &contextSnapshot);
            savedTypeEnv = semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            semantic_typecheck_register_parameter_bindings(state, analyzer, funcDecl->params);
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_EXTERN_DELEGATE_DECLARATION: {
            SZrExternDelegateDeclaration *delegateDecl = &node->data.externDelegateDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;

            semantic_typecheck_push_compiler_context(
                    analyzer, ZR_NULL, node, &contextSnapshot);
            if (delegateDecl->params != ZR_NULL && delegateDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < delegateDecl->params->count; i++) {
                    if (delegateDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, delegateDecl->params->nodes[i]);
                    }
                }
            }
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;

            semantic_typecheck_push_compiler_context(analyzer, node, ZR_NULL, &contextSnapshot);
            if (structDecl->members != ZR_NULL && structDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structDecl->members->count; i++) {
                    if (structDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, structDecl->members->nodes[i]);
                    }
                }
            }
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;

            semantic_typecheck_push_compiler_context(analyzer, node, ZR_NULL, &contextSnapshot);
            if (classDecl->members != ZR_NULL && classDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->members->count; i++) {
                    if (classDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                              analyzer,
                                                                              classDecl->members->nodes[i]);
                    }
                }
            }
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_STRUCT_METHOD:
            semantic_typecheck_callable_body(state,
                                             analyzer,
                                             node,
                                             node->data.structMethod.params,
                                             ZR_NULL,
                                             node->data.structMethod.body);
            break;

        case ZR_AST_STRUCT_META_FUNCTION:
            semantic_typecheck_callable_body(state,
                                             analyzer,
                                             node,
                                             node->data.structMetaFunction.params,
                                             ZR_NULL,
                                             node->data.structMetaFunction.body);
            break;

        case ZR_AST_CLASS_METHOD:
            semantic_typecheck_callable_body(state,
                                             analyzer,
                                             node,
                                             node->data.classMethod.params,
                                             ZR_NULL,
                                             node->data.classMethod.body);
            break;

        case ZR_AST_CLASS_META_FUNCTION:
            semantic_typecheck_callable_body(state,
                                             analyzer,
                                             node,
                                             node->data.classMetaFunction.params,
                                             node->data.classMetaFunction.superArgs,
                                             node->data.classMetaFunction.body);
            break;

        case ZR_AST_ENUM_DECLARATION: {
            SZrEnumDeclaration *enumDecl = &node->data.enumDeclaration;
            if (enumDecl->members != ZR_NULL && enumDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < enumDecl->members->count; i++) {
                    if (enumDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, enumDecl->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            SZrTypeInferenceBranchScope branchScope;
            SZrInferredType conditionType;

            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->condition);
            ZrParser_InferredType_Init(state, &conditionType, ZR_VALUE_TYPE_OBJECT);
            (void)semantic_infer_node_type(state,
                                           analyzer,
                                           ifExpr->condition,
                                           &conditionType,
                                           ZR_NULL);
            ZrParser_InferredType_Free(state, &conditionType);
            semantic_record_constant_if_condition_facts(state, analyzer, node);

            (void)ZrParser_TypeInference_PushTrueBranchNumericRangeScope(analyzer->compilerState,
                                                                          ifExpr->condition,
                                                                          &branchScope);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->thenExpr);
            ZrParser_TypeInference_PopBranchScope(analyzer->compilerState, &branchScope);

            (void)ZrParser_TypeInference_PushFalseBranchNumericRangeScope(analyzer->compilerState,
                                                                          ifExpr->condition,
                                                                          &branchScope);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->elseExpr);
            ZrParser_TypeInference_PopBranchScope(analyzer->compilerState, &branchScope);
            (void)ZrParser_TypeInference_TryJoinIfElseNumericAssignments(analyzer->compilerState, node);
            break;
        }

        case ZR_AST_SWITCH_EXPRESSION: {
            semantic_typecheck_switch_expression(state, analyzer, node);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            SZrTypeEnvironment *savedLoopEnv = semantic_typecheck_push_loop_range_scope(state, analyzer, node, ZR_NULL);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, whileLoop->cond);
            ZrLanguageServer_SemanticAnalyzer_RecordConstantLoopConditionFacts(state,
                                                                               analyzer,
                                                                               whileLoop->cond,
                                                                               whileLoop->block);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, whileLoop->block);
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedLoopEnv);
            semantic_typecheck_join_loop_ranges(state, analyzer, node);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            if (forLoop->init != ZR_NULL &&
                forLoop->init->type == ZR_AST_VARIABLE_DECLARATION) {
                SZrTypeEnvironment *savedTypeEnv =
                    semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);

                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->init);
                SZrTypeEnvironment *savedLoopEnv = semantic_typecheck_push_loop_range_scope(state, analyzer, node, ZR_NULL);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->cond);
                ZrLanguageServer_SemanticAnalyzer_RecordConstantLoopConditionFacts(state,
                                                                                   analyzer,
                                                                                   forLoop->cond,
                                                                                   forLoop->block);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->step);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->block);
                semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedLoopEnv);
                semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            } else {
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->init);
                SZrTypeEnvironment *savedLoopEnv = semantic_typecheck_push_loop_range_scope(state, analyzer, node, ZR_NULL);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->cond);
                ZrLanguageServer_SemanticAnalyzer_RecordConstantLoopConditionFacts(state,
                                                                                   analyzer,
                                                                                   forLoop->cond,
                                                                                   forLoop->block);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->step);
                ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->block);
                semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedLoopEnv);
            }
            semantic_typecheck_join_loop_ranges(state, analyzer, node);
            break;
        }

        case ZR_AST_FOREACH_LOOP: {
            SZrForeachLoop *foreachLoop = &node->data.foreachLoop;
            SZrTypeEnvironment *savedTypeEnv =
                semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            SZrTypeEnvironment *iterationEnv = analyzer->compilerState->typeEnv;
            SZrTypeEnvironment *savedLoopEnv;

            /* The iterable is evaluated once, before any loop-carried writes. */
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, foreachLoop->expr);
            semantic_typecheck_register_foreach_binding(state,
                                                        analyzer,
                                                        foreachLoop,
                                                        foreachLoop->pattern != ZR_NULL
                                                            ? foreachLoop->pattern->location
                                                            : node->location);
            savedLoopEnv = semantic_typecheck_push_loop_range_scope(state, analyzer, node, iterationEnv);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, foreachLoop->block);
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedLoopEnv);
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            semantic_typecheck_join_loop_ranges(state, analyzer, node);
            break;
        }
        
        default:
            break;
    }
}

// 创建语义分析器
