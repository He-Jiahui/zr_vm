# Plan 03 Task 7.20 Canonical Receiver Signature Acceptance

## 验收基线

- 基线HEAD：`d94d750aac3721b36920204a73cd975f944ac7e1`。
- RED：source contract禁止`signature_resolve_method_help`，MSVC runner真实exit 1，唯一新增
  失败为该legacy helper仍存在。
- GREEN：删除method fallback与只被它引用的receiver AST/prototype/member/generic helper；
  source contracts恢复`65/65`。
- Source receiver call缺canonical payload时保持unavailable；binary/native receiver仅允许
  external canonical adapter或canonical query，不按member name回退。
- `super(...)`与constructor adapter未修改，后续单独迁移。

## 验收结果

- 三工具链source contracts `65/65`、semantic-query parity `9/9`，真实exit 0。
- 三工具链interface均`111 Pass / 2 Fail`，本任务direct/callable/lambda/receiver/generic
  receiver cases全部PASS，固定marker delta 0。
- 三工具链project均`56 Pass / 4 Fail`、runner exit 0；GCC同snapshot parent/overlay A/B
  精确证明四个imported/native marker集合不变，其余工具链marker同构。
- `git diff --check`、forbidden helper search与GCC/Clang warning audit通过。

## 状态与产出记录

- 完成时间：2026-08-29 16:43 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：method fallback RED/GREEN、dead helper删除、canonical/external receiver边界、
  三工具链真实退出与fixed marker审计。
