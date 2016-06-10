#include <ruby.h>
#include "tokenizer.h"

static VALUE cTokenizer = Qnil;

VALUE text_symbol,
  comment_start_symbol,
  comment_end_symbol,
  tag_start_symbol,
  tag_name_symbol,
  cdata_start_symbol,
  cdata_end_symbol,
  whitespace_symbol,
  attribute_name_symbol,
  solidus_symbol,
  equal_symbol,
  tag_end_symbol,
  attribute_value_start_symbol,
  attribute_value_end_symbol,
  attribute_unquoted_value_symbol,
  malformed_symbol
;

static void tokenizer_mark(void *ptr)
{}

static void tokenizer_free(void *ptr)
{
  struct tokenizer_t *tokenizer = ptr;
  if(tokenizer) {
    if(tokenizer->current_tag) {
      xfree(tokenizer->current_tag);
      tokenizer->current_tag = NULL;
    }
    xfree(tokenizer);
  }
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
  struct tokenizer_t *tokenizer = NULL;

  obj = TypedData_Make_Struct(klass, struct tokenizer_t, &tokenizer_data_type, tokenizer);

  memset((void *)&tokenizer->context, TOKENIZER_NONE, sizeof(struct tokenizer_t));

  return obj;
}

void tokenizer_init(struct tokenizer_t *tk)
{
  tk->current_context = 0;
  tk->context[0] = TOKENIZER_HTML;

  tk->attribute_value_start = 0;
  tk->found_attribute = 0;
  tk->current_tag = NULL;
  tk->is_closing_tag = 0;
  tk->last_token = Qnil;
  tk->callback_data = NULL;
  tk->f_callback = NULL;

  return;
}

static void tokenizer_yield_tag(struct tokenizer_t *tk, VALUE sym, long unsigned int length, void *data)
{
  tk->last_token = sym;
  rb_yield_values(3, sym, INT2NUM(tk->scan.cursor), INT2NUM(tk->scan.cursor + length));
}

static void tokenizer_callback(struct tokenizer_t *tk, VALUE sym, long unsigned int length)
{
  if(tk->f_callback)
    tk->f_callback(tk, sym, length, tk->callback_data);
  tk->scan.cursor += length;
}

static VALUE tokenizer_initialize_method(VALUE self)
{
  struct tokenizer_t *tk;

  Tokenizer_Get_Struct(self, tk);
  tokenizer_init(tk);
  tk->f_callback = tokenizer_yield_tag;

  return Qnil;
}

static inline int eos(struct scan_t *scan)
{
  return scan->cursor >= scan->length;
}

static inline long unsigned int length_remaining(struct scan_t *scan)
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

static int is_text(struct scan_t *scan, long unsigned int *length)
{
  long unsigned int i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(scan->string[i] == '<')
      break;
  }
  return *length != 0;
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

static int is_tag_start(struct scan_t *scan, long unsigned int *length,
  int *closing_tag, const char **tag_name, long unsigned int *tag_name_length)
{
  long unsigned int i, start;

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

static int is_tag_name(struct scan_t *scan, const char **tag_name, unsigned long int *tag_name_length)
{
  long unsigned int i;

  *tag_name_length = 0;
  *tag_name = &scan->string[scan->cursor];
  for(i = scan->cursor;i < scan->length; i++, (*tag_name_length)++) {
    if(scan->string[i] == ' ' || scan->string[i] == '\t' ||
        scan->string[i] == '\r' || scan->string[i] == '\n' ||
        scan->string[i] == '>' || scan->string[i] == '/')
      break;
  }

  return *tag_name_length != 0;
}
static int is_whitespace(struct scan_t *scan, unsigned long int *length)
{
  long unsigned int i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(scan->string[i] != ' ' && scan->string[i] != '\t' &&
        scan->string[i] != '\r' && scan->string[i] != '\n')
      break;
  }
  return *length != 0;
}

static int is_attribute_name(struct scan_t *scan, unsigned long int *length)
{
  long unsigned int i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(!is_alnum(scan->string[i]) && scan->string[i] != ':' &&
        scan->string[i] != '-' && scan->string[i] != '_' &&
        scan->string[i] != '.')
      break;
  }
  return *length != 0;
}

static int is_unquoted_value(struct scan_t *scan, unsigned long int *length)
{
  long unsigned int i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(scan->string[i] == ' ' || scan->string[i] == '\r' ||
        scan->string[i] == '\n' || scan->string[i] == '\t' ||
        scan->string[i] == '>')
      break;
  }
  return *length != 0;
}

static int is_attribute_string(struct scan_t *scan, unsigned long int *length, const char attribute_value_start)
{
  long unsigned int i;

  *length = 0;
  for(i = scan->cursor;i < scan->length; i++, (*length)++) {
    if(scan->string[i] == attribute_value_start)
      break;
  }
  return *length != 0;
}

