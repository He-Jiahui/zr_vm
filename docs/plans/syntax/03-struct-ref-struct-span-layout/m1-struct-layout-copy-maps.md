# 03-M1 Struct layout、copy 分类与通用 map 表示里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md`
的 `M1 Struct layout、copy 分类与通用 map 表示`。

## 状态与产出记录

- 完成时间：2026-07-20 20:34 +08:00
- 状态：已完成
- 完成项目：
  - 新增 contextual `init TypeRef(...)` AST，覆盖 qualified/constructed generic TypeRef、
    positional/named/default arguments；`init()` 保持 ordinary call，`new`、ownership construct
    与普通 call 继续使用独立语义路径。
  - 新增 `SZrBoundValueConstruct` 与 canonical constructor set，冻结 `TypeId + constructor
    SymbolId + parameter map`；显式 constructor 存在时不生成 raw-zero default，也不向
    `@call`、class allocation 或 runtime prototype fallback。
  - 新增 `VALUE_CONSTRUCT(destination PlaceId, TypeId, constructorId, arguments)` 与
    `FIELD_INITIALIZE` Semantic IR；local、inline field、fixed-array element 和 return
    construction 直接写最终 Place，不创建普通 struct object wrapper。
  - TypeLayout schema-v1 统一 size/alignment、field spans、nested layout identity、
    bitwise/fieldwise/move-only copy、none/fieldwise/customThenFields drop、GC scan kind、
    GC/ownership/ref maps、layout version/hash 与结构校验。
  - VM、metadata runtime、reflection、AOT C descriptor/runtime 共用同一 TypeLayout registry；
    generated C 发布相同 version/hash/copy/drop/scan/map contract，artifact roundtrip 保留
    constructor frame layout 与 bitmap flag；runtime focused 用例另行验证 receiver indirect alias。
  - inline struct array 使用对齐 object tail 与 layout id/hash 门禁；元素初始化、索引 Place、
    GC mark/rewrite 和 reverse drop 均通过 registry 执行，不把元素解释为 boxed values。
  - 显式 constructor receiver 绑定最终 inline destination；field store 更新 frame bitmap，
    VM unwind 只对已初始化字段执行逆序 partial drop，AOT generated cleanup 发布同一调用合同。
  - GCC 11.4 与 Clang 14 focused 各 106/106 PASS；MSVC 19.44 focused 为 105 PASS、
    0 FAIL、1 个既有 Unix-only shared-library IGNORE。三套
    `zr_vm_compiler_integration_test` 均 127/127 PASS 且真实进程 exit 0。
- 验收证据：
  - `tests/parser/test_struct_value_init.c`
  - `tests/core/test_type_layout_inline_copy.c`
  - `tests/core/test_type_layout_metadata_contracts.c`
  - `tests/core/test_inline_struct_array_layout.c`
  - `tests/module/test_metadata_runtime_type_layout.c`
  - `tests/parser/test_pre_semantic_ir.c`
  - `tests/parser/test_artifact_schema.c`
  - `tests/parser/test_aot_c_value_semir_contracts.c`
  - `tests/parser/test_aot_c_value_type_shared_library_smoke.c`
  - GCC/Clang/MSVC compiler integration 日志：
    `.codex/logs/s03m1-compiler-integration-{gcc,clang,msvc}.log`
- 里程碑提交：实现、测试、模块文档与本记录随
  `feat(syntax): add destination-first struct value layout` 一并提交。

## 边界与后继

- M1 冻结通用 field-slot/map 表示，但不宣称 resource/owner 的 retain/release、custom
  field teardown 或 GC bridge 已完成；这些语义由 Syntax 04 promotion gate 填充。
- VM 已验证 constructor throw 在当前 frame 内按 bitmap partial unwind 并进入 source catch；
  generated AOT C 已验证发出相同 cleanup contract。AOT direct callee 抛错后恢复 caller catch
  仍依赖完整跨函数异常协议，属于 Syntax 04，不以本里程碑的 generated-source contract
  冒充端到端执行通过。
- M2 继续在已冻结 TypeId/TypeLayout/receiver Place 上实现 class、struct、readonly struct、
  interface、override、method reference 与 property accessor 的 receiver effect，不按类型名或
  member name 推断。
