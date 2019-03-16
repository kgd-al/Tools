#ifndef _SELF_AWARE_GENOME_H_
#define _SELF_AWARE_GENOME_H_

#include <ostream>
#include <iomanip>
#include <string>
#include <map>

#include <type_traits>
#include <functional>

#include <atomic>

#include "../utils/utils.h"
#include "../external/json.hpp"
#include "../settings/configfile.h"
#include "../settings/mutationbounds.hpp"
#include "../random/dice.hpp"

/*!
 * \file selfawaregenome.hpp
 *
 * \brief Contains definitions and utilities for effortless implementation of basic
 * genome functionnality (random generation, mutation, crossing)
 *
 * Most of this file is not intended for the end-user.
 */

namespace config {

/// Store global variables for all self-aware genome configuration files
struct EDNAConfigFile_common {

  /// Sets whether or not to automatically log mutations and returns the
  /// previous value
  static bool autologMutations (bool b) {
    return autologMutations(b ? TRUE : FALSE);
  }

  /// Returns whether mutations are automatically logged
  static bool autologMutations(void) {
    return autologMutations(UNCHANGED);
  }

private:
  /// \cond internal

  /// Helper enumeration for a tri-state boolean
  enum State { TRUE, FALSE, UNCHANGED };

  /// Stores the autolog value as a singleton and manages access/updates
  static bool autologMutations (State s) {
    static std::atomic<bool> alm = [] {
      bool b = false;
      utils::getEnv<bool>("autologMutations", b);
      return b;
    }();
    bool prev = alm;
    switch (s) {
    case TRUE:  alm = true;   return prev;
    case FALSE: alm = false;  return prev;
    case UNCHANGED:           return alm;
    }
    return false;
  }
  /// \endcond
};

namespace _details {

/// Provides common types used for definining mutation bounds and rates
template <typename G>
struct EDNAConfigFileTypes {
  /// Helper alias for the relevant bounds abstraction type
  template <typename T>
  using Bounds = MutationSettings::Bounds<T, G>;

  /// Lazy-ass helper alias for the relevant bounds abstraction type
  template <typename T>
  using B = Bounds<T>;

  /// Helper alias to the mutation rates type
  using MutationRates = MutationSettings::MutationRates;

  /// Lazy-ass helper alias to the mutation rates type
  using MR = MutationRates;

  /// Helper alias to the distance weights type
  using DistanceWeights = std::map<std::string, float>;

  /// Lazy-ass helper alias to the distance weights type
  using DW = DistanceWeights;
};

} // end of namespace _details

template <typename G> struct EDNAConfigFile;
} // end of namespace config

namespace genotype {

// =============================================================================
// == Field extraction

/// Base template for a field extractor
template <typename T, typename ENABLE = void>
struct Extractor {
  static_assert(!std::is_same<T,T>::value,
                "No extractor defined for this type!");

  /// Extract contents of \p according to the path in \p field
  std::string operator() (const T &value, const std::string &field) const;
};


/// Extracts fundamental values by returning directly
template <typename T>
struct Extractor<T, typename std::enable_if<std::is_fundamental<T>::value
                                          || std::is_enum<T>::value, void>::type> {
  /// TODO Not documented
  std::string operator() (const T &value, const std::string &field) const {
    if (!field.empty())
      utils::doThrow<std::invalid_argument>(
        "No subfield in fundamental type ", utils::className<T>());

    std::ostringstream oss;
    oss << value;
    return oss.str();
  }
};

/// Produces an aggregated array of aggregated underlying values
template <typename T>
struct Extractor<T, typename std::enable_if<utils::is_cpp_array<T>::value
                                        || utils::is_std_vector<T>::value, void>::type> {
  /// TODO Not documented
  std::string operator() (const T &value, const std::string &field) const {
    if (field[0] == '[') {
      size_t index;
      char c;
      std::string subfield;
      std::stringstream ss (field);
      ss >> c >> index >> c >> subfield;

      if (value.size() <= index)
        utils::doThrow<std::invalid_argument>(
          "Out-of-bounds: index ", index, " is greater than size ", value.size());

      return Extractor<typename T::value_type>()(value[index], subfield);

    } else {
      using utils::operator<<;
      std::ostringstream oss;
      oss << value;
      return oss.str();
    }
  }
};


// =============================================================================
// == Field aggregation

/// Base template for a field aggregator
template <typename T, typename O, typename ENABLE = void>
struct Aggregator {
  static_assert(!std::is_same<T,T>::value,
                "No aggregator defined for this type!");

