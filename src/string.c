//
//  string.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#include "string.h"
#include "error.h"
#include <string.h>
#include <float.h>

extern inline void str_free(char *str);
extern inline char *str_clear(char *str);
extern inline str_size str_calculateLength(const char *str);
extern inline str_size str_length(const char *str);

extern inline SubstringIndex substring(str_size start, str_size count);
extern inline SubstringIndex substringStartEnd(str_size start, str_size onePastLast);
extern inline SubstringIndex substring_trimmed(SubstringIndex index);

struct MemoryPool *str_smallPool;
struct MemoryPool *str_mediumPool;

void str_initPools()
{
#ifdef STR_USE_MEMORY_POOLS
#ifdef DEBUG_VERBOSE
    printf("Initializing string pools.\n");
#endif
    str_smallPool = poolInit(STR_SMALL_SIZE);
    str_mediumPool = poolInit(STR_MEDIUM_SIZE);
#endif
}

void str_freePools()
{
#ifdef STR_USE_MEMORY_POOLS
#ifdef DEBUG_VERBOSE
    printf("Freeing string pools.\n");
#endif
    poolFree(str_smallPool);
    poolFree(str_mediumPool);
#endif
}

char * str_alloc(str_size capacity)
{
    StringHeader *header;
#ifdef STR_USE_MEMORY_POOLS
    if (capacity <= STR_SMALL_CAPACITY)
    {
        header = poolGetObject(str_smallPool);
        header->capacity = STR_SMALL_CAPACITY;
    }
    else if (capacity <= STR_MEDIUM_CAPACITY)
    {
        header = poolGetObject(str_mediumPool);
        header->capacity = STR_MEDIUM_CAPACITY;
    }
    else
#endif
    {
        assert(capacity < STR_SIZE_MAX - STR_HEADER_SIZE - 1);
        header = (StringHeader *)lox_allocn(char, STR_HEADER_SIZE + capacity + 1);
        if(header == NULL)
        {
            fatal_outOfMemory();
        }
        header->capacity = capacity;
    }

    header->length = 0;
    char *str = STR_FROM_HEADER(header);
    str[0] = '\0';
#ifdef STR_STORE_HASH
    header->hash = STR_HASH_UNAVAILABLE;
#endif
    
    return str;
}

char *str_realloc(char *str, str_size newCapacity)
{
    char *newString;
    StringHeader *header = STR_HEADER(str);
#ifdef STR_USE_MEMORY_POOLS
    if (header->capacity == STR_SMALL_CAPACITY)
    {
        newString = str_alloc(newCapacity);
        memcpy(newString, STR_FROM_HEADER(header), header->length + 1);
        STR_LENGTH(newString) = header->length;
        poolReleaseObject(header, str_smallPool);
    }
    else if (header->capacity == STR_MEDIUM_CAPACITY)
    {
        newString = str_alloc(newCapacity);
        memcpy(newString, STR_FROM_HEADER(header), header->length + 1);
        STR_LENGTH(newString) = header->length;
        poolReleaseObject(header, str_mediumPool);
    }
    else
#endif
    {
        StringHeader *newHeader = (StringHeader *)lox_realloc(header, STR_HEADER_SIZE + newCapacity + 1);
        
        if(newHeader == NULL)
        {
            fatal_outOfMemory();
        }
        newHeader->capacity = newCapacity;
        newString = STR_FROM_HEADER(newHeader);
    }
    
    return newString;
}

char * str_grow_(char *str, str_size newCapacity)
{
    if (STR_CAPACITY(str) < newCapacity)
    {
        str = str_realloc(str, newCapacity);
    }
    return str;
}

void str_setLength(char *str)
{
    STR_LENGTH(str) = str_calculateLength(str);
}

char * str_fromLiteral(const char *source)
{
    str_size len = str_calculateLength(source);
    char *result = str_alloc(len);
    memcpy(result, source, len + 1);
    assert(result[len] == '\0');
    STR_LENGTH(result) = len;
    return result;
}

char * str_dup(const char *source)
{
    str_size len = str_length(source);
    char *result = str_alloc(len);
    memcpy(result, source, len + 1);
    assert(result[len] == '\0');
    STR_LENGTH(result) = len;
    return result;
}

char * str_concat(const char *str1, const char *str2)
{
    str_size len1 = str_length(str1);
    str_size len2 = str_length(str2);
    char *result = str_alloc(len1 + len2);
    memcpy(result, str1, len1);
    memcpy(result + len1, str2, len2 + 1);
    str_size totalLen = len1 + len2;
    assert(result[totalLen] == '\0');
    STR_LENGTH(result) = totalLen;
    return result;
}

char * str_append_(char *str, const char *suffix)
{
    str_size len = str_length(str);
    str_size suffixLen = str_length(suffix);
    str_size totalLen = len + suffixLen;
    
    str_grow(str, totalLen);
    memcpy(str+len, suffix, suffixLen + 1);
    STR_LENGTH(str) = totalLen;
#ifdef STR_STORE_HASH
    STR_HASH(str) = STR_HASH_UNAVAILABLE;
#endif

    return str;
}

char * str_appendLiteral_(char *str, const char *suffix)
{
    str_size len = str_length(str);
    str_size suffixLen = str_calculateLength(suffix);
    str_size totalLen = len + suffixLen;
    
    str_grow(str, totalLen);
    memcpy(str+len, suffix, suffixLen + 1);
    STR_LENGTH(str) = totalLen;
#ifdef STR_STORE_HASH
    STR_HASH(str) = STR_HASH_UNAVAILABLE;
#endif

    return str;
}

char * str_fromDouble(double value)
{
    char *result = str_alloc(64);
    sprintf(result, "%.*g", DBL_DIG, value);
    STR_LENGTH(result) = str_calculateLength(result);
    return result;
}

char * str_fromInt64(int64_t value)
{
    char *result = str_alloc(64);
    sprintf(result, "%lld", value);
    STR_LENGTH(result) = str_calculateLength(result);
    return result;
}

// Returns a new string containing the substring of `str` defined
// by `index`. The returned string must be freed with str_free().
char * str_substring(const char * const str, SubstringIndex index)
{
    char *substring = str_alloc(index.count);
    
    char *dest = substring;
    const char *source = str + index.start;
    str_size len = index.count;
    STR_LENGTH(substring) = len;
    while(len > 0)
    {
        *dest++ = *source++;
        len--;
    }
    *dest = '\0';
    
    assert(str_length(substring) == str_calculateLength(substring));
    
    return substring;
}

bool str_isEqual(const char *s1, const char *s2)
{
    while(*s1 == *s2)
    {
        if(*s1 == '\0')
        {
            return true;
        }
        s1++;
        s2++;
    }
    return false;
}

// TODO: Test and tune the hash function
// NOTE: This is the djb2 hash function ( http://www.cse.yorku.ca/~oz/hash.html )
unsigned long
str_hashLiteral(const char *str)
{
    const unsigned char *bytes = (const unsigned char *)str;
    unsigned long hash = 5381;
    int c;

    while ((c = *bytes++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

unsigned long
str_hash(const char *str)
{
#ifdef STR_STORE_HASH
    if (STR_HASH(str) != STR_HASH_UNAVAILABLE)
    {
        return STR_HASH(str);
    }
#endif
    const unsigned char *bytes = (const unsigned char *)str;
    unsigned long hash = 5381;
    int c;
    
    while ((c = *bytes++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    
#ifdef STR_STORE_HASH
    STR_HASH(str) = (uint32_t)hash;
#endif
    return (uint32_t)hash;
}
