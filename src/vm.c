#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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

static void runtime_error(const char* format, ...);

static value_t clock_native(int argCount, value_t* args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static value_t printf_native(int argCount, value_t* args)
{
    if (argCount == 0)
        runtime_error("printf expects format string following by values: printf(format_string, ...)");

    // first arg is format string
    if (IS_OBJ(*args) && IS_STRING(*args))
    {
        obj_string_t* fmt = AS_STRING(*args);

        uint8_t argPos = 1;
        for (int i = 0; i < fmt->length; ++i)
        {
            char c = fmt->chars[i];

            if (c == '%')
            {
                print_value(args[argPos++]);
            }
            else
            {
                printf("%c", c);
            }
        }
        printf("\n");
    }
    else
    {
        runtime_error("printf expects format string following by values: printf(format_string, ...)");
    }

    return NIL_VAL;
}

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.openUpvalues = NULL;
}

static void runtime_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fputs("\n-------- call stack --------\n", stderr);

    for (int i = vm.frame_count - 1; i >= 0; i--)
    {
        call_frame_t* frame = &(vm.frames[i]);
        obj_function_t* func = frame->closure->function;
        // -1 because the IP is sitting on the next instruction to be executed.
        size_t instruction = frame->ip - func->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", func->chunk.lines[instruction]);
        if (func->name == NULL)
            fprintf(stderr, "script\n");
        else
            fprintf(stderr, "%s()\n", func->name->chars);
    }
    fputs("----------------------------\n", stderr);

    reset_stack();
}

static void define_native(const char* name, native_func_t func)
{
    push(OBJ_VAL(copy_string(name, (int)strlen(name))));
    push(OBJ_VAL(new_native(func)));
    table_set(&(vm.globals), AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void init_vm(void)
{
    reset_stack();
    init_table(&vm.globals);
    init_table(&vm.strings);
    vm.objects = NULL;

    define_native("clock", clock_native);
    define_native("printf", printf_native);
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

static bool call(obj_closure_t* closure, uint8_t argCount)
{
    if (closure->function->arity != argCount)
    {
        runtime_error("Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }

    if (vm.frame_count == FRAMES_MAX)
    {
        runtime_error("CallStack overflow.");
        return false;
    }

    call_frame_t* frame = &(vm.frames[vm.frame_count++]);
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;

    frame->slots = vm.stack_top - argCount - 1;
    return true;
}

static bool call_value(value_t callee, uint8_t argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
        case OBJ_CLOSURE:
            return call(AS_CLOSURE(callee), argCount);
        case OBJ_NATIVE: {
            native_func_t func = AS_NATIVE(callee);
            value_t result = func(argCount, vm.stack_top - argCount);
            vm.stack_top -= argCount + 1;
            push(result);
            return true;
        }

        default:
            // Non-callable object type.
            break;
        }
    }

    runtime_error("Can only call functions and classes.");
    return false;
}

static obj_upvalue_t* capture_upvalue(value_t* local)
{
    obj_upvalue_t* prevUpvalue = NULL;
    obj_upvalue_t* upvalue = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local)
    {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local)
        return upvalue;

    obj_upvalue_t* createdUpvalue = new_upvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL)
        vm.openUpvalues = createdUpvalue;
    else
        prevUpvalue->next = createdUpvalue;

    return createdUpvalue;
}

static void close_upvalues(value_t* last)
{
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last)
    {
        obj_upvalue_t* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &(upvalue->closed);
        vm.openUpvalues = upvalue->next;
    }
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
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define CONSTANT_LONG(name) int constIndex = 0; \
    constIndex |= ((int)READ_BYTE() << 16); \
    constIndex |= ((int)READ_BYTE() << 8); \
    constIndex |= READ_BYTE(); \
    value_t name = frame->closure->function->chunk.constants.values[constIndex]

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
            disassemble_instruction(&(frame->closure->function->chunk), (int)(frame->ip - frame->closure->function->chunk.code));
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

        case OP_GET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }

        case OP_SET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
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

        case OP_CALL: {
            uint8_t argCount = READ_BYTE();
            if (!call_value(peek(argCount), argCount))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &(vm.frames[vm.frame_count - 1]);
            break;
        }

        case OP_CLOSURE: {
            obj_function_t* func = AS_FUNCTION(READ_CONSTANT());
            obj_closure_t* clos = new_closure(func);
            push(OBJ_VAL(clos));
            for (int i = 0; i < clos->upvalueCount; i++)
            {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal)
                    clos->upvalues[i] = capture_upvalue(frame->slots + index);
                else
                    clos->upvalues[i] = frame->closure->upvalues[index];
            }
            break;
        }

        case OP_CLOSE_UPVALUE:
            close_upvalues(vm.stack_top - 1);
            pop();
            break;

        case OP_RETURN: {
            value_t result = pop();

            close_upvalues(frame->slots);

            vm.frame_count--;
            if (vm.frame_count == 0)
                return INTERPRET_OK;

            vm.stack_top = frame->slots;
            push(result);
            
            frame = &(vm.frames[vm.frame_count - 1]);
            break;
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
    obj_closure_t* closure = new_closure(func);
    pop();
    push(OBJ_VAL(closure));
    call_value(OBJ_VAL(closure), 0);

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
