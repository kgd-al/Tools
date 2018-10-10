#ifndef _UTILS_H_
#define _UTILS_H_

#include "cxxabi.h"

#include <numeric>
#include <algorithm>
#include <regex>
#include <sstream>

#include <vector>
#include <map>

/*!
 *\file utils.h
 *
 * Contains various functions/types used in a lot of places
 */

namespace utils {
// =============================================================================

/// Use the variable on non-debug builds to quiet the compilator down
#ifndef NDEBUG
#define assertVar(X)
#else
#define assertVar(X) (void)X;
#endif

/// Use values instead of std's references
/// \return the maximal between both arguments
template <typename T>
T vmax (T a, T b) { return a>b? a : b;  }

/// Use values instead of std's references
/// \return the maximal between both arguments
template <typename T>
T vmin (T a, T b) { return a<b? a : b;  }

/// The rad/degree ratio (180/pi)
extern double __toDegRatio;

/// Convert an angle in radian to degrees
inline double radToDeg(double rad) {
  return rad * __toDegRatio;
}

/// Compare two 3D vectors
template <typename T> bool operator< (const T& v0, const T& v1) {
  return (v0.x() != v1.x()) ?
        v0.x() < v1.x()
      : (v0.y() != v1.y()) ?
          v0.y() < v1.y()
        : v0.z() < v1.z();
}

/// \return \p val clipped in the [\p lower, \p upper] range
template <typename T> T clip (T lower, T val, T upper) {
  return std::max(lower, std::min(val, upper));
}

/// \return the sign of val (-1,0,1)
template <typename T> int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

/// \returns The unmangled name of the template class
template <typename T>
std::string className (void) {
  int status;
  char *demangled = abi::__cxa_demangle(typeid(T).name(),0,0,&status);
  std::string name (demangled);
  free(demangled);

  return name;
}

/// \returns The unmangled, unscoped name of the template class
/// eg foo::Bar becomes Bar and foo::Bar<foo::Baz> becomes Bar<Baz>
template <typename T>
std::string unscopedClassName (void) {
  static constexpr const char * RPL = "";
  std::string name = className<T>();
  return std::regex_replace(name, std::regex("([a-z]+::)+"), RPL);
}

/// \returns The innermost template argument of the provided string
/// eg "foo::Bar<foo::Baz>" becomes "Baz"
std::string innermostTemplateArgument (const std::string &cName);

/// \returns The innermost template argument of the provided class
/// eg foo::Bar<foo::Baz> becomes "Baz"
template <typename T>
std::string innermostTemplateArgument (void) {
  return innermostTemplateArgument(unscopedClassName<T>());
}

// =============================================================================
// == Personnal type traits

/// \brief Tests whether or not \p T can be called with these arguments
template<class T, class...Args>
struct is_callable {
  /// \cond deepmagic
  template<class U> static auto test(U* p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
  template<class U> static auto test(...) -> decltype(std::false_type());
  /// \endcond

  /// Whether or not \p T can be called with these arguments
  static constexpr bool value = decltype(test<T>(0))::value;
};

/// \brief Tests whether \p T has the same signature as an STL container
///
/// Credit goes to Mike Kinghan \@https://stackoverflow.com/a/16316640
template<typename T>
struct is_stl_container_like {
  /// \cond deepmagic
  typedef typename std::remove_const<T>::type test_type;

  template<typename A>
  static constexpr bool test(
    A * pt,
    A const * cpt = nullptr,
    decltype(pt->begin()) * = nullptr,
    decltype(pt->end()) * = nullptr,
    decltype(cpt->begin()) * = nullptr,
    decltype(cpt->end()) * = nullptr,
    typename A::iterator * pi = nullptr,
    typename A::const_iterator * pci = nullptr,
    typename A::value_type * pv = nullptr) {

    typedef typename A::iterator iterator;
    typedef typename A::const_iterator const_iterator;
    typedef typename A::value_type value_type;
    return  std::is_same<decltype(pt->begin()),iterator>::value &&
            std::is_same<decltype(pt->end()),iterator>::value &&
            std::is_same<decltype(cpt->begin()),const_iterator>::value &&
            std::is_same<decltype(cpt->end()),const_iterator>::value &&
            std::is_same<decltype(**pi),value_type &>::value &&
            std::is_same<decltype(**pci),value_type const &>::value;
  }

  template<typename A>
  static constexpr bool test(...) { return false; }

