---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_checked_integer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_signed_divide.inc
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_checked_integer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_signed_divide.inc
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
  - "user: 2026-10-03 continued SSA execution with independently verified subtasks"
tests:
  - tests/core/test_execution_checked_divide.c
  - tests/cmake/checked-divide-tests.cmake
doc_type: acceptance-record
status: verified-scope-only
---

# Checked signed i64 division prerequisite

## Scope

The five legacy signed DIV forms check both zero division and the unique
`INT64_MIN / -1` overflow pair before host C division. The pure private helper
preserves output on failure. Existing bodies are extracted to a focused include;
the large dispatcher changes only its include, ordinary DIV invocation, and
macro cleanup. Stores, fused loads, floating fallback, error PC and language
handler transfer retain their established contracts.

Unsigned division, modulo, generic mixed arithmetic, AOT and SSA projection
admission are outside this finite prerequisite. The Core runtime must be
verified before a later materializer adds DIV support.

## Baseline and actual VM RED

Before any provider changes, native SHA checks compared all **1980** sealed
support source/dependency inputs to the current E worktree: **zero drift**.
The original Core archive at
`D:/tmp/zr_vm/ssa-20261003-01a0fe2b/source-break/msvc-green/lib/zr_vm_core.lib`
had SHA-256
`c2dd68f801ce6dd2937d753a52bccfa22a6cba8e7075b404cc435478c31d9eb6`.
Core, Unity, xxHash and utf8proc archives were read in place and hashed before
and after; none was moved, copied or modified. Their source provenance is the
source owner's `final-support-source-and-dependency-inputs.json`, hash
`32138103d7d2734011af056d870b0f2ee91577d16489ba6dd746aa4d0474f6c4`.

The RED test had seven runtime groups and no calls to the new helper. Its
source hash was
`c44e2c44440faccce6de8a25ef8460aa3a1432e3e42a68f9f918603ad706d892`.
A native source-only snapshot copied the complete **101-file** local include
closure onto D. Five actual test/harness units compiled successfully under
MSVC 14.44.35207; link exit was **0**. The first runtime overflow case executed
real `DIV_SIGNED` with `INT64_MIN` and `-1` and exited **3221225621**, Windows
integer overflow **0xC0000095**. It was not a compile error, an undeclared helper,
a fabricated failure, or a timeout. The fixture log had no completed Unity
summary because the unguarded operation terminated the process.

The exact driver invocation was:

```text
python D:/tmp/zr_vm/ssa-20261003-01a0fe2b/div-vm/runtime_tu_driver.py D:/tmp/zr_vm/ssa-20261003-01a0fe2b/div-vm/snapshots/red-runtime-1-1790976826351245400/snapshot-config.json --toolchain msvc
```

Evidence is in `div-vm/red-support-proof.json`, `red-outcome.json`,
`msvc/red-runtime-1/receipt.json` and its compile/link/runtime logs. Both the
snapshot pre/post hashes and native original-versus-snapshot verification were
unchanged. The old provider snapshot remains immutable.

## Test inventory

The GREEN fixture adds an eighth pure helper group and structured message
checks after preserving the RED epoch. It executes actual Core function objects
through `ZrTests_Runtime_Function_ExecuteCaptureFailure` or host `TryRun`:

- Five DIV forms, with and without a trace observer, reject signed overflow.
- A 19-row catalog covers zero, both extrema, ±1, both minimum-value operands,
  four sign combinations, non-exact truncation toward zero, zero denominator
  and the overflow pair across all forms and both execution modes.
- The helper checks every catalog row, null output and operand/output aliases;
  failure preserves the previous output.
- Both input slots are also used as destinations for normal and overflow VM
  execution. `LOAD_STACK_CONST` reads through a distinct loaded slot 3.
- Failures expose normalized runtime status and exception, plus the exact
  distinct zero/overflow messages.
- A protected direct call checks the saved faulting PC and reuses the same state
  for a successful division after reset.
- Language catch/finally consumes an overflow; finally-only transfer preserves
  the failure while running cleanup.
- All five forms preserve their existing floating fallback.
- Four ordinary store forms release an owned destination on success and during
  registered overflow unwind; the plain form retains its plain-slot precondition.

Fixture state/function and actual owner/weak values have static lifetime across
Unity assertion longjmp. Cleanup clears the observer, closes registrations,
releases tracked stack destinations through saved offsets, releases external
owner/weak values, resets the thread, frees the Core function and destroys state.

## Tooling and source epochs