  /// Aggregates values accessible through \p access in the object array
  /// \p objects into output stream \p os with verbosity level of \p verbosity
  void operator() (std::ostream &os, const std::vector<O> &objects,
                   std::function<const T& (const O&)> access,
                   uint verbosity) const;
};

/// Aggregates fundamental values by producing a range of [min,...,max] values
template <typename T, typename O>
struct Aggregator<T, O, typename std::enable_if<std::is_fundamental<T>::value
                                             || std::is_enum<T>::value, void>::type> {
  /// TODO Not documented
  void operator() (std::ostream &os, const std::vector<O> &objects,
                   std::function<const T& (const O&)> access,
                   uint verbosity) const {
    std::vector<T> values;
    for (const O &object: objects)
      values.push_back(access(object));
    std::sort(values.begin(), values.end());

    // Can't print more values than we've got
    verbosity = std::min(verbosity, uint(values.size() - 2));
    os << "[";
    for (uint i=0; i<=verbosity+1; i++) {
      uint j = i * (values.size() - 1) / float(verbosity+1);
      os << " " << values[j];
    }
    os << " ]";
  }
};

/// Produces an aggregated array of aggregated underlying values
template <typename T, typename O>
struct Aggregator<T, O, typename std::enable_if<utils::is_cpp_array<T>::value, void>::type> {
  /// TODO Not documented
  void operator() (std::ostream &os, const std::vector<O> &objects,
                   std::function<const T& (const O&)> access,
                   uint verbosity) const {
    using T_ = typename T::value_type;

    os << "[\n";
    const auto &front = access(objects.front());
    for (uint i=0; i<front.size(); i++) {
      const auto subaccess = [access,i] (const auto &o) -> const T_& {
        return access(o)[i];
      };

      os << "\t";
      Aggregator<T_, O>()(os, objects, subaccess, verbosity);
      os << "\n";
    }
    os << "]\n";
  }
};

/// Delegates work to the aggregator for the underlying fundamental type
//template <typename T, typename O>
//struct Aggregator<T, O, typename std::enable_if<std::is_enum<T>::value
//                                                && !_details::is_pretty_enum<T>::value, void>::type> {
//  /// TODO Not documented
//  void operator() (std::ostream &os, const std::vector<O> &objects,
//                   std::function<const T& (const O&)> access,
//                   uint verbosity) const {

//    using underlying_t = typename std::underlying_type<T>::type;
//    Aggregator<underlying_t, O>()(
//      os, objects,
//      [access] (const auto &o) -> const underlying_t& {
//        return access(o);
//      }, verbosity);
//  }
//};

/// Cannot aggregate, instead list all values
template <typename T, typename O>
struct Aggregator<T, O, typename std::enable_if<_details::is_pretty_enum<T>::value, void>::type> {
  /// TODO Not documented
  void operator() (std::ostream &os, const std::vector<O> &objects,
                   std::function<const T& (const O&)> access,
                   uint verbosity) const {
    using EU = EnumUtils<T>;
    std::set<T> values;
    for (const O &object: objects)  values.insert(access(object));

    os << "[";
    for (T v: values) os << " " << EU::getName(v);
    os << " ]";
  }
};


// =============================================================================
// == Field metadata

namespace _details {
/// \cond internal

/// Declaration of the object type containing field management data
template <typename T, typename O, T O::* F>
struct EDNAFieldMetadata;

/// Genomic fields interface. Use to effortlessly manage values in artificial
/// genome
///
/// \tparam G The genome the managed field belong to
template <typename G>
class GenomeFieldInterface {
  /// Name of the field managed by this object
  std::string _name;

  /// Short name of the field managed by this object
  std::string _alias;

public:
  /// Helper alias to the source of randomness
  using Dice = rng::AbstractDice;

  /// Create an interface for the field \p name
  GenomeFieldInterface (const std::string &name, const std::string &alias)
    : _name(name), _alias(alias.empty() ? name : alias) {
    if (_name.size() < _alias.size())
      std::cerr << "WARNING: alias '" << _alias << " for field " << _name
                << " is suspciously too long" << std::endl;
  }

  /// \returns the name of the managed field
  const std::string& name (void) const {
    return _name;
  }

  /// \returns the short name of the managed field
  const std::string& alias (void) const {
    return _alias;
  }

  /// \returns Whether the managed field is also a self-aware genome
  virtual bool isSubgenomeField (void) const = 0;

  /// Stream the managed field to \p os
  virtual void print (std::ostream &os, const G &object) const = 0;

  /// Affect a random value to the corresponding field in \p genome
  virtual void random (G &genome, Dice &dice) const = 0;

  /// Randomly alter the value of the corresponding field \p genome
  virtual void mutate (G &genome, Dice &dice) const = 0;

  /// \returns the genomic distance between the corresponding fields in \p lhs and \p rhs
  virtual double distance (const G &lhs, const G &rhs) const = 0;

  /// Affects the appropriate field in \p res from either \p lhs 's or \p rhs 's
  /// based on a uniform coin toss
  virtual void cross (const G &lhs, const G &rhs, G &res, Dice &d) const = 0;

  /// \returns Whether the corresponding field is in the valid range
  virtual bool check (G &genome) const = 0;

  /// Dumps the corresponding field's value into the provided json
  virtual void to_json (nlohmann::json &j, const G &object) const = 0;

  /// Loads the corresponding field's value from the provided json
  virtual void from_json (const nlohmann::json &j, G &object) const = 0;

  /// \returns Whether the corresponding field values are equal in both objects
  virtual bool equal (const G &lhs, const G &rhs) const = 0;

