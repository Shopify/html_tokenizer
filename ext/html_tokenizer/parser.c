#include <ruby.h>
#include "parser.h"

static VALUE cParser = Qnil;

static VALUE
  none_symbol,
  tag_symbol,
  attribute_symbol,
  attribute_value_symbol,
  rawtext_symbol,
  comment_symbol,
  cdata_symbol
;

static void parser_mark(void *ptr)
{}

static void parser_free(void *ptr)
{
  struct parser_t *parser = ptr;
  if(parser) {
    if(parser->doc.data) {
      xfree(parser->doc.data);
      parser->doc.data = NULL;
    }
    xfree(parser);
  }
}

static size_t parser_memsize(const void *ptr)
{
  return ptr ? sizeof(struct parser_t) : 0;
}

const rb_data_type_t html_tokenizer_parser_data_type = {
  "html_tokenizer_parser",
  { parser_mark, parser_free, parser_memsize, },
#if defined(RUBY_TYPED_FREE_IMMEDIATELY)
  NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
#endif
};

static VALUE parser_allocate(VALUE klass)
{
  VALUE obj;
  struct parser_t *parser;

  obj = TypedData_Make_Struct(klass, struct parser_t, &html_tokenizer_parser_data_type, parser);

  return obj;
}

static inline void parser_append_ref(struct token_reference_t *dest, struct token_reference_t *src)
{
  if(dest->type == Qnil || dest->type != src->type || (dest->start + dest->length) != src->start) {
    dest->type = src->type;
    dest->start = src->start;
    dest->length = src->length;
  }
  else {
    dest->type = src->type;
    dest->length += src->length;
  }
}

static void parse_none(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == tag_start_symbol) {
    parser->context = PARSER_TAG_NAME;
    parser->tag.name.type = Qnil;
  }
  else if(ref->type == comment_start_symbol) {
    parser->context = PARSER_COMMENT;
    parser->comment.text.type = Qnil;
  }
  else if(ref->type == cdata_start_symbol) {
    parser->context = PARSER_CDATA;
    parser->cdata.text.type = Qnil;
  }
}

static void parse_rawtext(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == text_symbol) {
    parser_append_ref(&parser->rawtext.text, ref);
  }
  else {
    parser->context = PARSER_NONE;
    parse_none(parser, ref);
  }
  return;
}

static void parse_comment(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == comment_end_symbol) {
    parser->context = PARSER_NONE;
  }
  else if(ref->type == text_symbol) {
    parser_append_ref(&parser->comment.text, ref);
  }
  return;
}

static void parse_cdata(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == cdata_end_symbol) {
    parser->context = PARSER_NONE;
  }
  else if(ref->type == text_symbol) {
    parser_append_ref(&parser->cdata.text, ref);
  }
  return;
}

static void parse_tag_name(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == tag_end_symbol) {
    parser->context = PARSER_NONE;
  }
  else if(ref->type == tag_name_symbol) {
    parser_append_ref(&parser->tag.name, ref);
  }
  else if(ref->type == whitespace_symbol) {
    parser->context = PARSER_TAG;
  }
  else if(ref->type == solidus_symbol) {
    if(parser->tk.context[parser->tk.current_context] == TOKENIZER_HTML) {
      // solidus with opening tag, still expecting a tag name
    } else {
      // solidus not in tag name.
      parser->context = PARSER_TAG;
    }
  }
  return;
}

static void parse_tag(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == tag_end_symbol) {
    parser->context = PARSER_NONE;
  }
  else if(ref->type == tag_name_symbol) {
    parser_append_ref(&parser->tag.name, ref);
  }
  else if(ref->type == attribute_name_symbol) {
    parser->context = PARSER_ATTRIBUTE;
    parser_append_ref(&parser->attribute.name, ref);
    parser->attribute.value.type = Qnil;
  }
  else if(ref->type == attribute_value_start_symbol) {
    parser->context = PARSER_ATTRIBUTE_VALUE;
    parser->attribute.name.type = Qnil;
    parser->attribute.value.type = Qnil;
    parser->attribute.is_quoted = 1;
  }
  return;
}

