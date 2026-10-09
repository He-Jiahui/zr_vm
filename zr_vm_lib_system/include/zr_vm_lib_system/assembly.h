/** @file
 * ## Summary
 * 声明 `zr.system.assembly` 的三个同步 native 回调。它们面向当前项目输出的 `.zrm`，
 * 让脚本按容器逻辑名查询存在性、读取文本或读取原始字节；不接收宿主磁盘路径。
 *
 * ## Calling model
 * registry 先发布函数指针与单一 string 参数提示，native binding 后续在自己的 GC 域内
 * 同步派发。context、参数视图和 result 槽只在这次调用中借用，不能缓存到下一次调用。
 * 应通过模块导出调用这些接口，让 binding 完成参数和返回值的栈、根与异常管理。
 *
 * ## Ownership and constraints
 * state/global 必须属于已初始化且仍有效的项目 VM。资源 entry 借用 archive；普通返回前
 * 关闭 archive 并释放临时 bytes。交付后的 VM string/array 不借用 ZRM 缓冲区，由 VM
 * 管理；宿主若保存其裸对象指针，仍须遵守调用层的根与生命周期规则。
 * 缺参或类型错误经 VM 异常处理，不等同于 callback 的 `ZR_FALSE`。
 *
 * ## References
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:18`、`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:21`、`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:24` 登记三个导出。
 * - `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:274` 和 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277` 进入 GC native 域并实际调用 callback。
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:56` 从当前 global 查项目；`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:58` 解析项目输出归档。
 */

#ifndef ZR_VM_LIB_SYSTEM_ASSEMBLY_H
#define ZR_VM_LIB_SYSTEM_ASSEMBLY_H

#include "zr_vm_lib_system/conf.h"

/**
 * ## Summary
 * 查询当前项目输出程序集是否包含指定逻辑资源，供脚本在读取前选择缺失资源分支。
 * 结果槽中的 bool 表示存在性；callback 返回值表示本次 native 调用是否正常交付结果。
 *
 * ## Parameters
 * @param[in] context 本次 native 派发上下文，state/global 与参数视图必须有效。位置 0 为
 *                    string 逻辑名，例如 `config/default.txt`，随后还要通过 ZRM 名称校验。
 * @param[out] result binding 提供的可写结果槽；成功时写入 bool，不携带 archive 所有权。
 *
 * ## Returns
 * @return 正常交付 bool 时为 `ZR_TRUE`。归档不可用或条目缺失都写 `false`，找到条目写
 *         `true`，因此 `false` 结果不能区分缺失资源与打不开当前程序集。
 *         空 context/result 或名称校验失败时为 `ZR_FALSE`；缺参、非 string 经 VM 参数
 *         异常处理，不能当作通常返回 `ZR_FALSE` 的情况。
 *
 * ## Constraints and lifecycle
 * context/result 仅借用一个同步调用期。entry 不向调用方交付，归档在 bool 交付前关闭；
 * 返回的 bool 不保活项目或 archive。该查询只说明调用时的状态，不为后续读取保留归档。
 *
 * ## References
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:18` 注册函数指针；`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277` 实际派发。
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:147` 与 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:148` 将归档/条目不可用解释为正常 false 结果。
 * - `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1207` 和 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1211` 处理缺参与类型错误。
 * - `tests/system/test_system_assembly_module.c:237`、`tests/system/test_system_assembly_module.c:247` 是存在与缺失的实际导出调用。
 */
