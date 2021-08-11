#ifndef KGD_UTILS_MUTATION_BOUNDS_HPP
#define KGD_UTILS_MUTATION_BOUNDS_HPP

#include <cassert>

#include <type_traits>
#include <random>

#include "../random/dice.hpp"
#include "../utils/utils.h"

namespace config {

/*!
 *\file mutationbounds.hpp
 *
 * Contains helper structures for easier mutation bounds definition
 */

/// Provide handy types for mutation bounds definition
struct MutationSettings {

  /// Helper alias to the source of randomness
  using Dice = rng::AbstractDice;

  template <typename T, typename = void>
  struct BoundsOperators {
    static_assert(sizeof(T) == -1, "Invalid bound type");
  };

  template <typename T, typename = void>
  struct stddev_t {
    using type = float;
    static constexpr type implicitValue = 1e-2;
  };

  template <typename T, size_t SIZE>
  struct stddev_t<std::array<T,SIZE>> {
    using type = std::array<float,SIZE>;
    static constexpr type implicitValue = utils::uniformStdArray<type>(1e-2);
  };

  /// Mutation bounds for a specific genomic field
  /// \tparam T a primitive type (int, float, ...)
  template <typename T, typename O>
  struct Bounds {

    /// Helper alias to the specialized template performing the rand/mutate/...
    /// operations
    using Operators = BoundsOperators<T>;

    /// Minimal value reachable through random initialisation/mutation
    T min;

    /// Minimal value reachable through random initialisation
    T rndMin;

    /// Maximal value reachable through random initialisation
    T rndMax;

    /// Maximal value reachable through random initialisation/mutation
    T max;

    /// Standard deviations for automated mutations
    using StdDev_t = stddev_t<T>;
    using StdDev = typename stddev_t<T>::type;
    StdDev stddev;

    /// Default value
    Bounds (void) {}

    /// Use provided bounds a absolute minimal, initial minimal,
    /// initial maximal, absolute maximal
    Bounds (T min, T rndMin, T rndMax, T max,
            StdDev stddev = StdDev_t::implicitValue)
      : min(min), rndMin(rndMin), rndMax(rndMax), max(max), stddev(stddev) {
      assert(min <= rndMin && rndMin <= rndMax && rndMax <= max);
    }

    /// No difference between absolute and initial extremums
    Bounds (T min, T max, StdDev stddev = StdDev_t::implicitValue)
      : Bounds(min, min, max, max, stddev) {}

//    /// Different absolute and initial minimals. Identical for maximal.
//    Bounds (T min, T rndMin, bool, T max) : Bounds(min, rndMin, max, max) {}

//    /// Different absolute and initial maximals. Identical for minimal.
//    Bounds (T min, bool, T rndMax, T max) : Bounds(min, min, rndMax, max) {}

    /// \return a value in the initial range
    T rand (Dice &dice) const {
      return Operators::rand(rndMin, rndMax, dice);
    }

    /// \return the absolute distance for \p field between \p lhs and \p rhs
    /// scaled by the maximal span for this field
    double distance (const T &lhs, const T &rhs) const {
      return Operators::distance(lhs, rhs, min, max);
    }

    /// Mutates the \p v according to the absolute bounds
    void mutate (T &v, Dice &dice) const {
      Operators::mutate(v, min, max, stddev, dice);
    }

    /// Checks that \p v is inside the absolute bounds
    bool check (T &v) const {
      return Operators::check(v, min, max);
    }

    /// Streams a simple, space-delimited, representation
    friend std::ostream& operator<< (std::ostream& os, const Bounds &b) {
      using utils::operator<<;
      return os << "(" << b.min << " " << b.rndMin << " "
                << b.rndMax << " " << b.max << " " << b.stddev << ")";
    }

    /// Reads data from a simple, space-delimited, representation
    friend std::istream& operator>> (std::istream& is, Bounds &b) {
      using utils::operator>>;
      char junk;
      is >> junk >> b.min >> b.rndMin >> b.rndMax >> b.max >> b.stddev >> junk;
      return is;
    }

    /// Stores as a simple json array
    friend void to_json (nlohmann::json &j, const Bounds &b) {
      j = {b.min, b.rndMin, b.rndMax, b.max, b.stddev};
    }

    /// Restores from a simple json array
    friend void from_json (const nlohmann::json &j, Bounds &b) {
      uint i=0;
      b.min = j[i++];
      b.rndMin = j[i++];
      b.rndMax = j[i++];
      b.max = j[i++];
      b.stddev = j[i++];
    }
  };

  /// Another alias type for shorter configuration files
  using MutationRates = std::map<std::string, float>;
};

/// \cond internal

/// Bounds manager specialized in fundamental types (int, float, ...)
template <typename T>
struct MutationSettings::BoundsOperators<T, std::enable_if_t<std::is_fundamental<T>::value>> {
  using Dice = MutationSettings::Dice; ///< \copydoc MutationSettings::Dice

