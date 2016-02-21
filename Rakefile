# -*- ruby -*-

require "rubygems"
require "hoe"
require "rake/extensiontask"

# Hoe.plugin :compiler
# Hoe.plugin :gem_prelude_sucks
# Hoe.plugin :inline
# Hoe.plugin :minitest
# Hoe.plugin :racc
# Hoe.plugin :rcov
# Hoe.plugin :rdoc

Hoe.spec "html_tokenizer" do
  developer("EiNSTeiN_", "einstein@g3nius.org")
  license("MIT")

  self.readme_file = "README.md"
  self.extra_dev_deps << ['rake-compiler', '>= 0']
  self.spec_extras = { extensions: ["ext/html_tokenizer/extconf.rb"] }

  Rake::ExtensionTask.new('html_tokenizer', spec) do |ext|
    ext.lib_dir = File.join('lib', 'html_tokenizer')
  end
end

Rake::Task[:test].prerequisites << :compile
