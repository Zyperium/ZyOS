#include <Library/cystr.hpp>
#include <Library/string.h>

namespace lib {
    
    bool string::is_local() const {
        return (local.info <= SSO_CAPACITY);
    }

    string::string() {
        local.data[0] = '\0';
        local.info = 0;
    }

    string::string(const char *s) {
        if (!s) {
            local.data[0] = '\0';
            local.info = 0;
            return;
        }

        size_t len = strlen(s);

        if (len <= SSO_CAPACITY) {
            memcpy(local.data, s, len);
            local.data[len] = '\0';
            local.info = (unsigned char)len;
        } else {
            remote.data = new char[len + 1];
            memcpy(remote.data, s, len);
            remote.data[len] = '\0';
            remote.size = len;
            remote.capacity = len;
            local.info = 0xFF;
        }
    }

    string::~string() {
        if (!is_local()) delete[] remote.data;
    }

    string::string(const string& other) {
        if (other.is_local()) {
            memcpy(local.data, other.local.data, SSO_CAPACITY + 1);
            local.info = other.local.info;
        } else {
            size_t len = other.remote.size;
            remote.data = new char[len + 1];
            memcpy(remote.data, other.remote.data, len + 1);
            remote.size = len;
            remote.capacity = len;
            local.info = 0xFF;
        }
    }

    string::string(string&& other) noexcept {
        if (other.is_local()) {
            memcpy(local.data, other.local.data, SSO_CAPACITY + 1);
            local.info = other.local.info;
        } else {
            remote.data = other.remote.data;
            remote.size = other.remote.size;
            remote.capacity = other.remote.capacity;
            local.info = 0xFF;
        }

        other.local.data[0] = '\0';
        other.local.info = 0;
    }

    string& string::operator=(const string& other) {
        if (this == &other) return *this;

        if (!is_local()) delete[] remote.data;

        if (other.is_local()) {
            memcpy(local.data, other.local.data, SSO_CAPACITY + 1);
            local.info = other.local.info;
        } else {
            size_t len = other.remote.size;
            remote.data = new char[len + 1];
            memcpy(remote.data, other.remote.data, len + 1);
            remote.size = len;
            remote.capacity = len;
            local.info = 0xFF;
        }
        return *this;
    }

    string& string::operator=(const char* s) {
        if (!is_local()) {
            delete[] remote.data;
        }
    
        if (s == nullptr) {
            local.data[0] = '\0';
            local.info = 0;
            return *this;
        }
    
        size_t len = strlen(s);
    
        if (len <= SSO_CAPACITY) {
            memcpy(local.data, s, len);
            local.data[len] = '\0';
            local.info = (unsigned char)len;
        } else {
            remote.data = new char[len + 1];
            memcpy(remote.data, s, len);
            remote.data[len] = '\0';
            remote.size = len;
            remote.capacity = len;
            local.info = 0xFF;
        }
    
        return *this;
    }

    string& string::operator--() {
        size_t len = length();
        
        if (len == 0) return *this;

        size_t new_len = len - 1;

        if (is_local()) {
            local.data[new_len] = '\0';
            local.info = static_cast<unsigned char>(new_len);
        } else {
            remote.data[new_len] = '\0';
            remote.size = new_len;
        }

        return *this;
    }

    string string::operator--(int) {
        string duplicate(*this);
        --(*this);
        return duplicate;
    }

    string& string::operator+=(char c) {
        size_t len = length();
        size_t new_len = len + 1;

        if (is_local()) {
            if (new_len <= SSO_CAPACITY) {
                local.data[len] = c;
                local.data[new_len] = '\0';
                local.info = static_cast<unsigned char>(new_len);
            } else {
                size_t new_cap = new_len * 2;
                char* new_data = new char[new_cap + 1];

                memcpy(new_data, local.data, len);
                new_data[len] = c;
                new_data[new_len] = '\0';

                remote.data = new_data;
                remote.size = new_len;
                remote.capacity = new_cap;
                local.info = 0xFF;
            }
        } else {
            if (new_len <= remote.capacity) {
                remote.data[len] = c;
                remote.data[new_len] = '\0';
                remote.size = new_len;
            } else {
                size_t new_cap = remote.capacity * 2;
                char* new_data = new char[new_cap + 1];

                memcpy(new_data, remote.data, len);
                new_data[len] = c;
                new_data[new_len] = '\0';

                delete[] remote.data;
                remote.data = new_data;
                remote.size = new_len;
                remote.capacity = new_cap;
            }
        }

        return *this;
    }

    string& string::operator+=(const char* s) {
        if (!s) return *this;
    
        for (size_t i = 0; s[i] != '\0'; ++i) {
            *this += s[i];
        }
        return *this;
    }

    string& string::operator=(string&& other) noexcept {
        if (this == &other) return *this;
        if (!is_local()) {
            delete[] remote.data;
        }

        if (other.is_local()) {
            memcpy(local.data, other.local.data, SSO_CAPACITY + 1);
            local.info = other.local.info;
        } else {
            remote.data = other.remote.data;
            remote.size = other.remote.size;
            remote.capacity = other.remote.capacity;
            local.info = 0xFF;
        }

        other.local.data[0] = '\0';
        other.local.info = 0;

        return *this;
    }

    const char* string::c_str() const {
        return is_local() ? local.data : remote.data;
    }

    char& string::operator[](size_t index) {
        return is_local() ? local.data[index] : remote.data[index];
    }

    const char& string::operator[](size_t index) const {
        return is_local() ? local.data[index] : remote.data[index];
    }

    size_t string::length() const {
        return is_local() ? local.info : remote.size;
    }

    bool string::empty() const {
        return length() == 0;
    }

    void string::clear() {
        if (!is_local()) {
            delete[] remote.data;
        }
        local.data[0] = '\0';
        local.info = 0;
    }

    bool lib::string::operator==(const char* other) const {
        if (!other) return empty();
        return strcmp(c_str(), other) == 0;
    }

    bool lib::string::operator==(const string& other) const {
        if (length() != other.length()) return false;
        return strcmp(c_str(), other.c_str()) == 0;
    }

    bool lib::string::operator!=(const char* other) const {
        return !(*this == other);
    }

    bool lib::string::operator!=(const string& other) const {
        return !(*this == other);
    }
}