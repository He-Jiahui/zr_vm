# Plan 03 Task 7.21 Canonical Source Constructor Signature Acceptance

## 验收基线

- 基线HEAD：`9ecb19ad5a53ae85369edc293b2d7fc31c694013`。
- Parser RED：source class/struct constructor位置没有canonical `CallAt`、callable TypeId或
  resolved declaration identity。
- LSP RED：constructor producer接入后，detached compiler/symbol state仍无法取得signature help，
  证明dispatcher尚在canonical分支前依赖legacy compiler resolver。
- GREEN：parser发布同一constructor fact/query合同；LSP只在resolved SymbolId指向source
  meta-function时前置canonical consumer，fact移除后fail closed。
- Native/imported constructor仍由structured external adapter处理，不允许source declaration
  或类型名fallback越界。

## 验收结果

- 三工具链canonical consumers `20/20`、semantic query `30/30`、semantic-query parity
  `9/9`、source contracts `65/65`，均真实exit 0。
- 三工具链interface均`111 Pass / 2 Fail`、真实exit 1；source constructor和native
  constructor fail-closed cases全部PASS，固定两个外部marker delta 0。
- 三工具链project均`56 Pass / 4 Fail`、runner exit 0；四个既有imported/native marker
  集合不变。
- `git diff --check`、大型文件阈值与changed-file warning audit通过；MSVC仅输出既有
  `/W3`被`/W4`覆盖提示。

## 状态与产出记录

- 完成时间：2026-08-29 18:20 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：parser/LSP RED-GREEN、class/struct/named-argument query contract、detached snapshot
  consumer、fact-removal fail-closed、native adapter回归与三工具链marker A/B。
