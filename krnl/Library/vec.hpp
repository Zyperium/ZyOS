#pragma once
#include <stdint.h>
#include <stddef.h>

namespace lib {
    template <typename T> struct RemoveReference { typedef T Type; };
    template <typename T> struct RemoveReference<T&> { typedef T Type; };
    template <typename T> struct RemoveReference<T&&> { typedef T Type; };

    template <typename T>
    constexpr typename RemoveReference<T>::Type &&move(T &&arg) noexcept {
        return static_cast<typename RemoveReference<T>::Type&&>(arg);
    }

    template <typename T>
    class vec {
    private:
        T *m_data = nullptr;
        size_t m_size = 0;
        size_t m_capacity = 0;

        static T *allocate_raw(size_t capacity) {
            if (capacity == 0) return nullptr;
            return static_cast<T *>(operator new[](capacity  *sizeof(T)));
        }

        static void free_raw(T *ptr) {
            if (ptr) {
                operator delete[](ptr);
            }
        }

    public:
        vec() = default;

        explicit vec(size_t initial_capacity) {
            reserve(initial_capacity);
        }

        ~vec() {
            clear();
            free_raw(m_data);
        }

        vec(const vec&) = delete;
        vec &operator=(const vec&) = delete;

        vec(vec &&other) noexcept 
            : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }

        vec &operator=(vec &&other) noexcept {
            if (this != &other) {
                clear();
                free_raw(m_data);

                m_data = other.m_data;
                m_size = other.m_size;
                m_capacity = other.m_capacity;

                other.m_data = nullptr;
                other.m_size = 0;
                other.m_capacity = 0;
            }
            return *this;
        }

        [[nodiscard]] bool reserve(size_t new_capacity) {
            if (new_capacity <= m_capacity) return true;

            T *new_buffer = allocate_raw(new_capacity);
            if (!new_buffer) {
                return false;
            }

            for (size_t i = 0; i < m_size; ++i) {
                ::new (static_cast<void *>(&new_buffer[i])) T(move(m_data[i]));
                m_data[i].~T();
            }

            free_raw(m_data);

            m_data = new_buffer;
            m_capacity = new_capacity;
            return true;
        }

        [[nodiscard]] bool push_back(const T &value) {
            if (m_size >= m_capacity) {
                size_t new_cap = (m_capacity == 0) ? 4 : m_capacity * 2;
                if (!reserve(new_cap)) return false;
            }

            ::new (static_cast<void*>(&m_data[m_size])) T(value);
            m_size++;
            return true;
        }

        [[nodiscard]] bool push_back(T &&value) {
            if (m_size >= m_capacity) {
                size_t new_cap = (m_capacity == 0) ? 4 : m_capacity * 2;
                if (!reserve(new_cap)) return false;
            }

            ::new (static_cast<void*>(&m_data[m_size])) T(move(value));
            m_size++;
            return true;
        }

        template <typename... Args>
        [[nodiscard]] bool emplace_back(Args&&... args) {
            if (m_size >= m_capacity) {
                size_t new_cap = (m_capacity == 0) ? 4 : m_capacity * 2;
                if (!reserve(new_cap)) return false;
            }

            ::new (static_cast<void*>(&m_data[m_size])) T(static_cast<Args&&>(args)...);
            m_size++;
            return true;
        }

        void pop_back() {
            if (m_size > 0) {
                m_size--;
                m_data[m_size].~T();
            }
        }

        void clear() {
            for (size_t i = 0; i < m_size; ++i) {
                m_data[i].~T();
            }
            m_size = 0;
        }

        T &operator[](size_t index) { return m_data[index]; }
        const T &operator[](size_t index) const { return m_data[index]; }

        T *data() { return m_data; }
        const T *data() const { return m_data; }
        size_t size() const { return m_size; }
        size_t capacity() const { return m_capacity; }
        bool empty() const { return m_size == 0; }
    };
}