
#pragma once

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"
#include <stddef.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_VARIABLE(value) is_obj_type(value, OBJ_VARIABLE)
#define IS_PROCEDURE(value) is_obj_type(value, OBJ_PROCEDURE)
#define IS_OPERATION(value) is_obj_type(value, OBJ_OPERATION)

#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_VARIABLE(value) ((ObjVariable *)AS_OBJ(value))
#define AS_PROCEDURE(value) ((ObjProcedure *)AS_OBJ(value))
#define AS_OPERATION(value) ((ObjOperation *)AS_OBJ(value))

typedef enum ObjType {
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_STRING,
  OBJ_UPVALUE,
  OBJ_VARIABLE,
  OBJ_PROCEDURE,
  OBJ_OPERATION
} ObjType;

struct Obj {
  ObjType type;
  bool is_marked;
  struct Obj *next;
};

typedef struct ObjFunction {
  Obj obj;
  size_t arity;
  size_t upvalue_count;
  Chunk chunk;
  ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(size_t arg_count, Value *args);

struct ObjString {
  Obj obj;
  size_t length;
  char *chars;
  uint32_t hash;
};

typedef struct {
  Obj obj;
  ObjString *name;
  Value value;
} ObjVariable;

typedef struct {
  Obj obj;
  Value_Array stack;
  ObjString *name;
} ObjProcedure;

typedef struct {
  Obj obj;
  ObjString *type;
} ObjOperation;

typedef struct ObjUpvalue {
  Obj obj;
  Value *location;
  Value closed;
  struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction *function;
  ObjUpvalue **upvalues;
  size_t upvalue_count;
} ObjClosure;

ObjClosure *new_closure(ObjFunction *function);
ObjFunction *new_function();
ObjString *take_string(char *chars, size_t length);
ObjString *copy_string(const char *chars, size_t length, bool str_lit);
ObjVariable *new_variable();
ObjUpvalue *new_upvalue(Value *slot);
ObjProcedure *new_procedure();
ObjOperation *new_operation();
void print_object(Value value);

static inline bool is_obj_type(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
