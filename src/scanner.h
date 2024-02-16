
#pragma once

#include <stddef.h>

typedef enum TokenType {
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_CARROT,
  TOKEN_MINUS,
  TOKEN_PLUS,
  TOKEN_SLASH,
  TOKEN_STAR,
  TOKEN_MOD,

  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,
  TOKEN_QUESTION,

  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NUMBER,

  TOKEN_AND,
  TOKEN_FALSE,
  TOKEN_IF,
  TOKEN_OR,
  TOKEN_TRUE,
  TOKEN_WHILE,
  TOKEN_ARROW,
  TOKEN_COLON,

  TOKEN_ERROR,
  TOKEN_EOF
} TokenType;

typedef struct Token {
  TokenType type;
  const char *start;
  size_t length;
  size_t line;
} Token;

void init_scanner(const char *source);
Token scan_token();
