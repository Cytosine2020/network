#ifndef CS120_UTILITY_HPP
#define CS120_UTILITY_HPP


#include <cstdio>
#include <cstddef>
#include <type_traits>
#include <cstring>
#include <cstdlib>
#include <cstdint>


namespace cs120 {
#define cs120_static_inline static inline __attribute__((always_inline))
#define cs120_unused __attribute__((unused))
#define cs120_no_return __attribute__((noreturn))
#if defined(__DEBUG__)
#define cs120_inline inline
#else
#define cs120_inline inline __attribute__((always_inline))
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
    size_t begin_;
    size_t end_;

    Range() : begin_{0}, end_{0} {}

    explicit Range(size_t start) : begin_{start}, end_{0} {}

    Range(size_t start, size_t end) : begin_{start}, end_{end} {}

    size_t begin() const { return begin_; }

    size_t end() const { return end_; }
};


template<typename T>
class Array;

template<typename T>
class Slice;

template<typename T>
class MutSlice;


template<typename SubT, typename T>
struct IndexTrait {
private:
    const SubT *sub_type() const { return reinterpret_cast<const SubT *>(this); }

public:
    const T &operator[](size_t index) const {
        if (index >= sub_type()->size()) { cs120_abort("index out of boundary!"); }

        return sub_type()->begin()[index];
    }

    Slice<T> operator[](Range range) const {
        if (range.end() == 0) { range.end_ = sub_type()->size(); }

        if (range.begin() > range.end() || range.end() > sub_type()->size()) {
            cs120_abort("index out of boundary!");
        }

        return Slice<T>{sub_type()->begin() + range.begin(), range.end() - range.begin()};
    }
};

template<typename SubT, typename T>
struct MutIndexTrait : public IndexTrait<SubT, T> {
private:
    SubT *sub_type() { return reinterpret_cast<SubT *>(this); }

public:
    T &operator[](size_t index) {
        if (index >= sub_type()->size()) { cs120_abort("index out of boundary!"); }

        return sub_type()->begin()[index];
    }

    MutSlice<T> operator[](Range range) {
        if (range.end() == 0) { range.end_ = sub_type()->size(); }

        if (range.begin() > range.end() || range.end() > sub_type()->size()) {
            cs120_abort("index out of boundary!");
        }

        return MutSlice<T>{sub_type()->begin() + range.begin(), range.end() - range.begin()};
    }
};

template<typename SubT, typename T>
struct SliceTrait {
private:
    const SubT *sub_type() const { return reinterpret_cast<const SubT *>(this); }

public:
    Array<T> clone() const {
        Array other{this->size_};

        for (size_t i = 0; i < this->size_; ++i) {
            other.begin()[i] = this->begin()[i];
        }

        return other;
    }

    Array<T> shallow_clone() const {
        Array<T> other{};
        other.inner = new T[sub_type()->size()];
        other.size_ = sub_type()->size();

        memcpy(other.inner, this->inner, sub_type()->size() * sizeof(T));

        return other;
    }

    bool empty() const { return sub_type()->size() == 0; }
};

template<typename SubT, typename T>
struct MutSliceTrait : SliceTrait<SubT, T> {
private:
    SubT *sub_type() { return reinterpret_cast<SubT *>(this); }

public:
    void copy_from_slice(Slice<T> other) {
        if (sub_type()->size() != other.size()) { cs120_abort("slice size does not match!"); }

        for (size_t i = 0; i < sub_type()->size(); ++i) {
            sub_type()->begin()[i] = other.begin()[i];
        }
    }

    void shallow_copy_from_slice(Slice<T> other) {
        if (sub_type()->size() != other.size()) { cs120_abort("slice size does not match!"); }

        memcpy(sub_type()->begin(), other.begin(), sub_type()->size() * sizeof(T));
    }
};

template<typename SubT>
struct BufferTrait {
private:
    const SubT *sub_type() const { return reinterpret_cast<const SubT *>(this); }

public:
    template<typename T>
    const T *buffer_cast() const {
        if (!std::is_same<typename SubT::Item, uint8_t>::value) { return nullptr; }
        if (sub_type()->size() < sizeof(T)) { return nullptr; }
        return reinterpret_cast<const T *>(sub_type()->begin());
    }
};

template<typename SubT>
struct MutBufferTrait : public BufferTrait<SubT> {
private:
    SubT *sub_type() { return reinterpret_cast<SubT *>(this); }

public:
    template<typename T>
    T *buffer_cast() {
        if (!std::is_same<typename SubT::Item, uint8_t>::value) { return nullptr; }
        if (sub_type()->size() < sizeof(T)) { return nullptr; }
        else { return reinterpret_cast<T *>(sub_type()->begin()); }
    }
};


template<typename T>
class Array :
        public MutIndexTrait<Array<T>, T>,
        public MutSliceTrait<Array<T>, T>,
        public MutBufferTrait<Array<T>> {
private:
    T *inner;
    size_t size_;

public:
    using Item = T;

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

    size_t size() const { return size_; }

    T *begin() { return inner; }

    T *end() { return inner + size_; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~Array() { delete[] inner; }
};

template<typename T>
class MutSlice :
        public MutIndexTrait<Array<T>, T>,
        public MutSliceTrait<Array<T>, T>,
        public MutBufferTrait<Array<T>> {
private:
    T *inner;
    size_t size_;

public:
    using Item = T;

    MutSlice() : inner{nullptr}, size_{0} {}

    MutSlice(Array<T> &other) : inner{other.begin()}, size_{other.size()} {}

    MutSlice(T *inner, size_t size_) : inner{inner}, size_{size_} {}

    size_t size() const { return size_; }

    T *begin() { return inner; }

    T *end() { return inner + size_; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~MutSlice() = default;
};

template<typename T>
class Slice :
        public IndexTrait<Array<T>, T>,
        public SliceTrait<Array<T>, T>,
        public BufferTrait<Array<T>> {
private:
    const T *inner;
    size_t size_;

public:
    using Item = T;

    Slice() : inner{nullptr}, size_{0} {}

    Slice(const Array<T> &other) : inner{other.begin()}, size_{other.size()} {}

    Slice(MutSlice<T> other) : inner{other.begin()}, size_{other.size()} {}

    Slice(const T *inner, size_t size_) : inner{inner}, size_{size_} {}

    size_t size() const { return size_; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~Slice() = default;
};
}


#endif //CS120_UTILITY_HPP