static int is_comment_end(struct scan_t *scan, unsigned long int *length, const char **end)
{
  long unsigned int i;

  *length = 0;
  for(i = scan->cursor;i < (scan->length - 2); i++, (*length)++) {
    if(scan->string[i] == '-' && scan->string[i+1] == '-' &&
        scan->string[i+2] == '>') {
      *end = &scan->string[i];
      break;
    }
  }
  return *length != 0;
}

static int is_cdata_end(struct scan_t *scan, unsigned long int *length, const char **end)
{
  long unsigned int i;

  *length = 0;
  for(i = scan->cursor;i < (scan->length - 2); i++, (*length)++) {
    if(scan->string[i] == ']' && scan->string[i+1] == ']' &&
        scan->string[i+2] == '>') {
      *end = &scan->string[i];
      break;
    }
  }
  return *length != 0;
}

static int scan_html(struct tokenizer_t *tk)
{
  long unsigned int length = 0;

  if(is_comment_start(&tk->scan)) {
    tokenizer_callback(tk, comment_start_symbol, 4);
    push_context(tk, TOKENIZER_COMMENT);
    return 1;
  }
  else if(is_doctype(&tk->scan)) {
    tokenizer_callback(tk, tag_start_symbol, 9);
    push_context(tk, TOKENIZER_ATTRIBUTES);
    return 1;
  }
  else if(is_cdata_start(&tk->scan)) {
    tokenizer_callback(tk, cdata_start_symbol, 9);
    push_context(tk, TOKENIZER_CDATA);
    return 1;
  }
  else if(is_char(&tk->scan, '<')) {
    tokenizer_callback(tk, tag_start_symbol, 1);
    if(is_char(&tk->scan, '/')) {
      tk->is_closing_tag = 1;
      tokenizer_callback(tk, solidus_symbol, 1);
    }
    else {
      tk->is_closing_tag = 0;
    }
    if(tk->current_tag)
      *tk->current_tag = '\0';
    push_context(tk, TOKENIZER_OPEN_TAG);
    return 1;
  }
  else if(is_char(&tk->scan, '>')) {
    tokenizer_callback(tk, tag_end_symbol, 1);
    if(tk->current_tag && !tk->is_closing_tag) {
      if(!strcasecmp("title", tk->current_tag) ||
          !strcasecmp("textarea", tk->current_tag)) {
        push_context(tk, TOKENIZER_RCDATA);
      }
      else if(!strcasecmp("style", tk->current_tag) ||
          !strcasecmp("xmp", tk->current_tag) || !strcasecmp("iframe", tk->current_tag) ||
          !strcasecmp("noembed", tk->current_tag) || !strcasecmp("noframes", tk->current_tag) ||
          !strcasecmp("listing", tk->current_tag)) {
        push_context(tk, TOKENIZER_RAWTEXT);
      }
      else if(!strcasecmp("script", tk->current_tag)) {
        push_context(tk, TOKENIZER_SCRIPT_DATA);
      }
      else if(!strcasecmp("plaintext", tk->current_tag)) {
        push_context(tk, TOKENIZER_PLAINTEXT);
      }
    }
    return 1;
  }
  else if(is_text(&tk->scan, &length)) {
    tokenizer_callback(tk, text_symbol, length);
    return 1;
  }
  return 0;
}

static int scan_open_tag(struct tokenizer_t *tk)
{
  unsigned long int length = 0, tag_name_length = 0;
  const char *tag_name = NULL;

  if(is_tag_name(&tk->scan, &tag_name, &tag_name_length)) {
    tokenizer_callback(tk, tag_name_symbol, tag_name_length);

    if(tk->current_tag) {
      char *tmp;
      length = strlen(tk->current_tag) + strlen(tag_name) + 1;
      tmp = REALLOC_N(tk->current_tag, char, length);
      if(!tmp) {
        tk->current_tag = NULL;
        return 0;
      }
      tk->current_tag = tmp;
    } else {
      length = strlen(tag_name) + 1;
      tk->current_tag = ALLOC_N(char, length);
      if(!tk->current_tag)
        return 0;
      memset(tk->current_tag, 0, length);
    }

    strncat(tk->current_tag, tag_name, tag_name_length);
    return 1;
  }
  else if(is_char(&tk->scan, '/')) {
    tokenizer_callback(tk, solidus_symbol, 1);
    push_context(tk, TOKENIZER_ATTRIBUTES);
    return 1;
  }
  else if(is_whitespace(&tk->scan, &length)) {
    tokenizer_callback(tk, whitespace_symbol, length);
    push_context(tk, TOKENIZER_ATTRIBUTES);
    return 1;
  }
  else if(is_char(&tk->scan, '>')) {
    pop_context(tk);
    return 1;
  }
  return 0;
}

