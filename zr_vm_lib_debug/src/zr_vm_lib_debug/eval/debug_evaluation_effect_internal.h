#ifndef ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H
#define ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H

#include "debug_internal.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

/** 单次正式表达式分析的资源集合；AST、诊断和语义事实均以其中的解析器/编译器状态为生存期。
 * 初始化标志允许解析或推断在任意阶段失败后按依赖顺序释放。
 */
typedef struct SZrDebugFormalEvaluationContext {
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrStructuredDiagnostic parserDiagnostic;
    SZrAstNode *expression;
    const SZrSemanticExpressionFact *expressionFact;
    TZrBool parserStateInitialized;
    TZrBool compilerStateInitialized;
    TZrBool inferredTypeInitialized;
    TZrBool hasParserDiagnostic;
    TZrBool hasCanonicalFacts;
} SZrDebugFormalEvaluationContext;

void zr_debug_evaluation_effect_classify_structure(const SZrAstNode *node, TZrUInt32 *effectFlags);
void zr_debug_evaluation_effect_classify_resolved_properties(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        TZrUInt32 *effectFlags);
TZrBool zr_debug_evaluation_effect_has_canonical_facts(const SZrSemanticContext *context,
                                                       const SZrAstNode *expression,
                                                       const SZrSemanticExpressionFact *expressionFact);
/** @brief 在暂停帧绑定下解析表达式并准备语义事实，供副作用分类器读取。
 * @note 成功时调用方必须调用 zr_debug_formal_free_prepared_expression；成功不保证有规范语义事实。
 */
ZR_DEBUG_API TZrBool zr_debug_formal_prepare_expression(ZrDebugAgent *agent,
                                                        TZrUInt32 frameId,
                                                        const TZrChar *expression,
                                                        SZrDebugFormalEvaluationContext *outContext,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize);
TZrBool zr_debug_formal_prepare_expression_with_failure(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        SZrDebugFormalEvaluationContext *outContext,
        ZrDebugEvaluateFailure *outFailure,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);
/** @brief 释放准备阶段持有的推断结果、编译器、AST、诊断和解析器状态。 */
ZR_DEBUG_API void zr_debug_formal_free_prepared_expression(SZrDebugFormalEvaluationContext *context);
TZrBool zr_debug_formal_has_paused_array_index_facts(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *expression,
        const SZrSemanticExpressionFact *expressionFact);
/** @brief 在规范语义上下文中读取暂停帧并执行上层已许可的表达式节点。
 * @return false 表示执行错误；true 且 *outSupported 为 false 表示节点未由正式执行器处理。
 */
ZR_DEBUG_API TZrBool zr_debug_formal_evaluate_node(ZrDebugAgent *agent,
                                                   TZrUInt32 frameId,
                                                   const SZrSemanticContext *semanticContext,
                                                   const SZrAstNode *node,
                                                   SZrTypeValue *outValue,
                                                   TZrBool *outSupported,
                                                   TZrChar *errorBuffer,
                                                   TZrSize errorBufferSize);

#endif // ZR_VM_DEBUG_EVALUATION_EFFECT_INTERNAL_H

