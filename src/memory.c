//
//  memory.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 01/11/2017.
//

#include "memory.h"

unsigned long str_hashLiteral(const char *str);
unsigned long str_hash(const char *str);

#ifdef MEMORY_DEBUG

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef uint16_t memory_size;
#define MEMORY_MAX_SIZE ((memory_size)-1)

typedef struct
{
    uint32_t hash;
    memory_size count;
    memory_size capacity;
} AllocHeader;

typedef struct
{
    uint32_t hash;
} EndMarker;

#define ALLOC_OFFSET (sizeof(AllocHeader))
#define ALLOC_ENDMARKER_SIZE (sizeof(EndMarker))
#define ALLOC_TYPE_NAME_SIZE 32

#define ALLOC_POINTER_FROM_HEADER(header) (void *)((char *)header + ALLOC_OFFSET)
#define ALLOC_HEADER_FROM_POINTER(ptr) (AllocHeader *)((char *)ptr - ALLOC_OFFSET)
#define ALLOC_MARKER_FROM_HEADER(header, capacity) (EndMarker *)((char *)header + ALLOC_OFFSET + capacity);
#define ALLOC_MARKER_FROM_POINTER(ptr, capacity) (EndMarker *)((char *)ptr + capacity)

typedef struct
{
    uint32_t hash;
    int32_t count;
    int64_t size;
    uint32_t alloc_count;
    uint32_t realloc_count;
    uint32_t free_count;
    char type[ALLOC_TYPE_NAME_SIZE];
} AllocEntry;

// NOTE: ALLOC_DB_SIZE must be a power of two
#define ALLOC_DB_SIZE (1 << 9)
#define ALLOC_DB_MASK ((ALLOC_DB_SIZE) - 1)

static AllocEntry alloc_db[ALLOC_DB_SIZE];

static void alloc_initDB()
{
    for (int32_t i = 0; i < ALLOC_DB_SIZE; ++i)
    {
        alloc_db[i].hash = 0;
        alloc_db[i].count = 0;
        alloc_db[i].size = 0;
        alloc_db[i].alloc_count = 0;
        alloc_db[i].realloc_count = 0;
        alloc_db[i].free_count = 0;
        alloc_db[i].type[0] = '0';
    }
}

// NOTE: We need to align the marker on a 4 bytes boundary since it is an uint32_t
#define MARKER_ALIGNMENT 4

static void printPointerInfo(AllocHeader *header, const char *prefix, const char *filename, int line);

void lox_alloc_init()
{
    alloc_initDB();
}

static void printAllocEntry(const AllocEntry *entry)
{
    printf("%s (#%x) count %d (%lld bytes) - %d alloc %d free %d realloc.\n", entry->type, entry->hash, entry->count, entry->size, entry->alloc_count, entry->free_count, entry->realloc_count);
}

static const char * hashToType(uint32_t hash)
{
    uint32_t index = hash & ALLOC_DB_MASK;
    AllocEntry *entry = alloc_db + index;
    
    assert(hash == entry->hash);
    
    return entry->type;
}

/* allocatedPointers is a table where we store all pointers that are *
 * allocated, and allows us to check whether memory has been leaked. */

// NOTE: Maximum number of pointers that can be tracked
#define MAX_ALLOCATED_POINTERS 256*1024

static AllocHeader * allocatedPointers[MAX_ALLOCATED_POINTERS];
static int allocatedPointersCount = 0;

static void storePointer(AllocHeader *pointer)
{
    assert(allocatedPointersCount < MAX_ALLOCATED_POINTERS);
    allocatedPointers[allocatedPointersCount++] = pointer;
}

static void removePointer(AllocHeader *pointer)
{
    for(uint32_t i = 0; i < allocatedPointersCount; ++i)
    {
        if(allocatedPointers[i] == pointer)
        {
            allocatedPointers[i] = allocatedPointers[--allocatedPointersCount];
            return;
        }
    }
    INVALID_PATH;
}

void alloc_printDB()
{
    printf("\n--- Alloc DB -----------\n");
    for (int32_t i = 0; i < ALLOC_DB_SIZE; ++i)
    {
        if(alloc_db[i].hash != 0)
        {
            printAllocEntry(alloc_db + i);
        }
    }
    printf("--- Alloc DB end -------\n");

    printLeakedPointers();
}

