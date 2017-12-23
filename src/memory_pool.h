//
//  memory_pool.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 14/12/2017.
//

#ifndef memory_pool_h
#define memory_pool_h

#include <stdint.h>

typedef uint32_t ChunkSize;

typedef struct PoolChunk_tag
{
    struct PoolChunk_tag *nextChunk;
    void *memory;
    ChunkSize used;
    ChunkSize memorySize;
} PoolChunk;

typedef struct PoolUnusedObject_tag
{
    struct PoolUnusedObject_tag *next;
} PoolUnusedObject;

struct MemoryPool
{
    PoolChunk *firstChunk;
    PoolUnusedObject *firstUnused;
    int32_t chunksCount;
    ChunkSize objectSize;
};

struct MemoryPool* poolInit(ChunkSize objectSize);
void poolFree(struct MemoryPool *pool);
void * poolGetObject(struct MemoryPool *pool);

inline void poolReleaseObject(void* object, struct MemoryPool *pool)
{
    ((PoolUnusedObject *)object)->next = pool->firstUnused;
    pool->firstUnused = object;
}

#endif /* memory_pool_h */
