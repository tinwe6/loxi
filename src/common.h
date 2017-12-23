//
//  common.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#ifndef common_h
#define common_h

/* Language */

// In standar Lox, if a variable was defined but not assigned a
// value, we return nil.When LOX_ACCESSING_UNINIT_VAR_ERROR is
// defined, an error is thrown instead of returning nil.
#undef LOX_ACCESSING_UNINIT_VAR_ERROR

/* Limits */

// Maximum input line length in the repl
#define REPL_MAX_INPUT_LENGTH 1024

// Maximum number of variables that can be stored in a local environment
#define LOX_MAX_LOCAL_VARIABLES 255

// Maximum number of methods that can be defined in a class
#define LOX_METHODS_MAX_COUNT 32

// Maximum number of fields that can be stored in an instance
#define LOX_INSTANCE_MAX_FIELDS 256

// Maximum number of environments that can be simultaneously
#define LOX_MAX_ENVIRONMENTS 31*1024

// Maximum number of function arguments
#define LOX_MAX_ARG_COUNT 8

// Store global variables in a hash table
#define ENV_GLOBALS_USE_HASH 1

#ifdef ENV_GLOBALS_USE_HASH
// NOTE: Size of the globals hash table
#define ENV_GLOBAL_HASH_SIZE 512
#define ENV_GLOBAL_HASH_MASK (ENV_GLOBAL_HASH_SIZE - 1)
#endif

/* Time */

// If defined, clock() uses the mach absolute time instead of using
// the clock function of the standard C library.
//#define USE_MACH_TIME 1

//#if (defined (__APPLE__) && defined (__MACH__))

/* Strings */

// If defined, two pools of strings are created, one for small size
// strings and one for medium size strings. If the needed capacity
// exceeds the latter, usual allocate/free takes place.
#define STR_USE_MEMORY_POOLS 1

// Maximum capacities of small and medium size strings.
#ifdef STR_USE_MEMORY_POOLS
#define STR_SMALL_SIZE 32
#define STR_MEDIUM_SIZE 256
#endif

// If defined, the string stores its hash value when calculated.
#define STR_STORE_HASH 1

/* Garbage collector */

#define GC_INITIAL_ENVIRONMENTS_THRESHOLD 32
#define GC_LOCKS_STACK_SIZE 4096

// If defined, triggers a garbage collection at every place where
// a garbage collection could be triggered.
//#define GC_DEBUG 1

/* Memory */

// If defined, the interpreter frees all allocated memory before exiting.
// This option is turned on automatically if MEMORY_DEBUG is defined.
//#define MEMORY_FREE_ON_EXIT 1

// If defined, tracks all memory allocations and checks that there are no leaks.
//#define MEMORY_DEBUG 1

// If defined, (some of the) unused memory is set to a special value.
//#define DEBUG_SCRAMBLE_MEMORY 1

// If MEMORY_VERBOSE is defined, all allocations/frees are logged on the standard output
//#define MEMORY_VERBOSE 1

// If defined, some extra debugging info is displayed
//#define DEBUG_VERBOSE 1

/* Resolver */

// If defined, the resolved variable depth and index corresponding to expressions
// are stored in a hash map instead of a simple array.
#define USE_LOCALS_HASH_MAP 1

// Size of the resolver hash table (must be >= LOX_MAX_LOCAL_VARIABLES)
#define RESOLVER_HASH_TABLE_SIZE 255

// If defined, the debuggers prints debugging information
//#define RESOLVER_VERBOSE 1

/* Exit codes */

#define LOX_EXIT_CODE_OK                 0
#define LOX_EXIT_CODE_HAD_ERROR         65
#define LOX_EXIT_CODE_HAD_RUNTIME_ERROR 70
#define LOX_EXIT_CODE_FATAL_ERROR       -1

/**********************************************/

#ifdef MEMORY_VERBOSE
#define MEMORY_DEBUG 1
#endif

// NOTE: to check for memory leaks, we need to the interpreter to do a full clean up before exiting.
#if defined(MEMORY_DEBUG) && !defined(MEMORY_FREE_ON_EXIT)
#define MEMORY_FREE_ON_EXIT 1
#endif

// NOTE: to check for memory leaks of strings with MEMORY_DEBUG, we need to disable the memory pools first.
#if defined(MEMORY_DEBUG) && defined(STR_USE_MEMORY_POOLS)
#undef STR_USE_MEMORY_POOLS
#endif

// NOTE: disable asserts in release build
#ifdef RELEASE
#define NDEBUG 1
#endif

#include <assert.h>
#include <stdbool.h> // NOTE: ISO C99
#include <stddef.h> // Defines NULL
#include <stdlib.h>

#include "memory.h"


#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifdef DEBUG_SCRAMBLE_MEMORY
#define DEBUG_SCRAMBLE_VALUE (void *)0xbababebedadadede
#endif

#define max(a,b) (((a)<(b))?(b):(a))

#define global

#ifdef DEBUG
#define FATAL_ERROR assert(false);
#else
#define FATAL_ERROR exit(-1)
#endif

#define INVALID_PATH FATAL_ERROR
#define INVALID_CASE { FATAL_ERROR; } break
#define INVALID_DEFAULT_CASE default: { FATAL_ERROR; } break

#define ARRAY_COUNT(array) (sizeof array / sizeof array[0])

#define XSTR(a) STR(a)
#define STR(a) #a

#endif /* common_h */
