//
//  lox_class.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 08/11/2017.
//

#include "garbage_collector.h"
#include "interpreter.h"
#include "lox_class.h"
#include "lox_function.h"
#include "lox_instance.h"

extern inline bool isLoxClass(const Object *klass);

LOX_CALLABLE(class_call)
{
    struct LoxClassInitializerContext *initializerContext = (struct LoxClassInitializerContext *)context;
    Interpreter *interpreter = initializerContext->interpreter;
    LoxClass *klass = initializerContext->klass;

    LoxInstance *instance = instanceInit(klass);
    LoxFunction *initializer = findMethod(instance, klass, "init", &initializerContext->error, interpreter->collector);
    if(initializerContext->error)
    {
        return NULL;
    }
    Object *result;
    if (initializer != NULL)
    {
        result = function_call(initializer, args, &initializerContext->error, interpreter);

        env_release(initializer->closure);
        lox_free(initializer);
        
        if(initializerContext->error)
        {
            return NULL;
        }
    }
    else
    {
        result = obj_wrapInstance(instance, interpreter->collector);
    }
    return result;
}

const LoxFunction * findClassMethod(const LoxClass *klass, const char *name)
{
    for (int32_t index = 0; index < klass->methodsCount; ++index)
    {
        if(str_isEqual(name, klass->methods[index].name))
        {
            const LoxFunction *method = klass->methods[index].function;
            return method;
        }
    }
    return NULL;
}

int32_t class_arity(const LoxClass *klass)
{
    const LoxFunction *initializer = findClassMethod(klass, "init");
    if (initializer == NULL)
    {
        return 0;
    }
    int32_t arity = initializer->declaration->arity;
    return arity;
}

LoxClass * classInit(const char *name, LoxClass *superClass, MethodEntry *methods, int32_t methodsCount)
{
    LoxClass* klass = lox_alloc(LoxClass);
    if(klass == NULL)
    {
        fatal_outOfMemory();
    }
    assert(klass != NULL);
    klass->marked = GC_CLEAR;
    klass->name = name;
    klass->superClass = superClass;
    klass->methods = methods;
    klass->methodsCount = methodsCount;
    int32_t arity = class_arity(klass);
    klass->callable = callableInit(class_call, arity);
    return klass;
}

static void classFreeMethods(LoxClass *klass)
{
    for (int32_t index = 0; index < klass->methodsCount; ++index)
    {
        function_free(klass->methods[index].function);
    }
    lox_free(klass->methods);
}

void classFree(LoxClass *klass)
{
    classFreeMethods(klass);
    callableFree(klass->callable);
    lox_free(klass);
}

char * classToString(const LoxClass *klass)
{
    char *str = str_dup(klass->name);
    str_appendLiteral(str, " class");
    return str;
}

LoxFunction * findMethod(LoxInstance *instance, LoxClass *klass, const char *name, Error **error, GarbageCollector *collector)
{
    for (int32_t index = 0; index < klass->methodsCount; ++index)
    {
        if(str_isEqual(name, klass->methods[index].name))
        {
            const LoxFunction *function = klass->methods[index].function;
            LoxFunction *result = function_bind(function, instance, error, collector);
            return result;
        }
    }
    
    if (klass->superClass != NULL)
    {
        return findMethod(instance, klass->superClass, name, error, collector);
    }
    
    return NULL;
}