/**
 * @struct SZrDebugFormalEvaluationContext
 * @unit SZrDebugFormalEvaluationContext
 *
 * ## Summary
 * 承载一次正式表达式准备的解析器、编译器、推断结果、诊断及其借用事实，供分类器和正式执行器同步消费。
 *
 * ## Fields
 * - parserState：解析源码、AST 与诊断的解析器资源及 VM state。
 * - compilerState：暂停绑定和语义上下文的编译器资源。
 * - inferredType：由 inferredTypeInitialized 标记有效的推断输出。
 * - parserDiagnostic：仅 hasParserDiagnostic 为真时持有已复制诊断。
 * - expression：准备阶段持有、由配套释放函数销毁的 AST 根。
 * - expressionFact：借用当前 compilerState.semanticContext 内的根表达式事实。
 * - parserStateInitialized：指示解析器已初始化，释放时才调用 State_Free。
 * - compilerStateInitialized：指示编译器已初始化，释放时才调用 CompilerState_Free。
 * - inferredTypeInitialized：指示推断值已初始化，须先于编译器释放。
 * - hasParserDiagnostic：标记回调成功复制的诊断，控制其释放。
 * - hasCanonicalFacts：当前准备所得的规范事实可用性快照，不等于 pure。
 *
 * ## Value model
 * 全零表示未持有已标记资源；成功准备可持有资源但 hasCanonicalFacts 仍为 false。只在所属 VM 与当前准备结果存活期间使用内部指针。
 *
 * ## Invariants and lifecycle
 * 初始化标志支配资源释放；expressionFact 借用 compilerState.semanticContext，AST 与诊断依赖 parserState/state。上下文不可浅拷贝后分别释放；复用前先释放旧内容，不能把 prepare 的 memset 当资源回收。普通释放最后归零；此结构不提供锁或非局部异常自动清理。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect_internal.h:13
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:172
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:208
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:228
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:274
 */

/**
 * @fn void zr_debug_evaluation_effect_classify_structure(const SZrAstNode *node, TZrUInt32 *effectFlags)
 * @unit zr_debug_evaluation_effect_classify_structure
 *
 * ## Summary
 * 私有跨翻译单元接口：从已解析 AST 累加调用、分配和赋值能力位，供分类及正式执行前的门禁使用；收集的是潜在结构作用，不执行这些作用。
 *
 * ## Parameters
 * @param[in] node 借用节点；空节点不改变输出。
 * @param[in,out] effectFlags 调用方初始化的累加位集；只 OR 新位，空指针不处理。
 *
 * ## Returns
 * void；结果通过 effectFlags 累加，不清除已有位，也不报告节点支持度。
 *
 * ## Constraints and lifecycle
 * 逻辑、条件和 if 的所有受支持子分支均参与分类；lambda 计创建时分配，不遍历其函数体。本函数不证明规范事实、不授予能力，也不从 CALL 推断 NATIVE_CALL。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:223
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:231
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:257
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:277
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:297
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:379
 */

/**
 * @fn void zr_debug_evaluation_effect_classify_resolved_properties( const SZrSemanticContext *context, const SZrAstNode *node, TZrUInt32 *effectFlags)
 * @unit zr_debug_evaluation_effect_classify_resolved_properties
 *
 * ## Summary
 * 私有跨翻译单元接口：补充结构分类不可见的 getter 与所有权能力要求，供已取得规范事实的上层决定是否执行。
 *
 * ## Parameters
 * @param[in] context 与节点配套且调用期间稳定的语义上下文。
 * @param[in] node 借用 AST；空参数不处理。
 * @param[in,out] effectFlags 已有结构能力位；只累加，不重置。
 *
 * ## Returns
 * void；按查询成功的 getter SymbolId 和非违规所有权事实追加位；查询无匹配不单独报错。
 *
 * ## Constraints and lifecycle
 * 本函数不自行验证 hasCanonicalFacts；两个生产调用点先作该门禁。只查询事实并递归当前支持的子节点，不调用 getter，不把无新增位视为完整或纯的证明。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:358
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:370
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:373
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:386
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:390
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:373
 */

/**
 * @fn TZrBool zr_debug_evaluation_effect_has_canonical_facts(const SZrSemanticContext *context, const SZrAstNode *expression, const SZrSemanticExpressionFact *expressionFact)
 * @unit zr_debug_evaluation_effect_has_canonical_facts
 *
 * ## Summary
 * 私有跨翻译单元接口：合并根表达式事实的精确度与引用身份检查，为副作用策略提供规范事实门禁。
 *
 * ## Parameters
 * @param[in] context 与 expression 配套的语义上下文。
 * @param[in] expression 借用根 AST 节点。
 * @param[in] expressionFact 借用根表达式事实；须由同一上下文查得。
 *
 * ## Returns
 * @return 仅当三个参数非空、根事实 EXACT 且非 ERROR、范围内无未解析引用、当前标识符检查通过时 true。
 *
 * ## Constraints and lifecycle
 * 不执行表达式，也不检查最终能力白名单。本检查返回 false 后，准备层可另外核对暂停数组整数字面量索引窄路径；该补充不是本函数的返回语义。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:329
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:333
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:338
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:274
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:281
 */

