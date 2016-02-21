#include <ruby.h>
#include "html_tokenizer.h"

struct scan_t {
  VALUE source;
  uint8_t *string;
  uint32_t cursor;
  uint32_t length;
  uint8_t attribute_value_start;
  int found_attribute;
  int is_script, is_textarea, is_closing_tag;
  VALUE last_token;
};

static VALUE text_symbol,
  comment_start_symbol,
  comment_end_symbol,
  tag_start_symbol,
  cdata_start_symbol,
  cdata_end_symbol,
  whitespace_symbol,
  attribute_name_symbol,
  slash_symbol,
  equal_symbol,
  tag_end_symbol,
  tag_start_symbol,
  attribute_value_start_symbol,
  attribute_value_end_symbol,
  attribute_unquoted_value_symbol
;

static void tokenizer_mark(void *ptr)
{}

static void tokenizer_free(void *ptr)
{
  struct tokenizer_t *tokenizer = ptr;
  xfree(tokenizer);
}

static size_t tokenizer_memsize(const void *ptr)
{
  return ptr ? sizeof(struct tokenizer_t) : 0;
}

const rb_data_type_t tokenizer_data_type = {
  "html_tokenizer_tokenizer",
  { tokenizer_mark, tokenizer_free, tokenizer_memsize, },
#if defined(RUBY_TYPED_FREE_IMMEDIATELY)
  NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
#endif
};

static VALUE tokenizer_allocate(VALUE klass)
{
  VALUE obj;
  struct tokenizer_t *tokenizer;

  obj = TypedData_Make_Struct(klass, struct tokenizer_t, &tokenizer_data_type, tokenizer);

  memset((void *)&tokenizer->context, TOKENIZER_NONE, sizeof(struct tokenizer_t));

  return obj;
}

static VALUE tokenizer_initialize_method(VALUE self)
{
  struct tokenizer_t *tk;

  Tokenizer_Get_Struct(self, tk);

  tk->current_context = 0;
  tk->context[0] = TOKENIZER_DOCUMENT;

  return Qnil;
}

static inline int eos(struct scan_t *scan)
{
  return scan->cursor >= scan->length;
}

static inline int length_remaining(struct scan_t *scan)
{
  return scan->length - scan->cursor;
}

static inline void push_context(struct tokenizer_t *tk, enum tokenizer_context ctx)
{
  tk->context[++tk->current_context] = ctx;
}

static inline void pop_context(struct tokenizer_t *tk)
{
  tk->context[tk->current_context--] = TOKENIZER_NONE;
}

static int scan_once(struct tokenizer_t *tk, struct scan_t *scan);

static inline void yield(struct scan_t *scan, VALUE sym, uint32_t length)
{
  scan->last_token = sym;
  rb_yield_values(3, sym, INT2NUM(scan->cursor), INT2NUM(scan->cursor + length));
  scan->cursor += length;
}

static int is_text(struct scan_t *scan, uint32_t *length)
{
  uint32_t i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(scan->string[i] == '<')
      break;
  }
  return *length != 0;
}

static int scan_document(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0;
  if(is_text(scan, &length)) {
    yield(scan, text_symbol, length);
    return 1;
  }
  else {
    push_context(tk, TOKENIZER_HTML);
    return scan_once(tk, scan);
  }
}

static inline int is_comment_start(struct scan_t *scan)
{
  return (length_remaining(scan) >= 4) &&
    !strncmp((const char *)&scan->string[scan->cursor], "<!--", 4);
}

static inline int is_doctype(struct scan_t *scan)
{
  return (length_remaining(scan) >= 9) &&
    !strncasecmp((const char *)&scan->string[scan->cursor], "<!DOCTYPE", 9);
}

static inline int is_cdata_start(struct scan_t *scan)
{
  return (length_remaining(scan) >= 9) &&
    !strncasecmp((const char *)&scan->string[scan->cursor], "<![CDATA[", 9);
}

static inline int is_char(struct scan_t *scan, const char c)
{
  return (length_remaining(scan) >= 1) && (scan->string[scan->cursor] == c);
}

