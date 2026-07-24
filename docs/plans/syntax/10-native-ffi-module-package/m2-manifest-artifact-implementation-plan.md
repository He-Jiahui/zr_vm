# Syntax 10R M2 Manifest And Artifact Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `executing-plans` task by task. Each checked task is a
> separately committed Syntax 10R M2 sub-milestone.

**Goal:** Replace the v1-only project-manifest boundary with a domain-aware `.zrp` v2 reader/writer, then
connect exports, dependency lock identity, artifact entries, and provider phases without replacing the 06A
migration resolver prematurely.

**Architecture:** Keep `project.c` as the legacy project lifecycle owner, but move new version/schema work into
small `project_manifest_v2_*` modules. M1 `ModuleSpecifier` and `ModuleIdentity` are the only source spelling
and canonical-identity substrate. The legacy v1 reader remains an explicit migration path until every v2
consumer has passed its gate.

**Tech Stack:** C17, cJSON, Unity, CMake/CTest, WSL GCC/Clang, MSVC Debug.

---

## File Structure

- `zr_vm_library/include/zr_vm_library/project.h`: public project manifest version and later v2 resolver facts.
- `zr_vm_library/src/zr_vm_library/project/project_manifest_v2.h/.c`: focused v2 schema validators, readers,
  canonical writer, and lock projection helpers.
- `zr_vm_library/src/zr_vm_library/project/project.c`: narrow lifecycle dispatch only; it must not regain v2
  parsing loops.
- `tests/library/test_project_manifest_v2.c`: v2 TDD boundary and writer round-trip regression.
- `tests/library/test_project_manifest_normalization.c`: unchanged v1 migration guard.
- `tests/CMakeLists.txt`: focused M2 CTest registration.
- `docs/module-system/module-specifier-identity.md`, `docs/module-system/zrm-assembly-container.md`: module
  contract documentation.

## Task 1: M2.1 V2 Base Envelope Admission

**Files:**
- Create: `zr_vm_library/src/zr_vm_library/project/project_manifest_v2.h`
- Create: `zr_vm_library/src/zr_vm_library/project/project_manifest_v2.c`
- Create: `tests/library/test_project_manifest_v2.c`
- Modify: `zr_vm_library/include/zr_vm_library/project.h`
- Modify: `zr_vm_library/src/zr_vm_library/project/project.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/module-system/module-specifier-identity.md`
- Create: `docs/plans/syntax/10-native-ffi-module-package/m2-v2-manifest-admission.md`

- [x] **Step 1: Write the failing v2 admission test**

```c
static void test_project_manifest_v2_reads_required_base_envelope(void) {
    SZrLibrary_Project *project = new_project("{\"manifestVersion\":2,\"name\":\"physics\","
                                               "\"version\":\"1.0.0\",\"kind\":\"library\","
                                               "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(2u, project->manifestVersion);
    TEST_ASSERT_EQUAL_STRING("physics", string_text(project->name));
    TEST_ASSERT_EQUAL_STRING("1.0.0", string_text(project->version));
}
```

Also fix negative cases for a non-integral/unsupported manifest version and for v2 without one of `name`,
`version`, `kind`, `source`, `binary`, or `entry`; retain a v1 manifest normalization regression.

- [x] **Step 2: Run the focused target and capture RED**

Run:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cmake --build .codex/build-s10r-m2-gcc --target zr_vm_project_manifest_v2_test -j12 && ./.codex/build-s10r-m2-gcc/bin/zr_vm_project_manifest_v2_test'
```

Expected: the v2 envelope is rejected because the existing version gate accepts only v1.

- [x] **Step 3: Implement the minimal reader split**

```c
TZrBool library_project_manifest_validate_version(cJSON *manifestJson, TZrUInt32 *outManifestVersion) {
    /* Missing manifestVersion remains migration v1; only exact integers 1 and 2 are admitted. */
}

