---
related_code:
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_common/CMakeLists.txt
  - zr_vm_common/ThirdPartyMacros.cmake
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_lib_ffi/CMakeLists.txt
  - zr_vm_lib_thread/CMakeLists.txt
  - zr_vm_rust_binding/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_core/src/zr_vm_core/hash.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime.c
  - zr_vm_core/src/zr_vm_core/zrp_metadata.c
implementation_files:
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.h
plan_sources:
  - user: 全仓库首方代码调用链审查与意图注释任务
  - docs/code-review/comment-standard.md
tests:
  - tests/fixtures/projects/hello_world/hello_world.zrp
  - tests/module/test_metadata_token_model.c
  - tests/module/test_metadata_runtime_query.c
doc_type: testing-guide
---

# 2026-10-01 注释审查批次验收

## CommonMacros 的15个审查单元

独立 reviewer 阅读全部17个 include 方及六函数的调用上下文。当前首方构建范围是88文件；旧87文件快照之后，另一会话增加了12行的静态库异常/GC测试注册，且更新了 `tests/CMakeLists.txt`。两处变化已复采；CommonMacros 的 include、六函数调用位置及语句仍与草稿一致。调用数依次为 module15、executable1、module link48、internal link3、executable link12、install15。

专用台账登记六函数、一次 include 和八个独立契约块，共15单元：7 `commented`、5 `no-comment`、2 `BUG`、1 `TODO`。源码保留161行，六处注释位置改变，非注释 CMake 命令逐行相同。审查清楚区分逻辑目标名与目录名、PRIVATE 依赖和公开头使用要求、配置结束前解析的前向依赖，以及 CLI 自身链接与生成外部产物的宿主库目录。

八条旧 CommonMacros 记录从 common 总表迁至专用台账；其他旧行字节相同。FFI 旧表仅物理第4行的 evidence 字段改变，以16个精确位置说明 libffi attachment 的目标创建前提和两种变体调用；该修订不代表整张旧 FFI 表已通过当前规范。inventory 只更新 CommonMacros 一行的实际 SHA 和审查说明。

实际应用门禁确认15单元、340个非空 caller/evidence 锚点、161/161行、相同非注释命令、旧表精确移除8行、FFI精确单字段和inventory精确单行。schema 与 `git diff --check` 均退出0。原反向预检记录263个有效 CommonMacros 位置、19个表；源码行数和配置语句未移动。旧 FFI 的空行起始范围单独修正，不能用有效位置计数掩盖它。

## 已确认问题的边界

- `BUG:` MSVC19.44.35228.0 + Ninja 同开静态/共享库时，静态归档与共享导入库都输出 `lib/zr_vm_core.lib`，配置实际退出1。失败缓存和 receipt 保留；此批只在共同 basename 处标记，不改命名和库选择。
- `BUG:` 对已创建 executable 组合首方 plain 与已注册第三方 PRIVATE 链接入口，会触发 CMake 签名混用错误；当前 CLI 的相关调用全是 plain，不把该 API 组合缺陷误称默认 CLI 配置失败。
- `BUG:` thread 的逻辑目标名为 `zr_vm_thread_*`，CLI/Rust 却查询 `zr_vm_lib_thread_*`，因此跳过能力宏与模块注册。已登记完整调用链；`TODO:` 后续对应调用方模块在调用位置补充标签，行为修复另行处理。
- common 新源的增量发现、裸模块名宏的外部消费和 Windows 共享依赖导出声明，保留各自具体核查入口。

## CommonMacros 修改前后实际验证

复用本任务专属缓存 `D:/tmp/zr_vm/comment-review-20260930/msvc-shared`，采用项目默认 shared-only 配置，使用 VSdev 包装器分别运行显式配置、构建现有 `zr_vm_cli_executable` 和现有 hello_world 项目。未增加模拟实现的测试。

| 阶段 | 时间（UTC） | 配置 | CLI构建 | hello_world | 原生句柄终态 |
| --- | --- | --- | --- | --- | --- |
| 修改前 | 02:36:15—02:37:33 | 0 | 0 | 0，输出 hello world | session33967，退出0 |
| 修改后 | 03:10:48—03:11:38 | 0 | 0 | 0，输出 hello world | session51723，退出0 |

前后 CommonMacros SHA 分别为 `cdd3c30deb745111c26311a9283696c389c86da2adc1a78017551a9b0204fe4e` 和 `8ba6dcf309f57d4180868967faaa72c249fe20d903a7ed0a1507ed765bc8ac6b`，各自运行期间保持不变，各次构建配置文件哈希也未漂移。两次之间其他会话的源码和测试配置有更新；这两次构建不是整个工作区的受控旧版本对照。注释等价性另由精确源码门禁证明。

实际命令和日志在该缓存的 `common-macros-review-20261001/before.json`、`after.json` 及对应 configure/build/smoke 日志。修改后的18个 Ninja 动作包含其他会话已更新的 core 源编译与共享库重链接；没有宣称只重新配置了注释或完成全平台矩阵。

验证范围是 MSVC shared-only CLI 与一个现有项目。上批 GCC/Clang 四目标仍记录101例中88通过、13个相同 LSP 失败；此批没有重复或覆盖那个结果。没有全仓绿色或跨仓采集成功结论。

## Git 与跨仓协调

根代理只对本批精确文件使用正常串行提交，不清理或提交外部 dirty/untracked 文件，不删除共享锁，不使用私有 index。提交成员与实际 SHA 另由终态 receipt 核实。

