//
//  garbage_collector.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 20/11/2017.
//

#include "garbage_collector.h"

#include <stdio.h>

#include "common.h"
#include "lox_class.h"
#include "lox_function.h"
#include "lox_instance.h"
#include "objects.h"
#include "string.h"

extern inline bool gcLock(Object *object, GarbageCollector *collector);
extern inline void gcPopLock(GarbageCollector *collector);
extern inline void gcPopLockn(int32_t n, GarbageCollector *collector);
extern inline void gcClearLocks(GarbageCollector *collector);


#define GC_OBJECT_SIZE (sizeof(Object))
#define GC_OBJECTS_PER_PAGE ((PAGE_SIZE - sizeof(MemoryPage)) / GC_OBJECT_SIZE)

static void gcAllocObjects(GarbageCollector *collector)
{
    MemoryPage *page = (MemoryPage *)lox_allocn(uint8_t, PAGE_SIZE);
    if(page == NULL)
    {
        fatal_outOfMemory();
    }
    page->next = collector->memoryPages;
    collector->memoryPages = page;
    
    Object *firstObject = (Object *)(page + 1);
    Object *object = firstObject;
    int32_t count = GC_OBJECTS_PER_PAGE;
    while (count > 0)
    {
        Object *next = object + 1;
        object->type = OT_UNUSED;
        object->next = next;
        object = next;
        --count;
    }
    assert((uint8_t *)object < (uint8_t *)page + PAGE_SIZE);
    assert((uint8_t *)(object + 1) >= (uint8_t *)page + PAGE_SIZE);
    
    (object - 1)->next = collector->firstUnused;
    collector->firstUnused = (Object *)firstObject;
    collector->objectsCount += GC_OBJECTS_PER_PAGE;
    
#ifdef GC_KEEPS_STATS
    collector->unusedObjectsCount += GC_OBJECTS_PER_PAGE;
#endif
    
    if (collector->maxObjects < collector->objectsCount)
    {
        collector->maxObjects = collector->objectsCount;
    }
}

GarbageCollector *gcInit()
{
    GarbageCollector *collector = lox_alloc(GarbageCollector);
    collector->firstObject = NULL;
    collector->laundryList = NULL;
    collector->firstUnused = NULL;
    
    collector->objectsCount = 0;
    collector->maxObjects = 0;
    
    collector->firstEnvironment = NULL;
    collector->firstUnusedEnvironment = NULL;

    collector->environmentsCount = 0;
    collector->maxEnvironments = GC_INITIAL_ENVIRONMENTS_THRESHOLD;

    collector->visitedMark = 0;
    collector->recycledMark = 1;
    collector->lockedCount = 0;

    collector->memoryPages = NULL;

    collector->activeObjectsCount = 0;
    collector->activeEnvironmentsCount = 0;

#ifdef GC_KEEPS_STATS
    collector->unusedObjectsCount = 0;
    collector->unusedEnvironmentsCount = 0;
    collector->debug_recycledObjCount = 0;
    collector->debug_recycledEnvCount = 0;
#endif

    gcAllocObjects(collector);
    gcAllocObjects(collector);
    
    return collector;
}

static inline void objFree(Object *object)
{
    assert(object->type == OT_UNUSED);
#ifdef DEBUG_SCRAMBLE_MEMORY
    assert(object->klass == DEBUG_SCRAMBLE_VALUE);
#endif
}

Object * gcGetObject(GarbageCollector *collector)
{
#ifdef GC_DEBUG
    gcCollect(collector);
#endif
#ifdef GC_KEEPS_STATS
    assert(collector->activeObjectsCount + collector->unusedObjectsCount == collector->objectsCount);
#endif
    if (collector->firstUnused == NULL)
    {
#ifdef GC_KEEPS_STATS
        assert(collector->unusedObjectsCount == 0);
#endif
        if (collector->objectsCount >= collector->maxObjects)
        {
            gcCollect(collector);
        }
        if (collector->firstUnused == NULL)
        {
            gcAllocObjects(collector);
        }
    }
    
    Object *object = collector->firstUnused;
    collector->firstUnused = object->next;

    object->next = collector->firstObject;
    collector->firstObject = object;
    ++collector->activeObjectsCount;

#ifdef GC_KEEPS_STATS
    --collector->unusedObjectsCount;
#endif
    
    object->marked = GC_CLEAR;

    return object;
}