static inline int is_alnum(const char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int is_tag_start(struct scan_t *scan, uint32_t *length,
  int *closing_tag, uint8_t **tag_name, uint32_t *tag_name_length)
{
  uint32_t i, start;

  if(scan->string[scan->cursor] != '<')
    return 0;

  *length = 1;

  if(scan->string[scan->cursor+1] == '/') {
    *closing_tag = 1;
    (*length)++;
  } else {
    *closing_tag = 0;
  }

  *tag_name = &scan->string[scan->cursor + (*length)];
  start = *length;
  for(i = scan->cursor + (*length);i < scan->length; i++, (*length)++) {
    if(!is_alnum(scan->string[i]) && scan->string[i] != ':')
      break;
  }

  *tag_name_length = *length - start;
  return *length > start;
}

static int scan_html(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0, tag_name_length = 0;
  uint8_t *tag_name = NULL;

  if(eos(scan)) {
    return 0;
  }
  else if(is_comment_start(scan)) {
    yield(scan, comment_start_symbol, 4);
    push_context(tk, TOKENIZER_COMMENT);
    return 1;
  }
  else if(is_doctype(scan)) {
    yield(scan, tag_start_symbol, 9);
    push_context(tk, TOKENIZER_ATTRIBUTES);
    return 1;
  }
  else if(is_cdata_start(scan)) {
    yield(scan, cdata_start_symbol, 9);
    push_context(tk, TOKENIZER_CDATA);
    return 1;
  }
  else if(is_tag_start(scan, &length, &scan->is_closing_tag, &tag_name, &tag_name_length)) {
    yield(scan, tag_start_symbol, length);
    scan->is_script = scan->is_textarea = 0;
    if(!strncasecmp((const char *)tag_name, "script", tag_name_length)) {
      scan->is_script = 1;
    } else if(!strncasecmp((const char *)tag_name, "textarea", tag_name_length)) {
      scan->is_textarea = 1;
    }
    push_context(tk, TOKENIZER_ATTRIBUTES);
    return 1;
  }
  else if(is_char(scan, '>')) {
    yield(scan, tag_end_symbol, 1);
    if(scan->is_script) {
      push_context(tk, TOKENIZER_SCRIPT_TAG);
    } else if(scan->is_textarea) {
      push_context(tk, TOKENIZER_TEXTAREA_TAG);
    }
    return 1;
  }
  else if(is_text(scan, &length)) {
    yield(scan, text_symbol, length);
    return 1;
  }
  return 0;
}

static int is_whitespace(struct scan_t *scan, uint32_t *length)
{
  uint32_t i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(scan->string[i] != ' ' && scan->string[i] != '\t' &&
        scan->string[i] != '\r' && scan->string[i] != '\n')
      break;
  }
  return *length != 0;
}

static int is_attr_name(struct scan_t *scan, uint32_t *length)
{
  uint32_t i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(!is_alnum(scan->string[i]) && scan->string[i] != ':' &&
        scan->string[i] != '-' && scan->string[i] != '_' &&
        scan->string[i] != '.')
      break;
  }
  return *length != 0;
}

static int scan_attributes(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0;

  if(eos(scan)) {
    return 0;
  }
  else if(is_whitespace(scan, &length)) {
    yield(scan, whitespace_symbol, length);
    return 1;
  }
  else if(is_char(scan, '=')) {
    yield(scan, equal_symbol, 1);
    scan->found_attribute = 0;
    push_context(tk, TOKENIZER_ATTRIBUTE_VALUE);
    return 1;
  }
  else if(is_char(scan, '/')) {
    yield(scan, slash_symbol, 1);
    return 1;
  }
  else if(is_char(scan, '>')) {
    pop_context(tk);
    return 1;
  }
  else if(is_char(scan, '\'') || is_char(scan, '"')) {
    scan->attribute_value_start = scan->string[scan->cursor];
    yield(scan, attribute_value_start_symbol, 1);
    push_context(tk, TOKENIZER_ATTRIBUTE_STRING);
    return 1;
  }
  else if(is_attr_name(scan, &length)) {
    yield(scan, attribute_name_symbol, length);
    push_context(tk, TOKENIZER_ATTRIBUTE_NAME);
    return 1;
  }
  return 0;
}

static int scan_attribute_name(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0;

  if(eos(scan)) {
    return 0;
  }
  else if(is_attr_name(scan, &length)) {
    yield(scan, attribute_name_symbol, length);
    return 1;
  }
  else if(is_whitespace(scan, &length)) {
    pop_context(tk);
    return 1;
  }
  else if(is_char(scan, '/') || is_char(scan, '>') || is_char(scan, '=')) {
    pop_context(tk);
    return 1;
  }
  return 0;
}

