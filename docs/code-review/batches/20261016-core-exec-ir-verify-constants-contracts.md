---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.h
  - tests/parser/test_ssa_module_constant_pool_verifier.c
  - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_module_constant_pool_verifier.c
  - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc
doc_type: code-review-batch
---

# ExecIR 模块常量引用契约审查 — 2026-10-16

## 完整范围与入口

四份物理源码17单元：生产C两函数、私有头一接口及普通guard，测试夹具一结构及12函数。16项追加中文Doxygen，guard无需重复注释；17定义span、14函数体和562处文档/台账引用（160唯一）已核。详见 coverage/zr_vm_core_exec_ir_verify_constants.tsv；测试C文本包含cases.inc，八个实际定义各计一次。语句块随所属函数审查，未将引用文件或编译实例重复计数。

VerifyModule先验证模块结构，再逐函数执行VerifyFunction(ALL)，随后补充本模块的非空常量池引用校验；这是私有助手安全读池的前提。当前唯一生产直调用点在exec_ir_verify.c:711；公开VerifyModule调用方的生产/测试入口逐项核过，未把文档引用当程序调用。零大小模块池立即成功，不限制layoutId或类型，也不验证另配的Oracle池。非空池先核layoutId上界，再比较池项与结果typeToken，最后比较非零显式指令typeToken；不解释flags/bits。

## 所有权、失败与时序

函数与模块及被读取池在同步调用期间须保持有效、匹配且不并发修改或释放；私有接口不重复验证任意悬空指针、损坏区间或ValueId。自身不分配、不释放、不保留输入借用。诊断可空；非空槽须独立可写、与输入存储分离。助手只在失败时整体重建诊断，成功保留旧内容；公开入口的先清诊断行为另有明确边界。错误位置用0基数组下标形成1基instructionId，block取首个覆盖项。

测试夹具唯一拥有整个模块，function仅借用module.functions元素；模块函数数组迁移或FreeModule会使借用失效，不得按位复制出两个所有者。构造先清零，必须传未拥有资源或已释放的记录。expect_true失败直接exit(EXIT_FAILURE)，会跳过当前尾部释放及后续场景；没有Unity longjmp或tearDown保障。正常场景各自FreeModule，外部Oracle场景还先Free执行结果。

## 有限测试证据

普通main按固定顺序直调七场景：有效池索引、count排他边界、UINT32_MAX索引、池项类型不符、显式指令类型不符、函数/零池边界，以及外配Oracle SIGNED73。索引助手与两种类型失败核结构化诊断字段；成功场景不冒称验证了诊断或所有token/flags/bits。main只在七次调用全部返回后输出专用成功标记，不打印逐例结果。

修改前实际四源码原哈希与frozen及compile-input观察逐项一致；现有专用目标build0、CTest1成功，直接程序exit0及唯一footer已实测，84frozen/3553observed在运行内稳定。相对前次观察有3文件变化、1文件新增并另记证据。七条源码直调与成功出口合起来构成有限场景证据，没有Unity计数或七条逐例PASS输出。修改后实际strict通过：四源码共1,698个非注释token，kind/spelling/StartOfLine/LeadingSpace与原始物理坐标均一致。AFTER首轮build0、CTest1/direct0及完整成功输出与修改前相同，86frozen稳定，但长构建期间5个foreign FS观察文件发生变化，故该轮保留为INCOMPLETE/RETRY_REQUIRED，不作为最终稳定验收。随后在当前输入补跑同一现有目标，最终R2 build0、CTest1及直接exit0，stdout/stderr与BEFORE完整字节相同，唯一成功标记出现在七次原有源码直调之后；86frozen、3,561个保守观察输入在该轮内全部稳定。相对原BEFORE有19文件变化、8文件新增，逐项另记，不能称整个checkout隔离。七场景没有逐例打印或Unity计数；CTest重复同一程序不另计场景。只在运行结束后更新本批验收段，四源码与leaf未改；源码/台账独审、实际暂存独审及串行提交仍待完成。不外推全仓、Linux、默认SDK或暂存树执行。

本批无新BUG/TODO，不改实现、公开签名、断言或构建配置。另一个Deopt目标原基线28例27PASS1FAIL535，未在本批重跑或修复。

## 产物边界

只修改四源码注释、模块leaf/batch及inventory四行审核列，共7路径。复用已登记E盘产物，不建源码副本或新增测试。生命周期open、释放0B；其他会话内容和必要失败证据保留。
