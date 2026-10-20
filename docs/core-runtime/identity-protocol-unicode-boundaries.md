---
related_code:
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/utf8.c
  - zr_vm_core/src/zr_vm_core/hash_set.c
  - zr_vm_core/src/zr_vm_core/value.c
  - tests/core/test_identity_protocol_unicode.c
  - tests/core/test_identity_protocol_unicode_boundaries.c
---

# Identity, protocol, and UTF-8 boundaries

`ZrCore_RawObject_Construct` initializes an ordinary object's identity hash
from the address of its raw-object header. String construction replaces this
with the content hash. GC evacuation copies the raw header, including its hash,
while hash-set key rewriting updates the stored object reference. Hash equality
selects a bucket; object-key equality still compares object identity.

Compiled prototype metadata stores protocol capabilities in a 64-bit mask.
When module loading creates runtime prototypes, it must restore every defined
protocol bit through `ZR_PROTOCOL_ID_MAX`. The boundary regression uses the
complete currently defined range, including protocol 7 and the current maximum.

UTF-8 encoding accepts Unicode scalar values from U+0000 through U+10FFFF and
rejects U+D800 through U+DFFF. Failed scalar validation leaves both the output
buffer and output length unchanged. Successful encoding reports the number of
bytes excluding the optional NUL terminator.

The boundary test source needs a Unity test target linked with the parser/core
runtime test dependencies. This boundary test does not force an object-moving
GC cycle: the nearby minor-GC fixture allows either in-place survival or
forwarding, and the full-compaction fixture checks stable addresses. A
deterministic movement-and-rehash regression remains open. This note records
the intended contracts; it does not assert that the test has been built or run.
