#include <ruby.h>
#include "tokenizer.h"
#include "parser.h"

static VALUE mHtmlTokenizer = Qnil;

void Init_html_tokenizer()
{
  mHtmlTokenizer = rb_define_class("HtmlTokenizer", rb_cObject);
  Init_html_tokenizer_tokenizer(mHtmlTokenizer);
  Init_html_tokenizer_parser(mHtmlTokenizer);
}