static void parse_attribute(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == tag_end_symbol) {
    parser->context = PARSER_NONE;
  }
  else if(ref->type == attribute_name_symbol) {
    parser_append_ref(&parser->attribute.name, ref);
  }
  else if(ref->type == attribute_value_start_symbol) {
    parser->context = PARSER_ATTRIBUTE_VALUE;
    parser->attribute.is_quoted = 1;
  }
  else if(ref->type == attribute_unquoted_value_symbol) {
    parser->context = PARSER_ATTRIBUTE_UNQUOTED_VALUE;
    parser_append_ref(&parser->attribute.value, ref);
    parser->attribute.is_quoted = 0;
  }

  return;
}

static void parse_attribute_value(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == tag_end_symbol) {
    parser->context = PARSER_NONE;
  }
  else if(ref->type == attribute_name_symbol) {
    parser->context = PARSER_ATTRIBUTE;
    parser_append_ref(&parser->attribute.name, ref);
  }
  else if(ref->type == attribute_value_start_symbol) {
    parser_append_ref(&parser->attribute.value, ref);
  }
  else if(ref->type == text_symbol) {
    parser_append_ref(&parser->attribute.value, ref);
  }
  else if(ref->type == attribute_value_end_symbol) {
    parser->context = PARSER_TAG;
  }
  else if(ref->type == attribute_unquoted_value_symbol) {
    parser_append_ref(&parser->attribute.value, ref);
  }

  return;
}

static void parse_attribute_unquoted_value(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == tag_end_symbol) {
    parser->context = PARSER_NONE;
  }
  else if(ref->type == attribute_unquoted_value_symbol) {
    parser_append_ref(&parser->attribute.value, ref);
  }
  else {
    parser->context = PARSER_TAG;
  }

  return;
}

static inline int rawtext_context(struct parser_t *parser)
{
  enum tokenizer_context ctx = parser->tk.context[parser->tk.current_context];
  return (ctx == TOKENIZER_RCDATA || ctx == TOKENIZER_RAWTEXT ||
      ctx == TOKENIZER_SCRIPT_DATA || ctx == TOKENIZER_PLAINTEXT);
}

static void parser_tokenize_callback(struct tokenizer_t *tk, VALUE sym, unsigned long int length, void *data)
{
  struct parser_t *parser = (struct parser_t *)data;
  struct token_reference_t ref = { sym, tk->scan.cursor, length };

  switch(parser->context)
  {
  case PARSER_NONE:
    if(rawtext_context(parser))
      parse_rawtext(parser, &ref);
    else
      parse_none(parser, &ref);
    break;
  case PARSER_TAG_NAME:
    parse_tag_name(parser, &ref);
    break;
  case PARSER_TAG:
    parse_tag(parser, &ref);
    break;
  case PARSER_ATTRIBUTE:
    parse_attribute(parser, &ref);
    break;
  case PARSER_ATTRIBUTE_UNQUOTED_VALUE:
    parse_attribute_unquoted_value(parser, &ref);
    break;
  case PARSER_ATTRIBUTE_VALUE:
    parse_attribute_value(parser, &ref);
    break;
  case PARSER_CDATA:
    parse_cdata(parser, &ref);
    break;
  case PARSER_COMMENT:
    parse_comment(parser, &ref);
    break;
  }

  return;
}

static VALUE parser_initialize_method(VALUE self)
{
  struct parser_t *parser;

  Parser_Get_Struct(self, parser);

  parser->context = PARSER_NONE;

  tokenizer_init(&parser->tk);
  parser->tk.callback_data = parser;
  parser->tk.f_callback = parser_tokenize_callback;

  parser->doc.length = 0;
  parser->doc.data = NULL;

  return Qnil;
}

static int parser_document_append(struct parser_t *parser, const char *string, unsigned long int length)
{
  REALLOC_N(parser->doc.data, char, parser->doc.length + length + 1);
  strcpy(parser->doc.data+parser->doc.length, string);
  parser->doc.length += length;
  return 1;
}

static VALUE parser_parse_method(VALUE self, VALUE source)
{
  struct parser_t *parser = NULL;
  char *string = NULL;
  long unsigned int length = 0;

  if(NIL_P(source))
    return Qnil;

  Check_Type(source, T_STRING);
  Parser_Get_Struct(self, parser);

  string = StringValueCStr(source);
  length = strlen(string);

  parser->tk.scan.cursor = parser->doc.length;

  if(!parser_document_append(parser, string, length)) {
    // error
    return Qnil;
  }

  parser->tk.scan.string = parser->doc.data;
  parser->tk.scan.length = parser->doc.length;

  tokenizer_scan_all(&parser->tk);

  return Qtrue;
}

