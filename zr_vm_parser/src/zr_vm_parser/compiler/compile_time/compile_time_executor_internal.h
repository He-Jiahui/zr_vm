#ifndef ZR_VM_PARSER_COMPILE_TIME_EXECUTOR_INTERNAL_H
#define ZR_VM_PARSER_COMPILE_TIME_EXECUTOR_INTERNAL_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/comptime_cache.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc_domain.h"
#include "compiler/compile_time/compile_time_control_flow.h"

typedef struct SZrCompileTimeBinding {
    SZrString *name;
    SZrSyntaxContext syntaxContext;
    SZrTypeValue value;
    SZrGcRootHandle nameRoot;
    SZrGcRootHandle valueRoot;
    TZrBool hasNameRoot;
    TZrBool hasValueRoot;
    TZrBool isConst;
} SZrCompileTimeBinding;

struct SZrCompileTimeFrame {
    SZrArray bindings;
    /* Strong roots for expression results that must survive later CT
     * allocations until this frame is released. */
    SZrArray temporaryRoots;
    struct SZrCompileTimeFrame *parent;
    SZrState *state;
};

TZrBool ct_contexts_equal(
        SZrSyntaxContext left, SZrSyntaxContext right,
        EZrSyntaxNamespace namespaceKind);
TZrBool ct_value_needs_root(const SZrTypeValue *value);
const TZrChar *ct_name(SZrString *name);
void ct_error_name(SZrCompilerState *cs, SZrString *name, const TZrChar *prefix, SZrFileRange location);
TZrBool ct_truthy(const SZrTypeValue *value);
void ct_init_type_from_value(SZrCompilerState *cs, const SZrTypeValue *value, SZrInferredType *result);
TZrBool ct_string_equals(SZrString *value, const TZrChar *literal);
TZrBool ct_eval_import_expression(SZrCompilerState *cs, SZrAstNode *node, SZrTypeValue *result);
void ct_frame_init(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrCompileTimeFrame *parent);
void ct_frame_free(SZrCompilerState *cs, SZrCompileTimeFrame *frame);
TZrBool ct_frame_get(SZrCompileTimeFrame *frame, SZrString *name, SZrTypeValue *result);
TZrBool ct_frame_get_with_context(SZrCompileTimeFrame *frame,
                                  SZrString *name,
                                  SZrSyntaxContext syntaxContext,
                                  SZrTypeValue *result);
TZrBool ct_frame_set(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrString *name, const SZrTypeValue *value);
TZrBool ct_frame_declare(SZrCompilerState *cs,
                         SZrCompileTimeFrame *frame,
                         SZrString *name,
                         const SZrTypeValue *value,
                         TZrBool isConst);
TZrBool ct_frame_declare_with_context(
        SZrCompilerState *cs,
        SZrCompileTimeFrame *frame,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        const SZrTypeValue *value,
        TZrBool isConst);
TZrBool ct_frame_update(SZrCompilerState *cs,
                        SZrCompileTimeFrame *frame,
                        SZrString *name,
                        const SZrTypeValue *value,
                        SZrFileRange location);
TZrBool ct_frame_update_with_context(
        SZrCompilerState *cs,
        SZrCompileTimeFrame *frame,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        const SZrTypeValue *value,
        SZrFileRange location);
TZrBool ct_frame_has_binding(SZrCompileTimeFrame *frame, SZrString *name);
TZrBool ct_frame_has_binding_with_context(
        SZrCompileTimeFrame *frame,
        SZrString *name,
        SZrSyntaxContext syntaxContext);
TZrBool ct_frame_root_value(SZrCompilerState *cs,
                            SZrCompileTimeFrame *frame,
                            SZrTypeValue *value);
TZrBool ct_compile_time_variable_set_value(
        SZrCompilerState *cs,
        SZrCompileTimeVariable *variable,
        const SZrTypeValue *value);
TZrBool ct_compile_time_variable_get_value(
        SZrCompilerState *cs,
        SZrCompileTimeVariable *variable,
        SZrTypeValue *result);
SZrCompileTimeVariable *find_compile_time_variable(SZrCompilerState *cs, SZrString *name);
SZrCompileTimeVariable *find_compile_time_variable_with_context(
        SZrCompilerState *cs,
        SZrString *name,
        SZrSyntaxContext syntaxContext);
SZrCompileTimeFunction *find_compile_time_function(SZrCompilerState *cs, SZrString *name);
SZrCompileTimeFunction *find_compile_time_function_with_context(
        SZrCompilerState *cs,
        SZrString *name,
        SZrSyntaxContext syntaxContext);
