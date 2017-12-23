//
//  parser.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#include "parser.h"
#include "common.h"
#include "error.h"
#include "expr.h"
#include "error.h"
#include "expr.h"

typedef struct Parser
{
    Token *tokens;
    Token *current;
    Token *previous;
    
    const char *source;
    
    Error *error;
} Parser;

typedef struct
{
    Expr *expr;
    bool error;
} ParseResult;

/* Recursive descent parser */

static Parser * parser_init(Token *tokens, const char *source)
{
    Parser *parser = lox_alloc(Parser);
    
    parser->tokens = tokens;
    parser->current = tokens;
    parser->previous = NULL;
    
    parser->source = source;
    
    parser->error = NULL;
    
    return parser;
}

static void parser_free(Parser *parser)
{
    if (parser->error)
    {
        freeError(parser->error);
    }
    
    lox_free(parser);
}

// Returns the previous token
static inline Token * parser_previous(Parser *parser)
{
    return parser->previous;
}

// Returns the current token without consuming it
static inline Token * parser_peek(Parser *parser)
{
    return parser->current;
}

// True if all tokens have been consumed
static inline bool parser_isAtEnd(Parser *parser)
{
    return parser_peek(parser)->type == TT_EOF;
}

// Consume a token and return it
static inline Token * parser_advance(Parser *parser)
{
    if (!parser_isAtEnd(parser))
    {
        parser->previous = parser->current;
        parser->current = parser->current->nextInScan;
    }
    return parser_previous(parser);
}

// Returns true if the current token is of the given type, without consuming it.
static inline bool parser_check(Parser *parser, TokenType type)
{
    if (parser_isAtEnd(parser))
    {
        return false;
    }
    return parser_peek(parser)->type == type;
}

// Consumes the current token and returns true if it is of type1 or type2
static bool parser_match2(Parser *parser, TokenType type1, TokenType type2)
{
    if(parser_check(parser, type1) || parser_check(parser, type2))
    {
        parser_advance(parser);
        return true;
    }
    return false;
}

// Consumes the current token and returns true if it is an operator of type 'type'
static bool parser_match1(Parser *parser, TokenType type)
{
    if(parser_check(parser, type))
    {
        parser_advance(parser);
        return true;
    }
    return false;
}

#define parser_match(parser, array) parser_match_(parser, array, ARRAY_COUNT(array))
static bool parser_match_(Parser *parser, const TokenType *types, int32_t count)
{
    for (int32_t index = 0; index < count; index++)
    {
        if (parser_check(parser, types[index])) {
            parser_advance(parser);
            return true;
        }
    }
    return false;
}

//

static void parser_synchronize(Parser *parser)
{
    parser_advance(parser);
    
    while (!parser_isAtEnd(parser))
    {
        if (parser_previous(parser)->type == TT_SEMICOLON)
        {
            return;
        }
        
        switch (parser_peek(parser)->type)
        {
            case TT_CLASS:
            case TT_FUN:
            case TT_VAR:
            case TT_FOR:
            case TT_IF:
            case TT_WHILE:
            case TT_PRINT:
            case TT_RETURN:
                return;
            default:
                break;
        }
        
        parser_advance(parser);
    }
}

static inline Error * parser_throwError(Token *token, const char *message, Parser *parser)
{
    lox_token_error(parser->source, token, message);

    // NOTE: we only keep track of the last error that occurred for now.
    if(parser->error)
    {
        freeError(parser->error);
    }
    parser->error = initError(token, message);
    return parser->error;
}

// Consumes and returns the current token if it is of type `type`; otherwise, set a parse error and return NULL.
static Token * parser_consume(Parser *parser, TokenType type, const char *message)
{
    if (parser_check(parser, type))
    {
        return parser_advance(parser);
    }
    parser_throwError(parser_peek(parser), message, parser);

    return NULL;
}

