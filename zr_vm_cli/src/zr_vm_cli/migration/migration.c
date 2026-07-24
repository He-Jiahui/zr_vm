#include "migration/migration.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "project/project.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/legacy_migration.h"

static const TZrChar *zr_cli_migration_applicability_name(
        EZrLegacyMigrationApplicability applicability) {
    switch (applicability) {
        case ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE:
            return "machineApplicable";
        case ZR_LEGACY_MIGRATION_MAYBE_INCORRECT:
            return "maybeIncorrect";
        case ZR_LEGACY_MIGRATION_REQUIRES_REVIEW:
            return "requiresReview";
        case ZR_LEGACY_MIGRATION_BLOCKED:
            return "blocked";
        case ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED:
            return "targetNotPromoted";
        default:
            return "unknown";
    }
}

static void zr_cli_migration_write_json_string(FILE *output, const TZrChar *text) {
    const TZrChar *cursor = text != ZR_NULL ? text : "";

    fputc('"', output);
    while (*cursor != '\0') {
        unsigned char value = (unsigned char)*cursor++;

        if (value == '"' || value == '\\') {
            fputc('\\', output);
            fputc(value, output);
        } else if (value == '\n') {
            fputs("\\n", output);
        } else if (value == '\r') {
            fputs("\\r", output);
        } else if (value == '\t') {
            fputs("\\t", output);
        } else if (value < 0x20U) {
            fprintf(output, "\\u%04x", value);
        } else {
            fputc(value, output);
        }
    }
    fputc('"', output);
}

static void zr_cli_migration_write_json_path(FILE *output, const TZrChar *path) {
    const TZrChar *cursor = path != ZR_NULL ? path : "";

    fputc('"', output);
    while (*cursor != '\0') {
        TZrChar value = *cursor++;

        if (value == '\\') {
            fputc('/', output);
        } else if (value == '"') {
            fputs("\\\"", output);
        } else {
            fputc(value, output);
        }
    }
    fputc('"', output);
}

static const TZrChar *zr_cli_migration_string_text(const SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString((SZrString *)value) : "";
}

static void zr_cli_migration_write_json_item(
        FILE *output,
        const SZrLegacyMigrationItem *item,
        const TZrChar *path) {
    const SZrFileRange *range = &item->range;

    fputs("{\"diagnosticCode\":", output);
    zr_cli_migration_write_json_string(output, zr_cli_migration_string_text(item->diagnosticCode));
    fputs(",\"file\":", output);
    zr_cli_migration_write_json_path(output, path);
    fprintf(output,
            ",\"range\":{\"start\":%zu,\"end\":%zu}",
            (size_t)range->start.offset,
            (size_t)range->end.offset);
    fputs(",\"oldConstructKind\":", output);
    zr_cli_migration_write_json_string(output, zr_cli_migration_string_text(item->oldConstructKind));
    fputs(",\"targetConstructKind\":", output);
    zr_cli_migration_write_json_string(output, zr_cli_migration_string_text(item->targetConstructKind));
    fputs(",\"oldTargetBindingKind\":", output);
    zr_cli_migration_write_json_string(output, zr_cli_migration_string_text(item->oldTargetBindingKind));
    fputs(",\"resolvedTargetTypeId\":null", output);
    fputs(",\"applicability\":", output);
    zr_cli_migration_write_json_string(output, zr_cli_migration_applicability_name(item->applicability));
    fputs(",\"targetPlanId\":", output);
    zr_cli_migration_write_json_string(output, zr_cli_migration_string_text(item->targetPlanId));
    fprintf(output,
            ",\"targetPromotionGate\":%s",
            item->applicability == ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED ? "true" : "false");
    fprintf(output, ",\"hasFix\":%s", item->hasFix ? "true" : "false");
    fputs(",\"edits\":[", output);
    if (item->hasFix) {
        fprintf(output,
                "{\"range\":{\"start\":%zu,\"end\":%zu},\"text\":",
                (size_t)item->fix.editRange.start.offset,
                (size_t)item->fix.editRange.end.offset);
        zr_cli_migration_write_json_string(output, zr_cli_migration_string_text(item->fix.editText));
        fputc('}', output);
    }
    fputs("],\"relatedDeclarations\":[],\"reason\":", output);
    zr_cli_migration_write_json_string(output, zr_cli_migration_string_text(item->reason));
    fputc('}', output);
}

