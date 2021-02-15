#ifndef KGD_UTILS_INDENTINGOSTREAM_HPP
#define KGD_UTILS_INDENTINGOSTREAM_HPP

/*! \file indentingostream.h
 * \brief Provides declaration for an automatic stream indenter
 */

#include <ostream>

namespace utils {

/// Manages indentation for provided ostream
/// \author James Kanze @ https://stackoverflow.com/a/9600752
class IndentingOStreambuf : public std::streambuf {
  static constexpr uint DEFAULT_INDENT = 2;   ///< Default indenting value

  std::ostream*       _owner;   ///< Associated ostream
  std::streambuf*     _buffer;  ///< Associated buffer
  bool                _isAtStartOfLine; ///< Whether to insert indentation

  const std::string   _indent;  ///< Indentation value

protected:
  /// Overrides std::basic_streambuf::overflow to insert indentation at line start
  int overflow (int ch) override;

public:
  /// Creates a proxy buffer managing indentation level
  explicit IndentingOStreambuf(std::ostream& dest,
                               uint spaces = DEFAULT_INDENT);

  /// Returns control of the buffer to its owner
  virtual ~IndentingOStreambuf(void);
};

} // end of namespace utils

#endif // KGD_UTILS_INDENTINGOSTREAM_HPP
