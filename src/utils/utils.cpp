#include <cmath>
#include <fstream>
#include <iomanip>

#include "utils.h"

// global namespace

namespace utils {

double __toDegRatio = 180. / M_PI;

// =================================================================================================
    // Helper functions

bool reset (std::stringstream &ss) {
  ss.str("");
  ss.clear();
  return ss.good();
}

std::string trimLeading (std::string str, const std::string &whitespaces) {
    size_t i = str.find_first_not_of(whitespaces);
    str.erase(0, i);

    i = str.find_last_not_of(whitespaces);
    if (std::string::npos != i)
        str.erase(i+1);
    return str;
}

std::string trim (std::string str) {
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    return str;
}

std::string unquote (std::string str) {
    if (str.front() == '"' && str.back() == '"') { // Remove quotes from value
        str.erase( 0, 1 ); // erase the first character
        str.erase( str.size() - 1 ); // erase the last character
    }
    return str;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
        elems.push_back(item);
    return elems;
}

std::string readAll (const std::string &filename) {
    std::ifstream ifs (filename);
    if (!ifs) throw std::invalid_argument(std::string("Unable to open file ") + filename + " for reading");
    return readAll (ifs);
}

std::string readAll (std::ifstream &ifs) {
    std::stringstream ss;
    if (!ifs)   throw std::invalid_argument("Provided file stream is in an invalid state");
    ss << ifs.rdbuf();
    return ss.str();
}

std::ostream& operator<< (std::ostream &os, const CurrentTime &ct) {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);

    auto locale = os.getloc();
    os.imbue(std::locale("C"));
    os << std::put_time(&tm, ct.format.c_str());
    os.imbue(locale);
    return os;
}

} // end of namespace utils
