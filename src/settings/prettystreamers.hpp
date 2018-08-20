#ifndef _PRETTY_STREAMERS_HPP_
#define _PRETTY_STREAMERS_HPP_

#include <ostream>
#include <map>
#include <set>

#include "../random/dice.hpp"
#include "prettyenums.hpp"

// =================================================================================================
    // Helpers for write/read operations formating

// Pretty print/read base templates =============

template <typename T, typename Enabled = void>
struct PrettyWriter {
    void operator() (std::ostream &os, const T &value) {
        using utils::operator<<;
        os << value;
    }
};

template <typename T, typename Enabled = void>
struct PrettyReader {
    bool operator() (std::istream &is, T &value) {
        using utils::operator>>;
        return bool(is >> value);
    }
};


// Pretty Dices =================================

template <> struct PrettyWriter<rng::FastDice, void> {
    void operator() (std::ostream &os, const rng::FastDice &d) {
        os << d.getSeed();
    }
};
template <> struct PrettyReader<rng::FastDice, void> {
    bool operator() (std::istream &is, rng::FastDice &d) {
        rng::FastDice::Seed_t seed;
        is >> seed;
        d .reset(seed);
        return bool(is);
    }
};

template <> struct PrettyWriter<rng::AtomicDice, void> {
    void operator() (std::ostream &os, const rng::AtomicDice &d) {
        os << d.getSeed();
    }
};
template <> struct PrettyReader<rng::AtomicDice, void> {
    bool operator() (std::istream &is, rng::AtomicDice &d) {
        rng::AtomicDice::Seed_t seed;
        is >> seed;
        d.reset(seed);
        return bool(is);
    }
};

// Pretty enums =================================

template <typename E>
struct PrettyWriter<E, typename std::enable_if<_details::is_pretty_enum<E>::value>::type> {
    void operator() (std::ostream &os, const E &e) {
        os << EnumUtils<E>::getName(e);
    }
};

template <typename E>
struct PrettyReader<E, typename std::enable_if<_details::is_pretty_enum<E>::value>::type> {
    bool operator() (std::istream &is, E &e) {
        is >> e;
        return bool(is);
    }
};


// Pretty bools =================================

template <> struct PrettyWriter<bool, void> {
    void operator() (std::ostream &os, const bool &b) {
        auto flags = os.setf(std::ios::boolalpha);
        os << b;
        os.setf(flags);
    }
};
template <> struct PrettyReader<bool, void> {
    bool operator() (std::istream &is, bool &b) {
        auto flags = is.setf(std::ios::boolalpha);
        is >> b;
        is.setf(flags);
        return bool(is);
    }
};

// Safe strings =================================

template <> struct PrettyWriter<std::string, void> {
    void operator() (std::ostream &os, const std::string &s) {
        os << "\"" << s << "\"";
    }
};
template <> struct PrettyReader<std::string, void> {
    bool operator() (std::istream &is, std::string &s) {
        bool ok = bool(std::getline(is, s));
        s = utils::unquote(s);
        return ok && bool(is);
    }
};

// Pretty sets ==================================

template <typename V, typename C>
struct PrettyWriter<std::set<V,C>> {
    void operator() (std::ostream &os, const std::set<V,C> &set) {
        PrettyWriter<V> printer;
        for (const V &v: set) {
            printer(os, v);
            os << " ";
        }
    }
};

template <typename V, typename C>
struct PrettyReader<std::set<V,C>> {
    bool operator() (std::istream &is, std::set<V,C> &set) {
        PrettyReader<V> reader;
        V value;

        while (reader(is, value))   set.insert(value);

        if (is.eof() && is.fail())  is.clear();
        return (bool)is;
    }
};


// Pretty maps ==================================

template <typename K, typename V>
struct PrettyWriter<std::map<K,V>> {
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
        os << "map(" << utils::className<K>() << ", " << utils::className<V>() << ")\n";
        for (auto &p: map) {
            std::ostringstream oss;
            keyPrinter(oss, p.first);
            std::string key = oss.str();
            os << "    " << std::string(maxWidth-key.length(), ' ') << key << ": ";
            PrettyWriter<V>()(os, p.second);
            os << "\n";
        }
    }
};

template <typename K, typename V>
struct PrettyReader<std::map<K,V>> {
    bool operator() (std::istream &is, std::map<K,V> &map) {
        static const std::regex regKeyValue = std::regex("[[:space:]]*([^[:space:]][^:]*): (.*)");
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

#endif // _PRETTY_STREAMERS_HPP_