  static const bool value = test<test_type>(nullptr);
  /// \endcond
};

template<class T>
struct is_cpp_array : std::false_type {};

template<class T>
struct is_cpp_array<T[]> : std::true_type {};

template<class T, std::size_t N>
struct is_cpp_array<T[N]> : std::true_type {};

template <class T, std::size_t N>
struct is_cpp_array<std::array<T,N>> : std::true_type {};

// =============================================================================
// == Stream operators

/// Stream values from a std::array<...>
template <typename T, size_t SIZE>
std::ostream& operator<< (std::ostream &os, const std::array<T, SIZE> &a) {
  os << "[ ";
  for (auto &v: a) os << v << " ";
  return os << "]";
}

/// Stream values into a std::array<...>
template <typename T, size_t SIZE>
std::istream& operator>> (std::istream &is, std::array<T, SIZE> &a) {
  char c;
  is >> c;
  for (T &v: a) is >> v;
  return is;
}

/// Stream values into a std::vector<...>
template <typename T>
std::ostream& operator<< (std::ostream &os, const std::vector<T> &vec) {
  os << "[ ";
  for (auto &v: vec) os << v << " ";
  return os << "]";
}

/// Stream values from a std::pair<...>
template <typename T1, typename T2>
std::ostream& operator<< (std::ostream &os, const std::pair<T1, T2> &p) {
  return os << "{" << p.first << "," << p.second << "}";
}

/// \return a filled std::array
template <typename ARRAY>
ARRAY uniformStdArray(typename ARRAY::value_type v) {
  ARRAY a;
  a.fill(v);
  return a;
}

/// \return a std::array read from the provided json array
/// \throws std::logic_error if arrays have different sizes
template <typename JSON, typename ARRAY>
ARRAY readStdArray (const JSON &j) {
  using jsonArray = std::vector<typename ARRAY::value_type>;

  jsonArray ja = j.template get<jsonArray>();
  ARRAY a;

  if (ja.size() != a.size()) {
    std::ostringstream oss;
    oss << "Unable to parse [ ";
    for (auto &v: ja)   oss << v << " ";
    oss << "] as " << className<ARRAY>() << ": size mismatch";
    throw std::logic_error(oss.str());
  }

  for (uint i=0; i<a.size(); i++) a[i] = ja[i];
  return a;
}

/// Reset the provided stringstream (make empty and clears flags)
bool reset (std::stringstream &ss);

/// \returns The value identified by 'key'. It is removed from the container 'map'.
template <typename KEY, typename VAL>
VAL take (std::map<KEY, VAL> &map, KEY key) {
  auto it = map.find(key);
  VAL val = it->second;
  map.erase(it);
  return val;
}

/// \return a copy of \p str without leading and trailing spaces
std::string trimLeading (std::string str, const std::string &whitespaces=" \t");

/// \return a copy of \p str without spaces
std::string trim (std::string str);

/// \return a copy of \p str unquoted by one level
std::string unquote (std::string str);

/// \return The elements in the \p delim separated list \p s
std::vector<std::string> split(const std::string &s, char delim);

/// \return the contents of \p filename as a single string
std::string readAll (const std::string &filename);

/// \return the contents of \p ifs as a single string
std::string readAll (std::ifstream &ifs);

// =============================================================================

/// Helper structure for accessing current time
/// \code{cpp}
/// std::cout << "Current time is: " << CurrentTime{} << std::endl;
/// \endcode
struct CurrentTime {
  /// The format used for formatting
  const std::string format;

  /// Builds a time streamer using the (possibly default) specified format
  CurrentTime (const std::string &format="%c") : format(format) {}

  /// Insert the object into stream \p os
  friend std::ostream& operator<< (std::ostream &os, const CurrentTime &ct);
};

// =============================================================================

/// Provides checksum capabilities
template <typename JSON>
struct CRC32 {
  /// The integral type used to store the CRC
  using type = std::uint_fast32_t;

private:
  /// The lookup-table type
  using lookup_table_t = std::array<type, 256>;

  /// Generates a lookup table for the checksums of all 8-bit values.
  /// \authors https://rosettacode.org/wiki/CRC-32#C.2B.2B
  static lookup_table_t generate_crc_lookup_table() noexcept {
    auto const reversed_polynomial = type{0xEDB88320uL};

    // This is a function object that calculates the checksum for a value,
    // then increments the value, starting from zero.
    struct byte_checksum {
      type operator()(void) noexcept {
        auto checksum = static_cast<type>(n++);

        for (auto i = 0; i < 8; ++i)
          checksum = (checksum >> 1) ^ ((checksum & 0x1u) ? reversed_polynomial : 0);

        return checksum;
      }

      unsigned n = 0;
    };

    auto table = lookup_table_t{};
    std::generate(table.begin(), table.end(), byte_checksum{});

    return table;
  }

public:
  /// Calculates the CRC for any sequence of values. (You could use type traits
  /// and a static assert to ensure the values can be converted to 8 bits.)
  /// \authors https://rosettacode.org/wiki/CRC-32#C.2B.2B
  template <typename InputIterator>
  type operator() (InputIterator first, InputIterator last) const {
    // Generate lookup table only on first use then cache it (thread-safe).
    static lookup_table_t const table = generate_crc_lookup_table();

    // Calculate the checksum - make sure to clip to 32 bits, for systems that
    // don't have a true (fast) 32-bit type.
    return type{0xFFFFFFFFuL} &
          ~std::accumulate(first, last,
            ~type{0} & type{0xFFFFFFFFuL},
            [] (type checksum, type value) {
              return table[(checksum ^ value) & 0xFFu] ^ (checksum >> 8);
            }
          );
  }

  /// Function type for a converter json to byte array
  using Binarizer = std::vector<uint8_t> (*) (const JSON&);

  /// Computes the CRC associated to the provided json
  type operator() (const JSON &j, Binarizer binarizer = &JSON::to_cbor) const {
    auto bin = binarizer(j);
    return operator() (bin.begin(), bin.end());
  }
};

} // end of namespace utils


#endif // _UTILS_H_
