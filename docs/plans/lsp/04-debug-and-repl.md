---
doc_type: plan-detail
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
---

# 04 · 调试器数据/表达式/条件断点 与 REPL 表达式运行强化

现状盘点已表明：debug 的条件断点、hit-count、logpoint、表达式求值、变量检查**基本都已实现**。所以本篇重点是「**统一与增强**」而非「从零做」：把 debug 求值统一到 01 篇共享语义层，提升数据推断/表达式能力的一致性与深度；REPL 则要解决「执行状态不持久」这一本质限制。

---

## 1. 调试器：把求值统一到共享语义层

### 1.1 现状的割裂

`debug_eval.c`（`:907`）有一套独立的表达式解析+求值；主编译器、LSP、REPL 各有一套。结果：debug 里能算的表达式子集、类型展示、诊断质量都和编辑器里不一致。

### 1.2 方案

- 让 `debug_eval` 复用 01 篇 `semantic_query.h` 做**类型推断与语义事实**：求值结果的 `type_name`、`semantic_summary`、`reference_summary`（`debug.h:108`）直接来自统一推断，而非 debug 私有逻辑。
- 表达式编译路径统一：能编译为字节码在当前帧沙箱求值（借鉴 `lua/QuickJS-master/repl.c` 把 REPL 编成字节码），逐步替换 debug 私有迷你求值器，扩大可求值表达式范围（调用、lambda、泛型实例化）。
- 沙箱约束：条件断点 / watch 表达式默认**只读**（无 IO、不可改全局），防止求值副作用污染被调试程序；可配置放开。

### 1.3 数据推断增强

- 变量预览（`ReadVariables`）接 01 篇数值区间/所有权事实：在 hover/变量面板显示「x: int [0,9]」「buf: %unique，未释放」。
- `variables_reference` 递归展开补「所有权种类 + 引用计数」列（来自 `SZrOwnershipControl`）。

### 1.4 条件断点增强

条件断点已可用（`debug.c:592`）。增强项：
- 条件表达式编译期类型检查：设置断点时即校验条件是 bool、引用的变量在该帧可见，错误**在设置时**反馈而非命中时才报。
- watchpoint / 数据断点：维护表达式上次值，值变化时中断（借鉴增量表达式求值缓存）。
- 条件求值失败策略可配：默认「失败即中断并报错」，可选「失败视为 false 继续」。

### 1.5 DAP 兼容（中期）

当前自定义 `zrdbg/1`。中期加一个 DAP 适配层（把 `zrdbg/1` 方法映射到标准 DAP 请求/事件），让 VSCode 通用调试 UI 直接接入，扩展侧 `registerDesktopDebugSupport` 复用。借鉴调试事件/上下文分离模型（`lua/mono/mono/mini/debugger-agent.h`）。AOT 路径调试列为后续（需在生成的 C/LLVM 里保留行映射）。

---

## 2. REPL：持久执行上下文 + 更强表达式运行

### 2.1 本质限制（现状）

`repl.c:736` 每次提交 `CreateBareGlobal` 新建全局态，把会话源当前缀重编译（`repl.c:307-335`）。表达式求值不进会话，运行期对象/变量值跨提交丢失，且每次重编译全部 + 重载标准库。

### 2.2 方案：保活一个 REPL 全局态

- **会话级常驻全局态**：REPL 启动时创建一次 `SZrGlobalState` + 主线程态并**保活**，每条提交在其上**增量编译执行**，而非重建。
- **增量声明注入**：新 `var/func/class/...` 编译为追加到常驻全局态的声明，运行期对象保留；后续表达式可引用之前定义的变量与其运行期值。
- **表达式结果绑定**：表达式求值结果保留（如自动绑定到 `_` 或 `$1/$2`），可被后续表达式引用——真正的交互式。
- **标准库只加载一次**（启动时），消除每次重载开销。

### 2.3 风险与边界

- 重定义处理：同名 `var/func` 重新提交时的覆盖语义需明确（REPL 放宽为「后定义覆盖」）。
- 编译器需支持「向已物化全局态追加声明」——这是改动核心，需评估 `module_loader` / 全局符号表的可追加性；若一次到位风险高，可先做「保留运行期值的快照重放」过渡。
- 错误恢复：一条提交编译/运行失败不应破坏常驻全局态（事务式：失败则回滚该提交的副作用）。

### 2.4 表达式能力对齐

REPL 表达式走 01 篇统一推断与 02 篇诊断：`:type` 已展示语义事实，进一步让普通求值在出错时也给「具体问题 + 建议」（与编辑器一致）。

---

## 3. 落地顺序

1. debug_eval 类型/事实接 `semantic_query.h`（统一展示，低风险）。
2. 条件断点设置期类型检查 + 只读沙箱约束。
3. 变量预览补区间/所有权列。
4. REPL 常驻全局态（先「保留运行期值」过渡，再「增量声明注入」）。
5. watchpoint / 数据断点。
6. DAP 适配层。

测试：扩 `tests/debug/test_debug_expression_diagnostics.c`（统一推断后的类型/诊断）、`test_debug_agent.c`（条件断点设置期校验、watchpoint）；REPL 新增「跨提交引用变量与运行期值」用例。