  /// \returns the string equivalent of value under \p field in the provided object
  virtual std::string getField (const G &object, const std::string &field) const = 0;

  /// Writes an aggregate description of the managed field in each of the
  /// provided genomes into a synthetic description
  virtual void aggregate (std::ostream &os, const std::vector<G> &objects,
                          uint verbosity) const = 0;
};

/// Helper type storing the various types used by the self aware genomes
template <typename G>
struct EDNA_type_traits {
  /// The base type for this genome's field managers
  using value_t = _details::GenomeFieldInterface<G>;

  /// Helper alias to the source of randomness
  using Dice = typename value_t::Dice;

  /// Type used as value in the iterator
  using Iterator_value = std::reference_wrapper<value_t>;

  /// Iterator type for all auto-managed fields
  using Iterator = std::map<std::string, Iterator_value>;
};

/// \brief Genome field interface with direct access to the associated field
///
/// \tparam G the genome managed field belongs to
/// \tparam T the managed field's type
/// \tparam OFFSET the offset in G to the managed field
template <typename T, typename O, T O::*OFFSET>
class GenomeField : public GenomeFieldInterface<O> {

  /// Helper alias to the base type
  using Base = GenomeFieldInterface<O>;

protected:
  /// \returns A reference to the associated field
  static T& get (O &o) {
    return o.*OFFSET;
  }

  /// \returns A constant reference to the associated field
  static const T& get (const O &o) {
    return o.*OFFSET;
  }

public:
  /// Builds the base interface and register this field in \p registry
  template <typename R>
  GenomeField (const std::string &name, const std::string alias, R &registry)
    : GenomeFieldInterface<O> (name, alias) {
    registry.emplace(name, *this);
  }

  virtual ~GenomeField (void) = default;

  bool isSubgenomeField(void) const override {
    return false;
  }

  void print (std::ostream &os, const O &object) const override {
    using utils::operator<<;
    os << get(object);
  }

  /// \copydoc GenomeFieldInterface::check
  /// Prints a diagnostic in case of error
  bool check (O &object) const final {
    using utils::operator<<;

    T tmp = get(object);
    if (!checkPrivate(object)) {
      std::cerr << "Out-of-range value for field " << this->name() << ": "
                << tmp << " clipped to " << get(object) << std::endl;
      return false;
    }

    return true;
  }

  bool equal(const O &lhs, const O &rhs) const override {
    return get(lhs) == get(rhs);
  }

  void to_json (nlohmann::json &j, const O &object) const override {
    j = get(object);
  }

  void from_json (const nlohmann::json &j, O &object) const override {
    get(object) = j.get<T>();
  }

  std::string getField (const O &object, const std::string &field) const override {
    return Extractor<T>()(get(object), field);
  }

  void aggregate (std::ostream &os, const std::vector<O> &objects,
                  uint verbosity) const override {
    Aggregator<T, O>()(os, objects,
                       static_cast<const T&(*)(const O&)>(get), verbosity);
  }

protected:
  /// Delegate checker. Overriden in subclasses to perform the actual detection
  virtual bool checkPrivate (O &object) const = 0;
};

/// Helper alias to the pointer type for a genome field manager
template <typename T, typename O, T O::*F>
using GF_ptr = std::unique_ptr<_details::GenomeField<T, O, F>>;

/// A genomic field manager that relies on config::MutationSettings::Bounds to
/// automatically produce sensible values for the managed field
template <typename T, typename O, T O::*OFFSET>
class GenomeFieldWithBounds : public GenomeField<T,O,OFFSET> {

  /// \copydoc GenomeField::Base
  using Base = GenomeField<T,O,OFFSET>;

  using Base::get;
  using typename Base::Dice;

  /// Helper alias to the bounds type providing values for this field
  using Bounds = config::MutationSettings::Bounds<T, O>;

  /// Immutable reference to the bounds object providing values for this field
  const Bounds &_bounds;

public:
  /// Builds an interface for field \p name, registers it in \p registry and
  /// records the bounds object providing values for this field
  template <typename R>
  GenomeFieldWithBounds (const std::string &name, const std::string &alias,
                         R &registry, Bounds &bounds)
    : GenomeField<T,O,OFFSET> (name, alias, registry), _bounds(bounds) {}

  void random (O &object, Dice &dice) const override {
    get(object) = _bounds.rand(dice);
  }

  void mutate (O &object, Dice &dice) const override {
    _bounds.mutate(get(object), dice);
  }

  double distance (const O &lhs, const O &rhs) const override {
    return _bounds.distance(get(lhs), get(rhs));
  }

  void cross (const O &lhs, const O &rhs, O &res, Dice &d) const override {
    get(res) = d.toss(get(lhs), get(rhs));
  }

private:
  bool checkPrivate (O &object) const override {
    return _bounds.check(get(object));
  }
};


/// A genomic field manager that relies on functions to manage the associated
/// field
template <typename T, typename O, T O::*OFFSET>
class GenomeFieldWithFunctor : public GenomeField<T,O,OFFSET> {
protected:
  /// \copydoc GenomeField::Base
  using Base = GenomeField<T,O,OFFSET>;

