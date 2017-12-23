//
//  stmt.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 18/10/2017.
//

#ifndef stmt_h
#define stmt_h

#include "expr.h"

/*
 "Block      : List<Stmt> statements",
 "Class      : Token name, Expr superclass," +
             " List<Stmt.Function> methods",
 "Expression : Expr expression",
 "Function   : Token name, List<Token> parameters, List<Stmt> body",
 "If         : Expr condition, Stmt thenBranch, Stmt elseBranch",
 "Print      : Expr expression",
 "Return     : Token keyword, Expr value",
 "Var        : Token name, Expr initializer",
 "While      : Expr condition, Stmt body"
 */

#define FOREACH_STMT_NODE(code)                             \
    code(Block) code(Class) code(Expression) code(Function) \
    code(If)    code(Print) code(Return)     code(Var)      \
    code(While)

typedef enum StmtType
{
#define DEFINE_ENUM_TYPE(type) STMT_##type,
    FOREACH_STMT_NODE(DEFINE_ENUM_TYPE)
#undef DEFINE_ENUM_TYPE
} StmtType;

typedef struct Stmt
{
    StmtType type;
    struct Stmt *next;
} Stmt;

#define AS_STMT(stmt) (Stmt *)stmt

// Block : List<Stmt> statements
typedef struct
{
    Stmt stmt;
    Stmt *statements;
} BlockStmt;

// Expression : Expr expression
typedef struct
{
    Stmt stmt;
    Expr *expression;
} ExpressionStmt;

// "Function   : Token name, List<Token> parameters, List<Stmt> body",
typedef struct
{
    Stmt stmt;
    Token *name;
    Token **parameters;
    int32_t arity;
    Stmt *body;
} FunctionStmt;

// Class      : Token name, Expr superclass, List<Stmt.Function> methods
typedef struct
{
    Stmt stmt;
    Token *name;
    Expr *superClass;
    FunctionStmt *methods;
} ClassStmt;

// If : Expr condition, Stmt thenBranch, Stmt elseBranch
typedef struct
{
    Stmt stmt;
    Expr *condition;
    Stmt *thenBranch;
    Stmt *elseBranch;
} IfStmt;

// Print : Expr expression
typedef struct
{
    Stmt stmt;
    Expr *expression;
} PrintStmt;

// Return : Token keyword, Expr value
typedef struct
{
    Stmt stmt;
    Token *keyword;
    Expr *value;
} ReturnStmt;

// Var : Token name, Expr initializer
typedef struct
{
    Stmt stmt;
    Token *name;
    Expr *initializer;
} VarStmt;

// While : Expr condition, Stmt body
typedef struct
{
    Stmt stmt;
    Expr *condition;
    Stmt *body;
} WhileStmt;

Stmt * initBlock(Stmt *statements);
Stmt * initClass(Token *name, Expr *superClass, FunctionStmt *methods);
Stmt * initExpression(Expr *expr);
Stmt * initFunction(Token *name, Token **parameters, int32_t parametersCount, Stmt *body);
Stmt * initIf(Expr *condition, Stmt *thenBranch, Stmt *elseBranch);
Stmt * initPrint(Expr *expr);
Stmt * initReturn(Token *keyword, Expr *value);
Stmt * initVar(Token *name, Expr *initializer);
Stmt * initWhile(Expr *condition, Stmt *body);
void freeStmt(Stmt *stmt);
Stmt * stmt_last(Stmt *statements);
Stmt * stmt_appendTo(Stmt *head, Stmt *tail);

/* Visitor */

typedef struct
{
#define DEFINE_VISIT(type) \
void *(*visit##type)(type##Stmt *, void *context);
    
    FOREACH_STMT_NODE(DEFINE_VISIT)
#undef DEFINE_VISIT
} StmtVisitor;

void * stmt_accept_visitor(Stmt *stmt, StmtVisitor *visitor, void *context);

#endif /* stmt_h */