static int is_unquoted_value(struct scan_t *scan, uint32_t *length)
{
  uint32_t i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(scan->string[i] == ' ' || scan->string[i] == '\r' ||
        scan->string[i] == '\n' || scan->string[i] == '\t' ||
        scan->string[i] == '/' || scan->string[i] == '>')
      break;
  }
  return *length != 0;
}

static int scan_attribute_value(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0;

  if(scan->last_token == attribute_value_end_symbol) {
    pop_context(tk);
    return 1;
  }
  else if(eos(scan)) {
    return 0;
  }
  else if(is_char(scan, '/') || is_char(scan, '>')) {
    pop_context(tk);
    return 1;
  }
  else if(is_whitespace(scan, &length)) {
    yield(scan, whitespace_symbol, length);
    if(scan->found_attribute)
      pop_context(tk);
    return 1;
  }
  else if(is_char(scan, '\'') || is_char(scan, '"')) {
    scan->attribute_value_start = scan->string[scan->cursor];
    yield(scan, attribute_value_start_symbol, 1);
    push_context(tk, TOKENIZER_ATTRIBUTE_STRING);
    scan->found_attribute = 1;
    return 1;
  }
  else if(is_unquoted_value(scan, &length)) {
    yield(scan, attribute_unquoted_value_symbol, length);
    scan->found_attribute = 1;
    return 1;
  }
  return 0;
}

static int is_attr_string(struct scan_t *scan, uint32_t *length)
{
  uint32_t i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(scan->string[i] == scan->attribute_value_start)
      break;
  }
  return *length != 0;
}

static int scan_attribute_string(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0;

  if(eos(scan)) {
    return 0;
  }
  else if(is_char(scan, scan->attribute_value_start)) {
    yield(scan, attribute_value_end_symbol, 1);
    pop_context(tk);
    return 1;
  }
  else if(is_attr_string(scan, &length)) {
    yield(scan, text_symbol, length);
    return 1;
  }
  return 0;
}

static int is_comment_end(struct scan_t *scan, uint32_t *length)
{
  uint32_t i;

  *length = 0;
  for(i = scan->cursor;i < (scan->length - 2); i++, (*length)++) {
    if(scan->string[i] == '-' && scan->string[i+1] == '-' &&
        scan->string[i+2] == '>') {
      break;
    }
  }
  return *length != 0;
}

static int scan_comment(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0;

  if(eos(scan)) {
    return 0;
  }
  else if(is_comment_end(scan, &length)) {
    yield(scan, text_symbol, length);
    yield(scan, comment_end_symbol, 3);
    return 1;
  }
  else {
    yield(scan, text_symbol, scan->length - scan->cursor);
    return 1;
  }
  return 0;
}

static int is_cdata_end(struct scan_t *scan, uint32_t *length)
{
  uint32_t i;

  *length = 0;
  for(i = scan->cursor;i < (scan->length - 2); i++, (*length)++) {
    if(scan->string[i] == ']' && scan->string[i+1] == ']' &&
        scan->string[i+2] == '>') {
      break;
    }
  }
  return *length != 0;
}

static int scan_cdata(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0;

  if(eos(scan)) {
    return 0;
  }
  else if(is_cdata_end(scan, &length)) {
    yield(scan, text_symbol, length);
    yield(scan, cdata_end_symbol, 3);
    return 1;
  }
  else {
    yield(scan, text_symbol, scan->length - scan->cursor);
    return 1;
  }
  return 0;
}

static int scan_script_tag(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0, tag_name_length = 0;
  uint8_t *tag_name = NULL;
  int closing_tag = 0;

  if(eos(scan)) {
    return 0;
  }
  else if(is_tag_start(scan, &length, &closing_tag, &tag_name, &tag_name_length)) {
    if(closing_tag && !strncasecmp((const char *)tag_name, "script", tag_name_length)) {
      pop_context(tk);
    } else {
      yield(scan, text_symbol, length);
    }
    return 1;
  }
  else if(is_text(scan, &length)) {
    yield(scan, text_symbol, length);
    return 1;
  }
  else {
    yield(scan, text_symbol, scan->length - scan->cursor);
    return 1;
  }
  return 0;
}

