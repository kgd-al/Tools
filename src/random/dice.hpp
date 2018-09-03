#ifndef _DICE_HPP_
#define _DICE_HPP_

/******************************************************************************//**
 * \file
 * \brief Contains the logic for random number generation.
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
 * \brief Naive truncated normal distribution.
 *
 * Converges towards a 0.5 efficiency given reasonably bad parameters
 * (e.g. mean=max and min << max-stddev, tested with 10e7 evaluations)
 */
template<typename FLOAT>
class truncated_normal_distribution : public std::normal_distribution<FLOAT> {
#ifndef NDEBUG
  /// The number of attempts made to generate a number before failing (Debug only)
  static constexpr size_t MAX_TRIES = 100;
#endif

public:
  /// Creates a normal distribution with the usual parameters mu and sigma constrained to the interval [min, max]
  truncated_normal_distribution(FLOAT mu, FLOAT stddev, FLOAT min, FLOAT max, bool nonZero=true)
    : std::normal_distribution<FLOAT>(mu,stddev), _min(min), _max(max), _nonZero(nonZero) {
#ifndef NDEBUG
    assert(min < max); // No inverted bounds
    assert(fabs(mu - min) >  2. * stddev || fabs(mu - max) > 2. * stddev); // At least 47.5% of values are valid
    // e-g: min = 0; mu = max = 1; if std < 1/6 then for x a rnd number P(x in [min,max]) > 0.4985
#endif
  }

  /// Requests production of a new random value
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

  /// Prints this distribution to the provided stream
  friend std::ostream& operator<< (std::ostream &os, const truncated_normal_distribution &d) {
    return os << d.mean() << " " << d.stddev() << " " << d._min << " " << d._max;
  }

private:
  FLOAT _min; ///< The lowest value that can be produced
  FLOAT _max; ///< The largest value that can be produced
  bool _nonZero;  ///< Is zero a forbidden value ?
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

// =================================================================================================
/// \brief Random number generation for a wide range of use-cases/distributions.
class AbstractDice {
public:

  /// The type used as a seed
  using Seed_t = ulong;

  /// The underlying random number generator
  using BaseRNG_t = std::mt19937;

protected:
  /// Random number generator enriched with its seed
  struct RNG_t : public BaseRNG_t {

    /// Create a rng using the current time as a seed
    RNG_t (void) : RNG_t(currentMilliTime()) {}

    /// Create a rng using the providing \p seed
    RNG_t (Seed_t seed) : BaseRNG_t(seed), _seed(seed) {}

    /// \return a new number from the underlying random number generator
    virtual result_type operator() (void) {
      return BaseRNG_t::operator ()();
    }

    /// \return the seed used for this rng
    Seed_t getSeed (void) const {
      return _seed;
    }

    /// Compare values based on their underlying operator== and their seed value
    friend bool operator== (const RNG_t &lhs, const RNG_t &rhs) {
      return static_cast<const BaseRNG_t&>(lhs) == static_cast<const BaseRNG_t&>(rhs)
          && lhs._seed == rhs._seed;
    }

  private:
    Seed_t _seed; ///< The seed used by this rng
  };

private:
  /// \return the random number generator used by this dice
  virtual RNG_t& getRNG (void) = 0;

  /// \return a const reference to the random number generator used by this dice
  virtual const RNG_t& getRNG (void) const = 0;

public:
  virtual ~AbstractDice (void) {}

  /// Resets this dice the a blank state starting with \p newSeed
  virtual void reset (Seed_t newSeed) = 0;

  /// \return The value used to seed this object
  Seed_t getSeed(void) const {
    return getRNG().getSeed();
  }

  /// \return A random number following the provided distribution
  /// \tparam A valid distribution \see https://en.cppreference.com/w/cpp/numeric/random
  template<typename DIST>
  auto operator() (DIST d, typename std::enable_if_t<std::is_invocable<DIST, RNG_t&>::value, int> = 0) {
    return d(getRNG());
  }

  /// \return A random integer in the provided range [lower, upper]
  /// \tparam I An integer type (int, uint, long, ...)
  /// \throws std::invalid_argument if lower > upper
  template <typename I>
  typename std::enable_if<std::is_integral<I>::value, I>::type
  operator() (I lower, I upper) {
    if (lower > upper)
      throw std::invalid_argument("Cannot operator() a dice with lower > upper");
    return operator()(std::uniform_int_distribution<I>(lower, upper));
  }

