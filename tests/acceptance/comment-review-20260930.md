---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_observations.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_observations.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_observations.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_observations.h
plan_sources:
  - docs/code-review/comment-standard.md
  - user-approved whole-workspace caller-first comment review
tests:
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_compiler_return_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics.c
doc_type: acceptance-detail
---

# 2026-09-30 所有权观察模块注释审查验证

## 范围

本批补充 Observations C/H 的职责、引用快照、缓冲区归属、借用生命周期与失败契约注释；公开签名和非注释代码保持不变。

正式台账有 25 个真实单元。相关 Regions、Symbols 台账只调整引用本批 C/H 的授权 callers/evidence 字段；Symbols 已确认的 owner-pool BUG 记录保持原文。

本文件记录本批的有限验证。全仓文件清单、其他模块与并发 SSA、checkpoint 工作仍需各自完成验收。

## 基线

修改前源码取自 `4beaaccb0c93d18dd68232fe88b5e1ef261fb8f7`：

| 文件 | 修改前 SHA-256 | 本批冻结 SHA-256 |
| --- | --- | --- |
| Observations C | `7451b76a9c3eabe8c6ff445fbb458cba3e65e5ae9d172525c0faf12fb70dbe46` | `45604309a3af90109006870465afcd965eb372247e0b01d4aa7d30595a33baa8` |
| Observations H | `4f16ee194213353d5dd2ca4a494cd2a174ada2019c3b99ba9af17471843c197e` | `1b8bcd1356b4d9cf084947c859821fb406e493288d78273141cf9141487d89ae` |

C/H 分别增加 45/22 行注释；非注释 token 与上述修改前版本逐项相等。源码仍有原来的已证缺陷和待核实契约，本轮只记录问题。

共享工作树含其他任务的源码、测试与构建配置差异。本次构建使用新建的专属缓存；初始输入哈希记录在 `.codex/tmp/root_comment_validation_receipt_20260930.json`。GCC 已通过实际入口替换完成修改前对象比较；四目标结果在正常运行、当前对象对照和旧对象中完全相同。Clang 当前结果也与这份 GCC 失败集合一致。该结论不代表整个工作树的历史基线重建。

## 现有测试清单

| 现有目标 | 本批相关调用链 |
| --- | --- |
| `zr_vm_dataflow_engine_test` | CFG 传递、收敛与临时分析状态 |
| `zr_vm_compiler_semantic_query_diagnostics_test` | 语义诊断发布与 compiler 消费 |
| `zr_vm_compiler_return_ownership_diagnostics_test` | 返回值所有权、区域与错误投影 |
| `zr_vm_language_server_ownership_diagnostics_test` | MOVE、RELEASE、ERROR 事实和 LSP 诊断 |

`dataflow_engine` 和 `compiler_semantic_query_diagnostics` 属于 `language_pipeline` 套件，LSP ownership 属于 `language_server` 套件。`compiler_return_ownership_diagnostics` 已创建构建目标，但在当前 CTest 注册、套件和 manifest 中没有执行入口；本批将显式执行它，套件遗漏的意图另由测试审查核实。

本批通过仓库现有 `tests/cmake/run_executable_suite.cmake` 传入四个明确的可执行文件路径运行；不为本批新增或重写测试。两个已注册套件以各自构建根为工作目录，未设置 `UBSAN_OPTIONS`；本批按实际套件环境选取工作目录，不继承其他测试的环境属性。

现有正常、非法使用、区域释放和借用诊断用例可验证常规路径。观察缓冲区 OOM 的编译绕过链按完整合法静态调用路径标记 BUG；本批没有进行分配失败注入，也没有声称现有测试覆盖该故障。LSP 分配失败后的诊断完整性仍为 TODO。

## 工具与命令

WSL 工具链：GCC 11.4.0、Clang 14.0.0。两个独立缓存为：

- `D:/tmp/zr_vm/comment-review-20260930/gcc`
- `D:/tmp/zr_vm/comment-review-20260930/clang`

二者使用 Debug、Ninja、静态库和共享库配置，启用现有测试，关闭 Rust binding、扩展打包与性能 CTest。编译器临时目录分别设为对应缓存的 `tmp/`。

配置命令以 GCC 为例；Clang 配置只替换缓存末级目录和 C 编译器：

