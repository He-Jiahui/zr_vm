# Syntax 12 M1: Explicit Task Syntax And Effect

## 目标

实现计划 12 的第一个里程碑：`async fn` 的源码返回类型必须是显式
`zr.task.Task<T>`，`await` 是独立的语法/语义节点，并只接受由 native
descriptor 的结构化 `Task carrier` role 证明的闭合类型。此里程碑只建立
语法、canonical callable effect、await suspension fact 和静态边界；不实现
Task frame、Job、Scheduler、thread provider 或运行时执行策略。

## 已确认边界

- `async` 与 `await` 保持普通标识符上下文关键字；不添加 `task`、`job`、
  `scheduler`、`spawn`、`thread` 或 `coroutine` 关键字。
- 函数调用的真实 TypeRef 仍是源码中的 `Task<T>`。`async` 仅进入 canonical
  callable effect，不能把 `T` 隐式包成旧 `TaskRunner<T>`。
- `await` 不按完整类型名、成员名或错误文本判断。parser 产生 await AST，
  type inference 使用从 native descriptor/import metadata 恢复的
  `ZR_PROTOCOL_ID_TASK_HANDLE` carrier capability。
- `%async`、`%await`、`TaskRunner`、`Async`、`spawn/pump` 的删除和 artifact
  迁移属于设计的 M6；M1 不改变其兼容路径，也不将它们提升为新 contract。
- `Task` 的 public owner 固定为 OfficialNative `zr.task`。source alias 仅在
  import 后解析到该 canonical provider，不允许 `zr.thread.Task` 或同名
  source type 代替。

## Exact Write Set

| 层 | 路径 | 责任 |
|---|---|---|
| carrier descriptor | `zr_vm_library/src/zr_vm_library/task_runtime.c` | 消费既有 `ZR_PROTOCOL_ID_TASK_HANDLE` 登记的 `zr.task.Task<T>` carrier capability；不加入 Job/Scheduler API。 |
| AST/parser | `zr_vm_parser/include/zr_vm_parser/ast.h`、`zr_vm_parser/src/zr_vm_parser/parser/{parser_reserved_task.c,parser_expressions.c,parser_declarations.c,parser_expression_primary.c,parser_function_syntax.c,parser_class.c,parser_struct.c,parser_internal.h,parser_ast_free.c}`、`zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c` | 给 `await` 独立 AST payload；标记 explicit/legacy async named/member/lambda；仅解析 `await expr`，保留 legacy `%await` 现有迁移路径。 |
| semantic/effect | `zr_vm_parser/include/zr_vm_parser/syntax_contract.h`、`zr_vm_parser/src/zr_vm_parser/{syntax_contract.c,type_system.c,compiler.c,type_inference.c,type_inference/cfg.c}`、`zr_vm_parser/src/zr_vm_parser/compiler/{compiler_task_effects.c,compiler_task_effects_internal.h,compiler_task_effects_declarations.c,compiler_reference_escape.c}` | 使用 descriptor/import metadata 的 Task handle protocol 校验显式 Task carrier、async 参数/return 限制和 await scope；由 declaration-derived effect 统一投影 named/member/lambda canonical async effect，并以 await node 产生 suspension/borrow 边界。 |
| tests | `tests/task/test_task_runtime.c`、`tests/parser/test_place_cfg_graph.c` | descriptor role、source alias、AST/source signature、async/await positive/negative、Task payload TypeId 与 suspension CFG edge。 |
| docs | `docs/parser-and-semantics/async-task-syntax-and-effect.md`、`docs/parser-and-semantics/index.md`、本计划、M1 record、`tests/acceptance/2026-07-25-syntax-12-m1-explicit-task-syntax-effect.md` | module contract、完成状态与三工具链验收证据。 |

## 实施步骤

1. **RED: role and syntax contract**
   - 在 task/parser tests 固定 `import("zr.task")` alias 的 `Task<int>` type
     role、`async fn f(): task.Task<int>` canonical effect、`await task` AST。
   - 固定拒绝：async 缺少 Task carrier、`Task` 参数/返回中包含 ref-like value、
     非 async function 的 await，以及同名 source/import fake Task。

2. **Green: descriptor-to-prototype identity**
   - 复用已稳定的 `ZR_PROTOCOL_ID_TASK_HANDLE`，只让 `zr.task` descriptor 的
     `Task<T>` 赋予 Task carrier capability。
   - 验证 native import、source alias 和 generic closing 保留同一 protocol 与
     owner ModuleId，不引入完整类型名 fallback。

3. **Green: parser and canonical effect**
   - `async fn` 保留显式 return AST，不作 return/body wrapper。
   - `await expr` 产生 direct AST node；effect checker、reference escape 和
     type inference 只消费该节点及 Task role。named/member/lambda 的 async
     effect 均由声明派生并投影 `ZR_CANONICAL_CALLABLE_EFFECT_ASYNC`，其 return
     TypeId 不被替换。

4. **Regression and docs**
   - 先跑 focused parser/task targets，随后在独立目录以 GCC、Clang、MSVC
     运行相同目标；每条命令记录真实 process exit 和 Unity summary。
   - 更新 module docs、acceptance 与本 record 的完成时间/状态/产出。
   - 以独立 Git index exact-stage 本表路径，提交一个 M1 commit；不包含
     shared dirty paths、LSP 或未完成后续里程碑。

## 验收口径

- public `Task<T>` 的 owner 和 type role 均来自 `zr.task` descriptor；alias、
  source import 和闭合 generic 的 canonical type identity 一致。
- async callable 的 display/signature 是 `async fn ...: Task<T>`，return
  TypeId 是源码 carrier；不存在 hidden `TaskRunner` wrapper。
- await 的 result 由 closed `Task<T>` 的 payload 得出，非法 carrier 在
  lowering 前失败；它同时是 reference escape 的 suspension point。expression
  statement、return 和变量初始化均有 suspend/resume CFG 边。
- M1 不声称 task/frame runtime、Job/Scheduler 或 legacy deletion 已完成。
