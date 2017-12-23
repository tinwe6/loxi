//
//  token.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#include "token.h"
#include "error.h"

/* Keywords */

extern inline bool keyword_is_valid(KeywordEntry *entry);

KeywordEntry * lookup_keyword(const char *keyword)
{
    KeywordEntry *entry = keywords;
    while(keyword_is_valid(entry))
    {
        if(str_isEqual(entry->keyword, keyword))
        {
            break;
        }
        entry++;
    }
    return entry;
}

/* Tokens */

extern inline const char * get_identifier_name(const Token *token);
extern inline const char * get_string_value(const Token *token);
extern inline double get_number_value(const Token *token);

// Returns a new string with the value of a token literal.
// The returned string must be freed with str_free().
char *string_from_token_literal(const Token *token)
{
    char *result;
    switch(token->type) {
        case TT_NUMBER:
        {
            result = str_fromDouble(get_number_value(token));
        } break;
        case TT_STRING:
        {
            result = str_dup(get_string_value(token));
        } break;
        case TT_IDENTIFIER:
        {
            result = str_dup(get_identifier_name(token));
        } break;
        case TT_TRUE:
        {
            result = str_fromLiteral("true");
        } break;
        case TT_FALSE:
        {
            result = str_fromLiteral("false");
        } break;
        INVALID_DEFAULT_CASE;
    }
    return result;
}

// Returns a new string containing a description of `token`.
// The returned string must be freed with str_free().
char * token_to_string(const Token * const token, const char * const source)
{
    char *lexeme = str_substring(source, token->lexeme.index);
    char *str = str_fromLiteral(tt_name_string[token->type]);
    str_appendLiteral(str, " '");
    str_append(str, lexeme);
    str_appendLiteral(str, "'");
    switch(token->type)
    {
        case TT_NUMBER:
        {
            str_appendLiteral(str, " - value: ");
            char *number = str_fromDouble(get_number_value(token));
            str_append(str, number);
            str_free(number);
        } break;
            
        case TT_STRING:
        {
            str_appendLiteral(str, " - value: ");
            str_append(str, get_string_value(token));
        } break;
            
        case TT_IDENTIFIER:
        {
            str_appendLiteral(str, " - value: ");
            str_append(str, get_identifier_name(token));
        } break;
            
        default:
        {
            // do nothing
        } break;
    }
    str_free(lexeme);
    
    return str;
}

static Token * get_token()
{
    Token *token = lox_alloc(Token);
    if(token == NULL)
    {
        fatal_outOfMemory();
    }
    token->literal = NULL;
    token->nextInScan = NULL;
    return token;
}

static inline void token_set_lexeme(Token * token, Lexeme lexeme)
{
    token->lexeme = lexeme;
}

Token *token_number_literal(double value, Lexeme lexeme)
{
    Token *result = get_token();
    result->type = TT_NUMBER;
    result->number = value;
    token_set_lexeme(result, lexeme);

    return result;
}

Token * token_string_literal(const char * const str, Lexeme lexeme)
{
    Token *result = get_token();
    
    result->type = TT_STRING;
    result->literal = str_dup(str);
    token_set_lexeme(result, lexeme);

    return result;
}

Token * token_identifier(const char *str, Lexeme lexeme)
{
    Token *result = get_token();
    
    result->type = TT_IDENTIFIER;
    result->literal = str_dup(str);
    token_set_lexeme(result, lexeme);

    return result;
}

Token * token_atomic(TokenType type, Lexeme lexeme)
{
    assert(type != TT_STRING && type != TT_NUMBER);
    Token *result = get_token();
    result->type = type;
    token_set_lexeme(result, lexeme);

    return result;
}

void token_free(Token *token)
{
#ifdef TOKEN_UNION
    if(token->type == TT_IDENTIFIER || token->type == TT_STRING)
#else
    if(token->literal)
#endif
    {
        assert(token->type == TT_IDENTIFIER || token->type == TT_STRING);
        str_free(token->literal);
    }
    lox_free(token);
}

// Returns the number of elements in the list `elements`
int32_t token_count(Token *elements)
{
    int32_t count = 0;
    while(elements != NULL)
    {
        count++;
        elements = elements->nextInScan;
    }
    return count;
}

// Returns the last token in a non-empty chain of tokens.
Token * token_last(Token *expressions)
{
    assert(expressions != NULL);
    
    Token *last = expressions;
    while(last->nextInScan)
    {
        last = last->nextInScan;
    }
    return last;
}

Token * token_appendTo(Token *head, Token *tail)
{
    if (head == NULL)
    {
        return tail;
    }
    Token *last = token_last(head);
    last->nextInScan = tail;
    return head;
}