All source snapshots, objects, compiler temporaries, PDBs, executables and logs
are under the new `D:/tmp/zr_vm/ssa-20261003-01a0fe2b/div-vm` subtree. No compiled
output is relocated across drives. The frozen D driver derives from the shared
`actual_tu_driver.py`, adding configurable MSVC `/MDd` so its ABI matches the
proven read-only Core archive. `/showIncludes` records compiler header inputs.
Each subprocess has a bounded watchdog.

The GREEN snapshot is a distinct 102-file include closure. It compiles the
current dispatcher independently and supplies that fresh object before the
read-only baseline archive; the old dispatcher archive member must not be used.
The prior archive's other providers require exact current input compatibility.

## Results and acceptance decision

Root integrated the private fragment into `tests/CMakeLists.txt` at line 8774.
The prior bytes were preserved exactly: parent SHA-256 changed from
`79c3a1db47069a50ca2ad4a281ba988b17aaaedc1131458911f3218db37e25ae`
to `c9b0687b4b5e1e9de458f000b455deb78f54cf3ecb8cf13e8f451b657bdd6445`
by adding one `checked-divide-tests.cmake` include. The fragment remains
`7c8db48b49ab545cc1c1d82a670350eb27bf3f3c79ee1df770e4d59ca86769f4`.
The exact delta and before snapshot are recorded in
`validation-control/checked-divide-parent-integration.json`. Registered CTest
execution is recorded below; the integration itself is not a test pass.

The GREEN native command was:

```text
python D:/tmp/zr_vm/ssa-20261003-01a0fe2b/div-vm/runtime_tu_driver.py D:/tmp/zr_vm/ssa-20261003-01a0fe2b/div-vm/snapshots/green-runtime-1-1790977387785708700/snapshot-config.json --toolchain msvc
```

MSVC 14.44.35207 compiled all **six** actual units, including the new dispatcher,
with exit **0**; link exit was **0**. Real runtime execution reported
**8 Tests, 0 Failures, 0 Ignored**, exit **0**, in 44.5 seconds.
`div-vm/msvc/green-runtime-1/receipt.json` reports no snapshot input drift.
Native original/snapshot post-verification checked all **102** inputs with
zero drift. Archive hashes remained unchanged.

The actual MSVC `/showIncludes` records contain **96** local header/include
dependencies across the six units. Every one is present and unchanged in the
snapshot manifest, with no source path escape; the proof is
`div-vm/green-msvc-actual-input-proof.json`. Root independently re-executed the
same frozen native executable and observed all eight groups pass again. This
independent execution is separate from the implementer's recorded build/run.
The root check is the `checked-Core-signed-divide` entry in
`validation-control/root-independent-checks.json`; no separate raw rerun log
was created. The implementer's complete `fixture.log` remains the build/run
record.

The final compatibility scan against the old 1980-input superset found the
replaced checked header/dispatcher and a separately revised parser INVOKE test.
That parser test is not a provider of any of the four linked archives. Core
includes the private checked-integer header only from the dispatcher, and the
fresh dispatcher replaces that archive member. The exact deltas and read-only
archive hashes are in `div-vm/green-support-proof.json`.

Independent read-only PRE-spec and PRE-quality reviews passed the frozen six-file
implementation/test/module set. At that review epoch, both explicitly left final
CMake registration, Linux sanitizer evidence and the final acceptance freeze
pending. No semantic
blocker was found; these preliminary reviews are not final acceptance.

Actual VM RED and native GREEN are established. At this historical epoch, actual GCC/Clang sanitizer runtime execution, final
source seals and final independent review were pending. The later scoped GNU
result and its sanitizer limits are recorded below.
No acceptance or complete-plan claim is made yet.

## Registered MSVC CTest gate

The distinct `formal-msvc-1` epoch copies a complete **110-file** source/include
closure onto D and uses the actual checked-DIV fragment plus three exact parent
helper functions. Its independent CMake project completed genuine MSVC compiler
and ABI configuration; it does not bypass those checks. The original Core static
compile inventory has **215** source units; no new current Core C provider is
missing from that inventory.

Current dispatcher, SSA verifier and binding guard compile as three fresh Core
object units, followed by the proven read-only full Core archive. The fixture
and four actual harness units also compile freshly: **eight** actual units.
The link map shows all three fresh providers and **zero** old versions of those
three archive members. **130** extracted unchanged Core members map uniquely to
their original compile-command source and retain current matching source SHA.
All other original Core source/dependency superset inputs match; the changed
checked-integer header belongs to the replaced dispatcher closure. Four support
archive hashes remain identical to the preserved baseline.

