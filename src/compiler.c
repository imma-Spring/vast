
#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif /* ifdef DEBUG_PRINT_CODE */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Parser {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
} Parser;

typedef enum FunctionType { TYPE_SCRIPT } FunctionType;

typedef struct Compiler {
  ObjFunction *function;
  FunctionType type;

} Compiler;

Parser parser;
Compiler *current = NULL;

static Chunk *current_chunk() { return &current->function->chunk; }

static void error_at(Token *token, const char *message) {
  if (parser.panic_mode) {
    return;
  }
  parser.panic_mode = true;
  fprintf(stderr, "[line %zu] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothin
  } else {
    fprintf(stderr, " at '%.*s'", (int)token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.had_error = true;
}

static void error(const char *message) { error_at(&parser.previous, message); }

static void error_at_current(const char *message) {
  error_at(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;
  for (;;) {
    parser.current = scan_token();
    if (parser.current.type != TOKEN_ERROR) {
      break;
    }

    error_at_current(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  error_at_current(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type)) {
    return false;
  }
  advance();
  return true;
}

static Token synthetic_token(const char *text) {
  Token token;
  token.start = text;
  token.length = (size_t)(strlen(text));
  return token;
}

static void emit_byte(uint8_t byte) {
  write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, int8_t byte2) {
  emit_byte(byte1);
  emit_byte(byte2);
}

static void emit_return() { emit_byte(OP_RETURN); }

static uint8_t make_constant(Value value) {
  int constant = add_constant(current_chunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emit_constant(Value value) {
  emit_bytes(OP_CONSTANT, make_constant(value));
}

static void patch_jump(size_t offset) {
  size_t jump = current_chunk()->count - offset - 2;
  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  current_chunk()->code[offset] = (jump >> 8) & 0xff;
  current_chunk()->code[offset + 1] = jump & 0xff;
}

static void init_compiler(Compiler *compiler, FunctionType type) {
  compiler->function = NULL;
  compiler->type = type;
  compiler->function = new_function();
  current = compiler;
  if (type != TYPE_SCRIPT) {
    current->function->name =
        copy_string(parser.previous.start, parser.previous.length, false);
  }
}

static ObjFunction *end_compiler() {
  emit_return();
  ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    disassemble_chunk(current_chunk(), function->name != NULL
                                           ? function->name->chars
                                           : "<script>");
  }

#endif /* ifdef DEBUG_PRINT_CODE                                               \
  if (!parser.had_error) {                                                     \
    disassemble_chunk(current_chunk(), "code");                                \
  } */
  return function;
}

static uint8_t identifier_constant(Token *name) {
  return make_constant(OBJ_VAL(copy_string(name->start, name->length, false)));
}

static bool is_function(Token *token) {
  for (size_t i = 0; i < token->length; ++i) {
    if (!isupper(token->start[i]) && token->start[i] != '_') {
      return false;
    }
  }
  return true;
}

static bool identifiers_equal(Token *a, Token *b) {
  if (a->length != b->length) {
    return false;
  }
  return memcmp(a->start, b->start, a->length) == 0;
}

static void io() {
  if (match(TOKEN_DOT)) {
    emit_byte(OP_PRINT);
  } else if (match(TOKEN_CARROT)) {
    emit_byte(OP_SCAN);
  }
}

static void operator() {
  if (match(TOKEN_PLUS)) {
    emit_byte(OP_ADD);
  } else if (match(TOKEN_MINUS)) {
    emit_byte(OP_SUBTRACT);
  } else if (match(TOKEN_STAR)) {
    emit_byte(OP_MULTIPLY);
  } else if (match(TOKEN_SLASH)) {
    emit_byte(OP_DIVIDE);
  } else if (match(TOKEN_EQUAL_EQUAL)) {
    emit_byte(OP_EQUAL);
  } else if (match(TOKEN_BANG)) {
    emit_byte(OP_NOT);
  } else if (match(TOKEN_BANG_EQUAL)) {
    emit_bytes(OP_EQUAL, OP_NOT);
  } else if (match(TOKEN_LESS)) {
    emit_byte(OP_LESS);
  } else if (match(TOKEN_GREATER)) {
    emit_byte(OP_GREATER);
  } else if (match(TOKEN_LESS_EQUAL)) {
    emit_byte(OP_LESS_EQUAL);
  } else if (match(TOKEN_GREATER_EQUAL)) {
    emit_byte(OP_GREATER_EQUAL);
  } else if (match(TOKEN_QUESTION)) {
    emit_bytes(OP_NOT, OP_NOT);
  } else if (match(TOKEN_MOD)) {
    emit_byte(OP_MOD);
  }
}

static void conditional() {
  if (match(TOKEN_IF)) {
    emit_byte(OP_IF);
  } else if (match(TOKEN_WHILE)) {
    Token token = synthetic_token("while_condition");
    emit_bytes(OP_DEFINE_FUNCTION, identifier_constant(&token));
    emit_byte(OP_WHILE);
  }
}

static void parenthesis() {
  advance();
  if (match(TOKEN_RIGHT_PAREN)) {
    error_at_current("Must have operator before closing ')'.");
  }
  switch (parser.current.type) {
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_EQUAL:
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_BANG_EQUAL:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_BANG:
  case TOKEN_QUESTION:
  case TOKEN_COMMA:
  case TOKEN_DOT:
  case TOKEN_CARROT:
  case TOKEN_IF:
  case TOKEN_MOD:
    emit_bytes(OP_PUSH_OPERATION, identifier_constant(&parser.current));
    advance();
    break;
  default:
    error_at_current("operator is not allowed in '('_')'.");
    break;
  }
  consume(TOKEN_RIGHT_PAREN, "Missing closing ')'.");
}

static void variable() {
  emit_bytes(OP_VARIABLE, identifier_constant(&parser.current));
  advance();
}

static void function() {
  advance();
  emit_bytes(OP_DEFINE_FUNCTION, identifier_constant(&parser.current));
  advance();
}

static void instruction() {
  switch (parser.current.type) {
  case TOKEN_DOT:
  case TOKEN_CARROT:
    io();
    break;
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_BANG_EQUAL:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_BANG:
  case TOKEN_MOD:
  case TOKEN_QUESTION:
    operator();
    break;
  case TOKEN_LEFT_PAREN:
    parenthesis();
    break;
  case TOKEN_NUMBER: {
    advance();
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
    break;
  }
  case TOKEN_IDENTIFIER:
    variable();
    break;
  case TOKEN_COLON:
    advance();
    emit_constant(NIL_VAL);
    break;
  case TOKEN_EQUAL:
    advance();
    emit_byte(OP_SET_VARIABLE);
    break;
  case TOKEN_ARROW:
    function();
    break;
  case TOKEN_COMMA:
    emit_byte(OP_APPLY);
    advance();
    break;
  case TOKEN_STRING: {
    emit_constant(OBJ_VAL(copy_string(parser.current.start + 1,
                                      parser.current.length - 2, true)));
    advance();
    break;
  }
  case TOKEN_IF:
  case TOKEN_WHILE:
    conditional();
    break;

  default:
    error_at_current("Token not allowed.");
    advance();
  }
}

ObjFunction *compile(const char *source) {
  init_scanner(source);
  Compiler compiler;
  init_compiler(&compiler, TYPE_SCRIPT);

  parser.had_error = false;
  parser.panic_mode = false;

  advance();
  while (!match(TOKEN_EOF)) {
    instruction();
  }
  ObjFunction *function = end_compiler();
  return parser.had_error ? NULL : function;
}

void mark_compiler_roots() {
  Compiler *compiler = current;
  if (compiler != NULL) {
    mark_object((Obj *)compiler->function);
  }
}
