#ifndef _MUTATION_BOUNDS_HPP
#define _MUTATION_BOUNDS_HPP

#include <cassert>

#include <type_traits>
#include <random>

namespace config {

/*!
 *\file mutationbounds.hpp
 *
 * Contains helper structures for easier mutation bounds definition
 */

/// Provide handy types for mutation bounds definition
struct MutationSettings {

  /// Mutation bounds for a specific genomic field
  /// \tparam T a primitive type (int, float, ...)
  template <typename T, typename O>
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
    template <typename U=T>
    std::enable_if_t<std::is_fundamental<U>::value, double>
    span (void) const {
      return double(max) - double(min);
    }

    /// \return the range of valid values for these bounds
    template <typename U=T>
    std::enable_if_t<utils::is_cpp_array<U>::value, double>
    span (uint i) const {
      return double(max[i]) - double(min[i]);
    }

    /// \return the absolute distance for \p field between \p lhs and \p rhs
    /// scaled by the maximal span for this field
    template <typename U=T>
    std::enable_if_t<std::is_fundamental<U>::value, double>
    distance (const U &lhs, const U &rhs) const {
      return fabs(lhs - rhs) / span();
    }

    /// \return the absolute distance for multidimensional field between \p lhs
    /// and \p rhs scaled by the maximal span for this field
    template <typename U=T>
    std::enable_if_t<utils::is_cpp_array<U>::value, double>
    distance (const U &lhs, const U &rhs) const {
      double d = 0;
      uint i = 0;
      for (auto &v: lhs)
        d += fabs(lhs[i] - rhs[i]) / span(i), i++, (void)v;
      return d / i;
    }

    /// \return a value in the initial range
    /// \tparam RNG Probably a dice \see rng::AbstractDice
    template <typename RNG, typename U=T>
    std::enable_if_t<std::is_fundamental<U>::value, U>
    rand (RNG &rng) const {
      return rng(rndMin, rndMax);
    }

    /// \return a value in the initial, multidimensional, range
    /// \tparam RNG Probably a dice \see rng::AbstractDice
    template <typename RNG, typename U=T>
    std::enable_if_t<utils::is_cpp_array<U>::value, U>
    rand (RNG &rng) const {
      U tmp;
      uint i = 0;
      for (auto &v: tmp)
        v = rng(rndMin[i], rndMax[i]), i++;
      return tmp;
    }

    /// Mutates the \p v according to the absolute bounds
    template <typename U=T>
    std::enable_if_t<std::is_fundamental<U>::value, void>
    mutate (U &v, rng::AbstractDice &dice) const {
      mutate(v, min, max, dice);
    }

    /// Mutates a single value in the multidimensional \p v
    /// according to the absolute bounds
    template <typename U=T>
    std::enable_if_t<utils::is_cpp_array<U>::value, void>
    mutate (U &a, rng::AbstractDice &dice) const {
      uint i = dice(0ul, a.size());
      mutate(a[i], min[i], max[i], dice);
    }

    /// Checks that \p v is inside the absolute bounds
    template <typename U=T>
    std::enable_if_t<std::is_fundamental<U>::value, bool>
    check (U &v) const {
      return check(v, min, max);
    }

    /// Checks that each value in \p a is inside the absolute bounds
    template <typename U=T>
    std::enable_if_t<utils::is_cpp_array<U>::value, bool>
    check (U &a) const {
      uint i = 0;
      bool ok = true;
      for (auto &v: a)
        ok &= check(v, min[i], max[i]), i++;
      return ok;
    }

    /// Streams a simple, space-delimited, representation
    friend std::ostream& operator<< (std::ostream& os, const Bounds &b) {
      using utils::operator<<;
      return os << "(" << b.min << " " << b.rndMin << " " << b.rndMax << " " << b.max << ")";
    }

    /// Reads data from a simple, space-delimited, representation
    friend std::istream& operator>> (std::istream& is, Bounds &b) {
      using utils::operator>>;
      char junk;
      is >> junk >> b.min >> b.rndMin >> b.rndMax >> b.max >> junk;
      return is;
    }

  private:

    /// Mutates the integer \p v according to the absolute bounds
    template <typename U=T>
    static std::enable_if_t<std::is_integral<U>::value, void>
    mutate (U &v, const U min, const U max, rng::AbstractDice &dice) {
      assert(min <= v && v <= max);
      if (v == min)         v = min + 1;
      else if (v == max)    v = max - 1;
      else                  v += dice(.5)? -1 : 1;
    }

    /// Mutates the decimal \p v according to the absolute bounds
    template <typename U=T>
    static std::enable_if_t<std::is_floating_point<U>::value, void>
    mutate (U &v, const U min, const U max, rng::AbstractDice &dice) {
      static constexpr double __MAGIC_BULLET = 1e-2;

      assert(min <= v && v <= max);
      if (min < max) {
        rng::tndist dist (0, (max - min) * __MAGIC_BULLET, min - v, max - v, true);
        v += dice(dist);
      }
    }

    /// \returns whether \p v is in range [min, max]
    template <typename U=T>
    std::enable_if_t<std::is_fundamental<U>::value, bool>
    static check (U &v, const U min, const U max) {
      bool ok = true;
      if (v < min)  v = min,  ok &= false;
      if (max < v)  v = max,  ok &= false;
      return ok;
    }
  };

  /// Another alias type for shorter configuration files
  using MutationRates = std::map<std::string, float>;
};

} // end of namespace config

#endif
