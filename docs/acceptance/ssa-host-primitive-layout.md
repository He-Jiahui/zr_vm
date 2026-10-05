---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
  - tests/parser/test_ssa_host_primitive_layout.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/parser-and-semantics/ssa-dead-source-places.md
tests:
  - tests/parser/test_ssa_host_primitive_layout.c
  - tests/acceptance/ssa-host-primitive-layout.md
doc_type: acceptance
status: host-primitive-layout-focused-windows-green-accepted
---

# Host Primitive Layout Acceptance

## Scope and finite decision

The [module contract](../parser-and-semantics/ssa-host-primitive-layout.md)
describes a host-only layout row producer for actual canonical primitive i64.
The shared literal source fixture was committed in `72283127`; it preserves
one actual source preparation path and caller-owned teardown.

Behavioral RED is established and this finite host-row gate is **focused
Windows GREEN accepted**. Independent specification/correctness review has no
blocker. The callable UNSUPPORTED stub executed after successful build and
actual-source prerequisites; a separate fresh production run establishes GREEN.

## Immutable behavioral RED

Committed RED: `57c0dc8a2c9c0f21ec9fcb5fa05504168b8bd2fc`.
Receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/host-primitive-layout-red-receipt.json`.
SHA-256: `8C11B8D55D57CA66499A1892B8A0084D520636196120588665A407E95D3B8BF4`.

Windows x64 clang-cl 19 Debug with assertions and UBSan configure/build exited
0. Actual-source prerequisites passed 2/2 with Oracle 9/8; the compaction suite
passed 30/30. Host CTest exited 8: 15 cases, 12 failures, 0 ignored. The stub
failed three creation features and nine argument/diagnostic boundaries; the
real callable UNSUPPORTED refusal already passed. No UBSan diagnostic was
observed. Native session 44757 ended naturally with exit 8, active 0/stopped 0.
No compile/link failure substitutes for RED.

| Historical RED source | SHA-256 |
| --- | --- |
| Header | `6FAB12EDE6839F78FA57749E45F54378F2AE149D844FB9C5E7C2BF9BE52BF92C` |
| Unsupported TU | `D19D1F2F896A1C70B3AC1B3FAB651E79CD3D729C9E528805B096EBEF15B171A6` |
| Host fixture | `EC3692970210BE55C14781380451A572B37E5AC0A801BB70C9AC398A0A81926D` |
| Shared support header | `17FD2BBBFA6F625999CDFECA89693BAEB46F0F7F4F31AC7FD8A8C7FD6B5F7F39` |
| Shared support TU | `05FB240863B3EE4D3039836B9457D6A40E08BBF65A42711DF7EB2AA7AC8CCA23` |
| Direct CMake | `C0D7164AB56DFFE65BEA1D1D03EDCD3D2EC030947171FEF730705D357DE7DD0F` |
| Ordinary CMake | `86DE914E74FD9B1B28ABF5EF4C4308237B123F23D3148BEEC418EE9A9EF60FAF` |

| RED evidence | SHA-256 |
| --- | --- |
| `host-primitive-layout-red-configure.log` | `122C139EA43C3C98CE0B179F49A6541087EBA86E8E1BBD8D027A82181D80FFE0` |
| `host-primitive-layout-red-build.log` | `C3AB32B8AB23CA2BE80CA65B0990160FEAFFB5DB4745B37CC24ADE85A58B4A72` |
| `host-primitive-layout-prerequisites.log` | `6C32495A5E1B49EFB1A5745905BC3E7F2ED366CF9FCF2A5CA03D9B695C0FBDD1` |
| `host-primitive-layout-compaction-regression.log` | `B776749C07F3B06E7076C5CC165170D50C7F2A5039CB988E5600A27C2598D23F` |
| `host-primitive-layout-red.log` | `6C22B699950660CA7D436E788181EBBCA22094989A63872CA18584D5624D4067` |
| RED binary | `CF83D48273DB4C581387964F696978EBEC8194028B79BB764F0890514EED7D3C` |

The receipt pins target/source/header/settings metadata and both legitimate
shared-support target pairs. These pins describe RED, not the forthcoming
production GREEN or revised unknown-ID prerequisite assertion.

## Required evidence

Root's direct-checkout route builds `zr_vm_ssa_host_primitive_layout_test` and
runs independent `--prerequisites-only` before focused CTest
`ssa_host_primitive_layout`. The RED inventory has 15 cases: 2 source
prerequisites, 3 successful-row cases and 10 argument/shape/admission guard
cases. Fresh GREEN passes the same 15-case inventory, independently checked
2/2 prerequisites and a 30-case compaction regression.

The table/hash cases require actual canonical i64 nodes from real `return 9;`
and `return 8;` contexts. They append real module rows and independently compute
the exact 63 bytes. Dynamic checks distinguish layout IDs and context addresses;
TypeIds and salts are not assumed to differ. Faulted array/node records are
restored before assertions. These requirements establish finite row behavior,
not frame/AOT/native/retained artifact acceptance.

The actual implementation uses explicit ASCII-domain hex bytes and
fixed-width LE serialization, complete eight-byte byte-order comparison and
mixed-order refusal. Host sizeof/alignof values are cast only after u32 bounds
checks; C int64_t/CHAR_BIT and alignof give the storage/alignment facts, without
claiming an additional runtime geometry guard. Descriptor shape/span guards
precede Find; caller-owned sorted/live/readable interner storage remains a
precondition. The unknown-ID test now independently asserts Find returns null
before invoking the API. Those source revisions passed the fresh GREEN run.

Both CMake routes attach the same shared source support TU. Direct metadata
rejects duplicate membership within a target and records every target/source
pair, including legitimate sharing across targets. Header/source/setting hashes,
the final binary and actual terminal logs are pinned by the GREEN receipt.

## Fresh focused Windows GREEN

Immutable receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/host-primitive-layout-green-receipt.json`.
SHA-256: `A6D1A8E883E3A8F3890057AE4635502920826BB89E5CCEE25227A5FB173CC107`.
Its base and RED OID is `57c0dc8a2c9c0f21ec9fcb5fa05504168b8bd2fc`; current
source bytes are pinned independently of that historical stub commit.

