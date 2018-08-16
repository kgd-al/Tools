#include <fstream>

#include "configfile.h"

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
                                const std::string& name, const std::string& filename,
                                std::ostream &os) {

    if (iterator.size() == 0) {
        os << "Empty configuration file" << std::endl;
        return;
    }

    // Find directory name
    std::string thisDir;
    const bool toFile = (os.rdbuf() != std::cout.rdbuf());
    if (toFile) {
        const size_t pos = filename.find_last_of('/');
        if (pos != std::string::npos)   thisDir = filename.substr(0, pos+1);
    }

    // Finding max field name width
    uint maxNameWidth = 0;
    for (auto &p: iterator) {
        uint l = p.first.length();
        if (l > maxNameWidth)
            maxNameWidth = l;
    }


    // Generate/print header
    uint prefixSize = (*iterator.begin()).second.prefix().length();
    const std::string title = std::string(" ") + name + std::string(" ");
    uint halfTitleSize = (title.length()-1)/2;
    if (prefixSize + maxNameWidth <= halfTitleSize) maxNameWidth = halfTitleSize - prefixSize + 1;

    const std::string titlePrefix = std::string(prefixSize + maxNameWidth - (title.length()-1)/2, '=');
    const std::string fullHeader = std::string(2 * titlePrefix.length() + title.length(), '=');

    os << fullHeader << "\n"
       << titlePrefix << title << titlePrefix << "\n";

    if (!toFile) {
        os << std::string(maxNameWidth - 4, ' ') << "file: ";
        if (filename.length() > 0)
                os << filename;
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

        os << p.second.prefix()
           << std::string(maxNameWidth - p.first.length(), ' ')
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
    static const int D = 1;
    static const std::string debugHeader = std::string(50, '-');

    std::regex regEmpty = std::regex("[[:space:]]*");
    std::regex regComment = std::regex("#.*");
    std::regex regSeparator = std::regex ("=+");
    std::regex regConfigFileName = std::regex("=+ ([[:alnum:]]+) =+");
    std::regex regDataRow = std::regex("\\[.\\] +([[:alnum:]_]+): ?(.+)");
    std::regex regMapField = std::regex("map\\([[:alnum:]_: ]+, [[:alnum:]_: ]+\\)");

    std::smatch matches;

    std::string line;
    State state = START;

    bool ok = true;

    while (std::getline(is, line) && state != END) {
//        LOG(DEBUG, D) << line << std::endl;

        if (std::regex_match(line, regEmpty) || std::regex_match(line, regComment)) {
//            LOG(DEBUG, D) << debugHeader << "Ignoring empty/comment line" << std::endl;
            continue;
        }

        switch (state) {
        case START:
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
//                LOG(DEBUG, D) << debugHeader << "config file name = " << matches[1] << std::endl;
            }
        break;

        case HEADER:
            if (std::regex_match(line, regSeparator)) {
//                LOG(DEBUG, D) << debugHeader << "Leaving header" << std::endl;
                if (state != HEADER) throw std::invalid_argument("Missing config file type");
                state = BODY;
            }
        break;

        case BODY:
            if (std::regex_match(line, regSeparator)) {
//                LOG(DEBUG, D) << debugHeader << "Reached end of file" << std::endl;
                state = END;

            } else {
                if (std::regex_match (line, matches, regDataRow) && matches.size() == 3) {
                    std::string field = matches[1];
                    std::string value = matches[2];

                    // Assuming it is a map: gobble everything without prefix
                    if (std::regex_match(value, regMapField)) {
//                        LOG(DEBUG, D) << debugHeader << "assuming I got a map" << std::endl;
                        value.clear();
                        std::string buffer;
                        while ((is.peek() != '[')
                               && getline(is, buffer))
                            value.append(buffer + "\n");
                    }

//                    LOG(DEBUG, D) << debugHeader << "field: " << field << std::endl;
//                    LOG(DEBUG, D) << debugHeader << "value: '" << value << "'" << std::endl;

                    auto fieldIt = it.find(field);
                    if (fieldIt != it.end())
                        ok &= fieldIt->second.input(value, IConfigValue::FILE);

                    else {
                        ok = false;
//                        std::cerr << "Could not find field '" << field << "'" << std::endl;
                    }

                } else {
                    ok = false;
//                    std::cerr << "Ignoring invalid input line '" << line << "'" << std::endl;
                }
            }
        break;

        case END: break;
        }
    }

    return ok;
}


// =================================================================================================
    // Non-template config value base

AbstractConfigFile::IConfigValue::IConfigValue (
        AbstractConfigFile::ConfigIterator &registrationDeck, const char *name)
        : _origin(DEFAULT), _name(name) {

    registrationDeck.insert({_name, *this});
}

bool AbstractConfigFile::IConfigValue::input (const std::string &s, Origin o) {
    if (_origin != ENVIRONMENT) {

        try {

            if (performInput(s))
                    _origin = o;
            else    _origin = ERROR;

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
