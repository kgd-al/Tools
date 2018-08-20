#ifndef _DICE_HPP_
#define _DICE_HPP_

/******************************************************************************//**
 * @file
 * @brief Contains the logic for random number generation.
 *
 * Provides aliases for commonly used types as well as the main class Dice.
 */

#include <random>
#include <chrono>
#include <stdexcept>
#include <iostream>

#include <vector>
#include <map>

#include <mutex>

#ifndef NDEBUG
#include <sstream>
#include <exception>
#include <assert.h>
#endif

namespace rng {

/******************************************************************************//**
 * @brief Naive truncated normal distribution.
 *
 * Converges towards a 0.5 efficiency given reasonably bad parameters
 * (e.g. mean=max and min << max-stddev, tested with 10e7 evaluations)
 */
template<typename FLOAT>
class truncated_normal_distribution : public std::normal_distribution<FLOAT> {
#ifndef NDEBUG
  //! The number of attempts made to generate a number before failing (Debug only)
  static constexpr size_t MAX_TRIES = 100;
#endif

public:
  //! Creates a normal distribution with the usual parameters mu and sigma constrained to the interval [min, max]
  truncated_normal_distribution(FLOAT mu, FLOAT stddev, FLOAT min, FLOAT max, bool nonZero=true)
    : std::normal_distribution<FLOAT>(mu,stddev), _min(min), _max(max), _nonZero(nonZero) {
#ifndef NDEBUG
    assert(min < max); // No inverted bounds
    assert(fabs(mu - min) >  2. * stddev || fabs(mu - max) > 2. * stddev); // At least 47.5% of values are valid
    // e-g: min = 0; mu = max = 1; if std < 1/6 then for x a rnd number P(x in [min,max]) > 0.4985
#endif
  }

  //! Requests production of a new random value
  template<typename RNG>
  FLOAT operator() (RNG &rng) {
    FLOAT res;
#ifndef NDEBUG
    size_t tries = 0;
#endif
    do {
#ifndef NDEBUG
      if (tries++ > MAX_TRIES) {
        std::ostringstream oss;
        oss << "truncated_normal_distribution("
            << std::normal_distribution<FLOAT>::mean() << ", "
            << std::normal_distribution<FLOAT>::stddev() << ", "
            << _min << ", " << _max << ", " << std::boolalpha << _nonZero
            << ") failed to produce a valid in value in less than "
            << tries << " attempts";
        throw std::domain_error(oss.str());
      }
#endif
      res = std::normal_distribution<FLOAT>::operator()(rng);
    } while (res < _min || _max < res || (_nonZero && res == 0));
    return res;
  }

  //! Prints this distribution to the provided stream
  friend std::ostream& operator<< (std::ostream &os, const truncated_normal_distribution &d) {
    return os << d.mean() << " " << d.stddev() << " " << d._min << " " << d._max;
  }

private:
  FLOAT _min; //!< The lowest value that can be produced
  FLOAT _max; //!< The largest value that can be produced
  bool _nonZero;  //!< Is zero a forbidden value ?
};

// ===================================================================================

typedef std::uniform_real_distribution<double> uddist; ///< Alias for a decimal uniform distribution
typedef std::uniform_real_distribution<float> ufdist; ///< Alias for a small decimal uniform distribution
typedef std::uniform_int_distribution<uint> uudist; ///< Alias for an unsigned uniform integer distribution
typedef std::uniform_int_distribution<int> usdist; ///< Alias for a signed uniform integer distribution
typedef std::normal_distribution<double> ndist; ///< Alias for a decimal normal distribution
typedef truncated_normal_distribution<double> tndist; ///< Alias for a decimal truncated normal distribution
typedef std::bernoulli_distribution bdist;  ///< Alias for a coin flip distribution
typedef std::discrete_distribution<uint> rdist;   ///< Alias for a roulette distribution

// ===================================================================================

/******************************************************************************//**
 * @brief Class encapsulating random number generation.
 */
class AbstractDice {
public:
  using Seed_t = ulong;
  using BaseRNG_t = std::mt19937;

protected:
  struct RNG_t : public BaseRNG_t {
    RNG_t (void) : RNG_t(currentMilliTime()) {}
    RNG_t (Seed_t seed) : BaseRNG_t(seed), _seed(seed) {}

