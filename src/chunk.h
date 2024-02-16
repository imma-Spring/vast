#pragma once
#include "common.h"
#include "value.h"

typedef enum Op_Code {
  OP_PRINT,
  OP_SCAN,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MOD,

  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_GREATER_EQUAL,
  OP_LESS_EQUAL,
  OP_NOT,

  OP_WHILE,
  OP_IF,

  OP_PUSH_OPERATION,

  OP_VARIABLE,
  OP_SET_VARIABLE,

  OP_DEFINE_FUNCTION,
  OP_APPLY,

  OP_CONSTANT,
  OP_RETURN
} Op_Code;

typedef struct Chunk {
  size_t count;
  size_t capacity;
  uint8_t *code;
  size_t *lines;
  Value_Array constants;
} Chunk;

void init_chunk(Chunk *chunk);
void write_chunk(Chunk *chunk, uint8_t byte, size_t line);
void free_chunk(Chunk *chunk);
size_t add_constant(Chunk *chunk, Value value);
