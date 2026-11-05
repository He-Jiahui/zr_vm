# Runtime optional receiver ABI (GD-H2-R5-U-B)

An immediate named member call without statically classified member metadata stages
`[callee, receiver, explicit arguments...]`. Member lookup and argument evaluation
still happen once, in their existing order. Calls with known member metadata keep
their existing ABI.
Member extraction (`var f = value.method`) does not bind a receiver.

Callsite-cache kinds 12 (`DYN_MEMBER_CALL`), 13 (`DYN_MEMBER_TAIL_CALL`), and 14
(`DYN_MEMBER_CALL_SPREAD`) mark this convention. The first two use the existing
cached dynamic call opcodes and their cache-index operand. Spread uses
`FUNCTION_CALL_SPREAD`, with its cache identified by instruction index.
`memberEntryIndex` names the looked-up member. `argumentCount` includes the staged
receiver; for spread it counts only the fixed prefix, including that receiver.

`ZrCore_Object_MemberCallUsesReceiver` decides from the resolved callee on every
invocation, including polymorphic sites. It retains the receiver for a nonstatic
`METHOD` descriptor whose `methodFunction` is the exact callee function (or a VM
closure's function). Native closures marked `RECEIVER_FRAME` retain it;
`RECEIVER_CAPTURED` already owns its receiver and drops the staged one. A native
AOT shim can identify its underlying method function. Other callable values drop
the receiver. Arity and member names alone do not determine this decision.
Core built-in prototype methods now declare `RECEIVER_FRAME` at registration,
including methods called through `ZrCore_Object_InvokeMember`. Permanent stateless
built-ins with this flag remain eligible for session checkpoints; captured native
receivers remain subject to the existing checkpoint restrictions.

Own fields retain lookup precedence. A different function shadowing a prototype
method is an ordinary callable field; the same method function still satisfies
the identity rule. `ZrCore_Object_InvokeMember` uses the same predicate for own
fields and described members. This replaces its old behavior of supplying the
receiver to all nonstatic own-field closures. The host API preserves its legacy
receiver for inherited, descriptorless prototype callables (including the
`__constructor` protocol), except captured native bindings. This compatibility
path does not apply to VM optional member calls or to own-field shadows.
Callable objects retain their own `@call` receiver,
without receiving the outer object's staged receiver.

Dropping advances the callable over the receiver slot. Explicit argument slots
stay in place. Normal, tail, and spread VM paths use this normalization. Both C
and LLVM AOT call `ZrLibrary_AotRuntime_CallOptionalReceiver`, which uses the same
predicate before entering the existing call bridge. No bound-method allocation
is introduced. The AOT direct bridge consumes staged dense VALUE arguments rather
than rereading stale physical slots. Calls containing inline struct arguments
continue using their original frame layout. C scalar constants used by cached
dynamic calls are materialized into the argument window, including calls with
more than two arguments.
Optional call results invalidate reaching scalar facts in the C emitter: an
earlier integer in a reused temporary cannot classify the new boxed result.

The serialized `.zro` patch is 47. Cache rows retain their five-u32 layout, but
readers reject optional receiver kinds under older patches and validate their
member index, call opcode, instruction index, and argument prefix. Patch 46
nonoptional artifacts remain readable; older runtimes reject patch 47.

The AOT descriptor and execution contract ABIs are 18. AOT loading and the
optional host JIT facade reject ABI 17
before registration. Nested ExecIR artifacts and their golden fixtures carry the
same ABI version. This checkout has no host machine-code provider: its
unavailable/fallback path remains explicit, and execution uses the updated VM.
Any future provider must implement these kinds or deopt to that VM with the
staged window intact. A facade contract test is not machine-code execution proof.
