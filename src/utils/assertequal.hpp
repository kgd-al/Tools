#ifndef KGD_ASSERTEQUAL_H
#define KGD_ASSERTEQUAL_H

#include "utils.h"

namespace utils {

/// Allows comparison of NaN values for types with such values
template <typename T>
constexpr std::enable_if_t<std::numeric_limits<T>::has_quiet_NaN, bool>
assertNaNEqual (const T &lhs, const T &rhs) {
  return std::isnan(lhs) && std::isnan(rhs);
}

/// Types with no NaN values return false
template <typename T>
constexpr std::enable_if_t<!std::numeric_limits<T>::has_quiet_NaN, bool>
assertNaNEqual (const T&, const T&) {   return false;   }

template <typename T>
void assertDeepcopy (const T &lhs, const T &rhs) {
  if (&lhs == &rhs)
    Thrower<std::logic_error>("Assert deepcopy violated: ", &lhs, " == ", &rhs);
}

template <typename T>
constexpr std::enable_if_t<std::is_floating_point<T>::value, void>
assertFuzzyEqual (const T &lhs, const T &rhs,
                  const T &threshold, bool deepcopy) {
  if (std::fabs(lhs - rhs) > threshold)
    Thrower<std::logic_error>("Assert fuzzy equal violated: |", lhs, " - ",
                              rhs, "| = ", std::fabs(lhs-rhs), " > ",
                              threshold);

  if (deepcopy) assertDeepcopy(lhs, rhs);
}

/// Asserts that two fundamental values are equal.
/// if deepcopy is true, also assert that they are stored at different addresses
template <typename T>
std::enable_if_t<std::is_fundamental<T>::value, void>
assertEqual (const T &lhs, const T &rhs, bool deepcopy) {
  if (lhs != rhs && !assertNaNEqual(lhs, rhs))
    Thrower<std::logic_error>("Assert equal violated: ", lhs, " != ", rhs);

  if (deepcopy) assertDeepcopy(lhs, rhs);
}

/// Asserts that two enumeration values are equal
template <typename T>
std::enable_if_t<std::is_enum<T>::value, void>
assertEqual (const T &lhs, const T &rhs, bool deepcopy) {
  using ut = typename std::underlying_type<T>::type;
  assertEqual(ut(lhs), ut(rhs), deepcopy);
}

/// Asserts that two pointed-to values are equal
template <typename T>
void assertEqual (T const * const &lhs, T const * const &rhs, bool deepcopy) {
  assertEqual(bool(lhs), bool(rhs), deepcopy);
  if (lhs && rhs) assertEqual(*lhs, *rhs, deepcopy);
}

/// Asserts that two uniquely pointed-to values are equal
template <typename T>
void assertEqual (const std::unique_ptr<T> &lhs, const std::unique_ptr<T> &rhs,
                  bool deepcopy) {
  assertEqual(bool(lhs), bool(rhs), deepcopy);
  if (lhs && rhs) assertEqual(*lhs, *rhs, deepcopy);
}

/// Asserts that two shared pointed-to values are equal
template <typename T>
void assertEqual (const std::shared_ptr<T> &lhs, const std::shared_ptr<T> &rhs,
                  bool deepcopy) {
  assertEqual(bool(lhs), bool(rhs), deepcopy);
  if (lhs && rhs) assertEqual(*lhs, *rhs, deepcopy);
}

/// Asserts that two pairs are equal
template <typename T1, typename T2>
void assertEqual (const std::pair<T1,T2> &lhs, const std::pair<T1,T2> &rhs,
                  bool deepcopy) {
  assertEqual(lhs.first, rhs.first, deepcopy);
  assertEqual(lhs.second, rhs.second, deepcopy);
}

/// Asserts that two stl containers are equal
template <typename T>
std::enable_if_t<is_stl_container_like<T>::value, void>
assertEqual (const T &lhs, const T &rhs, bool deepcopy) {
  auto lhsB = std::begin(lhs), lhsE = std::end(lhs),
       rhsB = std::begin(rhs), rhsE = std::end(rhs);
  assertEqual(std::distance(lhsB, lhsE), std::distance(rhsB, rhsE), deepcopy);

  uint i=0;
  for (auto lhsIt = lhsB, rhsIt = rhsB; lhsIt != lhsE; ++lhsIt, ++rhsIt)
    assertEqual(*lhsIt, *rhsIt, deepcopy), i++;
}

/// Asserts that two stl containers are equal after sorting
template <typename T, typename P, typename V = typename T::value_type>
std::enable_if_t<is_stl_container_like<T>::value
              && std::is_nothrow_invocable_r<bool, P, V, V>::value, void>
assertEqual (const T &lhs, const T &rhs, const P &predicate, bool deepcopy) {
  std::vector<V> lhsSorted (lhs.begin(), lhs.end());
  std::sort(lhsSorted.begin(), lhsSorted.end(), predicate);

  std::vector<V> rhsSorted (rhs.begin(), rhs.end());
  std::sort(rhsSorted.begin(), rhsSorted.end(), predicate);

  assertEqual(lhsSorted, rhsSorted, deepcopy);
}

template <typename T>
void assertEqual (const GenomeID<T> &lhs, const GenomeID<T> &rhs,
                  bool deepcopy) {
  using ut = typename GenomeID<T>::ut;
  utils::assertEqual((ut)lhs.id, (ut)rhs.id, deepcopy);
}

} // end of namespace utils

#endif // KGD_ASSERTEQUAL_H