static void zr_cli_migration_write_report(
        FILE *output,
        const SZrCliCommand *command,
        const SZrLegacyMigrationPlan *plan,
        const TZrChar *path) {
    TZrSize index;

    if (command->migrationFormat == ZR_CLI_MIGRATION_FORMAT_TEXT) {
        for (index = 0U; index < plan->items.length; index++) {
            const SZrLegacyMigrationItem *item =
                    (const SZrLegacyMigrationItem *)ZrCore_Array_Get((SZrArray *)&plan->items, index);
            fprintf(output,
                    "%s:%zu-%zu %s %s%s\n",
                    path,
                    (size_t)item->range.start.offset,
                    (size_t)item->range.end.offset,
                    zr_cli_migration_applicability_name(item->applicability),
                    zr_cli_migration_string_text(item->oldConstructKind),
                    item->hasFix ? " fix" : "");
        }
        return;
    }
    fprintf(output,
            "{\"schemaVersion\":1,\"path\":");
    zr_cli_migration_write_json_path(output, path);
    fprintf(output,
            ",\"write\":%s,\"sourceHash\":%llu,\"items\":[",
            command->migrationWrite ? "true" : "false",
            (unsigned long long)plan->sourceHash);
    for (index = 0U; index < plan->items.length; index++) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get((SZrArray *)&plan->items, index);
        if (index > 0U) {
            fputc(',', output);
        }
        zr_cli_migration_write_json_item(output, item, path);
    }
    fputs("]}\n", output);
}

