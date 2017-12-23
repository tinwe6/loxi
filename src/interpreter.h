//
//  interpreter.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 17/10/2017.
//

#ifndef interpreter_h
#define interpreter_h

#include "environment.h"
#include "error.h"
#include "expr.h"
#include "garbage_collector.h"
#include "return.h"
#include "stmt.h"
#include "utility.h"

#include <setjmp.h>

// TODO: The size of the locals hash table limits the maximum size of
// the program that can be run. We need to allow it to grow dynamically.
// NOTE: Must be a power of 2
#define LOCALS_HASH_MAP_SIZE 1024

typedef struct
{
    Expr *expr;
    int32_t depth;
    int32_t index;
} LocalEntry;

typedef struct Interpreter_tag
{
    ExprVisitor exprVisitor;
    StmtVisitor stmtVisitor;
    
    Environment *globals;
    Environment *environment;
    LocalEntry locals[LOCALS_HASH_MAP_SIZE];
    int32_t localsCount;

    GarbageCollector *collector;

    Error *runtimeError;
    const char *source;

    Timer timer;

    struct timespec time_start;
    time_t clock_start;
    
    // exception handler
    jmp_buf catchLocation;
    
    // NOTE: for repl use only
    bool isREPL;
    bool exitREPL;
} Interpreter;

typedef struct
{
    Interpreter *interpreter;
    char *message;
    void (*callback)(Token *token, Interpreter *interpreter);
} ErrorCallback;

Interpreter * interpreter_init(bool isREPL);
void interpreter_free(Interpreter *interpreter);
void interpret(Stmt *statements, Interpreter *interpreter);
void interpreter_clearRuntimeError(Interpreter *interpreter);
Return * interpreter_executeBlock(Stmt *statements, Environment *environment, Interpreter *interpreter);
void interpreter_resolve(Expr *expr, int32_t depth, int32_t index, Interpreter *interpreter);

__attribute__((__noreturn__))
void interpreter_throwExit(Interpreter *interpreter);

#endif /* interpreter_h */
