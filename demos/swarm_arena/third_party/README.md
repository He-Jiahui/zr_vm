# GD-M1 dependency provenance

The 2026-10-29 HTTPS unblock supplied all 31 private X11/GL/Jinja packages and
this generated loader. The build uses these existing files; no apt-get or
system package/configuration changes are needed.

| Component | Source | Archive SHA-256 | License / state |
| --- | --- | --- | --- |
| GLFW 3.4 | https://codeload.github.com/glfw/glfw/tar.gz/refs/tags/3.4 | `c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01` | zlib, `glfw/LICENSE.md`; complete upstream source, unchanged |
| glad 2.0.8 generator | https://codeload.github.com/Dav1dde/glad/tar.gz/refs/tags/v2.0.8 | `44f06f9195427c7017f5028d0894f57eb216b0a8f7c4eda7ce883732aeb2d0fc` | Generator MIT; generated code CC0-1.0 (selected alternative) AND Apache-2.0; Khronos header notices retained |

`glad/` contains the already-generated GL 3.3 core C loader, with all 623
extensions selected by the upstream default, generated at 11:06 on 2026-10-29.
No generator/network call is part of CMake. Its three generated source/header
files are unchanged from the unblock. `glad/LICENSE` is the upstream license;
`glad/LICENSE-generated.md` quotes the upstream generated-code grant and names
the selected license. Full CC0-1.0/Apache-2.0 texts are in `glad/licenses/`.

The retained generator is `build/local-tools/sources/glad-2.0.8/`. Generation
used its extracted Jinja2/MarkupSafe dependencies, API `gl:core=3.3`, C output,
no built-in loader (`glfwGetProcAddress` supplies entry points). The generation
log records the Khronos registry/header HTTPS URLs; the generated file hashes
are preserved in the resume evidence. The generator archive and Debian
packages remain ignored under `build/local-tools/downloads/`.

Debian package SHA-256, control/copyright files, original archives and logs:
`/workspace/zr_vm-review/evidence/gd-m1-20261029/` (`debs.sha256`,
`deb-control/`, `archives.sha256`, `glad-generate-1102.log`). The resume verified
all 31 checksums. Build/test status and outstanding acceptance items:
`/workspace/zr_vm-review/codex-zr-gd-m1-resume-20261029.md`.
