//
//  garbage_collector.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 20/11/2017.
//

#ifndef garbage_collector_h
#define garbage_collector_h

#include "environment.h"
#include "memory.h"

// NOTE: objects with mark set to GC_CLEAR are considered unmarked.
#define GC_CLEAR -1

typedef struct MemoryPage_tag
{
    struct MemoryPage_tag *next;
} MemoryPage;

typedef struct GarbageCollector_tag
{
    Object *firstObject;
    Object *laundryList;
    Object *firstUnused;
    int32_t objectsCount;
    int32_t maxObjects;
    
    // NOTE: current mark values given to visited and recicled objects
    int32_t visitedMark;
    int32_t recycledMark;
    
    Environment *firstEnvironment;
    Environment *firstUnusedEnvironment;
    int32_t environmentsCount;
    int32_t maxEnvironments;

    Object *locked[GC_LOCKS_STACK_SIZE];
    int32_t lockedCount;
    
    MemoryPage *memoryPages;

    int32_t activeEnvironmentsCount;
    int32_t activeObjectsCount;

    // NOTE: This is just to take some stats
#ifdef GC_KEEPS_STATS
    int32_t unusedObjectsCount;
    int32_t unusedEnvironmentsCount;

    int32_t debug_recycledObjCount;
    int32_t debug_recycledEnvCount;
#endif
} GarbageCollector;

GarbageCollector * gcInit(void);
void gcFree(GarbageCollector *collector);
Object * gcGetObject(GarbageCollector *collector);
void gcSetGlobalEnvironment(Environment *globals, GarbageCollector *collector);
Environment * gcGetEnvironment(GarbageCollector *collector);
void gcCollect(GarbageCollector *collector);

inline bool gcLock(Object *object, GarbageCollector *collector)
{
    if (collector->lockedCount == ARRAY_COUNT(collector->locked))
    {
        // TODO: At the moment, a stack overflow exception is thrown here.
        // This is fine since GC_LOCKS_STACK_SIZE is chose to be large.
        // Should we keep it smaller and dynamically grow the stack instead?
        return false;
    }
    collector->locked[collector->lockedCount++] = object;
    return true;
}

inline void gcPopLock(GarbageCollector *collector)
{
    assert(collector->lockedCount > 0);
    --collector->lockedCount;
#ifdef DEBUG_SCRAMBLE_MEMORY
    collector->locked[collector->lockedCount] = DEBUG_SCRAMBLE_VALUE;
#endif
}

inline void gcPopLockn(int32_t n, GarbageCollector *collector)
{
    collector->lockedCount -= n;
#ifdef DEBUG_SCRAMBLE_MEMORY
    for (int32_t index = 0; index < n; ++index)
    {
        collector->locked[collector->lockedCount + index] = DEBUG_SCRAMBLE_VALUE;
    }
#endif
    assert(collector->lockedCount >= 0);
}

inline void gcClearLocks(GarbageCollector *collector)
{
    gcPopLockn(collector->lockedCount, collector);
}

#endif /* garbage_collector_h */
