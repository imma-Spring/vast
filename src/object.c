
#include "object.h"
#include "chunk.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATE_OBJ(type, object_type)                                        \
  (type *)allocate_object(sizeof(type), object_type)

static Obj *allocate_object(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->is_marked = false;
  object->next = vm.objects;
  vm.objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocat %zu for %d\n", (void *)object, size, type);
#endif /* ifdef DEBUG_LOG_GC */

  return object;
}

ObjClosure *new_closure(ObjFunction *function) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalue_count);
  for (size_t i = 0; i < function->upvalue_count; ++i) {
    upvalues[i] = NULL;
  }
  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalue_count = function->upvalue_count;
  return closure;
}

ObjFunction *new_function() {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->upvalue_count = 0;
  function->name = NULL;
  init_chunk(&function->chunk);
  return function;
}

static char *format(char *chars) {
  size_t max_length = strlen(chars);
  char *formated = malloc(max_length + 1);
  size_t j = 0;
  for (size_t i = 0; i < max_length; ++i, ++j) {
    if (chars[i] == '\\' && i < max_length - 1) {
      switch (chars[i + 1]) {
      case 'n':
        formated[j] = '\n';
        i++;
        break;
      case 't':
        formated[j] = '\t';
        i++;
        break;
      case 'r':
        formated[j] = '\r';
        i++;
        break;
      case '\\':
        formated[j] = '\\';
        i++;
        break;
      case '\"':
        formated[j] = '\"';
        i++;
        break;
      default:
        break;
      }
    } else {
      formated[j] = chars[i];
    }
  }
  formated[j] = '\0';
  return formated;
}

static ObjString *allocate_string(char *chars, size_t length, uint32_t hash,
                                  bool string_literal) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = string_literal ? format(chars) : chars;
  string->hash = hash;
  push(OBJ_VAL(string));
  table_set(&vm.strings, string, NIL_VAL);
  pop();
  return string;
}

static uint32_t hash_string(const char *key, size_t length) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < length; ++i) {
    hash ^= (uint32_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString *take_string(char *chars, size_t length) {
  uint32_t hash = hash_string(chars, length);
  ObjString *interned = table_find_string(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return allocate_string(chars, length, hash, true);
}

ObjString *copy_string(const char *chars, size_t length, bool strlit) {
  uint32_t hash = hash_string(chars, length);
  ObjString *interned = table_find_string(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    return interned;
  }
  char *heap_chars = ALLOCATE(char, length + 1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';
  return allocate_string(heap_chars, length, hash, strlit);
}

ObjVariable *new_variable() {
  ObjVariable *variable = ALLOCATE_OBJ(ObjVariable, OBJ_VARIABLE);
  variable->name = NULL;
  variable->value = NIL_VAL;
  return variable;
}

ObjUpvalue *new_upvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}

ObjProcedure *new_procedure() {
  ObjProcedure *procedure = ALLOCATE_OBJ(ObjProcedure, OBJ_PROCEDURE);
  procedure->name = NULL;
  init_value_array(&procedure->stack);
  return procedure;
}

ObjOperation *new_operation() {
  ObjOperation *operation = ALLOCATE_OBJ(ObjOperation, OBJ_OPERATION);
  operation->type = NULL;
  return operation;
}

static void print_function(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

static void print_procedure(ObjProcedure *procedure) {
  printf("<%s> [", procedure->name->chars);
  for (size_t i = 0; i < procedure->stack.count; ++i) {
    print_value(procedure->stack.value[i]);
    if (i < procedure->stack.count - 1) {
      printf(", ");
    }
  }
  printf("]");
}

void print_object(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_CLOSURE:
    print_function(AS_CLOSURE(value)->function);
    break;
  case OBJ_FUNCTION:
    print_function(AS_FUNCTION(value));
    break;
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  case OBJ_VARIABLE: {
    printf("%s={", AS_VARIABLE(value)->name->chars);
    print_value(AS_VARIABLE(value)->value);
    printf("}");
    break;
  }
  case OBJ_PROCEDURE:
    print_procedure(AS_PROCEDURE(value));
    break;
    break;
  case OBJ_UPVALUE:
    printf("upvalue");
    break;
  case OBJ_OPERATION:
    printf("%s", AS_OPERATION(value)->type->chars);
    break;
  }
}
