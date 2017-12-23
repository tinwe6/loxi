//
//  lox_instance.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 08/11/2017.
//

#ifndef lox_instance_h
#define lox_instance_h

#include "error.h"
#include "lox_class.h"
#include "token.h"
#include "common.h"

typedef struct
{
    const char *name;
    Object *value;
} FieldEntry;

typedef struct LoxInstance_tag
{
    LoxClass *klass;
    // TODO: Use a hash table instead of a simple array to store the fields?
    FieldEntry fields[LOX_INSTANCE_MAX_FIELDS];
    int32_t fieldsCount;
    int32_t marked;
} LoxInstance;

LoxInstance * instanceInit(LoxClass *klass);
void instanceFree(LoxInstance *instance);
char * instanceToString(const LoxInstance *instance);
Object * instanceGet(LoxInstance *instance, const Token *property, Error **error, GarbageCollector *collector);
void instanceSet(LoxInstance *instance, const Token *property, Object *value);

inline bool isLoxInstance(const Object *function)
{
    return function->type == OT_INSTANCE;
}

#endif /* lox_instance_h */
