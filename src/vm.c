#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

const char* INTERPRET_RESULT_STRING[] = {
    "OK",
    "COMPILE_ERROR",
    "RUNTIME_ERROR"
};

vm_t vm;

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
}

static void runtime_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n-------- call stack --------\n", stderr);

    for (uint8_t i = 0; i < vm.frame_count; ++i)
    {
        call_frame_t* frame = &(vm.frames[i]);

        size_t instruction = frame->ip - frame->function->chunk.code;
        char* name = frame->function->name != NULL ? frame->function->name->chars : "<script>";
        fprintf(stderr, "[line %d] in %s\n", frame->function->chunk.lines[instruction], name);
    }
    fputs("----------------------------\n", stderr);

    reset_stack();
}

void init_vm(void)
{
    reset_stack();
    init_table(&vm.globals);
    init_table(&vm.strings);
    vm.objects = NULL;
}

void free_vm(void)
{
    free_table(&vm.globals);
    free_table(&vm.strings);
    free_objects();
}

static value_t peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static bool isFalsey(value_t value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)) || (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

static void concatenate()
{
    obj_string_t* b = AS_STRING(pop());
    obj_string_t* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    obj_string_t* result = take_string(chars, length);
    push(OBJ_VAL(result));
}

static interpret_result_t run(bool traceExecution)
{
    call_frame_t* frame = &(vm.frames[vm.frame_count - 1]);

#define READ_BYTE() (*(frame->ip++))
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define CONSTANT_LONG(name) int constIndex = 0; \
    constIndex |= ((int)READ_BYTE() << 16); \
    constIndex |= ((int)READ_BYTE() << 8); \
    constIndex |= READ_BYTE(); \
    value_t name = frame->function->chunk.constants.values[constIndex]

#define STRING_LONG(name) CONSTANT_LONG(constant); \
    obj_string_t* name = AS_STRING(constant)

#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)

    for (;;)
    {
//#ifdef DEBUG_TRACE_EXECUTION
        if (traceExecution)
        {
            printf("          ");
            for (value_t* slot = vm.stack; slot < vm.stack_top; slot++)
            {
                printf("[ ");
                print_value(*slot);
                printf(" ]");
            }
            printf("\n");
            disassemble_instruction(&(frame->function->chunk), (int)(frame->ip - frame->function->chunk.code));
        }
//#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
        case OP_CONSTANT: {
            value_t constant = READ_CONSTANT();
            push(constant);
            break;
        }

        case OP_CONSTANT_LONG: {
            CONSTANT_LONG(constant);
            push(constant);
            break;
        }

        case OP_NIL: push(NIL_VAL); break;
        case OP_TRUE: push(BOOL_VAL(true)); break;
        case OP_FALSE: push(BOOL_VAL(false)); break;
        case OP_POP: pop(); break;

        case OP_POPN: {
            uint8_t n = READ_BYTE();
            vm.stack_top -= n;
            break;
        }

        case OP_GET_LOCAL: {
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
            break;
        }

        case OP_GET_GLOBAL: {
            obj_string_t* name = READ_STRING();
            value_t value;
            if (!table_get(&vm.globals, name, &value))
            {
                runtime_error("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }

        case OP_GET_GLOBAL_LONG: {
            STRING_LONG(name);
            value_t value;
            if (!table_get(&vm.globals, name, &value))
            {
                runtime_error("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }

        case OP_DEFINE_GLOBAL: {
            obj_string_t* name = READ_STRING();
            table_set(&vm.globals, name, peek(0));
            pop();
            break;
        }

        case OP_DEFINE_GLOBAL_LONG: {
            STRING_LONG(name);
            table_set(&vm.globals, name, peek(0));
            pop();
            break;
        }

        case OP_SET_LOCAL: {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            break;
        }

        case OP_SET_GLOBAL: {
            obj_string_t* name = READ_STRING();
            if (table_set(&vm.globals, name, peek(0)))
            {
                runtime_error("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }

        case OP_SET_GLOBAL_LONG: {
            STRING_LONG(name);
            if (table_set(&vm.globals, name, peek(0)))
            {
                runtime_error("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }

        case OP_EQUAL: {
            value_t a = pop();
            value_t b = pop();
            push(BOOL_VAL(values_equal(a, b)));
            break;
        }

        case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
        case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
        case OP_ADD: {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
            {
                concatenate();
            }
            else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
            {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            }
            else
            {
                runtime_error("Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
        case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
        case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
        case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0)))
            {
                runtime_error("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;

        case OP_PRINT: {
            print_value(pop());
            printf("\n");
            break;
        }

        case OP_JUMP: {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }

        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0)))
            {
                frame->ip += offset;
            }
            break;
        }

        case OP_LOOP: {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }

        case OP_RETURN: {
            return INTERPRET_OK;
        }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

interpret_result_t interpret(const char* source, interpreter_params_t* params)
{
    obj_function_t* func = compile(source, params->print_disassembly);
    if (func == NULL)
        return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(func));
    call_frame_t* frame = &(vm.frames[vm.frame_count++]);
    frame->function = func;
    frame->ip = func->chunk.code;
    frame->slots = vm.stack;

    return run(params->trace_execution);
}

void push(value_t value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

value_t pop(void)
{
    vm.stack_top--;
    return *vm.stack_top;
}
