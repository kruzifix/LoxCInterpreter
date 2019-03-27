#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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

typedef void(*ParseFn)(bool);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    precedence_t precedence;
} parse_rule_t;

parser_t parser;
chunk_t* compiling_chunk;

static chunk_t* current_chunk(void)
{
    return compiling_chunk;
}

static void error_at(token_t* token, const char* message)
{
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
        fprintf(stderr, " at end");
    else if (token->type != TOKEN_ERROR)
        fprintf(stderr, " at '%.*s'", token->length, token->start);

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

static void advance(void)
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

static bool check(token_type_t type)
{
    return parser.current.type == type;
}

static bool match(token_type_t type)
{
    if (!check(type))
        return false;
    advance();
    return true;
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

static void emit_with_arg(uint8_t code, uint8_t codeLong, int arg)
{
    if (arg <= 0xFF)
    {
        emit_bytes(code, (uint8_t)arg);
    }
    else
    {
        emit_bytes(codeLong, (arg >> 16) & 0xFF);
        emit_bytes((arg >> 8) & 0xFF, arg & 0xFF);
    }
}

static void emit_return(void)
{
    emit_byte(OP_RETURN);
}

static int make_constant(value_t value)
{
    return add_constant(current_chunk(), value);
}

static void emit_constant(value_t value)
{
    write_constant(current_chunk(), value, parser.previous.line);
}

static void end_compiler(void)
{
    emit_return();

#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error)
    {
        disassemble_chunk(current_chunk(), "code");
    }
#endif
}

static void expression(void);
static void statement(void);
static void declaration(void);
static int identifier_constant(token_t* token);

static parse_rule_t* get_rule(token_type_t type);
static void parse_precedence(precedence_t precedence);

static void binary(bool canAssign)
{
    token_type_t operatorType = parser.previous.type;

    parse_rule_t* rule = get_rule(operatorType);
    parse_precedence((precedence_t)(rule->precedence + 1));

    switch (operatorType)
    {
    case TOKEN_BANG_EQUAL: emit_bytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL: emit_byte(OP_EQUAL); break;
    case TOKEN_GREATER: emit_byte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS: emit_byte(OP_LESS); break;
    case TOKEN_LESS_EQUAL: emit_bytes(OP_GREATER, OP_NOT); break;

    case TOKEN_PLUS:  emit_byte(OP_ADD); break;
    case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
    case TOKEN_STAR:  emit_byte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
    default:
        return;
    }
}

static void literal(bool canAssign)
{
    switch (parser.previous.type)
    {
    case TOKEN_FALSE: emit_byte(OP_FALSE); break;
    case TOKEN_NIL: emit_byte(OP_NIL); break;
    case TOKEN_TRUE: emit_byte(OP_TRUE); break;
    default:
        return;
    }
}

static void grouping(bool canAssign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "expected ')' after expression.");
}

static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string(bool canAssign)
{
    emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void named_variable(token_t name, bool canAssign)
{
    int arg = identifier_constant(&name);

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emit_with_arg(OP_SET_GLOBAL, OP_SET_GLOBAL_LONG, arg);
    }
    else
    {
        emit_with_arg(OP_GET_GLOBAL, OP_GET_GLOBAL_LONG, arg);
    }
}

static void variable(bool canAssign)
{
    named_variable(parser.previous, canAssign);
}

static void unary(bool canAssign)
{
    token_type_t operatorType = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (operatorType)
    {
    case TOKEN_BANG:
        emit_byte(OP_NOT);
        break;
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
    { unary,    NULL,    PREC_NONE },       // TOKEN_BANG
    { NULL,     binary,  PREC_EQUALITY },   // TOKEN_BANG_EQUAL
    { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL
    { NULL,     binary,  PREC_EQUALITY },   // TOKEN_EQUAL_EQUAL
    { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER
    { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER_EQUAL
    { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS
    { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS_EQUAL
    { variable, NULL,    PREC_NONE },       // TOKEN_IDENTIFIER
    { string,   NULL,    PREC_NONE },       // TOKEN_STRING
    { number,   NULL,    PREC_NONE },       // TOKEN_NUMBER
    { NULL,     NULL,    PREC_AND },        // TOKEN_AND
    { NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS
    { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE
    { literal,     NULL,    PREC_NONE },    // TOKEN_FALSE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_FUN
    { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR
    { NULL,     NULL,    PREC_NONE },       // TOKEN_IF
    { literal,     NULL,    PREC_NONE },    // TOKEN_NIL
    { NULL,     NULL,    PREC_OR },         // TOKEN_OR
    { NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT
    { NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN
    { NULL,     NULL,    PREC_NONE },       // TOKEN_SUPER
    { NULL,     NULL,    PREC_NONE },       // TOKEN_THIS
    { literal,     NULL,    PREC_NONE },    // TOKEN_TRUE
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
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= get_rule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixRule = get_rule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target.");
        expression();
    }
}

static parse_rule_t* get_rule(token_type_t type)
{
    return &rules[type];
}

static int identifier_constant(token_t* token)
{
    return make_constant(OBJ_VAL(copy_string(token->start, token->length)));
}

static int parse_variable(const char* errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifier_constant(&parser.previous);
}

static void define_variable(int global)
{
    emit_with_arg(OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG, global);
}

static void expression(void)
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void expression_statement(void)
{
    expression();
    emit_byte(OP_POP);
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
}

static void var_declaration(void)
{
    int global = parse_variable("Expect variable name.");

    if (match(TOKEN_EQUAL))
    {
        expression();
    }
    else
    {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    define_variable(global);
}

static void print_statement(void)
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void synchronize(void)
{
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;

        switch (parser.previous.type)
        {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;
        default:
            // do nothing
            ;
        }

        advance();
    }
}

static void declaration(void)
{
    if (match(TOKEN_VAR))
    {
        var_declaration();
    }
    else
    {
        statement();
    }

    if (parser.panic_mode) synchronize();
}

static void statement(void)
{
    if (match(TOKEN_PRINT))
    {
        print_statement();
    }
    else
    {
        expression_statement();
    }
}

bool compile(const char* source, chunk_t* chunk)
{
    init_scanner(source);

    compiling_chunk = chunk;
    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    end_compiler();
    return !parser.had_error;
}
