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

namespace genotype {

namespace _details {

template <typename O>
class GenomeField_base {
  std::string _name;

public:
  using Dice = rng::AbstractDice;

  GenomeField_base (std::string name) : _name(name) {}

  virtual void print (std::ostream &os, const O &object) const = 0;

  virtual void random (O &o, Dice &d) const = 0;
  virtual void mutate (O &o, Dice &d) const = 0;
  virtual double distance (const O &lhs, const O &rhs) const = 0;
  virtual void cross (const O &lhs, const O &rhs, O &res, Dice &d) const = 0;
};

template <typename T, typename O, T O::*OFFSET>
class GenomeField : public GenomeField_base<O> {
protected:
  T& get (O &o) const {
    return o.*OFFSET;
  }

  const T& get (const O &o) const {
    return o.*OFFSET;
  }

public:
  template <typename R>
  GenomeField (const std::string &name, R &registry)
    : GenomeField_base<O> (name) {
    registry.emplace(name, *this);
  }

  virtual ~GenomeField (void) = default;

  void print (std::ostream &os, const O &object) const override {
    using utils::operator<<;
    os << get(object);
  }
};

template <typename T, typename O, T O::*OFFSET>
class GenomeFieldWithBounds : public GenomeField<T,O,OFFSET> {
  using Base = GenomeField<T,O,OFFSET>;

  using Base::get;
  using typename Base::Dice;

  using Bounds = config::MutationSettings::Bounds<T, O>;
  const Bounds &_bounds;

public:
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
};

template <typename T, typename O, T O::*OFFSET>
class GenomeFieldWithFunctor : public GenomeField<T,O,OFFSET> {
  using Base = GenomeField<T,O,OFFSET>;
  using typename Base::Dice;
  using Base::get;

  using Random_f = std::function<T(Dice&)>;
  using Mutate_f = std::function<void(T&, Dice&)>;
  using Cross_f = std::function<T(const T&, const T&, Dice&)>;
  using Distn_f = std::function<double(const T&, const T&)>;

  const struct Functor {
    Random_f random;
    Mutate_f mutate;
    Cross_f cross;
    Distn_f distance;
  } _functor;

  template <typename F>
  Functor map (F f_) {
    Functor f;
    f.random = f_.random;
    f.mutate = f_.mutate;
    f.cross = f_.cross;
    f.distance = f_.distance;
    return f;
  }

public:
  template <typename R, typename F>
  GenomeFieldWithFunctor (const std::string &name, R &registry, F f)
    : GenomeFieldWithFunctor(name, registry, map(f)) {}

  template <typename R>
  GenomeFieldWithFunctor (const std::string &name, R &registry,
                          Random_f rndF, Mutate_f mutF,
                          Cross_f crsF, Distn_f dstF)
    : GenomeFieldWithFunctor (name, registry, Functor{rndF, mutF, crsF, dstF}) {}

  template <typename R>
  GenomeFieldWithFunctor (const std::string &name, R &registry, Functor f)
    : GenomeField<T,O,OFFSET> (name, registry), _functor(f) {}

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
};

class SelfAwareGenome_base {
public:
  SelfAwareGenome_base();
};

template <typename T>
auto buildMap(std::initializer_list<std::pair<const T*,float>> &&l) {
  std::map<std::string, float> map;

  return map;
}

} // end of namespace _details

template <typename G>
class SelfAwareGenome : public _details::SelfAwareGenome_base {
protected:
  using this_t = G;
  using value_t = _details::GenomeField_base<G>;
  using value_ptr = std::unique_ptr<value_t>;

  using Dice = typename value_t::Dice;

public:

// =============================================================================
// == Evolutionary interface

  static G random (Dice &dice) {
    G tmp;
    for (auto &it: _iterator) get(it).random(tmp, dice);
    return tmp;
  }

