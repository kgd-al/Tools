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
  GENOME_FIELD(int, intField)
  GENOME_FIELD(float, floatField)
  GENOME_FIELD(std::vector<float>, vectorField)

  using A = std::array<float,2>;
  GENOME_FIELD(A, arrayField)
};
}
// end of genome.h

// genomeconfig.h
namespace config {
#define CFILE Test
struct CFILE : public ConfigFile<CFILE> {
  template <typename T>
  using B = MutationSettings::Bounds<T, genotype::Test>;

  using A = std::array<float,2>;

  DECLARE_PARAMETER(B<int>, intFieldBounds)
  DECLARE_PARAMETER(B<A>, arrayFieldBounds)
};
#undef CFILE
}
// end of genomeconfig.h

// genome.cpp
//namespace genotype {
#define GENOME Test
#define CFILE config::GENOME
DECLARE_GENOME_FIELD_WITH_BOUNDS(int, intField, 1, 2, 3, 4)

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
DECLARE_GENOME_FIELD_WITH_FUNCTOR(float, floatField, FloatFunctor())

using A = genotype::GENOME::A;
DECLARE_GENOME_FIELD_WITH_BOUNDS(A, arrayField, A{-10,0}, A{0,10})

#undef CFILE
#undef GENOME
//}
// end of genome.cpp



int main (void) {
  using Genome = genotype::Test;

  config::Test::setupConfig("", config::SHOW);

  Genome g {};
  std::cout << "\nDefault init g:\n" << g << std::endl;

  g.intField = 42;
  g.floatField = 42.f;
  g.arrayField = {4,2};
  g.vectorField = {4,2};
  std::cout << "\nModified g:\n" << g << std::endl;

  return 0;
}
