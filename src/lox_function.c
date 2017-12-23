//
//  lox_function.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 31/10/2017.
//

#include "lox_function.h"

#include "garbage_collector.h"
#include "lox_instance.h"
#include "interpreter.h"
#include "return.h"

extern inline bool isLoxFunction(const Object *function);

LoxFunction * function_init(FunctionStmt *declaration, Environment *closure, bool isInitializer)
{
    LoxFunction *function = lox_alloc(LoxFunction);
    if(function == NULL)
    {
        fatal_outOfMemory();
    }
    function->declaration = declaration;
    function->closure = closure;
    function->isInitializer = isInitializer;
    function->marked = GC_CLEAR;
    return function;
}

void function_free(LoxFunction *function)
{
    lox_free(function);
}

Object * function_call(const LoxFunction *function, LoxArguments *args, Error **error, Interpreter *interpreter)
{
    assert(args->count == function->declaration->arity);
    
    // NOTE: we dynamically create a new local environment for
    //       the function to allow for recursion.
    Environment *environment = env_init(function->closure, error, interpreter->collector);
    if (*error)
    {
        return NULL;
    }
    assert(environment != NULL);
    
    for(int32_t i = 0; i < function->declaration->arity; i++)
    {
        Token *parameter = function->declaration->parameters[i];
        *error = env_defineLocal(parameter, args->values[i], environment);
        assert(*error == NULL);
    }
    
    Return *ret = interpreter_executeBlock(function->declaration->body, environment, interpreter);

    env_release(environment);
    
    Object *result;
    if (function->isInitializer)
    {
        result = env_getAt(/*"this"*/NULL, 0, 0, function->closure, error, interpreter->collector);
        if(*error)
        {
            return NULL;
        }
    }
    else if(ret != NULL)
    {
        result = return_unwrap(ret);
    }
    else
    {
        result = obj_newNil(interpreter->collector);
    }
    assert(result != NULL);
    
    return result;
}

int32_t function_arity(LoxFunction *function)
{
    int32_t arity = function->declaration->arity;
    return arity;
}

char * function_toString(LoxFunction *function, Interpreter *interpreter)
{
    char *str = str_fromLiteral("<fn ");
    {
        char *tok = token_to_string(function->declaration->name, interpreter->source);
        str_append(str, tok);
        str_free(tok);
    }
    str_appendLiteral(str, ">");
    return str;
}

LoxFunction * function_bind(const LoxFunction *function, LoxInstance *instance, Error **error, GarbageCollector *collector)
{
    Environment *environment = env_init(function->closure, error, collector);
    if (*error)
    {
        return NULL;
    }
    assert(environment != NULL);

    env_defineThis(obj_wrapInstance(instance, collector), environment);

    LoxFunction *result = function_init(function->declaration, environment, function->isInitializer);
    return result;
}


