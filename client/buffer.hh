#ifndef MUTATED_BUFFER_HH
#define MUTATED_BUFFER_HH

/**
 * buffer.hh - circular buffer implementation.
 */

#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <utility>

/**
 * A buffer iterator.
 *
 * TODO: Not safe, we'll go past the end if keep incrementing.
 */
template <typename T>
class buffer_iterator
  : public std::iterator<std::bidirectional_iterator_tag, T>
{
  public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = T const *;
    using reference = T &;

  private:
    pointer ptr_;
    const const_pointer span1_end_;
    const const_pointer span2_start_;

  public:
    buffer_iterator(void) noexcept : ptr_{nullptr},
                                     span1_end_{nullptr},
                                     span2_start_{nullptr}
    {
    }

    buffer_iterator(pointer p, const_pointer s1_end,
                    const_pointer s2_start) noexcept : ptr_{p},
                                                       span1_end_{s1_end},
                                                       span2_start_{s2_start}
    {
    }

    buffer_iterator(const buffer_iterator &bi) = default;
    buffer_iterator &operator=(const buffer_iterator &bi) = default;

    ~buffer_iterator(void) {}

    reference operator*(void) const { return *ptr_; }
    pointer operator->(void) const { return ptr_; }

    buffer_iterator &operator++(void) // pre-increment
    {
        if (++ptr_ == span1_end_) {
            ptr_ = const_cast<pointer>(span2_start_);
        }
        return *this;
    }

    buffer_iterator operator++(int) // post-increment
    {
        buffer_iterator tmp(*this);
        if (++ptr_ == span1_end_) {
            ptr_ = span2_start_;
        }
        return tmp;
    }

    buffer_iterator &operator--(void) // pre-decrement
    {
        if (ptr_ == span2_start_) {
            ptr_ = span1_end_;
        }
        ptr_--;
        return *this;
    }

    buffer_iterator operator--(int) // post-decrement
    {
        buffer_iterator tmp(*this);
        if (ptr_ == span2_start_) {
            ptr_ = span1_end_;
        }
        ptr_--;
        return tmp;
    }

    bool operator==(const buffer_iterator &bi) const
    {
        return ptr_ == bi.ptr_ && span1_end_ == bi.span1_end_ &&
               span2_start_ == bi.span2_start_;
    }

    bool operator!=(const buffer_iterator &bi) const { return !(*this == bi); }
};

/**
 * Circular buffer, only useful in a single-threaded fashion.
 */
