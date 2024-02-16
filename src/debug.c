
#include "debug.h"
#include "chunk.h"
#include "object.h"
#include "value.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void disassemble_chunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);
  for (size_t offset = 0; offset < chunk->count;) {
    offset = disassemble_instruction(chunk, offset);
  }
}

static size_t simple_instruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static size_t byte_instruction(const char *name, Chunk *chunk, size_t offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static size_t jump_instruction(const char *name, int sign, Chunk *chunk,
                               size_t offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4zu -> %zu", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

static size_t constant_instruction(const char *name, Chunk *chunk,
                                   size_t offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  print_value(chunk->constants.value[constant]);
  printf("'\n");
  return offset + 2;
}

static size_t invoke_instruction(const char *name, Chunk *chunk,
                                 size_t offset) {
  uint8_t constant = chunk->code[offset + 1];
  uint8_t arg_count = chunk->code[offset + 2];
  printf("%-16s (%d args) %d '", name, arg_count, constant);
  print_value(chunk->constants.value[constant]);
  printf("'\n");
  return offset + 3;
}

size_t disassemble_instruction(Chunk *chunk, size_t offset) {
  printf("%04zu ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4zu ", chunk->lines[offset]);
  }

  switch (chunk->code[offset]) {
  case OP_CONSTANT:
    return constant_instruction("OP_CONSTANT", chunk, offset);
  case OP_ADD:
    return simple_instruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simple_instruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simple_instruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simple_instruction("OP_DIVIDE", offset);
  case OP_PRINT:
    return simple_instruction("OP_PRINT", offset);
  case OP_SCAN:
    return simple_instruction("OP_SCAN", offset);
  case OP_APPLY:
    return simple_instruction("OP_APPLY", offset);
  case OP_RETURN:
    return simple_instruction("OP_RETURN", offset);
  case OP_PUSH_OPERATION:
    return constant_instruction("OP_PUSH_OPERATION", chunk, offset);
  case OP_VARIABLE:
    return constant_instruction("OP_VARIABLE", chunk, offset);
  case OP_SET_VARIABLE:
    return simple_instruction("OP_SET_VARIABLE", offset);
  case OP_DEFINE_FUNCTION:
    return constant_instruction("OP_DEFINE_FUNCTION", chunk, offset);
  case OP_EQUAL:
    return simple_instruction("OP_EQUAL", offset);
  case OP_GREATER:
    return simple_instruction("OP_GREATER", offset);
  case OP_LESS:
    return simple_instruction("OP_LESS", offset);
  case OP_GREATER_EQUAL:
    return simple_instruction("OP_GREATER_EQUAL", offset);
  case OP_LESS_EQUAL:
    return simple_instruction("OP_LESS_EQUAL", offset);
  case OP_NOT:
    return simple_instruction("OP_NOT", offset);
  case OP_WHILE:
    return simple_instruction("OP_WHILE", offset);
  case OP_IF:
    return simple_instruction("OP_IF", offset);
  case OP_MOD:
    return simple_instruction("OP_MOD", offset);
  default:
    printf("Unkown opcode %d\n", chunk->code[offset]);
    return offset - 1;
  }
}
