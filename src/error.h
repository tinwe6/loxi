//
//  error.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 18/10/2017.
//

#ifndef error_h
#define error_h

struct Token_tag;
typedef struct Token_tag Token;

typedef struct
{
    const Token *token;
    char *message;
} Error;

Error * initError(const Token *token, const char *message);
Error * initErrorString(const Token *token, char *message);
Error * initErrorIdentifier(const char *prefixLiteral, const Token *identifier, const char *suffixLiteral);
void freeError(Error *error);
void lox_token_error(const char *source, const Token *token, const char *message);
void lox_error(int line, const char *message);
void lox_runtimeError(Error *error);
__attribute__((__noreturn__)) void fatal_outOfMemory(void);

#endif /* error_h */
