#ifndef _PRETTY_ENUMS_HPP_
#define _PRETTY_ENUMS_HPP_

#include <set>

#include "../utils/utils.h"

/*!
 *\file Implements prettier enumerations.
 *
 * For an enumeration 'E' declared as #DEFINE_PRETTY_ENUMERATION(E, ...) or
 *  #DEFINE_UNSCOPED_PRETTY_ENUMERATION(E, ...), the structure EnumUtils<E> allows:
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
 * DEFINE_PRETTY_ENUMERATION(FOO, foo, bar, baz)
 * DEFINE_PRETTY_ENUMERATION(FOO, foo = 0, bar = 1, baz = 2)
 * DEFINE_PRETTY_ENUMERATION(FOO, foo = 1, bar = 2, baz = 4)
 */

namespace _details {

template <class T, std::size_t = sizeof(T)>
std::true_type is_complete_impl(T *);

std::false_type is_complete_impl(...);

template <typename>
struct EnumMetaData;

template <class T>
struct is_pretty_enum {
    static constexpr bool value =
        decltype(is_complete_impl(std::declval<EnumMetaData<T>*>()))::value;
};

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

} // end of namespace details


template <typename>
struct EnumUtils;

template <typename E> class EnumUtils {

    using MetaData = _details::EnumMetaData<E>;

    /*!
     * \brief Extract the values for the enumeration described in the MetaData type and builds
     * the name \leftrightarrow value maps as well as the number of distinct values
     */
    struct Maps {
        struct CaseInsensitiveLess {
            struct CaseInsensitiveCompare {
                bool operator() (const unsigned char &c1, const unsigned char &c2) const {
                    return toupper(c1) < toupper(c2);
                }
            };
            bool operator() (const std::string &s1, const std::string &s2) const {
                return std::lexicographical_compare (
                    s1.begin(), s1.end(), s2.begin(), s2.end(), CaseInsensitiveCompare()
                );
            }
        };

        using EnumValues = std::set<E>;
        EnumValues values;

        using EnumValueNameMap = std::map<E, std::string>;
        EnumValueNameMap valueToName;

        using EnumNameValueMap = std::map<std::string, E, CaseInsensitiveLess>;
        EnumNameValueMap nameToValue;

        static const uint _size = _details::countValues(MetaData::values());

        Maps (void) {
            std::vector<std::string> names =
                utils::split(utils::trim(MetaData::values()), ',');
            int i=0;
            for (const std::string &iterator: names) {
                std::vector<std::string> eqtokens = utils::split(iterator, '=');
                std::string name = prettyEnumName(eqtokens[0]);
                E value;
                if (eqtokens.size() == 1) { // Name only -> assign next value
                    value = E(i++);
                    valueToName.insert({value, name});
                } else { // Value provided
                    std::istringstream iss (eqtokens.at(1));
                    uint v;
                    iss >> v;
                    if (iss) { // Value was an int -> use it
                        value = E(v);
                        valueToName.insert({value, name});
                        i = v;
                    } else { // Value was a name -> use its value
                        value = nameToValue.at(eqtokens.at(1));
                    }
                }
                values.insert(value);
                nameToValue.insert({name, value});
                nameToValue.insert({addScope(name), value});
            }
        }

        std::string prettyEnumName (const std::string &name) {
            std::string prettyName (name);
            prettyName[0] = toupper(prettyName[0]);
            transform(prettyName.begin()+1, prettyName.end(), prettyName.begin()+1, ::tolower);
            std::replace(prettyName.begin(), prettyName.end(), '_', ' ');
            return prettyName;
        }
    };

    /*! \brief Allows internal access the Map singleton associated with this enumeration */
    static const Maps& getMaps (void) {
        static const Maps localMaps;
        return localMaps;
    }

    /*! \brief Helper methods returning the enumerator value prepended with the enumeration name */
    inline static std::string addScope (const std::string &s) {
        return std::string(name()) + "::" + s;
    }

public:
    /*! The underlying is currently fixed to unsigned int */
    using underlying_t = uint;

    /*! the type of an uniform distribution that can generate valid random enumerator
     * \see Dice::roll(ENUM)
     */
    using dist_t = std::uniform_int_distribution<underlying_t>;

    static constexpr E min (void) { return getMaps().values.front(); }
    static constexpr underlying_t minU (void) { return underlying_t(min()); }

    static constexpr E max (void) { return getMaps().values().back(); }
    static constexpr underlying_t maxU (void) { return underlying_t(max()); }

    /*! \return a uniform distribution object
     * @see Dice::roll(ENUM) and dist_t
     */
    static constexpr const auto& values (void) {
        return getMaps().values;
    }

    /*! Converts a value of the underlying type into an enumerator. No checks are performed */
    static constexpr E fromUnderlying (underlying_t value) {
        return E(value);
    }

    /*! Converts an enumerator into a value of the underlying type. No checks are performed */
    static constexpr underlying_t toUnderlying (E value) {
        return underlying_t(value);
    }

    /*! \return The name associated with the enumerator equivalent to this value, if any.
     * \throws std::out_of_range if the enumerator does not exist
     */
    static const std::string& getName (uint iValue) {
        return getName(E(iValue));
    }

    /*! \return The name associated with this enumerator.
     * \throws std::out_of_range if the enumerator does not exist
     */
    static const std::string& getName (E value) {
        return getMaps().valueToName.at(value);
    }

    /*! \return The scoped name (prepended with the enumeration name) associated with this enumerator.
     * \throws std::out_of_range if the enumerator does not exist
     */
    static const std::string getScopedName (E value) {
        return addScope(getName(value));
    }

    /*! \return The enumerator associated with this name
     * \throws std::out_of_range if no such enumerator exists
     */
    static E getValue (const std::string &name) {
        return getMaps().nameToValue.at(utils::trimLeading(name));
    }

    /*! Asserts: 0 \leq toUnderlying(value) < size() */
    static constexpr bool isValid (E value) {
        return getMaps().values.find(value) != getMaps().values.end();
    }

    /*! \return The name of this enumeration */
    static constexpr const char* name (void) {
        return MetaData::name();
    }

    /*! \return the number of different enumerators */
    static constexpr uint size (void) {
        return Maps::_size;
    }

    /*! \return An interator suitable for use in for-range loops */
    static const auto& iterator (void) {
        return getMaps().values;
    }

    /*!
     * Provides an instantiation of the type std::integer_sequence<E, E::Value1, E::Value2, ...>
     */
    using enum_sequence =
        typename _details::enum_sequence_helper::make_index_sequence<E, size()>::stdtype;
};