  void mutate (Dice &dice) {
    std::string fieldName = dice.pickOne(_mutationRates.get());
    std::cerr << "Mutated field " << fieldName << std::endl;
    _iterator.at(fieldName).get().mutate(static_cast<G&>(*this), dice);
  }

  friend double distance (const this_t &lhs, const this_t &rhs) {
    double d = 0;
    for (auto &it: _iterator)
      d+= get(it).distance(lhs, rhs);
    return d;
  }

  friend this_t cross (const this_t &lhs, const this_t &rhs, Dice &dice) {
    this_t res;
    for (auto &it: _iterator)
      get(it).cross(lhs, rhs, res, dice);
    return res;
  }

// =============================================================================
// == Utilities

  friend std::ostream& operator<< (std::ostream &os, const SelfAwareGenome &g) {
    for (auto &it: _iterator) {
      os << "\t" << it.first << ": ";
      get(it).print(os, static_cast<const G&>(g));
      os << "\n";
    }
    return os;
  }

protected:
  using Iterator_value = std::reference_wrapper<value_t>;
  using Iterator = std::map<std::string, Iterator_value>;

  static Iterator _iterator;

  using MutationRates = std::reference_wrapper<const config::MutationSettings::MutationRates>;
  static const MutationRates _mutationRates;

  static const auto& get (const typename Iterator::value_type &it) {
    return it.second.get();
  }

private:
  /// \todo !!
  static const bool valid = [] {
    bool ok = true;
    std::cerr << "Static check of " << utils::className<G>() << ": " << ok << std::endl;
    return ok;
  }();
};
template <typename G>
typename SelfAwareGenome<G>::Iterator SelfAwareGenome<G>::_iterator;

#define SELF_AWARE_GENOME(NAME) \
  NAME : public SelfAwareGenome<NAME>

#define DECLARE_GENOME_FIELD(TYPE, NAME)  \
  TYPE NAME;                              \
  static const std::unique_ptr<           \
    _details::GenomeField<                \
        TYPE, this_t, &this_t::NAME       \
    >> _##NAME##_metadata;

#define __TARGS(ITYPE, TYPE, NAME)  \
  genotype::_details::ITYPE<        \
    TYPE, genotype::GENOME,         \
    &genotype::GENOME::NAME         \
  >

#define __GENOME_FIELD(NAME) genotype::GENOME::_##NAME##_metadata

#define __DEFINE_GENOME_FIELD_PRIVATE(ITYPE, TYPE, NAME, ...)   \
  namespace genotype {                                          \
  const std::unique_ptr<__TARGS(GenomeField, TYPE, NAME)>       \
    __GENOME_FIELD(NAME) =                                      \
    std::make_unique<__TARGS(ITYPE, TYPE, NAME)>(               \
      #NAME, genotype::GENOME::_iterator, __VA_ARGS__           \
    );                                                          \
  }


#define DEFINE_GENOME_FIELD_WITH_BOUNDS(TYPE, NAME, ...)              \
  DEFINE_PARAMETER(CFILE::B<TYPE>, NAME##Bounds, __VA_ARGS__)         \
  __DEFINE_GENOME_FIELD_PRIVATE(GenomeFieldWithBounds, TYPE, NAME,    \
                                 config::GENOME::NAME##Bounds.ref())

#define DEFINE_GENOME_FIELD_WITH_FUNCTOR(TYPE, NAME, F) \
  __DEFINE_GENOME_FIELD_PRIVATE(GenomeFieldWithFunctor, TYPE, NAME, F)

#define MUTATION_RATE(NAME, VALUE) \
  std::pair<genotype::_details::GenomeField_base<genotype::GENOME>*, float>( \
    __GENOME_FIELD(NAME).get(), VALUE \
  )

#define DEFINE_GENOME_MUTATION_RATES(...)       \
  namespace config {                            \
  DEFINE_MAP_PARAMETER(CFILE::M, mutationRates, \
    genotype::_details::buildMap<               \
      genotype::_details::GenomeField_base<     \
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
