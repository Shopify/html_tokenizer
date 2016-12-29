require "minitest/autorun"
require "html_tokenizer"

class HtmlTokenizer::ParserTest < Minitest::Test
  def test_empty_context
    parse
    assert_equal :none, @parser.context
  end

  def test_open_tag
    parse("<div")
    assert_equal :tag_name, @parser.context
    assert_equal "div", @parser.tag_name
  end

  def test_open_attribute_value
    parse('<div "foo')
    assert_equal :attribute_value, @parser.context
    assert_equal 'foo', @parser.attribute_value
    parse('bar"')
    assert_equal :tag, @parser.context
    assert_equal 'foobar', @parser.attribute_value
    assert_equal true, @parser.attribute_quoted?
  end

  def test_multi_part_namespace_tag
    parse("<foo:")
    assert_equal "foo:", @parser.tag_name
    parse("bar")
    assert_equal "foo:bar", @parser.tag_name
  end

  def test_solidus_after_tag_name
    parse("<foo/")
    assert_equal "foo", @parser.tag_name
    assert_equal :tag, @parser.context
  end

  def test_whitespace_after_tag_name
    parse("<foo ")
    assert_equal "foo", @parser.tag_name
    assert_equal :tag, @parser.context
  end

  def test_context_is_tag_name_just_after_solidus
    parse("</")
    assert_equal :tag_name, @parser.context
  end

  def test_close_tag
    parse("<div", ">")
    assert_equal :none, @parser.context
  end

  def test_attribute_name
    parse("<div foo")
    assert_equal "div", @parser.tag_name
    assert_equal :attribute, @parser.context
    assert_equal "foo", @parser.attribute_name
    parse("bla")
    assert_equal "foobla", @parser.attribute_name
  end

  def test_attribute_name_and_close
    parse("<div foo>")
    assert_equal "div", @parser.tag_name
    assert_equal "foo", @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal :none, @parser.context
  end

  def test_attribute_solidus_close
    parse("<div foo/>")
    assert_equal "div", @parser.tag_name
    assert_equal "foo", @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal :none, @parser.context
  end

  def test_attribute_value_solidus_close
    parse("<div 'foo'/>")
    assert_equal "div", @parser.tag_name
    assert_equal nil, @parser.attribute_name
    assert_equal "foo", @parser.attribute_value
    assert_equal true, @parser.attribute_quoted?
    assert_equal :none, @parser.context
  end

  def test_attribute_value_and_tag_close
    parse('<div "foo">')
    assert_equal "div", @parser.tag_name
    assert_equal nil, @parser.attribute_name
    assert_equal 'foo', @parser.attribute_value
    assert_equal true, @parser.attribute_quoted?
    assert_equal :none, @parser.context
  end
  def test_attribute_value_equal_and_tag_close
    parse("<div foo=>")
    assert_equal "div", @parser.tag_name
    assert_equal "foo", @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal :none, @parser.context
  end

  def test_attribute_value_open_quote
    parse("<div '")
    assert_equal nil, @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal true, @parser.attribute_quoted?
    assert_equal :attribute_value, @parser.context
  end

  def test_attribute_name_and_value_open_quote
    parse("<div foo='")
    assert_equal nil, @parser.attribute_value
    assert_equal true, @parser.attribute_quoted?
    assert_equal :attribute_value, @parser.context
  end

  def test_attribute_value_open
    parse("<div foo=")
    assert_equal "div", @parser.tag_name
    assert_equal "foo", @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal :attribute, @parser.context
  end

  def test_attribute_name_with_solidus
    parse("<div foo=/bar")
    assert_equal "bar", @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal false, @parser.attribute_quoted?
    assert_equal :attribute, @parser.context
  end

  def test_attribute_with_value_with_solidus
    parse("<div foo='bar'")
    assert_equal "foo", @parser.attribute_name
    assert_equal "bar", @parser.attribute_value
    assert_equal :tag, @parser.context
    parse("/baz")
    assert_equal "baz", @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal true, @parser.attribute_quoted?
    assert_equal :attribute, @parser.context
  end

  def test_attribute_with_unquoted_value
    parse("<div foo=bar")
    assert_equal "foo", @parser.attribute_name
    assert_equal "bar", @parser.attribute_value
    assert_equal false, @parser.attribute_quoted?
    assert_equal :attribute_value, @parser.context
  end

  def test_attribute_with_unquoted_value_tag_end
    parse("<div foo=bar>")
    assert_equal "foo", @parser.attribute_name
    assert_equal "bar", @parser.attribute_value
    assert_equal false, @parser.attribute_quoted?
    assert_equal :none, @parser.context
  end

  def test_attribute_with_unquoted_value_with_solidus
    parse("<div foo=ba", "r", "/baz")
    assert_equal "baz", @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal false, @parser.attribute_quoted?
    assert_equal :attribute, @parser.context
  end

  def test_attribute_with_unquoted_value_with_space
    parse("<div foo=ba", "r", " baz")
    assert_equal "baz", @parser.attribute_name
    assert_equal nil, @parser.attribute_value
    assert_equal false, @parser.attribute_quoted?
    assert_equal :attribute, @parser.context
  end

  def test_attribute_with_multipart_unquoted_value
    parse("<div foo=ba", "r", "&baz")
    assert_equal "foo", @parser.attribute_name
    assert_equal "bar&baz", @parser.attribute_value
    assert_equal false, @parser.attribute_quoted?
    assert_equal :attribute_value, @parser.context
  end

  def test_comment_context
    parse("<!--")
    assert_equal :comment, @parser.context
    assert_equal nil, @parser.comment_text
  end

  def test_cdata_context
    parse("<![CDATA[")
    assert_equal :cdata, @parser.context
    assert_equal nil, @parser.cdata_text
  end

  def test_comment_text
    parse("<!-- foo")
    assert_equal :comment, @parser.context
    assert_equal " foo", @parser.comment_text
  end

  def test_cdata_text
    parse("<![CDATA[ foo")
    assert_equal :cdata, @parser.context
    assert_equal " foo", @parser.cdata_text
  end

  def test_multipart_comment
    parse("<!-- f", "oo", "bar")
    assert_equal :comment, @parser.context
    assert_equal " foobar", @parser.comment_text
  end

  def test_multipart_cdata
    parse("<![CDATA[ f", "oo", "bar")
    assert_equal :cdata, @parser.context
    assert_equal " foobar", @parser.cdata_text
  end

  def test_comment_end
    parse("<!-- foo -->")
    assert_equal :none, @parser.context
    assert_equal " foo ", @parser.comment_text
  end

  def test_cdata_end
    parse("<![CDATA[ foo ]]>")
    assert_equal :none, @parser.context
    assert_equal " foo ", @parser.cdata_text
  end

  def test_plaintext_never_stops_parsing
    parse("<plaintext>")
    assert_equal :rawtext, @parser.context
    assert_equal "plaintext", @parser.tag_name
    assert_equal nil, @parser.rawtext_text

    parse("some", "<text")
    assert_equal :rawtext, @parser.context
    assert_equal "some<text", @parser.rawtext_text

    parse("<plaintext")
    assert_equal :rawtext, @parser.context
    assert_equal "some<text<plaintext", @parser.rawtext_text

    parse("</plaintext>")
    assert_equal :rawtext, @parser.context
    assert_equal "some<text<plaintext</plaintext>", @parser.rawtext_text
  end

  %w(title textarea style xmp iframe noembed noframes).each do |name|
    define_method "test_#{name}_rawtext" do
      parse("<#{name}>")
      assert_equal :rawtext, @parser.context
      assert_equal name, @parser.tag_name
      assert_equal nil, @parser.rawtext_text

      parse("some", "<text")
      assert_equal :rawtext, @parser.context
      assert_equal "some<text", @parser.rawtext_text

      parse("<#{name}")
      assert_equal :rawtext, @parser.context
      assert_equal "some<text<#{name}", @parser.rawtext_text

      parse("</#{name}")
      assert_equal :tag_name, @parser.context
      assert_equal "some<text<#{name}", @parser.rawtext_text

      parse(">")
      assert_equal :none, @parser.context
      assert_equal "some<text<#{name}", @parser.rawtext_text
    end
  end

  def test_script_rawtext
    parse("<script>data data data")
    assert_equal :rawtext, @parser.context
    assert_equal "script", @parser.tag_name
    assert_equal "data data data", @parser.rawtext_text
    parse("</script")
    assert_equal :tag_name, @parser.context
    assert_equal "script", @parser.tag_name
    parse(">")
    assert_equal :none, @parser.context
  end

  def test_consecutive_scripts
    parse("<script>foo\n</script>\n<script>bar</script> bla")
    assert_equal :none, @parser.context
  end

  private

  def parse(*parts)
    @parser ||= HtmlTokenizer::Parser.new
    parts.each do |part|
      @parser.parse(part)
    end
  end
end
