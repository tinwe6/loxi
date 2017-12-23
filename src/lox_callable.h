//
//  lox_callable.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 21/10/2017.
//

#ifndef lox_callable_h
#define lox_callable_h

#include <stdint.h>

struct LoxArguments_tag;
#define LOX_CALLABLE(name) struct Object_tag *name(struct LoxArguments_tag *args, void *context)
typedef LOX_CALLABLE(lox_callable_function);

typedef struct LoxCallable_tag
{
    lox_callable_function *function;
    int32_t arity;
} LoxCallable;

LOX_CALLABLE(lox_clock);
LOX_CALLABLE(lox_help);
LOX_CALLABLE(lox_env);
LOX_CALLABLE(lox_quit);

LoxCallable * callableInit(lox_callable_function *f, int32_t arity);
void callableFree(LoxCallable *callable);

inline int32_t callableArity(const LoxCallable *f)
{
    return f->arity;
}

#include "objects.h"

inline bool isLoxCallable(const Object *callee)
{
    return callee->type == OT_CALLABLE;
}

#endif /* lox_callable_h */