/**
 * @fn TZrBool zr_debug_formal_prepare_expression(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, SZrDebugFormalEvaluationContext *outContext, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * @unit zr_debug_formal_prepare_expression
 *
 * ## Summary
 * 建立供副作用分类和正式读值使用的单次上下文；该包装不要求结构化失败对象。
 *
 * ## Parameters
 * @param[in,out] agent 借用 agent/state 以建立解析、编译和推断资源；调用方维持暂停绑定稳定。
 * @param[in] frameId 传给语义绑定与暂停数组事实检查的帧标识。
 * @param[in] expression 调用期间有效的 NUL 结尾表达式源码。
 * @param[out] outContext 必需输出；入口归零，须为空闲上下文，避免覆盖尚未释放的资源。
 * @param[out] errorBuffer 可空的诊断缓冲区；不保证成功时清空旧文本。
 * @param[in] errorBufferSize 诊断可写容量，零容量不写。
 *
 * ## Returns
 * @return false 表示请求或解析失败，普通解析失败已释放并归零 context。true 后调用方必须释放；绑定注册或推断未成功时仍可能 true 且没有规范事实。
 *
 * ## Constraints and lifecycle
 * 直接委托 with_failure，outFailure 传 NULL；不执行表达式，也不承诺上下文已可安全求值。成功持有的指针不得越过配套释放或 VM 生命周期；非局部异常不会由本接口自动转换成 false。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:292
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:298
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:250
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:261
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:268
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:172
 */

/**
 * @fn TZrBool zr_debug_formal_prepare_expression_with_failure( ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, SZrDebugFormalEvaluationContext *outContext, ZrDebugEvaluateFailure *outFailure, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * @unit zr_debug_formal_prepare_expression_with_failure
 *
 * ## Summary
 * 解析表达式并注册暂停绑定、推断类型和规范事实，同时按请求/解析阶段提供结构化诊断。
 *
 * ## Parameters
 * @param[in,out] agent 借用 agent/state 以建立解析、编译和推断资源；调用方维持暂停绑定稳定。
 * @param[in] frameId 传给语义绑定与暂停数组事实检查的帧标识。
 * @param[in] expression 调用期间有效的 NUL 结尾表达式源码。
 * @param[out] outContext 必需输出；入口归零，须为空闲上下文，避免覆盖尚未释放的资源。
 * @param[out] outFailure 可空的结构化失败槽；仅相关失败分支写入，入口不统一清零，调用方须按需初始化。
 * @param[out] errorBuffer 可空的诊断缓冲区；不保证成功时清空旧文本。
 * @param[in] errorBufferSize 诊断可写容量，零容量不写。
 *
 * ## Returns
 * @return outContext 为 NULL 或请求/解析失败返回 false；普通解析失败先释放。成功解析后即使绑定、推断或完整规范事实不可用，也可 true，调用方须检查 context 并最终释放。
 *
 * ## Constraints and lifecycle
 * 先将空闲输出归零，再初始化解析器/回调，之后初始化编译器和推断值。outFailure 不在入口清空；成功不保证 errorBuffer 为空。资源仅由上下文拥有，事实是内部借用，非局部跳转不保证执行尾部释放。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:205
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:208
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:228
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:231
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:250
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:254
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:261
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:268
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:277
 */

