/**
 * External scanner for tree-sitter-spip.
 *
 * Two external tokens:
 *   CONTENT_CHAR — one character of HTML/text content (not SPIP)
 *   SPIP_WS     — whitespace inside SPIP constructs (between criteria, etc.)
 */

#include "tree_sitter/parser.h"

enum TokenType {
  CONTENT_CHAR,
  SPIP_WS,
};

void *tree_sitter_spip_external_scanner_create(void) { return NULL; }
void tree_sitter_spip_external_scanner_destroy(void *p) { (void)p; }
unsigned tree_sitter_spip_external_scanner_serialize(void *p, char *b) {
  (void)p; (void)b; return 0;
}
void tree_sitter_spip_external_scanner_deserialize(void *p, const char *b, unsigned n) {
  (void)p; (void)b; (void)n;
}

static bool is_ws(int32_t c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * Check if current position starts a SPIP construct.
 */
static bool at_spip_start(TSLexer *lexer) {
  int32_t c = lexer->lookahead;

  switch (c) {
    case '(': {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      if (lexer->lookahead == '#') return true;
      return false;
    }

    case '#': {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      int32_t c2 = lexer->lookahead;
      if (c2 >= 'A' && c2 <= 'Z') return true;
      return false;
    }

    case '<': {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);

      if (lexer->lookahead == 'B') return true;

      if (lexer->lookahead == 'I') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == 'N') return true;
        return false;
      }

      if (lexer->lookahead == 'm') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == 'u') return true;
        return false;
      }

      if (lexer->lookahead == ':') return true;

      if (lexer->lookahead == '/') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == 'B') return true;
        if (lexer->lookahead == 'm') {
          lexer->advance(lexer, false);
          if (lexer->lookahead == 'u') return true;
          return false;
        }
        if (lexer->lookahead == '/') {
          lexer->advance(lexer, false);
          if (lexer->lookahead == 'B') return true;
        }
        return false;
      }

      return false;
    }

    case '[':
      return true;

    case ']':
      return true;

    default:
      return false;
  }
}

bool tree_sitter_spip_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
  (void)payload;

  if (lexer->eof(lexer)) return false;

  // ── SPIP_WS: whitespace inside SPIP constructs ──
  if (valid_symbols[SPIP_WS] && is_ws(lexer->lookahead)) {
    lexer->mark_end(lexer);

    while (is_ws(lexer->lookahead) && !lexer->eof(lexer)) {
      lexer->advance(lexer, false);
    }

    int32_t next = lexer->lookahead;
    bool followed_by_spip = false;

    switch (next) {
      case '{':
      case '|':
      case ')':
      case '*':
      case '>':
      case '/':
        followed_by_spip = true;
        break;
      default:
        break;
    }

    if (followed_by_spip) {
      lexer->mark_end(lexer);
      lexer->result_symbol = SPIP_WS;
      return true;
    }

    return false;
  }

  // ── CONTENT_CHAR: one character of non-SPIP content ──
  if (!valid_symbols[CONTENT_CHAR]) return false;
  if (at_spip_start(lexer)) return false;

  // Consume one character as content
  lexer->mark_end(lexer);
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = CONTENT_CHAR;
  return true;
}
