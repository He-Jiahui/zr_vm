---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
tests:
  - tests/CMakeLists.txt
  - tests/cmake/ssa-hotpatch-apply-sources.cmake
  - tests/library/test_ssa_capability_validation.c
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_canonical_zraf_validation.inc
  - tests/library/test_ssa_canonical_zraf_eis6_guard.inc
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-hotpatch-canonical-zraf-validation.md
  - tests/core/test_ssa_generation_publication.c
  - tests/library/test_ssa_rollback_restricted.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
doc_type: module-detail
status: partial
---

# Hotpatch capability validation contract

Hotpatch admission is a closed-world intersection:

    required = manifest.requiredCapabilities | every requirement.requiredBits
    admissible = (required & ~hostAllowedCapabilities) == 0

The capability closure is computed before generation preparation. Every
requirement must have a non-zero token and bit set, a zero reserved field, and
must fit the bounded requirement count. Unknown manifest flags, missing rows,
and capability escalation are reported with a structured diagnostic; callers
must not turn them into a generic false/success result.

ZrCore_HotPatch_ValidateCapabilityClosure only rejects unknown manifest flags
and invokes the shared closure calculation in hotpatch_capability.c. It does
not call the main validator or perform identity/signature checks; the host
must invoke the appropriate full admission entry separately. The output mask is written only after all rows pass.
Repeated requirements are harmless (bitwise union), while a single
out-of-policy bit rejects the complete patch.

The host must separately validate artifact schema, ABI/profile identity,
immutable content, and machine-code policy. Capability admission does not
authorize relocation or native imports by itself. iOS/WASM restricted profiles
therefore remain interpreter-only and reject relocation sections before
execution.

Validation snapshots the `patchId` and `publicContractHash` values consumed by
Apply and generation preparation, alongside the existing content hash,
capability mask, policy hash, and target profile. It also captures the original
borrowed byte address and length. Apply rehashes that span before registry
lookup or generation preparation and reports `content-mismatch` if its contents
changed since validation. This catches persistent changes made after Validate
returns and by a successful signature callback.

The validated token still borrows its artifact and manifest pointers and does
not copy or pin artifact bytes. Callers must keep the captured byte span alive
and prevent concurrent writes while Apply hashes it. This check does not make
the bytes immutable or cover later writes. The current generation manager
records identity metadata and does not install executable bytes, so any later
artifact consumer must preserve and verify the same content identity.

`ZrCore_HotPatch_ValidateZraf` is the typed outer-container entry point. It
checks limits, hashes the complete `ZRAF` byte span, validates the host policy,
and requires both the manifest `contentHash` and host `expectedContentHash` to
match. It snapshots policy and identity inputs before calling the host
signature verifier with the complete outer span. After comparing the
pre-callback and post-callback hashes, it proceeds only if they match; then it
reads the outer document, checks the copied expected public identity, opens the
canonical ExecIR module, verifies that graph, and requires exactly one function matching the explicit
`(entryFunctionToken, entrySignatureHash)` selector. It does not infer an
entry from function order or from the manifest. The selector check does not infer an entry from function order. Admission
remains limited by the current opener schema and contract-table checks; no
claim about every representable function graph follows from this entry check.
The public input and validated token preserve `outerLength` as `TZrSize`; the
validator rejects lengths above `ZR_ARTIFACT_MAX_BYTE_LENGTH` or the `UInt32`
hash/callback limit before reading bytes. It passes the original size to the
artifact readers and narrows only for the hash and signature callback APIs. A
limit diagnostic therefore reports the original caller-supplied length
instead of a truncated value.

Before the host signature callback, the entry point snapshots the expected
identity, input scalars, and manifest scalars, and reduces the borrowed
requirement rows to a validated capability mask with the shared capability
closure helper. The synchronous callback receives the complete outer bytes and
signature before the canonical container and nested graph are parsed. The
entry point hashes the same captured span again after the callback; a differing
hash returns `ZR_HOT_PATCH_CONTENT_CHANGED` and publishes no token. Rejected
callbacks stop before structural parsing; a successful callback proceeds to
outer identity, ExecIR verification, and selector checks. Only after all checks
pass does the output token receive by-value identity, selector, and policy
snapshots. The legacy inner-ExecIR `Validate` path retains its existing
behavior and error order.

The outer-byte pointer in `SZrValidatedHotPatchZraf` is still borrowed. The
caller must keep its allocation alive through validation and every later
`RecheckZrafContent` call, and externally synchronize any writers with those
operations. The span must remain stable from its initial hash through signature
verification and structural decoding, including while the callback runs; the
callback must treat it as read-only. The post-callback hash rejects when its
result differs from the pre-callback hash. It cannot detect a callback mutation
that is restored before return, and it does not make concurrent writes atomic.
Recheck detects only changes visible to its hash while the borrowed storage
remains live; it does not pin or own bytes. The signature callback authenticates
the outer span only; the host-supplied manifest and expected identity are
separately trusted inputs whose checked values are captured before the callback.
This API validates canonical ExecIR metadata and does not relocate, install, or
execute code.

