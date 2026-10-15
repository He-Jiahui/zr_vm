---
related_code:
  - zr_vm_core/include/zr_vm_core/contiguous_view.h
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_array_lowering.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/contiguous_view.h
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
  - user: 2026-10-15 Core contiguous-view contract review assignment
tests:
  - tests/core/test_ssa_arrays_slices.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-arrays-slices-view-boundaries.md
doc_type: workflow-detail
---

# Core contiguous view 契约审查

## 范围与清点

本批复核 Core contiguous-view 的既有逐单元台账
[coverage/zr_vm_core_contiguous_view.tsv](../coverage/zr_vm_core_contiguous_view.tsv)，保留原有
25 个精确 key 与原结论，新增 key 数为 0。范围包括公开头文件中的 3 个结构体、1 个枚举和
3 个 flags 宏（7 项）、5 个公开 API、C 实现中的 5 个公开函数与 3 个内部 helper，以及
5 个独立语句块。当前结论为 20 项 commented、5 项 TODO、0 项 BUG；这些计数按逐单元台账行
计，不代表 5 个 TODO 都是运行期缺陷。

头文件和 C 文件的详细中文 Doxygen 说明追加在原始程序文本 EOF，保留原声明、定义和调用的
物理行号。台账证据仍指向当前工作树的实际调用或语句，不把 descriptor 测试替身冒充真实
storage 访问。最终冻结源码和台账的 raw SHA-256 如下；本批文档自身由提交 blob 与暂存收据固定。

| 路径 | SHA-256 |
| --- | --- |
| `zr_vm_core/include/zr_vm_core/contiguous_view.h` | `27c37a22841c1bf4130cdf992709311bb8f1298f7264137f7d564d1c7d29b182` |
| `zr_vm_core/src/zr_vm_core/object/contiguous_view.c` | `35f7e884a2e5c5d254348a56e225ac8d0cc53d6eb7fa0a2bf513f59f1cc66a6b` |
| `docs/code-review/coverage/zr_vm_core_contiguous_view.tsv` | `16d2ebc00eac82ae2990023a45370262474a897be5c4364b1ec6245538712ff0` |


## 调用链与构建入口

ZrCore_View_Create 在 zr_vm_core/src/zr_vm_core/object/contiguous_view.c:63-69 把 request 字段复制到局部 candidate，经
ZrCore_View_Validate（zr_vm_core/src/zr_vm_core/object/contiguous_view.c:68）通过后一次性写入 view；失败时只有在 request、view、
diagnostic 使用独立存储的前提下保持旧 view。Slice 在 zr_vm_core/src/zr_vm_core/object/contiguous_view.c:92-96 复制候选、调整窗口、再次
Validate 后发布；精确的 slice == view 完整对象别名由候选拷贝支持，其他部分重叠组合或
diagnostic 与 descriptor 重叠不在契约内。IndexOffset 的仓内产品入口是
zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c:6，该 facade 只转发
Core 计算的相对 byte offset，不取元素地址。

tests/cmake/ssa-tests.cmake:1108 建立 target，tests/cmake/ssa-tests.cmake:1110-1112 直接编译测试、Core
contiguous-view C 和 Parser facade，tests/cmake/ssa-tests.cmake:1113-1116 配置 include 根，tests/cmake/ssa-tests.cmake:1118-1119 注册 CTest
ssa_arrays_slices。测试中的 ownerRoot=(TZrPtr)(uintptr_t)1u 是不被 Core 解引用的非空
标记值；这些断言覆盖 metadata-only 的 Create/Slice/Validate 以及经 facade 的
IndexOffset，不能证明真实 owner、backing、GC root、pin、读写权限或元素访问。断言位于
assert 内，NDEBUG 构建会移除这些 API 调用，因此验证需使用启用断言的配置。

可在 Root 复用的 ClangCL 与 MSVC SDK 的 Debug/static 构建目录先构建 zr_vm_ssa_arrays_slices_test，再以
ctest --test-dir <同一构建目录> -C Debug --output-on-failure -R ^ssa_arrays_slices$ 运行
本用例。本批不自行创建或操作构建目录；Root 负责记录实际目录、命令与退出状态。

## 契约边界与已知待确认项

- length == 0 仍要求非零 stride 和 elementSize，但允许空 ownerRoot，并跳过非空窗口的
  末元素范围算术。
- stride 只要求非零，可以小于 elementSize，因此 Core 不禁止逻辑元素区间重叠；算术检查
  只证明 TZrSize 可表示，不证明 backing extent、连续布局或地址有效。
- currentGeneration == 0 明确跳过 generation 比较；IndexOffset 当前固定以 0 调用
  Validate。接入真实可变 storage 前，generation 来源、owner 保活、GC root、layout hash、
  extent、pin 和只读能力仍由 storage adapter 收敛。
