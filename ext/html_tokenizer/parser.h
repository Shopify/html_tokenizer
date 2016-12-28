#pragma once
#include "tokenizer.h"

enum parser_context {
  PARSER_NONE,
  PARSER_TAG_NAME,
  PARSER_TAG,
  PARSER_ATTRIBUTE,
  PARSER_ATTRIBUTE_VALUE,
  PARSER_ATTRIBUTE_UNQUOTED_VALUE,
  PARSER_COMMENT,
  PARSER_CDATA,
};

struct parser_document_t {
  long unsigned int length;
  char *data;
};

struct token_reference_t {
  enum token_type type;
  long unsigned int start;
  long unsigned int length;
};

struct parser_tag_t {
  struct token_reference_t name;
  int is_closing;
};

struct parser_attribute_t {
  struct token_reference_t name;
  struct token_reference_t value;
  int is_quoted;
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

extern const rb_data_type_t ht_parser_data_type;
#define Parser_Get_Struct(obj, sval) TypedData_Get_Struct(obj, struct parser_t, &ht_parser_data_type, sval)
