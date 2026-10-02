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
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_owner_sets.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_observations.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_public_contract.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
implementation_files:
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
plan_sources:
  - user: 全仓库首方代码调用链审查与意图注释任务
  - docs/code-review/comment-standard.md
tests:
  - tests/fixtures/projects/hello_world/hello_world.zrp
  - tests/module/test_metadata_token_model.c
  - tests/module/test_metadata_runtime_query.c
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_compiler_return_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics_owner_set_cases.h
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_property_consumer_contracts.c
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

## Ownership dataflow driver 的26个审查单元

根代理和两位独立 reviewer 检查当前 driver 的3个类型、14个普通函数、3个回调、1个递归原型及5个契约块。全部17个有直接调用的函数调用集合和3个回调注册点已核对，并沿通用 solver 的同步 dispatch、compiler late-check 与 LSP diagnostics 消费解释使用意图。台账共26单元：21 `commented`、1 `TODO`、2 `BUG`、2 `no-comment`；未加注释的单元也保留理由。

源码由767行增至853行，只增加86行注释，所有原始源码行和非注释token相同。注释说明一次分析的状态所有权、同步回调借用栈上analysis的限制、读取检查先于写入重置的目的、UNKNOWN owner集合与已知EMPTY的区别，以及返回false可能留下部分context事实。两个BUG分别登记合法loan alias分配失败后的诊断遗漏，以及合法Unique消费后的观测分配失败被compiler调用链忽略；lambda独立CFG与LSP best-effort失败处理仍保留具体TODO。没有运行allocator故障注入，没有修改行为。

正式门禁核对26单元、265个caller/evidence锚点、3549个分类首方文件及416张正式台账。7张既有表的92条物理记录中，382个现有driver反向位置按精确原始行映射更新；单位、判断及非driver证据未改变。独立迁移清单与根代理反向多重集合逐项一致。inventory只更新driver一行；模块文档与索引同时补充职责和调用限制。schema、非空/弱锚点、token等价性及限定路径 `git diff --check` 均退出0。

独立语义报告保持冻结；其首次hash manifest末尾为字面量反斜杠n，作者原地修复了manifest，未按要求另建版本。有效manifest和报告hash已由根代理重查，这个过程限制仍保留。独立反向helper最初按unit名判归属的检查，被根代理针对实际file列的检查补足；没有据机械计数推断语义正确或全仓完成。

## Driver修改前后实际验证

使用本任务专属 `D:/tmp/zr_vm/comment-review-20260930/gcc`、`clang` 缓存，构建并运行现有 `zr_vm_dataflow_engine_test`、`zr_vm_compiler_semantic_query_diagnostics_test`、`zr_vm_compiler_return_ownership_diagnostics_test`、`zr_vm_language_server_ownership_diagnostics_test` 四个可执行文件。Windows使用 `msvc-shared` 缓存，验证shared-only配置、现有CLI及hello_world。命令和完整日志保存在各缓存的 `ownership-driver-review-20261001/before.json`、`after.json` 及对应日志。

| 工具链 | 修改前（UTC、原生句柄） | 修改后（UTC、原生句柄） | 实际结果 |
| --- | --- | --- | --- |
| GCC | 04:57:08—05:14:42，80224 | 06:03:55—06:51:19，63810 | 前后构建退出0；各101例，88通过、13失败、0忽略，测试退出1 |
| Clang | 04:57:08—05:10:33，10798 | 06:03:56—06:49:45，92140 | 前后构建退出0；各101例，88通过、13失败、0忽略，测试退出1 |
| MSVC | 04:57:13—05:02:40，55338 | 06:03:57—06:06:33，35325 | 前后配置、CLI构建、hello_world均退出0，输出hello world |

07:51:54Z的逐例比较确认GCC/Clang前后101个用例的目标、名称、顺序和状态完全相同。13个失败全属于既有LSP ownership diagnostics，完整名称保存在comparison receipt；这些失败未跳过、未修复，也不作为全绿结果。修改前源SHA为 `46f33ac267e402ebfd12a9e411e73adcd2b692cb1437796264d7d2edac1812fe`，修改后为 `6addd445734be338bf527db7ad89448cf845f6df3f7c49b570ba28658cef5e7e`，六次运行各自保持对应源哈希不变。两次Linux修改后各8个Ninja动作。

