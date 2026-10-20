#include "zr_vm_lib_debug/coverage.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"

/* hook 回调只有 state 参数，因此用进程内活动表找回该 state 的 Coverage 会话。 */
/* TODO: 活动表为无锁链表；核对多 state 并发运行时 hook 与 Start/Stop 的线程约束，
 * 并用并发采集测试决定是否需要互斥或按 state 存储的会话指针。 */
static ZrDebugCoverage *g_active_coverages = ZR_NULL;

/* hook 期间借用当前栈帧的名称和源信息，随后复制到 CoverageLine 快照。 */
typedef struct ZrDebugCoverageLocation {
    const SZrFunction *function;
    const TZrChar *name;
    const TZrChar *source;
    TZrUInt32 line;
} ZrDebugCoverageLocation;

static void zr_debug_coverage_copy_text(TZrChar *destination, TZrSize destinationSize, const TZrChar *source) {
    TZrSize length;

    if (destination == ZR_NULL || destinationSize == 0u) {
        return;
    }

    if (source == ZR_NULL) {
        source = "";
    }

    length = strlen(source);
    if (length >= destinationSize) {
        length = destinationSize - 1u;
    }
    if (length > 0u) {
        memcpy(destination, source, length);
    }
    destination[length] = '\0';
}

/* 仅把原 hook 原先订阅的事件转发给它，避免 Coverage 扩展 LINE mask 改变旧观察者语义。 */
static TZrUInt32 zr_debug_coverage_event_mask(EZrDebugHookEvent event) {
    if (event >= ZR_DEBUG_HOOK_EVENT_MAX) {
        return 0u;
    }
    return 1u << event;
}

static const TZrChar *zr_debug_coverage_function_name(const SZrFunction *function) {
    if (function == ZR_NULL || function->functionName == ZR_NULL) {
        return "<anonymous>";
    }
    return ZrCore_String_GetNativeString(function->functionName);
}

static const TZrChar *zr_debug_coverage_function_source(const SZrFunction *function) {
    if (function == ZR_NULL || function->sourceCodeList == ZR_NULL) {
        return "";
    }
    return ZrCore_String_GetNativeString(function->sourceCodeList);
}

static ZrDebugCoverage *zr_debug_coverage_find_active(SZrState *state) {
    ZrDebugCoverage *coverage;

    for (coverage = g_active_coverages; coverage != ZR_NULL; coverage = coverage->next_active) {
        if (coverage->state == state) {
            return coverage;
        }
    }

    return ZR_NULL;
}

