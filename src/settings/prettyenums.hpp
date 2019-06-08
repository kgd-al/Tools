#ifndef _PRETTY_ENUMS_HPP_
#define _PRETTY_ENUMS_HPP_

#include <set>

#include "../utils/utils.h"

/*!
 *\file prettyenums.hpp
 *
 * \brief Implements prettier enumerations.
 *
 * For an enumeration 'E' declared as DEFINE_PRETTY_ENUMERATION(E, ...) or
 *  DEFINE_UNSCOPED_PRETTY_ENUMERATION(E, ...), the structure EnumUtils<E> allows:
 *  - conversion to/from string with exception thrown on invalid inputs (case insensitive, with
 *  or without scope)
 *  - built-in i/o stream operators
 *  - safe conversion to/from uint
 *  - querying the enumeration name (as a std::string)
 *  - querying the number of elements
 *  - iterating through the values
 *
 * If a value has multiple aliases its string representation is that of the first entry.
 * It is not required for the values to be contiguous or in increasing order
 *
 * Exemples:
 * \code{cpp}
 * DEFINE_PRETTY_ENUMERATION(FOO, foo, bar, baz)
 * DEFINE_PRETTY_ENUMERATION(FOO, foo = 0, bar = 1, baz = 2)
 * DEFINE_PRETTY_ENUMERATION(FOO, foo = 1, bar = 2, baz = 4)
 * \endcode
 */

/// \cond internal
namespace _details {

/// Helper type to check if a type has been defined
template <class T, std::size_t = sizeof(T)>
std::true_type is_complete_impl(T *);

/// Default type denoting a declared but undefined type
std::false_type is_complete_impl(...);

/// \cond deepmagic
namespace deepmagic {
template <typename>
struct EnumMetaData;
}
/// \endcond

/// Helper type to check if a type is an enumeration with associated metadata
template <class T>
struct is_pretty_enum {
  /// Whether or not type \p T has enumeration metadata
  static constexpr bool value =
      decltype(is_complete_impl(std::declval<deepmagic::EnumMetaData<T>*>()))::value;
};

/// \return the number of comma-separated values in \p s
constexpr uint countValues (const char *s) {
  uint count = 0;
  const char *current = s;
  while (*current) {
    if ((*current) == ',')
      count++;
    current++;
  }
  return count+1;
}


/// \cond deepmagic
namespace enum_sequence_helper {

template <typename E, E... es>
struct iseq {
  using type = iseq;
  using stdtype = std::integer_sequence<E, es...>;
};

template <typename E, class Sequence1, class Sequence2>
struct _merge_and_renumber;

template <typename E, E... I1, E... I2>
struct _merge_and_renumber<E, iseq<E, I1...>, iseq<E, I2...>>
    : iseq<E, I1..., (E(sizeof...(I1)+uint(I2)))...>
{};

// --------------------------------------------------------------

template <typename E, size_t N>
struct make_index_sequence
    : _merge_and_renumber<E, typename make_index_sequence<E, N/2>::type,
    typename make_index_sequence<E, N - N/2>::type>
{};

template<typename E> struct make_index_sequence<E, 0> : iseq<E> { };
template<typename E> struct make_index_sequence<E, 1> : iseq<E, E(0)> { };

} // end of namespace enum_sequence_helper
/// \endcond

} // end of namespace details
/// \endcond

