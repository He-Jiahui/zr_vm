#include "ssa_owner_fault_allocator.h"

#include <stdlib.h>

static size_t failureOrdinal;
static size_t allocationOrdinal;
static size_t outstanding;
static int failed;

void ssa_owner_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = 0;
}

int ssa_owner_allocation_failed(void) { return failed; }
size_t ssa_owner_outstanding_allocations(void) { return outstanding; }

static int allocation_should_fail(void) {
    ++allocationOrdinal;
    if (failureOrdinal != 0u && allocationOrdinal == failureOrdinal) {
        failed = 1;
        return 1;
    }
    return 0;
}

static void *owner_malloc(size_t size) {
    void *result;
    if (allocation_should_fail()) return NULL;
    result = malloc(size);
    if (result != NULL) ++outstanding;
    return result;
}

static void *owner_calloc(size_t count, size_t size) {
    void *result;
    if (allocation_should_fail()) return NULL;
    result = calloc(count, size);
    if (result != NULL) ++outstanding;
    return result;
}

static void owner_free(void *memory) {
    if (memory != NULL) --outstanding;
    free(memory);
}

/* Compile the production analysis with allocation interception only in this
 * test executable. Other state-map suites link its ordinary translation unit. */
#define malloc owner_malloc
#define calloc owner_calloc
#define free owner_free
#include "exec_ir/exec_ir_owner_state.c"

/**
 * @var static size_t failureOrdinal
 *
 * ## Summary
 * 指定要拒绝的单个分配ordinal；0禁用注入。
 *
 * ## Constraints and lifecycle
 * 配置由ssa_owner_fail_allocation写入；allocation_should_fail在每次hook分配时比较，匹配仅在该次返回失败。
 * 不拥有内存，也不因owner分析成功或失败自动复位。禁用/换轮次须显式调用control；不表示自然malloc/calloc失败。
 * 状态为本allocator翻译单元的static存储：每个独立测试可执行程序各有实例，同一进程内由调用方串行使用，不支持并发或重入的故障配置。
 * 它只描述经过本TU hook的分配，不计整个进程、Clone、liveness、state-map、prepared池或测试fixture在其他TU的分配。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:5
 * - tests/parser/ssa_owner_fault_allocator.c:11
 * - tests/parser/ssa_owner_fault_allocator.c:21
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:324
 * - tests/parser/test_ssa_state_map_ownership.c:325
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:360
 * - tests/parser/test_ssa_state_map_ownership.c:361
 * - tests/parser/test_ssa_state_map_ownership.c:362
 */

/**
 * @var static size_t allocationOrdinal
 *
 * ## Summary
 * 记录自上次control调用以来经过malloc/calloc hook的分配尝试序号。
 *
 * ## Constraints and lifecycle
 * 每次allocation_should_fail先递增再比较；禁用注入时仍递增，调用control重置为0。
 * 包含返回NULL的尝试，也包含在同一Build早期失败后仍继续执行的后续分配表达式；不是存活分配数。计数使用size_t且无额外饱和/上限检查，现有测试只遍历小ordinal。
 * 状态为本allocator翻译单元的static存储：每个独立测试可执行程序各有实例，同一进程内由调用方串行使用，不支持并发或重入的故障配置。
 * 它只描述经过本TU hook的分配，不计整个进程、Clone、liveness、state-map、prepared池或测试fixture在其他TU的分配。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:6
 * - tests/parser/ssa_owner_fault_allocator.c:12
 * - tests/parser/ssa_owner_fault_allocator.c:20
 * - tests/parser/ssa_owner_fault_allocator.c:21
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:324
 * - tests/parser/test_ssa_state_map_ownership.c:325
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:360
 * - tests/parser/test_ssa_state_map_ownership.c:361
 * - tests/parser/test_ssa_state_map_ownership.c:362
 */