期间其他写入组将HEAD从 `f9828dc877293e5d0d0d8512a81af19ebafe0991` 推进至 `7e3e0782f30ff2d05017ebc79901fec7d52e9c87`。这是限定目标的真实比较，源码行为不变另由精确注释等价性证明；没有将并发工作区视为受控旧版本。恢复上下文后两个Linux工具句柄已不可查询，随后读取了其实际terminal回执、构建/测试步骤与源码哈希，没有重启验证来替换结果。

本批按13条精确路径正常串行提交：driver源、专用台账、7张位置迁移表、inventory、模块文档、索引和本文。提交成员与规范化Git blob另由终态receipt核实。唯一readiness队列仍为 `01a0e3a1-2bbe-7f23-9026-9e5aa6296c8f`；未收到capture START/terminal result/release，未重复validation或执行archive。模块验证与commit不构成跨仓捕获封印。

## 公开语义查询头的58个审查单元

本批定义边界为公开 H 的31个API、17个类型与10个独立契约。根代理读取生产与测试的完整调用函数，并接受 core、call、symbol、type/contract 独立复核；当前3549个分类首方文件中枚举619个直接调用、61个声明/定义和288个完整caller函数。没有额外API函数指针或其它首方语言引用命中；Rust/CLI间接入口仍按其各自实际调用链定位，检索无命中不等于全仓无间接入口。

正式查询头由346行增至457行，非注释token完全相同。专用台账58单元记录35 commented、7 TODO、15 BUG、1 no-comment与332个证据锚点。paired include guard有明确无需再解释实现的理由。FormatCall优先事实的signatureDisplay；ReferencesOf没有复用元素宽度校验，宽度由调用方满足；设计差异保留具体TODO，不声称已观察到合法调用违反宽度前提。

候选字段callableTypeId原样传递symbol.typeId，方法producer可能写ownerTypeId；debug临时求值context的数值TypeId传输保留代际设计TODO。BUG限定在已由合法producer和具体首次分配失败闭合的静态链，没有运行OOM注入。最终symbols reviewer独立全文读29个生产及24个关键测试函数，其他测试语义来自先前完整分片与根代理阅读；字节核对不冒充独立全文审读。

实际门禁于15:14:56Z确认查询头SHA `05f8731295fd85ffc250a2a034007e114233cb7fc97001e37cfe9cf752cab6f0`、58单元/619调用/332证据和7张旧表的20行/70反向锚点。十项旧comment锚点按已复核的职责段落映射，四处过时契约说明同步更正。七表一次整体补丁stdout曾被工具截断；没有从该输出应用任何表，之后逐表完整输出、应用并核对实际内容。源码及台账schema、精确锚点与限定路径diff检查均退出0；这些门禁不替代运行验证。

## 查询头修改前后实际验证

使用原任务专属 gcc/clang/msvc-shared 缓存中的 query-header-review-20261001，保留全部首次结果和恢复结果。修改前Linux七个现有query/property程序各163例、161 PASS/2 FAIL；失败身份为 `test_diagnostic_registry_assigns_stable_descriptors` 和 `test_diagnostic_message_table_covers_registry_and_falls_back_to_english`。

| 工具链 | 修改后实际观察 | 原生/步骤状态 |
| --- | --- | --- |
| GCC | WSL CreateVm/0x800705b4超时，0编译动作、0测试例 | session12074退出1；WSL步骤4294967295 |
| Clang | 同一VM创建超时，0编译动作、0测试例 | session31291退出1；WSL步骤4294967295 |
| MSVC首次 | 配置0；构建工具句柄丢失，随后任务launcher/cache进程核查0匹配；未观察smoke | session18947最终句柄Unknown，构建退出码未知，原running receipt保留 |
| MSVC恢复 | 15:55:23–15:58:08Z，独立after-recovery配置/CLI构建/direct smoke，输出hello world | session34056退出0；三步骤0/0/0 |
| GCC恢复 | 21:01:02–21:18:16Z，81个构建动作；七程序163例，161 PASS/2 FAIL/0 IGNORE | session99332退出1；构建0，逐例状态与before相同 |
| Clang恢复 | 21:01:02–21:17:35Z，81个构建动作；七程序163例，161 PASS/2 FAIL/0 IGNORE | session14689退出1；构建0，逐例状态与before相同 |

