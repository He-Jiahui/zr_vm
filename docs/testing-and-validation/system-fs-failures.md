---
related_code:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - tests/system/test_system_fs_failures.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h
  - tests/system/test_system_fs_failures.c
  - tests/cmake/system-fs-failure-tests.cmake
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/system/test_system_fs_failures.c
  - tests/cmake/system-fs-failure-tests.cmake
doc_type: testing-guide
---

# FS 文本及包装失败回归

本组直接链接实际 Core、Library、System 实现，不复制或替换 FS 源码。模块文本调用经 `ZrLib_CallModuleExport`；流的 open/readText/close 经 `ZrLib_CallValue` 与实际注册方法。File 夹具用公开 `ZrLib_Type_NewInstance` 创建注册实例并写 fullPath，不授予脚本构造器 lowering 覆盖。

`module_nul` 与 `stream_nul` 读取实际三字节 `A,0,B` 文件，按 VM String 的存储字节长度及 memcmp 验证结果；流还检查公开 position 快照消费完整输入。普通 alpha、空文本分别有模块和流控制；open_control 检查正常打开、读取与关闭。

Windows `open_oom` 在故障前对唯一 payload.bin 独占打开并记录卷序列号／文件索引。真实 Core allocator 只在该文件独占探测返回 ERROR_SHARING_VIOLATION 时，持续拒绝新 OBJECT、sizeof(SZrObject) 请求，确保注入已发生于实际打开之后的 wrapper 分配。返回后再次独占打开同一路径并核对文件身份，验证临时句柄已关闭；随后撤障重试和显式关闭。该检查不使用全进程句柄数或 Core 存活块数冒充宿主句柄证据。非 Windows 不注册该用例。

每个独立进程由 caller 传入独立 child 根；根已存在即拒绝，防止覆盖先前证据。默认父目录为构建树内的 `tests/system-fs-failures`；本轮通过 CMake cache 参数指向已登记的 `E:/cargo-targets/zr_vm/tests/review-bugfix-20261016/system-fs-failures`。测试不自行删除，root 在进程退出后按文件哈希清理。进程即使失败也会由操作系统最终关闭句柄，不能据此把返回时的泄漏断言当作通过。

目标 `zr_vm_system_fs_failures_test`；CTest 前缀 `system_fs_failure_`；Windows 八项、其它平台七项，每项 30 秒。独立 CMake 片段检查所有七个实际链接目标，裁剪配置缺任一目标即跳过。

root 实际 caller-exception-fs-red 的 configure/build 均为 0、1935 输入未变化；两个 NUL 用例长度断言失败，open_oom 指定文件独占打开断言失败，其余五项控制通过。生产修复现静态冻结，等待统一 GREEN。不覆盖原生 malloc 文本缓冲、目录列表、writeBytes 物化暂存区的异常清理；这些必须另建真实 CRT 观察边界后取证。也不覆盖并发文件访问、物理路径别名或 GC 并发恢复。


## 最小生产事务

ReadTextFromHandle 使用实际已读 size 调用 Core String_Create，再以 SetStringObject 发布，保留首 NUL 后字节。创建和发布置于局部 TryRun，调用方 context 持有 text/size/result，返回后释放原生 text 再传播捕获状态。outReadCount 仅在成功发布后设置；不回滚已消费位置。该清理是发布路径的必要生命周期维护，不以 NUL 回归授予独立 CRT malloc OOM 证据；一次性 readText 外层 handle 的异常关闭仍未修。

私有 NewStreamObject 新增 outHandleTransferred/outStatus，公开 API 不变。局部 TryRun 的调用方 context 持有临时根、data、转移标志及结果；每次可能分配后从临时根重取对象。hidden owner 字段正常发布后立即标记转移，中间没有分配。未提交 data 返回前 free；所有成功 Begin 均配对 End。File.open 只对未转移 handle 做 CloseHandle，再传播原异常；提交后的 data/handle 属于 wrapper finalizer，不因后续失败 double-close。普通 NULL 与捕获异常分开表达，不把普通失败伪装成 MemoryError。初始 StreamSyncFields 的普通 false 忽略、路径借用的 GC 压力、关闭失败及提交后故障的独立回归仍不在本组信用内。

改动仅扩展既有 stream 构造／发布的事务边界，没有新增独立子系统。fs_stream.c 含大量逐函数历史注释、约 1035 行，保留单一流职责；此处不为窄清理辅助拆出跨文件私有状态。fs_entry 仅编排私有结果和关闭，未把业务复制进调用方。其它 foreign 注释和行为保持；过时注释仅收紧本次已修改函数的直接契约。

## Root acceptance

Actual ClangCL configure/build0, 168/168 CTest, pool14, exceptions15 and CLI hello0; 1944 input hashes unchanged; independent review ACCEPT. Windows only. Evidence: `reports/review-bugfix-20261016/integrated-support-batch-6-green-{0..5}.log` and receipt under the configured external artifact root.
