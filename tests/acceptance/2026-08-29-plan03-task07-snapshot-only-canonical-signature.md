# Plan 03 Task 7.18 Snapshot-Only Canonical Signature Help Acceptance

## 验收基线

- 基线 HEAD：`fa2dc6cc871a0b70b2db1bc11f7df7bd8148e9db`。
- RED：取得 exact direct-call `CallAt/FormatCall` 后，将 analyzer
  `compilerState`/`symbolTable` 置空；旧入口因 compiler-state前置门禁失败，
  interface由固定 `109/4` 变为 `108/5`，新增失败仅为direct-call case。
- GREEN：同一 detached analyzer 返回 `inspect(value: int): int`；删除同一
  expression的call payload后仍返回 unavailable。
- Canonical resolver在源码顺序上先于 legacy compiler-state guard；旧
  constructor/super/method/function fallback仍不能在无compiler state时执行。
- 不修改 parser facts、Syntax05 property/interface路径或 external provider
  contract。

## 验收结果

- GCC/Clang/MSVC semantic-query parity：`9/9`。
- GCC/Clang/MSVC source contracts：`65/65`。
- Full interface：三套均 `109 Pass / 4 Fail`，新增 direct-call case PASS，固定
  marker delta 0，runner真实 exit 1，不计 GREEN。
- Project features：三套均 `54 Pass / 6` 固定 marker，runner exit 0，delta 0。
- Callable-value/lambda producer marker保持不变，未用consumer fallback掩盖。

## 状态与产出记录

- 完成时间：2026-08-29 03:38 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：detached compiler/symbol signature projection、canonical-first
  ordering contract、真实退出与固定 marker 审计。
