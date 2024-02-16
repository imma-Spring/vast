#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

VM vm;

ObjString *plus, *minus, *star, *divide, *dot, *scan, *equal, *less, *greater,
    *less_equal, *greater_equal, *not, *not_equal, *question, *_if, *set,
    *comma, *mod;

static void operators() {
  plus = copy_string("+", 1, false);
  minus = copy_string("-", 1, false);
  star = copy_string("*", 1, false);
  divide = copy_string("/", 1, false);
  dot = copy_string(".", 1, false);
  scan = copy_string("^", 1, false);
  equal = copy_string("?=", 2, false);
  less = copy_string("<", 1, false);
  greater = copy_string(">", 1, false);
  less_equal = copy_string("<=", 2, false);
  greater_equal = copy_string(">=", 2, false);
  not = copy_string("!", 1, false);
  not_equal = copy_string("!=", 2, false);
  question = copy_string("?", 1, false);
  _if = copy_string("if", 2, false);
  set = copy_string("=", 1, false);
  comma = copy_string(",", 1, false);
  mod = copy_string("%%", 1, false);
}

static double str_to_double(char *input, int *status_code) {
  if (*input == '\n') {
    *status_code = -1;
    return 0;
  }
  bool decimals = false;
  double val = 0;
  double place = 1;
  while (isdigit(*input) || (!decimals && *input == '.')) {
    if (*input == '.') {
      decimals = true;
      input++;
      continue;
    }
    if (decimals) {
      place /= 10;
    }
    if (place == 1) {
      val *= 10;
    }
    val += (*input - '0') * place;
    input++;
  }
  if (*input == '\n') {
    input++;
  }
  if (*input != '\0') {
    *status_code = -1;
    return 0;
  }
  *status_code = 0;
  return val;
}

static void reset_stack() {
  vm.stack_top = vm.stack;
  vm.frame_count = 0;
  vm.open_upvalues = NULL;
}

static void runtime_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (ssize_t i = vm.frame_count - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %zu] in ", function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  reset_stack();
}

void init_VM() {
  reset_stack();
  vm.objects = NULL;
  vm.bytes_allocated = 0;
  vm.next_gc = 1024 * 1024;
  vm.gray_capacity = 0;
  vm.gray_count = 0;
  vm.gray_stack = NULL;
  init_table(&vm.globals);
  init_table(&vm.strings);

  vm.init_string = NULL;
  vm.init_string = copy_string("init", 4, false);
  operators();
}

void free_VM() {
  free_table(&vm.globals);
  free_table(&vm.strings);
  vm.init_string = NULL;
  free_objects();
}

void push(Value value) {
  *vm.stack_top = value;
  vm.stack_top++;
}

Value pop() {
  vm.stack_top--;
  return *vm.stack_top;
}

static Value peek(int distance) { return vm.stack_top[-1 - distance]; }

static bool call(ObjClosure *closure, size_t arg_count) {
  if (arg_count != closure->function->arity) {
    runtime_error("Expected %d arguments but got %d.", closure->function->arity,
                  arg_count);
    return false;
  }
  if (vm.frame_count == FRAMES_MAX) {
    runtime_error("Stack overflow.");
    return false;
  }
  CallFrame *frame = &vm.frames[vm.frame_count++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stack_top - arg_count - 1;
  return true;
}

static ObjUpvalue *capture_upvalue(Value *local) {
  ObjUpvalue *prev_upvalue = NULL;
  ObjUpvalue *upvalue = vm.open_upvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prev_upvalue = upvalue;
    upvalue = upvalue->next;
  }
  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }
  ObjUpvalue *created_upvalue = new_upvalue(local);
  created_upvalue->next = upvalue;
  if (prev_upvalue == NULL) {
    vm.open_upvalues = created_upvalue;
  } else {
    prev_upvalue->next = created_upvalue;
  }
  return created_upvalue;
}

static void close_upvalue(Value *last) {
  while (vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
    ObjUpvalue *upvalue = vm.open_upvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.open_upvalues = upvalue->next;
  }
}

