---
doc_type: code-review-batch
related_code:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.h
  - tests/library/test_native_plugin_shadow_io.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - tests/cmake/native-plugin-shadow-io-tests.cmake
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.h
plan_sources:
  - .codex/sessions/20261016-review-bugfix.md
tests:
  - tests/library/test_native_plugin_shadow_io.c
  - tests/cmake/native-plugin-shadow-io-tests.cmake
---
# Plugin 影子副本 I/O 契约审查

## Summary

范围为三个首方文件的20个单元：11个函数体、2个声明、3个宏、3个状态和1个关闭语句块。逐单元职责、调用者、约束与无需重复注释的理由记录在 coverage/zr_vm_library_native.tsv；本批核查全部可检索直接调用、allocator 回调、平台条件编译和 loader 间接入口。13项已注释、2项保留同一旧并发 TODO 的镜像、5项无需新增注释，无未审查单元。6旧键迁移到实际定义并更正合同，14新键补齐遗漏，累计交付计数只增加14。

新增15处 Doxygen，按适用内容说明 Summary、Parameters、Returns、Constraints and lifecycle、References；shadowCounter 使用 Value model 描述静态状态。注释仅追加于EOF；原始程序字节前缀、所有非注释token及原行列保持。公开签名和行为未修改。

## 调用及资源边界

生产链为 native_binding_registry_plugin.c:219 调用 prepare，prepare:124 选择临时缓存目录、:152 调用路径复制，路径层:101 将两流交给 owning-stream helper。loader 上游 native_binding_registry_plugin.c:404、:509 以及 test_native_registry_oom.c:227、:242 是间接入口；本批未运行 OOM 入口，不借该文件名授予失败分支测试信用。成功副本由 registry 装载失败分支或 handle record 生命周期尝试删除；复制层 FALSE 不承诺目标不存在，remove 的错误未汇总。

copy_plugin_streams 接管并分别关闭两个 FILE；任何一次关闭失败也继续关闭另一流，返回后不再允许调用者使用或重复关闭。它不拥有路径、不做 fsync/映像验证。路径组合与临时目录是有限容量与共享命名协议，不提供独占创建或目录认证。shadowCounter 为进程级非原子静态序号，调用前需外层串行化；是否在多状态装载入口已满足这一约束仍为原 TODO，两个串行场景未验证并发。

测试 CHECK 失败立即从 int helper 返回1，可能跳过其后的本地清理；正常结束的有限夹具观察不能推广为任意断言失败或全宿主无泄漏。POSIX 分支、SIGPIPE、源读错误、短写、源 fclose 失败和全部删除错误均仅静态审查。

## 逐单元定义、入口与测试证据

下表 C 为 native_binding_plugin_shadow_io.c，H 为同名私有头，T 为 tests/library/test_native_plugin_shadow_io.c，P 为 native_binding_registry_plugin.c。完整文件路径与每项契约见九列叶表。OF 为 open_failure，CF 为 target_close_failure；“间接链”只表示静态可达，不证明分支执行。

