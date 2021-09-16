#ifndef KGD_UTILS_CONFIG_FILE_H
#define KGD_UTILS_CONFIG_FILE_H

#include <regex>
#include <iostream>
#include <fstream>
#include <map>

/// TODO Remove experimental when gaining access to a recent enough version of g++

#ifdef EXPERIMENTAL_STD_FILESYSTEM
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#else
#include <filesystem>
namespace stdfs = std::filesystem;
#endif

#include "../external/json.hpp"

#include "../utils/utils.h"
#include "prettystreamers.hpp"
#include "prettyenums.hpp"


/*!
 * \file configfile.h
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
\endcode

    Notes: It is imperative that all types manipulated can be streamed in/out
        std::map are implemented only its template arguments need to be streamable
        Enumerations can be made to satisfy this requirement by using #DEFINE_PRETTY_ENUMERATION
*/

DEFINE_NAMESPACE_PRETTY_ENUMERATION(config, Verbosity, QUIET, SHOW, PARANOID)

namespace config {

//! \brief Returns a single string containing all valid verbosity values (for user information)
std::string verbosityValues (void);

// =============================================================================
// Parameter defining macros

/// @cond internal

/// Evaluates to the declaration of a static config value of requested \p TYPE and \p NAME
#define __DECLARE_PARAMETER_PRIVATE(TYPE, NAME) \
  static ConfigValue<TYPE> NAME;

/// Collects the required data for creation of a config value named \p NAME
#define __BUILD_DATA(NAME) \
  IConfigValue::BuildData { \
    config_iterator(), \
    #NAME, \
    nextIndex(), \
  }

/// Evaluates to the definition of a config value of requested \p TYPE and \p NAME
#define __DEFINE_PARAMETER_PRIVATE(CFILE, TYPE, NAME, ...)        \
  CFILE::ConfigValue<TYPE> CFILE::NAME (__BUILD_DATA(NAME), __VA_ARGS__);

/// @endcond

/// Hide CRTP from config file declaration
#define CONFIG_FILE(NAME) \
  NAME : public ConfigFile<NAME>

// ====================================
// Common parameters

/// Declares a config value with given \p TYPE and \p NAME
#define DECLARE_PARAMETER(TYPE, NAME) \
  __DECLARE_PARAMETER_PRIVATE(TYPE, NAME)

/// Defines a config value for configuration file \p CFILE with given \p TYPE
/// and \p NAME build from the variadic arguments
#define DEFINE_PARAMETER_FOR(CFILE, TYPE, NAME, ...) \
  __DEFINE_PARAMETER_PRIVATE(CFILE, TYPE, NAME, __VA_ARGS__)

/// Defines a config value with given \p TYPE and \p NAME build from the variadic arguments
#define DEFINE_PARAMETER(TYPE, NAME, ...) \
  DEFINE_PARAMETER_FOR(CFILE, TYPE, NAME, __VA_ARGS__)

/// Defines a config value for configuration file \p CFILE of container type
/// \p TYPE named \p NAME. Variadic arguments are used to instantiate on-the-fly
#define DEFINE_CONTAINER_PARAMETER_FOR(CFILE, TYPE, NAME, ...) \
  __DEFINE_PARAMETER_PRIVATE(CFILE, TYPE, NAME, TYPE(__VA_ARGS__))

/// Defines a config value of container (map, set, vector, ...) type \p TYPE
/// named \p NAME. Variadic arguments are used to instantiate on-the-fly
#define DEFINE_CONTAINER_PARAMETER(TYPE, NAME, ...) \
  DEFINE_PARAMETER_FOR(CFILE, TYPE, NAME, TYPE(__VA_ARGS__))

// ====================================
// Constant parameters

/// Declares a constant config value of type \p TYPE named \p NAME
#define DECLARE_CONST_PARAMETER(TYPE, NAME) \
  static ConstConfigValue<TYPE> NAME;

/// Defines a constant config value with given \p TYPE and \p NAME build from
/// the variadic arguments
#define DEFINE_CONST_PARAMETER(TYPE, NAME, ...)                         \
  CFILE::ConstConfigValue<TYPE> CFILE::NAME (__BUILD_DATA(NAME), __VA_ARGS__);


// ====================================
// Constexpr (debug) parameters

#ifndef NDEBUG
/// Declares a config value of type \p TYPE name \p NAME iff the NDEBUG macro is undefined
#define DECLARE_DEBUG_PARAMETER(TYPE, NAME, ...) \
  __DECLARE_PARAMETER_PRIVATE(TYPE, NAME)

/// Defines a config value of type \p TYPE name \p NAME iff the NDEBUG macro is undefined
#define DEFINE_DEBUG_PARAMETER(TYPE, NAME, ...) \
  DEFINE_PARAMETER_FOR(CFILE, TYPE, NAME, __VA_ARGS__)

#else

/// Declares a compile-constant config value of type \p TYPE named \p NAME iff NDEBUG macro is defined
#define DECLARE_DEBUG_PARAMETER(TYPE, NAME, ...) \
  static constexpr ConstexprConfigValue<TYPE> NAME { __VA_ARGS__ };

/// No-op if the NDEBUG macro is defined (variable is compile-constant and header only)
#define DEFINE_DEBUG_PARAMETER(TYPE, NAME, ...)

#endif

/// Declares the current config file depends on another named \p NAME of type \p TYPE
#define DECLARE_SUBCONFIG(TYPE, NAME) \
  static SubconfigFile<TYPE> NAME;

/// Defines the dependency for another config file named \p NAME of type \p TYPE
#define DEFINE_SUBCONFIG(TYPE, NAME) \
  CFILE::SubconfigFile<TYPE> CFILE::NAME (__BUILD_DATA(NAME));


// =============================================================================
// String convertible enums

/// \cond internal
namespace _details {

// =============================================================================
/// \brief Non-template config file base
struct AbstractConfigFile {
protected:
  struct IConfigValue;

