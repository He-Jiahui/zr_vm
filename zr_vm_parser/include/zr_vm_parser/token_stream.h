//
// Native immutable token streams for compile-time macros.
//

#ifndef ZR_VM_PARSER_TOKEN_STREAM_H
#define ZR_VM_PARSER_TOKEN_STREAM_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/syntax_context.h"
#include "zr_vm_parser/token_span.h"

/* token_stream.h owns the native token payload but reuses the stable lexer
 * token ids.  lexer.h intentionally does not include this header, avoiding a
 * cycle for lexer-facing code. */
typedef struct SZrGlobalState SZrGlobalState;
typedef struct SZrParserComptimeBudget SZrParserComptimeBudget;
typedef struct SZrTokenStream SZrTokenStream;
typedef struct SZrTokenTree SZrTokenTree;

typedef enum EZrTokenTriviaKind {
    ZR_TOKEN_TRIVIA_WHITESPACE = 0,
    ZR_TOKEN_TRIVIA_NEWLINE = 1,
    ZR_TOKEN_TRIVIA_LINE_COMMENT = 2,
    ZR_TOKEN_TRIVIA_BLOCK_COMMENT = 3,
    ZR_TOKEN_TRIVIA_OTHER = 4
} EZrTokenTriviaKind;

typedef struct SZrTokenTrivia {
    EZrTokenTriviaKind kind;
    SZrTokenText text;
} SZrTokenTrivia;

typedef enum EZrTokenTreeKind {
    ZR_TOKEN_TREE_GROUP = 0,
    ZR_TOKEN_TREE_IDENT = 1,
    ZR_TOKEN_TREE_PUNCT = 2,
    ZR_TOKEN_TREE_LITERAL = 3
} EZrTokenTreeKind;

typedef enum EZrTokenDelimiter {
    ZR_TOKEN_DELIMITER_NONE = 0,
    ZR_TOKEN_DELIMITER_PAREN = 1,
    ZR_TOKEN_DELIMITER_BRACE = 2,
    ZR_TOKEN_DELIMITER_BRACKET = 3
} EZrTokenDelimiter;

typedef enum EZrTokenSpacing {
    ZR_TOKEN_SPACING_ALONE = 0,
    ZR_TOKEN_SPACING_JOINT = 1
} EZrTokenSpacing;

/* Native semantic payload; text is the decoded string or identifier name,
 * distinct from the tree's exact raw spelling. No VM pointers are retained. */
typedef struct SZrTokenNativeSemInfo {
    EZrToken token;
    TZrBool booleanValue;
    TZrInt64 intValue;
    TZrDouble floatValue;
    TZrInt32 charValue;
    TZrBool isRawIdentifier;
    SZrTokenText text;
    SZrTokenText suffix;
} SZrTokenNativeSemInfo;

/* Pure-native ownership. Create starts an unsealed builder with one reference;
 * Retain/Release are valid before and after Seal. There is deliberately no
 * public Free operation. A sealed stream is immutable and safe to share.
 * Budget is borrowed only until Seal. Builder operations reject a context
 * table with an independently active transaction instead of partially
 * publishing native storage outside that transaction. */
ZR_PARSER_API SZrTokenStream *ZrParser_TokenStream_Create(
        SZrGlobalState *global,
        SZrSyntaxContextTable *contextTable,
        SZrParserComptimeBudget *budget);
ZR_PARSER_API void ZrParser_TokenStream_Retain(SZrTokenStream *stream);
ZR_PARSER_API void ZrParser_TokenStream_Release(SZrTokenStream *stream);
ZR_PARSER_API TZrBool ZrParser_TokenStream_Seal(SZrTokenStream *stream);
ZR_PARSER_API TZrBool ZrParser_TokenStream_IsSealed(const SZrTokenStream *stream);
ZR_PARSER_API SZrSyntaxContextTable *ZrParser_TokenStream_ContextTable(
        const SZrTokenStream *stream);

/* Source bytes and generated text are copied into the stream arena. */
ZR_PARSER_API TZrBool ZrParser_TokenStream_AddSource(
        SZrTokenStream *stream,
        const TZrChar *sourceName,
        TZrSize sourceNameLength,
        const TZrChar *bytes,
        TZrSize byteLength,
        TZrTokenSourceId *outSourceId);
ZR_PARSER_API TZrBool ZrParser_TokenStream_CopyText(
        SZrTokenStream *stream,
        const TZrChar *bytes,
        TZrSize byteLength,
        SZrTokenText *outText);
ZR_PARSER_API const TZrChar *ZrParser_TokenStream_SourceBytes(
        const SZrTokenStream *stream,
        TZrTokenSourceId sourceId,
        TZrSize *outLength);
ZR_PARSER_API const TZrChar *ZrParser_TokenStream_SourceName(
        const SZrTokenStream *stream,
        TZrTokenSourceId sourceId,
        TZrSize *outLength);
ZR_PARSER_API const TZrChar *ZrParser_TokenStream_TextBytes(
        const SZrTokenStream *stream,
        SZrTokenText text);

/* Append operations are the only mutating operations and are valid only before
 * Seal. They copy trivia/text references and leave the stream unchanged on
 * budget/allocation/validation failure. outIndex is an index for At(). */
ZR_PARSER_API TZrBool ZrParser_TokenStream_AppendGroup(
        SZrTokenStream *stream,
        EZrTokenDelimiter delimiter,
        SZrTokenSpan openSpan,
        SZrTokenSpan closeSpan,
        const SZrTokenTree *const *children,
        TZrSize childCount,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex);
