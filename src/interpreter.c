//
//  interpreter.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 17/10/2017.
//

#include "interpreter.h"

#include "common.h"
#include "garbage_collector.h"
#include "lox_callable.h"
#include "lox_class.h"
#include "lox_function.h"
#include "lox_instance.h"
#include "objects.h"
#include "return.h"
#include "token.h"

/* Exceptions */

#define LOX_EXCEPTION_SETUP_LONGJMP 0
#define LOX_EXCEPTION_RUNTIME_ERROR 1
#define LOX_EXCEPTION_EXIT          2

__attribute__((__noreturn__))
void interpreter_throwExit(Interpreter *interpreter)
{
    interpreter->exitREPL = true;
    longjmp(interpreter->catchLocation, LOX_EXCEPTION_EXIT);
}


__attribute__((__noreturn__))
static inline void interpreter_throwError(Error *error, Interpreter *interpreter)
{
    // NOTE: we only keep track of the last error that occurred for now.
    if(interpreter->runtimeError)
    {
        freeError(interpreter->runtimeError);
    }
    interpreter->runtimeError = error;
    
    longjmp(interpreter->catchLocation, LOX_EXCEPTION_RUNTIME_ERROR);
}

__attribute__((__noreturn__))
static inline void interpreter_throwNewError(Token *token, const char *message, Interpreter *interpreter)
{
    Error *error = initError(token, message);
    interpreter_throwError(error, interpreter);
}

__attribute__((__noreturn__))
static inline void interpreter_throwNewErrorString(Token *token, char *message, Interpreter *interpreter)
{
    Error *error = initErrorString(token, message);
    interpreter_throwError(error, interpreter);
}

__attribute__((__noreturn__))
static inline void interpreter_throwErrorIdentifier(const char *prefixLiteral, const Token *identifier, const char *suffixLiteral, Interpreter *interpreter)
{
    Error *error = initErrorIdentifier(prefixLiteral, identifier, suffixLiteral);
    interpreter_throwError(error, interpreter);
}

void interpreter_clearRuntimeError(Interpreter *interpreter)
{
    if(interpreter->runtimeError)
    {
        freeError(interpreter->runtimeError);
        interpreter->runtimeError = NULL;
    }
}

// NOTE: Pushes an object on the stack of locked objects. If the stack is
//       full, throws a stack overflow exception.
#define GC_LOCK(object, token) do {                                       \
    if (gcLock(object, interpreter->collector) == false)                  \
        interpreter_throwNewError(token, "Stack overflow.", interpreter); \
} while (0)

/* Evaluation/execution/calls */

static inline Object * evaluate(Expr *expr, Interpreter *interpreter)
{
    Object *result = (Object *)expr_accept_visitor(expr, &interpreter->exprVisitor, interpreter);
    return result;
}

static inline Return * execute(Stmt *statement, Interpreter *interpreter)
{
    Return *ret = (Return *)stmt_accept_visitor(statement, &interpreter->stmtVisitor, interpreter);
    return ret;
}

Return * interpreter_executeBlock(Stmt *statements, Environment *environment, Interpreter *interpreter)
{
    Environment *previous = interpreter->environment;
    interpreter->environment = environment;
    
    Return *ret = NULL;
    Stmt *statement = statements;
    while(statement)
    {
        ret = execute(statement, interpreter);
        if(ret != NULL)
        {
            break;
        }
        statement = statement->next;
    }

    interpreter->environment = previous;

    return ret;
}

static inline Object * interpreter_call(lox_callable_function *f, LoxArguments *arguments, Interpreter *interpreter)
{
    Object *result = f(arguments, interpreter);
    return result;
}

static inline Object *interpreter_callClass(LoxClass *klass, LoxArguments *arguments, Error **error, Interpreter *interpreter)
{
    struct LoxClassInitializerContext context = {interpreter, klass, NULL};
    Object *result = klass->callable->function(arguments, &context);
    *error = context.error;
    return result;
}

/* Locals */

#ifdef USE_LOCALS_HASH_MAP
// TODO: test/tune hash function
// NOTE: This is essentially http://xoroshiro.di.unimi.it/splitmix64.c
static inline uint32_t hashPointer(const void *pointer)
{
    uint64_t z = (uint64_t)pointer;
    z += 0x9e3779b97f4a7c15;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return (uint32_t)(z ^ (z >> 31));
}
#endif