/* 同一个 state 只对应一个 Coverage，hook 才能无歧义地定位其行表。 */
static TZrBool zr_debug_coverage_register_active(ZrDebugCoverage *coverage) {
    if (coverage == ZR_NULL || coverage->state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (zr_debug_coverage_find_active(coverage->state) != ZR_NULL) {
        return ZR_FALSE;
    }

    coverage->next_active = g_active_coverages;
    g_active_coverages = coverage;
    return ZR_TRUE;
}

static void zr_debug_coverage_unregister_active(ZrDebugCoverage *coverage) {
    ZrDebugCoverage **cursor = &g_active_coverages;

    while (*cursor != ZR_NULL) {
        if (*cursor == coverage) {
            *cursor = coverage->next_active;
            coverage->next_active = ZR_NULL;
            return;
        }
        cursor = &(*cursor)->next_active;
    }
}

/* 行结果随入口函数树和执行期 LINE hook 增长；注册时的扩容失败返回调用方，
 * hook 路径的扩容失败会静默少报该行。 */
/* TODO: 倍增容量和 sizeof(*lines) * newCapacity 未查溢出；核对 TZrSize 上限与
 * 超大函数树的可达性，再决定是否补受控失败路径。 */
static TZrBool zr_debug_coverage_reserve_lines(ZrDebugCoverage *coverage, TZrSize minimumCapacity) {
    ZrDebugCoverageLine *lines;
    TZrSize newCapacity;

    if (coverage == ZR_NULL) {
        return ZR_FALSE;
    }
    if (coverage->line_capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = coverage->line_capacity == 0u ? 32u : coverage->line_capacity * 2u;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    lines = (ZrDebugCoverageLine *)realloc(coverage->lines, sizeof(*lines) * newCapacity);
    if (lines == ZR_NULL) {
        return ZR_FALSE;
    }

    coverage->lines = lines;
    coverage->line_capacity = newCapacity;
    return ZR_TRUE;
}

static TZrSize zr_debug_coverage_find_line(const ZrDebugCoverage *coverage,
                                           const SZrFunction *function,
                                           TZrUInt32 line) {
    TZrSize index;

    if (coverage == ZR_NULL || function == ZR_NULL || line == 0u) {
        return (TZrSize)-1;
    }

    for (index = 0u; index < coverage->line_count; index++) {
        if (coverage->lines[index].function == function && coverage->lines[index].line == line) {
            return index;
        }
    }

    return (TZrSize)-1;
}

/* 以函数身份和行号去重，合并预注册的可执行行与运行时实际命中行。 */
static TZrSize zr_debug_coverage_add_line(ZrDebugCoverage *coverage,
                                          const SZrFunction *function,
                                          TZrUInt32 line,
                                          TZrBool executable,
                                          TZrBool executed) {
    TZrSize existingIndex;
    TZrSize index;
    ZrDebugCoverageLine *entry;

    if (coverage == ZR_NULL || function == ZR_NULL || line == 0u) {
        return (TZrSize)-1;
    }

    existingIndex = zr_debug_coverage_find_line(coverage, function, line);
    if (existingIndex != (TZrSize)-1) {
        coverage->lines[existingIndex].executable = (TZrBool)(coverage->lines[existingIndex].executable || executable);
        coverage->lines[existingIndex].executed = (TZrBool)(coverage->lines[existingIndex].executed || executed);
        return existingIndex;
    }

    if (!zr_debug_coverage_reserve_lines(coverage, coverage->line_count + 1u)) {
        return (TZrSize)-1;
    }

    index = coverage->line_count++;
    entry = &coverage->lines[index];
    memset(entry, 0, sizeof(*entry));
    entry->function = function;
    entry->line = line;
    entry->executable = executable;
    entry->executed = executed;
    zr_debug_coverage_copy_text(entry->name,
                                sizeof(entry->name),
                                zr_debug_coverage_function_name(function));
    zr_debug_coverage_copy_text(entry->source,
                                sizeof(entry->source),
                                zr_debug_coverage_function_source(function));
    return index;
}

/* LINE hook 的事件行号可能为空，借当前激活帧补全来源后才记入报告。 */
static TZrBool zr_debug_coverage_capture_location(SZrState *state,
                                                  const SZrDebugInfo *debugInfo,
                                                  ZrDebugCoverageLocation *outLocation) {
    SZrDebugActivation activation;
    SZrDebugInfo info;

    if (state == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outLocation, 0, sizeof(*outLocation));
    memset(&activation, 0, sizeof(activation));
    if (!ZrCore_Debug_GetStack(state, 0u, &activation) || activation.function == ZR_NULL) {
        return ZR_FALSE;
    }

    outLocation->function = activation.function;
    outLocation->line = debugInfo != ZR_NULL ? (TZrUInt32)debugInfo->currentLine : 0u;
    outLocation->name = zr_debug_coverage_function_name(activation.function);
    outLocation->source = zr_debug_coverage_function_source(activation.function);

    memset(&info, 0, sizeof(info));
    if (ZrCore_Debug_GetInfo(state,
                             &activation,
                             (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                ZR_DEBUG_INFO_SOURCE_FILE |
                                                ZR_DEBUG_INFO_LINE_NUMBER),
                             &info)) {
        if (info.name != ZR_NULL) {
            outLocation->name = info.name;
        }
        if (info.source != ZR_NULL) {
            outLocation->source = info.source;
        }
        if (outLocation->line == 0u) {
            outLocation->line = (TZrUInt32)info.currentLine;
        }
    }

    return (TZrBool)(outLocation->line != 0u);
}

static void zr_debug_coverage_record_line(ZrDebugCoverage *coverage,
                                          SZrState *state,
                                          const SZrDebugInfo *debugInfo) {
    ZrDebugCoverageLocation location;
    TZrSize lineIndex;

    if (coverage == ZR_NULL || debugInfo == ZR_NULL || debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE) {
        return;
    }
    if (!zr_debug_coverage_capture_location(state, debugInfo, &location)) {
        return;
    }

    lineIndex = zr_debug_coverage_add_line(coverage, location.function, location.line, ZR_TRUE, ZR_TRUE);
    /* TODO: LINE hook 中分配失败只跳过该行，报告仍像完整采集；核对 CLI 是否
     * 需要 incomplete 状态及可控内存失败测试，避免将少报解释成未执行。 */
    if (lineIndex == (TZrSize)-1) {
        return;
    }
    zr_debug_coverage_copy_text(coverage->lines[lineIndex].name,
                                sizeof(coverage->lines[lineIndex].name),
                                location.name);
    zr_debug_coverage_copy_text(coverage->lines[lineIndex].source,
                                sizeof(coverage->lines[lineIndex].source),
                                location.source);
}

/* Coverage 作为临时 hook 包装层，先记录自身 LINE 事件，再按原 mask 转发。 */
static void zr_debug_coverage_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    ZrDebugCoverage *coverage = zr_debug_coverage_find_active(state);

    if (coverage != ZR_NULL && debugInfo != ZR_NULL && debugInfo->event == ZR_DEBUG_HOOK_EVENT_LINE) {
        zr_debug_coverage_record_line(coverage, state, debugInfo);
    }

    if (coverage != ZR_NULL &&
        coverage->previous_hook != ZR_NULL &&
        debugInfo != ZR_NULL &&
        (coverage->previous_mask & zr_debug_coverage_event_mask(debugInfo->event)) != 0u) {
        coverage->previous_hook(state, debugInfo);
    }
}

ZR_DEBUG_API void ZrDebug_Coverage_Init(ZrDebugCoverage *coverage) {
    if (coverage == ZR_NULL) {
        return;
    }
    memset(coverage, 0, sizeof(*coverage));
}

ZR_DEBUG_API void ZrDebug_Coverage_Reset(ZrDebugCoverage *coverage) {
    if (coverage == ZR_NULL) {
        return;
    }
    coverage->line_count = 0u;
}

/* CLI 在执行入口前注册静态可执行行，以便报告包含未命中的行。 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunction(ZrDebugCoverage *coverage,
                                                       const struct SZrFunction *function) {
    TZrSize activeLineCount;
    TZrUInt32 *activeLines;
    TZrSize index;

    if (coverage == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    activeLineCount = ZrCore_Debug_GetActiveLines(function, ZR_NULL, 0u);
    if (activeLineCount == 0u) {
        return ZR_TRUE;
    }

    activeLines = (TZrUInt32 *)malloc(sizeof(*activeLines) * activeLineCount);
    if (activeLines == ZR_NULL) {
        return ZR_FALSE;
    }

    activeLineCount = ZrCore_Debug_GetActiveLines(function, activeLines, activeLineCount);
    for (index = 0u; index < activeLineCount; index++) {
        if (zr_debug_coverage_add_line(coverage, function, activeLines[index], ZR_TRUE, ZR_FALSE) == (TZrSize)-1) {
            free(activeLines);
            return ZR_FALSE;
        }
    }

    free(activeLines);
    return ZR_TRUE;
}

ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunctionTree(ZrDebugCoverage *coverage,
                                                           const struct SZrFunction *function) {
    TZrUInt32 index;

    if (coverage == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrDebug_Coverage_RegisterFunction(coverage, function)) {
        return ZR_FALSE;
    }

    for (index = 0u; index < function->childFunctionLength; index++) {
        if (!ZrDebug_Coverage_RegisterFunctionTree(coverage, &function->childFunctionList[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 与 VM 共享唯一 debug hook 槽位；保留旧配置后扩展 LINE 事件。 */
/* TODO: 若活动期间其它组件调用 ZrCore_Debug_SetHook，Stop 会恢复过期快照并覆盖
 * 新 hook；核对嵌入式宿主是否允许动态换 hook，再定义串行化或栈式所有权。 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_Start(ZrDebugCoverage *coverage, struct SZrState *state) {
    if (coverage == ZR_NULL || state == ZR_NULL || coverage->active) {
        return ZR_FALSE;
    }

    coverage->state = state;
    coverage->previous_hook = ZrCore_Debug_GetHook(state);
    coverage->previous_mask = ZrCore_Debug_GetHookMask(state);
    coverage->previous_count = ZrCore_Debug_GetHookCount(state);
    if (coverage->previous_hook == ZR_NULL && coverage->previous_mask != 0u) {
        coverage->state = ZR_NULL;
        coverage->previous_mask = 0u;
        coverage->previous_count = 0u;
        return ZR_FALSE;
    }
    if (!zr_debug_coverage_register_active(coverage)) {
        coverage->state = ZR_NULL;
        coverage->previous_hook = ZR_NULL;
        coverage->previous_mask = 0u;
        coverage->previous_count = 0u;
        return ZR_FALSE;
    }

    /* hook 一旦安装即可从任意 LINE 事件回查会话，故先发布 active 链表状态。 */
    coverage->active = ZR_TRUE;
    ZrCore_Debug_SetHook(state,
                         zr_debug_coverage_hook,
                         coverage->previous_mask | ZR_DEBUG_HOOK_MASK_LINE,
                         coverage->previous_count);
    return ZR_TRUE;
}