The public scalar ExecIR reader may decode typed EIS6 binding-row schemas, but
the canonical outer-artifact opener currently admits only functions using
`ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY`. It checks every decoded function
immediately after scalar decoding and before contract validation or publication.
A typed function returns `ZR_ARTIFACT_STATUS_INVALID_SECTION` for
`ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE` at the nested scalar payload offset; the
temporary decoded module is freed and the caller's empty output module remains
unchanged. The ZRAF validator preserves that source offset and clears its
validated output. This keeps typed binding-row data outside the canonical
hotpatch admission path until its full validation contract is defined.

## 当前接口限制与后续消费

旧 Validate 核对的是捕获的 inner ExecIR 视图 buffer，不做完整 ZRAF 结构解码；它在验签后读取需求，验证期间输入须稳定。ZRAF 入口则在回调前捕获清单/身份和授权闭包，再认证完整外层跨度。旧入口的 expectedContentHash 可为零，ZRAF 的该值必须非零。两个结果类型不能将内层/外层内容哈希混用。

Prepare 只复制验证时捕获的身份/profile 等标量至版本记录，不安装字节或再次验签。旧 Apply 用原 contentBytes/contentLength 重哈希；ZRAF Recheck 只检查跨度/成功标记并比较当前哈希，不重新解析或核验宿主策略。借用跨度有效期、写入同步和最后消费者仍由宿主负责，immutableContent 不是内存冻结。

StatusName 返回静态借用文字，部分枚举/未知值共用兜底，程序按状态枚举决策。Diagnostic 的 token/sourceOffset 是检查点相关信息，可零、需求/入口/产物 token、产物字节偏移或图指令 ID。PolicyHash 只覆盖有限版本/flags/目标 ABI/profile 与 host/base 字段，不含补丁 ID、内容或逐需求，不是完整清单认证。

TODO：从公开验签回调核查仓外宿主的算法/信任根和清单绑定策略；当前仓内正向入口是测试桩，不能据此宣称生产密码学认证通过。完整 signed manifest 集成与从完整 IR 推导能力需求仍未完成。

## 既有测试记录与本次信用边界

以下运行数据是既有文档的历史记录，本次未读取其 native 日志重授当前信用，也没有编译或运行新的头文件候选。本次只确认当前正向 caller、注册/dispatch、支持 schema 和生命周期契约；历史结果不代表此次 after 原始字节已运行通过。

Focused CTest coverage is provided by ssa_capability_validation,
ssa_canonical_zraf_validation, and
ssa_rollback_restricted. The tests exercise a valid closure, escalation,
unknown flags, signature/content failures, idempotent application, fresh
generation rollback, and restricted-section rejection. The
[canonical ZRAF validation acceptance](../../tests/acceptance/ssa-hotpatch-canonical-zraf-validation.md),
[Apply-time content mutation acceptance](../../tests/acceptance/ssa-hotpatch-content-mutation.md)
records that sequential byte changes are rejected before registry/generation
publication. The canonical fixture also records callback ordering, full-width
length rejection, and policy/identity snapshot behavior under callback writes.
The EIS6 guard fixture requires successful public scalar decoding and Core
verification for typed-empty and typed-CALL modules before checking that the
canonical module opener and ZRAF validator reject them with the scalar payload
offset and no published output. The new guard cases passed with the EIS6
codec on MSVC 19.44, GCC 11.4 and Clang 14 in the current SSA artifact caches
under `D:/tmp/zr_vm`. The GCC/Clang 19-suite runs passed this canonical gate
while separately exposing an obsolete conditional-cleanup fixture. The guard
therefore has evidence of successful public decode followed by canonical
rejection; rejection by the previous scalar reader is not used as evidence.
Earlier current-root Clang evidence covers ten incremental build targets (85/85 build
steps) and four focused CTest cases (4/4, 3.65 seconds total): artifact writer
(2.45 s), artifact roundtrip (0.14 s), canonical ZRAF validation (0.03 s), and
legacy capability validation (0.01 s). Earlier current-source MSVC evidence also
records a five-target incremental build (352/352) and its prior foundation
CTest gate. The latest current-root GCC incremental build covered thirteen targets (107/107 steps, exit code 0); its selected CTest gate passed 13/13 cases in 71.52 seconds, including artifact writer (12.47 s), roundtrip (2.38 s), canonical ZRAF validation (0.89 s), and legacy capability validation (0.76 s). Logs: D:/tmp/zr_vm/ssa-control/gcc-current-thirteen-target-build.log and D:/tmp/zr_vm/ssa-control/gcc-current-thirteen-ctests.log. These focused results do not complete 08.02: full canonical signed manifest integration and capability-closure derivation from the complete verified IR remain open. Availability is explicit: an unsupported capability
or backend is never reported as accepted.
