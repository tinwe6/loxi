//
//  main.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 12/10/2017.
//

#include <string.h>

#include "common.h"
#include "interpreter.h"
#include "parser.h"
#include "resolver.h"
#include "scanner.h"
#include "utility.h"

global bool lox_hadError_ = false;
global bool lox_hadRuntimeError_ = false;

static void lox_clearError()
{
    lox_hadError_ = false;
}

static void run(const char *source, Interpreter *interpreter)
{
    Token *tokens = scan(source);
    Stmt *statements = parse(tokens, source);
    
    // NOTE: Stop if there was a syntax error.
    if (lox_hadError_)
    {
        return;
    }
    
    interpreter->source = source;
    resolve(statements, interpreter);

    // NOTE: Stop if there was a resolution error.
    if (lox_hadError_)
    {
        return;
    }
    
    interpret(statements, interpreter);

    while(tokens)
    {
        Token *next = tokens->nextInScan;
        token_free(tokens);
        tokens = next;
    }
    freeStmt(statements);
}

void runFile(const char *filename)
{
    char *source = readFile(filename);
    if(source == NULL)
    {
        exit(LOX_EXIT_CODE_OK);
    }
    
    Interpreter *interpreter = interpreter_init(false);
    if (interpreter == NULL)
    {
        fprintf(stderr, "Fatal error: could not start the interpreter.");
        exit(LOX_EXIT_CODE_FATAL_ERROR);
    }

    run(source, interpreter);

    if (lox_hadError_)
    {
        exit(LOX_EXIT_CODE_HAD_ERROR);
    }
    if (lox_hadRuntimeError_)
    {
        exit(LOX_EXIT_CODE_HAD_RUNTIME_ERROR);
    }
    
#ifdef MEMORY_FREE_ON_EXIT
    interpreter_free(interpreter);
    str_free(source);
#endif
}

typedef struct Line_tag
{
    char *source;
    Token *tokens;
    Stmt *statements;
    int32_t line;
    struct Line_tag *next;
} Line;

Line *lineInit(char *source)
{
    Line *input = lox_alloc(Line);
    if(input == NULL)
    {
        fatal_outOfMemory();
    }
    input->source = str_fromLiteral(source);
    return input;
}

static void repl()
{
    printf("Welcome to LOXI, the Lox Interpreter\n");
    printf("Type 'help();' for help or 'quit();' to exit.\n");

    Line *lines = NULL;
    char input[REPL_MAX_INPUT_LENGTH];
    
    Interpreter *interpreter = interpreter_init(true);
    if (interpreter == NULL)
    {
        fprintf(stderr, "Fatal error: could not start the interpreter.");
        exit(LOX_EXIT_CODE_FATAL_ERROR);
    }
    
    int32_t lineNumber = 1;
    
    do {
        printf("%d> ", lineNumber);
        if (!fgets(input, REPL_MAX_INPUT_LENGTH, stdin))
        {
            printf("\n");
            break;
        }
        input[strlen(input) - 1] = '\0';
        Line *currentLine = lineInit(input);
        currentLine->line = lineNumber++;
        currentLine->next = lines;
        lines = currentLine;
        
        currentLine->tokens = scanLine(currentLine->source, currentLine->line);
        currentLine->statements = parse(currentLine->tokens, currentLine->source);
        
        // NOTE: Stop if there was a syntax error.
        if (!lox_hadError_)
        {
            
            interpreter->source = currentLine->source;
            resolve(currentLine->statements, interpreter);

            if (!lox_hadError_)
            {
                interpret(currentLine->statements, interpreter);
            }
        }

        // NOTE: We don't interrupt interactive session if an error happened
        lox_clearError();
        interpreter_clearRuntimeError(interpreter);
        
        gcCollect(interpreter->collector);
    } while (interpreter->exitREPL != true);

#ifdef MEMORY_FREE_ON_EXIT
    // NOTE: Free all lines
    Line *line = lines;
    while(line)
    {
        Token *token = line->tokens;
        while(token)
        {
            Token *next = token->nextInScan;
            token_free(token);
            token = next;
        }
        freeStmt(line->statements);
        str_free(line->source);
        
        Line *next = line->next;
        lox_free(line);
        line = next;
    }
    
    interpreter_free(interpreter);
#endif
}

int main(int argc, const char * argv[])
{
    lox_alloc_init();
    str_initPools();
    lox_clearError();
    
    if(argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(LOX_EXIT_CODE_FATAL_ERROR);
    }

#ifdef MEMORY_FREE_ON_EXIT
    str_freePools();
#endif
    
#ifdef MEMORY_DEBUG
    alloc_printActiveDB();
    printLeakedPointers();
#endif
    
    return LOX_EXIT_CODE_OK;
}
