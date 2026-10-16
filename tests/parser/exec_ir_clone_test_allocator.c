#undef malloc
#undef calloc
#undef realloc
#undef free

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct SZrExecIrTestAllocation {
    void *address;
    struct SZrExecIrTestAllocation *next;
} SZrExecIrTestAllocation;

static SZrExecIrTestAllocation *g_zr_exec_ir_test_allocations;
static uint64_t g_zr_exec_ir_test_allocation_count;
static uint64_t g_zr_exec_ir_test_fail_at;
static uint64_t g_zr_exec_ir_test_live_count;

static void zr_exec_ir_test_track(void *address) {
    SZrExecIrTestAllocation *entry;
    if (address == NULL) return;
    entry = (SZrExecIrTestAllocation *)malloc(sizeof(*entry));
    if (entry == NULL) abort();
    entry->address = address;
    entry->next = g_zr_exec_ir_test_allocations;
    g_zr_exec_ir_test_allocations = entry;
    ++g_zr_exec_ir_test_live_count;
}

static void zr_exec_ir_test_untrack(void *address) {
    SZrExecIrTestAllocation **cursor = &g_zr_exec_ir_test_allocations;
    while (*cursor != NULL) {
        if ((*cursor)->address == address) {
            SZrExecIrTestAllocation *entry = *cursor;
            *cursor = entry->next;
            free(entry);
            --g_zr_exec_ir_test_live_count;
            return;
        }
        cursor = &(*cursor)->next;
    }
}

static int zr_exec_ir_test_should_fail(void) {
    ++g_zr_exec_ir_test_allocation_count;
    return g_zr_exec_ir_test_fail_at != 0u &&
           g_zr_exec_ir_test_allocation_count == g_zr_exec_ir_test_fail_at;
}

void *zr_exec_ir_test_malloc(size_t size) {
    void *address;
    if (zr_exec_ir_test_should_fail()) return NULL;
    address = malloc(size);
    zr_exec_ir_test_track(address);
    return address;
}

void *zr_exec_ir_test_calloc(size_t count, size_t size) {
    void *address;
    if (zr_exec_ir_test_should_fail()) return NULL;
    address = calloc(count, size);
    zr_exec_ir_test_track(address);
    return address;
}

void *zr_exec_ir_test_realloc(void *storage, size_t size) {
    void *address;
    SZrExecIrTestAllocation *entry = g_zr_exec_ir_test_allocations;
    while (entry != NULL && entry->address != storage) entry = entry->next;
    if (zr_exec_ir_test_should_fail()) return NULL;
    address = realloc(storage, size);
    if (address == NULL) return NULL;
    if (entry != NULL) entry->address = address;
    else zr_exec_ir_test_track(address);
    return address;
}

void zr_exec_ir_test_free(void *storage) {
    if (storage != NULL) zr_exec_ir_test_untrack(storage);
    free(storage);
}

void ZrCore_ExecIr_TestAllocatorReset(void) {
    g_zr_exec_ir_test_allocation_count = 0u;
    g_zr_exec_ir_test_fail_at = 0u;
}

void ZrCore_ExecIr_TestAllocatorFailAt(uint64_t allocationIndex) {
    g_zr_exec_ir_test_allocation_count = 0u;
    g_zr_exec_ir_test_fail_at = allocationIndex;
}

uint64_t ZrCore_ExecIr_TestAllocatorCount(void) {
    return g_zr_exec_ir_test_allocation_count;
}

uint64_t ZrCore_ExecIr_TestAllocatorLiveCount(void) {
    return g_zr_exec_ir_test_live_count;
}

/**
 * @anchor exec_ir_clone_test_native_allocator_boundary
 *
 * ## Summary
 * 在实现替身分配器前取消强制包含头的四个分配别名，保留真实libc分配和释放入口，避免包装器递归调用自身。
 *
 * ## Constraints and lifecycle
 * 只影响本TU此处之后的预处理环境；malloc/calloc/realloc/free的真实声明随后由stdlib提供。追踪链表节点的malloc/free不纳入注入尝试数或存活payload数，节点分配失败走abort。它不取消其他TUs中的强制头宏，也不改变生产源的物理定义或创建新生产函数键。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:1
 * - tests/parser/exec_ir_clone_test_allocator.c:4
 * - tests/parser/exec_ir_clone_test_allocator.c:8
 * - tests/parser/exec_ir_clone_test_allocator.c:23
 * - tests/parser/exec_ir_clone_test_allocator.c:37
 * - tests/parser/exec_ir_clone_test_allocator.h:15
 */

