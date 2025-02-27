#include "tree_sitter/alloc.h"
#include "tree_sitter/array.h"
#include "tree_sitter/parser.h"
#include <string.h>
#include <wctype.h>

typedef Array(uint32_t) String;

typedef Array(String) Vector;

typedef struct State {
  Vector *brackets;
  Vector *tags;
} State;

enum TokenType {
  START_TAG_NAME,
  END_TAG_NAME,
  ERRONEOUS_PAIR,
  LITERAL,
  BRACKET_OPEN,
  BRACKET_CLOSE,
  END,
};

// static void print_string(String *s) {
//   for (uint32_t i = 0; i < s->size; i++) {
//     fprintf(stderr, "%c", s->contents[i]);
//   }
// }

static inline bool string_eq(String *a, String *b) {
  if (a->size != b->size) {
    return false;
  }
  return memcmp(a->contents, b->contents, a->size * sizeof(uint32_t)) == 0;
}

static bool scan_bracket_open(Vector *brackets, TSLexer *lexer) {

  String bracket = array_new();
  switch (lexer->lookahead) {
  case '{':
    array_push(&bracket, lexer->lookahead);
    lexer->mark_end(lexer);
    lexer->advance(lexer, false);

    if (lexer->lookahead == '*') {
      return false;
    }

    lexer->mark_end(lexer);
    break;
  case '(':
  case '[':
    array_push(&bracket, lexer->lookahead);
    lexer->advance(lexer, false);
    break;
  default:
    return false;
  }

  array_push(brackets, bracket);

  lexer->result_symbol = BRACKET_OPEN;

  return true;
}

static bool scan_bracket_close(Vector *brackets, TSLexer *lexer) {

  String bracket = array_new();
  switch (lexer->lookahead) {
  case ')':
    array_push(&bracket, '(');
    break;
  case '}':
    array_push(&bracket, '{');
    break;
  case ']':
    array_push(&bracket, '[');
    break;
  default:
    array_delete(&bracket);
    return false;
  }

  lexer->advance(lexer, false);

  if (brackets->size > 0 && string_eq(array_back(brackets), &bracket)) {
    array_delete(&array_pop(brackets));
    lexer->result_symbol = BRACKET_CLOSE;
  } else {
    lexer->result_symbol = ERRONEOUS_PAIR;
  }

  array_delete(&bracket);
  return lexer->result_symbol == BRACKET_CLOSE;
}

static String scan_tag_name(TSLexer *lexer) {
  String tag_name = array_new();
  while (lexer->lookahead != '(' && lexer->lookahead != ')' &&
         lexer->lookahead != '[' && lexer->lookahead != ']' &&
         lexer->lookahead != '{' && lexer->lookahead != '}' &&
         lexer->lookahead != '\\' && !iswspace(lexer->lookahead) &&
         !(lexer->eof(lexer))) {
    array_push(&tag_name, lexer->lookahead);
    lexer->advance(lexer, false);
  }
  return tag_name;
}

static bool scan_start_tag_name(Vector *tags, TSLexer *lexer) {
  String tag_name = scan_tag_name(lexer);
  if (tag_name.size == 0) {
    array_delete(&tag_name);
    return false;
  }

  lexer->result_symbol = START_TAG_NAME;
  lexer->mark_end(lexer);

  while (lexer->lookahead != '\n') {
    if (lexer->lookahead == '(' || lexer->lookahead == '{' ||
        lexer->lookahead == '[') {
      return true;
    }
    lexer->advance(lexer, false);
  }
  array_push(tags, tag_name);
  return true;
}

static bool scan_end_tag_name(Vector *tags, TSLexer *lexer) {
  String tag_name = scan_tag_name(lexer);
  if (tag_name.size == 0) {
    array_delete(&tag_name);
    return false;
  }

  if (tags->size > 0 && string_eq(array_back(tags), &tag_name)) {
    array_delete(&array_pop(tags));
    lexer->result_symbol = END_TAG_NAME;
  } else {
    lexer->result_symbol = ERRONEOUS_PAIR;
  }

  array_delete(&tag_name);
  return lexer->result_symbol == END_TAG_NAME;
}

static bool scan_literal(TSLexer *lexer) {
  if (lexer->lookahead != '`') {
    return false;
  }

  lexer->advance(lexer, false);
  String literal = array_new();

  while (lexer->lookahead != '`' && !lexer->eof(lexer)) {
    array_push(&literal, lexer->lookahead);
    lexer->advance(lexer, false);
  }
  lexer->advance(lexer, false);
  lexer->result_symbol = LITERAL;
  array_delete(&literal);

  return true;
}