template <typename T, std::size_t BUFSZ = 1024> class buffer
{
  public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = T const *;
    using reference = T &;
    using const_reference = const T &;
    using iterator = buffer_iterator<T>;
    using size_type = std::size_t;

  private:
    value_type buf_[BUFSZ]; // buffer
    pointer head_;          // at-current pointer
    pointer tail_;          // 1-past pointer
    size_type used_;

    const_pointer bufcap(void) const noexcept { return buf_ + BUFSZ; }

  public:
    /**
     * Construct a new buffer.
     */
    buffer(void) noexcept : head_{buf_}, tail_{buf_}, used_{0} {}

    /* Deconstruct a buffer. */
    ~buffer(void) noexcept {}

    /* Don't allow copy or move */
    buffer(const buffer &) = delete;
    buffer(buffer &&) = delete;
    buffer operator=(const buffer &) = delete;
    buffer operator=(buffer &&) = delete;

    // TODO: avail/items are not the clearest names

    /* Size returns the size of the buffer. */
    size_type size(void) const noexcept { return BUFSZ; }

    /* Avail returns the available space in the buffer. */
    size_type avail(void) const noexcept { return BUFSZ - used_; }

    /* Stored returns the amount of useful data stored in the buffer. */
    size_type items(void) const noexcept { return used_; }

    /**
     * Queue_prep prepares a queue operation by returning a pair of pointers
     * that satisfies the request len of the buffer. The length of the
     * array pointed to by the first element of the pair is returned by len.
     * @len: the space requested to queue items.
     * @return: a pair of pointers satisfying the request, the length of the
     * first array is returned by len, the second array will be null or the
     * remaining length.
     */
    std::pair<pointer, pointer> queue_prep(size_type &len)
    {
        if (len == 0) {
            throw std::invalid_argument("buffer::queue_prep: len = 0");
        } else if (used_ == BUFSZ) {
            throw std::system_error(ENOSPC, std::system_category(),
                                    "buffer::queue_prep: buffer full");
        } else if (len > avail()) {
            throw std::system_error(
              ENOSPC, std::system_category(),
              "buffer::queue_prep: not enough buffer space ");
        }

        pointer p1 = tail_;
        pointer p2 = nullptr;
        if (head_ <= tail_) {
            size_type avail = size_type(bufcap() - tail_);
            if (len >= avail) {
                len = avail;
                p2 = buf_;
            }
        }
        return std::make_pair(p1, p2);
    }

    /**
     * Queue_commit finalizes a previously prepare queue (queue_prep), marking
     * that amount of space as queued.
     * @len: the amount of space to regard as committed.
     */
    void queue_commit(const size_type len)
    {
        if (len == 0) {
            throw std::invalid_argument("buffer::queue_commit: len = 0");
        } else if (used_ == BUFSZ) {
            throw std::system_error(ENOSPC, std::system_category(),
                                    "buffer::queue_commit: buffer full");
        } else if (len > avail()) {
            throw std::system_error(
              ENOSPC, std::system_category(),
              "buffer::queue_commit: not enough buffer space ");
        }

        if (head_ <= tail_) {
            size_type end_free = size_type(bufcap() - tail_);
            if (len < end_free) {
                tail_ += len;
            } else {
                tail_ = buf_ + len - end_free;
            }
        } else {
            tail_ += len;
        }
        used_ += len;
    }

    /**
     * Queue returns a continguous array (segment) at most as large as the
     * requested length, but possibly smaller (if need to wrap).
     * @len: the requested length, and on return the actual length of the
     * segment.
     * @return: the contiguous array of length len.
     */
    pointer queue(size_type &len)
    {
        if (len > avail()) {
            len = avail();
        }
        auto ptrs = queue_prep(len);
        queue_commit(len);
        return ptrs.first;
    }

    /**
     * Peek returns a pair of pointers to satisfy the request for a array
     * segment of length len. Since we may need to wrap to return len array
     * items, we return a pair of pointers and set len on return to the size of
     * the array the first pointer refers to.
     * @len: the requested array size, and on return, the size of the first
     * array.
     * @return: a pair of arrays that together have len items. The second array
     * may be null if no wrapping is needed.
     */
    std::pair<pointer, pointer> peek(size_type &len)
    {
        if (len == 0) {
            throw std::invalid_argument("buffer::peek: len = 0");
        } else if (used_ == 0) { // C
            throw std::system_error(ENOSPC, std::system_category(),
                                    "buffer::peek: buffer empty");
        } else if (len > items()) {
            throw std::system_error(ENOSPC, std::system_category(),
                                    "buffer::peek: not enough items");
        }

        pointer p1 = head_;
        pointer p2 = nullptr;

        if (head_ >= tail_) { // B or D
            size_type end_used = bufcap() - head_;
            if (len > end_used) {
                len = end_used;
                p2 = buf_;
            }
        }
        return std::make_pair(p1, p2);
    }

    /**
     * Drop drops len items from the buffer.
     * @len: the number of items to drop.
     */
    void drop(const size_type len)
    {
        if (len == 0) {
            return;
        } else if (used_ == 0) { // C
            throw std::system_error(ENOSPC, std::system_category(),
                                    "buffer::drop: buffer empty");
        } else if (len > items()) {
            throw std::system_error(ENOSPC, std::system_category(),
                                    "buffer::drop: not enough items");
        }

        if (head_ >= tail_ and
            len >= size_type(bufcap() - head_)) { // B/D when wrapping
            head_ = buf_ + len - (bufcap() - head_);
        } else { // A or (B/D when not wrapping)
            head_ += len;
        }
        used_ -= len;
    }

    /**
     * Dequeue returns a pointer to a contiguous array as close to the
     * requested length as possible, and, frees that space for future queue
     * operations.
     * @len: the request array size, and on return, the size of the returned
     * array.
     * @return: a contiguous array of size len.
     */
    pointer dequeue(size_type &len)
    {
        if (len > used_) {
            len = used_;
        }
        auto ptrs = peek(len);
        drop(len);
        return ptrs.first;
    }

    iterator begin(void)
    {
        if (used_ == 0 or head_ < tail_) {
            return buffer_iterator<T>(head_, nullptr, nullptr);
        } else {
            return buffer_iterator<T>(head_, bufcap(), buf_);
        }
    }

    iterator end(void)
    {
        if (used_ == 0 or head_ < tail_) {
            return buffer_iterator<T>(tail_, nullptr, nullptr);
        } else {
            return buffer_iterator<T>(tail_, bufcap(), buf_);
        }
    }
};

using charbuf = buffer<char, 1024 * 1024>;

#endif /* MUTATED_BUFFER_HH */