template <typename E, std::enable_if_t<_details::is_pretty_enum<E>::value, int> = 0>
std::ostream& operator<< (std::ostream &os, const E& e) {
    return os << EnumUtils<E>::getScopedName(e);
}

template <typename E, std::enable_if_t<_details::is_pretty_enum<E>::value, int> = 0>
std::istream& operator>> (std::istream &is, E& e) {
    std::string val;
    is >> val;
    e = EnumUtils<E>::getValue(val);
    return is;
}

#define DEFINE_PRETTY_ENUMERATION(NAME, ...)        \
    __PRETTY_ENUM_DECLARE(class, NAME, __VA_ARGS__) \
    __PRETTY_ENUM_METADATA(, NAME, __VA_ARGS__)

#define DEFINE_UNSCOPED_PRETTY_ENUMERATION(NAME, ...) \
    __PRETTY_ENUM_DECLARE(, NAME, __VA_ARGS__)   \
    __PRETTY_ENUM_METADATA(, NAME, __VA_ARGS__)

#define DEFINE_NAMESPACE_PRETTY_ENUMERATION(NAMESPACE, NAME, ...)           \
    namespace NAMESPACE { __PRETTY_ENUM_DECLARE(, NAME, __VA_ARGS__) } \
    __PRETTY_ENUM_METADATA(NAMESPACE, NAME, __VA_ARGS__)

#define __PRETTY_ENUM_DECLARE(SCOPE, NAME, ...) \
    enum SCOPE NAME { __VA_ARGS__ };

#define __PRETTY_ENUM_METADATA(NAMESPACE, NAME, ...)    \
    namespace _details {                                \
    template <> struct EnumMetaData<NAMESPACE::NAME> {  \
        static constexpr const char * name (void) {     \
            return #NAME;                               \
        }                                               \
        static constexpr const char * values (void) {   \
            return #__VA_ARGS__;                        \
        }                                               \
    };                                                  \
    }                                                   \

#endif // _PRETTY_ENUMS_HPP_
