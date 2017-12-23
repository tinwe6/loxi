//
//  lox_instance.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 08/11/2017.
//

#include "garbage_collector.h"
#include "lox_function.h"
#include "lox_instance.h"
#include "memory.h"
#include "string.h"

extern inline bool isLoxInstance(const Object *function);

LoxInstance * instanceInit(LoxClass *klass)
{
    LoxInstance *instance = lox_alloc(LoxInstance);
    if(instance == NULL)
    {
        fatal_outOfMemory();
    }
    instance->marked = GC_CLEAR;
    instance->klass = klass;
    instance->fieldsCount = 0;
    return instance;
}

void instanceFree(LoxInstance *instance)
{
    // NOTE: the fields are objects, and they're taken
    //       care of by the garbage collector.
    lox_free(instance);
}

char * instanceToString(const LoxInstance *instance)
{
    char *str = str_dup(instance->klass->name);
    str_appendLiteral(str, " instance");
    return str;
}

// Returns the index of the field named `name`.
// If such a field does not exist, returns -1.
static int32_t indexOf(const char *name, const FieldEntry *fields, int32_t fieldsCount)
{
    for (int32_t index = 0; index < fieldsCount; ++index)
    {
        if (str_isEqual(name, fields[index].name))
        {
            return index;
        }
    }
    return -1;
}

// NOTE: We first look for a field, and if not found for a method.
//       This implies that fields shadow methods.
Object * instanceGet(LoxInstance *instance, const Token *property, Error **error, GarbageCollector *collector)
{
    const char *name = get_identifier_name(property);
    
    int32_t index = indexOf(name, instance->fields, instance->fieldsCount);
    if (index != -1)
    {
        Object *result = obj_dup(instance->fields[index].value, collector);
        return result;
    }
    
    LoxFunction *method = findMethod(instance, instance->klass, name, error, collector);
    if (*error)
    {
        return NULL;
    }
    if (method != NULL)
    {
        Object *result = obj_wrapFunction(method, collector);
        env_release(method->closure);
        return result;
    }
    
    *error = initErrorIdentifier("Undefined property '", property, "'.");
    return NULL;
}

// Stores a duplicate of value in the field `property` of instance.
// NOTE: the value is duplicated since the interpreter uses it as return value too.
void instanceSet(LoxInstance *instance, const Token *property, Object *value)
{
    const char *name = get_identifier_name(property);
    int32_t index = indexOf(name, instance->fields, instance->fieldsCount);
    if (index == -1)
    {
        assert(instance->fieldsCount < LOX_INSTANCE_MAX_FIELDS);
        index = instance->fieldsCount;
        instance->fields[index].name = name;
        instance->fieldsCount++;
    }
    else
    {
        assert(instance->fields[index].value != NULL);
    }
    instance->fields[index].value = value;
}

