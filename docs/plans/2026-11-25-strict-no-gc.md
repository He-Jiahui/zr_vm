# `StrictNoGC`: ownership first for zr_vm

## Contract

Set `"StrictNoGC": true` in the `.zrp` to make ordinary `new` a compile error and run the project without a garbage collector or managed objects. The normal way to write strict Zr code is to **use values by default, borrow when passing them around, and use `own` when they need a heap lifetime**. Pools are an optional storage optimization.

```json
{
  "manifestVersion": 3,
  "StrictNoGC": true
}
```

Manifest version 3 keeps the version 2 format and makes older tools reject the project instead of silently ignoring the flag. Missing or `false` preserves ordinary behavior. Support both the interpreter and AOT C. The guarantee covers project startup through shutdown; compiler and LSP internals are outside it.

## Ownership and cleanup

| Zr operation | Strict behavior |
|---|---|
| `init T(...)` | Construct an inline value in its destination. Use scoped `ref`, `ref readonly`, or `ref struct` views to access it without taking ownership. |
| `own T(...)` | Explicitly allocate a `Unique<T>` on the manual heap. The owner moves but cannot be copied. `drop` or scope exit destroys the value and releases its storage. |
| `share(unique)` | Consume the unique owner and produce a `Shared<T>`. Copies retain it; releasing the last strong owner destroys the payload. Shared access remains readonly. |
| `degrade(shared)` / `wake(weak)` | Create a non-owning `Weak<T>` and explicitly try to upgrade it. `wake` returns null after the last strong owner is gone. |
| `new T(...)` / `intoGc(...)` | Always rejected. A `let` or `readonly` declaration does not exempt the initializer. |

```zr
let student: Unique<Student> = own Student(42);
let shared: Shared<Student> = share(student);
let observer: Weak<Student> = degrade(shared);

drop(shared);
let expired = wake(observer); // null: no strong owner remains
```

- Support fixed-layout structs and resource classes whose methods and destructor resolve statically. Reuse existing move and borrow analysis to reject use after move, borrowing past an owner’s lifetime, and moving, sharing, or dropping an owner while it is borrowed.
- Give strict owners a dedicated representation. The existing construction path can construct a managed object before wrapping it in an owner, and current resource cleanup uses GC roots and pins. In strict mode, `own` allocates one block with the payload and its ownership header. `share`, `degrade`, and `wake` need no heap allocation. Weak handles can keep the backing block alive, but the payload has already been destroyed when the last strong owner goes away. Start with single-threaded ownership.
- Drop initialized values exactly once on normal exits and VM-controlled faults. If `own` runs out of memory, report an allocation failure and unwind without allocating. If construction fails partway through, drop only the fields already initialized. Verify that destructors and their callees cannot allocate. Keep a separate fallible `ManualBlock.tryAlloc` API for explicitly owned byte buffers with scoped views.
- Allow strong cycles, as Rust does with `Rc`. Use `Weak` for parent links and other back-references. Debug shutdown reports surviving strong owners by type and allocation site, then releases the runtime’s remaining backing memory without tracing the object graph. User destructors in a surviving cycle are not guaranteed to run. Preserve the program’s original exit status.

## Compiler and runtime enforcement

1. **Reject illegal construction at the source.** Check every source module in the dependency graph before optimization, including dead code and static initializers. Distinguish explicit `own` from ordinary `new` even where today’s lowering shares a construction path. Tell the author which alternative fits: `init` for inline storage, a borrow for temporary access, or `own` for an escaping owner. Surface the same diagnostics in the CLI and LSP.
2. **Verify the executable as well.** Allow only known strict layouts, operations, and call targets. Reject hidden managed allocation from boxing, collections, strings, capturing closures, reflection, or suspension frames. Reject unverified binaries, general FFI, and managed fallback. Put the strict policy and ABI version in the artifact and cache key. Add a CLI `--require-strict-no-gc` check so CI can reject a change that removes the setting instead of accidentally building in managed mode.
3. **Use one strict runtime for both backends.** Select it before managed VM startup. The interpreter executes verified typed IR; AOT C uses the same strict allocation, ownership, and cleanup helpers. Neither path creates a collector or GC domain. Ordinary projects keep their existing runtime and ownership behavior.

## Optional pools and acceptance

Deliver ownership first, then add `StaticPool<T>` for workloads that benefit from slot reuse. A pool handle is a non-owning slot identity; it does not use `Weak<T>` or `wake`. Pools grow automatically by slabs, may have a hard cap, and shrink only on explicit request. Recycle invalidates all copies of a handle immediately and clears the slot after the last open view closes. Debug shutdown reports and returns outstanding rentals before freeing the slabs, preserving the original exit status.

Run the same ownership fixtures in the interpreter and AOT C. Verify zero collector creation, managed allocation, or GC steps across startup, execution, failure, and shutdown. Test move and borrow errors, weak expiry, exact-once drop, partial construction, allocation failure, and cycle reporting. Verify that explicit and hidden managed allocation fail to compile, that old artifacts fail to load, and that CI rejects a disabled flag. For pools, test stale handles across recycle and shrink/regrow, plus debug cleanup of unrecycled slots.
