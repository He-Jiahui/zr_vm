#ifndef ZR_VM_PARSER_PROPERTY_MIGRATION_H
#define ZR_VM_PARSER_PROPERTY_MIGRATION_H

#include "zr_vm_parser/parser.h"
#include "zr_vm_core/array.h"

typedef struct SZrLegacyPropertyMigrationEntry {
    SZrAstNode *declaration;
    TZrUInt32 memberOrdinal;
} SZrLegacyPropertyMigrationEntry;

typedef struct SZrLegacyPropertyMigrationCollection {
    SZrArray entries;
    TZrUInt32 nextMemberOrdinal;
} SZrLegacyPropertyMigrationCollection;

void parser_property_migration_collection_init(
        SZrState *state,
        SZrLegacyPropertyMigrationCollection *collection);

TZrBool parser_property_migration_collection_append(
        SZrState *state,
        SZrLegacyPropertyMigrationCollection *collection,
        SZrAstNode *declaration);

void parser_property_migration_collection_mark_current_member(
        SZrLegacyPropertyMigrationCollection *collection);

void parser_property_migration_collection_publish_and_free(
        SZrParserState *ps,
        SZrLegacyPropertyMigrationCollection *collection);

#endif
