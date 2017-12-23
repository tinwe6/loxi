//
//  objects.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 17/10/2017.
//

#ifndef objects_h
#define objects_h

#include "common.h"

#include <inttypes.h>

// NOTE: definition of the object types

#define FOREACH_OBJECT(obj)                              \
  obj(NIL)       obj(BOOLEAN)  obj(CALLABLE) obj(CLASS)  \
  obj(FUNCTION)  obj(INSTANCE) obj(NUMBER)   obj(STRING) \
  obj(ARGUMENTS) obj(UNUSED)

typedef enum ObjectType
{
#define DEFINE_ENUM_TYPE(type) OT_##type,
    FOREACH_OBJECT(DEFINE_ENUM_TYPE)
#undef DEFINE_ENUM_TYPE
} ObjectType;

#define OBJECT_VALUE_OFFSET sizeof(ObjectType)

struct LoxArguments_tag;
typedef struct LoxArguments_tag LoxArguments;

struct LoxCallable_tag;
typedef struct LoxCallable_tag LoxCallable;

struct LoxClass_tag;
typedef struct LoxClass_tag LoxClass;

struct LoxFunction_tag;
typedef struct LoxFunction_tag LoxFunction;

struct LoxInstance_tag;
typedef struct LoxInstance_tag LoxInstance;

struct GarbageCollector_tag;
typedef struct GarbageCollector_tag GarbageCollector;

typedef struct Object_tag
{
    union {
        double number;
        bool boolean;
        char *string;
        LoxArguments *arguments;
        LoxCallable *callable;
        LoxClass *klass;
        LoxFunction *function;
        LoxInstance *instance;
        void *value;
    };
    struct Object_tag *next;
    ObjectType type;
    int32_t marked;
#ifdef DEBUG
    int32_t debugID;
#endif
} Object;

#include "lox_callable.h"

const char * obj_typeLiteral(ObjectType type);
Object * obj_dup(const Object *obj, GarbageCollector *collector);
bool isTruthy(Object *object);
bool isEqual(Object *a, Object *b);
char * obj_stringify(const Object *object);
char * obj_description(const Object *object);
void obj_print(Object *obj);

#include "string.h"
#include "lox_callable.h"

#if DEBUG
extern int32_t obj_debugID;
#endif

struct GarbageCollector_tag;
typedef struct GarbageCollector_tag GarbageCollector;
Object * gcGetObject(GarbageCollector *collector);

inline Object * objNew(ObjectType type, GarbageCollector *collector)
{
    Object *object = gcGetObject(collector);
    object->type = type;
#if DEBUG
    object->debugID = obj_debugID++;
#endif
    return object;
}

inline Object * obj_newBoolean(bool value, GarbageCollector *collector)
{
    Object *object = objNew(OT_BOOLEAN, collector);
    object->boolean = value;
    return object;
}

inline Object * obj_wrapCallable(LoxCallable *callable, GarbageCollector *collector)
{
    Object *object = objNew(OT_CALLABLE, collector);
    object->callable = callable;
    return object;
}

inline Object * obj_newCallable(LoxCallable *callable, GarbageCollector *collector)
{
    Object *object = objNew(OT_CALLABLE, collector);
    object->callable = callableInit(callable->function, callable->arity);
    return object;
}

inline Object * obj_wrapClass(LoxClass *klass, GarbageCollector *collector)
{
    Object *object = objNew(OT_CLASS, collector);
    object->klass = klass;
    return object;
}

inline Object * obj_wrapFunction(LoxFunction *function, GarbageCollector *collector)
{
    Object *object = objNew(OT_FUNCTION, collector);
    object->function = function;
    return object;
}

inline Object * obj_wrapInstance(LoxInstance *instance, GarbageCollector *collector)
{
    Object *object = objNew(OT_INSTANCE, collector);
    object->instance = instance;
    return object;
}

inline Object * obj_newNil(GarbageCollector *collector)
{
    Object *object = objNew(OT_NIL, collector);
    return object;
}

inline Object * obj_newNumber(double value, GarbageCollector *collector)
{
    Object *object = objNew(OT_NUMBER, collector);
    object->number = value;
    return object;
}

inline Object * obj_newString(const char *str, GarbageCollector *collector)
{
    Object *object = objNew(OT_STRING, collector);
    object->string = str_dup(str);
    return object;
}

inline Object * obj_wrapString(char *str, GarbageCollector *collector)
{
    Object *object = objNew(OT_STRING, collector);
    object->string = str;
    return object;
}

inline Object * obj_wrapArguments(LoxArguments *arguments, GarbageCollector *collector)
{
    Object *object = objNew(OT_ARGUMENTS, collector);
    object->arguments = arguments;
    return object;
}

inline bool obj_unwrapBoolean(const Object *obj)
{
    assert(obj->type == OT_BOOLEAN);
    return obj->boolean;
}

inline const LoxCallable * obj_unwrapCallable(const Object *obj)
{
    assert(obj->type == OT_CALLABLE);
    return obj->callable;
}

inline LoxFunction * obj_unwrapFunction(const Object *obj)
{
    assert(obj->type == OT_FUNCTION);
    return obj->function;
}

inline LoxClass * obj_unwrapClass(const Object *obj)
{
    if (obj == NULL)
    {
        return NULL;
    }
    assert(obj->type == OT_CLASS);
    return obj->klass;
}

inline LoxInstance * obj_unwrapInstance(const Object *obj)
{
    assert(obj->type == OT_INSTANCE);
    return obj->instance;
}

inline void * obj_unwrapNil(const Object *obj)
{
    assert(obj->type == OT_NIL);
    return NULL;
}

inline double obj_unwrapNumber(const Object *obj)
{
    assert(obj->type == OT_NUMBER);
    return obj->number;
}

inline const char * obj_unwrapString(const Object *obj)
{
    assert(obj->type == OT_STRING);
    return obj->string;
}

inline LoxArguments * obj_unwrapArguments(const Object *obj)
{
    assert(obj->type == OT_ARGUMENTS);
    return obj->arguments;
}

#endif /* objects_h */