    virtual result_type operator() (void) {
      return BaseRNG_t::operator ()();
    }

    Seed_t getSeed (void) const {
      return _seed;
    }

    friend bool operator== (const RNG_t &lhs, const RNG_t &rhs) {
      return static_cast<const BaseRNG_t&>(lhs) == static_cast<const BaseRNG_t&>(rhs)
          && lhs._seed == rhs._seed;
    }

  private:
    Seed_t _seed;
  };

private:
  virtual RNG_t& getRNG (void) = 0;
  virtual const RNG_t& getRNG (void) const = 0;

public:
  virtual ~AbstractDice (void) {}

  virtual void reset (Seed_t newSeed) = 0;

  /// @return The value used to seed this object
  Seed_t getSeed(void) const {
    return getRNG().getSeed();
  }

  /// @return A random number following the provided distribution
  template<typename DIST>
#if __cplusplus <= 201402L
  auto operator() (DIST d, typename std::enable_if_t<!std::is_same<std::result_of_t<DIST(RNG_t&)>, void>::value, int> = 0) {
#else
  auto operator() (DIST d, typename std::enable_if_t<std::is_invocable<DIST, RNG_t&>::value, int> = 0) {
#endif
    return d(getRNG());
  }

  /// @return A random integer in the provided range [lower, upper]
  /// @throws std::invalid_argument if !(lower <= upper)
  template <typename I>
  typename std::enable_if<std::is_integral<I>::value, I>::type
  operator() (I lower, I upper) {
    if (lower > upper)
      throw std::invalid_argument("Cannot operator() a dice with lower > upper");
    return operator()(std::uniform_int_distribution<I>(lower, upper));
  }

  /// @return A random decimal in the provided range [lower, upper[
  /// @throws std::invalid_argument if !(lower < upper)
  template <typename F>
  typename std::enable_if<std::is_floating_point<F>::value, F>::type
  operator() (F lower, F upper) {
    if (lower > upper)
      throw std::invalid_argument("Cannot operator() a dice with lower >= upper");
    if (lower == upper) return lower;
    return operator()(std::uniform_real_distribution<F>(lower, upper));
  }

  /// @return A random boolean following the provided coin toss probability
  bool operator() (double heads) {
    return operator()(bdist(heads));
  }

  /// @return An iterator to a (uniformly) random position in the container
  /// @attention The container MUST NOT be empty
  template <typename CONTAINER>
  auto operator() (CONTAINER &c) -> decltype(c.begin()) {
    typedef typename std::iterator_traits<decltype(c.begin())>::iterator_category category;
    if (c.empty())
      throw std::invalid_argument("Cannot pick a random value from an empty container");
    return operator()(c.begin(), c.size(), category());
  }

  /// @return An iterator to a (uniformly) random position between 'begin' and 'end'
  /// @attention The distance between these iterators must be STRICTLY positive
  template <typename IT, typename CAT = typename std::iterator_traits<IT>::iterator_category>
  IT operator() (IT begin, IT end) {
    auto dist = std::distance(begin, end);
    if (dist == 0)
      throw std::invalid_argument("Cannot pick a random value from an empty iterator range");
    return operator()(begin, dist, CAT());
  }

  template <typename T>
  T toss (const T &v1, const T &v2) {
    return operator()(.5) ? v1 : v2;
  }

  template <typename CONTAINER>
  void shuffle(CONTAINER &c) {
    using std::swap;
    for (size_t i=0; i<c.size(); i++) {
      size_t j = operator()(size_t(0), i);
      swap(c[i], c[j]);
    }
  }

  /// @return A value T taken at a random position in the map according to discrete distribution
  /// described in its values
  template <typename T>
  T pickOne (const std::map<T, float> &map) {
    std::vector<T> keys;
    std::vector<float> values;

    keys.reserve(map.size());
    values.reserve(map.size());
    for (auto &p: map) {
      keys.push_back(p.first);
      values.push_back(p.second);
    }

    return keys.at(operator()(rdist(values.begin(), values.end())));
  }

  /// @return A random unit vector
  template <typename T>
  void randomUnitVector (T *t) {
    double cosphi = operator()(-1., 1.);
    double sinphi = sqrt(1 - cosphi*cosphi);
    double theta = operator()(0., 2.*M_PI);

    t[0] = sinphi * cos(theta);
    t[1] = sinphi * sin(theta);
    t[2] = cosphi;
  }


  static ulong currentMilliTime (void) {
    return std::chrono::duration_cast<std::chrono::milliseconds >(
          std::chrono::high_resolution_clock::now().time_since_epoch()
          ).count();
  }

  friend std::ostream& operator<< (std::ostream &os, const AbstractDice &dice) {
    return os << "D" << dice.getSeed();
  }

  friend std::istream& operator>> (std::istream &is, AbstractDice &dice) {
    char D;
    Seed_t seed;
    is >> D >> seed;
    if (is) dice.reset(seed);
    return is;
  }

private:
  template <typename IT, typename D = typename std::iterator_traits<IT>::difference_type>
  IT operator() (IT begin, D dist, std::random_access_iterator_tag) {
    return begin + operator()(D(0), dist-1);
  }

  template <typename IT, typename D = typename std::iterator_traits<IT>::difference_type>
  IT operator() (IT begin, D dist, std::forward_iterator_tag) {
    auto it = begin;
    std::advance(it, operator()(D(0), dist-1));
    return it;
  }
};

class FastDice : public AbstractDice {
  RNG_t _rng;

