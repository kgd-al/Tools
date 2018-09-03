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

  /// Mutation bounds for a specific field
  /// \tparam T a primitive type (int, float, ...)
  /// \tparam O the object containing this field
  /// \tparam FIELD the field offset
  template <typename T, typename O>
  class Bounds {
    /// The field type for these bounds
    using Field = T O::*;

    /// The offset to the managed field
    Field field;

  public:
    /// Minimal value reachable through random initialisation/mutation
    T min;

    /// Minimal value reachable through random initialisation
    T rndMin;

    /// Maximal value reachable through random initialisation
    T rndMax;

    /// Maximal value reachable through random initialisation/mutation
    T max;

    /// Use provided bounds a absolute minimal, initial minimal, initial maximal, absolute maximal
    Bounds (Field f, T min, T rndMin, T rndMax, T max)
      : field(f), min(min), rndMin(rndMin), rndMax(rndMax), max(max) {
      assert(min <= rndMin && rndMin <= rndMax && rndMax <= max);
    }

    /// No difference between absolute and initial extremums
    Bounds (Field f, T min, T max) : Bounds(f, min, min, max, max) {}

    /// Different absolute and initial minimals. Identical for maximal.
    Bounds (Field f, T min, T rndMin, bool, T max) : Bounds(f, min, rndMin, max, max) {}

    /// Different absolute and initial maximals. Identical for minimal.
    Bounds (Field f, T min, bool, T rndMax, T max) : Bounds(f, min, min, rndMax, max) {}

    /// \return the range of valid values for these bounds
    double span (void) const {
      return double(max) - double(min);
    }

    /// \return the absolute distance for \p field between \p lhs and \p rhs
    /// scaled by the maximal span for this field
    double distance (const O &lhs, const O &rhs) const {
      return fabs(lhs.*field - rhs.*field) / span();
    }

    /// \return a value in the initial range
    /// \tparam RNG Probably a dice \see rng::AbstractDice
    template <typename RNG>
    T rand (RNG &rng) const {
      return rng(rndMin, rndMax);
    }

    /// Mutates the integer \p v according to the absolute bounds
    std::enable_if_t<std::is_integral<T>::value, void>
    mutate (O &o, rng::AbstractDice &dice) const {
      T &v = o.*field;
      assert(min <= v && v <= max);
      if (v == min)         v = min + 1;
      else if (v == max)    v = max - 1;
      else                  v += dice(.5)? -1 : 1;
    }

    /// Constant defining the scale of variation a decimal can expect
    static constexpr double __MUTATE_SCALAR_MAGIC_BULLET = 1e-2;

    /// Mutates the decimal \p v according to the absolute bounds
    std::enable_if_t<std::is_floating_point<T>::value, void>
    mutate (O &o, rng::AbstractDice &dice) const {
      T &v = o.*field;
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

  /// Another alias type for shorter configuration files
  template <typename ENUM>
  using MutationRates = std::map<ENUM, float>;
};

#endif
