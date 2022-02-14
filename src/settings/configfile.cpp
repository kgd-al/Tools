#include <fstream>
#include <functional>

#include "configfile.h"

namespace config {

std::string verbosityValues (void) {
  using EU = EnumUtils<Verbosity>;
  std::ostringstream oss;
  oss << "Valid values are: ";
  for (Verbosity v: EU::iterator())   oss << EU::getName(v) << " ";
  return oss.str();
}

namespace _details {

// =============================================================================
// Non-template config file base

/// \cond internal
const std::map<AbstractConfigFile::IConfigValue::Origin, std::string>
AbstractConfigFile::IConfigValue::_prefixes = [] {
  using Origin = AbstractConfigFile::IConfigValue::Origin;
  std::map<Origin, std::string> map {
    {     Origin::DEFAULT, "D" },
    {        Origin::FILE, "F" },
    {        Origin::LOAD, "L" },
    { Origin::ENVIRONMENT, "E" },
    {    Origin::OVERRIDE, "O" },
    {    Origin::CONSTANT, "C" },
    {       Origin::ERROR, "!" },
  };

  for (auto &p: map)
    p.second = std::string("[") + p.second + std::string("] ");

  return map;
}();

void AbstractConfigFile::write (const ConfigIterator &iterator,
                                const std::string& name, const stdfs::path& path,
                                std::ostream &os) {

//  std::cerr << __PRETTY_FUNCTION__
//            << "\n\t> Writing " << name << " to " << path << "\n";

  if (iterator.size() == 0) {
    os << "Empty configuration file: " << name
       << " (either voluntarily or it is unused by this executable)\n"
       << std::endl;
    return;
  }

  // Find directory name
  stdfs::path thisDir;
  const bool toFile = (os.rdbuf() != std::cout.rdbuf());
  if (toFile) thisDir = path.parent_path();

  // Finding max field name width
  size_t maxNameWidth = 0;
  for (auto &p: iterator) {
    size_t l = p.first.length();
    if (l > maxNameWidth)
      maxNameWidth = l;
  }

  // Generate/print header
  size_t prefixSize = toFile ? 0 : (*iterator.begin()).second.prefix().length();
  const std::string title = std::string(" ") + name + std::string(" ");
  size_t halfTitleSize = (title.length()-1)/2;
  if (prefixSize + maxNameWidth <= halfTitleSize)
    maxNameWidth = halfTitleSize - prefixSize + 1;

  const std::string titlePrefix =
      std::string(prefixSize + maxNameWidth - (title.length()-1)/2, '=');

  const std::string fullHeader =
      std::string(2 * titlePrefix.length() + title.length(), '=');

  os << fullHeader << "\n"
     << titlePrefix << title << titlePrefix << "\n";

  if (!toFile) {
    os << std::string(maxNameWidth - 4, ' ') << "file: ";
    if (!path.empty())
      os << path.string();
    else    os << "*default*";
    os << "\n";
  }

  os << fullHeader << "\n\n";

  using SortedIterator = std::vector<std::reference_wrapper<IConfigValue>>;

  /// Print values (and if a config file is found store it for later)
  SortedIterator sortedIterator;
  for (auto &p: iterator) sortedIterator.push_back(std::ref(p.second));
  std::sort(sortedIterator.begin(), sortedIterator.end(),
            [] (const IConfigValue &lhs, const IConfigValue &rhs) {
    return lhs.index() < rhs.index();
  });

  SortedIterator subconfigFiles;
  for (IConfigValue &v: sortedIterator) {
    if (v.isConfigFile()) {
      if (toFile) {
        dynamic_cast<TSubconfigFile&>(v).printConfig(thisDir);

      } else
        subconfigFiles.push_back(v);
    }

    if (!toFile)  os << v.prefix();
    os << std::string(maxNameWidth - v.name().length(), ' ')
       << v.name() << ": " << v << "\n";
  }

  os << "\n" << fullHeader << std::endl;

  /// If not printing to file, subconfig files are printed afterwards
  for (IConfigValue &v: subconfigFiles) {
    os << "\n";
    dynamic_cast<TSubconfigFile&>(v).printConfig(os);
  }
}

bool debugField(const std::string &field) {
  return field.substr(0, 6) == "DEBUG_";
}

enum State { START, HEADER, BODY, END };
AbstractConfigFile::ReadResult
AbstractConfigFile::read(ConfigIterator &it,
                         const std::string &name,
                         std::istream &is,
                         const stdfs::path &path) {

//  std::cerr << __PRETTY_FUNCTION__
//            << "\n\t> Reading " << name << " from " << path << "\n";

  // Regular expressions for parsing file content
  std::regex regEmpty = std::regex("[[:space:]]*");
  std::regex regComment = std::regex("#.*");
  std::regex regSeparator = std::regex ("=+");
  std::regex regConfigFileName = std::regex("=+ (\\w+) =+");
  std::regex regDataRow = std::regex(" *(\\w+): ?(.+)");
  std::regex regMapField =
      std::regex("map\\([[:alnum:]_:<,> ]+, [[:alnum:]_:<> ]+\\) \\{");

  // Storage space for matches in regexp
  std::smatch matches;

  // Current line
  std::string line;

  // Start in initial state
  State state = START;

  // No problem (for now)
  ReadResult res = OK;

  std::set<std::string> expectedFields;
  for (const auto &f: it) expectedFields.insert(f.first);

  // Read another line
  while (std::getline(is, line) && state != END) {

    // Ignore empty lines and comments
    if (std::regex_match(line, regEmpty) || std::regex_match(line, regComment))
      continue;

    switch (state) {
    case START: // Looking for config file name
      if (std::regex_match (line, matches, regConfigFileName)) {
        if (matches.size() > 1 && matches[1] == name)
          state = HEADER;
        else {
          res |= CONFIG_FILE_TYPE_MISMATCH;
          throw std::invalid_argument(
                std::string("Wrong config file type. Expected '")
                + name + std::string("' got '") + matches[1].str() + "'"
              );
        }
      }
      break;

    case HEADER: // Looking for end of region line
      if (std::regex_match(line, regSeparator))
        state = BODY;
      break;

    case BODY: // Parsing values. Exiting when finding end of region line.
      if (std::regex_match(line, regSeparator)) {
        state = END;

      } else {
        if (std::regex_match (line, matches, regDataRow)
            && matches.size() == 3) {
          std::string field = matches[1];
          std::string value = matches[2];

          // Assuming it is a map: gobble everything without prefix
          if (std::regex_match(value, regMapField)) {
            value.clear();
            std::string buffer;
            while (getline(is, buffer) && buffer != "}")
              value.append(buffer + "\n");
          }

          // Find ConfigValue with this name and make it parse the data
          auto fieldIt = it.find(field);
          if (fieldIt != it.end()) {
            bool isConfigFile = fieldIt->second.isConfigFile();

            std::string value_ = value;
            if (isConfigFile && stdfs::path(value).parent_path().empty())
              value_ = path.parent_path() / value;

            bool ok = fieldIt->second.input(value_, IConfigValue::FILE);
            expectedFields.erase(fieldIt->first);

            if (!ok) {
              if (isConfigFile) {
                res |= SUBCONFIG_FILE_ERROR;
                std::cerr << "Subconfig file '" << field << "' of '" << name
                          << "' had errors" << std::endl;
              } else {
                res |= FIELD_PARSE_ERROR;
                std::cerr << "Error parsing field '" << field << " with value '"
                          << value << "' in config file " << name << std::endl;
              }
            }
          }

          else {  // Error if could not find
            if (!debugField(field)) {
              std::cerr << "Could not find field '" << field
                        << "' in config file " << name << std::endl;
              res |= FIELD_UNKNOWN_ERROR;
            }
          }

        } else {  // Error if current line is not a valid data field
          std::cerr << "Could not parse '" << line << "' in config file "
                    << name << std::endl;
          res |= LINE_INVALID_FORMAT;
        }
      }
      break;

      // We're happily exiting
    case END: break;
    }
  }

  // ignore debug values
  for (auto it = expectedFields.begin(); it != expectedFields.end(); ) {
    if (debugField(*it))
      it = expectedFields.erase(it);
    else
      ++it;
  }

  if (!expectedFields.empty()) {
    std::cerr << "Could not find a value for field(s):\n";
    for (const std::string &f: expectedFields)
      std::cerr << "\t'" << f << "'\n";
    res |= FIELD_MISSING_ERROR;
  }

  // Notify
  return res;
}

void AbstractConfigFile::serialize (const ConfigIterator &iterator,
                                    const stdfs::path &path,
                                    nlohmann::json &j) {
  j["path"] = path;
  for (const auto &p: iterator)
      p.second.toJson(j[p.first]);
}

void AbstractConfigFile::deserialize (ConfigIterator &iterator,
                                      stdfs::path &path,
                                      const nlohmann::json &j) {
  path = j["path"].get<stdfs::path>();
  for (auto &p: iterator) {
    auto it = j.find(p.first);
    if (it != j.end())
      p.second.fromJson(*it);
    else
      std::cerr << "Unable to find field " << p.first << " in config file "
                << path << std::endl;
  }
}


// =============================================================================
// Non-template config value base

AbstractConfigFile::IConfigValue::IConfigValue (
    const BuildData &bd, Origin origin)
  : _origin(origin), _name(bd.name), _index(bd.index) {

  // Register into ConfigFile container
  bd.registrationDeck.insert({_name, *this});
}

bool AbstractConfigFile::IConfigValue::input (const std::string &s, Origin o) {
  // Environmental and override values have final say
  if (_origin < o) {

    try {

      // Delegate reading
      if (performInput(s))
            _origin = o;
      else  _origin = ERROR;

    } catch (std::exception &e) {
      std::cerr
          << "Unable to convert '" << s
          << "' to '" << typeName()
          << "'. Error was: " << e.what() << std::endl;

      _origin = ERROR;
    }
  }

  return _origin != ERROR;
}

void AbstractConfigFile::IConfigValue::checkEnv (const char *name) {
  if (const char *env = getenv(name))
    input(utils::unquote(env), ENVIRONMENT);
}

/// \endcond

} // end of namespace _details

} // end of namespace _config
