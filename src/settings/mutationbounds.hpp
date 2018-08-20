#ifndef _MUTATION_BOUNDS_HPP
#define _MUTATION_BOUNDS_HPP

#include <cassert>

struct MutationSettings {

  template <typename T>
  struct Bounds {
    T min, rndMin, rndMax, max;
    Bounds (T min, T rndMin, T rndMax, T max)
      : min(min), rndMin(rndMin), rndMax(rndMax), max(max) {
      assert(min <= rndMin && rndMin <= rndMax && rndMax <= max);
    }

    Bounds (T min, T max) : Bounds(min, min, max, max) {}
    Bounds (T min, T rndMin, bool, T max) : Bounds(min, rndMin, max, max) {}
    Bounds (T min, bool, T rndMax, T max) : Bounds(min, min, rndMax, max) {}

    double span (void) const {
      return double(max) - double(min);
    }

    template <typename RNG>
    T rand (RNG &rng) const {
      return rng(rndMin, rndMax);
    }

    template <typename U=T>
    std::enable_if_t<std::is_integral<U>::value, void>
    mutate (U &v, rng::AbstractDice &dice) const {
      assert(min <= v && v <= max);
      if (v == min)         v = min + 1;
      else if (v == max)    v = max - 1;
      else                  v += dice(.5)? -1 : 1;
    }

    static constexpr double __MUTATE_SCALAR_MAGIC_BULLET = 100.;
    template <typename U=T>
    std::enable_if_t<std::is_floating_point<U>::value, void>
    mutate (U &v, rng::AbstractDice &dice) const {
      assert(min <= v && v <= max);
      if (min < max) {
        rng::tndist dist (0, span()/__MUTATE_SCALAR_MAGIC_BULLET, min - v, max - v, true);
        v += dice(dist);
      }
    }

    friend std::ostream& operator<< (std::ostream& os, const Bounds &b) {
      return os << "(" << b.min << " " << b.rndMin << " " << b.rndMax << " " << b.max << ")";
    }

    friend std::istream& operator>> (std::istream& is, Bounds &b) {
      char junk;
      is >> junk >> b.min >> b.rndMin >> b.rndMax >> b.max >> junk;
      return is;
    }

  };
  template <typename T> using B = Bounds<T>;

  template <typename ENUM>
  using MutationRates = std::map<ENUM, float>;
};

#endif