// Inserts expression `expr` in the hash map
void interpreter_resolve(Expr *expr, int32_t depth, int32_t index, Interpreter *interpreter)
{
    assert(interpreter->localsCount != LOCALS_HASH_MAP_SIZE);
#ifdef USE_LOCALS_HASH_MAP
    uint32_t hash = hashPointer(expr) & (LOCALS_HASH_MAP_SIZE - 1);
    while(interpreter->locals[hash].expr != 0)
    {
        ++hash;
        if (hash == LOCALS_HASH_MAP_SIZE)
        {
            hash = 0;
        }
    }
#else
    uint32_t hash = interpreter->localsCount;
#endif
    interpreter->locals[hash].expr = expr;
    interpreter->locals[hash].depth = depth;
    interpreter->locals[hash].index = index;
    interpreter->localsCount++;
}

// Returns the entry for the local object represented by expr.
// If not found, returns NULL instead (the variable is a global)
static const LocalEntry *
localsGet(const void *expr, const LocalEntry *locals, int32_t localsCount)
{
#ifdef USE_LOCALS_HASH_MAP
    uint32_t i = hashPointer(expr) & (LOCALS_HASH_MAP_SIZE - 1);
    do {
        const LocalEntry *entry = locals + i;
        if (entry->expr == expr)
        {
            return entry;
        }
        if (entry->expr == 0)
        {
            return NULL;
        }
        ++i;
        if (i == LOCALS_HASH_MAP_SIZE)
        {
            i = 0;
        }
    } while(true);
#else
    for (int32_t i = 0; i < localsCount; i++)
    {
        const LocalEntry *entry = locals + i;
        if (entry->expr == expr)
        {
            return entry;
        }
    }
    return NULL;
#endif
}

/* Variables definition/assignment */

// Looks up the identifier amongs the locals, and if not found among the globals.
// If the identifier is found, returns a copy of its value; if not returns NULL.
// The caller is responsible for freeing the value.
static Object *
lookUpVariable(const Token *identifier, const void *expr, Interpreter *interpreter)
{
    Object *value;
    Error *error = NULL;
    const LocalEntry *entry = localsGet(expr, interpreter->locals, interpreter->localsCount);
    if (entry != NULL)
    {
        value = env_getAt(identifier, entry->depth, entry->index, interpreter->environment, &error, interpreter->collector);
    } else {
        value = env_getGlobal(identifier, interpreter->globals, &error, interpreter->collector);
    }
    if (error)
    {
        interpreter_throwError(error, interpreter);
        assert(value == NULL);
    }
    return value;
}

static void assignVariable(const Token *identifier, const void *expr, Object *value, Interpreter *interpreter)
{
    const LocalEntry *entry = localsGet(expr, interpreter->locals, interpreter->localsCount);
    if (entry != NULL)
    {
        env_assignAt(value, entry->depth, entry->index, interpreter->environment, interpreter->collector);
    } else {
        Error *error = env_assignGlobal(identifier, value, interpreter->globals, interpreter->collector);
        if(error != NULL)
        {
            interpreter_throwError(error, interpreter);
        }
    }
}

static void interpreter_defineNative(char *name, lox_callable_function f, int32_t argsCount, Interpreter *interpreter)
{
    Object *callable = obj_wrapCallable(callableInit(f, argsCount), interpreter->collector);
    env_defineNative(name, callable, interpreter->globals);
}

/* Visitors helpers */

// Returns true if the operand is a number, otherwise throws a runtime error and returns false.
static bool checkNumberOperand(Token *operator, Object *operand, Interpreter *interpreter)
{
    if (operand->type == OT_NUMBER)
    {
        return true;
    }
    interpreter_throwNewError(operator, "Operand must be a number.", interpreter);
    return false;
}

// Returns true if the operand are numbers, otherwise throws a runtime error and returns false.
static bool checkNumberOperands(Token *operator, Object *left, Object *right, Interpreter *interpreter)
{
    if (left->type == OT_NUMBER && right->type == OT_NUMBER)
    {
        return true;
    }
    interpreter_throwNewError(operator, "Operands must be numbers.", interpreter);
    return false;
}

