---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_state_map_clone_fault_cases.h
  - tests/parser/ssa_state_map_clone_fault_allocator.h
  - tests/parser/ssa_state_map_clone_fault_allocator.c
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/cmake/ssa-tests.cmake
  - tests/CMakeLists.txt
  - docs/core-runtime/execution-state-maps.md
  - tests/acceptance/2026-10-02-state-map-clone-oom.md
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/parser/test_ssa_state_map_clone_fault_cases.h
  - tests/parser/ssa_state_map_clone_fault_allocator.h
  - tests/parser/test_ssa_state_maps.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_state_map_clone_fault_cases.h
  - tests/parser/ssa_state_map_clone_fault_allocator.h
  - tests/parser/ssa_state_map_clone_fault_allocator.c
  - tests/cmake/ssa-tests.cmake
  - tests/CMakeLists.txt
doc_type: code-review-batch
---

# Core state-map clone 生命周期与失败契约

## 范围

本批审查并只为 29 个精确单元追加 EOF Doxygen：Core storage C 的九个类型/函数、private storage header 的一个声明、public state-map header 的三个声明、fault case header 的一个快照类型/三个 helper/四个测试、allocator header 的四个接口，以及主 test C 中四个直接 Clone 用例。完整 key 和定义、调用、Unity 注册证据见 docs/code-review/coverage/zr_vm_core_state_map_clone.tsv。

主测试文件仍有其他已登记 Builder 单元和未审单元；本批没有把它标成全文件审完。test_state_map_function_clone_keeps_side_table_independent 是 CloneFunction 集成用例，不在这 29 个 key 中。storage C 其余五个未纳入单元（StateMapStorageValid 与四个 materialization 函数）及 private header 中其他声明也未取得本批审查信用。

六个源码目标的修改前 raw SHA-256 与 lease 中记录逐项一致。allocator implementation tests/parser/ssa_state_map_clone_fault_allocator.c 带有既存 include-path 工作区差异；本批只读取其包装范围作为契约证据，没有改动该 C 文件，也不授予其整文件审查信用。其他会话 owner 保持其排除范围。

## Clone 与存储契约

StateMapClone 先处理空参数；source 与 destination 地址完全相同时直接成功，不做 shape validation，也不分配或修改 map。其余路径分别检查 source/destination 的四池存储形状，再按 capacity 字节范围拒绝池间重叠。实现不会验证 entry/checkpoint 的语义，也不检查两个 map 记录结构体本身的重叠；不同地址的记录对象与池须互不重叠且分别可安全释放。调用期间 source 记录及池不得被并发修改或释放，destination 须由调用方独占写入和释放。

Clone 将 functionToken、signatureHash、generation 复制到局部候选，并依次复制 entries、valuePool、rootPool、ownerStatePool 的逻辑 count 元素。成功副本的 capacity 收紧为 count。任一池复制失败时，局部候选统一 Free，原 source 和 destination 不变；四池全部准备好后才释放旧 destination 并转移候选，因此成功后旧 destination 池的借用地址立即失效。该操作不递增 generation、没有内部同步；允许其他只读方同时读取稳定的 source，但 source 记录及池不得被并发修改或释放，destination 须独占写入和释放。

storage shape 检查只验证 count 不超过 capacity、capacity 与 NULL 指针配对、容量字节可表示、同一 map 的四个容量区间不相交。它不证明池地址实际可读、checkpoint 有效或池能安全释放。Clone 的跨 map 检查比较全部 4×4 对容量区间；只要检测到重叠就拒绝，保护随后对旧目标池的释放。

StateMapInit 仅清零记录，不先释放旧池，适用于新鲜、零初始化或已释放 map。StateMapFree 释放四池并清零，但不校验 alias 或分配来源；传入者负责确保每个非空池可以独立释放，Free 后所有池借用指针失效。

## Fault wrapper 与测试边界

allocator H 的四个接口访问 allocator wrapper translation unit 的静态状态。失败 ordinal 从一次设置后的首个 wrapper malloc 起算；零 ordinal 关闭注入且重置 attempts/failure。wrapper 仅包住测试 TU 中宏替换 malloc/free 后文本包含的 exec_ir_state_map_storage.c；常规 fixture 的 calloc/realloc、其他 translation unit 的分配和自然 malloc 失败不属于 injected failure 或 tracked outstanding。状态无同步，Unity case 按单线程顺序使用。测试目标把 wrapper C 加入同一 executable，见 tests/cmake/ssa-tests.cmake:397 和 :410；CTest 注册为 ssa_state_maps，见 :422。

四个 fault case 分别检查四个分配 ordinal 失败、四池 Clone 成功后替换已分配目标、四池为空时不尝试分配，以及同地址 self-clone 的快捷返回。成功用例验证四个池地址均不同，但数据比较只覆盖每池首个逻辑元素；释放 source 后也只读取首元素。failure case 对完整 map 记录和每个 capacity=2 池的两槽字节做快照比较。所有 Unity 断言若提前失败，只 longjmp 离开当前 case；runner 随后继续 teardown 和后续用例，当前函数结尾的局部 Free 可能被跳过。

