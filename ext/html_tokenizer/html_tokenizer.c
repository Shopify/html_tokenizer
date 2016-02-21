#include <ruby.h>

static VALUE tokenizer_tokenize_method(VALUE self, VALUE source)
{
  Check_Type(source, T_STRING);

  return rb_str_new2("hello world");
}

void Init_html_tokenizer()
{
  VALUE mHtmlTokenizer = rb_define_module("HtmlTokenizer");
  VALUE cTokenizer = rb_define_class_under(mHtmlTokenizer, "Tokenizer", rb_cObject);
  rb_define_method(cTokenizer, "tokenize", tokenizer_tokenize_method, 1);
}