  /// Iterable container for the fields declared through the macros DECLARE_*
  using ConfigIterator = std::map<std::string, IConfigValue&>;

  /// Base class for a generic, typeless, config value
  struct IConfigValue {

    /// Specify where the current value comes from
    enum Origin {
      DEFAULT = 0,      ///< Built-in value
      FILE = 1,         ///< Value read from a config file
      LOAD = 2,         ///< Value restored from a savefile
      ENVIRONMENT = 3,  ///< Value provided by the environment
      OVERRIDE = 4,     ///< Value overriden by code
      CONSTANT = 5,     ///< Constant built-in value (\see ConstConfigValue)
      ERROR = 10
    };

  private:

    /// Look-up-table for Origin to character conversion
    static const std::map<Origin, std::string> _prefixes;

    /// Cannot be copy constructed
    IConfigValue(const IConfigValue &) = delete;

    /// Cannot be move constructed
    IConfigValue(IConfigValue &&) = delete;

    /// Provides a common interface. See ConfigValue::performInput and TSubconfigFile::performInput.
    virtual bool performInput (const std::string &s) = 0;

  protected:
    /// Origin of the current value
    Origin _origin;

    /// Name of the config value (i-e field name in the parent config file)
    std::string _name;

    /// Index of the config value in its file
    uint _index;

    /// Optional comment
    std::string _comment;

    /// If the environment contains a value from this field, use it
    void checkEnv (const char *name);

  public:
    /// Contains the necessary information for registering a config value
    struct BuildData {
      /// The iterator in which to store the encapsulating data
      ConfigIterator &registrationDeck;

      /// The name of the config value
      const char *name;

      /// The position of the config value in its file
      uint index;
    };

    /// Build a config value named \p name and register it
    IConfigValue (const BuildData &bd, Origin origin = DEFAULT);

    /// Provides a common interface. See ConfigValue::output and TSubconfigFile::output.
    virtual std::ostream& output (std::ostream &os) const = 0;

    /// Delegates work to the overriden performInput unless the current source has higher precedance
    bool input (const std::string &s, Origin o);

    /// Provides a common interface.
    /// See ConfigValue::toJson and TSubconfigFile::toJson.
    virtual void toJson (nlohmann::json &j) const = 0;

    /// Provides a common interface.
    /// See ConfigValue::fromJson and TSubconfigFile::fromJson.
    virtual void fromJson (const nlohmann::json &j) = 0;

    /// \returns the name (key) of this config value
    const std::string& name (void) const {
      return _name;
    }

    /// \returns the position of this config value in its corresponding file
    uint index (void) const {
      return _index;
    }

    /// For discrimination between value-based and subconfig-based fields
    virtual bool isConfigFile (void) const {
      return false;
    }

