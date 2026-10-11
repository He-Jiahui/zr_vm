/**
 * @file coverage.h
 *
 * ## Summary
 * 公开覆盖率会话、静态可执行行登记和运行期行命中快照的 C API。调用方先为
 * 一个 VM state 建立会话，登记入口函数树，再在执行窗口内安装 LINE hook；停止
 * 后可在不再登记、Reset 或 Destroy 的读取期遍历采集窗口行结果并生成报告。
 *
 * ## Constraints and lifecycle
 * 行结果中的函数指针借用 VM 函数对象，文本字段则由会话复制并按固定容量截断。
 * 一个 state 同时只能关联一个活动 Coverage；进程级活动表无锁，宿主须协调
 * Start/Stop 的表写入与任一 Coverage hook 的表遍历，并避免同一会话的登记/重置与 hook 并发。
 *
 * ## References
 * `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:264` implements the session wrapper and LINE hook;
 * `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:102` registers the entry tree; `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:103` installs the hook; `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1151` stops it before reporting;
 * `docs/wiki/03-modules/debug-testing-api.md:100` records the Init/register/Start/Stop/report lifecycle.
 */
#ifndef ZR_VM_DEBUG_COVERAGE_H
#define ZR_VM_DEBUG_COVERAGE_H

#include "zr_vm_lib_debug/conf.h"
#include "zr_vm_core/debug.h"

/**
 * @brief 覆盖率行快照中函数名数组的容量（含终止 NUL 的存储上限）。
 *
 * ## Value model
 * 该值复用调试库名称快照容量；超长名称由实现截断，调用方不能把数组长度
 * 当作原始函数名的实际长度。
 *
 * ## References
 * `ZrDebugCoverageLine::name` 使用该容量；`ZR_DEBUG_NAME_CAPACITY` 在
 * `zr_vm_lib_debug/include/zr_vm_lib_debug/conf.h:12` 定义名称容量上限。
 */
#define ZR_DEBUG_COVERAGE_NAME_CAPACITY ZR_DEBUG_NAME_CAPACITY
/**
 * @brief 覆盖率行快照中来源文本数组的容量（含终止 NUL 的存储上限）。
 *
 * ## Value model
 * 该值复用调试库有界文本容量；超长来源名会被截断，空来源以零长度字符串
 * 保存，不把截断文本当作完整源路径。
 *
 * ## References
 * `zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h:79` 使用该容量；
 * `zr_vm_lib_debug/include/zr_vm_lib_debug/conf.h:11` 定义文本容量上限。
 */
#define ZR_DEBUG_COVERAGE_SOURCE_CAPACITY ZR_DEBUG_TEXT_CAPACITY

/**
 * ## Summary
 * 保存一个函数的一条源码行在覆盖率会话中的可执行分类和动态命中状态。
 * 静态登记先把可执行行写入结果，使从未触发 LINE hook 的行也能出现在报告中；
 * 运行期 LINE 命中按同一函数身份和行号合并，若尚未登记也会新增可执行记录。
 *
 * ## Fields
 * 字段注释说明函数身份、文本快照、行键以及两个布尔状态的含义。
 *
 * ## Value model
 * 当前实现以 `(function, line)` 为唯一键；同一函数的重复登记或后续 LINE
 * 事件只合并 `executable`/`executed`，不会再产生第二条记录。
 *
 * ## Invariants and lifecycle
 * `function` 只借用 VM 对象；`name` 与 `source` 是会话拥有的 NUL 终止快照，
 * 可能因容量限制截断。通过 `ZrDebug_Coverage_GetLine` 取得的指针只在对应
 * Coverage 的行数组未因新增记录扩容前通常保持物理地址；扩容可能使指针失效，重复登记或 LINE 命中仍可能原位更新字段。Reset 会清空可读前缀并允许以后复用，Destroy 才释放数组。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:316 supplies static rows;
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:249 merges LINE hits, including a row first seen dynamically;
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:207 writes the fields and tests/profile/test_coverage.c:132 asserts them.
 */
