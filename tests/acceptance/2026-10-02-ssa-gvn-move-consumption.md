---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
  - docs/plans/ssa/guides/B-passes-analysis.md
tests:
  - tests/parser/test_ssa_gvn_range.c
doc_type: acceptance-record
status: scoped-accepted
---

# SSA 02.02: preserve MOVE consumption in GVN

## Scope and baseline

GVN classified `MOVE` as a pure expression because its opcode schema has no
memory/effect flags. Repeated moves of one source were therefore commoned:
the second move became `COPY` of the first result. MOVE consumes its source;
the replacement instead made the second result initialized even though its
source had already been consumed. The repair excludes MOVE from pure GVN
candidates and preserves the instruction and original source operand.

The fixture passes full structural/SSA/effect verification; Core owner-state
analysis separately shows the first source becomes MOVED and the second MOVE
cannot initialize its result. The test compares these states before and after
GVN, then repeats GVN to cover stable consumption on another run. New checks
remain active when `NDEBUG` is defined and fail through `exit(EXIT_FAILURE)`.

## Test inventory

- `test_gvn_preserves_repeated_move_consumption`: first MOVE transitions its
  UNIQUE source from INITIALIZED to MOVED, the second MOVE sees MOVED, and its
  result remains UNINITIALIZED. Both MOVE instructions, the source operand,
  and absence of successful CSE remarks are checked after repeated GVN.
- `test_gvn_still_reuses_repeated_scalar_copy`: ordinary scalar COPY still
  reuses the first result, retains the second SSA result ID, emits the expected
  source remark, and passes full verification.
- The complete `ssa_gvn_range` executable also covers pure ADD reuse,
  dominating-block reuse, sibling-block rejection, implicit conversion target
  types, malformed storage, alias conservatism, and range-proof boundaries.

## Commands and evidence

All build directories, logs, object files, and compiler TMP/TEMP/TMPDIR are
under `D:/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move` (WSL `/mnt/d/tmp/...`).
`validate.py` reads the exact existing `zr_vm_ssa_gvn_range_test` CMake source
list and directly builds that standalone executable without configuring or
editing CMake. Each completed run records its command, source SHA256 values,
build exit, and execution exit in its phase directory's `receipt.json`.
Linux source compilation uses at most two workers for final validation.

Windows focused commands use the installed Visual Studio developer environment:

```powershell
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move/validate.py msvc red-deterministic
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move/validate.py msvc green
```

Linux commands use an explicit distro, working directory, and Linux-only PATH:

```powershell
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin TMPDIR=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move/validate.py gcc red-parallel
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin TMPDIR=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move/validate.py clang green
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin TMPDIR=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move/finish_shared_clang.py
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin TMPDIR=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move/finish_linux.py gcc gcc-red-parallel
```

## Results and limits

| Phase | Build exit | Execution exit | Evidence |
| --- | --- | --- | --- |
| MSVC deterministic RED | 0 | 1 | `msvc-red-deterministic/{build,run}.log` |
| GCC parallel RED | 0 | 1 | `gcc-red-parallel/{build,run}.log` |
| MSVC GREEN | 0 | 0 | `msvc-green/{build,run}.log` |
| Clang shared ASan/UBSan GREEN | 0 | 0 | `clang-shared-green/{build,run}.log` |
| GCC GREEN | 0 | 0 | `gcc-green-final/{build,run}.log` |

Both deterministic RED runs print `Repeated MOVE initialized consumed result:
expected 4, got 1`, directly identifying UNINITIALIZED versus INITIALIZED.
The first MSVC RED reached the same failing assertion but timed out because
the CRT opened an assertion dialog; the explicit failure path provides the
stable exit above. The initial sequential GCC compilation timed out after
240 seconds under filesystem load; the separate parallel RED completed.
These attempt logs are retained. Final Linux compilation is limited to two
workers. Clang compiled every source with sanitizers; its static-runtime links
timed out after 120 and 900 seconds. The single alternative
`finish_shared_clang.py` directly relinks the retained objects with
`-shared-libasan`, then executes with
`LD_LIBRARY_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/lib/linux`.
Its build and runtime both exit zero, and its sanitizer log is empty.
GCC GREEN reuses the RED objects whose source hashes match and
recompiles the changed test/GVN pair. Final source hashes are checked again
after linking/execution. The task-owned linkers were observed waiting in
`p9_client_rpc`; PIDs and start times are recorded in `link-p9-state.txt`.
GCC is Ubuntu 11.4.0; MSVC is 19.44.35228; Clang is Ubuntu 14.0.0-1ubuntu1.1 and uses
`-fsanitize=address,undefined -fno-omit-frame-pointer` with leak detection and
halt-on-error enabled.

GCC, MSVC, and Clang shared ASan/UBSan accept this focused MOVE
candidate-classification repair. The final validation receipts confirm
unchanged source hashes before and after the run. Direct oracle/ExecBC execution, source-language
lowering, general ownership availability across intervening operations,
memory GVN, range propagation, guard deletion, and the complete 02.02 exit
gate are outside this focused record. No ownership state-map, runtime,
verifier, shared CMake, or dirty scalar-test source was edited.

## Root independent validation

The root reviewed the four-file change and built the formal existing
`zr_vm_ssa_gvn_range_test` target in its fresh MSVC Debug repository tree at
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc` (exit zero;
`control/independent-targets-build-v2.log`). The registered `ssa_gvn_range`
CTest passed in 82.64 seconds. The same five-test run timed out in the separate
generation test, so only the individual GVN result is accepted from
`control/independent-targets-ctest.log`, not the entire suite.

The root independently executed the final Clang shared-ASan/UBSan binary:

```powershell
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_linux_artifact.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/gvn-root-clang-sanitizer.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/gvn-move/clang-shared-green/zr_vm_ssa_gvn_range_test
```

That run exited zero in 6.69 seconds with no sanitizer diagnostic. A separate
read-only gpt-6-sol review found no introduced issue in this scoped MOVE
classification fix. All generated root outputs and temporary files remained
under the D-drive artifact root. General availability of owned CSE candidates
is still a separate follow-up.