| 单元 | 定义 | 调用者或入口 | 有限测试证据及限制 | 结论 |
| --- | --- | --- | --- | --- |
| native_registry_ensure_directory_exists | C:6–20 | C:49 (direct call) | OF间接链；EEXIST类型复验、递归创建及目录认证未测。 | commented |
| native_registry_get_plugin_shadow_cache_directory | C:23–50 | C:124 (direct call) | OF间接链；bufferSize仅拒0，PathJoin固定容量；无分支trace。 | commented |
| native_registry_copy_plugin_streams | C:53–78 | C:101 (direct call); T:102 (direct test call) | CF直接调用helper返回FALSE；NULL/流别名、路径删除和持久化不在证明内。 | commented |
| native_registry_copy_binary_file | C:81–106 | C:152 (direct call) | OF间接链；CF未调用此路径层，不能证明其remove(target)分支。 | commented |
| native_registry_prepare_shadow_plugin_path | C:109–153 | P:219 (direct call) | OF经load入口，未追踪影子成功/原路径回退分支；外层串行约束待核实。 | TODO |
| native_registry_prepare_shadow_plugin_path::shadowCounter | C:113 | C:136 (read/write preincrement in enclosing function) | OF不证明计数增量发生；进程静态无锁/原子，无并发/回绕试验。 | TODO |
| native_registry_copy_plugin_streams::close_failure | C:69–75 | C:101 (copy helper entry); T:102 (test drives target close failure) | CF前置fwrite成功、fclose为EOF及helper FALSE；源关闭失败、短写未测。 | no-comment |
| native_registry_copy_plugin_streams@declaration | H:10 | C:101 (direct call); T:102 (direct call) | CF实际消费接口；返回后两流失效，路径清理由调用方负责。 | commented |
| native_registry_prepare_shadow_plugin_path@declaration | H:13–16 | P:219 (direct call) | OF间接消费；不证明回退选择或自动删除。 | commented |
| ZR_VM_LIBRARY_NATIVE_BINDING_PLUGIN_SHADOW_IO_H | H:1–2 | C:3 (literal header include); P:3 (literal header include); T:22 (literal header include) | 当前Windows编译可见性；常规include guard，无运行资源协议。 | no-comment |
| _POSIX_C_SOURCE | T:3 | T:16 (subsequent POSIX includes); T:64 (fdopen use) | 仅POSIX静态审查；Windows构建不执行该宏分支。 | no-comment |
| CHECK | T:25–30 | T:81 (callable macro expansion; early-return int helper); T:82 (callable macro expansion; early-return int helper); T:83 (callable macro expansion; early-return int helper); T:84 (callable macro expansion; early-return int helper); T:88 (callable macro expansion; early-return int helper); T:90 (callable macro expansion; early-return int helper); T:91 (callable macro expansion; early-return int helper); T:92 (callable macro expansion; early-return int helper); T:94 (callable macro expansion; early-return int helper); T:101 (callable macro expansion; early-return int helper); T:103 (callable macro expansion; early-return int helper); T:104 (callable macro expansion; early-return int helper); T:156 (callable macro expansion; early-return int helper); T:158 (callable macro expansion; early-return int helper); T:159 (callable macro expansion; early-return int helper); T:161 (callable macro expansion; early-return int helper); T:162 (callable macro expansion; early-return int helper); T:163 (callable macro expansion; early-return int helper); T:165 (callable macro expansion; early-return int helper); T:166 (callable macro expansion; early-return int helper) | OF/CF进程均0；未做逐CHECK trace，早退可能跳过本地清理。 | commented |
| test_allocator | T:32–42 | T:164 (allocator function pointer passed to GlobalState_New) | OF普通allocator回调；realloc失败保留旧块属静态合同，未注入OOM。 | commented |
| broken_pipe_writer | T:45–68 | T:80 (direct call); T:95 (direct call) | CF Windows断管前置见证；HANDLE→descriptor→FILE；POSIX及关闭清理错误未测。 | commented |
| test_target_close_failure | T:70–106 | T:209 (ordinary main selector dispatch) | CF正常返回0且源夹具remove成功；第二对流由helper消费，无自动tearDown。 | commented |
| test_target_close_failure::payload | T:71 | T:83 (preflight fwrite bytes); T:91 (fixture source fwrite bytes) | CF用同一payload写入；static const，sizeof−1排除末NUL，不转移资源。 | no-comment |
| shadow_file_count | T:109–141 | T:159 (baseline cache count); T:178 (after retries cache count) | OF观察before=0/after=0；Windows排除目录，POSIX只比较前缀，不证明内容或全泄漏。 | commented |
| test_open_failure_removes_shadow | T:143–196 | T:208 (ordinary main selector dispatch) | OF三次拒载，module/handle长度保持观测基线、FindModule NULL、计数与源字节保持；非成功插件/OOM/所有loader错误证明。 | commented |
| test_open_failure_removes_shadow::invalidImage | T:144 | T:162 (invalid fixture fwrite); T:188 (observed byte count); T:189 (source content memcmp) | OF回读与源字节比较排除末NUL；不证明所有装载诊断。 | no-comment |
| main | T:198–211 | tests/cmake/native-plugin-shadow-io-tests.cmake:21 (CTest executable command; OS main dispatch) | 同EXE的OF/CF selector各实际exit0；未知selector返回2、POSIX信号分支仅静态，超时由外层提供。 | commented |

## 原有 BUG 与 TODO 的核对

旧叶表把关闭错误记录在 copy_binary_file::close_failure，实际关闭逻辑已在 copy_plugin_streams:69–75 分别检查两个 fclose；故迁移至真实 helper 块并记录 no-comment。本轮没有修复代码，不把既有关闭错误修复记为当前成果。保留的一个并发 TODO 在 prepare 函数与 shadowCounter 状态各有台账镜像，核查入口 P:219、C:113/136/145；无新增 BUG 或 TODO 问题。

## 构建及现有测试

使用实际 checkout 和既有 E:/cargo-targets/zr_vm/build/first-party-review-01a13463/clang-cl-products-debug；Windows ClangCL/Debug/Ninja，同配置、工具、环境与目标的 BEFORE-r4/AFTER-r4 逐对核对。WSL 入口此前为 Wsl/Service/CreateInstance/E_ACCESSDENIED，本批未获 WSL/POSIX 执行证据。BUILD_DEBUG_LIB/RUST/HOST_JIT/WASM 仍OFF，不为当前批切换其他功能。

