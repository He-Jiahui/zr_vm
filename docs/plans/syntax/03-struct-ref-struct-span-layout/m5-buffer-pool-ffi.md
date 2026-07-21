# 03-M5 Buffer/pool/FFI integration 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md`
的 `M5 Buffer/pool/FFI integration`。

## 状态与产出记录

- 完成时间：2026-07-21 13:30 +08:00
- 状态：已完成
- 完成项目：
  - 已新增 `zr.pooling` native module、`BufferPool` 与 `PoolLease<T>`：rent 按精确长度
    创建或复用 GC-tracked backing array，generation 单调递增，return/reuse counter 精确，
    close 清空元素并 single-return；重复 close 不产生 double-return。
  - 已让真实 `PoolLease<T>` descriptor 发布 owner contiguous-source protocol、length/create
    role、index/close meta；compiler 直接携带 resolved receiver Place，view create/load 产生
    fresh ValueId，owner loan 活跃时阻止 close/reuse，NLL 后允许返回池。
  - 已补异常作用域 cleanup：handler 保存 to-be-closed boundary，异常进入 catch/finally 或
    继续上抛前按 LIFO 关闭本作用域 registration；close error 在预留 scratch slot 中构造，
    stack relocation 后重新加载 resource，不覆盖相邻 local/callable。
  - 已完成 `BufferHandle.pin()` / `Ptr<u8>.span()` 显式 native pin 合同：pointer 保留 owner、
    native address 与 byte length；owner close 在 pin 存活时延迟释放；pointer explicit close/
    finalizer 只 unpin 一次，closed owner 拒绝新 pin，byte index/write 做精确 bounds/value 检查。
  - 已验证 moving/full compact GC 期间 pinned Span 地址稳定；active native view 阻止
    pointer close/unpin，最后一次 view use 后允许幂等 close，避免 stale pointer。
  - 已修复 `SUPER_DYN_CALL_NO_ARGS` 在 TypeLayout frame 下按 logical slot 建 call window 导致
    覆盖 layout-backed payload 的问题；该路径现复用 frame-layout-aware generic pre-call，
    物理 call window 位于完整 frame storage 之后。
  - 已扩展 artifact ContractRow 的 callable escape flags 与 ABI lowering kind，并发布
    `ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi`；VM/AOT 对 TypeRef token/hash、ref-like
    flags、layout version/hash、callable escape 与 lowering kind 使用同一 gate。ZR value-frame
    与显式 native marshalling 可通过，ref-like native-direct 必须失败。
  - 已将 FFI pinned pointer view 从 1042 行的 `runtime.c` 拆分为独立
    `ffi_runtime_pointer_view.c`，原文件降至 903 行；同步补齐 library/core/artifact 模块文档。

## 当前验证结果

- 固定验收快照：基线 `106f667b5737d0e86aeb9d29b5ed30c922efffcd` 加 M5 的 32 个
  code/test/module-doc exact paths；逐文件 SHA-256 mismatch=0，并复制 7 个锁定 submodule
  revision。三份既有 Syntax 草案、LSP paths 和 build/generated 目录均未进入快照。
- GCC 11.4、Clang 14.0、MSVC 19.44 各 10/10 target 真实进程 exit 0；每套共
  330 Tests、0 Failures。GCC/Clang 0 Ignored；MSVC 2 Ignored，均为既有 Unix-only strict
  AOT shared-library 执行边界。
- `zr_vm_buffer_pool_ffi_test`：8 Tests、0 Failures、0 Ignored，覆盖 descriptor、single-return、
  live owner/native view conflict、32 轮 pool/full-GC stress、throw/catch cleanup、pinned owner
  close/full compact GC、幂等 unpin 与 VM/AOT public ref-like ABI 正负门禁。
- 父级矩阵每套：Span core 15/15、pre-SemIR 9/9、type inference 119/119、ref struct
  restrictions 10/10、canonical consumers 15/15、artifact schema 13/13、AOT SemIR contracts
  8/8、strict AOT shared-library smoke 6/6、compiler integration 127/127。
- GCC/Clang/MSVC 的 `zr_vm_cli_executable` 均构建通过，`zr_vm_cli --version` 均真实 exit 0：
  分别报告 GNU 11.4、Clang 14.0 与 MSVC 19.44 Debug runtime。
- 最终证据：
  - `.codex/logs/s03m5-final-{gcc,clang,msvc}-build.log`
  - `.codex/logs/s03m5-final-{gcc,clang,msvc}-zr_vm_buffer_pool_ffi_test.log`
  - `.codex/logs/s03m5-final-{gcc,clang,msvc}-zr_vm_span_core_test.log`
  - `.codex/logs/s03m5-final-{gcc,clang,msvc}-zr_vm_aot_c_value_type_shared_library_smoke_test.log`
  - `.codex/logs/s03m5-final-{gcc,clang,msvc}-zr_vm_compiler_integration_test.log`
  - `.codex/logs/s03m5-final-{gcc,clang,msvc}-cli-build.log`

## 当前实现边界

- PoolLease 与 pinned pointer 只通过 protocol/member role、canonical TypeId、Place/loan 与
  TypeLayout 驱动；compiler/runtime 不按 `PoolLease`、`Ptr`、`span` 名字重建语义。
- 当前 pool 复用 exact-length backing array，未引入 size class、并发池或 generational slab；
  这些属于后续独立计划，不改变本里程碑的 single-return/borrow gate。
- pinned safe view 当前冻结为 `Ptr<u8>` byte surface；任意 typed native slice 需要元素大小、
  alignment、length provenance 和 marshaller 的完整结构化合同，不能把 byteLength 当元素数。
- schema v1 沿用 ContractRow 原 reserved slot 编码 ABI lowering kind，旧 artifact 解码为 NONE；
  新 public ref-like ABI gate 要求显式非 NONE lowering，并拒绝 native direct。
- M5 完成 Syntax03 计划的最后一个里程碑；后续 Syntax 计划必须继续按各自里程碑和独立
  completion record/commit 执行。
