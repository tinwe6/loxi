//
//  memory_pool.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 14/12/2017.
//

#include "memory_pool.h"
#include "common.h"
#include "error.h"

extern inline void poolReleaseObject(void *object, struct MemoryPool *pool);

static void poolAlloc(int32_t numPages, struct MemoryPool *pool)
{
    ChunkSize totalSize = PAGE_SIZE*numPages;
    PoolChunk *chunk = (PoolChunk *)lox_allocn(uint8_t, totalSize);
    if(chunk == NULL)
    {
        fatal_outOfMemory();
    }
    chunk->memory = (void *)(chunk + 1);
    chunk->memorySize = totalSize - sizeof(PoolChunk);
    chunk->used = 0;
    chunk->nextChunk = pool->firstChunk;
    pool->firstChunk = chunk;
}

struct MemoryPool * poolInit(ChunkSize objectSize)
{
    struct MemoryPool *pool = lox_alloc(struct MemoryPool);
    if(pool == NULL)
    {
        fatal_outOfMemory();
    }
    pool->firstChunk = NULL;
    pool->firstUnused = NULL;
    pool->chunksCount = 0;
    pool->objectSize = objectSize;
    poolAlloc(1, pool);
    return pool;
}

void poolFree(struct MemoryPool *pool)
{
    while(pool->firstChunk)
    {
        PoolChunk *chunk = pool->firstChunk;
        pool->firstChunk = chunk->nextChunk;
        lox_free(chunk);
    }
    lox_free(pool);
}

void * poolGetObject(struct MemoryPool *pool)
{
    PoolChunk *chunk = pool->firstChunk;
    
    if (pool->firstUnused)
    {
        PoolUnusedObject *object = pool->firstUnused;
        pool->firstUnused = object->next;
        return (void *)object;
    }
    
    if (chunk->used + pool->objectSize > chunk->memorySize)
    {
        poolAlloc(1, pool);
        chunk = pool->firstChunk;
    }
    void *object = (uint8_t *)chunk->memory + chunk->used;
    chunk->used += pool->objectSize;
    
    return object;
}

