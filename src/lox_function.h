//
//  lox_function.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 31/10/2017.
//

#ifndef lox_function_h
#define lox_function_h

#include "environment.h"
#include "lox_callable.h"
#include "stmt.h"

struct Interpreter_tag;
typedef struct Interpreter_tag Interpreter;

#define LOX_FUNCTION(name) Object *name(Object **args, uint32_t argsCount, void *context)
typedef LOX_FUNCTION(lox_function);

typedef struct LoxFunction_tag
{
    FunctionStmt *declaration;
    Environment *closure;
    bool isInitializer;
    int32_t marked;
} LoxFunction;

typedef struct LoxArguments_tag
{
    Object * values[LOX_MAX_ARG_COUNT];
    int32_t count;
} LoxArguments;

static inline LoxArguments *argumentsInit()
{
    LoxArguments *arguments = lox_alloc(LoxArguments);
    return arguments;
}


static inline void argumentsFree(LoxArguments *arguments)
{
    lox_free(arguments);
}

LoxFunction * function_init(FunctionStmt *declaration, Environment *closure, bool isInitializer);
void function_free(LoxFunction *function);
Object * function_call(const LoxFunction *function, LoxArguments *args, Error **error, Interpreter *interpreter);
int32_t function_arity(LoxFunction *function);
char * function_toString(LoxFunction *function, Interpreter *interpreter);
LoxFunction * function_bind(const LoxFunction *function, LoxInstance *instance, Error **error, GarbageCollector *collector);

inline bool isLoxFunction(const Object *function)
{
    return function->type == OT_FUNCTION;
}

#endif /* lox_function_h */