/* Expr visitors */

static void * interpreter_visitAssignExpr(Assign *expr, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    
    Object *value = evaluate(expr->value, interpreter);
    assert(value != NULL);
    GC_LOCK(value, expr->name);
    Object *duplicate = obj_dup(value, interpreter->collector);
    GC_LOCK(duplicate, expr->name);
    assignVariable(expr->name, expr, duplicate, interpreter);
    gcPopLock(interpreter->collector);
    gcPopLock(interpreter->collector);
    
    return (void *)value;
}

// NOTE: In a binary expression, we evaluate the operands in left-to-right order.
//       Also, we evaluate the operands before checking their types.
static void * interpreter_visitBinaryExpr(Binary *expr, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    
    Object *left = evaluate(expr->left, context);
    GC_LOCK(left, expr->operator);
    
    Object *right = evaluate(expr->right, context);
    GC_LOCK(right, expr->operator);

    Object *result = NULL;
    TokenType type = expr->operator->type;
    switch (type) {
        case TT_GREATER: {
            checkNumberOperands(expr->operator, left, right, interpreter);
            result = obj_newBoolean(obj_unwrapNumber(left) > obj_unwrapNumber(right), interpreter->collector);
        } break;
        case TT_GREATER_EQUAL:
        {
            checkNumberOperands(expr->operator, left, right, interpreter);
            result = obj_newBoolean(obj_unwrapNumber(left) >= obj_unwrapNumber(right), interpreter->collector);
        } break;
        case TT_LESS:
        {
            checkNumberOperands(expr->operator, left, right, interpreter);
            result = obj_newBoolean(obj_unwrapNumber(left) < obj_unwrapNumber(right), interpreter->collector);
        } break;
        case TT_LESS_EQUAL:
        {
            checkNumberOperands(expr->operator, left, right, interpreter);
            result = obj_newBoolean(obj_unwrapNumber(left) <= obj_unwrapNumber(right), interpreter->collector);
        } break;
        case TT_MINUS:
        {
            checkNumberOperands(expr->operator, left, right, interpreter);
            result = obj_newNumber(obj_unwrapNumber(left) - obj_unwrapNumber(right), interpreter->collector);
        } break;
        case TT_PLUS:
        {
            if (left->type == OT_NUMBER && right->type == OT_NUMBER)
            {
                result = obj_newNumber(obj_unwrapNumber(left) + obj_unwrapNumber(right), interpreter->collector);
            }
            else if (left->type == OT_STRING && right->type == OT_STRING)
            {
                result = obj_wrapString(str_concat(obj_unwrapString(left), obj_unwrapString(right)), interpreter->collector);
            }
            else if ((left->type == OT_STRING) && (right->type == OT_NUMBER))
            {
                // NOTE: LOX tests do not stringify bools and nil
                char *rightString = obj_stringify(right);
                result = obj_wrapString(str_concat(obj_unwrapString(left), rightString), interpreter->collector);
                str_free(rightString);
            }
            else if ((right->type == OT_STRING) && (left->type == OT_NUMBER))
            {
                // NOTE: LOX tests do not stringify bools and nil
                char *leftString = obj_stringify(left);
                result = obj_wrapString(str_concat(leftString, obj_unwrapString(right)), interpreter->collector);
                str_free(leftString);
            }
            else
            {
                interpreter_throwNewError(expr->operator, "Operands must be two numbers or two strings.", interpreter);
            }
        } break;
        case TT_SLASH:
        {
            checkNumberOperands(expr->operator, left, right, interpreter);
            double denominator = obj_unwrapNumber(right);
            if(denominator == 0.0)
            {
                interpreter_throwNewError(expr->operator, "Division by zero.", interpreter);
            }
            else
            {
                result = obj_newNumber(obj_unwrapNumber(left) / denominator, interpreter->collector);
            }
        } break;
        case TT_STAR:
        {
            checkNumberOperands(expr->operator, left, right, interpreter);
            result = obj_newNumber(obj_unwrapNumber(left) * obj_unwrapNumber(right), interpreter->collector);
        } break;
        case TT_BANG_EQUAL:
        {
            result = obj_newBoolean(!isEqual(left, right), interpreter->collector);
        } break;
        case TT_EQUAL_EQUAL:
        {
            result = obj_newBoolean(isEqual(left, right), interpreter->collector);
        } break;
        INVALID_DEFAULT_CASE;
    }
    
    gcPopLock(interpreter->collector);
    gcPopLock(interpreter->collector);
    
    return (void *)result;
}

