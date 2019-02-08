#ifndef KGD_UTILS_FUNCTIONS_H
#define KGD_UTILS_FUNCTIONS_H

/*!
  \file functions.h

  Contains definition for various widely used functions
*/

#include <math.h>

#include <type_traits>

namespace utils {

/// \returns \f$ g(x) = e^{- \frac{(x-mu)^2}{2*s^2}} \f$
template <typename T>
inline
std::enable_if_t<std::is_floating_point<T>::value, T>
gauss (T x, T mu, T sigma) {
  return exp( - (x-mu)*(x-mu) / (2*sigma*sigma));
}

/*! \returns \f$ x so that gauss(x, mu, sigma) = y
             \land (sign < 0 \to x < mu) \land (sign > 0 \to x > mu) \f$
*/
template <typename T>
inline
std::enable_if_t<std::is_floating_point<T>::value, T>
gauss_inverse (T y, T mu, T sigma, int sign) {
  return mu + sign * sqrt(-2 * sigma * sigma * log(y));
}

} // end of namespace utils

#endif // KGD_UTILS_FUNCTIONS_H