static VALUE parser_context_method(VALUE self)
{
  struct parser_t *parser = NULL;

  Parser_Get_Struct(self, parser);

  switch(parser->context) {
  case PARSER_NONE:
    return rawtext_context(parser) ? rawtext_symbol : none_symbol;
  case PARSER_TAG_NAME:
    return tag_name_symbol;
  case PARSER_TAG:
    return tag_symbol;
  case PARSER_ATTRIBUTE:
    return attribute_symbol;
  case PARSER_ATTRIBUTE_UNQUOTED_VALUE:
  case PARSER_ATTRIBUTE_VALUE:
    return attribute_value_symbol;
  case PARSER_COMMENT:
    return comment_symbol;
  case PARSER_CDATA:
    return cdata_symbol;
  default:
    return Qnil;
  }

  return Qnil;
}

static inline VALUE ref_to_str(struct parser_t *parser, struct token_reference_t *ref)
{
  if(ref->type == Qnil || parser->doc.data == NULL)
    return Qnil;
  return rb_str_new(parser->doc.data+ref->start, ref->length);
}

static VALUE parser_tag_name_method(VALUE self)
{
  struct parser_t *parser = NULL;
  Parser_Get_Struct(self, parser);
  return ref_to_str(parser, &parser->tag.name);
}

static VALUE parser_attribute_name_method(VALUE self)
{
  struct parser_t *parser = NULL;
  Parser_Get_Struct(self, parser);
  return ref_to_str(parser, &parser->attribute.name);
}

static VALUE parser_attribute_value_method(VALUE self)
{
  struct parser_t *parser = NULL;
  Parser_Get_Struct(self, parser);
  return ref_to_str(parser, &parser->attribute.value);
}

static VALUE parser_attribute_is_quoted_method(VALUE self)
{
  struct parser_t *parser = NULL;
  Parser_Get_Struct(self, parser);
  return parser->attribute.is_quoted ? Qtrue : Qfalse;
}

static VALUE parser_comment_text_method(VALUE self)
{
  struct parser_t *parser = NULL;
  Parser_Get_Struct(self, parser);
  return ref_to_str(parser, &parser->comment.text);
}

static VALUE parser_cdata_text_method(VALUE self)
{
  struct parser_t *parser = NULL;
  Parser_Get_Struct(self, parser);
  return ref_to_str(parser, &parser->cdata.text);
}

static VALUE parser_rawtext_text_method(VALUE self)
{
  struct parser_t *parser = NULL;
  Parser_Get_Struct(self, parser);
  return ref_to_str(parser, &parser->rawtext.text);
}

void Init_html_tokenizer_parser(VALUE mHtmlTokenizer)
{
  cParser = rb_define_class_under(mHtmlTokenizer, "Parser", rb_cObject);
  rb_define_alloc_func(cParser, parser_allocate);
  rb_define_method(cParser, "initialize", parser_initialize_method, 0);
  rb_define_method(cParser, "parse", parser_parse_method, 1);
  rb_define_method(cParser, "context", parser_context_method, 0);
  rb_define_method(cParser, "tag_name", parser_tag_name_method, 0);
  rb_define_method(cParser, "attribute_name", parser_attribute_name_method, 0);
  rb_define_method(cParser, "attribute_value", parser_attribute_value_method, 0);
  rb_define_method(cParser, "attribute_quoted?", parser_attribute_is_quoted_method, 0);
  rb_define_method(cParser, "comment_text", parser_comment_text_method, 0);
  rb_define_method(cParser, "cdata_text", parser_cdata_text_method, 0);
  rb_define_method(cParser, "rawtext_text", parser_rawtext_text_method, 0);

  none_symbol = ID2SYM(rb_intern("none"));
  tag_symbol = ID2SYM(rb_intern("tag"));
  attribute_symbol = ID2SYM(rb_intern("attribute"));
  attribute_value_symbol = ID2SYM(rb_intern("attribute_value"));
  rawtext_symbol = ID2SYM(rb_intern("rawtext"));
  comment_symbol = ID2SYM(rb_intern("comment"));
  cdata_symbol = ID2SYM(rb_intern("cdata"));
}