  using Base::get;

private:
  using typename Base::Dice;

  /// Function for generating a random, valid value for the associated field
  using Random_f = std::function<T(Dice&)>;

  /// Function for altering the associated field's value
  using Mutate_f = std::function<void(T&, Dice&)>;

  /// Function returning either the first argument or the second
  using Cross_f = std::function<T(const T&, const T&, Dice&)>;

  /// Function return the genomic distance between both arguments
  using Distn_f = std::function<double(const T&, const T&)>;

  /// Function returning whether the argument is valid
  using Check_f = std::function<bool(T&)>;

public:
  /// Encapsulates the function set needed to managed a field
  const struct Functor {
    Random_f random = nullptr;    ///< \copydoc Random_f
    Mutate_f mutate = nullptr;    ///< \copydoc Mutate_f
    Cross_f cross = nullptr;      ///< \copydoc Cross_f
    Distn_f distance = nullptr;   ///< \copydoc Distn_f
    Check_f check = nullptr;      ///< \copydoc Check_f

  } _functor; ///< The function set used to manage this field

  /// Builds a genomic interface for field \p name, registers it in registry and
  /// stores the set of functions managing the associated field
  template <typename R>
  GenomeFieldWithFunctor (const std::string &name, const std::string &alias,
                          R &registry, Functor f)
    : GenomeField<T,O,OFFSET> (name, alias, registry), _functor(f) {
    checkFunctor(&Functor::random, "random");
    checkFunctor(&Functor::mutate, "mutate");
    checkFunctor(&Functor::cross, "cross");
    checkFunctor(&Functor::distance, "distance");
    checkFunctor(&Functor::check, "check");

    assert(_functor.random && _functor.mutate && _functor.cross
           && _functor.distance && _functor.check);
  }

  void random (O &object, Dice &dice) const override {
    get(object) = _functor.random(dice);
  }

  void mutate (O &object, Dice &dice) const override {
    _functor.mutate(get(object), dice);
  }

  double distance (const O &lhs, const O &rhs) const override {
    return _functor.distance(get(lhs), get(rhs));
  }

  void cross (const O &lhs, const O &rhs, O &res, Dice &d) const override {
    get(res) = _functor.cross(get(lhs), get(rhs), d);
  }

private:
  /// Asserts that \p field in this objects functor is not null
  /// \throws std::invalid_argument if it is
  template <typename F>
  void checkFunctor (F Functor::*field, const std::string &fieldName) {
    if (!(_functor.*field))
      throw std::invalid_argument("Provided functor." + fieldName
                                  + " for auto-field " + this->name()
                                  + " is null");
  }

  bool checkPrivate (O &object) const override {
    return _functor.check(get(object));
  }
};

/// A genomic field manager that delegates work to GenomeFieldWithFunctor
/// by building a functor for managed self-aware genome
template <typename T, typename O, T O::*OFFSET>
class GenomeFieldForSubgenome : public GenomeFieldWithFunctor<T,O,OFFSET> {

  /// Helper alias type the functor-based field manager
  using Base = GenomeFieldWithFunctor<T,O,OFFSET>;
  using Base::Base::get;

  /// Helper alias type to the functor type used by Base
  using Functor = typename Base::Functor;

  /// \return A functor delegating work to the managed self-awarre genome
  Functor buildFromSubgenome (void) {
    using D = typename EDNA_type_traits<T>::Dice;
    Functor f;
    f.random = T::random;
    f.mutate = static_cast<void(*)(T&,D&)>(T::mutate);
    f.cross = T::cross;
    f.distance = T::distance;
    f.check = static_cast<bool(*)(T&)>(T::check);
    return f;
  }

public:
  /// Builds a genomic interface for the (sub) self-aware genome field \p name,
  /// registers it in \p registry and provides Base with the necessary set of
  /// functions
  template <typename R>
  GenomeFieldForSubgenome(const std::string &name, const std::string &alias,
                          R &registry, int) : Base(name, alias, registry,
                                                   buildFromSubgenome()) {}

  /// \returns true. This is a edna field
  bool isSubgenomeField(void) const override {
    return true;
  }
};

/// Helper function for generating the mutation rates map from an initializer
/// list of {field,rate}.
/// Performs validity checks (though not that efficiently)
/// \throws std::invalid_argument if a field is missing its mutation rate
template <typename G, typename T = EDNA_type_traits<G>>
auto buildMap(const typename T::Iterator &it,
              std::initializer_list<std::pair<const typename T::value_t*,float>> &&l) {

  std::ostringstream oss;
  oss << "Checking " << utils::className<G>() << ":\n";

  bool ok = true;

  // Collect all registered fields
  std::set<typename T::Iterator::key_type> fNames;
  for (const auto &p: it) fNames.insert(p.first);

  // Check that the input only contains known field names
  for (const auto &p: l) {
    const auto &name = p.first->name();
    auto i = it.find(name);
    fNames.erase(name);
    if (i == it.end())
      oss << "\tInitializer list for mutation rates contains unknown value '"
          << name << "'\n", ok = false;
  }

  // Check that no fields are left uninitialized
  for (const auto &n: fNames)
    oss << "\tNo mutation rate defined for field " << n << "\n", ok = false;

  // Bail-out if errors were encountered
  if (!ok)
    throw std::invalid_argument(oss.str());

  // Otherwise everything's good we can build the map
  std::map<std::string, float> map;
  for (auto &p: l)  map[p.first->name()] = p.second;
  return map;
}

/// Helper type to force linker error on undefined genome field
/// \todo NOT WORKING
//template <typename T1, typename T2>
//struct Checker;

/// Manages indentation for provided ostream
/// \author James Kanze @ https://stackoverflow.com/a/9600752
class IndentingOStreambuf : public std::streambuf {
  static constexpr uint INDENT = 2; ///< Indentation between hierarchical levels

