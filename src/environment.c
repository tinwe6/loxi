//
//  environment.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 18/10/2017.
//

#include "environment.h"

#include "garbage_collector.h"
#include "lox_function.h"
#include "memory.h"
#include "string.h"
#include "token.h"

extern inline bool env_isGlobal(const Environment *environment);
extern inline void env_release(Environment *environment);
extern inline Error * env_defineLocal(const Token *var, Object *value, Environment *environment);
extern inline void env_defineThis(Object *value, Environment *environment);
extern inline void env_defineSuper(Object *value, Environment *environment);

// Initializes and returns a new environment.
// Returns NULL and sets error if there was a stack overflow.
Environment * env_init(Environment *enclosing, Error **error, GarbageCollector *collector)
{
    Environment *environment = gcGetEnvironment(collector);
    if (environment == NULL)
    {
        *error = initError(NULL, "Stack overflow.");
        return NULL;
    }
    environment->enclosing = enclosing;
    environment->slotsUsed = 0;
    environment->isActive = true;

#ifdef DEBUG
    static int32_t debugID = 1;
    if(enclosing == NULL)
    {
        // NOTE: global environment has id 0.
        environment->debugID = 0;
    }
    else
    {
        environment->debugID = debugID++;
    }
#endif
    return environment;
}

// Initializes and returns the global environment.
Environment * env_initGlobal(GarbageCollector *collector)
{
    EnvironmentGlobal *globals = lox_alloc(EnvironmentGlobal);
    Environment *environment = &globals->environment;
    environment->enclosing = NULL;
    environment->slotsUsed = 0;
    environment->isActive = true;
    
#ifdef DEBUG
    // NOTE: global environment has always id 0.
    environment->debugID = 0;
#endif

#ifdef ENV_GLOBALS_USE_HASH
    for(int32_t index = 0; index < ENV_GLOBAL_HASH_SIZE; ++index)
#else
    for(int32_t index = 0; index < ENV_MAX_CAPACITY; ++index)
#endif
    {
        GLOBALS_NAME(globals, index) = NULL;
    }

    gcSetGlobalEnvironment(environment, collector);
    
    return environment;
}

// Frees the environment and its contents.
// NOTE: The garbage collector takes care of freeing the values.
void env_free(Environment *environment)
{
    if (env_isGlobal(environment))
    {
#ifdef ENV_GLOBALS_USE_HASH
        for(int32_t index = 0; index < ENV_GLOBAL_HASH_SIZE; ++index)
        {
            if (GLOBALS_NAME(environment, index) != NULL)
            {
                str_free(GLOBALS_NAME(environment, index));
            }
        }
#else
        for(int32_t index = 0; index < environment->slotsUsed; ++index)
        {
            str_free(GLOBALS_NAME(environment, index));
        }
#endif
    }
    lox_free(environment);
}

// Returns the index of the variable `name` in the global environment.
// If a variable with that name is not yet defined, returns the index
// of the next free slot.
static int32_t env_indexOf(const char *name, Environment *globals)
{
    assert(env_isGlobal(globals));

#ifdef ENV_GLOBALS_USE_HASH
    unsigned long hash = str_hash(name);
//    int32_t startIndex = (int32_t)(hash % ENV_GLOBAL_HASH_SIZE);
    int32_t startIndex = (int32_t)(hash & ENV_GLOBAL_HASH_MASK);
    int32_t index = startIndex;
    while ((GLOBALS_NAME(globals, index) != NULL) &&
           (!str_isEqual(GLOBALS_NAME(globals, index), name)))
    {
        ++index;
        if (index > ENV_GLOBAL_HASH_SIZE)
        {
            index = 0;
        }
    }
#else
    int32_t index = 0;
    while(index < globals->slotsUsed)
    {
        if (str_isEqual(GLOBALS_NAME(globals, index), name))
        {
            break;
        }
        ++index;
    }
#endif
    
    return index;
}

// Defines in `environment` a new variable. Its name is in the
// identifier token `var`, its value is carried by the object `value`.
// Returns NULL
Error * env_define(const Token *var, Object *value, Environment *environment)
{
    Error *error;
    if (env_isGlobal(environment))
    {
        error = env_defineGlobal(var, value, environment);
    }
    else
    {
        error = env_defineLocal(var, value, environment);
    }
    return error;
}

