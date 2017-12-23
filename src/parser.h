//
//  parser.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

/*
 Note: This is a recursive descent parser that currently
       implements the following grammar:
 
 program     → declaration* eof ;
 declaration → classDecl
             | funDecl
             | varDecl
             | statement ;
 classDecl   → "class" IDENTIFIER "{" function* "}" ;
 funDecl     → "fun" function ;
 function    → IDENTIFIER "(" parameters? ")" block ;
 parameters  → IDENTIFIER ( "," IDENTIFIER )* ;
 varDecl     → "var" IDENTIFIER ( "=" expression )? ";" ;
 statement   → exprStmt
             | forStmt
             | ifStmt
             | printStmt
             | returnStmt
             | whileStmt
             | block ;
 exprStmt    → expression ";" ;
 forStmt     → "for" "(" ( varDecl | exprStmt | ";" ) expression? ";" expression? ")" statement ;
 ifStmt      → "if" "(" expression ")" statement ( "else" statement )? ;
 printStmt   → "print" expression ";" ;
 returnStmt  → "return" expression? ";" ;
 whileStmt   → "while" "(" expression ")" statement ;
 block       → "{" declaration* "}" ;
 
 expression     → assignment ;
 assignment     → ( call "." )? IDENTIFIER "=" assignment
                | logic_or;
 logic_or       → logic_and ( "or" logic_and )* ;
 logic_and      → equality ( "and" equality )* ;
 equality       → comparison ( ( "!=" | "==" ) comparison )* ;
 comparison     → addition ( ( ">" | ">=" | "<" | "<=" ) addition )* ;
 addition       → multiplication ( ( "-" | "+" ) multiplication )* ;
 multiplication → unary ( ( "/" | "*" ) unary )* ;
 unary          → ( "!" | "-" ) unary | call ;
 call           → primary ( "(" arguments? ")" | "." IDENTIFIER )* ;
 arguments      → expression ( "," expression )* ;
 primary        → "true" | "false" | "null" | "this"
                | NUMBER | STRING | IDENTIFIER | "(" expression ")"
                | "super" "." IDENTIFIER ;
 */

#ifndef parser_h
#define parser_h

#include "token.h"
#include "stmt.h"

Stmt * parse(Token *tokens, const char *source);

#endif /* parser_h */
