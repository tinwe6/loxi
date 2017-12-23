//
//  memory.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 01/11/2017.
//

#ifndef memory_h
#define memory_h

#include "common.h"

#include <stdio.h>

#ifdef MEMORY_DEBUG

void lox_alloc_init(void);
void printLeakedPointers(void);
void alloc_printDB(void);
void alloc_printActiveDB(void);
void alloc_pointerInfo(void *ptr);

void * lox_alloc_(size_t capacity, int32_t count, const char *file, int line, const char *type);
void * lox_realloc_(void *ptr, int32_t count, const char *file, int line);
void lox_free_(void *ptr, const char *file, int line);

#define ALLOC_STR(x)  #x
#define lox_alloc(type) lox_allocn(type, 1)
#define lox_allocn(type, count) (type *)lox_alloc_(sizeof(type), count, __FILE__, __LINE__, ALLOC_STR(type))
#define lox_realloc(ptr, count) lox_realloc_(ptr, count, __FILE__, __LINE__)
#define lox_free(ptr) lox_free_(ptr, __FILE__, __LINE__)

#else

#include <stdlib.h>
#define lox_alloc(type) lox_allocn(type, 1)
#define lox_allocn(type, count) (type *)malloc(count*sizeof(type))
// NOTE: -IMPORTANT- This realloc does not keep track of the size
//       of the type, and thus works for chars only at the moment
#define lox_realloc(ptr, count) realloc(ptr, count)
#define lox_free(ptr) free(ptr)

#define lox_alloc_init(void)
#define alloc_printDB(void)
#define alloc_printActiveDB(void)

#endif

#endif /* memory_h */
