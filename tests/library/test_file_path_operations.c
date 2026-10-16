/* Real host-file APIs. The caller registers and later cleans each fixture root. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zr_vm_library/file.h"

static int failures;
static void check(int condition, const char *message) {
    fprintf(stderr, "%s: %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}

static int join(const char *root, const char *leaf, char *out) {
    int count = snprintf(out, ZR_LIBRARY_MAX_PATH_LENGTH, "%s/%s", root, leaf);
    return count >= 0 && (size_t)count < ZR_LIBRARY_MAX_PATH_LENGTH;
}

static int write_file(char *path, const char *text) {
    FILE *stream = fopen(path, "wb");
    size_t length = strlen(text);
    int result;
    if (stream == NULL) return 0;
    result = fwrite(text, 1, length, stream) == length;
    if (fclose(stream) != 0) result = 0;
    return result;
}

static int matches(char *path, const char *text) {
    char buffer[64];
    FILE *stream = fopen(path, "rb");
    size_t count;
    int result;
    if (stream == NULL) return 0;
    count = fread(buffer, 1, sizeof(buffer), stream);
    result = !ferror(stream) && count == strlen(text) && memcmp(buffer, text, count) == 0;
    if (fclose(stream) != 0) result = 0;
    return result;
}

static void path_case(const char *name) {
    char output[ZR_LIBRARY_MAX_PATH_LENGTH];
    if (strcmp(name, "join_overflow") == 0) {
        char left[ZR_LIBRARY_MAX_PATH_LENGTH], right[ZR_LIBRARY_MAX_PATH_LENGTH];
        memset(left, 'a', sizeof(left) - 1); left[sizeof(left) - 1] = '\0';
        memset(right, 'b', sizeof(right) - 1); right[sizeof(right) - 1] = '\0';
        strcpy(output, "sentinel"); errno = 0;
        ZrLibrary_File_PathJoin(left, right, output);
        check(output[0] == '\0' && errno == ENAMETOOLONG,
              "join rejects truncation and clears its void output");
    } else if (strcmp(name, "join_control") == 0) {
        ZrLibrary_File_PathJoin("alpha", "beta", output);
        check(strcmp(output, "alpha/beta") == 0 || strcmp(output, "alpha\\beta") == 0,
              "ordinary path join is complete");
    }
#ifdef _WIN32
    else if (strcmp(name, "unc_root") == 0) {
        check(ZrLibrary_File_NormalizePath("\\\\server\\share\\folder\\..\\leaf", output, sizeof(output)),
              "UNC path normalizes without host filesystem access");
        check(strcmp(output, "\\\\server\\share\\leaf") == 0, "UNC server/share root keeps two separators");
    } else if (strcmp(name, "drive_relative") == 0) {
        check(!ZrLibrary_File_IsAbsolutePath("E:relative"), "drive-relative path is not absolute");
        check(!ZrLibrary_File_NormalizePath("E:relative", output, sizeof(output)),
              "unsupported drive-relative path is rejected rather than changed to a root path");
    }
#endif
}

static void filesystem_case(const char *name, char *root) {
    char source[ZR_LIBRARY_MAX_PATH_LENGTH], target[ZR_LIBRARY_MAX_PATH_LENGTH];
    char sourceFile[ZR_LIBRARY_MAX_PATH_LENGTH], siblingFile[ZR_LIBRARY_MAX_PATH_LENGTH];
    int success;
    check(ZrLibrary_File_Exist(root) == ZR_LIBRARY_FILE_NOT_EXIST, "registered fixture root is new");
    if (ZrLibrary_File_Exist(root) != ZR_LIBRARY_FILE_NOT_EXIST) return;
    check(ZrLibrary_File_CreateDirectories(root), "create only the registered fixture root");
    if (strcmp(name, "copy_ancestor") == 0 || strcmp(name, "move_ancestor") == 0) {
        if (!join(root, "ancestor/source", source) || !join(root, "ancestor", target) ||
            !join(source, "payload.txt", sourceFile) || !join(target, "sibling.txt", siblingFile)) return;
        check(ZrLibrary_File_CreateDirectories(source), "create source directory under target ancestor");
        check(write_file(sourceFile, "source payload") && write_file(siblingFile, "sibling payload"), "create owned source and sibling files");
        success = strcmp(name, "copy_ancestor") == 0
                ? ZrLibrary_File_Copy(source, target, ZR_TRUE) : ZrLibrary_File_Move(source, target, ZR_TRUE);
        check(!success, "overlapping ancestor target is rejected");
        check(matches(sourceFile, "source payload"), "ancestor rejection preserves the source");
        check(matches(siblingFile, "sibling payload"), "ancestor rejection preserves sibling data");
        return;
    }
    if (!join(root, "source.txt", source) || !join(root, "target.txt", target)) return;
    if (strcmp(name, "move_missing_source") == 0) {
        check(write_file(target, "target payload"), "create an existing overwrite target");
        check(!ZrLibrary_File_Move(source, target, ZR_TRUE), "missing source move fails");
        check(matches(target, "target payload"), "missing source does not delete the existing target");
        return;
    }
    if (strcmp(name, "copy_component_prefix_control") == 0) {
        if (!join(root, "source", source) || !join(root, "source-other", target) ||
            !join(source, "payload.txt", sourceFile) || !join(target, "payload.txt", siblingFile)) return;
        check(ZrLibrary_File_CreateDirectories(source) && write_file(sourceFile, "source payload"), "create source with component-prefix sibling");
        check(ZrLibrary_File_Copy(source, target, ZR_FALSE), "component-prefix sibling remains a valid copy target");
        check(matches(sourceFile, "source payload") && matches(siblingFile, "source payload"), "sibling copy preserves complete data");
        return;
    }
    check(write_file(source, "source payload"), "create owned source data");
    if (strcmp(name, "move_self") == 0 || strcmp(name, "move_self_no_overwrite") == 0) {
        success = ZrLibrary_File_Move(source, source, strcmp(name, "move_self") == 0);
        check(success, "same normalized source and target move is a successful no-op");
        check(matches(source, "source payload"), "same-path move preserves source contents");
    } else if (strcmp(name, "copy_self_control") == 0) {
        check(ZrLibrary_File_Copy(source, source, ZR_TRUE), "same-path copy remains a successful no-op");
        check(matches(source, "source payload"), "same-path copy preserves source contents");
    } else if (strcmp(name, "copy_sibling_control") == 0) {
        check(ZrLibrary_File_Copy(source, target, ZR_FALSE), "ordinary copy succeeds");
        check(matches(source, "source payload") && matches(target, "source payload"), "ordinary copy preserves both complete files");
    } else if (strcmp(name, "move_sibling_control") == 0) {
        check(ZrLibrary_File_Move(source, target, ZR_FALSE), "ordinary move succeeds");
        check(ZrLibrary_File_Exist(source) == ZR_LIBRARY_FILE_NOT_EXIST && matches(target, "source payload"), "ordinary move publishes the complete target");
    }
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    if (strncmp(argv[1], "join_", 5) == 0 || strcmp(argv[1], "unc_root") == 0 || strcmp(argv[1], "drive_relative") == 0) path_case(argv[1]);
    else filesystem_case(argv[1], argv[2]);
    return failures ? 1 : 0;
}
