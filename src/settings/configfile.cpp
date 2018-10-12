#include <fstream>

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

// =================================================================================================
// Non-template config file base

const std::map<AbstractConfigFile::IConfigValue::Origin, std::string>
AbstractConfigFile::IConfigValue::_prefixes = [] {
  using Origin = AbstractConfigFile::IConfigValue::Origin;
  std::map<Origin, std::string> map {
    {     Origin::DEFAULT, "D" },
    {        Origin::FILE, "F" },
    { Origin::ENVIRONMENT, "E" },
    {       Origin::ERROR, "!" },
  };

  for (auto &p: map)
    p.second = std::string("[") + p.second + std::string("] ");

  return map;
}();

void AbstractConfigFile::write (const ConfigIterator &iterator,
                                const std::string& name, const stdfs::path& path,
                                std::ostream &os) {

  if (iterator.size() == 0) {
    os << "Empty configuration file: " << name << " (either voluntarily or it is unused by this executable)\n" << std::endl;
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
  if (prefixSize + maxNameWidth <= halfTitleSize) maxNameWidth = halfTitleSize - prefixSize + 1;

  const std::string titlePrefix = std::string(prefixSize + maxNameWidth - (title.length()-1)/2, '=');
  const std::string fullHeader = std::string(2 * titlePrefix.length() + title.length(), '=');

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

  /// Print values (and if a config file is found store it for later)
  ConfigIterator subconfigFiles;
  for (auto &p: iterator) {
    if (p.second.isConfigFile()) {
      if (toFile) {
        dynamic_cast<TSubconfigFile&>(p.second).printConfig(thisDir);

      } else
        subconfigFiles.insert(p);
    }

    if (!toFile)  os << p.second.prefix();
    os << std::string(maxNameWidth - p.first.length(), ' ')
       << p.first << ": " << p.second << "\n";
  }

  os << "\n" << fullHeader << std::endl;

  /// If not printing to file, subconfig files are printed afterwards
  for (auto &p: subconfigFiles) {
    os << "\n";
    dynamic_cast<TSubconfigFile&>(p.second).printConfig(os);
  }
}

enum State { START, HEADER, BODY, END };
bool AbstractConfigFile::read(ConfigIterator &it, const std::string &name, std::istream &is) {
  // Regular expressions for parsing file content
  std::regex regEmpty = std::regex("[[:space:]]*");
  std::regex regComment = std::regex("#.*");
  std::regex regSeparator = std::regex ("=+");
  std::regex regConfigFileName = std::regex("=+ ([[:alnum:]]+) =+");
  std::regex regDataRow = std::regex(" *([[:alnum:]_]+): ?(.+)");
  std::regex regMapField = std::regex("map\\([[:alnum:]_: ]+, [[:alnum:]_: ]+\\)");

  // Storage space for matches in regexp
  std::smatch matches;

  // Current line
  std::string line;

  // Start in initial state
  State state = START;

  // No problem (for now)
  bool ok = true;

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
          ok = false;
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
        if (std::regex_match (line, matches, regDataRow) && matches.size() == 3) {
          std::string field = matches[1];
          std::string value = matches[2];

          // Assuming it is a map: gobble everything without prefix
          if (std::regex_match(value, regMapField)) {
            value.clear();
            std::string buffer;
            while ((is.peek() != '[') && getline(is, buffer))
              value.append(buffer + "\n");
          }

          // Find ConfigValue with this name and make it parse the data
          auto fieldIt = it.find(field);
          if (fieldIt != it.end())
            ok &= fieldIt->second.input(value, IConfigValue::FILE);

          else // Abort if could not find
            ok = false;

        } else // Abort if current line is not a valid data field
          ok = false;
      }
      break;

      // We're happily exiting
    case END: break;
    }
  }

  // Notify
  return ok;
}


// =================================================================================================
// Non-template config value base

AbstractConfigFile::IConfigValue::IConfigValue (
    AbstractConfigFile::ConfigIterator &registrationDeck, const char *name)
  : _origin(DEFAULT), _name(name) {

  // Register into ConfigFile container
  registrationDeck.insert({_name, *this});
}

bool AbstractConfigFile::IConfigValue::input (const std::string &s, Origin o) {
  // Environment values have final say
  if (_origin != ENVIRONMENT) {

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


// =================================================================================================
// Template config file generic functions


} // end of namespace _details

} // end of namespace _config
