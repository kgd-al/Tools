#ifndef _CONFIG_FILE_H_
#define _CONFIG_FILE_H_

#include <regex>
#include <iostream>
#include <fstream>

#include "../utils/utils.h"
#include "prettystreamers.hpp"
#include "prettyenums.hpp"


/*!
 * \file
 * Contains the facilities for producing self-aware hierarchical configuration files
 *
 * Canonical example:

\code{cpp}
// .h
#define CFILE MyConfigFile
struct CFILE : public ConfigFile<FILE> {
    DECLARE_PARAMETER(uint, intField1)
    DECLARE_PARAMETER(uint, intField2)
    DECLARE_PARAMETER(std::string, stringField)
    DECLARE_SUBCONFIG(MyOtherConfigFile, otherStuff)
};
#undef CFILE

// .cpp
#define CFILE MyConfigFile
DEFINE_PARAMETER(uint, intField1, 11)
DEFINE_PARAMETER(uint, intField2, 12)
DEFINE_PARAMETER(std::string, stringField, "string value")
DEFINE_SUBCONFIG(MyOtherConfigFile, otherStuff)
#undef CFILE
\end{code}

    Notes: It is imperative that all types manipulated can be streamed in/out
        std::map are implemented only its template arguments need to be streamable
        Enumerations can be made to satisfy this requirement by using #DEFINE_PRETTY_ENUMERATION
*/

DEFINE_PRETTY_ENUMERATION(Verbosity, QUIET, SHOW, PARANOID)
std::string verbosityValues (void);

// =================================================================================================
    // Parameter defining macros