  /// \copydoc MutationSettings::Bounds::rand
  static T rand (T min, T max, Dice &dice) {
    return dice(min, max);
  }

  /// \return the range of valid values for these bounds
  static double span (double min, double max) {
    return max - min;
  }

  /// \copydoc MutationSettings::Bounds::distance
  static double distance (T lhs, T rhs, T min, T max) {
    return (std::max(lhs, rhs) - std::min(lhs, rhs)) / span(min, max);
  }

  /// \copydoc MutationSettings::Bounds::mutate
  static void mutate (T &v, T min, T max, float param, Dice &dice) {
    _mutate(v, min, max, param, dice);
  }

  /// \copydoc MutationSettings::Bounds::check
  static bool check (T &v, T min, T max) {
    bool ok = true;
    if (v < min)  v = min,  ok &= false;
    if (max < v)  v = max,  ok &= false;
    return ok;
  }

private:

  /// Mutates the integer \p v according to the absolute bounds
  template <typename U=T>
  static std::enable_if_t<std::is_integral<U>::value, void>
  _mutate (U &v, const U min, const U max, float /*param*/, Dice &dice) {
    assert(min <= v && v <= max);
    if (v == min)         v = min + 1;
    else if (v == max)    v = max - 1;
    else                  v += dice(.5)? -1 : 1;
  }

  /// Mutates the decimal \p v according to the absolute bounds
  template <typename U=T>
  static std::enable_if_t<std::is_floating_point<U>::value, void>
  _mutate (U &v, const U min, const U max, float stddev, Dice &dice) {
    assert(min <= v && v <= max);
    if (min < max) {
      rng::tndist dist (0, (max - min) * stddev, min - v, max - v, true);
      v += dice(dist);
    }
  }
};

/// Bounds manager specialized for enumeration types (actually just forwards
/// values as integers)
template <typename E>
struct MutationSettings::BoundsOperators<E, std::enable_if_t<std::is_enum<E>::value>> {
  using Dice = MutationSettings::Dice; ///< \copydoc MutationSettings::Dice

  /// Helper alias to the actual behind-the-scenes worker
  using Operator = MutationSettings::BoundsOperators<int>;

  /// \return a value in the initial enumeration range
  static E rand (E min, E max, Dice &dice) {
    return E(Operator::rand(int(min), int(max), dice));
  }

  /// \return the distance between two enumeration values
  static double distance (E lhs, E rhs, E min, E max) {
    return Operator::distance(int(lhs), int(rhs), int(min), int(max));
  }

  /// Change the enumeration value by another, valid, one
  static void mutate (E &v, E min, E max, float param, Dice &dice) {
    int iv = v;
    Operator::mutate(iv, int(min), int(max), param, dice);
    v = E(iv);
  }

  /// \returns whether the enumeration value is the valid range
  static bool check (E &v, E min, E max) {
    int iv = v;
    bool ok = Operator::check(iv, int(min), int(max));
    v = E(iv);
    return ok;
  }
};

/// Bounds manager specialized for array types
template <typename T>
struct MutationSettings::BoundsOperators<T, std::enable_if_t<utils::is_cpp_array<T>::value>> {
  using Dice = MutationSettings::Dice; ///< \copydoc MutationSettings::Dice

  /// Helper alias to the single-value worker
  using FOperators = MutationSettings::BoundsOperators<typename T::value_type>;

  /// \return a value in the initial, multidimensional, range
  static T rand (const T &min, const T &max, Dice &dice) {
    T tmp;
    uint i = 0;
    for (auto &v: tmp)
      v = dice(min[i], max[i]), i++;
    return tmp;
  }

  /// \return the range of valid values for these bounds
  static double span (const T &min, const T &max, uint i) {
    return double(max[i]) - double(min[i]);
  }

  /// \return the absolute distance for multidimensional field between \p lhs
  /// and \p rhs scaled by the maximal span for this field
  static double distance (const T &lhs, const T &rhs, const T &min, const T &max) {
    double d = 0;
    uint i = 0;
    for (auto &v: lhs)
      d += FOperators::distance(lhs[i], rhs[i], min[i], max[i]), i++, (void)v;
    return d;
  }

  /// Mutates a single value in the multidimensional \p v
  /// according to the absolute bounds
  template <typename P>
  static void mutate (T &a, const T &min, const T &max, const P &params,
                      Dice &dice) {
    uint i = dice(0ul, a.size()-1);
    FOperators::mutate(a[i], min[i], max[i], params[i], dice);
  }

  /// Checks that each value in \p a is inside the absolute bounds
  static bool check (T &a, const T &min, const T &max) {
    uint i = 0;
    bool ok = true;
    for (auto &v: a)
      ok &= FOperators::check(v, min[i], max[i]), i++;
    return ok;
  }
};

/// \endcond

} // end of namespace config

#endif