/// Provides reflexive information on an enumeration
/// \tparam E a prettied-up enumeration
template <typename E> class EnumUtils {
public:
  /// The underlying type of the managed enum
  using underlying_t = typename std::underlying_type<E>::type;

private:
  /// Alias for the associated metadata
  using MetaData = _details::deepmagic::EnumMetaData<E>;

  /// Extract the values for the enumeration described in the MetaData type and populates the
  /// various lists
  struct Maps {

    /// Case insensitive comparison functor for strings
    struct CaseInsensitiveLess {

      /// Case insensitive comparison functor for characters
      struct CaseInsensitiveCompare {

        /// \return the case insensitive comparison between \p c1 and \p c2
        bool operator() (const unsigned char &c1, const unsigned char &c2) const {
          return toupper(c1) < toupper(c2);
        }
      };

      /// \return the case insensitive comparison between \p s1 and \p s2
      bool operator() (const std::string &s1, const std::string &s2) const {
        return std::lexicographical_compare (
          s1.begin(), s1.end(), s2.begin(), s2.end(), CaseInsensitiveCompare()
        );
      }
    };

    /// Alias for a set of enumeration values
    using EnumValues = std::set<E>;

    /// The values described by \p E
    EnumValues values;

    /// Alias for a set of the underlying values
    using UnderlyingValues = std::set<underlying_t>;

    /// The underlying values described by \p E
    UnderlyingValues uvalues;

    /// Alias for a map type from enumeration value to name
    using EnumValueNameMap = std::map<E, std::string>;

    /// Maps enumeration values to their names
    EnumValueNameMap valueToName;

    /// Maps enumeration values to their prettified names
    EnumValueNameMap valueToPrettyName;

    /// Alias for a map type from enumeration name to value
    using EnumNameValueMap = std::map<std::string, E, CaseInsensitiveLess>;

    /// Maps enumeration names to their values
    EnumNameValueMap nameToValue;

    /// Number of enumerators
    static const uint _size = _details::countValues(MetaData::values());

    Maps (void) {
      // Retrieve the enumerators
      std::vector<std::string> names =
          utils::split(utils::trim(MetaData::values()), ',');

      int i=0;
      for (const std::string &iterator: names) {
        /// Parse the enumerator
        std::vector<std::string> eqtokens = utils::split(iterator, '=');
        std::string name = eqtokens[0];
        std::string pname = prettyEnumName(name);
        E value;

        if (eqtokens.size() == 1) { // Name only -> assign next value
          value = E(i++);
          valueToName.insert({value, name});
          valueToPrettyName.insert({value, pname});

        } else { // Value provided
          std::istringstream iss (eqtokens.at(1));
          uint v;
          iss >> v;

          if (iss) { // Value was an int -> use it
            value = E(v);
            valueToName.insert({value, name});
            valueToPrettyName.insert({value, pname});
            i = v;

          } else // Value was a name -> use its value
            value = nameToValue.at(eqtokens.at(1));
        }

        // Populate the containers
        values.insert(value);
        uvalues.insert(underlying_t(value));
        nameToValue.insert({name, value});
        nameToValue.insert({addScope(name), value});
        nameToValue.insert({pname, value});
      }
    }

    /// \return A properly formatted enumerator name (lower case, with spaces)
    std::string prettyEnumName (const std::string &name) {
      std::string prettyName (name);
      prettyName[0] = toupper(prettyName[0]);
      transform(prettyName.begin()+1, prettyName.end(), prettyName.begin()+1, ::tolower);
      std::replace(prettyName.begin(), prettyName.end(), '_', ' ');
      return prettyName;
    }
  };

  /// Allows internal access the Map singleton associated with this enumeration
  static const Maps& getMaps (void) {
    static const Maps localMaps;
    return localMaps;
  }

  /// Helper methods returning the enumerator value prepended with the enumeration name
  inline static std::string addScope (const std::string &s) {
    return std::string(name()) + "::" + s;
  }

public:
  /// the type of an uniform distribution that can generate valid random enumerator
  /// \see Dice::roll(ENUM)
  using dist_t = std::uniform_int_distribution<underlying_t>;

  /// \return The lowest enumerator
  static constexpr E min (void) { return getMaps().values.front(); }

  /// \return The underlying value of the lowest enumerator
  static constexpr underlying_t minU (void) { return underlying_t(min()); }

  /// \return The largest enumerator
  static constexpr E max (void) { return getMaps().values().back(); }

  /// \return The underlying value of the largest enumerator
  static constexpr underlying_t maxU (void) { return underlying_t(max()); }

  /// \return a uniform distribution object
  /// \see Dice::roll(ENUM) and dist_t
  static constexpr const auto& values (void) {
    return getMaps().values;
  }

  /// Converts a value of the underlying type into an enumerator. No checks are performed
  static constexpr E fromUnderlying (underlying_t value) {
    return E(value);
  }

  /// Converts an enumerator into a value of the underlying type. No checks are performed
  static constexpr underlying_t toUnderlying (E value) {
    return underlying_t(value);
  }

  /// \return The name associated with the enumerator equivalent to this value, if any.
  /// \throws std::out_of_range if the enumerator does not exist
  static const std::string& getName (uint iValue, bool pretty = true) {
    return getName(E(iValue), pretty);
  }

  /// \return The name associated with this enumerator.
  /// \throws std::out_of_range if the enumerator does not exist
  static const std::string& getName (E value, bool pretty = true) {
    if (pretty) return getMaps().valueToPrettyName.at(value);
    else        return getMaps().valueToName.at(value);
  }

  /// \return The scoped name (prepended with the enumeration name) associated with this enumerator.
  /// \throws std::out_of_range if the enumerator does not exist
  static std::string getScopedName (E value) {
    return addScope(getName(value, false));
  }

  /// \return The enumerator associated with this name
  /// \throws std::out_of_range if no such enumerator exists
  static E getValue (std::string name) {
    name = utils::trimLeading(name);
    std::replace(name.begin(), name.end(), '_', ' ');
    try {
      return getMaps().nameToValue.at(name);
    } catch (std::out_of_range&) {
      std::ostringstream oss;
      oss << "'" << name << "' is not a valid enumerator for '"
          << EnumUtils::name();
      throw std::out_of_range(oss.str());
    }
  }

  /// Asserts: 0 &le; toUnderlying(value) < size()
  static constexpr bool isValid (E value) {
    return getMaps().values.find(value) != getMaps().values.end();
  }

  /// \return The name of this enumeration
  static constexpr const char* name (void) {
    return MetaData::name();
  }

  /// \return the number of different enumerators
  static constexpr uint size (void) {
    return Maps::_size;
  }

  /// \return An interator suitable for use in for-range loops
  static const auto& iterator (void) {
    return getMaps().values;
  }

  /// \return An interator suitable for for-range loops over the underlying
  /// values
  static const auto& iteratorUValues (void) {
    return getMaps().uvalues;
  }

  /// Provides an instantiation of the type std::integer_sequence<E, E::Value1, E::Value2, ...>
  using enum_sequence =
  typename _details::enum_sequence_helper::make_index_sequence<E, size()>::stdtype;
};