typedef struct ZrDebugCoverageLine {
    /** 借用的 VM 函数身份；调用方须使该函数至少存活到最后一次读取行快照。 */
    const struct SZrFunction *function;
    /** 从函数名复制的有界快照；缺省匿名函数文本为 `<anonymous>`，可能截断。 */
    TZrChar name[ZR_DEBUG_COVERAGE_NAME_CAPACITY];
    /** 从函数来源复制的有界快照；无来源时为空字符串，可能截断。 */
    TZrChar source[ZR_DEBUG_COVERAGE_SOURCE_CAPACITY];
    /** 一基源码行号；零行号不进入覆盖率结果。 */
    TZrUInt32 line;
    /** 该行是否纳入可执行报告；静态登记或 LINE 命中创建记录时置为真。 */
    TZrBool executable;
    /** 该行是否至少收到过一次 LINE hook 命中；静态未命中行保持假。 */
    TZrBool executed;
} ZrDebugCoverageLine;

/**
 * ## Summary
 * 保存一个 state 的覆盖率会话状态、行快照数组以及 Start 前的 VM debug hook
 * 配置。调用方分配该对象，先用 `ZrDebug_Coverage_Init` 初始化，再登记函数树、
 * 启动采集、停止采集并销毁；库只拥有 `lines` 数组，不接管 state 或函数对象。
 *
 * ## Fields
 * `state` 和 `previous_hook`/mask/count 描述活动窗口及恢复点；`active` 标记
 * 窗口；`lines`、`line_count`、`line_capacity` 管理拥有的行数组；`next_active`
 * 是库内部活动表链指针。
 *
 * ## Value model
 * `line_count` 始终表示可读数组前缀，容量由 `line_capacity` 给出；静态登记和
 * LINE hook 都通过同一 `(function, line)` 键更新前缀。活动时，覆盖率 hook 在
 * 保留原订阅事件的同时强制订阅 LINE。
 *
 * ## Invariants and lifecycle
 * 同一 state 只允许一个活动对象。活动期间 state、函数对象和原 hook 所依赖的
 * 外部资源必须保持有效；调用方不得直接以 `ZrCore_Debug_SetHook` 改写该 state，
 * 否则 Stop 只能恢复保存的旧快照。当前活动表无锁，Start/Stop 的表写入不得与
 * 任一 hook 遍历并发；同一会话的行数组修改/读取也须由宿主协调。
 * TODO: `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:12` 的全局活动表无锁，需用多 state 并发采集测试核对同步模型。
 * TODO: `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:391` 的外部 SetHook 冲突需核对宿主允许策略。
 * TODO: `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:249` 的 LINE 扩容失败没有 incomplete 标记，需沿 CLI 报告 `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:201` 和可控 OOM 测试核对。
 * TODO: `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:119`、`zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:124` 的倍增/字节乘法、`zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:173` 的计数加一及 `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:309` 的静态行号数组乘法未查上界；沿 `zr_vm_common/include/zr_vm_common/zr_common_conf.h:35`、`zr_vm_core/include/zr_vm_core/function.h:646`、`zr_vm_core/src/zr_vm_core/debug.c:744` 核实各宽度和可达规模，尚不足以标 BUG。
 * `Destroy` 会先 Stop（若仍活动）、释放 `lines` 并恢复为可重新 Init 的空对象。
 *
 * ## References
 * `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:374` publishes the hook and `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:393` unregisters it;
 * `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:290` reuses the array;
 * `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:102` and `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:103` show the normal registration/start sequence.
 */
typedef struct ZrDebugCoverage {
    /** 活动会话绑定的 VM state 借用指针；空闲或 Stop 后为 NULL。 */
    struct SZrState *state;
    /** Start 前 state 中的旧 hook；Stop 原样交还给 VM。 */
    FZrDebugHook previous_hook;
    /** Start 前的事件订阅掩码；Coverage 只在此基础上加入 LINE。 */
    TZrUInt32 previous_mask;
    /** Start 前的 COUNT 周期；Stop 时与旧掩码一起恢复。 */
    TZrUInt32 previous_count;
    /** 是否已加入活动表并安装 Coverage hook。 */
    TZrBool active;
    /** 会话拥有的行快照数组；由内部扩容并由 Destroy 释放。 */
    ZrDebugCoverageLine *lines;
    /** `lines` 中当前可读记录数；Reset 只将其清零。 */
    TZrSize line_count;
    /** `lines` 已分配的元素容量；不小于 `line_count`。 */
    TZrSize line_capacity;
    /** 活动表的内部后继指针；调用方不得修改或跨活动窗口保存。 */
    struct ZrDebugCoverage *next_active;
} ZrDebugCoverage;

