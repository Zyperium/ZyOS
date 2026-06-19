#pragma once
#include <stddef.h>
#include <stdint.h>

namespace lib {
    class string {
    protected:
        static const size_t SSO_CAPACITY = 23;

    private:
        struct RemoteLayout {
            char* data;
            size_t size;
            size_t capacity;
        };

        struct LocalLayout {
            char data[SSO_CAPACITY + 1];
            unsigned char info;
        };

        union {
            RemoteLayout remote;
            LocalLayout local;
        };

        bool is_local() const;
    
    public:
        string();
        string(const char* s);
        ~string();

        string(const string& other);
        string(string&& other) noexcept;
        string& operator=(const string& other);
        string& operator=(const char *other);
        string& operator=(string&& other) noexcept;

        const char* c_str() const;
        char& operator[](size_t index);
        const char& operator[](size_t index) const;

        size_t length() const;
        bool empty() const;
        string substr(size_t pos, size_t count = -1) const;
        void clear();

        bool operator==(const char* other) const;
        bool operator==(const string& other) const;
        bool operator!=(const char* other) const;
        bool operator!=(const string& other) const;
        string& operator+=(char c);
        string& operator+=(const char* s);
        string& operator--();
        string operator--(int);
    };
}