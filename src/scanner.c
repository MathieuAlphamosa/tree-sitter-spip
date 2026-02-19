/**
 * External scanner for tree-sitter-spip.
 *
 * Two external tokens:
 *   CONTENT_CHAR — one character of HTML/text content (not SPIP)
 *   SPIP_WS     — whitespace between SPIP sub-tokens (criteria, filters, etc.)
 *
 * The key insight: the grammar declares `$._spip_ws` in positions where
 * whitespace is expected *inside* SPIP constructs (between criteria, between
 * filters, before closing `>`).  The scanner checks `valid_symbols[SPIP_WS]`
 * to know when the parser is *inside* a SPIP construct and expects whitespace.
 *
 * When SPIP_WS is valid:
 *   - If current char is whitespace AND followed by a SPIP continuation
 *     token ({, |, ), *, >) → consume whitespace, emit SPIP_WS
 *   - Otherwise fall through to CONTENT_CHAR logic
 *
 * When only CONTENT_CHAR is valid:
 *   - If current char starts a SPIP construct → return false
 *   - If current char is a grammar-reserved token ({, }, |, ), *) → return false
 *   - Otherwise → consume one char, emit CONTENT_CHAR
 */

#include "tree_sitter/parser.h"

enum TokenType {
  CONTENT_CHAR,
  SPIP_WS,
};

void *tree_sitter_spip_external_scanner_create(void) {
  return NULL;
}

void tree_sitter_spip_external_scanner_destroy(void *payload) {
  (void)payload;
}

unsigned tree_sitter_spip_external_scanner_serialize(void *payload,
                                                     char *buffer) {
  (void)payload;
  (void)buffer;
  return 0;
}

void tree_sitter_spip_external_scanner_deserialize(void *payload,
                                                   const char *buffer,
                                                   unsigned length) {
  (void)payload;
  (void)buffer;
  (void)length;
}

/**
 * Check if a character is whitespace.
 */
static bool is_ws(int32_t c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * Return true if the current position looks like the start of a SPIP
 * construct that the grammar should parse instead of content.
 */
static bool at_spip_start(TSLexer *lexer) {
  int32_t c = lexer->lookahead;

  switch (c) {
    case '<': {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      int32_t c2 = lexer->lookahead;

      if (c2 == 'B') {
        lexer->advance(lexer, false);
        int32_t c3 = lexer->lookahead;
        if (c3 == 'O' || c3 == '_') return true;
        return false;
      }
      if (c2 == '/') {
        lexer->advance(lexer, false);
        int32_t c3 = lexer->lookahead;
        if (c3 == 'B') {
          lexer->advance(lexer, false);
          int32_t c4 = lexer->lookahead;
          if (c4 == 'O' || c4 == '_') return true;
          return false;
        }
        if (c3 == '/') {
          lexer->advance(lexer, false);
          if (lexer->lookahead == 'B') return true;
          return false;
        }
        if (c3 == 'm') {
          lexer->advance(lexer, false);
          if (lexer->lookahead == 'u') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == 'l') {
              lexer->advance(lexer, false);
              if (lexer->lookahead == 't') {
                lexer->advance(lexer, false);
                if (lexer->lookahead == 'i') return true;
              }
            }
          }
          return false;
        }
        return false;
      }
      if (c2 == 'I') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == 'N') return true;
        return false;
      }
      if (c2 == 'm') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == 'u') return true;
        return false;
      }
      if (c2 == ':') return true;
      return false;
    }

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

    case '[': {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      if (lexer->lookahead == '(') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '#') {
          lexer->advance(lexer, false);
          if (lexer->lookahead == 'R') return true;
        }
      }
      return true;
    }

    case ']':
    case '{':
    case '}':
    case ')':
    case '*':
      return true;

    case '|': {
      // Only block | when followed by a filter name character.
      // This avoids blocking | used as a visual separator in HTML (e.g. "link | link").
      // SPIP filter names start with: a-z, A-Z, _, !, =, <, >, ?, *
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      int32_t c2 = lexer->lookahead;
      if ((c2 >= 'a' && c2 <= 'z') || (c2 >= 'A' && c2 <= 'Z') ||
          c2 == '_' || c2 == '!' || c2 == '=' || c2 == '<' ||
          c2 == '>' || c2 == '?' || c2 == '*' || c2 == '{') {
        return true;
      }
      return false;
    }

    default:
      return false;
  }
}

bool tree_sitter_spip_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
  (void)payload;

  if (lexer->eof(lexer)) {
    return false;
  }

  // ── SPIP_WS: whitespace inside SPIP constructs ──
  // The parser requests SPIP_WS when it's inside a loop_open, balise,
  // include_tag, etc. and expects optional whitespace before the next
  // sub-token ({criteria}, |filter, >, etc.).
  if (valid_symbols[SPIP_WS] && is_ws(lexer->lookahead)) {
    lexer->mark_end(lexer);

    // Consume all contiguous whitespace
    while (is_ws(lexer->lookahead) && !lexer->eof(lexer)) {
      lexer->advance(lexer, false);
    }

    // Check what follows
    int32_t next = lexer->lookahead;
    bool followed_by_spip = false;

    switch (next) {
      case '{':
      case '|':
      case ')':
      case '*':
      case '>':
        followed_by_spip = true;
        break;
      default:
        // Check for SPIP construct starts too (e.g. #TAG after whitespace)
        followed_by_spip = at_spip_start(lexer);
        break;
    }

    if (followed_by_spip) {
      // Emit the whitespace as SPIP_WS
      lexer->mark_end(lexer);
      lexer->result_symbol = SPIP_WS;
      return true;
    }

    // Whitespace not followed by SPIP token.
    // Return false — the parser will try other alternatives.
    // Tree-sitter will retry at the original position.
    return false;
  }

  // ── CONTENT_CHAR: one character of non-SPIP content ──
  if (!valid_symbols[CONTENT_CHAR]) {
    return false;
  }

  if (at_spip_start(lexer)) {
    return false;
  }

  // Not a SPIP construct — consume one character as content
  lexer->mark_end(lexer);
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = CONTENT_CHAR;
  return true;
}