/* CLI 写报告前先停止采集，确保结果数组不再被 hook 改动。 */
ZR_DEBUG_API void ZrDebug_Coverage_Stop(ZrDebugCoverage *coverage) {
    SZrState *state;

    if (coverage == ZR_NULL || !coverage->active) {
        return;
    }

    state = coverage->state;
    if (state != ZR_NULL) {
        ZrCore_Debug_SetHook(state, coverage->previous_hook, coverage->previous_mask, coverage->previous_count);
    }
    zr_debug_coverage_unregister_active(coverage);
    coverage->state = ZR_NULL;
    coverage->previous_hook = ZR_NULL;
    coverage->previous_mask = 0u;
    coverage->previous_count = 0u;
    coverage->active = ZR_FALSE;
}

ZR_DEBUG_API void ZrDebug_Coverage_Destroy(ZrDebugCoverage *coverage) {
    if (coverage == ZR_NULL) {
        return;
    }
    if (coverage->active) {
        ZrDebug_Coverage_Stop(coverage);
    }
    free(coverage->lines);
    ZrDebug_Coverage_Init(coverage);
}

ZR_DEBUG_API TZrSize ZrDebug_Coverage_GetLineCount(const ZrDebugCoverage *coverage) {
    return coverage != ZR_NULL ? coverage->line_count : 0u;
}

