//
//  stmt.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 18/10/2017.
//

#include "stmt.h"

Stmt * initBlock(Stmt *statements)
{
    BlockStmt *stmt = lox_alloc(BlockStmt);
    stmt->stmt.type = STMT_Block;
    stmt->stmt.next = NULL;
    stmt->statements = statements;
    return AS_STMT(stmt);
}

Stmt * initExpression(Expr *expr)
{
    ExpressionStmt *stmt = lox_alloc(ExpressionStmt);
    stmt->stmt.type = STMT_Expression;
    stmt->stmt.next = NULL;
    stmt->expression = expr;
    return AS_STMT(stmt);
}

Stmt * initClass(Token *name, Expr *superClass, FunctionStmt *methods)
{
    ClassStmt *stmt = lox_alloc(ClassStmt);
    stmt->stmt.type = STMT_Class;
    stmt->stmt.next = NULL;
    stmt->name = name;
    stmt->superClass = superClass;
    stmt->methods = methods;
    return AS_STMT(stmt);
}

Stmt * initFunction(Token *name, Token **parameters, int32_t parametersCount, Stmt *body)
{
    FunctionStmt *stmt = lox_alloc(FunctionStmt);
    stmt->stmt.type = STMT_Function;
    stmt->stmt.next = NULL;
    stmt->name = name;
    stmt->parameters = parameters;
    stmt->arity = parametersCount;
    stmt->body = body;

    return AS_STMT(stmt);
}

Stmt * initIf(Expr *condition, Stmt *thenBranch, Stmt *elseBranch)
{
    IfStmt *stmt = lox_alloc(IfStmt);
    stmt->stmt.type = STMT_If;
    stmt->stmt.next = NULL;
    stmt->condition = condition;
    stmt->thenBranch = thenBranch;
    stmt->elseBranch = elseBranch;
    return AS_STMT(stmt);
}

Stmt * initPrint(Expr *expr)
{
    PrintStmt *stmt = lox_alloc(PrintStmt);
    stmt->stmt.type = STMT_Print;
    stmt->stmt.next = NULL;
    stmt->expression = expr;
    return AS_STMT(stmt);
}

Stmt * initReturn(Token *keyword, Expr *value)
{
    ReturnStmt *stmt = lox_alloc(ReturnStmt);
    stmt->stmt.type = STMT_Return;
    stmt->stmt.next = NULL;
    stmt->keyword = keyword;
    stmt->value = value;
    return AS_STMT(stmt);
}

Stmt * initVar(Token *name, Expr *initializer)
{
    VarStmt *stmt = lox_alloc(VarStmt);
    stmt->stmt.type = STMT_Var;
    stmt->stmt.next = NULL;
    stmt->name = name;
    stmt->initializer = initializer;
    return AS_STMT(stmt);
}

Stmt * initWhile(Expr *condition, Stmt *body)
{
    WhileStmt *stmt = lox_alloc(WhileStmt);
    stmt->stmt.type = STMT_While;
    stmt->stmt.next = NULL;
    stmt->condition = condition;
    stmt->body = body;
    return AS_STMT(stmt);
}

// Free the list of statements stmt.
// If stmt is NULL the function does nothing.
void freeStmt(Stmt *stmt)
{
    while(stmt)
    {
        switch (stmt->type) {
            case STMT_Block: {
                BlockStmt *block = (BlockStmt *)stmt;
                freeStmt(block->statements);
            } break;
            case STMT_Class: {
                ClassStmt *classStmt = (ClassStmt *)stmt;
                freeStmt(AS_STMT(classStmt->methods));
                free_expr(classStmt->superClass);
            } break;
            case STMT_Expression: {
                ExpressionStmt *expression = (ExpressionStmt *)stmt;
                free_expr(expression->expression);
            } break;
            case STMT_Function: {
                FunctionStmt *fun = (FunctionStmt *)stmt;
                freeStmt(fun->body);
                lox_free(fun->parameters);
            } break;
            case STMT_If: {
                IfStmt *ifStmt = (IfStmt *)stmt;
                free_expr(ifStmt->condition);
                freeStmt(ifStmt->thenBranch);
                freeStmt(ifStmt->elseBranch);
            } break;
            case STMT_Print: {
                PrintStmt *print = (PrintStmt *)stmt;
                free_expr(print->expression);
            } break;
            case STMT_Return: {
                ReturnStmt *ret = (ReturnStmt *)stmt;
                free_expr(ret->value);
            } break;
            case STMT_Var: {
                VarStmt *var = (VarStmt *)stmt;
                free_expr(var->initializer);
            } break;
            case STMT_While: {
                WhileStmt *whileStmt = (WhileStmt *)stmt;
                free_expr(whileStmt->condition);
                freeStmt(whileStmt->body);
            } break;
        }
        Stmt *next = stmt->next;
        lox_free(stmt);
        stmt = next;
    }
}

// Returns the last statement in a non-empty chain of statements.
Stmt * stmt_last(Stmt *statements)
{
    assert(statements != NULL);
    
    Stmt *last = statements;
    while(last->next)
    {
        last = last->next;
    }
    return last;
}

Stmt * stmt_appendTo(Stmt *head, Stmt *tail)
{
    if (head == NULL)
    {
        return tail;
    }
    Stmt *last = stmt_last(head);
    last->next = tail;
    return head;
}

void * stmt_accept_visitor(Stmt *stmt, StmtVisitor *visitor, void *context)
{
    void *result = NULL;
    switch(stmt->type)
    {
#define DEFINE_VISIT_CASE(type)                                  \
    case STMT_##type: {                                          \
    result = visitor->visit##type((type##Stmt *)stmt, context); \
} break;
            
        FOREACH_STMT_NODE(DEFINE_VISIT_CASE)
            
#undef DEFINE_VISIT_CASE
    }
    return result;
}
