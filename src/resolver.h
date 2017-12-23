//
//  resolver.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 03/11/2017.
//

#ifndef resolver_h
#define resolver_h

#include "interpreter.h"
#include "stmt.h"

void resolve(Stmt* statements, Interpreter *interpreter);

#endif /* resolver_h */
