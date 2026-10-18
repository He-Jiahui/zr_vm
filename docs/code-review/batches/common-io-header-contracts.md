---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io/io.c
  - zr_vm_core/src/zr_vm_core/io/io_runtime.c
  - zr_vm_core/src/zr_vm_core/io/io_call_binding.c
  - zr_vm_core/src/zr_vm_core/io/io_source_free.c
  - zr_vm_core/src/zr_vm_core/io/io_bytecode_verify.h
  - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer.c
  - tests/harness/path_support.c
  - tests/cmake/audit-regression-tests.cmake
  - tests/cmake/close-proxy-tests.cmake
  - tests/core/test_io_validation.c
  - tests/module/test_close_proxy_legacy_patch.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
plan_sources:
  - "user: 全仓首方全量审查、按需补中文契约注释；保持程序行为和公开 API。"
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_io_validation.c
  - tests/module/test_close_proxy_legacy_patch.c
doc_type: module-detail
---

# IO 公共标记与二进制布局契约审查

本批全量复核 zr_io_conf.h 的80个单元：61个代码单元、19个人工协议块。旧61键由60个代码单元和一个FUNCTION草图组成；新增19键由ARRAY_INITIAL_LENGTH宏和18个协议块组成，不称为19个新函数。36项已注释、3项TODO、41项有证据地无需追加注释，39段中文Doxygen覆盖职责、使用时机、必要约束、不变量、资源生命周期和引用。完整定义、调用、入口、测试证据及逐项结论见[九列表](../coverage/zr_vm_common_public_codegen_contracts.tsv)。

writer当前写出patch45，reader顶层接收16..45。各patch常量只控制对应的读取、verifier或metadata条件，常量存在不表示每个旧版本或opcode均已验证。头部记录并检查当前原生宽度及端序，TZrSize的实际标量读写不因SIZE_T_SIZE宏为8而自动可移植；本批不将现行zro描述为跨宿主通用格式。模块与成员的ENUM标签、完整枚举与单enumField、增量capture表与递归CLOSURE函数子树分别解释。当前writer部分generic/member/import/declare为零计数或stub，不承诺任意完整模块往返。

读回调缓冲区在其读取窗口内借用；ReadSourceNew普通路径复制为拥有的有界native缓冲区。字符串创建同步复制，GC字符串和值的生命周期不同于native源树释放；空字符串可返回NULL，必须结合长度、上下文和hasReadError解释。普通NULL/FALSE状态与Throw或Unity非局部退出分开，尾部释放、close和事务回滚只按实际路径说明。

保留两个既有TODO家族：COMMON-IO-MEMBER-STATUS-ENCODING用于成员状态编码，核查io.h状态字段、reader实际读取及writer分支；COMMON-IO-LAYOUT-SKETCH用于旧FUNCTION/CLOSURE速记与实际递归字段、版本顺序的差异，核查io_read_function和writer函数。3个TODO台账行不等于3个问题。源文件TODO:字面数2→5是既有问题的文档镜像，宽词TODO为2→6，BUG:仍为0；新增问题身份0。缺陷只记录，不修改行为。

应用本批注释前的调用补证覆盖3975成员、74物理标识符的199次文本出现，其中192个C代码位置、4个C注释位置、3个非C文本位置。定义、预处理、类型/枚举使用不能自动算函数调用；别名/view及协议块的零独立拼写不用于判定死代码或仓外无入口。113个Doxygen引用属于208个正式表与文档引用并集，另核104个定义/member范围。caller独审仅将旧代码计数int61更正为60，保留原报告，其他合同及文档未作隐藏改动。

源码只更换19处旧完整注释的首行，将草图标明为历史速记，并在EOF增加39段批准Doxygen。原298行的行号及EOL保持，307个程序/字符串词元的完整拼写、原行和UTF8字节列一致；有限正则模型不冒充Clang/AST证明。原始9973B前缀和绝对offset不保持：19个owned旧引用行原文改变、49个仅offset迁移、13个原文与offset保持，127个外部引用另守原文/EOL/offset。先去除精确40253B后缀，再逆向19行替换，恢复原9973B原件。最终51748B、1075CRLF；公开声明和程序行为保持。

ObjectAF桥仅核上一批实际消费者端点及当前源状态，不充当IO decoder修改前基线。本批另冻两组既有目标zr_vm_io_validation_test和zr_vm_close_proxy_legacy_patch_test的实际当前依赖，包含Parser；修改前后均实际构建并运行原EXE，无selector参数、无新增或削弱测试。3个Unity PASS ID及1+2的零失败footer完整保留。IO120秒为本任务外部watchdog，原CTest未指定TIMEOUT；legacy30秒来自实际CTest定义。对象文件、archive、EXE、PDB的每阶段实际身份重新冻结，不要求跨阶段二进制字节相等。

验证IO合法writer输出及已有错误头、版本、长度、截断拒绝；legacy核既有opcode编号和patch43/未来46 gate。拒绝分支在TryRun中被断言，正常测试进程仍预期0，不当作子进程失败。有限3项不授全部16..45兼容、全部opcode、OOM、线程或全平台覆盖。

测试固定写出io_validation.zro与close_proxy_legacy_patch/bin/simple.zro；修改前四个已登记路径必须不存在，修改后仅可复用实际BEFORE留下的精确空父目录。IO普通remove未检查、Unity提前跳转可能绕过清理；legacy先检查remove再释放，失败残留保留证据，不清理后重跑。共享tests_generated不归本任务整体所有。TEMP/cwd不改变编译期固定输出路径。

证据：

- common-scalar-io-gc-config-current-contract-review-r1.json与原独审19项typed更正：本批80合同及39文档。
- common-io-header-current-caller-supplement-r1.json及sole独审e25c57f0f2075011e7a6693b4b9d1dd4d0011cd3fdbe55de493fc8794e081a61：当前调用、引用及精确计数更正。
- common-io-header-ObjectAF-consumer-baseline-bridge-r1.json：前一模块实际端点与当前源状态；不授本批decoder运行信用。
- common-io-header-source-comment-application-r1.json：19整行注释、39Dox、精确逆向与208引用迁移。
- common-io-header-native-before-r1和after-r1的frozen plan、唯一driver独审、results/process/selected-input-closure及Root真实outer终态。
- common-io-header-native-final-acceptance-r1.json：f11709fe827c516769bf8749b2d19a3026981cfe7aef7f187e1a20e29ff00490，只接受本批有限注释配对，不把静态编译或历史失败转成未运行项通过。

check-batch与暂存空白检查实际成功；全仓原有相同失败不称全仓通过。共享leaf仅更新61旧键并追加19键，194条外围物理行保留，其他模块不重计。只提交本头、所属台账、本批说明、inventory三格和导航。仅Windows clang-cl/MSVC Debug有限证据，无Linux/WSL/CTest或跨平台通过结论。产物生命周期OPEN，保留复用E构建与必要失败证据，源码副本0，手工释放0B。