// NOTE: the callee expression is evaluated first, then all arguments from left to right.
static void * interpreter_visitCallExpr(Call *expr, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    Object *callee = evaluate(expr->callee, interpreter);
    GC_LOCK(callee, expr->paren);

    LoxArguments *arguments = argumentsInit();
    if(arguments == NULL)
    {
        fatal_outOfMemory();
    }
    arguments->count = 0;
    
    // NOTE: we wrap `arguments` in an object so that it is not leaked in case of an exception.
    //       Then, by locking it, we protect the objects it carries too from release.
    Object *argumentsObj = obj_wrapArguments(arguments, interpreter->collector);
    GC_LOCK(argumentsObj, expr->paren);
    
    Expr *argExpr = expr->arguments;
    while (argExpr != NULL)
    {
        Object *argument = evaluate(argExpr, interpreter);
        // NOTE: if the source was parsed correctly, the function must have less than LOX_MAX_ARG_COUNT arguments
        assert(arguments->count < LOX_MAX_ARG_COUNT);
        arguments->values[arguments->count++] = argument;
        argExpr = argExpr->next;
    }

    int32_t arity;
    if(isLoxCallable(callee))
    {
        const LoxCallable *function = obj_unwrapCallable(callee);
        arity = callableArity(function);
        if(arguments->count == arity)
        {
            Object *result = interpreter_call(function->function, arguments, interpreter);
            // NOTE: unlock `arguments`
            gcPopLock(interpreter->collector);
            // NOTE: unlock `callee`
            gcPopLock(interpreter->collector);
            return (void *)result;
        }
    }
    else if(isLoxFunction(callee))
    {
        LoxFunction *function = obj_unwrapFunction(callee);
        arity = function->declaration->arity;
        if(arguments->count == arity)
        {
            Error *error = NULL;
            Object *result = function_call(function, arguments, &error, interpreter);
            if (error)
            {
                assert(error->token == NULL);
                error->token = expr->paren;
                interpreter_throwError(error, interpreter);
                return NULL;
            }
            // NOTE: unlock `arguments`
            gcPopLock(interpreter->collector);
            // NOTE: unlock `callee`
            gcPopLock(interpreter->collector);

            return (void *)result;
        }
    }
    else if(isLoxClass(callee))
    {
        LoxClass *klass = obj_unwrapClass(callee);
        arity = klass->callable->arity;
        if(arguments->count == arity)
        {
            Error *error = NULL;
            Object *result = interpreter_callClass(klass, arguments, &error, interpreter);
            if (error != NULL)
            {
                assert(error->token == NULL);
                error->token = expr->paren;
                interpreter_throwError(error, interpreter);
                return NULL;
            }
            
            // NOTE: unlock `arguments`
            gcPopLock(interpreter->collector);
            // NOTE: unlock `callee`
            gcPopLock(interpreter->collector);

            return (void *)result;
        }
    }
    else
    {
        interpreter_throwNewError(expr->paren, "Can only call functions and classes.", interpreter);
    }

    char *message = str_alloc(128);
    sprintf(message, "Expected %d arguments but got %d.", arity, arguments->count);
    interpreter_throwNewErrorString(expr->paren, message, interpreter);
}

static void * interpreter_visitGetExpr(Get *expr, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    Object *result = NULL;
    
    Object *object = evaluate(expr->object, interpreter);
    assert(object != NULL);
    Error *error = NULL;
    if (isLoxInstance(object))
    {
        GC_LOCK(object, expr->name);
        LoxInstance *instance = obj_unwrapInstance(object);
        result = instanceGet(instance, expr->name, &error, interpreter->collector);
        gcPopLock(interpreter->collector);
    }
    else
    {
        error = initError(expr->name, "Only instances have properties.");
    }
    if (error != NULL)
    {
        interpreter_throwError(error, interpreter);
    }
    return result;
}

static void  * interpreter_visitGroupingExpr(Grouping *expr, void *context)
{
    Object *result = evaluate(expr->expression, context);
    return (void *)result;
}

