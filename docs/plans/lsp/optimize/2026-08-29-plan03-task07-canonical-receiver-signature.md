# Plan 03 Task 7.20 Canonical Receiver Signature

## 目标

- 删除receiver method signature help的request-time AST/type/member/generic reconstruction。
- 让source receiver call只消费`CallAt/FormatCall`，binary/native receiver只消费external
  canonical adapter或canonical query fact。
- 保留尚未迁移的`super(...)`与constructor结构化adapter，不把method fallback换名搬家。

## 完成项目

- Source contract新增`signature_resolve_method_help`禁止项并取得单一RED。
- 删除method fallback入口、临时receiver primary、request-time `ExpressionType_Infer`、
  prototype member-name递归、AST method-name搜索与本地receiver generic闭合。
- 删除只被该fallback引用的dead helper chain，`lsp_signature_help.c`净减少647行。
- Canonical local/general与external callable adapter顺序不变；source canonical payload缺失仍
  fail closed。
- 新增module文档说明dispatch顺序、snapshot lifetime、exactness与constructor边界。

## 验证

- GCC/Clang/MSVC source contracts：`65/65`，真实exit 0。
- GCC/Clang/MSVC semantic-query parity：`9/9`，真实exit 0。
- 三工具链interface均`111 Pass / 2 Fail`；两个外部overlay marker为class-member
  navigation与reference-call diagnostic，本任务receiver signature cases全部PASS，runner
  真实exit 1，不计GREEN。
- 三工具链project均`56 Pass / 4 Fail`、runner真实exit 0；四个imported/native marker在
  fixed parent与overlay完全一致，delta 0，不计GREEN。
- GCC/Clang最终编译对本次文件无新增warning；MSVC仅有既有`/W3`被`/W4`覆盖提示。
- Full stdio仍被既有generic short-circuit diagnostic缺失在目标场景前阻断，未计GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 16:43 +08:00。
- 状态：已完成。
- 完成项目：receiver signature source-contract RED/GREEN、647行第二套语义删除、
  canonical/external dispatch边界、三工具链focused与project/interface marker A/B。