```text
wsl.exe --exec cmake -S /mnt/e/Git/zr_vm -B /mnt/d/tmp/zr_vm/comment-review-20260930/gcc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DBUILD_TESTS=ON -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=ON -DBUILD_RUST_BINDING=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF -DZR_VM_REGISTER_PERFORMANCE_CTEST=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

构建四个目标：

```text
python -X utf8 .codex/tmp/root_run_comment_validation_20260930.py gcc build
python -X utf8 .codex/tmp/root_run_comment_validation_20260930.py clang build
```

构建通过后分别运行同一组现有测试：

```text
python -X utf8 .codex/tmp/root_run_comment_validation_20260930.py gcc test
python -X utf8 .codex/tmp/root_run_comment_validation_20260930.py clang test
```

包装器把完整命令、输出和终态退出码存入各缓存中的 `comment-review-<phase>.log` 与 `comment-review-<phase>-receipt.json`。

## 当前结果

| 门槛 | 实际状态 |
| --- | --- |
| 25 单元与授权旧表 9 行的 schema、弱锚、caller 检查 | 通过 |
| C/H 非注释 token 与修改前源码比较 | 相等 |
| 旧表 19 处锚点语句比较 | 同一语句；H include 不变 |
| 独立 formal review | 通过；补正 4 个 evidence 单元后由原 reviewer 复核 |
| 全部现有 TSV 的本批反向引用 | 扫描 452 张表；9 张表 297 条引用，无无效或空行锚 |
| GCC / Clang 配置 | 两者均终态退出 0 |
| GCC 四目标构建 | 1008/1008，2026-09-30 19:49:27 UTC 终态退出 0 |
| GCC 现有四目标测试 | dataflow 9/9、compiler diagnostics 64/64、return ownership 3/3 通过；LSP 12/25 通过、13 项失败，runner 终态退出 1 |
| Clang 四目标构建 | 1012/1012，2026-09-30 22:44:19 UTC 终态退出 0 |
| Clang 现有四目标测试 | 前三组 9/9、64/64、3/3 通过；LSP 12/25 通过、同样 13 项失败，runner 终态退出 1 |
| 修改前动态失败集合比较 | GCC 正常运行、当前对象 preload 对照、旧对象 preload 基线的四组逐项名称及状态相等；LSP 实际绑定三个替换入口 |
| GCC 实际动态链接 | 四个 ELF 均依赖共享 parser；使用 RUNPATH，非 RPATH；已核实 Observations 三个入口导出 |
| 两工具链当前测试集合比较 | 四目标全部 101 个用例名称及通过、失败、忽略集合相等；88 通过、13 失败，无忽略 |
| MSVC 兼容性验证 | 19.44.35228.0，共享库配置、CLI 构建与 hello_world 均终态退出 0 |
| MSVC 同时启用静态及共享库 | 独立首次配置终态退出 1：Ninja 报 lib/zr_vm_core.lib 双重输出规则；失败回执保留 |

## 验收决定

**本批有限验证完成，保留已复现的失败集合。** 独立注释审查、静态契约核对和非注释 token 比较通过。GCC 旧 Observations 对象同样出现全部 13 项 LSP 失败；Clang 当前集合与之相等；MSVC 共享库模式 CLI smoke 通过。没有声称 LSP、双库 MSVC 配置或全仓测试通过。

共享索引锁后续已在只读采样中消失，根代理随后正常提交了独立审查通过的库存 24 行。本模块提交仍须重新核对锁、暂存区和明确的文件清单；不能沿用一次旧采样推定当前索引可用。根代理未删除锁或清理外来索引内容。

## 当前失败与比较限制

GCC、Clang 原始测试日志和终态回执分别在各自专属缓存的 `comment-review-test.log`、`comment-review-test-receipt.json`。13 项 LSP 失败涉及 loaned return、unique move、borrow/loan regions、owner release、using scope release、weak use、alias rebinding 与直接 weak receiver guards。GCC 的修改前对象已复现全部相同失败；这项比较只排除本批 Observations 注释的影响，没有定位失败根因。

第一轮基线 helper 在解析链接输入时把 SONAME 选项当作库路径，编译旧对象后退出；修正后的第二轮旧对象编译退出 0，但全量共享 parser 重链接超过 180 秒限时。两轮回执都保留为未完成。最终比较复用这份按实际编译参数生成的旧 Observations 对象，使用同一批现有 ELF。正常运行、当前对象 preload 对照、旧对象 preload 基线的用例名称及状态完全相同；两个 preload 分支均取得 parser 到替换对象三个公开函数的真实绑定记录，前后输入对象、现有动态库、测试 ELF 与本批 C/H 哈希保持一致。

最终 GCC 比较回执为 `D:/tmp/zr_vm/comment-review-20260930/gcc/observations-interpose-20261001/comparison-receipt.json`，终态退出 0。两工具链日志的用例集合和有限输入摘要核对记录为 `.codex/tmp/root_comment_linux_comparison_receipt_20261001.json`；该检查只读取现有日志，没有重复执行测试，也没有生成 Clang 的旧对象基线。

Windows 首次专属缓存为 `D:/tmp/zr_vm/comment-review-20260930/msvc`，显式同时启用静态与共享库，配置因输出文件碰撞失败。随后按项目默认的共享库模式使用新的 `msvc-shared` 缓存；三个终态回执为 `configure-receipt.json`、`build-receipt.json`、`smoke-receipt.json`。通过 `using-vsdevcmd` 提供的脚本导入 x64 工具链；脚本 ExecutionPolicy 仅在调用进程中设置 Bypass，没有改变持久策略。运行现有 `tests/fixtures/projects/hello_world/hello_world.zrp` 输出 `hello world`。未进行 Windows 全量测试。

Clang 构建中的只读 strace 观察到真实目录和源码的 `newfstatat/getdents` 仍在推进，并有明显的 WSL Plan9 文件访问延迟；该证据只能说明采样期间的 I/O 进度。没有禁用 glob、修改生成依赖检查或终止其他任务。

初始输入记录之后，其他任务修改了 `tests/cmake/ssa-tests.cmake`；选取的 Observations C/H、四个测试源文件和顶层配置摘要在后续采样中保持一致。有限路径哈希比较不是整个构建输入的封存证明。此次没有 capture START、完整库存归档或 capture 通过结果。
