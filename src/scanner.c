//
//  scanner.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 16/10/2017.
//

#include "scanner.h"

#include "error.h"
#include "utility.h"

typedef struct Scanner
{
    const char *source;
    
    Token *first_token;
    Token *last_token;
    
    str_size start;
    str_size current;
    str_size end;
    
    int line;
} Scanner;

static Scanner * scanner_init(const char *source, int32_t firstLineNum)
{
    Scanner *scanner = lox_alloc(Scanner);
    if(scanner == NULL)
    {
        fatal_outOfMemory();
    }
    scanner->source = source;
    
    scanner->first_token = NULL;
    scanner->last_token = NULL;
    
    scanner->start = 0;
    scanner->current = 0;
    assert(str_calculateLength(source) == str_length(source));
    scanner->end = str_length(source);
    
    // NOTE: internally the first line is num 0.
    scanner->line = firstLineNum - 1;
    
    return scanner;
}

static inline bool scanner_is_at_end(const Scanner *scanner)
{
    return scanner->current >= scanner->end;
}

static inline Lexeme scanner_current_lexeme(const Scanner *scanner)
{
    Lexeme result;
    result.index = substringStartEnd(scanner->start, scanner->current);
    result.line = scanner->line;
    return result;
}

static inline char scanner_advance(Scanner *scanner)
{
    char result = scanner->source[scanner->current];
    scanner->current++;
    
    return result;
}

static inline bool scanner_match(Scanner *scanner, char expected)
{
    if(scanner_is_at_end(scanner) || scanner->source[scanner->current] != expected)
    {
        return false;
    }
    scanner->current++;
    return true;
}

// NOTE: returns 0 if we are past the end of the source.
static inline char scanner_peek(const Scanner *scanner)
{
    if(scanner_is_at_end(scanner))
    {
        return '\0';
    }
    return scanner->source[scanner->current];
}

static inline char scanner_peek_next(const Scanner *scanner)
{
    size_t index_next = scanner->current + 1;
    if(index_next >= scanner->end)
    {
        return '\0';
    }
    return scanner->source[index_next];
}

static void scanner_add_token(Scanner *scanner, Token *token)
{
    if(scanner->first_token == NULL)
    {
        scanner->first_token = token;
        scanner->last_token = token;
    }
    else
    {
        scanner->last_token->nextInScan = token;
        scanner->last_token = token;
    }
}

static void scanner_atomic(Scanner *scanner, TokenType type)
{
    Token *token = token_atomic(type, scanner_current_lexeme(scanner));
    scanner_add_token(scanner, token);
}

static void scanner_number(Scanner *scanner)
{
    while(is_digit(scanner_peek(scanner)))
    {
        scanner_advance(scanner);
    }
    
    // Look for a fractional part.
    if (scanner_peek(scanner) == '.' && is_digit(scanner_peek_next(scanner)))
    {
        // Consume the '.'
        scanner_advance(scanner);
        
        while (is_digit(scanner_peek(scanner)))
        {
            scanner_advance(scanner);
        }
    }
    
    char *text = str_substring(scanner->source, substringStartEnd(scanner->start, scanner->current));
    double value = atof(text);
    str_free(text);
    
    Token *token = token_number_literal(value, scanner_current_lexeme(scanner));
    scanner_add_token(scanner, token);
}

static void scanner_string(Scanner *scanner)
{
    while (scanner_peek(scanner) != '\"' && !scanner_is_at_end(scanner))
    {
        if (scanner_peek(scanner) == '\n')
        {
            scanner->line++;
        }
        scanner_advance(scanner);
    }
    
    // Unterminated string.
    if (scanner_is_at_end(scanner))
    {
        lox_error(scanner->line, "Unterminated string.");
    }
    else
    {
        // The closing '"'
        scanner_advance(scanner);
        
        Lexeme lexeme = scanner_current_lexeme(scanner);
        // Trim the surrounding quotes.
        SubstringIndex index = substring_trimmed(lexeme.index);
        char *value = str_substring(scanner->source, index);
        
        Token *token = token_string_literal(value, lexeme);
        str_free(value);
        
        scanner_add_token(scanner, token);
    }
}

