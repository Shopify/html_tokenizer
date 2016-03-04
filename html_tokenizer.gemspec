Gem::Specification.new do |s|
  s.name    = "html_tokenizer"
  s.version = "0.0.1"
  s.summary = "HTML Tokenizer"
  s.author  = "Francois Chagnon"

  s.files = Dir.glob("ext/**/*.{c,rb}") +
            Dir.glob("lib/**/*.rb")

  s.extensions << "ext/html_tokenizer/extconf.rb"

  s.add_development_dependency "rake-compiler"
end