static inline Vector *erase_vector(void *payload) {
  Vector *v = (Vector *)payload;
  for (uint32_t i = 0; i < v->size; ++i) {
    array_delete(array_get(v, i));
  }
  array_delete(v);
  return v;
}
static inline State *erase_state(void *payload) {
  State *state = (State *)payload;

  erase_vector(state->tags);
  erase_vector(state->brackets);

  return state;
}

void *tree_sitter_tek_external_scanner_create() {
  Vector *tags = ts_calloc(1, sizeof(Vector));
  Vector *brackets = ts_calloc(1, sizeof(Vector));

  if (tags == NULL || brackets == NULL)
    abort();

  array_init(tags);
  array_init(brackets);
  State *state = ts_malloc(sizeof(State));
  state->tags = tags;
  state->brackets = brackets;
  return state;
}

void tree_sitter_tek_external_scanner_destroy(void *payload) {
  State *state = erase_state(payload);
  ts_free(state);
}

static unsigned serialize_vector(unsigned m_index, Vector *v, char *buffer) {
  unsigned start = m_index;
  uint8_t count = v->size > UINT8_MAX ? UINT8_MAX : v->size;
  uint8_t serialized_count = 0;
  m_index += sizeof serialized_count;

  memcpy(&buffer[m_index], &count, sizeof count);

  m_index += sizeof count;

  for (; serialized_count < count; ++serialized_count) {
    String *string = array_get(v, serialized_count);

    uint8_t string_length = string->size > UINT8_MAX ? UINT8_MAX : string->size;

    if (m_index + 2 + string_length * sizeof(uint32_t) >=
        TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
      break;
    }

    memcpy(&buffer[m_index], &string_length, sizeof string_length);
    m_index += sizeof string_length;

    if (string_length > 0) {
      memcpy(&buffer[m_index], string->contents,
             string_length * sizeof(uint32_t));
      m_index += string_length * sizeof(uint32_t);
    }

    array_delete(string);
  }

  memcpy(&buffer[start], &serialized_count, sizeof serialized_count);
  return m_index;
}

unsigned tree_sitter_tek_external_scanner_serialize(void *payload,
                                                    char *buffer) {
  State *state = (State *)payload;
  unsigned m_index = 0;
  m_index = serialize_vector(m_index, state->brackets, buffer);
  m_index = serialize_vector(m_index, state->tags, buffer);
  return m_index;
}

static unsigned deserialize_vector(unsigned m_index, Vector *v,
                                   const char *buffer) {
  uint8_t count = 0;
  uint8_t serialized_count = 0;

  memcpy(&serialized_count, &buffer[m_index], sizeof serialized_count);
  m_index += sizeof serialized_count;
  memcpy(&count, &buffer[m_index], sizeof count);
  m_index += sizeof count;

  if (count == 0)
    return m_index;

  uint8_t remainig = count - serialized_count;

  array_reserve(v, count);

  for (; serialized_count > 0; serialized_count--) {
    String string = array_new();
    memcpy(&string.size, &buffer[m_index], sizeof(uint8_t));
    m_index++;

    if (string.size > 0) {
      array_reserve(&string, string.size + 1);
      memcpy(string.contents, &buffer[m_index], string.size * sizeof(uint32_t));
      m_index += string.size * sizeof(uint32_t);
    }
    array_push(v, string);
  }
  for (; remainig > 0; remainig--) {
    String string = array_new();
    array_push(v, string);
  }
  return m_index;
}

void tree_sitter_tek_external_scanner_deserialize(void *payload,
                                                  const char *buffer,
                                                  unsigned length) {
  State *state = erase_state(payload);
  if (length == 0)
    return;

  unsigned m_index = 0;
  m_index = deserialize_vector(m_index, state->brackets, buffer);
  m_index = deserialize_vector(m_index, state->tags, buffer);
}

bool tree_sitter_tek_external_scanner_scan(void *payload, TSLexer *lexer,
                                           const bool *valid_symbols) {
  State *state = (State *)payload;

  if (valid_symbols[END]) {
    return lexer->eof(lexer);
  }

  if (valid_symbols[ERRONEOUS_PAIR]) {
    return false;
  }

  while (iswspace(lexer->lookahead)) {
    lexer->advance(lexer, true);
  }

  if (valid_symbols[START_TAG_NAME]) {
    if (scan_start_tag_name(state->tags, lexer)) {
      return true;
    }
  }
  if (valid_symbols[END_TAG_NAME]) {
    if (scan_end_tag_name(state->tags, lexer)) {
      return true;
    }
  }
  if (valid_symbols[LITERAL]) {
    if (scan_literal(lexer)) {
      return true;
    }
  }
  if (valid_symbols[BRACKET_OPEN]) {
    if (scan_bracket_open(state->brackets, lexer)) {
      return true;
    }
  }
  if (valid_symbols[BRACKET_CLOSE]) {
    if (scan_bracket_close(state->brackets, lexer)) {
      return true;
    }
  }

  return false;
}
