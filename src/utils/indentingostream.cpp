#include "indentingostream.h"

#include <cassert>
#include <iostream>

namespace utils {

int IndentingOStreambuf::overflow (int ch) {
  if (_isAtStartOfLine && ch != '\n')
    _buffer->sputn(_indent.data(), _indent.size());
  _isAtStartOfLine = (ch == '\n');
  return _buffer->sputc(ch);
}

IndentingOStreambuf::IndentingOStreambuf(std::ostream &dest, uint spaces)
  : _owner(&dest), _buffer(dest.rdbuf()),
    _isAtStartOfLine(true),
    _indent(spaces, ' ' ) { _owner->rdbuf( this );  }

IndentingOStreambuf::~IndentingOStreambuf(void) { _owner->rdbuf(_buffer); }

} // end of namespace utils
