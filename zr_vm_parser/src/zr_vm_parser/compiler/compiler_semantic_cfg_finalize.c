#include "compiler_internal.h"
#include "compiler_semantic_compare.h"
#include <stdlib.h>

/* Starting a graph at the end of a source body must not turn missing
 * SemanticIR producers into executable external/default values. This is a
 * conservative capability preflight for previously inactive entry bodies,
 * not an AST-to-CFG reconstruction. Existing active CFG producers retain
 * their own control-flow preflights. */
/**
 * @brief 为入口脚本的直线能力预检保存待访 AST 节点及顺序消费游标。
 * @note place 初始化位图只从更早的 SemanticIR 写操作推导；failed 表示预检资源不足，不能把不完整扫描误认成受支持。
 */
typedef struct SZrSemanticCfgSourceWorklist {
    const SZrAstNode **nodes;
    TZrBool *initializedPlaces;
    size_t length;
    size_t capacity;
    TZrSize scannedInstruction;
    TZrSize nextWriteInstruction;
    TZrBool failed;
} SZrSemanticCfgSourceWorklist;

/**
 * @brief 将 AST 节点加入显式 worklist，避免按用户表达式深度递归遍历。
 * @pre pending 为当前预检私有状态；节点可为空，空节点不参与扫描。
 * @note 分配失败只标记预检失败，由外层终止并释放 scratch，不转成分析成功。
 */
static void compiler_semantic_cfg_queue_node(
        SZrCompilerState *cs, SZrSemanticCfgSourceWorklist *pending,
        const SZrAstNode *node) {
    size_t capacity;
    const SZrAstNode **nodes;
    (void)cs;
    if (node == ZR_NULL || pending->failed) return;
    if (pending->length == pending->capacity) {
        capacity = pending->capacity == 0U ? ZR_PARSER_INITIAL_CAPACITY_SMALL
                                          : pending->capacity * 2U;
        if (capacity < pending->capacity || capacity > SIZE_MAX / sizeof(*nodes)) {
            pending->failed = ZR_TRUE;
            return;
        }
        nodes = (const SZrAstNode **)realloc(pending->nodes, capacity * sizeof(*nodes));
        if (nodes == ZR_NULL) {
            pending->failed = ZR_TRUE;
            return;
        }
        pending->nodes = nodes;
        pending->capacity = capacity;
    }
    pending->nodes[pending->length++] = node;
}

/** @brief 判断可选 AST 子节点表是否没有成员，供直线语法白名单筛选复合节点。
 * @return 空指针或零成员代表此可选列表不产生额外运行时工作。
 */
static TZrBool compiler_semantic_cfg_empty_nodes(const SZrAstNodeArray *nodes) {
    return (TZrBool)(nodes == ZR_NULL || nodes->count == 0U);
}

/**
 * @brief 判断 SemanticIR 指令源范围是否对应当前 AST 节点，用于将已有产物关联回源语法。
 * @pre instruction 与 node 均为非空有效对象，源位置来自同一次编译。
 */
static TZrBool compiler_semantic_cfg_same_source(
        const SZrSemanticIrInstruction *instruction, const SZrAstNode *node) {
    return (TZrBool)(instruction->sourceRange.source == node->location.source &&
            instruction->sourceRange.start.offset == node->location.start.offset &&
            instruction->sourceRange.end.offset == node->location.end.offset);
}

/**
 * @brief 从顺序游标之后查找指定节点对应的初始化/写入产物，并推进游标。
 * @note 游标按 worklist 保持的源码顺序消费，避免后续节点的写操作替前面的 AST 节点背书。
 */
