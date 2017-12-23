//
//  environment.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 18/10/2017.
//

#ifndef environment_h
#define environment_h

#include <inttypes.h>
#include "error.h"
#include "objects.h"
#include "token.h"
#include "common.h"

/* Local environments */

#define ENV_MAX_CAPACITY (LOX_MAX_LOCAL_VARIABLES + 1)

typedef struct Environment_tag
{
    struct Environment_tag *enclosing;
    Object *values[ENV_MAX_CAPACITY];
    int32_t slotsUsed;

    /* Members used by the garbage collector */
    int32_t marked;
    // NOTE: next environment in the list of environments kept by the GC
    struct Environment_tag *next;
    // NOTE: if true, the environment and all the values it contains are
    //       marked by the garbage collector
    bool isActive;

#ifdef DEBUG
    int32_t debugID;
#endif
} Environment;

#ifdef ENV_GLOBALS_USE_HASH
typedef struct
{
    char *name;
    uint32_t index;
} EnvHashEntry;
#endif

/* Global environment */

typedef struct
{
    Environment environment;
    // NOTE: This is the hash table for the global variables.
    // If names[i] == NULL, the i-th slot is available; if not
    // index[i] yields the index in the values array where the
    // object assigned to names[i] is stored.
#ifdef ENV_GLOBALS_USE_HASH
    EnvHashEntry table[ENV_GLOBAL_HASH_SIZE];
#else
    char *names[ENV_MAX_CAPACITY];
#endif
} EnvironmentGlobal;

#ifdef ENV_GLOBALS_USE_HASH
#define GLOBALS_NAME(env, i) ((EnvironmentGlobal *)env)->table[i].name
#define GLOBALS_INDEX(env, i) ((EnvironmentGlobal *)env)->table[i].index
#else
#define GLOBALS_NAME(env, i) ((EnvironmentGlobal *)env)->names[i]
#endif

Environment * env_init(Environment *enclosing, Error **error, GarbageCollector *collector);
Environment * env_initGlobal(GarbageCollector *collector);
void env_free(Environment *environment);
void env_freeObjects(Environment *environment);

Error * env_define(const Token *var, Object *value, Environment *environment);
Error * env_defineGlobal(const Token *var, Object *value, Environment *globals);
void env_defineNative(const char *name, Object *value, Environment *globals);
Object * env_getAt(const Token *identifier, int32_t distance, int32_t index, Environment *environment, Error **error, GarbageCollector *collector);
Error * env_assign(const char *name, Object *value, Environment *environment);
void env_assignAt(Object *value, int32_t distance, int32_t index, Environment *environment, GarbageCollector *collector);

Object * env_getGlobal(const Token *identifier, Environment *globals, Error **error, GarbageCollector *collector);
Error * env_assignGlobal(const Token *identifier, Object *value, Environment *globals, GarbageCollector *collector);

void env_printReport(const Environment *environment);
void env_printReportAll(const Environment *environment);

// Returns true iff `environment` is the global environment
inline bool env_isGlobal(const Environment *environment)
{
    return environment->enclosing == NULL;
}

// Tell the garbage collector that it shouldn't automatically
// retain all objects stored in `environment`.
inline void env_release(Environment *environment)
{
    environment->isActive = false;
}

// Defines a new variable in a local environment environment. Its value
// is retained by the environment: the object is *not* duplicated.
inline Error * env_defineLocal(const Token *var, Object *value, Environment *environment)
{
    assert(!env_isGlobal(environment));
    Error *error = NULL;
    
    // NOTE: This test could be directly handled by the resolver.
    if(environment->slotsUsed < ENV_MAX_CAPACITY)
    {
        environment->values[environment->slotsUsed++] = value;
    }
    else
    {
        error = initError(var, "Too many constants in one chunk.");
    }
    
    return error;
}

// Defines "this" in `environment`. `value` must be an instance object.
inline void env_defineThis(Object *value, Environment *environment)
{
    assert(!env_isGlobal(environment));
    assert(value->type == OT_INSTANCE);
    environment->values[environment->slotsUsed++] = value;
}

// Defines "super" in `environment`. `value` must be an class object.
inline void env_defineSuper(Object *value, Environment *environment)
{
    assert(!env_isGlobal(environment));
    assert(value->type == OT_CLASS);
    environment->values[environment->slotsUsed++] = value;
}

#endif /* environment_h */
