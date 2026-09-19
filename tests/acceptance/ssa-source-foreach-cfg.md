# SSA source foreach CFG acceptance

## Scope

This slice connects the existing source `foreach` lowering to the canonical
iterator invoke contract. Static, protocol-resolved iterables with one
identifier binding publish `ITER_INIT`, `ITER_MOVE_NEXT`, and `ITER_CURRENT`
as distinct SemanticIR operations. Each operation has one operand, one typed
result, and ordered normal/exception successors. The exception successor is a
zero-instruction propagation sink.

Move-next's normal continuation branches true to current-value retrieval and
false to the join. Current-value retrieval initializes the binding Place only
on its normal body edge. Body fallthrough and direct `continue;` return to
move-next; direct `break;` reaches the exhaustion join. The compiler captures
the pre-iteration slot shape, refreshes the surviving facts after evaluating
the iterable once, and restores that snapshot at the join. Iterable
assignments remain visible while iterable-result and iteration-only temporary
slots are truncated. The established ExecBC iterator instruction sequence is
unchanged.

Dynamic iterator operations, destructuring bindings, unresolved element
types, nonlinear iterables, nonterminal exits, and cleanup-sensitive bodies
remain conservative fallback boundaries. The fallback regression asserts that
cleanup-bearing explicit types are rejected during preflight, before any
canonical iterator operation is emitted. Inferred body declarations are also
kept conservative until their cleanup contract can be resolved before
lowering. Later call-driven CFG startup stays blocked after fallback.

## Focused coverage

`tests/parser/test_pre_semantic_ir_foreach_cfg.inc` verifies:

- the three typed SemanticIR operations and their operand/result chain;
- ordered normal/exception edges and explicit exceptional sinks;
- the move-next condition, continue/fallthrough backedge, and break join;
- normal-edge binding initialization followed by a body load;
- exact restoration of an outer binding's slot identity and value after a
  loop-body write, truncation of temporary slots, and a post-loop read;
- preservation of an iterable assignment's updated value while its result and
  iterator temporary slots are truncated at the join;
- unsupported nested-condition, explicit ownership, source `@close`, and
  inferred-declaration fallback with zero surviving canonical iterator ops;
- declared-child `foreach` isolation from the entry function CFG;
- unchanged legacy `ITER_INIT`, `ITER_MOVE_NEXT`, and `ITER_CURRENT` emission;
- successful SemanticIR validation and ExecIR construction.

The focused file also keeps destructuring on the legacy bytecode path.
`tests/parser/test_pre_semantic_ir_loop_exit_cfg.inc` keeps a nonterminal
`continue` fixture on the persistent fallback path. Both verify that no
canonical iterator operation survives and later call CFG startup stays
blocked.

## Validation

- Windows MSVC Debug: `zr_vm_pre_semantic_ir_test` passed 77/77.
- Windows MSVC Debug: the ten focused SSA adjacency executables passed 10/10
  through `ctest` (`ssa_effects_verifier` through
  `ssa_oracle_projections`).
- WSL GCC 11.4 Debug: `zr_vm_pre_semantic_ir_test` passed 77/77 and the same
  focused SSA adjacency set passed 10/10.
- WSL Clang 14 Debug: `zr_vm_pre_semantic_ir_test` passed 77/77 and the same
  focused SSA adjacency set passed 10/10.
- WSL Clang 14 ASan+UBSan Debug: `zr_vm_pre_semantic_ir_test` passed 77/77
  with leak detection enabled and no sanitizer diagnostic.
- Windows MSVC W2: both iterator regressions passed. The complete executable
  remained at its established 18/20 baseline because
  `test_w2_super_array_add_variable_value_elides_dead_receiver_setup` and
  `test_w2_get_member_slot_direct_result_store_elides_temp_copy` still fail;
  neither failure exercises this phase's source `foreach` CFG path.
- `python scripts/validate_wiki.py --root .` passed for 116 Markdown files,
  115 manifest pages, and 644 local links.
- `python -m unittest tests.scripts.test_validate_wiki -v` passed 5/5.
- `git diff --check` passed for the exact phase file set (line-ending notices
  only).