/**
 * ## Summary
 * 把调用方分配的 Coverage 对象置为空闲初始状态，准备接收函数登记或 Start。
 * 它只建立结构体初始值，不分配行数组，也不取得 VM state。
 *
 * ## Parameters
 * @param[in,out] coverage 调用方分配的对象；可为 NULL（此时不产生副作用）。
 *     首次使用或刚由 `ZrDebug_Coverage_Destroy` 复位的对象才可直接初始化；已有
 *     行数组或活动 hook 的对象必须先 Destroy。
 *
 * ## Returns
 * 无返回值；非 NULL 对象的状态字段全部清零。
 *
 * ## Constraints and lifecycle
 * 不得把 Init 当作复用已拥有数组的 Reset；直接覆盖已初始化对象会丢失数组指针。
 * Init 完成后才能调用 RegisterFunction、Start 或查询接口。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:283 clears the object; CLI initializes the stack session at
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:961.
 */
ZR_DEBUG_API void ZrDebug_Coverage_Init(ZrDebugCoverage *coverage);
/**
 * ## Summary
 * 清空当前会话的可读行数，同时保留已分配容量以便再次登记，适合宿主复用同一
 * Coverage 对象而不重复申请初始数组。
 *
 * ## Parameters
 * @param[in,out] coverage 已经 Init 的会话对象；可为 NULL（此时不产生副作用）。
 *
 * ## Returns
 * 无返回值；`line_count` 变为零，既有数组容量和 hook 配置不变。
 *
 * ## Constraints and lifecycle
 * 应在 Stop 后、下一轮登记前调用；实现不拒绝活动对象，此时 Reset 会使旧
 * 记录在逻辑上不可读，后续 LINE/登记可复用其存储；不自动重新登记静态行，继续生成完整分母前需再次
 * 调用 RegisterFunction 或 RegisterFunctionTree。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:290 clears the readable prefix; static rows are added at
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:316.
 */
