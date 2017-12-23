//
//  resolver.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 03/11/2017.
//

#include "resolver.h"

#include "common.h"
#include "error.h"
#include "expr.h"
#include "memory.h"
#include "string.h"

typedef struct
{
    const char *name;
    int32_t index; // NOTE: unique id
    bool isDefined;
} ResolverEntry;

typedef struct ResolverHashTable
{
    ResolverEntry entries[RESOLVER_HASH_TABLE_SIZE];
    int32_t entriesCount;
    struct ResolverHashTable *nextInStack;
} ResolverHashTable;

typedef struct
{
    ResolverHashTable *top;
} ResolverStack;

typedef enum
{
    FT_NONE,
    FT_FUNCTION,
    FT_INITIALIZER,
    FT_METHOD,
} FunctionType;

typedef enum
{
    CT_NONE,
    CT_CLASS,
    CT_SUBCLASS,
} ClassType;

typedef struct
{
    ExprVisitor exprVisitor;
    StmtVisitor stmtVisitor;
    Interpreter *interpreter;
    ResolverStack scopes;
    FunctionType currentFunction;
    ClassType currentClass;
    Error *error;
    
    char *thisString;
    char *superString;
} Resolver;


static inline Error * resolver_throwError(Token *token, const char *message, Resolver *resolver)
{
    lox_token_error(resolver->interpreter->source, token, message);

    // NOTE: we only keep track of the last error that occurred for now.
    if(resolver->error)
    {
        freeError(resolver->error);
    }
    resolver->error = initError(token, message);
    
    return resolver->error;
}

// Stack

static inline void
push(ResolverHashTable *table, ResolverStack *stack)
{
    table->nextInStack = stack->top;
    stack->top = table;
}

static inline ResolverHashTable *
pop(ResolverStack *stack)
{
    ResolverHashTable *table = stack->top;
    stack->top = table->nextInStack;
    return table;
}

static inline ResolverHashTable *
peek(ResolverStack *stack)
{
    ResolverHashTable *table = stack->top;
    return table;
}

static inline bool
stackIsEmpty(ResolverStack *stack)
{
    bool result = (stack->top == NULL);
    return result;
}

// HashMap

static inline ResolverHashTable*
table_init()
{
    ResolverHashTable *table = lox_alloc(ResolverHashTable);
    if(table == NULL)
    {
        fatal_outOfMemory();
    }

    for(int32_t i = 0; i < RESOLVER_HASH_TABLE_SIZE; i++)
    {
        table->entries[i].name = NULL;
    }
    table->entriesCount = 0;
    
    return table;
}

static inline void
table_free(ResolverHashTable *table)
{
    lox_free(table);
}

static bool
table_insert(const char *name, bool isDefined, ResolverHashTable *table)
{
    static_assert(LOX_MAX_LOCAL_VARIABLES <= RESOLVER_HASH_TABLE_SIZE, "Resolver hash table size not large enough to store all local variables.");
    if (table->entriesCount == LOX_MAX_LOCAL_VARIABLES)
    {
        // NOTE: Too many local variables
        return false;
    }
    
    int32_t tableIndex = (str_hash(name) % RESOLVER_HASH_TABLE_SIZE);
    while (table->entries[tableIndex].name != NULL)
    {
        tableIndex++;
        if (tableIndex == RESOLVER_HASH_TABLE_SIZE)
        {
            tableIndex = 0;
        }
    }
    
    ResolverEntry *entry = table->entries + tableIndex;
    
    entry->name = name;
    entry->index = table->entriesCount;
    entry->isDefined = isDefined;

#ifdef RESOLVER_VERBOSE
    printf("R table_ins(%d) '%s' isDefined: %d\n", table->entriesCount, name, isDefined);
#endif
    
    table->entriesCount++;
    
    return true;
}