/**
 * @struct SZrExecIrTestAllocation
 *
 * ## Summary
 * 保存一个成功payload分配的地址及追踪链表后继，供移动realloc更新和free按地址移除。
 *
 * ## Fields and invariants
 * address记录当前payload地址，realloc成功可改写；next仅连接本tracker节点。类型本身不授予任意指针可free的证明。
 *
 * ## Constraints and lifecycle
 * 这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。节点由真实malloc创建、真实free销毁，address借指向payload而非复制或保活它；正常包装操作维持每个受跟踪存活payload一个节点。Reset/FailAt不清链。节点分配失败abort，不能把记账节点也算成28/60注入范围。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:10
 * - tests/parser/exec_ir_clone_test_allocator.c:23
 * - tests/parser/exec_ir_clone_test_allocator.c:25
 * - tests/parser/exec_ir_clone_test_allocator.c:35
 * - tests/parser/exec_ir_clone_test_allocator.c:37
 * - tests/parser/exec_ir_clone_test_allocator.c:69
 * - tests/parser/exec_ir_clone_test_allocator.c:74
 */

/**
 * @var static SZrExecIrTestAllocation *g_zr_exec_ir_test_allocations
 *
 * ## Summary
 * 持有本编译实例的存活payload追踪链表头，供分配登记、realloc查找和释放移除。
 *
 * ## Constraints and lifecycle
 * 静态初始化为空；track发布新头，untrack移除首个匹配节点，Reset/FailAt均保留它。链表节点独立拥有，payload释放仍由真实free执行。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:15
 * - tests/parser/exec_ir_clone_test_allocator.c:27
 * - tests/parser/exec_ir_clone_test_allocator.c:32
 * - tests/parser/exec_ir_clone_test_allocator.c:69
 * - tests/parser/exec_ir_clone_test_allocator.c:84
 */

/**
 * @var static uint64_t g_zr_exec_ir_test_allocation_count
 *
 * ## Summary
 * 累计自最近一次Reset或FailAt起到达的包装malloc/calloc/realloc尝试，供故障遍历验证在首个失败处停止。
 *
 * ## Constraints and lifecycle
 * should_fail在真实分配或合成失败前递增；合成失败、自然失败均计入，free与记账节点分配不计。uint64计数按C无符号运算，不附加溢出保护。Reset/FailAt清零，Count只读。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:16
 * - tests/parser/exec_ir_clone_test_allocator.c:46
 * - tests/parser/exec_ir_clone_test_allocator.c:85
 * - tests/parser/exec_ir_clone_test_allocator.c:90
 * - tests/parser/exec_ir_clone_test_allocator.c:95
 */

/**
 * @var static uint64_t g_zr_exec_ir_test_fail_at
 *
 * ## Summary
 * 选择当前串行场景中从1开始的一次包装分配尝试，0表示不注入故障。
 *
 * ## Constraints and lifecycle
 * FailAt覆盖此值并清尝试数；Reset清零禁用。should_fail只在累计数相等时返回真，之后不自动重新arm；自然OOM不改该位置。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:17
 * - tests/parser/exec_ir_clone_test_allocator.c:47
 * - tests/parser/exec_ir_clone_test_allocator.c:48
 * - tests/parser/exec_ir_clone_test_allocator.c:86
 * - tests/parser/exec_ir_clone_test_allocator.c:91
 */

