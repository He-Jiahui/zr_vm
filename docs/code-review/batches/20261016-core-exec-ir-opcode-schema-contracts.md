---
doc_type: code-review-batch
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_builder_iterator_invokes.c
  - tests/parser/test_ssa_builder_control_edges.c
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_effects_verifier_invoke.inc
  - tests/cmake/ssa-builder-tests.cmake
  - tests/cmake/ssa-tests.cmake
---

# ExecIR opcode 记录与包含协议审查

本批仅完成 exec_ir_opcode.def 的35条人工编写数据记录和1个独立包含协议块，共36个物理单元；九列表为 coverage/zr_vm_core_exec_ir_opcode_schema.tsv，均为commented，无未审单元、新增BUG或TODO。宏调用记录不计作函数或宏定义，相关公开枚举、Core静态表和查询函数是借用证据，不重复授予实现审查信用。

原5996B、49LF前缀完整保留，EOF追加59471B、981LF中文文档。1段@file与35段@anchor/@par说明各字段、调用意图、约束、资源边界与引用；六个字段没有伪装为函数参数或返回值。文档与已独审R2逐字相同，台账前8列复用已接受提案，reason列仅将候选状态更新为实际应用状态。没有改签名、程序token、断言、测试、构建配置或行为。

## 协作、不变量与生命周期

调用者先定义六形参ZR_EXEC_IR_OP，文本包含后自行#undef；公开H借此生成自动编号枚举，Core C借此生成静态元数据和名称。本文件故意没有include guard；行序影响编号，不能根据某个映射忽略字段就放宽其它消费者的要求。已有记录不表示所有后端都支持执行。

operandArity是上界或VARIADIC，Core将变长下界设为0，并专门将RETURN下界设为0，因此RETURN是0..1。结构验证仍独立检查实际池范围、值及CFG等条件。内存类别位集、schema flags、instruction.flags和派生effects分域；零内存token方向不等于没有副作用，MAY_GC没有独立的通用effect位。元数据表与名称静态持有，查询只返回借用；记录展开自身不分配运行期资源，也不承担函数、指令或provider的释放。

完整当前字面检索2460处、205个程序文件；其中header probe的两个sizeof(&函数)只是未求值地址，不算调用。B(op) token拼接另有11个间接常量展开，ADD5、SUB2、MUL4，与字面出现分列。225个完整依赖pin已经包含本def，2798个含EOL行pin支持定义、调用、构建与测试。R1到R2的69个依赖及行坐标变化已有可逆修订和独审记录，未把旧坐标偶然相同文本当作原位置未变。活跃SSA的public/private接口迁移仍是只读依赖。

## 既有测试与验收边界

三个既有metadata/effect目标分别提供迭代器元数据、异常payload形状、CALL/INVOKE memory token及DIV所需throw flag的有限证据。它们检查部分字段和验证失败，不证明全部35条记录的全部字段、每个后端或真实指令执行。普通C check失败可退出进程并跳过后续局部释放；直接运行完整流按目标比较，不换算成Unity或逐case跟踪。

复用E盘Windows Clang19.1.5 Debug构建。Numeric11历史AFTER之后的一处SSA检查脚本变化已纳入本批fresh BEFORE：225个锁定输入和3595个有限观察输入，不声称完整依赖闭包、SDK/配置、原子checkout隔离、Linux或默认SDK验证。首次控制器把已含目标文件的225依赖重复加为226，计数断言在注册、产物创建、构建和源码写入之前终止；修正后的实际BEFORE独立保存，失败控制器不提供运行证据。

严格词法比较通过，原程序 token、flags 与原坐标一致；两次 stdin raw lexer 完整消费输出并退出0。三个既有 Opcode 元数据/效果目标 BEFORE/AFTER 构建均exit0、CTest均3/3、三个直接main均exit0；各目标完整stdout/stderr逐字相同，五个argv、20项cache及三个实际源编译命令相同且无NDEBUG。225个锁定输入、3595个有限观察输入在各次运行首尾稳定，前后仅本def的36段EOF注释变化。实际source/leaf与stage独审、串行提交仍待完成。
