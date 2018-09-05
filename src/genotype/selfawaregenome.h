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

protected:
  using Dice = rng::AbstractDice;

public:
  GenomeField_base (std::string name) : _name(name) {}

  virtual void print (std::ostream &os, const O &object) const = 0;

  virtual void random (O &object, Dice &dice) const = 0;
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
};

template <typename T, typename O, T O::*OFFSET>
class GenomeFieldWithFunctor : public GenomeField<T,O,OFFSET> {
  using Base = GenomeField<T,O,OFFSET>;
  using typename Base::Dice;
  using Base::get;

  using Random_f = std::function<T(Dice&)>;
  using Mutate_f = std::function<void(T&, Dice&)>;
  using Cross_f = std::function<T(const T&, const T&, Dice&)>;
  using Distn_f = std::function<void(const T&, const T&)>;

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
};

class SelfAwareGenome_base {
public:
  SelfAwareGenome_base();
};

} // end of namespace _details

template <typename G>
class SelfAwareGenome : public _details::SelfAwareGenome_base {
public:
  friend std::ostream& operator<< (std::ostream &os, const SelfAwareGenome &g) {
    for (auto &it: _iterator) {
      os << "\t" << it.first << ": ";
      it.second.get().print(os, static_cast<const G&>(g));
      os << "\n";
    }
    return os;
  }

protected:
  using this_t = G;

  using Iterator_value = std::reference_wrapper<_details::GenomeField_base<G>>;
  using Iterator = std::map<std::string, Iterator_value>;

  static Iterator _iterator;

private:
};
template <typename G>
typename SelfAwareGenome<G>::Iterator SelfAwareGenome<G>::_iterator;

#define SELF_AWARE_GENOME(NAME) \
  NAME : public SelfAwareGenome<NAME>

#define GENOME_FIELD(TYPE, NAME)                              \
  TYPE NAME;                       \
  static const std::unique_ptr< \
    _details::GenomeField< \
        TYPE, this_t, &this_t::NAME  \
    >> _##NAME##_metadata;

#define __TARGS(ITYPE, TYPE, NAME) \
  genotype::_details::ITYPE<                      \
    TYPE, genotype::GENOME, &genotype::GENOME::NAME \
  >

#define __DECLARE_GENOME_FIELD_PRIVATE(ITYPE, TYPE, NAME, ...) \
  const std::unique_ptr<__TARGS(GenomeField, TYPE, NAME)> \
    genotype::GENOME::_##NAME##_metadata = \
    std::make_unique<__TARGS(ITYPE, TYPE, NAME)>( \
      #NAME, genotype::GENOME::_iterator, __VA_ARGS__ \
    );


#define DECLARE_GENOME_FIELD_WITH_BOUNDS(TYPE, NAME, ...) \
  DEFINE_PARAMETER(CFILE::B<TYPE>, NAME##Bounds, &genotype::GENOME::NAME, __VA_ARGS__) \
  __DECLARE_GENOME_FIELD_PRIVATE(GenomeFieldWithBounds, TYPE, NAME, config::GENOME::NAME##Bounds.ref())

#define DECLARE_GENOME_FIELD_WITH_FUNCTOR(TYPE, NAME, F)            \
  __DECLARE_GENOME_FIELD_PRIVATE(GenomeFieldWithFunctor, TYPE, NAME, F)

} // end of namespace genotype

#endif // _SELF_AWARE_GENOME_H_