/**
 * @var static uint64_t g_zr_exec_ir_test_live_count
 *
 * ## Summary
 * 记录当前链表登记的存活payload数量，作为失败回滚的有限资源守恒证据。
 *
 * ## Constraints and lifecycle
 * track非NULL发布后递增，untrack找到节点才递减，realloc既有节点成功仅改地址。Reset/FailAt不清零；不统计链表节点、未经过强制替换的linked分配或整个进程堆。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:18
 * - tests/parser/exec_ir_clone_test_allocator.c:28
 * - tests/parser/exec_ir_clone_test_allocator.c:38
 * - tests/parser/exec_ir_clone_test_allocator.c:74
 * - tests/parser/exec_ir_clone_test_allocator.c:99
 */

/**
 * @fn static void zr_exec_ir_test_track(void *address)
 *
 * ## Summary
 * 在真实分配返回后登记非NULL payload，建立后续realloc/free的地址记账。
 *
 * ## Parameters
 * @param[in] address 新成功payload地址；NULL表示没有payload并直接返回。
 *
 * ## Returns
 * 无C返回值；正常返回时非NULL地址已入链并增加live计数，节点分配失败调用abort终止进程。
 *
 * ## Constraints and lifecycle
 * 地址须尚未在链中登记并由兼容C分配器分配；函数不读取或初始化payload。节点使用真实malloc，不递归进入故障计数。正常节点由untrack释放，payload生命周期由调用者及包装free/realloc管理。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:20
 * - tests/parser/exec_ir_clone_test_allocator.c:22
 * - tests/parser/exec_ir_clone_test_allocator.c:23
 * - tests/parser/exec_ir_clone_test_allocator.c:24
 * - tests/parser/exec_ir_clone_test_allocator.c:55
 * - tests/parser/exec_ir_clone_test_allocator.c:63
 * - tests/parser/exec_ir_clone_test_allocator.c:75
 */

/**
 * @fn static void zr_exec_ir_test_untrack(void *address)
 *
 * ## Summary
 * 在真实free前按地址从追踪链表删除首个匹配项，并释放该记账节点。
 *
 * ## Parameters
 * @param[in] address 待解除登记的payload地址值；仅比较指针，不读取payload。
 *
 * ## Returns
 * 无C返回值；找到节点则live减一并返回，没有匹配项则保持链表和live计数。
 *
 * ## Constraints and lifecycle
 * 本函数不释放payload、不证明该地址可free，也不清空调用者指针；实际free在包装器中随后执行。正常登记唯一性由包装调用路径维持，重复或无效释放不能靠此查找变安全。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:31
 * - tests/parser/exec_ir_clone_test_allocator.c:35
 * - tests/parser/exec_ir_clone_test_allocator.c:37
 * - tests/parser/exec_ir_clone_test_allocator.c:38
 * - tests/parser/exec_ir_clone_test_allocator.c:80
 * - tests/parser/exec_ir_clone_test_allocator.c:81
 */

/**
 * @fn static int zr_exec_ir_test_should_fail(void)
 *
 * ## Summary
 * 在每次包装分配开始时推进尝试序号，判断本次是否命中预设单次故障。
 *
 * ## Returns
 * @return 返回非0表示非零fail_at与递增后的尝试计数相等，否则返回0；不执行实际分配。
 *
 * ## Constraints and lifecycle
 * malloc/calloc/realloc均在委托libc之前调用，合成失败也已计数。0位置禁用；该函数不自动清除fail_at、不保存自然OOM标志，也不操作live链。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:45
 * - tests/parser/exec_ir_clone_test_allocator.c:46
 * - tests/parser/exec_ir_clone_test_allocator.c:47
 * - tests/parser/exec_ir_clone_test_allocator.c:53
 * - tests/parser/exec_ir_clone_test_allocator.c:61
 * - tests/parser/exec_ir_clone_test_allocator.c:71
 */

