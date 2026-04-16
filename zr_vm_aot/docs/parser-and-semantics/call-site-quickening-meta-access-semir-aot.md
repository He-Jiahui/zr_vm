---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - tests/instructions/test_instructions.c
  - tests/parser/test_execbc_aot_pipeline.c
plan_sources:
  - user: 2026-04-03 实现 ZR 三层 IR 与 Ownership/AOT 字节码重构方案，并继续推进 dynamic/meta 指令族、quickening/superinstruction 生成器
  - user: 2026-04-03 接 call-site quickening 然后单独开 META_GET/META_SET 语义指令族
  - user: 2026-04-04 做 meta access 的 slot cache / PIC，然后把同样的 call-site cache 机制扩到 META_CALL / DYN_CALL 系列
tests:
  - tests/instructions/test_instructions.c
  - tests/parser/test_execbc_aot_pipeline.c
  - tests/parser/test_meta_call_pipeline.c
  - tests/parser/test_dynamic_iteration_pipeline.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_tail_call_pipeline.c
  - tests/fixtures/projects/hello_world/hello_world.zrp
doc_type: module-detail
---

# Call-Site Quickening And Meta Access `SemIR -> ExecBC -> AOT`

## 目标

这一轮把两类此前还混在一起的行为拆开了：

- `ExecBC` 上的 call-site quickening 继续只服务解释执行速度
- `SemIR` / AOT 上的 property getter/setter 语义单独收敛成 `META_GET` / `META_SET`

关键边界是：

- `SemIR` 不依赖 quickened opcode
- `ExecBC` 不负责声明 property accessor 的稳定语义名字

## ExecBC Call-Site Quickening

`compiler_quickening.c` 现在会在 `SemIR` 已经构建完成之后，把两类调用位点重写成专门的 `ExecBC` fast path：

- 零参数 superinstruction
- 带 `CALLSITE_CACHE_TABLE` 的 cached call-site

- `FUNCTION_CALL(argCount=0) -> SUPER_FUNCTION_CALL_NO_ARGS`
- `DYN_CALL(argCount=0) -> SUPER_DYN_CALL_NO_ARGS`
- `META_CALL(argCount=0) -> SUPER_META_CALL_NO_ARGS`
- `FUNCTION_TAIL_CALL(argCount=0) -> SUPER_FUNCTION_TAIL_CALL_NO_ARGS`
- `DYN_TAIL_CALL(argCount=0) -> SUPER_DYN_TAIL_CALL_NO_ARGS`
- `META_TAIL_CALL(argCount=0) -> SUPER_META_TAIL_CALL_NO_ARGS`

这些 opcode 只存在于 `ExecBC`：

- `execution_dispatch.c` 为这些 zero-arg non-tail / tail 变体提供独立 dispatch path
- tail-site 变体会继续走 `execution_try_reuse_tail_call_frame(...)`，因此不丢掉真正的 frame reuse
- `writer_intermediate.c` 会在执行字节码列表里打印这些 superinstruction
- `SemIR` 与 AOT artifact 仍保留原始语义 opcode，例如 `META_CALL`

因此零参数调用的 fast path 能减少解释器里“读取参数数量后再分支”的常规开销，同时不污染稳定语义层。

对非零参数的 dynamic/meta 调用位点，这一轮也会把 generic call 改写成 cached 变体，并把 cache metadata 显式写进 `CALLSITE_CACHE_TABLE`：

- `META_CALL(argCount>0) -> SUPER_META_CALL_CACHED`
- `DYN_CALL(argCount>0) -> SUPER_DYN_CALL_CACHED`
- `META_TAIL_CALL(argCount>0) -> SUPER_META_TAIL_CALL_CACHED`
- `DYN_TAIL_CALL(argCount>0) -> SUPER_DYN_TAIL_CALL_CACHED`

每个 call-site cache entry 现在都会显式携带：

- `kind`
- `instructionIndex`
- `memberEntryIndex`
- `deoptId`
- `argumentCount`
- runtime hit/miss 计数

这保证：

- `ExecBC` 可以直接按缓存站点做 quickening 和 re-quickening
- `.zro` / runtime load 可以不丢失调用位点的参数个数与语义 kind
- `SemIR` / AOT 仍保持 `META_CALL` / `DYN_CALL` / `META_TAIL_CALL` / `DYN_TAIL_CALL` 的稳定契约，不反向依赖 quickened opcode

同一轮 quickening 还会把 property accessor 站点登记进 `CALLSITE_CACHE_TABLE`，并把 `ExecBC` 上的 generic meta access 改写成 cached 变体：

- instance getter:
  - `META_GET -> SUPER_META_GET_CACHED`
- instance setter:
  - `META_SET -> SUPER_META_SET_CACHED`
- static getter:
  - `META_GET(static accessor) -> SUPER_META_GET_STATIC_CACHED`
- static setter:
  - `META_SET(static accessor) -> SUPER_META_SET_STATIC_CACHED`

这里的 cache 只属于 `ExecBC`：

