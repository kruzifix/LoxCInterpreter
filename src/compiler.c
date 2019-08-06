#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    token_t name;
    int depth;
} local_t;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} function_type_t;

typedef struct compiler__ {
    struct compiler__* enclosing;
    obj_function_t* function;
    function_type_t type;

    local_t locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
} compiler_t;

parser_t parser;
compiler_t* current = NULL;
chunk_t* compiling_chunk;

static chunk_t* current_chunk(void)
{
    return &(current->function->chunk);
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

static void emit_short(int value)
{
    emit_bytes((value >> 8) & 0xFF, value & 0xFF);
}

static int emit_jump(uint8_t instruction)
{
    emit_byte(instruction);
    emit_bytes(0xFF, 0xFF);
    return current_chunk()->count - 2;
}

static void emit_loop(int loopStart)
{
    emit_byte(OP_LOOP);

    int offset = current_chunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
    {
        error("loop body too large!");
    }

    emit_short(offset);
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
    emit_byte(OP_NIL);
    emit_byte(OP_RETURN);
}

static int make_constant(value_t value)
{
    int arg = add_constant(current_chunk(), value);

    if (arg > 0xFFFFFF)
    {
        error("Too many constants in chunk.");
        return 0;
    }

    return arg;
}

static void emit_constant(value_t value)
{
    write_constant(current_chunk(), value, parser.previous.line);
}

static void patch_jump(int offset)
{
    int jump = current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX)
    {
        error("Too much code to jump over.");
    }

    current_chunk()->code[offset] = (jump >> 8) & 0xFF;
    current_chunk()->code[offset + 1] = jump & 0xFF;
}