static bool is_falsey(Value value) {
  return IS_NIL(value) || (IS_NUMBER(value) && AS_NUMBER(value) == 0.0) ||
         (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatonate() {
  ObjString *b = AS_STRING(peek(0));
  ObjString *a = AS_STRING(peek(1));
  size_t length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';
  ObjString *result = take_string(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}
static void vars_to_vals() {
  Value v2 = peek(0);
  Value v1 = peek(1);
  if (IS_VARIABLE(v1) && IS_VARIABLE(v2)) {
    Value v3 = AS_VARIABLE(v1)->value;
    Value v4 = AS_VARIABLE(v2)->value;
    pop();
    pop();
    push(v3);
    push(v4);
  } else if (IS_VARIABLE(v1)) {
    Value v3 = AS_VARIABLE(v1)->value;
    pop();
    pop();
    push(v3);
    push(v2);
  } else if (IS_VARIABLE(v2)) {
    Value v4 = AS_VARIABLE(v2)->value;
    pop();
    pop();
    push(v1);
    push(v4);
  }
}
#ifdef DEBUG_TRACE_EXECUTION
static void stack_print() {
  CallFrame *frame = &vm.frames[vm.frame_count - 1];
  printf("          ");
  for (Value *slot = vm.stack; slot < vm.stack_top; slot++) {
    printf("[ ");
    print_value(*slot);
    printf(" ]");
  }
  printf("\n");
  disassemble_instruction(
      &frame->closure->function->chunk,
      (int)(frame->ip - frame->closure->function->chunk.code));
}
#endif /* ifdef DEBUG_TRACE_EXECUTION */

static InterpretResult run_function(ObjProcedure *procedure);

static void define_function(ObjString *name) {
  ObjProcedure *procedure = new_procedure();
  procedure->name = name;
  size_t i = 0;
  while (i < STACK_MAX && peek(i) != NIL_VAL) {
    write_value_array(&procedure->stack, peek(i));
    i++;
  }
  for (size_t j = 0; j <= i; ++j) {
    pop();
  }
  table_set(&vm.globals, name, OBJ_VAL(procedure));
}

static void modulo() {
  Value a = peek(1);
  Value b = peek(0);
  double a_ = AS_NUMBER(a);
  double b_ = AS_NUMBER(b);
  while (a_ >= b_) {
    a_ -= b_;
  }
  pop();
  pop();
  push(NUMBER_VAL(a_));
}

#define BINARY_OP(value_type, op)                                              \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtime_error("Operands must be numbers.");                              \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(value_type(a op b));                                                  \
  } while (false)

static InterpretResult run_operation(ObjOperation *operation) {
  if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(plus))) {
    vars_to_vals();
    if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
      concatonate();
    } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
      double b = AS_NUMBER(pop());
      double a = AS_NUMBER(pop());
      push(NUMBER_VAL(a + b));
    } else {
      runtime_error("Operands must be either two strings or two numbers.");
      return INTERPRET_RUNTIME_ERROR;
    }
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(minus))) {
    vars_to_vals();
    BINARY_OP(NUMBER_VAL, -);
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(star))) {
    vars_to_vals();
    BINARY_OP(NUMBER_VAL, *);
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(divide))) {
    vars_to_vals();
    BINARY_OP(NUMBER_VAL, /);
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(mod))) {
    vars_to_vals();
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
      runtime_error("Operands must be both be numbers");
      return INTERPRET_RUNTIME_ERROR;
    }
    modulo();
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(equal))) {
    vars_to_vals();
    Value b = pop();
    Value a = pop();
    printf("a: ");
    print_value(a);
    printf("\nb: ");
    print_value(b);
    printf("\n");
    push(BOOL_VAL(values_equal(a, b)));
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(greater))) {
    vars_to_vals();
    BINARY_OP(NUMBER_VAL, >);
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(less))) {
    vars_to_vals();
    BINARY_OP(NUMBER_VAL, <);
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(greater_equal))) {
    vars_to_vals();
    BINARY_OP(NUMBER_VAL, >=);
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(less_equal))) {
    vars_to_vals();
    BINARY_OP(NUMBER_VAL, <=);
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(not ))) {
    if (IS_VARIABLE(peek(0))) {
      Value value = AS_VARIABLE(peek(0))->value;
      pop();
      push(value);
    }
    Value not = BOOL_VAL(is_falsey(peek(0)));
    pop();
    push(not );
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(question))) {
    if (IS_VARIABLE(peek(0))) {
      Value value = AS_VARIABLE(peek(0))->value;
      pop();
      push(value);
    }
    Value not = BOOL_VAL(!is_falsey(peek(0)));
    pop();
    push(not );
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(_if))) {
    Value val = pop();
    vars_to_vals();
    push(val);
    Value boolean = BOOL_VAL(!is_falsey(peek(0)));
    Value path;
    if (AS_BOOL(boolean)) {
      path = peek(2);
    } else {
      path = peek(1);
    }
    pop();
    pop();
    pop();
    InterpretResult result = INTERPRET_OK;
    if (IS_PROCEDURE(path)) {
      result = run_function(AS_PROCEDURE(path));
    } else if (IS_OPERATION(path)) {
      result = run_operation(AS_OPERATION(path));
    } else {
      push(path);
    }
    if (result == INTERPRET_RUNTIME_ERROR) {
      return INTERPRET_RUNTIME_ERROR;
    }
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(dot))) {
    if (IS_VARIABLE(peek(0))) {
      print_value(AS_VARIABLE(peek(0))->value);
      pop();
      return INTERPRET_OK;
    }
    print_value(pop());
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(scan))) {
    char buffer[1024];

    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
      int status_code = 0;
      double result = str_to_double(buffer, &status_code);

      if (status_code == -1) {
        size_t length = strlen(buffer);
        char *str = ALLOCATE(char, length);
        memcpy(str, buffer, length - 1);
        str[length - 1] = '\0';
        push(OBJ_VAL(take_string(str, length)));
      } else {
        push(NUMBER_VAL(result));
      }
    } else {
      runtime_error("reached end of input.");
      return INTERPRET_RUNTIME_ERROR;
    }
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(set))) {
    if (!IS_VARIABLE(peek(0))) {
      runtime_error("Can only asign to variables.");
      return INTERPRET_RUNTIME_ERROR;
    }
    table_set(&vm.globals, AS_VARIABLE(peek(0))->name, peek(1));
    pop();
    pop();
  } else if (values_equal(OBJ_VAL(operation->type), OBJ_VAL(comma))) {
    if (!IS_VARIABLE(peek(0))) {
      runtime_error("can not run a non procedure.");
      return INTERPRET_RUNTIME_ERROR;
    }
    Value value;
    table_get(&vm.globals, AS_VARIABLE(peek(0))->name, &value);
    if (!IS_PROCEDURE(value)) {
      table_delete(&vm.globals, AS_VARIABLE(peek(0))->name);
      runtime_error("can not run a non procedure.");
      return INTERPRET_RUNTIME_ERROR;
    }
    pop();
    InterpretResult result = run_function(AS_PROCEDURE(value));
    if (result == INTERPRET_RUNTIME_ERROR) {
      return INTERPRET_RUNTIME_ERROR;
    }
  }

  return INTERPRET_OK;
}

