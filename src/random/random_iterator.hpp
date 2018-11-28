#ifndef _RANDOM_ITERATOR_HPP_
#define _RANDOM_ITERATOR_HPP_

/*!
 * Contains the definition for a random iterator with for-range compatibilities
 */

#include "dice.hpp"

namespace rng {

namespace _details {

/// \cond internal

/*!
 * \brief Randomly traverses the provided container.
 * Credit: http://anderberg.me/2016/07/04/c-custom-iterators/
 *
 * TODO Does not work with iterators (only full fledge containers)
 * TODO Remove internal vector (if possible)
 * TODO Separate iterators and for-range wrapper
 */
template <typename C>
struct RandomIteratorImpl {
  /// Helper alias to the iterator type of the provided
  using IT = decltype(std::declval<C&>().begin());

  /// Helper alias to the container's size type
  using size_type = typename C::size_type;

  RandomIteratorImpl(void) = default;

  /// Builds a random iterator from a container using the provided shuffler
  explicit RandomIteratorImpl(C &c, AbstractDice &dice) {
    make_shuffled_buffer(c, dice);
  }

  /// Builds an iterator pointing to the last value of container
  explicit RandomIteratorImpl(size_type end) {
    current = end;
  }

  /// Dereference current value
  auto& operator*(void) {
    return *buffer.at(current);
  }

  /// Dereference current value (const version)
  const auto& operator*(void) const {
    return *buffer.at(current);
  }

  /// Pre-increment
  RandomIteratorImpl& operator++(void) {
    ++current;
    return *this;
  }

  /// Post-increment
  RandomIteratorImpl operator++(int) {
    RandomIteratorImpl tmp = *this;
    ++current;
    return tmp;
  }

  /// Equality
  bool operator==(const RandomIteratorImpl& rhs) {
    return current == rhs.current;
  }

  /// Inequality
  bool operator!=(const RandomIteratorImpl& rhs) {
    return current != rhs.current;
  }

private:
  /*! Since we're not concerned with optimal performace, we'll simply
   * store the shuffled sequence in a vector.
   */
  using Buffer = std::vector<IT>;
  static_assert(std::is_same<size_type, typename Buffer::size_type>::value,
                "Provided container has a strange size type");

  /*! \brief Helper function, uses Fisher-Yattes modern inside-out algorithm */
  void make_shuffled_buffer (C &c, AbstractDice &dice) {
    if (c.empty())  return;

    buffer.reserve(c.size());

#if D_RANDOM_ITERATOR
#warning Debugging random iterator
    std::vector<uint> indices;
    indices.reserve(c.size());
#endif


    for (IT it=c.begin(); it!=c.end(); ++it) {
      size_type j = dice(size_type(0), buffer.size());
#if D_RANDOM_ITERATOR
      if (j == buffer.size())
        indices.push_back(buffer.size());
      else {
        indices.push_back(indices.at(j));
        indices.at(j) = buffer.size();
      }
#endif

      if (j == buffer.size())
        buffer.push_back(it);
      else {
        buffer.push_back(buffer.at(j));
        buffer.at(j) = it;
      }
    }

#if D_RANDOM_ITERATOR
    LOG_BLOCK(DEBUG, D_RANDOM_ITERATOR) {
      log() << "Suffled(" << c.size() << "): ";
      for (uint i: indices)   log() << i << " ";
      log() << Log::endl;
    }
#endif

    assert(c.size() == buffer.size());
  }

  /// Suffled collection (iterators pointing to the real container values)
  Buffer buffer;

  /// Current index in the shuffled collection
  size_type current { 0 };
};

/*! Allows for-range to randomly traverse a STL container */
template <typename C>
class RandomIterator {
  /// Helper alias to the underlying random iterator
  using IT = RandomIteratorImpl<C>;

  /// Reference to the randomly accessed container
  C &container;

  /// Reference to the shuffler object
  AbstractDice &dice;

public:
  /// \copydoc RandomIteratorImpl
  RandomIterator (C &c, AbstractDice &d) : container(c), dice(d) {}

  /// \returns Random iterator to first element in the permutation
  IT begin (void) {   return IT(container, dice);     }

  /// \returns Random iterator to one-past-last element in the permutation
  IT end (void) {     return IT(container.size());    }
};

/// \endcond

} // end of namespace _details

/// Helper method to generate a random iterator while auto-deducing the container type
template <typename C>
_details::RandomIterator<C> randomIterator(C &c, AbstractDice &d) {
  return _details::RandomIterator<C>(c,d);
}

} // end of namespace rng

#endif // _RANDOM_ITERATOR_HPP_