/**
 * @fn void zr_debug_formal_free_prepared_expression(SZrDebugFormalEvaluationContext *context)
 * @unit zr_debug_formal_free_prepared_expression
 *
 * ## Summary
 * 按依赖顺序回收单次准备拥有的推断值、编译器、AST、诊断和解析器，结束所有内部借用。
 *
 * ## Parameters
 * @param[in,out] context 可空；非空时须为全零或由配套准备函数形成、尚未复制所有权的上下文。
 *
 * ## Returns
 * void；普通完成后整块归零，内部 AST/事实/诊断指针失效；全零上下文可再次释放。
 *
 * ## Constraints and lifecycle
 * 顺序为推断值、编译器、AST、已复制诊断、解析器，依各标志决定是否调用析构。所属 VM/state 必须仍存活；不能在其他消费者仍读内部指针时释放，也不负责 outValue 中 VM 对象的生命周期。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:172
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:176
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:192
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect_internal.h:13
 */

/**
 * @fn TZrBool zr_debug_formal_has_paused_array_index_facts( ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticContext *semanticContext, const SZrAstNode *expression, const SZrSemanticExpressionFact *expressionFact)
 * @unit zr_debug_formal_has_paused_array_index_facts
 *
 * ## Summary
 * 在通常的规范事实门禁失败后，只为能从当前暂停帧核实的数组根和整数字面量索引提供窄补充。
 *
 * ## Parameters
 * @param[in] agent 借用的 agent/state，供读取暂停帧、运行时根或闭包捕获。
 * @param[in] frameId 暂停帧标识；底层按 0 或 frameId-1 选核心帧。
 * @param[in] semanticContext 与表达式配套的借用语义上下文。
 * @param[in] expression 主表达式 AST，根须为标识符且具有至少一个成员。
 * @param[in] expressionFact 当前根的借用事实，须 EXACT 且非 ERROR。
 *
 * ## Returns
 * @return 仅在根引用已解析且 SymbolId 有效、当前读取值为 ARRAY、每个成员均为计算式整数字面量索引时 true；否则 false。
 *
 * ## Constraints and lifecycle
 * 不实际取数组元素，不检查字面索引是否在界内，也不授予分配或调用能力。暂停读值仍按实际引用来源核对 symbol/type/place 或 token；不是任意缺失语义事实的宽松回退。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:228
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:238
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:249
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:258
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:265
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:64
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:118
 */

/**
 * @fn TZrBool zr_debug_formal_evaluate_node(ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticContext *semanticContext, const SZrAstNode *node, SZrTypeValue *outValue, TZrBool *outSupported, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * @unit zr_debug_formal_evaluate_node
 *
 * ## Summary
 * 消费上层已校验的规范事实与能力许可，读取暂停值或执行正式执行器支持的表达式子集；本低层接口不重新作能力授权。
 *
 * ## Parameters
 * @param[in,out] agent 借用 agent/state；支持的数组字面量会分配 VM 对象。
 * @param[in] frameId 读暂停绑定所用的帧标识。
 * @param[in] semanticContext 与 node 配套且未释放的语义上下文。
 * @param[in] node 合法、有限的 AST 节点，调用期间保持有效。
 * @param[in,out] outValue 借用的结果槽；成功支持时写结果，错误或不支持时不得假定清零或回滚。
 * @param[out] outSupported 必需支持度槽；与 bool 返回值共同判断，不能只看返回 true。
 * @param[out] errorBuffer 可空的诊断区，未写错误分支可能保留旧文本。
 * @param[in] errorBufferSize 诊断可写容量。
 *
 * ## Returns
 * @return false 表示正式执行错误；true 且 outSupported 为 false 表示请求无效或当前节点/运行时情况未由此执行器处理。只有 true 且 supported 时消费结果。
 *
 * ## Constraints and lifecycle
 * 上层在执行前独立禁止赋值并比对能力位。unsupported 或错误可能已读取子节点、产生中间值或分配，不承诺事务回滚；结果中的 VM 引用依赖外部 VM/GC 生命周期，不能当作 AST 所有权。只覆盖普通返回，非局部 Throw 不由此声明捕获。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:347
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:362
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:387
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:438
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:602
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:794
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:857
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:379
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:428
 */
