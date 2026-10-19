#ifndef ZR_VM_CLI_COMMAND_NUMBERS_INTERNAL_H
#define ZR_VM_CLI_COMMAND_NUMBERS_INTERNAL_H

#include "command/command.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Parse the two bounded numeric option forms without accepting strto* saturation.
 * A prior errno value must not turn a valid option into a range error. */
static TZrBool zr_cli_command_parse_positive_uint32(
        const TZrChar *text,
        TZrUInt32 *outValue) {
    TZrChar *end = ZR_NULL;
    unsigned long value;

    if (text == ZR_NULL || text[0] == '\0' || outValue == ZR_NULL || text[0] == '-') {
        return ZR_FALSE;
    }
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value == 0UL || value > UINT32_MAX) {
        return ZR_FALSE;
    }
    *outValue = (TZrUInt32)value;
    return ZR_TRUE;
}

static TZrBool zr_cli_command_parse_duration_milliseconds(
        const TZrChar *text,
        TZrUInt64 *outMilliseconds) {
    TZrChar *end = ZR_NULL;
    unsigned long long value;
    TZrUInt64 multiplier;

    if (text == ZR_NULL || text[0] == '\0' || outMilliseconds == ZR_NULL || text[0] == '-') {
        return ZR_FALSE;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text || value == 0ULL) {
        return ZR_FALSE;
    }
    if (strcmp(end, "ms") == 0) {
        multiplier = 1U;
    } else if (strcmp(end, "s") == 0) {
        multiplier = 1000U;
    } else if (strcmp(end, "m") == 0) {
        multiplier = 60000U;
    } else {
        return ZR_FALSE;
    }
    if ((TZrUInt64)value > UINT64_MAX / multiplier) {
        return ZR_FALSE;
    }
    *outMilliseconds = (TZrUInt64)value * multiplier;
    return ZR_TRUE;
}

#endif