    /// Provides a common interface. See ConfigValue::typeName and TSubconfigFile::typeName.
    virtual const std::string& typeName (void) const = 0;

    /// Returns a single-character string denoted where the value was obtained from
    const std::string& prefix (void) const{
      return _prefixes.at(_origin);
    }
  };

  /// Instantiable config value of type \p T
  /// \tparam T the value contained by this item (e.g. int, float, double, map<...>, ...)
  template <typename T>
  struct ConfigValue : public IConfigValue {

    /// Build a config value named \p name from arguments \p args and register it
    template <typename... ARGS>
    ConfigValue(const BuildData &bd, ARGS&&... args)
      : IConfigValue(bd), _value(std::forward<ARGS>(args)...) {
      checkEnv(bd.name); // Need to have it here for dynamic dispatch
    }

    /// Return a const reference to the underlying value
    const T& operator() (void) const {
      return _value;
    }

    /// Returns a mutable referance to the underlying value. Use with caution!
    T& ref (void) {
      return _value;
    }

    /// Replaces current value with provided one
    T overrideWith (T other) {
      if (_origin < OVERRIDE) {
        _origin = OVERRIDE;
        T tmp = _value;
        _value = other;
        return tmp;
      }
      return _value;
    }

    /// Delegate writting this item to the appropriate PrettyWriter
    std::ostream& output(std::ostream &os) const override {
      PrettyWriter<T>()(os, _value);
      return os;
    }

    /// Stores value into the provided json
    void toJson (nlohmann::json &j) const override {
      j = _value;
    }

    /// Restores value from the provided json
    void fromJson (const nlohmann::json &j) override {
      if (_origin < LOAD) {
        _origin = LOAD;
        _value = j.get<T>();
      }
    }

  private:
    /// The stored value
    T _value;

    /// Run-time knowledge of the stored type
    const std::string& typeName (void) const override {
      static const std::string _localTypeName = utils::className<T>();
      return _localTypeName;
    }

    /// Delegate parsing the input string \p s to the appropriate PrettyReader
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

  /// Placeholder for a constant wrapper for values of type \p T
  /// \tparam T the value contained by this item
  ///  (e.g. int, float, double, map<...>, ...)
  template <typename T>
  struct ConstConfigValue : public IConfigValue {

    /// Builds a compile-constant value for type T
    template <typename... ARGS>
    ConstConfigValue (const BuildData &bd, ARGS&&... args)
      : IConfigValue(bd, CONSTANT), _value(std::forward<ARGS>(args)...) {}

    /// Access the compile-constant value stored by this item
    const T& operator() (void) const {  return _value;  }

    /// Delegate writting this item to the appropriate PrettyWriter
    std::ostream& output(std::ostream &os) const override {
      PrettyWriter<T>()(os, _value);
      return os;
    }

    /// Stores value into the provided json
    void toJson (nlohmann::json &j) const override {
      j = _value;
    }

    /// Restores value from the provided json
    void fromJson (const nlohmann::json &j) override {
      _value = j.get<T>();
    }

  private:
    T _value; ///< The compile-constant value

    /// Run-time knowledge of the stored type
    const std::string& typeName (void) const override {
      static const std::string _localTypeName = utils::className<T>();
      return _localTypeName;
    }

    /// Does nothing
    bool performInput (const std::string &) override { return true; }
  };

  /// Placeholder for a compile-constant wrapper for values of type \p T
  /// \tparam T the value contained by this item
  /// (e.g. int, float, double, map<...>, ...)
  template <typename T>
  struct ConstexprConfigValue {

    /// Builds a compile-constant value for type T
    template <typename... ARGS>
    constexpr ConstexprConfigValue (ARGS&&... args)
      : _value(std::forward<ARGS>(args)...) {}

    /// Access the compile-constant value stored by this item
    constexpr const T& operator() (void) const {  return _value;  }

  private:
    T _value; ///< The compile-constant value
  };

  /// Base class for a ConfigValue holding a reference to a child configuration file
  struct TSubconfigFile : public IConfigValue {

    /// Build and register a config file named \p name
    TSubconfigFile (const BuildData &bd) : IConfigValue(bd) {}

    virtual ~TSubconfigFile(void) = default;

    /// Is indeed a config file
    bool isConfigFile(void) const override {
      return true;
    }

