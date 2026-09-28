//
// CLI-local configuration constants.
//

#ifndef ZR_VM_CLI_CONF_H
#define ZR_VM_CLI_CONF_H

#include "zr_vm_common.h"

#define ZR_CLI_API ZR_API

#define ZR_CLI_ERROR_BUFFER_LENGTH 512U
#define ZR_CLI_SOURCE_HASH_HEX_LENGTH ZR_STABLE_HASH_HEX_BUFFER_LENGTH
/* BUG: 物理行超过 1023 字节时，fgets 分段而 REPL 每段都追加换行，改写单行表达式；
 * 恰满 1023 字节时，后续行结束符会被当成空行触发提交；现有 E2E 只覆盖短行。 */
#define ZR_CLI_REPL_LINE_BUFFER_LENGTH 1024U
#define ZR_CLI_REPL_BUFFER_INITIAL_CAPACITY 256U
#define ZR_CLI_COLLECTION_INITIAL_CAPACITY 8U
#define ZR_CLI_SMALL_COLLECTION_INITIAL_CAPACITY 4U
/* TODO: project/compiler/REPL 的扩容路径直接乘此系数；核查极大输入下的容量溢出，
 * 并在各调用点补对应上界测试。 */
#define ZR_CLI_COLLECTION_GROWTH_FACTOR 2U

#endif // ZR_VM_CLI_CONF_H