static void init_compiler(compiler_t* compiler, function_type_t type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function();
    current = compiler;

    if (type != TYPE_SCRIPT)
    {
        current->function->name = copy_string(parser.previous.start, parser.previous.length);
    }

    local_t* local = &current->locals[current->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static obj_function_t* end_compiler(bool printCode)
{
    emit_return();

    obj_function_t* func = current->function;

    //#ifdef DEBUG_PRINT_CODE
    if (printCode && !parser.had_error)
    {
        disassemble_chunk(current_chunk(), func->name != NULL ? func->name->chars : "<script>");
    }
    //#endif

    current = current->enclosing;
    return func;
}

static void begin_scope(void)
{
    ++current->scope_depth;
}

static void end_scope(void)
{
    --current->scope_depth;

    int n = 0;

    while (current->local_count > 0 &&
        current->locals[current->local_count - 1].depth > current->scope_depth)
    {
        ++n;
        --current->local_count;
    }

    if (n > 0)
        emit_bytes(OP_POPN, (uint8_t)n);
}

static void expression(void);
static void statement(void);
static void declaration(void);
static void fun_declaration(void);
static int identifier_constant(token_t* token);
static int resolve_local(compiler_t* compiler, token_t* name);

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

static uint8_t argument_list(void)
{
    uint8_t argCount = 0;

    if (!check(TOKEN_RIGHT_PAREN))
    {
        do {
            expression();

            if (argCount == 255)
            {
                error("Cannot have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void call(bool canAssign)
{
    uint8_t argCount = argument_list();
    emit_bytes(OP_CALL, argCount);
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
    int arg = resolve_local(current, &name);
    bool local = true;
    if (arg == -1)
    {
        arg = identifier_constant(&name);
        local = false;
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        if (local)
            emit_bytes(OP_SET_LOCAL, (uint8_t)arg);
        else
            emit_with_arg(OP_SET_GLOBAL, OP_SET_GLOBAL_LONG, arg);
    }
    else
    {
        if (local)
            emit_bytes(OP_GET_LOCAL, (uint8_t)arg);
        else
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

static void and_(bool canAssign)
{
    int endJump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(endJump);
}

static void or_(bool canAssign)
{
    int elseJump = emit_jump(OP_JUMP_IF_FALSE);
    int endJump = emit_jump(OP_JUMP);

    patch_jump(elseJump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);
    patch_jump(endJump);
}

static void list(bool canAssign)
{
    int valueCount = 0;
    if (!match(TOKEN_RIGHT_BRACKET))
    {
        do {
            expression();
            valueCount++;
        } while (match(TOKEN_COMMA));
        consume(TOKEN_RIGHT_BRACKET, "Expect ']' after values.");
    }

    emit_byte(OP_CREATE_LIST);
    emit_short(valueCount);
}

static void index_collection(bool canAssign)
{
    expression();

    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index value.");

    emit_byte(OP_GET_ELEMENT);
}

static void map(bool canAssign)
{
    // parse fields
    int fieldCount = 0;

    while (match(TOKEN_IDENTIFIER))
    {
        emit_constant(OBJ_VAL(copy_string(parser.previous.start, parser.previous.length)));

        consume(TOKEN_COLON, "Expect ':' after key.");

        expression();

        fieldCount++;

        match(TOKEN_COMMA);
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after map expression.");

    emit_byte(OP_CREATE_MAP);
    emit_short(fieldCount);
}

parse_rule_t rules[] = {
    { grouping, call,    PREC_CALL },       // TOKEN_LEFT_PAREN
    { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN
    { map,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE
    { list,    index_collection,    PREC_CALL },       // TOKEN_LEFT_BRACKET
    { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACKET
    { NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA
    { NULL,     NULL,    PREC_CALL },       // TOKEN_DOT
    { unary,    binary,  PREC_TERM },       // TOKEN_MINUS
    { NULL,     binary,  PREC_TERM },       // TOKEN_PLUS
    { NULL,     NULL,    PREC_NONE },       // TOKEN_COLON
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
    { NULL,     and_,    PREC_AND },        // TOKEN_AND
    { NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS
    { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE
    { literal,     NULL,    PREC_NONE },    // TOKEN_FALSE
    { NULL,     NULL,    PREC_NONE },       // TOKEN_FUN
    { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR
    { NULL,     NULL,    PREC_NONE },       // TOKEN_IF
    { literal,     NULL,    PREC_NONE },    // TOKEN_NIL
    { NULL,     or_,    PREC_OR },         // TOKEN_OR
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

static bool identifier_equals(token_t* a, token_t* b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(compiler_t* compiler, token_t* name)
{
    for (int i = compiler->local_count - 1; i >= 0; --i)
    {
        local_t* local = compiler->locals + i;
        if (identifier_equals(name, &local->name))
        {
            if (local->depth == -1)
            {
                error("Cannot read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static void add_local(token_t name)
{
    if (current->local_count == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    local_t* local = current->locals + current->local_count++;
    local->name = name;
    local->depth = -1;
}

static void declare_variable(void)
{
    if (current->scope_depth == 0)
        return;

    token_t* name = &parser.previous;
    for (int i = current->local_count - 1; i >= 0; --i)
    {
        local_t* local = current->locals + i;
        if (local->depth != -1 && local->depth < current->scope_depth)
            break;
        if (identifier_equals(name, &local->name))
        {
            error("Variable with this name already declared in this scope.");
        }
    }

    add_local(*name);
}

static int parse_variable(const char* errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declare_variable();
    if (current->scope_depth > 0)
        return 0;

    return identifier_constant(&parser.previous);
}

static void mark_initialized(void)
{
    if (current->scope_depth == 0)
        return;
    current->locals[current->local_count - 1].depth = current->scope_depth;

}

static void define_variable(int global)
{
    if (current->scope_depth > 0)
    {
        mark_initialized();
        return;
    }

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

static void if_statement(void)
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after if.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    int elseJump = emit_jump(OP_JUMP);

    patch_jump(thenJump);
    emit_byte(OP_POP);

    if (match(TOKEN_ELSE))
    {
        statement();
    }

    patch_jump(elseJump);
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

static void for_statement(void)
{
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_VAR))
    {
        var_declaration();
    }
    else if (match(TOKEN_SEMICOLON))
    {

    }
    else
    {
        expression_statement();
    }

    int loopStart = current_chunk()->count;

    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON))
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }

    if (!match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emit_jump(OP_JUMP);

        int incrementStart = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after clauses.");

        emit_loop(loopStart);
        loopStart = incrementStart;
        patch_jump(bodyJump);
    }

    statement();

    emit_loop(loopStart);

    if (exitJump != -1)
    {
        patch_jump(exitJump);
        emit_byte(OP_POP);
    }

    end_scope();
}

static void return_statement(void)
{
    if (current->type == TYPE_SCRIPT)
        error("Cannot return from top-level code.");

    if (match(TOKEN_SEMICOLON))
        emit_return();
    else
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
}

static void while_statement(void)
{
    int loopStart = current_chunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    statement();

    emit_loop(loopStart);

    patch_jump(exitJump);
    emit_byte(OP_POP);
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
    if (match(TOKEN_FUN))
    {
        fun_declaration();
    }
    else if (match(TOKEN_VAR))
    {
        var_declaration();
    }
    else
    {
        statement();
    }

    if (parser.panic_mode) synchronize();
}

static void block(void)
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(function_type_t type)
{
    compiler_t compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do {
            int paramConst = parse_variable("Expect parameter name.");
            define_variable(paramConst);

            current->function->arity++;
            if (current->function->arity > 8)
            {
                error("Cannot have more than 8 parameters.");
            }
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    consume(TOKEN_LEFT_BRACE, "Expect '{' after function body.");
    block();

    // TODO: somehow pass 'printCode' param to here!
    obj_function_t* func = end_compiler(false);
    emit_constant(OBJ_VAL(func));
}

static void fun_declaration(void)
{
    int global = parse_variable("Expect function name");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void statement(void)
{
    if (match(TOKEN_PRINT))
    {
        print_statement();
    }
    else if (match(TOKEN_IF))
    {
        if_statement();
    }
    else if (match(TOKEN_FOR))
    {
        for_statement();
    }
    else if (match(TOKEN_RETURN))
    {
        return_statement();
    }
    else if (match(TOKEN_WHILE))
    {
        while_statement();
    }
    else if (match(TOKEN_LEFT_BRACE))
    {
        begin_scope();
        block();
        end_scope();
    }
    else
    {
        expression_statement();
    }
}

obj_function_t* compile(const char* source, bool printCode)
{
    init_scanner(source);
    compiler_t compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    obj_function_t* func = end_compiler(printCode);
    return parser.had_error ? NULL : func;
}
