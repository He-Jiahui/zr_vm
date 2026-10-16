---
related_code:
  - scripts/benchmark/benchmark_compare_viewer.html
  - scripts/benchmark/aggregate_benchmark_summary.py
  - tests/benchmarks/test_benchmark_compare_viewer.py
  - tests/benchmarks/benchmark_compare_viewer_harness.js
  - tests/cmake/benchmark-viewer-tests.cmake
implementation_files:
  - scripts/benchmark/benchmark_compare_viewer.html
  - scripts/benchmark/aggregate_benchmark_summary.py
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/benchmarks/test_benchmark_compare_viewer.py
  - tests/benchmarks/benchmark_compare_viewer_harness.js
  - tests/cmake/benchmark-viewer-tests.cmake
doc_type: module-detail
---

# Benchmark 对比页面的数据入口

## 输入与展示

`benchmark_compare_viewer.html` 是独立页面。文件选择器经 FileReader 以 UTF-8 读取 JSON；`aggregate_benchmark_summary.py --bundle-html` 则把摘要的 UTF-8 JSON 编码为 base64，写入页面的数据 script。两条入口共用 normalizePayload 和 renderAll，不导出页面私有函数。

聚合摘要只需包含 benchmark_report 或 comparison_report 中至少一份报告。仅 comparison 的摘要仍展示相对比率表，并清除旧 benchmark 的矩阵、分类表与条形区，避免前一次文件内容残留。原始 performance_report、comparison_report 的 legacy 包装入口继续有效。此修复不重定义聚合器的测量身份、比率有效性或 schema gate。

## Bundle 写入与 UTF-8

实际模板的第一个 `__ZR_BENCH_SUMMARY_B64__` 位于 `zr-bench-embed` 数据 script，后面的 JavaScript 还有同一占位符字面量，用于识别未打包页面。注入器只替换第一次出现；后续 guard 字面量保留，生成页因而会自动加载，原始模板仍等待文件选择。若日后改变模板中标记的顺序，必须同步注入器与此契约；本组不新增另一套模板协议。

atob 只返回每个字节映射为一个字符的字符串。页面把它转为 Uint8Array，再以严格 UTF-8 TextDecoder 解码后 JSON.parse，支持中文、emoji 和路径中的非 ASCII 字符。解码或 JSON 解析异常继续由现有嵌入错误区展示。运行环境要求支持页面已有 ES6 语法、FileReader、atob、Uint8Array 和 TextDecoder 的现代浏览器。

## 分类与文本边界

实现名 C#/.NET 与既有 .NET 别名都归原生/JIT 分组，不归解释器组。全量表的数据和比例计算不变。

aggregated_at_utc 在拼入 meta.innerHTML 前经过 escapeHtml，与目录和包含报告名保持同样的文本规则。来自文件 JSON 的 `<img ...>`、引号和 ampersand 只显示为文字，不成为 HTML 标记。本组只处理已确认的 timestamp 插值点，不宣称进行了整个 viewer 的完整安全审计。

## 真实测试与覆盖限制

从仓库根运行：

```powershell
$env:BENCHMARK_VIEWER_NODE = "C:/nvm4w/nodejs/node.exe"
$env:PYTHONIOENCODING = "utf-8"
& E:/cargo-targets/zr_vm/cache/python-3.12.10-embed-amd64/python.exe -B tests/benchmarks/test_benchmark_compare_viewer.py
```

通用测试本身不绑定上述机器路径：Node 可经 BENCHMARK_VIEWER_NODE 指定，也可从 PATH 查找。测试通过真实 Python 注入器生成内存中的 HTML，Node vm 执行页面完整 inline JavaScript。最小 DOM/FileReader 桩仅模拟浏览器边界；文件用例实际调用已注册 change 回调，bundle 用例只执行自动加载，未复制实现或导出私有函数。Node atob 桩刻意返回 latin1 字节映射，避免错误地用 UTF-8 解码掩盖生产缺陷。生成页面不写磁盘，不下载第三方依赖。

CTest 在已配置 benchmark Python 且能找到 Node 时注册 `benchmark_compare_viewer_python`，通过 `-B` 禁用 Python 字节码产物；可用 `ctest -R '^benchmark_compare_viewer_python$' --output-on-failure` 单独运行。未安装 Node 时仍可使用其他 benchmark 测试。

九项 unittest 覆盖五个目标行为及四项控制：comparison-only 文件及区域重置；真实 bundle 无文件自动加载；bundle 中文和非 BMP；C#/.NET 分类；恶意 timestamp 转义；legacy performance 加 .NET 别名；legacy comparison；未注入模板等待输入；无效 JSON 诊断。每次 Node 进程限时 10 秒，单段页面脚本执行限时 2 秒。

真实基线运行 9 项：5 FAIL、4 PASS。先单独限制注入替换后再跑 9 项：自动加载 PASS，中文断言明确观察到乱码，另三问题仍 FAIL；这一步为 UTF-8 提供不受自动加载故障遮蔽的 RED。最终全部 9 PASS，测试与 harness 在三个阶段哈希不变。Python 3.12.10 `-B` 与 Node v22.22.3 用于实际运行。日志、输入身份及差异登记在任务专属 E 根 manifest。未运行 native 构建，没有更改其冻结输入。

此套验证检查 DOM 输出字符串及入口事件，不模拟真实浏览器 HTML parser、事件执行、布局或视觉效果；timestamp 断言证明相关赋值已转义，不以桩环境声称实际执行过恶意事件。另未覆盖未知格式字段、全部 schema 版本或完整浏览器兼容矩阵。