static Value variable_to_value(Value val) {
  if (IS_VARIABLE(val)) {
    ObjVariable *v = AS_VARIABLE(val);
    Value variable_value;
    return variable_value;
  }
  return val;
}

static InterpretResult run_function(ObjProcedure *procedure) {
  for (ssize_t i = procedure->stack.count - 1; i >= 0; --i) {
    if (IS_OPERATION(procedure->stack.value[i])) {
      InterpretResult result =
          run_operation(AS_OPERATION(procedure->stack.value[i]));
      if (result == INTERPRET_RUNTIME_ERROR) {
        return INTERPRET_RUNTIME_ERROR;
      }
    } else if (IS_VARIABLE(procedure->stack.value[i])) {
      ObjString *name = AS_VARIABLE(procedure->stack.value[i])->name;
      ObjVariable *variable = new_variable();
      Value value;
      table_get(&vm.globals, name, &value);
      variable->name = name;
      variable->value = value;
      push(OBJ_VAL(variable));
    } else {
      push(procedure->stack.value[i]);
    }
  }
  return INTERPRET_OK;
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frame_count - 1];
  operators();
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2]) << 8 | frame->ip[-1]))
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.value[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    stack_print();
#endif /* ifdef DEBUG_TRACE_EXECUTION */
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_ADD: {
      vars_to_vals();
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatonate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      } else {
        runtime_error("Operands must be either two strings or two numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SUBTRACT:
      vars_to_vals();
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      vars_to_vals();
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DIVIDE:
      vars_to_vals();
      BINARY_OP(NUMBER_VAL, /);
      break;
    case OP_MOD: {
      vars_to_vals();
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        runtime_error("Operands must be both be numbers");
        return INTERPRET_RUNTIME_ERROR;
      }
      modulo();
      break;
    }
    case OP_EQUAL: {
      vars_to_vals();
      Value b = peek(0);
      Value a = peek(1);
      Value result = BOOL_VAL(values_equal(a, b));
      pop();
      pop();
      push(result);
      break;
    }
    case OP_GREATER:
      vars_to_vals();
      BINARY_OP(NUMBER_VAL, >);
      break;
    case OP_LESS:
      vars_to_vals();
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_GREATER_EQUAL:
      vars_to_vals();
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_LESS_EQUAL:
      vars_to_vals();
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_NOT: {
      if (IS_VARIABLE(peek(0))) {
        Value value = AS_VARIABLE(peek(0))->value;
        pop();
        push(value);
        break;
      }
      Value not = BOOL_VAL(is_falsey(peek(0)));
      pop();
      push(not );

      break;
    }
    case OP_WHILE: {
      Value while_condition;
      Value while_body_var;
      while_body_var = peek(0);
      if (!IS_VARIABLE(while_body_var)) {
        runtime_error("body of while must be a variable.");
      }
      Value while_body;
      table_get(&vm.globals, AS_VARIABLE(while_body_var)->name, &while_body);
      table_get(&vm.globals, copy_string("while_condition", 15, false),
                &while_condition);
      if (!IS_PROCEDURE(while_condition) || !IS_PROCEDURE(while_body)) {
        runtime_error("'while_condition' and 'while_body' must be procedures.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjProcedure *while_condition_proc = AS_PROCEDURE(while_condition);
      ObjProcedure *while_body_proc = AS_PROCEDURE(while_body);
      pop();
      do {
        // Evaluate the condition
        InterpretResult condition_result = run_function(while_condition_proc);
        if (condition_result == INTERPRET_RUNTIME_ERROR) {
          return INTERPRET_RUNTIME_ERROR;
        }

        // Check if the condition is true
        Value condition_value = peek(0);

        if (is_falsey(condition_value)) {
          // Condition is false, break out of the loop
          if (vm.stack_top > &(vm.stack[1])) {
            pop();
          }
          break;
        }
        if (vm.stack_top > &(vm.stack[1])) {
          pop();
        }
        // Execute the body of the while loop
        InterpretResult body_result = run_function(while_body_proc);
        if (body_result == INTERPRET_RUNTIME_ERROR) {
          return INTERPRET_RUNTIME_ERROR;
        }
        // Repeat the loop as long as the condition is true
      } while (true);

      break;
    }
    case OP_IF: {
      Value val = pop();
      vars_to_vals();
      push(val);
      Value boolean = BOOL_VAL(!is_falsey(peek(0)));
      Value path;
      if (AS_BOOL(boolean)) {
        path = peek(2);
      } else {
        path = peek(1);
      }
      pop();
      pop();
      pop();
      InterpretResult result = INTERPRET_OK;
      if (IS_PROCEDURE(path)) {
        result = run_function(AS_PROCEDURE(path));
      } else if (IS_OPERATION(path)) {
        result = run_operation(AS_OPERATION(path));
      } else {
        push(path);
      }
      if (result == INTERPRET_RUNTIME_ERROR) {
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_PRINT: {
      if (IS_VARIABLE(peek(0))) {
        print_value(AS_VARIABLE(peek(0))->value);
        pop();
        break;
      }
      print_value(pop());
      break;
    }
    case OP_SCAN: {
      char buffer[1024];

      if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        int status_code = 0;
        double result = str_to_double(buffer, &status_code);

        if (status_code == -1) {
          size_t length = strlen(buffer);
          char *str = ALLOCATE(char, length);
          memcpy(str, buffer, length - 1);
          str[length - 1] = '\0';
          push(OBJ_VAL(take_string(str, length)));
        } else {
          push(NUMBER_VAL(result));
        }
      } else {
        runtime_error("reached end of input.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_PUSH_OPERATION: {
      ObjString *op = READ_STRING();
      ObjOperation *operation = new_operation();
      operation->type = op;
      push(OBJ_VAL(operation));
      break;
    }
    case OP_VARIABLE: {
      ObjString *name = READ_STRING();
      ObjVariable *variable = new_variable();
      Value value;
      table_get(&vm.globals, name, &value);
      variable->name = name;
      variable->value = value;
      push(OBJ_VAL(variable));
      break;
    }
    case OP_SET_VARIABLE: {
      if (!IS_VARIABLE(peek(0))) {
        runtime_error("Can only asign to variables.");
        return INTERPRET_RUNTIME_ERROR;
      }
      table_set(&vm.globals, AS_VARIABLE(peek(0))->name, peek(1));
      pop();
      pop();
      break;
    }
    case OP_DEFINE_FUNCTION:
      define_function(READ_STRING());
      break;
    case OP_APPLY: {
      if (!IS_VARIABLE(peek(0))) {
        runtime_error("can not run a non procedure.");
        return INTERPRET_RUNTIME_ERROR;
      }
      Value value;
      table_get(&vm.globals, AS_VARIABLE(peek(0))->name, &value);
      if (!IS_PROCEDURE(value)) {
        table_delete(&vm.globals, AS_VARIABLE(peek(0))->name);
        runtime_error("can not run a non procedure.");
        return INTERPRET_RUNTIME_ERROR;
      }
      pop();
      InterpretResult result = run_function(AS_PROCEDURE(value));
      if (result == INTERPRET_RUNTIME_ERROR) {
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_RETURN:
      return INTERPRET_OK;
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    }
  }

#undef READ_CONSTANT
#undef READ_BYTE
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
  ObjFunction *function = compile(source);
  if (function == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  push(OBJ_VAL(function));
  ObjClosure *closure = new_closure(function);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}
