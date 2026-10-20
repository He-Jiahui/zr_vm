#include "compiler/tool/compile_tool_execution_scope.h"

#include "compiler/compile_time/compile_time_executor_internal.h"
#include "compiler/compile_time/compile_time_import.h"
#include "compiler/tool/compile_tool_binding.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

TZrBool ZrParser_CompileToolExecutionScope_DeclareFunctionShadow(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_FUNCTION_DECLARATION) {
        SZrIdentifier *name = node->data.functionDeclaration.name;
        const SZrCompileToolBinding *existing;

        if (name == ZR_NULL || name->name == ZR_NULL) {
            return ZR_TRUE;
        }
        existing = ZrParser_CompileToolBinding_Resolve(cs, name->name);
        return (existing != ZR_NULL &&
                existing->kind == ZR_COMPILE_TOOL_BINDING_SHADOW) ||
               ZrParser_CompileToolBinding_DeclareShadow(cs, name->name);
    }

    return ZR_TRUE;
}

TZrBool ZrParser_CompileToolExecutionScope_DeclarePatternShadows(
        SZrCompilerState *cs,
        SZrAstNode *pattern) {
    SZrAstNodeArray *keys = ZR_NULL;

    if (pattern == ZR_NULL) {
        return ZR_TRUE;
    }
    if (pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ZrParser_CompileToolBinding_DeclareShadow(
                cs, pattern->data.identifier.name);
    }
    if (pattern->type == ZR_AST_KEY_VALUE_PAIR) {
        return ZrParser_CompileToolExecutionScope_DeclarePatternShadows(
                cs, pattern->data.keyValuePair.value);
    }
    if (pattern->type == ZR_AST_UNPACK_LITERAL) {
        return ZrParser_CompileToolExecutionScope_DeclarePatternShadows(
                cs, pattern->data.unpackLiteral.element);
    }
    if (pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
        keys = pattern->data.destructuringObject.keys;
    } else if (pattern->type == ZR_AST_DESTRUCTURING_ARRAY) {
        keys = pattern->data.destructuringArray.keys;
    } else {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; keys != ZR_NULL && index < keys->count; index++) {
        if (!ZrParser_CompileToolExecutionScope_DeclarePatternShadows(
                    cs, keys->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static SZrAstNode *compile_tool_scope_selected_branch(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_COMPILE_TIME_DECLARATION ||
        !node->data.compileTimeDeclaration.isConditionalPruning ||
        !node->data.compileTimeDeclaration.buildFactsEvaluated) {
        return ZR_NULL;
    }
    return node->data.compileTimeDeclaration.selectedBranch;
}

static TZrBool compile_tool_scope_predeclare_bindings(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes) {
    for (TZrSize index = 0U; nodes != ZR_NULL && index < nodes->count; index++) {
        if (!ZrParser_CompileToolExecutionScope_DeclareFunctionShadow(
                    cs, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }
    for (TZrSize index = 0U; nodes != ZR_NULL && index < nodes->count; index++) {
        SZrAstNode *node = nodes->nodes[index];
        SZrAstNode *branch;

        if (compiler_is_compile_tool_import_declaration(cs->state, node)) {
            if (!ZrParser_CompileToolExecution_DeclareImport(cs, node)) {
                return ZR_FALSE;
            }
            continue;
        }
        branch = compile_tool_scope_selected_branch(node);
        if (branch != ZR_NULL &&
            !(branch->type == ZR_AST_BLOCK
                      ? compile_tool_scope_predeclare_bindings(
                                cs, branch->data.block.body)
                      : ZrParser_CompileToolExecutionScope_DeclareFunctionShadow(
                                cs, branch))) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static SZrImportedCompileTimeModule *compile_tool_scope_find_retained_module(
        SZrCompilerState *owner,
        SZrString *moduleName) {
    for (; owner != ZR_NULL; owner = owner->compileToolProviderParent) {
        for (TZrSize index = 0U;
             index < owner->importedCompileTimeModules.length;
             index++) {
            SZrImportedCompileTimeModule **module =
                    (SZrImportedCompileTimeModule **)ZrCore_Array_Get(
                            &owner->importedCompileTimeModules, index);
            if (module != ZR_NULL && *module != ZR_NULL &&
                (*module)->moduleName != ZR_NULL &&
                ZrCore_String_Equal((*module)->moduleName, moduleName)) {
                return *module;
            }
        }
    }
    return ZR_NULL;
}

static TZrBool compile_tool_scope_declare_source_alias(
        SZrCompilerState *cs,
        SZrCompilerState *owner,
        SZrAstNode *node) {
    SZrVariableDeclaration *declaration = &node->data.variableDeclaration;
    SZrAstNode *modulePath;
    SZrImportedCompileTimeModule *module;

    if (declaration->pattern == ZR_NULL ||
        declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        declaration->pattern->data.identifier.name == ZR_NULL ||
        declaration->value == ZR_NULL ||
        declaration->value->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_TRUE;
    }
    modulePath = declaration->value->data.importExpression.modulePath;
    if (modulePath == ZR_NULL || modulePath->type != ZR_AST_STRING_LITERAL ||
        modulePath->data.stringLiteral.value == ZR_NULL) {
        return ZR_TRUE;
    }
    module = compile_tool_scope_find_retained_module(
            owner, modulePath->data.stringLiteral.value);
    return module == ZR_NULL ||
           ZrParser_CompileTimeImport_RegisterModuleAlias(
                   cs, declaration->pattern->data.identifier.name,
                   module, node->location, ZR_FALSE);
}

static TZrBool compile_tool_scope_rebuild_array(
        SZrCompilerState *cs,
        SZrCompilerState *owner,
        SZrAstNodeArray *nodes);

static TZrBool compile_tool_scope_declare_source_node(
        SZrCompilerState *cs,
        SZrCompilerState *owner,
        SZrAstNode *node) {
    SZrAstNode *branch;

    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    if (compiler_is_compile_tool_import_declaration(cs->state, node)) {
        return ZrParser_CompileToolExecution_DeclareImport(cs, node);
    }
    if (node->type == ZR_AST_VARIABLE_DECLARATION) {
        return ZrParser_CompileToolExecutionScope_DeclarePatternShadows(
                       cs, node->data.variableDeclaration.pattern) &&
               compile_tool_scope_declare_source_alias(cs, owner, node);
    }
    branch = compile_tool_scope_selected_branch(node);
    if (branch == ZR_NULL) {
        return ZR_TRUE;
    }
    return branch->type == ZR_AST_BLOCK
                   ? compile_tool_scope_rebuild_array(cs, owner, branch->data.block.body)
                   : compile_tool_scope_declare_source_node(cs, owner, branch);
}

/* Replay only lexical facts already selected by BuildFacts, in its two-pass order. */
static TZrBool compile_tool_scope_rebuild_array(
        SZrCompilerState *cs,
        SZrCompilerState *owner,
        SZrAstNodeArray *nodes) {
    if (!compile_tool_scope_predeclare_bindings(cs, nodes)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; nodes != ZR_NULL && index < nodes->count; index++) {
        if (!compile_tool_scope_declare_source_node(cs, owner, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool compile_tool_scope_enter_ast(
        SZrCompilerState *cs,
        SZrCompilerState *owner,
        SZrAstNode *scriptAst,
        SZrImportedCompileTimeModule *module,
        SZrCompileToolExecutionScope *scope) {
    if (scope == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(scope, 0, sizeof(*scope));
    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawCopy(
            &scope->previousBindings, &cs->compileToolBindings,
            sizeof(scope->previousBindings));
    ZrCore_Memory_RawCopy(
            &scope->previousModuleAliases, &cs->importedCompileTimeModuleAliases,
            sizeof(scope->previousModuleAliases));
    scope->previousModule = cs->activeImportedCompileTimeModule;
    ZrCore_Array_Init(
            cs->state, &cs->compileToolBindings,
            scope->previousBindings.elementSize, scope->previousBindings.capacity);
    ZrCore_Array_Init(
            cs->state, &cs->importedCompileTimeModuleAliases,
            scope->previousModuleAliases.elementSize, scope->previousModuleAliases.capacity);
    cs->activeImportedCompileTimeModule = module;
    scope->entered = ZR_TRUE;

    if (cs->compileToolBindings.head == ZR_NULL ||
        cs->importedCompileTimeModuleAliases.head == ZR_NULL ||
        (scriptAst != ZR_NULL && scriptAst->type == ZR_AST_SCRIPT &&
         !compile_tool_scope_rebuild_array(cs, owner, scriptAst->data.script.statements))) {
        ZrParser_CompileToolExecutionScope_Leave(cs, scope);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_CompileToolExecutionScope_EnterAst(
        SZrCompilerState *cs,
        SZrAstNode *scriptAst,
        SZrImportedCompileTimeModule *module,
        SZrCompileToolExecutionScope *scope) {
    return compile_tool_scope_enter_ast(cs, cs, scriptAst, module, scope);
}

static SZrCompilerState *compile_tool_scope_find_root_owner(
        SZrCompilerState *cs,
        const SZrCompileTimeFunction *function) {
    for (; cs != ZR_NULL; cs = cs->compileToolProviderParent) {
        SZrArray *functions = &cs->compileTimeFunctions;

        if (!functions->isValid || functions->head == ZR_NULL ||
            functions->elementSize != sizeof(SZrCompileTimeFunction *)) {
            continue;
        }
        for (TZrSize index = 0U; index < functions->length; index++) {
            SZrCompileTimeFunction **candidate =
                    (SZrCompileTimeFunction **)ZrCore_Array_Get(functions, index);
            if (*candidate == function) {
                return cs;
            }
        }
    }
    return ZR_NULL;
}

static SZrCompilerState *compile_tool_scope_find_module_owner(
        SZrCompilerState *cs,
        SZrImportedCompileTimeModule *module) {
    for (SZrCompilerState *owner = cs; owner != ZR_NULL;
         owner = owner->compileToolProviderParent) {
        for (TZrSize index = 0U;
             index < owner->importedCompileTimeModules.length;
             index++) {
            SZrImportedCompileTimeModule **candidate =
                    (SZrImportedCompileTimeModule **)ZrCore_Array_Get(
                            &owner->importedCompileTimeModules, index);
            if (*candidate == module) {
                return owner;
            }
        }
    }
    /* EnterAst also supports declaration collection before a module is published. */
    return cs;
}

TZrBool ZrParser_CompileToolExecutionScope_EnterFunction(
        SZrCompilerState *cs,
        const SZrCompileTimeFunction *function,
        SZrCompileToolExecutionScope *scope) {
    SZrCompilerState *owner;
    SZrImportedCompileTimeModule *module;

    if (scope == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(scope, 0, sizeof(*scope));
    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }
    module = function->ownerModule;
    owner = module != ZR_NULL
                    ? compile_tool_scope_find_module_owner(cs, module)
                    : compile_tool_scope_find_root_owner(cs, function);
    if (owner == ZR_NULL) {
        return ZR_FALSE;
    }
    return compile_tool_scope_enter_ast(
            cs, owner, module != ZR_NULL ? module->scriptAst : owner->scriptAst,
            module, scope);
}

void ZrParser_CompileToolExecutionScope_Leave(
        SZrCompilerState *cs,
        SZrCompileToolExecutionScope *scope) {
    if (cs == ZR_NULL || scope == ZR_NULL || !scope->entered) {
        return;
    }
    ZrCore_Array_Free(cs->state, &cs->compileToolBindings);
    ZrCore_Array_Free(cs->state, &cs->importedCompileTimeModuleAliases);
    ZrCore_Memory_RawCopy(
            &cs->compileToolBindings, &scope->previousBindings,
            sizeof(scope->previousBindings));
    ZrCore_Memory_RawCopy(
            &cs->importedCompileTimeModuleAliases, &scope->previousModuleAliases,
            sizeof(scope->previousModuleAliases));
    cs->activeImportedCompileTimeModule = scope->previousModule;
    scope->entered = ZR_FALSE;
}
