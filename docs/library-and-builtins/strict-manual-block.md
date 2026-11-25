# Manual byte buffers in StrictNoGC

Use `ManualBlock.tryAlloc(byteCount)` when a project with `"StrictNoGC": true` needs a byte buffer whose length is only known at runtime. The allocation is explicit and returns `null` if the count is invalid or the allocation fails. A zero length is valid. A successful buffer contains zeroes until it is written.

```zr
let packet: ManualBlock = ManualBlock.tryAlloc(64);
if (packet == null) { return -1; }

if (true) {
    let bytes: ref ManualBlock = ref packet;
    bytes[0] = 42;
}

let moved = packet;
let bytes: ref readonly ManualBlock = ref moved;
return bytes[0];
```

`ManualBlock` is an owner. Binding it to another local moves it; dropping it or leaving its scope releases the bytes. A writable view is exclusive. Several readonly views can coexist. While a view is open, the owner cannot move or be dropped. A view does not retain the allocation or escape its scope.

Use `view.length` for the byte count. Indexing reads an `int` in `0..255`; writing requires a writable view and an `int` in that range. Negative indices, indices at or beyond `length`, and out-of-range writes fault. Borrowing a null result also faults, so test the result of `tryAlloc` before borrowing. The strict runtime releases live owners when it unwinds these VM faults.