Parser call-transfer and unrelated test deltas in the 1980-input superset are
recorded explicitly and are not providers of the linked Core archive. This
gate proves scoped compatible support and current replacement objects; it does
not assert that the complete old Core archive represents current sources.

The first build/run passed but CMake misdecoded the real Chinese showIncludes
prefix, leaving Ninja header counts zero. That preliminary dependency result
was rejected. The owned D project sets the observed prefix explicitly and
performs a clean rebuild; no E provider changes were needed. Final Ninja evidence
has **8 VALID, 0 STALE** records with nonzero dependency counts and **98** unique
local dependencies, all present and unchanged in the 110-file snapshot. Native
original/snapshot verification reports zero drift or source escape.

Final configure, eight-unit build and link all exited **0**. The actual registered
`execution_checked_divide` CTest passed in **1.11 seconds**, with
**8 Tests, 0 Failures, 0 Ignored**. Evidence is
`div-vm/formal-msvc-1/configure-prefix-2-receipt.json`,
`build-prefix-2-receipt.json`, `ctest-prefix-2-receipt.json`, their logs,
`ninja-deps-prefix-2.log`, and `div-vm/formal-msvc-input-proof.json` (`valid: true`).
The proof records exact source, snapshot, executable, link-map and archive SHA.

Root independently repeated this generated CTest registration using a copy of
registration metadata under `validation-control/checked-divide-root-ctest` and
the original executable in place. No binary was copied. The independent run
reported **1/1 passed**, **8 Tests, 0 Failures, 0 Ignored**, with test time
**1.51 seconds**. Its receipt is
`validation-control/checked-divide-root-ctest/receipt.json`; the registration
metadata and executable remained unchanged. The executable SHA-256 is
`0b564617ca8183d526e630cc08dc4216dbf3c6e38320ee79e04df6811d479c3c`.

The first shared Linux configure attempt reached its bounded 189-second
watchdog before progress. It did not compile or execute the DIV fixture and is
an environment startup failure, not a signed-division compiler/test result.
The setup owner diagnoses that launch while preserving the failed attempt.

## Required test include correction and current MSVC gate

The first actual GNU sanitizer compile diagnosed an implicit declaration of
`ZrCore_Execute` in the fixture. The fixture now includes its declaring public
header, `zr_vm_core/execution.h`. This is a single test include change; the three
runtime providers, private CMake fragment and module document retain their frozen
hashes. The old test hash
`bbf0dd86ee643566115136d987f0ba61d15eb63f736b3a0d6ff1363b0363e00a`
and all earlier snapshots and receipts remain preserved. The current test hash is
`c05d333c285f2a2d310e62f1d3dfb14766faa60d4dbfec396b7f4f0f80daf130`.
The release and correction are recorded in `div-vm/test-include-epoch-2.json`.

The distinct `formal-msvc-2` project completes genuine compiler and ABI
configuration and compiles the updated test as one fresh actual unit. Seven
previously successful provider/harness objects are read in their original D
locations. Their object hashes and unchanged source/include closure are bound to
the prior formal proof and the new 110-file snapshot; the sole source difference
is the fixture include. No object is copied or rebuilt from an older test.

Configure, fresh test compilation and link exit **0**. The actual registered
`execution_checked_divide` CTest passes **1/1**, with **8 Tests, 0 Failures,
0 Ignored**, test time **1.41 seconds**. Ninja records **1 VALID, 0 STALE** with
**64** actual local dependencies for the newly compiled fixture. The link map
continues to bind **130** unchanged extracted Core source members and excludes
the three replaced Core archive members. All four archive hashes remain
unchanged. That epoch executable SHA-256 is
`4410f50a8bb219321efb77c6ff73e2f24dd27d30e502ecd68880efa13042cd57`.

Records for this historical epoch are `div-vm/formal-msvc-2/ctest-exact.log`,
`ctest-exact-receipt.json`, `ninja-deps-exact.log` and
`div-vm/formal-msvc-2-input-proof.json` (`valid: true`, SHA-256
`39512ca1fd8a2de95f979d14c0fd22e44cc91adbd07a2f2e2c19a2dc821cf2d4`).
The initial CTest filter matched no tests and the initial bare Ninja command was
not found; those setup attempts are retained and do not count as test passes.

## Historical Linux support and sanitizer attempts