  std::ostream*       _owner;   ///< Associated ostream
  std::streambuf*     _buffer;  ///< Associated buffer
  bool                _isAtStartOfLine; ///< Whether to insert indentation
  const std::string   _indent;  ///< Indentation value

protected:
  /// Overrides std::basic_streambuf::overflow to insert indentation at line start
  virtual int overflow (int ch) {
    if ( _isAtStartOfLine && ch != '\n' )
      _buffer->sputn(_indent.data(), _indent.size());
    _isAtStartOfLine = (ch == '\n');
    return _buffer->sputc(ch);
  }

  /// \returns index of indent-storage space inside the ios_base static region
  static uint index (void) {
    static uint i = std::ios_base::xalloc();
    return i;
  }

  /// \returns the indent level for the associated level
  uint indent (uint di = 0) {
    long &i = _owner->iword(index());
    if (di != 0)  i += di;
    return i;
  }

public:
  /// Creates a proxy buffer managing indentation level
  explicit IndentingOStreambuf(std::ostream& dest)
    : _owner(&dest), _buffer(dest.rdbuf()),
      _isAtStartOfLine(true), _indent(indent(INDENT), ' ' ) {
      _owner->rdbuf( this );
  }

  /// Returns control of the buffer to its owner
  virtual ~IndentingOStreambuf(void) {
    _owner->rdbuf(_buffer);
    indent(-INDENT);
  }
};

/// \endcond
} // end of namespace _details

/// \brief Inherit from this class to gain access to a number of automated functions
///  (random generation, mutation, crossover, distance computation, bounds check)
///
/// Conforming to the interface requires some, minimal adaptations, in the form
/// of englobing macros.
///
/// Two types of field are currently built-in:
///   - Bounds-driven types:
///     - All primitive/fundamentals (int, float, uint, ...)
///     - std::array and other fixed size container (e.g. NOT std::vector)
///
///   - Functor-controlled types:
///     - Everything for which you (the end-user) can write the necessary functions
///
/// \tparam The derived type
///
/// \par Full blown example:
/// \snippet selfawaregenome_test.cpp selfAwareGenomeFullExample
template <typename G>
class EDNA {
protected:
  /// Helper alias to the derived type
  using this_t = G;

  /// Alias to helper type traits
  using traits = _details::EDNA_type_traits<G>;

  /// Helper alias to the source of randomness
  using Dice = typename traits::Dice;

public:
  /// Helper alias to the configuration type
  using config_t = config::EDNAConfigFile<G>;

// =============================================================================
// == Evolutionary interface

  /// Alters a single auto-managed field in the genome based on the relative
  /// mutation rates and valid bounds/generators
  void mutate (Dice &dice) {
    mutate(downcast(*this), dice);
  }

  /// \returns the genomic distance between all auto-managed fields in both
  /// arguments
  friend double distance (const G &lhs, const G &rhs) {
    return EDNA<G>::distance(lhs, rhs);
  }

  /// \returns the result of crossing all auto-managed fields in both arguments
  friend G cross (const G &lhs, const G &rhs, Dice &dice) {
    return EDNA<G>::cross(lhs, rhs, dice);
  }

  /// \returns Whether all auto-managed fields in this genome are valid
  bool check (void) {
    return check(downcast(*this));
  }

// =============================================================================
// == Evolutionary interface (static version)

  /// \returns A genome with all auto-managed fields initialized to appropriate
  /// values
  static G random (Dice &dice) {
    G tmp;
    for (auto &it: _iterator) get(it).random(tmp, dice);
    tmp.randomExtension(dice);
    return tmp;
  }

  /// Static implementation of mutate(Dice)
  static void mutate (G &object, Dice &dice) {
    std::string fieldName = dice.pickOne(_mutationRates.get());
    const auto &manager = _iterator.at(fieldName).get();

    if (config::EDNAConfigFile_common::autologMutations()
        && !manager.isSubgenomeField()) {
      std::cerr << "Mutated field " << fieldName << " from ";
      manager.print(std::cerr, object);
    }

    manager.mutate(object, dice);
    object.mutateExtension(dice);

    if (config::EDNAConfigFile_common::autologMutations() && !manager.isSubgenomeField()) {
      std::cerr << " to ";
      manager.print(std::cerr, object);
      std::cerr << std::endl;
    }
  }

