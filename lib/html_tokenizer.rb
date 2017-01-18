require 'html_tokenizer_ext'

module HtmlTokenizer
  class ParserError < RuntimeError
    attr_reader :line, :column
    def initialize(message, line, column)
      super(message)
      @line = line
      @column = column
    end
  end
end
