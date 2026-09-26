---
related_code:
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.h
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_core/include/zr_vm_core/zrp_metadata.h
  - zr_vm_core/src/zr_vm_core/zrp_metadata.c
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_core/src/zr_vm_core/zrp_metadata.c
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
tests:
  - tests/cli/test_cli_zrp_metadata_dump.c
  - tests/cli/test_cli_args.c
doc_type: module-detail
---

# ZRP Metadata Inspection Commands

The CLI has three inspection modes: `--dump-zrp-metadata`,
`--diff-zrp-metadata`, and `--check-zrp-metadata-version`. Argument parsing in
`command.c` selects a mode, `app.c` calls the corresponding `Run*Path` function,
and that function reads its input file as raw bytes before calling a `Write*`
function. The writer borrows the caller's buffer and output stream; the path
entry owns and frees the file buffer. A write failure can leave partial text in
the output stream.

The dump and diff modes require a complete current-format binary metadata
header that passes the core reader and validator. Dump prints each section's
size and record count in a fixed order. Diff validates both inputs before
printing its first line and reports nonnegative removed-byte and removed-record
counts; an increase is shown as zero removed, and section payloads are not
compared. The version mode first reads a 16-byte prefix so it can report an
unsupported version. It prints `status=unsupported` and returns a failure
status for an unknown layout; `status=ok` still requires full core validation.

## Input Source To Confirm

The help examples name `.zrp` files, while ordinary project `.zrp` manifests
are JSON. These inspection paths require raw binary metadata bytes. The AOT
backend can generate or rewrite such a blob in memory, but the current review
has not established a standalone disk file that supplies these CLI modes.
The `TODO:` note in `zrp_metadata_dump.c` records the source and naming question;
the command's intended file input must be confirmed before changing its help
or path handling. `test_cli_zrp_metadata_dump.c` exercises the binary input
contract with constructed metadata buffers and temporary files.
