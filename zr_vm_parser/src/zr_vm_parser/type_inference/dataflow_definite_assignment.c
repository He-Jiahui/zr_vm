#include "dataflow_definite_assignment.h"

/* definite-assignment 的 join 将冲突路径压到保守顶态，避免把单一路径写入当成必然赋值。 */
static EZrParserDefiniteAssignmentState definite_assignment_join_value(
        EZrParserDefiniteAssignmentState left,
        EZrParserDefiniteAssignmentState right) {
    if (left == right) {
        return left;
    }
    return ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT;
}

/* 返回逐符号状态向量的字节长度，供通用 dataflow 为每块分配快照。 */
/* TODO: 核实 context 级符号表相对单 CFG 块上限的可达长度、symbol ID/entry 上限、size_t/enum ABI 与双缓冲分配前置，并审查导出 helper 外部契约；未证实合法回绕可达 initEntry 前不定 BUG。 */
TZrSize ZrParser_DefiniteAssignment_StateSize(TZrSize symbolCount) {
    return symbolCount * sizeof(EZrParserDefiniteAssignmentState);
}

/* 初始化调用方提供的完整符号向量；缓冲由通用引擎持有，本函数只写入枚举值。 */
void ZrParser_DefiniteAssignment_InitState(
        void *state,
        TZrSize symbolCount,
        EZrParserDefiniteAssignmentState initialState) {
    EZrParserDefiniteAssignmentState *states = (EZrParserDefiniteAssignmentState *)state;
    TZrSize index;

    if (states == ZR_NULL) {
        return;
    }

    for (index = 0; index < symbolCount; index++) {
        states[index] = initialState;
    }
}

/* 越界读取采用 MAYBE_INIT，保守地避免缺少槽位时误报为确定未初始化或已初始化。 */
EZrParserDefiniteAssignmentState ZrParser_DefiniteAssignment_Get(
        const void *state,
        TZrSize symbolCount,
        TZrSize symbolIndex) {
    const EZrParserDefiniteAssignmentState *states =
            (const EZrParserDefiniteAssignmentState *)state;

    if (states == ZR_NULL || symbolIndex >= symbolCount) {
        return ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT;
    }

    return states[symbolIndex];
}

/* 仅更新已分配向量中的有效槽；拒绝越界索引，避免写到调用方状态向量之外。 */
void ZrParser_DefiniteAssignment_Set(
        void *state,
        TZrSize symbolCount,
        TZrSize symbolIndex,
        EZrParserDefiniteAssignmentState value) {
    EZrParserDefiniteAssignmentState *states = (EZrParserDefiniteAssignmentState *)state;

    if (states == ZR_NULL || symbolIndex >= symbolCount) {
        return;
    }

    states[symbolIndex] = value;
}

/* 对逐符号向量应用同一格 join；返回值供数据流 worklist 判断是否需要重访。 */
TZrBool ZrParser_DefiniteAssignment_Join(
        void *dst,
        const void *src,
        TZrSize symbolCount) {
    EZrParserDefiniteAssignmentState *dstStates = (EZrParserDefiniteAssignmentState *)dst;
    const EZrParserDefiniteAssignmentState *srcStates =
            (const EZrParserDefiniteAssignmentState *)src;
    TZrBool changed = ZR_FALSE;
    TZrSize index;

    if (dstStates == ZR_NULL || srcStates == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < symbolCount; index++) {
        EZrParserDefiniteAssignmentState joined =
                definite_assignment_join_value(dstStates[index], srcStates[index]);
        if (joined != dstStates[index]) {
            dstStates[index] = joined;
            changed = ZR_TRUE;
        }
    }

    return changed;
}