当前配置链 tests/CMakeLists.txt:806 include tests/cmake/native-plugin-shadow-io-tests.cmake，link helper 定义在 tests/CMakeLists.txt:233。构建目标 zr_vm_native_plugin_shadow_io_test；现有两个 CTest 名为 native_plugin_shadow_io_open_failure 与 native_plugin_shadow_io_target_close_failure，配置TIMEOUT30。本批实际直接运行该EXE加两个原selector和各自独占workspace，未声称调用ctest。

| 阶段 | 构建 | open_failure | target_close_failure | 外层实际终态 |
| --- | --- | --- | --- | --- |
| BEFORE-r4 | PID 46312，exit0 | PID 9584，exit0 | PID 44556，exit0 | Root52076/session97283/a3407f，actual0 |
| AFTER-r4 | PID 42764，exit0 | PID 8804，exit0 | PID 51456，exit0 | Root16832/session43534/d3ddd0，actual0 |

每阶段实际3个native child与4个只读Ninja query全部等待终态，共7个direct children，无超时。OF观察影子条目前后均0；CF以真实断管流证明缓冲写成功而关闭失败，并检查生产helper返回FALSE。两个stdout为空、stderr保留预期诊断；不要求stderr为空。现有测试未新增或修改断言。selected1743首方中的变化严格限于三份批准EOF注释，非owned严格输入、配置、9工具、3环境helper与最终runtime输入在各阶段截止点稳定；该输入核对不是二进制逐字节相同声明。

## 验证过程中的有限失败记录

旧before-r2构建和四query已成功，但DOT parser把Ninja多输出零输入phony的未消费输出sink误当缺失节点，selector未运行。最小验证工具修订仅接受精确manifest行与205输出中的2已labelled/203未消费sink，9个RAM正负用例及独立复核通过，其他缺端点仍拒绝；随后真实BEFORE-r3重新执行成功；历史AFTER-r3因下述DCE漂移未执行。最终BEFORE-r4与AFTER-r4重新成对执行通过。旧失败原件保留，不当作产品BUG或已执行测试。

历史r3第一次三EOF写入后，原wrapper因git diff --check行尾提示而退出1，该Git命令本身exit0；只读恢复证明当时的原前缀加批准suffix。启动旧AFTER-r3前又发现SSA的DCE真实程序变化，故旧AFTER未执行；本任务只截去自己的三个注释后缀以恢复精确原始前缀，回退后校验同样遇行尾提示并由只读receipt核实。新metadata5把当前DCE版本显式加入44项strict；真实BEFORE-r4通过后再次追加同一批准注释并完成AFTER-r4。历史原件保留，DCE变更归SSA，不当作本批注释成果或隐藏为仅注释差异。

## Evidence references

报告物理根为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463；以下报告及SHA可复核，未复制源码树。
- native-plugin-shadow-io-current-contract-review-r1.json，SHA256 e25e8d0a0096b2f3e242a5aa52b5b4597ba6276987b6d5ab77d9f345f1bed7d6，775319B。
- native-plugin-shadow-io-current-contract-review-r2.json，SHA256 54fef76afbc56d6da7f3144560c262c197b8ef4a2f358d098f3476b4947850df，13809B。
- native-plugin-shadow-io-current-contract-review-r3.json，SHA256 69d10950429ed4d9d735507632ce58865527d2686820484be1b843fbb1c5659b，37499B。
- native-plugin-shadow-io-validation-before-r4-results.json，SHA256 0048507a16974df81519ee11c5dcd188f2c61e293bfae9c60d9583fa12aa2963，745746B。
- native-plugin-shadow-io-validation-after-r4-results.json，SHA256 d57e2876626480e4a5585f30d0f24b202fa9dd7e66c7bff588858c2e080fa0a3，745414B。
- native-plugin-shadow-io-final-acceptance-r4.json，SHA256 398a7f465c73f2ebf4b333b8504be6e1b4669631388ee573ae20bb747407cd2b，8904B。
- native-plugin-shadow-io-comment-equivalence-r4.json：SHA256 4f4888c18bc0d876433ea4086e3c06077bf8d42cc371b4ffa258259f05519e90。
- native-plugin-shadow-io-independent-contract-review-r3.json：SHA256 d2e5a604ac6e8d07cd992f3097105deee3ed6f62eb9b0e0467da4735927d4d91。

## 生命周期

所有本批direct children已等待实际终态；报告和失败原件保留，构建目录复用，既有临时workspace保留待明确归属清理。生命周期OPEN，本批人工释放0B。此前自动清理拒绝不重试或绕过；不宣称全部产物已关闭。全仓目标仍有待审文件，本批仅结清此三个文件与20单元。
