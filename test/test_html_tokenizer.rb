require "minitest/autorun"
require "html_tokenizer"

class TestHtmlTokenizer < MiniTest::Unit::TestCase
  def test_tokenize_text
    result = tokenize("\n    hello world\n    ")
    assert_equal [[:text, "\n    hello world\n    "]], result
  end

  def test_tokenize_doctype
    assert_equal [
      [:tag_start, "<!DOCTYPE"], [:whitespace, " "],
      [:attribute_name, "html"], [:tag_end, ">"]
    ], tokenize("<!DOCTYPE html>")
  end

  def test_tokenize_multiple_elements
    assert_equal [
      [:tag_start, "<div"], [:tag_end, ">"],
      [:text, " bla "],
      [:tag_start, "<strong"], [:tag_end, ">"]
    ], tokenize("<div> bla <strong>")
  end

  def test_tokenize_complex_doctype
    text = '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">'
    assert_equal [
      [:tag_start, "<!DOCTYPE"], [:whitespace, " "],
      [:attribute_name, "html"], [:whitespace, " "],
      [:attribute_name, "PUBLIC"], [:whitespace, " "],
      [:attribute_value_start, "\""], [:text, "-//W3C//DTD XHTML 1.0 Transitional//EN"], [:attribute_value_end, "\""],
      [:whitespace, " "],
      [:attribute_value_start, "\""], [:text, "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"], [:attribute_value_end, "\""],
      [:tag_end, ">"]
    ], tokenize(text)
  end

  def test_tokenize_html_comment
    skip
    result = tokenize("<!-- COMMENT -->")
    assert_equal [[:comment_start, "<!--"], [:text, " COMMENT "], [:comment_end, "-->"]], result
  end

  def test_tokenize_comment_with_newlines
    skip
    result = tokenize <<-EOF
      <!-- debug: <%== @unsafe %> -->
    EOF

    assert_equal [
      [:text, "      "], [:comment_start, "<!--"],
      [:text, " debug: <%== @unsafe %> "],
      [:comment_end, "-->"], [:text, "\n"]
    ], result
  end

  def test_tokenize_cdata_section
    skip
    result = tokenize("<![CDATA[  bla bla <!&@#> foo ]]>")
    assert_equal [[:cdata_start, "<![CDATA["], [:text, "  bla bla <!&@#> foo "], [:cdata_end, "]]>"]], result
  end

  def test_tokenize_basic_tag
    result = tokenize("<div>")
    assert_equal [[:tag_start, "<div"], [:tag_end, ">"]], result
  end

  def test_tokenize_namespaced_tag
    result = tokenize("<ns:foo>")
    assert_equal [[:tag_start, "<ns:foo"], [:tag_end, ">"]], result
  end

  def test_tokenize_end_tag
    result = tokenize("</div>")
    assert_equal [[:tag_start, "</div"], [:tag_end, ">"]], result
  end

  def test_tokenize_tag_attribute_with_double_quote
    result = tokenize('<div foo="bar">')
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_value_start, "\""], [:text, "bar"], [:attribute_value_end, "\""],
      [:tag_end, ">"]
    ], result
  end

  def test_tokenize_tag_attribute_without_space
    result = tokenize('<div foo="bar"baz>')
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_value_start, "\""], [:text, "bar"], [:attribute_value_end, "\""],
      [:attribute_name, "baz"],
      [:tag_end, ">"]
    ], result
  end

  def test_tokenize_multipart_unquoted_attribute
    result = tokenize('<div foo=', 'bar', 'baz>')
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_unquoted_value, "bar"],
      [:attribute_unquoted_value, "baz"], [:tag_end, ">"]
    ], result
  end

  def test_tokenize_quoted_attribute_separately
    result = tokenize('<div foo=', "'bar'", '>')
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_value_start, "'"], [:text, "bar"], [:attribute_value_end, "'"],
      [:tag_end, ">"]
    ], result
  end

  def test_tokenize_quoted_attribute_in_multiple_parts
    result = tokenize('<div foo=', "'bar", "baz'", '>')
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_value_start, "'"], [:text, "bar"], [:text, "baz"], [:attribute_value_end, "'"],
      [:tag_end, ">"]
    ], result
  end

  def test_tokenize_tag_attribute_with_single_quote
    result = tokenize("<div foo='bar'>")
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_value_start, "'"], [:text, "bar"], [:attribute_value_end, "'"],
      [:tag_end, ">"]
    ], result
  end

  def test_tokenize_tag_attribute_with_no_quotes
    result = tokenize("<div foo=bla bar=blo>")
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_unquoted_value, "bla"], [:whitespace, " "],
      [:attribute_name, "bar"], [:equal, "="], [:attribute_unquoted_value, "blo"],
      [:tag_end, ">"]
    ], result
  end

  def test_tokenize_double_equals
    result = tokenize("<div foo=blabar=blo>")
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_unquoted_value, "blabar=blo"],
      [:tag_end, ">"]
    ], result
  end

  def test_tokenize_closing_tag
    result = tokenize('<div foo="bar" />')
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "foo"], [:equal, "="], [:attribute_value_start, "\""], [:text, "bar"], [:attribute_value_end, "\""], [:whitespace, " "],
      [:slash, "/"], [:tag_end, ">"]
    ], result
  end

  def test_tokenize_script_tag
    skip
    result = tokenize('<script>foo <b> bar</script>')
    assert_equal [
      [:tag_start, "<script"], [:tag_end, ">"],
      [:text, "foo "], [:text, "<"], [:text, "b> bar"],
      [:tag_start, "</script"], [:tag_end, ">"],
    ], result
  end

  def test_tokenize_textarea_tag
    skip
    result = tokenize('<textarea>hello</textarea>')
    assert_equal [
      [:tag_start, "<textarea"], [:tag_end, ">"],
      [:text, "hello"],
      [:tag_start, "</textarea"], [:tag_end, ">"],
    ], result
  end

  def test_tokenize_script_containing_html
    skip
    result = tokenize('<script type="text/html">foo <b> bar</script>')
    assert_equal [
      [:tag_start, "<script"], [:whitespace, " "],
      [:attribute_name, "type"], [:equal, "="], [:attribute_value_start, "\""], [:text, "text/html"], [:attribute_value_end, "\""],
      [:tag_end, ">"],
      [:text, "foo "], [:text, "<"], [:text, "b> bar"],
      [:tag_start, "</script"], [:tag_end, ">"],
    ], result
  end

  def test_end_of_tag_on_newline
    data = ["\
          <div define=\"{credential_96_credential1: new Shopify.ProviderCredentials()}\"
                  ", "", ">"]
    result = tokenize(*data)
    assert_equal [
      [:text, "          "],
      [:tag_start, "<div"], [:whitespace, " "], [:attribute_name, "define"], [:equal, "="], [:attribute_value_start, "\""], [:text, "{credential_96_credential1: new Shopify.ProviderCredentials()}"], [:attribute_value_end, "\""],
      [:whitespace, "\n                  "], [:tag_end, ">"]
    ], result
  end

  def test_tokenize_multi_part_attribute_name
    result = tokenize('<div data-', 'shipping', '-type>')
    assert_equal [
      [:tag_start, "<div"], [:whitespace, " "],
      [:attribute_name, "data-"], [:attribute_name, "shipping"], [:attribute_name, "-type"],
      [:tag_end, ">"],
    ], result
  end

  def test_tokenize_attribute_name_with_space_before_equal
    result = tokenize('<a href ="http://www.cra-arc.gc.ca/tx/bsnss/tpcs/gst-tps/menu-eng.html">GST/HST</a>')
    assert_equal [
      [:tag_start, "<a"], [:whitespace, " "],
      [:attribute_name, "href"], [:whitespace, " "], [:equal, "="],
      [:attribute_value_start, "\""], [:text, "http://www.cra-arc.gc.ca/tx/bsnss/tpcs/gst-tps/menu-eng.html"], [:attribute_value_end, "\""],
      [:tag_end, ">"], [:text, "GST/HST"], [:tag_start, "</a"], [:tag_end, ">"]
    ], result
  end

  private

  def tokenize(*parts)
    tokens = []
    @tokenizer = HtmlTokenizer::Tokenizer.new
    parts.each do |part|
      @tokenizer.tokenize(part) { |name, start, stop| tokens << [name, part[start..(stop-1)]] }
    end
    tokens
  end
end
