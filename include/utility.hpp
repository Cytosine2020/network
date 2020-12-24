#ifndef CS120_UTILITY_HPP
#define CS120_UTILITY_HPP


#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <utility>
#include <type_traits>


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

cs120_static_inline const char *bool_to_string(bool value) { return value ? "true" : "false"; }

class Empty{};

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

template<typename T, size_t end, size_t begin, ssize_t offset = 0, bool = (begin > offset)>
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
constexpr T get_bits(T val) {
    static_assert(sizeof(T) * 8 >= end, "end exceed length");
    static_assert(end > begin, "end need to be bigger than start");
    static_assert(sizeof(T) * 8 >= end - begin + offset, "result exceed length");

    return _get_bits<T, end, begin, offset>::inner(val);
}

template<typename T, size_t begin, ssize_t offset = 0>
constexpr T get_bit(T val) { return get_bits<T, begin + 1, begin, offset>(val); }

template<typename T>
constexpr T divide_ceil(T a, T b) {
    static_assert(std::is_unsigned<T>::value, "this function is only for unsigned!");

    return a == 0 ? 0 : (a - 1) / b + 1;
}


struct Range {
private:
    size_t begin_;
    size_t end_;

public:
    Range() : begin_{0}, end_{0} {}

    explicit Range(size_t start) : begin_{start}, end_{0} {}

    Range(size_t start, size_t end) : begin_{start}, end_{end} {
        if (start == 0 && end == 0) {
            begin_ = 1;
            end_ = 1;
        }
    }

    size_t begin() const { return begin_; }

    size_t end() const { return end_; }
};


template<typename T>
class Array;

template<typename T>
class Slice;

template<typename T>
class MutSlice;

template<typename U, typename T, bool = std::is_pod<T>::value>
struct _clone;

template<typename U, typename T>
struct _clone<U, T, true> {
    static Array<T> inner(const U &self) {
        Array<T> other{};

        if (!self->empty()) {
            other.inner = new T[self->size()];
            other.size_ = self->size();
            memcpy(other.begin(), self.begin(), self->size() * sizeof(T));
        }

        return other;
    }
};

template<typename U, typename T>
struct _clone<U, T, false> {
    static Array<T> inner(const U &self) {
        if (self->empty()) { return Array<T>{}; }

        Array<T> other{self->size()};
        for (size_t i = 0; i < self->size(); ++i) { other.begin()[i] = self.begin()[i]; }
        return other;
    }
};

template<typename U, typename T, bool = std::is_pod<T>::value>
struct _copy;

template<typename U, typename T>
struct _copy<U, T, true> {
    static void inner(U &self, Slice<T> other) {
        if (!self.empty()) { memcpy(self.begin(), other.begin(), self.size() * sizeof(T)); }
    }
};

template<typename U, typename T>
struct _copy<U, T, false> {
    static void inner(const U &self, Slice<T> other) {
        for (size_t i = 0; i < self.size(); ++i) { self.begin()[i] = other.begin()[i]; }
    }
};

template<typename T, bool = std::is_pod<T>::value>
struct clear;

template<typename T>
struct clear<T, true> {
    static void inner(cs120_unused T &item) {}
};

template<typename T>
struct clear<T, false> {
    static void inner(T &item) {
        T take{};
        std::swap(item, take);
    }
};

template<typename SubT, typename T>
struct SliceTrait {
protected:
    const SubT *sub_type() const { return reinterpret_cast<const SubT *>(this); }

public:
    using Item = T;

    const T &operator[](size_t index) const {
        if (index >= sub_type()->size()) { cs120_abort("index out of boundary!"); }

        return sub_type()->begin()[index];
    }

    Slice<T> operator[](Range range) const {
        if (range.end() == 0) { range = Range{range.begin(), sub_type()->size()}; }
        if (range.begin() == range.end()) { return Slice<T>{}; }

        if (range.begin() > range.end() || range.end() > sub_type()->size()) {
            cs120_abort("index out of boundary!");
        }

        return Slice<T>{sub_type()->begin() + range.begin(), range.end() - range.begin()};
    }

    Array<T> clone() const { return _clone<SubT, T>::inner(*sub_type()); }

    bool empty() const { return sub_type()->size() == 0; }