static ResolverEntry *
table_get(const char *name, ResolverHashTable *table)
{
    int32_t tableIndex = (str_hash(name) % RESOLVER_HASH_TABLE_SIZE);
    
    int32_t startIndex = tableIndex;
    while(table->entries[tableIndex].name != NULL)
    {
        if (str_isEqual(name, table->entries[tableIndex].name))
        {
            ResolverEntry *entry = table->entries + tableIndex;
            return entry;
        }
        tableIndex++;
        if (tableIndex == RESOLVER_HASH_TABLE_SIZE)
        {
            tableIndex = 0;
        }
        if (tableIndex == startIndex)
        {
            break;
        }
    }
    return NULL;
}

static bool
table_containsKey(const char *name, ResolverHashTable *table)
{
    int32_t tableIndex = (str_hash(name) % RESOLVER_HASH_TABLE_SIZE);
    int32_t startIndex = tableIndex;
    while(table->entries[tableIndex].name != NULL)
    {
        if (str_isEqual(name, table->entries[tableIndex].name))
        {
            return true;
        }
        tableIndex++;
        if (tableIndex == RESOLVER_HASH_TABLE_SIZE)
        {
            tableIndex = 0;
        }
        if (tableIndex == startIndex)
        {
            break;
        }
    }
    return false;
}

static void
resolveStmt(Stmt *stmt, Resolver *resolver)
{
    stmt_accept_visitor(stmt, &resolver->stmtVisitor, resolver);
}

static void
resolveExpr(Expr *expr, Resolver *resolver)
{
    expr_accept_visitor(expr, &resolver->exprVisitor, resolver);
}

static void
resolveStmtList(Stmt *statements, Resolver *resolver)
{
    Stmt *statement = statements;
    while(statement)
    {
        resolveStmt(statement, resolver);
        statement = statement->next;
    }
}

static void
beginScope(Resolver *resolver)
{
    ResolverHashTable *table = table_init();
    push(table, &resolver->scopes);
}

static void
endScope(Resolver *resolver)
{
    ResolverHashTable *table = pop(&resolver->scopes);
    table_free(table);
}

static Error *
declare(Token *name, Resolver *resolver)
{
    Error *error = NULL;
    if (!stackIsEmpty(&resolver->scopes))
    {
        ResolverHashTable *scope = peek(&resolver->scopes);
        const char *nameStr = get_identifier_name(name);
        if (table_containsKey(nameStr, scope))
        {
            error = resolver_throwError(name, "Variable with this name already declared in this scope.", resolver);
        }
        bool success = table_insert(nameStr, false, scope);
        if (!success)
        {
            error = resolver_throwError(name, "Too many local variables in function.", resolver);
        }
    }
    else
    {
        // global variable
    }
    return error;
}

static void
define(Token *name, Resolver *resolver)
{
    if (!stackIsEmpty(&resolver->scopes))
    {
        ResolverHashTable *scope = peek(&resolver->scopes);
        const char *nameStr = get_identifier_name(name);
        ResolverEntry *entry = table_get(nameStr, scope);
        assert(entry != NULL);
        entry->isDefined = true;
    }
    else
    {
        // global variable
    }
}

static void
resolveLocal(Expr *expr, const char *name, Resolver *resolver)
{
    ResolverHashTable *scope = peek(&resolver->scopes);
    // Note: Number of scopes between the current innermost
    //       scope and the scope where the variable was found.
    int32_t depth = 0;
    while(scope)
    {
        ResolverEntry *entry = table_get(name, scope);
        if (entry != NULL)
        {
#ifdef RESOLVER_VERBOSE
            printf("R Name '%s' resolved at depth %d (expr %p type %d)\n", name, depth, (void *)expr, expr->type);
#endif
            interpreter_resolve(expr, depth, entry->index, resolver->interpreter);
            return;
        }
        ++depth;
        scope = scope->nextInStack;
    }
    
    // NOTE: Not found. Assume it is global.
}

static void
resolveFunction(FunctionStmt *function, FunctionType type, Resolver *resolver)
{
    FunctionType enclosingFunction = resolver->currentFunction;
    resolver->currentFunction = type;
    
    beginScope(resolver);
    for(int32_t i = 0; i < function->arity; ++i)
    {
        Token *param = function->parameters[i];
        Error *error = declare(param, resolver);
        if(error)
        {
            break;
        }
        define(param, resolver);
    }
    resolveStmtList(function->body, resolver);
    endScope(resolver);
    
    resolver->currentFunction = enclosingFunction;
}