TZrBool library_project_manifest_v2_validate_base(cJSON *manifestJson) {
    /* Require nonempty name/version/kind/source/binary/entry strings before Project_New consumes them. */
}
```

Set `project->manifestVersion` in `ZrLibrary_Project_New`, call the v2 base validator only for version 2, and
preserve top-level v2 `version` when no legacy `assembly.version` is present. No alias, dependency, provider,
or writer behavior is added in this sub-milestone.

- [x] **Step 4: Re-run focused and v1 regression tests**

Run the v2 target and `zr_vm_project_manifest_normalization_test`; both must pass. Review the diff to ensure
`project.c` only dispatches to the new module and owns no new schema loop.

- [x] **Step 5: Complete M2.1 evidence and commit**

Run the focused test under GCC, Clang, and MSVC, record status/time/output in
`docs/plans/syntax/10-native-ffi-module-package/m2-v2-manifest-admission.md`, and exact-stage only Task 1
paths for one commit.

## Task 2: M2.2 V2 Package, Alias, And Dependency Declarations

**Files:**
- Modify: `project_manifest_v2.h/.c`, `project.h`, and the focused v2 test
- Modify: `project.c` only to install/free the parsed v2 data
- Modify: `docs/module-system/module-specifier-identity.md`

- [ ] **Step 1: Add RED cases for `aliases`, `package`, and `dependencies`**

```c
/* #alias targets parse through ZrLibrary_ModuleSpecifier_Parse; package keys use exactly @identifier.
 * A dependency declares exactly one source and a version requirement. */
```

Include rejection for `pathAliases`, `$dependency`, `@org/math` package roots, alias-to-alias recursion, and
an unexported package submodule.

- [ ] **Step 2: Implement structured declaration storage**

Store alias target specifiers, package root/export pairs, and dependency package identity/version requirements
as structured fields. Do not turn `#`/`@` prefixes into stripped strings or choose a filesystem provider.

- [ ] **Step 3: Verify resolution boundary**

Resolve exactly one alias or package export through M1 data, preserving the target domain. Existing v1 alias,
`$dependency`, and `&dependency` inputs stay only in the migration adapter.

- [ ] **Step 4: Validate and commit M2.2**

Run focused v2/legacy resolver tests across GCC, Clang, MSVC; update the M2 status record and commit only its
exact paths.

## Task 3: M2.3 Canonical V2 Writer And Dependency Lock Projection

**Files:**
- Modify: `project_manifest_v2.h/.c` and focused v2 test
- Create: `tests/library/test_project_manifest_v2_writer.c` if the existing test becomes unwieldy
- Modify: module docs and the M2 status record

- [ ] **Step 1: Write round-trip RED**

```c
TEST_ASSERT_TRUE(ZrLibrary_ProjectManifestV2_Write(project, output, sizeof(output)));
TEST_ASSERT_NOT_NULL(ZrLibrary_Project_New(state, output, projectPath));
TEST_ASSERT_NULL(strstr(output, "pathAliases"));
TEST_ASSERT_NULL(strstr(output, "C:/"));
```

- [ ] **Step 2: Implement canonical writer**

Emit only v2 spellings (`#alias`, `@package`) in deterministic key order. Write resolved package version,
content hash, transitive identity, and provider facts into a separate lock projection; never serialize local
absolute cache paths into the manifest.

- [ ] **Step 3: Validate and commit M2.3**

Verify read-write-read identity equivalence, writer rejection of incomplete declarations, and migration-reader
separation across all three toolchains before an exact-path commit.

## Task 4: M2.4 Artifact Entry And Provider Phase Bridge

**Files:**
- Modify: `project_manifest_v2.*`, provider-location modules, `.zrm` reader/writer modules, and focused tests
- Modify: project public facts only where provider kind/phase must be returned structurally
- Modify: `docs/module-system/zrm-assembly-container.md` and the M2 status record

- [ ] **Step 1: Write RED for `.zrm` default entry and phase mismatch**

```c
/* Package root maps to an explicit exported/default zrm entry; Runtime cannot consume CompileTool provider. */
```

- [ ] **Step 2: Add resolver-result facts**

Return selected provider kind, provider phase, artifact entry, and contract hash separately from
`SZrLibrary_ModuleIdentity`; reject unknown domain tags, identity mismatch, and phase mismatch before load.

- [ ] **Step 3: Add `.zro` dependency round-trip**

Serialize and reload the domain-aware dependency identity without locator-derived TypeId fields. Test source,
zrm, and descriptor provider selection with the same identity.

- [ ] **Step 4: Validate M2 gate and commit M2.4**

Run manifest/resolver/artifact tests and consumer smoke under GCC, Clang, and MSVC. Update the M2 record with
the gate evidence and commit only the final exact path set.

## Coverage Check

- M2.1 covers v2 version admission and required base envelope without changing migration resolution.
- M2.2 covers v2 aliases, package exports, and dependency declaration identity.
- M2.3 covers deterministic v2 output and non-portable locator exclusion from manifest output.
- M2.4 covers artifact entries, provider phase, and `.zro` dependency round-trip.
- FFI ABI, native extern syntax, concrete `zr.*` provider inventory, and repository-wide 06B migration remain
  outside this plan.