ZR_DEBUG_API void ZrDebug_Coverage_Reset(ZrDebugCoverage *coverage);
/**
 * ## Summary
 * 读取函数的去重可执行源码行，并把这些行加入 Coverage 的静态分母；新记录的
 * `executable` 为真而 `executed` 为假，因此未运行的行也能出现在最终报告。
 * 该函数只登记一个函数，不遍历其 `childFunctionList`。
 *
 * ## Parameters
 * @param[in,out] coverage 已 Init 且供本轮采集使用的会话；函数可能扩容其行数组，
 *     失败时已加入的前缀仍保留。
 * @param[in] function VM 保持有效的函数对象；执行位置表及非 NULL 的名称/来源
 *     文本在登记期可读；NULL 名称/来源分别回退为 `<anonymous>`/空串，函数身份仍只借用。
 *
 * ## Returns
 * @return `ZR_TRUE` 表示没有可执行行或所有行已加入；`ZR_FALSE` 表示参数无效、
 *     临时行号数组分配失败或行表扩容失败，失败可能留下部分登记结果。
 *
 * ## Constraints and lifecycle
 * 调用方应在 Start 前完成登记并保持函数对象存活到最后一次报告读取；重复登记按
 * `(function, line)` 去重。函数执行位置由 `ZrCore_Debug_GetActiveLines` 提供，零
 * 行号不会进入分母。活动期间调用虽无显式拒绝，但与 LINE hook 并发修改不受保护。
 *
 * ## References
 * `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:333` calls this function for each tree node; CLI begins registration at
 * `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:102`; core line enumeration is implemented at
 * `zr_vm_core/src/zr_vm_core/debug.c:744`.
 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunction(ZrDebugCoverage *coverage, const struct SZrFunction *function);
/**
 * ## Summary
 * 从给定入口函数开始递归登记整个嵌套函数树的可执行源码行，为一次 CLI 或宿主
 * 执行建立完整的静态覆盖率分母。
 *
 * ## Parameters
 * @param[in,out] coverage 已 Init 的会话对象；每个函数的登记都会追加或合并行记录。
 * @param[in] function 入口函数；`childFunctionLength > 0` 时 childFunctionList 及
 *     各子函数须在递归期间稳定可读；零长度不解引用该列表，结果只借用函数身份。
 *
 * ## Returns
 * @return `ZR_TRUE` 表示入口及全部后代登记成功；`ZR_FALSE` 表示参数无效或任一
 *     函数登记/分配失败，已经处理的入口或子树记录不会回滚。
 *
 * ## Constraints and lifecycle
 * 应在 Start 前调用；当前实现按 child 数组顺序递归，依赖 VM 函数树有限且无环，
 * 不执行函数、也不安装 hook。失败后若要重试，调用方应根据已有部分登记决定
 * Reset 后重新提交完整入口树。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:338 walks the child function list; CLI invokes it at
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:102; the profile test invokes it at
 * tests/profile/test_coverage.c:119.
 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunctionTree(ZrDebugCoverage *coverage,
                                                           const struct SZrFunction *function);
/**
 * ## Summary
 * 在指定 VM state 上安装 Coverage 的 LINE hook，保存 Start 前的 hook、事件掩码和
 * COUNT 周期基值（不保存剩余计数），并加入按 state 查找的活动表。旧 hook 仍收到原订阅
 * 事件；Coverage 额外订阅 LINE 以更新行命中状态。
 *
 * ## Parameters
 * @param[in,out] coverage 已 Init 且尚未活动的会话；其中已有的静态行记录会被保留。
 * @param[in,out] state 将执行被登记函数的 VM state；在 Stop 前必须保持有效。
 *
 * ## Returns
 * @return `ZR_TRUE` 表示 hook 已安装并会话已活动；`ZR_FALSE` 表示参数无效、对象
 *     已活动、state 已被另一 Coverage 占用，或 state 的旧掩码非零却没有旧 hook。
 *     若调用时已经 active，函数在保存任何新绑定前直接返回 false 并保留旧绑定；
 *     仅在本次取得 state/hook 快照后发生失败时，才清除本次快照；失败不会安装新 hook。
 *
 * ## Constraints and lifecycle
 * 一个 state 只能有一个活动 Coverage；调用方应先完成函数树登记，再在执行窗口
 * 开始前 Start。活动表和 hook 槽位均无并发仲裁；活动期间不要调用
 * `ZrCore_Debug_SetHook` 替换同一 state 的 hook，否则 Stop 会用旧快照覆盖外部新配置。
 * Stop 必须在 state 仍存活时调用。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:374 installs the wrapper; core hook state is updated at
 * zr_vm_core/src/zr_vm_core/debug.c:663.
 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_Start(ZrDebugCoverage *coverage, struct SZrState *state);
/**
 * ## Summary
 * 结束活动采集，先将 state 的 debug hook、掩码和 COUNT 周期恢复为 Start 前的快照，
 * 再从活动表摘除会话并清除活动绑定，使行数组可稳定读取和报告。
 *
 * ## Parameters
 * @param[in,out] coverage 活动或空闲会话；NULL 或未活动对象调用时不产生副作用。
 *
 * ## Returns
 * 无返回值；活动对象最终为非活动状态，`state`、旧 hook 和旧掩码字段被清空。
 *
 * ## Constraints and lifecycle
 * 必须在绑定的 state 尚未销毁时调用；CLI 在写 coverage 报告前先 Stop。Stop 不
 * 释放行数组，`GetLine` 指针仍受后续 Register、Reset 或 Destroy 约束。若活动期间
 * 外部改写 hook，当前实现不检测冲突而直接恢复保存的旧配置。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:391 restores the saved hook; early/error cleanup calls it at
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:81, while the successful report path stops at
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1151 and the test stops at tests/profile/test_coverage.c:125.
 */
