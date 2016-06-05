#pragma once
#include "tokenizer.h"

enum parser_context {
  PARSER_NONE,
  PARSER_TAG,
  PARSER_ATTRIBUTE,
  PARSER_ATTRIBUTE_VALUE,
  PARSER_ATTRIBUTE_UNQUOTED_VALUE,
  PARSER_COMMENT,
  PARSER_CDATA,
};

struct parser_document_t {
  uint32_t length;
  char *data;
};

struct token_reference_t {
  VALUE type;
  uint32_t start;
  uint32_t length;
};

struct parser_tag_t {
  struct token_reference_t name;
  int is_closing;
};

struct parser_attribute_t {
  struct token_reference_t name;
  struct token_reference_t value;
};

struct parser_rawtext_t {
  struct token_reference_t text;
};

struct parser_comment_t {
  struct token_reference_t text;
};

struct parser_cdata_t {
  struct token_reference_t text;
};

struct parser_t
{
  struct tokenizer_t tk;

  struct parser_document_t doc;

  enum parser_context context;
  struct parser_tag_t tag;
  struct parser_attribute_t attribute;
  struct parser_rawtext_t rawtext;
  struct parser_comment_t comment;
  struct parser_cdata_t cdata;
};

void Init_html_tokenizer_parser(VALUE mHtmlTokenizer);

extern const rb_data_type_t html_tokenizer_parser_data_type;
#define Parser_Get_Struct(obj, sval) TypedData_Get_Struct(obj, struct parser_t, &html_tokenizer_parser_data_type, sval)
