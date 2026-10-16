---
doc_type: code-review-batch
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.h
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_effects_verifier_invoke.inc
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_effects_verifier_invoke.inc
---

# ExecIR 自然回边与 Effect 验证测试契约审查

本批四份物理源码 43 单元：生产 C 三函数，私有头一接口及普通 guard，测试主文件 33 函数和一个文本包含组合块，扩展 .inc 三 helper 和一个 case。42 单元补充完整中文 Doxygen，普通 guard 记录无需重复说明；43 个定义范围、40 个函数体和 2270 处文档/leaf 引用（467 唯一）已核。所有原始字节作为前缀保留，文档在 EOF 追加，相关其他模块只作调用证据。逐单元台账为 coverage/zr_vm_core_exec_ir_effect_verification.tsv。

## 分类器使用前提和输出生命周期

自然回边按全局 predecessor 槽定位，含平行边的 occurrence；可达源块的目标支配性通过排除目标后的反向 entry 绕行判断，不能按块声明顺序判断。调用方须先保证 CFG 块池、1 基 ID、各 range 及加法合法；分类器不替代完整结构/前后继对称验证。entry 无效时的既有实现可成功返回全 FALSE 数组，不能把成功解释为完整 CFG 合法。零 predecessor 直接成功且输出 NULL。

function 和 outBackedges 均须非空，输出槽独立可写且旧分配已由调用方释放，诊断可空且不得覆盖输入/输出槽。函数与各池在同步借用期间保持有效且无并发改写/释放。三个 calloc 与一个 malloc 取得分析暂存，统一失败时释放已取得全部对象，outBackedges 保持 NULL；成功只把 backedges 数组唯一所有权转给调用者，用 free 释放，其余暂存即时释放。诊断只在错误分支重建，成功不保证清空旧值；公共包装同样不补足非法输入检查。

## 测试入口与断言边界

主文件在 ok/new_function 后、main 前文本包含 .inc；.inc 复用同 TU static helper，物理定义各计一次。module 唯一拥有函数与池，返回 function 是其数组元素借用，数组迁移/FreeModule 后失效。局部浅拷贝只是只读/受控变异视图，恢复共享字段后才释放 owner。普通 ok 失败会 fprintf 并 exit(EXIT_FAILURE)，跳过尾部恢复、释放及后续 case；没有 Unity/tearDown 恢复契约。

main 先运行 .inc 的 INVOKE memory 场景，再运行主文件 25 场景，全部返回才输出唯一 PASS footer。覆盖有限的 effect/memory phi 合流和回边、区域版本、schema、范围与有序平行边、五个 SSA 定义/支配/异常可用性场景；五个仅 STRUCTURE|SSA 的场景不能算作 EFFECT 分支覆盖。每例只认源码明确求值的诊断字段和短路条件，不推断前置拒绝之后的分支或未注入的分配失败。INVOKE 场景核两个后继与两区域的版本、错误来源和受控浅借用恢复。

## 验证证据

修改前四源码 raw 哈希、frozen pin、compile-input 观察逐项相符，Windows 既有专用目标 build exit0、CTest 1 项通过、直接程序 exit0；90 frozen/3583 observed 输入在运行内观测稳定。修改后 strict 核对 12,328 个非注释 token、预处理标志和原行列坐标一致，8 次 lexer 均已等待终态。AFTER R2 build exit0、CTest 1 项通过、direct exit0，完整 stdout/stderr 与 BEFORE 逐字节相同：27 字节 stdout 为 ssa effects verifier PASS 后跟 CRLF，stderr 为空。26 个无条件源码直调加成功出口提供有限场景证据，没有逐例运行输出或 Unity 计数。R2 的 92 frozen 输入保持一致，但保守 3583 文件扫描有两份 LSP 测试头变化；Ninja 实际目标为 19 个 C 编译单元，含依赖头及 .inc 共 70 份首方输入，这 70 份在 R2 前、运行起止和闭包核查时哈希一致，变化文件均不在目标依赖中。BEFORE 至 R2 的实际首方依赖变化恰为本批四份 EOF 注释文件；R1 的无关 Parser 文件漂移及两次广域扫描差异均保留。仅接受此专用目标的有限证据；不声称全仓稳定、SDK 全量哈希稳定、区间原子性、隔离行为比较、Linux、默认 SDK 或暂存树运行验收。最终 source/leaf 与暂存独审另行绑定，batch 最终文字不冒充已运行源码输入。

本批无新增 BUG/TODO，不修改实现、公开签名、断言、测试列表或构建配置。源码四份、leaf、batch、inventory 四行审核列共七路径。相关模块的既有失败与其他会话修改不并入。本批复用登记的 E 盘构建与缓存，不建源码副本或重复实现测试；释放 0 B，生命周期开放，必要证据保留。
