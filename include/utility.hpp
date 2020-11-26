#ifndef CS120_UTILITY_HPP
#define CS120_UTILITY_HPP


#include <cstdio>
#include <cstddef>
#include <type_traits>
#include <cstring>
#include <cstdlib>


namespace cs120 {
#define cs120_static_inline static inline __attribute__((always_inline))
#define cs120_unused __attribute__((unused))
#define cs120_no_return __attribute__((noreturn))
#if defined(__DEBUG__)
#define cs120_inline
#else
#define cs120_inline inline __attribute__((__always_inline__))
#endif

cs120_static_inline void _warn(const char *file, int line, const char *msg) {
    fprintf(stderr, "Warn at file %s, line %d: %s\n", file, line, msg);
}

#define cs120_warn(msg) cs120::_warn(__FILE__, __LINE__, msg)

cs120_static_inline cs120_no_return void _abort(const char *file, int line, const char *msg) {
    fprintf(stderr, "Abort at file %s, line %d: %s\n", file, line, msg);
    abort();
}

#define cs120_abort(msg) cs120::_abort(__FILE__, __LINE__, msg)

cs120_static_inline cs120_no_return void _unreachable(const char *file, int line, const char *msg) {
    fprintf(stderr, "Unreachable at file %s, line %d: %s\n", file, line, msg);
    abort();
}

#define cs120_unreachable(msg) cs120::_unreachable(__FILE__, __LINE__, msg)


template<typename T, size_t end, size_t begin>
struct bits_mask {
private:
    using RetT = typename std::enable_if<(std::is_unsigned<T>::value && sizeof(T) * 8 >= end &&
                                          end > begin), T>::type;

public:
    static constexpr RetT val =
            ((static_cast<T>(1u) << (end - begin)) - static_cast<T>(1u)) << begin;
};

template<typename T, size_t begin>
struct bit_mask {
public:
    static constexpr T val = bits_mask<T, begin + 1, begin>::val;
};

template<typename T, size_t end, size_t begin, ssize_t offset = 0, bool flag = (begin > offset)>
struct _get_bits;

template<typename T, size_t end, size_t begin, ssize_t offset>
struct _get_bits<T, end, begin, offset, true> {
public:
    static constexpr T inner(T val) {
        return (val >> (begin - offset)) & bits_mask<T, end - begin, 0>::val << offset;
    }
};

template<typename T, size_t end, size_t begin, ssize_t offset>
struct _get_bits<T, end, begin, offset, false> {
public:
    static constexpr T inner(T val) {
        return (val << (offset - begin)) & bits_mask<T, end - begin, 0>::val << offset;
    }
};

template<typename T, size_t end, size_t begin, ssize_t offset = 0>
constexpr inline T get_bits(T val) {
    static_assert(sizeof(T) * 8 >= end, "end exceed length");
    static_assert(end > begin, "end need to be bigger than start");
    static_assert(sizeof(T) * 8 >= end - begin + offset, "result exceed length");

    return _get_bits<T, end, begin, offset>::inner(val);
}

template<typename T, size_t begin, ssize_t offset = 0>
constexpr inline T get_bit(T val) { return get_bits<T, begin + 1, begin, offset>(val); }

template<typename T>
T divide_ceil(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "this function is only for unsigned!");

    return a == 0 ? 0 : (a - 1) / b + 1;
}


struct Range {
    size_t start;
    size_t end;
};


template<typename T>
class MutSlice;

template<typename T>
class Slice;

template<typename T>
class Array {
private:
    T *inner;
    size_t size_;

public:
    Array() : inner{nullptr}, size_{0} {}

    explicit Array(size_t size) : inner{new T[size]{}}, size_{size} {}

    Array(Array &&other) noexcept: inner{other.inner}, size_{other.size_} {
        other.inner = nullptr;
    }

    Array &operator=(Array &&other) noexcept {
        if (this != &other) {
            delete[] this->inner;
            this->inner = other.inner;
            this->size_ = other.size_;
            other.inner = nullptr;
        }

        return *this;
    }

