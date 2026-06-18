#ifndef ZR_VM_PARSER_COMPILER_USING_PLUGIN_GUARD_ESCAPE_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_USING_PLUGIN_GUARD_ESCAPE_INTERNAL_H

#include "compiler_internal.h"
#include "compiler_using_plugin_guard_escape.h"

typedef struct SZrPluginGuardEscapeScan {
    SZrCompilerState *cs;
    SZrString *moduleName;
    SZrArray localNames;
    SZrArray pluginNames;
    SZrArray shadowNames;
} SZrPluginGuardEscapeScan;

TZrBool plugin_guard_name_array_contains(SZrArray *names, SZrString *name);
void plugin_guard_push_name_unique(SZrPluginGuardEscapeScan *scan, SZrArray *names, SZrString *name);
TZrBool plugin_guard_report_escape(SZrPluginGuardEscapeScan *scan,
                                   SZrFileRange location,
                                   const TZrChar *reason);
SZrString *plugin_guard_bare_identifier_name(SZrAstNode *node);

TZrBool plugin_guard_block_contains_scoped_value(SZrPluginGuardEscapeScan *scan, SZrAstNode *node);
TZrBool plugin_guard_callable_body_contains_scoped_value(SZrPluginGuardEscapeScan *scan,
                                                        SZrAstNodeArray *params,
                                                        SZrParameter *args,
                                                        SZrAstNode *body);
TZrBool plugin_guard_expression_contains_scoped_value(SZrPluginGuardEscapeScan *scan, SZrAstNode *node);
TZrBool plugin_guard_expression_contains_lambda_capture(SZrPluginGuardEscapeScan *scan, SZrAstNode *node);
TZrBool plugin_guard_expression_passes_scoped_value_as_argument(SZrPluginGuardEscapeScan *scan,
                                                                SZrAstNode *node);
TZrBool plugin_guard_expression_contains_container_escape(SZrPluginGuardEscapeScan *scan, SZrAstNode *node);
TZrBool plugin_guard_scan_expression_side_effects(SZrPluginGuardEscapeScan *scan, SZrAstNode *node);
TZrBool plugin_guard_scan_generic_argument_metadata(SZrPluginGuardEscapeScan *scan,
                                                    SZrAstNodeArray *arguments,
                                                    SZrFileRange fallbackLocation,
                                                    const TZrChar *reason);
TZrBool plugin_guard_scan_statement(SZrPluginGuardEscapeScan *scan, SZrAstNode *node);

#endif
