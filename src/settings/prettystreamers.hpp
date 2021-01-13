#ifndef KGD_UTILS_PRETTY_STREAMERS_HPP
#define KGD_UTILS_PRETTY_STREAMERS_HPP

#include <ostream>
#include <map>
#include <set>

#include "../utils/utils.h"
#include "../random/dice.hpp"
#include "prettyenums.hpp"

/*!
 * \file prettystreamers.hpp
 *
 * Contains helper templates for reading/writing various types
 */

// =============================================================================
/// Generic pretty printer. Delegates to T::operator<<
template <typename T, typename Enabled = void>
struct PrettyWriter {
  /// Write the argument to \p os
  void operator() (std::ostream &os, const T &value) {
    using utils::operator<<;
    os << value;
  }
};

/// Generic pretty reader. Delegates to T::operator>>
template <typename T, typename Enabled = void>
struct PrettyReader {
  /// Transfer contents of \p is into the argument
  bool operator() (std::istream &is, T &value) {
    using utils::operator>>;
    return bool(is >> value);
  }
};


// =============================================================================
/// FastDice pretty printer
template <> struct PrettyWriter<rng::FastDice, void> {
  /// \copydoc PrettyWriter
  void operator() (std::ostream &os, const rng::FastDice &d) {
    os << d.getSeed();
  }
};

/// FastDice pretty reader
template <> struct PrettyReader<rng::FastDice, void> {
  /// \copydoc PrettyReader
  bool operator() (std::istream &is, rng::FastDice &d) {
    rng::FastDice::Seed_t seed;
    is >> seed;
    d .reset(seed);
    return bool(is);
  }
};

/// AtomicDice pretty printer
template <> struct PrettyWriter<rng::AtomicDice, void> {
  /// \copydoc PrettyWriter
  void operator() (std::ostream &os, const rng::AtomicDice &d) {
    os << d.getSeed();
  }
};

/// AtomicDice pretty reader
template <> struct PrettyReader<rng::AtomicDice, void> {
  /// \copydoc PrettyReader
  bool operator() (std::istream &is, rng::AtomicDice &d) {
    rng::AtomicDice::Seed_t seed;
    is >> seed;
    d.reset(seed);
    return bool(is);
  }
};

// =============================================================================
/// Pretty-enumeration pretty printer (yep, lots of prettiness around)
template <typename E>
struct PrettyWriter<E, typename std::enable_if<_details::is_pretty_enum<E>::value>::type> {
  /// \copydoc PrettyWriter
  void operator() (std::ostream &os, const E &e) {
    os << EnumUtils<E>::getName(e);
  }
};

/// Pretty-enumeration pretty reader
template <typename E>
struct PrettyReader<E, typename std::enable_if<_details::is_pretty_enum<E>::value>::type> {
  /// \copydoc PrettyReader
  bool operator() (std::istream &is, E &e) {
    is >> e;
    return bool(is);
  }
};


// =============================================================================
/// Pretty booleans writer
template <> struct PrettyWriter<bool, void> {
  /// \copydoc PrettyWriter
  void operator() (std::ostream &os, const bool &b) {
    auto flags = os.setf(std::ios::boolalpha);
    os << b;
    os.setf(flags);
  }
};

/// Pretty booleans reader
template <> struct PrettyReader<bool, void> {
  /// \copydoc PrettyReader
  bool operator() (std::istream &is, bool &b) {
    auto flags = is.setf(std::ios::boolalpha);
    is >> b;
    is.setf(flags);
    return bool(is);
  }
};


// =============================================================================
/// Safe strings writer
template <> struct PrettyWriter<std::string, void> {
  /// \copydoc PrettyWriter
  void operator() (std::ostream &os, const std::string &s) {
    os << "\"" << s << "\"";
  }
};

/// Safe strings reader
template <> struct PrettyReader<std::string, void> {
  /// \copydoc PrettyReader
  bool operator() (std::istream &is, std::string &s) {
    bool ok = bool(std::getline(is, s));
    s = utils::unquote(s);
    return ok && bool(is);
  }
};


// =============================================================================
/// Pretty sets writer
template <typename V, typename C>
struct PrettyWriter<std::set<V,C>> {
  /// \copydoc PrettyWriter
  void operator() (std::ostream &os, const std::set<V,C> &set) {
    PrettyWriter<V> printer;
    for (const V &v: set) {
      printer(os, v);
      os << " ";
    }
  }
};

/// Pretty sets reader
template <typename V, typename C>
struct PrettyReader<std::set<V,C>> {
  /// \copydoc PrettyReader
  bool operator() (std::istream &is, std::set<V,C> &set) {
    PrettyReader<V> reader;
    V value;

    set.clear();
    while (is.peek() != EOF && reader(is, value))   set.insert(value);

    if (is.eof() && is.fail())  is.clear();
    return (bool)is;
  }
};


// =============================================================================

/// Specialization for prettier string class name printing
template <>
inline std::string utils::className<std::string> (void) {
  return "std::string";
}

/// Pretty maps writer
template <typename K, typename V>
struct PrettyWriter<std::map<K,V>> {
  /// \copydoc PrettyWriter
  void operator() (std::ostream &os, const std::map<K,V> &map) {
    PrettyWriter<K> keyPrinter;
    uint maxWidth = 0;
    for (auto &p: map) {
      std::ostringstream oss;
      keyPrinter(oss, p.first);
      uint width = oss.str().length();
      if (width > maxWidth)
        maxWidth = width;
    }
    os << "map("
       << utils::className<K>()
       << ", "
       << utils::className<V>() << ") {\n";
    for (auto &p: map) {
      std::ostringstream oss;
      keyPrinter(oss, p.first);
      std::string key = oss.str();
      os << "    " << std::string(maxWidth-key.length(), ' ') << key << ": ";
      PrettyWriter<V>()(os, p.second);
      os << "\n";
    }
    os << "}";
  }
};

/// Pretty maps reader
template <typename K, typename V>
struct PrettyReader<std::map<K,V>> {
  /// \copydoc PrettyReader
  bool operator() (std::istream &is, std::map<K,V> &map) {
    static const std::regex regKeyValue =
        std::regex("[[:space:]]*([^[:space:]][^:]*): (.*)");

    std::smatch matches;

    std::string line;
    while (getline(is, line)) {
      if (line.size() == 0) continue;

      if (std::regex_match(line, matches, regKeyValue)) {
        std::istringstream isskey = std::istringstream (matches[1]),
                           issval = std::istringstream (matches[2]);
        K key;
        V value;
        PrettyReader<K>()(isskey, key);
        PrettyReader<V>()(issval, value);

        map[key] = value;
      }
    }
    if (is.eof() && is.fail())  is.clear();
    return (bool)is;
  }
};

#endif // KGD_UTILS_PRETTY_STREAMERS_HPP
