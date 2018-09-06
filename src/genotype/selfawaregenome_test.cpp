#include <iostream>

#include "selfawaregenome.hpp"

// =============================================================================
// == Test area

#include <array>
namespace config {
struct Test;
}

// =============================================================================
// genome.h
namespace genotype {
class SELF_AWARE_GENOME(Test) {
public:
  DECLARE_GENOME_FIELD(int, intField)
  DECLARE_GENOME_FIELD(float, floatField)
  DECLARE_GENOME_FIELD(std::vector<float>, vectorField)

  using A = std::array<float,2>;
  DECLARE_GENOME_FIELD(A, arrayField)
};
}
// end of genome.h

// =============================================================================
// genomeconfig.h
namespace config {
#define CFILE Test
struct CFILE : public ConfigFile<CFILE> {
  template <typename T>
  using B = MutationSettings::Bounds<T, genotype::Test>;
  using M = MutationSettings::MutationRates;

  using A = std::array<float,2>;

  DECLARE_PARAMETER(B<int>, intFieldBounds)
  DECLARE_PARAMETER(B<A>, arrayFieldBounds)

  DECLARE_PARAMETER(M, mutationRates)
};
#undef CFILE
}
// end of genomeconfig.h

// =============================================================================
// genome.cpp
#define GENOME Test
#define CFILE config::GENOME
DEFINE_GENOME_FIELD_WITH_BOUNDS(int, intField, 1, 2, 3, 4)

static const auto floatFunctor = [] {
  GENOME_FIELD_FUNCTOR(float, floatField) functor {};
  functor.random =
      [] (auto &dice) { return dice(-1.f, 1.f); };

  functor.mutate =
      [] (float &f, auto &dice) { f += dice(-1.f, 1.f); };

  functor.cross =
      [] (const float &lf, const float &rf, auto &dice) {
        return dice.toss(lf, rf);
      };

  functor.distance =
      [] (const float &lf, const float &rf) { return fabs(lf - rf); };

  functor.check =
      [] (float &lf) {
        bool ok = true;
        if (lf < 0) lf = 0, ok &= false;
        if (1 < lf) lf = 1, ok &= false;
        return ok;
      };

  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(float, floatField, floatFunctor())

using A = genotype::GENOME::A;
DEFINE_GENOME_FIELD_WITH_BOUNDS(A, arrayField, A{-10,0}, A{0,10})

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(  intField, 2.f ),
  MUTATION_RATE(floatField, 1.f ),
  MUTATION_RATE(arrayField, 4.f ),
})

#undef CFILE
#undef GENOME
// end of genome.cpp


// =============================================================================
// == Possible use cases
int main (void) {
  using Genome = genotype::Test;

  config::Test::setupConfig("", config::SHOW);

  Genome g0 {};
  std::cout << "\nDefault init g0:\n" << g0 << std::endl;

  g0.intField = 42;
  g0.floatField = 42.f;
  g0.arrayField = {4,2};
  g0.vectorField = {4,2};
  std::cout << "\nModified g0:\n" << g0 << std::endl;
  std::cout << "\nis g0 valid? " << g0.check() << std::endl;

  rng::FastDice dice;
  Genome g1 = Genome::random(dice);
  std::cout << "\nRandom g1:\n" << g1 << std::endl;

  for (uint i=0; i<5; i++) {
    g1.mutate(dice);
    std::cout << "\nAfter mutation " << i << ":\n" << g1 << std::endl;
  }

  std::cout << "\nDistance(g0,g1) = " << distance(g0, g1) << std::endl;
  std::cout << "\ng0 x g1 = " << cross(g0, g1, dice) << std::endl;

  return 0;
}
