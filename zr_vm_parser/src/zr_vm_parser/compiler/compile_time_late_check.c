#include "compile_time_executor_internal.h"

static TZrBool compile_time_execute_late_check_node(
        SZrCompilerState *cs,
        SZrAstNode *node);

static TZrBool compile_time_execute_late_check_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes) {
    for (TZrSize index = 0; nodes != ZR_NULL && index < nodes->count; index++) {
        if (!compile_time_execute_late_check_node(cs, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool compile_time_execute_late_check_node(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrCompileTimeDeclaration *declaration;
    SZrAstNode *selectedBranch;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return cs != ZR_NULL;
    }
    if (node->type == ZR_AST_SCRIPT) {
        return compile_time_execute_late_check_array(
                cs, node->data.script.statements);
    }
    if (node->type == ZR_AST_BLOCK) {
        return compile_time_execute_late_check_array(
                cs, node->data.block.body);
    }
    if (node->type != ZR_AST_COMPILE_TIME_DECLARATION) {
        return ZR_TRUE;
    }

    declaration = &node->data.compileTimeDeclaration;
    if (declaration->isConditionalPruning) {
        selectedBranch = declaration->selectedBranch;
        return selectedBranch == ZR_NULL ||
               compile_time_execute_late_check_node(cs, selectedBranch);
    }
    if (declaration->declarationType == ZR_COMPILE_TIME_FUNCTION) {
        return ZR_TRUE;
    }
    return ZrParser_CompileTimeDeclaration_Execute(cs, node) &&
           !cs->hasCompileTimeError && !cs->hasError && !cs->hasFatalError;
}

TZrBool ZrParser_CompileTime_ExecuteLateChecksInCompilerState(
        SZrCompilerState *cs,
        SZrAstNode *ast) {
    if (cs == ZR_NULL || ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        cs->compilePhase != ZR_PARSER_COMPILE_PHASE_LATE_CHECK) {
        return ZR_FALSE;
    }
    return compile_time_execute_late_check_node(cs, ast);
}