ZR_PARSER_API TZrBool ZrParser_TokenStream_AppendIdent(
        SZrTokenStream *stream,
        SZrTokenNativeSemInfo semInfo,
        SZrTokenText rawText,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex);
ZR_PARSER_API TZrBool ZrParser_TokenStream_AppendPunct(
        SZrTokenStream *stream,
        SZrTokenNativeSemInfo semInfo,
        SZrTokenText rawText,
        TZrUInt32 codepoint,
        EZrTokenSpacing spacing,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex);
ZR_PARSER_API TZrBool ZrParser_TokenStream_AppendLiteral(
        SZrTokenStream *stream,
        SZrTokenNativeSemInfo semInfo,
        SZrTokenText rawText,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex);

ZR_PARSER_API TZrSize ZrParser_TokenStream_Count(const SZrTokenStream *stream);
ZR_PARSER_API SZrTokenSpan ZrParser_TokenStream_EndSpan(
        const SZrTokenStream *stream);
ZR_PARSER_API const SZrTokenTree *ZrParser_TokenStream_At(
        const SZrTokenStream *stream,
        TZrSize index);

ZR_PARSER_API EZrTokenTreeKind ZrParser_TokenTree_Kind(const SZrTokenTree *tree);
ZR_PARSER_API SZrTokenSpan ZrParser_TokenTree_Span(const SZrTokenTree *tree);
ZR_PARSER_API SZrSyntaxContext ZrParser_TokenTree_Context(const SZrTokenTree *tree);
ZR_PARSER_API TZrBool ZrParser_TokenTree_NativeSemInfo(
        const SZrTokenTree *tree,
        SZrTokenNativeSemInfo *outSemInfo);
/* Exact raw spelling; NativeSemInfo.text instead holds decoded semantics. */
ZR_PARSER_API SZrTokenText ZrParser_TokenTree_Text(const SZrTokenTree *tree);
ZR_PARSER_API SZrTokenText ZrParser_TokenTree_Suffix(const SZrTokenTree *tree);
ZR_PARSER_API TZrUInt32 ZrParser_TokenTree_PunctCodepoint(const SZrTokenTree *tree);
ZR_PARSER_API EZrTokenSpacing ZrParser_TokenTree_PunctSpacing(const SZrTokenTree *tree);
ZR_PARSER_API EZrTokenDelimiter ZrParser_TokenTree_GroupDelimiter(const SZrTokenTree *tree);
ZR_PARSER_API SZrTokenSpan ZrParser_TokenTree_GroupOpenSpan(const SZrTokenTree *tree);
ZR_PARSER_API SZrTokenSpan ZrParser_TokenTree_GroupCloseSpan(const SZrTokenTree *tree);
ZR_PARSER_API TZrSize ZrParser_TokenTree_GroupChildCount(const SZrTokenTree *tree);
ZR_PARSER_API const SZrTokenTree *ZrParser_TokenTree_GroupChildAt(
        const SZrTokenTree *tree,
        TZrSize index);
/* Trivia inside an otherwise empty group, between its delimiters. */
ZR_PARSER_API TZrSize ZrParser_TokenTree_GroupInnerTriviaCount(const SZrTokenTree *tree);
ZR_PARSER_API TZrBool ZrParser_TokenTree_GroupInnerTriviaAt(
        const SZrTokenTree *tree, TZrSize index, SZrTokenTrivia *outTrivia);
/* Unattached trivia for a stream containing no semantic token trees. */
ZR_PARSER_API TZrSize ZrParser_TokenStream_EmptyTriviaCount(const SZrTokenStream *stream);
ZR_PARSER_API TZrBool ZrParser_TokenStream_EmptyTriviaAt(
        const SZrTokenStream *stream, TZrSize index, SZrTokenTrivia *outTrivia);
ZR_PARSER_API TZrSize ZrParser_TokenTree_LeadingTriviaCount(const SZrTokenTree *tree);
ZR_PARSER_API TZrBool ZrParser_TokenTree_LeadingTriviaAt(
        const SZrTokenTree *tree,
        TZrSize index,
        SZrTokenTrivia *outTrivia);
ZR_PARSER_API TZrSize ZrParser_TokenTree_TrailingTriviaCount(const SZrTokenTree *tree);
ZR_PARSER_API TZrBool ZrParser_TokenTree_TrailingTriviaAt(
        const SZrTokenTree *tree,
        TZrSize index,
        SZrTokenTrivia *outTrivia);

/* Source-token construction parses/decodes once. Compilation later consumes
 * the stored tree/semantics directly; it must never stringify and re-lex. */
ZR_PARSER_API TZrBool ZrParser_TokenStream_FromText(
        SZrState *state,
        SZrSyntaxContextTable *contextTable,
        const TZrChar *sourceName,
        TZrSize sourceNameLength,
        const TZrChar *bytes,
        TZrSize byteLength,
        SZrParserComptimeBudget *budget,
        SZrTokenStream **outStream);
ZR_PARSER_API TZrBool ZrParser_TokenStream_Concat(
        SZrTokenStream *left,
        SZrTokenStream *right,
        SZrParserComptimeBudget *budget,
        SZrTokenStream **outStream);

#endif // ZR_VM_PARSER_TOKEN_STREAM_H
