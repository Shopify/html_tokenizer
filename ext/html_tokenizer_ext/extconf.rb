require 'mkmf'

$CXXFLAGS += " -std=c++11 "
$CXXFLAGS += " -g -Og -ggdb "
$CFLAGS += " -g -Og -ggdb "

if ENV['DEBUG']
  $CXXFLAGS += "  -DDEBUG "
  $CFLAGS += "  -DDEBUG "
end

create_makefile('html_tokenizer_ext')