早前Ubuntu-22.04的running状态并未证明命令可执行，两次有限 `/bin/true` 探针仍超时，原收据保留。随后实际读取SSA会话17:19–18:04Z的较新WSL验证日志，再于21:00:36Z执行一次20秒上限的 `/bin/true`，实际0.358秒退出0，据此启动独立after-recovery验证。生成阶段曾等待挂载文件系统的 `p9_client_rpc`，最终两次构建均完成；未停止WSL服务或外部作业。GCC/Clang的before、首次失败after与after-recovery收据、命令和完整日志全部保存在原缓存的 `query-header-review-20261001/`，没有覆盖或跳过失败结果。

21:20Z的逐例比较确认GCC和Clang各163个目标/用例/状态多重集合与修改前完全一致，两个失败身份仍是上述诊断registry/message-table用例，未把包装器退出1改写为全绿。before查询头SHA为 `91dabf637ff270bdb5dbc8f8ed00659c42228ef126b608c821bae7fc8c4fa8ab`，after与恢复运行保持 `05f8731295fd85ffc250a2a034007e114233cb7fc97001e37cfe9cf752cab6f0`。MSVC原before包装器失败/OSError、独立CLI烟测及中断after仍保留，恢复三步骤0/0/0另由windows-validation-closure核验。此验收是注释批次的限定目标对照，期间其他会话改动core与SSA代码；没有受控整个旧工作区、故障注入或全仓绿色结论。

本批采用15条精确路径正常串行提交：查询头、58单元台账、7张位置迁移表、模块文档、foundation文档、parser索引、code-review索引、inventory和本文。提交成员及实际Git blob另由终态收据核实；不纳入其他会话的源码、测试或临时产物。唯一readiness队列仍为 `01a0e3a1-2bbe-7f23-9026-9e5aa6296c8f`，未收到capture START/terminal/release；本批源码哈希、验证与模块提交均不构成跨仓捕获封印。

## Semantic Context 与 Facts 的310个审查单元

本批正式边界为 `semantic.c`、`semantic.h`、`semantic/semantic_facts.c` 与 `semantic_facts.h`。144条宏观注释说明快照身份、provider/resolver 和诊断缓存、注册与发布约束、载荷所有权以及查询证据的含义。context台账179项、facts台账131项，共250项字段契约；guard、内部控制块和无需额外注释的单元各自保留结论。

调用方语义来自15个完整函数分片、3个非函数引用分片、原作者阅读与根代理复核；原函数语料有1481个不同调用或支撑函数、1749个分配角色，原作者另有194个角色。独立context实现、facts实现和facts头部review核对真正的生产与消费协议，最终144条注释另经过独立复核，135条接受、9条精确修订；7项字段说明勘误保留。最后两个台账reviewer没有重新独立全文阅读所有汇总调用参数或250字段，不能把其指纹核对扩大成重复全文审读。根代理逐项复核最终310项intent、constraints、decision和reason；格式和调用方向门禁只确认这些判断适用于当前源码。

property基础发布未认证accessor存在，relations的FUNCTION、非零callable ID及typeId相等也不等于完整canonical节点验证。scope发布复制parentScopeId，只重写本条id；visible发布需要已有scope/symbol，位置与owner/kind仍有各自限制。ReferenceAtPosition先偏好起点命中，之后才比较宽度和用途；NumericByNode选择单一已有事实，不合并范围或风险。EXACT/IMPLICIT conversion标签不普遍证明兼容性已检查，普通调用producer明确关闭共享mapper的该检查。

三类BUG是限定合法输入路径的静态结论：types/scopeFacts初始原生分配失败未传播、overload members初始分配失败仍发布，以及内部独立VM长串未交接GC根。第三类限定为同线程默认incremental模式、至少128字节的独立副本、成功完成FullGC、没有补充host trace；URI消费需要两个同SymbolId的import候选，所有权消息的首个失效读取位于REPL的repl_string_text。原输入有根不保护独立副本；不宣称所有Expression/Reference consumer、其它GC模式、短串或未完成FullGC都已证明相同故障。UInt32 ID耗尽策略保留具体TODO。未执行分配失败注入或动态GC复现。

