#include "indentingostream.h"

#include <cassert>
#include <iostream>

namespace utils {

int IndentingOStreambuf::overflow (int ch) {
  if ( _isAtStartOfLine && ch != '\n' )
    _buffer->sputn(_indent.data(), _indent.size());
  _isAtStartOfLine = (ch == '\n');
  return _buffer->sputc(ch);
}

uint IndentingOStreambuf::index (void) {
  static uint i = std::ios_base::xalloc();
  return i;
}

uint IndentingOStreambuf::indent (int di) {
  long &i = _owner->iword(index());
  if (di != 0)  i += di;
  return i;
}

IndentingOStreambuf::IndentingOStreambuf(std::ostream& dest, uint spaces)
  : _owner(&dest), _buffer(dest.rdbuf()),
    _isAtStartOfLine(true),
    _thisIndent(spaces), _indent(indent(_thisIndent), ' ' ) {
    _owner->rdbuf( this );
}

IndentingOStreambuf::~IndentingOStreambuf(void) {
  _owner->rdbuf(_buffer);
  indent(-int(_thisIndent));
}

} // end of namespace utils