// Returns a new environment, or NULL if it could not be allocated.
Environment * gcGetEnvironment(GarbageCollector *collector)
{
#ifdef GC_DEBUG
    gcCollect(collector);
#endif
    if(collector->firstUnusedEnvironment == NULL)
    {
        if (collector->environmentsCount >= collector->maxEnvironments)
        {
            gcCollect(collector);
        }
    }
    
    Environment *environment;
    if (collector->firstUnusedEnvironment)
    {
        environment = collector->firstUnusedEnvironment;
        collector->firstUnusedEnvironment = environment->next;
#ifdef GC_KEEPS_STATS
        --collector->unusedEnvironmentsCount;
#endif
    }
    else
    {   // Alloc a bunch of them at a time?
        if(collector->environmentsCount >= LOX_MAX_ENVIRONMENTS)
        {
            return NULL;
        }
        environment = lox_alloc(Environment);
        if(environment == NULL)
        {
            fatal_outOfMemory();
        }
        ++collector->environmentsCount;
    }
    environment->next = collector->firstEnvironment;
    collector->firstEnvironment = environment;
    environment->marked = GC_CLEAR;
    ++collector->activeEnvironmentsCount;

    return environment;
}


void gcSetGlobalEnvironment(Environment *globals, GarbageCollector *collector)
{
    ++collector->environmentsCount;
    globals->next = collector->firstEnvironment;
    collector->firstEnvironment = globals;
    globals->marked = GC_CLEAR;
    ++collector->activeEnvironmentsCount;
}

static void gcMarkEnvironment(Environment *environment, GarbageCollector *collector);
static void gcMarkObject(Object *object, GarbageCollector *collector);

static inline void gcMarkArguments(LoxArguments *args, GarbageCollector *collector)
{
    for (int32_t i = 0; i < args->count; ++i)
    {
        gcMarkObject(args->values[i], collector);
    }
}

static inline void gcMarkClass(LoxClass *klass, GarbageCollector *collector)
{
    klass->marked = collector->visitedMark;
    for(int32_t index = 0; index < klass->methodsCount; ++index)
    {
        gcMarkEnvironment(klass->methods[index].function->closure, collector);
    }
    if ((klass->superClass) && (klass->superClass->marked != collector->visitedMark))
    {
        gcMarkClass(klass->superClass, collector);
    }
}

static inline void gcMarkFunction(LoxFunction *function, GarbageCollector *collector)
{
    function->marked = collector->visitedMark;
    gcMarkEnvironment(function->closure, collector);
}

static inline void gcMarkInstance(LoxInstance *instance, GarbageCollector *collector)
{
    instance->marked = collector->visitedMark;
    for (int32_t index = 0; index < instance->fieldsCount; ++index)
    {
        gcMarkObject(instance->fields[index].value, collector);
    }
    gcMarkClass(instance->klass, collector);
}

static void gcMarkObject(Object *object, GarbageCollector *collector)
{
    if (object == NULL || object->marked == collector->visitedMark)
    {
        return;
    }
    object->marked = collector->visitedMark;
    switch (object->type)
    {
        case OT_ARGUMENTS: {
            gcMarkArguments(obj_unwrapArguments(object), collector);
        } break;
        case OT_CLASS: {
            gcMarkClass(obj_unwrapClass(object), collector);
        } break;
        case OT_FUNCTION: {
            gcMarkFunction(object->function, collector);
        } break;
        case OT_INSTANCE: {
            gcMarkInstance(obj_unwrapInstance(object), collector);
        } break;
        case OT_BOOLEAN:
        case OT_CALLABLE:
        case OT_NIL:
        case OT_NUMBER:
        case OT_STRING:
            break;
        case OT_UNUSED:
            INVALID_CASE;
    }
}

