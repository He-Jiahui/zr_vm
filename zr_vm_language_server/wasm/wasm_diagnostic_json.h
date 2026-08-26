#ifndef ZR_VM_LANGUAGE_SERVER_WASM_DIAGNOSTIC_JSON_H
#define ZR_VM_LANGUAGE_SERVER_WASM_DIAGNOSTIC_JSON_H

struct cJSON;
struct SZrArray;
struct SZrState;
struct SZrString;

cJSON *ZrLanguageServer_Wasm_SerializeDiagnostics(
        SZrState *state,
        SZrArray *diagnostics,
        const SZrString *uri);

#endif // ZR_VM_LANGUAGE_SERVER_WASM_DIAGNOSTIC_JSON_H