  /// Static implementation of distance(G&,G&)
  static double distance (const G &lhs, const G &rhs) {
    double d = 0;
    for (auto &it: _iterator)
      d += _distanceWeights.get().at(get(it).name()) * get(it).distance(lhs, rhs);
    lhs.distanceExtension(rhs, d);
    return d;
  }

  /// Static implementation of cross(G&,G&,Dice&)
  static G cross (const G &lhs, const G &rhs, Dice &dice) {
    G res;
    for (auto &it: _iterator)
      get(it).cross(lhs, rhs, res, dice);
    res.crossExtension(lhs, rhs, dice);
    return res;
  }

  /// Static implementation of check()
  static bool check (G &object){
    bool ok = true;
    for (auto &it: _iterator) ok &= get(it).check(object);
    object.checkExtension(ok);
    return ok;
  }

// =============================================================================
// == Utilities

  /// \returns the string equivalent of value under \p field in this object
  std::string getField (const std::string &field) const {
    return getField(downcast(*this), field);
  }

  /// \returns the string equivalent of value under \p field in the provided object
  static std::string getField (const G &object, const std::string &field) {
    std::string localField = field;
    if (localField[0] == '.') localField.erase(0 ,1);
    size_t subfield_i = localField.find_first_of("[.");
    std::string subField;

    if (subfield_i != std::string::npos) {
      subField = localField.substr(subfield_i);
      localField = localField.substr(0, subfield_i);
      if (subField[0] == '.') subField.erase(0, 1);
    }

    auto it = _iterator.find(localField);
    if (it == _iterator.end())
      utils::doThrow<std::invalid_argument>(
        "'", localField, "' is not a valid field for ", utils::className<G>(),
        "\nnote: in call to getField(", field, ")");

    return get(*it).getField(object, subField);
  }

  /// Shorter alias to the json type
  using json = nlohmann::json;

  /// Dump data into a json
  friend void to_json (json &j, const G &g) {
    for (auto &it: _iterator)
      get(it).to_json(j[get(it).alias()], downcast(g));
    g.to_jsonExtension(j);
  }

  /// Load data from a json
  friend void from_json (const json &j_, G &g) {
    std::ostringstream oss;
    bool ok = true;

    auto j = j_;
    g.from_jsonExtension(j);
    for (auto &it: _iterator) {
      auto &gf = get(it);
      auto jit = j.find(gf.alias());
      if (jit != j.end()) {
        gf.from_json(*jit, downcast(g));
        j.erase(jit);

      } else {
        ok = false;
        oss << "Unable to find field " << it.first << "\n";
      }
    }

    ok &= j.empty();
    for (json::iterator it = j.begin(); it != j.end(); ++it)
      oss << "Extra field " << it.key() << "\n";

    g.check();

    if (!ok)
      throw std::invalid_argument(oss.str());
  }

  /// \returns the default file extension for serialized self aware genomes
  virtual std::string extension (void) const {
    return ".edna.json";
  }

  /// \returns a string representation of this genome's contents
  std::string dump (uint indent = -1) const {
    return json(downcast(*this)).dump(indent);
  }

  /// Writes this genome to \p filepath
  /// \throws std::invalid_argument if the path is not writable
  void toFile (const std::string &filepath, int indent = -1) {
    stdfs::path path (filepath);
    if (path.extension() == "")
      path += extension();

    std::ofstream ofs (path);
    if (!ofs.is_open())
      utils::doThrow<std::invalid_argument>("Unable to write to ", path);

    check();
    ofs << dump(indent);
  }

  /// Load a self-aware genome from provided \p filepath
  static G fromFile (const std::string &filepath) {
    auto s = utils::readAll(filepath);
    auto j = json::parse(s);
    return G(j);
  }

  /// Aggregates a collection of genome into a synthetic description
  static void aggregate (std::ostream &os, const std::vector<G> &objects,
                         uint verbosity = 0) {

    if (objects.size() < 2)
      utils::doThrow<std::invalid_argument>(
        "Aggregating ", objects.size(), " makes no sense...");

    _details::IndentingOStreambuf indent(os);
    for (auto &it: _iterator) {
      os << get(it).alias() << ": ";
      get(it).aggregate(os, objects, verbosity);
      os << "\n";
    }
  }


// =============================================================================
// == Operators
  /// Stream all auto-managed fields to \p os
  friend std::ostream& operator<< (std::ostream &os, const G &g) {
    _details::IndentingOStreambuf indent(os);
    os << "\n";
    for (auto &it: _iterator) {
      os << get(it).alias() << ": ";
      get(it).print(os, g);
      os << "\n";
    }
    g.to_streamExtension(os);
    return os;
  }

  /// \returns whether both arguments are equal
  friend bool operator== (const G &lhs, const G &rhs) {
    bool eq = true;
    for (auto &it: _iterator)
      eq &= (get(it).equal(lhs, rhs));
    lhs.equalExtension(rhs, eq);
    return eq;
  }


// =============================================================================
// == End-user extensions (for manually managed fields)

protected:
  /// Called on the newly-created object
  virtual void randomExtension (Dice&) {}

