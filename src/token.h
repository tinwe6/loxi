//
//  token.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#ifndef token_h
#define token_h

#include "string.h"

/* Token types */

// NOTE: we use the preprocessor to generate all enum cases and an array with the corresponding name strings.
#define TOKEN_LIST(token, key) \
/* Single-character tokens */ \
token(LEFT_PAREN,  "(") token(RIGHT_PAREN, ")") token(LEFT_BRACE, "{") \
token(RIGHT_BRACE, "}") token(COMMA,       ",")  token(DOT,       ".") \
token(MINUS,       "-") token(PLUS,        "+")  token(SEMICOLON, ";") \
token(SLASH,       "/") token(STAR,        "*") \
/* One or two character tokens */ \
token(BANG,    "!") token(BANG_EQUAL,    "!=") \
token(EQUAL,   "=") token(EQUAL_EQUAL,   "==") \
token(GREATER, ">") token(GREATER_EQUAL, ">=") \
token(LESS,    "<") token(LESS_EQUAL,    "<=") \
/* Literals */ \
token(IDENTIFIER, NULL) token(STRING, NULL) token(NUMBER, NULL) \
/* Keywords */ \
key(AND,   "and")   key(CLASS,  "class")  key(ELSE,  "else")  \
key(FALSE, "false") key(FUN,    "fun")    key(FOR,   "for")   \
key(IF,    "if")    key(NIL,    "nil")    key(OR,    "or")    \
key(PRINT, "print") key(RETURN, "return") key(SUPER, "super") \
key(THIS,  "this")  key(TRUE,   "true")   key(VAR,   "var")   \
key(WHILE, "while") key(EOF,    "eof")

#define IGNORE(type, string)
#define FOREACH_TOKEN(code)   TOKEN_LIST(code, code)
#define FOREACH_SYMBOL(code)  TOKEN_LIST(code, IGNORE)
#define FOREACH_KEYWORD(code) TOKEN_LIST(IGNORE, code)

typedef enum TokenType
{
#define DEFINE_ENUM_TYPE(type, string) TT_##type,
    FOREACH_TOKEN(DEFINE_ENUM_TYPE)
#undef DEFINE_ENUM_TYPE
} TokenType;

static const char * const tt_name_string[] =
{
#define DEFINE_TYPE_NAME(type, string) #type,
    FOREACH_TOKEN(DEFINE_TYPE_NAME)
#undef DEFINE_TYPE_NAME
};

static const char * const tt_symbol_string[] =
{
#define DEFINE_TYPE_STRING(type, string) string,
    FOREACH_TOKEN(DEFINE_TYPE_STRING)
#undef DEFINE_TYPE_STRING
};

/* Keywords */

typedef struct KeywordEntry {
    char *keyword;
    TokenType type;
} KeywordEntry;

static KeywordEntry keywords[] = {
#define DEFINE_KEYWORD_ENTRY(name, string) \
    {string, TT_##name},
    FOREACH_KEYWORD(DEFINE_KEYWORD_ENTRY)
#undef DEFINE_KEYWORD_ENTRY
    // NOTE: The following marks the end of the array and must come last.
    {"", 0}
};

typedef union
{
    struct
    {
        str_size start; // start index of lexeme in source
        str_size count; // number of characters in the lexeme
        int32_t line;   // source line where the lexeme appears
    };
    struct
    {
        SubstringIndex index;
        int32_t line_;
    };
} Lexeme;

/* Tokens */

typedef struct Token_tag
{
    double number;
    char *literal;

    // Note: used by the scanner to link the tokens as they appear in the source code in a list
    struct Token_tag *nextInScan;

    TokenType type;
    Lexeme lexeme;
} Token;

void token_free(Token *token);
Token * token_atomic(TokenType type, Lexeme lexeme);
Token * token_identifier(const char *str, Lexeme lexeme);
Token * token_string_literal(const char * const str, Lexeme lexeme);
Token * token_number_literal(double value, Lexeme lexeme);
char * token_to_string(const Token * const token, const char * const source);
char * string_from_token_literal(const Token *token);

int32_t token_count(Token *elements);
Token * token_last(Token *expressions);
Token * token_appendTo(Token *head, Token *tail);

KeywordEntry * lookup_keyword(const char *keyword);

inline bool keyword_is_valid(KeywordEntry *entry)
{
    assert(entry != NULL);
    return entry->keyword[0] != '\0';
}

inline double get_number_value(const Token *token)
{
    assert(token->type == TT_NUMBER);
    return token->number;
}

inline const char * get_string_value(const Token *token)
{
    assert(token->type == TT_STRING);
    return token->literal;
}

inline const char * get_identifier_name(const Token *token)
{
    assert(token->type == TT_IDENTIFIER);
    return token->literal;
}

#endif /* token_h */
