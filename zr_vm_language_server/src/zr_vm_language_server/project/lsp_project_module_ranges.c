#include "project/lsp_project_internal.h"

/* 项目源模块的导航/诊断入口优先指向当前 AST 中与索引模块键一致的声明。 */
/* AST 缺失或编辑中模块名暂不一致时退回文件首位，避免跳到过期声明。 */
SZrFileRange ZrLanguageServer_LspProject_GetSourceModuleEntryRange(
        SZrLspContext *context,
        const SZrLspProjectFileRecord *record) {
    SZrFilePosition origin = ZrParser_FilePosition_Create(0, 1, 1);
    SZrFileRange fallback = ZrParser_FileRange_Create(
            origin, origin, record != ZR_NULL ? record->uri : ZR_NULL);
    SZrAstNode *ast;
    SZrAstNode *moduleDeclaration;
    SZrAstNode *moduleName;
    SZrFileRange range;

    if (context == ZR_NULL || context->parser == ZR_NULL ||
        record == ZR_NULL || record->uri == ZR_NULL) {
        return fallback;
    }

    ast = ZrLanguageServer_IncrementalParser_GetAST(
            context->parser, record->uri);
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        return fallback;
    }

    moduleDeclaration = ast->data.script.moduleName;
    if (moduleDeclaration == ZR_NULL ||
        moduleDeclaration->type != ZR_AST_MODULE_DECLARATION) {
        return fallback;
    }

    moduleName = moduleDeclaration->data.moduleDeclaration.name;
    if (moduleName == ZR_NULL || moduleName->type != ZR_AST_STRING_LITERAL ||
        moduleName->data.stringLiteral.value == ZR_NULL ||
        record->moduleName == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(
                moduleName->data.stringLiteral.value, record->moduleName)) {
        return fallback;
    }

    range = moduleName->location;
    if (range.source == ZR_NULL) {
        range.source = record->uri;
    }
    return range;
}
