# Reference Place access and out flow

本文说明 syntax plan 02 M2 建立的引用调用 Place 限制和 `out` definite-assignment
边界。M1 保存的 parameter passing contract 与 call marker 是唯一调用契约，Place 分类和
控制流状态负责静态验证；VM 与 AOT 不增加运行时 borrow table 或动态兜底。

## Call boundary

`ZrParser_PlaceExpression_Classify()` 将可寻址表达式归为 local、field 或 index Place。
标识符是 local Place；只含 member/index projection 的 primary expression 保持为 Place；
函数调用、算术表达式和其他 rvalue 返回 invalid。解引用仍由 Place graph 的
`ZR_PARSER_PLACE_PROJECTION_DEREFERENCE` 表示，和 field、dynamic/constant index、tuple、
union projection 使用同一 overlap 模型。

调用契约逐参数验证：

- value/in 参数不接受显式 marker。
- ref/ref readonly/scoped ref 参数必须使用 `ref`，且实参必须是可写或可寻址 Place。
- out 参数必须使用 `out`，且实参必须是 writable Place。
- marker 不允许省略后由 overload resolution 猜测。
- named argument 先按参数名映射回目标参数，再匹配 passing contract；源码实参顺序不参与
  契约身份。

类型推断路径和字节码调用编译路径执行同一规则。后者不能成为绕过 marker/Place 验证的
降级入口。

## Out state

每个 out 参数在函数入口为 uninitialized。标量使用一个状态槽；source struct 按必需的
非 static 字段建立状态槽。整值赋值初始化全部槽，字段赋值只初始化对应槽。读取整值或
未初始化字段、compound assignment 对旧值的读取，以及把未初始化值传给非 out 参数都在
入口状态上报错。

正常控制流采用 definite-assignment 交集：

- if/else 在所有继续执行的分支上取交集。
- 条件循环保留零次迭代入口；`while (true)` 只从实际 break edge 汇合退出状态。
- normal return 和函数体 fallthrough 要求所有 out 字段均已初始化。
- throw edge 不要求 out 初始化，也不向调用者承诺新值。
- try 正常路径与 catch 正常路径取交集；catch 从调用可能抛出前的状态进入。
- `callee(out value)` 只在调用正常返回后把对应 Place 标为 initialized，从而转移
  definite-assignment 责任。

返回 `false` 仍属于 normal return，不改变上述要求。需要“失败时无值”的 API 应返回
`Option<T>`，不能借异常或布尔返回绕过 out 初始化。

## Module boundary

参数/字段状态建模与入口位于 `compiler_out_definite_assignment.c`，表达式和 CFG-like
语句传播位于 `compiler_out_definite_assignment_flow.c`，共享内部类型位于对应 internal
header。旧的整参数布尔扫描已从 `compiler_generic_semantics.c` 移除。

M2 不创建 LoanId，不实现 shared/mutable loan、reborrow 或 NLL。这些属于 M3；新增借用
规则必须消费 canonical contract、Place overlap 和 CFG facts，不得恢复基于旧 passing
mode 的并行语义系统。
