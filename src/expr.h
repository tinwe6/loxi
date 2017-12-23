//
//  expr.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#ifndef expr_h
#define expr_h

#include "token.h"

/*
 "Assign   : Token name, Expr value",
 "Binary   : Expr left, Token operator, Expr right",
 "Call     : Expr callee, Token paren, List<Expr> arguments",
 "Get      : Expr object, Token name",
 "Grouping : Expr expression",
 "Literal  : Object value",
 "Logical  : Expr left, Token operator, Expr right",
 "Set      : Expr object, Token name, Expr value",
 "Super    : Token keyword, Token method",
 "This     : Token keyword",
 "Unary    : Token operator, Expr right",
 "Variable : Token name"
 */

/*
 The Expr structure is used for the nodes of the abstract syntax tree.
*/

#define FOREACH_AST_NODE(code)                    \
    code(Assign)   code(Binary)   code(Call)      \
    code(Get)      code(Grouping) code(Unary)     \
    code(Literal)  code(Logical)  code(This)      \
    code(Set)      code(Super)    code(Variable)

typedef enum
{
#define DEFINE_ENUM_TYPE(type) EXPR_##type,
    FOREACH_AST_NODE(DEFINE_ENUM_TYPE)
#undef DEFINE_ENUM_TYPE
} ExprType;


typedef struct Expr_tag
{
    ExprType type;
    struct Expr_tag *next;
} Expr;

#define AS_EXPR(expr) (Expr *)expr

Expr * init_assign(Token *name, Expr *value);
Expr * init_binary(Expr *left, Token *operator, Expr *right);
Expr * init_call(Expr *callee, Token *paren, Expr *arguments);
Expr * init_get(Expr *object, Token *name);
Expr * init_grouping(Expr *expression);
Expr * init_literal(Token *value);
Expr * init_logical(Expr *left, Token *operator, Expr *right);
Expr * init_set(Expr *object, Token *name, Expr *value);
Expr * init_super(Token *keyword, Token *method);
Expr * init_this(Token *keyword);
Expr * init_unary(Token *operator, Expr *right);
Expr * init_variable(Token *name);

Expr * init_bool_literal(bool value);
Expr * init_nil_literal(void);
Expr * init_number_literal(double value);

void free_expr(Expr *expr);

int32_t expr_count(Expr *elements);
Expr * expr_appendTo(Expr *head, Expr *tail);

Expr * make_test_expr(void);

// NOTE:   "Assign   : Token name, Expr value",
typedef struct
{
    Expr expr;
    Token *name;
    Expr *value;
} Assign;

// NOTE: "Binary   : Expr left, Token operator, Expr right",
typedef struct
{
    Expr expr;
    Expr *left;
    Token *operator;
    Expr *right;
} Binary;

// NOTE:  "Call     : Expr callee, Token paren, List<Expr> arguments",
typedef struct
{
    Expr expr;
    Expr *callee;
    Token *paren; // token for the closing parenthesis; its location is used when reporting a runtime error
    Expr *arguments; // List of expressions
} Call;

// NOTE:  "Get      : Expr object, Token name",
typedef struct
{
    Expr expr;
    Expr *object;
    Token *name;
} Get;

// NOTE:  "Grouping : Expr expression",
typedef struct
{
    Expr expr;
    Expr *expression;
} Grouping;

// NOTE:  "Literal  : Object value",
typedef struct
{
    Expr expr;
    Token value;
} Literal;

// NOTE:  "Logical  : Expr left, Token operator, Expr right",
typedef struct
{
    Expr expr;
    Expr *left;
    Token *operator;
    Expr *right;
} Logical;

// NOTE: "Set      : Expr object, Token name, Expr value",
typedef struct
{
    Expr expr;
    Expr *object;
    Token *name;
    Expr *value;
} Set;

// NOTE: "Super    : Token keyword, Token method",
typedef struct
{
    Expr expr;
    Token *keyword;
    Token *method;
} Super;

// NOTE: "This     : Token keyword",
typedef struct
{
    Expr expr;
    Token *keyword;
} This;

// NOTE: "Unary    : Token operator, Expr right",
typedef struct
{
    Expr expr;
    Token *operator;
    Expr *right;
} Unary;

// NOTE: "Variable : Token name"
typedef struct
{
    Expr expr;
    Token *name;
} Variable;

/***********/
/* Visitor */
/***********/

typedef struct
{
#define DEFINE_VISIT(type) \
void *(*visit##type)(type *, void *context);
    
    FOREACH_AST_NODE(DEFINE_VISIT)
#undef DEFINE_VISIT
} ExprVisitor;

void * expr_accept_visitor(Expr *expr, ExprVisitor *visitor, void *context);

#endif /* expr_h */