/**
 * @var static size_t outstanding
 *
 * ## Summary
 * 计数本TU hook取得且尚未经本hook释放的非NULL分配。
 *
 * ## Constraints and lifecycle
 * 成功hook分配增加，owner_free非NULL减少；control不清除此计数。
 * 不是完整allocator注册表：不记录地址/尺寸，不拒绝双重或外来free。所有非NULL传入owner_free的地址须是本hook尚未释放的分配，否则计数和释放前提均失效。
 * 状态为本allocator翻译单元的static存储：每个独立测试可执行程序各有实例，同一进程内由调用方串行使用，不支持并发或重入的故障配置。
 * 它只描述经过本TU hook的分配，不计整个进程、Clone、liveness、state-map、prepared池或测试fixture在其他TU的分配。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:7
 * - tests/parser/ssa_owner_fault_allocator.c:17
 * - tests/parser/ssa_owner_fault_allocator.c:32
 * - tests/parser/ssa_owner_fault_allocator.c:40
 * - tests/parser/ssa_owner_fault_allocator.c:45
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:324
 * - tests/parser/test_ssa_state_map_ownership.c:325
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:360
 * - tests/parser/test_ssa_state_map_ownership.c:361
 * - tests/parser/test_ssa_state_map_ownership.c:362
 */

/**
 * @var static int failed
 *
 * ## Summary
 * 保存本轮是否命中过一次指定的注入ordinal。
 *
 * ## Constraints and lifecycle
 * control置0；选定ordinal时置1，后续尝试不自动清除，getter只读。
 * 自然malloc/calloc返回NULL不会自行将该标记置1。标记为0不能证明没有真实OOM，标记为1也只说明本轮曾命中注入点，不能定位全部失败原因。
 * 状态为本allocator翻译单元的static存储：每个独立测试可执行程序各有实例，同一进程内由调用方串行使用，不支持并发或重入的故障配置。
 * 它只描述经过本TU hook的分配，不计整个进程、Clone、liveness、state-map、prepared池或测试fixture在其他TU的分配。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:8
 * - tests/parser/ssa_owner_fault_allocator.c:13
 * - tests/parser/ssa_owner_fault_allocator.c:16
 * - tests/parser/ssa_owner_fault_allocator.c:22
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:324
 * - tests/parser/test_ssa_state_map_ownership.c:325
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:360
 * - tests/parser/test_ssa_state_map_ownership.c:361
 * - tests/parser/test_ssa_state_map_ownership.c:362
 */

/**
 * @fn void ssa_owner_fail_allocation(size_t ordinal)
 *
 * ## Summary
 * 设置本TU下一轮单点故障的ordinal，并将allocationOrdinal与failed重置为0；传0用于关闭注入。
 *
 * ## Parameters
 * @param[in] ordinal 0禁用注入；非零指定下一轮hook尝试中恰好这一序号被拒绝，并重置尝试序号与failed标记。
 *
 * ## Returns
 * 无C返回值；仅修改控制状态，不分配、释放或复位outstanding。
 *
 * ## Constraints and lifecycle
 * 调用方须串行控制，不得与hook执行并发。已有hook分配仍须按原所有权释放；不能通过重新配置把未释放计数伪装为0。
 * ownership两个OOM case在生产调用返回后先传0，再读outstanding并执行断言；其数据流不依赖tearDown自动禁用注入。
 * ordinal表示hook尝试，不是整个消费者API的第几个全局malloc。单次消费者可多次调用OwnerAnalysisBuild，因此不能由本TU固定8个源码分配表达式推断整个测试恰好8次失败。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:10
 * - tests/parser/ssa_owner_fault_allocator.c:11
 * - tests/parser/ssa_owner_fault_allocator.c:12
 * - tests/parser/ssa_owner_fault_allocator.c:13
 * - tests/parser/ssa_owner_fault_allocator.c:19
 * - tests/parser/ssa_owner_fault_allocator.c:21
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:325
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:361
 * - tests/parser/test_ssa_conditional_cleanup.c:661
 * - tests/parser/test_ssa_conditional_cleanup.c:664
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:362
 */