  /// \return A random decimal in the provided range [lower, upper[ if lower < upper.
  /// \tparam F A decimal type (float, double, ...)
  /// \throws std::invalid_argument if lower > upper
  template <typename F>
  typename std::enable_if<std::is_floating_point<F>::value, F>::type
  operator() (F lower, F upper) {
    if (lower > upper)
      throw std::invalid_argument("Cannot operator() a dice with lower >= upper");
    if (lower == upper) return lower;
    return operator()(std::uniform_real_distribution<F>(lower, upper));
  }

  /// \return A random boolean following the provided coin toss probability
  bool operator() (double heads) {
    return operator()(bdist(heads));
  }

  /// \return An iterator to a (uniformly) random position in the container
  /// \tparam CONTAINER A container with accessible member functions begin() and size()
  /// \attention The container MUST NOT be empty
  template <typename CONTAINER>
  auto operator() (CONTAINER &c) -> decltype(c.begin()) {
    typedef typename std::iterator_traits<decltype(c.begin())>::iterator_category category;
    if (c.empty())
      throw std::invalid_argument("Cannot pick a random value from an empty container");
    return operator()(c.begin(), c.size(), category());
  }

  /// \return An iterator to a (uniformly) random position between 'begin' and 'end'
  /// \tparam IT An iterator with at least ForwardIterator capabilities
  /// \see https://en.cppreference.com/w/cpp/named_req/ForwardIterator
  /// \attention The distance between these iterators must be STRICTLY positive
  template <typename IT, typename CAT = typename std::iterator_traits<IT>::iterator_category>
  IT operator() (IT begin, IT end) {
    auto dist = std::distance(begin, end);
    if (dist == 0)
      throw std::invalid_argument("Cannot pick a random value from an empty iterator range");
    return operator()(begin, dist, CAT());
  }

  /// \return Either \p v1 or \p v2 depending on the result of an uniform coin toss
  /// \tparam A copyable value
  template <typename T>
  T toss (const T &v1, const T &v2) {
    return operator()(.5) ? v1 : v2;
  }

  /// Suffles the contents of \p c via an in-place implementation of the fisher-yattes algorithm
  /// \see https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
  /// \tparam CONTAINER type with random access capabilities (e.g. vector)
  template <typename CONTAINER>
  void shuffle(CONTAINER &c) {
    using std::swap;
    for (size_t i=0; i<c.size(); i++) {
      size_t j = operator()(size_t(0), i);
      swap(c[i], c[j]);
    }
  }

  /// \return A value T taken at a random position in the map according to discrete distribution
  /// described in its values
  /// \tparam T a copyable type
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

  /// \return A random unit vector
  /// \tparam T a double compatible type (e.g. float loses some precision, int loses at lot)
  template <typename T>
  void randomUnitVector (T *t) {
    double cosphi = operator()(-1., 1.);
    double sinphi = sqrt(1 - cosphi*cosphi);
    double theta = operator()(0., 2.*M_PI);

    t[0] = sinphi * cos(theta);
    t[1] = sinphi * sin(theta);
    t[2] = cosphi;
  }

  /// \return The current time, as viewed by the system, in milli seconds.
  /// Used internally as the default seed for dices
  static ulong currentMilliTime (void) {
    return std::chrono::duration_cast<std::chrono::milliseconds >(
          std::chrono::high_resolution_clock::now().time_since_epoch()
          ).count();
  }

  /// Write current seed
  friend std::ostream& operator<< (std::ostream &os, const AbstractDice &dice) {
    return os << "D" << dice.getSeed();
  }

  /// Retrieve current seed
  friend std::istream& operator>> (std::istream &is, AbstractDice &dice) {
    char D;
    Seed_t seed;
    is >> D >> seed;
    if (is) dice.reset(seed);
    return is;
  }

private:
  /// \return An iterator it so that \p begin <= it < \p begin + \p dist
  /// \tparam IT A random access iterator
  template <typename IT, typename D = typename std::iterator_traits<IT>::difference_type>
  IT operator() (IT begin, D dist, std::random_access_iterator_tag) {
    return begin + operator()(D(0), dist-1);
  }