ZR_DEBUG_API const ZrDebugCoverageLine *ZrDebug_Coverage_GetLine(const ZrDebugCoverage *coverage, TZrSize index) {
    if (coverage == ZR_NULL || index >= coverage->line_count) {
        return ZR_NULL;
    }
    return &coverage->lines[index];
}

/* Coverage collection contracts: qualified EOF documentation; original program prefix is unchanged. */

/**
 * @var ZrDebugCoverage *g_active_coverages
 * ## Summary
 * 保存进程内已发布的 Coverage 会话，使只有 state 参数的 VM hook 能找回行数组和原 hook 快照。此表不拥有会话对象或 VM。
 *
 * ## Fields
 * - 表头及 next_active 是借用链接；查找按 state 指针匹配已入链节点，不另检查 active。
 * - 注册拒绝同一 state 的第二个活动会话；Stop 必须摘链后才能销毁宿主对象。
 *
 * ## Invariants and lifecycle
 * Start 在安装包装 hook 前发布 active，Stop 在恢复原 hook 后摘链。宿主必须使链表写入与所有 state 的 hook 遍历互斥，并协调同一行数组的读写；不能从每 state 单实例推导跨 state 线程安全。
 * TODO: 保留现有多 state 同步问题；沿 register_active/unregister_active、宿主调度及并发采集用例核实共享同步策略。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:65; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:78; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:91; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:264。
 */

/**
 * @struct ZrDebugCoverageLocation
 * ## Summary
 * 把一次当前栈帧的位置读取暂存在栈上，供 LINE hook 更新自有行快照；它不是可跨执行阶段保存的 VM 根。
 *
 * ## Fields
 * - function 借用当前栈帧的函数身份；name/source 借用调试接口或函数元数据的文本，也可能指向静态回退文本。
 * - line 是本次取得的非零源码行号；捕获失败时结构可已被清零或部分填写，不可消费。
 *
 * ## Invariants and lifecycle
 * capture_location 只在当前 hook 窗口交付此视图，record_line 随即复制 name/source 字节到会话拥有的定长数组。函数指针仍借用 VM，结果读取结束前须保持函数元数据存活；本结构无分配和释放责任。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:194; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:236; zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h:73。
 */

/**
 * @fn void zr_debug_coverage_copy_text(TZrChar *destination, TZrSize destinationSize, const TZrChar *source)
 * ## Summary
 * 将借用的函数名或来源复制为行记录自有文本，避免报告遍历依赖临时调试文本的寿命。
 *
 * ## Parameters
 * @param[out] destination 可写目标字节数组；NULL 或 destinationSize 为零时不写入。
 * @param[in] destinationSize 目标实际可写容量，按 TZrChar 字节计，包含终止 NUL 的位置。
 * @param[in] source 本次调用期间有效的非重叠 NUL 结尾文本；NULL 交付空串。
 *
 * ## Returns
 * 通过 destination 交付最多 destinationSize-1 个源字节和结尾 NUL；NULL/零容量无副作用，不返回错误状态。
 *
 * ## Constraints and lifecycle
 * 调用方保证非 NULL 目标具有声明的容量，且源/目标满足 strlen/memcpy 的有效、非重叠缓冲区前提。截断按字节进行，不承诺 UTF-8 码点边界；不分配、不转移或释放源文本。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:184; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:187; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:255; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:258。
 */

/**
 * @fn TZrUInt32 zr_debug_coverage_event_mask(EZrDebugHookEvent event)
 * ## Summary
 * 把回调事件转换为订阅位，用原掩码筛选向旧 hook 转发的事件。
 *
 * ## Parameters
 * @param[in] event VM 派发的合法非负 EZrDebugHookEvent；event >= MAX 时视为无订阅位。
 *
 * ## Returns
 * @return 合法事件返回 1u << event；超过事件上界返回零，不修改任何会话或 VM 状态。
 *
 * ## Constraints and lifecycle
 * 仅处理当前事件枚举，不接受伪造的负值。包装层新增 LINE 订阅不等于旧 hook 也订阅 LINE；转发还须与 previous_mask 相交。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:274; zr_vm_core/include/zr_vm_core/debug.h:77。
 */

/**
 * @fn const TZrChar *zr_debug_coverage_function_name(const SZrFunction *function)
 * ## Summary
 * 为尚未取得完整调试位置的行记录提供函数名回退视图，避免空函数或空字符串元数据进入复制步骤。
 *
 * ## Parameters
 * @param[in] function 可为 NULL 的借用函数；非 NULL 的 name 元数据在本次读取期间有效。
 *
 * ## Returns
 * @return function 或 functionName 为 NULL 时返回静态 <anonymous>；否则直接借用 ZrCore_String_GetNativeString(functionName) 的结果，不另检测 native 指针；调用方不释放返回值。
 *
 * ## Constraints and lifecycle
 * 返回值不是新分配字符串，也不延长函数或 VM 字符串寿命。add_line/捕获位置随后立即复制文本；静态匿名名仅表示缺少名称，不是函数去重标识。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:186; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:212。
 */

