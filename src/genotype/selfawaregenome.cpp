#include <iostream>

#include "selfawaregenome.h"

namespace genotype {

namespace _details {

SelfAwareGenome_base::SelfAwareGenome_base() {

}

} // end of namespace _details

} // end of namespace genotype


// =============================================================================
// == Test area

#include <array>
namespace config {
struct Test;
}

/// Allowed bounds-driven types:
///   - All primitive/fundamentals (int, float, uint, ...)
///   - std::array and other fixed size container (e.g. NOT std::vector)
///
/// Allowed functor-controlled types:
///   - Everything for with you (the end-user) can write the necessary functions
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

// genome.cpp
//namespace genotype {
#define GENOME Test
#define CFILE config::GENOME
DEFINE_GENOME_FIELD_WITH_BOUNDS(int, intField, 1, 2, 3, 4)

struct FloatFunctor {
  using Dice = rng::AbstractDice;
  static constexpr auto random =
      [] (Dice &dice) { return dice(-1.f, 1.f); };

  static constexpr auto mutate =
      [] (float &f, Dice &dice) { f += dice(-1.f, 1.f); };

  static constexpr auto cross =
      [] (const float &lf, const float &rf, Dice &dice) { return dice.toss(lf, rf); };

  static constexpr auto distance =
      [] (const float &lf, const float &rf) { return fabs(lf - rf); };
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(float, floatField, FloatFunctor())

using A = genotype::GENOME::A;
DEFINE_GENOME_FIELD_WITH_BOUNDS(A, arrayField, A{-10,0}, A{0,10})

DEFINE_GENOME_MUTATION_RATES(/*std::initializer_list<std::pair<const genotype::_details::GenomeField_base<genotype::GENOME>*,float>>*/{
  /*std::pair<const genotype::_details::GenomeField_base<genotype::GENOME>*,float>(*/MUTATION_RATE(  intField, 2.f )/*)*/,
  MUTATION_RATE(floatField, 1.f ),
  MUTATION_RATE(arrayField, 4.f ),
})

#undef CFILE
#undef GENOME
//}
// end of genome.cpp



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
