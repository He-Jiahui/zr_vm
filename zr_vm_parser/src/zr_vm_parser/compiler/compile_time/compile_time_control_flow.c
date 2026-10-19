#include "compiler/compile_time/compile_time_control_flow.h"

#include "compiler/compile_time/compile_time_executor_internal.h"
#include "compiler/compile_time/comptime_runtime_contract.h"

#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"

#include <string.h>

static void comptime_flow_reset(SZrCompilerState *cs,
                                SZrCompileTimeFlowResult *result) {
    if (result == ZR_NULL) {
        return;
    }
    if (result->hasValueRoot && cs != ZR_NULL) {
        ZrCore_GcRootHandle_Release(cs->state, &result->valueRoot);
        result->hasValueRoot = ZR_FALSE;
    }
    result->status = ZR_PARSER_COMPTIME_FLOW_NORMAL;
    ZrCore_Value_ResetAsNull(&result->value);
}

static TZrBool comptime_flow_root_value(
        SZrCompilerState *cs,
        const SZrTypeValue *value,
        SZrGcRootHandle *root,
        TZrBool *hasRoot) {
    if (cs == ZR_NULL || value == ZR_NULL || root == ZR_NULL || hasRoot == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!value->isGarbageCollectable || value->value.object == ZR_NULL) {
        return ZR_TRUE;
    }
    if (*hasRoot) {
        return ZrCore_GcRootHandle_Update(
                cs->state, root, value->value.object);
    }
    if (!ZrCore_GcRootHandle_Create(
                cs->state, value->value.object, root)) {
        return ZR_FALSE;
    }
    *hasRoot = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool comptime_flow_resolve_value(
        SZrCompilerState *cs,
        SZrTypeValue *value,
        const SZrGcRootHandle *root,
        TZrBool hasRoot) {
    SZrRawObject *resolved;

    if (!hasRoot) {
        return ZR_TRUE;
    }
    resolved = ZR_NULL;
    if (cs == ZR_NULL || value == ZR_NULL || root == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(cs->state, root, &resolved) ||
        resolved == ZR_NULL) {
        return ZR_FALSE;
    }
    value->value.object = resolved;
    return ZR_TRUE;
}

static void comptime_flow_release_root(
        SZrCompilerState *cs,
        SZrGcRootHandle *root,
        TZrBool *hasRoot) {
    if (cs != ZR_NULL && root != ZR_NULL && hasRoot != ZR_NULL && *hasRoot) {
        ZrCore_GcRootHandle_Release(cs->state, root);
        *hasRoot = ZR_FALSE;
    }
}

static EZrCompileTimeFlowStatus comptime_flow_error(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const TZrChar *message,
        SZrCompileTimeFlowResult *result) {
    if (cs != ZR_NULL && message != ZR_NULL) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                message,
                node != ZR_NULL ? node->location
                                : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    }
    if (result != ZR_NULL) {
        comptime_flow_release_root(cs, &result->valueRoot, &result->hasValueRoot);
        ZrCore_Value_ResetAsNull(&result->value);
        result->status = ZR_PARSER_COMPTIME_FLOW_ERROR;
    }
    return ZR_PARSER_COMPTIME_FLOW_ERROR;
}

static EZrCompileTimeFlowStatus comptime_flow_copy_status(
        SZrCompilerState *cs,
        EZrCompileTimeFlowStatus status,
        SZrCompileTimeFlowResult *source,
        SZrCompileTimeFlowResult *result) {
    if (result != ZR_NULL) {
        comptime_flow_release_root(cs, &result->valueRoot, &result->hasValueRoot);
        result->status = status;
        if (source != ZR_NULL) {
            result->value = source->value;
            if (source->hasValueRoot) {
                /* Flow results are stack-owned temporaries.  A caller that
                 * propagates an abrupt result takes over its root handle. */
                result->valueRoot = source->valueRoot;
                result->hasValueRoot = source->hasValueRoot;
                source->hasValueRoot = ZR_FALSE;
            } else if (!comptime_flow_root_value(
                               cs,
                               &result->value,
                               &result->valueRoot,
                               &result->hasValueRoot)) {
                result->status = ZR_PARSER_COMPTIME_FLOW_ERROR;
                return result->status;
            }
        }
    }
    return status;
}