// Defines a new variable in the global environment.
// NOTE: we allow redefinitions of global variables
Error * env_defineGlobal(const Token *var, Object *value, Environment *globals)
{
    assert(env_isGlobal(globals));

    const char *name = get_identifier_name(var);
#ifdef ENV_GLOBALS_USE_HASH
    int32_t hashIndex = env_indexOf(name, globals);
    if (GLOBALS_NAME(globals, hashIndex) == NULL)
    {
        if (globals->slotsUsed == ENV_MAX_CAPACITY)
        {
            return initError(var, "Too many constants in one chunk.");
        }
        GLOBALS_NAME(globals, hashIndex) = str_dup(name);
        GLOBALS_INDEX(globals, hashIndex) = globals->slotsUsed;
        globals->slotsUsed++;
    }
    int32_t index = GLOBALS_INDEX(globals, hashIndex);
#else
    int32_t index = env_indexOf(name, globals);
    if (index == globals->slotsUsed)
    {
        // NOTE: This test could be directly handled by the resolver.
        if (globals->slotsUsed == ENV_MAX_CAPACITY)
        {
            return initError(var, "Too many constants in one chunk.");
        }
        GLOBALS_NAME(globals, globals->slotsUsed) = str_dup(name);
        globals->slotsUsed++;
    }
#endif
    globals->values[index] = value;
    
    return NULL;
}

// Defines a native function in the global environment.
void env_defineNative(const char *name, Object *value, Environment *globals)
{
    assert(env_isGlobal(globals));
    assert(globals->slotsUsed < ENV_MAX_CAPACITY);
#ifdef ENV_GLOBALS_USE_HASH
    char *nameStr = str_fromLiteral(name);
    int32_t hashIndex = env_indexOf(nameStr, globals);
    assert(GLOBALS_NAME((EnvironmentGlobal *)globals, hashIndex) == NULL);
    GLOBALS_NAME(globals, hashIndex) = nameStr;
    GLOBALS_INDEX(globals, hashIndex) = globals->slotsUsed;
#else
    GLOBALS_NAME(globals, globals->slotsUsed) = str_fromLiteral(name);
#endif
    globals->values[globals->slotsUsed] = value;
    globals->slotsUsed++;
}

static inline Environment * env_ancestor(int32_t distance, Environment *environment)
{
    for (int32_t i = 0; i < distance; i++)
    {
        environment = environment->enclosing;
        assert(environment != NULL);
    }
    return environment;
}

// Looks up the variable `identifier` in `environment` and returns
// a duplicate object carrying its value if it finds it, or NULL
// in which case `error` is set to an appropriate value.
// The variable location is determined by `distance` and `index`, as
// calculated by the resolver.
Object * env_getAt(const Token *identifier, int32_t distance, int32_t index, Environment *environment, Error **error, GarbageCollector *collector)
{
    assert(!env_isGlobal(environment));
    Environment *env = env_ancestor(distance, environment);
    assert(index >= 0 && index < env->slotsUsed);
    Object *value = env->values[index];
    if(value == NULL)
    {
        // NOTE: the variable was defined but not assigned a value.
#ifdef LOX_ACCESSING_UNINIT_VAR_ERROR
        *error = initErrorIdentifier("Accessing uninitialized variable '", identifier, "'.");
#else
        value = obj_newNil(collector);
#endif
    }
    else
    {
        value = obj_dup(value, collector);
    }

    return value;
}

// Assigns a new value to the object at (`distance`, `index`) relative to `environment`.
void env_assignAt(Object *value, int32_t distance, int32_t index, Environment *environment, GarbageCollector *collector)
{
    assert(value != NULL);
    Environment *env = env_ancestor(distance, environment);
    
    assert(index >= 0 && index < env->slotsUsed);
    env->values[index] = value;
}

