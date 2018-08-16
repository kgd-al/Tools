#ifndef _UTILS_H_
#define _UTILS_H_

#include "cxxabi.h"

#include <numeric>

namespace utils {
#ifndef NDEBUG
#define assertVar(X)
#else
#define assertVar(X) (void)X;
#endif

template <typename T>
T vmax (T a, T b) { return a>b? a : b;  }

template <typename T>
T vmin (T a, T b) { return a<b? a : b;  }

extern double __toDegRatio;
inline double radToDeg(double rad) {
    return rad * __toDegRatio;
}

template <typename T> bool operator< (const T& v0, const T& v1) {
    return (v0.x() != v1.x()) ?
                v0.x() < v1.x()
              : (v0.y() != v1.y()) ?
                    v0.y() < v1.y()
                  : v0.z() < v1.z();
}

template <typename T> T clip (T lower, T val, T upper) {
    return std::max(lower, std::min(val, upper));
}

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

/*!
  \returns The unmangled name of the template class
*/
template <typename T>
std::string className (void) {
    int status;
    char *demangled = abi::__cxa_demangle(typeid(T).name(),0,0,&status);
    std::string name (demangled);
    free(demangled);

    return name;
}

template <typename T>
std::string unscopedClassName (void) {
    std::string name = className<T>();
    size_t nameStart = name.find_last_of(':');
    if (nameStart != std::string::npos)
            return name.substr(nameStart+1);
    else    return name;
}

/*!
 * \@brief Tests whether or not T can be called with these arguments
 */
template<class T, class...Args>
struct is_callable {
    template<class U> static auto test(U* p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
    template<class U> static auto test(...) -> decltype(std::false_type());

    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T, size_t SIZE>
std::ostream& operator<< (std::ostream &os, const std::array<T, SIZE> &a) {
    os << "[ ";
    for (auto &v: a) os << v << " ";
    return os << "]";
}

template <typename T, size_t SIZE>
std::istream& operator>> (std::istream &is, std::array<T, SIZE> &a) {
    char c;
    is >> c;
    for (T &v: a) is >> v;
    return is;
}

template <typename T1, typename T2>
std::ostream& operator<< (std::ostream &os, const std::pair<T1, T2> &p) {
    return os << "{" << p.first << "," << p.second << "}";
}
template <typename ARRAY>
ARRAY uniformStdArray(typename ARRAY::value_type v) {
    ARRAY a;
    a.fill(v);
    return a;
}

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

/*!
 * \returns The value identified by 'key'. It is removed from the container 'map'.
 */
template <typename KEY, typename VAL>
VAL take (std::map<KEY, VAL> &map, KEY key) {
    auto it = map.find(key);
    VAL val = it->second;
    map.erase(it);
    return val;
}

/*! \brief Removes leading and trailing spaces from 'str' */
std::string trimLeading (std::string str, const std::string &whitespaces=" \t");

/*! \brief Removes all space from 'str' */
std::string trim (std::string str);

/*! \brief Removes one layer of quoting from 'str' */
std::string unquote (std::string str);

/*! \brief Cuts 's' in tokens separated by the character 'delim' */
std::vector<std::string> split(const std::string &s, char delim);

std::string readAll (const std::string &filename);
std::string readAll (std::ifstream &ifs);

struct CurrentTime {
    const std::string format;

    CurrentTime (const std::string &format="%c") : format(format) {}

    friend std::ostream& operator<< (std::ostream &os, const CurrentTime &ct);
};


/*! \brief Provides checksum capabilities */
template <typename JSON>
struct CRC32 {
    using type = std::uint_fast32_t;

private:
    using lookup_table_t = std::array<type, 256>;

    /*!
     * Generates a lookup table for the checksums of all 8-bit values.
     * \authors https://rosettacode.org/wiki/CRC-32#C.2B.2B
     */
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
    /*!
     * Calculates the CRC for any sequence of values. (You could use type traits and a
     * static assert to ensure the values can be converted to 8 bits.)
     * \authors https://rosettacode.org/wiki/CRC-32#C.2B.2B
     */
    template <typename InputIterator>
    type operator() (InputIterator first, InputIterator last) const {
        // Generate lookup table only on first use then cache it - this is thread-safe.
        static lookup_table_t const table = generate_crc_lookup_table();

        // Calculate the checksum - make sure to clip to 32 bits, for systems that don't
        // have a true (fast) 32-bit type.
        return type{0xFFFFFFFFuL} &
              ~std::accumulate(first, last,
                  ~type{0} & type{0xFFFFFFFFuL},
                    [] (type checksum, type value) {
                        return table[(checksum ^ value) & 0xFFu] ^ (checksum >> 8);
                    }
              );
    }

    using Binarizer = std::vector<uint8_t> (*) (const JSON&);
    type operator() (const JSON &j, Binarizer binarizer = &JSON::to_cbor) const {
        auto bin = binarizer(j);
        return operator() (bin.begin(), bin.end());
    }
};

} // end of namespace utils


#endif // _UTILS_H_
