//
//  scanner.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#ifndef scanner_h
#define scanner_h

#include "token.h"

Token * scanLine(const char *source, int32_t lineNumber);
Token * scan(const char *source);

#endif /* scanner_h */