static TZrBool zr_cli_migration_write_file_atomically(
        const TZrChar *path,
        const TZrChar *text,
        TZrSize length) {
    TZrChar temporaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    FILE *file;
    TZrBool result = ZR_FALSE;
    TZrUInt32 attempt;

#ifdef _WIN32
    TZrUInt32 processId = (TZrUInt32)GetCurrentProcessId();
#else
    TZrUInt32 processId = (TZrUInt32)getpid();
#endif

    if (path == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }
    file = ZR_NULL;
    for (attempt = 0U; attempt < 128U; attempt++) {
        if (snprintf(temporaryPath,
                     sizeof(temporaryPath),
                     "%s.zr-migrate-tmp.%u.%u",
                     path,
                     processId,
                     attempt) < 0 || strlen(temporaryPath) >= sizeof(temporaryPath)) {
            return ZR_FALSE;
        }
        file = fopen(temporaryPath, "wbx");
        if (file != ZR_NULL) {
            break;
        }
        if (errno != EEXIST) {
            return ZR_FALSE;
        }
    }
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((length == 0U || fwrite(text, 1U, length, file) == length) && fflush(file) == 0) {
        result = ZR_TRUE;
    }
    fclose(file);
    if (!result) {
        remove(temporaryPath);
        return ZR_FALSE;
    }
#ifdef _WIN32
    if (MoveFileExA(temporaryPath, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        remove(temporaryPath);
        return ZR_FALSE;
    }
#else
    if (rename(temporaryPath, path) != 0) {
        remove(temporaryPath);
        return ZR_FALSE;
    }
#endif
    return ZR_TRUE;
}

static TZrBool zr_cli_migration_has_machine_edits(const SZrLegacyMigrationPlan *plan) {
    TZrSize index;

    if (plan == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < plan->items.length; index++) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get((SZrArray *)&plan->items, index);
        if (item != ZR_NULL && item->applicability == ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE &&
            item->hasFix) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_cli_migration_validate_current_source(
        SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName) {
    SZrAstNode *ast;
    SZrCompileResult compileResult;
    TZrBool valid;

    if (state == ZR_NULL || source == ZR_NULL || sourceName == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&compileResult, 0, sizeof(compileResult));
    ast = ZrParser_Parse(state, source, sourceLength, sourceName);
    if (ast == ZR_NULL) {
        return ZR_FALSE;
    }
    valid = ZrParser_Compiler_CompileWithTests(state, ast, &compileResult);
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    return valid;
}

static TZrBool zr_cli_migration_path_has_suffix(const TZrChar *path, const TZrChar *suffix) {
    TZrSize pathLength;
    TZrSize suffixLength;

    if (path == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }
    pathLength = strlen(path);
    suffixLength = strlen(suffix);
    return pathLength >= suffixLength && strcmp(path + pathLength - suffixLength, suffix) == 0;
}

static TZrBool zr_cli_migration_path_is_excluded(const TZrChar *path) {
    return path != ZR_NULL &&
           (strstr(path, "/bin/") != ZR_NULL || strstr(path, "\\bin\\") != ZR_NULL ||
            strstr(path, "/golden/") != ZR_NULL || strstr(path, "\\golden\\") != ZR_NULL ||
            strstr(path, "/generated/") != ZR_NULL || strstr(path, "\\generated\\") != ZR_NULL ||
            strstr(path, "/.codex/") != ZR_NULL || strstr(path, "\\.codex\\") != ZR_NULL);
}

static TZrBool zr_cli_migration_run_one(
        const SZrCliCommand *command,
        SZrState *state,
        const TZrChar *path,
        FILE *output,
        FILE *errorOutput) {
    TZrChar *source = ZR_NULL;
    TZrSize sourceLength = 0U;
    TZrChar *revalidated = ZR_NULL;
    TZrSize revalidatedLength = 0U;
    TZrChar *migrated = ZR_NULL;
    TZrSize migratedLength = 0U;
    SZrString *sourceName;
    SZrLegacyMigrationPlan plan = {0};
    TZrBool result = ZR_FALSE;

    if (!ZrCli_Project_ReadTextFile(path, &source, &sourceLength)) {
        fprintf(errorOutput, "failed to read migration source: %s\n", path);
        goto cleanup;
    }
    sourceName = ZrCore_String_Create(
            state,
            (TZrNativeString)path,
            strlen(path));
    if (sourceName == ZR_NULL || !ZrParser_LegacyMigration_PlanSource(
                                      state,
                                      source,
                                      sourceLength,
                                      sourceName,
                                      &plan)) {
        fprintf(errorOutput, "failed to build migration plan: %s\n", path);
        goto cleanup;
    }
    if (command->migrationWrite && zr_cli_migration_has_machine_edits(&plan)) {
        if (!ZrCli_Project_ReadTextFile(path, &revalidated, &revalidatedLength) ||
            revalidatedLength != sourceLength || memcmp(revalidated, source, sourceLength) != 0) {
            fprintf(errorOutput, "migration source changed before write: %s\n", path);
            goto cleanup;
        }
        if (!ZrParser_LegacyMigration_ApplyMachineEdits(
                    state,
                    &plan,
                    source,
                    sourceLength,
                    &migrated,
                    &migratedLength) ||
            !zr_cli_migration_validate_current_source(state, migrated, migratedLength, sourceName) ||
            !zr_cli_migration_write_file_atomically(path, migrated, migratedLength)) {
            fprintf(errorOutput, "failed to apply or validate migration edits: %s\n", path);
            goto cleanup;
        }
    }
    zr_cli_migration_write_report(output, command, &plan, path);
    result = ZR_TRUE;

cleanup:
    if (migrated != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, migrated, migratedLength + 1U);
    }
    free(revalidated);
    ZrParser_LegacyMigration_PlanFree(state, &plan);
    free(source);
    return result;
}

int ZrCli_Migration_Run(const SZrCliCommand *command, FILE *output, FILE *errorOutput) {
    SZrGlobalState *global = ZR_NULL;
    SZrState *state;
    EZrLibrary_File_Exist existence;
    SZrLibrary_File_List files;
    TZrSize index;
    int result = 1;

    if (command == ZR_NULL || command->mode != ZR_CLI_MODE_MIGRATE_SYNTAX ||
        command->migrationPath == ZR_NULL || output == ZR_NULL || errorOutput == ZR_NULL) {
        return 1;
    }
    global = ZrCli_Project_CreateBareGlobal();
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        fprintf(errorOutput, "failed to create migration parser state\n");
        goto cleanup;
    }
    if (!ZrCli_Project_RegisterStandardModules(global)) {
        fprintf(errorOutput, "failed to register migration compiler modules\n");
        goto cleanup;
    }
    state = global->mainThreadState;
    existence = ZrLibrary_File_Exist((TZrNativeString)command->migrationPath);
    if (existence == ZR_LIBRARY_FILE_IS_FILE) {
        if (!zr_cli_migration_path_has_suffix(command->migrationPath, ".zr") ||
            (!command->migrationIncludeGenerated &&
             zr_cli_migration_path_is_excluded(command->migrationPath))) {
            fprintf(errorOutput, "migration path is excluded: %s\n", command->migrationPath);
            goto cleanup;
        }
        result = zr_cli_migration_run_one(command, state, command->migrationPath, output, errorOutput) ? 0 : 1;
        goto cleanup;
    }
    if (existence != ZR_LIBRARY_FILE_IS_DIRECTORY ||
        !ZrLibrary_File_ListDirectory((TZrNativeString)command->migrationPath, ZR_TRUE, &files)) {
        fprintf(errorOutput, "migration path is not a readable file or directory: %s\n", command->migrationPath);
        goto cleanup;
    }
    result = 0;
    for (index = 0U; index < files.count; index++) {
        const SZrLibrary_File_ListEntry *entry = &files.entries[index];

        if (entry->existence != ZR_LIBRARY_FILE_IS_FILE ||
            !zr_cli_migration_path_has_suffix(entry->path, ".zr") ||
            (!command->migrationIncludeGenerated && zr_cli_migration_path_is_excluded(entry->path))) {
            continue;
        }
        if (!zr_cli_migration_run_one(command, state, entry->path, output, errorOutput)) {
            result = 1;
            break;
        }
    }
    ZrLibrary_File_List_Free(&files);

cleanup:
    if (global != ZR_NULL) {
        ZrCore_GlobalState_Free(global);
    }
    return result;
}