    void format() const {
        size_t i = 0;

        for (auto &item: *sub_type()) {
            if (i % 32 == 0) { printf(&"\n%.4X: "[i == 0], static_cast<uint32_t>(i)); }
            printf("%.2X ", item);
            ++i;
        }

        printf("\n");
    }

    template<typename U>
    const U *buffer_cast() const {
        if (!std::is_same<typename SubT::Item, uint8_t>::value) { return nullptr; }
        if (sub_type()->size() < sizeof(U)) { return nullptr; }
        auto ptr = reinterpret_cast<size_t>(sub_type()->begin());
        if ((ptr & (std::alignment_of<U>::value - 1)) != 0) {
            cs120_abort("given ptr not aligned!");
        }
        return U::from_slice(*sub_type());
    }
};

template<typename SubT, typename T>
struct MutSliceTrait : public SliceTrait<SubT, T> {
protected:
    SubT *sub_type() { return reinterpret_cast<SubT *>(this); }

public:
    T &operator[](size_t index) {
        if (index >= sub_type()->size()) { cs120_abort("index out of boundary!"); }

        return sub_type()->begin()[index];
    }

    MutSlice<T> operator[](Range range) {
        if (range.end() == 0) { range = Range{range.begin(), sub_type()->size()}; }
        if (range.begin() == range.end()) { return MutSlice<T>{}; }

        if (range.begin() > range.end() || range.end() > sub_type()->size()) {
            cs120_abort("index out of boundary!");
        }

        return MutSlice<T>{sub_type()->begin() + range.begin(), range.end() - range.begin()};
    }

    void copy_from_slice(Slice<T> other) {
        if (sub_type()->size() != other.size()) { cs120_abort("slice size does not match!"); }
        _copy<SubT, T>::inner(*sub_type(), other);
    }

    template<typename U>
    U *buffer_cast() {
        if (!std::is_same<typename SubT::Item, uint8_t>::value) { return nullptr; }
        if (sub_type()->size() < sizeof(U)) { return nullptr; }
        auto ptr = reinterpret_cast<size_t>(sub_type()->begin());
        if ((ptr & (std::alignment_of<U>::value - 1)) != 0) {
            cs120_abort("given ptr not aligned!");
        }
        return U::from_slice((*sub_type())[Range{}]);
    }
};

template<typename T, size_t len>
class Buffer : public MutSliceTrait<Buffer<T, len>, T> {
private:
    T inner[len];

public:
    Buffer() : inner{} {}

    size_t size() const { return len; }

    T *begin() { return inner; }

    T *end() { return &inner[len]; }

    const T *begin() const { return inner; }

    const T *end() const { return &inner[len]; }

    ~Buffer() = default;
};

template<typename T>
class Array : public MutSliceTrait<Array<T>, T> {
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

    size_t size() const { return size_; }

    T *begin() { return inner; }

    T *end() { return inner + size_; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~Array() { delete[] inner; }
};

template<typename T>
class MutSlice : public MutSliceTrait<MutSlice<T>, T> {
private:
    T *inner;
    size_t size_;

public:
    MutSlice() : inner{nullptr}, size_{0} {}

    MutSlice(T *inner, size_t size_) : inner{inner}, size_{size_} {
        auto ptr = reinterpret_cast<size_t>(inner);
        if ((ptr & (std::alignment_of<T>::value - 1)) != 0) {
            cs120_abort("given ptr not aligned!");
        }
    }

    size_t size() const { return size_; }

    T *begin() { return inner; }

    T *end() { return inner + size_; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~MutSlice() = default;
};

template<typename T>
class Slice : public SliceTrait<Slice<T>, T> {
private:
    const T *inner;
    size_t size_;

public:
    Slice() : inner{nullptr}, size_{0} {}

    Slice(MutSlice<T> other) : inner{other.begin()}, size_{other.size()} {}

    Slice(const T *inner, size_t size_) : inner{inner}, size_{size_} {
        auto ptr = reinterpret_cast<size_t>(inner);
        if ((ptr & (std::alignment_of<T>::value - 1)) != 0) {
            cs120_abort("given ptr not aligned!");
        }
    }

    size_t size() const { return size_; }

    const T *begin() const { return inner; }

    const T *end() const { return inner + size_; }

    ~Slice() = default;
};
}


#endif //CS120_UTILITY_HPP