// Looks up the variable in the global environment, using a hash table.
// If it is defined, returns a duplicate of its value, otherwise NULL and
// error is set to an appropriate value.
Object * env_getGlobal(const Token *identifier, Environment *globals, Error **error, GarbageCollector *collector)
{
    assert(env_isGlobal(globals));
    Object *value;
#ifdef ENV_GLOBALS_USE_HASH
    int32_t hashIndex = env_indexOf(get_identifier_name(identifier), globals);
    if(GLOBALS_NAME(globals, hashIndex) != NULL)
    {
        int32_t index = GLOBALS_INDEX(globals, hashIndex);
#else
    int32_t index = env_indexOf(get_identifier_name(identifier), globals);
    if(index < globals->slotsUsed)
    {
#endif
        value = globals->values[index];
        if(value == NULL)
        {
            // NOTE: the variable was defined but not assigned a value.
#ifdef LOX_ACCESSING_UNINIT_VAR_ERROR
            *error = initErrorIdentifier("Accessing uninitialized variable '", identifier, "'.");
#else
            value = obj_newNil(collector);
#endif
        }
        else
        {
            value = obj_dup(value, collector);
        }
    }
    else
    {
        value = NULL;
        *error = initErrorIdentifier("Undefined variable '", identifier, "'.");
    }
    return value;
}

Error *env_assignGlobal(const Token *identifier, Object *value, Environment *globals, GarbageCollector *collector)
{
    assert(env_isGlobal(globals));
    assert(value != NULL);
    
    Error *error = NULL;
    const char *name = get_identifier_name(identifier);
#ifdef ENV_GLOBALS_USE_HASH
    int32_t hashIndex = env_indexOf(name, globals);
    if ( GLOBALS_NAME(globals, hashIndex) != NULL)
    {
        int32_t index = GLOBALS_INDEX(globals, hashIndex);
#else
    int32_t index = env_indexOf(name, globals);
    if(index < globals->slotsUsed)
    {
#endif
        globals->values[index] = value;
    }
    else
    {
        error = initErrorIdentifier("Undefined variable '", identifier, "'.");
    }
    return error;
}

/***********************/
 
static int32_t env_debugID(const Environment *env)
{
    if (env)
    {
#ifdef DEBUG
        return env->debugID;
#else
        return (int32_t)env;
#endif
    }
    return -1;
}

static int32_t env_enclosingDebugID(const Environment *env)
{
    if (env)
    {
#ifdef DEBUG
        return env_debugID(env->enclosing);
#else
        return (int32_t)env->enclosing;
#endif
    }
    return -1;
}

static void env_printGlobal(const Environment *globals)
{
    assert(env_isGlobal(globals));
    printf("Global environment - id: %d, %d symbols defined\n", env_debugID(globals), globals->slotsUsed);
#ifdef ENV_GLOBALS_USE_HASH
    for(int32_t index = 0; index < ENV_GLOBAL_HASH_SIZE; ++index)
    {
        if (GLOBALS_NAME(globals, index) != NULL)
        {
            char *value = obj_description(globals->values[GLOBALS_INDEX(globals, index)]);
            printf(" %d. %s: %s\n", GLOBALS_INDEX(globals, index), GLOBALS_NAME(globals, index), value);
            str_free(value);
        }
    }
#else
    for(int32_t index = 0; index < globals->slotsUsed; ++index)
    {
        char *value = obj_description(globals->values[index]);
        printf(" %d. %s: %s\n", index,  GLOBALS_NAME(globals, index), value);
        str_free(value);
    }
#endif
}

static void env_printLocal(const Environment *environment)
{
    assert(!env_isGlobal(environment));
    printf("Environment id: %d, %d symbols defined, (enclosing id: %d)\n", env_debugID(environment), environment->slotsUsed, env_enclosingDebugID(environment));
    for(int32_t index = 0; index < environment->slotsUsed; ++index)
    {
        char *value = obj_description(environment->values[index]);
        printf(" %d. %s\n", index, value);
        str_free(value);
    }
}

void env_printReport(const Environment *environment)
{
    if (env_isGlobal(environment))
    {
        env_printGlobal(environment);
    }
    else
    {
        env_printLocal(environment);
    }
}

void env_printReportAll(const Environment *environment)
{
    printf("\n--- Environment Report -------\n");
    while (environment != NULL)
    {
        env_printReport(environment);
        environment = environment->enclosing;
    }
    printf("--- Environment Report end ---\n");
}