static TZrBool comptime_flow_make_string_key(
        SZrCompilerState *cs,
        SZrString *name,
        SZrTypeValue *key) {
    if (cs == ZR_NULL || name == ZR_NULL || key == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static TZrBool comptime_flow_bind_pattern(
        SZrCompilerState *cs,
        SZrCompileTimeFrame *frame,
        SZrAstNode *pattern,
        const SZrTypeValue *value,
        TZrBool isConst) {
    if (cs == ZR_NULL || frame == ZR_NULL || pattern == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ct_frame_declare_with_context(
                cs,
                frame,
                pattern->data.identifier.name,
                pattern->data.identifier.syntaxContext,
                value,
                isConst);
    }

    if (pattern->type == ZR_AST_KEY_VALUE_PAIR) {
        /* Pattern pairs carry binding in key and source field in value. */
        return comptime_flow_bind_pattern(
                cs, frame, pattern->data.keyValuePair.key, value, isConst);
    }

    if (pattern->type == ZR_AST_UNPACK_LITERAL) {
        return comptime_flow_bind_pattern(
                cs, frame, pattern->data.unpackLiteral.element, value, isConst);
    }

    if (pattern->type == ZR_AST_DESTRUCTURING_ARRAY) {
        SZrAstNodeArray *keys = pattern->data.destructuringArray.keys;
        SZrObject *sourceObject;
        if ((value->type != ZR_VALUE_TYPE_ARRAY &&
             value->type != ZR_VALUE_TYPE_OBJECT) ||
            value->value.object == ZR_NULL) {
            return comptime_flow_error(
                           cs,
                           pattern,
                           "Compile-time array destructuring requires an array or object value",
                           ZR_NULL) != ZR_PARSER_COMPTIME_FLOW_ERROR;
        }
        sourceObject = ZR_CAST_OBJECT(cs->state, value->value.object);
        for (TZrSize index = 0U;
             keys != ZR_NULL && index < keys->count;
             index++) {
            SZrAstNode *keyNode = keys->nodes[index];
            SZrTypeValue key;
            SZrTypeValue element;
            const SZrTypeValue *resolved;
            if (keyNode == ZR_NULL ||
                keyNode->type != ZR_AST_IDENTIFIER_LITERAL) {
                return ZR_FALSE;
            }
            /* Array destructuring addresses the source by numeric position. */
            ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)index);
            resolved = ZrCore_Object_GetValue(cs->state, sourceObject, &key);
            if (resolved == ZR_NULL) {
                ZrCore_Value_ResetAsNull(&element);
            } else {
                element = *resolved;
            }
            if (!comptime_flow_bind_pattern(cs, frame, keyNode, &element, isConst)) {
                return ZR_FALSE;
            }
        }
        return ZR_TRUE;
    }

    if (pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
        SZrAstNodeArray *keys = pattern->data.destructuringObject.keys;
        SZrObject *sourceObject;
        if ((value->type != ZR_VALUE_TYPE_OBJECT &&
             value->type != ZR_VALUE_TYPE_ARRAY) ||
            value->value.object == ZR_NULL) {
            return ZR_FALSE;
        }
        sourceObject = ZR_CAST_OBJECT(cs->state, value->value.object);
        for (TZrSize index = 0U;
             keys != ZR_NULL && index < keys->count;
             index++) {
            SZrAstNode *entry = keys->nodes[index];
            SZrAstNode *keyNode = entry;
            SZrAstNode *bindingNode = entry;
            SZrTypeValue key;
            SZrTypeValue field;
            const SZrTypeValue *resolved;
            if (entry != ZR_NULL && entry->type == ZR_AST_KEY_VALUE_PAIR) {
                /* Destructuring syntax stores {binding: sourceField}. */
                keyNode = entry->data.keyValuePair.value;
                bindingNode = entry->data.keyValuePair.key;
            }
            if (keyNode == ZR_NULL ||
                keyNode->type != ZR_AST_IDENTIFIER_LITERAL ||
                !comptime_flow_make_string_key(
                        cs, keyNode->data.identifier.name, &key)) {
                return ZR_FALSE;
            }
            resolved = ZrCore_Object_GetValue(cs->state, sourceObject, &key);
            if (resolved == ZR_NULL) {
                ZrCore_Value_ResetAsNull(&field);
            } else {
                field = *resolved;
            }
            if (!comptime_flow_bind_pattern(cs, frame, bindingNode, &field, isConst)) {
                return ZR_FALSE;
            }
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrAstNode *comptime_flow_for_condition(SZrAstNode *node) {
    if (node != ZR_NULL && node->type == ZR_AST_EXPRESSION_STATEMENT) {
        return node->data.expressionStatement.expr;
    }
    return node;
}

static EZrCompileTimeFlowStatus comptime_flow_execute_while(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrCompileTimeFlowResult *result) {
    SZrWhileLoop *loop = &node->data.whileLoop;
    SZrCompileTimeFrame loopFrame;
    SZrCompileTimeFrame *activeFrame = frame;

    ct_frame_init(cs, &loopFrame, frame);
    activeFrame = &loopFrame;
    for (;;) {
        SZrTypeValue condition;
        SZrGcRootHandle conditionRoot = {0};
        TZrBool hasConditionRoot = ZR_FALSE;
        EZrCompileTimeFlowStatus status;
        SZrCompileTimeFlowResult bodyResult = {0};
        if (!evaluate_compile_time_expression_internal(
                    cs, loop->cond, activeFrame, &condition)) {
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_error(cs, node, ZR_NULL, result);
        }
        if (!comptime_flow_root_value(
                    cs, &condition, &conditionRoot, &hasConditionRoot) ||
            !comptime_flow_resolve_value(
                    cs, &condition, &conditionRoot, hasConditionRoot)) {
            comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_error(
                    cs, node,
                    "Compile-time while condition could not be rooted",
                    result);
        }
        if (!ct_truthy(&condition)) {
            comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
            ct_frame_free(cs, &loopFrame);
            return ZR_PARSER_COMPTIME_FLOW_NORMAL;
        }
        comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
        status = ZrParser_CompileTimeFlow_ExecuteStatement(
                cs, loop->block, activeFrame, ZR_TRUE, &bodyResult);
        if (status == ZR_PARSER_COMPTIME_FLOW_BREAK) {
            ct_frame_free(cs, &loopFrame);
            return ZR_PARSER_COMPTIME_FLOW_NORMAL;
        }
        if (status == ZR_PARSER_COMPTIME_FLOW_CONTINUE) {
            continue;
        }
        if (status != ZR_PARSER_COMPTIME_FLOW_NORMAL) {
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_copy_status(cs, status, &bodyResult, result);
        }
    }
}

static EZrCompileTimeFlowStatus comptime_flow_execute_for(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrCompileTimeFlowResult *result) {
    SZrForLoop *loop = &node->data.forLoop;
    SZrCompileTimeFrame loopFrame;
    SZrCompileTimeFrame *activeFrame = frame;
        SZrCompileTimeFlowResult stepResult = {0};
    EZrCompileTimeFlowStatus status;

    ct_frame_init(cs, &loopFrame, frame);
    activeFrame = &loopFrame;
    if (loop->init != ZR_NULL) {
        status = ZrParser_CompileTimeFlow_ExecuteStatement(
                cs, loop->init, activeFrame, ZR_FALSE, &stepResult);
        if (status != ZR_PARSER_COMPTIME_FLOW_NORMAL) {
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_copy_status(cs, status, &stepResult, result);
        }
    }

    for (;;) {
        SZrTypeValue condition;
        SZrGcRootHandle conditionRoot = {0};
        TZrBool hasConditionRoot = ZR_FALSE;
        SZrCompileTimeFlowResult bodyResult = {0};
        SZrAstNode *conditionNode = comptime_flow_for_condition(loop->cond);
        if (conditionNode != ZR_NULL &&
            !evaluate_compile_time_expression_internal(
                    cs, conditionNode, activeFrame, &condition)) {
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_error(cs, node, ZR_NULL, result);
        }
        if (conditionNode != ZR_NULL &&
            (!comptime_flow_root_value(
                     cs, &condition, &conditionRoot, &hasConditionRoot) ||
             !comptime_flow_resolve_value(
                     cs, &condition, &conditionRoot, hasConditionRoot))) {
            comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_error(
                    cs, node,
                    "Compile-time for condition could not be rooted",
                    result);
        }
        if (conditionNode != ZR_NULL && !ct_truthy(&condition)) {
            comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
            break;
        }
        comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);

        status = ZrParser_CompileTimeFlow_ExecuteStatement(
                cs, loop->block, activeFrame, ZR_TRUE, &bodyResult);
        if (status == ZR_PARSER_COMPTIME_FLOW_RETURN ||
            status == ZR_PARSER_COMPTIME_FLOW_ERROR) {
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_copy_status(cs, status, &bodyResult, result);
        }
        if (status == ZR_PARSER_COMPTIME_FLOW_BREAK) {
            break;
        }
        if (loop->step != ZR_NULL) {
            if (!evaluate_compile_time_expression_internal(
                        cs, loop->step, activeFrame, &stepResult.value)) {
                ct_frame_free(cs, &loopFrame);
                return comptime_flow_error(cs, node, ZR_NULL, result);
            }
        }
        /* CONTINUE reaches the step exactly like normal completion. */
    }
    ct_frame_free(cs, &loopFrame);
    return ZR_PARSER_COMPTIME_FLOW_NORMAL;
}

static EZrCompileTimeFlowStatus comptime_flow_execute_foreach(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrCompileTimeFlowResult *result) {
    SZrForeachLoop *loop = &node->data.foreachLoop;
    SZrTypeValue iterable;
    SZrTypeValue iterator;
    SZrTypeValue moved;
    SZrTypeValue current;
    SZrGcRootHandle iterableRoot = {0};
    SZrGcRootHandle iteratorRoot = {0};
    SZrGcRootHandle currentRoot = {0};
    TZrBool hasIterableRoot = ZR_FALSE;
    TZrBool hasIteratorRoot = ZR_FALSE;
    TZrBool hasCurrentRoot = ZR_FALSE;
    SZrCompileTimeFrame loopFrame;
    EZrCompileTimeFlowStatus status;

    if (!evaluate_compile_time_expression_internal(cs, loop->expr, frame, &iterable) ||
        !comptime_flow_root_value(
                cs, &iterable, &iterableRoot, &hasIterableRoot) ||
        !comptime_flow_resolve_value(
                cs, &iterable, &iterableRoot, hasIterableRoot) ||
        iterable.type != ZR_VALUE_TYPE_ARRAY ||
        !ZrCore_Object_IterInit(cs->state, &iterable, &iterator) ||
        !comptime_flow_root_value(
                cs, &iterator, &iteratorRoot, &hasIteratorRoot)) {
        comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
        comptime_flow_release_root(cs, &iteratorRoot, &hasIteratorRoot);
        return comptime_flow_error(
                cs, node,
                "Compile-time foreach requires a built-in array iterable",
                result);
    }
    ZrCore_Value_ResetAsNull(&moved);
    ZrCore_Value_ResetAsNull(&current);
    ct_frame_init(cs, &loopFrame, frame);
    for (;;) {
        SZrCompileTimeFlowResult bodyResult = {0};
        if (!ZrParser_ComptimeRuntime_Consume(
                    cs,
                    ZR_PARSER_COMPTIME_BUDGET_FUEL,
                    1U,
                    node->location)) {
            comptime_flow_release_root(cs, &currentRoot, &hasCurrentRoot);
            comptime_flow_release_root(cs, &iteratorRoot, &hasIteratorRoot);
            comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_error(cs, node, ZR_NULL, result);
        }
        if (!comptime_flow_resolve_value(
                    cs, &iterator, &iteratorRoot, hasIteratorRoot) ||
            !ZrCore_Object_IterMoveNext(cs->state, &iterator, &moved) ||
            !comptime_flow_resolve_value(
                    cs, &iterator, &iteratorRoot, hasIteratorRoot)) {
            comptime_flow_release_root(cs, &currentRoot, &hasCurrentRoot);
            comptime_flow_release_root(cs, &iteratorRoot, &hasIteratorRoot);
            comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_error(
                    cs, node,
                    "Compile-time foreach iterator failed to advance", result);
        }
        if (!ct_truthy(&moved)) {
            comptime_flow_release_root(cs, &iteratorRoot, &hasIteratorRoot);
            comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
            break;
        }
        ZrCore_Value_ResetAsNull(&current);
        if (!ZrCore_Object_IterCurrent(cs->state, &iterator, &current) ||
            !comptime_flow_root_value(
                    cs, &current, &currentRoot, &hasCurrentRoot) ||
            !comptime_flow_resolve_value(
                    cs, &current, &currentRoot, hasCurrentRoot) ||
            !comptime_flow_bind_pattern(
                    cs, &loopFrame, loop->pattern, &current, loop->isConst)) {
            comptime_flow_release_root(cs, &currentRoot, &hasCurrentRoot);
            comptime_flow_release_root(cs, &iteratorRoot, &hasIteratorRoot);
            comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_error(
                    cs, node,
                    "Compile-time foreach pattern binding failed", result);
        }
        comptime_flow_release_root(cs, &currentRoot, &hasCurrentRoot);
        status = ZrParser_CompileTimeFlow_ExecuteStatement(
                cs, loop->block, &loopFrame, ZR_TRUE, &bodyResult);
        if (status == ZR_PARSER_COMPTIME_FLOW_BREAK) {
            comptime_flow_release_root(cs, &iteratorRoot, &hasIteratorRoot);
            comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
            break;
        }
        if (status == ZR_PARSER_COMPTIME_FLOW_CONTINUE ||
            status == ZR_PARSER_COMPTIME_FLOW_NORMAL) {
            continue;
        }
        if (status == ZR_PARSER_COMPTIME_FLOW_RETURN ||
            status == ZR_PARSER_COMPTIME_FLOW_ERROR) {
            comptime_flow_release_root(cs, &iteratorRoot, &hasIteratorRoot);
            comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
            ct_frame_free(cs, &loopFrame);
            return comptime_flow_copy_status(cs, status, &bodyResult, result);
        }
        comptime_flow_release_root(cs, &iteratorRoot, &hasIteratorRoot);
        comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
        ct_frame_free(cs, &loopFrame);
        return comptime_flow_copy_status(cs, status, &bodyResult, result);
    }
    comptime_flow_release_root(cs, &currentRoot, &hasCurrentRoot);
    comptime_flow_release_root(cs, &iterableRoot, &hasIterableRoot);
    ct_frame_free(cs, &loopFrame);
    return ZR_PARSER_COMPTIME_FLOW_NORMAL;
}

static EZrCompileTimeFlowStatus comptime_flow_execute_switch(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        TZrBool inLoop,
        SZrCompileTimeFlowResult *result) {
    SZrSwitchExpression *switchExpr = &node->data.switchExpression;
    SZrTypeValue switchValue;
    SZrGcRootHandle switchRoot = {0};
    SZrGcRootHandle caseRoot = {0};
    TZrBool hasSwitchRoot = ZR_FALSE;
    TZrBool hasCaseRoot = ZR_FALSE;
    SZrAstNode *selectedBlock = ZR_NULL;
    if (!evaluate_compile_time_expression_internal(
                cs, switchExpr->expr, frame, &switchValue) ||
        !comptime_flow_root_value(
                cs, &switchValue, &switchRoot, &hasSwitchRoot) ||
        !comptime_flow_resolve_value(
                cs, &switchValue, &switchRoot, hasSwitchRoot)) {
        comptime_flow_release_root(cs, &switchRoot, &hasSwitchRoot);
        return comptime_flow_error(cs, node, ZR_NULL, result);
    }
    for (TZrSize index = 0U;
         switchExpr->cases != ZR_NULL && index < switchExpr->cases->count;
         index++) {
        SZrAstNode *caseNode = switchExpr->cases->nodes[index];
        SZrTypeValue caseValue;
        if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE ||
            !evaluate_compile_time_expression_internal(
                    cs, caseNode->data.switchCase.value, frame, &caseValue)) {
            comptime_flow_release_root(cs, &caseRoot, &hasCaseRoot);
            comptime_flow_release_root(cs, &switchRoot, &hasSwitchRoot);
            return comptime_flow_error(
                    cs,
                    caseNode != ZR_NULL ? caseNode : node,
                    ZR_NULL,
                    result);
        }
        if (!comptime_flow_root_value(
                    cs, &caseValue, &caseRoot, &hasCaseRoot) ||
            !comptime_flow_resolve_value(
                    cs, &switchValue, &switchRoot, hasSwitchRoot) ||
            !comptime_flow_resolve_value(
                    cs, &caseValue, &caseRoot, hasCaseRoot)) {
            comptime_flow_release_root(cs, &caseRoot, &hasCaseRoot);
            comptime_flow_release_root(cs, &switchRoot, &hasSwitchRoot);
            return comptime_flow_error(
                    cs, node,
                    "Compile-time switch value could not be rooted",
                    result);
        }
        if (ZrCore_Value_Equal(cs->state, &switchValue, &caseValue)) {
            selectedBlock = caseNode->data.switchCase.block;
            comptime_flow_release_root(cs, &caseRoot, &hasCaseRoot);
            break;
        }
        comptime_flow_release_root(cs, &caseRoot, &hasCaseRoot);
    }
    if (selectedBlock == ZR_NULL && switchExpr->defaultCase != ZR_NULL) {
        selectedBlock = switchExpr->defaultCase->data.switchDefault.block;
    }
    if (selectedBlock == ZR_NULL) {
        comptime_flow_release_root(cs, &switchRoot, &hasSwitchRoot);
        return ZR_PARSER_COMPTIME_FLOW_NORMAL;
    }
    comptime_flow_release_root(cs, &switchRoot, &hasSwitchRoot);
    return ZrParser_CompileTimeFlow_ExecuteStatement(
            cs, selectedBlock, frame, inLoop, result);
}

EZrCompileTimeFlowStatus ZrParser_CompileTimeFlow_ExecuteStatement(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        TZrBool inLoop,
        SZrCompileTimeFlowResult *result) {
    SZrCompileTimeFlowResult localResult = {0};

    if (result == ZR_NULL) {
        result = &localResult;
    }
    comptime_flow_reset(cs, result);
    if (cs == ZR_NULL || node == ZR_NULL) {
        return comptime_flow_error(cs, node, "Invalid compile-time statement", result);
    }
    if (!ZrParser_ComptimeRuntime_Consume(
                cs, ZR_PARSER_COMPTIME_BUDGET_FUEL, 1U, node->location)) {
        if (result != ZR_NULL) {
            result->status = ZR_PARSER_COMPTIME_FLOW_ERROR;
        }
        return ZR_PARSER_COMPTIME_FLOW_ERROR;
    }

    switch (node->type) {
        case ZR_AST_BLOCK:
            return ZrParser_CompileTimeFlow_ExecuteBlock(
                    cs, node, frame, inLoop, result);
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *statement = &node->data.returnStatement;
            if (statement->expr != ZR_NULL &&
                !evaluate_compile_time_expression_internal(
                        cs, statement->expr, frame, &result->value)) {
                return comptime_flow_error(cs, node, ZR_NULL, result);
            }
            if (statement->expr != ZR_NULL &&
                !comptime_flow_root_value(
                        cs,
                        &result->value,
                        &result->valueRoot,
                        &result->hasValueRoot)) {
                return comptime_flow_error(
                        cs,
                        node,
                        "Compile-time return value could not be rooted",
                        result);
            }
            result->status = ZR_PARSER_COMPTIME_FLOW_RETURN;
            return result->status;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *declaration = &node->data.variableDeclaration;
            SZrTypeValue value;
            if (frame == ZR_NULL) {
                return register_compile_time_variable_declaration(
                               cs, node, node->location)
                               ? ZR_PARSER_COMPTIME_FLOW_NORMAL
                               : ZR_PARSER_COMPTIME_FLOW_ERROR;
            }
            if (declaration->value != ZR_NULL &&
                !evaluate_compile_time_expression_internal(
                        cs, declaration->value, frame, &value)) {
                return comptime_flow_error(cs, node, ZR_NULL, result);
            }
            if (declaration->value == ZR_NULL) {
                ZrCore_Value_ResetAsNull(&value);
            }
            if (!comptime_flow_bind_pattern(
                    cs, frame, declaration->pattern, &value, declaration->isConst)) {
                return comptime_flow_error(
                        cs, node,
                        "Compile-time variable pattern binding failed", result);
            }
            return ZR_PARSER_COMPTIME_FLOW_NORMAL;
        }
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *expression = &node->data.ifExpression;
            SZrTypeValue condition;
            SZrGcRootHandle conditionRoot = {0};
            TZrBool hasConditionRoot = ZR_FALSE;
            SZrAstNode *branch;
            if (!evaluate_compile_time_expression_internal(
                        cs, expression->condition, frame, &condition)) {
                return comptime_flow_error(cs, node, ZR_NULL, result);
            }
            if (!comptime_flow_root_value(
                        cs, &condition, &conditionRoot, &hasConditionRoot) ||
                !comptime_flow_resolve_value(
                        cs, &condition, &conditionRoot, hasConditionRoot)) {
                comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
                return comptime_flow_error(
                        cs,
                        node,
                        "Compile-time if condition could not be rooted",
                        result);
            }
            branch = ct_truthy(&condition)
                             ? expression->thenExpr
                             : expression->elseExpr;
            if (branch == ZR_NULL) {
                comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
                return ZR_PARSER_COMPTIME_FLOW_NORMAL;
            }
            if (branch->type == ZR_AST_BLOCK ||
                branch->type == ZR_AST_RETURN_STATEMENT ||
                branch->type == ZR_AST_IF_EXPRESSION ||
                branch->type == ZR_AST_VARIABLE_DECLARATION ||
                branch->type == ZR_AST_FUNCTION_DECLARATION ||
                branch->type == ZR_AST_EXPRESSION_STATEMENT ||
                branch->type == ZR_AST_WHILE_LOOP ||
                branch->type == ZR_AST_FOR_LOOP ||
                branch->type == ZR_AST_FOREACH_LOOP ||
                branch->type == ZR_AST_SWITCH_EXPRESSION ||
                branch->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
                EZrCompileTimeFlowStatus branchStatus =
                        ZrParser_CompileTimeFlow_ExecuteStatement(
                                cs, branch, frame, inLoop, result);
                comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
                return branchStatus;
            }
            {
                TZrBool evaluated = evaluate_compile_time_expression_internal(
                        cs, branch, frame, &result->value);
                comptime_flow_release_root(cs, &conditionRoot, &hasConditionRoot);
                return evaluated
                               ? ZR_PARSER_COMPTIME_FLOW_NORMAL
                               : comptime_flow_error(cs, branch, ZR_NULL, result);
            }
        }
        case ZR_AST_WHILE_LOOP:
            return comptime_flow_execute_while(cs, node, frame, result);
        case ZR_AST_FOR_LOOP:
            return comptime_flow_execute_for(cs, node, frame, result);
        case ZR_AST_FOREACH_LOOP:
            return comptime_flow_execute_foreach(cs, node, frame, result);
        case ZR_AST_SWITCH_EXPRESSION:
            return comptime_flow_execute_switch(cs, node, frame, inLoop, result);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            if (node->data.breakContinueStatement.expr != ZR_NULL) {
                return comptime_flow_error(
                        cs, node,
                        "Compile-time break/continue does not support a return value",
                        result);
            }
            if (!inLoop) {
                return comptime_flow_error(
                        cs,
                        node,
                        node->data.breakContinueStatement.isBreak
                                ? "Compile-time break is outside a loop"
                                : "Compile-time continue is outside a loop",
                        result);
            }
            result->status = node->data.breakContinueStatement.isBreak
                                     ? ZR_PARSER_COMPTIME_FLOW_BREAK
                                     : ZR_PARSER_COMPTIME_FLOW_CONTINUE;
            return result->status;
        case ZR_AST_FUNCTION_DECLARATION:
            if (frame == ZR_NULL) {
                return register_compile_time_function_declaration(
                               cs, node, node->location)
                               ? ZR_PARSER_COMPTIME_FLOW_NORMAL
                               : ZR_PARSER_COMPTIME_FLOW_ERROR;
            }
            return comptime_flow_error(
                    cs,
                    node,
                    "Nested compile-time function declarations are not supported in local frames",
                    result);
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return ZrParser_CompileTimeDeclaration_Execute(cs, node)
                           ? ZR_PARSER_COMPTIME_FLOW_NORMAL
                           : ZR_PARSER_COMPTIME_FLOW_ERROR;
        default:
            return evaluate_compile_time_expression_internal(
                           cs, node, frame, &result->value)
                           ? ZR_PARSER_COMPTIME_FLOW_NORMAL
                           : comptime_flow_error(cs, node, ZR_NULL, result);
    }
}

