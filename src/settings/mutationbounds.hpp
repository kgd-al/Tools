#ifndef _MUTATION_BOUNDS_HPP
#define _MUTATION_BOUNDS_HPP

#include <cassert>

#include <random>
/*!
 *\file mutationbounds.hpp
 *
 * Contains helper structures for easier mutation bounds definition
 */

/// Provide handy types for mutation bounds definition
struct MutationSettings {

  /// Mutation bounds for a primitive type
  /// \tparam T a primitive type (int, float, ...)
  template <typename T>
  struct Bounds {

    /// Minimal value reachable through random initialisation/mutation
    T min;

    /// Minimal value reachable through random initialisation
    T rndMin;

    /// Maximal value reachable through random initialisation
    T rndMax;

    /// Maximal value reachable through random initialisation/mutation
    T max;

    /// Use provided bounds a absolute minimal, initial minimal, initial maximal, absolute maximal
    Bounds (T min, T rndMin, T rndMax, T max)
      : min(min), rndMin(rndMin), rndMax(rndMax), max(max) {
      assert(min <= rndMin && rndMin <= rndMax && rndMax <= max);
    }

    /// No difference between absolute and initial extremums
    Bounds (T min, T max) : Bounds(min, min, max, max) {}

    /// Different absolute and initial minimals. Identical for maximal.
    Bounds (T min, T rndMin, bool, T max) : Bounds(min, rndMin, max, max) {}

    /// Different absolute and initial maximals. Identical for minimal.
    Bounds (T min, bool, T rndMax, T max) : Bounds(min, min, rndMax, max) {}

    /// \return the range of valid values for these bounds
    double span (void) const {
      return double(max) - double(min);
    }

    /// \return a value in the initial range
    /// \tparam RNG Probably a dice \see rng::AbstractDice
    template <typename RNG>
    T rand (RNG &rng) const {
      return rng(rndMin, rndMax);
    }

    /// Mutates the integer \p v according to the absolute bounds
    template <typename U=T>
    std::enable_if_t<std::is_integral<U>::value, void>
    mutate (U &v, rng::AbstractDice &dice) const {
      assert(min <= v && v <= max);
      if (v == min)         v = min + 1;
      else if (v == max)    v = max - 1;
      else                  v += dice(.5)? -1 : 1;
    }

    /// Constant defining the scale of variation a decimal can expect
    static constexpr double __MUTATE_SCALAR_MAGIC_BULLET = 1e-2;

    /// Mutates the decimal \p v according to the absolute bounds
    template <typename U=T>
    std::enable_if_t<std::is_floating_point<U>::value, void>
    mutate (U &v, rng::AbstractDice &dice) const {
      assert(min <= v && v <= max);
      if (min < max) {
        rng::tndist dist (0, span() * __MUTATE_SCALAR_MAGIC_BULLET, min - v, max - v, true);
        v += dice(dist);
      }
    }

    /// Streams a simple, space-delimited, representation
    friend std::ostream& operator<< (std::ostream& os, const Bounds &b) {
      return os << "(" << b.min << " " << b.rndMin << " " << b.rndMax << " " << b.max << ")";
    }

    /// Reads data from a simple, space-delimited, representation
    friend std::istream& operator>> (std::istream& is, Bounds &b) {
      char junk;
      is >> junk >> b.min >> b.rndMin >> b.rndMax >> b.max >> junk;
      return is;
    }

  };

  /// Alias type for shorter configuration files
  template <typename T> using B = Bounds<T>;

  /// Another alias type for shorter configuration files
  template <typename ENUM>
  using MutationRates = std::map<ENUM, float>;
};

#endif