唯一已接受的 readiness 队列为 `01a0e3a1-2bbe-7f23-9026-9e5aa6296c8f`。本批未收到 capture START/terminal result/release，也未执行 inventory/archive、重复 validation 或网络调整。源码哈希、构建终态和模块提交不构成采集封印。

## 编译器签名 C/H 的46个审查单元

根代理逐项复核全部643行C和144行H及其真实调用上下文，修正旧签名格式说明。C登记21函数、一个域常量和八个独立契约块；H登记15声明和一次include guard。最终专用台账共46单元：35 `commented`、7 `TODO`、3 `BUG`、1 `no-comment`。旧45单元台账漏掉include guard，且部分调用方列表不完整。

新台账补齐73个直接调用锚点，其中C单元7个、H接口66个；H接口与其C定义的当前调用集合一致，声明、定义和测试内手写prototype不算调用。独立重扫当前2621个首方C/H文件，无函数指针或其他非调用引用；另扫927个首方脚本、跨语言源、配置和样例文件，对这21个签名函数没有额外引用。随后读取计长/写出配对、字符串与类型快照、临时数组释放、token/record发布及公开reader前置条件，机械计数不是语义验收的替代。

实际源码只修订C31/C519与H9/H103四行注释。方法writer的 `BUG:` 明确以合法非零泛型blob、合法ZRP signature pool及成功附加为前提；compiler函数级raw heap至该pool的自动桥接未获证实，保持独立 `TODO:`。arity超过255的上限、递归计长累加及字符串键规则的其他疑问仍逐项登记。哈希返回注释补充底层分配/更新失败也可返回零，并将target fallback的失败传播与core零哈希放行路径记录为 `TODO:`，不把未完成的合法producer/loader前提升级为BUG。

正式应用后两源仍为643/144行，非注释token与修改前相同。editor只在H的补丁上下文规范化11个CRLF结尾，在模块文档上下文规范化7个结尾，并省去草稿末尾一个额外空行；H正文与草稿完全相同，文档除换行及末尾空行外完全相同。原草稿、初次字节不匹配及后续实际物化receipt均保留。inventory仅更新这两个源的物理3012/3013行，H的SHA随后按实际字节精确修订。

模块文档保留所有旧物理位置，修正643行的一段旧格式说明并追加调用边界审查。前后门禁扫描415张正式台账的334个反向锚点，其中12个指向该模块文档；全部仍有效，无现有锚点落在四条改写的注释或643行文档句子上。正式46单元schema、非空/弱锚点检查及 `git diff --check` 均退出0。

## 签名模块的修改前后实际验证

使用本任务专属 `D:/tmp/zr_vm/comment-review-20260930/gcc`、`clang` 和 `msvc-shared` 缓存。Linux分别构建现有 `zr_vm_metadata_token_model_test` 与 `zr_vm_metadata_runtime_query_test`，通过未改动的 `run_executable_suite.cmake` 运行；两程序各21/25例。Windows验证范围为项目shared-only配置、现有CLI和hello_world项目。

| 工具链 | 修改前（UTC、原生终态） | 修改后（UTC、原生终态） | 实际结果 |
| --- | --- | --- | --- |
| GCC | 03:25:33—04:06:22，session47055退出0 | 04:17:47—04:30:37，session70085退出0 | 前后46/46通过，用例身份、顺序、状态及行号一致 |
| Clang | 03:25:35—03:51:35，session45910退出0 | 04:17:48—04:36:00，session11440退出0 | 前后46/46通过，用例身份、顺序、状态及行号一致 |
| MSVC | 03:32:53—03:37:31，session38990退出0 | 04:17:50—04:20:29，session94834退出0 | 前后配置、CLI构建、hello_world均退出0，均输出hello world |

修改前C/H SHA分别为 `c0dce8fdf99af076055128c212e6e46fb5fef2812de24c631a9845468e19a892` / `5f465773429aa04af3e62df2e8bd6341bf4e4d6e6d323de58c2ab27ca53bdd96`；实际修改后分别为 `9d7e0fb77d8854141bc8e66661171a15ba7b1a9f4c2f1f002dd81c53b9cb747e` / `ee4c86b885dc7f8c9370d39f3447acd1e41005ce57bb156ac78b0adf96d61c52`。六次运行各自保持对应两源哈希不变。GCC/Clang修改后各19个Ninja动作，MSVC修改后15个动作；实际命令、逐项状态和完整日志保存在各缓存的 `metadata-signature-review-20261001/before.json`、`after.json` 及对应日志。

逐例前后比较receipt于04:38:42Z确认上述结果。其他会话在两阶段间更新了工作区，HEAD从 `9de3d7398c8971c1a22a908597dcf5286f4dfbf0` 推进到 `49a01febdc57ab83b2616bea7c8e38e560677f2b`；这些是限定范围的真实验证及精确注释等价性证据，不是整个旧工作区的受控对照。现有用例未执行非零METHOD泛型格式差异或分配失败的运行时故障注入，相关BUG采用合法输入的完整静态调用证据。没有覆盖上一批101例中13个既有LSP失败，也没有全仓绿色或跨仓捕获成功结论。

该模块采用六条精确路径的正常串行提交：两源、专用台账、inventory、模块文档及本文。提交成员、实际文件和规范化Git blob另用终态receipt核实。唯一readiness队列保持不变；本模块同样未收到capture START/terminal result/release。