/**
 * @fn const TZrChar *zr_debug_coverage_function_source(const SZrFunction *function)
 * ## Summary
 * 为行记录取得函数来源的借用视图，并对缺少来源的函数提供空串回退。
 *
 * ## Parameters
 * @param[in] function 可为 NULL 的借用函数；非 NULL 的 source 元数据在本次读取期间有效。
 *
 * ## Returns
 * @return function 或 sourceCodeList 为 NULL 时返回静态空串；否则直接借用 ZrCore_String_GetNativeString(sourceCodeList) 的结果，不另检测 native 指针；调用方不释放返回值。
 *
 * ## Constraints and lifecycle
 * 来源只用于报告展示，不参与函数身份与行号去重。返回文本须在复制完成前有效；不承诺路径规范化、磁盘文件存在或 UTF-8 截断完整性。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:189; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:213。
 */

/**
 * @fn ZrDebugCoverage *zr_debug_coverage_find_active(SZrState *state)
 * ## Summary
 * 从包装 hook 的 state 找到已发布的活动会话，也为注册阶段检测同 state 冲突。
 *
 * ## Parameters
 * @param[in] state 按指针身份查找的借用 VM state；不会取得或释放 VM。
 *
 * ## Returns
 * @return 返回首个 state 指针相同的已入链借用节点，无匹配返回 NULL；不另检查 active，不修改链表。
 *
 * ## Constraints and lifecycle
 * 节点对象由宿主持有并须覆盖 hook 遍历窗口。依赖注册层维护每 state 单实例和外部同步；无匹配不是该 state 未执行过代码的证据。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:82; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:265; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:12。
 */

/**
 * @fn TZrBool zr_debug_coverage_register_active(ZrDebugCoverage *coverage)
 * ## Summary
 * 在 hook 安装前建立 state 到会话的唯一借用链接，禁止同一 VM 同时被两个 Coverage 会话包装。
 *
 * ## Parameters
 * @param[in,out] coverage 已初始化、state 已绑定且尚未由 Start 发布的会话；对象由宿主持有。
 *
 * ## Returns
 * @return NULL 会话/state 或已有活动同 state 节点时返回 FALSE 且不插入；成功前插表头并返回 TRUE。
 *
 * ## Constraints and lifecycle
 * 只维护链接，不设置 active、不安装 hook、不分配内存。Start 在此成功后置 active，再调用 SetHook；注册及所有 hook 遍历需要宿主共同的同步约束。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:364; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:373; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:374。
 */

/**
 * @fn void zr_debug_coverage_unregister_active(ZrDebugCoverage *coverage)
 * ## Summary
 * 在旧 hook 已恢复后摘除当前会话，解除进程表对宿主对象的借用。
 *
 * ## Parameters
 * @param[in,out] coverage 拟摘除的宿主会话；NULL 或未在表内时无操作。
 *
 * ## Returns
 * 找到完全相同的节点时修复前驱链接并清空其 next_active；不释放对象、state 或行数组。
 *
 * ## Constraints and lifecycle
 * Stop 负责先恢复 VM hook，再调用本函数，最后清除绑定/active。摘链需与所有 hook 遍历同步；仅清链不代表可以忽略仍在执行的回调。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:391; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:393; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:401。
 */

/**
 * @fn TZrBool zr_debug_coverage_reserve_lines(ZrDebugCoverage *coverage, TZrSize minimumCapacity)
 * ## Summary
 * 为动态行表扩容，同时在普通 realloc 失败时保留原数组和已采集前缀。
 *
 * ## Parameters
 * @param[in,out] coverage 已初始化的行数组所有者；NULL 返回 FALSE。
 * @param[in] minimumCapacity 请求的最小元素数，不是字节数；由 add_line 的下一行需求产生。
 *
 * ## Returns
 * @return 现有容量足够或扩容成功返回 TRUE；NULL 会话/realloc 失败返回 FALSE，失败不更新 lines/line_capacity。
 *
 * ## Constraints and lifecycle
 * 成功可能移动整块数组并使 GetLine 借用指针失效，已有元素内容仍由 realloc 保留。首次目标容量为32，后续倍增；不保证并发读写安全。
 * TODO: 保留既有容量/宽度家族，沿倍增、sizeof 乘法、add_line 计数加一及静态登记规模上界核查可达溢出；当前未证明完整合法触发路径，不标 BUG。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:173; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:119; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:124; zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h:112。
 */