static void * interpreter_visitLiteralExpr(Literal *expr, void *context)
{
    GarbageCollector *collector = ((Interpreter *)context)->collector;
    Object *object;
    Literal *literal = (Literal *)expr;
    switch(literal->value.type) {
        case TT_NUMBER:
        {
            object = obj_newNumber(literal->value.number, collector);
        } break;
        case TT_STRING:
        {
            object = obj_newString(get_string_value(&literal->value), collector);
        } break;
        case TT_TRUE:
        {
            object = obj_newBoolean(true, collector);
        } break;
        case TT_FALSE:
        {
            object = obj_newBoolean(false, collector);
        } break;
        case TT_NIL:
        {
            object = obj_newNil(collector);
        } break;
        INVALID_DEFAULT_CASE;
    }
    
    return (void *)object;
}

static void * interpreter_visitLogicalExpr(Logical *expr, void *context)
{
    Object *left = evaluate(expr->left, context);
    
    if(expr->operator->type == TT_OR)
    {
        if(isTruthy(left))
        {
            return left;
        }
    }
    else
    {
        assert(expr->operator->type == TT_AND);
        if(!isTruthy(left))
        {
            return left;
        }
    }
    
    Interpreter *interpreter = (Interpreter *)context;
    GC_LOCK(left, expr->operator);
    Object *right = evaluate(expr->right, context);
    gcPopLock(interpreter->collector);

    return right;
}

static void *interpreter_visitSetExpr(Set *expr, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    Object *value = NULL;

    Object *object = evaluate(expr->object, context);
    assert(object != NULL);
    if(isLoxInstance(object))
    {
        LoxInstance *instance = obj_unwrapInstance(object);
        
        GC_LOCK(object, expr->name);
        value = evaluate(expr->value, context);
        if(value != NULL)
        {
            GC_LOCK(value, expr->name);
            Object *duplicate = obj_dup(value, interpreter->collector);
            instanceSet(instance, expr->name, duplicate);
            gcPopLock(interpreter->collector);
        }
        gcPopLock(interpreter->collector);
    }
    else
    {
        interpreter_throwNewError(expr->name, "Only instances have fields.", interpreter);
    }
    return value;
}

static void * interpreter_visitSuperExpr(Super *expr, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    const LocalEntry *entry = localsGet(expr, interpreter->locals, interpreter->localsCount);
    Error *error = NULL;
    Object *superClass = env_getAt(expr->keyword, entry->depth, entry->index, interpreter->environment, &error, interpreter->collector);
    if (error)
    {
        interpreter_throwError(error, interpreter);
    }
    GC_LOCK(superClass, expr->keyword);
    
    // NOTE: "this" is always one level nearer than "super"'s environment.
    Object *object = env_getAt(expr->keyword, entry->depth - 1, 0, interpreter->environment, &error, interpreter->collector);
    if (error)
    {
        interpreter_throwError(error, interpreter);
    }
    GC_LOCK(object, expr->keyword);
    
    const char *methodName = get_identifier_name(expr->method);
    LoxFunction *method = findMethod(obj_unwrapInstance(object), obj_unwrapClass(superClass), methodName, &error, interpreter->collector);
    if (error)
    {
        interpreter_throwError(error, interpreter);
    }
    if (method == NULL)
    {
        interpreter_throwErrorIdentifier("Undefined property '", expr->method, "'.", interpreter);
    }
    assert(method != NULL);
    Object *result = obj_wrapFunction(method, interpreter->collector);
    gcPopLock(interpreter->collector); // NOTE: unlocks object
    gcPopLock(interpreter->collector); // NOTE: unlocks superClass

    env_release(method->closure);

    return (void *)result;
}

static void * interpreter_visitThisExpr(This *expr, void *context)
{
    Object *result = lookUpVariable(expr->keyword, (Expr *)expr, (Interpreter *)context);
    return (void *)result;
}