#define DECLARE_PARAMETER(TYPE, NAME) static ConfigValue<TYPE> NAME;
#define __DEFINE_PARAMETER_PRIVATE(TYPE, NAME, ...) \
    CFILE::ConfigValue<TYPE> CFILE::NAME (config_iterator(), #NAME, __VA_ARGS__);

#define DEFINE_PARAMETER(TYPE, NAME, ...) __DEFINE_PARAMETER_PRIVATE(TYPE, NAME, __VA_ARGS__)
#define DEFINE_MAP_PARAMETER(TYPE, NAME, ...) __DEFINE_PARAMETER_PRIVATE(TYPE, NAME, TYPE(__VA_ARGS__))

#define DECLARE_SUBCONFIG(TYPE, NAME) static SubconfigFile<TYPE> NAME;
#define DEFINE_SUBCONFIG(TYPE, NAME) \
    CFILE::SubconfigFile<TYPE> CFILE::NAME (config_iterator(), #NAME);


// =================================================================================================
    // String convertible enums

namespace _details {

// =================================================================================================
    // Non-template config file base

struct AbstractConfigFile {
protected:
    struct IConfigValue;
    using ConfigIterator = std::map<std::string, IConfigValue&>;

    struct IConfigValue {
        enum Origin { DEFAULT, FILE, ENVIRONMENT, ERROR };
    private:
        static const std::map<Origin, std::string> _prefixes;
        Origin _origin;

        std::string _name;

        IConfigValue(const IConfigValue &) = delete;
        IConfigValue(IConfigValue &&) = delete;

        virtual bool performInput (const std::string &s) = 0;

    protected:
        void checkEnv (const char *name);

    public:
        IConfigValue (ConfigIterator &registrationDeck, const char *name);

        virtual std::ostream& output (std::ostream &os) const = 0;

        bool input (const std::string &s, Origin o);

        virtual bool isConfigFile (void) const {
            return false;
        }

        virtual const std::string& typeName (void) const = 0;
        const std::string& prefix (void) const{
            return _prefixes.at(_origin);
        }
    };

    template <typename T>
    struct ConfigValue : public IConfigValue {

        template <typename... ARGS>
        ConfigValue(ConfigIterator &registrationDeck, const char *name, ARGS&&... args)
            : IConfigValue(registrationDeck, name), _value(std::forward<ARGS>(args)...) {
            checkEnv(name); // Need to have it here for dynamic dispatch
        }

        const T& operator() (void) const {
            return _value;
        }

        T& ref (void) {
            return _value;
        }

        std::ostream& output(std::ostream &os) const override {
            PrettyWriter<T>()(os, _value);
            return os;
        }

    private:
        T _value;

        const std::string& typeName (void) const {
            static const std::string _localTypeName = utils::className<T>();
            return _localTypeName;
        }

        bool performInput (const std::string &s) override {
            std::istringstream iss (s);
            T tmp (_value);

            if (PrettyReader<T>()(iss, tmp)) {
                _value = tmp;
                return true;

            } else
                return false;
        }

    };

    struct TSubconfigFile : public IConfigValue {
        TSubconfigFile (ConfigIterator &registrationDeck, const char *name)
            : IConfigValue(registrationDeck, name) {}

        bool isConfigFile(void) const {
            return true;
        }

        virtual void printConfig(const std::string &path) = 0;
        virtual void printConfig(std::ostream &os) = 0;
    };

    template <typename C>
    struct SubconfigFile : public TSubconfigFile {
        SubconfigFile(ConfigIterator &registrationDeck, const char *name)
            : TSubconfigFile(registrationDeck, name) {
            checkEnv(name); // Need to have it here for dynamic dispatch
        }

    private:
        const std::string& typeName (void) const {
            return C::name();
        }

        std::ostream& output (std::ostream &os) const override {
            return os << C::_filename;
        }

        void printConfig(const std::string &path) { C::printConfig(path);   }
        void printConfig(std::ostream &os) {        C::printConfig(os);     }

        bool performInput (const std::string &s) override {
            return C::readConfig(s);
        }
    };

    friend std::ostream& operator<< (std::ostream &os, const IConfigValue &value) {
        return value.output(os);
    }

public:
    static void write (const ConfigIterator &it, const std::string &name, const std::string &filename,
                                 std::ostream &os);

    static bool read (ConfigIterator &it, const std::string &name, std::istream &is);
};

} // end of namespace _details

// =================================================================================================
    // Config file template implementation

template <class F>
class ConfigFile : public _details::AbstractConfigFile {
    friend class SubconfigFile<F>;
protected:
    static constexpr const char *FOLDER = "configs/";
    static constexpr const char *EXT = ".config";
    static std::string _filename;

    static ConfigIterator& config_iterator (void) {
        static ConfigIterator localIterator;
        return localIterator;
    }

public:
    static const std::string& name (void) {
        static const std::string _localName = utils::unscopedClassName<F>();
        return _localName;
    }

    static void setupConfig(const std::string &inFilename = "", Verbosity v = Verbosity::QUIET) {
        if (inFilename.size() > 0)  readConfig(inFilename);

        if (v >= Verbosity::SHOW)
            printConfig();

        if (v >= Verbosity::PARANOID) {
            std::cout << "Please take some time to review the configuration values and press any"
                         "key when you are certain.";
            std::cin.get();
        }
    }

    static bool printConfig(std::string fullpath) {
        if (fullpath.size() == 0)
            fullpath = FOLDER + name() + EXT;

        else if (fullpath.back() == '/')
            fullpath += name() + EXT;

        _filename = fullpath;

        (void)system((std::string("mkdir -p ") + _filename).c_str());
        std::ofstream ofs (_filename);
        if (!ofs) {
          std::cerr << "Failed to open " << _filename << " to write " << name() << std::endl;
          return false;
        }

        std::cout << "Writing " << name() << " and subconfig files at " << _filename << std::endl;
        printConfig(ofs);
        return true;
    }

    static void printConfig (std::ostream &os = std::cout) {
        write(config_iterator(), name(), _filename, os);
    }

    static bool readConfig (const std::string &filename) {
        std::ifstream ifs (filename);
        bool ok = read(config_iterator(), name(), ifs);
        _filename = filename;
        return ok;
    }

    static IConfigValue& configValue (const std::string &name) {
        auto it = config_iterator().find(name);
        if (it == config_iterator().end()) {
            std::ostringstream oss;
            oss << "Unable to find configuration value '"
                << name << "' in " << ConfigFile::name() << std::endl;
//            LOG(FATAL, INT_MAX) << oss.str().c_str();
            throw std::invalid_argument(oss.str());

        } else return it->second;
    }
};

template <typename T> std::string ConfigFile<T>::_filename;

#endif // _CONFIG_FILE_H_