static void scanner_identifier(Scanner *scanner)
{
    while(is_alphanumeric(scanner_peek(scanner)))
    {
        scanner_advance(scanner);
    }
    
    // See if the identifier is a reserved word.
    Lexeme lexeme = scanner_current_lexeme(scanner);
    char *str = str_substring(scanner->source, lexeme.index);
    KeywordEntry *entry = lookup_keyword(str);
    
    if(keyword_is_valid(entry))
    {
        scanner_atomic(scanner, entry->type);
    }
    else
    {
        Token *token = token_identifier(str, lexeme);
        scanner_add_token(scanner, token);
    }
    str_free(str);
}

static void scanner_scanToken(Scanner *scanner)
{
    char c = scanner_advance(scanner);
    
    switch (c) {
        case '(': { scanner_atomic(scanner, TT_LEFT_PAREN); } break;
        case ')': { scanner_atomic(scanner, TT_RIGHT_PAREN); } break;
        case '{': { scanner_atomic(scanner, TT_LEFT_BRACE); } break;
        case '}': { scanner_atomic(scanner, TT_RIGHT_BRACE); } break;
        case ',': { scanner_atomic(scanner, TT_COMMA); } break;
        case '.': { scanner_atomic(scanner, TT_DOT); } break;
        case '-': { scanner_atomic(scanner, TT_MINUS); } break;
        case '+': { scanner_atomic(scanner, TT_PLUS); } break;
        case ';': { scanner_atomic(scanner, TT_SEMICOLON); } break;
        case '*': { scanner_atomic(scanner, TT_STAR); } break;
        case '!': {
            scanner_atomic(scanner,
                           scanner_match(scanner, '=') ? TT_BANG_EQUAL : TT_BANG);
        } break;
        case '=': {
            scanner_atomic(scanner,
                           scanner_match(scanner, '=') ? TT_EQUAL_EQUAL : TT_EQUAL);
        } break;
        case '<': {
            scanner_atomic(scanner,
                           scanner_match(scanner, '=') ? TT_LESS_EQUAL : TT_LESS);
        } break;
        case '>': {
            scanner_atomic(scanner,
                           scanner_match(scanner, '=') ? TT_GREATER_EQUAL : TT_GREATER);
        } break;
            
        case '/':
        {
            if (scanner_match(scanner, '/'))
            {
                // A comment goes until the end of the line.
                while ((scanner_peek(scanner) != '\n') && !scanner_is_at_end(scanner))
                {
                    scanner_advance(scanner);
                }
            }
            else if(scanner_match(scanner, '*'))
            {
                // Start c-like comment
                int level = 1;
                while ((level > 0) && !scanner_is_at_end(scanner))
                {
                    if((scanner_peek(scanner) == '*') && (scanner_peek_next(scanner) == '/'))
                    {
                        level--;
                        scanner_advance(scanner);
                    }
                    else if ((scanner_peek(scanner) == '/') && (scanner_peek_next(scanner) == '*'))
                    {
                        level++;
                        scanner_advance(scanner);
                    }
                    scanner_advance(scanner);
                }
                if(level > 0)
                {
                    lox_error(scanner->line, "Unterminated /* comment.");
                }
            }
            else
            {
                scanner_atomic(scanner, TT_SLASH);
            }
        } break;
            
        case ' ':
        case '\r':
        case '\t':
        {
            // Ignore whitespace.
        } break;
            
        case '\n':
        {
            scanner->line++;
        } break;
            
        case '\"':
        {
            scanner_string(scanner);
        } break;
            
        default:
        {
            if(is_digit(c))
            {
                scanner_number(scanner);
            }
            else if(is_alpha(c))
            {
                scanner_identifier(scanner);
            }
            else
            {
                lox_error(scanner->line, "Unexpected character.");
            }
        }
    }
}

static Token * scan_tokens(Scanner *scanner)
{
    while (!scanner_is_at_end(scanner))
    {
        // We are the beginning of the next lexeme.
        scanner->start = scanner->current;
        scanner_scanToken(scanner);
    }
    
    Token *eof = token_atomic(TT_EOF, scanner_current_lexeme(scanner));
    scanner_add_token(scanner, eof);
    
    return scanner->first_token;
}

Token * scanLine(const char *source, int32_t lineNumber)
{
    Scanner *scanner = scanner_init(source, lineNumber);
    Token *tokens = scan_tokens(scanner);
    lox_free(scanner);
    return tokens;
}

Token * scan(const char *source)
{
    return scanLine(source, 1);
}