static Expr * parse_expression(Parser *parser);
// NOTE: primary → "true" | "false" | "null" | "nil"
//               | NUMBER | STRING
//               | "(" expression ")"
//               | IDENTIFIER ;
static Expr * parse_primary(Parser *parser)
{
    Expr *result = NULL;
    
    if (parser_match1(parser, TT_FALSE))
    {
        result = init_bool_literal(false);
    }
    else if (parser_match1(parser, TT_TRUE))
    {
        result = init_bool_literal(true);
    }
    else if (parser_match1(parser, TT_NIL))
    {
        result = init_nil_literal();
    }
    else if (parser_match(parser, ((TokenType[]){TT_NUMBER, TT_STRING})))
    {
        result = init_literal(parser_previous(parser));
    }
    else if (parser_match1(parser, TT_SUPER))
    {
        Token *keyword = parser_previous(parser);
        if (!parser_consume(parser, TT_DOT, "Expect '.' after 'super'."))
        {
            return NULL;
        }
        Token *method = parser_consume(parser, TT_IDENTIFIER,
                                       "Expect superclass method name.");
        result = init_super(keyword, method);
    }
    else if (parser_match1(parser, TT_THIS))
    {
        result = init_this(parser_previous(parser));
    }
    else if (parser_match1(parser, TT_IDENTIFIER))
    {
        result = init_variable(parser_previous(parser));
    }
    else if (parser_match1(parser, TT_LEFT_PAREN))
    {
        Expr *expr = parse_expression(parser);
        if(expr == NULL)
        {
            free_expr(expr);
        }
        else
        {
            if (!parser_consume(parser, TT_RIGHT_PAREN, "Expect ')' after expression."))
            {
                free_expr(expr);
            }
            else
            {
                result = init_grouping(expr);
            }
        }
    }
    else
    {
        parser_throwError(parser_peek(parser), "Expect expression.", parser);
    }
    
    return result;
}

static Expr * parse_call(Parser *parser);
// NOTE: unary → ( "!" | "-" ) unary | call ;
static Expr * parse_unary(Parser *parser)
{
    Expr *result;
    
    if (parser_match(parser, ((TokenType[]){TT_BANG, TT_MINUS})))
    {
        Token *operator = parser_previous(parser);
        Expr *right = parse_unary(parser);
        if(right)
        {
            result = init_unary(operator, right);
        }
        else
        {
            result = NULL;
        }
    }
    else
    {
        result = parse_call(parser);
    }
    return result;
}

static Expr * parse_finishCall(Expr *callee, Parser *parser)
{
    Expr *arguments = NULL;
    if (!parser_check(parser, TT_RIGHT_PAREN))
    {
        do {
            if (expr_count(arguments) >= LOX_MAX_ARG_COUNT)
            {
                // NOTE: reports an error but does not throw it; the parser is still in a valid state
                parser_throwError(parser_peek(parser), "Cannot have more than " XSTR(LOX_MAX_ARG_COUNT) " arguments.", parser);
            }
            Expr *expr = parse_expression(parser);
            if (expr == NULL)
            {
                free_expr(arguments);
                free_expr(callee);
                return NULL;
            }
            arguments = expr_appendTo(arguments, expr);
        } while (parser_match1(parser, TT_COMMA));
    }
    
    Token *paren = parser_consume(parser, TT_RIGHT_PAREN, "Expect ')' after arguments.");
    if (paren == NULL)
    {
        free_expr(arguments);
        free_expr(callee);
        return NULL;
    }
    
    Expr *call = init_call(callee, paren, arguments);
    return (void *)call;
}

// NOTE:  call → primary ( "(" arguments? ")" | "." IDENTIFIER )* ;
static Expr * parse_call(Parser *parser)
{
    Expr *expr = parse_primary(parser);
    if (expr == NULL)
    {
        return NULL;
    }
    
    while (true)
    {
        if (parser_match1(parser, TT_LEFT_PAREN))
        {
            expr = parse_finishCall(expr, parser);
        }
        else if (parser_match1(parser, TT_DOT))
        {
            Token *name = parser_consume(parser, TT_IDENTIFIER, "Expect property name after '.'.");
            if (name == NULL)
            {
                free_expr(expr);
                return NULL;
            }
            expr = init_get(expr, name);
        }
        else
        {
            break;
        }
    }
    
    return expr;
}

// NOTE: multiplication -> unary ( ( "/" | "*" ) unary )* ;
static Expr * parse_multiplication(Parser *parser)
{
    Expr *expr = parse_unary(parser);
    if(expr == NULL)
    {
        return NULL;
    }
    
    while (parser_match(parser, ((TokenType[]){TT_SLASH, TT_STAR})))
    {
        Token *operator = parser_previous(parser);
        Expr *right = parse_unary(parser);
        if(right == NULL)
        {
            free_expr(expr);
            return NULL;
        }
        expr = init_binary(expr, operator, right);
    }
    
    return expr;
}

