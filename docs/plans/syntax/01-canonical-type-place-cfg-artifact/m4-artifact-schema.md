# 01-M4 规范 artifact schema 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md` 的 `M4 artifact schema`。

## 状态与产出记录

- 完成时间：2026-07-19 20:31 +08:00
- 状态：已完成
- 完成项目：
  - 建立统一 `ZRAF` version 1 envelope，以固定小端宽度编码 112-byte header、24-byte section directory 和所有公开 rows，不写入裸 C struct、指针或宿主 ABI padding。
  - 明确 `.zrs/.zri/.zro` 三类职责和允许 section；`.zrs` 保存 syntax/source projection，`.zri` 保存 readable semantic projection，`.zro` 保存 executable public tables、code 与 relocation projection。
  - 建立 StringHeap、TypeDef、TypeRef、TypeSpec、MemberDef、PropertyDef、SignatureHeap、Contract、Layout、Code、Relocation、Debug、SyntaxTree 与 SemanticIR section。
  - TypeDef 稳定编码 value/GC/resource/readonly/ref-like/drop/valueConstructible capability，以及公开 constructor token/signature/contract；Layout 稳定编码 size/alignment、GC scan、ownership map、version/hash 与 StableSlotSource contract。
  - 建立完整 signature grammar，覆盖 primitive、TypeDef、generic parameter/instance、typed const、array、tuple、union、nullable、function、REF、READONLY_VIEW、精确 OWNER kind、never/error。
  - callable signature 独立编码 receiver/ref-export effect 和每个参数的 passing/escape/entry-init/exit-init/temporary/call-site contract，不把 `:`、`->`、`=>` 表层 delimiter 纳入类型身份。
  - 将 TypeRef、TypeSpec、structural Signature、Layout version/hash、callable Contract 和 Module hash 分离，并为每种 mismatch 提供精确状态与 expected/actual 诊断，不允许按名称降级绑定。
  - 实现可读 `.zrs/.zri` text projection，保留 literal syntax/SemIR 内容并使用确定性 final `payload-hex` 完成 text/binary exact roundtrip。
  - 实现 canonical type parser bridge：递归序列化 M1 TypeId graph、完整验证 signature、通过 canonical interner 导入，并证明 source compile 与 binary import 得到相同 TypeId、signature 和 public identity。
  - 安全拒绝 unknown mandatory section、truncated blob/artifact、非法 token、超限 count、duplicate/forbidden/overlapping section、非法 element width、递归/child limit 和越界 relocation；unknown optional section 仅在完成边界校验后跳过。
  - 建立 zero/256-row、duplicate signature、repeat hash、value construction、内部/外部 mismatch 和真实 source compile 回归；最终 `zr_vm_artifact_schema_test` 为 13/13。
  - 完成 MSVC 19.44、GCC 11.4 和 Clang 14 三套 9-target/254-test 矩阵；GCC/Clang 均从 Git index 暂存快照逐字节验证，最终 Critical/Important 审查为 GO（0 Critical、0 Important）。
- 验收证据：
  - `tests/acceptance/2026-07-19-syntax-01-m4-artifact-schema.md`
  - `docs/parser-and-semantics/artifact-schema-and-type-projection.md`
  - GCC 暂存快照：`/home/hejiahui/zr_vm-syntax-m4-staged-gcc-20260719-r3`
  - Clang 暂存快照：`/home/hejiahui/zr_vm-syntax-m4-staged-clang-20260719-r3`
  - MSVC 工作树：`build-syntax-01-m1-msvc`
  - GCC/Clang 的 `GCC_INDEX_MATCH` / `CLANG_INDEX_MATCH` 逐字节确认暂存 M4 文件等于 Git index；两套 WSL 环境均以 `*_M4_MATRIX_PASS` 完成 254/254。
  - WSL 快照仅额外填充 submodule 内容，并叠加当前非 M4 的 `profile.h/profile.c` 构建前置基线；该边界已在 acceptance 中明确记录。
- 里程碑提交：本记录随 `feat(syntax): complete canonical artifact schema milestone` 一并提交。

## 边界与后继

- M4 定义 canonical schema、public identity、reader/writer 与 parser projection；现有 `SZrIo` 仍是尚未迁移 consumer 的 compatibility path。
- local Place graph、block init facts、LoanId/origin/last-use、local range、AST pointer 和 runtime pointer 不进入 `.zro`。
- 下一里程碑严格进入 M5 consumers：迁移 VM、AOT、LSP、reflection、debug、CLI 与 legacy writers/loaders；验证 VM/AOT 结果及失败行为一致、LSP 与 compiler 同源、reflection/layout 不按名称猜测，并在正式 cutover 拒绝旧 schema、要求重新编译。