/**
 * @fn TZrSize zr_debug_coverage_find_line(const ZrDebugCoverage *coverage, const SZrFunction *function, TZrUInt32 line)
 * ## Summary
 * 按函数身份和行号检索静态登记与运行命中的共同记录，支持后续标志合并。
 *
 * ## Parameters
 * @param[in] coverage 只读借用的有效行表；NULL 视为无匹配。
 * @param[in] function 函数身份指针，NULL 视为无匹配，不依函数名称比较。
 * @param[in] line 非零源码行号，零视为无匹配。
 *
 * ## Returns
 * @return 命中返回 [0,line_count) 的零基索引；无匹配或非法输入返回 (TZrSize)-1，不修改表。
 *
 * ## Constraints and lifecycle
 * 只扫描当前有效前缀，不能与 Reset、扩容、Destroy 或 hook 写入并发。函数对象须覆盖表中所有身份引用的使用期；索引本身不延长记录寿命。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:166; zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h:73。
 */

/**
 * @fn TZrSize zr_debug_coverage_add_line(ZrDebugCoverage *coverage, const SZrFunction *function, TZrUInt32 line, TZrBool executable, TZrBool executed)
 * ## Summary
 * 合并预登记的可执行分母与 LINE 事件的实际命中，保持同一函数身份/行号至多一条记录。
 *
 * ## Parameters
 * @param[in,out] coverage 已初始化、由宿主串行访问的行表；NULL 返回哨兵。
 * @param[in] function 有效借用函数身份及可读展示元数据，NULL 返回哨兵。
 * @param[in] line 非零源码行号，零返回哨兵。
 * @param[in] executable 本次是否确认该行可执行，与已有标志作逻辑或。
 * @param[in] executed 本次是否实际命中，与已有标志作逻辑或。
 *
 * ## Returns
 * @return 返回已有或新建索引；非法输入/扩容失败返回 (TZrSize)-1。普通扩容失败保留既有记录，成功新建后行数加一并复制名称/来源。
 *
 * ## Constraints and lifecycle
 * 重复登记不会清除 executed；新增记录可能 realloc 并使行指针失效。会话拥有文本数组，function 仍借用 VM；静态登记传 executed=FALSE，动态命中传 TRUE。容量上界沿既有 TODO 跟踪。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:249; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:316; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:166; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:173。
 */

/**
 * @fn TZrBool zr_debug_coverage_capture_location(SZrState *state, const SZrDebugInfo *debugInfo, ZrDebugCoverageLocation *outLocation)
 * ## Summary
 * 从当前第零层激活记录取得函数、名称、来源和非零行号，为一条 LINE 快照提供临时借用视图。
 *
 * ## Parameters
 * @param[in] state 当前 hook 所属且栈帧稳定的借用 VM；NULL 返回 FALSE。
 * @param[in] debugInfo 可为 NULL 的事件信息；非 NULL 时优先取其 currentLine，文本仍向核心调试接口查询。
 * @param[out] outLocation 调用期有效的栈上视图输出；NULL 返回 FALSE，失败时可能清零或部分填写，不可消费。
 *
 * ## Returns
 * @return GetStack 成功且函数存在、最终行号非零时返回 TRUE。GetInfo 成功时更新非 NULL 名称/来源，并只在事件行号为零时回填行号；其失败不必然使本函数失败。
 *
 * ## Constraints and lifecycle
 * 只请求 NAME/SOURCE/LINE，不请求 PUSH_FUNCTION；不保存 VM 根或取得文本所有权。输出仅用于本次 record_line，文本随后复制；读取失败不向采集者提供独立错误对象。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:245; zr_vm_core/src/zr_vm_core/debug/debug.c:290; zr_vm_core/src/zr_vm_core/debug/debug.c:570。
 */

/**
 * @fn void zr_debug_coverage_record_line(ZrDebugCoverage *coverage, SZrState *state, const SZrDebugInfo *debugInfo)
 * ## Summary
 * 把一次 LINE 事件转为 executable/executed 均为真的行记录，并立即复制更精确的调试展示文本。
 *
 * ## Parameters
 * @param[in,out] coverage 由当前 state 找到的会话及其自有行表；NULL 忽略。
 * @param[in] state 当前 hook VM 的借用状态，须保持栈帧/函数有效。
 * @param[in] debugInfo 借用事件信息；NULL 或非 LINE 事件不记录。
 *
 * ## Returns
 * 通过 coverage 行数组交付或合并命中；无位置、非 LINE 或添加失败时直接返回，不向 VM/CLI 回传失败状态。
 *
 * ## Constraints and lifecycle
 * 捕获视图只活到本次回调结束；快照文本由会话拥有，function 仍借用。重复命中可原位更新既有展示文本；调用方须避免与数组遍历/Reset/Destroy 并发。
 * 保留原 allocation_failure 块 TODO：LINE 分配失败会少报且没有 incomplete 标记，需沿 CLI 报告消费和可控 OOM 核验，不把未记录行当作未执行。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:268; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:249; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:250; zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:205。
 */