static void * interpreter_visitUnaryExpr(Unary *expr, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;

    Object *right = evaluate(expr->right, context);
    
    GC_LOCK(right, expr->operator);
    Object *result = NULL;
    switch (expr->operator->type)
    {
        case TT_MINUS:
        {
            checkNumberOperand(expr->operator, right, interpreter);
            result = obj_newNumber(-obj_unwrapNumber(right), interpreter->collector);
        } break;
        case TT_BANG:
        {
            result = obj_newBoolean(!isTruthy(right), interpreter->collector);
        } break;
        INVALID_DEFAULT_CASE;
    }
    gcPopLock(interpreter->collector);

    return (void *)result;
}

static void * interpreter_visitVariableExpr(Variable *expr, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    Object *value = lookUpVariable(expr->name, (Expr *)expr, interpreter);
    return (void *)value;
}

/* Stmt visitors */

static void * interpreter_visitBlockStmt(BlockStmt *stmt, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    Error *error = NULL;
    Environment *environment = env_init(interpreter->environment, &error, interpreter->collector);
    if (error)
    {
        interpreter_throwError(error, interpreter);
    }
    Return *ret = interpreter_executeBlock(stmt->statements, environment, interpreter);
    env_release(environment);

    return ret;
}

static void * interpreter_visitClassStmt(ClassStmt *stmt, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    
    Error *error = env_define(stmt->name, NULL, interpreter->environment);
    if (error)
    {
        interpreter_throwError(error, interpreter);
    }

    Object *superClass = NULL;
    Environment *closure = interpreter->environment;
    if (stmt->superClass != NULL) {
        superClass = evaluate(stmt->superClass, interpreter);
        if (!isLoxClass(superClass))
        {
            interpreter_throwNewError(stmt->name, "Superclass must be a class.", interpreter);
        }
        GC_LOCK(superClass, stmt->name);
        closure = env_init(interpreter->environment, &error, interpreter->collector);
        gcPopLock(interpreter->collector);
        if (error)
        {
            interpreter_throwError(error, interpreter);
        }
        env_defineSuper(superClass, closure);
    }
    
    MethodEntry *methods = lox_allocn(MethodEntry, LOX_METHODS_MAX_COUNT);
    if(methods == NULL)
    {
        fatal_outOfMemory();
    }
    int32_t methodsCount = 0;
    FunctionStmt *method = stmt->methods;
    while(method)
    {
        assert(method->stmt.type == STMT_Function);
        const char *methodName = get_identifier_name(method->name);
        bool isInitializer = str_isEqual(methodName, "init");
        LoxFunction *function = function_init(method, closure, isInitializer);
        methods[methodsCount].name = methodName;
        methods[methodsCount].function = function;
        ++methodsCount;
        method = (FunctionStmt *)method->stmt.next;
    }
    
    const char *name = get_identifier_name(stmt->name);
    LoxClass *klass = classInit(name, obj_unwrapClass(superClass), methods, methodsCount);
    Object *classObj = obj_wrapClass(klass, interpreter->collector);
    
    assignVariable(stmt->name, stmt, classObj, interpreter);

    return NULL;
}

static void * interpreter_visitExpressionStmt(ExpressionStmt *expr, void *context)
{
    // Note: we discard the result of the evaluation.
    evaluate(expr->expression, (Interpreter *)context);
    
    return NULL;
}

static void * interpreter_visitFunctionStmt(FunctionStmt *stmt, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    LoxFunction *function = function_init(stmt, interpreter->environment, false);
    Object *funObject = obj_wrapFunction(function, interpreter->collector);
    Error *error = env_define(stmt->name, funObject, interpreter->environment);
    if (error)
    {
        interpreter_throwError(error, interpreter);
    }

    return NULL;
}

static void * interpreter_visitIfStmt(IfStmt *stmt, void *context)
{
    Return *ret;
    
    Object *condition = evaluate(stmt->condition, (Interpreter *)context);
    if (isTruthy(condition))
    {
        ret = execute(stmt->thenBranch, (Interpreter *)context);
    }
    else if (stmt->elseBranch != NULL)
    {
        ret = execute(stmt->elseBranch, (Interpreter *)context);
    }
    else
    {
        ret = NULL;
    }
    return ret;
}

static void * interpreter_visitPrintStmt(PrintStmt *stmt, void *context)
{
    Object *value = evaluate(stmt->expression, (Interpreter *)context);
    assert(value != NULL);

    char *str = obj_stringify(value);
    printf("%s\n", str);
    str_free(str);
    
    return NULL;
}

