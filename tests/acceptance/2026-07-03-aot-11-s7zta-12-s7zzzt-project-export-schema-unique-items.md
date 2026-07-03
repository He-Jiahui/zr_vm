# AOT 11-S7ZTA / 12-S7ZZZT Project Export Schema Unique Items

## Scope
- Tightens `.zrp` schema parity for project manifest export declarations.
- Affected layer: `zr_vm_language_server_extension/schemas/zrp.schema.json`.

## Baseline
- Before this slice, the `exports` schema described the array and item shape but did not reject fully duplicated export declaration objects.
- The project parser and generated manifest export table now reject duplicate `kind + target` entries, so schema-aware tooling should provide at least the matching exact-object duplicate feedback.
- JSON Schema draft-07 cannot express the full semantic `kind + target` uniqueness constraint while unknown item properties remain allowed; parser/generator checks remain authoritative for that case.

## Test Inventory
- Schema assertion:
  - Reads `zrp.schema.json`.
  - Requires `properties.exports.uniqueItems == true`.
- JSON syntax validation:
  - `python -m json.tool` over the schema file.

## Tooling Evidence
- RED:
  - `python -c "import json; schema=json.load(open('zr_vm_language_server_extension/schemas/zrp.schema.json', encoding='utf-8')); assert schema['properties']['exports'].get('uniqueItems') is True"`
  - Failed with `AssertionError`.
- GREEN:
  - Same assertion passed after adding `uniqueItems: true`.
  - `python -m json.tool .\zr_vm_language_server_extension\schemas\zrp.schema.json` passed.

## Results
- Schema exact duplicate export object parity is now present.
- The schema remains intentionally less strict than the parser for semantic duplicates with different extra properties.

## Acceptance Decision
- Accepted for 11-S7ZTA / 12-S7ZZZT.
- Remaining risks are outside this slice: full semantic uniqueness belongs to the project parser/generated table, and broader metadata sweep/pruning, full trim analyzer, annotation/promotion policy, provider binding edges, and ABI drift/deopt closure remain open.
