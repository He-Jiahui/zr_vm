#include "compiler_function_body.h"

void compiler_compile_function_body(SZrCompilerState *cs, SZrAstNode *node) {
    SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
    TZrBool functionContainsYield = compiler_iterator_function_contains_yield(
            funcDecl->body);

    // 2. 编译函数体
    if (funcDecl->body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, funcDecl->body);
        if (!cs->hasError) {
            compiler_validate_out_parameter_definite_assignment(cs,
                                                                funcDecl->params,
                                                                funcDecl->body,
                                                                node->location);
        }
    }

    if (!cs->hasError && functionContainsYield &&
        !compiler_semantic_ir_record_iterator_complete(cs, node->location)) {
        ZrParser_Compiler_Error(
                cs,
                "Failed to record iterator completion semantic fact",
                node->location);
    }
}