    /// Requests recursively printing out this config file to the disk file at \p path
    virtual void printConfig(const std::string &path) = 0;

    /// Requests recursively printing out this config file to stream \p os
    virtual void printConfig(std::ostream &os) = 0;
  };

  /// Instantiable config value for a reference to child configuration file of type \p C
  /// \tparam C the child config file type
  template <typename C>
  struct SubconfigFile : public TSubconfigFile {

    /// Build and register this as a reference to subconfig file of type \p C
    SubconfigFile(const BuildData &bd) : TSubconfigFile(bd) {
      checkEnv(bd.name); // Need to have it here for dynamic dispatch
    }

  private:
    const std::string& typeName (void) const override {
      return C::name();
    }

    /// Prints out the current filename for this config file
    std::ostream& output (std::ostream &os) const override {
      return os << C::_path.filename().string();
    }

    /// Delegate printing out this wrapper to the underlying configuration file
    void printConfig(const std::string &path) override {
      C::printConfig(path);
    }

    /// Delegate printing out this wrapper to the underlying configuration file
    void printConfig(std::ostream &os) override {
      C::printConfig(os);
    }

    /// Delegate reading into this wrapper to the underlying configuration file
    bool performInput (const std::string &s) override {
      return C::readConfig(s);
    }

    void toJson (nlohmann::json &j) const override {
      C::serialize(j);
    }

    void fromJson (const nlohmann::json &j) override {
      _origin = LOAD;
      C::deserialize(j);
    }
  };

  /// Delegate printing out generic config value to their appropriate overloaded output function
  friend std::ostream& operator<< (std::ostream &os, const IConfigValue &value) {
    return value.output(os);
  }

  /// Result of reading a config file from an input stream
  enum ReadResult {
    OK = 0, ///< No problem

    /// [FATAL] Reading from a different config file
    CONFIG_FILE_TYPE_MISMATCH = 1,

    /// [ERROR] Invalid input line
    LINE_INVALID_FORMAT = 2,

    /// [ERROR] Provided field was not recognized
    FIELD_UNKNOWN_ERROR = 4,

    /// [ERROR] Field parsing was unsuccessful
    FIELD_PARSE_ERROR = 8,

    /// [ERROR] Subconfig file got errors
    SUBCONFIG_FILE_ERROR = 16,

    /// [WARNING] Could not find a value for a field
    FIELD_MISSING_ERROR = 32
  };

  /// Read results can be combined
  inline friend ReadResult operator| (ReadResult lhs, ReadResult rhs) {
    using RR_t = std::underlying_type<ReadResult>::type;
    return static_cast<ReadResult>(
      static_cast<RR_t>(lhs) | static_cast<RR_t>(rhs)
    );
  }

  /// Read results can be combined-assigned
  inline friend ReadResult& operator|= (ReadResult &flags, ReadResult flag) {
    return flags = flags | flag;
  }

  /// Read results can be bitwise observed
  inline friend ReadResult operator& (ReadResult lhs, ReadResult rhs) {
    using RR_t = std::underlying_type<ReadResult>::type;
    return static_cast<ReadResult>(
      static_cast<RR_t>(lhs) & static_cast<RR_t>(rhs)
    );
  }

public:
  /// Write config values for \p name as described in \p it to the stream \p os.
  /// If \p os is a filestream \p filename then contains the corresponding path
  static void write (const ConfigIterator &it, const std::string &name, const stdfs::path &path,
                     std::ostream &os);

  /// Read config values for \p name into \p it from the provided stream \p is
  static ReadResult read (ConfigIterator &it,
                          const std::string &name,
                          std::istream &is,
                          const stdfs::path &path);

  /// Records this parameters and subconfig files into the provided json
  static void serialize (const ConfigIterator &iterator,
                         const stdfs::path &path,
                         nlohmann::json &j);

  /// Restores this parameters and subconfig files from the provided json
  static void deserialize (ConfigIterator &iterator,
                           stdfs::path &path,
                           const nlohmann::json &j);
};

} // end of namespace _details
/// \endcond

// =============================================================================
// Config file template implementation

/// The user-exposed interface to a configuration file
///
template <class F>
class ConfigFile : public _details::AbstractConfigFile {
  friend struct SubconfigFile<F>;
protected:

  /// Default folder for storing configuration files
  static constexpr const char *FOLDER = "configs/";