/**
 * @fn void zr_debug_coverage_hook(SZrState *state, SZrDebugInfo *debugInfo)
 * ## Summary
 * 包装 VM 的单一 debug hook：先采集自己的 LINE，再按 Start 保存的旧订阅掩码转发事件。
 *
 * ## Parameters
 * @param[in,out] state VM 当前派发状态；用于查表并原样借给旧 hook，不转移所有权。
 * @param[in,out] debugInfo 可为 NULL 的本次事件视图；非 NULL 时原样传给旧 hook，其可观察/修改行为由原回调契约决定。
 *
 * ## Returns
 * 无返回状态；未找到会话时不采集、不转发，新增 LINE 不会无条件转发给未订阅 LINE 的旧 hook。
 *
 * ## Constraints and lifecycle
 * 会话及 previous_hook 必须在回调期间有效。核心普通 hook 派发通过 allowDebugHook 抑制当前包装层递归，但不由此获得线程安全或任意旧回调非局部退出的回滚保证；宿主协调 Stop/Destroy 及链表写入。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:375; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:265; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:274; zr_vm_core/src/zr_vm_core/debug/debug.c:1140。
 */

/**
 * @fn void ZrDebug_Coverage_Init(ZrDebugCoverage *coverage)
 * ## Summary
 * 建立空闲 Coverage 初态，让后续静态登记、Start/Stop 和最终 Destroy 共用明确的所有权起点。
 *
 * ## Parameters
 * @param[in,out] coverage 新对象或已完整 Destroy 的宿主存储；NULL 无操作。
 *
 * ## Returns
 * 非 NULL 对象的全部字段清零；不分配资源，也不回收已有数组或恢复已有 hook。
 *
 * ## Constraints and lifecycle
 * 不能用 Init 覆盖仍拥有行数组或仍活动的对象；那会丢失释放/恢复所需状态。先 Destroy 再重新初始化；对象存储本身始终由宿主持有。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:996; tests/profile/test_coverage.c:118; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:409。
 */

/**
 * @fn void ZrDebug_Coverage_Reset(ZrDebugCoverage *coverage)
 * ## Summary
 * 清空逻辑行记录但复用已分配容量，供宿主重新建立下一轮覆盖率分母。
 *
 * ## Parameters
 * @param[in,out] coverage 已初始化且由宿主排他访问的会话；NULL 无操作。
 *
 * ## Returns
 * 仅将 line_count 置零；不清空容量、不释放 lines、不停止 hook、不恢复 VM 配置。
 *
 * ## Constraints and lifecycle
 * 原 GetLine 记录逻辑失效且存储可复用，之后须重新 RegisterFunction/Tree 才包含静态未命中行。建议 Stop 后 Reset；活动时调用不会自动禁止后续 LINE 再填表，宿主负责串行化。当前无检索到的外部直接调用不证明接口不可用。
 *
 * ## References
 * zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h:183; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:290; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:294; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:349。
 */

/**
 * @fn TZrBool ZrDebug_Coverage_RegisterFunction(ZrDebugCoverage *coverage, const struct SZrFunction *function)
 * ## Summary
 * 把单函数的去重可执行行加入分母，让报告区分未执行行与实际 LINE 命中。
 *
 * ## Parameters
 * @param[in,out] coverage 已初始化的会话；建议在 Start 前登记，NULL 返回 FALSE。
 * @param[in] function 有效借用函数及稳定的位置表/名称/来源；NULL 返回 FALSE。
 *
 * ## Returns
 * @return 无可执行行或全部登记成功返回 TRUE。临时行号数组 malloc 或添加行失败返回 FALSE；临时数组在普通失败/成功路径释放，已登记前缀与旧 executed 标志保留，不全量回滚。
 *
 * ## Constraints and lifecycle
 * 先询问行数再读取行表，调用期函数元数据须稳定并保持同一计数上界。临时数组归本函数，行数组归 coverage，函数身份只借用；不是执行函数。数组字节乘法/可达宽度仍属于头文件既有 TODO，未授 OOM 测试信用。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:333; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:304; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:309; zr_vm_core/src/zr_vm_core/debug/debug.c:733。
 */

/**
 * @fn TZrBool ZrDebug_Coverage_RegisterFunctionTree(ZrDebugCoverage *coverage, const struct SZrFunction *function)
 * ## Summary
 * 在执行入口前递归登记根函数及所有 childFunctionList 子函数，使未进入分支也有静态行分母。
 *
 * ## Parameters
 * @param[in,out] coverage 已初始化并串行访问的会话，NULL 返回 FALSE。
 * @param[in] function 稳定、有限且无环的函数树根借用指针，NULL 返回 FALSE。
 *
 * ## Returns
 * @return 本节点及子树均登记成功返回 TRUE；任一失败立即返回 FALSE，之前节点/行的结果保留，调用方可决定销毁或重试。
 *
 * ## Constraints and lifecycle
 * 遍历不执行 ZR 函数，也不取得函数树所有权；调用期子数组和位置元数据须有效稳定，报告结束前相关函数身份仍须存活。依赖函数树无环/深度适合宿主栈，不自带循环检测或错误对象。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:106; tests/profile/test_coverage.c:119; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:333; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:338。
 */

