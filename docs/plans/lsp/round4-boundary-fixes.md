# LSP Round 4: reproductions, fixes, and acceptance

Status: **Draft; not merge-ready.** This initial change publishes the reproduction and review checklist first. Production corrections and executable regressions will be added to this PR before it is marked ready.

## Scope and isolation

- The initial commit contains only this plan and the three-source generic-field fixture under `tests/fixtures/projects/lsp_boundary_round4`.
- Do not include unrelated working-tree changes, compiled artifacts, VSIX files, machine profiles, or private debug-controller code.
- The separate lint-analysis foundation is not part of this PR. The numeric-worker changes depend on that unpublished foundation; they must remain a separately scoped change rather than implicitly bringing the foundation into this PR.
- Component evidence collected on the private CD66-derived baseline is useful diagnostic evidence, **not** proof that this PR's base builds or passes. Rebase/selective integration and testing of the exact PR commit are required.

## Reproduction

The fixture exercises two physically distinct `Box<T>.value` declarations, `int` and `string` instantiations, direct and compound field writes, a captured receiver, nested lexical shadowing, a side-effecting computed index, and supplementary/non-ASCII text.

For each physical field family, navigation must retain the actual owner and exact token range. There are six uses, seven locations when the declaration is included, and seven rename-preview edits. The four reads and four writes in the main source must resolve to the appropriate provider. Do not apply a rename while collecting this baseline. A future executable test must compute the ranges from the actual UTF-16 source; duplicate or foreign results cannot be normalized into a pass.

Additional cases to add with the production fixes:

1. Only main open; CRLF providers and a never-opened Unicode consumer. All valid locations must be included; closed-document wire edit versions must be null.
2. `readonly` and `scoped ref readonly` parameters: preserve the distinction between parameter passing contracts and body value types. Valid reads remain navigable. Illegal direct parameter/field writes produce the real permission diagnostic, no resolved writable authority, and no fabricated unresolved-name diagnostic.
3. Warm closed LF cache rewritten to raw CRLF: Completion, Hover, SignatureHelp, Definition, References, Rename, Implementation, symbols and document links must use a coherent prepared view. Nested resolution must not free an analyzer or AST still borrowed by its caller.
4. Real `didOpen(high version) -> didClose -> disk watcher -> didOpen(version 1, unsaved text) -> didChange(version 2)`. The new open epoch is authoritative; subsequent changes remain monotonic. Deleting the disk file must not destroy an open overlay.
5. Array binding groups: reassign, capture and valid `out` use; exact identities, all-or-nothing publication, failure quarantine, and 13 real allocation-denial sites with recovery. Preserve genuine invalid-alias errors. Same-block or implicit-return cases not actually supported must not gain fabricated authority.
6. Malformed dependencies, a physical peer project with identical names, changed membership/configuration, stale raw bytes, UTF-8/UTF-16 conversion, and final publication fences. Reject the whole protected result rather than returning a usable partial edit.

## Current evidence and remaining failures

- A matched private numeric component passed bounded worker tests and one actual 41-source run selecting only ZRL210/ZRL211: `complete=true`, one checked source, no incomplete checks. Successful frame sizes were not measured. This is **not** full-policy completion.
- The original full-policy run still exposed ZRL304 transport size and separate unavailable native facts. Do not remove a genuine INCOMPLETE or turn UNKNOWN into NO.
- Static review rejected an intermediate FIELD candidate for a newly introduced analyzer/AST lifetime error and two denied-write consumer/diagnostic errors. The ingress/lifecycle correction remains pending.
- The array-binding candidate passed source/syntax review, but its isolated shared build reached the Windows compiler optimization time budget before linking. Its three native acceptance processes have not run.
- A real isolated VS Code host wrote and saved the complex fixture through editor APIs. Debugger attachment occurred, but a controller child-session bookkeeping error prevented a verified breakpoint-trigger request. This is not a completed debugger or editor acceptance pass.

## Merge and installation gates

- [ ] Review the actual scoped production diff against the PR base and identify any separately approved prerequisites.
- [ ] Build a coherent source/header/generated-header/DLL/import-library cohort; preserve unrelated user changes.
- [ ] Run the relevant existing regressions and new positive/negative native tests, including allocator and identity controls.
- [ ] Run negotiated UTF-8 and UTF-16 stdio cases, real close/reopen notifications and no-partial-publication tests.
- [ ] In an isolated VS Code development host, write the complex cases, verify the actual backend, hit a real extension-JS breakpoint, resume that same request, and verify public navigation/reference/rename-preview behavior.
- [ ] Resolve all known failures in the declared matrix; obtain passing CI on the exact PR head and review the final diff.
- [ ] Mark ready and merge the scoped repair PR only after the preceding gates pass.
- [ ] Build the VSIX from the accepted cohort, install it, reload, and verify the installed extension/backend identity and representative requests.

No finite matrix proves every theoretically possible language or LSP boundary. Report the exercised cases and any remaining unsupported or unverified behavior explicitly.