The sole support writer produced a real GNU 11.4 full Core archive from **215**
Core and **2** vendor translation units. Its receipt has **217 VALID, 0 STALE**
records and **405** actual local source/header inputs matched against E and D.
The **108** SDK hashes are post-build seals; they do not establish a pre-build
SDK inventory. The read-only support proof is
`source-break/linux-core-support-seal.json`, SHA-256
`df9f5023adeb7a4212690040f9025314d6ea5100d260104047639c5520ffb34d`.
The actual Core archive at `source-break/gcc-native-build/lib/libzr_vm_core.a`
has SHA-256
`b16666b03e31598599d33358be8b21e953ac562b3e8c3d64c45bf415ca07821c`.
Unity support is separately sealed in `linux-unity-support-seal.json`, SHA-256
`af8380509fcea217077ea04a3a7a6708d1eeafdfd0cc0e38cc25321345db0e45`;
its one-unit archive has SHA-256
`77e8cefb3c11ecdd89b9a707fd68f25ae0aeb86228550493577e90dcb800103c`.

The historical intended sanitizer scope was the actual fixture, four harness units
and current dispatcher, linked before this read-only plain GNU support. It is
not a claim that every Core provider is instrumented. The first GNU attempt
preserves the missing-prototype compile failure and a dispatcher timeout. The
second attempt uses the corrected fixture and verifies reuse of four successful
harness objects, but its fresh fixture and dispatcher both reach their bounded
180- and 600-second time limits. It records zero snapshot drift and no successful
link or runtime. Partial outputs from timed-out compilations are not adopted.
These terminal attempts are in `div-vm/linux-sanitized-1/gcc` and
`div-vm/linux-sanitized-2/gcc`; These attempts did not establish a sanitizer runtime pass.

## Current MSVC support and independent registered gate

The former `formal-msvc-1` and `formal-msvc-2` archives were compatible at their
recorded source epochs. Later Core frame-parameter changes prevent describing
those old whole archives as current support. The current shared MSVC successor
instead binds its selected providers and current dependencies. Root independently
executed its actual registered CTest: **1/1 passed**, **8 Tests, 0 Failures,
0 Ignored**, and **1118** input pins remained unchanged. Its receipt is
`validation-control/div-current-shared-msvc-root/receipt.json`, SHA-256
`8aa4a4989a6664ee32045eb1a58bf814a652cc9b564857fa06637c58d7ec3dec`.

## Current GNU provider and finite sanitizer coverage

The accepted current GNU provider comprises **219** genuine plain Core objects
in their original D locations. Root independently checked their source/header,
object, SDK and tool provenance before acceptance; the direct-object adoption is
`validation-control/current219-direct-provider-root-adoption-v1.json`, SHA-256
`59f9becdaf876233f42be53063c13f0d39212a87f433d234dd8f39cc01ab7f79`.
The failed archive attempt produced an eight-byte archive with zero members and
is preserved as a failure. The successful link uses the actual ordered object
paths directly; it does not claim that failed archive or a new whole CMake build
was accepted.

Five separately accepted GNU ASan/UBSan objects cover the actual fixture and four
actual harness units. They are read in place with their exact source/header,
compiler-flag and object proofs. Core, Unity, xxHash and utf8proc providers are
plain. The instrumented dispatcher compilation reached its original **600-second**
bound and failed; its partial product is not linked. The original immediate
group observation and later proven empty group remain separate retained records.
No dispatcher, whole-Core sanitizer or Clang pass is claimed.

The real GNU query and native LLD link both completed naturally with exit **0**.
The ordered link inputs are five sanitized consumers, 219 plain Core objects and
three support archives: **227** inputs. Actual trace evidence binds all 219
current Core paths in order. Actual map evidence uniquely selects the current
dispatcher, frame-parameter/staging providers and Profile definition. The
expanded argument list and actual SDK closure match their admission contract.
The binary is ELF64 x86-64 `ET_EXEC`; the audit records its finite header,
segment and selected table checks and explicitly disclaims complete ELF
validation.

The outer whole link invocation still **FAILED** because its Windows Job found
residual descendants after the helper primary exited. No residual child identity
is inferred. A separately leased read-only audit accepted the successful query,
LLD, binary, map and trace subproducts. Root's finite product adoption is
`div-vm/retained-B-link-Root-product-adoption-v1.json`, SHA-256
`901c9e67dabba4cdfed1af891dcaa763f55d8e8b939225151b7cf348658a861d`.
The original failed whole receipt remains
`validation-control/B-direct219-Root-link-run-v1/Root-invocation.json`, SHA-256
`fe348a05df16431f3acfc93be2ee7459e79eaad6d0056fd24d463c5037e9cc14`.

## First actual GNU runtime and registered CTest