/**
 * @fn int ssa_owner_allocation_failed(void)
 *
 * ## Summary
 * 读取自最近一次control配置以来是否命中过指定的注入ordinal。
 *
 * ## Returns
 * @return 命中过返回1，否则返回0；读取不清除标记。
 *
 * ## Constraints and lifecycle
 * 只有allocation_should_fail匹配时置1，控制函数会清零。真实malloc/calloc返回NULL不自行设置此标记；其他TU分配也不可见。
 * 返回整数不持有资源，调用方须与配置/hook串行，不能把该值当全进程分配成功证明。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:8
 * - tests/parser/ssa_owner_fault_allocator.c:13
 * - tests/parser/ssa_owner_fault_allocator.c:16
 * - tests/parser/ssa_owner_fault_allocator.c:22
 * - tests/parser/test_ssa_state_map_ownership.c:324
 * - tests/parser/test_ssa_state_map_ownership.c:360
 * - tests/parser/test_ssa_conditional_cleanup.c:663
 */

/**
 * @fn size_t ssa_owner_outstanding_allocations(void)
 *
 * ## Summary
 * 返回本TU hook成功分配后尚未经过owner_free释放的非NULL分配计数。
 *
 * ## Returns
 * @return 返回当前outstanding值，不重置计数，不传回内存所有权。
 *
 * ## Constraints and lifecycle
 * 只在所有非NULL释放均来自本hook且每次仅释放一次的前提下反映该TU存活分配数；未记录地址，不能检测外来free、双free或其他TU泄漏。
 * ssa_owner_fail_allocation不修改该数。测试在关闭注入后比较0，含义是当前hook范围无未释放计数，不是全程序泄漏检查。
 * 计数不是原子变量，只能串行访问；返回后计数也可能被后续hook改变。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:7
 * - tests/parser/ssa_owner_fault_allocator.c:17
 * - tests/parser/ssa_owner_fault_allocator.c:32
 * - tests/parser/ssa_owner_fault_allocator.c:40
 * - tests/parser/ssa_owner_fault_allocator.c:45
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:362
 * - tests/parser/test_ssa_conditional_cleanup.c:665
 */

/**
 * @fn static int allocation_should_fail(void)
 *
 * ## Summary
 * 每次malloc/calloc hook进入时推进allocationOrdinal，并在非零failureOrdinal与之相等时选择一次注入失败。
 *
 * ## Returns
 * @return 本次应注入返回1并把failed置1；其他情况返回0。
 *
 * ## Constraints and lifecycle
 * 0配置仅禁用拒绝，仍推进尝试序号；failed只在配置重置时清除，后续非匹配尝试不清除它。
 * 函数不分配、不清理，也不检测底层allocator的自然失败。size_t计数按C整数规则递增，无额外上限机制；现有测试只用有限小序号。
 * 与两个hook及控制接口共用TU-static状态，须串行，不能作为全进程或跨TU故障调度器。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:19
 * - tests/parser/ssa_owner_fault_allocator.c:20
 * - tests/parser/ssa_owner_fault_allocator.c:21
 * - tests/parser/ssa_owner_fault_allocator.c:22
 * - tests/parser/ssa_owner_fault_allocator.c:23
 * - tests/parser/ssa_owner_fault_allocator.c:25
 * - tests/parser/ssa_owner_fault_allocator.c:30
 * - tests/parser/ssa_owner_fault_allocator.c:38
 */

/**
 * @fn static void *owner_malloc(size_t size)
 *
 * ## Summary
 * 为本TU在宏定义之后展开的owner分析malloc调用提供单点注入；没有注入时委托原C malloc，成功非NULL才增加outstanding。
 *
 * ## Parameters
 * @param[in] size 原样转发给底层C malloc的字节数量；本hook不另作尺寸验证。
 *
 * ## Returns
 * @return 命中注入或底层malloc失败返回NULL；成功返回由调用方拥有、须经本TU对应Free路径释放的内存。size为0时遵循底层malloc行为，不额外承诺非NULL。
 *
 * ## Constraints and lifecycle
 * 此函数的定义出现在#define malloc之前，体内malloc调用不递归替换为自己。后面的对象宏把包含生产C中的malloc标识符替换为本函数。
 * 注入失败会将failed置1；底层自然NULL不会额外置1。返回非NULL才记存活数量，失败不产生可释放对象。
 * 正常生产分析通过OwnerAnalysisFree/统一scratch清理在同一编译TU释放。caller不得把外来分配或已释放地址交给owner_free；状态须串行使用。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:28
 * - tests/parser/ssa_owner_fault_allocator.c:30
 * - tests/parser/ssa_owner_fault_allocator.c:31
 * - tests/parser/ssa_owner_fault_allocator.c:32
 * - tests/parser/ssa_owner_fault_allocator.c:33
 * - tests/parser/ssa_owner_fault_allocator.c:51
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:307
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:308
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:309
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:362
 */