  /// Called on the mutated object
  virtual void mutateExtension (Dice&) {}

  /// Called on the first object
  virtual void distanceExtension (const G&, double &) const {}

  /// Called on the child object
  virtual void crossExtension (const G&, const G&, Dice&) {}

  /// Called on the checked object
  virtual void checkExtension (bool&) {}

  /// Called on the first object
  virtual void equalExtension (const G&, bool &) const {}

  /// Called on the serialized object
  virtual void to_jsonExtension (json &) const {}

  /// Called on the deserialized object
  /// \warning Removing all manually managed field from the json is mandatory
  virtual void from_jsonExtension (json &) {}

  /// Called on the streamed object
  virtual void to_streamExtension (std::ostream &) const {}

// =============================================================================
// == Reserved use

public:
  /// \returns a constant reference to the internal iterator object
  /// \attention This should be of little use to the end-user
  static const auto& iterator (void) {
    return _iterator;
  }

protected:
  /// Iterator object for all auto-managed fields
  static typename traits::Iterator _iterator;

  /// Helper alias for the reference to the config-embedded mutation rates
  using MutationRates = std::reference_wrapper<
    const config::MutationSettings::MutationRates>;

  /// Reference to the config-embedded mutation rates
  static const MutationRates _mutationRates;

  /// Helper alias for the reference to the config-embedded mutation rates
  using DistanceWeights = std::reference_wrapper<
    const typename config::_details::EDNAConfigFileTypes<G>::DistanceWeights>;

  /// Reference to the config-embedded distance weights
  static const DistanceWeights _distanceWeights;

  /// \return An immutable reference to the field manager
  static const auto& get (const typename traits::Iterator::value_type &it) {
    return it.second.get();
  }

  /// Downcasts the provided genome to its derived type
  static G& downcast (EDNA &g) { return static_cast<G&>(g);  }

  /// Upcasts the provided genome to its base type
  static EDNA& upcast (G &g) { return static_cast<EDNA&>(g);  }

  /// Downcasts the provided constant genome to its derived constant type
  static const G& downcast (const EDNA &g) {
    return static_cast<const G&>(g);
  }

