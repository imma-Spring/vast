
#include "scanner.h"
#include "common.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct Scanner {
  const char *start;
  const char *current;
  size_t line;
} Scanner;

Scanner scanner;

void init_scanner(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_at_end() { return *scanner.current == '\0'; }

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static char peek() { return *scanner.current; }

static char peek_next() {
  if (is_at_end()) {
    return '\0';
  }
  return scanner.current[1];
}

static bool match(char expected) {
  if (is_at_end()) {
    return false;
  }
  if (*scanner.current != expected) {
    return false;
  }
  scanner.current++;
  return true;
}

static Token make_token(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (size_t)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

static Token error_token(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (size_t)strlen(message);
  token.line = scanner.line;
  return token;
}

static void skip_whitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    case '\n':
      scanner.line++;
      advance();
      break;
    case '/':
      if (peek_next() == '/') {
        while (peek() != '\n' && !is_at_end()) {
          advance();
        }
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
}

static TokenType check_keyword(size_t start, size_t length, const char *rest,
                               TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }
  return TOKEN_IDENTIFIER;
}

static TokenType identifier_type() {
  switch (scanner.start[0]) {
  case 'a':
    return check_keyword(1, 2, "nd", TOKEN_AND);
  case 'i':
    return check_keyword(1, 1, "f", TOKEN_IF);
  case 'o':
    return check_keyword(1, 1, "r", TOKEN_OR);
  case 'w':
    return check_keyword(1, 4, "hile", TOKEN_WHILE);
  case 'f':
    return check_keyword(1, 4, "alse", TOKEN_FALSE);
  case 't':
    return check_keyword(1, 3, "rue", TOKEN_TRUE);
  }
  return TOKEN_IDENTIFIER;
}

static Token identifier() {
  while (is_alpha(peek()) || is_digit(peek())) {
    advance();
  }
  return make_token(identifier_type());
}

static Token number() {
  while (is_digit(peek())) {
    advance();
  }

  if (peek() == '.' && is_digit(peek_next())) {
    advance();
    while (is_digit(peek())) {
      advance();
    }
  }
  return make_token(TOKEN_NUMBER);
}

static Token string() {
  while ((peek() != '\'') && !is_at_end()) {
    if (peek() == '\n') {
      scanner.line++;
    }
    if (peek() == '\\') {
      switch (peek_next()) {
      case '\\':
      case '\'':
      case '\r':
      case '\n':
      case '\t':
        advance();
        break;
      }
    }
    advance();
  }

  if (is_at_end()) {
    return error_token("Unterminated string.");
  }

  advance();
  return make_token(TOKEN_STRING);
}

Token scan_token() {
  skip_whitespace();
  scanner.start = scanner.current;
  if (is_at_end()) {
    return make_token(TOKEN_EOF);
  }

  char c = advance();
  if (is_digit(c)) {
    return number();
  }
  if (is_alpha(c)) {
    return identifier();
  }

  switch (c) {
  case '(':
    return make_token(TOKEN_LEFT_PAREN);
  case ')':
    return make_token(TOKEN_RIGHT_PAREN);
  case ',':
    return make_token(TOKEN_COMMA);
  case '.':
    return make_token(TOKEN_DOT);
  case '%':
    return make_token(TOKEN_MOD);
  case '+':
    return make_token(TOKEN_PLUS);
  case '-':
    return make_token(TOKEN_MINUS);
  case '*':
    return make_token(TOKEN_STAR);
  case '/':
    return make_token(TOKEN_SLASH);
  case '^':
    return make_token(TOKEN_CARROT);
  case ':':
    return make_token(TOKEN_COLON);
  case '=':
    return make_token(match('>') ? TOKEN_ARROW : TOKEN_EQUAL);
  case '?':
    return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_QUESTION);
  case '!':
    return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '<':
    return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '\'':
    return string();
  }

  return error_token("Unexpected character.");
}