  /// Default extension for configuration files
  static constexpr const char *EXT = ".config";

  /// Current filename for this ConfigFile
  static stdfs::path _path;

  /// \returns next index value
  static uint nextIndex (void) {
    static uint index = 0;
    return index++;
  }

  /// Provides access to the underlying container
  static ConfigIterator& config_iterator (void) {
    static ConfigIterator localIterator;
    return localIterator;
  }

  /// Returns the preferred filename for this configuration file
  static stdfs::path defaultFilename (void) {
    return stdfs::path (name() + EXT);
  }

  /// Returns the preferred location for this configuration file
  static stdfs::path defaultPath (void) {
    return stdfs::path (".") / FOLDER / defaultFilename();
  }

public:
  /// \returns the current path (if any) of the file whose values are used
  static const stdfs::path path (void) {
    return _path;
  }

  /// Return the name of this ConfigFile's actual type (the template argument)
  static const std::string& name (void) {
    static const std::string _localName = utils::innermostTemplateArgument<F>();
    return _localName;
  }

  /// If a filename is provided config values are read from it, otherwise built-in values are used.
  /// If filename = "auto" the default location is used \see defaultPath
  /// Subconfig file are searched in the same folder as their parent
  /// If the verbosity value \p v is greater than quiet the resulting configuration is displayed.
  /// If the verbosity is PARANOID a confirmation will be required
  static void setupConfig(std::string inFilename = "", Verbosity v = Verbosity::QUIET) {
    if (inFilename == "auto")
      inFilename = defaultPath().string();

    if (inFilename.size() > 0)  readConfig(inFilename);

    if (v >= Verbosity::SHOW)
      printConfig();

    if (v >= Verbosity::PARANOID) {
      std::cout << "Please take some time to review the configuration values and press any"
                   "key when you are certain.";
      std::cin.get();
    }
  }

  /// Prints this configuration file (and its children) to the requested location.
  /// If \p path is empty, the default path is used @see defaultPath
  /// If only a directory is provided, the default filename is used @see defaultFilename
  /// Parent directories are created as needed
  ///
  /// \return Whether writing was performed
  static bool printConfig(stdfs::path path,
                          const std::string &header = "Writing") {
    if (path.empty()) _path = defaultPath();

    else if (path.extension() != EXT)
      _path = path / defaultFilename();

    if (stdfs::exists(_path)) {
      std::cerr << "Output path " << _path << " already exists. Overwrite? y/n "
                << std::flush;
      if (std::cin.get() != 'y')  return false;
      std::cerr << std::endl;
    }

    stdfs::create_directories(_path.parent_path());
    std::ofstream ofs (_path);
    if (!ofs.is_open()) {
      std::cerr << "Failed to open " << _path << " to write " << name() << std::endl;
      return false;
    }

    std::cout << header << " " << _path << std::endl;
    printConfig(ofs);
    return true;
  }

  /// Prints this configuration to the provided stream
  static void printConfig (std::ostream &os = std::cout) {
    write(config_iterator(), name(), _path, os);
  }

  /// Load configuration data stored at path
  static bool readConfig (const stdfs::path &path) {
    std::ifstream ifs (path);
    ReadResult res = read(config_iterator(), name(), ifs, path);
    _path = path;

    if (!stdfs::exists(path))
      printConfig("", "Writing default config to");

    else if ((res & FIELD_MISSING_ERROR) != OK)
      printConfig(path, "Updating");

    return res == OK;
  }

  /// Delegate serialization to the abstract version
  static void serialize (nlohmann::json &j) {
    _details::AbstractConfigFile::serialize(config_iterator(), _path, j);
  }

  /// Delegate deserialization to the abstract version
  static void deserialize (const nlohmann::json &j) {
    _details::AbstractConfigFile::deserialize(config_iterator(), _path, j);
  }

  /// Access a configuration value by its name
  static IConfigValue& configValue (const std::string &name) {
    auto it = config_iterator().find(name);
    if (it == config_iterator().end())
      utils::doThrow<std::invalid_argument>(
        "Unable to find configuration value '", name, "' in ",
        ConfigFile::name());

    return it->second;
  }
};

template <typename T> stdfs::path ConfigFile<T>::_path;

} // end of namespace config

#endif // KGD_UTILS_CONFIG_FILE_H
