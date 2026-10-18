---
related_code:
  - scripts/benchmark/benchmark_compare_viewer.html
  - scripts/benchmark/aggregate_benchmark_summary.py
implementation_files:
  - scripts/benchmark/benchmark_compare_viewer.html
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/benchmarks/test_benchmark_compare_viewer.py
  - tests/benchmarks/benchmark_compare_viewer_harness.js
  - tests/cmake/benchmark-viewer-tests.cmake
doc_type: code-review-batch
---

# Benchmark 查看器：当前调用与生命周期契约

本批完整复审模板内 25 个单元：20 个函数、2 个状态和 3 个语句块；IIFE 继续沿用原 block 键，实际 21 个函数体没有重复授信。24 个旧键原位更新，仅补登 Uint8Array.from 的字节回调。逐项定义、调用方、入口、测试证据、契约和结论见[单元台账](../coverage/scripts_remaining.tsv)：15 commented、8 no-comment、1 BUG、1 TODO，无未审单元。新增 15 段相邻 JSDoc，六个单表达式回调由父文档解释，两个静态状态无需额外注释。

## 职责、使用时机与上下层边界

页面加载时先解析固定 DOM，再执行私有 IIFE，安装文件选择 listener 并尝试嵌入数据。聚合器 _inject_bundle 以 UTF-8 JSON 的 base64 只替换首个数据标记；模板检测字面量继续保留。模板未打包时静默等待文件，不是异常。字节回调只读取 atob 字符的码元，Uint8Array 持有临时字节，TextDecoder 负责 fatal UTF-8 解码；解析、识别与渲染异常在入口显示。

normalizePayload 只兼容外层报告形状；聚合对象与旧报告的浅包装均借用原数据，不执行完整 schema、有限数值或测量可比性验证。implCategory 的固定别名须与上游实现名称协作；C#/.NET 和 .NET 均归入 native。renderFilteredMatrix 对缺失的可选容器直接返回，并以组内 PASS 数值求分母；全量矩阵和条形图各按自己的行数据计算，zr 对照页仅展示上游比值，不重新证明可比性。

escapeHtml 用于 HTML 文本，不是 URL、JavaScript 或任意属性编码器；String 转换仍可能抛异常。renderAll 先清错误，再更新元数据和各区域，缺少某类报告会清相应旧区域。入口 catch 不回滚已经写入的 DOM；失败后的页面可能保留旧区域或部分更新。showError 使用 textContent，空消息隐藏错误，不负责清空报告。

## 资源和时序约束

脚本没有导出接口；闭包与 listener 由页面 document 保活，节点是借用引用。固定 id、脚本位于 DOM 之后及上游报告格式是调用前提。各次 change 创建独立 FileReader，仅读取首个选中文件为 UTF-8；onload 闭包借用当次 reader。没有取消旧 reader、选择代次检查、onerror 或 onabort 处理，读取失败可能保留原报告。这里的资源边界是浏览器对象和页面寿命，不推导跨页面或跨线程所有权。

TODO: “最后选择的文件应优先展示”是否为产品约定仍需核实。入口为 benchmark_compare_viewer.html:689 与 :709，核查 FileReader 完成顺序、连续选择、失败和取消场景。现有 harness 的 Reader 同步触发 onload，不能证明真实浏览器异步完成顺序；本轮不把意图不明提升为 BUG。

## 已复现问题与历史记录更正

BUG: 实现名为 __proto__ 的合法 legacy performance JSON 会在全量矩阵漏列。renderMatrix 在 benchmark_compare_viewer.html:545 用普通对象收集实现名，再于 :554 读取 Object.keys；同一输入的条形图仍显示该名称。通过现有 Node VM harness 的文件选择入口，使用 __proto__ PASS/ms=1 和 C PASS/ms=2，实测错误区隐藏、矩阵有 C 无 __proto__、条形图有两项。修改前后 probe 的 3210 字节输出完全相同，本轮只标记与记录，没有行为修复。

原台账中 comparison-only 识别、首标记替换、C# 别名和元数据转义四个 BUG，以及 UTF-8 TODO，均已与修改前的当前实现及现有测试交叉核实。这些历史记录已不适用于当前代码，更新为真实契约；既有行为修复不计作本轮修改或修复信用。FileReader 时序与上述新复现的矩阵问题各自保留独立判断。

## 注释纯度与前后验证

原始模板 17713 字节、492 LF（461 CRLF）；插入 15 段 JSDoc 后为 28657 字节、732 LF（637 CRLF）。按原偏移逆向移除插入可精确恢复全部原字节和原 EOL；无接口、程序语句、样式、数据标记或测试修改。新增文本均位于实际 script 内，未引入关闭 script 的字面量。该证据不声称经过 JavaScript AST 等价验证。

前后使用同一物理 Python 3.12.10 和 Node、同 argv/env、现有 E 盘专用临时目录运行现有测试。9 个相同 test ID 在两次运行均通过，两个测试父进程、两个 prototype probe 均自然退出 0，无 Root 新增超时；测试日志仅运行时间变化，prototype 原始输出逐字节相同。实际执行的是当前 HTML 内联脚本，没有复制渲染实现或新增重复测试。

既有测试覆盖嵌入自动加载、UTF-8 中文与非 BMP、comparison-only 及旧区域清空、C# 分组、非法 JSON、legacy 对照、原模板等待和时间字段转义。范围限 Node VM 与同步 Reader harness，不扩为完整浏览器、异步时序、全部 schema、平台矩阵或原生 Rust 测试通过。

本次行号移动同时精确迁移 harness/test 两份台账的 22 个旧行、28 个 caller/evidence 单元格和 67 个 HTML 行号；其余列与字节保持，不授予额外单元审查信用。旧批次文档保留其历史窗口。首方清单仅更新本 HTML 一行，导航仅追加本批链接。

## 交付与产物边界

F1 提案加 R2 的最小记录修正经过非作者 I2 独审；源码原字节复原、现有测试配对和实际暂存差异继续按独立复核门禁交付。仅提交本模板、三份指定台账、本批文档、导航及清单指定一行。其他会话工作树与正常 index 中的四项改动均保留。

本批只增加 1 个 canonical key；历史 Root 交付计数由 2872 变为 2873，该数不是全仓去重覆盖率。原生 Rust BEFORE 已自然结束且编译失败，本批没有激活原提案中的前瞻并发例外，也不给未运行的 checkpoint 测试授信。构建、缓存、私有 index 和必要证据保留于 E:/cargo-targets/zr_vm；本批释放 0B，产物生命周期仍为 OPEN。