    Array copy() const {
        Array other{this->size_};

        for (size_t i = 0; i < this->size_; ++i) {
            other.begin()[i] = this->begin()[i];
        }

        return other;
    }

    Array shallow_copy() const {
        Array other{};
        other.inner = new T[size_];
        other.size_ = size_;

        memcpy(other.inner, this->inner, size_ * sizeof(T));

        return other;
    }

    T &operator[](size_t index) {
        if (index >= size_) { cs120_abort("index out of boundary!"); }

        return inner[index];
    }

    const T &operator[](size_t index) const {
        if (index >= size_) { cs120_abort("index out of boundary!"); }

        return inner[index];
    }

    MutSlice<T> operator[](Range range) {
        if (range.start > range.end || range.end > size_) {
            cs120_abort("index out of boundary!");
        }

        return MutSlice<T>{inner + range.start, range.end - range.start};
    }

    Slice<T> operator[](Range range) const {
        if (range.start > range.end || range.end > size_) {
            cs120_abort("index out of boundary!");
        }

        return Slice<T>{inner + range.start, range.end - range.start};
    }

    size_t size() const { return size_; }

    bool empty() const { return size() == 0; }

    T *begin() { return inner; }

    T *end() { return inner + size_; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~Array() { delete[] inner; }
};

template<typename T>
class MutSlice {
private:
    T *inner;
    size_t size_;

public:
    MutSlice(T *inner, size_t size_) : inner{inner}, size_{size_} {}

    Array<T> copy() const {
        Array<T> other{this->size_};

        for (size_t i = 0; i < this->size_; ++i) {
            other.begin()[i] = this->begin()[i];
        }

        return other;
    }

    Array<T> shallow_copy() const {
        Array<T> other{};
        other.inner = new T[size_];
        other.size_ = size_;

        memcpy(other.inner, this->inner, size_ * sizeof(T));

        return other;
    }

    T &operator[](size_t index) {
        if (index >= size_) { cs120_abort("index out of boundary!"); }

        return inner[index];
    }

    const T &operator[](size_t index) const {
        if (index >= size_) { cs120_abort("index out of boundary!"); }

        return inner[index];
    }

    MutSlice<T> operator[](Range range) {
        if (range.start > range.end || range.end > size_) {
            cs120_abort("index out of boundary!");
        }

        return MutSlice<T>{inner + range.start, range.end - range.start};
    }

    Slice<T> operator[](Range range) const {
        if (range.start > range.end || range.end > size_) {
            cs120_abort("index out of boundary!");
        }

        return Slice<T>{inner + range.start, range.end - range.start};
    }

    size_t size() const { return size_; }

    bool empty() const { return size() == 0; }

    T *begin() { return inner; }

    T *end() { return inner + size_; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~MutSlice() = default;
};

template<typename T>
class Slice {
private:
    const T *inner;
    size_t size_;

public:
    Slice(MutSlice<T> other) : inner{other.begin()}, size_{other.size()} {}

    Slice(const T *inner, size_t size_) : inner{inner}, size_{size_} {}

    Array<T> copy() const {
        Array<T> other{this->size_};

        for (size_t i = 0; i < this->size_; ++i) {
            other.begin()[i] = this->begin()[i];
        }

        return other;
    }

    Array<T> shallow_copy() const {
        Array <T> other{};
        other.inner = new T[size_];
        other.size_ = size_;

        memcpy(other.inner, this->inner, size_ * sizeof(T));

        return other;
    }

    const T &operator[](size_t index) const {
        if (index >= size_) { cs120_abort("index out of boundary!"); }

        return inner[index];
    }

    Slice<T> operator[](Range range) const {
        if (range.start > range.end || range.end > size_) {
            cs120_abort("index out of boundary!");
        }

        return Slice<T>{inner + range.start, range.end - range.start};
    }

    size_t size() const { return size_; }

    bool empty() const { return size() == 0; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~Slice() = default;
};

}


#endif //CS120_UTILITY_HPP
