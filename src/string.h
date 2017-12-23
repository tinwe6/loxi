//
//  string.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#ifndef string_h
#define string_h

#include "common.h"

#ifdef STR_STORE_HASH
// NOTE: When the hash field has this value, the hash is recalculated
#define STR_HASH_UNAVAILABLE 0
#endif

typedef uint32_t str_size;
#define STR_SIZE_MAX ((uint32_t)-1)

typedef struct SubstringIndex
{
    str_size start; // start index of the substring
    str_size count; // number of characters in the substring
} SubstringIndex;

void str_initPools(void);
void str_freePools(void);

char * str_alloc(str_size capacity);
void str_setLength(char *str);
char * str_fromLiteral(const char *source);
char * str_dup(const char *source);
char * str_concat(const char *str1, const char *str2);
char * str_fromDouble(double value);
char * str_fromInt64(int64_t value);
bool str_isEqual(const char *s1, const char *s2);

#define str_grow(str, newCapacity) do { str = str_grow_(str, newCapacity); } while(0);
#define str_append(str, suffix) do { str = str_append_(str, suffix); } while(0);
#define str_appendLiteral(str, suffix) do { str = str_appendLiteral_(str, suffix); } while(0);

char * str_append_(char* str, const char* suffix);
char * str_appendLiteral_(char* str, const char* suffix);

char* str_substring(const char* const str, SubstringIndex index);

unsigned long str_hashLiteral(const char *str);
unsigned long str_hash(const char *str);


inline SubstringIndex substring(str_size start, str_size count)
{
    SubstringIndex index = {start, count};
    return index;
}

inline SubstringIndex substringStartEnd(str_size start, str_size onePastLast)
{
    assert(onePastLast >= start);
    str_size count = onePastLast - start;
    SubstringIndex index = substring(start, count);
    return index;
}

inline SubstringIndex substring_trimmed(SubstringIndex index)
{
    SubstringIndex trimmed = (index.count >= 2) ? (SubstringIndex){index.start + 1, index.count - 2}
    : (SubstringIndex){index.start, 0};
    return trimmed;
}

typedef struct
{
    str_size capacity;
    str_size length;
#ifdef STR_STORE_HASH
    uint32_t hash;
#endif
} StringHeader;

#define STR_HEADER_SIZE sizeof(StringHeader)

#define STR_HEADER(string) ((StringHeader *)(string - STR_HEADER_SIZE))
#define STR_FROM_HEADER(header) ((char *)header + STR_HEADER_SIZE)
#define STR_HEADER_SIZE sizeof(StringHeader)
#define STR_CAPACITY(string) (STR_HEADER(string))->capacity
#define STR_LENGTH(string) (STR_HEADER(string))->length
#ifdef STR_STORE_HASH
#define STR_HASH(string) (STR_HEADER(string))->hash
#endif

#define STR_SMALL_CAPACITY (STR_SMALL_SIZE - 1 - STR_HEADER_SIZE)
#define STR_MEDIUM_CAPACITY (STR_MEDIUM_SIZE - 1 - STR_HEADER_SIZE)


#include "memory_pool.h"

extern struct MemoryPool *str_smallPool;
extern struct MemoryPool *str_mediumPool;

inline void str_free(char *str)
{
    StringHeader *header = STR_HEADER(str);
#ifdef STR_USE_MEMORY_POOLS
    if (header->capacity == STR_SMALL_CAPACITY)
    {
        poolReleaseObject(header, str_smallPool);
    }
    else if (header->capacity == STR_MEDIUM_CAPACITY)
    {
        poolReleaseObject(header, str_mediumPool);
    }
    else
#endif
    {
        lox_free(header);
    }
}

inline char *str_clear(char *str)
{
    str[0] = '\0';
    STR_LENGTH(str) = 0;
#ifdef STR_STORE_HASH
    STR_HASH(str) = STR_HASH_UNAVAILABLE;
#endif
    return str;
}

inline str_size str_calculateLength(const char *str)
{
    str_size len = 0;
    while(str[len] != '\0')
    {
        len++;
    }
    return len;
}

inline str_size str_length(const char *str)
{
    str_size len = STR_LENGTH(str);
    return len;
}

#endif /* string_h */