// NOTE: addition -> multiplication ( ( "-" | "+" ) multiplication )* ;
static Expr * parse_addition(Parser *parser)
{
    Expr *expr = parse_multiplication(parser);
    if(expr == NULL)
    {
        return NULL;
    }

    while (parser_match(parser, ((TokenType[]){TT_MINUS, TT_PLUS})))
    {
        Token *operator = parser_previous(parser);
        Expr *right = parse_multiplication(parser);
        if(right == NULL)
        {
            free_expr(expr);
            return NULL;
        }
        expr = init_binary(expr, operator, right);
    }
    
    return expr;
}

// NOTE: comparison -> addition ( ( ">" | ">=" | "<" | "<=" ) addition )* ;
static Expr * parse_comparison(Parser *parser) {
    Expr *expr = parse_addition(parser);
    if(expr == NULL)
    {
        return NULL;
    }

    const TokenType tokens[] = {TT_GREATER, TT_GREATER_EQUAL, TT_LESS, TT_LESS_EQUAL};
    while (parser_match(parser, tokens))
    {
        Token *operator = parser_previous(parser);
        Expr *right = parse_addition(parser);
        if(right == NULL)
        {
            free_expr(expr);
            return NULL;
        }
        expr = init_binary(expr, operator, right);
    }
    
    return expr;
}


// NOTE: equality -> comparison ( ( "!=" | "==" ) comparison )* ;
static Expr * parse_equality(Parser *parser)
{
    Expr *expr = parse_comparison(parser);
    if(expr == NULL)
    {
        return NULL;
    }

    while (parser_match2(parser, TT_BANG_EQUAL, TT_EQUAL_EQUAL))
    {
        Token *operator = parser_previous(parser);
        Expr *right = parse_comparison(parser);
        if(right == NULL)
        {
            free_expr(expr);
            return NULL;
        }
        expr = init_binary(expr, operator, right);
    }
    
    return expr;
}

// NOTE: logic_and → equality ( "and" equality )* ;
static Expr * parse_and(Parser *parser)
{
    Expr *expr = parse_equality(parser);
    if(expr == NULL)
    {
        return NULL;
    }
    
    while (parser_match1(parser, TT_AND))
    {
        Token *operator = parser_previous(parser);
        Expr *right = parse_equality(parser);
        if(right == NULL)
        {
            free_expr(expr);
            return NULL;
        }
        expr = init_logical(expr, operator, right);
    }
    
    return expr;
}

// NOTE: logic_or → logic_and ( "or" logic_and )* ;
static Expr * parse_or(Parser *parser)
{
    Expr *expr = parse_and(parser);
    if(expr == NULL)
    {
        return NULL;
    }
    
    while (parser_match1(parser, TT_OR))
    {
        Token *operator = parser_previous(parser);
        Expr *right = parse_and(parser);
        if(right == NULL)
        {
            free_expr(expr);
            return NULL;
        }
        expr = init_logical(expr, operator, right);
    }
    
    return expr;
}

// assignment → ( call "." )? IDENTIFIER "=" assignment | logic_or;
static Expr * parse_assignment(Parser *parser)
{
    Expr *expr = parse_or(parser);
//    Expr *expr = parse_equality(parser);
    
    if (parser_match1(parser, TT_EQUAL))
    {
        Token *equals = parser_previous(parser);
        Expr *value = parse_assignment(parser);
        
        if (expr->type == EXPR_Variable)
        {
            Variable *varExpr = (Variable *)expr;
            Token *name = varExpr->name;
            lox_free(varExpr);
            return init_assign(name, value);
        }
        else if (expr->type == EXPR_Get)
        {
            Get *getExpr = (Get *)expr;
            Expr *setExpr = init_set(getExpr->object, getExpr->name, value);
            lox_free(getExpr);
            return setExpr;
        }
        parser_throwError(equals, "Invalid assignment target.", parser);
    }
    
    return expr;
}

// NOTE: expression -> assignment ;
static Expr * parse_expression(Parser *parser)
{
    return parse_assignment(parser);
}

