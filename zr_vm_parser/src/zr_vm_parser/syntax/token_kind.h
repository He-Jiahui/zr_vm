#ifndef ZR_VM_PARSER_TOKEN_KIND_H
#define ZR_VM_PARSER_TOKEN_KIND_H

#include "zr_vm_parser/lexer.h"

EZrToken ZrParser_Token_SinglePunct(TZrUInt32 codepoint);
EZrToken ZrParser_Token_CompoundPunct(TZrUInt32 first, TZrUInt32 second,
                                     TZrUInt32 third, TZrBool hasThird);
TZrBool ZrParser_Token_IsLiteral(EZrToken token);
TZrBool ZrParser_Token_IsPunct(EZrToken token);

#endif