static int scan_textarea_tag(struct tokenizer_t *tk, struct scan_t *scan)
{
  uint32_t length = 0, tag_name_length = 0;
  uint8_t *tag_name = NULL;
  int closing_tag = 0;

  if(eos(scan)) {
    return 0;
  }
  else if(is_tag_start(scan, &length, &closing_tag, &tag_name, &tag_name_length)) {
    if(closing_tag && !strncasecmp((const char *)tag_name, "textarea", tag_name_length)) {
      pop_context(tk);
    } else {
      yield(scan, text_symbol, length);
    }
    return 1;
  }
  else if(is_text(scan, &length)) {
    yield(scan, text_symbol, length);
    return 1;
  }
  else {
    yield(scan, text_symbol, scan->length - scan->cursor);
    return 1;
  }
  return 0;
}

static int scan_once(struct tokenizer_t *tk, struct scan_t *scan)
{
  switch(tk->context[tk->current_context]) {
  case TOKENIZER_DOCUMENT:
    return scan_document(tk, scan);
  case TOKENIZER_HTML:
    return scan_html(tk, scan);
  case TOKENIZER_COMMENT:
    return scan_comment(tk, scan);
  case TOKENIZER_CDATA:
    return scan_cdata(tk, scan);
  case TOKENIZER_SCRIPT_TAG:
    return scan_script_tag(tk, scan);
  case TOKENIZER_TEXTAREA_TAG:
    return scan_textarea_tag(tk, scan);
  case TOKENIZER_ATTRIBUTES:
    return scan_attributes(tk, scan);
  case TOKENIZER_ATTRIBUTE_NAME:
    return scan_attribute_name(tk, scan);
  case TOKENIZER_ATTRIBUTE_VALUE:
    return scan_attribute_value(tk, scan);
  case TOKENIZER_ATTRIBUTE_STRING:
    return scan_attribute_string(tk, scan);
  }
  return 0;
}

static void scan_all(struct tokenizer_t *tk, struct scan_t *scan)
{
  while(!eos(scan) && scan_once(tk, scan)) {}
  if(!eos(scan)) {
    // unparseable data at eof.
  }
  return;
}

static VALUE tokenizer_tokenize_method(VALUE self, VALUE source)
{
  struct tokenizer_t *tk = NULL;
  struct scan_t scan;

  Check_Type(source, T_STRING);
  Tokenizer_Get_Struct(self, tk);

  scan.source = source;
  scan.string = RSTRING_PTR(source);
  scan.cursor = 0;
  scan.length = RSTRING_LEN(source);

  scan.is_script = scan.is_textarea = 0;
  scan.is_closing_tag = 0;

  scan_all(tk, &scan);

  return Qnil;
}

void Init_html_tokenizer()
{
  VALUE mHtmlTokenizer = rb_define_module("HtmlTokenizer");
  VALUE cTokenizer = rb_define_class_under(mHtmlTokenizer, "Tokenizer", rb_cObject);
  rb_define_alloc_func(cTokenizer, tokenizer_allocate);
  rb_define_method(cTokenizer, "initialize", tokenizer_initialize_method, 0);
  rb_define_method(cTokenizer, "tokenize", tokenizer_tokenize_method, 1);

  text_symbol = ID2SYM(rb_intern("text"));
  comment_start_symbol = ID2SYM(rb_intern("comment_start"));
  comment_end_symbol = ID2SYM(rb_intern("comment_end"));
  tag_start_symbol = ID2SYM(rb_intern("tag_start"));
  cdata_start_symbol = ID2SYM(rb_intern("cdata_start"));
  cdata_end_symbol = ID2SYM(rb_intern("cdata_end"));
  whitespace_symbol = ID2SYM(rb_intern("whitespace"));
  attribute_name_symbol = ID2SYM(rb_intern("attribute_name"));
  slash_symbol = ID2SYM(rb_intern("slash"));
  equal_symbol = ID2SYM(rb_intern("equal"));
  tag_end_symbol = ID2SYM(rb_intern("tag_end"));
  tag_start_symbol = ID2SYM(rb_intern("tag_start"));
  attribute_value_start_symbol = ID2SYM(rb_intern("attribute_value_start"));
  attribute_value_end_symbol = ID2SYM(rb_intern("attribute_value_end"));
  attribute_unquoted_value_symbol = ID2SYM(rb_intern("attribute_unquoted_value"));
}