四源文件由2755行增至3267行，Git差异为515行注释增加、3行旧注释删除，所有非注释token相同。正式逻辑行与修订稿完全一致；两个context文件有行尾规范化，实际SHA单独记录。47份已有台账迁移359个引用，359个端点逐字对应原语句；其它审查文字保留，不重新认证其结论。两份新台账的check-batch、55条源/表/文档路径的diff检查及模块本地链接检查均实际退出0。首版helper将LF规范化实际hash与混合行尾草稿原始hash比较而失败；该失败保留，随后比较双方原始与逻辑内容，未发现正文差异，未作为源码BUG/TODO。

## Context／Facts 修改前后实际验证

使用任务专属 `context-facts-review-20261001/` 收据和日志，保留before与after，复用原gcc/clang/msvc-shared缓存。Linux运行七个既有query/property程序，加semantic_facts、canonical_type_graph与dataflow_engine共十个程序。

| 工具链 | 修改前实际终态 | 修改后实际终态 |
| --- | --- | --- |
| GCC | 21:33:47–21:34:32Z；native81895退出1；构建0，208例206 PASS/2 FAIL | 21:46:54–21:56:08Z；native65780退出1；292个构建动作、构建0，208例206 PASS/2 FAIL |
| Clang | 21:33:47–21:34:29Z；native8212退出1；构建0，208例206 PASS/2 FAIL | 21:46:54–21:53:46Z；native9790退出1；292个构建动作、构建0，208例206 PASS/2 FAIL |
| MSVC | 21:33:47–21:34:22Z；native13389退出0；configure/build/hello_world均0 | 21:46:54–21:47:26Z；native82228退出0；configure/build/hello_world均0 |

实际逐例比较确认Linux两个编译器的目标、用例和状态多重集合各自与before完全一致。失败仍是 `test_diagnostic_registry_assigns_stable_descriptors` 和 `test_diagnostic_message_table_covers_registry_and_falls_back_to_english`，未改写包装器退出1或声称全绿。四源hash在每次运行开始与结束一致，after为 `69b7ffd6…`、`a98ffb0f…`、`e4544397…`、`53d906dd…`；完整SHA在application与validation-closure收据中保存。闭合收据SHA为 `d05b8ed747c3aafe98c768c64e1d124cd8bda09b2dabac5a97126f1e238e8882`，同时核对429个相关源码及89个已接受私有artifact指纹。

提交前重新扫描3555个首方文件，覆盖59个公开接口及6个context内部helper，共1383个直接调用点、124个声明/定义，无函数地址或其它首方语言文本命中。原汇总语料为1380个调用；首次完整扫描发现三个 `.inc` getter 调用不在原FULL角色中，而非仅漏填已有角色的调用字段。subagent07与根代理新增全文阅读三个函数及实际producer/consumer链：弱引用optional调用检查guard的kind/mode；两个动态object用例有意保留来源SymbolId而TypeId=INVALID，不用0查询canonical节点。原1481个不同函数加三项补充为1484；没有将扫描指纹冒充原来的FULL记录。三项调用补入facts实现/声明四行台账，全部新证据使用精确point；130行reason只去除已经过时的草稿状态前缀，语义内容保留。这三项测试没有额外运行，现有208例比较仍是上表限定目标。扫描器的配对头/内部helper区分及 `.inc` 分类失败结果保留，均不是源码BUG/TODO。

此验收针对注释批次的限定目标。共享工作区仍有其他会话的core/SSA源码及测试修改；未把整个旧工作区当作受控基线。本批按59条精确路径正常串行提交，成员和实际Git blob由提交终态收据确认；其它会话的dirty、untracked、submodule和作业保持原状。唯一readiness队列仍为 `01a0e3a1-2bbe-7f23-9026-9e5aa6296c8f`，未收到capture START/terminal/release；模块验证与commit不构成跨仓捕获封印，全仓任务尚未完成。