/**
 * @fn TZrBool ZrDebug_Coverage_Start(ZrDebugCoverage *coverage, struct SZrState *state)
 * ## Summary
 * 保存原 hook、事件掩码和 COUNT 周期基值，发布唯一 state 会话后只追加 LINE 订阅，开始动态采集。
 *
 * ## Parameters
 * @param[in,out] coverage 已初始化且非活动的宿主会话；已 active 时早返保留原绑定/数组。
 * @param[in,out] state 将安装包装 hook 的 VM；非 NULL，须活到 Stop/Destroy 恢复完成。
 *
 * ## Returns
 * @return NULL 参数/已 active、旧 hook 为 NULL 而旧 mask 非零、同 state 冲突时返回 FALSE；后两种新绑定失败会清空本次快照，行数组保持。成功返回 TRUE 且 active 已发布、包装 hook 已安装。
 *
 * ## Constraints and lifecycle
 * 先注册链表、置 active，再 SetHook，防止新 hook 查不到会话；Start/Stop 均须与所有 hook 遍历协调。保存的 count 是周期基值而非剩余倒计时，SetHook 会重启计数。Start 不负责函数树登记或 VM 生命周期。
 * TODO: 保留外部 hook 所有权家族；活动期间其它组件若 SetHook，Stop 将恢复旧快照并覆盖新配置，需沿嵌入宿主的换 hook 策略验证串行或栈式所有权。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:107; tests/profile/test_coverage.c:120; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:364; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:373; zr_vm_core/src/zr_vm_core/debug/debug.c:663。
 */

/**
 * @fn void ZrDebug_Coverage_Stop(ZrDebugCoverage *coverage)
 * ## Summary
 * 结束采集并交还 VM 的原 hook 配置，使宿主可以稳定遍历结果或随后销毁数组。
 *
 * ## Parameters
 * @param[in,out] coverage 由 Start 成功发布的会话；NULL 或非活动时无操作。
 *
 * ## Returns
 * 先恢复 previous_hook/mask/count，再摘除活动表节点并清空绑定/active；不清空行数和行数组。
 *
 * ## Constraints and lifecycle
 * state 及旧 hook 在恢复期间必须有效；恢复周期基值会重启 COUNT 倒计时，不恢复原剩余 tick。停止不修复活动期间外部换 hook 的冲突，也不提供跨线程等待正在执行回调的机制；宿主先保证时序与同步。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:85; zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1204; tests/profile/test_coverage.c:125; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:391; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:393。
 */

/**
 * @fn void ZrDebug_Coverage_Destroy(ZrDebugCoverage *coverage)
 * ## Summary
 * 在最终释放前先停止活动采集，再释放会话自有行数组并恢复空闲初态。
 *
 * ## Parameters
 * @param[in,out] coverage 已初始化的宿主对象；NULL 无操作，活动时其 VM 必须仍有效。
 *
 * ## Returns
 * 活动对象经 Stop 交还 hook，free(lines) 后 Init；对象存储本身、state 和函数元数据不由此释放。
 *
 * ## Constraints and lifecycle
 * 所有 GetLine 指针均失效；必须先结束并发读写/回调，再 Destroy，随后才能释放 VM/函数或宿主对象。重复 Destroy 空闲已初始化对象可用，不能用于任意未初始化字节。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:86; zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1211; tests/profile/test_coverage.c:146; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:406; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:409。
 */

/**
 * @fn TZrSize ZrDebug_Coverage_GetLineCount(const ZrDebugCoverage *coverage)
 * ## Summary
 * 为报告遍历取得当前逻辑行前缀长度，不推断覆盖率采集是否完整。
 *
 * ## Parameters
 * @param[in] coverage 只读借用的会话；NULL 视为无行。
 *
 * ## Returns
 * @return NULL 返回零，否则返回 line_count；不分配、不冻结或排序行数组。
 *
 * ## Constraints and lifecycle
 * 先 Stop 或通过宿主同步保证遍历窗口稳定；Reset/新增 LINE 可改变逻辑长度。返回数量包含静态登记和动态新增记录，不等于执行次数、源码总行数或完整性标志。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:205; tests/profile/test_coverage.c:127; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:252。
 */

/**
 * @fn const ZrDebugCoverageLine *ZrDebug_Coverage_GetLine(const ZrDebugCoverage *coverage, TZrSize index)
 * ## Summary
 * 把一条会话内部行快照借给报告调用者，暴露函数身份、展示文本及可执行/已执行标志。
 *
 * ## Parameters
 * @param[in] coverage 只读借用的会话，NULL 返回 NULL。
 * @param[in] index 当前 [0,line_count) 范围内的零基记录索引；越界返回 NULL。
 *
 * ## Returns
 * @return 有效索引返回内部 const 指针；NULL/越界返回 NULL，不分配且不转移任何所有权。
 *
 * ## Constraints and lifecycle
 * 新增记录只有扩容时才可能使物理指针失效；重复登记/LINE 可原位修改内容，Reset 使记录逻辑失效并复用存储，Destroy 释放数组。function 仍借用 VM；报告读取须与行表 mutation/hook 串行。
 *
 * ## References
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:206; tests/profile/test_coverage.c:129; zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h:73; zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:173。
 */