static void gcMarkEnvironment(Environment *environment, GarbageCollector *collector)
{
    if(environment == NULL || environment->marked == collector->visitedMark)
    {
        return;
    }
    environment->marked = collector->visitedMark;

    for (int32_t index = 0; index < environment->slotsUsed; ++index)
    {
        gcMarkObject(environment->values[index], collector);
    }
    gcMarkEnvironment(environment->enclosing, collector);
}

// Releases an object. The Object structure is moved to the unused objects list so
// that it can be reused. If its value cannot be shared it's freed if appropriate.
// If more objects can retain the same value (class, function, or instance) the object
// is sent to the laundry list the first time that value is met; otherwise it is moved
// to the unused object list as usual. The values in the laundry list will be freed in
// a subsequent step; and they are marked with recycledMark so that we know we already
// visited them.
static void gcRelease(Object *object, GarbageCollector *collector)
{
    assert(object->marked != collector->visitedMark && object->marked != collector->recycledMark);
    switch (object->type)
    {
        case OT_ARGUMENTS:
        {
            argumentsFree(object->arguments);
        } break;
        case OT_CALLABLE:
        {
            callableFree(object->callable);
        } break;
        case OT_CLASS:
        {
            LoxClass *klass = obj_unwrapClass(object);
            if((klass->marked != collector->visitedMark) && (klass->marked != collector->recycledMark))
            {
                klass->marked = collector->recycledMark;
                object->next = collector->laundryList;
                collector->laundryList = object;
                return;
            }
        } break;
        case OT_FUNCTION:
        {
            LoxFunction *function = obj_unwrapFunction(object);
            if ((function->marked != collector->visitedMark) &&
                (function->marked != collector->recycledMark))
            {
                function->marked = collector->recycledMark;
                object->next = collector->laundryList;
                collector->laundryList = object;
                return;
            }
        } break;
        case OT_INSTANCE:
        {
            LoxInstance *instance = obj_unwrapInstance(object);
            if((instance->marked != collector->visitedMark) && (instance->marked != collector->recycledMark))
            {
                instance->marked = collector->recycledMark;
                object->next = collector->laundryList;
                collector->laundryList = object;
                return;
            }
        } break;
        case OT_STRING:
        {
            str_free(object->string);
        } break;
        case OT_BOOLEAN:
        case OT_NIL:
        case OT_NUMBER:
        {
            // do nothing
        } break;
        case OT_UNUSED:
            INVALID_PATH;
    }
    object->type = OT_UNUSED;
#ifdef DEBUG_SCRAMBLE_MEMORY
    object->klass = DEBUG_SCRAMBLE_VALUE;
#endif
    object->next = collector->firstUnused;
    collector->firstUnused = object;
#ifdef GC_KEEPS_STATS
    ++collector->unusedObjectsCount;
    ++collector->debug_recycledObjCount;
#endif
}

// Sweep step of the mark & sweep garbage collector
static void gcSweep(GarbageCollector *collector)
{
    // NOTE: Visits all active objects and releases the ones
    // that were not marked.
    Object **object = &collector->firstObject;
    while (*object)
    {
        assert((*object)->marked != collector->recycledMark);
        if ((*object)->marked == collector->visitedMark)
        {
            object = &(*object)->next;
        }
        else
        {
            Object *released = *object;
            *object = released->next;
            gcRelease(released, collector);
            --collector->activeObjectsCount;
        }
    }

    // NOTE: Free the objects in the laundry list
    while(collector->laundryList != NULL)
    {
        Object *object = collector->laundryList;
        switch (object->type)
        {
            case OT_CLASS: {
                assert(object->klass->marked == collector->recycledMark);
                classFree(object->klass);
            } break;
            case OT_CALLABLE: {
                callableFree(object->callable);
            } break;
            case OT_FUNCTION: {
                assert(object->function->marked == collector->recycledMark);
                function_free(object->function);
            } break;
            case OT_INSTANCE: {
                instanceFree(object->instance);
            } break;
                INVALID_DEFAULT_CASE;
        }
        collector->laundryList = object->next;
        
        object->type = OT_UNUSED;
#ifdef DEBUG_SCRAMBLE_MEMORY
        object->klass = DEBUG_SCRAMBLE_VALUE;
#endif
        object->next = collector->firstUnused;
        collector->firstUnused = object;
#ifdef GC_KEEPS_STATS
        ++collector->unusedObjectsCount;
        ++collector->debug_recycledObjCount;
#endif
    }
    
    // NOTE: Recycle the environments that were not marked.
    Environment **env = &collector->firstEnvironment;
    while (*env)
    {
        assert((*env)->marked != collector->recycledMark);
        if ((*env)->marked == collector->visitedMark)
        {
            env = &(*env)->next;
        }
        else
        {
            Environment *released = *env;
            *env = released->next;
            released->next = collector->firstUnusedEnvironment;
            collector->firstUnusedEnvironment = released;
            --collector->activeEnvironmentsCount;
#ifdef GC_KEEPS_STATS
            ++collector->unusedEnvironmentsCount;
            ++collector->debug_recycledEnvCount;
#endif
        }
    }
}