/**
 * @fn void *zr_exec_ir_test_malloc(size_t size)
 *
 * ## Summary
 * 包装malloc，将一次payload分配计入可选择的故障序列，并对真实成功的非NULL结果建立存活记账。
 *
 * ## Parameters
 * @param[in] size 请求的字节数，按真实malloc解释；0字节结果依赖C库，不保证非NULL。
 *
 * ## Returns
 * @return 返回NULL表示命中合成故障或真实malloc失败/返回NULL；否则返回需与本包装free或适用realloc配对的payload指针。
 *
 * ## Constraints and lifecycle
 * should_fail先计数，命中时不调用libc；真实NULL不入链。记账节点自然OOM会abort，不作为可恢复NULL返回。分配的字节不初始化；释放后借用失效。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:51
 * - tests/parser/exec_ir_clone_test_allocator.c:53
 * - tests/parser/exec_ir_clone_test_allocator.c:54
 * - tests/parser/exec_ir_clone_test_allocator.c:55
 * - tests/parser/exec_ir_clone_test_allocator.h:10
 */

/**
 * @fn void *zr_exec_ir_test_calloc(size_t count, size_t size)
 *
 * ## Summary
 * 包装calloc，把整个count×size请求作为一次故障尝试，成功后登记payload供统一回滚检查。
 *
 * ## Parameters
 * @param[in] count 交给真实calloc的元素数量。
 * @param[in] size 每个元素字节数；乘法及零请求结果由真实calloc处理，本包装不另做乘法验证。
 *
 * ## Returns
 * @return 返回NULL表示合成拒绝或真实calloc返回NULL；否则返回零初始化payload并登记一个存活项。
 *
 * ## Constraints and lifecycle
 * 一次调用只推进一个ordinal，不按元素计次。只有非NULL结果入链；记账节点OOM调用abort。正常payload由配对free/realloc处理，不能把所有linked库分配都算进该tracker。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:59
 * - tests/parser/exec_ir_clone_test_allocator.c:61
 * - tests/parser/exec_ir_clone_test_allocator.c:62
 * - tests/parser/exec_ir_clone_test_allocator.c:63
 * - tests/parser/exec_ir_clone_test_allocator.h:11
 */

/**
 * @fn void *zr_exec_ir_test_realloc(void *storage, size_t size)
 *
 * ## Summary
 * 包装realloc，在不破坏旧登记的情况下先注入故障；真实成功后更新原节点地址或登记新payload。
 *
 * ## Parameters
 * @param[in,out] storage NULL或兼容C realloc的现有分配首地址；非NULL正常调用须来自同一配对生命周期且未释放。
 * @param[in] size 本克隆增长路径使用正字节请求；0字节realloc的实现定义行为不由本tracker补偿或认证。
 *
 * ## Returns
 * @return 合成故障返回NULL且尚未调用libc，旧payload和登记保持；正字节真实realloc返回NULL时也保留旧payload及登记；非NULL成功返回新地址并维护一个登记。
 *
 * ## Constraints and lifecycle
 * 先按旧地址找节点，再调用should_fail。成功后旧地址借用按realloc规则失效，即使新值相同也应使用返回值；原节点存在时不改变live计数，否则创建节点。正字节前提对应Core reserve仅在真正增长且elementSize非零时调用。size0不能套用上述失败保留保证。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:67
 * - tests/parser/exec_ir_clone_test_allocator.c:69
 * - tests/parser/exec_ir_clone_test_allocator.c:70
 * - tests/parser/exec_ir_clone_test_allocator.c:71
 * - tests/parser/exec_ir_clone_test_allocator.c:72
 * - tests/parser/exec_ir_clone_test_allocator.c:73
 * - tests/parser/exec_ir_clone_test_allocator.c:74
 * - tests/parser/exec_ir_clone_test_allocator.c:75
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:44
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:72
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 * - tests/parser/exec_ir_clone_test_allocator.h:12
 */

/**
 * @fn void zr_exec_ir_test_free(void *storage)
 *
 * ## Summary
 * 释放payload前尝试移除其追踪节点，使正常销毁与克隆回滚更新同一live计数。
 *
 * ## Parameters
 * @param[in] storage NULL或兼容真实free且尚未释放的分配首地址；传入值不回写为NULL。
 *
 * ## Returns
 * 无C返回值；NULL遵循真实free的无操作语义，非NULL在解除可能的登记后委托真实free。
 *
 * ## Constraints and lifecycle
 * 未登记地址也会传给真实free，不能据此证明任意地址合法。仅找到节点才live减一；节点先释放，payload随后释放，所有旧payload借用失效。free不计入故障ordinal；双重free、并发访问或来自不兼容allocator的地址均由调用方排除。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:79
 * - tests/parser/exec_ir_clone_test_allocator.c:80
 * - tests/parser/exec_ir_clone_test_allocator.c:81
 * - tests/parser/exec_ir_clone_test_allocator.h:13
 */

