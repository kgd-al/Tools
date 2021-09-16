#ifndef KGD_UTILS_H
#define KGD_UTILS_H

#include "cxxabi.h"

#include <cmath>
#include <numeric>
#include <algorithm>
#include <regex>
#include <sstream>

#include <array>
#include <vector>
#include <set>
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

/// \return \p val clipped (in-place) in the [\p lower, \p upper] range
template <typename T> T iclip(T lower, T &val, T upper) {
  return val = std::max(lower, std::min(val, upper));
}

/// \p val clipped (in-place) the [lower, inf] range
template <typename T> T iclip_min(T lower, T &val) {
  if (val < lower)  val = lower;
  return val;
}

/// \p val clipped (in-place) the [-inf,upper] range
template <typename T> T iclip_max(T &val, T upper) {
  if (upper < val)  val = upper;
  return val;
}

/// \return \p val clipped in the [\p lower, \p upper] range
template <typename T> T clip (T lower, T val, T upper) {
  return iclip(lower, val, upper);
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

  using TR = typename std::remove_reference<T>::type;
  std::string prefix, suffix;
  if (std::is_const<TR>::value)
      prefix += "const ";
  if (std::is_volatile<TR>::value)
      prefix += "volatile ";
  if (std::is_lvalue_reference<T>::value)
      suffix += "&";
  else if (std::is_rvalue_reference<T>::value)
      suffix += "&&";
  return prefix + name + suffix;
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
    typename A::value_type * /*pv*/ = nullptr) {

    typedef typename A::iterator iterator;
    typedef typename A::const_iterator const_iterator;
    typedef typename A::value_type value_type;
    return  std::is_same<decltype(pt->begin()),iterator>::value &&
            std::is_same<decltype(pt->end()),iterator>::value &&
            std::is_same<decltype(cpt->begin()),const_iterator>::value &&
            std::is_same<decltype(cpt->end()),const_iterator>::value &&
            (
                 std::is_same<decltype(**pi),value_type &>::value
              || std::is_same<decltype(**pi),value_type const &>::value
            ) &&
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

template<typename T> struct is_std_vector : public std::false_type {};

template<typename T, typename A>
struct is_std_vector<std::vector<T, A>> : public std::true_type {};

/// Helper type trait to check whether \p Base<...> is a base class of Derived
template <template <typename...> class Base, typename Derived>
struct is_base_template_of {

  /// Helper alias to the derived type with no const qualificiation
  using U = typename std::remove_cv<Derived>::type;

  /// Test case for self aware genomes
  template <typename... Args>
  static std::true_type test(Base<Args...>*);

  /// Test case for other types
  static std::false_type test(void*);

  /// Whether or not \p Derived is a subclass of \p Base
  static constexpr bool value =
      decltype(test(std::declval<U*>()))::value;
};

// =============================================================================
// == Reversed iterable
// == Credit goes to Prikso NAI @https://stackoverflow.com/a/28139075

/// \cond internal

/// Type wrapper for a reverse-iterated standard container
template <typename T>
struct reversion_wrapper {  T& iterable; /*!< The iterable container */ };

/// Mapping rbegin to begin for reverse iteration
template <typename T>
auto begin (reversion_wrapper<T> w) { return std::rbegin(w.iterable); }

/// Mapping rend to end for reverse iteration
template <typename T>
auto end (reversion_wrapper<T> w) { return std::rend(w.iterable); }
/// \endcond

/// Reverse iterator for painless injection in for-range loops
template <typename T>
reversion_wrapper<T> reverse (T&& iterable) { return { iterable }; }

// =============================================================================
// == Stream operators

/// Throws an exception of type T with the message built from the variadic
/// arguments
template <typename T, typename... ARGS>
void doThrow (ARGS... args) {
  std::ostringstream oss;
  using expander = int[];
  (void)expander{0, (void(oss << std::forward<ARGS>(args)),0)...};
  throw T(oss.str());
}

/// Stream values from a std::array<...>
template <typename T, size_t SIZE>
std::ostream& operator<< (std::ostream &os, const std::array<T, SIZE> &a) {
  os << "[ ";
  for (const auto &v: a) os << v << " ";
  return os << "]";
}

/// Stream values into a std::array<...>
template <typename T, size_t SIZE>
std::istream& operator>> (std::istream &is, std::array<T, SIZE> &a) {
  char c;
  is >> c;
  for (T &v: a) is >> v;
  is >> c;
  return is;
}

/// Stream values from a std::vector<...>
template <typename T>
std::ostream& operator<< (std::ostream &os, const std::vector<T> &vec) {
  os << "[ ";
  for (const auto &v: vec) os << v << " ";
  return os << "]";
}

/// Stream values from a std::set<...>
template <typename T, typename C>
std::ostream& operator<< (std::ostream &os, const std::set<T, C> &s) {
  os << "[ ";
  for (const auto &v: s) os << v << " ";
  return os << "]";
}

/// Stream values from a std::pair<...>
template <typename T1, typename T2>
std::ostream& operator<< (std::ostream &os, const std::pair<T1, T2> &p) {
  return os << "{" << p.first << "," << p.second << "}";
}

/// \return a filled std::array
template <typename ARRAY>
constexpr ARRAY uniformStdArray(typename ARRAY::value_type v) {
  ARRAY a {};
  for (auto &i: a)  i = v;
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
  if (it == map.end())
    doThrow<std::invalid_argument>("'", key, "' is not a key of the provided map");
  VAL val = it->second;
  map.erase(it);
  return val;
}

/// Normalizes the provided value-rate map
template <typename T>
void normalize (std::map<T, float> &m) {
  float sum = 0;
  for (const auto &p: m)  sum += p.second;
  for (auto &p: m)  p.second /= sum;
}

/// Generates a normalized value-rate map from the provided initializer list
template <typename T>
std::map<T, float> normalize(std::initializer_list<std::pair<const T,
                                                             float>> l) {
  std::map<T, float> m (l);
  normalize(m);
  return m;
}

/// Generates a normalized name-rate map from the provided initializer list
inline std::map<std::string, float>
normalizeRates (std::initializer_list<std::pair<const std::string, float>> l) {
  return normalize<std::string>(l);
}

/// \return a copy of \p str without leading and trailing spaces
std::string trimLeading (std::string str, const std::string &whitespaces=" \t");

/// \return a copy of \p str without spaces
std::string trim (std::string str);

/// \return a copy of \p str unquoted by one level
std::string unquote (std::string str);

/// \return The elements in the \p delim separated list \p s
std::vector<std::string> split(const std::string &s, char delim);

/// \return The elements in the provided iterable delimited by \p delim
template <typename IT>
std::string join (const IT &begin, const IT &end, const std::string &delim) {
  std::string s;
  for (IT it = begin; it != end; ++it) {
    if (it != begin) s += delim;
    s += *it;
  }
  return s;
}

/// \return the contents of \p filename as a single string
std::string readAll (const std::string &filename);

/// \return the contents of \p ifs as a single string
std::string readAll (std::ifstream &ifs);

// =============================================================================

/// Strong-typed environment extractor
template <typename T>
bool getEnv (const char *name, T &value) {
  if (const char *env = getenv(name)) {
    std::stringstream ss;
    ss << env;
    ss >> value;
    return bool(ss);
  }
  return false;
}


// =============================================================================

// =============================================================================

/// Gobbles up variadic arguments (@see make_if)
template <typename ...ARGS>
void gobbleUnused (ARGS&& ...) {}

/// Empty structure (@see make_if)
struct __attribute__((__unused__)) Nothing {};

/// Returns an object of type as produced by T(args...) IFF v is true
/// Otherwise returns a 'Nothing' object
template <bool v, typename T, typename ...ARGS>
auto make_if (ARGS&&... args) {
  if constexpr (v)
    return T(std::forward<ARGS>(args)...);
  else {
    gobbleUnused(args...);
    return Nothing{};
  }
}

// =============================================================================

/// Helper structure for accessing current time
/// \code{cpp}
/// std::cout << "Current time is: " << CurrentTime{} << std::endl;
/// \endcode
struct CurrentTime {
  /// The format to use unless otherwise specified
  static constexpr auto defaultFormat = "%c";

  /// The format used for formatting
  const std::string format;

  /// Builds a time streamer using the (possibly default) specified format
  CurrentTime (const std::string &format=defaultFormat) : format(format) {}

  /// Try to determine the char count of the displayed time according to the
  /// provided format
  static uint width (const std::string &format=defaultFormat);

  /// Insert the object into stream \p os
  friend std::ostream& operator<< (std::ostream &os, const CurrentTime &ct);
};

// =============================================================================
// Cleaner management of IDs

/// Helper struct for strongly encapsulating an integer-based id
template <typename T>
struct GenomeID {
  using ut = uint;  ///< The underlying type

  /// The containing enumeration
  enum class ID : ut {
    INVALID = 0 ///< IDs are 1-based, zero is reserved for errors
  };

  ID id;  ///< The actual value

  /// Default-constructs to the first valid id
  GenomeID (void) : id(next(ID::INVALID)) {}

  /// Explicit construct using the provided value
  /// \warning IDs are 1-based so the actual value is incremented
  explicit GenomeID (ut value) {
    id = next(ID(value));
  }

  /// Access the id
  constexpr operator ID (void) const {
    return id;
  }

  /// Increment an id
  static constexpr GenomeID next (GenomeID &gid) {
    GenomeID current = gid;
    gid.id = next(gid.id);
    return current;
  }

private:
  /// Private increment helper
  static constexpr ID next (ID id) {  return ID(ut(id)+1);  }
};

/// IDs are transparently streamable
template <typename T>
std::ostream& operator<< (std::ostream &os, const GenomeID<T> &gid) {
  return os << typename GenomeID<T>::ut(gid.id);
}

/// IDs are transparently comparable
template <typename T>
bool operator< (const GenomeID<T> &lhs, const GenomeID<T> &rhs) {
  return lhs.id < rhs.id;
}

/// Generate an ID field for a given structure
#define HAS_GENOME_ID(C) \
  using ID = utils::GenomeID<C>; \
  ID id;

// =============================================================================

/// Provides checksum capabilities
template <typename JSON>
struct CRC32 {
  /// The integral type used to store the CRC
  using type = std::uint_fast32_t;

  /// Number of bytes used to represent this CRC
  static constexpr size_t bytes = 4;

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


#endif // KGD_UTILS_H
