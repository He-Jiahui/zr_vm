#include "compile_expression_internal.h"

#include <string.h>

#include "zr_vm_parser/semantic_facts.h"

/* 语义事实用 operation 记录 intrinsic，通用所有权 lowering 则以 builtinKind 分派；
 * 此处集中维护两套枚举之间的对应关系。 */
static EZrOwnershipBuiltinKind ownership_intrinsic_builtin_kind(
        EZrOwnershipIntrinsicOperation operation) {
    switch (operation) {
        case ZR_OWNERSHIP_INTRINSIC_SHARE:
            return ZR_OWNERSHIP_BUILTIN_KIND_SHARE;
        case ZR_OWNERSHIP_INTRINSIC_DEGRADE:
            return ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE;
        case ZR_OWNERSHIP_INTRINSIC_WAKE:
            return ZR_OWNERSHIP_BUILTIN_KIND_WAKE;
        case ZR_OWNERSHIP_INTRINSIC_INTO_GC:
            return ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC;
        case ZR_OWNERSHIP_INTRINSIC_DROP:
            return ZR_OWNERSHIP_BUILTIN_KIND_DROP;
        default:
            return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }
}

/* 表达式分派把专用 AST 节点交给这里；以类型推断发布的语义事实作为 lowering 输入，
 * 再适配到已有的 ownership builtin 编译路径。 */
void compile_ownership_intrinsic_expression(SZrCompilerState *cs, SZrAstNode *node) {
    const SZrOwnershipIntrinsicFact *fact;
    SZrConstructExpression constructExpression;
    SZrInferredType inferredType;
    EZrOwnershipBuiltinKind builtinKind;

    /* 编译器错误状态由外层调用链统一处理；不在缺失上下文或既有错误后继续生成代码。 */
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError ||
        node->type != ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION) {
        return;
    }

    /* 类型推断负责发布 canonical intrinsic fact。若前序阶段尚未发布，就在此触发推断并释放临时类型；
     * 推断可能扩充事实数组，因此随后重新查找并只使用新取得的借用指针。 */
    fact = ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
            cs->semanticContext, node);
    if (fact == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(cs, node, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return;
        }
        ZrParser_InferredType_Free(cs->state, &inferredType);
        fact = ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
                cs->semanticContext, node);
    }

    /* 缺少事实或实参时不能再从词面拼回操作；用节点范围报告编译失败，避免错误 lowering。 */
    if (fact == ZR_NULL || fact->argument == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "Ownership intrinsic is missing its canonical semantic fact", node->location);
        return;
    }

    /* 尚未支持的语义操作必须在进入通用 helper 前失败关闭，避免将 NONE 当作有效 builtin。 */
    builtinKind = ownership_intrinsic_builtin_kind(fact->operation);
    if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_NONE) {
        ZrParser_Compiler_Error(cs, "Unsupported ownership intrinsic", fact->range);
        return;
    }

    /* 这里不是对象构造：临时 construct 仅承载事实中的操作数和 builtin tag；统一 helper
     * 继续负责操作数约束、槽位转移及 pre-execution SemIR lowering。 */
    memset(&constructExpression, 0, sizeof(constructExpression));
    constructExpression.target = fact->argument;
    constructExpression.builtinKind = builtinKind;
    constructExpression.isNew = ZR_FALSE;
    /* TODO: 本调用忽略 helper 的 bool；另一个 construct-call 包装器会对 false 且无 cs->hasError 追加诊断。
     * 需核实这里所有可达的 false 路径都设置错误状态，否则应补上相同的兜底诊断。 */
    compile_ownership_builtin_expression(
            cs, &constructExpression, ZR_PARSER_SLOT_NONE, fact->range);
}
