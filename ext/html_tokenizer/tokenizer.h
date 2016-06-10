#pragma once

enum tokenizer_context {
  TOKENIZER_NONE = 0,
  TOKENIZER_HTML,
  TOKENIZER_OPEN_TAG,
  TOKENIZER_CDATA,
  TOKENIZER_RCDATA, // title, textarea
  TOKENIZER_RAWTEXT, // style, xmp, iframe, noembed, noframes
  TOKENIZER_SCRIPT_DATA, // script
  TOKENIZER_PLAINTEXT, // plaintext
  TOKENIZER_COMMENT,
  TOKENIZER_ATTRIBUTES,
  TOKENIZER_ATTRIBUTE_NAME,
  TOKENIZER_ATTRIBUTE_VALUE,
  TOKENIZER_ATTRIBUTE_STRING,
};

extern VALUE text_symbol,
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

struct scan_t {
  char *string;
  long unsigned int cursor;
  long unsigned int length;
};

struct tokenizer_t
{
  enum tokenizer_context context[100];
  uint32_t current_context;

  void *callback_data;
  void (*f_callback)(struct tokenizer_t *tk, VALUE sym, long unsigned int length, void *data);

  char attribute_value_start;
  int found_attribute;

  char *current_tag;

  int is_closing_tag;
  VALUE last_token;

  struct scan_t scan;
};


void Init_html_tokenizer_tokenizer(VALUE mHtmlTokenizer);
void tokenizer_init(struct tokenizer_t *tk);
void scan_all(struct tokenizer_t *tk);

extern const rb_data_type_t tokenizer_data_type;
#define Tokenizer_Get_Struct(obj, sval) TypedData_Get_Struct(obj, struct tokenizer_t, &tokenizer_data_type, sval)
