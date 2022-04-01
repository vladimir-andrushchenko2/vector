#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory(const RawMemory&) = delete;

    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory() = default;

    explicit RawMemory(size_t capacity) : buffer_(Allocate(capacity)), capacity_(capacity) {}

    RawMemory(RawMemory&& other) noexcept { Swap(other); }

    RawMemory& operator=(RawMemory&& other) noexcept {
        Swap(other);
        return *this;
    }

    ~RawMemory() { Deallocate(buffer_); }

    T* operator+(size_t offset) noexcept { return buffer_ + offset; }

    const T* operator+(size_t offset) const noexcept { return buffer_ + offset; }

    T& operator[](size_t index) noexcept { return buffer_[index]; }

    const T& operator[](size_t index) const noexcept { return buffer_[index]; }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept { return buffer_; }

    T* GetAddress() noexcept { return buffer_; }

    size_t Capacity() const { return capacity_; }

private:
    static T* Allocate(size_t n) { return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr; }

    static void Deallocate(T* buf) noexcept { operator delete(buf); }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other) : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept { Swap(other); }

    Vector& operator=(const Vector& other) {
        if (other.size_ > data_.Capacity()) {
            Vector temp(other);
            Swap(temp);

        } else {
            std::copy_n(other.data_.GetAddress(), std::min(Size(), other.Size()), data_.GetAddress());

            if (Size() > other.Size()) {
                std::destroy_n(data_ + other.Size(), Size() - other.Size());
            } else if (Size() < other.Size()) {
                std::uninitialized_copy_n(other.data_.GetAddress() + Size(), other.Size() - Size(),
                                          data_.GetAddress() + Size());
            }
        }

        size_ = other.Size();

        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    ~Vector() { std::destroy_n(data_.GetAddress(), size_); };

    iterator begin() noexcept { return data_.GetAddress(); };

    iterator end() noexcept { return data_.GetAddress() + size_; };

    const_iterator begin() const noexcept { return data_.GetAddress(); };

    const_iterator end() const noexcept { return data_.GetAddress() + size_; };

    const_iterator cbegin() const noexcept { return data_.GetAddress(); };

    const_iterator cend() const noexcept { return data_.GetAddress() + size_; };

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        MoveDataIfPossibleOrCopyInstead(data_.GetAddress(), Size(), new_data.GetAddress());

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        Reserve(new_size);

        if (new_size < size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        } else if (new_size > size_) {
            std::uninitialized_default_construct_n(data_ + size_, new_size - size_);
        }

        size_ = new_size;
    }

    template <typename U>
    void PushBack(U&& value) {
        EmplaceBack(std::forward<U>(value));
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        --size_;
        std::destroy_at(data_ + size_);
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == data_.Capacity()) {
            size_t temp_size = (size_ == 0) ? 1 : size_ * 2;

            RawMemory<T> new_data(temp_size);

            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);

            MoveDataIfPossibleOrCopyInstead(data_.GetAddress(), Size(), new_data.GetAddress());

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);

        } else {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }

        ++size_;

        return data_[size_ - 1];
    }

    size_t Size() const noexcept { return size_; }

    size_t Capacity() const noexcept { return data_.Capacity(); }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    iterator Insert(const_iterator pos, const T& value) { return Emplace(pos, value); }

    iterator Insert(const_iterator pos, T&& value) { return Emplace(pos, std::move(value)); }

    template <typename InputIt, typename OutputIt>
    void MoveDataIfPossibleOrCopyInstead(InputIt input_it, size_t n, OutputIt output_it) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(input_it, n, output_it);
        } else {
            std::uninitialized_copy_n(input_it, n, output_it);
        }
    }

    template <typename... Args>
    iterator InsertWithRealloc(iterator pos, Args&&... args) {
        size_t dist = std::distance(begin(), pos);

        size_t temp_size = (size_ == 0) ? 1 : size_ * 2;
        RawMemory<T> new_data(temp_size);

        try {
            new (new_data.GetAddress() + dist) T(std::forward<Args>(args)...);

            MoveDataIfPossibleOrCopyInstead(data_.GetAddress(), dist, new_data.GetAddress());
            MoveDataIfPossibleOrCopyInstead(data_.GetAddress() + dist, size_ - dist, new_data.GetAddress() + dist + 1);

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
        } catch (...) {
            std::destroy_at(std::addressof(new_data));
        }

        return &data_[dist];
    }

    template <typename... Args>
    iterator InsertWithoutRealloc(iterator pos, Args&&... args) {
        size_t dist = std::distance(begin(), pos);

        iterator it = &data_[dist];

        if (pos == nullptr || pos == end()) {
            new (end()) T(std::forward<Args>(args)...);
        } else {
            new (end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(it, end() - 1, end());
            *it = T(std::forward<Args>(args)...);
        }

        ++size_;

        return it;
    }

    template <typename... Args>
    iterator Emplace(const_iterator p, Args&&... args) {
        iterator pos = const_cast<iterator>(p);

        if (size_ == Capacity()) {
            return InsertWithRealloc(pos, std::forward<Args>(args)...);
        } else {
            return InsertWithoutRealloc(pos, std::forward<Args>(args)...);
        }
    }

    iterator Erase(const_iterator p) {
        iterator pos = const_cast<iterator>(p);

        if (begin() == end()) {
            return pos;
        }

        std::move(pos + 1, end(), pos);

        PopBack();

        return pos;
    }

    const T& operator[](size_t index) const noexcept { return data_[index]; }

    T& operator[](size_t index) noexcept { return data_[index]; }

private:
    static void DestroyN(T* buf, size_t n) noexcept {
        std::destroy_n(buf, n);
    }

    static void CopyConstruct(T* buf, const T& elem) { new (buf) T(elem); }

    static void Destroy(T* buf) noexcept { std::destroy_at(buf); }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
