#pragma once

enum tokenizer_context {
  TOKENIZER_NONE = 0,
  TOKENIZER_DOCUMENT,
  TOKENIZER_HTML,
  TOKENIZER_CDATA,
  TOKENIZER_SCRIPT_TAG,
  TOKENIZER_TEXTAREA_TAG,
  TOKENIZER_COMMENT,
  TOKENIZER_ATTRIBUTES,
  TOKENIZER_ATTRIBUTE_NAME,
  TOKENIZER_ATTRIBUTE_VALUE,
  TOKENIZER_ATTRIBUTE_STRING,
};

struct scan_t {
  VALUE source;
  const char *string;
  uint32_t cursor;
  uint32_t length;
};

struct tokenizer_t
{
  enum tokenizer_context context[100];
  uint32_t current_context;

  char attribute_value_start;
  int found_attribute;
  int is_script, is_textarea, is_closing_tag;
  VALUE last_token;

  struct scan_t scan;
};

extern const rb_data_type_t tokenizer_data_type;
#define Tokenizer_Get_Struct(obj, sval) TypedData_Get_Struct(obj, struct tokenizer_t, &tokenizer_data_type, sval)