static int scan_attributes(struct tokenizer_t *tk)
{
  unsigned long int length = 0;

  if(is_whitespace(&tk->scan, &length)) {
    tokenizer_callback(tk, whitespace_symbol, length);
    return 1;
  }
  else if(is_char(&tk->scan, '=')) {
    tokenizer_callback(tk, equal_symbol, 1);
    tk->found_attribute = 0;
    push_context(tk, TOKENIZER_ATTRIBUTE_VALUE);
    return 1;
  }
  else if(is_char(&tk->scan, '/')) {
    tokenizer_callback(tk, solidus_symbol, 1);
    return 1;
  }
  else if(is_char(&tk->scan, '>')) {
    pop_context(tk);
    return 1;
  }
  else if(is_char(&tk->scan, '\'') || is_char(&tk->scan, '"')) {
    tk->attribute_value_start = tk->scan.string[tk->scan.cursor];
    tokenizer_callback(tk, attribute_value_start_symbol, 1);
    push_context(tk, TOKENIZER_ATTRIBUTE_STRING);
    return 1;
  }
  else if(is_attribute_name(&tk->scan, &length)) {
    tokenizer_callback(tk, attribute_name_symbol, length);
    push_context(tk, TOKENIZER_ATTRIBUTE_NAME);
    return 1;
  }
  return 0;
}

static int scan_attribute_name(struct tokenizer_t *tk)
{
  unsigned long int length = 0;

  if(is_attribute_name(&tk->scan, &length)) {
    tokenizer_callback(tk, attribute_name_symbol, length);
    return 1;
  }
  else if(is_whitespace(&tk->scan, &length) || is_char(&tk->scan, '/') ||
      is_char(&tk->scan, '>') || is_char(&tk->scan, '=')) {
    pop_context(tk);
    return 1;
  }
  return 0;
}

static int scan_attribute_value(struct tokenizer_t *tk)
{
  unsigned long int length = 0;

  if(tk->last_token == attribute_value_end_symbol) {
    pop_context(tk);
    return 1;
  }
  else if(is_char(&tk->scan, '/') || is_char(&tk->scan, '>')) {
    pop_context(tk);
    return 1;
  }
  else if(is_whitespace(&tk->scan, &length)) {
    tokenizer_callback(tk, whitespace_symbol, length);
    if(tk->found_attribute)
      pop_context(tk);
    return 1;
  }
  else if(is_char(&tk->scan, '\'') || is_char(&tk->scan, '"')) {
    tk->attribute_value_start = tk->scan.string[tk->scan.cursor];
    tokenizer_callback(tk, attribute_value_start_symbol, 1);
    push_context(tk, TOKENIZER_ATTRIBUTE_STRING);
    tk->found_attribute = 1;
    return 1;
  }
  else if(is_unquoted_value(&tk->scan, &length)) {
    tokenizer_callback(tk, attribute_unquoted_value_symbol, length);
    tk->found_attribute = 1;
    return 1;
  }
  return 0;
}

static int scan_attribute_string(struct tokenizer_t *tk)
{
  unsigned long int length = 0;

  if(is_char(&tk->scan, tk->attribute_value_start)) {
    tokenizer_callback(tk, attribute_value_end_symbol, 1);
    pop_context(tk);
    return 1;
  }
  else if(is_attribute_string(&tk->scan, &length, tk->attribute_value_start)) {
    tokenizer_callback(tk, text_symbol, length);
    return 1;
  }
  return 0;
}

static int scan_comment(struct tokenizer_t *tk)
{
  unsigned long int length = 0;
  const char *comment_end = NULL;

  if(is_comment_end(&tk->scan, &length, &comment_end)) {
    tokenizer_callback(tk, text_symbol, length);
    if(comment_end)
      tokenizer_callback(tk, comment_end_symbol, 3);
    return 1;
  }
  else {
    tokenizer_callback(tk, text_symbol, length_remaining(&tk->scan));
    return 1;
  }
  return 0;
}

static int scan_cdata(struct tokenizer_t *tk)
{
  unsigned long int length = 0;
  const char *cdata_end = NULL;

  if(is_cdata_end(&tk->scan, &length, &cdata_end)) {
    tokenizer_callback(tk, text_symbol, length);
    if(cdata_end)
      tokenizer_callback(tk, cdata_end_symbol, 3);
    return 1;
  }
  else {
    tokenizer_callback(tk, text_symbol, length_remaining(&tk->scan));
    return 1;
  }
  return 0;
}