/**
 * @fn void ZrCore_ExecIr_TestAllocatorReset(void)
 *
 * ## Summary
 * 在成功探测或场景收尾时关闭注入并开始新的尝试计数区间，保留已有fixture的存活记账。
 *
 * ## Returns
 * 无C返回值；allocation_count和fail_at均置0。
 *
 * ## Constraints and lifecycle
 * 不释放payload或节点，不修改live_count。必须先结束上一操作，再Reset后构造下一轮需要免注入的fixture或正常释放；不能用Reset掩盖泄漏。当前fault循环结束后显式Reset再Free。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:84
 * - tests/parser/exec_ir_clone_test_allocator.c:85
 * - tests/parser/exec_ir_clone_test_allocator.c:86
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:218
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:243
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:284
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:420
 * - tests/parser/test_ssa_core_model.c:10
 */

/**
 * @fn void ZrCore_ExecIr_TestAllocatorFailAt(uint64_t allocationIndex)
 *
 * ## Summary
 * 在下一次Clone调用前重新arm一个从1计数的分配故障位置。
 *
 * ## Parameters
 * @param[in] allocationIndex 本轮唯一合成拒绝的尝试序号；0关闭注入但仍将尝试数归零。
 *
 * ## Returns
 * 无C返回值；新位置替换旧位置，allocation_count重置为0。
 *
 * ## Constraints and lifecycle
 * 保留已有payload和live链，不能与另一线程的分配/释放或查询交错。当前两遍遍历分别传1..count，单次Clone结束后先读取计数/live及快照再重新arm；循环最终Reset禁用。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:89
 * - tests/parser/exec_ir_clone_test_allocator.c:90
 * - tests/parser/exec_ir_clone_test_allocator.c:91
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:227
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:268
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:231
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:272
 * - tests/parser/test_ssa_core_model.c:11
 */

/**
 * @fn uint64_t ZrCore_ExecIr_TestAllocatorCount(void)
 *
 * ## Summary
 * 读取最近Reset或FailAt以来包装分配尝试的数量，用于成功探测和失败短路断言。
 *
 * ## Returns
 * @return 返回当前uint64累计尝试数，不清空或增加计数。
 *
 * ## Constraints and lifecycle
 * 包含malloc/calloc/realloc合成与真实失败，不包含free或记账节点分配。必须在下一次Reset/FailAt前读取；不等于成功分配数、所有进程分配数或固有CloneAPI常量。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:94
 * - tests/parser/exec_ir_clone_test_allocator.c:95
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:221
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:222
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:261
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:262
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:308
 * - tests/parser/test_ssa_core_model.c:12
 */

/**
 * @fn uint64_t ZrCore_ExecIr_TestAllocatorLiveCount(void)
 *
 * ## Summary
 * 读取当前跟踪payload数，用于比较失败Clone前后的有限资源余额和默认runner末尾检查。
 *
 * ## Returns
 * @return 返回当前uint64 live计数，不修改链表或注入状态。
 *
 * ## Constraints and lifecycle
 * 与最近Reset/FailAt无关，只有正常登记/解除登记改变余额。不能证明未被宏替换的分配器、动态库、记账节点或任意堆均无泄漏；默认main到达末尾才检查0，五selector提前返回不经过该guard。这些状态由本支持C的单个编译实例共享；所有包装分配、释放、Reset、FailAt及查询须串行，代码没有原子操作或锁。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.c:98
 * - tests/parser/exec_ir_clone_test_allocator.c:99
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:224
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:234
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:264
 * - tests/parser/exec_ir_clone_test_allocator_cases.inc:275
 * - tests/parser/test_ssa_core_model.c:950
 * - tests/parser/test_ssa_core_model.c:13
 */