ZR_DEBUG_API void ZrDebug_Coverage_Stop(ZrDebugCoverage *coverage);
/**
 * ## Summary
 * 终结 Coverage 会话：若仍活动则先 Stop，然后释放内部行快照数组，并把对象
 * 复位为可重新 Init 的空闲状态。
 *
 * ## Parameters
 * @param[in,out] coverage 由调用方持有的会话对象；NULL 调用无操作。
 *
 * ## Returns
 * 无返回值；内部行数组被释放，所有此前由 GetLine 返回的指针都失效。
 *
 * ## Constraints and lifecycle
 * 活动对象销毁仍要求其绑定 state 有效，因为 Destroy 会调用 Stop；若 Stop 后仍需读取
 * 报告，必须保持行记录中借用的函数对象存活，完成 GetLine 遍历后才能释放函数/state。
 * 调用完成后对象本身仍可重新 Init，不能继续使用旧的 `line_count`、`line_capacity` 或行指针。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:408 frees the line array; the successful CLI report path destroys after reporting at
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1158; the early/error helper performs Stop then Destroy at
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:82.
 */
ZR_DEBUG_API void ZrDebug_Coverage_Destroy(ZrDebugCoverage *coverage);
/**
 * ## Summary
 * 返回当前会话中可读行快照的数量，供调用方以零基索引遍历 `GetLine`。
 *
 * ## Parameters
 * @param[in] coverage 会话对象，可为 NULL。
 *
 * ## Returns
 * @return 非 NULL 对象的当前 `line_count`；NULL 返回零。活动期间该数量可能随
 * LINE hook 增长；Stop 后只有在调用方不再登记或 Reset 的读取期，数量才适合作为报告行数。
 *
 * ## Constraints and lifecycle
 * 返回值只描述当前数组前缀，不保证行的排序或静态/动态来源；读取每项前仍须用
 * 同一会话和索引调用 `ZrDebug_Coverage_GetLine`。Reset 后数量为零，Destroy 后
 * 对象也返回零。
 *
 * ## References
 * CLI obtains the count at zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:201 and reads each row at
 * zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:202; the test drives the same loop from
 * tests/profile/test_coverage.c:128.
 */
ZR_DEBUG_API TZrSize ZrDebug_Coverage_GetLineCount(const ZrDebugCoverage *coverage);
/**
 * ## Summary
 * 按零基索引借用当前会话的一条覆盖率行快照，供报告读取函数身份、来源、行号和
 * 可执行/命中状态。
 *
 * ## Parameters
 * @param[in] coverage 已 Init 的会话对象，可为 NULL；返回指针不转移所有权。
 * @param[in] index `0 <= index < ZrDebug_Coverage_GetLineCount(coverage)` 的零基
 *     下标；越界只返回 NULL。
 *
 * ## Returns
 * @return 指向会话内部 `ZrDebugCoverageLine` 的只读借用指针；coverage 为 NULL 或
 *     index 越界时返回 NULL。指针指向的函数字段仍借用 VM，文本字段由会话持有。
 *
 * ## Constraints and lifecycle
 * 调用方应在 Stop 后完成遍历；新增行仅在需要扩容时使此前物理指针失效；重复登记或 LINE hook 也可能原位更新该记录。Reset
 * 使记录逻辑失效并允许以后复用，Destroy 释放数组。必须在函数和 state 生命周期内读取
 * `function` 字段，不得把该地址保存到会话之外长期使用。
 *
 * ## References
 * CLI writes executable and executed at zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:207; the test reads each
 * borrowed row at tests/profile/test_coverage.c:129 and asserts hit/miss states at
 * tests/profile/test_coverage.c:132 and tests/profile/test_coverage.c:137.
 */
ZR_DEBUG_API const ZrDebugCoverageLine *ZrDebug_Coverage_GetLine(const ZrDebugCoverage *coverage, TZrSize index);

#endif
