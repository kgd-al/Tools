#ifndef INDENTINGOSTREAM_HPP
#define INDENTINGOSTREAM_HPP

/*! \file indentingostream.h
 * \brief Provides declaration for an automatic stream indenter
 */

#include <ostream>

namespace utils {

/// Manages indentation for provided ostream
/// \author James Kanze @ https://stackoverflow.com/a/9600752
class IndentingOStreambuf : public std::streambuf {
  std::ostream*       _owner;   ///< Associated ostream
  std::streambuf*     _buffer;  ///< Associated buffer
  bool                _isAtStartOfLine; ///< Whether to insert indentation

  const uint          _thisIndent;  ///< Local indentation
  const std::string   _indent;  ///< Indentation value

protected:
  /// Overrides std::basic_streambuf::overflow to insert indentation at line start
  int overflow (int ch) override;

  /// \returns index of indent-storage space inside the ios_base static region
  static uint index (void);

  /// \returns the indent level for the associated level
  uint indent (int di = 0);

public:
  /// Creates a proxy buffer managing indentation level
  explicit IndentingOStreambuf(std::ostream& dest, uint spaces = 2);

  /// Returns control of the buffer to its owner
  virtual ~IndentingOStreambuf(void);
};

} // end of namespace utils

#endif // INDENTINGOSTREAM_HPP