Configure/build/independent prerequisites/CTest all exited 0. Prerequisites
passed 2/2; two CTest suites passed: host 15/15 and compaction 30/30, with
0 failures, 0 ignored and no UBSan diagnostic. This run did not execute the
previous eight-suite source-consumer matrix. Native session 6885 ended naturally,
active 0/stopped 0. The source adapter now satisfies host-row/table/hash and
failure-preservation assertions; the RED diagnostic failures are resolved.

| Current GREEN source | SHA-256 |
| --- | --- |
| Host header | `ED1DF6C2372CECC910B9E3887C37AC56CFB46DE2956EAD0D1769E1F77150F836` |
| Host production TU | `46ADA3B39E9D54D7F780A9077B76BA684F2A4461E6667BE22F15407CBC07ECE2` |
| Host fixture | `B939C262062E11B116697ECAEECC85C7CDDBE5F7339A7479FE4BBCAC4DFABDFE` |
| Shared support header | `17FD2BBBFA6F625999CDFECA89693BAEB46F0F7F4F31AC7FD8A8C7FD6B5F7F39` |
| Shared support TU | `05FB240863B3EE4D3039836B9457D6A40E08BBF65A42711DF7EB2AA7AC8CCA23` |
| Direct CMake | `C0D7164AB56DFFE65BEA1D1D03EDCD3D2EC030947171FEF730705D357DE7DD0F` |
| Ordinary CMake | `86DE914E74FD9B1B28ABF5EF4C4308237B123F23D3148BEEC418EE9A9EF60FAF` |
| Current canonical type producer | `8E976561AC0B1D5CD2CEF1C553299B50A8DDAF7BA37A7F44498C87123109B4B7` |
| Current Stable64 producer | `6736DA847C6A6933765F427770D7875BCCEDBBEE34E5D6EDB774C6819BBDFA0A` |

| GREEN evidence | SHA-256 |
| --- | --- |
| `host-primitive-layout-green-configure.log` | `6F66D57038EF2A976518C7D91C3BB29F790A8BA3F88BC0FED2FF6806AAF7A008` |
| `host-primitive-layout-green-build.log` | `95883D119DC1D211854C9ED0DF4163C3F224AB7A026DDC0C254F616DAB2B1C41` |
| `host-primitive-layout-green-prerequisites.log` | `E710BA7F5B941205E8F646BDDA8A088C306E06165822A28C60183E3625874BEF` |
| `host-primitive-layout-green.log` | `F52401C4565EE02A633245EF5C107F25D72EC522DD4528B75BE5F9AE1C25DF8F` |
| `host-primitive-layout-msvc-smoke.log` | `5721EE6B9134F9A4AFDFBF151A7B1096064DFA67E747F376A116E7717B45EFC3` |
| Current host binary | `9C6FD6BE0EBA9F364808FD2763D00076CEC1535D7783F3F5A9D79C5915E4D805` |
| `source-direct-target-sources.tsv` | `E0C6810C4009272F4A078C8296AE69D3E5CF85885F867E388D006BD12433B64E` |
| `source-direct-header-hashes.tsv` | `931CC03C5B6EAFFF03907A39892363C9F266622DED5E2546D8DF3768819A07E9` |
| `source-direct-target-settings.tsv` | `1BB4C68BAD5B9E6D67A391C19A531B5E2A54B8AFF97B3008D5A78E6412C5D183` |

### MSVC compilation boundary

The new production TU also compiled with MSVC14.44.35207 absolute `cl.exe`,
exit 0, naturally terminal. Its full args/INCLUDE/TEMP environment is preserved
in the same GREEN receipt. The output object SHA-256 is
`3FBCD36E091AB7C357386EE09E32E50E477545630D8897D5A1DB71170CB42BB5`;
the args receipt SHA-256 is
`0AABE67A4C0FE221E386324E58D0FB06F49E87B57EAA9CE3D7C1FCBBF323BA5C`.
C4127 constant-condition warnings were emitted at production lines 54 and 106.
This is TU compile-only evidence, not warning-free build or MSVC runtime proof.

## Platform and boundary

Outputs remain on E: under the managed cargo-targets build/report roots. Tests
read the live checkout without a source snapshot or cross-drive copy. Network,
FFI, providers, capabilities and hotpatch/security entry points are not exercised.
The existing WSL `Wsl/EnumerateDistros/Service/E_ACCESSDENIED` leaves Linux
GCC/Clang OPEN. The new MSVC smoke is compile-only as recorded above. Full SSA47
and native/frame/artifact gates remain OPEN.

The [test acceptance guide](../../tests/acceptance/ssa-host-primitive-layout.md)
owns the detailed inventory, commands and final acceptance decision.
