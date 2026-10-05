---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/hotpatch_capability.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
tests:
  - tests/library/test_ssa_capability_validation.c
  - tests/library/test_ssa_canonical_zraf_validation.inc
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-hotpatch-requirement-limit.md
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
doc_type: core-runtime-contract
status: implemented
---

# 热补丁能力清单准入契约

## 宿主策略与能力闭包

`capability_manifest.h` 定义进程内、含借用指针的宿主策略输入，不是可直接落盘的 ZRAF 编码。宿主提供清单、当前模块/公开契约、ABI/profile、许可位和验签回调。验证器将清单总需求与逐 token 需求取并集；每个需求 token/位集合非零、reserved 为零，并拒绝集合外能力，不能将越权位悄悄裁剪。

主入口和独立闭包共享 4096 项上限；超限在回调和逐项读取前拒绝，diagnostic.expected/actual 记录上限和项数。独立闭包只核声明形状、已知 flags 与授权集合，不调用主验证器完成签名、身份或产物结构校验。这里的声明需求并非从完整 IR 推导出来的权限证明。

## 两个验证入口

旧 `ZrCore_HotPatch_Validate` 捕获 `artifact.buffer` 的地址、长度和计算哈希，核对清单内容预期、基模块/公开契约、ABI/profile 和禁止 flags，再调用宿主同步验签，随后读取需求。输入及存储在验证期间须有效稳定。它不执行完整 ZRAF 的 Read/Open/Verify；旧 expectedPatchId/expectedContentHash 的零值表示不附加该项比较，清单内容哈希仍必须匹配。

`ZrCore_HotPatch_ValidateZraf` 校验完整有界外层跨度，要求非零宿主 expectedContentHash 和入口 token/signatureHash。清单内容预期也覆盖完整外层，不能用内层哈希代替。它在回调前复制策略和公开身份并归并需求，回调认证完整跨度后复核哈希，再读取外层、核身份、打开支持的 canonical 图、VerifyModule 并要求 token/签名组合恰有一个匹配。临时图释放，只有所有检查成功才输出标量和借用跨度；不重定位、安装、执行或发布补丁。canonical opener 当前拒绝 typed binding rows，不能据“canonical”声称全部图 schema 可准入。

## 结果、存储有效期与后续消费

旧令牌保存验证时捕获的 patchId/契约/profile 等标量及原字节跨度。Apply 先重算这个跨度的哈希再判重、Prepare/Publish，不从后改的视图/清单重取发布身份；Prepare 复制身份元数据至代际记录，不复制或安装可执行字节，也不再次验签。`immutableContent` 是准入标记，不代表存储被固定或冻结。

ZRAF 令牌按值保存公开身份及入口策略，外层字节仍由调用者持有。`RecheckZrafContent` 只核跨度/两个成功标记并重算哈希，不重新验签、解码图或授权宿主策略。调用者须保留存储至最后读取并同步写入者；回调后的哈希比较只能发现最终可见变化，不能发现已恢复的改写或提供并发原子性。

## 状态、诊断与有限策略身份

部署决策按 `EZrHotPatchCapabilityStatus`，不按 `StatusName`：部分状态及未知值共用静态兜底字符串，返回字符串无需释放。BASE_MISMATCH 还包含内容预期和可选补丁 ID 失配；ARTIFACT_INVALID 包含旧空 buffer、ZRAF reader/opener/图/selector 失败；LIMIT 包含需求项数和完整跨度上限；CONTENT_CHANGED 用于 ZRAF 回调后或 Recheck 的可见哈希改变。

Diagnostic.token 可零或来自需求、请求入口、产物 expected/actual token；sourceOffset 随检查点表示需求来源、artifact byteOffset 或 ExecIR instructionId，不能统一视为源文件偏移。成功或无定位时可为零。`ComputePolicyHash` 仅混合版本/flags、目标 ABI/profile 与宿主/base 字段，不包含 patchId、contentHash 或逐项需求，不是签名或完整清单身份。

## 待核入口和证据范围

TODO：仓内正向调用使用测试验签桩；真实宿主验签算法、信任根与清单绑定从公开验签回调接入核查。宿主仍须保证清单/身份可信来源与借用存储活期，不能从通过测试桩推导密码学认证通过。

`ssa_capability_validation` 与 `ssa_canonical_zraf_validation` 是当前 CMake 注册的正向契约夹具；后者通过 artifact-v6 测试的 `--validate-zraf` dispatch 和 include 执行入口。它们覆盖计数边界、字段快照、验签形状与可见内容变化等场景。本次注释整合只做源码/注册静态阅读，没有新的 configure/build/native/CTest 或密码学验证结果；历史接受记录不会升级为当前候选原始字节的运行信用。
