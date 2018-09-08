#include <iostream>

#include "../settings/configfile.h"
#include "../settings/mutationbounds.hpp"

#include <array>
namespace config {
template <typename G>
struct SAGConfigFileTypes {
  template <typename T>
  using B = MutationSettings::Bounds<T, G>;
  using M = MutationSettings::MutationRates;
};
template <typename G> struct SAGConfigFile;
}

#include "selfawaregenome.hpp"

// =============================================================================
// == Internal genome with a single fundamental field
// =============================================================================
// trivial.h
namespace genotype {
class SELF_AWARE_GENOME(InternalTrivial) {
public:
  DECLARE_GENOME_FIELD(float, floatField)
};
} // end of namespace genotype
// end of trivial.h

// =============================================================================
// trivialconfig.h
namespace genotype { struct InternalTrivial; }
namespace config {
template <>
struct SAG_CONFIG_FILE(genotype::InternalTrivial) {
  DECLARE_PARAMETER(B<float>, floatFieldBounds)
  DECLARE_PARAMETER(M, mutationRates)
};
} // end of namespace config
// end of trivialconfig.h

// =============================================================================
// trivial.cpp
#define GENOME InternalTrivial
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, floatField, -4.f, 0.f, 0.f, 4.f)

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(floatField, 1.f ),
})

#undef GENOME
// end of trivial.cpp

// =============================================================================
// == Internal genome with a single complex field
// =============================================================================
// complex.h
namespace genotype {
class SELF_AWARE_GENOME(InternalComplex) {
public:
  DECLARE_GENOME_FIELD(std::string, stringField)
};
} // end of namespace genotype
// end of complex.h

// =============================================================================
// complexconfig.h
namespace genotype { struct InternalComplex; }
namespace config {
template <>
struct SAG_CONFIG_FILE(genotype::InternalComplex) {
  using M = MutationSettings::MutationRates;
  DECLARE_PARAMETER(M, mutationRates)
};
} // end of namespace config
// end of complexconfig.h

// =============================================================================
// complex.cpp
#define GENOME InternalComplex

static const auto stringFunctor = [] {
  static constexpr auto min = 'a';
  static constexpr auto max = 'z';
  static constexpr auto span = max - min;

  GENOME_FIELD_FUNCTOR(std::string, stringField) functor {};
  functor.random =
      [] (auto &) { return std::string(); };

  functor.mutate =
      [] (auto &s, auto &dice) { s += dice(min, max); };

  functor.cross =
      [] (const auto &ls, const auto &rs, auto &dice) {
        uint i = dice(0ul, std::min(ls.size(), rs.size()));
        return ls.substr(0, i) + rs.substr(i);
      };

  functor.distance =
      [] (const auto &ls, const auto &rs) {
        if (ls.size() != rs.size())
          return fabs(double(ls.size()) - double(rs.size()));

        double d = 0;
        for (uint i=0; i<ls.size(); i++)
          d += fabs(ls[i] - rs[i]) / span;
        return d;
      };

  functor.check =
      [] (auto &s) {
        bool ok = true;
        for (char &c: s) {
          if (!std::islower(c)) {
            if (std::isupper(c))
                  c = std::tolower(c);
            else  c = 'a';
            ok &= false;
          }
        }
        return ok;
      };

  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(std::string, stringField, stringFunctor())

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(stringField, 1.f ),
})

#undef GENOME
// end of complex.cpp


// =============================================================================
// == External genome with a fundamental types and sub-genomes
// =============================================================================
// genome.h
namespace genotype {
class SELF_AWARE_GENOME(External) {
public:
  DECLARE_GENOME_FIELD(int, intField)
  DECLARE_GENOME_FIELD(std::vector<float>, vectorField)

  using A = std::array<float,2>;
  DECLARE_GENOME_FIELD(A, arrayField)
};
} // end of namespace genotype
// end of genome.h

// =============================================================================
// genomeconfig.h
namespace genotype { struct External; }
namespace config {
template <>
struct SAG_CONFIG_FILE(genotype::External) {
  using A = std::array<float,2>;

  DECLARE_PARAMETER(B<int>, intFieldBounds)
  DECLARE_PARAMETER(B<std::vector<float>>, vectorFieldBounds)
  DECLARE_PARAMETER(B<A>, arrayFieldBounds)

//  DECLARE_PARAMETER(M, mutationRates)
};
} // end of namespace config
// end of genomeconfig.h

// =============================================================================
// genome.cpp
#define GENOME External
DEFINE_GENOME_FIELD_WITH_BOUNDS(int, intField, 1, 2, 3, 4)

using V = std::vector<float>;
static const auto vectorFunctor = [] {
  GENOME_FIELD_FUNCTOR(V, vectorField) functor {};
  functor.random = [] (auto &) { return V(); };
  functor.mutate = [] (auto &, auto &) {};
  functor.cross = [] (const auto &, const auto &, auto &) { return V(); };
  functor.distance = [] (const auto &, const auto &) { return 0; };
  functor.check = [] (auto &) { return false; };
  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(V, vectorField, vectorFunctor())

using A = genotype::GENOME::A;
DEFINE_GENOME_FIELD_WITH_BOUNDS(A, arrayField, A{-10,0}, A{0,10})

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(  intField, 2.f ),
  MUTATION_RATE(arrayField, 4.f ),
})

#undef GENOME
// end of genome.cpp


// =============================================================================
// == Possible use cases
template <typename GENOME, typename F>
void showcase (F setter) {
  GENOME::config_t::setupConfig("", config::SHOW);

  GENOME g0 {};

  std::cout << "\n" << utils::className<GENOME>() << " size: "
            << sizeof(g0) << std::endl;

  std::cout << "\nDefault init g0:\n" << g0 << std::endl;

  setter(g0);
  std::cout << "\nModified g0:\n" << g0 << std::endl;
  std::cout << "\nis g0 valid? " << g0.check() << std::endl;

  rng::FastDice dice;
  GENOME g1 = GENOME::random(dice);
  std::cout << "\nRandom g1:\n" << g1 << std::endl;

  for (uint i=0; i<5; i++) {
    g1.mutate(dice);
    std::cout << "\nAfter mutation " << i << ":\n" << g1 << std::endl;
  }

  std::cout << "\nDistance(g0,g1) = " << distance(g0, g1) << std::endl;
  std::cout << "\ng0 x g1 = " << cross(g0, g1, dice) << std::endl;
}


int main (void) {
//  showcase<genotype::InternalTrivial>([] (auto &g) {});

//  showcase<genotype::InternalComplex>([] (auto &g) {
//    g.stringField = "tOt!";
//  });

  showcase<genotype::External>([] (auto &g) {
    g.intField = 42;
    g.arrayField = {4,2};
    g.vectorField = {4,2};
  });

  return 0;
}
