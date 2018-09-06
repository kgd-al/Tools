#ifndef _SELF_AWARE_GENOME_H_
#define _SELF_AWARE_GENOME_H_

#include <ostream>
#include <string>
#include <map>

#include <functional>

#include "../utils/utils.h"
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

namespace genotype {

namespace _details {
/// \cond internal

/// Genomic fields interface. Use to effortlessly manage values in artificial
/// genome
///
/// \tparam G The genome the managed field belong to
template <typename G>
class GenomeFieldInterface {
  /// Name of the field managed by this object
  std::string _name;

public:
  /// Helper alias to the source of randomness
  using Dice = rng::AbstractDice;

  /// Create an interface for the field \p name
  GenomeFieldInterface (std::string name) : _name(name) {}

  /// \returns the name of the managed field
  const std::string& name (void) const {
    return _name;
  }

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
  using Base::name;

protected:
  /// \returns A reference to the associated field
  T& get (O &o) const {
    return o.*OFFSET;
  }

  /// \returns A constant reference to the associated field
  const T& get (const O &o) const {
    return o.*OFFSET;
  }

public:
  /// Builds the base interface and register this field in \p registry
  template <typename R>
  GenomeField (const std::string &name, R &registry)
    : GenomeFieldInterface<O> (name) {
    registry.emplace(name, *this);
  }

  virtual ~GenomeField (void) = default;

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
      std::cerr << "Out-of-range value for field " << name() << ": "
                << tmp << " clipped to " << get(object) << std::endl;
      return false;
    }

    return true;
  }

protected:
  /// Delegate checker. Overriden in subclasses to perform the actual detection
  virtual bool checkPrivate (O &object) const = 0;
};

/// A genomic field manager that relies on config::MutationSettings::Bounds to
/// automatically produce sensible values for the managed field
template <typename T, typename O, T O::*OFFSET>
class GenomeFieldWithBounds : public GenomeField<T,O,OFFSET> {

  /// \copydoc GenomeField::Base
  using Base = GenomeField<T,O,OFFSET>;

  using Base::get;
//  using Base::name;
  using typename Base::Dice;

  /// Helper alias to the bounds type providing values for this field
  using Bounds = config::MutationSettings::Bounds<T, O>;

  /// Immutable reference to the bounds object providing values for this field
  const Bounds &_bounds;

public:
  /// Builds an interface for field \p name, registers it in \p registry and
  /// records the bounds object providing values for this field
  template <typename R>
  GenomeFieldWithBounds (const std::string &name, R &registry, Bounds &bounds)
    : GenomeField<T,O,OFFSET> (name, registry), _bounds(bounds) {}

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

  /// \copydoc GenomeField::Base
  using Base = GenomeField<T,O,OFFSET>;
  using typename Base::Dice;
  using Base::get;

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
  GenomeFieldWithFunctor (const std::string &name, R &registry,
                          Random_f rndF, Mutate_f mutF,
                          Cross_f crsF, Distn_f dstF, Check_f chkF)
    : GenomeFieldWithFunctor (name, registry,
                              Functor{rndF, mutF, crsF, dstF, chkF}) {}

  /// Builds a genomic interface for field \p name, registers it in registry and
  /// stores the set of functions managing the associated field
  template <typename R>
  GenomeFieldWithFunctor (const std::string &name, R &registry, Functor f)
    : GenomeField<T,O,OFFSET> (name, registry), _functor(f) {
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
  bool checkPrivate (O &object) const override {
    return _functor.check(get(object));
  }
};

/// Helper function for generating the mutation rates map from an initializer
/// list of {field,rate}
template <typename T>
auto buildMap(std::initializer_list<std::pair<const T*,float>> &&l) {
  std::map<std::string, float> map;
  for (auto &p: l)  map[p.first->name()] = p.second;
  return map;
}

/// \endcond
} // end of namespace _details

/// \brief Inherit from this class to gain access to a number of automated functions (
/// random generation, mutation, crossover, distance computation, bounds check)
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

/// \tparam The derived type
///
/// \see selfawaregenome_test.cpp for implementation exemple
template <typename G>
class SelfAwareGenome {
protected:
  /// Helper alias to the derived type
  using this_t = G;

  /// The base type for this genome's field managers
  using value_t = _details::GenomeFieldInterface<G>;

  /// Helper alias to the source of randomness
  using Dice = typename value_t::Dice;

public:

// =============================================================================
// == Evolutionary interface

  /// \returns A genome with all auto-managed fields initialized to appropriate
  /// values
  static G random (Dice &dice) {
    G tmp;
    for (auto &it: _iterator) get(it).random(tmp, dice);
    return tmp;
  }

  /// Alters a single auto-managed field in the genome based on the relative
  /// mutation rates and valid bounds/generators
  void mutate (Dice &dice) {
    std::string fieldName = dice.pickOne(_mutationRates.get());
    std::cerr << "Mutated field " << fieldName << std::endl;
    _iterator.at(fieldName).get().mutate(static_cast<G&>(*this), dice);
  }

  /// \returns the genomic distance between all auto-managed fields in both
  /// arguments
  friend double distance (const this_t &lhs, const this_t &rhs) {
    double d = 0;
    for (auto &it: _iterator)
      d+= get(it).distance(lhs, rhs);
    return d;
  }

