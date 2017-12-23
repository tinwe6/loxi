//
//  lox_class.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 08/11/2017.
//

#ifndef lox_class_h
#define lox_class_h

#include "lox_callable.h"
#include "memory.h"
#include "objects.h"
#include "string.h"

struct Interpreter_tag;
typedef struct Interpreter_tag Interpreter;

typedef struct
{
    const char *name;
    LoxFunction *function;
} MethodEntry;

typedef struct LoxClass_tag
{
    LoxCallable *callable;
    const char *name;
    LoxClass *superClass;
    // TODO: Use a hash table instead of a simple array to store the methods?
    MethodEntry *methods;
    int32_t methodsCount;
    int32_t marked;
} LoxClass;

struct LoxClassInitializerContext
{
    Interpreter *interpreter;
    LoxClass *klass;
    Error *error;
};

LoxClass * classInit(const char *name, LoxClass *superClass, MethodEntry *methods, int32_t methodsCount);
void classFree(LoxClass *klass);
char * classToString(const LoxClass *klass);
LoxFunction * findMethod(LoxInstance *instance, LoxClass *klass, const char *name, Error **error, GarbageCollector *collector);

inline bool isLoxClass(const Object* klass)
{
    return klass->type == OT_CLASS;
}

#endif /* lox_class_h */
