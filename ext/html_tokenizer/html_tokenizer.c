#include <ruby.h>
#include "tokenizer.h"
#include "parser.h"


void Init_html_tokenizer()
{
  VALUE mHtmlTokenizer = rb_define_module("HtmlTokenizer");
  Init_tokenizer(mHtmlTokenizer);
  Init_parser(mHtmlTokenizer);
}