/**
 * @fn static void *owner_calloc(size_t count, size_t size)
 *
 * ## Summary
 * 为本TU的owner分析calloc调用提供单点故障和outstanding计数；未命中注入时调用原C calloc。
 *
 * ## Parameters
 * @param[in] count 原样转发底层calloc的元素数量。
 * @param[in] size 原样转发底层calloc的单元素字节数；hook不单独检查乘法，生产Build有自己的尺寸前置检查。
 *
 * ## Returns
 * @return 注入或底层失败返回NULL；非NULL时返回底层calloc给出的对象并增加outstanding，所有权由生产分析取得。
 *
 * ## Constraints and lifecycle
 * 此定义早于#define calloc，底层调用保留原C allocator，不会递归进入本hook。calloc的初始化、自然OOM、零长度及乘法处理按底层实现；hook本身不增加形状验证。
 * selected ordinal置failed，底层自然NULL不额外设置failed；只有非NULL结果计数。生产Build先检查尺寸，再调用这里的5个源码分配表达式。
 * 归还必须经过同TU的匹配free路径，不能把outstanding当地址注册表或全进程存活分配数；所有hook/control串行使用。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:36
 * - tests/parser/ssa_owner_fault_allocator.c:38
 * - tests/parser/ssa_owner_fault_allocator.c:39
 * - tests/parser/ssa_owner_fault_allocator.c:40
 * - tests/parser/ssa_owner_fault_allocator.c:41
 * - tests/parser/ssa_owner_fault_allocator.c:52
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:297
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:302
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:303
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:304
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:305
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:306
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:362
 */

/**
 * @fn static void owner_free(void *memory)
 *
 * ## Summary
 * 对非NULL地址先减少outstanding，再调用原C free；NULL同样传给C free但不改变计数。
 *
 * ## Parameters
 * @param[in] memory NULL或本TU hook返回且尚未释放的、兼容C free的独占指针；不接受任意外来非NULL地址。
 *
 * ## Returns
 * 无C返回值；有效非NULL分配被释放，其旧指针/元素借用失效。
 *
 * ## Constraints and lifecycle
 * 此定义早于#define free，底层free保持原C释放器。hook不校验地址来源、是否已释放或outstanding是否为0，非NULL参数必须是本hook尚未释放的分配。
 * 不能释放借用池、栈内存或其他TU随意传来的非NULL分配来获得可信计数；双重/外来free也不由此wrapper修复。
 * 生产OwnerAnalysisFree释放3输出池，统一路径释放5个scratch；同一源码行可含多个free表达式，不能把5个坐标误当作只有5次静态free调用。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:44
 * - tests/parser/ssa_owner_fault_allocator.c:45
 * - tests/parser/ssa_owner_fault_allocator.c:46
 * - tests/parser/ssa_owner_fault_allocator.c:53
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:211
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:212
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:213
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:313
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:419
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:362
 */

/**
 * @def malloc
 *
 * ## Summary
 * 把后续包含生产分析中的malloc标识符替换为owner_malloc。
 *
 * ## Returns
 * 这是对象式预处理替换，不是带参数宏或C返回语句；参数/结果由对应hook函数合同规定。
 *
 * ## Constraints and lifecycle
 * 替换从本#define开始到本TU结束，没有跨翻译单元或链接器级拦截；本文件最后文本包含生产owner分析C。其原物理定义仍属于生产C，不为此编译实例另计生产函数键。
 * stdlib.h在宏定义前包含；三个hook定义也在宏之前，hook内部仍调用原C allocator。所有权、自然失败、计数和串行前提见对应hook。
 * 当前替身涵盖owner分析3输出+5scratch，其他TU中的Clone/liveness/map/prepared/fixture分配仍使用它们原来的allocator。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:51
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:307
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:308
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:309
 */