static int scan_rawtext(struct tokenizer_t *tk)
{
  long unsigned int length = 0, tag_name_length = 0;
  const char *tag_name = NULL;
  int closing_tag = 0;

  if(is_tag_start(&tk->scan, &length, &closing_tag, &tag_name, &tag_name_length)) {
    if(closing_tag && !strncasecmp((const char *)tag_name, tk->current_tag, tag_name_length)) {
      pop_context(tk);
    } else {
      tokenizer_callback(tk, text_symbol, length);
    }
    return 1;
  }
  else if(is_text(&tk->scan, &length)) {
    tokenizer_callback(tk, text_symbol, length);
    return 1;
  }
  else {
    tokenizer_callback(tk, text_symbol, length_remaining(&tk->scan));
    return 1;
  }
  return 0;
}

static int scan_plaintext(struct tokenizer_t *tk)
{
  tokenizer_callback(tk, text_symbol, length_remaining(&tk->scan));
  return 1;
}

static int scan_once(struct tokenizer_t *tk)
{
  switch(tk->context[tk->current_context]) {
  case TOKENIZER_NONE:
    break;
  case TOKENIZER_HTML:
    return scan_html(tk);
  case TOKENIZER_OPEN_TAG:
    return scan_open_tag(tk);
  case TOKENIZER_COMMENT:
    return scan_comment(tk);
  case TOKENIZER_CDATA:
    return scan_cdata(tk);
  case TOKENIZER_RCDATA:
  case TOKENIZER_RAWTEXT:
  case TOKENIZER_SCRIPT_DATA:
    /* we don't consume character references so all
      of these states are effectively the same */
    return scan_rawtext(tk);
  case TOKENIZER_PLAINTEXT:
    return scan_plaintext(tk);
  case TOKENIZER_ATTRIBUTES:
    return scan_attributes(tk);
  case TOKENIZER_ATTRIBUTE_NAME:
    return scan_attribute_name(tk);
  case TOKENIZER_ATTRIBUTE_VALUE:
    return scan_attribute_value(tk);
  case TOKENIZER_ATTRIBUTE_STRING:
    return scan_attribute_string(tk);
  }
  return 0;
}

void scan_all(struct tokenizer_t *tk)
{
  while(!eos(&tk->scan) && scan_once(tk)) {}
  if(!eos(&tk->scan)) {
    tokenizer_callback(tk, malformed_symbol, length_remaining(&tk->scan));
  }
  return;
}

static VALUE tokenizer_tokenize_method(VALUE self, VALUE source)
{
  struct tokenizer_t *tk = NULL;
  char *c_source;

  if(NIL_P(source))
    return Qnil;

  Check_Type(source, T_STRING);
  Tokenizer_Get_Struct(self, tk);

  c_source = StringValueCStr(source);
  tk->scan.cursor = 0;
  tk->scan.length = strlen(c_source);

  tk->scan.string = calloc(1, tk->scan.length+1);
  strncpy(tk->scan.string, c_source, tk->scan.length);

  scan_all(tk);

  return Qtrue;
}

void Init_html_tokenizer_tokenizer(VALUE mHtmlTokenizer)
{
  cTokenizer = rb_define_class_under(mHtmlTokenizer, "Tokenizer", rb_cObject);
  rb_define_alloc_func(cTokenizer, tokenizer_allocate);
  rb_define_method(cTokenizer, "initialize", tokenizer_initialize_method, 0);
  rb_define_method(cTokenizer, "tokenize", tokenizer_tokenize_method, 1);

  text_symbol = ID2SYM(rb_intern("text"));
  comment_start_symbol = ID2SYM(rb_intern("comment_start"));
  comment_end_symbol = ID2SYM(rb_intern("comment_end"));
  tag_start_symbol = ID2SYM(rb_intern("tag_start"));
  tag_name_symbol = ID2SYM(rb_intern("tag_name"));
  cdata_start_symbol = ID2SYM(rb_intern("cdata_start"));
  cdata_end_symbol = ID2SYM(rb_intern("cdata_end"));
  whitespace_symbol = ID2SYM(rb_intern("whitespace"));
  attribute_name_symbol = ID2SYM(rb_intern("attribute_name"));
  solidus_symbol = ID2SYM(rb_intern("solidus"));
  equal_symbol = ID2SYM(rb_intern("equal"));
  tag_end_symbol = ID2SYM(rb_intern("tag_end"));
  attribute_value_start_symbol = ID2SYM(rb_intern("attribute_value_start"));
  attribute_value_end_symbol = ID2SYM(rb_intern("attribute_value_end"));
  attribute_unquoted_value_symbol = ID2SYM(rb_intern("attribute_unquoted_value"));
  malformed_symbol = ID2SYM(rb_intern("malformed"));
}
