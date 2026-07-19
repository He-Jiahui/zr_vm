#include "parser_internal.h"

static TZrBool current_identifier_is(SZrParserState *ps, const TZrChar *value) {
    return ps != ZR_NULL && ps->lexer->t.token == ZR_TK_IDENTIFIER &&
           current_identifier_equals(ps, value);
}

TZrBool parse_parameter_source_passing_form(
        SZrParserState *ps,
        EZrParameterSourcePassingForm *sourceForm,
        EZrParameterPassingMode *legacyMode,
        SZrFileRange *location) {
    SZrFileRange startLoc;
    TZrBool scoped = ZR_FALSE;
    TZrBool readonly = ZR_FALSE;

    if (ps == ZR_NULL || sourceForm == ZR_NULL || legacyMode == ZR_NULL || location == ZR_NULL) {
        return ZR_FALSE;
    }
    startLoc = get_current_token_location(ps);

    if (ps->lexer->t.token == ZR_TK_IN) {
        *sourceForm = ZR_PARAMETER_SOURCE_IN;
        *legacyMode = ZR_PARAMETER_PASSING_MODE_IN;
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_OUT) {
        *sourceForm = ZR_PARAMETER_SOURCE_OUT;
        *legacyMode = ZR_PARAMETER_PASSING_MODE_OUT;
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        if (current_identifier_is(ps, "scoped")) {
            scoped = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
        if (ps->lexer->t.token != ZR_TK_REF) {
            if (scoped) {
                report_error(ps, "Expected 'ref' after 'scoped' in parameter contract");
            }
            return ZR_FALSE;
        }
        ZrParser_Lexer_Next(ps->lexer);
        if (current_identifier_is(ps, "readonly")) {
            readonly = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
        *legacyMode = ZR_PARAMETER_PASSING_MODE_REF;
        if (scoped) {
            *sourceForm = readonly ? ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY
                                   : ZR_PARAMETER_SOURCE_SCOPED_REF;
        } else {
            *sourceForm = readonly ? ZR_PARAMETER_SOURCE_REF_READONLY : ZR_PARAMETER_SOURCE_REF;
        }
    }

    *location = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
    return ZR_TRUE;
}
