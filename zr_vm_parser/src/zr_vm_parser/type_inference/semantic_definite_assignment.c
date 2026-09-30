#include "dataflow.h"
#include "dataflow_definite_assignment.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

/**
 * @brief 将本轮语义引用事实里的去重符号 ID 映射为数据流状态槽。
 * @note 数组由一次公开 resolver 调用临时持有；槽序由首次遇到的 resolved declaration/read/write fact 决定。
 */
typedef struct SZrSemanticDaSymbolMap {
    SZrArray symbolIds;
} SZrSemanticDaSymbolMap;

/**
 * @brief 在单个 CFG 根分析期间借用语义上下文、符号槽表与 READ 聚合缓冲。
 * @note context 与 symbols 不转移所有权；两个按事实索引的缓冲由 run_cfg_for_root 分配和释放。
 */
typedef struct SZrSemanticDaAnalysis {
    SZrSemanticContext *context;
    const SZrSemanticDaSymbolMap *symbols;
    EZrParserDefiniteAssignmentState *readStates;
    TZrBool *readStateSeen;
    TZrSize readStateCount;
} SZrSemanticDaAnalysis;

/** @brief 以指针身份或字符串值比较两个 source 名称；两个空名称不匹配。 */
static TZrBool semantic_da_same_source(SZrString *left, SZrString *right) {
    if (left == right) {
        return ZR_TRUE;
    }
    return left != ZR_NULL &&
           right != ZR_NULL &&
           ZrCore_String_Equal(left, right);
}

/** @brief 仅把正 offset 视作可用于字节区间比较的位置。 */
static TZrBool semantic_da_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

/** @brief source 或任一非零端点字段存在时，视为可参与 fact 归属匹配的范围。 */
static TZrBool semantic_da_range_is_known(const SZrFileRange *range) {
    if (range == ZR_NULL) {
        return ZR_FALSE;
    }

    return range->source != ZR_NULL ||
           range->start.line != 0 ||
           range->start.column != 0 ||
           range->start.offset != 0 ||
           range->end.line != 0 ||
           range->end.column != 0 ||
           range->end.offset != 0;
}

/**
 * @brief 在同一 source 内判断外层 AST 范围是否覆盖事实范围。
 * @note 两边都有 offset 信息时优先比较字节端点；否则退回行列端点。
 */
static TZrBool semantic_da_range_contains_range(const SZrFileRange *outer,
                                                const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !semantic_da_range_is_known(outer) ||
        !semantic_da_range_is_known(inner) ||
        !semantic_da_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }

    if ((semantic_da_has_offset(&outer->start) ||
         semantic_da_has_offset(&outer->end)) &&
        (semantic_da_has_offset(&inner->start) ||
         semantic_da_has_offset(&inner->end))) {
        return inner->start.offset >= outer->start.offset &&
               inner->end.offset <= outer->end.offset;
    }

    if (inner->start.line < outer->start.line ||
        inner->end.line > outer->end.line) {
        return ZR_FALSE;
    }
    if (inner->start.line == outer->start.line &&
        inner->start.column < outer->start.column) {
        return ZR_FALSE;
    }
    if (inner->end.line == outer->end.line &&
        inner->end.column > outer->end.column) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/** @brief 先用 AST 节点身份归属 fact；身份不同时以同 source 的范围覆盖作回退。 */
static TZrBool semantic_da_node_contains_fact(SZrAstNode *node,
                                              const SZrSemanticReferenceFact *fact) {
    if (node == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fact->node == node) {
        return ZR_TRUE;
    }
    return semantic_da_range_contains_range(&node->location, &fact->range);
}