// Run the mark & sweep garbage collector
void gcCollect(GarbageCollector *collector)
{
#ifdef GC_KEEPS_STATS
    assert(collector->activeObjectsCount + collector->unusedObjectsCount == collector->objectsCount);
#endif
    // NOTE: First we mark all objects that have been locked
    for(int32_t index = 0; index < collector->lockedCount; ++index)
    {
        gcMarkObject(collector->locked[index], collector);
    }
    
    // NOTE: Next we mark all objects stored in active environments
    for(Environment *env = collector->firstEnvironment;
        env != NULL;
        env = env->next)
    {
        if(env->isActive)
        {
            gcMarkEnvironment(env, collector);
        }
    }

    // NOTE: Finally we perform the sweep step
    gcSweep(collector);

    // NOTE: Update the thresholds for the next garbage collection
    collector->maxObjects = max(2*collector->activeObjectsCount, collector->objectsCount);
    collector->maxEnvironments = max(2*collector->activeEnvironmentsCount, collector->objectsCount);

    // NOTE: define new values for the marks, so we do not reset the marks of all remaining objects
    collector->visitedMark += 2;
    collector->recycledMark += 2;
    if(collector->visitedMark == (1 << 30))
    {
        collector->visitedMark = 0;
        collector->recycledMark = 1;
    }
}

// Releases all objects.
static void gcClean(GarbageCollector *collector)
{
#ifdef GC_KEEPS_STATS
    assert(collector->activeObjectsCount + collector->unusedObjectsCount == collector->objectsCount);
#endif
    assert(collector->lockedCount == 0);
    // NOTE: At this point everything is unmarked, so the sweep step releases everything.
    gcSweep(collector);
   
#ifdef GC_KEEPS_STATS
    printf("MAX ACTIVE obj: %d  env: %d - RECYCLED obj: %d env: %d\n", collector->objectsCount, collector->environmentsCount, collector->debug_recycledObjCount - collector->unusedObjectsCount, collector->debug_recycledEnvCount - collector->unusedEnvironmentsCount);
#endif
}

// Releases everything and frees the garbage collector.
void gcFree(GarbageCollector *collector)
{
    gcClean(collector);
    
    assert(collector->firstEnvironment == NULL);
    assert(collector->firstObject == NULL);
    assert(collector->activeEnvironmentsCount == 0);
    
    Object *object = collector->firstUnused;
    while(object)
    {
        Object *next = object->next;
        objFree(object);
        --collector->objectsCount;
#ifdef GC_KEEPS_STATS
        --collector->unusedObjectsCount;
#endif
        object = next;
    }
    assert(collector->objectsCount == 0);
    
    MemoryPage *page = collector->memoryPages;
    while(page)
    {
        MemoryPage *next = page->next;
        lox_free(page);
        page = next;
    }
    
    Environment *environment = collector->firstUnusedEnvironment;
    while(environment)
    {
        Environment *next = environment->next;
        env_free(environment);
        --collector->environmentsCount;
        environment = next;
#ifdef GC_KEEPS_STATS
        --collector->unusedEnvironmentsCount;
#endif
    }
    assert(collector->environmentsCount == 0);
    
    lox_free(collector);
}