EZrCompileTimeFlowStatus ZrParser_CompileTimeFlow_ExecuteBlock(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        TZrBool inLoop,
        SZrCompileTimeFlowResult *result) {
    SZrCompileTimeFlowResult localResult = {0};
    SZrCompileTimeFrame blockFrame;
    SZrCompileTimeFrame *activeFrame = frame;
    SZrBlock *block;

    if (result == ZR_NULL) {
        result = &localResult;
    }
    comptime_flow_reset(cs, result);
    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_BLOCK) {
        return comptime_flow_error(cs, node, "Invalid compile-time block", result);
    }
    if (frame != ZR_NULL) {
        ct_frame_init(cs, &blockFrame, frame);
        activeFrame = &blockFrame;
    }
    block = &node->data.block;
    for (TZrSize index = 0U;
         block->body != ZR_NULL && index < block->body->count;
         index++) {
        SZrCompileTimeFlowResult statementResult = {0};
        EZrCompileTimeFlowStatus status =
                ZrParser_CompileTimeFlow_ExecuteStatement(
                        cs,
                        block->body->nodes[index],
                        activeFrame,
                        inLoop,
                        &statementResult);
        if (status != ZR_PARSER_COMPTIME_FLOW_NORMAL) {
            if (frame != ZR_NULL) {
                ct_frame_free(cs, &blockFrame);
            }
            return comptime_flow_copy_status(cs, status, &statementResult, result);
        }
    }
    if (frame != ZR_NULL) {
        ct_frame_free(cs, &blockFrame);
    }
    return ZR_PARSER_COMPTIME_FLOW_NORMAL;
}
