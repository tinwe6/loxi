//
//  return.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 02/11/2017.
//

#ifndef return_h
#define return_h

#include "objects.h"

typedef struct
{
    Object value;
} Return;

#include "garbage_collector.h"
#include "memory.h"

inline Return * return_init(Object *value, GarbageCollector *collector)
{
    Return *ret = (Return *)value;
    if(ret == NULL)
    {
        ret = (Return *)obj_newNil(collector);
    }
    return ret;
}

inline Object * return_unwrap(Return *ret)
{
    Object *value = (Object *)ret;
    return value;
}

#endif /* return_h */
