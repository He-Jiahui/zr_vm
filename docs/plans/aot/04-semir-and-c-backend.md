---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 AOT 转为真实 C 元素操作（= / memcpy / +-*/）/ 减轻对解释器依赖 / 解释器与纯 C 双执行
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
references:
  - lua/hybridclr/libil2cpp/codegen/il2cpp-codegen.h
---

# 04 · SemIR 统一中间层 + 纯 C 后端降级规则

承接差距 G1/G3/G7。目标：确立 typed SemIR 为解释器与 C 后端的**唯一共享中间层**，
并给出每条 typed opcode 的**纯 C 降级规则**，使 AOT 产物是真实 C 元素操作而非 VM 调用。

## 1. SemIR 作为单一中间层

```
            源码 / AST
                │  编译 + 类型流分析（03）
                ▼
        typed SemIR  ──────────────┐
         (function.h)               │
        ┌──────────┴──────────┐     │
        ▼                     ▼     ▼
  解释器 typed 快路径      纯 C 后端    dynamic 字节码（回退）
  (执行 SemIR)          (04 降级)     (现有 251 指令)
```

- 解释器与 C 后端消费**同一份** SemIR + 同一份 layout（不变量 C），保证语义一致（差距 G7）。
- dynamic 部分继续走字节码；typed↔dynamic 经 deopt/bridge（`05`）衔接。

## 2. 纯 C 降级规则表（核心交付物）

每条 typed opcode → 固定 C 模板。`sN` 表示槽 N 的 C 局部（类型由 `semIrTypeTable` 决定）。
**所有模板零 VM 调用**（不变量 B）。

| SemIR opcode | typeTableIndex 例 | 生成 C |
|--------------|-------------------|--------|
| `CONST` | i64 | `s2 = (TZrInt64)1234;` |
| `ADD` | i64 | `s2 = s0 + s1;` |
| `ADD` | f64 | `s2 = s0 + s1;`（s* 为 `TZrFloat64`） |
| `SUB/MUL` | u64 | `s2 = s0 - s1;` / `s2 = s0 * s1;` |
| `DIV/MOD` | i64 | `if (ZR_UNLIKELY(s1==0)) { /*deopt 或 throw 点*/ } s2 = s0 / s1;` |
| `AND/OR/XOR` | u32 | `s2 = s0 & s1;` 等 |
| `SHL/SHR` | i32 | `s2 = s0 << s1;` |
| `NEG/NOT` | i64 | `s2 = -s0;` / `s2 = ~s0;` |
| `EQ/LT/...` | f64 | `s2 = (TZrBool)(s0 < s1);` |
| `CONV` | i32→f64 | `s1 = (TZrFloat64)s0;` |
| `FIELD_ADDR` | struct@off | `pf = (TZrInt64*)((TZrByte*)&s0 + 8);` |
| `LOAD_VALUE` | — | `s1 = *pf;` |
| `STORE_VALUE` | — | `*pf = s0;`（引用字段加写屏障点，05） |
| `COPY_VALUE` | blittable | `memcpy(&s1, &s0, sizeof(ZrLayout_x));` |
| `COPY_VALUE` | 非 blittable | 逐字段 `s1.f = s0.f;`（引用字段经屏障） |
| `INIT_VALUE` | struct | `memset(&s0, 0, sizeof(...));` 或逐字段置默认 |
| `BR_COND` | — | `if (s0) goto L_then; else goto L_else;` |
| `SWITCH` | union.tag | `switch (u.tag) { case ...: goto L_k; }` |
| `CALL_TYPED` | sig | `s2 = zr_fn_Bar(state, s0, s1);`（按 02§4 ABI） |
| `RETURN_TYPED` | sig | `return sR;` 或 `*__ret = sR; return;` |

**对照现状（要被取代的半降级写法）**：
`backend_aot_c_lowering_typed_arithmetic.c` 当前发射
`SZrTypeValue *dst = ZrCore_Stack_GetValue(...); ... ZR_VALUE_FAST_SET(dst, nativeInt64, l+r, ...)`。
本计划将其替换为上表的 `s2 = s0 + s1;`（dst/l/r 为 C 局部，见 `02` §3.2）。

## 3. 函数体发射结构（il2cpp 风格）

`backend_aot_c_function_body.c` 生成的 typed 函数骨架：

```c
ZR_AOT_EXPORT void zr_fn_<Module>_<Name>(SZrState *state, /* typed 形参 */) {
    /* 1. 帧 GC 根登记（仅含引用/非 blittable struct 的槽，见 05） */
    /* 2. 槽声明：TZrInt64 s0; ZrLayout_42 s1; ... */
    /* 3. 块标签 + 纯 C 语句（按 §2 降级表） */
L_blk0: ...
L_blk1: ...
    /* 4. deopt 跳板：goto zr_deopt_<id>，封送 typed 槽→tagged union 后回字节码解释 */
    return;
}
```

- 形参/返回遵循 `02` §4 的 struct ABI。
- 仅在分配/调用/屏障/异常/deopt 处出现受控 runtime 调用，其余纯 C。

## 4. AOT ABI 契约（`zr_aot_abi.h`）

- 明确 typed 函数的 C 签名规则、struct 传参/返回约定、`__ret` 隐藏指针顺序、
  错误/异常返回通道（如返回状态码 + state 上的 pending error）。
- 解释器侧提供 `CALL_TYPED` 调到“已 AOT 函数”的入口适配；未 AOT 时回落到 SemIR 解释执行
  （hybrid：同一 SemIR 两种执行）。
- runtime 调用点白名单（允许出现在 typed C 中的函数）：分配、写屏障、GC 安全点、
  动态调用 thunk、异常 throw/unwind、deopt 封送。**白名单之外的 VM 调用视为违规**（CI 检查，见 `06`）。

## 5. 与 LLVM 后端的关系

- C 后端是主交付（用户明确要纯 C）；LLVM 后端（`backend_aot_llvm_*`）复用同一 SemIR 与降级语义，
  作为可选高性能路径。降级规则表（§2）对两者是同一份语义规范。

## 6. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 04-S1 | 固化降级规则表为后端内“opcode→C 模板”单一映射模块 | 代码审查 + 表覆盖所有 typed opcode |
| 04-S2 | typed 算术/比较/位/转换改发裸 C（取代 FAST_SET 写法） | AOT 产物 grep 无 `ZrCore_Stack_GetValue`/`ZR_VALUE_FAST_SET` |
| 04-S3 | 帧槽落为 C 局部 + GC 根登记骨架 | 生成物编译通过；GC 压力测试不丢引用 |
| 04-S4 | `FIELD_ADDR/LOAD/STORE/COPY` 发裸 C（`.`/偏移/memcpy） | struct 字段读写 AOT 为纯 C，结果对齐解释器 |
| 04-S5 | `CALL_TYPED/RETURN_TYPED` 按 ABI 发真实 C 调用/返回 | 跨 typed 函数调用 AOT 与解释器一致 |
| 04-S6 | deopt 跳板 + typed↔dynamic 封送（衔接 05） | 触发 deopt 用例正确回退且结果正确 |
| 04-S7 | CI：typed 函数 C 产物的 VM 调用白名单校验 | 违规调用使构建失败 |

## 7. 验收口径（不变量 B 的可测形式）

- 取一组全 typed 基准函数（数值、struct、union、循环、跨函数调用），生成 C 后：
  - `grep -E 'ZrCore_Stack_GetValue|ZR_VALUE_FAST_SET'` 命中数 = 0；
  - 算术/赋值/字段/比较/分支均为 C 原生运算符；
  - 与解释器执行逐位一致（黄金测试）。