/// Prints the scoped name for a pretty-enum value \p e to stream \p os
template <typename E, std::enable_if_t<_details::is_pretty_enum<E>::value, int> = 0>
std::ostream& operator<< (std::ostream &os, const E& e) {
  return os << EnumUtils<E>::getScopedName(e);
}

/// Read a pretty-enum value \p e from stream \p os
/// \attention space support is brittle (at best)
template <typename E, std::enable_if_t<_details::is_pretty_enum<E>::value, int> = 0>
std::istream& operator>> (std::istream &is, E& e) {
  std::string val ("");
  bool ok = false;

  do {
    /// Gobble up a space-delimited value
    std::string v;
    is >> v;
    if (!val.empty()) val += " ";
    val += v;

    /// Do not panic if stream is empty
    try {
      e = EnumUtils<E>::getValue(val);
      ok = true;
    } catch (std::out_of_range&) {}

    /// Keep going while there are values to read
  } while (!ok && is);

  if (!ok) {
    std::ostringstream oss;
    oss << "Unable to transform " + val + " into enum value of type " + EnumUtils<E>::name();
    throw std::out_of_range(oss.str());
  }

  return is;
}

/// Define a pretty enumeration residing in scope \p NAME and no namespace
#define DEFINE_PRETTY_ENUMERATION(NAME, ...)      \
  __PRETTY_ENUM_DECLARE(class, NAME, __VA_ARGS__) \
  __PRETTY_ENUM_METADATA(, NAME, __VA_ARGS__)

/// Define a pretty enumeration residing in global scope
#define DEFINE_UNSCOPED_PRETTY_ENUMERATION(NAME, ...) \
  __PRETTY_ENUM_DECLARE(, NAME, __VA_ARGS__)          \
  __PRETTY_ENUM_METADATA(, NAME, __VA_ARGS__)

/// Define a pretty enumeration residing in namespace \p NAMESPACE
#define DEFINE_NAMESPACE_PRETTY_ENUMERATION(NAMESPACE, NAME, ...)     \
  namespace NAMESPACE { __PRETTY_ENUM_DECLARE(, NAME, __VA_ARGS__) }  \
  __PRETTY_ENUM_METADATA(NAMESPACE, NAME, __VA_ARGS__)


/// \cond internal

/// Declare a pretty enumeration, possibly with a scope
#define __PRETTY_ENUM_DECLARE(SCOPE, NAME, ...) \
  enum SCOPE NAME { __VA_ARGS__ };

/// Defines the pretty enumeration metadata
#define __PRETTY_ENUM_METADATA(NAMESPACE, NAME, ...)    \
  namespace _details::deepmagic {                       \
    template <> struct EnumMetaData<NAMESPACE::NAME> {  \
      static constexpr const char * name (void) {       \
        return #NAME;                                   \
      }                                                 \
      static constexpr const char * values (void) {     \
      return #__VA_ARGS__;                              \
      }                                                 \
    };                                                  \
  }                                                     \
/// \endcond

#endif // _PRETTY_ENUMS_HPP_