  /// Upcasts the provided constant genome to its base type
  static const EDNA& upcast (const G &g) {
    return static_cast<const EDNA&>(g);
  }
};

template <typename G>
typename EDNA<G>::traits::Iterator EDNA<G>::_iterator;

/// Delegates extraction to the SelfAwareGenome::getField
template <typename T>
struct Extractor<T,
    typename std::enable_if<utils::is_base_template_of<EDNA,
    T>::value, void>::type> {
  /// TODO Not documented
  std::string operator() (const T &object, const std::string &field) const {

    return T::getField(object, field);
  }
};

/// Delegates aggregation to the SelfAwareGenome::aggregate
template <typename T, typename O>
struct Aggregator<T, O,
    typename std::enable_if<utils::is_base_template_of<EDNA,
                                                       T>::value, void>::type> {
  /// TODO Not documented
  void operator() (std::ostream &os, const std::vector<O> &objects,
                   std::function<const T& (const O&)> access,
                   uint verbosity) const {

    std::vector<T> subobjects;
    for (const O &object: objects)  subobjects.push_back(access(object));
    os << "\n";
    T::aggregate(os, subobjects, verbosity);
  }
};

/// \cond internal

/// The namespace for genetic material
#define __NMSP genotype

/// The namespace for private genetic material
#define __NMSP_D __NMSP::_details

/// The qualified genomic type
#define __SGENOME __NMSP::GENOME

/// The qualified configuration file
#define __SCONFIG __SGENOME::config_t

/// The template specialization for the given field
#define __METADATA(OBJECT, TYPE, FIELD) \
  EDNAFieldMetadata<TYPE, OBJECT, &OBJECT::FIELD>

/// The fully-qualified genome field manager name for type \p ITYE, field type
/// \p TYPE and name \p NAME
#define __TARGS(ITYPE, TYPE, NAME)  \
  __NMSP_D::ITYPE<                  \
    TYPE, __SGENOME,                \
    &__SGENOME::NAME                \
  >

/// Defines the field manager for field \p NAME of type \p TYPE
/// Instantiates an object of derived type \p ITYPE
#define __DEFINE_GENOME_FIELD_PRIVATE(ITYPE, TYPE, NAME, ALIAS, ...)  \
  namespace genotype::_details {                                      \
  const GF_ptr<TYPE, GENOME, &GENOME::NAME>                           \
    __METADATA(GENOME, TYPE, NAME)::metadata =                        \
      std::make_unique<__TARGS(ITYPE, TYPE, NAME)>(                   \
        #NAME, ALIAS, GENOME::_iterator, __VA_ARGS__                  \
      );                                                              \
  }

/// \endcond

/// Includes the necessary friend declarations
#define APT_EDNA()                                  \
  friend struct config::EDNAConfigFile<this_t>;     \
                                                    \
  template <typename O_, typename T_, T_ O_::* F_>  \
  friend struct _details::EDNAFieldMetadata;


/// Hides away the CRTP and multiple inheritance
#define EDNA_CONFIG_FILE(GENOME)                          \
  EDNAConfigFile<__NMSP::GENOME>                          \
  : public _details::EDNAConfigFileTypes<__NMSP::GENOME>, \
    public ConfigFile<EDNAConfigFile<__NMSP::GENOME>>

/// Declares that \p FIELD of \p TYPE in \p OBJECT should be automanaged (i.e
/// automated mutations, crossing, printing, ...)
/// \see EDNA
#define DECLARE_GENOME_FIELD(OBJECT, TYPE, FIELD)               \
  namespace _details {                                          \
  template <>                                                   \
  struct __METADATA(OBJECT, TYPE, FIELD) {                      \
    static const GF_ptr<TYPE, OBJECT, &OBJECT::FIELD> metadata; \
  };                                                            \
  }


/// Defines the genomic field \p NAME with type \p TYPE passing the variadic
/// arguments to build the mutation bounds
#define DEFINE_GENOME_FIELD_WITH_BOUNDS(TYPE, NAME, ALIAS, ...)     \
  DEFINE_PARAMETER_FOR(__SCONFIG, __SCONFIG::B<TYPE>,               \
                       NAME##Bounds, __VA_ARGS__)                   \
  __DEFINE_GENOME_FIELD_PRIVATE(GenomeFieldWithBounds, TYPE,        \
                                NAME, ALIAS,                        \
                                __SCONFIG::NAME##Bounds.ref())

/// Defines the genomic field \p NAME (aka \p ALIAS) with type \p TYPE passing the variadic
/// arguments to build the function set
#define DEFINE_GENOME_FIELD_WITH_FUNCTOR(TYPE, NAME, ALIAS, F) \
  __DEFINE_GENOME_FIELD_PRIVATE(GenomeFieldWithFunctor, TYPE, NAME, ALIAS, F)

/// Defines the genomic field \p NAME with type \p TYPE as a subgenome forwarding
/// its functions to the functor constructor
#define DEFINE_GENOME_FIELD_AS_SUBGENOME(TYPE, NAME, ALIAS) \
  __DEFINE_GENOME_FIELD_PRIVATE(GenomeFieldForSubgenome, TYPE, NAME, ALIAS, 0)

/// Helper alias to the type of a functor object for a specific, auto-managed,
/// field
#define GENOME_FIELD_FUNCTOR(TYPE, NAME) \
  __TARGS(GenomeFieldWithFunctor, TYPE, NAME)::Functor

/// Defines an object linking a mutation rate to an automatic field manager
#define MUTATION_RATE(NAME, VALUE)        \
  std::pair<                              \
    __NMSP_D::GenomeFieldInterface<       \
      __SGENOME                           \
    >*, float                             \
  >(genotype::_details::__METADATA(       \
    __SGENOME,                            \
    decltype(__SGENOME::NAME),            \
    NAME                                  \
  )::metadata.get(), VALUE)

/// Defines the mutation rates map for the current genome
#define DEFINE_GENOME_MUTATION_RATES(...) \
  namespace config {                      \
  DEFINE_MAP_PARAMETER_FOR(__SCONFIG,     \
    __SCONFIG::MR, mutationRates,         \
    __NMSP_D::buildMap<__SGENOME>(        \
      __SGENOME::iterator(), __VA_ARGS__  \
    )                                     \
  )                                       \
  }                                       \
                                          \
  namespace __NMSP {                      \
  template<>                              \
  const EDNA<GENOME>::MutationRates       \
    EDNA<GENOME>::_mutationRates =        \
    std::ref(__SCONFIG::mutationRates()); \
  }


/// Defines an object linking a distance weight to an automatic field manager
#define DISTANCE_WEIGHT(NAME, VALUE)      \
  std::pair<                              \
    __NMSP_D::GenomeFieldInterface<       \
      __SGENOME                           \
    >*, float                             \
  >(genotype::_details::__METADATA(       \
    __SGENOME,                            \
    decltype(__SGENOME::NAME),            \
    NAME                                  \
  )::metadata.get(), VALUE)

/// Defines the distance weights for the current genome
#define DEFINE_GENOME_DISTANCE_WEIGHTS(...) \
  namespace config {                        \
  DEFINE_MAP_PARAMETER_FOR(__SCONFIG,       \
    __SCONFIG::DW, distanceWeights,         \
    __NMSP_D::buildMap<__SGENOME>(          \
      __SGENOME::iterator(), __VA_ARGS__    \
    )                                       \
  )                                         \
  }                                         \
                                            \
  namespace __NMSP {                        \
  template<>                                \
  const EDNA<GENOME>::DistanceWeights       \
    EDNA<GENOME>::_distanceWeights =        \
    std::ref(__SCONFIG::distanceWeights()); \
}

} // end of namespace genotype

#endif // _SELF_AWARE_GENOME_H_