TZrBool ZrSystem_Assembly_ResourceExists(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 从当前项目程序集读取一个逻辑资源，交付 VM 字符串，供脚本消费文本资源。
 * 普通成功返回后，结果不借用压缩包或临时 C 缓冲区；需保留全部字节时使用 Bytes 接口。
 *
 * ## Parameters
 * @param[in] context 当前 native 派发上下文；位置 0 必须为有效 string 逻辑名，state/global
 *                    须属于仍有效的项目 VM。输入只借用本次调用期。
 * @param[out] result binding 的可写结果槽，通常接收 VM 管理的 string；回调不转移临时
 *                   bytes/archive 的所有权，也不授予裸 VM 指针跨调用的保活保证。
 *
 * ## Returns
 * @return 普通路径读取字节并从 `SetString` 正常返回后为 `ZR_TRUE`。空 context/result、
 *         名称校验失败或临时文本 malloc 失败为 `ZR_FALSE`。缺参/类型错误走 VM 参数异常。
 *         归档/资源查找返回失败或 `ReadEntry` 报告失败时调用不返回的 `RunError`；
 *         不能描述成抛错后返回 `ZR_FALSE`，也不保证所有解压失败都会被报告。当前不校验文本编码。
 *
 * ## Constraints and lifecycle
 * 普通返回前释放 ZRM bytes、临时文本并关闭 archive；result 的 VM string 由 VM 管理。
 * 资源名只定位当前项目的输出归档，不选择另一磁盘文件。文本 setter 使用 NUL 终止语义。
 * BUG: 合法资源可含 `41 00 42`，但本路径经 C 字符串长度只交付 `A`，丢失 NUL 后内容。
 *      `zr_vm_library/src/zr_vm_library/zrm.c:746` 按原始文件读取资源，`zr_vm_library/src/zr_vm_library/zrm.c:845` 按显式长度入包，
 *      `zr_vm_library/src/zr_vm_library/zrm.c:1328` 返回解压字节数；`zr_vm_core/src/zr_vm_core/string.c:973` 按 NUL 文本长度建 VM 字符串。
 * BUG: 已找到条目仍可能读取失败；zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1640 先清输出长度，
 *      zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1655 和 zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1661 在分配或解压失败时返回 NULL。
 *      zr_vm_library/src/zr_vm_library/zrm.c:1322 仅拒绝 NULL 且长度非零，故 NULL/0 被误报成功，
 *      本回调可把失败误交付为空结果；此处是静态失败链证据，未执行故障注入。
 * BUG: 合法资源已读且临时文本 malloc 成功时，若未驻留的短文本发生 API cache miss，
 *      先前 VM 对象创建成功，而驻留表达到扩容阈值后 HASH_BUCKET 申请失败并保留旧桶，
 *      zr_vm_core/src/zr_vm_core/hash_set.c:181 与 zr_vm_core/include/zr_vm_core/hash_set.h:537 正常返回失败，
 *      zr_vm_core/src/zr_vm_core/string.c:808、zr_vm_core/src/zr_vm_core/string.c:974 传递 NULL；
 *      zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1313 不写结果，而 zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:191 仍报告成功。
 *      短串上限由 zr_vm_common/include/zr_vm_common/zr_string_conf.h:16 定为 127 字节；这是完整静态失败链，未执行故障注入。
 * TODO: 沿 zr_vm_core/src/zr_vm_core/memory.c:46 的异常入口与 zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:195 的普通收尾核查非局部退出时 text/bytes/archive 的清理责任。
 *
 * ## References
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:21` 登记文本导出；`tests/system/test_system_assembly_module.c:257` 是正常文本资源的实际调用。
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:174` 和 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:179` 进入资源错误报告；
 *   `zr_vm_core/src/zr_vm_core/debug.c:1080` 声明 `RunError` 不返回。
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:190` 交付文本，`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:195` 和 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:196` 清理普通返回路径。
 */
TZrBool ZrSystem_Assembly_ReadResourceText(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把当前项目的一个逻辑资源交付为 VM 整数数组，供二进制消费者逐字节处理内容。
 * 元素保持原始顺序和零字节，避免文本接口的 NUL 终止限制。
 *
 * ## Parameters
 * @param[in] context 本次同步 native 派发上下文；位置 0 为通过 string 读取及 ZRM 校验的
 *                    逻辑名。state/global 和参数对象必须有效，且只借用此次调用期。
 * @param[out] result 可写 binding 结果槽；成功接收 VM array，每个元素是 `0..255` 的整数。
 *                   数组由 VM 管理；元素是整数值，结果不借用随后释放的 ZRM bytes。
 *
 * ## Returns
 * @return 普通读取并构造完整数组时为 `ZR_TRUE`。空 context/result、名称校验、数组创建
 *         或 PushValue 正常报告失败时为 `ZR_FALSE`；结果只应在成功时消费。
 *         缺参/类型错误通过 VM 异常报告；归档/资源查找或 `ReadEntry` 报告失败时进入
 *         不返回的 `RunError`。helper 误报成功的解压失败不经过这条错误分支。
 *
 * ## Constraints and lifecycle
 * 普通返回前释放 bytes 并关闭 archive；部分构造的 VM 数组仍归 VM 管理，不交给调用方
 * 释放。结果不保活项目或 archive，保存裸 VM 对象仍须由上层维持根。
 * BUG: 已找到条目仍可能读取失败；zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1640 先清输出长度，
 *      zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1655 和 zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1661 在分配或解压失败时返回 NULL。
 *      zr_vm_library/src/zr_vm_library/zrm.c:1322 仅拒绝 NULL 且长度非零，故 NULL/0 被误报成功，
 *      本回调可把失败误交付为空结果；此处是静态失败链证据，未执行故障注入。
 * TODO: 数组构造可能进入 VM 分配；沿 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:107`、`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:115` 的下层失败协议与
 *       `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:225` 的清理位置核查非局部异常是否跳过 bytes/archive 收尾。
 *
 * ## References
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:24` 注册字节导出；`tests/system/test_system_assembly_module.c:267` 调用，`tests/system/test_system_assembly_module.c:273` 核对内容。
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:114` 保留每个 byte 的整数值；`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:120` 交付 VM 数组。
 * - `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:224` 构造结果，`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:225` 和 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:226` 清理普通返回路径。
 * - `zr_vm_core/src/zr_vm_core/debug.c:1080` 是资源错误使用的不返回入口。
 */
TZrBool ZrSystem_Assembly_ReadResourceBytes(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_ASSEMBLY_H