// NOTE: exprStmt  → expression ";" ;
static Stmt * parse_expressionStatement(Parser *parser)
{
    Expr *expr = parse_expression(parser);
    if(expr == NULL)
    {
        return NULL;
    }

    if(!parser_consume(parser, TT_SEMICOLON, "Expect ';' after expression."))
    {
        free_expr(expr);
        return NULL;
    }

    Stmt *result = initExpression(expr);
    return result;
}

static Stmt * parse_declaration(Parser *parser);
// Parses a block, starting from the token following the opening brace.
// If the block is parsed without errors, the closing brace is consumed,
// and the list of statements found in the block is returned.
static Stmt * parse_block(Parser *parser)
{
    Stmt *first = NULL;
    Stmt *last = NULL;

    while (!parser_check(parser, TT_RIGHT_BRACE) && !parser_isAtEnd(parser))
    {
        Stmt *statement = parse_declaration(parser);
        if (statement == NULL)
        {
            // NOTE: There was an error in the statement. We skip to the next statement
            continue;
        }
        if(last)
        {
            last->next = statement;
        }
        else
        {
            first = statement;
        }
        last = statement;
        assert(last->next == NULL);
    }
    
    if (!parser_consume(parser, TT_RIGHT_BRACE, "Expect '}' after block."))
    {
        freeStmt(first);
        return NULL;
    }

    return first;
}

// NOTE: printStmt → "print" expression ";" ;
static Stmt * parse_printStatement(Parser *parser)
{
    Expr *value = parse_expression(parser);
    if(value == NULL)
    {
        return NULL;
    }
    if (!parser_consume(parser, TT_SEMICOLON, "Expect ';' after value."))
    {
        free_expr(value);
        return NULL;
    }
    
    Stmt *result = initPrint(value);
    return result;
}

// NOTE: returnStmt → "return" expression? ";" ;
static Stmt * parse_returnStatement(Parser *parser)
{
    Token *keyword = parser_previous(parser);

    Expr *value = NULL;
    if (!parser_check(parser, TT_SEMICOLON))
    {
        value = parse_expression(parser);
    }

    if (!parser_consume(parser, TT_SEMICOLON, "Expect ';' after value."))
    {
        free_expr(value);
        return NULL;
    }
    
    Stmt *result = initReturn(keyword, value);
    return result;
}

static Stmt * parse_forStatement(Parser *parser);
static Stmt * parse_ifStatement(Parser *parser);
static Stmt * parse_whileStatement(Parser *parser);
// NOTE:  statement → exprStmt | ifStmt | printStmt | returnStmt | whileStmt | block ;
static Stmt * parse_statement(Parser *parser)
{
    if (parser_match1(parser, TT_FOR))
    {
        return parse_forStatement(parser);
    }
    if (parser_match1(parser, TT_IF))
    {
        return parse_ifStatement(parser);
    }
    if (parser_match1(parser, TT_PRINT))
    {
        return parse_printStatement(parser);
    }
    if (parser_match1(parser, TT_RETURN))
    {
        return parse_returnStatement(parser);
    }
    if (parser_match1(parser, TT_WHILE))
    {
        return parse_whileStatement(parser);
    }
    if (parser_match1(parser, TT_LEFT_BRACE))
    {
        return initBlock(parse_block(parser));
    }
    return parse_expressionStatement(parser);
}