static TZrBool compiler_semantic_cfg_has_source_write(
        const SZrCompilerState *cs, const SZrAstNode *node,
        EZrSemanticIrOpcode opcode,
        TZrSize *nextWriteInstruction) {
    TZrSize index;
    for (index = *nextWriteInstruction;
         index < cs->preSemanticIr.instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr, index);
        if (instruction != ZR_NULL && instruction->opcode == opcode &&
            compiler_semantic_cfg_same_source(instruction, node)) {
            *nextWriteInstruction = index + 1U;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/**
 * @brief 预检转换和存储输入是否具备后续执行 IR lowering 可消费的值与规范类型。
 * @return 发现缺失值、类型 token 不一致或非支持的跨类型转换时返回 false，使入口图留在分析用途。
 */
static TZrBool compiler_semantic_cfg_has_complete_value_types(
        const SZrCompilerState *cs) {
    TZrSize index;
    if (!compiler_semantic_compare_validate(cs)) return ZR_FALSE;
    for (index = 0U; index < cs->preSemanticIr.instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr, index);
        const SZrSemanticIrValue *input;
        if (instruction == ZR_NULL ||
            (instruction->opcode != ZR_SEMANTIC_IR_CONVERT &&
             instruction->opcode != ZR_SEMANTIC_IR_STORE)) continue;
        input = ZrParser_SemanticIr_Value(&cs->preSemanticIr, instruction->valueId);
        if (input == ZR_NULL) return ZR_FALSE;
        if (instruction->opcode == ZR_SEMANTIC_IR_CONVERT &&
            instruction->scalarConversionTypeToken != 0u) {
            const SZrCanonicalTypeNode *target = cs->semanticContext != ZR_NULL
                    ? ZrParser_CanonicalType_Find(
                            cs->semanticContext, instruction->typeId)
                    : ZR_NULL;
            if (target == ZR_NULL || target->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
                !ZR_VALUE_IS_TYPE_NUMBER(target->data.primitive.valueType) ||
                instruction->scalarConversionTypeToken !=
                        (TZrUInt32)target->data.primitive.valueType)
                return ZR_FALSE;
        }
        if (input->typeId != instruction->typeId) {
            const SZrCanonicalTypeNode *sourceType;
            const SZrCanonicalTypeNode *targetType;
            const SZrSemanticIrValue *result;
            if (instruction->opcode != ZR_SEMANTIC_IR_CONVERT ||
                cs->semanticContext == ZR_NULL) return ZR_FALSE;
            result = ZrParser_SemanticIr_Value(
                    &cs->preSemanticIr, instruction->resultValueId);
            if (result == ZR_NULL || result->typeId != instruction->typeId)
                return ZR_FALSE;
            sourceType = ZrParser_CanonicalType_Find(
                    cs->semanticContext, input->typeId);
            targetType = ZrParser_CanonicalType_Find(
                    cs->semanticContext, instruction->typeId);
            if (sourceType == ZR_NULL || targetType == ZR_NULL ||
                sourceType->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
                targetType->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
                !ZR_VALUE_IS_TYPE_NUMBER(sourceType->data.primitive.valueType) ||
                !ZR_VALUE_IS_TYPE_NUMBER(targetType->data.primitive.valueType) ||
                instruction->scalarConversionTypeToken == 0u)
                return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 判断给定 LOAD 之前，目标 place 是否已有同一前缀中的初始化或存储。
 * @pre placeId 是 SemanticIR place 图的一基有效编号；pending 在本次预检中复用。
 * @return 返回之前指令前缀内的初始化事实；scratch 分配失败同时设置 failed 以区分资源错误。
 */
static TZrBool compiler_semantic_cfg_place_initialized(
        const SZrCompilerState *cs, SZrSemanticCfgSourceWorklist *pending,
        TZrSize beforeInstruction, TZrPlaceId placeId) {
    TZrSize placeCount = cs->preSemanticIr.places.places.length;
    if (placeId == ZR_PLACE_ID_INVALID || placeId > placeCount ||
        placeCount > SIZE_MAX / sizeof(*pending->initializedPlaces)) return ZR_FALSE;
    if (pending->initializedPlaces == ZR_NULL) {
        pending->initializedPlaces = (TZrBool *)realloc(
                ZR_NULL, placeCount * sizeof(*pending->initializedPlaces));
        if (pending->initializedPlaces == ZR_NULL) {
            pending->failed = ZR_TRUE;
            return ZR_FALSE;
        }
        memset(pending->initializedPlaces, 0,
               placeCount * sizeof(*pending->initializedPlaces));
    }
    for (; pending->scannedInstruction < beforeInstruction;
         ++pending->scannedInstruction) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(
                        &cs->preSemanticIr, pending->scannedInstruction);
        if (instruction != ZR_NULL &&
            instruction->placeId != ZR_PLACE_ID_INVALID &&
            instruction->placeId <= placeCount &&
            (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE ||
             instruction->opcode == ZR_SEMANTIC_IR_STORE))
            pending->initializedPlaces[instruction->placeId - 1U] = ZR_TRUE;
    }
    return pending->initializedPlaces[placeId - 1U];
}

/**
 * @brief 为入口 AST 标识符读取寻找同源 LOAD，并要求该 LOAD 所读 place 已先初始化。
 * @note 仅证明有语义产物且局部位置可读；全局、闭包和子函数读取没有该路径的 producer 时由外层回退。
 */
static TZrBool compiler_semantic_cfg_has_source_load(
        const SZrCompilerState *cs, const SZrAstNode *node,
        TZrSize *nextInstruction, SZrSemanticCfgSourceWorklist *pending) {
    TZrSize index;
    for (index = *nextInstruction; index < cs->preSemanticIr.instructions.length;
         ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr, index);
        if (instruction != ZR_NULL && instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
            compiler_semantic_cfg_same_source(instruction, node)) {
            *nextInstruction = index + 1U;
            /* A closure declaration can provide a local LOAD while writing
             * only legacy ExecBC. Do not treat its place as initialized. */
            return compiler_semantic_cfg_place_initialized(
                    cs, pending, index, instruction->placeId);
        }
    }
    return ZR_FALSE;
}

/**
 * @brief 将直线预检明确支持的二元运算符映射到其 SemanticIR opcode。
 * @return 只接受加、减、乘；未知/空节点返回 INVALID，让调用方拒绝提升而不猜测语义。
 */
static EZrSemanticIrOpcode compiler_semantic_cfg_binary_opcode(
        const SZrAstNode *node) {
    const TZrChar *op;
    if (node == ZR_NULL || node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_SEMANTIC_IR_INVALID;
    }
    op = node->data.binaryExpression.op.op;
    if (op == ZR_NULL) return ZR_SEMANTIC_IR_INVALID;
    if (compiler_semantic_compare_source_supported(node))
        return ZR_SEMANTIC_IR_COMPARE;
    if (strcmp(op, "+") == 0) return ZR_SEMANTIC_IR_ADD;
    if (strcmp(op, "-") == 0) return ZR_SEMANTIC_IR_SUB;
    if (strcmp(op, "*") == 0) return ZR_SEMANTIC_IR_MUL;
    return ZR_SEMANTIC_IR_INVALID;
}

/**
 * @brief 确认 AST 二元表达式已有同源且形态完整的 SemanticIR 结果指令。
 * @note 此处只核对 opcode、双操作数和结果编号；可执行类型约束由 complete-value preflight 继续核对。
 */
static TZrBool compiler_semantic_cfg_has_source_binary(
        const SZrCompilerState *cs, const SZrAstNode *node) {
    EZrSemanticIrOpcode opcode = compiler_semantic_cfg_binary_opcode(node);
    TZrSize index;
    if (opcode == ZR_SEMANTIC_IR_INVALID) return ZR_FALSE;
    for (index = 0U; index < cs->preSemanticIr.instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr, index);
        if (instruction != ZR_NULL && instruction->opcode == opcode &&
            compiler_semantic_cfg_same_source(instruction, node)) {
            return (TZrBool)(instruction->operandCount == 2U &&
                             instruction->resultValueId != ZR_VALUE_ID_INVALID &&
                             (opcode != ZR_SEMANTIC_IR_COMPARE ||
                              instruction->comparisonPredicate ==
                                      (strcmp(node->data.binaryExpression.op.op, "<") == 0
                                       ? ZR_EXEC_IR_COMPARE_KIND_LESS
                                       : ZR_EXEC_IR_COMPARE_KIND_GREATER)));
        }
    }
    return ZR_FALSE;
}

/**
 * @brief 预检无活动 CFG 的顶层脚本是否能安全提升为可执行的直线 CFG。
 * @pre 调用者仅在 CFG inactive、startup 未阻止/抑制且尚未终止时考虑提升。
 * @return supported=false 表示保守转为分析图；worklist scratch 失败返回 false 并使验证停止。
 * @note 这是 producer 能力检查，不重建 AST 控制流，也不读取 ExecBC 来替缺失的 SemanticIR 产物背书。
 */
static TZrBool compiler_semantic_cfg_straight_line_is_supported(
        SZrCompilerState *cs, TZrBool *outSupported) {
    SZrSemanticCfgSourceWorklist pending = {0};
    TZrSize nextReadInstruction = 0U;
    TZrBool supported = ZR_TRUE;
    *outSupported = ZR_FALSE;
    if (cs->currentAst == ZR_NULL || cs->currentAst->type != ZR_AST_SCRIPT ||
        cs->currentFunctionNode != ZR_NULL) {
        return ZR_TRUE;
    }
    if (!compiler_semantic_cfg_has_complete_value_types(cs)) return ZR_TRUE;
    /* An explicit worklist avoids introducing another recursive walk over
     * user-controlled expression depth. No ExecBC instruction is consulted. */
    /* 反向压入 script 子项，使 LIFO worklist 仍按源码顺序核对写入与读取。 */
    compiler_semantic_cfg_queue_node(cs, &pending, cs->currentAst);
    while (supported && !pending.failed && pending.length != 0U) {
        const SZrAstNode *node = pending.nodes[--pending.length];
        /* 仅列出已能由 SemanticIR producer 完整表达的表面语法；默认拒绝提升。 */
        switch (node->type) {
            case ZR_AST_SCRIPT: {
                const SZrAstNodeArray *statements = node->data.script.statements;
                TZrSize index;
                if (statements != ZR_NULL) {
                    for (index = statements->count; index > 0U; --index)
                        compiler_semantic_cfg_queue_node(
                                cs, &pending, statements->nodes[index - 1U]);
                }
                break;
            }
            case ZR_AST_VARIABLE_DECLARATION:
                supported = (TZrBool)(node->data.variableDeclaration.pattern != ZR_NULL &&
                        node->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
                        node->data.variableDeclaration.value != ZR_NULL &&
                        compiler_semantic_cfg_has_source_write(
                                cs, node->data.variableDeclaration.pattern,
                                ZR_SEMANTIC_IR_INITIALIZE,
                                &pending.nextWriteInstruction));
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.variableDeclaration.value);
                break;
            case ZR_AST_EXPRESSION_STATEMENT:
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.expressionStatement.expr);
                break;
            case ZR_AST_BOOLEAN_LITERAL:
            case ZR_AST_INTEGER_LITERAL:
            case ZR_AST_FLOAT_LITERAL:
            case ZR_AST_STRING_LITERAL:
            case ZR_AST_CHAR_LITERAL:
            case ZR_AST_NULL_LITERAL:
                break;
            case ZR_AST_IDENTIFIER_LITERAL:
                /* Only a source read with a real local LOAD is complete.
                 * Global, closure, child-function and type identifier reads
                 * currently emit legacy bytecode without a SemanticIR load. */
                supported = compiler_semantic_cfg_has_source_load(
                    cs, node, &nextReadInstruction, &pending);
                break;
            case ZR_AST_BINARY_EXPRESSION:
                supported = compiler_semantic_cfg_has_source_binary(
                        cs, node);
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.binaryExpression.right);
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.binaryExpression.left);
                break;
            case ZR_AST_PRIMARY_EXPRESSION:
                supported = compiler_semantic_cfg_empty_nodes(
                        node->data.primaryExpression.members);
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.primaryExpression.property);
                break;
            case ZR_AST_ASSIGNMENT_EXPRESSION:
                supported = (TZrBool)(node->data.assignmentExpression.op.op != ZR_NULL &&
                        strcmp(node->data.assignmentExpression.op.op, "=") == 0 &&
                        node->data.assignmentExpression.left != ZR_NULL &&
                        node->data.assignmentExpression.left->type == ZR_AST_IDENTIFIER_LITERAL &&
                        compiler_semantic_cfg_has_source_write(
                                cs, node->data.assignmentExpression.left,
                                ZR_SEMANTIC_IR_STORE,
                                &pending.nextWriteInstruction));
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.assignmentExpression.right);
                break;
            case ZR_AST_CONSTRUCT_EXPRESSION:
                /* An ownership wrapper over an existing local has a producer;
                 * new/resource construction first emits an instance and may
                 * invoke an initializer only in the legacy instruction path. */
                supported = (TZrBool)(
                        node->data.constructExpression.builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE &&
                        !node->data.constructExpression.isNew &&
                        !node->data.constructExpression.isResourceSurface &&
                        compiler_semantic_cfg_empty_nodes(node->data.constructExpression.args) &&
                        node->data.constructExpression.target != ZR_NULL &&
                        node->data.constructExpression.target->type == ZR_AST_IDENTIFIER_LITERAL);
                break;
            case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION:
                /* The intrinsic consumes its canonical identifier place;
                 * it does not compile the argument as an expression read. */
                supported = (TZrBool)(
                        node->data.ownershipIntrinsicExpression.argument != ZR_NULL &&
                        node->data.ownershipIntrinsicExpression.argument->type ==
                                ZR_AST_IDENTIFIER_LITERAL);
                break;
            case ZR_AST_FUNCTION_DECLARATION:
                /* The child body is isolated, but a named declaration still
                 * emits CREATE_CLOSURE and SET_STACK in the parent ExecBC. */
                supported = ZR_FALSE;
                break;
            case ZR_AST_CLASS_DECLARATION:
                /* Empty type declarations have no entry runtime initializer.
                 * Never silently skip static fields, decorators or members. */
                supported = (TZrBool)(
                        node->data.classDeclaration.generic == ZR_NULL &&
                        compiler_semantic_cfg_empty_nodes(node->data.classDeclaration.members) &&
                        compiler_semantic_cfg_empty_nodes(node->data.classDeclaration.inherits) &&
                        compiler_semantic_cfg_empty_nodes(node->data.classDeclaration.decorators));
                break;
            default:
                supported = ZR_FALSE;
                break;
        }
    }
    free(pending.nodes);
    free(pending.initializedPlaces);
    *outSupported = (TZrBool)(supported && !pending.failed);
    return (TZrBool)!pending.failed;
}