- `.zro` / runtime function 会显式保存 `CALLSITE_CACHE_TABLE`
- `SemIR` / AOT artifact 仍只保留稳定语义名 `META_GET` / `META_SET`
- 具体 quickened opcode、cache kind、命中计数都允许继续演进，不构成稳定语义契约
- meta access cache 已经不是单态 entry，而是固定容量为 2 的 PIC

## `META_GET` / `META_SET` 语义层

这一轮把 property accessor 从 helper 序列真正提升成了独立 ExecBC opcode：

- getter:
  - `META_GET(dest=receiverSlot, receiverSlot, hiddenGetterMemberId)`
- setter:
  - `META_SET(dest=receiverResultSlot, assignedValueSlot, hiddenSetterMemberId)`

`compiler_semir.c` 仍保持原来的稳定语义 opcode：

- `META_GET`
- `META_SET`

其中：

- direct ExecBC `META_GET` / `META_SET` 直接映射到同名 `SemIR`
- 旧 helper 形态的 fallback 识别仍保留，避免历史执行字节码或中间构造在 `SemIR` 侧失配

这样做的结果是：

- `ExecBC` 不再为 property accessor 额外生成 `GET_MEMBER + FUNCTION_CALL + SET_STACK`
- `SemIR` / `.zri` / AOT artifact 继续把“普通成员取值”与“属性 accessor 语义”区分开

## ExecBC Runtime Path

执行层新增了两条专门路径：

- `execution_dispatch.c`
  - 新增 `META_GET` / `META_SET` dispatch case
- `execution_meta_access.c`
  - 负责处理 destination 与 receiver/value 别名
  - 统一走 hidden accessor member invocation
  - meta access cached hit path 现在显式以“receiver prototype + owner prototype version + resolved callable/descriptor”做 2-slot PIC guard
  - miss path 会在 receiver/result 别名时先快照 receiver，再刷新 cache，避免 `SUPER_META_GET_CACHED` 被结果覆盖后拿错 receiver
  - `Weak/Miss -> refresh -> replace one PIC slot` 的策略不会再清空整张 entry，只会轮转更新单个 slot
- `object.c`
  - 新增 `ZrCore_Object_InvokeMember(...)`
  - prototype receiver 现在会显式把自身当作 descriptor lookup 根，从而支持 static hidden accessor 的 direct 调用
- cached `META_CALL` / `DYN_CALL` / tail-call 变体与 meta access 共用 `SZrFunctionCallSiteCacheEntry` / `SZrFunctionCallSitePicSlot`

运行时边界保持不变：

- `ExecBC` 里仍使用编译器生成的 hidden accessor symbol
  - `__get_<property>`
  - `__set_<property>`
- slow path 仍复用共享 callable/runtime contract，不新增第二套 property 语义实现
- meta access 当前是 2-slot PIC：
  - 同一个 cache entry 最多命中两个 receiver prototype 形状
  - receiver prototype 改变、owner prototype member version 改变、或 descriptor/callable 失配时会 miss 并刷新对应 slot
- cached call-site 当前也复用 2-slot PIC 容量：
  - `META_CALL` / `DYN_CALL` / `META_TAIL_CALL` / `DYN_TAIL_CALL` 都按 receiver prototype 做 polymorphic inline cache
  - `argumentCount` 仍是站点稳定元数据，不进入运行时形状匹配

## AOT Contract

`backend_aot.c` 继续把 `META_GET` / `META_SET` 视为稳定 AOTIR opcode，并统一接入现有 `FUNCTION_PRECALL` runtime contract。

这表示：

- C backend 会在文本 artifact 中写出 `META_GET` / `META_SET`
- LLVM backend 也会保留同名 opcode listing
- 两个后端都继续只依赖共享 runtime callable contract，而不会私自重新发明 property accessor 语义

当前这仍是“语义稳定、执行可演进”的边界：

- 以后即使把 property accessor 执行层替换成真正的专用 ExecBC opcode 或 inline cache，AOT 侧仍可以保持 `META_GET` / `META_SET`
- cached `META_CALL` / `DYN_CALL` / tail-call 也仍然只在 `ExecBC` 存在；AOT 不消费 quickened opcode，而是消费 `SemIR` + runtime contract

## Artifact 与 Child Function 对齐

这一轮还补齐了此前不完整的 artifact/运行时对齐链：

- `writer_intermediate.c` 现在会递归输出 child function 的：
  - `TYPE_TABLE`
  - `OWNERSHIP_TABLE`
  - `EFFECT_TABLE`
  - `BLOCK_GRAPH`
  - `SEMIR`
  - `DEOPT_MAP`
  - `CALLSITE_CACHE_TABLE`
  - `EH_TABLE`
- `CALLSITE_CACHE_TABLE` 文本里会额外打印每个 PIC slot 的 receiver/owner version、descriptor index、static 标记、是否缓存 function
- `backend_aot.c` 会递归遍历整个 function tree，而不是只消费根函数的 `SemIR`
- 编译期 quickening 与 `.zro` runtime load 都会把常量池里的 function/closure 常量重绑到已经 quicken 完成的 `childFunctionList`

最后这一点很关键，因为 `CREATE_CLOSURE` 运行时取的是常量池中的函数对象：