static inline bool alloc_entryIsFree(AllocEntry *entry)
{
    bool isFree = (entry->count == 0) && (entry->size == 0);
    return isFree;
}

int64_t alloc_active()
{
    int64_t totalBytes = 0;
    for (int32_t i = 0; i < ALLOC_DB_SIZE; ++i)
    {
        AllocEntry *entry = alloc_db + i;
        if(entry->hash != 0)
        {
            if (!alloc_entryIsFree(entry))
            {
                totalBytes += entry->size;
            }
        }
    }
    return totalBytes;
}

void alloc_printActiveDB()
{
    int64_t totalBytes = alloc_active();
    if(totalBytes == 0)
    {
        return;
    }
    printf("\n--- Alloc Active -------\n");
    for (int32_t i = 0; i < ALLOC_DB_SIZE; ++i)
    {
        AllocEntry *entry = alloc_db + i;
        if(entry->hash != 0)
        {
            if (!alloc_entryIsFree(entry))
            {
                printAllocEntry(entry);
            }
        }
    }
    printf("--- Alloc Active end ---\n");
    printf("%lld bytes leaked.\n", totalBytes);
}

static void alloc_recordAlloc(uint32_t hash, memory_size count, memory_size capacity, const char *type)
{
    uint32_t index = hash & ALLOC_DB_MASK;
    AllocEntry *entry = alloc_db + index;
    
    if(entry->hash == 0)
    {
        // first entry
        entry->hash = hash;
        snprintf(entry->type, ALLOC_TYPE_NAME_SIZE, "%s", type);
    }
    else
    {
        // NOTE: we don't want clashes in our table
        assert(entry->hash == hash);
    }
    entry->count += (int32_t)count;
    entry->size += capacity;
    entry->alloc_count++;
}

static void alloc_recordRealloc(uint32_t hash, memory_size count, memory_size capacity, memory_size oldCount, memory_size oldCapacity)
{
    uint32_t index = hash & ALLOC_DB_MASK;
    AllocEntry *entry = alloc_db + index;

    assert(entry->hash != 0);
    assert(entry->hash == hash);

    entry->count += (int32_t)count - (int32_t)oldCount;
    entry->size += (int64_t)capacity - (int64_t)oldCapacity;
    entry->realloc_count++;
}

static void alloc_recordFree(uint32_t hash, memory_size count, memory_size capacity)
{
    uint32_t index = hash & ALLOC_DB_MASK;
    AllocEntry *entry = alloc_db + index;

    assert(entry->hash == hash);
    entry->count -= (int32_t)count;
    entry->size -= capacity;
    entry->free_count++;

    assert(alloc_db->count >= 0);
    assert(alloc_db->size >= 0);
}

#ifdef MEMORY_VERBOSE
static const char * filenameFromPath(const char *path)
{
    const char *name = path;
    while(*path != '\0')
    {
        if(*path == '/')
        {
            name = path+1;
        }
        path++;
    }
    return name;
}
#endif

// Returns a pointer to `count` contiguous blocks of memory,
// each one of size `capacity`.
// If the allocation was unsuccssful, returns NULL.
void * lox_alloc_(size_t capacity, int32_t count, const char *file, int line, const char *type)
{
    assert(count < MEMORY_MAX_SIZE);
    assert(capacity < MEMORY_MAX_SIZE / count);
    size_t totalCapacity = capacity*count;
    assert(totalCapacity < MEMORY_MAX_SIZE - ALLOC_OFFSET - ALLOC_ENDMARKER_SIZE - MARKER_ALIGNMENT );

    uint32_t hash = (uint32_t)str_hashLiteral(type);

    totalCapacity = (totalCapacity + MARKER_ALIGNMENT) & ~(MARKER_ALIGNMENT-1);
    size_t size = totalCapacity + ALLOC_OFFSET + ALLOC_ENDMARKER_SIZE;
    AllocHeader *header = (AllocHeader *)malloc(size);
    storePointer(header);

    header->capacity = (memory_size)totalCapacity;
    header->count = (memory_size)count;
    header->hash = hash;

    EndMarker *marker = ALLOC_MARKER_FROM_HEADER(header, totalCapacity);
    marker->hash = hash;
    
    void *pointer = ALLOC_POINTER_FROM_HEADER(header);
    
    alloc_recordAlloc(hash, (memory_size)count, (memory_size)totalCapacity, type);
    
#ifdef MEMORY_VERBOSE
    const char *name = filenameFromPath(file);
    memset(pointer, 0, totalCapacity);
    printPointerInfo(header, "ALLOC ", name, line);
#endif
    return pointer;
}