/**
 * @brief 为未提升或不支持的入口脚本构造仅供语义分析消费的保守总图。
 * @pre 当前不存在活动源 CFG；现存语义指令会放入单个 entry 范围。
 * @return CFG 块、边或范围绑定失败时返回 false。
 * @note 合成 RETURN/EXIT 边只让分析有完整入口/出口；它不是执行终结符，严格 ExecIR builder 必须拒绝。
 */
static TZrBool compiler_semantic_cfg_build_analysis_graph(SZrCompilerState *cs) {
    SZrSemanticIrFunction *function = &cs->preSemanticIr;
    TZrUInt32 entryBlock, exitBlock;
    ZrParser_Cfg_Free(cs->state, &function->cfg);
    ZrParser_Cfg_Init(cs->state, &function->cfg);
    entryBlock = ZrParser_Cfg_AppendBlock(
            cs->state, &function->cfg, ZR_PARSER_CFG_BLOCK_ENTRY, ZR_NULL);
    exitBlock = ZrParser_Cfg_AppendBlock(
            cs->state, &function->cfg, ZR_PARSER_CFG_BLOCK_EXIT, ZR_NULL);
    if (entryBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        exitBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    function->cfg.entryBlockId = entryBlock;
    function->cfg.exitBlockId = exitBlock;
    return (TZrBool)(ZrParser_Cfg_Connect(
                &function->cfg, entryBlock, exitBlock,
                ZR_PARSER_CFG_EDGE_RETURN, ZR_NULL) &&
            ZrParser_SemanticIr_BindBlockRange(
                function, &function->cfg, entryBlock, 0U,
                (TZrUInt32)function->instructions.length,
                ZR_PARSER_CFG_TERMINATOR_RETURN) &&
            ZrParser_SemanticIr_BindBlockRange(
                function, &function->cfg, exitBlock,
                (TZrUInt32)function->instructions.length, 0U,
                ZR_PARSER_CFG_TERMINATOR_EXIT));
}

/**
 * @brief 将已通过能力预检的顶层直线指令前缀事务式提升为活动入口 CFG。
 * @pre CFG 尚未激活，预检已证明每个运行时相关 AST 单元有可执行 SemanticIR producer。
 * @return 重新绑定分块或补隐式出口失败时恢复原图及 append-only 序列长度，并返回 false。
 * @note 成功后丢弃旧 CFG 容器；SemanticIR 指令不重编译，只补块范围及隐式函数出口。
 */
static TZrBool compiler_semantic_cfg_promote_straight_line(SZrCompilerState *cs) {
    SZrSemanticIrFunction *function = &cs->preSemanticIr;
    SZrParserCfg previousCfg = function->cfg;
    TZrSize previousInstructions = function->instructions.length;
    TZrSize previousSourceMap = function->sourceMap.length;
    TZrSize previousOperands = function->valueOperands.length;
    TZrUInt32 previousBlock = cs->preSemanticIrCfgBlock;
    TZrUInt32 previousStart = cs->preSemanticIrCfgStart;
    TZrBool previousValidated = cs->preSemanticIrValidated;

    /* 新图构造失败时下面的 rollback 恢复旧分析图和所有已追加序列边界。 */
    ZrParser_Cfg_Init(cs->state, &function->cfg);
    if (!compiler_semantic_cfg_ensure_active(cs) ||
        !compiler_semantic_cfg_finish(cs)) {
        ZrParser_Cfg_Free(cs->state, &function->cfg);
        function->cfg = previousCfg;
        function->instructions.length = previousInstructions;
        function->sourceMap.length = previousSourceMap;
        function->valueOperands.length = previousOperands;
        cs->preSemanticIrCfgBlock = previousBlock;
        cs->preSemanticIrCfgStart = previousStart;
        cs->preSemanticIrCfgActive = ZR_FALSE;
        cs->preSemanticIrValidated = previousValidated;
        return ZR_FALSE;
    }
    ZrParser_Cfg_Free(cs->state, &previousCfg);
    return ZR_TRUE;
}

typedef struct SZrCompilerSemanticCfgFinishTransaction {
    SZrCompilerState stagedCompiler;
    TZrBool ownsCfg;
    TZrBool ownsInstructions;
    TZrBool ownsSourceMap;
    TZrBool ownsValueOperands;
} SZrCompilerSemanticCfgFinishTransaction;

/* Clone a core array into independently owned storage and reserve enough room
 * for the finalizer's bounded append set.  Do not use Array_Init/Append here:
 * those void helpers cannot report allocator failure to this transaction. */
static TZrBool compiler_semantic_cfg_clone_array(
        SZrState *state, const SZrArray *source, TZrSize extraCapacity,
        SZrArray *destination) {
    TZrSize capacity;
    TZrSize byteCount;
    TZrSize copyBytes;

    if (destination == ZR_NULL) return ZR_FALSE;
    memset(destination, 0, sizeof(*destination));
    if (state == ZR_NULL || state->global == ZR_NULL || source == ZR_NULL ||
        !source->isValid || source->head == ZR_NULL ||
        source->elementSize == 0U || source->length > source->capacity ||
        extraCapacity > SIZE_MAX - source->length ||
        source->capacity > SIZE_MAX / source->elementSize) {
        return ZR_FALSE;
    }

    capacity = source->length + extraCapacity;
    if (capacity < source->capacity) capacity = source->capacity;
    if (capacity == 0U) capacity = 1U;
    if (capacity > SIZE_MAX / source->elementSize) return ZR_FALSE;
    byteCount = capacity * source->elementSize;
    copyBytes = source->length * source->elementSize;
    destination->head = ZR_CAST_UINT8_PTR(ZrCore_Memory_RawMallocWithType(
            state->global, byteCount, ZR_MEMORY_NATIVE_TYPE_ARRAY));
    if (destination->head == ZR_NULL) return ZR_FALSE;
    destination->elementSize = source->elementSize;
    destination->length = source->length;
    destination->capacity = capacity;
    destination->isValid = ZR_TRUE;
    ZrCore_Memory_RawCopy(destination->head, source->head, copyBytes);
    return ZR_TRUE;
}

/* A Cfg value contains owned block and per-block edge arrays.  Copying only
 * the outer SZrParserCfg would retain aliases into the graph being finalized. */
static TZrBool compiler_semantic_cfg_clone_cfg(
        SZrState *state, const SZrParserCfg *source,
        SZrParserCfg *destination) {
    TZrSize blockIndex;

    if (destination == ZR_NULL) return ZR_FALSE;
    memset(destination, 0, sizeof(*destination));
    if (state == ZR_NULL || source == ZR_NULL || source->state != state ||
        !source->blocks.isValid || source->blocks.head == ZR_NULL ||
        source->blocks.elementSize != sizeof(SZrParserCfgBlock)) {
        return ZR_FALSE;
    }

    destination->state = source->state;
    destination->semanticContext = source->semanticContext;
    destination->entryBlockId = source->entryBlockId;
    destination->exitBlockId = source->exitBlockId;
    if (!compiler_semantic_cfg_clone_array(
                state, &source->blocks, 1U, &destination->blocks)) {
        memset(destination, 0, sizeof(*destination));
        return ZR_FALSE;
    }

    /* The raw block copy temporarily contains source edge pointers.  Clear
     * every one before a clone can fail and enter the common owned cleanup. */
    for (blockIndex = 0U; blockIndex < destination->blocks.length;
         ++blockIndex) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(
                &destination->blocks, blockIndex);
        memset(&block->outgoingEdges, 0, sizeof(block->outgoingEdges));
    }
    for (blockIndex = 0U; blockIndex < source->blocks.length; ++blockIndex) {
        const SZrParserCfgBlock *sourceBlock =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&source->blocks, blockIndex);
        SZrParserCfgBlock *destinationBlock =
                (SZrParserCfgBlock *)ZrCore_Array_Get(
                        &destination->blocks, blockIndex);
        if (sourceBlock == ZR_NULL || destinationBlock == ZR_NULL ||
            sourceBlock->outgoingEdges.elementSize !=
                    sizeof(SZrParserCfgEdge) ||
            !compiler_semantic_cfg_clone_array(
                    state, &sourceBlock->outgoingEdges, 1U,
                    &destinationBlock->outgoingEdges)) {
            ZrParser_Cfg_Free(state, destination);
            memset(destination, 0, sizeof(*destination));
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void compiler_semantic_cfg_finish_transaction_dispose(
        SZrCompilerSemanticCfgFinishTransaction *transaction) {
    SZrState *state;
    if (transaction == ZR_NULL) return;
    state = transaction->stagedCompiler.state;
    if (transaction->ownsCfg) {
        ZrParser_Cfg_Free(
                state, &transaction->stagedCompiler.preSemanticIr.cfg);
    }
    if (transaction->ownsInstructions) {
        ZrCore_Array_Free(state,
                &transaction->stagedCompiler.preSemanticIr.instructions);
    }
    if (transaction->ownsSourceMap) {
        ZrCore_Array_Free(state,
                &transaction->stagedCompiler.preSemanticIr.sourceMap);
    }
    if (transaction->ownsValueOperands) {
        ZrCore_Array_Free(state,
                &transaction->stagedCompiler.preSemanticIr.valueOperands);
    }
    memset(transaction, 0, sizeof(*transaction));
}

static TZrBool compiler_semantic_cfg_finish_transaction_prepare(
        SZrCompilerState *compiler,
        SZrCompilerSemanticCfgFinishTransaction *transaction) {
    SZrParserCfg cfgCopy;
    SZrArray instructionCopy;
    SZrArray sourceMapCopy;
    SZrArray operandCopy;

    if (compiler == ZR_NULL || transaction == ZR_NULL) return ZR_FALSE;
    memset(transaction, 0, sizeof(*transaction));
    memset(&cfgCopy, 0, sizeof(cfgCopy));
    memset(&instructionCopy, 0, sizeof(instructionCopy));
    memset(&sourceMapCopy, 0, sizeof(sourceMapCopy));
    memset(&operandCopy, 0, sizeof(operandCopy));
    transaction->stagedCompiler = *compiler;

    if (!compiler_semantic_cfg_clone_cfg(
                compiler->state, &compiler->preSemanticIr.cfg, &cfgCopy)) {
        return ZR_FALSE;
    }
    transaction->stagedCompiler.preSemanticIr.cfg = cfgCopy;
    transaction->ownsCfg = ZR_TRUE;
    if (!compiler_semantic_cfg_clone_array(
                compiler->state, &compiler->preSemanticIr.instructions,
                2U, &instructionCopy)) {
        compiler_semantic_cfg_finish_transaction_dispose(transaction);
        return ZR_FALSE;
    }
    transaction->stagedCompiler.preSemanticIr.instructions = instructionCopy;
    transaction->ownsInstructions = ZR_TRUE;
    if (!compiler_semantic_cfg_clone_array(
                compiler->state, &compiler->preSemanticIr.sourceMap,
                2U, &sourceMapCopy)) {
        compiler_semantic_cfg_finish_transaction_dispose(transaction);
        return ZR_FALSE;
    }
    transaction->stagedCompiler.preSemanticIr.sourceMap = sourceMapCopy;
    transaction->ownsSourceMap = ZR_TRUE;
    if (!compiler_semantic_cfg_clone_array(
                compiler->state, &compiler->preSemanticIr.valueOperands,
                2U, &operandCopy)) {
        compiler_semantic_cfg_finish_transaction_dispose(transaction);
        return ZR_FALSE;
    }
    transaction->stagedCompiler.preSemanticIr.valueOperands = operandCopy;
    transaction->ownsValueOperands = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_finish_staged_state_is_valid(
        const SZrCompilerState *compiler) {
    const SZrSemanticIrFunction *function;
    const SZrParserCfg *cfg;
    TZrSize blockIndex;

    if (compiler == ZR_NULL) return ZR_FALSE;
    function = &compiler->preSemanticIr;
    cfg = &function->cfg;
    if (!compiler->preSemanticIrCfgActive ||
        compiler->preSemanticIrCfgBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !cfg->blocks.isValid || cfg->blocks.head == ZR_NULL ||
        cfg->blocks.elementSize != sizeof(SZrParserCfgBlock) ||
        cfg->blocks.length > cfg->blocks.capacity ||
        cfg->entryBlockId >= cfg->blocks.length ||
        cfg->exitBlockId >= cfg->blocks.length ||
        !function->instructions.isValid ||
        function->instructions.head == ZR_NULL ||
        function->instructions.elementSize !=
                sizeof(SZrSemanticIrInstruction) ||
        function->instructions.length > function->instructions.capacity ||
        !function->sourceMap.isValid || function->sourceMap.head == ZR_NULL ||
        function->sourceMap.elementSize !=
                sizeof(SZrSemanticIrSourceMapEntry) ||
        function->sourceMap.length > function->sourceMap.capacity ||
        function->sourceMap.length != function->instructions.length ||
        !function->valueOperands.isValid ||
        function->valueOperands.head == ZR_NULL ||
        function->valueOperands.elementSize != sizeof(TZrValueId) ||
        function->valueOperands.length > function->valueOperands.capacity) {
        return ZR_FALSE;
    }
    for (blockIndex = 0U; blockIndex < cfg->blocks.length; ++blockIndex) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)
                ZrCore_Array_Get((SZrArray *)&cfg->blocks, blockIndex);
        if (block == ZR_NULL || block->id != blockIndex ||
            !block->outgoingEdges.isValid ||
            block->outgoingEdges.head == ZR_NULL ||
            block->outgoingEdges.elementSize != sizeof(SZrParserCfgEdge) ||
            block->outgoingEdges.length > block->outgoingEdges.capacity ||
            block->successorCount != block->outgoingEdges.length) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void compiler_semantic_cfg_finish_transaction_commit(
        SZrCompilerState *compiler,
        SZrCompilerSemanticCfgFinishTransaction *transaction) {
    SZrSemanticIrFunction *destination = &compiler->preSemanticIr;
    SZrSemanticIrFunction *staged =
            &transaction->stagedCompiler.preSemanticIr;

    /* All replacements were allocated and finalized on the staged copy, so
     * this publication path performs no allocation and cannot expose a prefix. */
    ZrParser_Cfg_Free(compiler->state, &destination->cfg);
    ZrCore_Array_Free(compiler->state, &destination->instructions);
    ZrCore_Array_Free(compiler->state, &destination->sourceMap);
    ZrCore_Array_Free(compiler->state, &destination->valueOperands);
    destination->cfg = staged->cfg;
    destination->instructions = staged->instructions;
    destination->sourceMap = staged->sourceMap;
    destination->valueOperands = staged->valueOperands;
    memset(&staged->cfg, 0, sizeof(staged->cfg));
    memset(&staged->instructions, 0, sizeof(staged->instructions));
    memset(&staged->sourceMap, 0, sizeof(staged->sourceMap));
    memset(&staged->valueOperands, 0, sizeof(staged->valueOperands));
    transaction->ownsCfg = ZR_FALSE;
    transaction->ownsInstructions = ZR_FALSE;
    transaction->ownsSourceMap = ZR_FALSE;
    transaction->ownsValueOperands = ZR_FALSE;
    compiler->preSemanticIrCfgActive =
            transaction->stagedCompiler.preSemanticIrCfgActive;
    compiler->preSemanticIrCfgTerminated =
            transaction->stagedCompiler.preSemanticIrCfgTerminated;
    compiler->preSemanticIrCfgBlock =
            transaction->stagedCompiler.preSemanticIrCfgBlock;
    compiler->preSemanticIrCfgStart =
            transaction->stagedCompiler.preSemanticIrCfgStart;
    compiler->preSemanticIrValidated =
            transaction->stagedCompiler.preSemanticIrValidated;
}

static TZrBool compiler_semantic_cfg_finish_active_transaction(
        SZrCompilerState *compiler) {
    SZrCompilerSemanticCfgFinishTransaction transaction;

    if (compiler == ZR_NULL || !compiler->preSemanticIrCfgActive) {
        return ZR_FALSE;
    }
    if (compiler->preSemanticIrCfgBlock ==
        ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return compiler_semantic_cfg_finish(compiler);
    }
    if (!compiler_semantic_cfg_finish_transaction_prepare(
                compiler, &transaction)) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_cfg_finish(&transaction.stagedCompiler) ||
        !compiler_semantic_cfg_finish_staged_state_is_valid(
                &transaction.stagedCompiler)) {
        compiler_semantic_cfg_finish_transaction_dispose(&transaction);
        return ZR_FALSE;
    }
    compiler_semantic_cfg_finish_transaction_commit(compiler, &transaction);
    compiler_semantic_cfg_finish_transaction_dispose(&transaction);
    return ZR_TRUE;
}

/**
 * @brief 在 SemanticIR 验证前收束源 CFG，或为入口脚本选择可执行提升/分析专用总图。
 * @pre 由 ValidatePreSemanticIr 在初始化后的编译器状态上调用；源编译器已写出本阶段可见的 SemanticIR。
 * @return 活动 CFG 的出口绑定、能力预检、CFG 构造或提升失败时返回 false，调用者不得继续标记 IR 已验证。
 * @note active 路径保留可选/调用/异常边并只补函数出口；inactive 且能力不足或启动被阻止时绝不伪造可执行 CFG。
 */
TZrBool compiler_semantic_cfg_finalize(SZrCompilerState *cs) {
    TZrBool supported = ZR_FALSE;
    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) return ZR_FALSE;
    /*
     * optional、invoke 与异常边由各自的控制流 lowering 先行落图；此处只
     * 关闭当前延续并补函数出口，不能重建或覆盖上游已绑定的分支关系。
     */
    if (cs->preSemanticIrCfgActive)
        return compiler_semantic_cfg_finish_active_transaction(cs);
    if (!cs->preSemanticIrCfgStartupBlocked &&
        !cs->preSemanticIrCfgStartupSuppressed &&
        !cs->preSemanticIrCfgTerminated) {
        if (!compiler_semantic_cfg_straight_line_is_supported(cs, &supported))
            return ZR_FALSE;
    }
    if (supported) {
        return compiler_semantic_cfg_promote_straight_line(cs);
    }
    return compiler_semantic_cfg_build_analysis_graph(cs);
}
