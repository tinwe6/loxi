//
//  return.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 02/11/2017.
//

#include "return.h"

extern inline Return * return_init(Object *value, GarbageCollector *collector);
extern inline Object * return_unwrap(Return *ret);
