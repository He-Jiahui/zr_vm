---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.h
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/parser/test_ssa_state_map_ownership.c
doc_type: code-review-batch
---

# StateMap owner helper 契约审查 — 2026-10-16

## 范围与入口

本批7个唯一单元：实现4函数、私有头2接口及普通include guard1；6项补中文Doxygen，guard无需额外源码注释。两个物理文件的完整清单闭合，无新增BUG/TODO，不改签名、执行语句、断言或构建。明细为coverage/zr_vm_core_state_map_owner_helpers.tsv。

直接调用为size1/fail8/owners_valid1/resolve2；两处头include及31处CMake源码引用交叉核验。owners_valid由整图验证器逐entry调用，resolve由MaterializeState的NULLtarget和候选输出路径调用；上游Validate6和Materialize39入口含测试inc均追踪。fault TU文本include只在另一编译实例替换owner-analysis分配，不重复本批物理定义的信用，不把源码列表引用都当实际编译。

## 调用与生命周期

owners_valid借用同一未变function的owner/liveness分析和已验证live/root/identity存储，自查owner slice。每个required live恰一次，保守附加值须静态可用；对应静态owner和GC/UNIQUE/SHARED的逻辑root次数须一致。这里没有真实GC根注册、payload验证、分配或输入所有权转移，也不是可绕过上游存储校验的整图验证器。

resolve要求witness指针/长度成对，非空时全function长度但只读取所选live值的valueId-1槽。具体witness须在静态mask内，非CONDITIONAL还须等于静态值。无witness且prepared为空可保留静态CONDITIONAL；非空候选拒绝未解的CONDITIONAL。witness非空即便无输出仍可分配两个临时分析。

prepared是外层私有候选；逐live写ownerStates后稳定压缩原root列表，仅保留INITIALIZED项，不清尾部、不改容量。较晚witness失败可能留下较早ownerStates写入，本helper不回滚。外层失败释放候选、全部成功才释放旧target并提交；不得把外层事务归给helper或任意别名调用。

诊断可空，非空须独立可写，不与输入、analysis、prepared记录及池重叠。owner builder失败自行清理已取得资源后直接返回；进入liveness/witness后统一finish释放两分析。同步借用不保存输入或保活对象，调用方负责稳定性和并发互斥。

## 测试证据边界

现有conditional witness/root用例检查root1/0、具体owner和有限record字节保持；NULLtarget无witness成功不等于公开调用无分配。伪造静态owner、缺required live和managed root的失败只支持实际断言及已追踪首拒绝分支，不能从用例名扩大覆盖范围。

没有直接size极限/zero-size运行证明；既有outer owner OOM没有witness，不证明resolver自身nonnull-witness及liveness分配失败。record字节相同不独立证明五池内容相同。Unity长跳会跳过未执行局部清理；共享fixture、fault allocator和其他测试仅作为证据，已有单元不重复授信。

## 基线与验收

修改前两个raw文件与前批实际三目标native的输入SHA相同；直接复用其build0、CTest3/3及42/8/29共79场景输出作比较基线，不另建源码副本或before构建树。前批四目标聚合构建在范围外string.c:512的!void HashSet_Init失败，SCCP14未执行；原失败和并发输入变化保留，不称四目标或固定全仓通过。

本批Clang原始词法检查通过：1257个非注释token的种类、拼写、StartOfLine/LeadingSpace及原行列均一致，两文件原始前缀完整保留。独立实际source/leaf审查ACCEPT，核实6份Doxygen、7行台账、物理清单、180个非空引用和限定inventory差异。

前批string.c编译阻塞已由文件所属会话改写；本批未修复该代码或调整构建配置。第一次重新尝试四目标，实际build0、CTest4/4及四直接程序exit0，但期间tests/CMakeLists.txt从e223f77d…变为da4b3d4c…，唯一外部输入变化使62冻结输入检查失败。原始日志、冻结失败及进程收据保留，未将其作为最终验收。

随后复用同一实际构建目录、相同四目标和配置重新验证：build0、CTest4/4，直接42/14/8/29共93个场景通过，按各runner格式逐行等于先前实际基线；SCCP14单独比对较早Validate41实际输出，不冒称前批受阻时已执行。最终运行62冻结输入与3517个保守代码输入均稳定；间隔内外部改动另记SHA。不新增测试或源码副本，不构成完整套件、固定全仓、独立暂存树、Linux或默认SDK验证。

提交仅纳入两源码、7行台账、本说明及inventory两指定行列，共5路径。Root及独立reviewer检查实际暂存差异、上述验证和git diff --cached --check；提交后核路径、blob、parent、He-Jiahui身份和外部索引/工作树保留，才记7项历史交付信用。

## 证据与产物

同任务报告根E:/cargo-targets/zr_vm/reports/first-party-review-01a13463保存不可变producer/独审提案、Root audit、source lease及后续strict/native/stage/commit收据。CommonMacros源码列表引用输入与提案raw存在外来差异，相关GLOB_RECURSE入口重新读取；原有调用行通过EOF追加保留。进一步实际输入漂移逐项记SHA，不扩大冻结范围。

复用E盘实际checkout build/cache，无源码副本或新增测试。生命周期open，本批释放0B；既有清理自动拒绝、必要失败证据及共享/外部/未知产物保留。