static void * interpreter_visitReturnStmt(ReturnStmt *stmt, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    
    Object *value = NULL;
    if (stmt->value != NULL)
    {
        value = evaluate(stmt->value, interpreter);
    }

    Return *ret = return_init(value, interpreter->collector);
    return ret;
}

// If the variable has an initializer, evaluate it. If not, sets the variable to `nil`.
static void * interpreter_visitVarStmt(VarStmt *stmt, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;

    Object *value = NULL;
    if (stmt->initializer != NULL)
    {
        value = evaluate(stmt->initializer, interpreter);
    }
    Error *error = env_define(stmt->name, value, interpreter->environment);
    if (error)
    {
        interpreter_throwError(error, interpreter);
    }

    return NULL;
}

static void * interpreter_visitWhileStmt(WhileStmt *stmt, void *context)
{
    Interpreter *interpreter = (Interpreter *)context;
    Return *ret = NULL;

    while (true)
    {
        Object *condition = evaluate(stmt->condition, interpreter);
        if (isTruthy(condition))
        {
            ret = execute(stmt->body, interpreter);
            if (ret != NULL)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
    
    return ret;
}

/* Interpreter */

Interpreter * interpreter_init(bool isREPL)
{
    Interpreter *interpreter = lox_alloc(Interpreter);
    
    interpreter->collector = gcInit();

    Environment *globals = env_initGlobal(interpreter->collector);
    if (globals == NULL)
    {
        return NULL;
    }
    interpreter->globals = globals;
    interpreter->environment = interpreter->globals;

    for (uint32_t index = 0; index < LOCALS_HASH_MAP_SIZE; ++index)
    {
        interpreter->locals[index].expr = 0;
    }
    
    interpreter->localsCount = 0;

    interpreter_defineNative("clock", lox_clock, 0, interpreter);
    if (isREPL)
    {
        interpreter_defineNative("help", lox_help, 0, interpreter);
        interpreter_defineNative("quit", lox_quit, 0, interpreter);
        interpreter_defineNative("env", lox_env, 0, interpreter);
    }

    interpreter->runtimeError = NULL;
    interpreter->source = NULL;
    
    interpreter->timer = timer_init();
    
    interpreter->isREPL = isREPL;
    interpreter->exitREPL = false;

    // Initialize expression visitor
    {
        ExprVisitor *visitor = &interpreter->exprVisitor;
#define DEFINE_VISITOR(type) \
        visitor->visit##type = interpreter_visit##type##Expr;
        FOREACH_AST_NODE(DEFINE_VISITOR)
#undef DEFINE_VISITOR
    }
    
    // Initialize statement visitor
    {
    StmtVisitor *visitor = &interpreter->stmtVisitor;
#define DEFINE_VISITOR(type) \
    visitor->visit##type = interpreter_visit##type##Stmt;
        
    FOREACH_STMT_NODE(DEFINE_VISITOR)
#undef DEFINE_VISITOR
    }
    
    return interpreter;
}

void interpreter_free(Interpreter *interpreter)
{
    assert(interpreter->environment == interpreter->globals);

    if(interpreter->runtimeError)
    {
        freeError(interpreter->runtimeError);
    }
    gcFree(interpreter->collector);

    lox_free(interpreter);
}

void interpret(Stmt *statements, Interpreter *interpreter)
{
    Stmt *currentStatement = statements;
    switch (setjmp(interpreter->catchLocation))
    {
        case LOX_EXCEPTION_SETUP_LONGJMP:
        {
            while(currentStatement)
            {
#ifdef DEBUG
                Return *ret = execute(currentStatement, interpreter);
                assert(ret == NULL);
#else
                execute(currentStatement, interpreter);
#endif
                currentStatement = currentStatement->next;
            }
        } break;
            
        case LOX_EXCEPTION_RUNTIME_ERROR:
        {
            interpreter->environment = interpreter->globals;
            gcClearLocks(interpreter->collector);
            lox_runtimeError(interpreter->runtimeError);
        } break;
            
        case LOX_EXCEPTION_EXIT:
        {
            interpreter->environment = interpreter->globals;
            gcClearLocks(interpreter->collector);
        } break;
            
            INVALID_DEFAULT_CASE;
    }
}
