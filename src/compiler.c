#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

typedef struct {
    token_t current;
    token_t previous;
    bool had_error;
    bool panic_mode;
} parser_t;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + - 
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! - +
    PREC_CALL,       // . () []
    PREC_PRIMARY
} precedence_t;

typedef void(*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    precedence_t precedence;
} parse_rule_t;

parser_t parser;
chunk_t* compiling_chunk;

static chunk_t* current_chunk()
{
    return compiling_chunk;
}

static void error_at(token_t* token, const char* message)
{
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
        fprintf(stderr, " at end");
    else if (token->type != TOKEN_ERROR)
        fprintf(stderr, "at '%.*s'", token->length, token->start);

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char* message)
{
    error_at(&parser.previous, message);
}

static void error_at_current(const char* message)
{
    if (parser.panic_mode)
        return;
    parser.panic_mode = true;
    error_at(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;

    for (;;)
    {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR)
            break;

        error_at_current(parser.current.start);
    }
}

static void consume(token_type_t type, const char* message)
{
    if (parser.current.type == type)
    {
        advance();
        return;
    }

    error_at_current(message);
}

static void emit_byte(uint8_t byte)
{
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2)
{
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_return()
{
    emit_byte(OP_RETURN);
}

static uint8_t make_constant(value_t value)
{
    int constant = add_constant(current_chunk(), value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

static void emit_constant(value_t value)
{
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void end_compiler()
{
    emit_return();
}

static void expression();
static parse_rule_t* get_rule(token_type_t type);
static void parse_precedence(precedence_t precedence);

static void binary()
{
    token_type_t operatorType = parser.previous.type;

    parse_rule_t* rule = get_rule(operatorType);
    parse_precedence((precedence_t)(rule->precedence + 1));

    switch (operatorType)
    {
    case TOKEN_PLUS:  emit_byte(OP_ADD); break;
    case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
    case TOKEN_STAR:  emit_byte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
    default:
        return;
    }
}

static void grouping()
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "expected ')' after expression.");
}

static void number()
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(value);
}

static void unary()
{
    token_type_t operatorType = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (operatorType)
    {
    case TOKEN_MINUS:
        emit_byte(OP_NEGATE);
        break;
    default:
        return;
    }
}

parse_rule_t rules[] = {
    { grouping, NULL,    PREC_CALL },       // TOKEN_LEFT_PAREN
    { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN
    { NULL,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA
    { NULL,     NULL,    PREC_CALL },       // TOKEN_DOT
    { unary,    binary,  PREC_TERM },       // TOKEN_MINUS
    { NULL,     binary,  PREC_TERM },       // TOKEN_PLUS
    { NULL,     NULL,    PREC_NONE },       // TOKEN_SEMICOLON
    { NULL,     binary,  PREC_FACTOR },     // TOKEN_SLASH
    { NULL,     binary,  PREC_FACTOR },     // TOKEN_STAR
    { NULL,     NULL,    PREC_NONE },       // TOKEN_BANG
    { NULL,     NULL,    PREC_EQUALITY },   // TOKEN_BANG_EQUAL
    { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL
    { NULL,     NULL,    PREC_EQUALITY },   // TOKEN_EQUAL_EQUAL
    { NULL,     NULL,    PREC_COMPARISON }, // TOKEN_GREATER
    { NULL,     NULL,    PREC_COMPARISON }, // TOKEN_GREATER_EQUAL
    { NULL,     NULL,    PREC_COMPARISON }, // TOKEN_LESS
    { NULL,     NULL,    PREC_COMPARISON }, // TOKEN_LESS_EQUAL
    { NULL,     NULL,    PREC_NONE },       // TOKEN_IDENTIFIER
    { NULL,     NULL,    PREC_NONE },       // TOKEN_STRING
    { number,   NULL,    PREC_NONE },       // TOKEN_NUMBER
    { NULL,     NULL,    PREC_AND },        // TOKEN_AND
    { NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS
    { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_FALSE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_FUN
    { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR
    { NULL,     NULL,    PREC_NONE },       // TOKEN_IF
    { NULL,     NULL,    PREC_NONE },       // TOKEN_NIL
    { NULL,     NULL,    PREC_OR },         // TOKEN_OR
    { NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT
    { NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN
    { NULL,     NULL,    PREC_NONE },       // TOKEN_SUPER
    { NULL,     NULL,    PREC_NONE },       // TOKEN_THIS
    { NULL,     NULL,    PREC_NONE },       // TOKEN_TRUE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_VAR
    { NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR
    { NULL,     NULL,    PREC_NONE },       // TOKEN_EOF
};

static void parse_precedence(precedence_t precedence)
{
    advance();
    ParseFn prefixRule = get_rule(parser.previous.type)->prefix;
    if (!prefixRule)
    {
        error("expected expression.");
        return;
    }
    prefixRule();

    while (precedence <= get_rule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixRule = get_rule(parser.previous.type)->infix;
        infixRule();
    }
}

static parse_rule_t* get_rule(token_type_t type)
{
    return &rules[type];
}

void expression()
{
    parse_precedence(PREC_ASSIGNMENT);
}

bool compile(const char* source, chunk_t* chunk)
{
    init_scanner(source);

    compiling_chunk = chunk;
    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "expected end of expression.");

    end_compiler();
    return !parser.had_error;
}