static Stmt * parse_varDeclaration(Parser *parser);
// NOTE: for loop desugaring into while loop.
static Stmt * parse_forStatement(Parser *parser)
{
    if (!parser_consume(parser, TT_LEFT_PAREN, "Expect '(' after 'for'."))
    {
        return NULL;
    }
    
    Stmt *initializer;
    if (parser_match1(parser, TT_SEMICOLON))
    {
        initializer = NULL;
    }
    else
    {
        if (parser_match1(parser, TT_VAR))
        {
            initializer = parse_varDeclaration(parser);
        }
        else
        {
            initializer = parse_expressionStatement(parser);
        }
        if (initializer == NULL)
        {
            return NULL;
        }
    }
    
    Expr *condition = NULL;
    if (!parser_check(parser, TT_SEMICOLON))
    {
        condition = parse_expression(parser);
        if (condition == NULL)
        {
            freeStmt(initializer);
            return NULL;
        }
    }
    if (!parser_consume(parser, TT_SEMICOLON, "Expect ';' after loop condition."))
    {
        freeStmt(initializer);
        free_expr(condition);
        return NULL;
    }
    
    Expr *increment = NULL;
    if (!parser_check(parser, TT_RIGHT_PAREN))
    {
        increment = parse_expression(parser);
        if (increment == NULL)
        {
            freeStmt(initializer);
            free_expr(condition);
            return NULL;
        }
    }
    if (!parser_consume(parser, TT_RIGHT_PAREN, "Expect ')' after for clauses."))
    {
        freeStmt(initializer);
        free_expr(condition);
        free_expr(increment);
        return NULL;
    }
    
    Stmt *body = parse_statement(parser);
    
    if (increment != NULL)
    {
        body = initBlock(stmt_appendTo(body, initExpression(increment)));
    }
    
    if (condition == NULL)
    {
        condition = init_bool_literal(true);
    }
    
    body = initWhile(condition, body);
    
    if (initializer != NULL)
    {
        body = initBlock(stmt_appendTo(initializer, body));
    }
    
    return body;
}

// NOTE: ifStmt → "if" "(" expression ")" statement ( "else" statement )? ;
// NOTE: To solve the dangling else ambiguity, we bind the `else` to the nearest `if` that precedes it.
static Stmt * parse_ifStatement(Parser *parser)
{
    if(!parser_consume(parser, TT_LEFT_PAREN, "Expect '(' after 'if'."))
    {
        return NULL;
    }

    Expr *condition = parse_expression(parser);
    if (condition == NULL)
    {
        return NULL;
    }
    
    if (!parser_consume(parser, TT_RIGHT_PAREN, "Expect ')' after if condition."))
    {
        free_expr(condition);
        return NULL;
    }

    Stmt *thenBranch = parse_statement(parser);
    if(thenBranch == NULL)
    {
        free_expr(condition);
        return NULL;
    }

    Stmt *elseBranch = NULL;
    if (parser_match1(parser, TT_ELSE))
    {
        elseBranch = parse_statement(parser);
        if(elseBranch == NULL)
        {
            free_expr(condition);
            freeStmt(thenBranch);
            return NULL;
        }
    }
    
    Stmt *result = initIf(condition, thenBranch, elseBranch);
    return result;
}

static Stmt * parse_varDeclaration(Parser *parser)
{
    Token *name = parser_consume(parser, TT_IDENTIFIER, "Expect variable name.");
    if (name == NULL)
    {
        return NULL;
    }
    Expr *initializer = NULL;
    if (parser_match1(parser, TT_EQUAL))
    {
        initializer = parse_expression(parser);
        if (initializer == NULL)
        {
            return NULL;
        }
    }
    
    if (!parser_consume(parser, TT_SEMICOLON, "Expect ';' after variable declaration."))
    {
        free_expr(initializer);
        return NULL;
    }

    Stmt *result = initVar(name, initializer);
    return result;
}

static Stmt * parse_whileStatement(Parser *parser)
{
    if (!parser_consume(parser, TT_LEFT_PAREN, "Expect '(' after 'while'."))
    {
        return NULL;
    }
    
    Expr *condition = parse_expression(parser);
    if (condition == NULL)
    {
        return NULL;
    }
    
    if (!parser_consume(parser, TT_RIGHT_PAREN, "Expect ')' after if condition."))
    {
        free_expr(condition);
        return NULL;
    }

    Stmt *body = parse_statement(parser);
    if (body == NULL)
    {
        free_expr(condition);
        return NULL;
    }

    Stmt *result = initWhile(condition, body);
    return (void *)result;
}

typedef enum
{
    FK_Function = 0,
    FK_Method,
    // NOTE: FK_Count must be the last entry
    FK_Count,
} FunctionKind;

static char *FKErrorName = {"Expect function name."};
static char *FKErrorParen = {"Expect '(' after function name."};
static char *FKErrorBody = {"Expect '{' before function body."};

