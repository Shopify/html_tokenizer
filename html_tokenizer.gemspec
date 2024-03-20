# frozen_string_literal: true

require_relative "lib/html_tokenizer/version"

Gem::Specification.new do |spec|
  spec.name    = "html_tokenizer"
  spec.version = HtmlTokenizer::VERSION
  spec.summary = "HTML Tokenizer"
  spec.author  = "Francois Chagnon"

  spec.homepage      = "https://github.com/Shopify/html_tokenizer"
  spec.license       = "MIT"

  spec.metadata["allowed_push_host"] = "https://rubygems.org/"

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = spec.homepage

  spec.files = Dir.glob("ext/**/*.{c,h,rb}") +
            Dir.glob("lib/**/*.rb")

  spec.extensions    = ['ext/html_tokenizer_ext/extconf.rb']
  spec.files         = `git ls-files -z`.split("\x0")
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib", "ext"]
end
