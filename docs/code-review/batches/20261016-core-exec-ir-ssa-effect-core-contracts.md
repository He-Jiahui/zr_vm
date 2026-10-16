---
doc_type: code-review-batch
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_invoke_result_availability.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_builder_dominance.c
---

# ExecIR SSA 与 effect/memory 验证阶段契约审查

本批三份物理源码 30 单元：SSA C 十二函数与一私有分析结构，私有头一接口和普通 guard，effects C 十五函数。29 单元补充中文 Doxygen，guard 记录无需重复说明；30 定义范围、27 函数体、89 参数及 23 返回标签已核。原始字节前缀保留，文档在 EOF 追加；1117 处原始含 LF/CRLF 行哈希、文档与 leaf 的 918 引用（392 唯一）逐项核对。逐单元台账为 coverage/zr_vm_core_exec_ir_ssa_effect_core.tsv，38 个现有测试证据配置只作依赖，不重复交付测试单元。

## 进入验证阶段与诊断边界

VerifyFunction 先清诊断并做通用预检，再依所选 level 进入 STRUCTURE、SSA、EFFECT；未选 STRUCTURE 时 SSA 不补齐所有 CFG 互反及终结器约束。私有 SSA 入口不能接收任意 NULL 或畸形池。SSA setter 只写六字段，不清 sourceId 与其它字段；部分调用分支再显式补 sourceId。effects 错误 helper 每次非空调用重建诊断，而公共直接 VerifyEffects 的成功路径不保证清空旧诊断；应以布尔返回判定本轮结果。输出诊断不得覆盖仍需读取的输入，借用只限同步调用期，期间不得修改、重分配或释放输入。

## 分析资源、不变量与跨层协作

SSA 支配记录唯一拥有 reachable、dominators、instructionBlocks 三池，不能通过浅拷贝共享释放责任。分析重新计算入口可达与支配矩阵，不信任 cached immediateDominator；不可达块不作为支配证明。构造失败自清并清零记录，成功由 wrapper 释放；临时 stack/candidate、三张定义表及异常路径遍历的 visited/stack 在取得后的各返回路径释放。异常结果查询的 FALSE 还可能表示 OOM，两处实际调用都检查 outOfMemory 副通道并拒绝，不能单独把 FALSE 当可用。PHI 依有序 predecessor occurrence 检查，正常完成与异常边的结果发布分别验证；cleanup 的特殊豁免不取消前置值 ID 与 owner 形状责任。

effects 先做明列存储形状和标量 PHI 前驱匹配，再借用回边分类，分别计算 memory/effect 终端与 known 事实，最后逐指令核 schema flags 与 token。每个临时数组由所属分析释放，backedges 由公开入口在后续拒绝或最终扫描前释放；没有资源转移给调用方。tagged region 与 legacy 规则按实际分支解释，known 不足时不能夸大为全图完整证明。直接 VerifyEffects 不替代 VerifyFunction(ALL)，后者仍可在附带 stateMap 处失败。未注入的分析分配失败、极限计数及平台分支记为证据范围，不据此新标缺陷。

## 验证证据

修改前三份原始源码逐项匹配实际 BEFORE pin、frozen 及 compile-input 观察；既有 Windows 四个专用目标 build exit0，CTest 4 项全通过，effects/invoke/value/dominance 直接程序均 exit0。198 frozen/3583 observed 输入在运行起止观测稳定。effects 26、value 13、dominance 3 为固定源码调用与唯一成功 footer，不是逐例运行记录或 Unity 计数；invoke 输出 12 条显式 PASS 与 12 cases/0 failures。只把实际可达的 Core 断言用于对应阶段证据，Parser/append 的前置拒绝不证明下游 Core 分支。修改后 strict 核对 10,808 个非注释 token、预处理标志及原行列坐标相同，6 次 lexer 已等待终态。历史 BEFORE R1 与 AFTER R1 四目标通过的证据保留；此后外会话调整 Parser exec_ir_build.c 并新增 storage_internal.h，AFTER R2 build exit0、CTest exit8（3通过/1失败），effects/invoke/value 三个直接程序 exit0，dominance exit1。为取得相同依赖下的原件对照，仅在实际 checkout 短时精确剥离本批三源的 29 个 EOF 注释，完成 fresh BEFORE R2 后在 finally 恢复完全相同的 after 字节；未创建源码副本。原件对照同样 build0/CTest8/三个直接程序0及dominance1，四程序完整 stdout/stderr 与 AFTER R2 逐字节相同。实际 Ninja 四目标 19/15/18/29 个对象、32 份唯一翻译单元、125 个首方输入在两次记录运行的基线/起止观测除本批三源注释外相同，六条执行命令、七份选定构建配置与二十项 cache 条件一致。两次均 200 frozen 稳定；fresh BEFORE 3585 保守输入稳定，AFTER 3584 扫描中的一份 proof.c 变化在实际 125 输入之外。dominance 只证实 test_ssa_builder_dominance.c:71–77 首个复合检查失败，不能据错误文字推断 Build 已返回 TRUE 或具体哪一子条件失败，后续两场景和本地 Free 未达。AFTER R2 早于 fresh BEFORE 执行，最后恢复的 after 原字节已由 R2 实际测试，恢复后未另跑一次。两次运行结束后的 foreign Builder 变化另记，当前外来版本未授运行验收；本批 918 处文档/台账引用不依赖该文件的迁移行号。外部 SDK 头仅作运行后观测，无运行内全哈希证明。最终 source/leaf 和暂存独审另行绑定；不外推全仓、Linux、默认 SDK、隔离行为比较、原子 checkout 或暂存树验收，最终 batch 文字不冒充运行输入。

本批无新增 BUG/TODO，不修改实现、公开签名、断言或构建配置；相关模块既有 Deopt535 测试前提失败不在此修复。源码三份、leaf、batch 和 inventory 三行审核列共六路径。复用登记的 E 盘构建和缓存，不建源码副本或新增重复实现测试；释放 0 B，生命周期开放，必要证据保留。