- 如果常量池继续指向 quickening 之前的子函数副本，`SUPER_DYN_CALL_CACHED` / `SUPER_META_CALL_CACHED` 会看不到正确的 call-site cache table
- 现在编译期和 runtime load 都会把这些常量重绑到 inline child function tree，因此解释执行、`.zro` round-trip、以及 AOT 辅助 artifact 都能看到一致的 cached metadata

## 当前边界

这一轮故意保留了几个约束：

- property accessor 的 stable symbol 仍然是编译器生成的 hidden accessor 名字，不去猜测任意普通方法调用
- `SemIR` / AOT 契约没有因为 ExecBC 改成 direct opcode 而改变
- `META_GET` / `META_SET` 现在已经有固定 2-slot PIC，但还没有继续做 megamorphic fallback table
- cached `META_CALL` / `DYN_CALL` / tail-call 目前也仍然是固定容量 PIC，而不是无限扩展的 shape cache
- cache table 只服务解释执行快路径，不反向污染 `SemIR` / AOT 层

这保证稳定语义层先立住，同时允许执行层继续沿着更细粒度 cache 方向演进。

## 验证证据

本轮实际跑过的验证：

```powershell
wsl bash -lc "cmake --build build/codex-wsl-gcc-debug --target zr_vm_instructions_test zr_vm_meta_call_pipeline_test zr_vm_tail_call_pipeline_test zr_vm_execbc_aot_pipeline_test -j 8"
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_instructions_test"
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_meta_call_pipeline_test"
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_tail_call_pipeline_test"
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test"
wsl bash -lc "cmake --build build/codex-wsl-clang-debug --target zr_vm_instructions_test zr_vm_meta_call_pipeline_test zr_vm_tail_call_pipeline_test zr_vm_execbc_aot_pipeline_test -j 8"
wsl bash -lc "./build/codex-wsl-clang-debug/bin/zr_vm_instructions_test"
wsl bash -lc "./build/codex-wsl-clang-debug/bin/zr_vm_meta_call_pipeline_test"
wsl bash -lc "./build/codex-wsl-clang-debug/bin/zr_vm_tail_call_pipeline_test"
wsl bash -lc "./build/codex-wsl-clang-debug/bin/zr_vm_execbc_aot_pipeline_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build\codex-msvc18-cli-debug -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc18-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc18-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

验收点：

- `test_execbc_aot_pipeline.c`
  - 零参数 direct/dynamic/meta 的 non-tail 与 tail call site 都会 quicken 成 `SUPER_*_NO_ARGS`
  - 一参数 `META_CALL` / `DYN_CALL` / tail-call 站点会 quicken 成 cached 变体并保留 `argumentCount`
  - `SemIR` 仍保留 `META_CALL`
  - `SemIR` 仍保留 `DYN_CALL` / `DYN_TAIL_CALL` / `META_TAIL_CALL`
  - property getter/setter 会直接落成 ExecBC `META_GET` / `META_SET`
  - property accessor site 会生成 `CALLSITE_CACHE_TABLE`
  - child function 的 `.zri` / AOT artifact 会递归保留 `SEMIR`、`DEOPT_MAP`、`CALLSITE_CACHE_TABLE`
  - static property accessor 会落成 `SUPER_META_GET_STATIC_CACHED` / `SUPER_META_SET_STATIC_CACHED`
  - 同时在 `SemIR` / AOT 中继续保留 `META_GET` / `META_SET`
- `test_instructions.c`
  - `META_GET` / `META_SET` 会直接调 hidden accessor runtime path
  - static hidden accessor 通过 `ZrCore_Object_InvokeMember(...)` 时不会错误注入 receiver
  - `SUPER_META_GET_CACHED` 会在 receiver prototype 改变时重新 guard，而不是错用旧 cache
  - `SUPER_META_CALL_CACHED` / `SUPER_DYN_CALL_CACHED` 会填充并命中 2-slot call-site PIC
- `test_meta_call_pipeline.c` / `test_tail_call_pipeline.c`
  - child function 的 `DYNAMIC_RUNTIME` / `SEMIR` / `CALLSITE_CACHE_TABLE` 会递归写入 `.zri`
  - 既有 `META_CALL` / `DYN_CALL` / `DYN_TAIL_CALL` / `META_TAIL_CALL` 语义未被本轮 quickening 破坏
  - 编译期 child closure 会重绑到 quickened `childFunctionList`，不会因为常量池指向旧副本而丢掉 cache table
- `test_dynamic_iteration_pipeline.c` / `test_semir_pipeline.c`
  - 既有 dynamic iteration 与 `SemIR -> ExecBC -> AOT` 边界继续成立
- Windows MSVC smoke
  - `using-vsdevcmd` 已成功导入 `cl` / `cmake` 环境
  - 本机 `Visual Studio 17 2022` 生成器因缺少 `v143` toolset 不能作为 smoke 目标
  - `Visual Studio 18 2026` 生成器下的 `zr_vm_cli_executable` 构建成功
  - `hello_world.zrp` 运行输出 `hello world`
