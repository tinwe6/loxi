//
//  error.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 18/10/2017.
//

#include "error.h"
#include "common.h"
#include "token.h"
#include "string.h"

Error * initErrorString(const Token *token, char *message)
{
    Error *error = lox_alloc(Error);
    if(error == NULL)
    {
        fprintf(stderr, "%s\n[line %d]\n", message, token->lexeme.line + 1);
        fatal_outOfMemory();
    }
    error->message = message;
    error->token = token;
    return error;
}

Error * initError(const Token *token, const char *message)
{
    Error *error = initErrorString(token, str_fromLiteral(message));
    return error;
}

static char * error_buildMessage(const char *prefixLiteral, const Token *identifier, const char *suffixLiteral)
{
    char *message = str_fromLiteral(prefixLiteral);
    str_append(message, get_identifier_name(identifier));
    str_appendLiteral(message, suffixLiteral);
    return message;
}

Error * initErrorIdentifier(const char *prefixLiteral, const Token *identifier, const char *suffixLiteral)
{
    Error *error = lox_alloc(Error);
    if(error == NULL)
    {
        fprintf(stderr, "%s%s%s\n[line %d]\n", prefixLiteral, identifier->literal, suffixLiteral, identifier->lexeme.line + 1);
        fatal_outOfMemory();
    }
    error->message = error_buildMessage(prefixLiteral, identifier, suffixLiteral);
    error->token = identifier;
    
    return error;
}

void freeError(Error *error)
{
    assert(error != NULL);
    assert(error->message != NULL);
    str_free(error->message);
    lox_free(error);
}

/******************/
/* Error handling */
/******************/

extern bool lox_hadError_;
extern bool lox_hadRuntimeError_;

static void lox_report(int line, const char *location, const char *message)
{
    fprintf(stderr, "[line %d] Error%s: %s\n", line+1, location, message);
    lox_hadError_ = true;
}

void lox_token_error(const char *source, const Token *token, const char *message)
{
    if (token->type == TT_EOF)
    {
        lox_report(token->lexeme.line, " at end", message);
    }
    else
    {
        char *lexeme = str_substring(source, token->lexeme.index);
        char location[256];
        snprintf(location, 256, " at '%s'", lexeme);
        lox_report(token->lexeme.line, location, message);
        str_free(lexeme);
    }
}

void lox_error(int line, const char *message)
{
    lox_report(line, "", message);
}

void lox_runtimeError(Error *error)
{
    fprintf(stderr, "%s\n[line %d]\n", error->message, error->token->lexeme.line + 1);
    lox_hadRuntimeError_ = true;
}

__attribute__((__noreturn__))
void fatal_outOfMemory()
{
    fprintf(stderr, "Fatal error: out of memory");
    exit(LOX_EXIT_CODE_FATAL_ERROR);
}