/** @brief 在线性去重槽表中查找有效 symbol ID，并可选返回其状态向量索引。 */
static TZrBool semantic_da_symbol_map_find(const SZrSemanticDaSymbolMap *map,
                                           TZrSymbolId symbolId,
                                           TZrSize *outIndex) {
    TZrSize index;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (map == ZR_NULL ||
        !map->symbolIds.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (index = 0; index < map->symbolIds.length; index++) {
        TZrSymbolId *candidate =
                (TZrSymbolId *)ZrCore_Array_Get((SZrArray *)&map->symbolIds, index);
        if (candidate != ZR_NULL && *candidate == symbolId) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/**
 * @brief 将有效 symbol ID 至多加入一次，以保持每个事实符号对应一个状态槽。
 * @note 调用前必须已成功建立足够容量的数组；本函数把加入结果作为布尔成功返回。
 */
static TZrBool semantic_da_symbol_map_add(SZrState *state,
                                          SZrSemanticDaSymbolMap *map,
                                          TZrSymbolId symbolId) {
    if (state == ZR_NULL ||
        map == ZR_NULL ||
        !map->symbolIds.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (semantic_da_symbol_map_find(map, symbolId, ZR_NULL)) {
        return ZR_TRUE;
    }

    ZrCore_Array_Push(state, &map->symbolIds, &symbolId);
    return ZR_TRUE;
}

/**
 * @brief 从整个 semantic context 的 resolved declaration/read/write facts 建立共享状态槽表。
 * @note 初始容量按事实总数预留，去重后槽数不超过事实数。
 * BUG: Array_Init 分配失败仍留下 valid 且正 capacity；若有至少一个有效符号，后续 Push 会断言或向空缓冲写入。
 */
static TZrBool semantic_da_build_symbol_map(SZrSemanticContext *context,
                                            SZrSemanticDaSymbolMap *map) {
    TZrSize capacity;
    TZrSize index;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        map == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    capacity = context->referenceFacts.length > 0
                       ? context->referenceFacts.length
                       : ZR_PARSER_INITIAL_CAPACITY_TINY;
    ZrCore_Array_Init(context->state, &map->symbolIds, sizeof(TZrSymbolId), capacity);

    for (index = 0; index < context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index);
        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID) {
            continue;
        }
        if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
            fact->kind == ZR_SEMANTIC_REFERENCE_READ ||
            fact->kind == ZR_SEMANTIC_REFERENCE_WRITE) {
            if (!semantic_da_symbol_map_add(context->state, map, fact->symbolId)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

/** @brief 用创建映射时相同的 semantic state 释放本次 resolver 独占的槽表缓冲。 */
static void semantic_da_free_symbol_map(SZrSemanticContext *context,
                                        SZrSemanticDaSymbolMap *map) {
    if (context == ZR_NULL || context->state == ZR_NULL || map == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(context->state, &map->symbolIds);
}

/** @brief 比较用于变量声明槽回查的非空名称，优先利用相同字符串对象身份。 */
static TZrBool semantic_da_names_equal(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

/**
 * @brief 按事实数组顺序寻找第一个名称相同且已进入槽表的 resolved 符号。
 * BUG: 此查找扫描全 context facts 并取首个同名 resolved symbolId，不核对 declaration AST 身份或 callable scope；不同 callable scope 的同名变量 ID 不同，transfer 会重置并初始化错误槽，声明自身 READ 仍见 UNINIT，query 因而误报。
 */
static TZrBool semantic_da_find_symbol_by_name(const SZrSemanticDaAnalysis *analysis,
                                               SZrString *name,
                                               TZrSymbolId *outSymbolId,
                                               TZrSize *outSymbolIndex) {
    TZrSize index;

    if (outSymbolId != ZR_NULL) {
        *outSymbolId = ZR_SEMANTIC_ID_INVALID;
    }
    if (outSymbolIndex != ZR_NULL) {
        *outSymbolIndex = 0;
    }
    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid ||
        name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < analysis->context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(&analysis->context->referenceFacts, index);
        TZrSize symbolIndex;
        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_da_names_equal(fact->name, name) ||
            !semantic_da_symbol_map_find(analysis->symbols, fact->symbolId, &symbolIndex)) {
            continue;
        }

        if (outSymbolId != ZR_NULL) {
            *outSymbolId = fact->symbolId;
        }
        if (outSymbolIndex != ZR_NULL) {
            *outSymbolIndex = symbolIndex;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/** @brief 将三值数据流状态写回语义 fact；未知默认保守映射为 MAYBE_INIT。 */
static EZrSemanticDefiniteAssignmentState semantic_da_to_reference_state(
        EZrParserDefiniteAssignmentState state) {
    switch (state) {
        case ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT:
            return ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT;
        case ZR_PARSER_DEFINITE_ASSIGNMENT_INIT:
            return ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT;
        case ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT:
        default:
            return ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT;
    }
}

/**
 * @brief 将语义 fact 状态转成数据流域状态，UNKNOWN 与未识别值落到 MAYBE_INIT。
 * TODO: 全仓当前只有此定义而没有调用；确认它是待接入的回读边界还是可删的遗留 helper。
 */
static EZrParserDefiniteAssignmentState semantic_da_from_reference_state(
        EZrSemanticDefiniteAssignmentState state) {
    switch (state) {
        case ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT:
            return ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT;
        case ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT:
            return ZR_PARSER_DEFINITE_ASSIGNMENT_INIT;
        case ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT:
        case ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN:
        default:
            return ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT;
    }
}

/** @brief 同一 READ fact 多次经过 CFG 时保留相同状态，任意状态冲突归为 MAYBE_INIT。 */
static EZrParserDefiniteAssignmentState semantic_da_join_state(
        EZrParserDefiniteAssignmentState left,
        EZrParserDefiniteAssignmentState right) {
    return left == right ? left : ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT;
}

/** @brief 无 initializer 的变量声明从 UNINIT 开始，其余 statement 默认按 INIT。 */
static EZrParserDefiniteAssignmentState semantic_da_declaration_state(SZrAstNode *statement) {
    if (statement != ZR_NULL &&
        statement->type == ZR_AST_VARIABLE_DECLARATION &&
        statement->data.variableDeclaration.value == ZR_NULL) {
        return ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT;
    }
    return ZR_PARSER_DEFINITE_ASSIGNMENT_INIT;
}

/** @brief 处理变量声明 fact 时先标成 UNINIT，使 initializer 内的自读仍看见未赋值；其他声明沿用 statement 状态。 */
static EZrParserDefiniteAssignmentState semantic_da_declaration_fact_state(SZrAstNode *statement) {
    if (statement != ZR_NULL && statement->type == ZR_AST_VARIABLE_DECLARATION) {
        return ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT;
    }
    return semantic_da_declaration_state(statement);
}

/** @brief 以整张 symbol map 的槽数解释状态向量，并委托 Set 安全更新单个索引。 */
static void semantic_da_set_slot(void *state,
                                 const SZrSemanticDaAnalysis *analysis,
                                 TZrSize symbolIndex,
                                 EZrParserDefiniteAssignmentState value) {
    if (analysis == ZR_NULL || analysis->symbols == ZR_NULL) {
        return;
    }
    ZrParser_DefiniteAssignment_Set(state,
                                    analysis->symbols->symbolIds.length,
                                    symbolIndex,
                                    value);
}

/** @brief 同步更新当前 fact 的可查询状态和它对应的路径状态槽。 */
static void semantic_da_apply_fact_state(void *state,
                                         const SZrSemanticDaAnalysis *analysis,
                                         SZrSemanticReferenceFact *fact,
                                         EZrParserDefiniteAssignmentState value) {
    TZrSize symbolIndex;

    if (state == ZR_NULL ||
        analysis == ZR_NULL ||
        fact == ZR_NULL ||
        !semantic_da_symbol_map_find(analysis->symbols, fact->symbolId, &symbolIndex)) {
        return;
    }

    fact->definiteAssignmentState = semantic_da_to_reference_state(value);
    fact->hasDefiniteAssignmentState = ZR_TRUE;
    semantic_da_set_slot(state, analysis, symbolIndex, value);
}

/**
 * @brief 读取当前路径的 READ 状态，并按 fact 原始索引累计跨 worklist 访问的 join 结果。
 * @note readStates 与 readStateSeen 的长度须和同一次分析的 referenceFacts 快照一致。
 */
static void semantic_da_read_fact_state(void *state,
                                        const SZrSemanticDaAnalysis *analysis,
                                        SZrSemanticReferenceFact *fact,
                                        TZrSize factIndex) {
    TZrSize symbolIndex;
    EZrParserDefiniteAssignmentState value;

    if (state == ZR_NULL ||
        analysis == ZR_NULL ||
        fact == ZR_NULL ||
        !semantic_da_symbol_map_find(analysis->symbols, fact->symbolId, &symbolIndex)) {
        return;
    }

    value = ZrParser_DefiniteAssignment_Get(state,
                                            analysis->symbols->symbolIds.length,
                                            symbolIndex);
    if (analysis->readStates != ZR_NULL &&
        analysis->readStateSeen != ZR_NULL &&
        factIndex < analysis->readStateCount) {
        if (analysis->readStateSeen[factIndex]) {
            analysis->readStates[factIndex] =
                    semantic_da_join_state(analysis->readStates[factIndex], value);
        } else {
            analysis->readStates[factIndex] = value;
            analysis->readStateSeen[factIndex] = ZR_TRUE;
        }
    }
    fact->definiteAssignmentState = semantic_da_to_reference_state(value);
    fact->hasDefiniteAssignmentState = ZR_TRUE;
}

/** @brief 仅把成功 CFG 分析中见过的 READ 聚合状态写回相同事实索引。 */
static void semantic_da_apply_read_states(const SZrSemanticDaAnalysis *analysis) {
    TZrSize index;

    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid ||
        analysis->readStates == ZR_NULL ||
        analysis->readStateSeen == ZR_NULL) {
        return;
    }

    for (index = 0;
         index < analysis->readStateCount && index < analysis->context->referenceFacts.length;
         index++) {
        SZrSemanticReferenceFact *fact;

        if (!analysis->readStateSeen[index]) {
            continue;
        }

        fact = (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &analysis->context->referenceFacts,
                index);
        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_READ) {
            continue;
        }

        fact->definiteAssignmentState = semantic_da_to_reference_state(analysis->readStates[index]);
        fact->hasDefiniteAssignmentState = ZR_TRUE;
    }
}

/** @brief 只从 identifier-pattern 的变量声明抽取名称；解构 pattern 不在此处映射。 */
static SZrString *semantic_da_variable_declaration_name(SZrAstNode *statement) {
    SZrAstNode *pattern;

    if (statement == ZR_NULL || statement->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_NULL;
    }

    pattern = statement->data.variableDeclaration.pattern;
    if (pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    return pattern->data.identifier.name;
}

/** @brief 将当前简单变量声明名称转换为全局事实槽表中的一个状态索引。 */
static TZrBool semantic_da_variable_declaration_slot(SZrAstNode *statement,
                                                     const SZrSemanticDaAnalysis *analysis,
                                                     TZrSize *outSymbolIndex) {
    SZrString *name = semantic_da_variable_declaration_name(statement);
    TZrSymbolId symbolId;

    if (outSymbolIndex != ZR_NULL) {
        *outSymbolIndex = 0;
    }
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    return semantic_da_find_symbol_by_name(analysis, name, &symbolId, outSymbolIndex);
}

/**
 * @brief 按 statement 的已知表达式子节点或节点范围筛选本次 transfer 应消费的事实。
 * @note block、catch、switch-default 与 try 语句不直接归属事实；其他未列类型走节点/范围回退。
 * TODO: parser 将 lambda body 保存在 initializer 内，compiler 会编译 block 并产生 READ facts；resolver 只为 SCRIPT/FUNCTION/BLOCK 建 CFG root 且 default 直接返回，range fallback 可能把嵌套 body fact 归入外层 statement。先定闭包创建/调用的 DA 语义，再用合法嵌套 lambda fixture 核对 fact/query 边界。
 */
static TZrBool semantic_da_fact_in_statement(SZrAstNode *statement,
                                             const SZrSemanticReferenceFact *fact) {
    if (statement == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (statement->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_da_node_contains_fact(statement->data.variableDeclaration.pattern, fact) ||
                   semantic_da_node_contains_fact(statement->data.variableDeclaration.value, fact);
        case ZR_AST_EXPRESSION_STATEMENT:
            return semantic_da_node_contains_fact(statement->data.expressionStatement.expr, fact);
        case ZR_AST_RETURN_STATEMENT:
            return semantic_da_node_contains_fact(statement->data.returnStatement.expr, fact);
        case ZR_AST_THROW_STATEMENT:
            return semantic_da_node_contains_fact(statement->data.throwStatement.expr, fact);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return semantic_da_node_contains_fact(statement->data.breakContinueStatement.expr, fact);
        case ZR_AST_IF_EXPRESSION:
            return semantic_da_node_contains_fact(statement->data.ifExpression.condition, fact);
        case ZR_AST_WHILE_LOOP:
            return semantic_da_node_contains_fact(statement->data.whileLoop.cond, fact);
        case ZR_AST_FOR_LOOP:
            return semantic_da_node_contains_fact(statement->data.forLoop.cond, fact);
        case ZR_AST_FOREACH_LOOP:
            return semantic_da_node_contains_fact(statement->data.foreachLoop.expr, fact) ||
                   semantic_da_node_contains_fact(statement->data.foreachLoop.pattern, fact);
        case ZR_AST_SWITCH_EXPRESSION:
            return semantic_da_node_contains_fact(statement->data.switchExpression.expr, fact);
        case ZR_AST_SWITCH_CASE:
            return semantic_da_node_contains_fact(statement->data.switchCase.value, fact);
        case ZR_AST_BLOCK:
        case ZR_AST_CATCH_CLAUSE:
        case ZR_AST_SWITCH_DEFAULT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return ZR_FALSE;
        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_da_range_contains_range(&statement->data.functionDeclaration.nameLocation,
                                                    &fact->range);
        default:
            return semantic_da_node_contains_fact(statement, fact);
    }
}

/**
 * @brief 对一个 CFG statement 按引用事实顺序执行 declaration、read 与 write 状态转移。
 * @note 简单声明先重置 UNINIT，处理 initializer facts 后才按是否存在 initializer 置为 INIT。
 */
static void semantic_da_transfer_statement(SZrAstNode *statement, void *state, void *userData) {
    SZrSemanticDaAnalysis *analysis = (SZrSemanticDaAnalysis *)userData;
    TZrBool hasDeclarationSlot = ZR_FALSE;
    TZrSize declarationSlot = 0;
    TZrSize index;

    if (statement == ZR_NULL ||
        state == ZR_NULL ||
        analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid) {
        return;
    }

    hasDeclarationSlot = semantic_da_variable_declaration_slot(statement,
                                                               analysis,
                                                               &declarationSlot);
    if (hasDeclarationSlot) {
        semantic_da_set_slot(state,
                             analysis,
                             declarationSlot,
                             ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT);
    }

    for (index = 0; index < analysis->context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(&analysis->context->referenceFacts, index);
        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_da_fact_in_statement(statement, fact)) {
            continue;
        }

        switch (fact->kind) {
            case ZR_SEMANTIC_REFERENCE_READ:
                semantic_da_read_fact_state(state, analysis, fact, index);
                break;
            case ZR_SEMANTIC_REFERENCE_WRITE:
                semantic_da_apply_fact_state(state,
                                             analysis,
                                             fact,
                                             ZR_PARSER_DEFINITE_ASSIGNMENT_INIT);
                break;
            case ZR_SEMANTIC_REFERENCE_DECLARATION:
                semantic_da_apply_fact_state(state,
                                             analysis,
                                             fact,
                                             semantic_da_declaration_fact_state(statement));
                break;
            default:
                break;
        }
    }

    if (hasDeclarationSlot && statement->data.variableDeclaration.value != ZR_NULL) {
        semantic_da_set_slot(state,
                             analysis,
                             declarationSlot,
                             ZR_PARSER_DEFINITE_ASSIGNMENT_INIT);
    }
}

/**
 * @brief 为一个独立 CFG 根初始化整张 context 级符号向量。
 * TODO: 当前所有槽都从 INIT 开始，局部声明再由 transfer 重置；核对跨函数/捕获/模块符号是否都应视作该根入口已初始化。
 */
static void semantic_da_init_entry(void *state, void *userData) {
    SZrSemanticDaAnalysis *analysis = (SZrSemanticDaAnalysis *)userData;

    if (analysis == ZR_NULL || analysis->symbols == ZR_NULL) {
        return;
    }

    ZrParser_DefiniteAssignment_InitState(state,
                                          analysis->symbols->symbolIds.length,
                                          ZR_PARSER_DEFINITE_ASSIGNMENT_INIT);
}

/** @brief 把 definite-assignment 三值格的逐槽合并委托给通用 worklist 回调接口。 */
static TZrBool semantic_da_join(void *dst, const void *src, void *userData) {
    SZrSemanticDaAnalysis *analysis = (SZrSemanticDaAnalysis *)userData;

    if (analysis == ZR_NULL || analysis->symbols == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrParser_DefiniteAssignment_Join(dst,
                                            src,
                                            analysis->symbols->symbolIds.length);
}

/**
 * @brief 为一个 AST 分析根构建 CFG、运行前向数据流并在成功后提交聚合 READ 状态。
 * @note context、root 与 symbols 都是借用对象；本函数负责 CFG、result、两份 read 缓冲的分配和配对释放。
 * TODO: transfer 期间会即时改写 fact，而 LSP caller 忽略 FALSE 后仍物化/查询诊断；明确失败时部分 fact 是否可见及应由哪层截断发布。
 */
static TZrBool semantic_da_run_cfg_for_root(SZrSemanticContext *context,
                                            const SZrSemanticDaSymbolMap *symbols,
                                            SZrAstNode *root) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    SZrSemanticDaAnalysis semanticAnalysis;
    TZrBool ok;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        symbols == ZR_NULL ||
        root == ZR_NULL ||
        symbols->symbolIds.length == 0) {
        return ZR_TRUE;
    }

    semanticAnalysis.context = context;
    semanticAnalysis.symbols = symbols;
    semanticAnalysis.readStateCount = context->referenceFacts.length;
    semanticAnalysis.readStates = ZR_NULL;
    semanticAnalysis.readStateSeen = ZR_NULL;

/** readStates/readStateSeen 按 referenceFacts 的同一 index 空间并行分配，seen 区分未访问与状态枚举值。 */
    if (semanticAnalysis.readStateCount > 0) {
        semanticAnalysis.readStates =
                (EZrParserDefiniteAssignmentState *)ZrCore_Memory_RawMallocWithType(
                        context->state->global,
                        semanticAnalysis.readStateCount *
                                sizeof(EZrParserDefiniteAssignmentState),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
        semanticAnalysis.readStateSeen =
                (TZrBool *)ZrCore_Memory_RawMallocWithType(
                        context->state->global,
                        semanticAnalysis.readStateCount * sizeof(TZrBool),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (semanticAnalysis.readStates == ZR_NULL ||
            semanticAnalysis.readStateSeen == ZR_NULL) {
            if (semanticAnalysis.readStates != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        semanticAnalysis.readStates,
                        semanticAnalysis.readStateCount *
                                sizeof(EZrParserDefiniteAssignmentState),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            if (semanticAnalysis.readStateSeen != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        semanticAnalysis.readStateSeen,
                        semanticAnalysis.readStateCount * sizeof(TZrBool),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(semanticAnalysis.readStates,
                             0,
                             semanticAnalysis.readStateCount *
                                     sizeof(EZrParserDefiniteAssignmentState));
        ZrCore_Memory_RawSet(semanticAnalysis.readStateSeen,
                             0,
                             semanticAnalysis.readStateCount * sizeof(TZrBool));
    }

/** 三个回调共享 symbolIds.length 个枚举槽；userData 指向本栈帧对象，仅在同步 Run 调用期间有效。 */
    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = ZrParser_DefiniteAssignment_StateSize(symbols->symbolIds.length);
    analysis.initEntry = semantic_da_init_entry;
    analysis.join = semantic_da_join;
    analysis.transferStatement = semantic_da_transfer_statement;
    analysis.userData = &semanticAnalysis;

    ZrParser_Cfg_Init(context->state, &cfg);
    ZrParser_DataflowResult_Init(&result);
    ok = ZrParser_Cfg_BuildWithSemanticContext(
                 context->state, &cfg, root, context) &&
         ZrParser_Dataflow_Run(context->state, &cfg, &analysis, &result);
    if (ok) {
        semantic_da_apply_read_states(&semanticAnalysis);
    }
    ZrParser_DataflowResult_Free(context->state, &result);
    ZrParser_Cfg_Free(context->state, &cfg);
    if (semanticAnalysis.readStates != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                semanticAnalysis.readStates,
                semanticAnalysis.readStateCount * sizeof(EZrParserDefiniteAssignmentState),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (semanticAnalysis.readStateSeen != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                semanticAnalysis.readStateSeen,
                semanticAnalysis.readStateCount * sizeof(TZrBool),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ok;
}

static TZrBool semantic_da_resolve_node(SZrSemanticContext *context,
                                        const SZrSemanticDaSymbolMap *symbols,
                                        SZrAstNode *node);

/** @brief 只递归函数式 block 的直接语句；每个嵌套 function declaration 再建立自己的 CFG 根。 */
static TZrBool semantic_da_resolve_function_like_body(SZrSemanticContext *context,
                                                      const SZrSemanticDaSymbolMap *symbols,
                                                      SZrAstNode *body) {
    TZrSize index;

    if (body == ZR_NULL || body->type != ZR_AST_BLOCK || body->data.block.body == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < body->data.block.body->count; index++) {
        if (!semantic_da_resolve_node(context, symbols, body->data.block.body->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 仅为 script、function declaration 和 block 路径调度 CFG 根。
 * BUG: class/struct 本身落入 CFG 的普通 statement；范围回退可把成员方法局部声明和 READ 一并交给外层 transfer，而成员方法 body 没有独立 CFG 根。未初始化声明 fact 因外层 statement 类型而被标 INIT，query 因此不发应有诊断。合法方法体由 compiler_class/compiler_struct 编译，需补成员根遍历与端到端覆盖。
 */
static TZrBool semantic_da_resolve_node(SZrSemanticContext *context,
                                        const SZrSemanticDaSymbolMap *symbols,
                                        SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (!semantic_da_run_cfg_for_root(context, symbols, node)) {
                return ZR_FALSE;
            }
            if (node->data.script.statements == ZR_NULL) {
                return ZR_TRUE;
            }
            for (index = 0; index < node->data.script.statements->count; index++) {
                if (!semantic_da_resolve_node(context,
                                              symbols,
                                              node->data.script.statements->nodes[index])) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_AST_FUNCTION_DECLARATION:
            if (!semantic_da_run_cfg_for_root(context, symbols, node)) {
                return ZR_FALSE;
            }
            return semantic_da_resolve_function_like_body(context,
                                                         symbols,
                                                         node->data.functionDeclaration.body);
        case ZR_AST_BLOCK:
            return semantic_da_resolve_function_like_body(context, symbols, node);
        default:
            return ZR_TRUE;
    }
}

/**
 * @brief 用 AST 的前向 CFG 合并 resolved reference facts 的 definite-assignment 状态。
 * @pre context、referenceFacts 与 root 在整个调用期间保持有效；函数借用它们，并会修改匹配 fact 的状态字段。
 * @return 输入无效或实现显式检测到的 helper/CFG/dataflow 失败时返回 FALSE；未检测的 Array_Init 分配失败不保证在此返回，见上文 BUG。
 */
TZrBool ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(
        SZrSemanticContext *context,
        SZrAstNode *root) {
    SZrSemanticDaSymbolMap symbols;
    TZrBool ok;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        root == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&symbols.symbolIds);
    if (!semantic_da_build_symbol_map(context, &symbols)) {
        semantic_da_free_symbol_map(context, &symbols);
        return ZR_FALSE;
    }

    ok = semantic_da_resolve_node(context, &symbols, root);
    semantic_da_free_symbol_map(context, &symbols);
    return ok;
}
