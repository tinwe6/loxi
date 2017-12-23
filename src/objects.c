//
//  objects.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 17/10/2017.
//

#include "garbage_collector.h"
#include "lox_callable.h"
#include "lox_class.h"
#include "lox_function.h"
#include "lox_instance.h"
#include "memory.h"
#include "objects.h"
#include "string.h"

#include <math.h>

extern inline Object * objNew(ObjectType type, GarbageCollector *collector);
extern inline Object * obj_newBoolean(bool value, GarbageCollector *collector);
extern inline Object * obj_wrapCallable(LoxCallable *callable, GarbageCollector *collector);
extern inline Object * obj_newCallable(LoxCallable *callable, GarbageCollector *collector);
extern inline Object * obj_wrapClass(LoxClass *klass, GarbageCollector *collector);
extern inline Object * obj_wrapFunction(LoxFunction *function, GarbageCollector *collector);
extern inline Object * obj_wrapInstance(LoxInstance *instance, GarbageCollector *collector);
extern inline Object * obj_newNil(GarbageCollector *collector);
extern inline Object * obj_newNumber(double value, GarbageCollector *collector);
extern inline Object * obj_newString(const char *str, GarbageCollector *collector);
extern inline Object * obj_wrapString(char *str, GarbageCollector *collector);
extern inline Object * obj_wrapArguments(LoxArguments *arguments, GarbageCollector *collector);

extern inline bool obj_unwrapBoolean(const Object *obj);
extern inline const LoxCallable * obj_unwrapCallable(const Object *obj);
extern inline LoxClass * obj_unwrapClass(const Object *obj);
extern inline LoxFunction * obj_unwrapFunction(const Object *obj);
extern inline LoxInstance * obj_unwrapInstance(const Object *obj);
extern inline void * obj_unwrapNil(const Object *obj);
extern inline double obj_unwrapNumber(const Object *obj);
extern inline const char * obj_unwrapString(const Object *obj);
extern inline LoxArguments * obj_unwrapArguments(const Object *obj);

#if DEBUG
// NOTE: Here we store the last debug ID that was assigned
int32_t obj_debugID = 0;
#endif

// Array of the object type names
static const char * const object_type_string[] =
{
#define DEFINE_TYPE_STRING(type) XSTR(type),
    FOREACH_OBJECT(DEFINE_TYPE_STRING)
#undef DEFINE_TYPE_STRING
};

// Returns a string literal with the name of the object type
const char * obj_typeLiteral(ObjectType type)
{
    return object_type_string[type];
}

Object * obj_dup(const Object *obj, GarbageCollector *collector)
{
    switch (obj->type) {
        case OT_BOOLEAN:
            return obj_newBoolean(obj->boolean, collector);
        case OT_CALLABLE:
            return obj_newCallable(obj->callable, collector);
        case OT_CLASS:
            return obj_wrapClass(obj->klass, collector);
        case OT_INSTANCE:
            return obj_wrapInstance(obj->instance, collector);
        case OT_FUNCTION:
            return obj_wrapFunction(obj->function, collector);
        case OT_NIL:
            return obj_newNil(collector);
        case OT_NUMBER:
            return obj_newNumber(obj->number, collector);
        case OT_STRING:
            return obj_newString(obj->string, collector);
        case OT_ARGUMENTS:
            // NOTE: we never duplicate an Arguments object.
            INVALID_CASE;
        case OT_UNUSED:
            INVALID_CASE;
    }
}

bool isTruthy(Object *object)
{
    if (object->type == OT_NIL)
    {
        return false;
    }
    if (object->type == OT_BOOLEAN)
    {
        return obj_unwrapBoolean(object);
    }
    return true;
}

bool isEqual(Object *a, Object *b)
{
    if (a->type == OT_NIL && b->type == OT_NIL)
    {
        return true;
    }
    if (a->type == OT_BOOLEAN && b->type == OT_BOOLEAN)
    {
        return obj_unwrapBoolean(a) == obj_unwrapBoolean(b);
    }
    if (a->type == OT_CALLABLE && b->type == OT_CALLABLE)
    {
        const LoxCallable *callableA = obj_unwrapCallable(a);
        const LoxCallable *callableB = obj_unwrapCallable(b);
        return callableA->function == callableB->function;
    }
    if (a->type == OT_FUNCTION && b->type == OT_FUNCTION)
    {
        const LoxFunction *funcA = obj_unwrapFunction(a);
        const LoxFunction *funcB = obj_unwrapFunction(b);
        return ((funcA->declaration == funcB->declaration) && (funcA->closure == funcB->closure));
    }
    if (a->type == OT_NUMBER && b->type == OT_NUMBER)
    {
        return obj_unwrapNumber(a) == obj_unwrapNumber(b);
    }
    if (a->type == OT_STRING && b->type == OT_STRING)
    {
        return str_isEqual(obj_unwrapString(a), obj_unwrapString(b));
    }
    return false;
}