Root's separately leased runtime invocation completed naturally with exit **0**
in **112.899 seconds**, with **875** current input pins unchanged and no cleanup
actions. The invocation receipt is
`validation-control/B-retained-runtime-Root-run-v4/Root-invocation.json`, SHA-256
`ec1de5d5b329f69652c651694f800765dcb1b8051ef45435e7ff170ccd350991`.
That transport receipt alone is not the finite runtime adoption. Root separately
checked the actual test logs, owned groups and the **756** Linux pre/post input
pins before accepting this finite runtime result in
`div-vm/retained-B-runtime-Root-adoption-v4.json`, SHA-256
`f6e9f2be27bc3a708ba965c8914b6c13bed2a46e8f813e33ab72cd78245a8e38`.

The actual Linux receipt `div-vm/retained-B-runtime-v4/runtime/receipt.json`
reports **8 Tests, 0 Failures, 0 Ignored** for direct execution and for the
registered test; CTest passes **1/1**. The direct and CTest primary/group IDs are
**148300** and **148302**. Both exit **0**, are reaped with empty owned groups,
have no signal/timeout/CANCEL or cleanup errors, and satisfy the original strict
**120/180-second** full-phase limits. ASan leak detection and UBSan halt-on-error
are enabled. Their coverage remains limited to the five instrumented consumers.
The retained executable stays in place with SHA-256
`216b46ff7e8ca88ae65a0c5d0b4a7185b4da44d10de41876b2f41488a2ef8e99`.

The registration repeat copies three genuinely generated CTest files. It changes
only the checked-DIV executable argument and leaves both child files byte exact.
The genuine private fragment remains
`7c8db48b49ab545cc1c1d82a670350eb27bf3f3c79ee1df770e4d59ca86769f4`.
This is a focused registered test gate, not the full parent test suite. Native
WSL transport uses the bounded Popen client; acceptance also requires actual
Linux child/group evidence and does not claim Windows WSL service descendants
are empty. The original **420-second** whole invocation budget is unchanged.

Root's current GNU/Python/CTest logical-route adoption is
`validation-control/current-GNU8-Python-CTest-logical-Root-guard-v1/Root-route-adoption.json`,
SHA-256 `15c79ed896be39afd439d082e58c103b8d85be80a30ab1176dae6463ec405334`.
The actual `/usr/bin/ctest` bytes and runtime SDK closure are pinned pre/post.

## Independent actual GNU repeat

Root repeated the same binary in a distinct output directory using the same
reviewed control logic and unchanged **120/180/420-second** limits. No query,
compiler, relink or compiled-product copy occurred. The repeat completed
naturally with exit **0** in **125.604 seconds**; **877** Root input pins and
**756** actual Linux input pins were unchanged. Direct execution again reports
**8 Tests, 0 Failures, 0 Ignored**; the registered CTest passes **1/1** with the
same eight Unity groups. VM group **155315** and CTest group **155319** exit
naturally, are reaped and are empty, with no signals, timeout/CANCEL or cleanup
errors. Their complete observed phases take **0.601** and **1.056 seconds**.
The binary hash remains `216b46ff7e8ca88ae65a0c5d0b4a7185b4da44d10de41876b2f41488a2ef8e99`.

The Root invocation is
`validation-control/B-retained-runtime-repeat-Root-run-v6/Root-invocation.json`,
SHA-256 `a8a3e8b207558574d82502956fe962b652201cd91ed31b913305e9f5c15931f3`.
Root separately checked the actual logs, both owned groups and exact pre/post
inputs, then accepted the repeat in
`div-vm/retained-B-runtime-repeat-Root-adoption-v6.json`, SHA-256
`81c30e967ef7b8fe4b12df6b36c0b449a11dcb27afadd8867e0c74ee97fca849`.
Both runtime gates use the frozen checked-DIV providers and fixture used by the
current shared MSVC registered gate. MSVC's receipt binds its selected support
providers; these results do not claim a fresh whole-current MSVC Core archive.

## Finite acceptance decision and remaining integration

Actual old-source VM overflow RED, the current MSVC registered gate, scoped GNU
direct/registered execution and the independent GNU repeat are established.
Runtime controls, retained-product audit, and final code/document consistency
received independent SPEC and QUALITY review. The finite change is recorded in
Git history. Root stages the parent CMake integration as the single checked-divide
include while preserving unrelated worktree changes.

Earlier failed epochs and the outer whole link **FAILED** record remain
immutable. The final accepted GNU result uses **219 plain Core** objects and
**five instrumented fixture/harness** units. There is no instrumented-dispatcher,
whole-Core sanitizer or Clang pass. This finite prerequisite does not close any
of the **47 open SSA plan leaves**, enable DIV in the canonical materializer, or
accept the entire SSA plan. The upper consumer requires its own reviewed
implementation and commit after this Core prerequisite is committed.
