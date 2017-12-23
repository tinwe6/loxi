//
//  lox_callable.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 21/10/2017.
//

#include "lox_callable.h"
#include "common.h"
#include "garbage_collector.h"

extern inline int32_t callableArity(const LoxCallable *f);
extern inline bool isLoxCallable(const Object *callee);

LoxCallable * callableInit(lox_callable_function *f, int32_t arity)
{
    assert(arity >= 0 && arity <= LOX_MAX_ARG_COUNT);

    LoxCallable* callable = lox_alloc(LoxCallable);
    if(callable == NULL)
    {
        fatal_outOfMemory();
    }
    callable->function = f;
    callable->arity = arity;
    
    return callable;
}

void callableFree(LoxCallable *callable)
{
        lox_free(callable);
}

/* LOX native functions */

#include "interpreter.h"
#include <stdio.h>

// clock() returns the time elapsed in milliseconds since a reference time
LOX_CALLABLE(lox_clock)
{
    Interpreter *interpreter = (Interpreter *)context;
    double elapsedSec = timer_elapsedSec(&interpreter->timer);
        
    Object *result = obj_newNumber(elapsedSec*1000, interpreter->collector);
    return result;
}

// env() prints all objects defined in the current environment
LOX_CALLABLE(lox_env)
{
    Interpreter *interpreter = (Interpreter *)context;
    env_printReportAll(interpreter->environment);
    
    Object *result = obj_newNil(interpreter->collector);
    return result;
}

// quit() exits the interpreter
LOX_CALLABLE(lox_quit)
{
    Interpreter *interpreter = (Interpreter *)context;
    if (interpreter->isREPL)
    {
        interpreter_throwExit(interpreter);
    }
    else
    {
        exit(0);
    }
}

// help() prints a some help in the interpreter
LOX_CALLABLE(lox_help)
{
    printf("\nLoxi is an interpreter for the Lox language, as described on\nhttp://www.craftinginterpreters.com/the-lox-language.html\n\n");
    printf("Native functions:\n");
    printf(" clock() - returns the time (in msec) elapsed since the start\n");
    printf(" env()   - prints objects defined in current environment\n");
    printf(" help()  - prints this help\n");
    printf(" quit()  - exits the interpreter\n");
    printf("\n");
    return NULL;
}