- ZR_VIEW_FLAG_PINNED 只要求 lifetimeRegion != 0，不获取、维持或验证实际 pin；
  READ_ONLY 和 INLINE_STORAGE 同样只记录调用方意图。枚举中的 GENERATION、LIFETIME、未知
  flags 和 NOT_CONTIGUOUS 没有本批测试信用。
- diagnostic 是同步清零/写入的值型输出，须与输入 descriptor 及其他输出使用独立存储；
  只有 Slice 明确支持 slice == view 的完整对象别名。任何未知输出重叠都不承诺安全。
- test_basic_view_boundaries 初始请求在 tests/core/test_ssa_arrays_slices.c:100 设置 byteOffset=8；随后 tests/core/test_ssa_arrays_slices.c:125 设置
  stride=SIZE_MAX、length=2，并在 tests/core/test_ssa_arrays_slices.c:127 调用 Create。实现先在 zr_vm_core/src/zr_vm_core/object/contiguous_view.c:42 计算 span，再于
  zr_vm_core/src/zr_vm_core/object/contiguous_view.c:43 检查 byteOffset + span；该请求在此处已溢出，zr_vm_core/src/zr_vm_core/object/contiguous_view.c:44 的 elementSize 加法不会执行。
  因而此用例不证明该末元素尺寸加法的溢出分支。

## 证据与验证边界

本批源码只改契约注释，不改公开签名和程序 token。C 原始 1–121 行全部不变；头文件仅原位纠正第 64、75 行注释的输出别名边界，其余说明追加在 EOF。Clang stdin raw lexer 对最终 H/C 的 1,148 个非注释 token、StartOfLine/LeadingSpace 标志及原 token 行号逐项核对通过，所有输出已消费。证据 core-contiguous-view-comment-equivalence-r3.json 的 SHA-256 为 64671f349f3e9a16e50d4570b472669bb242e8469b09925c46dd379bdd881fbd。r1/r2 候选及其验证只作为修改历史保留，最终信用来自 r3。最后的 r3 只纠正内部 helper 的两行 EOF 调用者引用，未改变任何原始程序行。Parser facade 和测试的注释在独立 11 单元批次提交，不计入本批。

同一实际 checkout 和 E:/cargo-targets/zr_vm/build/first-party-review-01a13463/clang-cl-products-debug 构建目录的修改前基线 build exit 0（8.765 秒）、CTest ssa_arrays_slices 1/1（exit 0，2.469 秒）；其 results SHA-256 为 d7de1f969bf9530ae62e88f0c8e086588e757f89ed6c971ab9724b9768bcbb75。最终 r3 使用相同 Debug/static 配置实际重编译 Core C 并重新链接目标；Parser C 和测试的最后改动此前已在 r2 实际重编译，本轮文件保持不变。r3 build exit 0（15.547 秒），CTest 同名测试 1/1（exit 0，3.047 秒）。结果 array-contiguous-view-final-native-r3-results.json 的 SHA-256 为 c0f9de12b7d52bf31508c901c969a97139153b6221e23a62a46e4b49f048ec79；comparison 的 SHA-256 为 7d17d928b8afdfd1eb41c55f7ccd08003b671d0f1b17e6202b604e4909ca4242。未定义 NDEBUG；这是一个包含七个 assert 函数调用的 CTest 进程，未独立报告七项测试。

修改前后 3,216 个保守输入中，仅本两批四个注释文件原始哈希变化，其余 3,212 项相同；修改后运行期间 3,216 项和 10 冻结文件均稳定，子进程全部等待退出。array-contiguous-view-caller-unit-audit-r1.json（SHA-256 fbf8b9b17be0f0da190f6b787151ba167df2077f76fd9a7db9b321900876e702）记录 36 个完整单元边界、定义和全部可检索调用；其中待最终核对的 Core leaf 以本批冻结表和最终 check-batch/暂存审查关闭。该检索未找到 Init 的仓内直接调用，也未找到生产 storage 访问链；公开 ABI 的仓外消费者仍未核实，不据此判为死代码。

本轮执行只提供描述符元数据、算术及 facade 诊断信用；generation 不匹配、PINNED/未知 flags 拒绝、输出别名、真实 backing、GC/pin、生产 SSA/AOT 发射和元素访问均未执行。保留原有 5 项 TODO 的决定与核查入口，包括非连续诊断、storage adapter 的 root/布局/pin 责任和 IndexOffset 的真实 generation 来源；没有新增 BUG/TODO 标签。

产物复用当前任务在 E:/cargo-targets/zr_vm 的构建、缓存和必要证据，本批释放 0 字节。先前清理被自动审批以 blocked by policy 拒绝，保留项未重试清理，任务产物清理保持未关闭。