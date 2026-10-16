#ifndef ZR_EXEC_IR_CLONE_TEST_ALLOCATOR_H
#define ZR_EXEC_IR_CLONE_TEST_ALLOCATOR_H

#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

void *zr_exec_ir_test_malloc(size_t size);
void *zr_exec_ir_test_calloc(size_t count, size_t size);
void *zr_exec_ir_test_realloc(void *storage, size_t size);
void zr_exec_ir_test_free(void *storage);

#define malloc zr_exec_ir_test_malloc
#define calloc zr_exec_ir_test_calloc
#define realloc zr_exec_ir_test_realloc
#define free zr_exec_ir_test_free

#endif

#endif

/**
 * @fn void *zr_exec_ir_test_malloc(size_t size)
 *
 * ## Summary
 * 声明测试强制包含环境使用的malloc替身；包装malloc，将一次payload分配计入可选择的故障序列，并对真实成功的非NULL结果建立存活记账。
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
 * - tests/parser/exec_ir_clone_test_allocator.h:10
 * - tests/parser/exec_ir_clone_test_allocator.c:51
 */

/**
 * @fn void *zr_exec_ir_test_calloc(size_t count, size_t size)
 *
 * ## Summary
 * 声明测试强制包含环境使用的calloc替身；包装calloc，把整个count×size请求作为一次故障尝试，成功后登记payload供统一回滚检查。
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
 * - tests/parser/exec_ir_clone_test_allocator.h:11
 * - tests/parser/exec_ir_clone_test_allocator.c:59
 */

/**
 * @fn void *zr_exec_ir_test_realloc(void *storage, size_t size)
 *
 * ## Summary
 * 声明测试强制包含环境使用的realloc替身；包装realloc，在不破坏旧登记的情况下先注入故障；真实成功后更新原节点地址或登记新payload。
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
 * - tests/parser/exec_ir_clone_test_allocator.h:12
 * - tests/parser/exec_ir_clone_test_allocator.c:67
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:75
 */

/**
 * @fn void zr_exec_ir_test_free(void *storage)
 *
 * ## Summary
 * 声明测试强制包含环境使用的free替身；释放payload前尝试移除其追踪节点，使正常销毁与克隆回滚更新同一live计数。
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
 * - tests/parser/exec_ir_clone_test_allocator.h:13
 * - tests/parser/exec_ir_clone_test_allocator.c:79
 */

/**
 * @def malloc
 *
 * ## Summary
 * 在定义ZR_EXEC_IR_TEST_ALLOCATOR时把后续TU中的malloc标识符映射到本测试的zr_exec_ir_test_malloc，使克隆路径及配对销毁进入同一tracker。
 *
 * ## Constraints and lifecycle
 * 对象式宏没有形参或自己的C返回值；适用原分配器参数及返回契约由被替换接口承担。stdlib在别名之前包含，避免改写系统原声明。强制包含只影响选定target的当前源TUs，不拦截预编译库内部调用；allocator.c先#undef四名以委托真实libc。不能与CFG/EIS6/Owner等另一套分配宏混用而不重新核预处理环境；该文件没有生产API重命名。 启用标志须在首次包含该头之前定义；guard已建立后再定义标志不会自动补装别名。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.h:15
 * - tests/parser/exec_ir_clone_test_allocator.h:4
 * - tests/parser/exec_ir_clone_test_allocator.h:8
 * - tests/parser/exec_ir_clone_test_allocator.c:1
 * - tests/parser/exec_ir_clone_test_allocator.c:4
 */

/**
 * @def calloc
 *
 * ## Summary
 * 在定义ZR_EXEC_IR_TEST_ALLOCATOR时把后续TU中的calloc标识符映射到本测试的zr_exec_ir_test_calloc，使克隆路径及配对销毁进入同一tracker。
 *
 * ## Constraints and lifecycle
 * 对象式宏没有形参或自己的C返回值；适用原分配器参数及返回契约由被替换接口承担。stdlib在别名之前包含，避免改写系统原声明。强制包含只影响选定target的当前源TUs，不拦截预编译库内部调用；allocator.c先#undef四名以委托真实libc。不能与CFG/EIS6/Owner等另一套分配宏混用而不重新核预处理环境；该文件没有生产API重命名。 启用标志须在首次包含该头之前定义；guard已建立后再定义标志不会自动补装别名。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.h:16
 * - tests/parser/exec_ir_clone_test_allocator.h:4
 * - tests/parser/exec_ir_clone_test_allocator.h:8
 * - tests/parser/exec_ir_clone_test_allocator.c:1
 * - tests/parser/exec_ir_clone_test_allocator.c:4
 */

/**
 * @def realloc
 *
 * ## Summary
 * 在定义ZR_EXEC_IR_TEST_ALLOCATOR时把后续TU中的realloc标识符映射到本测试的zr_exec_ir_test_realloc，使克隆路径及配对销毁进入同一tracker。
 *
 * ## Constraints and lifecycle
 * 对象式宏没有形参或自己的C返回值；适用原分配器参数及返回契约由被替换接口承担。stdlib在别名之前包含，避免改写系统原声明。强制包含只影响选定target的当前源TUs，不拦截预编译库内部调用；allocator.c先#undef四名以委托真实libc。不能与CFG/EIS6/Owner等另一套分配宏混用而不重新核预处理环境；该文件没有生产API重命名。 启用标志须在首次包含该头之前定义；guard已建立后再定义标志不会自动补装别名。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.h:17
 * - tests/parser/exec_ir_clone_test_allocator.h:4
 * - tests/parser/exec_ir_clone_test_allocator.h:8
 * - tests/parser/exec_ir_clone_test_allocator.c:1
 * - tests/parser/exec_ir_clone_test_allocator.c:4
 */

/**
 * @def free
 *
 * ## Summary
 * 在定义ZR_EXEC_IR_TEST_ALLOCATOR时把后续TU中的free标识符映射到本测试的zr_exec_ir_test_free，使克隆路径及配对销毁进入同一tracker。
 *
 * ## Constraints and lifecycle
 * 对象式宏没有形参或自己的C返回值；适用原分配器参数及返回契约由被替换接口承担。stdlib在别名之前包含，避免改写系统原声明。强制包含只影响选定target的当前源TUs，不拦截预编译库内部调用；allocator.c先#undef四名以委托真实libc。不能与CFG/EIS6/Owner等另一套分配宏混用而不重新核预处理环境；该文件没有生产API重命名。 启用标志须在首次包含该头之前定义；guard已建立后再定义标志不会自动补装别名。
 *
 * ## References
 * - tests/parser/exec_ir_clone_test_allocator.h:18
 * - tests/parser/exec_ir_clone_test_allocator.h:4
 * - tests/parser/exec_ir_clone_test_allocator.h:8
 * - tests/parser/exec_ir_clone_test_allocator.c:1
 * - tests/parser/exec_ir_clone_test_allocator.c:4
 */
