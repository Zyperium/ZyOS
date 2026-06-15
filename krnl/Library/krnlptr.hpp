#pragma once


namespace lib {
    template <typename T>
    class sptr {
    private:
        T *m_ptr{};

    public:
        sptr() = default;
        sptr(T *ptr) : m_ptr(ptr) {
            if (m_ptr) m_ptr->add_ref();
        }

        ~sptr() {
            if (m_ptr) m_ptr->release();
        }

        sptr(const sptr &other) : m_ptr(other.m_ptr) {
            if (m_ptr) m_ptr->add_ref();
        }

        sptr &operator=(const sptr &other) {
            if (this != &other) {
                if (m_ptr) m_ptr->release();
                m_ptr = other.m_ptr;
                if (m_ptr) m_ptr->add_ref();
            }
            return *this;
        }

        sptr(sptr &&other) noexcept : m_ptr(other.m_ptr) {
            other.m_ptr = nullptr;
        }

        sptr &operator=(sptr &&other) noexcept {
            if (this != &other) {
                if (m_ptr) m_ptr->release();
                m_ptr = other.m_ptr;
                other.m_ptr = nullptr;
            }
            return *this;
        }

        T *get() const { return m_ptr; }
        T *operator->() const { return m_ptr; }
        explicit operator bool() const { return m_ptr != nullptr; }
    };

    template <typename T>
    class uptr {
    private:
        T *m_ptr{nullptr};

    public:
        uptr() = default;
        explicit uptr(T *ptr) : m_ptr(ptr) {}

        ~uptr() {
            if (m_ptr) {
                delete m_ptr; 
            }
        }

        uptr(const uptr &other) = delete;
        uptr &operator=(const uptr &other) = delete;

        uptr(uptr &&other) noexcept : m_ptr(other.m_ptr) {
            other.m_ptr = nullptr;
        }

        uptr &operator=(uptr &&other) noexcept {
            if (this != &other) {
                if (m_ptr) delete m_ptr;
                m_ptr = other.m_ptr;
                other.m_ptr = nullptr;
            }
            return *this;
        }

        T *get() const { return m_ptr; }
        T *operator->() const { return m_ptr; }
    };
}