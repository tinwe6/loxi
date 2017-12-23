//
//  expr.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#include "expr.h"
#include "common.h"

/* Expressions */

Expr * init_assign(Token *name, Expr *value)
{
    Assign *assign = lox_alloc(Assign);
    assign->expr.type = EXPR_Assign;
    assign->expr.next = NULL;
    assign->name = name;
    assign->value = value;
    return AS_EXPR(assign);
}

Expr * init_binary(Expr *left, Token *operator, Expr *right)
{
    Binary *binary = lox_alloc(Binary);
    binary->expr.type = EXPR_Binary;
    binary->expr.next = NULL;
    binary->left = left;
    binary->operator = operator;
    binary->right = right;
    return AS_EXPR(binary);
}

Expr * init_call(Expr *callee, Token *paren, Expr *arguments)
{
    Call *call = lox_alloc(Call);
    call->expr.type = EXPR_Call;
    call->expr.next = NULL;
    call->callee = callee;
    call->paren = paren;
    call->arguments = arguments;
    return AS_EXPR(call);
}

Expr * init_get(Expr *object, Token *name)
{
    Get *get = lox_alloc(Get);
    get->expr.type = EXPR_Get;
    get->expr.next = NULL;
    get->object = object;
    get->name = name;
    return AS_EXPR(get);
}

Expr * init_grouping(Expr *expression)
{
    Grouping *grouping = lox_alloc(Grouping);
    grouping->expr.type = EXPR_Grouping;
    grouping->expr.next = NULL;
    grouping->expression = expression;
    return AS_EXPR(grouping);
}

Expr * init_literal(Token *value)
{
    Literal *literal = lox_alloc(Literal);
    literal->expr.type = EXPR_Literal;
    literal->expr.next = NULL;
    literal->value = *value;
    return AS_EXPR(literal);
}

Expr * init_logical(Expr *left, Token *operator, Expr *right)
{
    Logical *logical = lox_alloc(Logical);
    logical->expr.type = EXPR_Logical;
    logical->expr.next = NULL;
    logical->left = left;
    logical->operator = operator;
    logical->right = right;
    return AS_EXPR(logical);
}

Expr * init_set(Expr *object, Token *name, Expr *value)
{
    Set *set = lox_alloc(Set);
    set->expr.type = EXPR_Set;
    set->expr.next = NULL;
    set->object = object;
    set->name = name;
    set->value = value;
    return AS_EXPR(set);
}

Expr * init_super(Token *keyword, Token *method)
{
    Super *super = lox_alloc(Super);
    super->expr.type = EXPR_Super;
    super->expr.next = NULL;
    super->keyword = keyword;
    super->method = method;
    return AS_EXPR(super);
}


Expr * init_this(Token *keyword)
{
    This *this = lox_alloc(This);
    this->expr.type = EXPR_This;
    this->expr.next = NULL;
    this->keyword = keyword;
    return AS_EXPR(this);
}

Expr * init_unary(Token *operator, Expr *right)
{
    Unary *unary = lox_alloc(Unary);
    unary->expr.type = EXPR_Unary;
    unary->expr.next = NULL;
    unary->operator = operator;
    unary->right = right;
    return AS_EXPR(unary);
}

Expr * init_variable(Token *name)
{
    Variable *variable = lox_alloc(Variable);
    variable->expr.type = EXPR_Variable;
    variable->expr.next = NULL;
    variable->name = name;
    return AS_EXPR(variable);
}

Expr * init_bool_literal(bool value)
{
    Literal *literal = lox_alloc(Literal);
    literal->expr.type = EXPR_Literal;
    literal->expr.next = NULL;
    literal->value.type = value ? TT_TRUE : TT_FALSE;
    literal->value.literal = NULL;
    literal->value.nextInScan = NULL;
    return AS_EXPR(literal);
}

Expr * init_nil_literal()
{
    Literal *literal = lox_alloc(Literal);
    literal->expr.type = EXPR_Literal;
    literal->expr.next = NULL;
    literal->value.type = TT_NIL;
    literal->value.literal = NULL;
    literal->value.nextInScan = NULL;
    return AS_EXPR(literal);
}

Expr * init_number_literal(double value)
{
    Literal *literal = lox_alloc(Literal);
    literal->expr.type = EXPR_Literal;
    literal->expr.next = NULL;
    literal->value.type = TT_NUMBER;
    literal->value.number = value;
    literal->value.literal = NULL;
    literal->value.nextInScan = NULL;
    return AS_EXPR(literal);
}

// NOTE: The tokens are owned by the token list in the scanner,
//       and must not be freed here.
void free_expr(Expr *expr)
{
   while(expr)
   {
       switch (expr->type) {
           case EXPR_Assign: {
               Assign *assign = (Assign *)expr;
               free_expr(assign->value);
           } break;
           case EXPR_Binary: {
               Binary *binary = (Binary *)expr;
               free_expr(binary->left);
               free_expr(binary->right);
           } break;
           case EXPR_Call: {
               Call *call = (Call *)expr;
               free_expr(call->callee);
               free_expr(call->arguments);
           } break;
           case EXPR_Get: {
               Get *get = (Get *)expr;
               free_expr(get->object);
           } break;
           case EXPR_Grouping: {
               Grouping *grouping = (Grouping *)expr;
               free_expr(grouping->expression);
           } break;
           case EXPR_Literal: {
               // do nothing
           } break;
           case EXPR_Logical: {
               Logical *logical = (Logical *)expr;
               free_expr(logical->left);
               free_expr(logical->right);
           } break;
           case EXPR_Set: {
               Set *set = (Set *)expr;
               free_expr(set->object);
               free_expr(set->value);
           } break;
           case EXPR_Super: {
               // do nothing
           } break;
           case EXPR_This: {
               // do nothing
           } break;
           case EXPR_Unary: {
               Unary *unary = (Unary *)expr;
               free_expr(unary->right);
           } break;
           case EXPR_Variable: {
               // do nothing
           } break;
       }
       Expr *next = expr->next;
       lox_free(expr);
       expr = next;
   }
}

// Returns the count of element in the list elements
int32_t expr_count(Expr *elements)
{
    int32_t count = 0;
    while(elements != NULL)
    {
        count++;
        elements = elements->next;
    }
    return count;
}

// Returns the last statement in a non-empty chain of statements.
static inline Expr * expr_last(Expr *expressions)
{
    assert(expressions != NULL);
    
    Expr *last = expressions;
    while(last->next)
    {
        last = last->next;
    }
    return last;
}

Expr * expr_appendTo(Expr *head, Expr *tail)
{
    if (head == NULL)
    {
        return tail;
    }
    Expr *last = expr_last(head);
    last->next = tail;
    return head;
}

Expr * make_test_expr()
{
    Expr *expr = init_binary(init_unary(token_atomic(TT_MINUS, (Lexeme){{0, 0, 0}}), init_number_literal(123)),
                             token_atomic(TT_STAR, (Lexeme){{0, 0, 0}}),
                             init_grouping(init_number_literal(45.67)));
    return expr;
}

void * expr_accept_visitor(Expr *expr, ExprVisitor *visitor, void *context)
{
    void *result = NULL;
    switch(expr->type)
    {
#define DEFINE_VISIT_CASE(type)                                 \
        case EXPR_##type: {                                     \
            result = visitor->visit##type((type *)expr, context); \
        } break;

        FOREACH_AST_NODE(DEFINE_VISIT_CASE)
            
#undef DEFINE_VISIT_CASE
    }
    return result;
}
