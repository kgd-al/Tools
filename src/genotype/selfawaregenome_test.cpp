/// [selfAwareGenomeFullExample]

#include <array>
#include <iostream>

#include "../settings/configfile.h"
#include "../settings/mutationbounds.hpp"

#include "selfawaregenome.hpp"

// =============================================================================
// == Internal genome with a single fundamental field
// =============================================================================
// trivial.h
namespace genotype {
class SELF_AWARE_GENOME(InternalTrivial) {
public:
  InternalTrivial(void) {}
  virtual ~InternalTrivial(void) = default;

  DECLARE_GENOME_FIELD(float, floatField)
};
} // end of namespace genotype
// end of trivial.h

// =============================================================================
// trivialconfig.h
namespace genotype { class InternalTrivial; }
namespace config {
template <>
struct SAG_CONFIG_FILE(InternalTrivial) {
  DECLARE_PARAMETER(B<float>, floatFieldBounds)
  DECLARE_PARAMETER(M, mutationRates)
};
} // end of namespace config
// end of trivialconfig.h

// =============================================================================
// trivial.cpp
#define GENOME InternalTrivial
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, floatField, "ff", -4.f, 0.f, 0.f, 4.f)

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
  InternalComplex(void) {}

  DECLARE_GENOME_FIELD(std::string, stringField)
};
} // end of namespace genotype
// end of complex.h

// =============================================================================
// complexconfig.h
namespace genotype { class InternalComplex; }
namespace config {
template <>
struct SAG_CONFIG_FILE(InternalComplex) {
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
      [] (auto &dice) { return std::string(dice(1u,2u), '#'); };

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
DEFINE_GENOME_FIELD_WITH_FUNCTOR(std::string, stringField, "sf", stringFunctor())

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
enum Enum { V0, V1, V2 };
std::ostream& operator<< (std::ostream &os, Enum e) {
  return os << int(e);
}
std::istream& operator>> (std::istream &is, Enum &e) {
  int iv;
  is >> iv;
  e = Enum(iv);
  return is;
}

struct SELF_AWARE_GENOME(External) {
public:
  External(void) {}
  virtual ~External(void) = default;

  DECLARE_GENOME_FIELD(int, intField)
  DECLARE_GENOME_FIELD(std::vector<InternalTrivial>, vectorField)
  DECLARE_GENOME_FIELD(InternalComplex, recField)

  DECLARE_GENOME_FIELD(Enum, enumField)

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
struct SAG_CONFIG_FILE(External) {
  using A = std::array<float,2>;

  DECLARE_PARAMETER(B<int>, intFieldBounds)
  DECLARE_PARAMETER(B<A>, arrayFieldBounds)
  DECLARE_PARAMETER(B<genotype::Enum>, enumFieldBounds)

  DECLARE_PARAMETER(M, mutationRates)
};
} // end of namespace config
// end of genomeconfig.h

// =============================================================================
// genome.cpp
#define GENOME External
DEFINE_GENOME_FIELD_WITH_BOUNDS(int, intField, "", 1, 2, 3, 4)

using V = std::vector<genotype::InternalTrivial>;
static const auto vectorFunctor = [] {
  GENOME_FIELD_FUNCTOR(V, vectorField) functor {};
  functor.random = [] (auto &) { return V(); };
  functor.mutate = [] (auto &v, auto &d) {
    v.push_back(genotype::InternalTrivial::random(d));
  };
  functor.cross = [] (const auto &lhs, const auto &rhs, auto &) {
    V res;
    res.insert(res.end(), lhs.begin(), lhs.end());
    res.insert(res.end(), rhs.begin(), rhs.end());
    return res;
  };
  functor.distance = [] (const auto &lhs, const auto &rhs) {
    return fabs(double(lhs.size()) - double(rhs.size()));
  };
  functor.check = [] (auto &) { return true; };
  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(V, vectorField, "vf", vectorFunctor())
DEFINE_GENOME_FIELD_AS_SUBGENOME(genotype::InternalComplex, recField, "rf")

DEFINE_GENOME_FIELD_WITH_BOUNDS(genotype::Enum, enumField, "ef", genotype::Enum::V0, genotype::Enum::V2)

using A = genotype::GENOME::A;
DEFINE_GENOME_FIELD_WITH_BOUNDS(A, arrayField, "af", A{-10,0}, A{0,10})

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(  intField, 2.f ),
  MUTATION_RATE(arrayField, 4.f ),
  MUTATION_RATE(vectorField, 4.f ),
  MUTATION_RATE(recField, 4.f ),
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

  std::cout << "\nDefault init g0:" << g0 << std::endl;

  setter(g0);
  std::cout << "\nModified g0:" << g0 << std::endl;
  std::cout << "\nis g0 valid? " << g0.check() << std::endl;

  rng::FastDice dice;
  GENOME g1 = GENOME::random(dice);
  std::cout << "\nRandom g1:" << g1 << std::endl;

  for (uint i=0; i<5; i++) {
    g1.mutate(dice);
    std::cout << "\nAfter mutation " << i << ":" << g1 << std::endl;
  }

  std::cout << "\nDistance(g0,g1) = " << distance(g0, g1) << std::endl;
  std::cout << "\ng0 x g1 = " << cross(g0, g1, dice) << std::endl;

  std::string serialTestFile ("showcase_" + utils::unscopedClassName<GENOME>() + ".gnm");
  g0.toFile(serialTestFile);
  GENOME g0_ = GENOME::fromFile(serialTestFile);
  assert(g0 == g0_);

  std::cout << "g1 serialized: " << g1.dump(2) << std::endl;

  std::cout.flush();
  std::cerr.flush();
}

int main (void) {
  showcase<genotype::InternalTrivial>([] (auto&) {});

  showcase<genotype::InternalComplex>([] (auto &g) {
    g.stringField = "tOt!";
  });

  showcase<genotype::External>([] (auto &g) {
    rng::FastDice dice (1);
    g.intField = 42;
    g.arrayField = {4,2};
    g.recField = genotype::InternalComplex::random(dice);
    g.vectorField = {
      genotype::InternalTrivial::random(dice),
      genotype::InternalTrivial::random(dice)
    };
  });

  return 0;
}
/// [selfAwareGenomeFullExample]