  RNG_t& getRNG (void) override { return _rng; }
  const RNG_t& getRNG (void) const override { return _rng; }

public:
  FastDice (void) {}
  FastDice (Seed_t seed) : _rng(seed) {}

  FastDice (const FastDice &other) : FastDice(other.getSeed()) {}
  FastDice operator= (const FastDice &that) {
    if (this != &that)  _rng = RNG_t(that.getSeed());
    return *this;
  }

  void reset (Seed_t newSeed) override {
    _rng = RNG_t(newSeed);
  }

  friend bool operator== (const FastDice &lhs, const FastDice &rhs) {
    return lhs._rng == rhs._rng;
  }
};

class AtomicDice : public AbstractDice {
  struct AtomicMT : public AbstractDice::RNG_t {
    using Base = AbstractDice::RNG_t;
    using lock = std::unique_lock<std::mutex>;
    mutable std::mutex mtx;

    AtomicMT(void) {}
    AtomicMT(AbstractDice::Seed_t seed) : Base(seed) {}

    AtomicMT (AtomicMT &&that) {
      lock l (that.mtx);
      swap(*this, that);
    }

    AtomicMT& operator= (AtomicMT that) {
      if (this != &that) { // prevent self lock
        lock lthis (mtx, std::defer_lock), lthat(that.mtx, std::defer_lock);
        std::lock(lthis, lthat);
        swap(*this, that);
      }
      return *this;
    }

    AtomicMT (const AtomicMT &that) : Base() {
      lock l (that.mtx);
      Base::operator=(that);
    }

    AbstractDice::RNG_t::result_type operator() (void) {
      lock l(mtx);
      return AbstractDice::RNG_t::operator ()();
    }

  private:
    void swap (AtomicMT &first, AtomicMT &second) {
      using std::swap;
      swap(static_cast<Base&>(first), static_cast<Base&>(second));
    }

  };

  AtomicMT _rng;

  RNG_t& getRNG (void) override { return _rng; }
  const RNG_t& getRNG (void) const override { return _rng; }

public:
  AtomicDice (void) {}
  AtomicDice (Seed_t seed) : _rng(seed) {}

  void reset (Seed_t newSeed) override {
    _rng = AtomicMT(newSeed);
  }

  /// \brief AtomicDice cannot be duplicated: equality is not possible
  friend bool operator== (const AtomicDice &, const AtomicDice &) {
    return false;
  }
};

} // end of namespace rng

#endif // _DICE_HPP_