static void *
resolver_visitBlockStmt(BlockStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    beginScope(resolver);
    resolveStmtList(stmt->statements, resolver);
    endScope(resolver);
    return NULL;
}

static void *
resolver_visitClassStmt(ClassStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    declare(stmt->name, resolver);
    define(stmt->name, resolver);
    
    ClassType enclosingClass = resolver->currentClass;
    resolver->currentClass = CT_CLASS;
    
    if (stmt->superClass != NULL)
    {
        resolver->currentClass = CT_SUBCLASS;
        resolveExpr(stmt->superClass, resolver);
        beginScope(resolver);
        ResolverHashTable *scope = peek(&resolver->scopes);
        table_insert(resolver->superString, true, scope);
    }
    
    beginScope(resolver);
    ResolverHashTable *scope = peek(&resolver->scopes);
    table_insert(resolver->thisString, true, scope);

    FunctionStmt *method = stmt->methods;
    while (method != NULL)
    {
        assert(method->stmt.type == STMT_Function);
        FunctionType declaration = FT_METHOD;
        if (str_isEqual(get_identifier_name(method->name), "init"))
        {
            declaration = FT_INITIALIZER;
        }
        resolveFunction(method, declaration, resolver);
        method = (FunctionStmt *)method->stmt.next;
    }
    endScope(resolver);
    
    if (stmt->superClass != NULL)
    {
        endScope(resolver);
    }

    resolveLocal((void *)stmt, get_identifier_name(stmt->name), resolver);
    resolver->currentClass = enclosingClass;
    return NULL;
}

static void *
resolver_visitVarStmt(VarStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    Error *error = declare(stmt->name, resolver);
    if (error)
    {
        return error;
    }
    if (stmt->initializer != NULL)
    {
        resolveExpr(stmt->initializer, resolver);
    }
    define(stmt->name, resolver);

    return NULL;
}

static void *
resolver_visitVariableExpr(Variable *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;

    const char *name = get_identifier_name(expr->name);
    if (!stackIsEmpty(&resolver->scopes))
    {
        ResolverHashTable *scope = peek(&resolver->scopes);
        ResolverEntry *entry = table_get(name, scope);
        if ((entry != NULL) && (entry->isDefined == false))
        {
            Error *error = resolver_throwError(expr->name, "Cannot read local variable in its own initializer.", resolver);
            return error;
        }
    }

    resolveLocal((Expr *)expr, name, resolver);
    
    return NULL;
}

static void *
resolver_visitAssignExpr(Assign *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    
    resolveExpr(expr->value, resolver);
    const char *name = get_identifier_name(expr->name);
    resolveLocal((Expr *)expr, name, resolver);

    return NULL;
}

static void *
resolver_visitFunctionStmt(FunctionStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    declare(stmt->name, resolver);
    define(stmt->name, resolver);
    resolveFunction(stmt, FT_FUNCTION, resolver);
    
    return NULL;
}

static void *
resolver_visitExpressionStmt(ExpressionStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(stmt->expression, resolver);
    return NULL;
}

static void *
resolver_visitIfStmt(IfStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(stmt->condition, resolver);
    resolveStmt(stmt->thenBranch, resolver);
    if (stmt->elseBranch != NULL)
    {
        resolveStmt(stmt->elseBranch, resolver);
    }

    return NULL;
}

static void *
resolver_visitPrintStmt(PrintStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(stmt->expression, resolver);
    return NULL;
}

static void *
resolver_visitReturnStmt(ReturnStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    
    if (resolver->currentFunction == FT_NONE) {
        Error *error = resolver_throwError(stmt->keyword, "Cannot return from top-level code.", resolver);
        return error;
    }
    
    if (stmt->value != NULL)
    {
        if (resolver->currentFunction == FT_INITIALIZER)
        {
            Error *error = resolver_throwError(stmt->keyword, "Cannot return a value from an initializer.", resolver);
            return error;
        }
        resolveExpr(stmt->value, resolver);
    }
    
    return NULL;
}