//  function    → IDENTIFIER "(" parameters? ")" block ;
static Stmt * parse_function(Parser *parser, FunctionKind kind)
{
    assert(kind < FK_Count);
    Token *name = parser_consume(parser, TT_IDENTIFIER, FKErrorName);
    if(name == NULL)
    {
        return NULL;
    }
    if(!parser_consume(parser, TT_LEFT_PAREN, FKErrorParen))
    {
        return NULL;
    }

    Token **parameters = lox_allocn(Token*, LOX_MAX_ARG_COUNT);
    if(parameters == NULL)
    {
        fatal_outOfMemory();
    }
    int32_t parametersCount = 0;
    if (!parser_check(parser, TT_RIGHT_PAREN))
    {
        // The function has at least one parameter
        do {
            if (parametersCount >= LOX_MAX_ARG_COUNT)
            {
                parser_throwError(parser_peek(parser), "Cannot have more than " XSTR(LOX_MAX_ARG_COUNT) " parameters.", parser);
                lox_free(parameters);
                return NULL;
            }
            
            Token *identifier = parser_consume(parser, TT_IDENTIFIER, "Expect parameter name.");
            if(identifier == NULL)
            {
                return NULL;
            }
            
            assert(parametersCount < LOX_MAX_ARG_COUNT);
            parameters[parametersCount++] = identifier;

        } while (parser_match1(parser, TT_COMMA));
    }
    if(!parser_consume(parser, TT_RIGHT_PAREN, "Expect ')' after parameters."))
    {
        lox_free(parameters);
        return NULL;
    }

    if(!parser_consume(parser, TT_LEFT_BRACE, FKErrorBody))
    {
        lox_free(parameters);
        return NULL;
    }

    Stmt *body = parse_block(parser);
    // NOTE: if block is NULL, the function has an empty body. This is allowed.

    Stmt *function = initFunction(name, parameters, parametersCount, body);
    return function;
}

// classDecl → "class" IDENTIFIER ( "<" IDENTIFIER )? "{" function* "}" ;
static Stmt * parse_classDeclaration(Parser *parser)
{
    Token *name = parser_consume(parser, TT_IDENTIFIER, "Expect class name.");
    
    Expr *superclass = NULL;
    if (parser_match1(parser, TT_LESS)) {
        if (!parser_consume(parser, TT_IDENTIFIER, "Expect superclass name."))
        {
            return NULL;
        }
        superclass = init_variable(parser_previous(parser));
    }
    
    if ((name == NULL) ||
        (!parser_consume(parser, TT_LEFT_BRACE, "Expect '{' before class body.")))
    {
        return NULL;
    }
    
    Stmt *methods = NULL;
    while (!parser_check(parser, TT_RIGHT_BRACE) && !parser_isAtEnd(parser))
    {
        Stmt *method = parse_function(parser, FK_Method);
        if (method == NULL)
        {
            freeStmt(methods);
            return NULL;
        }
        method->next = methods;
        methods = method;
    }

    if(!parser_consume(parser, TT_RIGHT_BRACE, "Expect '}' after class body."))
    {
        return NULL;
    }
    
    Stmt *classStmt = initClass(name, superclass, (FunctionStmt *)methods);
    return classStmt;
}

// NOTE:  declaration → classDecl | funDecl | varDecl | statement ;
static Stmt * parse_declaration(Parser *parser)
{
    Stmt *result;
    if (parser_match1(parser, TT_CLASS))
    {
        result = parse_classDeclaration(parser);
    }
    else if (parser_match1(parser, TT_FUN))
    {
        result = parse_function(parser, FK_Function);
    }
    else if (parser_match1(parser, TT_VAR))
    {
        result = parse_varDeclaration(parser);
    }
    else
    {
        result = parse_statement(parser);
    }

    if(result == NULL)
    {
        parser_synchronize(parser);
    }
    
    return result;
}

// NOTE: program   → statement* EOF ;
static Stmt * parse_program(Parser *parser)
{
    Stmt *firstStmt = NULL;
    Stmt *lastStmt = NULL;
    while(!parser_isAtEnd(parser))
    {
        Stmt *statement = parse_declaration(parser);
        if (statement == NULL)
        {
            continue;
        }
        if(lastStmt)
        {
            lastStmt->next = statement;
            lastStmt = statement;
        }
        else
        {
            firstStmt = statement;
            lastStmt = statement;
        }
        assert(lastStmt->next == NULL);
    }
    return firstStmt;
}

Stmt * parse(Token *tokens, const char *source)
{
    Parser *parser = parser_init(tokens, source);
    Stmt *statements = parse_program(parser);
    parser_free(parser);
    
    return statements;
}
