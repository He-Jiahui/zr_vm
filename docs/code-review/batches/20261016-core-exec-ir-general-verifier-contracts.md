---
doc_type: code-review-batch
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_deopt_validation.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_module_constant_pool_verifier.c
  - tests/parser/test_ssa_compare_predicate_domain.c
---

# ExecIR 通用预检与验证入口契约审查

本批一份 C 源码 22 单元：15 个函数与 7 个需独立说明的语句块，全部补充中文 Doxygen；22 个完整定义范围、15 个平衡函数体、46 个参数标签和 12 个返回标签逐项核对。原始 44218 字节及 914 行完整保留，22 份文档按所属定义顺序在 EOF 追加。逐项台账为 coverage/zr_vm_core_exec_ir_general_verifier.tsv。508 个物理引行含 LF/CRLF 原字节散列已核，调用搜索覆盖公开/私有入口及跨 Parser、artifact、hotpatch、pass 的间接验证链；12 个现有测试证据配置只作依赖，不重复交付其定义。

最终独审期间的当前符号搜索还发现外来测试源码新增四处调用：tests/parser/test_ssa_source_integer_expression_proof.c:304、:306 的 large_capture，以及同文件 :662、:664 的 span_owner_oracle 分别调用 VerifyModule/VerifyFunction。两行公开入口台账已补入这些调用及当前原始行证据，原提案的搜索全集仅代表其冻结时刻；不因此增加该测试的定义或运行信用。源码 Doxygen 和原始程序前缀不变。

## 入口、验证层次与首失败

ValidateModule 先清本轮诊断，再检查模块四类池形状、函数数组身份及每个函数的共同预检；它不执行全部 CFG/SSA/EFFECT、附带 map 或模块 owned constants 语义验证。VerifyModule 先完成整个模块的轻量预检，再按函数顺序执行 VerifyFunction(ALL) 和 owned-constant 检查；任一 FALSE 保留实际首拒绝阶段，后续阶段未执行。模块 constantCount 为 0 时 owned-pool helper 的特意跳过允许调用方使用独立 Oracle 常量池。

VerifyFunction 先清诊断，拒绝非法函数/level，再做共同预检，依序执行所选 STRUCTURE、SSA、EFFECT；附带 stateMap 在所选阶段成功后仍须验证。单选 STRUCTURE 不代表携带 map 时无分析分配，也不免除旧 map 对当前 IR 的约束。Parser 的 BuildStateMaps 在 zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:375 使用 stateMap=NULL 的浅视图，避免重建期间读取旧 map；当前 exec_ir_build.c 的 unpublished-SSA 视图另有局部 mapView，不能混为同一协作入口。通用验证入口不替其它调用方自动建立这些视图。下层验证可以因 OOM 返回 FALSE，不把所有 FALSE 都解释为输入语义错误。

## 语句块、不变量与资源边界

共同预检无动态分析分配，按形状、binding rows、entry/value、deopt reconstruction 与 aggregate、侧池 ID/范围、block、instruction schema/flags/arity 的实际顺序拒绝。range 先用减法界限判断再索引；空区间可以位于逻辑末端，capacity 中尚未发布内容不是有效元素。共享 phiIncoming 先依声明选区区分普通 value、effect 与 memory 域，再核后续 range/编码约束；分类不证明选区互斥或 token flow。COMPARE selector 与 TYPE_TEST match 元数据在 SSA/EFFECT 之前检查，DROP_IF_INITIALIZED 的 owner/定义/cleanup 前提同属共同预检。

STRUCTURE 逐 occurrence 核双向 CFG 的重复边身份；不把一次集合成员命中等同于完整多重边匹配。非空块先拒绝提前 terminator，再核末 terminator、正常/异常结果路径、后继数量及 ordinal，最后核 terminatorInstructionId；空指令区间跳过该块。EXCEPTION_PAYLOAD 只允许非入口的异常 handler，全部已列前驱须以异常边进入，同块最多一条；不要求每个 handler 必须携带 payload。复合拒绝的 expected/actual 不能反推唯一失败子条件。

clear helper 整体清诊断，普通 setter 只写实际列出的标量与 sourceId，不清未写 hash 字段。诊断与任何输入及池不得重叠；模块、函数、map 与侧池只在同步调用期借用，期间不得修改、重分配或释放。下层 SSA、effects、owner/liveness 分析自己负责临时资源释放，不向调用方转移资源；本文件不修复、封存或缓存输入。并发只读验证需独立诊断并遵守分配器和测试钩子的同步条件。

## 验证证据

实际原始源码 BEFORE 六个既有目标构建通过，CTest 6/6 通过，core/deopt/value/effects/constantpool/predicate 直接程序均 exit0；233 frozen 与 3585 conservative observed 输入在运行起止稳定。Deopt validation 是 7 个实际 Unity PASS；core 的默认 14 个本文件调用与 3 个条件纳入的 allocator case 采用普通程序，实际输出还证实 CloneFunction 28 个与 CloneModule 60 个故障点及 invalid-input 汇总通过，32 位 size_t 溢出情形在当前 64 位平台显式跳过。value 13、effects 26、constantpool 7 为固定源码调用和成功 footer，不作 Unity/逐例追踪；predicate 输出 9 个场景、60 个前提和 36 个真值通过。只认实际可达的 Core 断言，不将前置 append/Parser 拒绝当成后续 verifier 分支证据。

修改后 Clang19 stdin raw lexer 两次 exit0，4,969 个非注释 token 的 kind、spelling、StartOfLine、LeadingSpace 与原始行列完全一致，词法输出逐字节消费。AFTER 六目标构建、CTest 6/6 与六个直接程序均通过；六组完整 stdout/stderr 和退出码与 BEFORE 逐字节一致，8 条 argv 及 20 项 cache 相同。每次运行各自 233 frozen/3585 observed 起止稳定，跨运行 frozen 仅本源注释变化；广域观察另有外来 LSP CMake 与 integer_expression_proof.c 变化，保留全部散列和非隔离比较限制，不推断整个外来实现/配置等价。BEFORE 摘要21645a…、strict1582cf…、AFTER 摘要65ba4a705ffa6d42e5fb0cc2fbb40755cab33b60fc6102d021fbd6cd98eb9dd8 是登记的 E 盘证据入口。真实 source/leaf 与最终暂存独审随后作为独立门禁，不以测试通过替代注释真实性检查。已知其它模块 Deopt aggregates535 前提失败及 SSA dominance 历史 3PASS1FAIL 对照保留，不由本批六目标通过推翻。验证不外推全仓、Linux、默认 SDK、隔离行为、原子 checkout、完整 SDK 输入冻结或暂存树执行。

本批无新增 BUG/TODO，不改实现、公开签名、断言或构建配置。VerifyModule 的定义范围元数据曾漏末尾右花括号，已仅更正提案五字段并重新独审全部完整函数边界；原提案和旧报告保留，不把散列匹配当作完整性证明。源码、leaf、batch 与 inventory 一行审核列共四路径。复用登记 E 盘 build/cache，未建源码副本和重复实现测试；释放 0 B，生命周期开放，必要证据保留。