static void *
resolver_visitWhileStmt(WhileStmt *stmt, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(stmt->condition, resolver);
    resolveStmt(stmt->body, resolver);

    return NULL;
}

static void *
resolver_visitBinaryExpr(Binary *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(expr->left, resolver);
    resolveExpr(expr->right, resolver);
    
    return NULL;
}

static void *
resolver_visitCallExpr(Call *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(expr->callee, resolver);
    
    Expr *arg = expr->arguments;
    while (arg != NULL)
    {
        resolveExpr(arg, resolver);
        arg = arg->next;
    }
    
    return NULL;
}

static void *
resolver_visitGetExpr(Get *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(expr->object, resolver);
    return NULL;
}

static void *
resolver_visitGroupingExpr(Grouping *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(expr->expression, resolver);

    return NULL;
}

static void *
resolver_visitLiteralExpr(Literal *expr, void *context)
{
    return NULL;
}

static void *
resolver_visitLogicalExpr(Logical *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(expr->left, resolver);
    resolveExpr(expr->right, resolver);
    return NULL;
}

static void *
resolver_visitSetExpr(Set *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(expr->value, resolver);
    resolveExpr(expr->object, resolver);
    return NULL;
}

static void *
resolver_visitSuperExpr(Super *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    if (resolver->currentClass == CT_NONE)
    {
        Error *error = resolver_throwError(expr->keyword, "Cannot use 'super' outside of a class.", resolver);
        return error;
    }
    else if (resolver->currentClass != CT_SUBCLASS)
    {
        Error *error = resolver_throwError(expr->keyword, "Cannot use 'super' in a class with no superclass.", resolver);
        return error;
    }
    resolveLocal((Expr *)expr, resolver->superString, resolver);
    return NULL;
}

static void *
resolver_visitThisExpr(This *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;

    if (resolver->currentClass == CT_NONE)
    {
        Error *error = resolver_throwError(expr->keyword, "Cannot use 'this' outside of a class.", resolver);
        return error;
    }

    assert(expr->keyword->type == TT_THIS);
    resolveLocal((Expr *)expr, resolver->thisString, resolver);
    return NULL;
}
static void *
resolver_visitUnaryExpr(Unary *expr, void *context)
{
    Resolver *resolver = (Resolver *)context;
    resolveExpr(expr->right, resolver);
    
    return NULL;
}

static Resolver *
resolver_init(Interpreter *interpreter)
{
    Resolver *resolver = lox_alloc(Resolver);
    if(resolver == NULL)
    {
        fatal_outOfMemory();
    }
    resolver->interpreter = interpreter;
    
    // Initializes the stack of scopes
    resolver->scopes.top = NULL;
    
    resolver->currentFunction = FT_NONE;
    resolver->currentClass = CT_NONE;
    resolver->error = NULL;
    
    resolver->thisString = str_fromLiteral("this");
    resolver->superString = str_fromLiteral("super");
    
    // Initialize expression visitor
    {
        ExprVisitor *visitor = &resolver->exprVisitor;
#define DEFINE_VISITOR(type) \
        visitor->visit##type = resolver_visit##type##Expr;
        FOREACH_AST_NODE(DEFINE_VISITOR)
#undef DEFINE_VISITOR
    }
    
    // Initialize statement visitor
    {
        StmtVisitor *visitor = &resolver->stmtVisitor;
#define DEFINE_VISITOR(type) \
visitor->visit##type = resolver_visit##type##Stmt;
        
        FOREACH_STMT_NODE(DEFINE_VISITOR)
#undef DEFINE_VISITOR
    }
    
    return resolver;
}

static void resolver_free(Resolver *resolver)
{
    if(resolver->error)
    {
        freeError(resolver->error);
    }
    str_free(resolver->thisString);
    str_free(resolver->superString);
    assert(stackIsEmpty(&resolver->scopes));
    lox_free(resolver);
}

void resolve(Stmt* statements, Interpreter *interpreter)
{
    Resolver *resolver = resolver_init(interpreter);
    resolveStmtList(statements, resolver);
    resolver_free(resolver);
}