TZrBool ct_try_get_compile_time_value_with_context(
        SZrCompilerState *cs,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        SZrTypeValue *result);
SZrCompileTimeFunction *resolve_imported_compile_time_function(
        SZrCompilerState *cs,
        SZrString *moduleName,
        SZrString *functionName);
TZrBool ct_value_from_compile_time_function(SZrCompilerState *cs, SZrCompileTimeFunction *func, SZrTypeValue *result);
TZrBool ct_value_try_get_compile_time_function(SZrCompilerState *cs,
                                               const SZrTypeValue *value,
                                               SZrCompileTimeFunction **result);
TZrBool evaluate_compile_time_expression_internal(SZrCompilerState *cs,
                                                  SZrAstNode *node,
                                                  SZrCompileTimeFrame *frame,
                                                  SZrTypeValue *result);
TZrBool ct_eval_call_arg(SZrCompilerState *cs,
                        SZrFunctionCall *call,
                        SZrParameter *param,
                        TZrSize paramIndex,
                        SZrCompileTimeFrame *frame,
                        SZrTypeValue *result);
TZrBool ct_execute_compile_time_decorator_function(
        SZrCompilerState *cs,
        SZrCompileTimeFunction *decoratorFunction,
        SZrFunctionCall *constructorCall,
        const SZrTypeValue *targetSnapshot,
        const TZrChar *expectedTargetName,
        SZrCompileTimeFrame *lifetimeFrame,
        SZrTypeValue *patchResult,
        SZrFileRange location);
TZrBool register_compile_time_variable_declaration(SZrCompilerState *cs, SZrAstNode *node, SZrFileRange location);
TZrBool register_compile_time_function_declaration(SZrCompilerState *cs, SZrAstNode *node, SZrFileRange location);
TZrBool register_compile_time_function_alias(SZrCompilerState *cs,
                                             SZrString *aliasName,
                                             SZrAstNode *node,
                                             SZrFileRange location);
TZrBool ZrParser_CompileToolExecution_DeclareImport(
        SZrCompilerState *cs,
        SZrAstNode *node);
TZrBool ZrParser_CompileTime_RegisterDecoratorFunctionIfAvailable(SZrCompilerState *cs,
                                                                  SZrAstNode *node,
                                                                  SZrFileRange location);
TZrBool ZrParser_CompileTime_ApplyFunctionDecorators(SZrCompilerState *cs,
                                                     SZrAstNodeArray *decorators,
                                                     SZrFunction *function,
                                                     SZrFileRange location);
TZrBool ZrParser_CompileTime_ApplyMemberDecorators(SZrCompilerState *cs,
                                                   SZrAstNode *memberNode,
                                                   SZrAstNodeArray *decorators,
                                                   SZrTypeMemberInfo *memberInfo);
TZrBool ZrParser_CompileTime_ApplyParameterDecorators(SZrCompilerState *cs,
                                                      SZrAstNode *parameterNode,
                                                      TZrUInt32 position,
                                                      SZrFunctionMetadataParameter *parameterInfo);
TZrBool execute_compile_time_statement(SZrCompilerState *cs,
                                       SZrAstNode *node,
                                       SZrCompileTimeFrame *frame,
                                       TZrBool *didReturn,
                                       SZrTypeValue *result);
TZrBool execute_compile_time_block(SZrCompilerState *cs,
                                   SZrAstNode *node,
                                   SZrCompileTimeFrame *frame,
                                   TZrBool *didReturn,
                                   SZrTypeValue *result);
/**
 * @brief 在布局和 typed metadata 完成后执行模块级编译期检查指令。
 * @pre cs 是持有效 state 的已初始化编译器状态，ast 是 script，阶段为 LATE_CHECK；先完成 build facts 以选定分支和注册编译期函数。
 * @return 输入或阶段无效、某条选中指令失败时返回假；遍历完成时返回真。
 * @note 该入口可供独立测试显式设置阶段调用；正常编译由脚本收尾路径调用。
 */
ZR_PARSER_API TZrBool ZrParser_CompileTime_ExecuteLateChecksInCompilerState(
        SZrCompilerState *cs,
        SZrAstNode *ast);
TZrBool ZrParser_CompileTime_PrepareBuildFactsWithCache(
        SZrState *state,
        SZrAstNode *ast,
        SZrParserSourceComptimeCache *cache,
        const TZrChar *source,
        TZrSize sourceLength);

#endif // ZR_VM_PARSER_COMPILE_TIME_EXECUTOR_INTERNAL_H