void lox_free_(void *ptr, const char *file, int line)
{
    AllocHeader *header = ALLOC_HEADER_FROM_POINTER(ptr);
#ifdef MEMORY_VERBOSE
    const char *name = filenameFromPath(file);
    printPointerInfo(header, "FREE  ", name, line);
#endif

    memory_size capacity = header->capacity;
    EndMarker *marker = ALLOC_MARKER_FROM_POINTER(ptr, capacity);
    assert(header->hash == marker->hash);

    alloc_recordFree(header->hash, header->count, capacity);

    // NOTE: Invalidate the end marker
    marker->hash = 0xabecedaf;
//    memset(ptr, 0, capacity);
    
    removePointer(header);
    free(header);
}

void * lox_realloc_(void *ptr, int32_t count, const char *file, int line)
{
    assert(count < MEMORY_MAX_SIZE);
    AllocHeader *header = ALLOC_HEADER_FROM_POINTER(ptr);
    uint32_t hash = header->hash;
    memory_size oldCapacity = header->capacity;
    memory_size oldCount = header->count;

    EndMarker *marker = ALLOC_MARKER_FROM_POINTER(ptr, oldCapacity);
    assert(header->hash == marker->hash);
    
    // NOTE: Invalidate the end marker
    marker->hash = 0xabecedaf;
//    memset(ptr, 0, oldCapacity);
    
    memory_size elementSize = oldCapacity / oldCount;
    size_t capacity = elementSize*count;
    capacity = (capacity + MARKER_ALIGNMENT) & ~(MARKER_ALIGNMENT-1);

    size_t size = capacity + ALLOC_OFFSET + ALLOC_ENDMARKER_SIZE;
    assert(size < MEMORY_MAX_SIZE);

    removePointer(header);
    header = (AllocHeader *)realloc(header, size);
    storePointer(header);
    
    header->capacity = (memory_size)capacity;
    header->count = (memory_size)count;
    header->hash = hash;

    marker = ALLOC_MARKER_FROM_HEADER(header, capacity);
    marker->hash = hash;
    
    ptr = ALLOC_POINTER_FROM_HEADER(header);
    
    alloc_recordRealloc(hash, (memory_size)count, (memory_size)capacity, oldCount, oldCapacity);
    
#ifdef MEMORY_VERBOSE
    const char *name = filenameFromPath(file);
    printf("$ Realloc pointer at %p -- %s l.%d\n", ptr, name, line);
#endif
    
    return ptr;
}

#include "objects.h"
#include "string.h"

static void printPointerInfo(AllocHeader *header, const char *prefix, const char *filename, int line)
{
    printf("%s0x%p ", prefix, (void *)header);
    uint32_t hash = header->hash;
    printf("%-17s count: %d cap: %d", hashToType(hash), header->count, header->capacity);
    void *ptr = ALLOC_POINTER_FROM_HEADER(header);
    if (!strcmp(hashToType(hash), "char"))
    {
        printf(" -> '%s'", STR_FROM_HEADER(ptr));
    }
    else if (!strcmp(hashToType(hash), "Object *"))
    {
        printf(" -> type: %s", obj_typeLiteral(((Object *)ptr)->type));
    }
    if (filename)
    {
        printf(" -- %s l.%d\n", filename, line);
    }
    else
    {
        printf("\n");
    }
}

void printLeakedPointers()
{
    if (allocatedPointersCount)
    {
        printf("\n=== Leaked pointers:    ===\n");
        for (int32_t index = 0; index < allocatedPointersCount; index++)
        {
            AllocHeader *header = allocatedPointers[index];
            printPointerInfo(header, "", NULL, 0);
        }
        printf("=== End leaked pointers ===\n");
    }
}

#endif