// Returns a string that represents the object `obj`.
char * obj_stringify(const Object *object)
{
    char *string;
    if (object == NULL)
    {
        return str_fromLiteral("nil");
    }
    
    switch(object->type)
    {
        case OT_BOOLEAN:
        {
            bool value = obj_unwrapBoolean(object);
            string = str_fromLiteral(value ? "true" : "false");
        } break;
            
        case OT_CALLABLE:
        {
            // TODO: include the name of the native function.
            string = str_fromLiteral("<fn ");
            str_appendLiteral(string, ">");
        } break;

        case OT_CLASS:
        {
            const LoxClass *klass = obj_unwrapClass(object);
            string = str_dup(klass->name);
        } break;

        case OT_INSTANCE:
        {
            const LoxInstance *instance = obj_unwrapInstance(object);
            string = instanceToString(instance);
        } break;

        case OT_FUNCTION:
        {
            const LoxFunction *function = obj_unwrapFunction(object);
            string = str_fromLiteral("<fn ");
            str_append(string, get_identifier_name(function->declaration->name));
            str_appendLiteral(string, ">");
        } break;
            
        case OT_NIL:
        {
            string = str_fromLiteral("nil");
        } break;
            
        case OT_NUMBER:
        {
            double value = obj_unwrapNumber(object);
            if (value == 0)
            {
                if (signbit(value))
                {
                    string = str_fromLiteral("-0");
                }
                else
                {
                    string = str_fromLiteral("0");
                }
            }
            else if(value == (double)(int)value)
            {
                // Note: The number is an integer!
                string = str_fromInt64((int64_t)value);
            }
            else
            {
                string = str_fromDouble(value);
            }
        } break;
            
        case OT_STRING:
        {
            string = str_dup(obj_unwrapString(object));
        } break;
            
        case OT_ARGUMENTS:
        {
            string = str_fromLiteral("[args]");
        } break;
            
        case OT_UNUSED:
            INVALID_CASE;
    }
    return string;
}

// Returns a description of the object.
char * obj_description(const Object *object)
{
    char *string;
    if (object == NULL)
    {
        return str_fromLiteral("nil");
    }
    
    switch(object->type)
    {
        case OT_BOOLEAN:
        {
            bool value = obj_unwrapBoolean(object);
            string = str_fromLiteral(value ? "true" : "false");
        } break;
            
        case OT_CALLABLE:
        {
            const LoxCallable *callable = obj_unwrapCallable(object);
            string = str_fromLiteral("native function (");
            str_append(string, str_fromInt64(callable->arity));
            str_appendLiteral(string, " parameters)");
        } break;
            
        case OT_CLASS:
        {
            const LoxClass *klass = obj_unwrapClass(object);
            string = classToString(klass);
        } break;
            
        case OT_INSTANCE:
        {
            const LoxInstance *instance = obj_unwrapInstance(object);
            string = instanceToString(instance);
        } break;
            
        case OT_FUNCTION:
        {
            const LoxFunction *function = obj_unwrapFunction(object);
            string = str_fromLiteral("function (");
            str_append(string, str_fromInt64(function->declaration->arity));
            str_appendLiteral(string, " parameters)");
            if (function->isInitializer)
            {
                str_appendLiteral(string, " - class initializer");
            }
        } break;
            
        case OT_NIL:
        {
            string = str_fromLiteral("nil");
        } break;
            
        case OT_NUMBER:
        {
            double value = obj_unwrapNumber(object);
            if (value == 0)
            {
                if (signbit(value))
                {
                    string = str_fromLiteral("-0");
                }
                else
                {
                    string = str_fromLiteral("0");
                }
            }
            else if(value == (double)(int)value)
            {
                // Note: The number is an integer!
                string = str_fromInt64((int64_t)value);
            }
            else
            {
                string = str_fromDouble(value);
            }
        } break;
            
        case OT_STRING:
        {
            string = str_fromLiteral("\"");
            str_append(string, obj_unwrapString(object));
            str_appendLiteral(string, "\"");
        } break;

        case OT_ARGUMENTS:
        {
            string = str_fromLiteral("[args]");
        } break;

        case OT_UNUSED:
            INVALID_CASE;
    }
    return string;
}

void obj_print(Object *obj)
{
    char *str = obj_stringify(obj);
    printf("%s\n", str);
    str_free(str);
}