主 C 中四个直接 Clone 用例分别覆盖有限字段/指针的成功检查、完整浅拷贝目标 alias、destination 指向 source valuePool 内部的 cross-map overlap、以及单张 source 内 valuePool/rootPool 重叠。后两者在 Free 前手动脱离故意构造的借用地址。它们不证明四池所有字段都相等，也不外推其他池类型的 interior alias、map 记录重叠或语义 map 检查。CloneFunction 间接用例与 deopt aggregation 的直接调用可作为调用链证据，不并入本批测试 key。

本批未发现并记录新的 BUG/TODO。现有历史验收文档仅作线索；当前契约和测试信用以工作树实现及上述断言为准。

## 修改前证据与验证状态

Root 提供的修改前接管记录为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/state-map-clone-preedit-adoption-r1.json，SHA-256 2759023efad238c1c92d92d276b850cfae8512a2282db78a5f066efdd32f024b。该记录固定 HEAD 68be4daff31f5695aca71f68ece97df9dc21b8b7，采用同一实际 checkout 的既有构建和 ssa_state_maps 运行：构建 exit 0，direct 与 CTest 各执行 42 个 case 且 42/42 通过；3,216 个保守编译输入与该实际运行记录逐项相同。原始结果文件 state-map-builder-final-native-r1-results.json 的 SHA-256 为 9df5636687557ca0af2692e41c81ddc42cd07aa179dc2931ad74461f9f369a92，summary 为 e3d76b5fd5df0bc9911cdb2b0e34961b3f98bd532d56919634263a8a72ca4f9d，source pins 为 b7a4cb5015a46f5bced1ae102e7a0aadca68f06315e60c75ac8a5b83f97a0385。

这是注释前 42-case 目标基线，不代表这 42 个 case 都属于本批 Clone 审查。Root 已在同一实际 checkout、同一 clang-cl Debug 构建目录及既有 Unity noreturn override 配置下完成注释后构建、CTest 与 direct 运行，三者均终态 exit 0；CTest/direct 各 42/42 通过，全部用例的路径、原程序行号、名称、结果与消息逐项等于修改前基线。构建仅保留基线已有的 /experimental:c11atomics 未使用参数警告。

## 最终范围与验收

生产者初始租约为六份源码及本批 coverage/batch 两份文档；Root 最终提交范围另外包含共享 inventory 的六条所属记录，共九个路径。其余工作树和已有外来暂存项不纳入本批。主测试 C 的 44 个函数定义中，旧 Builder 五个加本批四个合计 9 个，仍有 35 个定义及其他适用声明待审；Core storage C 仍有五个未审函数，private H 仍有两个声明，public H 的其他类型与接口也未取得完成信用。只有 fault case H 的八个单元与 allocator H 的四个接口可标全文件 reviewed。

最终六源码的 strict R3 验证固定 13,769 个非注释 token，kind、spelling、StartOfLine、LeadingSpace 均一致；所有原程序 token 的物理行号和六个修改前 raw 字节前缀保持不变，主测试 C 既有 Builder 文档也未改动。新增 29 个 Doxygen 单元含 28 个带方向且名称后有空格的 @param、11 个非 void 的 @return；void 保留副作用说明，两个类型不虚构参数或返回值。self-clone 的池初始化、可释放及并发访问前提明确限于异址替换路径。

逐单元调用复核排除了头文件 include、函数定义和保存返回值后的断言对真实调用的冒充；Init/Free 的台账包含 parser 候选初始化、各回滚和旧 map 替换路径。check-batch 已核对最终 29 行、唯一键、九列与非空证据锚点；Root 与独立 reviewer 分别读取实现、调用、失败释放路径及实际断言。程序行未移动，反向引用的 Builder 台账不需修改行号，Root 另复核其原锚点。

实际 native R2 运行期间 3,216 个保守编译输入与 28 份冻结文件均稳定；与修改前实际基线相比，仅本批六份源码的 raw 注释内容变化，其他 3,210 个输入相同。运行后只修一条 allocator accessor 的 caller 锚点（91 是结果断言，85 才是真实调用），并完成本段与 inventory 元数据更新；运行时冻结记录保持原版本，六份测试过的源码未再修改。该目标的其余 34 个用例不授本批单元审查信用；本批不证明自然 OOM、记录体部分重叠、非法指针或并发访问，也不代表全套测试已运行。无新增测试、行为修复或 BUG/TODO。

证据位于 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/：state-map-clone-final-comment-equivalence-r3.json（SHA-256 0376970ddc1780e50faa5bee5f6e1fc04a0276652abf5d5a1d07ae22363c7a55）、state-map-clone-final-native-r2-summary.json（fca6b462d32de51a874dbfc1d4735a6efcaa4500ae977dc5b1bba54d880e419f）、state-map-clone-final-native-r2-comparison.json（605611521d2d14e6bf2556be4e078cc3d2657efd100efe13531f82486bd6866a）、state-map-clone-final-caller-unit-audit-r1.json（0046a9f5d2f16c8602942b902a818da78515df53c4956310bc1f075520b2d53a）和 state-map-clone-independent-source-review-r1.json（c62723d6f337d40be5e2f236abb0681bb09a2e98e86c0351cb005e24d8d3cd5f）。最终台账检查为 state-map-clone-final-check-batch-r4.json，暂存差异检查与提交收据按模块封存。