/**
 * @def calloc
 *
 * ## Summary
 * 把后续包含生产分析中的calloc标识符替换为owner_calloc。
 *
 * ## Returns
 * 这是对象式预处理替换，不是带参数宏或C返回语句；参数/结果由对应hook函数合同规定。
 *
 * ## Constraints and lifecycle
 * 替换从本#define开始到本TU结束，没有跨翻译单元或链接器级拦截；本文件最后文本包含生产owner分析C。其原物理定义仍属于生产C，不为此编译实例另计生产函数键。
 * stdlib.h在宏定义前包含；三个hook定义也在宏之前，hook内部仍调用原C allocator。所有权、自然失败、计数和串行前提见对应hook。
 * 当前替身涵盖owner分析3输出+5scratch，其他TU中的Clone/liveness/map/prepared/fixture分配仍使用它们原来的allocator。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:52
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:302
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:303
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:304
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:305
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:306
 */

/**
 * @def free
 *
 * ## Summary
 * 把后续包含生产分析中的free标识符替换为owner_free。
 *
 * ## Returns
 * 这是对象式预处理替换，不是带参数宏或C返回语句；参数/结果由对应hook函数合同规定。
 *
 * ## Constraints and lifecycle
 * 替换从本#define开始到本TU结束，没有跨翻译单元或链接器级拦截；本文件最后文本包含生产owner分析C。其原物理定义仍属于生产C，不为此编译实例另计生产函数键。
 * stdlib.h在宏定义前包含；三个hook定义也在宏之前，hook内部仍调用原C allocator。所有权、自然失败、计数和串行前提见对应hook。
 * 当前替身涵盖owner分析3输出+5scratch，其他TU中的Clone/liveness/map/prepared/fixture分配仍使用它们原来的allocator。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:53
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:211
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:212
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:213
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:313
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:419
 */

/**
 * @anchor ssa_owner_analysis_fault_instantiation
 *
 * ## Summary
 * 在三个allocator alias生效后文本包含原生产exec_ir_owner_state.c，生成带单点分配故障计数的owner分析编译实例。
 *
 * ## Returns
 * 编译本TU会提供原OwnerAnalysis/OwnerState等生产符号；这不是运行期函数返回，也不产生独立源码定义副本。
 *
 * ## Constraints and lifecycle
 * 本文件直接编译为测试target的一个TU，不被主ownership测试C文本包含。其header只声明控制接口；fault注入由link到这些控制接口及本替身实现共同形成。
 * 宏仅影响当前TU后续预处理文本。普通生产owner分析C在其他target中仍按正常allocator编译；同target须避免再显式编入同一普通实现。cleanup CMake明确从复用源表移除普通OwnerC，再加本wrapper；ownership目标本来就列wrapper而不列普通OwnerC。
 * 生产物理文件中的12函数和2宏不因文本include再产生新canonical信用。本block仅解释编译实例/宏范围，不替代生产函数或linked其他allocator的逐项审查。
 * 3输出+5scratch的8个分配表达式均进入本TUhook；消费者可多次调用该分析，测试ordinal循环不报告精确8个失败次数。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.c:1
 * - tests/parser/ssa_owner_fault_allocator.c:3
 * - tests/parser/ssa_owner_fault_allocator.c:28
 * - tests/parser/ssa_owner_fault_allocator.c:36
 * - tests/parser/ssa_owner_fault_allocator.c:44
 * - tests/parser/ssa_owner_fault_allocator.c:51
 * - tests/parser/ssa_owner_fault_allocator.c:52
 * - tests/parser/ssa_owner_fault_allocator.c:53
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:1
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:302
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:419
 * - tests/cmake/ssa-tests.cmake:258
 * - tests/cmake/ssa-cleanup-tests.cmake:7
 * - tests/cmake/ssa-cleanup-tests.cmake:10
 */