  /// \return An iterator it so that \p begin <= it < \p begin + \p dist
  /// \tparam IT An iterator with at least forward capabilities
  template <typename IT, typename D = typename std::iterator_traits<IT>::difference_type>
  IT operator() (IT begin, D dist, std::forward_iterator_tag) {
    auto it = begin;
    std::advance(it, operator()(D(0), dist-1));
    return it;
  }
};

/// A dice with focus on fast evaluation time.
/// \attention THREAD-UNSAFE
class FastDice : public AbstractDice {

  /// The underlying random number generator
  RNG_t _rng;

  RNG_t& getRNG (void) override { return _rng; }
  const RNG_t& getRNG (void) const override { return _rng; }

public:
  /// Builds a default dice with a default seed \see rng::AbstractDice::RNG_t()
  FastDice (void) {}

  /// Builds a dice starting at \p seed
  FastDice (Seed_t seed) : _rng(seed) {}

  /// Copy-build this dice based on \p other
  FastDice (const FastDice &other) : FastDice(other.getSeed()) {}

  /// Copy contents of \p other into this dice
  FastDice operator= (const FastDice &that) {
    if (this != &that)  _rng = RNG_t(that.getSeed());
    return *this;
  }

  void reset (Seed_t newSeed) override {
    _rng = RNG_t(newSeed);
  }

  /// Compare the underlying random number generator of both arguments
  friend bool operator== (const FastDice &lhs, const FastDice &rhs) {
    return lhs._rng == rhs._rng;
  }
};


/// A dice with focus on safety over speed
/// \attention THREAD-SAFE
class AtomicDice : public AbstractDice {

  /// Specialisation of the random number generator with mutex locking on access
  struct AtomicRNG_t : public AbstractDice::RNG_t {

    /// The base type
    using Base = AbstractDice::RNG_t;

    /// The lock type used
    using lock = std::unique_lock<std::mutex>;

    /// The mutex used for locking
    mutable std::mutex mtx;

    /// Use default seed \see rng::AbstractDice::RNG_t()
    AtomicRNG_t(void) {}

    /// Use \p seed
    AtomicRNG_t(AbstractDice::Seed_t seed) : Base(seed) {}

    /// Move contructible
    AtomicRNG_t (AtomicRNG_t &&that) {
      lock l (that.mtx);
      swap(*this, that);
    }

    /// Assignable
    AtomicRNG_t& operator= (AtomicRNG_t that) {
      if (this != &that) { // prevent self lock
        lock lthis (mtx, std::defer_lock), lthat(that.mtx, std::defer_lock);
        std::lock(lthis, lthat);
        swap(*this, that);
      }
      return *this;
    }

    /// Copy constructible
    AtomicRNG_t (const AtomicRNG_t &that) : Base() {
      lock l (that.mtx);
      Base::operator=(that);
    }

    /// Request an number from the underlying generator.
    /// Uses a mutex lock to ensure thread safety
    AbstractDice::RNG_t::result_type operator() (void) {
      lock l(mtx);
      return AbstractDice::RNG_t::operator ()();
    }

  private:
    /// Swap the contents of both arguments
    void swap (AtomicRNG_t &first, AtomicRNG_t &second) {
      using std::swap;
      swap(static_cast<Base&>(first), static_cast<Base&>(second));
    }

  };

  /// The underlying random number generator
  AtomicRNG_t _rng;

  RNG_t& getRNG (void) override { return _rng; }
  const RNG_t& getRNG (void) const override { return _rng; }

public:
  /// Default dice using default seed \see rng::AbstractDice::RNG_t()
  AtomicDice (void) {}

  /// Dice starting with \p seed
  AtomicDice (Seed_t seed) : _rng(seed) {}

  void reset (Seed_t newSeed) override {
    _rng = AtomicRNG_t(newSeed);
  }

  /// AtomicDice cannot be duplicated: equality is not possible thus always returns false
  friend constexpr bool operator== (const AtomicDice &, const AtomicDice &) {
    return false;
  }
};

} // end of namespace rng

#endif // _DICE_HPP_
