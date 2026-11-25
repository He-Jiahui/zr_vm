# StrictNoGC projects

In a version 3 `.zrp` manifest, add `"StrictNoGC": true`. The compiler rejects ordinary `new`, `intoGc`, and operations that need the managed runtime, including in unused functions and imported project sources. Older tools reject the version 3 manifest. Leaving the flag off keeps normal project behavior.

Use `init` for an inline value, scoped `ref` or `ref readonly` for access without ownership, and `own` when a value needs a manual heap lifetime. A `Unique<T>` moves. `share` consumes it and yields a readonly `Shared<T>`, which may be copied. The last strong owner destroys the payload. Use `degrade` for back references so they do not keep the payload alive.

```zr
resource class Student {
    pub var id: int;
    pub @constructor(id: int) { this.id = id; }
}

let student = own Student(42);
let owner = share(student);
let observer = degrade(owner);
let candidate = wake(observer);
if (candidate != null) {
    drop(owner);
    let recovered: Shared<Student> = candidate;
    return recovered.id;
}
return 0;
```

`wake` returns null if the payload is already gone. Inside the non-null branch you can read the payload, borrow it as `ref readonly`, or keep a strong reference with an explicit `Shared<T>` binding. The test must guard the same binding that you use.

In CI, require the project to stay strict even if someone edits its manifest:

```sh
zr_vm_cli --require-strict-no-gc --compile app.zrp --emit-aot-c
zr_vm_cli --require-strict-no-gc app.zrp --execution-mode binary
```

The first command writes a checked `.zro` and native C source. The second runs the checked binary in the strict interpreter. The generated C links against `zr_vm_strict_runtime` and uses the same ownership and cleanup rules. Rebuild artifacts when the tool reports a strict ABI mismatch.

The no-GC guarantee applies from project startup through teardown. The compiler and language server use their own runtime. Explicit manual allocations still allocate host memory; `StrictNoGC` does not promise an allocation-free workload. Strong reference cycles can keep payloads alive until shutdown, so use weak references for back edges.

## Optional pools

Use `StaticPool<T>` if a workload benefits from reusing slots. A pool has one owner and grows in slabs as needed. The `create<T>` argument is the hard cap in slots; `0` leaves the cap open. Keep a pool in a long-lived `let` binding when you want to reuse its storage across calls.

```zr
let pool: StaticPool<Student> = StaticPool.create<Student>(256);
let rental: PoolHandle<Student> = pool.tryRent(42);
if (pool.contains(rental)) {
    let view: ref readonly Student = pool.view(rental);
    pool.recycle(rental);
    return view.id;
}
return 0;
```

`tryRent` runs the record constructor and returns null if it cannot rent a slot. `PoolHandle<T>` is copyable and has no ownership. Use `pool.contains(handle)` to check whether a rental is still live. `view` faults on an expired handle; it returns a scoped readonly view on success. Recycling invalidates every copy of a handle immediately; views already open remain valid until their scope ends, then the payload is destroyed and its slot is cleared. `pool.shrink()` frees only empty slabs and returns the number freed. Debug shutdown reports outstanding rentals and disposes of them without changing the program's exit status.