  /// \returns the result of crossing all auto-managed fields in both arguments
  friend this_t cross (const this_t &lhs, const this_t &rhs, Dice &dice) {
    this_t res;
    for (auto &it: _iterator)
      get(it).cross(lhs, rhs, res, dice);
    return res;
  }

  /// \returns Whether all auto-managed fields in this genome are valid
  bool check (void) {
    bool ok = true;
    for (auto &it: _iterator) ok &= get(it).check(static_cast<G&>(*this));
    return ok;
  }

// =============================================================================
// == Utilities

  /// Stream all auto-managed fields to \p os
  friend std::ostream& operator<< (std::ostream &os, const SelfAwareGenome &g) {
    for (auto &it: _iterator) {
      os << "\t" << it.first << ": ";
      get(it).print(os, static_cast<const G&>(g));
      os << "\n";
    }
    return os;
  }

protected:
  /// Type used as value in the iterator
  using Iterator_value = std::reference_wrapper<value_t>;

  /// Iterator type for all auto-managed fields
  using Iterator = std::map<std::string, Iterator_value>;

  /// Iterator object for all auto-managed fields
  static Iterator _iterator;

  /// Helper alias for the reference to the config-embedded mutation rates
  using MutationRates = std::reference_wrapper<const config::MutationSettings::MutationRates>;

  /// Reference to the config-embedded mutation rates
  static const MutationRates _mutationRates;

  /// \return An immutable reference to the field manager
  static const auto& get (const typename Iterator::value_type &it) {
    return it.second.get();
  }
};
template <typename G>
typename SelfAwareGenome<G>::Iterator SelfAwareGenome<G>::_iterator;

/// \cond internal

/// The fully-qualified genome field manager name for type \p ITYE, field type
/// \p TYPE and name \p NAME
#define __TARGS(ITYPE, TYPE, NAME)  \
  genotype::_details::ITYPE<        \
    TYPE, genotype::GENOME,         \
    &genotype::GENOME::NAME         \
  >

/// The field manager for field \p NAME
#define __GENOME_FIELD(NAME) genotype::GENOME::_##NAME##_metadata

/// Defines the field manager for field \p NAME of type \p TYPE
/// Instantiates an object of derived type \p ITYPE
#define __DEFINE_GENOME_FIELD_PRIVATE(ITYPE, TYPE, NAME, ...)   \
  namespace genotype {                                          \
  const std::unique_ptr<__TARGS(GenomeField, TYPE, NAME)>       \
    __GENOME_FIELD(NAME) =                                      \
    std::make_unique<__TARGS(ITYPE, TYPE, NAME)>(               \
      #NAME, genotype::GENOME::_iterator, __VA_ARGS__           \
    );                                                          \
  }

/// \endcond

/// Hides away the CRTP requires for fields auto-management independancy
#define SELF_AWARE_GENOME(NAME) \
  NAME : public SelfAwareGenome<NAME>

/// Declare a genomic field of type \p TYPE and name \p NAME
#define DECLARE_GENOME_FIELD(TYPE, NAME)  \
  TYPE NAME;                              \
  static const std::unique_ptr<           \
    _details::GenomeField<                \
        TYPE, this_t, &this_t::NAME       \
    >> _##NAME##_metadata;

/// Defines the genomic field \p NAME with type \p TYPE passing the variadic
/// arguments to build the mutation bounds
#define DEFINE_GENOME_FIELD_WITH_BOUNDS(TYPE, NAME, ...)              \
  DEFINE_PARAMETER(CFILE::B<TYPE>, NAME##Bounds, __VA_ARGS__)         \
  __DEFINE_GENOME_FIELD_PRIVATE(GenomeFieldWithBounds, TYPE, NAME,    \
                                 config::GENOME::NAME##Bounds.ref())

/// Defines the genomic field \p NAME with type \p TYPE passing the variadic
/// arguments to build the function set
#define DEFINE_GENOME_FIELD_WITH_FUNCTOR(TYPE, NAME, F) \
  __DEFINE_GENOME_FIELD_PRIVATE(GenomeFieldWithFunctor, TYPE, NAME, F)

/// Helper alias to the type of a functor object for a specific, auto-managed,
/// field
#define GENOME_FIELD_FUNCTOR(TYPE, NAME) \
  __TARGS(GenomeFieldWithFunctor, TYPE, NAME)::Functor

/// Defines an object linking a mutation rate to an automatic field manager
#define MUTATION_RATE(NAME, VALUE)            \
  std::pair<                                  \
    genotype::_details::GenomeFieldInterface< \
      genotype::GENOME                        \
    >*, float                                 \
  >(__GENOME_FIELD(NAME).get(), VALUE)

/// Defines the mutation rates map for the current genome
#define DEFINE_GENOME_MUTATION_RATES(...)       \
  namespace config {                            \
  DEFINE_MAP_PARAMETER(CFILE::M, mutationRates, \
    genotype::_details::buildMap<               \
      genotype::_details::GenomeFieldInterface< \
        genotype::GENOME                        \
      >                                         \
    >(__VA_ARGS__)                              \
  )                                             \
  }                                             \
                                                \
  namespace genotype {                          \
  template<>                                    \
  const SelfAwareGenome<GENOME>::MutationRates  \
    SelfAwareGenome<GENOME>::_mutationRates =   \
    std::ref(CFILE::mutationRates());           \
  }

} // end of namespace genotype

#endif // _SELF_AWARE_GENOME_H_
